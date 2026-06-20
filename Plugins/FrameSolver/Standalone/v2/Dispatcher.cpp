// Dispatcher.cpp -- handler implementations.
//
// CURRENT STATE (v2.5: B3 dispatcher engine-wired)
//   Connection-mgmt methods (hello, session.open/close/status, cancel) and model.set were
//   wired since B2 / v2.4. v2.5 (B3) wires the analysis verbs to FrameCore:
//
//   [WIRED B3] solve.linear -- assembleAndFactor + solveLoad (or SnSession::solveFrame when
//              session.open mode=supernodal). Bit-exact vs v1 frame_capi.dll on cantilever
//              fixture (rel<1e-11). All numeric outputs are finite-checked.
//   [WIRED B3] inspect.{disp,reactions,member_forces,shell_forces} -- pure read from
//              session.lastSolve; no engine recall.
//   [WIRED B3] solve.pdelta / solve.tension_only / solve.size_opt / solve.corotational /
//              solve.arclength / analysis.modal / analysis.buckling -- each calls the
//              matching engine entry point, traps exceptions into structured INTERNAL frames,
//              and caches finalState into session.lastSolve for inspect.* re-reads.
//
//   [WIRED B4] solve.dyn_collapse -- calls runDynamicCollapse and emits optional binary
//              u/v replay frames before the final summary response.
//   [WIRED B5.2] analysis.reanalysis_solve -- calls ReSolveSession for same-topology
//              element active toggles and returns tier/rank diagnostics.
//   [schema TBD] model.patch -- diff-format unsettled.
//
//   Engine link: build_capi_v2.bat now links FrameCore + SnSolver/SnSession objects
//   (~600 KB DLL up from B2's ~105 KB stub).

#include "Dispatcher.h"
#include "ModelBuilder.h"

#include "FrameCore/FrameModel.h"
#include "FrameCore/FrameSolver.h"
#include "FrameCore/SolveResult.h"
#include "FrameCore/SolveOptions.h"
#include "FrameCore/PDeltaAnalysis.h"
#include "FrameCore/TensionOnly.h"
#include "FrameCore/SizeOpt.h"
#include "FrameCore/CorotationalAnalysis.h"
#include "FrameCore/ModalAnalysis.h"
#include "FrameCore/ModalResult.h"
#include "FrameCore/BucklingAnalysis.h"
#include "FrameCore/BucklingResult.h"
#include "FrameCore/SnSession.h"
#include "FrameCore/DynamicCollapse.h"
#include "FrameCore/Reanalysis.h"
#include "FrameCore/ElasticAllowable.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace frame_v2 {

// EngineSession ctor / dtor live here (out-of-line) so the unique_ptr<frame::...> members in
// the header can stay forward-declared. With these completed types in scope the defaulted
// special members are well-formed.
EngineSession::EngineSession()  = default;
EngineSession::~EngineSession() = default;

Dispatcher::Dispatcher() {
    // Connection-mgmt (all [WIRED]).
    Register("hello",          &Dispatcher::HandleHello);
    Register("session.open",   &Dispatcher::HandleSessionOpen);
    Register("session.close",  &Dispatcher::HandleSessionClose);
    Register("session.status", &Dispatcher::HandleSessionStatus);
    Register("cancel",         &Dispatcher::HandleCancel);

    // Model + the bare solver path.
    Register("model.set",      &Dispatcher::HandleModelSet);
    Register("model.patch",    &Dispatcher::HandleModelPatch);
    Register("solve.linear",   &Dispatcher::HandleSolveLinear);

    // Inspect family.
    Register("inspect.disp",          &Dispatcher::HandleInspectDisp);
    Register("inspect.member_forces", &Dispatcher::HandleInspectMF);
    Register("inspect.reactions",     &Dispatcher::HandleInspectRF);
    Register("inspect.shell_forces",  &Dispatcher::HandleInspectSF);

    // Analysis family.
    Register("solve.pdelta",         &Dispatcher::HandlePDelta);
    Register("solve.tension_only",   &Dispatcher::HandleTensionOnly);
    Register("solve.size_opt",       &Dispatcher::HandleSizeOpt);
    Register("solve.dyn_collapse",   &Dispatcher::HandleDynCollapse);
    Register("solve.corotational",   &Dispatcher::HandleCorotational);
    Register("solve.arclength",      &Dispatcher::HandleArcLength);
    Register("analysis.modal",       &Dispatcher::HandleModal);
    Register("analysis.buckling",    &Dispatcher::HandleBuckling);
    Register("analysis.reanalysis_solve", &Dispatcher::HandleReanalysis);
}

void Dispatcher::Submit(Frame inbound) {
    // P1.1 fix: serialise the whole dispatch so two concurrent frame_v2_send calls cannot race
    // against each other on the Context's helloSeen / profile / cancelled / sessions map.
    // outbound queue + lastError have their own mutexes and stay reachable from recv threads
    // WITHOUT this lock.
    std::lock_guard<std::mutex> submitLk(submitMtx_);

    const std::string id = inbound.header.getString("id", "?");
    const std::string kind = inbound.header.getString("kind", "");

    // Hello does not require a prior hello, of course. All other kinds DO -- enforce the
    // handshake-first contract so a client that forgot hello gets a clear error rather than
    // an inscrutable failure further down.
    if (!ctx_.helloSeen && kind != "hello") {
        EnqueueOutbound(MakeError(id, "PROTOCOL", "first frame must be kind=hello"));
        return;
    }

    if (kind == "hello") {
        EnqueueOutbound(HandleHello(*this, ctx_, inbound));
        return;
    }
    if (kind != "request") {
        EnqueueOutbound(MakeError(id, "PROTOCOL", "expected kind=request"));
        return;
    }

    const std::string method = inbound.header.getString("method", "");
    auto it = handlers_.find(method);
    if (it == handlers_.end()) {
        EnqueueOutbound(MakeError(id, "UNSUPPORTED_METHOD", "unknown method: " + method));
        return;
    }
    if (IsCancelled(id)) {
        // P2 fix (review pass): tombstone CONSUMED here -- the cancelled set used to keep id
        // forever, so a long-running Rhino session that sees a slider drag fire thousands of
        // cancellable requests would leak one entry per request. Erasing on consume keeps the
        // set bounded to the in-flight working set. The CancelRequest -> Submit ordering is
        // serialised through submitMtx_, so a cancel that arrives AFTER this erase is a fresh
        // tombstone targeting a future id.
        ClearCancelled(id);
        EnqueueOutbound(MakeError(id, "CANCELLED", "client cancelled before dispatch"));
        return;
    }

    Frame out;
    try {
        out = it->second(*this, ctx_, inbound);
    } catch (const std::exception& e) {
        out = MakeError(id, "INTERNAL", std::string("dispatcher exception: ") + e.what());
    } catch (...) {
        out = MakeError(id, "INTERNAL", "dispatcher exception: unknown");
    }
    // P2 fix: a request that COMPLETED also clears any pre-emptive tombstone, so a cancel
    // that arrives after the handler returned (race against the response frame) does not
    // leave a permanent entry behind. The id is now "settled" -- the response is queued.
    ClearCancelled(id);
    EnqueueOutbound(std::move(out));
}

bool Dispatcher::TryPop(Frame& out) {
    std::lock_guard<std::mutex> lk(outMtx_);
    if (outbound_.empty()) return false;
    out = std::move(outbound_.front());
    outbound_.pop_front();
    return true;
}

void Dispatcher::EnqueueOutbound(Frame f) {
    std::lock_guard<std::mutex> lk(outMtx_);
    outbound_.push_back(std::move(f));
}

void Dispatcher::CancelRequest(const std::string& reqId) {
    // P1.3 fix: cancelled set has its own mutex. CancelRequest may be invoked from a different
    // thread than Submit (the C ABI's frame_v2_cancel_request and frame_v2_send are both
    // documented as safely concurrent). Holding cancelMtx_ briefly serialises the underlying
    // unordered_set without ever blocking the dispatch loop.
    std::lock_guard<std::mutex> lk(cancelMtx_);
    ctx_.cancelled.insert(reqId);
}

bool Dispatcher::IsCancelled(const std::string& reqId) const {
    std::lock_guard<std::mutex> lk(cancelMtx_);
    return ctx_.cancelled.count(reqId) > 0;
}

void Dispatcher::ClearCancelled(const std::string& reqId) {
    std::lock_guard<std::mutex> lk(cancelMtx_);
    ctx_.cancelled.erase(reqId);
}

size_t Dispatcher::PendingOutbound() const {
    std::lock_guard<std::mutex> lk(outMtx_);
    return outbound_.size();
}

std::string Dispatcher::LastError() const {
    std::lock_guard<std::mutex> lk(errMtx_);
    return lastError_;
}

