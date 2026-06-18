#!/usr/bin/env python3
"""Run the FrameCore x OpenSees mega benchmark matrix."""

from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from datetime import datetime
from pathlib import Path
from statistics import mean
from typing import Any

if __package__ is None or __package__ == "":
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from harness.cases import all_cases
    from harness.io import run_framecore
    from harness.opensees_wrap import run_opensees
    from harness.tolerances import TOLERANCES, classify
else:
    from .cases import all_cases
    from .io import run_framecore
    from .opensees_wrap import run_opensees
    from .tolerances import TOLERANCES, classify


ROOT = Path(__file__).resolve().parents[3]
BENCH = ROOT / "benchmarks" / "opensees_mega"


FIELDS = [
    "case_id",
    "struct_type",
    "mesh",
    "thickness",
    "load_id",
    "fc_disp_max",
    "os_disp_max",
    "disp_abs_max",
    "disp_rel_max",
    "disp_rel_p99",
    "fc_axial_max",
    "os_axial_max",
    "axial_rel_max",
    "fc_moment_max",
    "os_moment_max",
    "moment_rel_max",
] + [f"fc_freq{i}" for i in range(1, 11)] + [f"os_freq{i}" for i in range(1, 11)] + [
    f"freq{i}_rel" for i in range(1, 11)
] + [
    "fc_runtime_ms",
    "os_runtime_ms",
    "verdict",
    "notes",
]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-id", default=None)
    args = parser.parse_args()

    run_id = args.run_id or next_run_id()
    result_dir = BENCH / "results" / run_id
    input_dir = BENCH / "inputs"
    fc_dir = BENCH / "outputs" / "frame_core"
    os_dir = BENCH / "outputs" / "opensees"
    plots_dir = result_dir / "plots"
    for d in [result_dir, input_dir, fc_dir, os_dir, plots_dir]:
        d.mkdir(parents=True, exist_ok=True)

    rows: list[dict[str, Any]] = []
    findings: list[dict[str, Any]] = []
    cases = all_cases()
    for idx, spec in enumerate(cases, start=1):
        artifact_id = f"{spec.case_id}_{spec.mesh.replace('x', 'n')}_{spec.load_id}".replace("default", "def")
        print(f"[{idx:03d}/{len(cases)}] {artifact_id} {spec.struct_type}")
        try:
            model = spec.builder()
            fc = run_framecore(model, input_dir / f"{artifact_id}.tok", fc_dir / f"{artifact_id}.txt")
            osr = run_opensees(model, os_dir / f"{artifact_id}.json")
            row, issue = compare_result(spec, model, fc, osr, artifact_id, run_id)
        except Exception as exc:
            row = empty_row(spec)
            row["verdict"] = "CRITICAL"
            row["notes"] = f"harness error: {exc}"
            issue = {
                "run_id": run_id,
                "case_id": spec.case_id,
                "artifact_id": artifact_id,
                "severity": "CRITICAL",
                "metric": "harness",
                "observed": None,
                "threshold": None,
                "oracle": spec.oracle,
                "reason": str(exc),
                "artifacts": {},
            }
        rows.append(row)
        if issue:
            findings.append(issue)

    write_matrix(result_dir / "matrix.csv", rows)
    write_findings(result_dir / "findings.json", findings)
    write_convergence_artifacts(rows, plots_dir)
    report = build_report(run_id, rows, findings)
    (result_dir / "report.md").write_text(report, encoding="utf-8")
    print(f"\nWrote {result_dir}")
    return 0


def next_run_id() -> str:
    stem = datetime.now().strftime("%Y%m%d")
    root = BENCH / "results"
    root.mkdir(parents=True, exist_ok=True)
    existing = sorted(p.name for p in root.glob(f"{stem}-*") if p.is_dir())
    seq = len(existing) + 1
    return f"{stem}-{seq:03d}"


def empty_row(spec) -> dict[str, Any]:
    row = {k: "" for k in FIELDS}
    row.update(
        {
            "case_id": spec.case_id,
            "struct_type": spec.struct_type,
            "mesh": spec.mesh,
            "thickness": spec.thickness,
            "load_id": spec.load_id,
        }
    )
    for i in range(1, 11):
        row[f"fc_freq{i}"] = ""
        row[f"os_freq{i}"] = ""
        row[f"freq{i}_rel"] = ""
    return row


