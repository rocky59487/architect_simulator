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

namespace frame_v2 {

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
    ctx.sessions[s->id] = s;

    JsonObject body;
    body.emplace("session", Json(s->id));
    body.emplace("ready",   Json(true));
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

    // [TODO B3] Construct frame::FrameModel from body and call engine's validate(); the schema
    // -> FrameModel conversion mirrors frame_cli_core.cpp::buildModel but reads from MiniJson
    // instead of istringstream. Real call site:
    //
    //   frame::FrameModel model = buildModelFromJson(*body);
    //   std::string why;
    //   if (!model.validate(why)) return MakeError(id, "VALIDATION_FAILED", why);
    //   sess->hasModel = true;
    //   sess->modelFingerprint = ...;
    //
    // For B2 we accept the JSON shape and return a placeholder dofCount so the dispatch path
    // and the v2_roundtrip gate's plumbing tests can run end-to-end.

    sess->hasModel = true;
    int64_t nNodes = 0;
    if (const Json* nodes = body->get("nodes"); nodes && nodes->isArray())
        nNodes = static_cast<int64_t>(nodes->asArray().size());

    JsonObject out;
    out.emplace("ok",       Json(true));
    out.emplace("dofCount", Json(nNodes * 6));
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

Frame Dispatcher::HandleSolveLinear(Dispatcher&, Context& ctx, const Frame& in) {
    const std::string id = in.header.getString("id", "?");
    const Json* body = in.header.get("body");
    std::string err;
    auto sess = resolveSession(ctx, body, err);
    if (!sess) return MakeError(id, "VALIDATION_FAILED", err);
    if (!sess->hasModel) return MakeError(id, "VALIDATION_FAILED", "session has no model; call model.set first");

    // [TODO B3] real engine call here. Returns SolveResult; map to body fields.
    // For B2 stub: emit the response SHAPE so a roundtrip test can verify the wire layout
    // without needing the engine. Treat as "engine returned 0 displacements"; the gate
    // marks solve.linear as SKIP until B3 replaces the placeholder.

    JsonObject empty;
    JsonObject out;
    out.emplace("singular",     Json(false));
    out.emplace("pivotMargin",  Json(0.0));
    out.emplace("disp",         Json(std::move(empty)));
    out.emplace("reactions",    Json(JsonObject{}));
    out.emplace("memberForces", Json(JsonObject{}));
    out.emplace("shellForces",  Json(JsonObject{}));
    out.emplace("_stub",        Json(true));  // explicit marker the gate looks for
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
}  // namespace

// ====================================================================================
// inspect.disp / mf / rf / sf -- TODO B3. They are registered so older callers get a
// structured protocol error, but they are not advertised in hello capabilities until the
// session owns a real cached SolveResult.
// ====================================================================================

Frame Dispatcher::HandleInspectDisp(Dispatcher&, Context&, const Frame& in) {
    return notImpl("inspect.disp", in);
}

Frame Dispatcher::HandleInspectMF(Dispatcher&, Context&, const Frame& in) { return notImpl("inspect.member_forces", in); }
Frame Dispatcher::HandleInspectRF(Dispatcher&, Context&, const Frame& in) { return notImpl("inspect.reactions", in); }
Frame Dispatcher::HandleInspectSF(Dispatcher&, Context&, const Frame& in) { return notImpl("inspect.shell_forces", in); }

// ====================================================================================
// Stubs (B3+ wires these). All return NOT_IMPLEMENTED -- the protocol level error code clearly
// distinguishes them from a real engine failure.
// ====================================================================================

Frame Dispatcher::HandleModelPatch  (Dispatcher&, Context&, const Frame& in) { return notImpl("model.patch",        in); }
Frame Dispatcher::HandlePDelta      (Dispatcher&, Context&, const Frame& in) { return notImpl("solve.pdelta",       in); }
Frame Dispatcher::HandleTensionOnly (Dispatcher&, Context&, const Frame& in) { return notImpl("solve.tension_only", in); }
Frame Dispatcher::HandleSizeOpt     (Dispatcher&, Context&, const Frame& in) { return notImpl("solve.size_opt",     in); }
Frame Dispatcher::HandleDynCollapse (Dispatcher&, Context&, const Frame& in) { return notImpl("solve.dyn_collapse", in); }
Frame Dispatcher::HandleCorotational(Dispatcher&, Context&, const Frame& in) { return notImpl("solve.corotational", in); }
Frame Dispatcher::HandleArcLength   (Dispatcher&, Context&, const Frame& in) { return notImpl("solve.arclength",    in); }
Frame Dispatcher::HandleModal       (Dispatcher&, Context&, const Frame& in) { return notImpl("analysis.modal",     in); }
Frame Dispatcher::HandleBuckling    (Dispatcher&, Context&, const Frame& in) { return notImpl("analysis.buckling",  in); }
Frame Dispatcher::HandleReanalysis  (Dispatcher&, Context&, const Frame& in) { return notImpl("analysis.reanalysis_solve", in); }

}  // namespace frame_v2
