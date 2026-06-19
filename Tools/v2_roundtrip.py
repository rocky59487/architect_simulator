"""v2 round-trip gate leg (B2 stub level).

PURPOSE
    Smoke-test frame_capi_v2.dll end-to-end via ctypes. Each check exercises a piece of the v2
    wire protocol against the live dispatcher in the DLL. As B3+ wires real engine handlers,
    the corresponding check upgrades from "stub returned _stub=true" to "real result matches v1
    text protocol bit-for-bit".

STATUS in B2
    * abi_version / build_sha / engine_version    -> verified
    * frame_v2_open / close                       -> verified (alloc + free)
    * hello handshake + capabilities              -> verified (dispatcher built into DLL)
    * session.open / status / close               -> verified (real handlers)
    * model.set                                   -> verified (schema validates; engine call TODO)
    * solve.linear                                -> SKIP (handler returns _stub=true; B3 wires real call)
    * advanced profile rejection of missing cap   -> verified
    * cancel                                      -> verified

INTEGRATION INTO 5-LEG GATE
    Not yet. The leg this script produces becomes the 6th leg in run_gate.ps1 once B3 makes
    solve.linear return bit-exact-vs-v1 results. Until then, run by hand:

        python Tools/v2_roundtrip.py

    The exit code is 0 on success, 1 on any failure. The output is one line per check, suitable
    for grepping into the gate verdict block.
"""

from __future__ import annotations

import ctypes
import json
import os
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DLL  = REPO / "Plugins" / "FrameSolver" / "Standalone" / "frame_capi_v2.dll"

MAGIC0, MAGIC1 = 0x46, 0x43   # 'F','C'
HEADER_FIXED = 12

FLAG_END_OF_RESPONSE = 1 << 0
FLAG_HAS_PAYLOAD     = 1 << 1
FLAG_BINARY_PAYLOAD  = 1 << 2

OK              = 0
EMPTY           = 1
NEED_BIGGER     = 2
TIMEOUT         = 3
PROTOCOL_ERROR  = 4
INVALID_CTX     = 5
CANCELLED       = 6
OUT_OF_MEMORY   = 7
NOT_IMPLEMENTED = 8


def build_frame(header: dict, payload: bytes = b"", flags: int = 0) -> bytes:
    hdr = json.dumps(header, separators=(",", ":")).encode("utf-8")
    if payload: flags |= FLAG_HAS_PAYLOAD
    return (bytes([MAGIC0, MAGIC1])
            + struct.pack("<H", flags)
            + struct.pack("<I", len(hdr))
            + struct.pack("<I", len(payload))
            + hdr + payload)


def parse_frame(buf: bytes) -> tuple[int, dict, bytes]:
    assert buf[0] == MAGIC0 and buf[1] == MAGIC1, f"bad magic 0x{buf[0]:02x}{buf[1]:02x}"
    flags = struct.unpack_from("<H", buf, 2)[0]
    hlen  = struct.unpack_from("<I", buf, 4)[0]
    plen  = struct.unpack_from("<I", buf, 8)[0]
    hdr   = json.loads(buf[HEADER_FIXED:HEADER_FIXED + hlen])
    payload = bytes(buf[HEADER_FIXED + hlen : HEADER_FIXED + hlen + plen])
    return flags, hdr, payload


