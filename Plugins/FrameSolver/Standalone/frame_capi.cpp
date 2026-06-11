// frame_capi.cpp -- C ABI DLL implementation (S6 J2). Thin wrapper over frame_cli::processAll, so
// the C API and frame_cli.exe share one protocol implementation (results are byte-identical, which
// Tools/cli_roundtrip.py verifies via ctypes).
#include "frame_capi.h"
#include "frame_cli_core.h"

#include <string>
#include <cstring>
#include <algorithm>

#ifndef FRAMECORE_BUILD_SHA
#define FRAMECORE_BUILD_SHA "unknown"
#endif

extern "C" {

const char* frame_capi_version(void) { return FRAMECORE_BUILD_SHA; }

int frame_capi_solve_text(const char* input, char* outBuf, int outCap) {
    const std::string out = frame_cli::processAll(input ? std::string(input) : std::string());
    const int n = static_cast<int>(out.size());
    if (outBuf && outCap > 0) {
        const int copy = std::min(n, outCap - 1);
        std::memcpy(outBuf, out.data(), static_cast<size_t>(copy));
        outBuf[copy] = '\0';
    }
    return n;
}

}  // extern "C"
