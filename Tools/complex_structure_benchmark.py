#!/usr/bin/env python
"""
Complex-structure correctness and performance benchmark for FrameCore.

The benchmark generates multi-story 3D braced frame buildings with hundreds to
thousands of DOFs. It compares FrameCore's frame_cli.exe against:
  * independent dense Python DSM for small/medium models;
  * OpenSeesPy displacement results when openseespy is available;
  * Python dense DSM runtime where dense solving is still practical.

Reported frame_cli time is end-to-end CLI time: process startup, solve, stdout
printing, and capture. The parse time is shown separately.
"""
from __future__ import annotations

import argparse
import copy
import math
import os
import statistics
import subprocess
import time
from dataclasses import dataclass

import numpy as np

import independent_precision_audit as audit


@dataclass
class CaseDef:
    name: str
    nx: int
    ny: int
    stories: int
    dense: bool
    opensees: bool


def node_id(i: int, j: int, k: int, nx: int, ny: int) -> int:
    return k * (nx + 1) * (ny + 1) + j * (nx + 1) + i


def make_tower(nx: int, ny: int, stories: int) -> dict:
    sx, sy, h = 6000.0, 5000.0, 3300.0
    steel = dict(E=200000.0, G=76923.07692307692, rho=7850.0)
    sections = [
        audit.square_section(520.0),  # columns
        audit.square_section(360.0),  # beams
        audit.square_section(220.0),  # braces
    ]
    nodes = []
    for k in range(stories + 1):
        # Mild geometric irregularity without changing connectivity.
        drift_x = 35.0 * k
        drift_y = -18.0 * k
        for j in range(ny + 1):
            for i in range(nx + 1):
                nodes.append(dict(
                    id=node_id(i, j, k, nx, ny),
                    x=i * sx + drift_x,
                    y=j * sy + drift_y,
                    z=k * h,
                    fix=[1, 1, 1, 1, 1, 1] if k == 0 else [0, 0, 0, 0, 0, 0],
                ))

    by_id = {n["id"]: n for n in nodes}
    members = []

    def add_member(i_id: int, j_id: int, sec: int) -> None:
        pi = (by_id[i_id]["x"], by_id[i_id]["y"], by_id[i_id]["z"])
        pj = (by_id[j_id]["x"], by_id[j_id]["y"], by_id[j_id]["z"])
        members.append(dict(
            id=len(members),
            i=i_id,
            j=j_id,
            mat=0,
            sec=sec,
            refvec=audit.refvec_for(pi, pj),
        ))

    # Columns.
    for k in range(stories):
        for j in range(ny + 1):
            for i in range(nx + 1):
                add_member(node_id(i, j, k, nx, ny), node_id(i, j, k + 1, nx, ny), 0)

    # Floor beams in X and Y directions.
    for k in range(1, stories + 1):
        for j in range(ny + 1):
            for i in range(nx):
                add_member(node_id(i, j, k, nx, ny), node_id(i + 1, j, k, nx, ny), 1)
        for j in range(ny):
            for i in range(nx + 1):
                add_member(node_id(i, j, k, nx, ny), node_id(i, j + 1, k, nx, ny), 1)

    # Perimeter diagonal braces. Alternate diagonals by story and bay to avoid symmetry.
    for k in range(stories):
        for i in range(nx):
            for j_face in (0, ny):
                if (i + k) % 2 == 0:
                    a, b = node_id(i, j_face, k, nx, ny), node_id(i + 1, j_face, k + 1, nx, ny)
                else:
                    a, b = node_id(i + 1, j_face, k, nx, ny), node_id(i, j_face, k + 1, nx, ny)
                add_member(a, b, 2)
        for j in range(ny):
            for i_face in (0, nx):
                if (j + k) % 2 == 0:
                    a, b = node_id(i_face, j, k, nx, ny), node_id(i_face, j + 1, k + 1, nx, ny)
                else:
                    a, b = node_id(i_face, j + 1, k, nx, ny), node_id(i_face, j, k + 1, nx, ny)
                add_member(a, b, 2)

    loads = []
    plan_nodes = (nx + 1) * (ny + 1)
    for k in range(1, stories + 1):
        story_factor = k / stories
        for j in range(ny + 1):
            for i in range(nx + 1):
                edge = int(i in (0, nx)) + int(j in (0, ny))
                tributary = 1.0 if edge == 0 else (0.65 if edge == 1 else 0.42)
                gravity = -38000.0 * tributary
                wind_x = 900.0 * story_factor * (1.0 + 0.08 * (j - ny / 2.0))
                wind_y = -350.0 * story_factor * (1.0 + 0.05 * (i - nx / 2.0))
                nid = node_id(i, j, k, nx, ny)
                loads.append(dict(node=nid, comp=[wind_x, wind_y, gravity, 0.0, 0.0, 0.0]))

    # Add two torsional roof moments through nodal moments to exercise rotational DOFs.
    roof_center_a = node_id(nx, ny, stories, nx, ny)
    roof_center_b = node_id(0, 0, stories, nx, ny)
    loads.append(dict(node=roof_center_a, comp=[0.0, 0.0, 0.0, 0.0, 0.0, 3.0e6]))
    loads.append(dict(node=roof_center_b, comp=[0.0, 0.0, 0.0, 0.0, -2.0e6, 0.0]))

    return dict(materials=[steel], sections=sections, nodes=nodes, members=members, nloads=loads, udls=[])


