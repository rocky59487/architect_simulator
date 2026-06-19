// FrameWire.h -- v2 wire-frame parse/serialize. Mirrors the layout documented in
// docs/specs/S6b_rhino_bridge_v2.md § 2.1 and frame_capi_v2.h:
//
//   +--------+--------+----------------+------------------+
//   | MAGIC  | FLAGS  | HEADER_LEN(4)  | PAYLOAD_LEN(4)   |   12 bytes fixed
//   +--------+--------+----------------+------------------+
//   | HEADER JSON (UTF-8, no NUL)                          |
//   +------------------------------------------------------+
//   | PAYLOAD (binary)                                     |
//   +------------------------------------------------------+
//
// Little-endian length fields. The header is plain JSON parsed by MiniJson; the payload is
// raw bytes (no parsing here, the dispatcher decides interpretation).
//
// Header-only.

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "MiniJson.h"

namespace frame_v2 {

inline constexpr uint8_t  kMagic0          = 'F';
inline constexpr uint8_t  kMagic1          = 'C';
inline constexpr size_t   kHeaderFixedBytes = 12;

// Sanity caps: the wire is uint32 length-prefixed so 4 GB is the protocol-level upper bound,
// but a malicious or buggy peer that sends `headerLen = 0xFFFFFFFF` would have us allocate
// 4 GB synchronously (and the C# side would int-cast a UIntPtr of that size to a negative
// `byte[]` length and bomb). Caps are roomy but bounded.
inline constexpr uint32_t kMaxHeaderLen     = 16 * 1024 * 1024;   // 16 MiB JSON header
inline constexpr uint32_t kMaxPayloadLen    = 256 * 1024 * 1024;  // 256 MiB binary payload

inline constexpr uint16_t kFlagEndOfResponse = 1u << 0;
inline constexpr uint16_t kFlagHasPayload    = 1u << 1;
inline constexpr uint16_t kFlagBinaryPayload = 1u << 2;

struct Frame {
    uint16_t            flags = 0;
    Json                header;          // parsed JSON object (kind/id/method/body/...)
    std::vector<uint8_t> payload;        // raw bytes
};

// Read one little-endian uint32 at offset `off` from `buf` of size `n`. Returns 0 on out-of-range
// AND signals failure via `ok` reference (caller checks).
inline uint32_t readLE32(const uint8_t* buf, size_t n, size_t off, bool& ok) {
    if (off + 4 > n) { ok = false; return 0; }
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(buf[off + i]) << (8 * i);
    return v;
}

inline void writeLE32(std::string& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out += static_cast<char>((v >> (8 * i)) & 0xFF);
}

inline void writeLE16(std::string& out, uint16_t v) {
    for (int i = 0; i < 2; ++i) out += static_cast<char>((v >> (8 * i)) & 0xFF);
}

// Parse exactly one frame at the start of `buf`. On success: fills `f`, sets `bytesConsumed`
// to the frame length, returns true. On malformed input: writes a one-line `err` and returns
// false. Caller controls partial-buffer streaming by calling Parse only when
// bytes >= kHeaderFixedBytes and inspecting `bytesNeeded` (output) for "incomplete" cases.
inline bool ParseFrame(const uint8_t* buf, size_t n, Frame& f,
                       size_t& bytesConsumed, size_t& bytesNeeded, std::string& err) {
    bytesConsumed = 0;
    bytesNeeded   = 0;
    if (n < kHeaderFixedBytes) { bytesNeeded = kHeaderFixedBytes; err = "incomplete fixed header"; return false; }
    if (buf[0] != kMagic0 || buf[1] != kMagic1) { err = "bad magic"; return false; }

    f.flags = static_cast<uint16_t>(buf[2]) | (static_cast<uint16_t>(buf[3]) << 8);

    bool ok = true;
    uint32_t headerLen  = readLE32(buf, n, 4, ok);
    uint32_t payloadLen = readLE32(buf, n, 8, ok);
    if (!ok) { err = "header length read failed"; return false; }

    // v2.4 release-hardening: reject pathological lengths up front (DoS / OOM guard).
    if (headerLen > kMaxHeaderLen)   { err = "header too large"; return false; }
    if (payloadLen > kMaxPayloadLen) { err = "payload too large"; return false; }

    size_t total = kHeaderFixedBytes + headerLen + payloadLen;
    if (n < total) { bytesNeeded = total; err = "incomplete frame"; return false; }

    // Parse JSON header
    std::string json(reinterpret_cast<const char*>(buf + kHeaderFixedBytes), headerLen);
    std::string jerr;
    if (!Json::parse(json, f.header, jerr)) { err = "bad header JSON: " + jerr; return false; }

    // Copy payload bytes
    f.payload.assign(buf + kHeaderFixedBytes + headerLen,
                     buf + kHeaderFixedBytes + headerLen + payloadLen);
    bytesConsumed = total;
    return true;
}

// Serialize a frame into a fresh byte string. Inline payload (no view variant — keep it simple
// for B2 stub; B5 can adopt span if profiling shows the copy matters).
inline std::string SerializeFrame(uint16_t flags, const Json& header,
                                   const std::vector<uint8_t>& payload = {}) {
    std::string dumped = header.dump();
    if (!payload.empty()) flags |= kFlagHasPayload;

    std::string out;
    out.reserve(kHeaderFixedBytes + dumped.size() + payload.size());
    out += static_cast<char>(kMagic0);
    out += static_cast<char>(kMagic1);
    writeLE16(out, flags);
    writeLE32(out, static_cast<uint32_t>(dumped.size()));
    writeLE32(out, static_cast<uint32_t>(payload.size()));
    out.append(dumped);
    if (!payload.empty())
        out.append(reinterpret_cast<const char*>(payload.data()), payload.size());
    return out;
}

}  // namespace frame_v2
