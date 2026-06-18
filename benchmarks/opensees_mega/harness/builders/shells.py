"""C-family curved and warped shell builders.

All cases use faceted MITC4 quads in N, mm, MPa. C3 and C4 follow the geometry
used by `Tools/shell_mitc4_deep_audit.py`; C1/C2/C5 are faceted/surrogate
geometry checks and are reported as known smooth-shell oracle gaps.
"""

from __future__ import annotations

import math

from ..model import Material, Model, Node, Shell
from .common import clone_with_load


def build_shell_case(case_id: str, mesh_n: int, load_id: str) -> Model:
    base = {
        "C1": _c1_spherical_dome,
        "C2": _c2_hypar,
        "C3": _c3_pinched_cylinder,
        "C4": _c4_scordelis_lo,
        "C5": _c5_freeform_surrogate,
    }[case_id](mesh_n)
    return clone_with_load(base, load_id)


def _mat(E: float = 30000.0, nu: float = 0.3) -> Material:
    return Material(E=E, nu=nu, G=E / (2.0 * (1.0 + nu)), rho=2400.0, shell=True)


def _c1_spherical_dome(n: int) -> Model:
    """C1 quarter spherical dome, R=30 m, clamped symmetry edges; faceted smooth-shell surrogate."""
    R = 30000.0
    phi_min = math.radians(8.0)
    phi_max = math.radians(50.0)
    theta_max = math.radians(90.0)
    t = R / 200.0
    m = Model(
        f"C1 spherical dome n={n}",
        materials=[_mat()],
        known_gap="smooth spherical shell oracle not directly represented by CLI flat facets",
        notes="Oracle: OpenSees ShellMITC4 on identical faceted quarter dome; smooth shell is known gap.",
    )
    nid = 0
    ids = []
    for j in range(n + 1):
        row = []
        phi = phi_min + (phi_max - phi_min) * j / n
        for i in range(n + 1):
            theta = theta_max * i / n
            x = R * math.sin(phi) * math.cos(theta)
            y = R * math.sin(phi) * math.sin(theta)
            z = R * math.cos(phi)
            edge = i in (0, n) or j in (0, n)
            m.nodes.append(Node(nid, x, y, z, [1, 1, 1, 1, 1, 1] if edge else [0, 0, 0, 0, 0, 0]))
            row.append(nid)
            nid += 1
        ids.append(row)
    _quad_grid(m, ids, t)
    return m


def _c2_hypar(n: int) -> Model:
    """C2 hyperbolic paraboloid saddle; warped-quadrilateral sensitivity check."""
    L, amp, t = 12000.0, 300.0, 120.0
    m = Model(
        f"C2 hypar warped shell n={n}",
        materials=[_mat()],
        known_gap="warped quads are projected to flat MITC4 facets",
        notes="Oracle: OpenSees ShellMITC4 on identical faceted hypar mesh.",
    )
    nid = 0
    ids = []
    for j in range(n + 1):
        row = []
        yy = -L / 2.0 + L * j / n
        for i in range(n + 1):
            xx = -L / 2.0 + L * i / n
            z = amp * (xx / (L / 2.0)) * (yy / (L / 2.0))
            edge = i in (0, n) or j in (0, n)
            m.nodes.append(Node(nid, xx, yy, z, [1, 1, 1, 1, 1, 1] if edge else [0, 0, 0, 0, 0, 0]))
            row.append(nid)
            nid += 1
        ids.append(row)
    _quad_grid(m, ids, t)
    return m