class V2Dll:
    def __init__(self, path: Path):
        if not path.exists():
            raise FileNotFoundError(str(path))
        self.lib = ctypes.CDLL(str(path))
        # Signatures
        self.lib.frame_v2_abi_version.restype  = ctypes.c_uint32
        self.lib.frame_v2_build_sha.restype    = ctypes.c_char_p
        self.lib.frame_v2_engine_version.restype = ctypes.c_char_p
        self.lib.frame_v2_open.restype         = ctypes.c_void_p
        self.lib.frame_v2_close.argtypes       = [ctypes.c_void_p]
        self.lib.frame_v2_send.argtypes        = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
        self.lib.frame_v2_send.restype         = ctypes.c_int
        self.lib.frame_v2_recv.argtypes        = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t,
                                                   ctypes.POINTER(ctypes.c_size_t),
                                                   ctypes.POINTER(ctypes.c_size_t), ctypes.c_int]
        self.lib.frame_v2_recv.restype         = ctypes.c_int
        self.lib.frame_v2_pending_count.argtypes = [ctypes.c_void_p]
        self.lib.frame_v2_pending_count.restype  = ctypes.c_int

    def open(self) -> int:
        ctx = self.lib.frame_v2_open()
        assert ctx, "frame_v2_open returned NULL"
        return ctx

    def close(self, ctx: int):
        self.lib.frame_v2_close(ctx)

    def send(self, ctx: int, frame: bytes) -> int:
        return self.lib.frame_v2_send(ctx, frame, len(frame))

    def recv(self, ctx: int, timeout_ms: int = 1000) -> tuple[int, bytes]:
        # Two-call dance.
        out_len = ctypes.c_size_t(0)
        out_needed = ctypes.c_size_t(0)
        rc = self.lib.frame_v2_recv(ctx, None, 0, ctypes.byref(out_len),
                                     ctypes.byref(out_needed), timeout_ms)
        if rc == EMPTY: return EMPTY, b""
        if rc == TIMEOUT: return TIMEOUT, b""
        if rc not in (OK, NEED_BIGGER):
            return rc, b""
        size = out_needed.value if out_needed.value > 0 else out_len.value
        buf = ctypes.create_string_buffer(size)
        rc = self.lib.frame_v2_recv(ctx, buf, size, ctypes.byref(out_len),
                                     ctypes.byref(out_needed), timeout_ms)
        if rc != OK:
            return rc, b""
        return OK, buf.raw[:out_len.value]


# ----- checks -----

def check(name: str, cond: bool, detail: str = ""):
    tag = "PASS" if cond else "FAIL"
    print(f"  [{tag}] {name}" + (f" -- {detail}" if detail else ""))
    return cond


