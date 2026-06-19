// ITransport.cs -- Layer 2 abstraction. Each TransportKind has one implementation; the
// FrameSession dispatcher never sees the difference. New transports plug in without touching
// Layer 3 (Model / Result / Component) code.

namespace FrameCore.Bridge;

/// <summary>
/// Layer 2 contract. Drives raw frames to/from the engine. All members are async because at
/// least one implementation (TCP, named pipe) is inherently async; in-process P/Invoke
/// implementations just complete synchronously inside a ValueTask.
/// </summary>
public interface ITransport : IAsyncDisposable
{
    /// <summary>Stable identifier for diagnostics ("capi_v2" / "stdio" / "pipe" / "tcp").</summary>
    string Name { get; }

    /// <summary>True once the underlying connection is open and ready for send/recv.</summary>
    bool IsOpen { get; }

    /// <summary>
    /// Open the transport. The implementation does NOT send the hello frame -- that is the
    /// Session's responsibility, kept above the transport line.
    /// </summary>
    Task OpenAsync(CancellationToken ct);

    /// <summary>
    /// Hand a complete framed message to the engine. Implementations may queue and complete
    /// the task before the engine has processed the frame -- the dispatcher always responds
    /// via a recv frame, so callers track completion by id, not by send-task completion.
    /// </summary>
    ValueTask SendFrameAsync(ReadOnlyMemory<byte> frame, CancellationToken ct);

    /// <summary>
    /// Pull the next complete framed message from the engine. The returned buffer is owned
    /// by the transport's pool and is valid until the next call into this transport; the
    /// FrameSession copies/decodes immediately, so this contract is sufficient.
    /// </summary>
    /// <returns>The raw frame bytes. <see cref="ReadOnlyMemory{Byte}.Empty"/> on closed pipe.</returns>
    ValueTask<ReadOnlyMemory<byte>> ReceiveFrameAsync(CancellationToken ct);

    /// <summary>
    /// P1.2 fix: send a protocol-level cancel for <paramref name="reqId"/> directly to the
    /// engine. This is what makes <see cref="CancellationToken"/> actually stop a long solve
    /// rather than just discarding the client-side TaskCompletionSource. Implementations
    /// should invoke the transport's native cancel entry point (for the in-process DLL it is
    /// <c>frame_v2_cancel_request</c>) so the engine drops queued work for that id and emits
    /// an <c>error { code: "CANCELLED" }</c> frame to terminate the request.
    ///
    /// B2 NOTE: the dispatcher's Submit loop is synchronous (one handler at a time), so a
    /// cancel that arrives while the engine is mid-solve only takes effect after the current
    /// handler returns. B4's streaming work adds in-handler cancel polling so DYNC / Modal
    /// can interrupt mid-iteration. Either way the client-side semantics are unchanged:
    /// CancellationToken.Cancel() → request id is dropped on the engine side, no stale frames.
    /// </summary>
    ValueTask CancelRequestAsync(string reqId, CancellationToken ct);
}