def _c3_pinched_cylinder(n: int) -> Model:
    """C3 pinched cylinder geometry from the MITC4 deep audit; quarter model with symmetry supports."""
    R, L, t = 300.0, 600.0, 3.0
    m = Model(
        f"C3 pinched cylinder n={n}",
        materials=[_mat(3.0e6, 0.3)],
        known_gap="coarse faceted cylinder converges to the shell benchmark but is not a curved element",
        notes="Reference: classical pinched cylinder benchmark; deep audit target w=1.8248e-5 at fine mesh.",
    )
    base = 4000

    def nid(i, j):
        return base + j * (n + 1) + i

    ids = []
    for j in range(n + 1):
        row = []
        for i in range(n + 1):
            th = 0.5 * math.pi * i / n
            z = (L * 0.5) * j / n
            fix = [0, 0, 0, 0, 0, 0]
            if i == 0:
                fix[1] = fix[3] = fix[5] = 1
            if i == n:
                fix[0] = fix[4] = fix[5] = 1
            if j == n:
                fix[2] = fix[3] = fix[4] = 1
            if j == 0:
                fix[0] = fix[1] = 1
            m.nodes.append(Node(nid(i, j), R * math.cos(th), R * math.sin(th), z, fix))
            row.append(nid(i, j))
        ids.append(row)
    _quad_grid(m, ids, t)
    m.analytic = {"node": nid(0, n), "dof": 0, "value": -1.8248e-5, "label": "pinched cylinder literature displacement"}
    return m


def _c4_scordelis_lo(n: int) -> Model:
    """C4 Scordelis-Lo roof geometry from the MITC4 deep audit."""
    R, L, phi0, t = 25.0, 50.0, math.radians(40.0), 0.25
    m = Model(
        f"C4 Scordelis-Lo roof n={n}",
        materials=[_mat(4.32e8, 0.0)],
        known_gap="faceted roof should converge to Scordelis-Lo smooth-shell reference",
        notes="Reference: Scordelis-Lo roof free-edge vertical displacement, deep audit target 0.3024.",
    )
    base = 3000

    def nid(i, j):
        return base + j * (n + 1) + i

    ids = []
    for j in range(n + 1):
        row = []
        for i in range(n + 1):
            phi = phi0 * i / n
            y = (L * 0.5) * j / n
            fix = [0, 0, 0, 0, 0, 0]
            if j == 0:
                fix[0] = fix[2] = 1
            if j == n:
                fix[1] = fix[3] = fix[5] = 1
            if i == 0:
                fix[0] = fix[4] = fix[5] = 1
            m.nodes.append(Node(nid(i, j), R * math.sin(phi), y, R * math.cos(phi), fix))
            row.append(nid(i, j))
        ids.append(row)
    _quad_grid(m, ids, t)
    m.analytic = {"node": nid(n, n), "dof": 2, "value": -0.3024, "label": "Scordelis-Lo roof reference"}
    return m


def _c5_freeform_surrogate(n: int) -> Model:
    """C5 faceted freeform grid shell; NURBS/smooth geometry is not a current CLI primitive."""
    L, t = 14000.0, 120.0
    m = Model(
        f"C5 freeform faceted shell n={n}",
        materials=[_mat()],
        known_gap="true NURBS freeform shell is unsupported by CLI; this is a faceted surrogate",
        notes="Oracle: OpenSees ShellMITC4 on identical faceted sinusoidal freeform mesh.",
    )
    nid = 0
    ids = []
    for j in range(n + 1):
        row = []
        y = L * j / n
        for i in range(n + 1):
            x = L * i / n
            z = 250.0 * math.sin(math.pi * i / n) * math.sin(1.5 * math.pi * j / n)
            edge = i in (0, n) or j in (0, n)
            m.nodes.append(Node(nid, x, y, z, [1, 1, 1, 1, 1, 1] if edge else [0, 0, 0, 0, 0, 0]))
            row.append(nid)
            nid += 1
        ids.append(row)
    _quad_grid(m, ids, t)
    return m


def _quad_grid(model: Model, ids: list[list[int]], t: float) -> None:
    sid = 0
    ny = len(ids) - 1
    nx = len(ids[0]) - 1
    for j in range(ny):
        for i in range(nx):
            model.shells.append(Shell(sid, [ids[j][i], ids[j][i + 1], ids[j + 1][i + 1], ids[j + 1][i]], 0, t))
            sid += 1
