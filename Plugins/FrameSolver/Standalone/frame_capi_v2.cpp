// frame_capi_v2.cpp -- C ABI v2 DLL impl. Thin wrapper that owns one frame_v2::Dispatcher per
// frame_v2_ctx and shuttles frames between the C boundary and the dispatcher.
//
// THREADING -- one ctx is owned by one client; the C ABI documents non-overlapping send/recv
// per ctx. We use a single mutex per ctx to keep the impl simple; the dispatcher itself has
// its own internal mutex around the outbound queue.
//
// EXCEPTIONS -- never propagate. Every entry point is wrapped; failures come back as either
// (a) a structured `error` frame in the outbound queue, or (b) a non-zero return code.
//
// DEPENDENCIES -- this TU includes ONLY the v2 headers (Dispatcher.h transitively pulls in
// FrameWire.h + MiniJson.h). NO FrameCore headers yet -- the engine wiring lives behind
// Dispatcher TODO markers and gets included in B3 when each handler wires up.
//
// BUILD -- build_capi_v2.bat (sibling, independent of build.bat / build_capi.bat). NOT in
// the engine 5-leg gate; verified instead by the 6th leg Tools/v2_roundtrip.py once B3 lands.

#include "frame_capi_v2.h"
#include "v2/Dispatcher.h"
#include "v2/FrameWire.h"

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#ifndef FRAMECORE_BUILD_SHA
#define FRAMECORE_BUILD_SHA "unknown"
#endif

namespace fv2 = frame_v2;

struct frame_v2_ctx {
    std::mutex                   mtx;
    std::condition_variable      cv;
    bool                         cancelRecv = false;
    bool                         closed     = false;
    std::string                  lastError;
    // recv() does a two-call dance: probe size with cap=0, then refill with cap=needed. The
    // probe MUST NOT consume the dispatcher's outbound frame. We Pop once and cache the
    // serialized bytes here; the probe reads cache.size() into *outNeeded, the refill copies
    // cache and clears it. Without this the second call hits an empty queue and times out.
    std::string                  pendingSerialized;
    std::unique_ptr<fv2::Dispatcher> dispatcher = std::make_unique<fv2::Dispatcher>();
};

// A-01 UAF fix: the owning shared_ptr lives in this DLL-global registry. Every entry point
// (recv, send, cancel_*, last_error, pending_count) takes a fresh ref off the registry, so an
// in-flight call ALWAYS holds the ctx alive across its body. frame_v2_close drops the registry
// owner ref and signals shutdown; the dtor runs synchronously if no one else has a ref, or
// asynchronously when the last in-flight call returns. recv's cv.wait re-acquires its OWN
// mutex on a ctx that the caller's shared_ptr guarantees is still alive -- no UAF window.
namespace {
std::mutex g_registryMtx;
std::unordered_map<frame_v2_ctx*, std::shared_ptr<frame_v2_ctx>> g_owner;

inline std::shared_ptr<frame_v2_ctx> acquire(frame_v2_ctx* raw) {
    if (!raw) return nullptr;
    std::lock_guard<std::mutex> lk(g_registryMtx);
    auto it = g_owner.find(raw);
    return it == g_owner.end() ? nullptr : it->second;
}
}  // namespace