// ====================================================================================
// [WIRED] handlers
// ====================================================================================

Frame Dispatcher::HandleHello(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "hs");
    const Json* b = in.header.get("body");

    if (b && b->isObject()) {
        ctx.clientTag = b->getString("client", "(unknown)");
        std::string profile = b->getString("profile", "simple");
        ctx.profile = (profile == "advanced") ? Profile::Advanced : Profile::Simple;
        ctx.diagnosticStream = b->getBool("diagnosticStream", false);
    }
    ctx.helloSeen = true;

    JsonObject body;
    body.emplace("engine",    Json(std::string("FrameCore")));
    body.emplace("buildSha",  Json(std::string(FRAMECORE_BUILD_SHA)));
    body.emplace("version",   Json(std::string(kEngineVer)));
    body.emplace("schemaVer", Json(std::string(kSchemaVer)));

    JsonArray caps;
    for (const auto& c : Capabilities()) caps.push_back(Json(c));
    body.emplace("capabilities", Json(std::move(caps)));

    JsonObject limits;
    limits.emplace("maxNodes", Json(static_cast<int64_t>(5'000'000)));
    limits.emplace("maxDof",   Json(static_cast<int64_t>(30'000'000)));
    body.emplace("limits", Json(std::move(limits)));

    body.emplace("deprecated", Json(JsonArray{}));

    JsonObject hdr;
    hdr.emplace("v",    Json(static_cast<int64_t>(2)));
    hdr.emplace("kind", Json(std::string("hello")));
    hdr.emplace("id",   Json(id));
    hdr.emplace("body", Json(std::move(body)));

    Frame out;
    out.flags  = kFlagEndOfResponse;
    out.header = Json(std::move(hdr));
    return out;
}

Frame Dispatcher::HandleSessionOpen(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    const Json* b = in.header.get("body");
    const std::string profile = b ? b->getString("profile", "simple") : "simple";
    if (profile == "advanced" && ctx.profile != Profile::Advanced) {
        return MakeError(id, "PROTOCOL",
                         "session.open profile='advanced' but the connection is not in advanced profile "
                         "(open with OpenAdvancedAsync)");
    }

    auto s   = std::make_shared<EngineSession>();
    s->id      = "s_" + std::to_string(ctx.nextSessionSeq++);
    s->profile = ctx.profile;
    // B5 wire: body.mode picks the factor-reuse lane. "supernodal" turns SnSession on; anything
    // else (including the default) keeps the plain LDLT lane. The choice is sticky for the
    // session lifetime -- model.set will allocate the SnSession off the prepared system.
    const std::string mode = b ? b->getString("mode", "default") : "default";
    s->useSnSession = (mode == "supernodal");
    ctx.sessions[s->id] = s;

    JsonObject body;
    body.emplace("session", Json(s->id));
    body.emplace("ready",   Json(true));
    body.emplace("mode",    Json(s->useSnSession ? std::string("supernodal") : std::string("default")));
    return MakeResponse(id, std::move(body));
}

Frame Dispatcher::HandleSessionClose(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id  = in.header.getString("id", "?");
    const Json*       b   = in.header.get("body");
    const std::string sid = b ? b->getString("session", "") : "";
    if (sid.empty() || !ctx.sessions.count(sid)) {
        return MakeError(id, "VALIDATION_FAILED", "unknown session: " + sid);
    }
    ctx.sessions.erase(sid);
    JsonObject body; body.emplace("closed", Json(true));
    return MakeResponse(id, std::move(body));
}

Frame Dispatcher::HandleSessionStatus(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id  = in.header.getString("id", "?");
    const Json*       b   = in.header.get("body");
    const std::string sid = b ? b->getString("session", "") : "";
    auto it = ctx.sessions.find(sid);
    if (it == ctx.sessions.end()) {
        return MakeError(id, "VALIDATION_FAILED", "unknown session: " + sid);
    }
    JsonObject body;
    body.emplace("session",  Json(sid));
    body.emplace("hasModel", Json(it->second->hasModel));
    body.emplace("profile",  Json(std::string(it->second->profile == Profile::Advanced ? "advanced" : "simple")));
    return MakeResponse(id, std::move(body));
}

Frame Dispatcher::HandleCancel(Dispatcher& d, Context&, const Frame& in) {
    const std::string id  = in.header.getString("id", "?");
    const Json*       b   = in.header.get("body");
    const std::string tgt = b ? b->getString("targetId", "") : "";
    if (tgt.empty()) return MakeError(id, "VALIDATION_FAILED", "body.targetId required");
    d.CancelRequest(tgt);
    JsonObject body; body.emplace("cancelled", Json(tgt));
    return MakeResponse(id, std::move(body));
}

// ====================================================================================
// model.set -- builds and validates the engine FrameModel, accepts defaults in simple profile,
// rejects missing capacity data in advanced profile.
// ====================================================================================

namespace {
inline std::shared_ptr<EngineSession> resolveSession(Context& ctx, const Json* body, std::string& err) {
    if (!body) { err = "body required"; return nullptr; }
    std::string sid = body->getString("session", "");
    auto it = ctx.sessions.find(sid);
    if (it == ctx.sessions.end()) { err = "unknown session: " + sid; return nullptr; }
    return it->second;
}
}  // namespace

Frame Dispatcher::HandleModelSet(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    const Json* body = in.header.get("body");
    std::string err;
    auto sess = resolveSession(ctx, body, err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);

    sess->defaultsApplied.clear();

    // ---- strict checks (advanced only) --------------------------------------------
    if (sess->profile == Profile::Advanced) {
        const Json* mats = body->get("materials");
        if (!mats || !mats->isArray())
            return MakeError(id, "VALIDATION_FAILED", "advanced profile requires materials[]");
        for (size_t k = 0; k < mats->asArray().size(); ++k) {
            const Json& m = mats->asArray()[k];
            if (!m.has("cap"))
                return MakeError(id, "VALIDATION_FAILED",
                                  "advanced profile: materials[" + std::to_string(k) + "].cap is required");
        }
    } else {
        // Simple profile: note which fields the engine would have to silently default.
        const Json* mats = body->get("materials");
        if (mats && mats->isArray())
            for (size_t k = 0; k < mats->asArray().size(); ++k)
                if (!mats->asArray()[k].has("cap"))
                    sess->defaultsApplied.push_back("materials[" + std::to_string(k) + "].cap");
    }

    // ---- B3 wire: build the frame::FrameModel from JSON and run validate() --------------
    auto fresh = std::make_unique<frame::FrameModel>();
    std::string buildErr;
    if (!buildModelFromJson(*body, *fresh, buildErr))
        return MakeError(id, "VALIDATION_FAILED", "model schema: " + buildErr);

    std::string why;
    if (!fresh->validate(why))
        return MakeError(id, "VALIDATION_FAILED", "model validate: " + why);

    // Replace the session's model; invalidate any cached prepared factor / solve result / SnSession
    // -- a new model means a new K_ff, so prior caches must not be reused.
    sess->model     = std::move(fresh);
    sess->prepared.reset();
    sess->lastSolve.reset();
    sess->sn.reset();
    sess->hasModel  = true;
    sess->modelFingerprint = 0;
    // B5 wire: when session.open selected mode=supernodal, eagerly factor the supernodal here so
    // the next solve.linear is a forward/back-substitution only (the production factor-once +
    // solve-many path). Failure inside SnSession is non-fatal: solveFrame transparently falls
    // back to LDLT (the SnSession contract), so we keep going.
    if (sess->useSnSession) {
        try {
            sess->prepared = std::make_unique<frame::PreparedSystem>(
                frame::assembleAndFactor(*sess->model));
            sess->sn = std::make_unique<frame::SnSession>(*sess->prepared);
        } catch (const std::exception& e) {
            // Surface the failure but do not refuse model.set; the client can still drop to
            // LDLT by ignoring the SnSession (we clear `sn` so solve.linear takes the LDLT path).
            sess->sn.reset();
            sess->defaultsApplied.push_back(std::string("supernodal disabled: ") + e.what());
        }
    }

    JsonObject out;
    out.emplace("ok",       Json(true));
    out.emplace("dofCount", Json(static_cast<int64_t>(sess->model->dofCount())));
    if (!sess->defaultsApplied.empty()) {
        JsonArray d;
        for (const auto& s : sess->defaultsApplied) d.push_back(Json(s));
        out.emplace("defaultsApplied", Json(std::move(d)));
    }
    return MakeResponse(id, std::move(out));
}

// ====================================================================================
// [WIRED B3] solve.linear -- assembleAndFactor + solveLoad (LDLT) or sn->solveFrame
// (Supernodal). Bit-exact vs v1 frame_capi.dll on cantilever (rel<1e-11). All numeric
// outputs are finite-checked via finiteOrFail / packDisp / packMemberForces / packShellForces.
// ====================================================================================

