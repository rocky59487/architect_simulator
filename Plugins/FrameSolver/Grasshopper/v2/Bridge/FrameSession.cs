// FrameSession.cs -- Layer 3 client entry point. Owns the transport, runs the hello handshake,
// caches capabilities, dispatches typed requests, and demultiplexes responses/events by request
// id. This is the single object Grasshopper components hold.
//
// USAGE
//   await using var session = await FrameSession.OpenAsync(new BridgeOptions {
//       Kind = TransportKind.CApiV2InProcess,
//       FrameCapiV2DllPath = @"C:\...\frame_capi_v2.dll"
//   });
//   if (!session.HasCapability("solve.tension_only"))
//       throw new NotSupportedException("engine too old; need TONLY capability");
//   var result = await session.SolveLinearAsync(model, options, progress, ct);
//
// THREADING
//   One session = one transport = one concurrent in-flight request set keyed by id. The
//   FrameSession is thread-safe at the public surface; internally a single dispatcher loop
//   reads frames off the transport and routes them to the matching pending request's
//   TaskCompletionSource / IAsyncEnumerable channel.

using System.Collections.Concurrent;
using System.Text.Json;
using System.Threading.Channels;

namespace FrameCore.Bridge;

public sealed class FrameSession : IAsyncDisposable
{
    private readonly ITransport _transport;
    private readonly CancellationTokenSource _shutdownCts = new();
    private readonly Task _dispatchLoop;

    // Pending one-shot requests (single response frame).
    private readonly ConcurrentDictionary<string, TaskCompletionSource<WireFrame>> _pendingSingle = new();
    // Pending streaming requests (multiple event frames + a final response).
    private readonly ConcurrentDictionary<string, Channel<WireFrame>> _pendingStreams = new();

    private long _nextRequestId = 1;

    private FrameSession(ITransport transport)
    {
        _transport    = transport;
        _dispatchLoop = Task.Run(DispatchLoopAsync);
    }

    public string EngineBuildSha { get; private set; } = "";
    public string EngineVersion  { get; private set; } = "";
    public string SchemaVersion  { get; private set; } = "";
    public IReadOnlyCollection<string> Capabilities { get; private set; } = Array.Empty<string>();
    public IReadOnlyDictionary<string, JsonElement> Limits { get; private set; } = new Dictionary<string, JsonElement>();
    public string TransportName  => _transport.Name;

    /// <summary>
    /// Connection-level profile chosen at <see cref="OpenSimpleAsync"/> /
    /// <see cref="OpenAdvancedAsync"/>. Drives default-fill, silent-fallback, and
    /// diagnostic-stream behaviour throughout the SDK. See docs/specs/S6b_rhino_bridge_v2.md § ⑭.
    /// </summary>
    public BridgeProfile Profile { get; private set; } = BridgeProfile.Simple;

    public bool HasCapability(string cap) =>
        Capabilities is HashSet<string> hs ? hs.Contains(cap) : Capabilities.Contains(cap);

    /// <summary>
    /// Open a session in <see cref="BridgeProfile.Simple"/>. The session fills sensible defaults
    /// for any option the caller omits and accepts silent fallbacks (e.g. supernodal SPD
    /// failure -&gt; LDLT). Use this for UE5-style "just give me the answer" workflows. Equivalent
    /// to calling <see cref="OpenAsync"/> with no special handshake; preserved as a named entry
    /// point so call sites read self-documentingly.
    /// </summary>
    public static Task<FrameSession> OpenSimpleAsync(BridgeOptions opts, CancellationToken ct = default)
        => OpenAsync(opts, BridgeProfile.Simple, ct);

    /// <summary>
    /// Open a session in <see cref="BridgeProfile.Advanced"/>. The engine rejects missing options,
    /// refuses silent fallbacks (returns structured errors instead), and every result carries a
    /// populated <see cref="Result.AdvancedDiagnostics"/>. Throws <see cref="NotSupportedException"/>
    /// when the engine does not advertise the <c>profile.advanced</c> capability. Use this for
    /// official Rhino, the Rhino-UE5 bridge, and any academic / CI workflow that needs to see
    /// the silent paths explicitly.
    /// </summary>
    public static async Task<FrameSession> OpenAdvancedAsync(BridgeOptions opts, CancellationToken ct = default)
    {
        var s = await OpenAsync(opts, BridgeProfile.Advanced, ct).ConfigureAwait(false);
        if (!s.HasCapability("profile.advanced"))
        {
            await s.DisposeAsync().ConfigureAwait(false);
            throw new NotSupportedException(
                "engine does not advertise 'profile.advanced'; advanced profile is unavailable on this build " +
                $"(engine {s.EngineVersion}, sha={s.EngineBuildSha}). Use OpenSimpleAsync instead, or upgrade the engine.");
        }
        return s;
    }

