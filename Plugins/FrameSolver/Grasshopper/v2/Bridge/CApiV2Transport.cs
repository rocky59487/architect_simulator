// CApiV2Transport.cs -- in-process transport over frame_capi_v2.dll.
//
// This is the FAST path (no process spawn, no pipe round-trip). Loads the DLL via P/Invoke,
// holds an opaque ctx handle, and shuttles frames via frame_v2_send / frame_v2_recv. The DLL
// itself is single-DLL-per-transport: each CApiV2Transport instance allocates its own ctx,
// so multiple FrameSessions in the same process do not interfere.
//
// CONCURRENCY MODE (v2.4 + B3 follow-up: SYNCHRONOUS dispatch)
//   The native dispatcher runs the handler on the caller's thread, so SendAsync below blocks
//   until the handler returns. For short calls (solve.linear / inspect.* / analysis.*) that is
//   sub-millisecond and the GH UI thread is fine. For long calls (solve.size_opt with many
//   iterations, solve.dyn_collapse with many frames) the caller MUST drive SendAsync from a
//   worker thread (this is what the AsyncFrameComponent pattern already does in our GH
//   components -- see AssembleModelComponent's `_assembling = AssembleAsync(...)`).
//
//   The server advertises this contract via the `transport.sync` capability in hello. When
//   the B4 redesign (HANDOFF_v2.4.md § 4 C-06 / C-07) lands, the capability flips to
//   `transport.async` and clients can stop their own worker-thread off-loading.
//
// LIFETIME -- the ctx is freed in DisposeAsync. If the host process forgets to dispose, the
// DLL leaks the ctx; the finalizer pattern is intentionally NOT used because P/Invoke into a
// possibly-unloaded module from the GC thread is asking for crashes.

using System.Runtime.InteropServices;

namespace FrameCore.Bridge;

internal sealed class CApiV2Transport : ITransport
{
    private readonly string _dllPath;
    private IntPtr _libHandle = IntPtr.Zero;   // P2.4 fix: keep the NativeLibrary handle so DisposeAsync can Free it.
    private IntPtr _ctx = IntPtr.Zero;
    private bool _disposed;

    public CApiV2Transport(string dllPath)
    {
        _dllPath = dllPath ?? throw new ArgumentNullException(nameof(dllPath));
    }

    public string Name => "capi_v2";
    public bool   IsOpen => _ctx != IntPtr.Zero;