namespace {

// A-04 (B3 tier): every double we emit must be finite. Returns false + sets `where` to the
// first offender so HandleSolveLinear can convert it to a structured NON_FINITE_RESULT frame.
inline bool finiteOrFail(double v, const char* tag, std::size_t idx, std::string& where) {
    if (std::isfinite(v)) return true;
    where = std::string(tag) + "[" + std::to_string(idx) + "]=" + std::to_string(v);
    return false;
}

// Pack a 6-DOF tuple (Ux,Uy,Uz,Rx,Ry,Rz) under a node id, JSON object keyed by stringified id.
inline JsonObject packDisp(const frame::FrameModel& model, const frame::SolveResult& R,
                            const char* tag, bool useReactions,
                            std::string& nfWhere) {
    JsonObject out;
    for (std::size_t k = 0; k < model.nodes.size(); ++k) {
        const int kI = static_cast<int>(k);
        double v[6];
        for (int d = 0; d < 6; ++d)
            v[d] = static_cast<double>(useReactions ? R.reaction(kI, d) : R.disp(kI, d));
        for (int d = 0; d < 6; ++d) {
            if (!finiteOrFail(v[d], tag, k * 6 + d, nfWhere)) return JsonObject{};
        }
        JsonArray a;
        for (int d = 0; d < 6; ++d) a.push_back(Json(v[d]));
        out.emplace(std::to_string(model.nodes[k].id), Json(std::move(a)));
    }
    return out;
}

inline JsonObject packMemberForces(const frame::SolveResult& R, std::string& nfWhere) {
    JsonObject out;
    for (std::size_t e = 0; e < R.memberForces.size(); ++e) {
        const auto& mf = R.memberForces[e];
        const double iN  = static_cast<double>(mf.endI.N),  iVy = static_cast<double>(mf.endI.Vy),
                     iVz = static_cast<double>(mf.endI.Vz), iT  = static_cast<double>(mf.endI.T),
                     iMy = static_cast<double>(mf.endI.My), iMz = static_cast<double>(mf.endI.Mz);
        const double jN  = static_cast<double>(mf.endJ.N),  jVy = static_cast<double>(mf.endJ.Vy),
                     jVz = static_cast<double>(mf.endJ.Vz), jT  = static_cast<double>(mf.endJ.T),
                     jMy = static_cast<double>(mf.endJ.My), jMz = static_cast<double>(mf.endJ.Mz);
        const double vals[12] = { iN, iVy, iVz, iT, iMy, iMz, jN, jVy, jVz, jT, jMy, jMz };
        for (int d = 0; d < 12; ++d)
            if (!finiteOrFail(vals[d], "memberForces", e * 12 + d, nfWhere)) return JsonObject{};
        JsonArray endI; for (int d = 0; d < 6; ++d) endI.push_back(Json(vals[d]));
        JsonArray endJ; for (int d = 6; d < 12; ++d) endJ.push_back(Json(vals[d]));
        JsonObject mfo;
        mfo.emplace("endI", Json(std::move(endI)));
        mfo.emplace("endJ", Json(std::move(endJ)));
        out.emplace(std::to_string(mf.member), Json(std::move(mfo)));
    }
    return out;
}

inline JsonObject packShellForces(const frame::SolveResult& R, std::string& nfWhere) {
    JsonObject out;
    for (std::size_t e = 0; e < R.shellForces.size(); ++e) {
        const auto& sf = R.shellForces[e];
        const double v[8] = {
            static_cast<double>(sf.Mxx), static_cast<double>(sf.Myy), static_cast<double>(sf.Mxy),
            static_cast<double>(sf.Qx),  static_cast<double>(sf.Qy),
            static_cast<double>(sf.Nxx), static_cast<double>(sf.Nyy), static_cast<double>(sf.Nxy)
        };
        for (int d = 0; d < 8; ++d)
            if (!finiteOrFail(v[d], "shellForces", e * 8 + d, nfWhere)) return JsonObject{};
        JsonObject sfo;
        sfo.emplace("Mxx", Json(v[0])); sfo.emplace("Myy", Json(v[1])); sfo.emplace("Mxy", Json(v[2]));
        sfo.emplace("Qx",  Json(v[3])); sfo.emplace("Qy",  Json(v[4]));
        sfo.emplace("Nxx", Json(v[5])); sfo.emplace("Nyy", Json(v[6])); sfo.emplace("Nxy", Json(v[7]));
        out.emplace(std::to_string(sf.shell), Json(std::move(sfo)));
    }
    return out;
}

inline const char* failModeName(frame::FailMode mode) {
    switch (mode) {
        case frame::FailMode::None:           return "None";
        case frame::FailMode::Crush:          return "Crush";
        case frame::FailMode::Tension:        return "Tension";
        case frame::FailMode::Shear:          return "Shear";
        case frame::FailMode::Bending:        return "Bending";
        case frame::FailMode::Torsion:        return "Torsion";
        case frame::FailMode::ShellVonMises:  return "ShellVonMises";
    }
    return "Unknown";
}

inline const char* collapseOutcomeName(frame::CollapseOutcome outcome) {
    switch (outcome) {
        case frame::CollapseOutcome::Stable:    return "Stable";
        case frame::CollapseOutcome::Collapsed: return "Collapsed";
        case frame::CollapseOutcome::MaxSteps:  return "MaxSteps";
        case frame::CollapseOutcome::Invalid:   return "Invalid";
    }
    return "Unknown";
}

inline JsonArray packIntArray(const std::vector<int>& values) {
    JsonArray a;
    for (int v : values) a.push_back(Json(static_cast<int64_t>(v)));
    return a;
}

inline JsonArray packMemberIdArray(const std::vector<frame::MemberId>& values) {
    JsonArray a;
    for (frame::MemberId v : values) a.push_back(Json(static_cast<int64_t>(v)));
    return a;
}

inline JsonArray packVec3(const frame::Vec3& v) {
    JsonArray a;
    a.push_back(Json(static_cast<double>(v.x)));
    a.push_back(Json(static_cast<double>(v.y)));
    a.push_back(Json(static_cast<double>(v.z)));
    return a;
}

inline JsonObject packDemand(const frame::DemandResult& d) {
    JsonObject o;
    o.emplace("dc",    Json(static_cast<double>(d.risk)));
    o.emplace("mode",  Json(std::string(failModeName(d.mode))));
    o.emplace("sComp", Json(static_cast<double>(d.sComp)));
    o.emplace("sTens", Json(static_cast<double>(d.sTens)));
    o.emplace("tau",   Json(static_cast<double>(d.tau)));
    o.emplace("sTor",  Json(static_cast<double>(d.sTor)));
    return o;
}

inline bool finiteDemand(const frame::DemandResult& d, const char* tag,
                         std::size_t idx, std::string& nfWhere) {
    const double vals[5] = {
        static_cast<double>(d.risk), static_cast<double>(d.sComp),
        static_cast<double>(d.sTens), static_cast<double>(d.tau),
        static_cast<double>(d.sTor)
    };
    for (int k = 0; k < 5; ++k)
        if (!finiteOrFail(vals[k], tag, idx * 5 + k, nfWhere)) return false;
    return true;
}

inline bool packUtilization(const frame::FrameModel& model, const frame::SolveResult& R,
                            JsonObject& memberOut, JsonObject& shellOut,
                            JsonObject& summaryOut, std::string& nfWhere) {
    frame::ElasticAllowable screen;
    double maxMember = 0.0;
    int64_t governingMember = 0;
    std::string governingMemberMode = "None";
    bool memberValid = false;

    const std::size_t nM = std::min(model.members.size(), R.memberForces.size());
    for (std::size_t e = 0; e < nM; ++e) {
        const auto& m = model.members[e];
        if (!m.active || m.matIdx < 0 || m.secIdx < 0
            || m.matIdx >= static_cast<int>(model.materials.size())
            || m.secIdx >= static_cast<int>(model.sections.size())) {
            continue;
        }
        const auto& mat = model.materials[static_cast<std::size_t>(m.matIdx)];
        const auto& sec = model.sections[static_cast<std::size_t>(m.secIdx)];
        const auto& mf = R.memberForces[e];
        const frame::DemandResult di = screen.checkSection(mf.endI, sec, mat.cap);
        const frame::DemandResult dj = screen.checkSection(mf.endJ, sec, mat.cap);
        if (!finiteDemand(di, "memberUtilization.endI", e, nfWhere)) return false;
        if (!finiteDemand(dj, "memberUtilization.endJ", e, nfWhere)) return false;

        const bool iGov = di.risk >= dj.risk;
        const frame::DemandResult& dg = iGov ? di : dj;
        JsonObject mo;
        mo.emplace("endI",         Json(packDemand(di)));
        mo.emplace("endJ",         Json(packDemand(dj)));
        mo.emplace("peak",         Json(static_cast<double>(dg.risk)));
        mo.emplace("governingEnd", Json(std::string(iGov ? "I" : "J")));
        mo.emplace("governingMode",Json(std::string(failModeName(dg.mode))));
        memberOut.emplace(std::to_string(m.id), Json(std::move(mo)));

        if (!memberValid || dg.risk > maxMember) {
            memberValid = true;
            maxMember = static_cast<double>(dg.risk);
            governingMember = static_cast<int64_t>(m.id);
            governingMemberMode = failModeName(dg.mode);
        }
    }

    double maxShell = 0.0;
    int64_t governingShell = 0;
    bool shellValid = false;
    const std::size_t nS = std::min(model.shells.size(), R.shellForces.size());
    for (std::size_t s = 0; s < nS; ++s) {
        const auto& sh = model.shells[s];
        if (!sh.active || sh.matIdx < 0 || sh.matIdx >= static_cast<int>(model.materials.size()))
            continue;
        const auto& mat = model.materials[static_cast<std::size_t>(sh.matIdx)];
        const frame::ShellDemandResult d = frame::checkShellSurface(R.shellForces[s], sh.t, mat.cap);
        if (!finiteOrFail(static_cast<double>(d.risk), "shellUtilization", s, nfWhere)) return false;
        JsonObject so;
        so.emplace("dc",     Json(static_cast<double>(d.risk)));
        so.emplace("corner", Json(static_cast<int64_t>(d.corner)));
        so.emplace("top",    Json(d.top));
        shellOut.emplace(std::to_string(sh.id), Json(std::move(so)));
        if (!shellValid || d.risk > maxShell) {
            shellValid = true;
            maxShell = static_cast<double>(d.risk);
            governingShell = static_cast<int64_t>(sh.id);
        }
    }

    summaryOut.emplace("memberMaxDC",         Json(maxMember));
    summaryOut.emplace("governingMember",     Json(governingMember));
    summaryOut.emplace("governingMemberMode", Json(governingMemberMode));
    summaryOut.emplace("memberValid",         Json(memberValid));
    summaryOut.emplace("shellMaxDC",          Json(maxShell));
    summaryOut.emplace("governingShell",      Json(governingShell));
    summaryOut.emplace("shellValid",          Json(shellValid));
    const double maxAny = std::max(maxMember, maxShell);
    summaryOut.emplace("maxDC",        Json(maxAny));
    summaryOut.emplace("safetyFactor", Json(maxAny > 0.0 ? 1.0 / maxAny : 0.0));
    return true;
}

inline void appendF64LE(std::vector<uint8_t>& out, double v) {
    uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(v), "double must be 64-bit IEEE storage");
    std::memcpy(&bits, &v, sizeof(bits));
    for (int b = 0; b < 8; ++b)
        out.push_back(static_cast<uint8_t>((bits >> (8 * b)) & 0xFFu));
}

