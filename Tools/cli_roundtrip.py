#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
S6 CLI round-trip integration test (NOT shipped in the game). Builds frame_cli and drives the
J1 text bridge end-to-end, checking invariants against analytic / engine oracles. Pure Python
(no openseespy). This is gate leg [5/5].

It exercises the S6 protocol additions (VERSION handshake, TONLY / SIZEOPT / DYNC commands and
the MEMBER ... tonly token) as PLUMBING: the rigorous mechanics are already verified by the
standalone F-tests, the UE automation and the deep audit -- this leg only proves the text bridge
round-trips correctly so a Grasshopper / external client can drive the engine.

Usage:  python Tools/cli_roundtrip.py        (exit 0 iff every check passes)
"""
import os, sys, math, subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI  = os.path.join(ROOT, "Plugins", "FrameSolver", "Standalone", "frame_cli.exe")
BUILD_CLI = os.path.join(ROOT, "Plugins", "FrameSolver", "Standalone", "build_cli.bat")

_fails = 0
def check(tag, ok, detail=""):
    global _fails
    if not ok:
        _fails += 1
    print(("  [PASS] " if ok else "  [FAIL] ") + tag + (("  " + detail) if detail else ""))


# ------------------------------------------------------------------ section helpers
def sq(side):
    """SEC tokens (A Iy Iz J cy cz Asy Asz) for a solid square, matching Section::Rectangular."""
    A = side * side
    I = side ** 4 / 12.0
    J = 0.1406 * side ** 4
    return "{} {} {} {} {} {} {} {}".format(A, I, I, J, side / 2, side / 2, 5.0 / 6 * A, 5.0 / 6 * A)

def rect(b, d):
    """SEC tokens for a rectangle, matching Section::Rectangular (cy=b/2, cz=d/2)."""
    A = b * d
    Iz = b * d ** 3 / 12.0
    Iy = d * b ** 3 / 12.0
    bb, tt = max(b, d), min(b, d)
    r = tt / bb
    J = bb * tt ** 3 * (1.0 / 3.0 - 0.21 * r * (1.0 - r ** 4 / 12.0))
    return "{} {} {} {} {} {} {} {}".format(A, Iy, Iz, J, b / 2, d / 2, 5.0 / 6 * A, 5.0 / 6 * A)


# ------------------------------------------------------------------ driver
def run(lines):
    text = "\n".join(lines) + "\nEND\n"
    # errors="replace": the cl.exe build chatter / stderr may carry non-UTF-8 (cp950) bytes on a
    # localized Windows; never let a decode hiccup crash the harness.
    p = subprocess.run([CLI], input=text, capture_output=True, text=True, errors="replace")
    if p.returncode != 0:
        raise RuntimeError("frame_cli rc=%d: %s" % (p.returncode, p.stderr))
    d = {"VERSION": None, "SINGULAR": None, "DISP": {}, "MF": {},
         "TONLY": None, "SLACK": [], "SIZEOPT": None, "AREA": {}, "WEIGHTVOL": None,
         "DYNC": None, "DEVENT": [], "_first": None}
    for ln in p.stdout.splitlines():
        t = ln.split()
        if not t:
            continue
        if d["_first"] is None:
            d["_first"] = t[0]
        tag = t[0]
        if tag == "VERSION":
            d["VERSION"] = t[1] if len(t) > 1 else ""
        elif tag == "SINGULAR":
            d["SINGULAR"] = int(t[1])
        elif tag == "DISP":
            d["DISP"][int(t[1])] = [float(x) for x in t[2:8]]
        elif tag == "MF":
            d["MF"][int(t[1])] = [float(x) for x in t[2:14]]
        elif tag == "TONLY":
            d["TONLY"] = (int(t[1]), int(t[2]), int(t[3]))
        elif tag == "SLACK":
            d["SLACK"] = [int(x) for x in t[1:]]
        elif tag == "SIZEOPT":
            d["SIZEOPT"] = (int(t[1]), int(t[2]), int(t[3]))
        elif tag == "AREA":
            d["AREA"][int(t[1])] = (float(t[2]), float(t[3]))
        elif tag == "WEIGHTVOL":
            d["WEIGHTVOL"] = float(t[1])
        elif tag == "DYNC":
            d["DYNC"] = (int(t[1]), int(t[2]), int(t[3]), float(t[4]))
        elif tag == "DEVENT":
            d["DEVENT"].append(t[1:])
    return d


def ensure_built():
    # errors="replace": cl.exe prints localized (cp950) progress; we only need the exit code, so a
    # decode failure must not raise in the capture reader thread.
    p = subprocess.run(["cmd", "/c", BUILD_CLI], capture_output=True, text=True, errors="replace")
    if p.returncode != 0 or not os.path.exists(CLI):
        print("[FAIL] build_cli.bat failed (rc=%d)" % p.returncode)
        print(p.stdout[-800:])
        print(p.stderr[-400:])
        sys.exit(1)


def main():
    ensure_built()
    print("CLI round-trip integration (frame_cli J1 text bridge)")

    # ---- Check 1: VERSION handshake + baseline cantilever DISP round-trips to PL^3/3EI ----
    side, E, G, P, L = 100.0, 210000.0, 80769.0, 1000.0, 2000.0
    m = ["MAT {} {} 7850".format(E, G), "SEC " + sq(side),
         "NODE 0 0 0 0 1 1 1 1 1 1 0 0 0 0 0 0",
         "NODE 1 {} 0 0 0 0 0 0 0 0 0 0 0 0 0 0".format(L),
         "MEMBER 0 0 1 0 0 0 0 1",
         "NLOAD 1 0 0 {} 0 0 0".format(P)]
    d = run(m)
    check("VERSION handshake is the first stdout line", d["_first"] == "VERSION" and bool(d["VERSION"]),
          "first=%s sha=%s" % (d["_first"], d["VERSION"]))
    I = side ** 4 / 12.0
    exact = P * L ** 3 / (3.0 * E * I)
    got = d["DISP"][1][2]            # uz at the tip
    rel = abs(got - exact) / abs(exact)
    check("baseline DISP cantilever == PL^3/3EI (rel<1e-6)", rel < 1e-6,
          "got=%.6g exact=%.6g rel=%.2e" % (got, exact, rel))

    # ---- Check 2: TONLY -- X-braced portal, one diagonal goes slack (mirrors F42 plumbing) ----
    H = 5.0e4
    portal = ["MAT 200000 76923.07692307692 7850",
              "SEC " + rect(250, 250), "SEC " + rect(60, 60),
              "NODE 0 0 0 0 1 1 1 1 1 1 0 0 0 0 0 0",
              "NODE 1 6000 0 0 1 1 1 1 1 1 0 0 0 0 0 0",
              "NODE 2 0 0 3000 0 1 0 0 0 0 0 0 0 0 0 0",
              "NODE 3 6000 0 3000 0 1 0 0 0 0 0 0 0 0 0 0",
              "MEMBER 0 0 2 0 0 0 1 0",
              "MEMBER 1 1 3 0 0 0 1 0",
              "MEMBER 2 2 3 0 0 0 1 0",
              "MEMBER 3 0 3 0 1 0 1 0 1 1",     # active=1, tonly=1
              "MEMBER 4 1 2 0 1 0 1 0 1 1",
              "NLOAD 2 {} 0 0 0 0 0".format(H),
              "TONLY"]
    d = run(portal)
    conv = d["TONLY"][0] if d["TONLY"] else -1
    check("TONLY converged + exactly one brace slack",
          d["TONLY"] is not None and conv == 1 and len(d["SLACK"]) == 1,
          "TONLY=%s slack=%s" % (d["TONLY"], d["SLACK"]))

    # ---- Check 3: SIZEOPT -- 10-bar truss plumbing (converges, 10 areas, positive volume) ----
    IN = 25.4
    bay = 360.0 * IN
    A0 = 10.0 * IN * IN
    Amin = 0.1 * IN * IN
    Pn = 1.0e5 * 4.4482216152605
    side10 = math.sqrt(A0)
    bars = [(5, 3), (3, 1), (6, 4), (4, 2), (3, 4), (1, 2), (5, 4), (6, 3), (3, 2), (4, 1)]
    truss = ["MAT 68947.572931684 26518.0 7850", "SEC " + sq(side10)]
    def node(i, x, z, sup):
        fix = "1 1 1 0 0 0" if sup else "0 1 0 0 0 0"
        return "NODE {} {} 0 {} {} 0 0 0 0 0 0".format(i, x, z, fix)
    truss += [node(1, 2 * bay, bay, False), node(2, 2 * bay, 0, False),
              node(3, bay, bay, False), node(4, bay, 0, False),
              node(5, 0, bay, True), node(6, 0, 0, True)]
    for k, (i, j) in enumerate(bars):
        truss.append("MEMBER {} {} {} 0 0 0 1 0".format(k, i, j))
    truss += ["NLOAD 2 0 0 {} 0 0 0".format(-Pn), "NLOAD 4 0 0 {} 0 0 0".format(-Pn),
              "SIZEOPT {} 100 1e-8".format(Amin)]
    d = run(truss)
    so = d["SIZEOPT"]
    check("SIZEOPT converged + 10 sized areas + positive volume",
          so is not None and so[0] == 1 and so[2] == 0 and len(d["AREA"]) == 10
          and d["WEIGHTVOL"] is not None and d["WEIGHTVOL"] > 0,
          "SIZEOPT=%s nAreas=%d vol=%s" % (so, len(d["AREA"]), d["WEIGHTVOL"]))

    # ---- Check 4: DYNC -- dynamic-collapse summary plumbing (vertical cantilever, remove member 0) ----
    dyn = ["MAT 210000 80769 7850", "SEC " + sq(100.0),
           "NODE 0 0 0 0 1 1 1 1 1 1 0 0 0 0 0 0",
           "NODE 1 0 0 3000 0 0 0 0 0 0 0 0 0 0 0 0",
           "MEMBER 0 0 1 0 0 1 0 0",
           "NLOAD 1 0 0 -1000 0 0 0",
           "DYNC 0.001 0.05 0"]            # remove member id 0 at t=0
    d = run(dyn)
    dy = d["DYNC"]
    # outcome enum: Stable=0, Collapsed=1, MaxSteps=2, Invalid=3 -> a terminal (non-Invalid) state
    check("DYNC summary well-formed + terminal outcome (not Invalid)",
          dy is not None and dy[0] in (0, 1, 2) and dy[1] >= 0 and dy[2] >= 0,
          "DYNC=%s" % (dy,))

    print("\n%s  (failures=%d)" % ("ALL PASS" if _fails == 0 else "FAILURES", _fails))
    return 0 if _fails == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
