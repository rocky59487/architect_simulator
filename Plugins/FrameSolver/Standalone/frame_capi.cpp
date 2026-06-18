// frame_capi.cpp -- C ABI DLL implementation (S6 J2). Thin wrapper over frame_cli::processAll, so
// the C API and frame_cli.exe share one protocol implementation (results are byte-identical, which
// Tools/cli_roundtrip.py verifies via ctypes).
#include "frame_capi.h"
#include "frame_cli_core.h"

#include <string>
#include <cstring>
#include <algorithm>
#include <exception>

#ifndef FRAMECORE_BUILD_SHA
#define FRAMECORE_BUILD_SHA "unknown"
#endif

extern "C" {

const char* frame_capi_version(void) { return FRAMECORE_BUILD_SHA; }

int frame_capi_solve_text(const char* input, char* outBuf, int outCap) {
    // R2 audit LC-05: extern "C" ABI may NOT propagate C++ exceptions to a host (UB across
    // most platforms; some hosts crash, others corrupt the stack). Catch everything and
    // surface the failure through the same text channel the engine uses for diagnostics,
    // returning -1 to indicate the buffer holds an error message instead of a result block.
    std::string out;
    try {
        out = frame_cli::processAll(input ? std::string(input) : std::string());
    } catch (const std::exception& e) {
        out = std::string("ERR frame_capi exception: ") + e.what() + "\n";
    } catch (...) {
        out = "ERR frame_capi unknown exception\n";
    }
    const int n = static_cast<int>(out.size());
    if (outBuf && outCap > 0) {
        const int copy = std::min(n, outCap - 1);
        std::memcpy(outBuf, out.data(), static_cast<size_t>(copy));
        outBuf[copy] = '\0';
    }
    // Return value semantics unchanged from v2.0: produced length only. The caller
    // detects an exception by looking for an "ERR " prefix in the buffer (the engine
    // already uses "ERR ..." for protocol-level errors, so this is a uniform channel).
    return n;
}

}  // extern "C"
