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
//   One Dispatcher per logical client connection. The C ABI owns the inbound worker thread:
//   frame_v2_send queues parsed frames, a per-context worker calls Submit(), and recv() drains
//   response/event frames from the outbound queue.
//
// PROFILES
//   profile=simple (default): silent fills, silent fallbacks, singular-as-flag. Matches v1
//   behaviour exactly so the v2 round-trip gate (Tools/v2_roundtrip.py) can compare bit-for-bit
//   against frame_cli's stdout.
//   profile=advanced: every silent path is converted to an `error` frame; results carry an
//   `advancedDiagnostics` object built from the engine's per-stage trace.
//
// SCOPE OF B3 (v2.5: dispatcher engine-wired)
//   * hello handshake — wired
//   * session.open / session.close / session.status — wired (real EngineSession + factor cache)
//   * model.set — accepts JSON, builds frame::FrameModel via ModelBuilder, runs validate(),
//                 invalidates any cached factor; optionally eager-factors supernodal SnSession
//   * solve.linear — wired; assembleAndFactor + solveLoad (or SnSession::solveFrame in
//                    supernodal mode); bit-exact vs v1 frame_capi.dll on cantilever (rel<1e-11)
//   * inspect.{disp,reactions,member_forces,shell_forces} — wired; read cached SolveResult
//   * solve.pdelta / solve.tension_only / solve.size_opt / solve.dyn_collapse /
//     solve.corotational / solve.arclength / analysis.modal / analysis.buckling /
//     analysis.reanalysis_solve are wired; each calls the engine,
//     returns structured response, caches finalState for inspect.* re-reads
//   * cancel — wired (per-id atomic tombstone, consumed on first match)
//
// Still deferred (return NOT_IMPLEMENTED):
//   * model.patch — schema TBD

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

// Forward declarations so this header stays Eigen-free; the full FrameCore types only need to
// be complete in Dispatcher.cpp (where the engine handlers live).
namespace frame {
    struct FrameModel;
    struct PreparedSystem;
    struct SolveResult;
    class  SnSession;
}

namespace frame_v2 {

#ifndef FRAMECORE_BUILD_SHA
#define FRAMECORE_BUILD_SHA "unknown"
#endif

inline constexpr uint32_t kAbiVersion   = 2;
inline constexpr const char* kEngineVer = "2.5.0";
inline constexpr const char* kSchemaVer = "2026.06";

enum class Profile { Simple, Advanced };

/// Per-engine-session state: factor handle, options snapshot, model fingerprint, defaults
/// fill tracker (simple profile populates this so client sees what was silently default-ed).
struct EngineSession {
    EngineSession();
    ~EngineSession();                              // out-of-line so unique_ptr<incomplete> works
    EngineSession(const EngineSession&) = delete;
    EngineSession& operator=(const EngineSession&) = delete;

    std::string  id;
    Profile      profile = Profile::Simple;
    bool         hasModel = false;
    uint64_t     modelFingerprint = 0;
    // B3 wire: real engine state. unique_ptr keeps these forward-declared in this header so the
    // dispatcher transport stays Eigen-free; the .cpp side completes the types.
    std::unique_ptr<frame::FrameModel>     model;
    std::unique_ptr<frame::PreparedSystem> prepared;
    std::unique_ptr<frame::SolveResult>    lastSolve;        // last successful solve.linear cache
    // B5 (factor-reuse) — session.open with body.mode="supernodal" turns this on; model.set then
    // also builds a SnSession off the prepared system. solve.linear routes through sn->solveFrame
    // instead of re-factoring; the supernodal factor amortises across many solve.linear calls.
    bool         useSnSession = false;
    std::unique_ptr<frame::SnSession>      sn;
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
/// Advertise ONLY methods whose handler
/// returns useful data today. The analysis and inspect verbs below run real FrameCore code
/// and return spec-shape responses (or a structured engine error frame on failure).
///
/// Still NOT advertised here: model.patch (schema TBD), binary.modes,
/// dyn_collapse.fragment_detail, diagnostic.stream.
inline std::vector<std::string> Capabilities() {
    return {
        // Connection-mgmt (WIRED).
        "cancel",
        "profile.advanced",
        "profile.simple",
        "session",
        // Model (validation + engine FrameModel build both WIRED in v2.5).
        "model.set",
        // Linear solve verb — bit-exact vs v1 frame_capi.dll (rel<1e-11 on cantilever).
        "solve.linear",
        // B3 wired analyses (v2.5). Each returns a structured response with finalState +
        // convergence flags; client may follow up with inspect.* on the cached SolveResult.
        "solve.pdelta",
        "solve.tension_only",
        "solve.size_opt",
        "solve.dyn_collapse",
        "solve.corotational",
        "solve.arclength",
        "analysis.modal",
        "analysis.buckling",
        "analysis.reanalysis_solve",
        // Inspect family — reads the session's cached SolveResult (set by the most recent
        // solve.*); returns spec-shape payloads with NaN/Inf guard via finiteOrFail.
        "inspect.disp",
        "inspect.reactions",
        "inspect.member_forces",
        "inspect.shell_forces",
        // B4 transport signal: frame_v2_send queues parsed frames and returns; a per-context
        // worker thread drives handlers and frame_v2_recv drains responses/events.
        "transport.async",
        // P1-3 (v2.7): solve.dyn_collapse pushes each frame event WHILE runDynamicCollapse is
        // running, and honours a mid-run cancel request via the engine's isCancelled poll.
        // Without this capability the client should expect frames in a burst after the final
        // response (v2.6 behaviour).
        "dyn_collapse.live",
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

    /// Push one parsed inbound frame in. The C ABI calls this from its per-context worker;
    /// queues any output frame(s) onto the outbound queue. NEVER throws — handler errors
    /// produce an `error` frame.
    void Submit(Frame inbound);

    /// Pull one outbound frame, if any. Returns false when queue empty.
    bool TryPop(Frame& out);

    /// Cancel an in-flight reqId. The handler checks `IsCancelled(id)` and may emit an
    /// `error { code: "CANCELLED" }` early.
    void CancelRequest(const std::string& reqId);
    bool IsCancelled(const std::string& reqId) const;
    /// Erase a tombstone after it has been observed (Submit calls this once the CANCELLED
    /// response is queued, and again after every completed request). Keeps the cancelled set
    /// bounded to the in-flight working set instead of accumulating one entry per cancel ever
    /// issued -- important for long-running Rhino sessions that see thousands of slider-drag
    /// cancellations.
    void ClearCancelled(const std::string& reqId);

    /// Snapshot of the connection state — useful for the C ABI's frame_v2_pending_count and
    /// frame_v2_last_error introspection.
    size_t PendingOutbound() const;
    std::string LastError() const;

    /// P1-3 (v2.7): handlers running a long analysis call this to push an event frame onto
    /// the outbound queue WHILE the engine is still running, so the client sees live progress
    /// instead of a wall of frames after the engine returns. The internal queue has its own
    /// mutex; Submit() does not need to be held here. Used by HandleDynCollapse to forward
    /// each (u,v) snapshot the moment runDynamicCollapse emits it.
    void Emit(Frame f) { EnqueueOutbound(std::move(f)); }

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

    // ----- engine-backed analysis handlers -----
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
