"""FrameCore CLI token writer and stdout parser."""

from __future__ import annotations

import subprocess
import time
from pathlib import Path
from typing import Any

from .model import Model


ROOT = Path(__file__).resolve().parents[3]
CLI = ROOT / "Plugins" / "FrameSolver" / "Standalone" / "frame_cli.exe"
BUILD_CLI = ROOT / "Plugins" / "FrameSolver" / "Standalone" / "build_cli.bat"


def ensure_cli() -> None:
    if CLI.exists():
        return
    rc = subprocess.run(["cmd", "/c", str(BUILD_CLI)], cwd=ROOT).returncode
    if rc != 0 or not CLI.exists():
        raise RuntimeError("could not build frame_cli.exe")


def model_to_tokens(model: Model) -> str:
    lines: list[str] = []
    if model.opt:
        lines.append(model.opt)
    # v2.3: warped/freeform shell meshes need explicit warpTolerance relaxation.
    # `warp_tol=None` (default) → no WARP line emitted → CLI keeps SolveOptions{}
    # default (warpTolerance=1e-6, strict) — back-compat for every non-warped case.
    if getattr(model, "warp_tol", None) is not None:
        uc = 1 if getattr(model, "use_warping_correction", True) else 0
        lines.append(f"WARP {model.warp_tol:.17g} {uc}")
    for m in model.materials:
        if m.shell:
            nu = 0.3 if m.nu is None else m.nu
            lines.append(f"SMAT {m.E:.17g} {nu:.17g} {m.G:.17g}")
        else:
            lines.append(f"MAT {m.E:.17g} {m.G:.17g} {m.rho:.17g}")
    for s in model.sections:
        lines.append(
            "SEC "
            f"{s.A:.17g} {s.Iy:.17g} {s.Iz:.17g} {s.J:.17g} "
            f"{s.cy:.17g} {s.cz:.17g} {s.Asy:.17g} {s.Asz:.17g}"
        )
    for n in model.nodes:
        fp = list(n.fix) + list(n.presc)
        lines.append(
            "NODE "
            f"{n.id} {n.x:.17g} {n.y:.17g} {n.z:.17g} "
            + " ".join(f"{v:.17g}" if isinstance(v, float) else str(v) for v in fp)
        )
    for e in model.members:
        rv = e.refvec
        lines.append(
            "MEMBER "
            f"{e.id} {e.i} {e.j} {e.mat} {e.sec} "
            f"{rv[0]:.17g} {rv[1]:.17g} {rv[2]:.17g} {e.active} {e.tonly}"
        )
    for s in model.shells:
        n = s.n
        lines.append(f"SHELL {s.id} {n[0]} {n[1]} {n[2]} {n[3]} {s.mat} {s.t:.17g} {s.active}")
    for l in model.nloads:
        c = l.comp
        lines.append(
            f"NLOAD {l.node} {c[0]:.17g} {c[1]:.17g} {c[2]:.17g} "
            f"{c[3]:.17g} {c[4]:.17g} {c[5]:.17g}"
        )
    for l in model.udls:
        lines.append(f"UDL {l.member} {l.wx:.17g} {l.wy:.17g} {l.wz:.17g}")
    for p in model.spress:
        lines.append(f"SPRESS {p.shell} {p.p:.17g}")
    for h in model.hinges:
        lines.append(f"HINGE {h.member} {h.dof} {h.Mp:.17g}")
    if model.eigen:
        lines.append(f"EIGEN {model.eigen}")
    if model.pdelta is not None:
        lines.append(f"PDELTA {model.pdelta}")
    lines.append("END")
    return "\n".join(lines) + "\n"


def parse_stdout(stdout: str) -> dict[str, Any]:
    out: dict[str, Any] = {
        "version": None,
        "singular": None,
        "disp": {},
        "rf": {},
        "mf": {},
        "sf": {},
        "freq": [],
        "freqerr": None,
        "pdstatus": None,
        "raw": stdout,
    }
    for ln in stdout.splitlines():
        t = ln.split()
        if not t:
            continue
        tag = t[0]
        if tag == "VERSION" and len(t) > 1:
            out["version"] = t[1]
        elif tag == "SINGULAR":
            out["singular"] = int(t[1])
        elif tag == "DISP":
            out["disp"][int(t[1])] = [float(x) for x in t[2:8]]
        elif tag == "RF":
            out["rf"][int(t[1])] = [float(x) for x in t[2:8]]
        elif tag == "MF":
            out["mf"][int(t[1])] = [float(x) for x in t[2:14]]
        elif tag == "SF":
            out["sf"][int(t[1])] = [float(x) for x in t[2:10]]
        elif tag == "FREQ":
            n = int(t[1])
            out["freq"] = [float(x) for x in t[2 : 2 + n]]
        elif tag == "FREQERR":
            out["freqerr"] = " ".join(t[1:])
        elif tag == "PDSTATUS":
            out["pdstatus"] = tuple(int(x) for x in t[1:4])
    return out


def run_framecore(model: Model, input_path: Path, output_path: Path) -> dict[str, Any]:
    ensure_cli()
    token_text = model_to_tokens(model)
    input_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    input_path.write_text(token_text, encoding="utf-8")
    t0 = time.perf_counter()
    p = subprocess.run([str(CLI)], input=token_text, capture_output=True, text=True, cwd=ROOT)
    runtime_ms = (time.perf_counter() - t0) * 1000.0
    output_path.write_text(p.stdout, encoding="utf-8")
    if p.returncode != 0:
        raise RuntimeError(p.stderr or "frame_cli failed")
    parsed = parse_stdout(p.stdout)
    parsed["runtime_ms"] = runtime_ms
    parsed["stderr"] = p.stderr
    return parsed
