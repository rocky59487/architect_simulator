// frame_cli_core.h -- the shared CLI text-protocol engine (S6). Parses one or more model BLOCKS
// (each ending in END) and returns the full text response, every block's output followed by an
// EOR line -- identical to streaming the CLI per block. Used by BOTH frame_cli.exe (stdin/stdout
// streaming wrapper) and the C API DLL (frame_capi), so the protocol has a single implementation.
#pragma once
#include <string>

namespace frame_cli {

// Process a whole input buffer (one or more blocks). Returns all output text (VERSION ... EOR per
// block). Engine-only, no UE; the authoritative wire protocol is docs/CLI_PROTOCOL.md.
std::string processAll(const std::string& input);

}  // namespace frame_cli