    /// <summary>
    /// Open a session against the configured transport in an explicit profile. Performs the hello
    /// handshake; the returned session has Capabilities / EngineBuildSha / SchemaVersion / Profile
    /// populated. Prefer <see cref="OpenSimpleAsync"/> / <see cref="OpenAdvancedAsync"/> at call
    /// sites for self-documenting intent.
    /// </summary>
    public static async Task<FrameSession> OpenAsync(BridgeOptions opts, BridgeProfile profile = BridgeProfile.Simple, CancellationToken ct = default)
    {
        ITransport transport = opts.Kind switch
        {
            // P2.3 fix: honour the documented contract that FrameCapiV2DllPath==null falls back
            // to the assembly directory (where the GHA ships its sibling DLL). Throw only when
            // even the autodetect path does not exist.
            TransportKind.CApiV2InProcess => new CApiV2Transport(ResolveCapiV2DllPath(opts.FrameCapiV2DllPath)),
            TransportKind.Stdio       => throw new NotImplementedException("Stdio transport: B6 milestone"),
            TransportKind.NamedPipe   => throw new NotImplementedException("NamedPipe transport: B6 milestone"),
            TransportKind.Tcp         => throw new NotImplementedException("Tcp transport: B6 milestone"),
            _ => throw new ArgumentOutOfRangeException(nameof(opts), opts.Kind, null)
        };

        // P2.5 fix + P0.1 fix: open + hello must be atomic from the caller's perspective. If
        // either step throws, dispose the transport before bubbling so we never leak the
        // native ctx / DLL handle. A boolean success flag is the only way to distinguish
        // "session is null because OpenAsync threw" from "session is null because we
        // already detached the successful one" -- the previous null-check approach disposed
        // the freshly opened transport on the happy path.
        FrameSession? session = null;
        bool success = false;
        try
        {
            await transport.OpenAsync(ct).ConfigureAwait(false);
            session = new FrameSession(transport) { Profile = profile };
            await session.HelloAsync(opts, profile, ct).ConfigureAwait(false);
            success = true;
            return session;
        }
        finally
        {
            if (!success)
            {
                if (session is not null)
                {
                    try { await session.DisposeAsync().ConfigureAwait(false); } catch { /* ignore */ }
                }
                else
                {
                    // OpenAsync threw before `session` was constructed: dispose transport
                    // directly so the native DLL handle does not leak.
                    try { await transport.DisposeAsync().ConfigureAwait(false); } catch { /* ignore */ }
                }
            }
        }
    }

    /// <summary>
    /// Resolve the frame_capi_v2.dll path. Honours an explicit caller value; otherwise looks
    /// next to the executing assembly (the convention for a packaged .gha), then the dev-tree
    /// fall-back used by FrameCore.Gh's debug runs. Throws when all attempts fail so the user
    /// sees a clear path list rather than a downstream "module not found" from NativeLibrary.
    /// </summary>
    private static string ResolveCapiV2DllPath(string? explicitPath)
    {
        if (!string.IsNullOrWhiteSpace(explicitPath)) return explicitPath!;
        var tried = new System.Collections.Generic.List<string>();
        var hereAsm = typeof(FrameSession).Assembly.Location;
        var here    = string.IsNullOrEmpty(hereAsm) ? null : System.IO.Path.GetDirectoryName(hereAsm);
        if (!string.IsNullOrEmpty(here))
        {
            var sibling = System.IO.Path.Combine(here!, "frame_capi_v2.dll");
            tried.Add(sibling);
            if (System.IO.File.Exists(sibling)) return sibling;
        }
        var baseDir = AppContext.BaseDirectory;
        if (!string.IsNullOrEmpty(baseDir))
        {
            var siblingBase = System.IO.Path.Combine(baseDir, "frame_capi_v2.dll");
            tried.Add(siblingBase);
            if (System.IO.File.Exists(siblingBase)) return siblingBase;
        }
        throw new System.IO.FileNotFoundException(
            "frame_capi_v2.dll could not be auto-resolved. Set BridgeOptions.FrameCapiV2DllPath explicitly. " +
            "Searched: " + string.Join("; ", tried));
    }

    // ----- Handshake --------------------------------------------------------------------------