    public Task OpenAsync(CancellationToken ct)
    {
        if (_disposed) throw new ObjectDisposedException(nameof(CApiV2Transport));
        if (_ctx != IntPtr.Zero) return Task.CompletedTask;

        // P/Invoke target is the file at _dllPath. We use NativeLibrary.Load + GetExport so we
        // can read frame_v2_abi_version() BEFORE binding any other entry point -- a v3 DLL
        // that decided to break v2 would have already returned 3 here, and we refuse early
        // with a clear diagnostic rather than crashing on a missing symbol later.
        _libHandle = NativeLibrary.Load(_dllPath);
        try
        {
            var abiPtr = NativeLibrary.GetExport(_libHandle, "frame_v2_abi_version");
            var abi = Marshal.GetDelegateForFunctionPointer<AbiVersionDelegate>(abiPtr)();
            if (abi < 2)
                throw new InvalidOperationException(
                    $"frame_capi_v2.dll reports ABI version {abi}; this SDK requires >= 2.");

            // P2.2 fix: bind EVERY delegate before allocating the native ctx. Previously we
            // called frame_v2_open mid-way, then continued binding GetExport calls; if any of
            // those threw, the catch block freed the library but never frame_v2_close'd the
            // ctx, leaking the dispatcher and its session map. Binding first means the catch
            // block sees _ctx == IntPtr.Zero and has nothing to leak.
            _sendDelegate          = Marshal.GetDelegateForFunctionPointer<SendDelegate>(
                                        NativeLibrary.GetExport(_libHandle, "frame_v2_send"));
            _recvDelegate          = Marshal.GetDelegateForFunctionPointer<RecvDelegate>(
                                        NativeLibrary.GetExport(_libHandle, "frame_v2_recv"));
            _closeDelegate         = Marshal.GetDelegateForFunctionPointer<CloseDelegate>(
                                        NativeLibrary.GetExport(_libHandle, "frame_v2_close"));
            _cancelRecvDelegate    = Marshal.GetDelegateForFunctionPointer<CancelRecvDelegate>(
                                        NativeLibrary.GetExport(_libHandle, "frame_v2_cancel_recv"));
            // P1.2 fix: also bind the protocol-level cancel so CancellationToken really
            // stops a long solve instead of just dropping the client-side TCS.
            _cancelRequestDelegate = Marshal.GetDelegateForFunctionPointer<CancelRequestDelegate>(
                                        NativeLibrary.GetExport(_libHandle, "frame_v2_cancel_request"));

            // Now safe to allocate the ctx — every delegate (including frame_v2_close) is
            // bound, so even if some future code adds work between this point and `return`,
            // the catch block CAN dispose the ctx properly.
            var openPtr = NativeLibrary.GetExport(_libHandle, "frame_v2_open");
            _ctx = Marshal.GetDelegateForFunctionPointer<OpenDelegate>(openPtr)();
            if (_ctx == IntPtr.Zero)
                throw new InvalidOperationException("frame_v2_open returned NULL");
        }
        catch
        {
            // P2.4 + P2.2: free the ctx (if it was allocated) THEN the library.
            if (_ctx != IntPtr.Zero)
            {
                try { _closeDelegate?.Invoke(_ctx); } catch { /* ignore */ }
                _ctx = IntPtr.Zero;
            }
            NativeLibrary.Free(_libHandle);
            _libHandle = IntPtr.Zero;
            throw;
        }

        return Task.CompletedTask;
    }

    public ValueTask SendFrameAsync(ReadOnlyMemory<byte> frame, CancellationToken ct)
    {
        ct.ThrowIfCancellationRequested();
        if (_ctx == IntPtr.Zero) throw new InvalidOperationException("transport not open");

        unsafe
        {
            using var pin = frame.Pin();
            var rc = _sendDelegate!(_ctx, (byte*)pin.Pointer, (UIntPtr)frame.Length);
            if (rc != 0) throw new InvalidOperationException($"frame_v2_send failed ({rc})");
        }
        return ValueTask.CompletedTask;
    }

    public ValueTask<ReadOnlyMemory<byte>> ReceiveFrameAsync(CancellationToken ct)
    {
        if (_ctx == IntPtr.Zero) throw new InvalidOperationException("transport not open");

        // Two-call dance: first call with cap=0 probes size, second call fetches.
        UIntPtr outLen = UIntPtr.Zero, needed = UIntPtr.Zero;
        unsafe
        {
            // We run the recv on a worker thread so a CancellationToken from a UI thread
            // can interrupt via frame_v2_cancel_recv. Block forever (-1) inside native;
            // the cancel hook releases us.
            using (ct.Register(() => { if (_ctx != IntPtr.Zero) _cancelRecvDelegate?.Invoke(_ctx); }))
            {
                var rc = _recvDelegate!(_ctx, null, UIntPtr.Zero, &outLen, &needed, -1);
                if (rc == /*FRAME_V2_CANCELLED*/ 6) throw new OperationCanceledException(ct);
                if (rc != /*FRAME_V2_NEED_BIGGER*/ 2 && rc != 0)
                    throw new InvalidOperationException($"frame_v2_recv probe failed ({rc})");

                var size = (int)(needed != UIntPtr.Zero ? needed : outLen);
                var buf = new byte[size];
                fixed (byte* p = buf)
                {
                    rc = _recvDelegate(_ctx, p, (UIntPtr)size, &outLen, &needed, -1);
                    if (rc == 6) throw new OperationCanceledException(ct);
                    if (rc != 0) throw new InvalidOperationException($"frame_v2_recv fetch failed ({rc})");
                }
                return new ValueTask<ReadOnlyMemory<byte>>(buf.AsMemory(0, (int)outLen));
            }
        }
    }

