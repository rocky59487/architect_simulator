// Dispatcher.h -- v2 dispatcher contract. ONE entry point that every transport (C ABI,
// stdio, named pipe, TCP) shares; new methods are added once and become available everywhere.
//
// SCOPE
//   The dispatcher owns:
//     * a method-name -> handler map (string -> std::function),
//     * a session-id -> EngineSession registry (factor reuse handle),
//     * a connection-level Context (profile, build sha, capability set),
//     * a thread-safe inbound frame queue + outbound frame queue (used by frame_capi_v2 to
//       implement send/recv).
//   It deliberately does NOT do any I/O itself. The C ABI / stdio transport push framed bytes
//   in and pull framed bytes out.
//
// THREADING
//   One Dispatcher per logical client connection. send() is the only thread-safe entry; it
//   enqueues the inbound frame and either runs the handler synchronously on a worker pool or
//   inline (B2 ships inline; B4 switches DYNC / Modal to a worker).
//
// PROFILES
//   profile=simple (default): silent fills, silent fallbacks, singular-as-flag. Matches v1
//   behaviour exactly so the v2 round-trip gate (Tools/v2_roundtrip.py) can compare bit-for-bit
//   against frame_cli's stdout.
//   profile=advanced: every silent path is converted to an `error` frame; results carry an
//   `advancedDiagnostics` object built from the engine's per-stage trace.
//
// SCOPE OF B2 (stub level)
//   * hello handshake — implemented
//   * session.open / session.close / session.status — implemented (no-op session)
//   * model.set — accepts and validates the JSON shape; engine call is a TODO marker
//   * solve.linear — TODO marker; returns a stub response that the gate skip-paths past
//   * every other method — registered but returns NOT_IMPLEMENTED
//   * cancel — implemented (sets a per-id atomic flag)
//
// All TODO markers are explicit `dispatchTODO(...)` calls so a search for `dispatchTODO`
// returns the complete list of "B3+ wire this".

#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "FrameWire.h"
#include "MiniJson.h"