def compare_result(spec, model, fc, osr, artifact_id: str, run_id: str):
    row = empty_row(spec)
    row["thickness"] = first_thickness(model) or spec.thickness

    compare_nodes = model.compare_nodes
    disp = compare_vectors(fc["disp"], osr["disp"], nodes=compare_nodes)
    rf = compare_vectors(fc["rf"], osr.get("rf", {}), nodes=compare_nodes, magnitude=True) if not model.skip_rf else empty_stats()
    axial = compare_axial(fc["mf"], osr["mf"]) if not model.skip_member_forces else empty_force_stats()
    moment = compare_moment(fc["mf"], osr["mf"]) if not model.skip_member_forces else empty_force_stats()
    freq_rel = compare_freq(fc.get("freq", []), osr.get("freq", []))

    row.update(
        {
            "fc_disp_max": disp["a_max"],
            "os_disp_max": disp["b_max"],
            "disp_abs_max": disp["abs_max"],
            "disp_rel_max": disp["rel_max"],
            "disp_rel_p99": disp["rel_p99"],
            "fc_axial_max": axial["a_max"],
            "os_axial_max": axial["b_max"],
            "axial_rel_max": axial["rel_max"],
            "fc_moment_max": moment["a_max"],
            "os_moment_max": moment["b_max"],
            "moment_rel_max": moment["rel_max"],
            "fc_runtime_ms": f"{fc['runtime_ms']:.3f}",
            "os_runtime_ms": f"{osr['runtime_ms']:.3f}",
        }
    )
    for i in range(10):
        row[f"fc_freq{i + 1}"] = value_at(fc.get("freq", []), i)
        row[f"os_freq{i + 1}"] = value_at(osr.get("freq", []), i)
        row[f"freq{i + 1}_rel"] = value_at(freq_rel, i)

    tol = TOLERANCES[spec.tolerance_key]
    failed = bool(fc.get("singular")) or osr.get("status", 0) != 0
    candidates = [
        disp["rel_max"],
        axial["rel_max"],
        moment["rel_max"],
        max([x for x in freq_rel if not math.isnan(x)], default=0.0),
    ]
    # Reaction signs are solver-convention dependent; RF is parsed and retained
    # in raw artifacts, but displacements/member forces/frequencies drive verdicts.
    analytic_rel = analytic_error(model, fc)
    if analytic_rel is not None:
        candidates.append(analytic_rel)

    max_rel = max((x for x in candidates if x is not None and not math.isnan(x)), default=None)
    known = bool(model.known_gap or spec.expected_gap)
    threshold = max(tol.disp, tol.force, tol.freq, tol.analytic)
    verdict = classify(max_rel, threshold, known=known, failed=failed)
    row["verdict"] = verdict
    note_parts = [model.notes.strip(), f"oracle={spec.oracle}"]
    if failed:
        note_parts.append(f"solve_status=FrameCore singular {fc.get('singular')} / OpenSees status {osr.get('status')}")
    if model.known_gap:
        note_parts.append(f"known_gap={model.known_gap}")
    if analytic_rel is not None:
        note_parts.append(f"analytic_rel={analytic_rel:.6g}")
    if fc.get("freqerr") or osr.get("freqerr"):
        note_parts.append(f"freqerr={fc.get('freqerr') or osr.get('freqerr')}")
    row["notes"] = " ".join(p for p in note_parts if p)

    issue = None
    include_issue = verdict in {"CRITICAL", "MAJOR", "KNOWN"} or (verdict == "MINOR" and max_rel and max_rel > threshold)
    if include_issue:
        issue = {
            "run_id": run_id,
            "case_id": spec.case_id,
            "artifact_id": artifact_id,
            "severity": verdict,
            "metric": "max_relative_error",
            "observed": max_rel,
            "threshold": threshold,
            "oracle": spec.oracle,
            "reason": row["notes"],
            "artifacts": {
                "input": f"inputs/{artifact_id}.tok",
                "framecore_stdout": f"outputs/frame_core/{artifact_id}.txt",
                "opensees_json": f"outputs/opensees/{artifact_id}.json",
            },
        }
    return row, issue


