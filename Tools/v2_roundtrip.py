"""v2 round-trip gate leg (B2 stub level).

PURPOSE
    Smoke-test frame_capi_v2.dll end-to-end via ctypes. Each check exercises a piece of the v2
    wire protocol against the live dispatcher in the DLL. As B3+ wires real engine handlers,
    the corresponding check upgrades from "stub returned _stub=true" to "real result matches v1
    text protocol bit-for-bit".

STATUS (B3 wire level)
    * abi_version / build_sha / engine_version    -> verified
    * frame_v2_open / close                       -> verified (alloc + free)
    * hello handshake + capabilities              -> verified (dispatcher built into DLL)
    * session.open / status / close               -> verified (real handlers)
    * model.set                                   -> verified (schema validates + builds FrameModel)
    * solve.linear                                -> verified (bit-exact vs v1 frame_capi.dll oracle)
    * advanced profile rejection of missing cap   -> verified
    * cancel                                      -> verified

INTEGRATION INTO 5-LEG GATE
    B3 promotes the SKIP to a PASS (bit-exact-vs-v1 on the cantilever fixture). The leg can
    enter run_gate.ps1 as the 6th leg whenever the orchestration accepts the conda OpenBLAS
    dependency (same as the standalone leg). Until then, run by hand:

        python Tools/v2_roundtrip.py

    The exit code is 0 on success, 1 on any failure. The output is one line per check, suitable
    for grepping into the gate verdict block.
"""

from __future__ import annotations

import ctypes
import json
import os
import re
import struct
import sys
from pathlib import Path

# Semver-ish: MAJOR.MINOR(.PATCH)? — accepts "2.8.1" / "2.9" / "3.0.0-dirty"-no.
# The literal check below catches a build that forgot to bump kEngineVer in Dispatcher.h
# when the rest of the release was tagged; the regex catches a build whose version string
# was corrupted to something non-numeric (a v2.5 audit found a `kEngineVer` literal that
# silently drifted 3 minor versions behind the tag).
ENGINE_VER_RE = re.compile(r"^\d+\.\d+(?:\.\d+)?$")
# Optional pin from env: set FRAMECORE_EXPECTED_ENGINE_VER=2.8.1 to enforce a literal match.
EXPECTED_ENGINE_VER = os.environ.get("FRAMECORE_EXPECTED_ENGINE_VER", "").strip()

REPO = Path(__file__).resolve().parent.parent
# Allow run_gpu_gate.ps1 to point at frame_capi_v2_cuda.dll without touching v2_roundtrip.
# Set FRAMECORE_V2_DLL to an absolute path; default stays the CPU-only dispatcher build.
_V2_DLL_OVERRIDE = os.environ.get("FRAMECORE_V2_DLL", "").strip()
DLL    = Path(_V2_DLL_OVERRIDE) if _V2_DLL_OVERRIDE \
         else REPO / "Plugins" / "FrameSolver" / "Standalone" / "frame_capi_v2.dll"

# On Python 3.8+, LoadLibrary does NOT search PATH by default. The CUDA-build dispatcher
# transitively depends on cuDSS / cuSPARSE / cuBLAS / cudart / nvJitLink (~7 DLLs) under
# the conda env. FRAMECORE_V2_DLL_DEPS_DIRS = "dir1;dir2" makes those paths visible to
# the DLL loader for this process only -- nothing global, nothing leaks.
for _dep_dir in os.environ.get("FRAMECORE_V2_DLL_DEPS_DIRS", "").split(os.pathsep):
    _dep_dir = _dep_dir.strip()
    if _dep_dir and os.path.isdir(_dep_dir) and hasattr(os, "add_dll_directory"):
        os.add_dll_directory(_dep_dir)
V1_DLL = REPO / "Plugins" / "FrameSolver" / "Standalone" / "frame_capi.dll"

# Python 3.8+ on Windows ignores %PATH% for native DLL dependencies; explicitly add the conda
# OpenBLAS/METIS bin dir so frame_capi_v2.dll (B3 wire links openblas.dll + metis.dll) can load.
# If conda was missing at build time the DLL was built with FRAMECORE_SUPERNODAL=0 and has no
# openblas dep -- the add_dll_directory call is then a harmless no-op.
if sys.platform == "win32":
    _conda_bin = os.environ.get(
        "SUPERNODAL_CONDA",
        str(Path(os.environ.get("USERPROFILE", "")) / "anaconda3" / "envs" / "framecore-direct" / "Library")
    )
    _conda_bin = Path(_conda_bin) / "bin"
    if _conda_bin.exists():
        os.add_dll_directory(str(_conda_bin))

