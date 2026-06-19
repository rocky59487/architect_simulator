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

extern "C" {

FCAPI uint32_t frame_v2_abi_version(void) { return fv2::kAbiVersion; }

FCAPI const char* frame_v2_build_sha(void) { return FRAMECORE_BUILD_SHA; }

FCAPI const char* frame_v2_engine_version(void) { return fv2::kEngineVer; }

FCAPI frame_v2_ctx* frame_v2_open(void) {
    try { return new frame_v2_ctx(); } catch (...) { return nullptr; }
}

FCAPI void frame_v2_close(frame_v2_ctx* ctx) {
    if (!ctx) return;
    {
        std::lock_guard<std::mutex> lk(ctx->mtx);
        ctx->closed = true;
        ctx->cancelRecv = true;
    }
    ctx->cv.notify_all();
    delete ctx;
}

FCAPI int frame_v2_send(frame_v2_ctx* ctx, const uint8_t* inFrame, size_t inLen) {
    if (!ctx)             return FRAME_V2_INVALID_CTX;
    if (!inFrame)         return FRAME_V2_PROTOCOL_ERROR;

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

FCAPI int frame_v2_recv(frame_v2_ctx* ctx,
                        uint8_t* outBuf, size_t outCap,
                        size_t* outLen, size_t* outNeeded,
                        int blockingMs) {
    if (!ctx) return FRAME_V2_INVALID_CTX;
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

FCAPI int frame_v2_cancel_recv(frame_v2_ctx* ctx) {
    if (!ctx) return FRAME_V2_INVALID_CTX;
    {
        std::lock_guard<std::mutex> lk(ctx->mtx);
        ctx->cancelRecv = true;
    }
    ctx->cv.notify_all();
    return FRAME_V2_OK;
}

FCAPI int frame_v2_cancel_request(frame_v2_ctx* ctx, const char* targetId) {
    if (!ctx || !targetId) return FRAME_V2_INVALID_CTX;
    ctx->dispatcher->CancelRequest(std::string(targetId));
    return FRAME_V2_OK;
}

FCAPI const char* frame_v2_last_error(const frame_v2_ctx* ctx) {
    if (!ctx) return nullptr;
    std::lock_guard<std::mutex> lk(const_cast<std::mutex&>(ctx->mtx));
    return ctx->lastError.empty() ? nullptr : ctx->lastError.c_str();
}

FCAPI int frame_v2_pending_count(const frame_v2_ctx* ctx) {
    if (!ctx) return -1;
    return static_cast<int>(ctx->dispatcher->PendingOutbound());
}

}  // extern "C"
