"""Benchmark matrix registry."""

from __future__ import annotations

from .builders import build_frame_case, build_shell_case, build_special_case
from .model import CaseSpec


LOADS = ["L1", "L2", "L3", "L4"]
SHELL_MESHES = [8, 16, 24]


def all_cases() -> list[CaseSpec]:
    cases: list[CaseSpec] = []
    for cid in [f"A{i}" for i in range(1, 9)]:
        for load_id in LOADS:
            cases.append(
                CaseSpec(
                    case_id=cid,
                    struct_type="A_building",
                    load_id=load_id,
                    mesh="default",
                    thickness="mixed",
                    tolerance_key="shell" if cid in {"A3", "A4", "A7", "A8"} else "beam",
                    builder=lambda cid=cid, load_id=load_id: build_frame_case(cid, load_id),
                    oracle="OpenSees equivalent elastic model",
                )
            )
    for cid in [f"B{i}" for i in range(1, 5)]:
        for load_id in LOADS:
            cases.append(
                CaseSpec(
                    case_id=cid,
                    struct_type="B_bridge",
                    load_id=load_id,
                    mesh="default",
                    thickness="member",
                    tolerance_key="beam",
                    builder=lambda cid=cid, load_id=load_id: build_frame_case(cid, load_id),
                    oracle="OpenSees equivalent elastic bridge/arch model",
                )
            )
    for cid in [f"C{i}" for i in range(1, 6)]:
        for mesh in SHELL_MESHES:
            for load_id in LOADS:
                cases.append(
                    CaseSpec(
                        case_id=cid,
                        struct_type="C_shell",
                        load_id=load_id,
                        mesh=f"{mesh}x{mesh}",
                        thickness="case",
                        tolerance_key="curved_shell",
                        builder=lambda cid=cid, mesh=mesh, load_id=load_id: build_shell_case(cid, mesh, load_id),
                        oracle="OpenSees ShellMITC4 faceted equivalent plus documented smooth-shell gap",
                        expected_gap="faceted flat-shell approximation",
                    )
                )
    for cid in [f"D{i}" for i in range(1, 5)]:
        for load_id in LOADS:
            cases.append(
                CaseSpec(
                    case_id=cid,
                    struct_type="D_special",
                    load_id=load_id,
                    mesh="default",
                    thickness="mixed",
                    tolerance_key="pdelta" if cid == "D1" else ("known_gap" if cid == "D2" else "beam"),
                    builder=lambda cid=cid, load_id=load_id: build_special_case(cid, load_id),
                    oracle="OpenSees equivalent state and analytic where available",
                    expected_gap="release/hinge direct mapping" if cid == "D2" else None,
                )
            )

    # Representative modal legs. L4 already gives every base case one extra load
    # beyond L1-L3; these L6 rows exercise FREQ parsing and OpenSees eigen.
    for cid in ["A1", "A2", "B1", "D3"]:
        cases.append(
            CaseSpec(
                case_id=cid,
                struct_type="L6_modal",
                load_id="L6",
                mesh="default",
                thickness="member",
                tolerance_key="beam",
                builder=lambda cid=cid: build_frame_case(cid, "L6") if cid.startswith(("A", "B")) else build_special_case(cid, "L6"),
                oracle="OpenSees eigen with consistent beam mass",
            )
        )
    return cases