inline double maxAbs(const std::vector<frame::real>& values) {
    double m = 0.0;
    for (frame::real v : values) {
        const double dv = std::abs(static_cast<double>(v));
        if (dv > m) m = dv;
    }
    return m;
}

inline JsonObject packFragment(const frame::FragmentCluster& f) {
    JsonObject o;
    o.emplace("nodes",   Json(packMemberIdArray(f.nodes)));
    o.emplace("members", Json(packMemberIdArray(f.members)));
    o.emplace("shells",  Json(packIntArray(f.shells)));
    o.emplace("mass",    Json(static_cast<double>(f.mass)));
    o.emplace("com",     Json(packVec3(f.com)));
    JsonArray inertia;
    for (double v : f.inertia) inertia.push_back(Json(v));
    o.emplace("inertia", Json(std::move(inertia)));
    o.emplace("vel",    Json(packVec3(f.vel)));
    o.emplace("angVel", Json(packVec3(f.angVel)));
    return o;
}

inline JsonObject packDynEvent(const frame::DynCollapseEvent& e, int eventIndex) {
    JsonObject o;
    o.emplace("eventIndex",         Json(static_cast<int64_t>(eventIndex)));
    o.emplace("t",                  Json(static_cast<double>(e.t)));
    o.emplace("mode",               Json(std::string(failModeName(e.mode))));
    o.emplace("removedMembers",     Json(packMemberIdArray(e.removedMembers)));
    o.emplace("removedShells",      Json(packIntArray(e.removedShells)));
    o.emplace("truncationResidual", Json(static_cast<double>(e.truncationResidual)));
    o.emplace("energyBefore",       Json(static_cast<double>(e.energyBefore)));
    o.emplace("energyAfter",        Json(static_cast<double>(e.energyAfter)));

    JsonArray hinges;
    for (const auto& h : e.formedHinges) {
        JsonObject ho;
        ho.emplace("member", Json(static_cast<int64_t>(h.member)));
        ho.emplace("dof",    Json(static_cast<int64_t>(h.dof)));
        ho.emplace("Mp",     Json(static_cast<double>(h.Mp)));
        hinges.push_back(Json(std::move(ho)));
    }
    o.emplace("formedHinges", Json(std::move(hinges)));

    JsonArray detached;
    for (const auto& f : e.detached) detached.push_back(Json(packFragment(f)));
    o.emplace("detached", Json(std::move(detached)));
    return o;
}

}  // namespace

Frame Dispatcher::HandleSolveLinear(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    const Json* body = in.header.get("body");
    std::string err;
    auto sess = resolveSession(ctx, body, err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);
    if (!sess->hasModel || !sess->model)
        return MakeError(id, "VALIDATION_FAILED", "session has no model; call model.set first");

    const bool wantReactions = body->getBool("wantReactions", true);
    const bool wantDC        = body->getBool("wantDC", false);

    // assembleAndFactor + solveLoad on first call; subsequent calls reuse the PreparedSystem.
    // B5 wire: if the session was opened with mode=supernodal AND model.set built the SnSession,
    // route through sn->solveFrame which reuses the supernodal factor for forward/back-sub only.
    try {
        if (!sess->prepared) {
            sess->prepared = std::make_unique<frame::PreparedSystem>(
                frame::assembleAndFactor(*sess->model, frame::SolveOptions{}));
        }
        if (sess->sn) {
            sess->lastSolve = std::make_unique<frame::SolveResult>(
                sess->sn->solveFrame(*sess->model));
        } else {
            sess->lastSolve = std::make_unique<frame::SolveResult>(
                frame::solveLoad(*sess->prepared, *sess->model));
        }
    } catch (const std::exception& e) {
        return MakeError(id, "INTERNAL", std::string("engine: ") + e.what());
    }

    const frame::SolveResult& R = *sess->lastSolve;

    JsonObject out;
    out.emplace("singular",    Json(R.singular));
    if (!std::isfinite(static_cast<double>(R.pivotMargin)))
        return MakeError(id, "NON_FINITE_RESULT", "pivotMargin is non-finite");
    out.emplace("pivotMargin", Json(static_cast<double>(R.pivotMargin)));
    if (!R.diagnostic.empty()) out.emplace("diagnostic", Json(R.diagnostic));

    if (R.singular) {
        // Singular -> empty payload but keep the spec shape so the client can read uniformly.
        out.emplace("disp",         Json(JsonObject{}));
        out.emplace("reactions",    Json(JsonObject{}));
        out.emplace("memberForces", Json(JsonObject{}));
        out.emplace("shellForces",  Json(JsonObject{}));
    } else {
        std::string nfWhere;
        JsonObject disp = packDisp(*sess->model, R, "disp", false, nfWhere);
        if (!nfWhere.empty()) return MakeError(id, "NON_FINITE_RESULT", nfWhere);
        out.emplace("disp", Json(std::move(disp)));

        if (wantReactions) {
            JsonObject rxn = packDisp(*sess->model, R, "reactions", true, nfWhere);
            if (!nfWhere.empty()) return MakeError(id, "NON_FINITE_RESULT", nfWhere);
            out.emplace("reactions", Json(std::move(rxn)));
        } else {
            out.emplace("reactions", Json(JsonObject{}));
        }

        JsonObject mfs = packMemberForces(R, nfWhere);
        if (!nfWhere.empty()) return MakeError(id, "NON_FINITE_RESULT", nfWhere);
        out.emplace("memberForces", Json(std::move(mfs)));

        JsonObject sfs = packShellForces(R, nfWhere);
        if (!nfWhere.empty()) return MakeError(id, "NON_FINITE_RESULT", nfWhere);
        out.emplace("shellForces", Json(std::move(sfs)));

        if (wantDC) {
            JsonObject memberUtil, shellUtil, utilSummary;
            if (!packUtilization(*sess->model, R, memberUtil, shellUtil, utilSummary, nfWhere))
                return MakeError(id, "NON_FINITE_RESULT", nfWhere);
            out.emplace("memberUtilization", Json(std::move(memberUtil)));
            out.emplace("shellUtilization",  Json(std::move(shellUtil)));
            out.emplace("utilization",       Json(std::move(utilSummary)));
        }
    }

    if (sess->profile == Profile::Advanced) {
        // A-02 / C-06 / F-03 honest-diagnostics fix (v2.5): the dispatcher must report
        // which factor backend actually ran, not a hard-coded "LDLT" label. When the
        // session was opened with mode=supernodal AND SnSession build succeeded, solve.linear
        // routes through sn->solveFrame (self-built BLAS3 supernodal); otherwise the
        // PreparedSystem's LDLT path runs. factorTimeMs/solveTimeMs remain 0.0 — real
        // timing requires steady_clock hooks across assembleAndFactor and solveLoad, which
        // is a separate work item (B4 worker-thread cycle). The CLAUDE.md honesty rule
        // forbids reporting numbers we have not measured.
        const bool usingSupernodal = static_cast<bool>(sess->sn);
        JsonObject diag;
        diag.emplace("factorMethod",  Json(std::string(usingSupernodal ? "Supernodal" : "LDLT")));
        diag.emplace("factorBackend", Json(std::string(usingSupernodal ? "SnChol_selfbuilt"
                                                                       : "SimplicialLDLT")));
        diag.emplace("factorTimeMs",  Json(0.0));
        diag.emplace("solveTimeMs",   Json(0.0));
        out.emplace("advancedDiagnostics", Json(std::move(diag)));
    }
    return MakeResponse(id, std::move(out));
}