    /// <summary>
    /// P1.2 fix: protocol-level cancel. Sends <c>frame_v2_cancel_request</c> to the native ctx
    /// so the engine drops the queued/in-flight handler for <paramref name="reqId"/> and emits
    /// an <c>error { code: "CANCELLED" }</c> frame instead of running to completion.
    /// </summary>
    public ValueTask CancelRequestAsync(string reqId, CancellationToken ct)
    {
        if (_ctx == IntPtr.Zero || _cancelRequestDelegate is null) return ValueTask.CompletedTask;
        // The native call is non-blocking — it just sets a flag — so we don't need to dispatch
        // off the calling thread. CT is honoured only as an early-out check.
        ct.ThrowIfCancellationRequested();
        var bytes = System.Text.Encoding.UTF8.GetBytes(reqId + "\0");
        unsafe
        {
            fixed (byte* p = bytes) _cancelRequestDelegate(_ctx, (sbyte*)p);
        }
        return ValueTask.CompletedTask;
    }

    public async ValueTask DisposeAsync()
    {
        if (_disposed) return;
        _disposed = true;
        if (_ctx != IntPtr.Zero)
        {
            _closeDelegate?.Invoke(_ctx);
            _ctx = IntPtr.Zero;
        }
        // P2.4 fix: free the NativeLibrary handle so long-running Rhino sessions do not leak
        // DLL handles across repeated Open/Close cycles, and so the file can be replaced for
        // an in-place upgrade.
        if (_libHandle != IntPtr.Zero)
        {
            try { NativeLibrary.Free(_libHandle); } catch { /* ignore */ }
            _libHandle = IntPtr.Zero;
        }
        await Task.CompletedTask;
    }

    // ----- P/Invoke delegates -----
    //
    // D-09 audit (HANDOFF_v2.4 § 4) -- audited line-by-line against frame_capi_v2.h prototypes.
    // Every delegate is Cdecl. Verified parameter widths against the x86_64 Windows ABI used by
    // the v2 DLL:
    //   * `frame_v2_ctx*`     <-> IntPtr               (8 bytes on x64; opaque)
    //   * `uint32_t`          <-> uint                 (4 bytes)
    //   * `int`               <-> int                  (4 bytes, signed)
    //   * `size_t`            <-> UIntPtr              (8 bytes on x64; native pointer-width)
    //   * `size_t*`           <-> UIntPtr*             (likewise)
    //   * `uint8_t* / void*`  <-> byte*                (1-byte data pointer)
    //   * `const char*`       <-> sbyte*               (1-byte data pointer; UTF-8 NUL-terminated
    //                                                    bytes supplied by the caller -- see
    //                                                    CancelRequestAsync's Encoding.UTF8.GetBytes
    //                                                    + explicit '\0' tail)
    // Result: no signature mismatch. The release-mode "silent stack corruption" failure mode
    // is ruled out for these 7 delegates. Re-run this audit any time frame_capi_v2.h grows a
    // new export.

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate uint AbiVersionDelegate();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate IntPtr OpenDelegate();

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private unsafe delegate int SendDelegate(IntPtr ctx, byte* frame, UIntPtr len);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private unsafe delegate int RecvDelegate(IntPtr ctx, byte* outBuf, UIntPtr outCap,
                                              UIntPtr* outLen, UIntPtr* outNeeded, int blockingMs);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void CloseDelegate(IntPtr ctx);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate int CancelRecvDelegate(IntPtr ctx);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private unsafe delegate int CancelRequestDelegate(IntPtr ctx, sbyte* targetId);

    private SendDelegate?          _sendDelegate;
    private RecvDelegate?          _recvDelegate;
    private CloseDelegate?         _closeDelegate;
    private CancelRecvDelegate?    _cancelRecvDelegate;
    private CancelRequestDelegate? _cancelRequestDelegate;
}
