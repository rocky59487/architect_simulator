// frame_capi.h -- C ABI for the FrameCore text-protocol engine (S6 J2). A minimal, stable C
// boundary that wraps the SAME wire protocol as frame_cli (docs/CLI_PROTOCOL.md): a client passes
// the model text in and reads the response text out. This is the long-term replacement for shelling
// out to frame_cli.exe (no process-spawn overhead) while reusing the gated, proven protocol core.
//
// Built as frame_capi.dll by build_capi.bat; smoke-tested via ctypes in Tools/cli_roundtrip.py.
#ifndef FRAME_CAPI_H
#define FRAME_CAPI_H

#if defined(_WIN32)
  #define FRAMECAPI __declspec(dllexport)
#else
  #define FRAMECAPI
#endif

#ifdef __cplusplus
extern "C" {
#endif

// The engine build SHA (static string; do not free).
FRAMECAPI const char* frame_capi_version(void);

// Process a CLI text-protocol input (one or more blocks ending in END). Writes the NUL-terminated
// response into outBuf (truncated to outCap-1 bytes). Returns the FULL response length in bytes
// (excluding the NUL); if the return value is >= outCap the output was truncated -- call again with
// a buffer of at least (returned length + 1). No globals -> reentrant.
FRAMECAPI int frame_capi_solve_text(const char* input, char* outBuf, int outCap);

#ifdef __cplusplus
}
#endif

#endif  // FRAME_CAPI_H
