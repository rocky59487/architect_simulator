// OpenFrameCoreComponent.cs -- Tab 1 "Setup" entry. Drops on canvas, opens a FrameSession via
// the configured transport, outputs a GH_FrameSession. Profile toggled via right-click menu.
//
// P1.1 fix: previous version used `async void SolveInstance` and wrote to IGH_DataAccess
// AFTER the await -- but Grasshopper's DA is only valid inside the synchronous SolveInstance
// scope. This rewrite follows the cache + ExpireSolution two-pass pattern that AsyncFrameComponent
// also uses:
//   Pass A: SolveInstance reads inputs synchronously, kicks off background OpenAsync, returns
//           with a "opening..." message. NO DA writes for the session output (we have no value).
//   Pass B: when OpenAsync completes, the worker schedules ExpireSolution on the GH UI thread.
//           GH calls SolveInstance again; this time the cached session is written into DA.
//
// Reset is a right-click menu action -- it invalidates the cached session and re-runs Pass A.

using System;
using System.Drawing;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using FrameCore.Bridge;
using FrameCore.Gh.Common;
using Grasshopper.Kernel;

namespace FrameCore.Gh.Components.Setup
{
    public sealed class OpenFrameCoreComponent : GH_Component
    {
        private FrameSession?           _session;
        private BridgeProfile           _profile = BridgeProfile.Simple;
        private CancellationTokenSource _cts     = new();
        private Task?                   _opening;
        private string                  _info    = "";
        private string                  _error   = "";
        private string                  _requestedDllKey = "";
        private string                  _sessionDllKey   = "";
        // P1.2 fix (third audit round): generation counter so a late-arriving background open
        // task from a previous Reset / profile switch cannot overwrite the current state. Every
        // Open*Async snapshots the generation at start and only commits if it matches at end;
        // ResetSession bumps the generation so all in-flight tasks become stale.
        private long                    _openGeneration = 0;
        // D-03 fix: the gen-match check (`Interlocked.Read(_openGeneration) == thisGen`) and the
        // _session/_info/_error writes that follow it have to happen atomically -- otherwise a
        // ResetSession that bumps _openGeneration after the check but before the writes makes
        // this task commit a stale session. Hold _openGate across check-then-write to close the
        // window. ResetSession also acquires _openGate when it bumps the generation, so the
        // bump is serialised against any in-flight commit.
        private readonly object         _openGate = new();

        public OpenFrameCoreComponent()
            : base("Open FrameCore", "FC",
                   "Connect to the FrameCore engine. Drop this once per definition; downstream " +
                   "components read its session output. Right-click to switch Simple/Advanced profile " +
                   "or to Reset and reconnect.",
                   Tabs.Category, Tabs.Setup)
        { }

        public override Guid ComponentGuid => new("D1F4C1AA-2A0B-4D3E-9C8E-FEE100000101");
        protected override Bitmap? Icon => Resources.LoadIcon("setup-open");

        protected override void RegisterInputParams(GH_InputParamManager p)
        {
            p.AddTextParameter("DLL", "D", "Path to frame_capi_v2.dll. Empty = autodetect next to GHA.", GH_ParamAccess.item, "");
        }

        protected override void RegisterOutputParams(GH_OutputParamManager p)
        {
            p.AddParameter(new GhParameters.Param_FrameSession(), "Session", "S",
                "Engine session — connect to AssembleModel and Solve.", GH_ParamAccess.item);
            p.AddTextParameter("Info", "I", "Engine version / build SHA / capabilities summary.", GH_ParamAccess.item);
        }

        protected override void SolveInstance(IGH_DataAccess da)
        {
            string dll = "";
            da.GetData(0, ref dll);
            var dllKey = DllKey(dll);

            if (_session is not null
                && _opening is null
                && !SameDllKey(_sessionDllKey, dllKey))
            {
                InvalidateCurrentSession();
            }

            // Pass B: cached session is ready, write outputs synchronously.
            if (_session is not null && _opening is null)
            {
                da.SetData(0, new GH_FrameSession(_session));
                da.SetData(1, _info);
                Message = $"{(_profile == BridgeProfile.Advanced ? "A" : "S")} · {_session.EngineVersion}";
                return;
            }

            // Surface a hard failure path immediately so the user sees the error on the canvas.
            if (!string.IsNullOrEmpty(_error))
            {
                if (SameDllKey(_requestedDllKey, dllKey))
                {
                    AddRuntimeMessage(GH_RuntimeMessageLevel.Error, _error);
                    Message = "error";
                    return;
                }
                _error = "";
            }

            // Pass A: kick off OpenAsync in the background and return with a placeholder.
            // We do NOT write to DA here (no value yet) -- GH treats the wire as empty until
            // the deferred ExpireSolution arrives.
            if (_opening is null)
            {
                _cts.Cancel(); _cts = new CancellationTokenSource();
                // P1.2 fix: snapshot the generation and profile FOR THIS TASK so a Reset that
                // bumps generation later cannot make this task write back its session.
                long thisGen      = Interlocked.Increment(ref _openGeneration);
                var  thisProfile  = _profile;
                _requestedDllKey = dllKey;
                _opening = OpenInBackgroundAsync(dll, dllKey, thisProfile, thisGen, _cts.Token);
            }
            Message = "opening...";
        }