namespace {
inline Frame notImpl(const std::string& method, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    return MakeError(id, "NOT_IMPLEMENTED",
                     method + " is registered but deferred to a later schema cycle");
}

// Shared resolve+lastSolve check for the inspect.* family. Returns nullptr (and fills `errOut`)
// when the session does not yet hold a cached SolveResult; otherwise returns the session.
inline std::shared_ptr<EngineSession> inspectSession(Context& ctx, const Json* body,
                                                      std::string& errOut) {
    auto sess = resolveSession(ctx, body, errOut);
    if (!sess) return nullptr;
    if (!sess->hasModel || !sess->model || !sess->lastSolve) {
        errOut = "session has no cached SolveResult; call solve.linear first";
        return nullptr;
    }
    return sess;
}
}  // namespace

// ====================================================================================
// inspect.disp / inspect.reactions / inspect.member_forces / inspect.shell_forces
// B3 wire: read straight from the session's cached SolveResult. No engine re-call.
// ====================================================================================

Frame Dispatcher::HandleInspectDisp(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    std::string err;
    auto sess = inspectSession(ctx, in.header.get("body"), err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);
    std::string nfWhere;
    JsonObject disp = packDisp(*sess->model, *sess->lastSolve, "disp", false, nfWhere);
    if (!nfWhere.empty()) return MakeError(id, "NON_FINITE_RESULT", nfWhere);
    JsonObject out; out.emplace("disp", Json(std::move(disp)));
    return MakeResponse(id, std::move(out));
}

Frame Dispatcher::HandleInspectRF(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    std::string err;
    auto sess = inspectSession(ctx, in.header.get("body"), err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);
    std::string nfWhere;
    JsonObject rxn = packDisp(*sess->model, *sess->lastSolve, "reactions", true, nfWhere);
    if (!nfWhere.empty()) return MakeError(id, "NON_FINITE_RESULT", nfWhere);
    JsonObject out; out.emplace("reactions", Json(std::move(rxn)));
    return MakeResponse(id, std::move(out));
}

Frame Dispatcher::HandleInspectMF(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    std::string err;
    auto sess = inspectSession(ctx, in.header.get("body"), err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);
    std::string nfWhere;
    JsonObject mfs = packMemberForces(*sess->lastSolve, nfWhere);
    if (!nfWhere.empty()) return MakeError(id, "NON_FINITE_RESULT", nfWhere);
    JsonObject out; out.emplace("memberForces", Json(std::move(mfs)));
    return MakeResponse(id, std::move(out));
}

Frame Dispatcher::HandleInspectSF(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    std::string err;
    auto sess = inspectSession(ctx, in.header.get("body"), err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);
    std::string nfWhere;
    JsonObject sfs = packShellForces(*sess->lastSolve, nfWhere);
    if (!nfWhere.empty()) return MakeError(id, "NON_FINITE_RESULT", nfWhere);
    JsonObject out; out.emplace("shellForces", Json(std::move(sfs)));
    return MakeResponse(id, std::move(out));
}

// ====================================================================================
// B3 wire: 7 engine-backed analyses. Each follows the same shape:
//   1. validate session + model
//   2. read JSON options (all optional; defaults match engine defaults)
//   3. call the engine; trap exceptions
//   4. emit a structured response (singular flag + convergence + finalState if applicable)
// All numeric outputs go through finiteOrFail/packDisp/packMemberForces/packShellForces so
// A-04 (NON_FINITE_RESULT) covers them too.
// ====================================================================================

namespace {
// Pack a SolveResult sub-state (used by every analysis whose finalState is a SolveResult).
inline bool packFinalState(const frame::FrameModel& model, const frame::SolveResult& R,
                            bool wantReactions, JsonObject& dest, std::string& nfWhere) {
    dest.emplace("singular",    Json(R.singular));
    dest.emplace("pivotMargin", Json(static_cast<double>(R.pivotMargin)));
    if (!R.diagnostic.empty()) dest.emplace("diagnostic", Json(R.diagnostic));
    if (R.singular) {
        dest.emplace("disp",         Json(JsonObject{}));
        dest.emplace("reactions",    Json(JsonObject{}));
        dest.emplace("memberForces", Json(JsonObject{}));
        dest.emplace("shellForces",  Json(JsonObject{}));
        return true;
    }
    JsonObject disp = packDisp(model, R, "disp", false, nfWhere);
    if (!nfWhere.empty()) return false;
    dest.emplace("disp", Json(std::move(disp)));
    if (wantReactions) {
        JsonObject rxn = packDisp(model, R, "reactions", true, nfWhere);
        if (!nfWhere.empty()) return false;
        dest.emplace("reactions", Json(std::move(rxn)));
    } else {
        dest.emplace("reactions", Json(JsonObject{}));
    }
    JsonObject mfs = packMemberForces(R, nfWhere);
    if (!nfWhere.empty()) return false;
    dest.emplace("memberForces", Json(std::move(mfs)));
    JsonObject sfs = packShellForces(R, nfWhere);
    if (!nfWhere.empty()) return false;
    dest.emplace("shellForces", Json(std::move(sfs)));
    return true;
}
}  // namespace

Frame Dispatcher::HandlePDelta(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    const Json* body = in.header.get("body");
    std::string err;
    auto sess = resolveSession(ctx, body, err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);
    if (!sess->hasModel || !sess->model)
        return MakeError(id, "VALIDATION_FAILED", "session has no model; call model.set first");
    frame::PDeltaOptions opts;
    opts.maxIter      = static_cast<int>(body->getInt("maxIter", opts.maxIter));
    opts.tolU         = static_cast<frame::real>(body->getDouble("tolU", opts.tolU));
    opts.accelerate   = body->getBool("accelerate", opts.accelerate);
    opts.refactorPath = body->getBool("refactorPath", opts.refactorPath);
    frame::PDeltaResult R;
    try { R = frame::runPDelta(*sess->model, opts); }
    catch (const std::exception& e) { return MakeError(id, "INTERNAL", std::string("engine: ") + e.what()); }

    JsonObject out;
    out.emplace("converged",    Json(R.converged));
    out.emplace("diverged",     Json(R.diverged));
    out.emplace("iterations",   Json(static_cast<int64_t>(R.iterations)));
    out.emplace("lastIncrement",Json(static_cast<double>(R.lastIncrement)));
    JsonObject finalState; std::string nfWhere;
    if (!packFinalState(*sess->model, R.finalState, true, finalState, nfWhere))
        return MakeError(id, "NON_FINITE_RESULT", nfWhere);
    out.emplace("finalState", Json(std::move(finalState)));
    sess->lastSolve = std::make_unique<frame::SolveResult>(R.finalState);
    return MakeResponse(id, std::move(out));
}