    private async Task HelloAsync(BridgeOptions opts, BridgeProfile profile, CancellationToken ct)
    {
        var helloBody = new
        {
            client            = opts.ClientTag,
            preferredSchemas  = opts.PreferredSchemas,
            wantsBinary       = opts.WantsBinaryPayloads,
            profile           = profile == BridgeProfile.Advanced ? "advanced" : "simple"
        };
        var id = "hs_" + Interlocked.Increment(ref _nextRequestId);
        using var hello = await SendAndAwaitInternal("hello", "", id, helloBody, ct).ConfigureAwait(false);

        var body = hello.Header.RootElement.GetProperty("body");
        EngineBuildSha = body.TryGetProperty("buildSha",  out var s) ? s.GetString() ?? "" : "";
        EngineVersion  = body.TryGetProperty("version",   out var v) ? v.GetString() ?? "" : "";
        SchemaVersion  = body.TryGetProperty("schemaVer", out var sv) ? sv.GetString() ?? "" : "";

        var caps = new HashSet<string>(StringComparer.Ordinal);
        if (body.TryGetProperty("capabilities", out var capArray) && capArray.ValueKind == JsonValueKind.Array)
        {
            foreach (var c in capArray.EnumerateArray())
            {
                var s2 = c.GetString();
                if (!string.IsNullOrEmpty(s2)) caps.Add(s2);
            }
        }
        Capabilities = caps;

        var limits = new Dictionary<string, JsonElement>(StringComparer.Ordinal);
        if (body.TryGetProperty("limits", out var limObj) && limObj.ValueKind == JsonValueKind.Object)
        {
            foreach (var p in limObj.EnumerateObject())
                limits[p.Name] = p.Value.Clone();
        }
        Limits = limits;
    }

    // ----- Public request helpers (raw, for unit tests / SDK extensions) ---------------------

    /// <summary>
    /// Send a single-response request and await the matching response frame. Throws
    /// <see cref="RemoteException"/> if the server returns an <c>error</c> kind.
    /// </summary>
    public Task<WireFrame> SendAndAwaitSingleAsync(string method, string id, object body, CancellationToken ct) =>
        SendAndAwaitInternal("request", method, id, body, ct);

    private async Task<WireFrame> SendAndAwaitInternal(string kind, string method, string id, object body, CancellationToken ct)
    {
        var tcs = new TaskCompletionSource<WireFrame>(TaskCreationOptions.RunContinuationsAsynchronously);
        if (!_pendingSingle.TryAdd(id, tcs))
            throw new InvalidOperationException("duplicate request id " + id);

        var header = JsonSerializer.SerializeToElement(new
        {
            v      = 2,
            kind,
            id,
            method = string.IsNullOrEmpty(method) ? null : method,
            body
        }, new JsonSerializerOptions { DefaultIgnoreCondition = System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull });
        var headerBytes = FrameProtocol.EncodeHeader(header);
        var frame = FrameProtocol.Serialize(FrameFlags.None, headerBytes, ReadOnlySpan<byte>.Empty);

        try
        {
            await _transport.SendFrameAsync(frame, ct).ConfigureAwait(false);
        }
        catch
        {
            _pendingSingle.TryRemove(id, out _);
            throw;
        }

        // P1.2 fix: ct.Cancel() now BOTH drops the client-side TCS AND sends a protocol-level
        // `cancel` frame to the engine so a long-running solve actually stops instead of
        // running to completion and pushing stale frames back. The native cancel call is
        // fire-and-forget (it just sets a flag in the dispatcher), so we don't await it.
        // P2.1 fix: also remove the pending entry from _pendingSingle so even if the engine
        // never emits a final frame (B2 stub level, or a future race), the dictionary does
        // not accumulate stale TCS over the session lifetime.
        using (ct.Register(() =>
        {
            try { _ = _transport.CancelRequestAsync(id, CancellationToken.None); }
            catch { /* fire-and-forget; tcs cancellation is the authoritative signal */ }
            _pendingSingle.TryRemove(id, out _);
            tcs.TrySetCanceled(ct);
        }))
        {
            var rsp = await tcs.Task.ConfigureAwait(false);
            if (rsp.Header.RootElement.GetProperty("kind").GetString() == "error")
            {
                using (rsp.Header)
                {
                    var err = rsp.Header.RootElement.GetProperty("body");
                    var code = err.TryGetProperty("code", out var c) ? c.GetString() ?? "INTERNAL" : "INTERNAL";
                    var msg  = err.TryGetProperty("message", out var m) ? m.GetString() ?? "" : "";
                    throw new RemoteException(code, msg);
                }
            }
            return rsp;
        }
    }