        private async Task OpenInBackgroundAsync(string dll, string dllKey,
                                                 BridgeProfile thisProfile, long thisGen, CancellationToken ct)
        {
            FrameSession? created = null;
            string nextInfo = "", nextError = "";
            try
            {
                var opts = new BridgeOptions
                {
                    Kind = TransportKind.CApiV2InProcess,
                    FrameCapiV2DllPath = string.IsNullOrWhiteSpace(dll) ? null : dll,
                    ClientTag = "FrameCore.Gh/2.0"
                };
                created = thisProfile == BridgeProfile.Advanced
                    ? await FrameSession.OpenAdvancedAsync(opts, ct).ConfigureAwait(false)
                    : await FrameSession.OpenSimpleAsync   (opts, ct).ConfigureAwait(false);
                nextInfo = $"FrameCore {created.EngineVersion} (sha={created.EngineBuildSha}) schema={created.SchemaVersion} caps={created.Capabilities.Count}";
            }
            catch (OperationCanceledException)
            {
                /* expected during Reset; nextError stays empty */
            }
            catch (FileNotFoundException ex) { nextError = ex.Message; }
            catch (NotSupportedException ex) { nextError = ex.Message; }
            catch (Exception ex)             { nextError = "OpenFrameCore: " + ex.Message; }

            // P1.2 + D-03 fix: commit the result only if the generation still matches AND hold
            // _openGate across check-then-write so a ResetSession can't slip in between (which
            // would let us commit a session that the user has already invalidated). The early
            // return after the gate uses Interlocked.Read for the cheap re-check on the way out.
            bool committed = false;
            lock (_openGate)
            {
                if (Interlocked.Read(ref _openGeneration) == thisGen)
                {
                    _session = created;
                    _info    = nextInfo;
                    _error   = nextError;
                    _sessionDllKey = created is not null ? dllKey : "";
                    _opening = null;
                    committed = true;
                }
            }
            if (!committed && created is not null)
            {
                try { await created.DisposeAsync().ConfigureAwait(false); } catch { /* ignore */ }
            }

            if (!committed)
                return;

            // Hop to the GH UI thread to re-trigger SolveInstance with the cached value.
            Grasshopper.Instances.DocumentEditor?.BeginInvoke((Action)(() =>
            {
                try { ExpireSolution(true); } catch { /* document already gone */ }
            }));
        }

        // ----- Right-click menu (Profile + Reset) ---------------------------------------------

        protected override void AppendAdditionalMenuItems(System.Windows.Forms.ToolStripDropDown menu)
        {
            base.AppendAdditionalMenuItems(menu);
            Menu_AppendItem(menu, "Profile: Simple (UE5)",   (_, __) => SetProfile(BridgeProfile.Simple),
                            true, _profile == BridgeProfile.Simple);
            Menu_AppendItem(menu, "Profile: Advanced (Rhino)", (_, __) => SetProfile(BridgeProfile.Advanced),
                            true, _profile == BridgeProfile.Advanced);
            Menu_AppendItem(menu, "Reset (reconnect)", (_, __) => ResetSession());
        }

        private void SetProfile(BridgeProfile p)
        {
            if (_profile == p) return;
            _profile = p;
            ResetSession();
        }

        private void ResetSession() => ResetSession(true);

        private void ResetSession(bool expire)
        {
            InvalidateCurrentSession();
            if (expire) ExpireSolution(true);
        }

        private void InvalidateCurrentSession()
        {
            // P1.2 + D-03 fix: bump the generation and clear cached state under _openGate so
            // OpenInBackgroundAsync's check-then-write region (which is also gated) cannot
            // observe a stale _openGeneration value and commit a session that we have just
            // invalidated. Once the lock is released, any post-lock commit returns early via
            // the !committed branch and disposes the orphaned session.
            FrameSession? stale;
            lock (_openGate)
            {
                Interlocked.Increment(ref _openGeneration);
                _cts.Cancel(); _cts = new CancellationTokenSource();
                _opening = null;
                stale = _session;
                _session = null; _info = ""; _error = "";
                _requestedDllKey = "";
                _sessionDllKey   = "";
            }
            if (stale is not null)
                _ = Task.Run(async () => { try { await stale.DisposeAsync(); } catch { /* ignore */ } });
        }

        private static string DllKey(string dll)
        {
            var trimmed = (dll ?? "").Trim();
            if (trimmed.Length == 0) return "";
            try { return Path.GetFullPath(trimmed); }
            catch { return trimmed; }
        }

        private static bool SameDllKey(string a, string b) =>
            string.Equals(a, b, StringComparison.OrdinalIgnoreCase);

        // ----- (De)serialisation of profile across GH definition save/load --------------------

        public override bool Write(GH_IO.Serialization.GH_IWriter w)
        {
            w.SetInt32("profile", (int)_profile);
            return base.Write(w);
        }
        public override bool Read(GH_IO.Serialization.GH_IReader r)
        {
            if (r.ItemExists("profile")) _profile = (BridgeProfile)r.GetInt32("profile");
            return base.Read(r);
        }

        public override void RemovedFromDocument(GH_Document document)
        {
            ResetSession(false);
            base.RemovedFromDocument(document);
        }
    }
}
