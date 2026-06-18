"""Small model schema shared by FrameCore token writer and OpenSees wrapper.

All dimensions and material values are in N, mm, MPa. Density follows the
FrameCore convention used by existing tests: kg/m^3 for input, converted to
tonne/mm^3 where OpenSees mass is needed.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable


Vec6 = list[float]


@dataclass
class Material:
    E: float
    G: float
    rho: float = 7850.0
    nu: float | None = None
    shell: bool = False


@dataclass
class Section:
    A: float
    Iy: float
    Iz: float
    J: float
    cy: float
    cz: float
    Asy: float = 0.0
    Asz: float = 0.0


@dataclass
class Node:
    id: int
    x: float
    y: float
    z: float
    fix: list[int]
    presc: Vec6 = field(default_factory=lambda: [0.0] * 6)


@dataclass
class Member:
    id: int
    i: int
    j: int
    mat: int
    sec: int
    refvec: tuple[float, float, float]
    active: int = 1
    tonly: int = 0


@dataclass
class Shell:
    id: int
    n: list[int]
    mat: int
    t: float
    active: int = 1


@dataclass
class NodalLoad:
    node: int
    comp: Vec6


@dataclass
class UDL:
    member: int
    wx: float
    wy: float
    wz: float


@dataclass
class SPress:
    shell: int
    p: float


@dataclass
class Hinge:
    member: int
    dof: int
    Mp: float


@dataclass
class Model:
    name: str
    materials: list[Material] = field(default_factory=list)
    sections: list[Section] = field(default_factory=list)
    nodes: list[Node] = field(default_factory=list)
    members: list[Member] = field(default_factory=list)
    shells: list[Shell] = field(default_factory=list)
    nloads: list[NodalLoad] = field(default_factory=list)
    udls: list[UDL] = field(default_factory=list)
    spress: list[SPress] = field(default_factory=list)
    hinges: list[Hinge] = field(default_factory=list)
    eigen: int = 0
    pdelta: int | None = None
    opt: str | None = None
    compare_nodes: list[int] | None = None
    skip_member_forces: bool = False
    skip_rf: bool = False
    analytic: dict[str, float | int | str] | None = None
    known_gap: str | None = None
    notes: str = ""
    tags: list[str] = field(default_factory=list)
    os_free_ry: list[int] = field(default_factory=list)
    os_hinge_moments: list[dict[str, float]] = field(default_factory=list)


@dataclass
class CaseSpec:
    case_id: str
    struct_type: str
    load_id: str
    mesh: str
    thickness: str
    tolerance_key: str
    builder: Callable[[], Model]
    oracle: str
    expected_gap: str | None = None


def square_section(side: float) -> Section:
    I = side**4 / 12.0
    return Section(
        A=side * side,
        Iy=I,
        Iz=I,
        J=0.1406 * side**4,
        cy=side / 2.0,
        cz=side / 2.0,
        Asy=0.0,
        Asz=0.0,
    )


def rect_section(b: float, d: float) -> Section:
    Iz = b * d**3 / 12.0
    Iy = d * b**3 / 12.0
    bb, tt = max(b, d), min(b, d)
    J = bb * tt**3 * (1.0 / 3.0 - 0.21 * (tt / bb) * (1.0 - tt**4 / (12.0 * bb**4)))
    return Section(A=b * d, Iy=Iy, Iz=Iz, J=J, cy=d / 2.0, cz=b / 2.0)


def refvec_for(pi: tuple[float, float, float], pj: tuple[float, float, float]) -> tuple[float, float, float]:
    vertical = abs(pi[0] - pj[0]) < 1.0e-9 and abs(pi[1] - pj[1]) < 1.0e-9
    return (1.0, 0.0, 0.0) if vertical else (0.0, 0.0, 1.0)
