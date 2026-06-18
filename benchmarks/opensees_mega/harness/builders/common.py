"""Shared builder helpers.

All builders use N, mm, MPa and keep references in docstrings or notes on the
returned model.
"""

from __future__ import annotations

import copy
import math

from ..model import Material, Model, NodalLoad, Section, UDL


STEEL = Material(E=210000.0, G=80769.0, rho=7850.0)
CONCRETE = Material(E=30000.0, G=12500.0, rho=2400.0, nu=0.2)
SHELL_CONCRETE = Material(E=30000.0, G=30000.0 / (2.0 * 1.3), rho=2400.0, nu=0.3, shell=True)


def clone_with_load(base: Model, load_id: str) -> Model:
    m = copy.deepcopy(base)
    m.tags.append(load_id)
    if load_id == "L1":
        add_gravity_surrogate(m)
    elif load_id == "L2":
        add_uniform_live_load(m)
    elif load_id == "L3":
        add_point_load(m)
    elif load_id == "L4":
        add_asymmetric_load(m)
    elif load_id == "L5":
        add_wind_load(m)
    elif load_id == "L6":
        m.eigen = min(10, max(3, len(m.nodes) // 3))
    else:
        raise ValueError(load_id)
    return m


def node_lookup(model: Model):
    return {n.id: n for n in model.nodes}


def member_length(model: Model, member) -> float:
    ns = node_lookup(model)
    a, b = ns[member.i], ns[member.j]
    return math.sqrt((b.x - a.x) ** 2 + (b.y - a.y) ** 2 + (b.z - a.z) ** 2)


def add_gravity_surrogate(model: Model) -> None:
    # Deterministic, conservative gravity-like nodal loading. This is deliberately
    # a benchmark load case, not a design self-weight calculator.
    loads: dict[int, float] = {}
    for e in model.members:
        if not e.active:
            continue
        w = 0.003 * model.sections[e.sec].A * member_length(model, e) / 1000.0
        loads[e.i] = loads.get(e.i, 0.0) - 0.5 * w
        loads[e.j] = loads.get(e.j, 0.0) - 0.5 * w
    for s in model.shells:
        area = shell_area(model, s)
        w = 0.024 * s.t * area / 1000.0
        for nid in s.n:
            loads[nid] = loads.get(nid, 0.0) - 0.25 * w
    for nid, fz in sorted(loads.items()):
        if abs(fz) > 0.0:
            model.nloads.append(NodalLoad(nid, [0.0, 0.0, fz, 0.0, 0.0, 0.0]))
    model.notes += " L1 uses deterministic gravity-like equivalent nodal loads."


def add_uniform_live_load(model: Model) -> None:
    horizontal_members = []
    ns = node_lookup(model)
    for e in model.members:
        if not e.active:
            continue
        a, b = ns[e.i], ns[e.j]
        if abs(a.z - b.z) < 1.0e-9 and member_length(model, e) > 100.0:
            horizontal_members.append(e)
    if horizontal_members:
        for e in horizontal_members:
            # FrameCore local-y follows the projected refVec. For the benchmark's
            # horizontal members refVec=(0,0,1), so local-y is vertical global Z.
            model.udls.append(UDL(e.id, 0.0, -0.8, 0.0))
        model.notes += " L2 uses member UDL on horizontal members."
    elif model.shells:
        shell_nodal_pressure(model, pressure=-0.01)
        model.notes += " L2 uses shell pressure represented as matching nodal loads."
    else:
        add_point_load(model)


def add_point_load(model: Model) -> None:
    free = [n for n in model.nodes if not all(n.fix)]
    if not free:
        return
    target = max(free, key=lambda n: (n.z, n.x + n.y))
    scale = 50000.0 if model.members else 100.0
    model.nloads.append(NodalLoad(target.id, [0.0, 0.0, -scale, 0.0, 0.0, 0.0]))
    model.notes += f" L3 point load at node {target.id}."


def add_asymmetric_load(model: Model) -> None:
    free = [n for n in model.nodes if not all(n.fix)]
    if not free:
        return
    target = max(free, key=lambda n: (n.x - n.y, n.z))
    model.nloads.append(NodalLoad(target.id, [25000.0, -12000.0, -25000.0, 0.0, 0.0, 0.0]))
    model.notes += f" L4 asymmetric load at node {target.id}."


def add_wind_load(model: Model) -> None:
    free = [n for n in model.nodes if not all(n.fix)]
    if not free:
        return
    max_z = max(n.z for n in free)
    targets = [n for n in free if n.z > 0.6 * max_z]
    f = 20000.0 / max(len(targets), 1)
    for n in targets:
        model.nloads.append(NodalLoad(n.id, [f, 0.0, 0.0, 0.0, 0.0, 0.0]))
    model.notes += " L5 wind load on upper nodes."


def shell_area(model: Model, shell) -> float:
    ns = node_lookup(model)
    p = [(ns[nid].x, ns[nid].y, ns[nid].z) for nid in shell.n]
    return 0.5 * norm(cross(vsub(p[2], p[0]), vsub(p[3], p[1])))


def shell_nodal_pressure(model: Model, pressure: float) -> None:
    loads: dict[int, float] = {}
    for s in model.shells:
        area = shell_area(model, s)
        fz = pressure * area * 0.25
        for nid in s.n:
            loads[nid] = loads.get(nid, 0.0) + fz
    for nid, fz in sorted(loads.items()):
        model.nloads.append(NodalLoad(nid, [0.0, 0.0, fz, 0.0, 0.0, 0.0]))


def vsub(a, b):
    return [a[i] - b[i] for i in range(3)]


def cross(a, b):
    return [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]


def norm(v) -> float:
    return math.sqrt(sum(x * x for x in v))


def plate_section(width: float, depth: float) -> Section:
    return Section(
        A=width * depth,
        Iy=depth * width**3 / 12.0,
        Iz=width * depth**3 / 12.0,
        J=0.1406 * min(width, depth) ** 4,
        cy=depth / 2.0,
        cz=width / 2.0,
    )