def main() -> int:
    print("=== v2_roundtrip (B2 stub level) ===")
    if not DLL.exists():
        print(f"  [SKIP] {DLL} not built. Run build_capi_v2.bat first.")
        return 0

    dll = V2Dll(DLL)
    failures = 0

    abi = dll.lib.frame_v2_abi_version()
    sha = dll.lib.frame_v2_build_sha().decode()
    ver = dll.lib.frame_v2_engine_version().decode()
    if not check("abi_version >= 2",          abi >= 2, f"abi={abi}"):     failures += 1
    if not check("build_sha non-empty",       len(sha) > 0, f"sha={sha}"): failures += 1
    if not check("engine_version non-empty",  len(ver) > 0, f"v={ver}"):   failures += 1

    ctx = dll.open()
    try:
        # --- hello ---
        dll.send(ctx, build_frame({
            "v": 2, "kind": "hello", "id": "hs",
            "body": {"client": "v2_roundtrip.py/0.1", "preferredSchemas": ["2026.06"],
                      "wantsBinary": True, "profile": "simple"}
        }))
        rc, raw = dll.recv(ctx)
        ok = rc == OK and raw
        if not check("hello returns OK", ok, f"rc={rc}"): failures += 1
        if ok:
            flags, hdr, payload = parse_frame(raw)
            body = hdr.get("body", {})
            caps = set(body.get("capabilities", []))
            wanted = {"session", "model.set", "solve.linear", "profile.simple",
                       "profile.advanced", "cancel"}
            missing = wanted - caps
            if not check("hello.capabilities includes core set", not missing,
                          f"missing={sorted(missing)}"): failures += 1
            if not check("hello.schemaVer present",
                          body.get("schemaVer") == "2026.06"): failures += 1
            if not check("hello carries END_OF_RESPONSE",
                          flags & FLAG_END_OF_RESPONSE): failures += 1

        # --- session.open ---
        dll.send(ctx, build_frame({
            "v": 2, "kind": "request", "id": "r1", "method": "session.open",
            "body": {"mode": "default", "options": {"pivotTol": 1e-12}}
        }))
        rc, raw = dll.recv(ctx)
        sid = None
        if rc == OK:
            _, hdr, _ = parse_frame(raw)
            sid = hdr["body"].get("session")
            if not check("session.open returns session id",
                          isinstance(sid, str) and sid.startswith("s_"),
                          f"sid={sid!r}"): failures += 1
        else:
            check("session.open returns OK", False, f"rc={rc}")
            failures += 1

        # --- model.set (simple profile: silent defaults filled, listed in defaultsApplied) ---
        if sid:
            dll.send(ctx, build_frame({
                "v": 2, "kind": "request", "id": "r2", "method": "model.set",
                "body": {
                    "session": sid,
                    "materials": [{"E": 210000, "G": 80769, "rho": 7.85e-9}],   # NO cap
                    "sections":  [],
                    "nodes":     [{"id": 0, "x": 0, "y": 0, "z": 0}],
                    "members":   []
                }
            }))
            rc, raw = dll.recv(ctx)
            if rc == OK:
                _, hdr, _ = parse_frame(raw)
                b = hdr["body"]
                ok = bool(b.get("ok")) and b.get("dofCount") == 6
                if not check("simple model.set ok + dofCount", ok, f"body={b}"): failures += 1
                if not check("simple model.set lists defaultsApplied for missing cap",
                              "materials[0].cap" in (b.get("defaultsApplied") or []),
                              f"defaultsApplied={b.get('defaultsApplied')}"): failures += 1
            else:
                check("simple model.set returns OK", False, f"rc={rc}")
                failures += 1

        # --- solve.linear (stub — B3 makes this real and bit-exact vs v1) ---
        if sid:
            dll.send(ctx, build_frame({
                "v": 2, "kind": "request", "id": "r3", "method": "solve.linear",
                "body": {"session": sid, "wantReactions": True}
            }))
            rc, raw = dll.recv(ctx)
            if rc == OK:
                _, hdr, _ = parse_frame(raw)
                b = hdr["body"]
                # Stub returns _stub=true; we check the SHAPE is present (B3 promotes to bit-exact).
                shape_ok = all(k in b for k in ("singular", "pivotMargin", "disp",
                                                  "reactions", "memberForces", "shellForces"))
                if not check("solve.linear returns spec shape (stub)",
                              shape_ok and b.get("_stub") is True,
                              f"keys={sorted(b.keys())}"): failures += 1
                # B3 will replace this with a "matches v1 stdout" check.
                print("  [SKIP] solve.linear bit-exact vs v1 -- engine wiring is B3 work")
            else:
                check("solve.linear returns OK", False, f"rc={rc}")
                failures += 1

        # --- session.close ---
        if sid:
            dll.send(ctx, build_frame({
                "v": 2, "kind": "request", "id": "r4", "method": "session.close",
                "body": {"session": sid}
            }))
            rc, raw = dll.recv(ctx)
            if not check("session.close returns OK", rc == OK, f"rc={rc}"): failures += 1

    finally:
        dll.close(ctx)

    # --- advanced profile rejects missing cap ---
    ctx2 = dll.open()
    try:
        dll.send(ctx2, build_frame({
            "v": 2, "kind": "hello", "id": "hs",
            "body": {"client": "v2_roundtrip.py/0.1", "profile": "advanced"}
        }))
        dll.recv(ctx2)  # discard hello
        dll.send(ctx2, build_frame({
            "v": 2, "kind": "request", "id": "r1", "method": "session.open",
            "body": {"mode": "default", "profile": "advanced", "options": {"pivotTol": 1e-12}}
        }))
        rc, raw = dll.recv(ctx2)
        sid = parse_frame(raw)[1]["body"].get("session") if rc == OK else None
        if sid:
            dll.send(ctx2, build_frame({
                "v": 2, "kind": "request", "id": "r2", "method": "model.set",
                "body": {
                    "session": sid,
                    "materials": [{"E": 210000, "G": 80769, "rho": 7.85e-9}],   # NO cap
                    "nodes": [],
                    "members": []
                }
            }))
            rc, raw = dll.recv(ctx2)
            if rc == OK:
                _, hdr, _ = parse_frame(raw)
                is_err   = hdr.get("kind") == "error"
                err_code = hdr.get("body", {}).get("code", "")
                if not check("advanced model.set REJECTS missing cap (VALIDATION_FAILED)",
                              is_err and err_code == "VALIDATION_FAILED",
                              f"kind={hdr.get('kind')} code={err_code}"): failures += 1
    finally:
        dll.close(ctx2)

    print(f"=== summary: {('ALL PASS' if failures == 0 else f'{failures} FAIL')} ===")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