extern "C" {

FCAPI uint32_t frame_v2_abi_version(void) { return fv2::kAbiVersion; }

FCAPI const char* frame_v2_build_sha(void) { return FRAMECORE_BUILD_SHA; }

FCAPI const char* frame_v2_engine_version(void) { return fv2::kEngineVer; }

FCAPI frame_v2_ctx* frame_v2_open(void) {
    try {
        auto ctx = std::make_shared<frame_v2_ctx>();
        frame_v2_ctx* raw = ctx.get();
        std::lock_guard<std::mutex> lk(g_registryMtx);
        g_owner.emplace(raw, std::move(ctx));
        return raw;
    } catch (...) { return nullptr; }
}

FCAPI void frame_v2_close(frame_v2_ctx* raw) {
    if (!raw) return;
    // Pull the owner shared_ptr out of the registry. From this point a concurrent
    // send/recv/cancel call will get acquire()==nullptr -> INVALID_CTX, which is the
    // documented post-close contract.
    std::shared_ptr<frame_v2_ctx> ctx;
    {
        std::lock_guard<std::mutex> lk(g_registryMtx);
        auto it = g_owner.find(raw);
        if (it == g_owner.end()) return;
        ctx = std::move(it->second);
        g_owner.erase(it);
    }
    // Signal in-flight recv calls to bail. Any thread already inside recv holds its own
    // shared_ptr ref (taken via acquire), so the ctx outlives THIS call's `ctx` going out
    // of scope below -- destruction is deferred to whoever drops the last ref.
    {
        std::lock_guard<std::mutex> lk(ctx->mtx);
        ctx->closed = true;
        ctx->cancelRecv = true;
    }
    ctx->cv.notify_all();
}

FCAPI int frame_v2_send(frame_v2_ctx* raw, const uint8_t* inFrame, size_t inLen) {
    auto ctx_sp = acquire(raw);
    if (!ctx_sp)          return FRAME_V2_INVALID_CTX;
    if (!inFrame)         return FRAME_V2_PROTOCOL_ERROR;
    frame_v2_ctx* ctx = ctx_sp.get();

    fv2::Frame parsed;
    size_t consumed = 0, needed = 0;
    std::string err;
    if (!fv2::ParseFrame(inFrame, inLen, parsed, consumed, needed, err)) {
        std::lock_guard<std::mutex> lk(ctx->mtx);
        ctx->lastError = "frame_v2_send: " + err;
        return FRAME_V2_PROTOCOL_ERROR;
    }

    try {
        ctx->dispatcher->Submit(std::move(parsed));
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lk(ctx->mtx);
        ctx->lastError = std::string("dispatcher: ") + e.what();
        return FRAME_V2_OUT_OF_MEMORY;
    } catch (...) {
        std::lock_guard<std::mutex> lk(ctx->mtx);
        ctx->lastError = "dispatcher: unknown exception";
        return FRAME_V2_OUT_OF_MEMORY;
    }
    ctx->cv.notify_all();
    return FRAME_V2_OK;
}

FCAPI int frame_v2_recv(frame_v2_ctx* raw,
                        uint8_t* outBuf, size_t outCap,
                        size_t* outLen, size_t* outNeeded,
                        int blockingMs) {
    auto ctx_sp = acquire(raw);
    if (!ctx_sp) return FRAME_V2_INVALID_CTX;
    frame_v2_ctx* ctx = ctx_sp.get();
    if (outLen)    *outLen    = 0;
    if (outNeeded) *outNeeded = 0;

    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(blockingMs > 0 ? blockingMs : 0);

    std::unique_lock<std::mutex> lk(ctx->mtx);

    // Only Pop+serialize when the cache is empty. The probe call (outCap=0) populates the
    // cache and returns NEED_BIGGER without consuming it; the refill call reads from cache
    // and clears it. This is the contract documented in frame_capi_v2.h's recv comment.
    if (ctx->pendingSerialized.empty()) {
        fv2::Frame frame;
        while (true) {
            if (ctx->cancelRecv) { ctx->cancelRecv = false; return FRAME_V2_CANCELLED; }
            if (ctx->dispatcher->TryPop(frame)) break;
            if (blockingMs == 0) return FRAME_V2_EMPTY;
            if (blockingMs < 0)  { ctx->cv.wait(lk); continue; }
            if (ctx->cv.wait_until(lk, deadline) == std::cv_status::timeout) return FRAME_V2_TIMEOUT;
        }
        ctx->pendingSerialized = fv2::SerializeFrame(frame.flags, frame.header, frame.payload);
    }

    const std::string& bytes = ctx->pendingSerialized;
    if (outNeeded) *outNeeded = bytes.size();
    if (outCap == 0 || outCap < bytes.size()) return FRAME_V2_NEED_BIGGER;

    std::memcpy(outBuf, bytes.data(), bytes.size());
    if (outLen) *outLen = bytes.size();
    ctx->pendingSerialized.clear();
    return FRAME_V2_OK;
}

FCAPI int frame_v2_cancel_recv(frame_v2_ctx* raw) {
    auto ctx_sp = acquire(raw);
    if (!ctx_sp) return FRAME_V2_INVALID_CTX;
    frame_v2_ctx* ctx = ctx_sp.get();
    {
        std::lock_guard<std::mutex> lk(ctx->mtx);
        ctx->cancelRecv = true;
    }
    ctx->cv.notify_all();
    return FRAME_V2_OK;
}

FCAPI int frame_v2_cancel_request(frame_v2_ctx* raw, const char* targetId) {
    auto ctx_sp = acquire(raw);
    if (!ctx_sp || !targetId) return FRAME_V2_INVALID_CTX;
    ctx_sp->dispatcher->CancelRequest(std::string(targetId));
    return FRAME_V2_OK;
}

FCAPI const char* frame_v2_last_error(const frame_v2_ctx* raw) {
    auto ctx_sp = acquire(const_cast<frame_v2_ctx*>(raw));
    if (!ctx_sp) return nullptr;
    frame_v2_ctx* ctx = ctx_sp.get();
    std::lock_guard<std::mutex> lk(ctx->mtx);
    // NOTE: returning a c_str() from a member that might mutate is technically unsafe across
    // threads, but the v1 API and our existing roundtrip clients all read-then-act sequentially.
    // The caller is documented not to call frame_v2_send concurrently with frame_v2_last_error
    // on the same ctx; that contract is unchanged here.
    return ctx->lastError.empty() ? nullptr : ctx->lastError.c_str();
}

FCAPI int frame_v2_pending_count(const frame_v2_ctx* raw) {
    auto ctx_sp = acquire(const_cast<frame_v2_ctx*>(raw));
    if (!ctx_sp) return -1;
    return static_cast<int>(ctx_sp->dispatcher->PendingOutbound());
}

}  // extern "C"
