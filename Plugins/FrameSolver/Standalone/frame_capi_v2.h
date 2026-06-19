// frame_capi_v2.h -- C ABI v2 for the FrameCore Rhino/Grasshopper bridge.
//
// SCOPE
//   This header defines the LONG-TERM-STABLE C boundary between FrameCore and any out-of-process
//   or in-process client (Rhino .gha, Python tools, future cloud sidecars). The wire schema is
//   documented in docs/specs/S6b_rhino_bridge_v2.md; this header is the TRANSPORT for that schema.
//   The existing frame_capi.h / frame_capi_solve_text() (J2) is NOT replaced -- it stays available
//   for legacy clients forever. v2 ships as a SEPARATE DLL (frame_capi_v2.dll) so v1 consumers
//   never break when v2 evolves.
//
// DESIGN PRINCIPLES (the reason this header looks the way it does)
//   1) Opaque context handle -- the ABI never exposes internal types; all state lives behind
//      a forward-declared struct, so future engine refactors don't break the boundary.
//   2) Frame-based send/recv -- the same dispatcher serves stdio, in-process DLL, named-pipe and
//      TCP transports. No per-method C entry point means new methods (solve.modal,
//      solve.dyn_collapse, ...) are added with ZERO ABI changes.
//   3) Two-call buffer dance with NEED_BIGGER -- the caller never has to guess buffer size.
//   4) blockingMs parameter -- caller chooses sync (block forever), polling (0 ms) or bounded
//      wait. GH UI thread can poll without freezing Rhino.
//   5) Strict integer ABI version -- frame_v2_abi_version() returns a single monotone integer;
//      a client built against v=2 refuses to talk to a DLL returning v=1, but a v=3 DLL is
//      REQUIRED to keep serving v=2 callers for at least two minor versions (see spec).
//   6) No C++ exceptions cross the boundary -- everything in the impl is wrapped, errors come
//      back as protocol `error` frames (matching the existing frame_capi.cpp pattern).
//   7) No globals visible to the caller -- every concurrent client allocates its own ctx.
//      OpenBLAS thread-count is still process-global (see SnSession.h); the wire `session.open`
//      with mode=supernodal serialises supernodal calls inside the engine, not here.
//
// AUTHORITATIVE PROTOCOL
//   - Wire schema: docs/specs/S6b_rhino_bridge_v2.md
//   - Frame layout: magic 'FC' + flags u16 + headerLen u32 LE + payloadLen u32 LE + header JSON
//                   + optional raw binary payload (typically little-endian IEEE-754 doubles).
//   - Handshake: client's first frame MUST be kind="hello"; server replies with capabilities.
//
// PROFILES (spec § ⑭)
//   The same DLL serves BOTH the simple (UE5-friendly, silent defaults + silent fallbacks +
//   singular-as-flag) and the advanced (Rhino-official / academic; no silent defaults, no silent
//   fallbacks, singular-as-error, per-result advancedDiagnostics, opt-in diagnostic.stream)
//   profiles. The wire ABI is identical; the difference is in the session-open body:
//     { "profile": "simple" | "advanced", "diagnosticStream": bool, ... }
//   The dispatcher inside frame_capi_v2.dll routes silent paths differently per session profile.
//   Advanced profile requires the engine to advertise the "profile.advanced" capability in its
//   hello reply -- this lets older DLLs reject advanced opens cleanly.
//
// LIFETIME
//   - frame_v2_open() returns a heap-allocated ctx; the caller MUST pair it with frame_v2_close().
//   - One ctx == one logical client connection (independent hello state, independent session set).
//   - It is SAFE to keep many ctxs in the same process; OpenBLAS thread-count caveats apply
//     only when multiple ctxs simultaneously drive mode=supernodal sessions.
//
// THREAD SAFETY (RPC-pattern contract — clarified after P1.1 fix)
//   Per ctx, the following concurrency is GUARANTEED safe and is the expected SDK pattern
//   (matches gRPC / Tonic / async-RPC dispatcher loops in any modern client SDK):
//
//     * Multiple threads may call frame_v2_send concurrently. Inbound frames are dispatched
//       in lock-step internally — handlers never race against each other on Context state.
//     * One thread may sit in frame_v2_recv (blocking or polling) while OTHER threads call
//       frame_v2_send / frame_v2_cancel_request / frame_v2_cancel_recv concurrently. This is
//       the standard "one recv loop + many request producers" pattern; the C# SDK's
//       FrameSession depends on it.
//     * Concurrent frame_v2_cancel_request and frame_v2_cancel_recv calls are safe from any
//       thread.
//
//   What is NOT safe:
//     * Two overlapping frame_v2_recv calls on the same ctx. The Pop/serialize/cache state in
//       the impl is single-reader; a second recv concurrently is undefined. Use one dispatch
//       thread per ctx.
//     * frame_v2_close while any send/recv/cancel call on the same ctx is in flight. Wait for
//       all calls to return before closing.
//
//   Different ctxs may be driven concurrently in every combination, EXCEPT for the OpenBLAS
//   caveat above (mode=supernodal sessions share OpenBLAS thread count process-globally).
//
// BUILD
//   - Built by build_capi_v2.bat as frame_capi_v2.dll (mirror of build_capi.bat) -- planned
//     in B1; not yet shipped. This header is the contract; the .cpp comes in B2 alongside
//     the dispatcher.