namespace frame_v2 {

#ifndef FRAMECORE_BUILD_SHA
#define FRAMECORE_BUILD_SHA "unknown"
#endif

inline constexpr uint32_t kAbiVersion   = 2;
inline constexpr const char* kEngineVer = "2.3.0";
inline constexpr const char* kSchemaVer = "2026.06";

enum class Profile { Simple, Advanced };

/// Per-engine-session state: factor handle, options snapshot, model fingerprint, defaults
/// fill tracker (simple profile populates this so client sees what was silently default-ed).
struct EngineSession {
    std::string  id;
    Profile      profile = Profile::Simple;
    bool         hasModel = false;
    uint64_t     modelFingerprint = 0;
    // Engine integration goes here in B3+; for B2 we just hold the metadata.
    // std::unique_ptr<frame::PreparedSystem>  prepared;
    // std::unique_ptr<frame::SnSession>       sn;
    // std::unique_ptr<frame::ReSolveSession>  resolve;
    std::vector<std::string> defaultsApplied;     // populated by model.set in simple profile
};

/// Connection-level context. One per client (one per ctx in C ABI v2).
struct Context {
    Profile      profile = Profile::Simple;
    bool         diagnosticStream = false;
    std::string  clientTag = "(unknown)";
    bool         helloSeen = false;
    std::unordered_map<std::string, std::shared_ptr<EngineSession>> sessions;
    std::unordered_set<std::string> cancelled;     // reqIds the client asked to cancel
    uint64_t     nextSessionSeq = 1;
};

/// Capability strings advertised in the hello reply.
///
/// P1.2 (second round) + P2.1 (third round) fix: a capability promise the engine cannot
/// actually fulfil is worse than no capability at all -- a client that checks HasCapability
/// would proceed and then receive a stub answer or NOT_IMPLEMENTED. We advertise ONLY methods
/// whose handler returns USEFUL data today:
///
///   * Connection-mgmt: session.open/close/status, cancel, profile selection — all WIRED.
///   * model.set: validates JSON shape end-to-end and emits defaultsApplied (simple) or
///     VALIDATION_FAILED (advanced). The engine-side FrameModel call is B3, but the
///     pre-engine semantics the client depends on (strict validation, defaults tracking)
///     are LIVE.
///   * solve.linear: shape-correct stub. Listed because the SDK needs SOMETHING to assert
///     "the solve verb exists"; the bit-exact-vs-v1 promise lands in B3 with no schema
///     change. The roundtrip gate's `[SKIP] solve.linear bit-exact vs v1` keeps this honest.
///
/// Removed from the advertised set in P2.1:
///   inspect.disp, inspect.member_forces, inspect.reactions, inspect.shell_forces — these
///   currently return empty payloads because there is no cached SolveResult yet (solve.linear
///   is a stub). Re-advertise in B3 once solve.linear is wired and the session caches a real
///   SolveResult.
///
/// Reserved for B3-B5 (NOT currently advertised):
///   inspect.* (B3, once solve.linear is wired) /
///   solve.pdelta / solve.tension_only / solve.size_opt / solve.dyn_collapse /
///   solve.corotational / solve.arclength / analysis.modal / analysis.buckling /
///   analysis.reanalysis_solve / model.patch / binary.modes / streaming / supernodal /
///   session.factor_reuse / dyn_collapse.full_frames / dyn_collapse.fragment_detail /
///   diagnostic.stream
inline std::vector<std::string> Capabilities() {
    return {
        // Connection-mgmt (WIRED).
        "cancel",
        "profile.advanced",
        "profile.simple",
        "session",
        // Model (validation WIRED; engine call B3).
        "model.set",
        // Solve verb exists; bit-exact-vs-v1 in B3.
        "solve.linear",
    };
}

/// Sentinel returned by handlers that have not been wired to the engine yet. The dispatcher
/// turns this into a structured `error { code: "NOT_IMPLEMENTED" }` frame so the client sees
/// a real protocol error instead of garbage.
inline Frame MakeError(const std::string& reqId, const std::string& code,
                       const std::string& message) {
    JsonObject body;
    body.emplace("code",     Json(code));
    body.emplace("severity", Json(std::string("request")));
    body.emplace("message",  Json(message));

    JsonObject hdr;
    hdr.emplace("v",    Json(static_cast<int64_t>(2)));
    hdr.emplace("kind", Json(std::string("error")));
    hdr.emplace("id",   Json(reqId));
    hdr.emplace("body", Json(std::move(body)));
    Frame f;
    f.flags  = kFlagEndOfResponse;
    f.header = Json(std::move(hdr));
    return f;
}

inline Frame MakeResponse(const std::string& reqId, JsonObject body, bool eor = true) {
    JsonObject hdr;
    hdr.emplace("v",    Json(static_cast<int64_t>(2)));
    hdr.emplace("kind", Json(std::string("response")));
    hdr.emplace("id",   Json(reqId));
    hdr.emplace("body", Json(std::move(body)));
    Frame f;
    f.flags  = eor ? kFlagEndOfResponse : 0;
    f.header = Json(std::move(hdr));
    return f;
}

inline Frame MakeEvent(const std::string& reqId, const std::string& channel, JsonObject details) {
    JsonObject body;
    body.emplace("channel", Json(channel));
    for (auto& kv : details) body.emplace(kv.first, std::move(kv.second));

    JsonObject hdr;
    hdr.emplace("v",    Json(static_cast<int64_t>(2)));
    hdr.emplace("kind", Json(std::string("event")));
    hdr.emplace("id",   Json(reqId));
    hdr.emplace("body", Json(std::move(body)));
    Frame f;
    f.flags  = 0;
    f.header = Json(std::move(hdr));
    return f;
}

/// One dispatcher per Context. Thread-safe on the public boundary.
class Dispatcher {
public:
    Dispatcher();

