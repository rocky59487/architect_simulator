"""A/B family frame, truss, bridge, and mixed shell/frame builders.

References: standard matrix-stiffness beam/frame benchmarks, Pratt truss
textbook load paths, and the project CLI protocol. Units: N, mm, MPa.
"""

from __future__ import annotations

import math

from ..model import Material, Member, Model, Node, Section, Shell, refvec_for, square_section
from .common import CONCRETE, SHELL_CONCRETE, STEEL, clone_with_load, plate_section


def build_frame_case(case_id: str, load_id: str) -> Model:
    base = {
        "A1": _a1_portal,
        "A2": _a2_multistory_frame,
        "A3": _a3_frame_shear_wall,
        "A4": _a4_tube_in_tube,
        "A5": _a5_pratt_truss,
        "A6": _a6_grid_roof,
        "A7": _a7_flat_plate_columns,
        "A8": _a8_drop_panel_edge_beam,
        "B1": _b1_simple_bridge,
        "B2": _b2_continuous_bridge,
        "B3": _b3_truss_bridge,
        "B4": _b4_arch_bridge,
    }[case_id]()
    return clone_with_load(base, load_id)


def _a1_portal() -> Model:
    """A1 portal frame sanity case; square RC members avoid local-axis ambiguity."""
    L, H = 6000.0, 3500.0
    nodes = [
        Node(0, 0.0, 0.0, 0.0, [1, 1, 1, 1, 1, 1]),
        Node(1, L, 0.0, 0.0, [1, 1, 1, 1, 1, 1]),
        Node(2, 0.0, 0.0, H, [0, 0, 0, 0, 0, 0]),
        Node(3, L, 0.0, H, [0, 0, 0, 0, 0, 0]),
    ]
    sec = square_section(500.0)
    members = [
        _member(0, 0, 2, nodes),
        _member(1, 1, 3, nodes),
        _member(2, 2, 3, nodes),
    ]
    return Model("A1 single-bay portal", [CONCRETE], [sec], nodes, members, notes="Oracle: OpenSees elasticBeamColumn.")


def _a2_multistory_frame() -> Model:
    """A2 regular 3D bay frame, reduced from the prompt scale for a fast default run."""
    return _grid_frame("A2 multistory multi-bay frame", nx=4, ny=3, nf=5, bay=5000.0, story=3200.0)


def _a3_frame_shear_wall() -> Model:
    """A3 frame with a central MITC4 wall strip; OpenSees uses ShellMITC4."""
    m = _grid_frame("A3 frame plus shear wall", nx=3, ny=2, nf=5, bay=4500.0, story=3200.0)
    m.materials.append(SHELL_CONCRETE)
    wall_mat = len(m.materials) - 1
    base_id = max(n.id for n in m.nodes) + 1
    shell_id = 0
    x = 4500.0
    y = 4500.0
    dz = 3200.0
    width = 2500.0
    left_ids = []
    right_ids = []
    for k in range(6):
        fix = [1, 1, 1, 1, 1, 1] if k == 0 else [0, 0, 0, 0, 0, 0]
        left_ids.append(base_id)
        m.nodes.append(Node(base_id, x - width / 2, y, k * dz, fix))
        base_id += 1
        right_ids.append(base_id)
        m.nodes.append(Node(base_id, x + width / 2, y, k * dz, fix))
        base_id += 1
    for k in range(5):
        m.shells.append(Shell(shell_id, [left_ids[k], right_ids[k], right_ids[k + 1], left_ids[k + 1]], wall_mat, 250.0))
        shell_id += 1
    m.notes += " Central wall is a faceted vertical MITC4 strip."
    return m


def _a4_tube_in_tube() -> Model:
    """A4 tube-in-tube surrogate: outer and inner shell tubes fixed at base."""
    m = Model("A4 tube-in-tube shell tower", materials=[SHELL_CONCRETE], notes="Oracle: OpenSees ShellMITC4 faceted tube.")
    levels = 6
    h = 3000.0
    outer = [(0.0, 0.0), (12000.0, 0.0), (12000.0, 9000.0), (0.0, 9000.0)]
    inner = [(4500.0, 3000.0), (7500.0, 3000.0), (7500.0, 6000.0), (4500.0, 6000.0)]
    nid = 0
    rings = []
    for pts in (outer, inner):
        ring_ids = []
        for k in range(levels + 1):
            ids = []
            for x, y in pts:
                m.nodes.append(Node(nid, x, y, k * h, [1, 1, 1, 1, 1, 1] if k == 0 else [0, 0, 0, 0, 0, 0]))
                ids.append(nid)
                nid += 1
            ring_ids.append(ids)
        rings.append(ring_ids)
    sid = 0
    for ring in rings:
        for k in range(levels):
            for i in range(4):
                m.shells.append(Shell(sid, [ring[k][i], ring[k][(i + 1) % 4], ring[k + 1][(i + 1) % 4], ring[k + 1][i]], 0, 180.0))
                sid += 1
    return m