#ifndef FRAME_CAPI_V2_H
#define FRAME_CAPI_V2_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
  #define FCAPI __declspec(dllexport)
#else
  #define FCAPI
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ----- ABI version --------------------------------------------------------------------------
//
// Returns the ABI generation. This header documents generation 2. A v3 DLL ships with
// frame_v2_abi_version()==3 BUT is REQUIRED to keep frame_v2_send/recv working for v=2
// callers for at least two minor versions after a v3 release (see spec § ⑤ SLA).
//
// Strictly monotone integer. Clients should check this before any other call and refuse
// to proceed if the value is lower than what they were compiled against.
FCAPI uint32_t frame_v2_abi_version(void);

// Engine build SHA (same static string semantics as the v1 frame_capi_version). Do not free.
FCAPI const char* frame_v2_build_sha(void);

// Engine version triple, "<major>.<minor>.<patch>" (e.g. "2.3.0"). Do not free.
FCAPI const char* frame_v2_engine_version(void);

// ----- Opaque context handle ----------------------------------------------------------------

struct frame_v2_ctx;
typedef struct frame_v2_ctx frame_v2_ctx;

// Allocate a new context. Returns NULL on out-of-memory. Each context maintains its own
// frame parser state, hello/capability cache, and session registry -- two contexts in the
// same process do not see each other's sessions.
//
// Implementation detail (informative): the impl uses a per-ctx mpsc queue between the inbound
// frame parser and the dispatcher worker, so frame_v2_send returns as soon as the frame is
// queued; frame_v2_recv pulls completed response/event frames out of the same queue.
FCAPI frame_v2_ctx* frame_v2_open(void);

// Release a context. After close() the pointer is dangling; callers must not reuse it.
FCAPI void frame_v2_close(frame_v2_ctx* ctx);

// ----- Result codes -------------------------------------------------------------------------

enum {
    FRAME_V2_OK              = 0,   // success
    FRAME_V2_EMPTY           = 1,   // recv had no frame ready and blockingMs == 0
    FRAME_V2_NEED_BIGGER     = 2,   // outCap too small; *outNeeded holds required size
    FRAME_V2_TIMEOUT         = 3,   // recv waited blockingMs but no frame arrived
    FRAME_V2_PROTOCOL_ERROR  = 4,   // malformed frame (bad magic/length/json)
    FRAME_V2_INVALID_CTX     = 5,   // ctx is NULL or already closed
    FRAME_V2_CANCELLED       = 6,   // recv interrupted by frame_v2_cancel_recv()
    FRAME_V2_OUT_OF_MEMORY   = 7,   // impl failed to allocate
    FRAME_V2_NOT_IMPLEMENTED = 8    // method is registered as capability but no impl yet
};

// ----- Frame transport ----------------------------------------------------------------------

// Send a single frame to the engine. inFrame points at the raw bytes of one frame, including
// magic + flags + header_len + payload_len + header_bytes + payload_bytes. The caller
// retains ownership of inFrame; the impl copies what it needs.
//
// Returns FRAME_V2_OK on success, FRAME_V2_PROTOCOL_ERROR on malformed framing or invalid
// JSON header, FRAME_V2_INVALID_CTX on a closed/null ctx, FRAME_V2_OUT_OF_MEMORY if the
// impl could not queue the frame.
//
// CURRENT IMPLEMENTATION (v2.4 + B3 follow-up, SYNCHRONOUS)
//   The dispatcher executes the handler on the caller's thread before frame_v2_send returns.
//   For short-running handlers (solve.linear / inspect.* / analysis.modal/buckling) the wait
//   is sub-millisecond. For long-running handlers (size_opt with many iterations, dyn_collapse
//   with many frames) the call blocks for the full handler runtime; clients that need a
//   responsive UI should drive frame_v2_send from a worker thread of their own.
//
// FUTURE (B4, planned -- see HANDOFF_v2.4.md § 4 C-06 / C-07)
//   A per-session worker thread will own the inbound queue, frame_v2_send will return as soon
//   as the frame is queued, and long-running handlers will stream `event` frames out via
//   frame_v2_recv. Until that lands, treat this entry point as a synchronous RPC -- the return
//   code reflects the handler's outcome too, not just the framing parse.
FCAPI int frame_v2_send(frame_v2_ctx* ctx, const uint8_t* inFrame, size_t inLen);