# --- shared "cantilever" fixture used by model.set + solve.linear bit-exact-vs-v1 check ----
# Cantilever (fixed at id=0, free at id=1, length 1000), one 200x200 section beam, tip load
# Fy=-1000. Same model expressed two ways (v1 text + v2 JSON) so the dispatcher response can
# be compared element-by-element to v1's text-protocol output.
CANTILEVER_V1_TEXT = (
    "MAT 210000 80769 0\n"
    "SEC 200 2000 2000 400 10 10 0 0\n"
    "NODE 0 0 0 0  1 1 1 1 1 1  0 0 0 0 0 0\n"
    "NODE 1 1000 0 0  0 0 0 0 0 0  0 0 0 0 0 0\n"
    "MEMBER 0 0 1 0 0  0 0 1\n"
    "NLOAD 1 0 -1000 0 0 0 0\n"
    "END\n"
)
CANTILEVER_V2_JSON = {
    "materials": [{"E": 210000, "G": 80769, "rho": 0}],
    "sections":  [{"A": 200, "Iy": 2000, "Iz": 2000, "J": 400, "cy": 10, "cz": 10}],
    "nodes": [
        {"id": 0, "x": 0, "y": 0, "z": 0, "fixed": [True, True, True, True, True, True]},
        {"id": 1, "x": 1000, "y": 0, "z": 0}
    ],
    "members":   [{"id": 0, "i": 0, "j": 1, "mat": 0, "sec": 0, "ref": [0, 0, 1]}],
    "nodalLoads": [{"node": 1, "comp": [0, -1000, 0, 0, 0, 0]}],
}

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
        self.lib.frame_v2_cancel_request.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        self.lib.frame_v2_cancel_request.restype  = ctypes.c_int

    def open(self) -> int:
        ctx = self.lib.frame_v2_open()
        assert ctx, "frame_v2_open returned NULL"
        return ctx

    def close(self, ctx: int):
        self.lib.frame_v2_close(ctx)

    def send(self, ctx: int, frame: bytes) -> int:
        return self.lib.frame_v2_send(ctx, frame, len(frame))

    def cancel_request(self, ctx: int, target_id: str) -> int:
        return self.lib.frame_v2_cancel_request(ctx, target_id.encode("utf-8"))

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


# ----- v1 oracle (frame_capi.dll text protocol) -----

def v1_solve_text(text: str) -> str:
    """Drive the v1 C ABI DLL on the same fixture so v2 results can be diff'd. Mirrors
    Tools/cli_roundtrip.py::capi_solve so the wire-level behaviour is identical."""
    lib = ctypes.CDLL(str(V1_DLL))
    lib.frame_capi_solve_text.restype  = ctypes.c_int
    lib.frame_capi_solve_text.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
    b = (text if text.rstrip().endswith("END") else text.rstrip() + "\nEND\n").encode()
    n = lib.frame_capi_solve_text(b, None, 0)
    buf = ctypes.create_string_buffer(n + 1)
    lib.frame_capi_solve_text(b, buf, n + 1)
    return buf.value.decode(errors="replace")


def parse_v1_output(text: str) -> dict:
    """Parse v1 DISP/RF/MF lines into the dispatcher's response shape so a per-element diff
    is straightforward. Unknown tags are ignored (forward-compatible with v1)."""
    disp, rxn, mf = {}, {}, {}
    for line in text.splitlines():
        parts = line.split()
        if not parts:
            continue
        tag = parts[0]
        if tag == "DISP" and len(parts) >= 8:
            disp[int(parts[1])] = [float(x) for x in parts[2:8]]
        elif tag == "RF" and len(parts) >= 8:
            rxn[int(parts[1])] = [float(x) for x in parts[2:8]]
        elif tag == "MF" and len(parts) >= 14:
            mf[int(parts[1])] = {
                "endI": [float(x) for x in parts[2:8]],
                "endJ": [float(x) for x in parts[8:14]],
            }
    return {"disp": disp, "reactions": rxn, "memberForces": mf}


def diff_vs_v1(v2_body: dict, v1_parsed: dict, tol: float = 1e-12) -> tuple[bool, str]:
    """Compare every disp/RF/MF entry between v1 (oracle) and v2 (dispatcher), using a
    relative tolerance scaled by the per-key magnitude. Returns (ok, first-mismatch-detail)."""
    def cmp_vec(a, b, where: str) -> tuple[bool, str]:
        if len(a) != len(b):
            return False, f"{where}: len mismatch v1={len(a)} v2={len(b)}"
        for k, (av, bv) in enumerate(zip(a, b)):
            scale = max(1.0, abs(av))
            if abs(av - bv) > tol * scale:
                return False, f"{where}[{k}]: v1={av:.12g} v2={bv:.12g} diff={abs(av-bv):.3e}"
        return True, ""

    for key in ("disp", "reactions"):
        v1m, v2m = v1_parsed[key], v2_body.get(key) or {}
        if set(v1m.keys()) != set(int(k) for k in v2m.keys()):
            return False, f"{key}: id sets differ v1={sorted(v1m.keys())} v2={sorted(v2m.keys())}"
        for nid, v in v1m.items():
            ok, why = cmp_vec(v, v2m[str(nid)], f"{key}[node={nid}]")
            if not ok:
                return False, why

    v1mf, v2mf = v1_parsed["memberForces"], v2_body.get("memberForces") or {}
    if set(v1mf.keys()) != set(int(k) for k in v2mf.keys()):
        return False, f"memberForces: id sets differ v1={sorted(v1mf.keys())} v2={sorted(v2mf.keys())}"
    for mid, mp in v1mf.items():
        v2mp = v2mf[str(mid)]
        for end in ("endI", "endJ"):
            ok, why = cmp_vec(mp[end], v2mp[end], f"memberForces[m={mid}].{end}")
            if not ok:
                return False, why
    return True, ""