def _a5_pratt_truss() -> Model:
    """A5 Pratt truss benchmark; beam elements with small bending stiffness compare axial load paths."""
    return _pratt("A5 30m Pratt truss", span=30000.0, panels=6, height=3500.0)


def _a6_grid_roof() -> Model:
    """A6 two-way roof grid/truss, 30 m by 40 m."""
    m = Model("A6 two-way grid roof", materials=[STEEL], sections=[square_section(120.0)], notes="Oracle: OpenSees 3D elasticBeamColumn.")
    nx, ny = 5, 6
    sx, sy = 30000.0 / nx, 40000.0 / ny
    nid = 0
    ids = []
    for j in range(ny + 1):
        row = []
        for i in range(nx + 1):
            edge = i in (0, nx) and j in (0, ny)
            m.nodes.append(Node(nid, i * sx, j * sy, 0.0, [1, 1, 1, 1, 1, 1] if edge else [0, 0, 0, 0, 0, 0]))
            row.append(nid)
            nid += 1
        ids.append(row)
    eid = 0
    for j in range(ny + 1):
        for i in range(nx):
            m.members.append(_member(eid, ids[j][i], ids[j][i + 1], m.nodes)); eid += 1
    for j in range(ny):
        for i in range(nx + 1):
            m.members.append(_member(eid, ids[j][i], ids[j + 1][i], m.nodes)); eid += 1
    for j in range(ny):
        for i in range(nx):
            m.members.append(_member(eid, ids[j][i], ids[j + 1][i + 1], m.nodes)); eid += 1
    return m


def _a7_flat_plate_columns() -> Model:
    """A7 flat plate floor with four columns; shell-column interface check."""
    return _slab_with_columns("A7 flat plate with four columns", drop=False)


def _a8_drop_panel_edge_beam() -> Model:
    """A8 plate-column model with drop panel and edge beams; thickness transition check."""
    return _slab_with_columns("A8 drop panel and edge beam", drop=True)


def _b1_simple_bridge() -> Model:
    """B1 simply supported bridge girder surrogate."""
    return _beam_line("B1 simple span bridge", spans=[20000.0])


def _b2_continuous_bridge() -> Model:
    """B2 three-span continuous bridge girder surrogate."""
    return _beam_line("B2 three-span continuous bridge", spans=[20000.0, 20000.0, 20000.0])


def _b3_truss_bridge() -> Model:
    """B3 longer truss bridge."""
    return _pratt("B3 truss bridge", span=45000.0, panels=9, height=5000.0)


def _b4_arch_bridge() -> Model:
    """B4 segmented parabolic arch bridge; curvature represented by straight beam segments."""
    m = Model("B4 segmented arch bridge", materials=[STEEL], sections=[square_section(180.0)], notes="Oracle: OpenSees faceted arch with same segments.")
    n = 12
    span, rise = 30000.0, 5000.0
    for i in range(n + 1):
        x = span * i / n
        z = 4.0 * rise * (x / span) * (1.0 - x / span)
        fix = [1, 1, 1, 1, 1, 1] if i in (0, n) else [0, 0, 0, 0, 0, 0]
        m.nodes.append(Node(i, x, 0.0, z, fix))
    for i in range(n):
        m.members.append(_member(i, i, i + 1, m.nodes))
    return m


