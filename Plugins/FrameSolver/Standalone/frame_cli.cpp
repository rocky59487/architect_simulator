// frame_cli -- thin stdin/stdout streaming wrapper around frame_cli_core (S6 J1/J1b/J1.5).
//
// Reads model BLOCKS line by line; on each `END` line it processes the buffered block through
// frame_cli::processAll and writes the response (VERSION ... EOR) with a flush, so a daemon client
// can stream many models through one process and read one response per EOR. A single-shot client
// (model + END + EOF) is the one-block case, byte-for-byte unchanged. The OpenSees harness sees the
// same DISP/MF lines (VERSION/EOR are unknown tokens it ignores). The wire protocol and the analysis
// commands (TONLY/SIZEOPT/DYNC, MAT cap token, etc.) live in frame_cli_core.cpp / docs/CLI_PROTOCOL.md.
// The SAME core backs the C API DLL (frame_capi), so the protocol has a single implementation.
#include "frame_cli_core.h"

#include <iostream>
#include <string>
#include <cstdio>

#ifndef FRAMECORE_BUILD_SHA
#define FRAMECORE_BUILD_SHA "unknown"
#endif

static bool isEndLine(const std::string& line) {
    const size_t a = line.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return false;
    const size_t z = line.find_last_not_of(" \t\r\n");
    return line.compare(a, z - a + 1, "END") == 0;
}

int main() {
    // Provenance to stderr; stdout is the parsed protocol stream.
    std::fprintf(stderr, "# frame_cli | build %s | compiled %s %s\n",
                 FRAMECORE_BUILD_SHA, __DATE__, __TIME__);

    std::string block, line;
    bool hasContent = false;
    while (std::getline(std::cin, line)) {
        block += line; block += "\n";
        if (line.find_first_not_of(" \t\r\n") != std::string::npos) hasContent = true;
        if (isEndLine(line)) {
            std::fputs(frame_cli::processAll(block).c_str(), stdout);
            std::fflush(stdout);              // daemon: one flushed response per block
            block.clear();
            hasContent = false;
        }
    }
    // Lenient: a final block that reached EOF without a trailing END is still solved once.
    if (hasContent) {
        std::fputs(frame_cli::processAll(block + "END\n").c_str(), stdout);
        std::fflush(stdout);
    }
    return 0;
}