# ----- checks -----

def check(name: str, cond: bool, detail: str = ""):
    tag = "PASS" if cond else "FAIL"
    print(f"  [{tag}] {name}" + (f" -- {detail}" if detail else ""))
    return cond


def main() -> int:
    print("=== v2_roundtrip (B3 wire level: solve.linear bit-exact vs v1) ===")
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
    if not check("engine_version semver-ish", bool(ENGINE_VER_RE.match(ver)), f"v={ver}"): failures += 1
    if EXPECTED_ENGINE_VER:
        if not check(f"engine_version == {EXPECTED_ENGINE_VER} (env pin)",
                      ver == EXPECTED_ENGINE_VER, f"got={ver}"): failures += 1

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
            wanted = {"session", "model.set", "solve.linear", "solve.dyn_collapse",
                       "analysis.reanalysis_solve", "profile.simple", "profile.advanced",
                       "cancel", "transport.async", "dyn_collapse.live",
                       # v3.1.0 (S11): stress-field post-process. Advertised regression guard
                       # so a future refactor that drops the capability string from
                       # Capabilities() fails this gate.
                       "inspect.stress_field"}
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
        # B3 wire: the model is a real cantilever with 1 member (FrameModel::validate() requires
        # >= 1 member or 1 shell). Missing 'cap' on materials is still silently defaulted and
        # surfaced through defaultsApplied -- that contract is unchanged from B2.
        if sid:
            simple_body = {"session": sid, **CANTILEVER_V2_JSON}
            dll.send(ctx, build_frame({
                "v": 2, "kind": "request", "id": "r2", "method": "model.set",
                "body": simple_body
            }))
            rc, raw = dll.recv(ctx)
            if rc == OK:
                _, hdr, _ = parse_frame(raw)
                b = hdr["body"]
                ok = bool(b.get("ok")) and b.get("dofCount") == 12
                if not check("simple model.set ok + dofCount", ok, f"body={b}"): failures += 1
                if not check("simple model.set lists defaultsApplied for missing cap",
                              "materials[0].cap" in (b.get("defaultsApplied") or []),
                              f"defaultsApplied={b.get('defaultsApplied')}"): failures += 1
            else:
                check("simple model.set returns OK", False, f"rc={rc}")
                failures += 1

            dup_nodes = dict(CANTILEVER_V2_JSON)
            dup_nodes["nodes"] = [
                {"id": 0, "x": 0, "y": 0, "z": 0, "fixed": [True, True, True, True, True, True]},
                {"id": 0, "x": 1000, "y": 0, "z": 0},
            ]
            dll.send(ctx, build_frame({
                "v": 2, "kind": "request", "id": "r2_dup", "method": "model.set",
                "body": {"session": sid, **dup_nodes}
            }))
            rc, raw = dll.recv(ctx)
            dup_rejected = False
            if rc == OK:
                _, hdr, _ = parse_frame(raw)
                dup_rejected = (hdr.get("kind") == "error"
                                and hdr.get("body", {}).get("code") == "VALIDATION_FAILED"
                                and "duplicates node id" in hdr.get("body", {}).get("message", ""))
            if not check("model.set rejects duplicate node ids", dup_rejected,
                          f"rc={rc} hdr={(parse_frame(raw)[1] if rc == OK else None)}"):
                failures += 1

        # --- solve.linear (B3 wired: bit-exact vs v1 frame_capi.dll on the same fixture) ---
        if sid:
            dll.send(ctx, build_frame({
                "v": 2, "kind": "request", "id": "r3", "method": "solve.linear",
                "body": {"session": sid, "wantReactions": True, "wantDC": True}
            }))
            rc, raw = dll.recv(ctx)
            if rc == OK:
                _, hdr, _ = parse_frame(raw)
                b = hdr["body"]
                shape_ok = all(k in b for k in ("singular", "pivotMargin", "disp",
                                                  "reactions", "memberForces", "shellForces",
                                                  "memberUtilization", "utilization"))
                if not check("solve.linear returns spec shape", shape_ok,
                              f"keys={sorted(b.keys())}"): failures += 1
                dc_ok = (isinstance(b.get("memberUtilization"), dict)
                         and "0" in b["memberUtilization"]
                         and b["memberUtilization"]["0"].get("peak", -1) >= 0
                         and b.get("utilization", {}).get("maxDC", -1) >= 0)
                if not check("solve.linear wantDC returns native D/C", dc_ok,
                              f"memberUtilization={b.get('memberUtilization')} utilization={b.get('utilization')}"):
                    failures += 1
                if not check("solve.linear not a stub (engine wired)",
                              b.get("_stub") is None, f"_stub={b.get('_stub')}"): failures += 1
                if not check("solve.linear not singular on cantilever",
                              b.get("singular") is False, f"singular={b.get('singular')}"):
                    failures += 1
                if V1_DLL.exists() and not b.get("singular"):
                    v1_out = v1_solve_text(CANTILEVER_V1_TEXT)
                    v1_parsed = parse_v1_output(v1_out)
                    # Tolerance is bounded by v1's "%.12g" text-protocol print precision: a 12-
                    # significant-digit print round-trips with rel ~5e-13 in the worst case, plus
                    # 1 ULP of the float reconstruction. 1e-11 leaves headroom but still catches
                    # a real numeric divergence (e.g. swapping LDLT for a different factor).
                    ok, why = diff_vs_v1(b, v1_parsed, tol=1e-11)
                    if not check("solve.linear bit-exact vs v1 (cantilever, rel<1e-11)", ok, why):
                        failures += 1
                    solve_linear_body = b
                else:
                    print("  [SKIP] v1 bit-exact compare -- frame_capi.dll (v1) not built")
                    solve_linear_body = b
            else:
                check("solve.linear returns OK", False, f"rc={rc}")
                failures += 1
                solve_linear_body = None

        # --- B3.3: inspect.* family -- bit-exact against the cached SolveResult from solve.linear ---
        def _post(rid, method, extra=None):
            body = {"session": sid}
            if extra:
                body.update(extra)
            dll.send(ctx, build_frame({
                "v": 2, "kind": "request", "id": rid, "method": method, "body": body
            }))
            rc_, raw_ = dll.recv(ctx)
            if rc_ != OK:
                return None, f"rc={rc_}"
            _, hdr_, _ = parse_frame(raw_)
            if hdr_.get("body", {}).get("code"):
                bd = hdr_["body"]
                return None, f"{bd.get('code')}: {bd.get('message')}"
            return hdr_.get("body"), ""

        if sid and solve_linear_body is not None and not solve_linear_body.get("singular"):
            for rid, method, key in [
                ("ri1", "inspect.disp",          "disp"),
                ("ri2", "inspect.reactions",     "reactions"),
                ("ri3", "inspect.member_forces", "memberForces"),
                ("ri4", "inspect.shell_forces",  "shellForces"),
            ]:
                body, why = _post(rid, method)
                if body is None:
                    check(f"{method} returns OK", False, why); failures += 1; continue
                if body.get(key) != solve_linear_body.get(key):
                    check(f"{method} == solve.linear.{key} (bit-exact)", False,
                          f"diff at key {key}"); failures += 1
                else:
                    check(f"{method} == solve.linear.{key} (bit-exact)", True)

            # v3.1.0 (S11): inspect.stress_field shape check. No bit-exact partner in
            # solve.linear (the field is a derived post-process); verify the JSON schema
            # documented in docs/specs/S11_stress_field.md and the no-shell sentinel.
            body, why = _post("ri5", "inspect.stress_field")
            if body is None:
                check("inspect.stress_field returns OK", False, why); failures += 1
            else:
                sf = body.get("stressField")
                if not check("inspect.stress_field has stressField key",
                              isinstance(sf, dict), f"body keys={sorted(body.keys())}"):
                    failures += 1
                else:
                    if not check("stress_field.samplesPerSpan == 11",
                                  sf.get("samplesPerSpan") == 11,
                                  f"got={sf.get('samplesPerSpan')}"): failures += 1
                    mems = sf.get("members")
                    if not check("stress_field.members is a 1-element list (cantilever)",
                                  isinstance(mems, list) and len(mems) == 1,
                                  f"got={type(mems).__name__} len={len(mems) if isinstance(mems,list) else 'n/a'}"):
                        failures += 1
                    elif True:
                        samples = mems[0].get("samples")
                        if not check("stress_field.members[0].samples len == 11",
                                      isinstance(samples, list) and len(samples) == 11,
                                      f"got={len(samples) if isinstance(samples,list) else type(samples).__name__}"):
                            failures += 1
                        if not check("stress_field.members[0].memberId == 0",
                                      mems[0].get("memberId") == 0,
                                      f"got={mems[0].get('memberId')}"): failures += 1
                    if not check("stress_field.governingMemberId == 0 (cantilever id)",
                                  sf.get("governingMemberId") == 0,
                                  f"got={sf.get('governingMemberId')}"): failures += 1
                    if not check("stress_field.governingShellId == -1 (no-shell sentinel)",
                                  sf.get("governingShellId") == -1,
                                  f"got={sf.get('governingShellId')}"): failures += 1
                    if not check("stress_field.shellsTop/shellsBot are empty (no shells)",
                                  sf.get("shellsTop") == [] and sf.get("shellsBot") == [],
                                  f"top={len(sf.get('shellsTop',[]))} bot={len(sf.get('shellsBot',[]))}"):
                        failures += 1
                    sigma = sf.get("globalMaxFiberSigma")
                    if not check("stress_field.globalMaxFiberSigma > 0",
                                  isinstance(sigma, (int, float)) and sigma > 0,
                                  f"got={sigma}"): failures += 1

            # samplesPerSpan range guard: < 2 and > 1024 must be VALIDATION_FAILED.
            body, why = _post("ri5_bad_low", "inspect.stress_field", {"samplesPerSpan": 1})
            if not check("inspect.stress_field samplesPerSpan=1 -> error",
                          body is None and "VALIDATION_FAILED" in why,
                          f"body={body} why={why}"): failures += 1
            body, why = _post("ri5_bad_high", "inspect.stress_field", {"samplesPerSpan": 2048})
            if not check("inspect.stress_field samplesPerSpan=2048 -> error",
                          body is None and "VALIDATION_FAILED" in why,
                          f"body={body} why={why}"): failures += 1

        # --- B3/B5: analysis methods -- shape-level check (no more NOT_IMPLEMENTED) ---
        if sid:
            method_specs = [
                # (rid, method, body_overrides, required-result-keys)
                ("rp",  "solve.pdelta",       None,
                 ("converged", "diverged", "iterations", "lastIncrement", "finalState")),
                ("rt",  "solve.tension_only", None,
                 ("converged", "cycled", "iterations", "slack", "finalState")),
                ("rs",  "solve.size_opt",     {"Amin": 1.0, "maxIter": 2},
                 ("iterations", "finalAreas", "finalDC")),
                ("rc",  "solve.corotational", {"loadSteps": 2, "maxIter": 10},
                 ("converged", "diverged", "loadStepsCompleted", "finalState")),
                # arc-length needs an explicit arcLength for the cantilever; pass a small Dl so it
                # at least returns a structured response (not bit-exact -- the geometry is not a
                # classic snap-through fixture, but the dispatcher path is what we are validating).
                ("ra",  "solve.arclength",    {"arcLength": 1.0, "arcSteps": 2, "maxIter": 20},
                 ("converged", "diverged", "pathLambda", "pathDisp", "finalState")),
                # modal needs Material.rho > 0; the cantilever has rho=0 so we expect a structured
                # 'singular: true' (consistent with engine behaviour), not a NOT_IMPLEMENTED.
                ("rm",  "analysis.modal",     {"numModes": 1},
                 ("singular", "modes")),
                ("rb",  "analysis.buckling",  None,
                 ("singular", "criticalFactor", "reportedCriticalFactor", "knockdownFactor", "mode")),
                ("rr",  "analysis.reanalysis_solve", {"memberActive": [{"id": 0, "active": True}]},
                 ("valid", "stats", "finalState")),
            ]
            for rid, method, extra, required in method_specs:
                body, why = _post(rid, method, extra)
                if body is None:
                    check(f"{method} returns OK (B3.3 wire, not NOT_IMPLEMENTED)", False, why)
                    failures += 1
                    continue
                missing = [k for k in required if k not in body]
                if not check(f"{method} returns spec shape", not missing,
                              f"missing={missing} keys={sorted(body.keys())}"):
                    failures += 1

            # B4 dynamic collapse may return Invalid on the zero-density cantilever, but the
            # dispatcher contract must still be a structured final response, not NOT_IMPLEMENTED.
            dll.send(ctx, build_frame({
                "v": 2, "kind": "request", "id": "rdc", "method": "solve.dyn_collapse",
                "body": {"session": sid, "dt": 0.001, "maxTime": 0.001,
                         "frameStride": 1, "streamFrames": False, "streamEvents": False}
            }))
            final = None
            for _ in range(8):
                rc, raw = dll.recv(ctx)
                if rc != OK:
                    break
                flags, hdr, _ = parse_frame(raw)
                if flags & FLAG_END_OF_RESPONSE:
                    final = hdr
                    break
            dyn_ok = (final is not None
                      and final.get("kind") == "response"
                      and all(k in final.get("body", {}) for k in ("outcome", "nEvents", "nFrames", "events")))
            if not check("solve.dyn_collapse returns final summary shape", dyn_ok,
                          f"final={final} rc={rc if final is None else OK}"):
                failures += 1

            # P1-3 (v2.7) live streaming: re-open a session with non-zero density and run a
            # short dyn_collapse with streamFrames=True. The dispatcher pushes each
            # dyn_collapse.frame event WHILE the integrator runs (via the engine's
            # onFrameEmitted callback), then a single final response. Verify that:
            #   (a) at least one frame event arrived before the response,
            #   (b) the live event count matches the final summary's nFrames,
            #   (c) frame events arrive interleaved BEFORE the response kind (not after).
            dll.send(ctx, build_frame({
                "v": 2, "kind": "request", "id": "rdc_l1", "method": "session.open",
                "body": {"mode": "default"}
            }))
            rc_l1, raw_l1 = dll.recv(ctx)
            if rc_l1 == OK:
                _, hdr_l1, _ = parse_frame(raw_l1)
                sid_l = hdr_l1["body"].get("session")
                mass_model = dict(CANTILEVER_V2_JSON)
                mass_model["materials"] = [{"E": 210000, "G": 80769, "rho": 7.85e-9}]
                if sid_l:
                    dll.send(ctx, build_frame({
                        "v": 2, "kind": "request", "id": "rdc_lm", "method": "model.set",
                        "body": {"session": sid_l, **mass_model}
                    }))
                    dll.recv(ctx)
                    dll.send(ctx, build_frame({
                        "v": 2, "kind": "request", "id": "rdc_lv", "method": "solve.dyn_collapse",
                        "body": {"session": sid_l, "dt": 0.001, "maxTime": 0.005,
                                 "frameStride": 1, "streamFrames": True, "streamEvents": False,
                                 "binaryFrames": False, "initialRemovals": []}
                    }))
                    live_frames = 0
                    final_l = None
                    for _ in range(64):
                        rc_x, raw_x = dll.recv(ctx)
                        if rc_x != OK: break
                        _, hdr_x, _ = parse_frame(raw_x)
                        kind = hdr_x.get("kind")
                        body_x = hdr_x.get("body", {})
                        if kind == "event" and body_x.get("channel") == "dyn_collapse.frame":
                            live_frames += 1
                        elif kind in ("response", "error"):
                            final_l = hdr_x
                            break
                    n_frames_final = (final_l.get("body", {}).get("nFrames", -1)
                                      if final_l is not None else -1)
                    ok_live = (final_l is not None
                               and final_l.get("kind") == "response"
                               and live_frames >= 1
                               and live_frames == n_frames_final)
                    if not check(
                        "P1-3: solve.dyn_collapse live emits frames during run "
                        "(live count matches final nFrames)",
                        ok_live,
                        f"live_frames={live_frames} nFrames={n_frames_final} "
                        f"final={None if final_l is None else final_l.get('kind')}"):
                        failures += 1
                    dll.send(ctx, build_frame({
                        "v": 2, "kind": "request", "id": "rdc_lc",
                        "method": "session.close", "body": {"session": sid_l}
                    }))
                    dll.recv(ctx)

            # R2.3 (v2.9) live event streaming: with mass + an initialRemovals scenario the
            # dispatcher should push at least one dyn_collapse.event channel WHILE
            # runDynamicCollapse runs (via the engine's onEventEmitted callback), the count
            # equal to the final summary's nEvents. liveEvents defaults to true so this is the
            # default contract starting v2.9.
            dll.send(ctx, build_frame({
                "v": 2, "kind": "request", "id": "rdc_e1", "method": "session.open",
                "body": {"mode": "default"}
            }))
            rc_e1, raw_e1 = dll.recv(ctx)
            if rc_e1 == OK:
                _, hdr_e1, _ = parse_frame(raw_e1)
                sid_e = hdr_e1["body"].get("session")
                if sid_e:
                    mass_model_e = dict(CANTILEVER_V2_JSON)
                    mass_model_e["materials"] = [{"E": 210000, "G": 80769, "rho": 7.85e-9}]
                    dll.send(ctx, build_frame({
                        "v": 2, "kind": "request", "id": "rdc_em", "method": "model.set",
                        "body": {"session": sid_e, **mass_model_e}
                    }))
                    dll.recv(ctx)
                    # initialRemovals = [0] triggers the t=0 brittle-removal event; the
                    # cantilever has at least member id 0 in the fixture. mass + frameStride=1
                    # keeps the run short.
                    dll.send(ctx, build_frame({
                        "v": 2, "kind": "request", "id": "rdc_ev", "method": "solve.dyn_collapse",
                        "body": {"session": sid_e, "dt": 0.001, "maxTime": 0.005,
                                 "frameStride": 1, "streamFrames": False, "streamEvents": True,
                                 "liveEvents": True, "binaryFrames": False,
                                 "initialRemovals": [0]}
                    }))
                    live_events = 0
                    final_e = None
                    for _ in range(64):
                        rc_x, raw_x = dll.recv(ctx)
                        if rc_x != OK: break
                        _, hdr_x, _ = parse_frame(raw_x)
                        kind = hdr_x.get("kind")
                        body_x = hdr_x.get("body", {})
                        if kind == "event" and body_x.get("channel") == "dyn_collapse.event":
                            live_events += 1
                        elif kind in ("response", "error"):
                            final_e = hdr_x
                            break
                    n_events_final = (final_e.get("body", {}).get("nEvents", -1)
                                      if final_e is not None else -1)
                    ok_live_ev = (final_e is not None
                                  and final_e.get("kind") == "response"
                                  and live_events == n_events_final)
                    if not check(
                        "R2.3: solve.dyn_collapse live events stream during run "
                        "(live count == final nEvents)",
                        ok_live_ev,
                        f"live_events={live_events} nEvents={n_events_final} "
                        f"final={None if final_e is None else final_e.get('kind')}"):
                        failures += 1
                    dll.send(ctx, build_frame({
                        "v": 2, "kind": "request", "id": "rdc_ec",
                        "method": "session.close", "body": {"session": sid_e}
                    }))
                    dll.recv(ctx)
            # Verify the capability is advertised by the v2.9 hello handshake.
            if not check("R2.3: hello.capabilities advertises 'dyn_collapse.live.events'",
                          "dyn_collapse.live.events" in caps,
                          f"caps subset has dyn_collapse.live.events? {'dyn_collapse.live.events' in caps}"):
                failures += 1
            # R2.3 (v2.10.1) GPU lane wire-up: 'solve.linear.gpu_backsub' should appear only
            # on engine binaries built with -DFRAMECORE_CUDA=1. The default frame_capi_v2.dll
            # (build_capi_v2.bat) does NOT define FRAMECORE_CUDA, so the capability must be
            # ABSENT here. The CUDA-enabled build (build_capi_v2_cuda.bat ->
            # frame_capi_v2_cuda.dll) advertises it; cover that in run_gpu_gate.ps1.
            expected_gpu_cap = os.environ.get("FRAMECORE_EXPECTED_GPU_CAP", "").lower() == "true"
            gpu_cap_present  = "solve.linear.gpu_backsub" in caps
            label = "R2.3 (v2.10.1): 'solve.linear.gpu_backsub' presence matches build flavour"
            if not check(label, gpu_cap_present == expected_gpu_cap,
                          f"expected={expected_gpu_cap} got={gpu_cap_present}"):
                failures += 1

            # session.open echoes the gpuBacksub flag. The default build should still accept
            # the body field; the engine just won't use the GPU lane. Round-trip the value to
            # prove the dispatcher is wired up (the response should mirror what we asked).
            dll.send(ctx, build_frame({
                "v": 2, "kind": "request", "id": "rgo", "method": "session.open",
                "body": {"mode": "supernodal", "gpuBacksub": True}
            }))
            rc_go, raw_go = dll.recv(ctx)
            ok_go = rc_go == OK and raw_go
            if not check("R2.3: session.open accepts body.gpuBacksub", ok_go, f"rc={rc_go}"):
                failures += 1
            elif ok_go:
                _, hdr_go, _ = parse_frame(raw_go)
                body_go = hdr_go.get("body", {})
                if not check("R2.3: session.open echoes gpuBacksub=true",
                              body_go.get("gpuBacksub") is True,
                              f"body={body_go}"):
                    failures += 1
                # Close the test session immediately so we don't leak it.
                sid_go = body_go.get("session", "")
                if sid_go:
                    dll.send(ctx, build_frame({
                        "v": 2, "kind": "request", "id": "rgoc",
                        "method": "session.close", "body": {"session": sid_go}
                    }))
                    dll.recv(ctx)

        # --- session.close ---
        if sid:
            dll.send(ctx, build_frame({
                "v": 2, "kind": "request", "id": "r4", "method": "session.close",
                "body": {"session": sid}
            }))
            rc, raw = dll.recv(ctx)
            if not check("session.close returns OK", rc == OK, f"rc={rc}"): failures += 1

        # --- B5: session.open mode=supernodal -> solve.linear reuses SnSession factor ---
        # Same cantilever fixture; SnSession internally falls back to LDLT on failure, so the
        # disp/RF/MF should still match v1 bit-exact (rel<1e-11) -- proving the supernodal lane
        # routes correctly AND the LDLT fallback rail is intact.
        dll.send(ctx, build_frame({
            "v": 2, "kind": "request", "id": "rb5_o", "method": "session.open",
            "body": {"mode": "supernodal", "options": {"pivotTol": 1e-12}}
        }))
        rc, raw = dll.recv(ctx)
        if rc == OK:
            _, hdr, _ = parse_frame(raw)
            sid_sn = hdr["body"].get("session")
            if hdr["body"].get("mode") != "supernodal":
                check("session.open(supernodal) returns mode='supernodal'", False,
                      f"mode={hdr['body'].get('mode')}"); failures += 1
            else:
                check("session.open(supernodal) returns mode='supernodal'", True)
            if sid_sn:
                dll.send(ctx, build_frame({
                    "v": 2, "kind": "request", "id": "rb5_m", "method": "model.set",
                    "body": {"session": sid_sn, **CANTILEVER_V2_JSON}
                }))
                rc2, raw2 = dll.recv(ctx)
                if rc2 != OK:
                    check("supernodal session.model.set OK", False, f"rc={rc2}"); failures += 1
                else:
                    dll.send(ctx, build_frame({
                        "v": 2, "kind": "request", "id": "rb5_s", "method": "solve.linear",
                        "body": {"session": sid_sn, "wantReactions": True}
                    }))
                    rc3, raw3 = dll.recv(ctx)
                    if rc3 != OK:
                        check("supernodal solve.linear OK", False, f"rc={rc3}"); failures += 1
                    else:
                        _, hdr3, _ = parse_frame(raw3)
                        b3 = hdr3["body"]
                        if not check("supernodal solve.linear not singular",
                                      b3.get("singular") is False, f"sing={b3.get('singular')}"):
                            failures += 1
                        if V1_DLL.exists() and not b3.get("singular"):
                            v1_parsed = parse_v1_output(v1_solve_text(CANTILEVER_V1_TEXT))
                            ok, why = diff_vs_v1(b3, v1_parsed, tol=1e-11)
                            if not check("supernodal solve.linear bit-exact vs v1 (rel<1e-11)",
                                          ok, why): failures += 1
                # C-09 / C-10 guard: a session opened with mode=supernodal owns a SnSession
                # factor, not the LDLT PreparedSystem that analysis.modal / analysis.buckling
                # require. The dispatcher must refuse with NOT_IMPLEMENTED rather than silently
                # building a parallel LDLT factor that defeats the supernodal factor-once
                # contract and mis-tags advancedDiagnostics' backend.
                dll.send(ctx, build_frame({
                    "v": 2, "kind": "request", "id": "rb5_mo", "method": "analysis.modal",
                    "body": {"session": sid_sn, "numModes": 1}
                }))
                rc_mo, raw_mo = dll.recv(ctx)
                if rc_mo == OK:
                    _, hdr_mo, _ = parse_frame(raw_mo)
                    is_err_mo = hdr_mo.get("kind") == "error"
                    code_mo   = hdr_mo.get("body", {}).get("code", "")
                    if not check("C-09: supernodal session refuses analysis.modal (NOT_IMPLEMENTED)",
                                  is_err_mo and code_mo == "NOT_IMPLEMENTED",
                                  f"kind={hdr_mo.get('kind')} code={code_mo}"):
                        failures += 1
                dll.send(ctx, build_frame({
                    "v": 2, "kind": "request", "id": "rb5_bk", "method": "analysis.buckling",
                    "body": {"session": sid_sn, "nev": 1}
                }))
                rc_bk, raw_bk = dll.recv(ctx)
                if rc_bk == OK:
                    _, hdr_bk, _ = parse_frame(raw_bk)
                    is_err_bk = hdr_bk.get("kind") == "error"
                    code_bk   = hdr_bk.get("body", {}).get("code", "")
                    if not check("C-10: supernodal session refuses analysis.buckling (NOT_IMPLEMENTED)",
                                  is_err_bk and code_bk == "NOT_IMPLEMENTED",
                                  f"kind={hdr_bk.get('kind')} code={code_bk}"):
                        failures += 1

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

    # --- P2 review-round: cancel tombstone gets CONSUMED, not left to leak ---
    # (a) pre-emptive cancel for id "rc1"; (b) send rc1 -> expect CANCELLED;
    # (c) re-send rc1 -> expect normal response (tombstone consumed in (b)).
    ctx3 = dll.open()
    try:
        dll.send(ctx3, build_frame({
            "v": 2, "kind": "hello", "id": "hs",
            "body": {"client": "v2_roundtrip.py/0.1", "profile": "simple"}
        }))
        hello_caps = set()
        rc, raw = dll.recv(ctx3)
        if rc == OK:
            _, hh, _ = parse_frame(raw)
            hello_caps = set(hh.get("body", {}).get("capabilities", []))
        if not check("B4: hello.capabilities advertises 'transport.async'",
                      "transport.async" in hello_caps and "transport.sync" not in hello_caps,
                      f"caps={sorted(hello_caps)}"): failures += 1

        # (a) pre-emptive cancel
        rc_c = dll.cancel_request(ctx3, "rc1")
        if not check("cancel_request(pre-emptive) returns OK", rc_c == OK, f"rc={rc_c}"):
            failures += 1
        # (b) send rc1 -> CANCELLED
        dll.send(ctx3, build_frame({
            "v": 2, "kind": "request", "id": "rc1", "method": "session.open",
            "body": {"mode": "default"}
        }))
        rc, raw = dll.recv(ctx3)
        is_cancelled = False
        if rc == OK:
            _, hdr, _ = parse_frame(raw)
            is_cancelled = (hdr.get("body", {}).get("code") == "CANCELLED")
        if not check("send id=rc1 after cancel -> CANCELLED",
                      is_cancelled, f"rc={rc} hdr={raw[:64] if rc==OK else None}"): failures += 1
        # (c) re-send rc1 -> expect normal response (tombstone consumed)
        dll.send(ctx3, build_frame({
            "v": 2, "kind": "request", "id": "rc1", "method": "session.open",
            "body": {"mode": "default"}
        }))
        rc, raw = dll.recv(ctx3)
        re_ok = False
        if rc == OK:
            _, hdr, _ = parse_frame(raw)
            re_ok = (hdr.get("kind") == "response"
                      and isinstance(hdr.get("body", {}).get("session"), str))
        if not check("re-send same id rc1 -> OK (tombstone consumed, no permanent leak)",
                      re_ok, f"rc={rc} hdr_body={(parse_frame(raw)[1].get('body') if rc==OK else None)}"):
            failures += 1
    finally:
        dll.close(ctx3)

    print(f"=== summary: {('ALL PASS' if failures == 0 else f'{failures} FAIL')} ===")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