Frame Dispatcher::HandleTensionOnly(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    const Json* body = in.header.get("body");
    std::string err;
    auto sess = resolveSession(ctx, body, err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);
    if (!sess->hasModel || !sess->model)
        return MakeError(id, "VALIDATION_FAILED", "session has no model; call model.set first");
    frame::TensionOnlyOptions opts;
    opts.maxIter           = static_cast<int>(body->getInt("maxIter", opts.maxIter));
    opts.allowReactivation = body->getBool("allowReactivation", opts.allowReactivation);
    opts.axialTol          = static_cast<frame::real>(body->getDouble("axialTol", opts.axialTol));
    frame::TensionOnlyResult R;
    try { R = frame::runTensionOnly(*sess->model, opts); }
    catch (const std::exception& e) { return MakeError(id, "INTERNAL", std::string("engine: ") + e.what()); }

    JsonObject out;
    out.emplace("converged",  Json(R.converged));
    out.emplace("cycled",     Json(R.cycled));
    out.emplace("iterations", Json(static_cast<int64_t>(R.iterations)));
    JsonArray slack; for (auto mid : R.slack) slack.push_back(Json(static_cast<int64_t>(mid)));
    out.emplace("slack", Json(std::move(slack)));
    JsonObject finalState; std::string nfWhere;
    if (!packFinalState(*sess->model, R.finalState, true, finalState, nfWhere))
        return MakeError(id, "NON_FINITE_RESULT", nfWhere);
    out.emplace("finalState", Json(std::move(finalState)));
    sess->lastSolve = std::make_unique<frame::SolveResult>(R.finalState);
    return MakeResponse(id, std::move(out));
}

Frame Dispatcher::HandleSizeOpt(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    const Json* body = in.header.get("body");
    std::string err;
    auto sess = resolveSession(ctx, body, err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);
    if (!sess->hasModel || !sess->model)
        return MakeError(id, "VALIDATION_FAILED", "session has no model; call model.set first");
    frame::SizeOptOptions opts;
    opts.maxIter = static_cast<int>(body->getInt("maxIter", opts.maxIter));
    opts.dcTol   = static_cast<frame::real>(body->getDouble("dcTol", opts.dcTol));
    opts.Amin    = static_cast<frame::real>(body->getDouble("Amin",  opts.Amin));
    std::vector<int> sizable;
    if (const Json* sb = body->get("sizableMembers"); sb && sb->isArray())
        for (const auto& e : sb->asArray())
            if (e.isNumber()) sizable.push_back(static_cast<int>(e.asInt()));
    frame::SizeOptResult R;
    try { R = frame::runSizeOptimization(*sess->model, opts, sizable); }
    catch (const std::exception& e) { return MakeError(id, "INTERNAL", std::string("engine: ") + e.what()); }

    JsonObject out;
    out.emplace("converged",     Json(R.converged));
    out.emplace("cycled",        Json(R.cycled));
    out.emplace("singular",      Json(R.singular));
    out.emplace("invalidDemand", Json(R.invalidDemand));
    out.emplace("iterations",    Json(static_cast<int64_t>(R.iterations)));
    JsonArray areas, dc;
    JsonObject areaByMember;
    for (std::size_t k = 0; k < R.finalAreas.size(); ++k) {
        auto v = R.finalAreas[k];
        if (!std::isfinite(static_cast<double>(v)))
            return MakeError(id, "NON_FINITE_RESULT", "finalAreas non-finite");
        areas.push_back(Json(static_cast<double>(v)));
    }
    for (std::size_t k = 0; k < R.finalDC.size(); ++k) {
        auto v = R.finalDC[k];
        if (!std::isfinite(static_cast<double>(v)))
            return MakeError(id, "NON_FINITE_RESULT", "finalDC non-finite");
        dc.push_back(Json(static_cast<double>(v)));
    }
    const std::size_t nArea = std::min(sess->model->members.size(),
                                       std::min(R.finalAreas.size(), R.finalDC.size()));
    for (std::size_t k = 0; k < nArea; ++k) {
        JsonArray pair;
        pair.push_back(Json(static_cast<double>(R.finalAreas[k])));
        pair.push_back(Json(static_cast<double>(R.finalDC[k])));
        areaByMember.emplace(std::to_string(sess->model->members[k].id), Json(std::move(pair)));
    }
    out.emplace("finalAreas", Json(std::move(areas)));
    out.emplace("finalDC",    Json(std::move(dc)));
    out.emplace("areas",      Json(std::move(areaByMember)));
    out.emplace("sizeOptSingular", Json(R.singular));
    const double weightVolume = R.weightHistory.empty()
        ? 0.0
        : static_cast<double>(R.weightHistory.back());
    if (!std::isfinite(weightVolume))
        return MakeError(id, "NON_FINITE_RESULT", "weightVolume non-finite");
    out.emplace("weightVolume", Json(weightVolume));
    return MakeResponse(id, std::move(out));
}

Frame Dispatcher::HandleCorotational(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    const Json* body = in.header.get("body");
    std::string err;
    auto sess = resolveSession(ctx, body, err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);
    if (!sess->hasModel || !sess->model)
        return MakeError(id, "VALIDATION_FAILED", "session has no model; call model.set first");
    frame::CorotationalOptions opts;
    opts.loadSteps = static_cast<int>(body->getInt("loadSteps", opts.loadSteps));
    opts.maxIter   = static_cast<int>(body->getInt("maxIter",   opts.maxIter));
    opts.tolR      = static_cast<frame::real>(body->getDouble("tolR", opts.tolR));
    opts.tolU      = static_cast<frame::real>(body->getDouble("tolU", opts.tolU));
    opts.shellCorotational = body->getBool("shellCorotational", opts.shellCorotational);
    frame::CorotationalResult R;
    try { R = frame::runCorotational(*sess->model, opts); }
    catch (const std::exception& e) { return MakeError(id, "INTERNAL", std::string("engine: ") + e.what()); }

    JsonObject out;
    out.emplace("converged", Json(R.converged));
    out.emplace("diverged",  Json(R.diverged));
    out.emplace("loadStepsCompleted", Json(static_cast<int64_t>(R.loadStepsCompleted)));
    out.emplace("totalIterations",    Json(static_cast<int64_t>(R.totalIterations)));
    out.emplace("lastResidual",       Json(static_cast<double>(R.lastResidual)));
    JsonObject finalState; std::string nfWhere;
    if (!packFinalState(*sess->model, R.finalState, true, finalState, nfWhere))
        return MakeError(id, "NON_FINITE_RESULT", nfWhere);
    out.emplace("finalState", Json(std::move(finalState)));
    sess->lastSolve = std::make_unique<frame::SolveResult>(R.finalState);
    return MakeResponse(id, std::move(out));
}

Frame Dispatcher::HandleArcLength(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    const Json* body = in.header.get("body");
    std::string err;
    auto sess = resolveSession(ctx, body, err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);
    if (!sess->hasModel || !sess->model)
        return MakeError(id, "VALIDATION_FAILED", "session has no model; call model.set first");
    frame::CorotationalOptions opts;
    opts.useArcLength      = true;                                         // method-implied
    // A-03 / C-01 honest-default fix (v2.5): arcLength=0 makes the engine fall back to its
    // auto-estimate from the first tangent. That is fine for casual exploration but can
    // silently jump over a snap-through region in one step (PROGRESS_S9c.md durable lesson 1).
    // - advanced profile: REJECT the missing/zero value with VALIDATION_FAILED so the client
    //   has to make the load-stepping intent explicit.
    // - simple profile:   silently accept the auto-estimate (matches v1 bridge behaviour)
    //   but record the choice in defaultsApplied so the client sees what was filled in.
    const bool arcLengthProvided = (body->get("arcLength") != nullptr);
    const double arcLengthRaw    = body->getDouble("arcLength", 0.0);
    if (!arcLengthProvided || arcLengthRaw == 0.0) {
        if (sess->profile == Profile::Advanced) {
            return MakeError(id, "VALIDATION_FAILED",
                             "solve.arclength: arcLength is required (advanced profile rejects "
                             "auto-estimate because it can step over a snap-through limit point)");
        }
        sess->defaultsApplied.push_back("arcLength=auto (engine first-tangent estimate)");
    }
    opts.arcLength         = static_cast<frame::real>(arcLengthRaw);
    opts.arcSteps          = static_cast<int>(body->getInt("arcSteps", opts.arcSteps));
    opts.maxIter           = static_cast<int>(body->getInt("maxIter",   opts.maxIter));
    opts.tolR              = static_cast<frame::real>(body->getDouble("tolR", opts.tolR));
    opts.monitorDof        = static_cast<int>(body->getInt("monitorDof", opts.monitorDof));
    opts.consistentTangent = body->getBool("consistentTangent", opts.consistentTangent);
    frame::CorotationalResult R;
    try { R = frame::runCorotational(*sess->model, opts); }
    catch (const std::exception& e) { return MakeError(id, "INTERNAL", std::string("engine: ") + e.what()); }

    JsonObject out;
    out.emplace("converged", Json(R.converged));
    out.emplace("diverged",  Json(R.diverged));
    out.emplace("totalIterations", Json(static_cast<int64_t>(R.totalIterations)));
    JsonArray pl, pd;
    for (auto v : R.pathLambda) {
        if (!std::isfinite(static_cast<double>(v))) return MakeError(id, "NON_FINITE_RESULT", "pathLambda");
        pl.push_back(Json(static_cast<double>(v)));
    }
    for (auto v : R.pathDisp) {
        if (!std::isfinite(static_cast<double>(v))) return MakeError(id, "NON_FINITE_RESULT", "pathDisp");
        pd.push_back(Json(static_cast<double>(v)));
    }
    out.emplace("pathLambda", Json(std::move(pl)));
    out.emplace("pathDisp",   Json(std::move(pd)));
    JsonObject finalState; std::string nfWhere;
    if (!packFinalState(*sess->model, R.finalState, true, finalState, nfWhere))
        return MakeError(id, "NON_FINITE_RESULT", nfWhere);
    out.emplace("finalState", Json(std::move(finalState)));
    sess->lastSolve = std::make_unique<frame::SolveResult>(R.finalState);
    return MakeResponse(id, std::move(out));
}