// Receive one frame from the engine into outBuf (capacity outCap).
//
//   blockingMs <  0  : block until a frame arrives.
//   blockingMs == 0  : non-blocking; returns FRAME_V2_EMPTY if nothing is ready.
//   blockingMs >  0  : wait up to blockingMs; returns FRAME_V2_TIMEOUT if the deadline hits.
//
// On FRAME_V2_OK *outLen is the number of bytes written into outBuf. On FRAME_V2_NEED_BIGGER
// *outNeeded is the size required; outBuf is not modified. The caller should re-allocate and
// retry. outNeeded / outLen may be NULL if the caller passes outCap=0 to probe the size --
// in that case outNeeded MUST be non-NULL.
//
// Frames arrive in send-order per request id but interleave across request ids (e.g. while a
// long-running solve.dyn_collapse stream is firing 'progress' events, other request ids may
// also fire). The client identifies the owner by the JSON header's "id" field.
//
// The END_OF_RESPONSE flag (bit 0 of the frame's FLAGS u16) is set on the FINAL frame of a
// given request id -- the client should treat that as the close-of-stream signal and stop
// collecting events for that id. (cancel acks count as a final frame too.)
FCAPI int frame_v2_recv(frame_v2_ctx* ctx,
                        uint8_t* outBuf, size_t outCap,
                        size_t* outLen, size_t* outNeeded,
                        int blockingMs);

// Wake any thread currently blocked in frame_v2_recv() on this ctx (it returns
// FRAME_V2_CANCELLED). Used by C# clients to interrupt the recv loop when the GH component
// is being disposed. Has no effect on in-flight requests inside the engine -- use the
// protocol-level "cancel" method to abort a long-running solve.
FCAPI int frame_v2_cancel_recv(frame_v2_ctx* ctx);

// Set an in-process tombstone for targetId. The next frame_v2_send carrying that id is
// rejected at dispatch with `error { code: "CANCELLED", message: "client cancelled before
// dispatch" }`. The tombstone is consumed on first match -- a subsequent send re-using the
// id is treated as a fresh request.
//
// NOTE on protocol semantics: this is a LOCAL shortcut, NOT the wire-level cancel. It does
// NOT enqueue a `cancel` REQUEST frame and does NOT produce a `cancel` RESPONSE frame on the
// outbound side. If the client wants the protocol-level cancel ack (so a peer logging the
// wire sees the cancellation), it should build and frame_v2_send a real request frame:
//     { v:2, kind:"request", id:<auto>, method:"cancel", body:{ targetId:"<targetId>" } }
// which goes through the dispatcher's HandleCancel and returns a normal response. Both paths
// set the same tombstone, so the cancellation effect on the targeted id is identical.
FCAPI int frame_v2_cancel_request(frame_v2_ctx* ctx, const char* targetId);

// ----- Diagnostics --------------------------------------------------------------------------

// Last protocol error in human-readable form, scoped to this ctx. Static-lifetime string,
// valid until the next call into the same ctx. NULL if no error has occurred. Useful for
// client log lines when frame_v2_send/recv returns a non-zero code without an "error" frame
// (e.g. malformed input that never reached the dispatcher).
FCAPI const char* frame_v2_last_error(const frame_v2_ctx* ctx);

// Number of frames currently buffered awaiting recv on this ctx. -1 on invalid ctx.
// Useful for GH components that want to drain fully before disposing.
FCAPI int frame_v2_pending_count(const frame_v2_ctx* ctx);

#ifdef __cplusplus
}  // extern "C"
#endif

// ----- Frame layout helpers (header-only, for clients in C++) ------------------------------
//
// The on-the-wire format is fixed and documented here in code so any C/C++ client can build
// frames without pulling in extra dependencies. A C# / Python client uses its own struct
// pack -- see FrameProtocol.cs in the C# SDK.

#define FRAME_V2_MAGIC0 0x46    /* 'F' */
#define FRAME_V2_MAGIC1 0x43    /* 'C' */

#define FRAME_V2_FLAG_END_OF_RESPONSE   (1u << 0)
#define FRAME_V2_FLAG_HAS_PAYLOAD       (1u << 1)
#define FRAME_V2_FLAG_BINARY_PAYLOAD    (1u << 2)

#define FRAME_V2_HEADER_FIXED_BYTES 12  /* magic(2) + flags(2) + headerLen(4) + payloadLen(4) */

#endif  /* FRAME_CAPI_V2_H */