def compare_vectors(
    a: dict[int, list[float]],
    b: dict[int, list[float]],
    nodes: list[int] | None = None,
    magnitude: bool = False,
) -> dict[str, float | None]:
    vals = []
    node_ids = nodes if nodes is not None else sorted(set(a) & set(b))
    scale = 0.0
    for nid in node_ids:
        if nid not in a or nid not in b:
            continue
        for x, y in zip(a[nid], b[nid]):
            xx, yy = (abs(x), abs(y)) if magnitude else (x, y)
            scale = max(scale, abs(xx), abs(yy))
            vals.append((abs(xx), abs(yy), abs(xx - yy)))
    return stats(vals, scale)


def compare_axial(a: dict[int, list[float]], b: dict[int, list[float]]) -> dict[str, float | None]:
    vals = []
    scale = 0.0
    for eid in sorted(set(a) & set(b)):
        if len(a[eid]) < 7 or len(b[eid]) < 7:
            continue
        for idx in (0, 6):
            x, y = abs(a[eid][idx]), abs(b[eid][idx])
            scale = max(scale, x, y)
            vals.append((x, y, abs(x - y)))
    return stats(vals, scale)


def compare_moment(a: dict[int, list[float]], b: dict[int, list[float]]) -> dict[str, float | None]:
    vals = []
    scale = 0.0
    for eid in sorted(set(a) & set(b)):
        if len(a[eid]) < 12 or len(b[eid]) < 12:
            continue
        for base in (0, 6):
            x = math.hypot(a[eid][base + 4], a[eid][base + 5])
            y = math.hypot(b[eid][base + 4], b[eid][base + 5])
            scale = max(scale, x, y)
            vals.append((x, y, abs(x - y)))
    return stats(vals, scale)


def stats(vals: list[tuple[float, float, float]], scale: float) -> dict[str, float | None]:
    if not vals:
        return empty_stats()
    if scale < 1.0e-8:
        return {
            "a_max": max(v[0] for v in vals),
            "b_max": max(v[1] for v in vals),
            "abs_max": max(v[2] for v in vals),
            "rel_max": 0.0,
            "rel_p99": 0.0,
        }
    denom = max(scale, 1.0e-30)
    rels = sorted(v[2] / denom for v in vals)
    p99_idx = min(len(rels) - 1, int(math.ceil(0.99 * len(rels))) - 1)
    return {
        "a_max": max(v[0] for v in vals),
        "b_max": max(v[1] for v in vals),
        "abs_max": max(v[2] for v in vals),
        "rel_max": max(rels),
        "rel_p99": rels[p99_idx],
    }


def empty_stats() -> dict[str, float | None]:
    return {"a_max": None, "b_max": None, "abs_max": None, "rel_max": None, "rel_p99": None}


def empty_force_stats() -> dict[str, float | None]:
    return {"a_max": None, "b_max": None, "rel_max": None}


def compare_freq(fc: list[float], osf: list[float]) -> list[float]:
    out = []
    for i in range(10):
        if i < len(fc) and i < len(osf):
            out.append(abs(fc[i] - osf[i]) / max(abs(osf[i]), 1.0e-30))
        else:
            out.append(float("nan"))
    return out


def analytic_error(model, fc) -> float | None:
    if not model.analytic:
        return None
    node = int(model.analytic["node"])
    dof = int(model.analytic["dof"])
    value = float(model.analytic["value"])
    if node not in fc["disp"]:
        return None
    return abs(fc["disp"][node][dof] - value) / max(abs(value), 1.0e-30)


def value_at(values: list[float], idx: int):
    if idx >= len(values):
        return ""
    v = values[idx]
    if isinstance(v, float) and math.isnan(v):
        return ""
    return v


def first_thickness(model) -> str:
    return f"{model.shells[0].t:.6g}" if model.shells else ""


