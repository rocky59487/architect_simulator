// Dispatcher.cpp -- handler implementations.
//
// CURRENT STATE (B2 stub level)
//   Methods marked [WIRED] are end-to-end functional against the engine: hello, session.open,
//   session.close, session.status, cancel are connection-mgmt and pure-C++ (no engine call) so
//   they ship complete in B2.
//
//   Methods marked [TODO B3] dispatch the JSON correctly, validate inputs, and return a
//   structured error (NOT_IMPLEMENTED). Wiring each one to the engine is a per-method exercise:
//     1. construct frame::FrameModel from body
//     2. call the matching engine function (solve / runPDelta / runTensionOnly / ...)
//     3. emit the result fields the spec § ④ defines
//     4. for advanced profile, also fill `advancedDiagnostics` from the engine traces
//     5. add a roundtrip-py check that the value matches v1's stdout output
//
//   No engine #include yet -- B2 deliberately keeps Dispatcher.cpp self-contained so building
//   it does not require the entire FrameCore source tree. B3 will add the includes and link
//   line in build_capi_v2.bat.

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

#include <cmath>

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

    // Model + the bare solver path -- model.set validates JSON; solve.linear is the [TODO B3]
    // anchor with the most-detailed scaffold so the others can be cloned from it.
    Register("model.set",      &Dispatcher::HandleModelSet);
    Register("model.patch",    &Dispatcher::HandleModelPatch);
    Register("solve.linear",   &Dispatcher::HandleSolveLinear);

    // Inspect family.
    Register("inspect.disp",          &Dispatcher::HandleInspectDisp);
    Register("inspect.member_forces", &Dispatcher::HandleInspectMF);
    Register("inspect.reactions",     &Dispatcher::HandleInspectRF);
    Register("inspect.shell_forces",  &Dispatcher::HandleInspectSF);

    // Analysis family. All [TODO B3].
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
// model.set -- VALIDATES the schema today, ACCEPTS in simple, REJECTS missing-cap in advanced.
// Engine call is the [TODO B3] anchor (commented out below); rest of model.set is real.
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
// [TODO B3] solve.linear -- skeleton that the roundtrip gate's plumbing tests rely on.
// The real implementation: assembleAndFactor + solveLoad, emit disp/reactions/MF/SF dicts.
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
    }

    if (sess->profile == Profile::Advanced) {
        JsonObject diag;
        diag.emplace("factorMethod",  Json(std::string("LDLT")));
        diag.emplace("factorBackend", Json(std::string("SimplicialLDLT")));
        diag.emplace("factorTimeMs",  Json(0.0));
        diag.emplace("solveTimeMs",   Json(0.0));
        out.emplace("advancedDiagnostics", Json(std::move(diag)));
    }
    return MakeResponse(id, std::move(out));
}

namespace {
inline Frame notImpl(const std::string& method, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    return MakeError(id, "NOT_IMPLEMENTED", method + " is registered but not wired to the engine yet (B2 stub)");
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
    for (auto v : R.finalAreas) {
        if (!std::isfinite(static_cast<double>(v)))
            return MakeError(id, "NON_FINITE_RESULT", "finalAreas non-finite");
        areas.push_back(Json(static_cast<double>(v)));
    }
    for (auto v : R.finalDC) {
        if (!std::isfinite(static_cast<double>(v)))
            return MakeError(id, "NON_FINITE_RESULT", "finalDC non-finite");
        dc.push_back(Json(static_cast<double>(v)));
    }
    out.emplace("finalAreas", Json(std::move(areas)));
    out.emplace("finalDC",    Json(std::move(dc)));
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
    opts.arcLength         = static_cast<frame::real>(body->getDouble("arcLength", 0.0));
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

// ====================================================================================
// Remaining stubs: dyn_collapse waits for B4 (streaming + binary payload); reanalysis_solve
// waits for B5 (factor-reuse session); model.patch waits for a schema decision.
// ====================================================================================

Frame Dispatcher::HandleModelPatch  (Dispatcher&, Context&, const Frame& in) { return notImpl("model.patch",        in); }
Frame Dispatcher::HandleDynCollapse (Dispatcher&, Context&, const Frame& in) { return notImpl("solve.dyn_collapse", in); }
Frame Dispatcher::HandleReanalysis  (Dispatcher&, Context&, const Frame& in) { return notImpl("analysis.reanalysis_solve", in); }

}  // namespace frame_v2