def _grid_frame(name: str, nx: int, ny: int, nf: int, bay: float, story: float) -> Model:
    m = Model(name, materials=[CONCRETE], sections=[square_section(450.0)], notes="Oracle: OpenSees elasticBeamColumn.")
    nid = 0
    ids = {}
    for k in range(nf + 1):
        for j in range(ny + 1):
            for i in range(nx + 1):
                fix = [1, 1, 1, 1, 1, 1] if k == 0 else [0, 0, 0, 0, 0, 0]
                ids[(i, j, k)] = nid
                m.nodes.append(Node(nid, i * bay, j * bay, k * story, fix))
                nid += 1
    eid = 0
    for k in range(nf):
        for j in range(ny + 1):
            for i in range(nx + 1):
                m.members.append(_member(eid, ids[(i, j, k)], ids[(i, j, k + 1)], m.nodes)); eid += 1
    for k in range(1, nf + 1):
        for j in range(ny + 1):
            for i in range(nx):
                m.members.append(_member(eid, ids[(i, j, k)], ids[(i + 1, j, k)], m.nodes)); eid += 1
        for j in range(ny):
            for i in range(nx + 1):
                m.members.append(_member(eid, ids[(i, j, k)], ids[(i, j + 1, k)], m.nodes)); eid += 1
    return m


def _pratt(name: str, span: float, panels: int, height: float) -> Model:
    m = Model(name, materials=[STEEL], sections=[square_section(100.0)], notes="Oracle: OpenSees beam-column truss surrogate.")
    dx = span / panels
    for i in range(panels + 1):
        fix_bot = [1, 1, 1, 1, 1, 1] if i in (0, panels) else [0, 1, 0, 1, 0, 1]
        m.nodes.append(Node(i, i * dx, 0.0, 0.0, fix_bot))
        m.nodes.append(Node(100 + i, i * dx, 0.0, height, [0, 1, 0, 1, 0, 1]))
    eid = 0
    for i in range(panels):
        for a, b in [(i, i + 1), (100 + i, 100 + i + 1), (i, 100 + i), (i, 100 + i + 1)]:
            m.members.append(_member(eid, a, b, m.nodes)); eid += 1
    m.members.append(_member(eid, panels, 100 + panels, m.nodes))
    return m


def _beam_line(name: str, spans: list[float]) -> Model:
    m = Model(name, materials=[STEEL], sections=[plate_section(300.0, 900.0)], notes="Oracle: OpenSees continuous elastic beam.")
    x = 0.0
    m.nodes.append(Node(0, x, 0.0, 0.0, [1, 1, 1, 1, 0, 0]))
    for i, L in enumerate(spans, start=1):
        x += L
        m.nodes.append(Node(i, x, 0.0, 0.0, [0, 1, 1, 0, 0, 0] if i < len(spans) else [0, 1, 1, 0, 0, 0]))
    for i in range(len(spans)):
        m.members.append(_member(i, i, i + 1, m.nodes))
    return m


def _slab_with_columns(name: str, drop: bool) -> Model:
    m = Model(name, materials=[CONCRETE, SHELL_CONCRETE], sections=[square_section(400.0)], notes="Oracle: OpenSees ShellMITC4 plus columns.")
    nx = ny = 4
    L = 8000.0
    z = 3200.0
    nid = 0
    top = []
    for j in range(ny + 1):
        row = []
        for i in range(nx + 1):
            m.nodes.append(Node(nid, L * i / nx, L * j / ny, z, [0, 0, 0, 0, 0, 0]))
            row.append(nid)
            nid += 1
        top.append(row)
    sid = 0
    for j in range(ny):
        for i in range(nx):
            thick = 260.0 if drop and 1 <= i <= 2 and 1 <= j <= 2 else 180.0
            m.shells.append(Shell(sid, [top[j][i], top[j][i + 1], top[j + 1][i + 1], top[j + 1][i]], 1, thick))
            sid += 1
    eid = 0
    for i, j in [(0, 0), (nx, 0), (0, ny), (nx, ny)]:
        base = nid
        m.nodes.append(Node(base, L * i / nx, L * j / ny, 0.0, [1, 1, 1, 1, 1, 1]))
        m.members.append(_member(eid, base, top[j][i], m.nodes))
        nid += 1
        eid += 1
    if drop:
        edge_sec = len(m.sections)
        m.sections.append(square_section(300.0))
        for j in (0, ny):
            for i in range(nx):
                m.members.append(_member(eid, top[j][i], top[j][i + 1], m.nodes, sec=edge_sec)); eid += 1
        for i in (0, nx):
            for j in range(ny):
                m.members.append(_member(eid, top[j][i], top[j + 1][i], m.nodes, sec=edge_sec)); eid += 1
    return m


def _member(eid: int, i: int, j: int, nodes: list[Node], mat: int = 0, sec: int = 0) -> Member:
    by_id = {n.id: n for n in nodes}
    a, b = by_id[i], by_id[j]
    return Member(eid, i, j, mat, sec, refvec_for((a.x, a.y, a.z), (b.x, b.y, b.z)))