def stats(model: dict) -> dict:
    fixed = sum(1 for n in model["nodes"] for f in n["fix"] if f)
    ndof = 6 * len(model["nodes"])
    return dict(nodes=len(model["nodes"]), members=len(model["members"]),
                dof=ndof, free_dof=ndof - fixed, loads=len(model.get("nloads", [])))


def parse_cli_stdout(stdout: str) -> dict:
    out = {"singular": None, "disp": {}, "mf": {}}
    for line in stdout.splitlines():
        toks = line.split()
        if not toks:
            continue
        if toks[0] == "SINGULAR":
            out["singular"] = int(toks[1])
        elif toks[0] == "DISP":
            out["disp"][int(toks[1])] = np.array([float(v) for v in toks[2:8]], dtype=float)
        elif toks[0] == "MF":
            out["mf"][int(toks[1])] = np.array([float(v) for v in toks[2:14]], dtype=float)
    return out


def run_cli_timed(model: dict) -> tuple[dict, float, float, int]:
    text = audit.model_to_cli(model)
    t0 = time.perf_counter()
    p = subprocess.run([audit.CLI], input=text, capture_output=True, text=True, check=False)
    t1 = time.perf_counter()
    if p.returncode != 0:
        raise RuntimeError(f"frame_cli failed rc={p.returncode}\n{p.stderr}")
    out = parse_cli_stdout(p.stdout)
    t2 = time.perf_counter()
    return out, t1 - t0, t2 - t1, len(p.stdout)


def try_import_opensees():
    try:
        import openseespy.opensees as ops
        return ops
    except Exception:
        return None


def run_opensees_timed(model: dict, ops) -> tuple[dict[int, np.ndarray], float]:
    t0 = time.perf_counter()
    ops.wipe()
    ops.model("basic", "-ndm", 3, "-ndf", 6)
    for n in model["nodes"]:
        ops.node(n["id"], float(n["x"]), float(n["y"]), float(n["z"]))
        ops.fix(n["id"], *[int(v) for v in n["fix"]])
    for e in model["members"]:
        rv = e["refvec"]
        tag = e["id"] + 1
        ops.geomTransf("Linear", tag, float(rv[0]), float(rv[1]), float(rv[2]))
        mat = model["materials"][e["mat"]]
        sec = model["sections"][e["sec"]]
        ops.element("elasticBeamColumn", tag, e["i"], e["j"], sec["A"], mat["E"], mat["G"],
                    sec["J"], sec["Iz"], sec["Iy"], tag)

    ops.timeSeries("Linear", 1)
    ops.pattern("Plain", 1, 1)
    for load in model.get("nloads", []):
        ops.load(load["node"], *[float(v) for v in load["comp"]])
    ops.constraints("Transformation")
    ops.numberer("RCM")
    ops.system("BandGeneral")
    ops.test("NormDispIncr", 1.0e-10, 20)
    ops.algorithm("Linear")
    ops.integrator("LoadControl", 1.0)
    ops.analysis("Static")
    ok = ops.analyze(1)
    if ok != 0:
        raise RuntimeError(f"OpenSees analyze failed with code {ok}")
    disp = {n["id"]: np.array([ops.nodeDisp(n["id"], k) for k in range(1, 7)], dtype=float)
            for n in model["nodes"]}
    t1 = time.perf_counter()
    return disp, t1 - t0


def disp_stack_cli(model: dict, out: dict) -> np.ndarray:
    ids = [n["id"] for n in model["nodes"]]
    by_id = {n["id"]: out["disp"][idx] for idx, n in enumerate(model["nodes"])}
    return np.concatenate([by_id[i] for i in ids])


def disp_stack_opensees(model: dict, disp: dict[int, np.ndarray]) -> np.ndarray:
    return np.concatenate([disp[n["id"]] for n in model["nodes"]])


def mf_stack_cli(model: dict, out: dict) -> np.ndarray:
    return np.concatenate([out["mf"][idx] for idx, _ in enumerate(model["members"])])


