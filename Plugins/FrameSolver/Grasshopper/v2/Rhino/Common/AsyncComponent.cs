// AsyncComponent.cs -- base class for any component that calls into the FrameCore engine.
// Encapsulates the cancel-previous + run-on-background + ExpireSolution-on-result loop, plus
// the wire-tip status update.
//
// USAGE
//   protected override async Task<TOut> RunAsync(TIn input, CancellationToken ct) {
//       return await Session.SolveLinearAsync(input.EngineSession, ct: ct);
//   }
//
// THREADING
//   SolveInstance runs on the GH main thread. We grab inputs there (DA reads are not thread-
//   safe outside SolveInstance), kick off RunAsync on the pool, and re-enter SolveInstance via
//   ExpireSolution to write the cached result back to DA. Two SolveInstance calls per user
//   action total — same shape as Karamba's async components.
//
// CANCEL
//   Each new SolveInstance cancels the previous one's CTS, so a slider drag does not pile up
//   in-flight solves. When advanced profile is on we also send a protocol-level `cancel` to
//   the engine via FrameSession (B4 wires this when DYNC streaming lands).

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using FrameCore.Bridge;
using Grasshopper.Kernel;

namespace FrameCore.Gh.Common
{
    public abstract class AsyncFrameComponent<TIn, TOut> : GH_Component
        where TIn  : class
        where TOut : class
    {
        protected AsyncFrameComponent(string name, string nickname, string description,
                                       string category, string subcategory)
            : base(name, nickname, description, category, subcategory) { }

        private CancellationTokenSource _cts = new();
        private TOut? _result;
        private bool  _solving;
        private bool  _cancelled;
        private long  _elapsedMs;
        private int   _defaults, _errors, _warnings;
        private bool  _terminalErrorReady;
        // P1.1 fix (third audit round): runtime-message emission MUST happen on the GH UI
        // thread. The background Task.Run catch used to call AddRuntimeMessage directly,
        // which is unsupported and can race with GH's own message strip rendering. We now
        // cache (level, text) entries from the worker and replay them on the next
        // synchronous SolveInstance.
        private readonly List<(GH_RuntimeMessageLevel Level, string Text)> _pendingMessages = new();
        private readonly object _pendingMessagesLock = new();
        protected BridgeProfile Profile { get; set; } = BridgeProfile.Simple;
        protected UnitSystem    Units   { get; set; } = UnitSystem.SI_N_mm_MPa;

        /// <summary>Read inputs synchronously on the GH main thread.</summary>
        protected abstract bool TryReadInputs(IGH_DataAccess da, out TIn input);

        /// <summary>Do the engine work asynchronously off the main thread.</summary>
        protected abstract Task<TOut> RunAsync(TIn input, CancellationToken ct);

        /// <summary>Write outputs synchronously on the GH main thread.</summary>
        protected abstract void WriteOutputs(IGH_DataAccess da, TOut result);

        /// <summary>Optional override: count silent defaults that were filled. Used in wire-tip.</summary>
        protected virtual int CountDefaults(TOut result) => 0;

        protected sealed override void SolveInstance(IGH_DataAccess da)
        {
            // First thing every SolveInstance does: drain any pending messages captured by a
            // background task. Safe to call AddRuntimeMessage here -- we are on the GH UI
            // thread by definition of SolveInstance.
            DrainPendingMessages();

            // A background failure self-expires once so the queued runtime message can be
            // emitted on the GH thread. Do not immediately start the same failed request again;
            // a later input-driven solution will clear this latch and retry normally.
            if (_terminalErrorReady)
            {
                _terminalErrorReady = false;
                _solving = false;
                FlushMessage();
                return;
            }

            // Phase B: result is ready, write outputs and update wire-tip.
            if (_result is not null)
            {
                WriteOutputs(da, _result);
                _defaults = CountDefaults(_result);
                FlushMessage();
                _result = null;
                _solving = false;
                return;
            }

            // Phase A: read inputs, cancel previous run, kick off background work.
            _cts.Cancel();
            _cts = new CancellationTokenSource();
            _cancelled = false;
            _errors = _warnings = 0;
            _solving = true;
            FlushMessage();

            if (!TryReadInputs(da, out var input)) { _solving = false; FlushMessage(); return; }

            var ct = _cts.Token;
            var watch = Stopwatch.StartNew();
            Task.Run(async () =>
            {
                try
                {
                    var r = await RunAsync(input, ct).ConfigureAwait(false);
                    watch.Stop();
                    _elapsedMs = watch.ElapsedMilliseconds;
                    if (ct.IsCancellationRequested) { _cancelled = true; return; }
                    _result = r;
                }
                catch (OperationCanceledException) { _cancelled = true; }
                catch (RemoteException ex)
                {
                    if (ct.IsCancellationRequested) { _cancelled = true; return; }
                    _errors = 1;
                    _terminalErrorReady = true;
                    // P1.1 fix: queue, do not call AddRuntimeMessage from a background thread.
                    EnqueueMessage(GH_RuntimeMessageLevel.Error, $"[{ex.Code}] {ex.Message}");
                }
                catch (Exception ex)
                {
                    if (ct.IsCancellationRequested) { _cancelled = true; return; }
                    _errors = 1;
                    _terminalErrorReady = true;
                    EnqueueMessage(GH_RuntimeMessageLevel.Error, ex.Message);
                }
                finally
                {
                    if (!ct.IsCancellationRequested)
                        Grasshopper.Instances.DocumentEditor?.BeginInvoke((Action)(() => ExpireSolution(true)));
                }
            });
        }

        private void EnqueueMessage(GH_RuntimeMessageLevel level, string text)
        {
            lock (_pendingMessagesLock) _pendingMessages.Add((level, text));
        }

        private void DrainPendingMessages()
        {
            (GH_RuntimeMessageLevel level, string text)[]? batch = null;
            lock (_pendingMessagesLock)
            {
                if (_pendingMessages.Count == 0) return;
                batch = _pendingMessages.ToArray();
                _pendingMessages.Clear();
            }
            foreach (var (level, text) in batch) AddRuntimeMessage(level, text);
        }

        private void FlushMessage()
        {
            var (text, _) = UiMessage.Format(Profile, Units, _elapsedMs,
                                              _defaults, _errors, _warnings,
                                              _solving, _cancelled);
            Message = text;
        }

        public override void RemovedFromDocument(GH_Document document)
        {
            try { _cts.Cancel(); } catch { /* ignore */ }
            _cts.Dispose();
            base.RemovedFromDocument(document);
        }
    }
}