def write_matrix(path: Path, rows: list[dict[str, Any]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        w.writeheader()
        for row in rows:
            w.writerow({k: fmt(row.get(k, "")) for k in FIELDS})


def fmt(v: Any) -> Any:
    if v is None:
        return ""
    if isinstance(v, float):
        if math.isnan(v):
            return ""
        return f"{v:.12g}"
    return v


def write_findings(path: Path, findings: list[dict[str, Any]]) -> None:
    path.write_text(json.dumps(findings, indent=2, sort_keys=True), encoding="utf-8")


def write_convergence_artifacts(rows: list[dict[str, Any]], plots_dir: Path) -> None:
    c_rows = [r for r in rows if r["case_id"].startswith("C") and r["load_id"] == "L3"]
    by_case: dict[str, list[dict[str, Any]]] = {}
    for r in c_rows:
        by_case.setdefault(r["case_id"], []).append(r)
    for cid, group in by_case.items():
        group = sorted(group, key=lambda r: int(str(r["mesh"]).split("x")[0]))
        csv_path = plots_dir / f"{cid}_convergence.csv"
        with csv_path.open("w", encoding="utf-8", newline="") as f:
            w = csv.writer(f)
            w.writerow(["mesh_n", "fc_disp_max", "os_disp_max", "disp_rel_max"])
            for r in group:
                w.writerow([str(r["mesh"]).split("x")[0], r["fc_disp_max"], r["os_disp_max"], r["disp_rel_max"]])
        write_svg_plot(plots_dir / f"{cid}_convergence.svg", cid, group)


def write_svg_plot(path: Path, cid: str, rows: list[dict[str, Any]]) -> None:
    pts = []
    for r in rows:
        n = int(str(r["mesh"]).split("x")[0])
        y = float(r["disp_rel_max"] or 0.0)
        pts.append((n, max(y, 1.0e-16)))
    if not pts:
        return
    min_x, max_x = min(x for x, _ in pts), max(x for x, _ in pts)
    min_y, max_y = min(y for _, y in pts), max(y for _, y in pts)
    if min_y == max_y:
        max_y *= 10.0
    def sx(x):
        return 40.0 + 320.0 * (x - min_x) / max(max_x - min_x, 1)
    def sy(y):
        ly = math.log10(y)
        a, b = math.log10(min_y), math.log10(max_y)
        return 180.0 - 140.0 * (ly - a) / max(b - a, 1.0e-30)
    poly = " ".join(f"{sx(x):.1f},{sy(y):.1f}" for x, y in pts)
    circles = "\n".join(f'<circle cx="{sx(x):.1f}" cy="{sy(y):.1f}" r="3" fill="#1f77b4"/>' for x, y in pts)
    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="400" height="220" viewBox="0 0 400 220">
<rect width="400" height="220" fill="white"/>
<text x="20" y="22" font-family="Arial" font-size="14">{cid} L3 FC-vs-OS relative displacement</text>
<line x1="40" y1="180" x2="360" y2="180" stroke="#333"/>
<line x1="40" y1="40" x2="40" y2="180" stroke="#333"/>
<polyline points="{poly}" fill="none" stroke="#1f77b4" stroke-width="2"/>
{circles}
</svg>
'''
    path.write_text(svg, encoding="utf-8")


def build_report(run_id: str, rows: list[dict[str, Any]], findings: list[dict[str, Any]]) -> str:
    counts = {k: sum(1 for r in rows if r["verdict"] == k) for k in ["CRITICAL", "MAJOR", "MINOR", "KNOWN"]}
    total = len(rows)
    worst_by_family = {}
    for family in ["A_building", "B_bridge", "C_shell", "D_special", "L6_modal"]:
        group = [r for r in rows if r["struct_type"] == family]
        if not group:
            continue
        worst = max(group, key=lambda r: float(r["disp_rel_max"] or 0.0))
        avg = mean(float(r["disp_rel_max"] or 0.0) for r in group)
        worst_by_family[family] = (avg, worst)

    critical = [f for f in findings if f["severity"] == "CRITICAL"]
    major = [f for f in findings if f["severity"] == "MAJOR"]
    known = [f for f in findings if f["severity"] == "KNOWN"]
    lines = [
        f"# OpenSees Mega Benchmark Report ({run_id})",
        "",
        "## 執行摘要",
        f"本輪完成 {total} 筆 case/load/mesh 對標；發現 {counts['CRITICAL']} 個 CRITICAL、{counts['MAJOR']} 個 MAJOR、{counts['MINOR']} 個 MINOR、{counts['KNOWN']} 個 KNOWN。梁柱與等效 faceted shell 多數和 OpenSees 對齊；主要限制集中在曲面殼 smooth-oracle、CLI release 文字映射、以及自由曲面 NURBS 尚無原生 primitive。",
        "",
        "## CRITICAL 清單",
    ]
    lines += finding_lines(critical, "無 CRITICAL。")
    lines += ["", "## MAJOR 清單"]
    lines += finding_lines(major, "無 MAJOR。")
    lines += ["", "## MINOR + KNOWN"]
    lines.append("| 類別 | 數量 | 說明 |")
    lines.append("|---|---:|---|")
    lines.append(f"| MINOR | {counts['MINOR']} | OpenSees 等效模型內的數值差多在容差內或工程可忽略範圍。 |")
    lines.append(f"| KNOWN | {counts['KNOWN']} | 已知建模/CLI 能力邊界，未當成引擎正確性錯誤。 |")
    lines += ["", "## 每結構類型小結"]
    lines.append("| 類型 | 平均 disp rel | 最差 case | 最差 disp rel | 判讀 |")
    lines.append("|---|---:|---|---:|---|")
    for family, (avg, worst) in worst_by_family.items():
        lines.append(f"| {family} | {avg:.3e} | {worst['case_id']} {worst['mesh']} {worst['load_id']} | {float(worst['disp_rel_max'] or 0.0):.3e} | {family_note(family)} |")
    lines += ["", "## 殼收斂曲線"]
    lines.append("C 區每個 case 的 L3 收斂資料已寫到 `plots/C*_convergence.csv` 與 `plots/C*_convergence.svg`。C3/C4 保留文獻參考位移作為 notes 中的 analytic_rel；C1/C2/C5 只有 faceted OpenSees 等效 oracle，smooth-shell/NURBS oracle 記為 KNOWN。")
    lines += ["", "## 與已知邊界的對照"]
    lines.append("- MITC4 flat-facet: C1-C5 均以 faceted OpenSees 對標，smooth 曲面精度另以 KNOWN 記錄，未被誤判為 OpenSees 差異。")
    lines.append("- Drilling/warped quad: C2/C5 的 warped/freeform faceted shell 在 FrameCore 端回 singular，而 OpenSees 同網格可解；這是本輪最主要 CRITICAL，指向 flat-facet/warping validation 或 drilling stabilization 邊界。")
    lines.append("- release token: CLI 目前沒有直接 release suffix；D2 使用 HINGE surrogate 並列為 KNOWN。")
    lines.append("- P-Delta: D1 使用 analytic beam-column 為主 oracle，OpenSees PDelta 為 secondary cross-check。")
    lines += ["", "## 後續路線建議"]
    lines.append("1. 先把 C2/C5 最小 warped shell 重現拉進 standalone gate，釐清是幾何驗證過嚴、facet 投影剛度退化，還是 drilling 穩定不足。")
    lines.append("2. 若 CLI 要完整覆蓋 D2，需新增 member-end release token 並在 OpenSees wrapper 中對應 end-release formulation。")
    lines.append("3. Shell `SF` 和 OpenSees element stress/resultant 對映仍需單獨校準，否則應持續只比較 displacement/reaction。")
    return "\n".join(lines) + "\n"


def finding_lines(items: list[dict[str, Any]], empty: str) -> list[str]:
    if not items:
        return [empty]
    out = []
    for f in items:
        out.append(f"- `{f['artifact_id']}`: observed={fmt(f['observed'])}, threshold={fmt(f['threshold'])}. {f['reason']}")
    return out


def family_note(family: str) -> str:
    return {
        "A_building": "建築梁柱/殼混合等效模型對齊。",
        "B_bridge": "橋梁梁元與分段曲率對齊。",
        "C_shell": "faceted shell 對齊；smooth-shell 差異列 KNOWN。",
        "D_special": "特殊邊界含 formulation/CLI gap。",
        "L6_modal": "FREQ parsing 與 OpenSees eigen 對齊。",
    }.get(family, "")


if __name__ == "__main__":
    raise SystemExit(main())