def median_run(fn, repeat: int):
    results = [fn() for _ in range(repeat)]
    return sorted(results, key=lambda item: item[1] if isinstance(item, tuple) else item)[len(results) // 2]


def run_case(case: CaseDef, repeat: int, ops) -> dict:
    model = make_tower(case.nx, case.ny, case.stories)
    st = stats(model)
    cli_runs = [run_cli_timed(model) for _ in range(repeat)]
    cli_runs.sort(key=lambda r: r[1])
    cli_out, cli_total, cli_parse, stdout_bytes = cli_runs[len(cli_runs) // 2]
    row = dict(case=case.name, **st, cli_total=cli_total, cli_parse=cli_parse,
               stdout_kb=stdout_bytes / 1024.0, singular=cli_out["singular"])

    if case.dense:
        t0 = time.perf_counter()
        py = audit.py_solve(model)
        t1 = time.perf_counter()
        row["py_dense"] = t1 - t0
        row["cond"] = py["cond"]
        row["disp_vs_py"] = audit.max_scaled_diff(disp_stack_cli(model, cli_out), py["u"])
        row["force_vs_py"] = audit.max_scaled_diff(mf_stack_cli(model, cli_out),
                                                   np.concatenate([py["mf"][i] for i in sorted(py["mf"])]))
        row["speedup_vs_py"] = row["py_dense"] / cli_total if cli_total > 0 else math.inf

    if case.opensees and ops is not None:
        os_runs = [run_opensees_timed(model, ops) for _ in range(max(1, min(repeat, 3)))]
        os_runs.sort(key=lambda r: r[1])
        os_disp, os_time = os_runs[len(os_runs) // 2]
        row["opensees"] = os_time
        row["disp_vs_opensees"] = audit.max_scaled_diff(disp_stack_cli(model, cli_out),
                                                        disp_stack_opensees(model, os_disp))
        row["speedup_vs_opensees"] = os_time / cli_total if cli_total > 0 else math.inf

    return row


def print_table(rows: list[dict]) -> None:
    print("=" * 132)
    print("Complex 3D frame benchmark")
    print("=" * 132)
    header = (
        f"{'case':<16} {'nodes':>6} {'members':>8} {'freeDOF':>8} "
        f"{'CLI ms':>9} {'parse ms':>9} {'Py dense ms':>12} {'OpenSees ms':>12} "
        f"{'dispPy':>10} {'forcePy':>10} {'dispOS':>10} {'speedPy':>9} {'speedOS':>9}"
    )
    print(header)
    print("-" * len(header))
    for r in rows:
        def ms(key):
            return f"{1000.0 * r[key]:9.1f}" if key in r else f"{'-':>9}"

        def ms12(key):
            return f"{1000.0 * r[key]:12.1f}" if key in r else f"{'-':>12}"

        def sci(key):
            return f"{r[key]:10.2e}" if key in r else f"{'-':>10}"

        def speed(key):
            return f"{r[key]:9.2f}x" if key in r else f"{'-':>9}"

        print(
            f"{r['case']:<16} {r['nodes']:6d} {r['members']:8d} {r['free_dof']:8d} "
            f"{ms('cli_total')} {ms('cli_parse')} {ms12('py_dense')} {ms12('opensees')} "
            f"{sci('disp_vs_py')} {sci('force_vs_py')} {sci('disp_vs_opensees')} "
            f"{speed('speedup_vs_py')} {speed('speedup_vs_opensees')}"
        )
    print("-" * len(header))
    print("CLI ms includes executable startup, solve, stdout printing, and capture; parse ms is Python stdout parsing only.")
    print("Py dense is independent dense DSM assembly/solve/recovery, useful for correctness but not scalable.")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repeat", type=int, default=3, help="repetitions for median timing")
    ap.add_argument("--no-opensees", action="store_true", help="skip OpenSeesPy timing/comparison")
    ap.add_argument("--xl", action="store_true", help="include one extra-large CLI/OpenSees case")
    ap.add_argument("--xxl-only", action="store_true", help="run only the XXL CLI/OpenSees case")
    args = ap.parse_args()

    if args.xxl_only:
        cases = [CaseDef("XXL-24st-12x9", 12, 9, 24, dense=False, opensees=not args.no_opensees)]
    else:
        cases = [
            CaseDef("S-4st-3x2", 3, 2, 4, dense=True, opensees=True),
            CaseDef("M-8st-5x4", 5, 4, 8, dense=True, opensees=True),
            CaseDef("L-12st-7x5", 7, 5, 12, dense=False, opensees=True),
        ]
    if args.xl and not args.xxl_only:
        cases.append(CaseDef("XL-18st-9x7", 9, 7, 18, dense=False, opensees=not args.no_opensees))

    ops = None if args.no_opensees else try_import_opensees()
    if not args.no_opensees and ops is None:
        print("[note] openseespy not available; OpenSees columns will be skipped.")

    rows = [run_case(c, max(1, args.repeat), ops) for c in cases]
    print_table(rows)

    failures = []
    for r in rows:
        if r["singular"] != 0:
            failures.append(f"{r['case']} singular")
        if "disp_vs_py" in r and r["disp_vs_py"] > 1.0e-8:
            failures.append(f"{r['case']} disp_vs_py={r['disp_vs_py']:.3e}")
        if "force_vs_py" in r and r["force_vs_py"] > 1.0e-8:
            failures.append(f"{r['case']} force_vs_py={r['force_vs_py']:.3e}")
        if "disp_vs_opensees" in r and r["disp_vs_opensees"] > 1.0e-6:
            failures.append(f"{r['case']} disp_vs_opensees={r['disp_vs_opensees']:.3e}")

    if failures:
        print("FAIL")
        for f in failures:
            print("  " + f)
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
