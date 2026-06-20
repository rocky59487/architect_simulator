// frame_capi_v2.cpp -- C ABI v2 DLL impl. Thin wrapper that owns one frame_v2::Dispatcher per
// frame_v2_ctx and shuttles frames between the C boundary and the dispatcher.
//
// THREADING -- one ctx is owned by one client. frame_v2_send parses and queues inbound frames;
// a per-context worker thread owns Dispatcher::Submit; frame_v2_recv drains outbound frames.
//
// EXCEPTIONS -- never propagate. Every entry point is wrapped; failures come back as either
// (a) a structured `error` frame in the outbound queue, or (b) a non-zero return code.
//
// DEPENDENCIES -- this TU includes only the v2 transport headers. Engine-facing code lives in
// Dispatcher.cpp; the C ABI stays a thin frame transport.
//
// BUILD -- build_capi_v2.bat (sibling, independent of build.bat / build_capi.bat). NOT in
// the engine 5-leg gate; verified instead by the 6th leg Tools/v2_roundtrip.py once B3 lands.

#include "frame_capi_v2.h"
#include "v2/Dispatcher.h"
#include "v2/FrameWire.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <thread>
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
    std::deque<fv2::Frame>       inbound;
    std::thread                  worker;
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

inline void workerLoop(std::shared_ptr<frame_v2_ctx> ctx) {
    while (true) {
        fv2::Frame frame;
        {
            std::unique_lock<std::mutex> lk(ctx->mtx);
            ctx->cv.wait(lk, [&] { return ctx->closed || !ctx->inbound.empty(); });
            if (ctx->closed) break;
            frame = std::move(ctx->inbound.front());
            ctx->inbound.pop_front();
        }

        try {
            ctx->dispatcher->Submit(std::move(frame));
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lk(ctx->mtx);
            ctx->lastError = std::string("dispatcher: ") + e.what();
        } catch (...) {
            std::lock_guard<std::mutex> lk(ctx->mtx);
            ctx->lastError = "dispatcher: unknown exception";
        }
        ctx->cv.notify_all();
    }
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
        ctx->worker = std::thread(workerLoop, ctx);
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
        ctx->inbound.clear();
    }
    ctx->cv.notify_all();
    if (ctx->worker.joinable())
        ctx->worker.join();
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

    {
        std::lock_guard<std::mutex> lk(ctx->mtx);
        if (ctx->closed) return FRAME_V2_INVALID_CTX;
        // v2.8.1 audit (C-02): cap the inbound queue. A long-running handler (e.g.
        // solve.dyn_collapse) blocks the worker for seconds; without a cap, a fast client
        // can pile up unbounded inbound frames during that window. 256 is comfortably above
        // any realistic Grasshopper burst (~10 frames during a single slider drag) but
        // well below per-process thread / heap exhaustion.
        if (ctx->inbound.size() >= 256) {
            ctx->lastError = "frame_v2_send: inbound queue depth cap (256) exceeded";
            return FRAME_V2_OUT_OF_MEMORY;
        }
        try {
            ctx->inbound.push_back(std::move(parsed));
        } catch (const std::bad_alloc&) {
            ctx->lastError = "frame_v2_send: inbound queue allocation failed";
            return FRAME_V2_OUT_OF_MEMORY;
        }
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
    thread_local std::string tlsLastError;
    tlsLastError = ctx->lastError;
    return tlsLastError.empty() ? nullptr : tlsLastError.c_str();
}

FCAPI int frame_v2_pending_count(const frame_v2_ctx* raw) {
    auto ctx_sp = acquire(const_cast<frame_v2_ctx*>(raw));
    if (!ctx_sp) return -1;
    return static_cast<int>(ctx_sp->dispatcher->PendingOutbound());
}

}  // extern "C"