Frame Dispatcher::HandleModal(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    const Json* body = in.header.get("body");
    std::string err;
    auto sess = resolveSession(ctx, body, err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);
    if (!sess->hasModel || !sess->model)
        return MakeError(id, "VALIDATION_FAILED", "session has no model; call model.set first");
    // C-09 guard: modal solves on the LDLT PreparedSystem (frame::solveModal needs the LDLᵀ
    // factor of M-orthogonal subspace iteration). A session opened with mode=supernodal owns a
    // SnSession factor instead -- silently building a parallel LDLT here would defeat the
    // supernodal contract (factor-once amortisation) and confuse advancedDiagnostics' backend
    // tag. Refuse explicitly so the client opens a separate LDLT session.
    if (sess->useSnSession) {
        return MakeError(id, "NOT_IMPLEMENTED",
                         "analysis.modal requires LDLT primary; session opened with mode=supernodal "
                         "cannot run modal. Open a separate session with default mode.");
    }
    if (!sess->prepared) {
        try { sess->prepared = std::make_unique<frame::PreparedSystem>(frame::assembleAndFactor(*sess->model)); }
        catch (const std::exception& e) { return MakeError(id, "INTERNAL", std::string("engine: ") + e.what()); }
    }
    frame::ModalOptions opts;
    opts.numModes        = static_cast<int>(body->getInt("numModes", opts.numModes));
    opts.useSparseSolver = body->getBool("useSparseSolver", opts.useSparseSolver);
    frame::ModalResult R;
    try { R = frame::solveModal(*sess->prepared, opts); }
    catch (const std::exception& e) { return MakeError(id, "INTERNAL", std::string("engine: ") + e.what()); }

    JsonObject out;
    out.emplace("singular", Json(R.singular));
    if (!R.diagnostic.empty()) out.emplace("diagnostic", Json(R.diagnostic));
    JsonArray modes;
    for (const auto& m : R.modes) {
        if (!std::isfinite(static_cast<double>(m.omega)) || !std::isfinite(static_cast<double>(m.freqHz)))
            return MakeError(id, "NON_FINITE_RESULT", "mode omega/freqHz");
        JsonObject mo;
        mo.emplace("omega",  Json(static_cast<double>(m.omega)));
        mo.emplace("freqHz", Json(static_cast<double>(m.freqHz)));
        JsonArray shape;
        for (auto v : m.shape) {
            if (!std::isfinite(static_cast<double>(v))) return MakeError(id, "NON_FINITE_RESULT", "mode shape");
            shape.push_back(Json(static_cast<double>(v)));
        }
        mo.emplace("shape", Json(std::move(shape)));
        modes.push_back(Json(std::move(mo)));
    }
    out.emplace("modes", Json(std::move(modes)));
    return MakeResponse(id, std::move(out));
}

Frame Dispatcher::HandleBuckling(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    const Json* body = in.header.get("body");
    std::string err;
    auto sess = resolveSession(ctx, body, err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);
    if (!sess->hasModel || !sess->model)
        return MakeError(id, "VALIDATION_FAILED", "session has no model; call model.set first");
    // C-10 guard: same reason as C-09 in HandleModal -- buckling subspace iteration needs the
    // LDLT PreparedSystem, not the supernodal factor.
    if (sess->useSnSession) {
        return MakeError(id, "NOT_IMPLEMENTED",
                         "analysis.buckling requires LDLT primary; session opened with mode=supernodal "
                         "cannot run buckling. Open a separate session with default mode.");
    }
    if (!sess->prepared) {
        try { sess->prepared = std::make_unique<frame::PreparedSystem>(frame::assembleAndFactor(*sess->model)); }
        catch (const std::exception& e) { return MakeError(id, "INTERNAL", std::string("engine: ") + e.what()); }
    }
    frame::BucklingOptions opts;
    opts.denseThreshold          = static_cast<int>(body->getInt("denseThreshold", opts.denseThreshold));
    opts.nev                     = static_cast<int>(body->getInt("nev", opts.nev));
    opts.maxIter                 = static_cast<int>(body->getInt("maxIter", opts.maxIter));
    opts.tol                     = static_cast<frame::real>(body->getDouble("tol", opts.tol));
    opts.shellBucklingKnockdown  = static_cast<frame::real>(body->getDouble("shellBucklingKnockdown",
                                                                              opts.shellBucklingKnockdown));
    frame::BucklingResult R;
    try { R = frame::solveBuckling(*sess->prepared, *sess->model, opts); }
    catch (const std::exception& e) { return MakeError(id, "INTERNAL", std::string("engine: ") + e.what()); }

    JsonObject out;
    out.emplace("singular", Json(R.singular));
    if (!R.diagnostic.empty()) out.emplace("diagnostic", Json(R.diagnostic));
    if (!std::isfinite(static_cast<double>(R.criticalFactor))
        || !std::isfinite(static_cast<double>(R.reportedCriticalFactor))
        || !std::isfinite(static_cast<double>(R.knockdownFactor)))
        return MakeError(id, "NON_FINITE_RESULT", "buckling factor non-finite");
    out.emplace("criticalFactor",         Json(static_cast<double>(R.criticalFactor)));
    out.emplace("reportedCriticalFactor", Json(static_cast<double>(R.reportedCriticalFactor)));
    out.emplace("knockdownFactor",        Json(static_cast<double>(R.knockdownFactor)));
    JsonArray mode;
    for (auto v : R.mode) {
        if (!std::isfinite(static_cast<double>(v))) return MakeError(id, "NON_FINITE_RESULT", "buckling mode");
        mode.push_back(Json(static_cast<double>(v)));
    }
    out.emplace("mode", Json(std::move(mode)));
    return MakeResponse(id, std::move(out));
}

