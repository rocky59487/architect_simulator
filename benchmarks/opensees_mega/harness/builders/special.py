"""D-family special load and boundary builders.

Units: N, mm, MPa. These cases exercise P-Delta, hinge/release surrogates,
prescribed support displacement, and combined load states.
"""

from __future__ import annotations

import math

from ..model import Hinge, Material, Member, Model, NodalLoad, Node, refvec_for, square_section
from .common import CONCRETE, STEEL, clone_with_load


def build_special_case(case_id: str, load_id: str) -> Model:
    base = {
        "D1": _d1_pdelta_column,
        "D2": _d2_hinge_surrogate,
        "D3": _d3_settlement,
        "D4": _d4_combined_load,
    }[case_id]()
    if load_id == "BASE":
        return base
    out = clone_with_load(base, load_id)
    if case_id == "D1" and load_id not in {"L1"}:
        out.analytic = None
        out.notes += " Analytic beam-column oracle disabled for modified load pattern."
    return out


def _d1_pdelta_column() -> Model:
    """D1 P-Delta column; analytic oracle is the beam-column stability function."""
    side, E = 200.0, 200000.0
    G = E / (2.0 * (1.0 + 0.3))
    L, H = 6000.0, 1000.0
    I = side**4 / 12.0
    frac = 0.3
    Pcr = math.pi**2 * E * I / (4.0 * L * L)
    P = frac * Pcr
    nE = 8
    nodes = [Node(k, 0.0, 0.0, L * k / nE, [1, 1, 1, 1, 1, 1] if k == 0 else [0, 0, 0, 0, 0, 0]) for k in range(nE + 1)]
    members = [Member(k, k, k + 1, 0, 0, (1.0, 0.0, 0.0)) for k in range(nE)]
    sec = square_section(side)
    k = math.sqrt(P / (E * I))
    exact = H * (math.tan(k * L) - k * L) / (P * k)
    m = Model(
        "D1 P-Delta column",
        materials=[Material(E, G, 7850.0)],
        sections=[sec],
        nodes=nodes,
        members=members,
        nloads=[NodalLoad(nE, [H, 0.0, -P, 0.0, 0.0, 0.0])],
        pdelta=0,
        analytic={"node": nE, "dof": 0, "value": exact, "label": "beam-column stability function"},
        notes="OpenSees PDelta is formulation-adjacent; analytic is primary oracle.",
    )
    return m


def _d2_hinge_surrogate() -> Model:
    """D2 release/hinge surrogate; direct MEMBER release token is not available in CLI text."""
    sec = square_section(100.0)
    L, P = 4000.0, 40000.0
    nodes = [
        Node(0, 0.0, 0.0, 0.0, [1, 1, 1, 1, 1, 1]),
        Node(1, L / 2.0, 0.0, 0.0, [0, 0, 0, 0, 0, 0]),
        Node(2, L, 0.0, 0.0, [1, 1, 1, 1, 1, 1]),
    ]
    members = [
        Member(0, 0, 1, 0, 0, (0.0, 0.0, 1.0)),
        Member(1, 1, 2, 0, 0, (0.0, 0.0, 1.0)),
    ]
    mp = 0.5 * P * L / 8.0
    return Model(
        "D2 hinge-state surrogate",
        materials=[STEEL],
        sections=[sec],
        nodes=nodes,
        members=members,
        nloads=[NodalLoad(1, [0.0, 0.0, -P, 0.0, 0.0, 0.0])],
        hinges=[Hinge(0, 5, mp)],
        os_free_ry=[0],
        os_hinge_moments=[{"node": 0, "My": -mp}],
        compare_nodes=[1],
        skip_member_forces=True,
        known_gap="CLI has no direct MEMBER release token; HINGE surrogate compares displacement state only",
        notes="Reference: textbook fixed beam hinge state represented in OpenSees by freed support Ry plus residual moment.",
    )


def _d3_settlement() -> Model:
    """D3 prescribed support settlement; OpenSees uses sp constraints."""
    sec = square_section(100.0)
    L, delta = 2000.0, 1.0
    nodes = [
        Node(0, 0.0, 0.0, 0.0, [1, 1, 1, 1, 1, 1]),
        Node(1, L / 2.0, 0.0, 0.0, [0, 0, 0, 0, 0, 0]),
        Node(2, L, 0.0, 0.0, [1, 1, 1, 1, 1, 1], [0.0, 0.0, -delta, 0.0, 0.0, 0.0]),
    ]
    members = [
        Member(0, 0, 1, 0, 0, (0.0, 0.0, 1.0)),
        Member(1, 1, 2, 0, 0, (0.0, 0.0, 1.0)),
    ]
    return Model(
        "D3 prescribed settlement",
        materials=[STEEL],
        sections=[sec],
        nodes=nodes,
        members=members,
        notes="Reference: OpenSees sp imposed support displacement.",
    )


def _d4_combined_load() -> Model:
    """D4 self-weight + prestress-like axial load + asymmetric live load surrogate."""
    L, H = 6000.0, 3500.0
    nodes = [
        Node(0, 0.0, 0.0, 0.0, [1, 1, 1, 1, 1, 1]),
        Node(1, L, 0.0, 0.0, [1, 1, 1, 1, 1, 1]),
        Node(2, 0.0, 0.0, H, [0, 0, 0, 0, 0, 0]),
        Node(3, L, 0.0, H, [0, 0, 0, 0, 0, 0]),
    ]
    members = [
        _member(0, 0, 2, nodes),
        _member(1, 1, 3, nodes),
        _member(2, 2, 3, nodes),
    ]
    loads = [
        NodalLoad(2, [12000.0, 0.0, -60000.0, 0.0, 0.0, 0.0]),
        NodalLoad(3, [0.0, 0.0, -90000.0, 0.0, 0.0, 0.0]),
        NodalLoad(2, [0.0, 0.0, -35000.0, 0.0, 0.0, 0.0]),
        NodalLoad(3, [0.0, 0.0, -35000.0, 0.0, 0.0, 0.0]),
    ]
    return Model(
        "D4 combined gravity/prestress/live surrogate",
        materials=[CONCRETE],
        sections=[square_section(450.0)],
        nodes=nodes,
        members=members,
        nloads=loads,
        notes="Reference: equivalent linear OpenSees model; prestress represented by compressive nodal loads.",
    )


def _member(eid: int, i: int, j: int, nodes: list[Node]) -> Member:
    by_id = {n.id: n for n in nodes}
    a, b = by_id[i], by_id[j]
    return Member(eid, i, j, 0, 0, refvec_for((a.x, a.y, a.z), (b.x, b.y, b.z)))