    /// Push one parsed inbound frame in. Runs the registered handler synchronously (B2);
    /// queues any output frame(s) onto the outbound queue. NEVER throws — handler errors
    /// produce an `error` frame.
    void Submit(Frame inbound);

    /// Pull one outbound frame, if any. Returns false when queue empty.
    bool TryPop(Frame& out);

    /// Cancel an in-flight reqId. The handler checks `IsCancelled(id)` and may emit an
    /// `error { code: "CANCELLED" }` early.
    void CancelRequest(const std::string& reqId);
    bool IsCancelled(const std::string& reqId) const;

    /// Snapshot of the connection state — useful for the C ABI's frame_v2_pending_count and
    /// frame_v2_last_error introspection.
    size_t PendingOutbound() const;
    std::string LastError() const;

private:
    using Handler = std::function<Frame(Dispatcher&, Context&, const Frame&)>;
    void Register(const std::string& method, Handler h) { handlers_[method] = std::move(h); }
    void EnqueueOutbound(Frame f);

    // ----- built-in handlers (Bridge/Dispatcher.cpp) -----
    static Frame HandleHello       (Dispatcher&, Context&, const Frame&);
    static Frame HandleSessionOpen (Dispatcher&, Context&, const Frame&);
    static Frame HandleSessionClose(Dispatcher&, Context&, const Frame&);
    static Frame HandleSessionStatus(Dispatcher&, Context&, const Frame&);
    static Frame HandleModelSet    (Dispatcher&, Context&, const Frame&);
    static Frame HandleSolveLinear (Dispatcher&, Context&, const Frame&);
    static Frame HandleInspectDisp (Dispatcher&, Context&, const Frame&);
    static Frame HandleCancel      (Dispatcher&, Context&, const Frame&);

    // ----- stubs (B3+ wires these to the engine) -----
    static Frame HandlePDelta       (Dispatcher&, Context&, const Frame&);
    static Frame HandleTensionOnly  (Dispatcher&, Context&, const Frame&);
    static Frame HandleSizeOpt      (Dispatcher&, Context&, const Frame&);
    static Frame HandleDynCollapse  (Dispatcher&, Context&, const Frame&);
    static Frame HandleCorotational (Dispatcher&, Context&, const Frame&);
    static Frame HandleArcLength    (Dispatcher&, Context&, const Frame&);
    static Frame HandleModal        (Dispatcher&, Context&, const Frame&);
    static Frame HandleBuckling     (Dispatcher&, Context&, const Frame&);
    static Frame HandleReanalysis   (Dispatcher&, Context&, const Frame&);
    static Frame HandleInspectMF    (Dispatcher&, Context&, const Frame&);
    static Frame HandleInspectRF    (Dispatcher&, Context&, const Frame&);
    static Frame HandleInspectSF    (Dispatcher&, Context&, const Frame&);
    static Frame HandleModelPatch   (Dispatcher&, Context&, const Frame&);

    std::unordered_map<std::string, Handler> handlers_;
    Context                                  ctx_;
    // Submit() serialises against concurrent inbound frames (P1.1 fix): the Context's
    // helloSeen / profile / sessions map are touched by handlers and CANNOT race with each
    // other. submitMtx_ wraps the whole dispatch so callers can fire frame_v2_send from
    // multiple threads safely.
    mutable std::mutex                       submitMtx_;
    // P1.3 fix: cancelled set has its OWN mutex so CancelRequest from any thread does not
    // block dispatch and vice-versa. ctx_.cancelled may only be accessed through CancelRequest
    // / IsCancelled, which both grab this mutex. Submit() reads cancelled via IsCancelled, so
    // it acquires cancelMtx_ briefly then releases — never held while dispatching a handler.
    mutable std::mutex                       cancelMtx_;
    mutable std::mutex                       outMtx_;
    std::deque<Frame>                        outbound_;
    mutable std::mutex                       errMtx_;
    std::string                              lastError_;
};

}  // namespace frame_v2