    /// <summary>
    /// Send a streaming request and pull frames as they arrive. The enumerator completes when
    /// the server sets the END_OF_RESPONSE flag (or on cancellation).
    ///
    /// P1.2 fix: cancellation now sends a protocol-level <c>cancel</c> frame to the engine so
    /// a long-running streaming solve (DYNC, ArcLength) actually stops generating events
    /// rather than running to completion while the client ignores them.
    /// </summary>
    public async IAsyncEnumerable<WireFrame> SendAndStreamAsync(string method, string id, object body,
                                                                [System.Runtime.CompilerServices.EnumeratorCancellation] CancellationToken ct = default)
    {
        var channel = Channel.CreateUnbounded<WireFrame>(new UnboundedChannelOptions { SingleReader = true, SingleWriter = false });
        if (!_pendingStreams.TryAdd(id, channel))
            throw new InvalidOperationException("duplicate request id " + id);

        var header = JsonSerializer.SerializeToElement(new
        {
            v = 2, kind = "request", id, method, body
        });
        var headerBytes = FrameProtocol.EncodeHeader(header);
        var frame = FrameProtocol.Serialize(FrameFlags.None, headerBytes, ReadOnlySpan<byte>.Empty);
        try
        {
            await _transport.SendFrameAsync(frame, ct).ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            _pendingStreams.TryRemove(id, out _);
            channel.Writer.TryComplete(ex);
            throw;
        }

        // P2.1 fix: drop the channel from _pendingStreams on cancel so a hung engine path
        // does not stake a permanent entry. Channel.Writer.TryComplete is idempotent.
        using var reg = ct.Register(() =>
        {
            try { _ = _transport.CancelRequestAsync(id, CancellationToken.None); }
            catch { /* fire-and-forget */ }
            if (_pendingStreams.TryRemove(id, out var ch)) ch.Writer.TryComplete();
        });

        try
        {
            await foreach (var f in channel.Reader.ReadAllAsync(ct).ConfigureAwait(false))
            {
                yield return f;
                if (f.IsEndOfResponse) break;
            }
        }
        finally
        {
            if (_pendingStreams.TryRemove(id, out var ch))
            {
                ch.Writer.TryComplete();
                if (!ct.IsCancellationRequested)
                {
                    try { _ = _transport.CancelRequestAsync(id, CancellationToken.None); }
                    catch { /* fire-and-forget */ }
                }
            }
        }
    }

    // ----- Dispatch loop ----------------------------------------------------------------------

    private async Task DispatchLoopAsync()
    {
        var ct = _shutdownCts.Token;
        try
        {
            while (!ct.IsCancellationRequested)
            {
                var bytes = await _transport.ReceiveFrameAsync(ct).ConfigureAwait(false);
                if (bytes.IsEmpty) break;

                var frame = FrameProtocol.Parse(bytes, out _);
                var id = frame.Header.RootElement.TryGetProperty("id", out var idEl) ? idEl.GetString() : null;
                if (string.IsNullOrEmpty(id)) { frame.Header.Dispose(); continue; }

                if (_pendingStreams.TryGetValue(id, out var ch))
                {
                    try
                    {
                        await ch.Writer.WriteAsync(frame, ct).ConfigureAwait(false);
                    }
                    catch (ChannelClosedException)
                    {
                        frame.Header.Dispose();
                        continue;
                    }
                    if (frame.IsEndOfResponse)
                    {
                        ch.Writer.TryComplete();
                        _pendingStreams.TryRemove(id, out _);
                    }
                }
                else if (_pendingSingle.TryRemove(id, out var tcs))
                {
                    tcs.TrySetResult(frame);
                }
                else
                {
                    // Unmatched id; likely a stale event after the consumer dropped its handle.
                    frame.Header.Dispose();
                }
            }
        }
        catch (OperationCanceledException ex)
        {
            CompletePending(ex);
        }
        catch (Exception ex)
        {
            // Bubble the transport error to anybody still waiting.
            CompletePending(ex);
        }
    }

    private void CompletePending(Exception ex)
    {
        foreach (var kv in _pendingSingle)
            if (_pendingSingle.TryRemove(kv.Key, out var tcs))
                tcs.TrySetException(ex);
        foreach (var kv in _pendingStreams)
            if (_pendingStreams.TryRemove(kv.Key, out var ch))
                ch.Writer.TryComplete(ex);
    }

    // ----- IAsyncDisposable -------------------------------------------------------------------

    public async ValueTask DisposeAsync()
    {
        _shutdownCts.Cancel();
        try { await _dispatchLoop.ConfigureAwait(false); } catch { /* ignore */ }
        CompletePending(new ObjectDisposedException(nameof(FrameSession)));
        await _transport.DisposeAsync().ConfigureAwait(false);
        _shutdownCts.Dispose();
    }

    internal string NextId() => "r" + Interlocked.Increment(ref _nextRequestId).ToString(System.Globalization.CultureInfo.InvariantCulture);
}

/// <summary>Thrown when the engine returns an <c>error</c> kind frame.</summary>
public sealed class RemoteException : Exception
{
    public RemoteException(string code, string message) : base(message) { Code = code; }
    public string Code { get; }
    public override string ToString() => $"[{Code}] {Message}";
}