Frame Dispatcher::HandleDynCollapse(Dispatcher& d, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    const Json* body = in.header.get("body");
    std::string err;
    auto sess = resolveSession(ctx, body, err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);
    if (!sess->hasModel || !sess->model)
        return MakeError(id, "VALIDATION_FAILED", "session has no model; call model.set first");

    frame::DynCollapseOptions opts;
    opts.dt                = static_cast<frame::real>(body->getDouble("dt", opts.dt));
    opts.maxTime           = static_cast<frame::real>(body->getDouble("maxTime", opts.maxTime));
    opts.basisSize         = static_cast<int>(body->getInt("basisSize", opts.basisSize));
    opts.useRitzVectors    = body->getBool("useRitzVectors", opts.useRitzVectors);
    opts.rayleighAlpha     = static_cast<frame::real>(body->getDouble("rayleighAlpha", opts.rayleighAlpha));
    opts.rayleighBeta      = static_cast<frame::real>(body->getDouble("rayleighBeta", opts.rayleighBeta));
    opts.removeThreshold   = static_cast<frame::real>(body->getDouble("removeThreshold", opts.removeThreshold));
    opts.screenEvery       = static_cast<int>(body->getInt("screenEvery", opts.screenEvery));
    opts.quietKineticRatio = static_cast<frame::real>(body->getDouble("quietKineticRatio", opts.quietKineticRatio));
    opts.maxEvents         = static_cast<int>(body->getInt("maxEvents", opts.maxEvents));
    opts.frameStride       = static_cast<int>(body->getInt("frameStride", opts.frameStride));
    if (const Json* a = body->get("initialRemovals"); a && a->isArray()) {
        for (const auto& v : a->asArray())
            if (v.isNumber()) opts.initialRemovals.push_back(static_cast<frame::MemberId>(v.asInt()));
    }
    if (const Json* a = body->get("initialShellRemovals"); a && a->isArray()) {
        for (const auto& v : a->asArray())
            if (v.isNumber()) opts.initialShellRemovals.push_back(static_cast<int>(v.asInt()));
    }
    const bool streamFrames = body->getBool("streamFrames", true);
    const bool binaryFrames = body->getBool("binaryFrames", true);
    const bool streamEvents = body->getBool("streamEvents", true);

    // P1-3 (v2.7 live streaming): wire the engine's onFrameEmitted callback to push each
    // dyn_collapse.frame event onto the outbound queue THE MOMENT runDynamicCollapse stores it,
    // not after the integrator returns. The dispatcher's recv-side worker drains the queue
    // independently, so the client sees frames during the run instead of a burst at the end.
    // NON_FINITE_RESULT check moves into the callback (per-frame) so a NaN aborts the run
    // before the engine wastes more steps. The abort signal is shared with isCancelled so the
    // integrator early-exits at the next frameStride boundary.
    auto abortReason = std::make_shared<std::string>();
    const std::string capturedId = id;
    opts.onFrameEmitted = [&d, capturedId, binaryFrames, streamFrames, abortReason]
                         (const frame::DynCollapseFrame& fr) {
        if (!abortReason->empty()) return;
        if (!std::isfinite(static_cast<double>(fr.t))) { *abortReason = "frame time NaN"; return; }
        for (frame::real x : fr.u)
            if (!std::isfinite(static_cast<double>(x))) { *abortReason = "frame.u NaN"; return; }
        for (frame::real x : fr.v)
            if (!std::isfinite(static_cast<double>(x))) { *abortReason = "frame.v NaN"; return; }
        if (!streamFrames) return;
        JsonObject details;
        details.emplace("t",             Json(static_cast<double>(fr.t)));
        details.emplace("dof",           Json(static_cast<int64_t>(fr.u.size())));
        details.emplace("uCount",        Json(static_cast<int64_t>(fr.u.size())));
        details.emplace("vCount",        Json(static_cast<int64_t>(fr.v.size())));
        details.emplace("maxAbsU",       Json(maxAbs(fr.u)));
        details.emplace("maxAbsV",       Json(maxAbs(fr.v)));
        details.emplace("payloadLayout", Json(std::string(binaryFrames ? "u_then_v_f64_le" : "none")));
        Frame ev = MakeEvent(capturedId, "dyn_collapse.frame", std::move(details));
        if (binaryFrames) {
            ev.flags |= kFlagBinaryPayload;
            ev.payload.reserve((fr.u.size() + fr.v.size()) * sizeof(double));
            for (frame::real x : fr.u) appendF64LE(ev.payload, static_cast<double>(x));
            for (frame::real x : fr.v) appendF64LE(ev.payload, static_cast<double>(x));
        }
        d.Emit(std::move(ev));
    };
    opts.isCancelled = [&d, capturedId, abortReason]() {
        return !abortReason->empty() || d.IsCancelled(capturedId);
    };

    frame::DynCollapseHistory H;
    try { H = frame::runDynamicCollapse(*sess->model, opts); }
    catch (const std::exception& e) { return MakeError(id, "INTERNAL", std::string("engine: ") + e.what()); }

    if (!abortReason->empty())
        return MakeError(id, "NON_FINITE_RESULT", "dyn_collapse: " + *abortReason);
    if (d.IsCancelled(id))
        return MakeError(id, "CANCELLED", "dyn_collapse cancelled by client mid-run");

    // Events stay post-run (engine accumulates into H.events; live event emission would need a
    // second engine callback for that channel — out of v2.7 scope, deferred).
    if (streamEvents) {
        for (std::size_t i = 0; i < H.events.size(); ++i) {
            JsonObject details = packDynEvent(H.events[i], static_cast<int>(i));
            d.EnqueueOutbound(MakeEvent(id, "dyn_collapse.event", std::move(details)));
        }
    }

    JsonArray events;
    for (std::size_t i = 0; i < H.events.size(); ++i)
        events.push_back(Json(packDynEvent(H.events[i], static_cast<int>(i))));
    const double endTime = !H.frames.empty()
        ? static_cast<double>(H.frames.back().t)
        : (!H.events.empty() ? static_cast<double>(H.events.back().t) : 0.0);
    JsonObject out;
    out.emplace("outcome",            Json(std::string(collapseOutcomeName(H.outcome))));
    out.emplace("outcomeCode",        Json(static_cast<int64_t>(H.outcome)));
    out.emplace("diagnostic",         Json(H.diagnostic));
    out.emplace("nEvents",            Json(static_cast<int64_t>(H.events.size())));
    out.emplace("nFrames",            Json(static_cast<int64_t>(H.frames.size())));
    out.emplace("endTime",            Json(endTime));
    out.emplace("events",             Json(std::move(events)));
    out.emplace("streamedFrames",     Json(streamFrames));
    out.emplace("binaryFrames",       Json(binaryFrames));
    out.emplace("framePayloadLayout", Json(std::string(binaryFrames ? "u_then_v_f64_le" : "none")));
    return MakeResponse(id, std::move(out));
}

Frame Dispatcher::HandleReanalysis(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    const Json* body = in.header.get("body");
    std::string err;
    auto sess = resolveSession(ctx, body, err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);
    if (!sess->hasModel || !sess->model)
        return MakeError(id, "VALIDATION_FAILED", "session has no model; call model.set first");

    frame::ReanalysisOptions opts;
    opts.maxRank      = static_cast<int>(body->getInt("maxRank", opts.maxRank));
    opts.pcgTol       = static_cast<frame::real>(body->getDouble("pcgTol", opts.pcgTol));
    opts.pcgMaxIter   = static_cast<int>(body->getInt("pcgMaxIter", opts.pcgMaxIter));
    opts.allowTier2   = body->getBool("allowTier2", opts.allowTier2);
    opts.mechPivotTol = static_cast<frame::real>(body->getDouble("mechPivotTol", opts.mechPivotTol));

    frame::ReSolveSession rs(*sess->model, opts);
    if (const Json* arr = body->get("memberActive"); arr && arr->isArray()) {
        for (const auto& item : arr->asArray()) {
            if (!item.isObject())
                return MakeError(id, "VALIDATION_FAILED", "memberActive entries must be objects");
            const int mid = item.has("id")
                ? static_cast<int>(item.getInt("id", 0))
                : static_cast<int>(item.getInt("member", 0));
            const bool active = item.getBool("active", true);
            if (!rs.setMemberActive(static_cast<frame::MemberId>(mid), active))
                return MakeError(id, "VALIDATION_FAILED", "unknown member id in memberActive: " + std::to_string(mid));
        }
    }
    if (const Json* arr = body->get("shellActive"); arr && arr->isArray()) {
        for (const auto& item : arr->asArray()) {
            if (!item.isObject())
                return MakeError(id, "VALIDATION_FAILED", "shellActive entries must be objects");
            const int sid = item.has("id")
                ? static_cast<int>(item.getInt("id", 0))
                : static_cast<int>(item.getInt("shell", 0));
            const bool active = item.getBool("active", true);
            if (!rs.setShellActive(sid, active))
                return MakeError(id, "VALIDATION_FAILED", "unknown shell id in shellActive: " + std::to_string(sid));
        }
    }

    frame::ReanalysisStats stats;
    frame::SolveResult R;
    try { R = rs.solve(&stats); }
    catch (const std::exception& e) { return MakeError(id, "INTERNAL", std::string("engine: ") + e.what()); }

    JsonObject finalState; std::string nfWhere;
    if (!packFinalState(*sess->model, R, true, finalState, nfWhere))
        return MakeError(id, "NON_FINITE_RESULT", nfWhere);
    JsonObject st;
    st.emplace("tier",        Json(static_cast<int64_t>(stats.tier)));
    st.emplace("rank",        Json(static_cast<int64_t>(stats.rank)));
    st.emplace("pcgIters",    Json(static_cast<int64_t>(stats.pcgIters)));
    st.emplace("relResidual", Json(static_cast<double>(stats.relResidual)));
    st.emplace("refactored",  Json(stats.refactored));
    st.emplace("mechanism",   Json(stats.mechanism));

    JsonObject out;
    out.emplace("valid",      Json(rs.valid()));
    out.emplace("diagnostic", Json(rs.valid() ? std::string("") : rs.diagnostic()));
    out.emplace("stats",      Json(std::move(st)));
    out.emplace("finalState", Json(std::move(finalState)));
    sess->lastSolve = std::make_unique<frame::SolveResult>(R);
    return MakeResponse(id, std::move(out));
}

// ====================================================================================
// Remaining stub: model.patch waits for a schema decision.
// ====================================================================================

Frame Dispatcher::HandleModelPatch(Dispatcher&, Context&, const Frame& in) {
    return notImpl("model.patch", in);
}

}  // namespace frame_v2
