# FrameCore — Structural Mechanics Engine

A self-contained **C++17 + Eigen** 3-D structural finite-element engine: beam-columns,
MITC4 flat shells, a full linear-analysis suite, a progressive- and dynamic-collapse line,
incremental reanalysis, second-order and large-displacement (co-rotational) analysis,
tension-only members, plastic hinges with N–M interaction, sizing and topology optimization,
and a text/C-API bridge for external clients (Grasshopper). It is the structural core of an
"architect simulator" graduation project, built as a **research-grade engine prototype**:
deliberately small, engine-agnostic, and — the actual point of the project — **anchored to
an independent oracle for every capability it claims**.

The public API uses only plain C++/POD types (no UE, no Eigen leakage), so the same source
compiles as a standalone console gate *and* as an Unreal Engine module. The core remains
C++17-compatible; the UE module target is compiled as C++20 because of the current UBT/toolchain.

> **Status (2026-06, S1–S10 + supernodal direct lane):** the five-leg verification gate is green —
> standalone `ALL PASS` (fixtures **F1–F56**) · **52** UE automation tests ·
> **OpenSees** strict cross-validation PASS · deep audit **104** independent checks ·
> CLI round-trip ALL PASS. One repo-relative command reproduces it (`-Engine` or `UE_ENGINE_ROOT`
> can point at a non-sibling Unreal install):
> `powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees`.
> The capability → oracle → measured-agreement map is **[`docs/VERIFICATION.md`](docs/VERIFICATION.md)**.

---

## Capability map

The engine is organized as layers, each behind the same two seams (`IElement` for element
types, `PreparedSystem` for factorization reuse), and each gated by its own oracles.

### 1 · Linear core

| Capability | Notes |
|---|---|
| 3-D linear-elastic direct stiffness | 12×12 Euler–Bernoulli beam-column; sparse assembly; `SimplicialLDLT` |
| Timoshenko shear flexibility | optional; reduces to Euler–Bernoulli as slenderness grows |
| End releases / static indeterminacy | per-member `release[12]`; static condensation of stiffness **and** fixed-end forces |
| **Mechanism / instability detection** | from the LDLᵀ factorization (near-zero / negative pivots), **not** connectivity — refuses to report forces on an unstable model |
| **MITC4 flat shell** (24 DOF) | Reissner–Mindlin facet: membrane + plate bending with MITC4 assumed shear (no locking) + Hughes–Brezzi drilling; recovers `{Mxx,Myy,Mxy,Qx,Qy,Nxx,Nyy,Nxy}` |
| Shell upgrades (S8, opt-in) | **QM6** incompatible membrane (substantially reduces in-plane locking: Cook's −0.9 % vs Q4 −3.2 %; passes the weak patch test for general quads) and **DKQ** thin plate (Kirchhoff fast path); both default-off, **bit-identical** to baseline when off |
| Elastic D/C screen | combined axial + biaxial bending + peak-factored shear + torsion vs allowable capacities; reports the governing mode |
| Grillage plate idealization | ν-inflated woven beam grid; kept as a cheap approximation alongside the true shell |
| Member end forces / reactions | local `{N,Vy,Vz,T,My,Mz}` at both ends; `R = K·u − F` |

### 2 · Linear analysis suite

| Analysis | Notes |
|---|---|
| Load cases, combinations, envelopes | `combine` / `envelope`; self-weight from `Material.rho` |
| **Factorize-once, solve-many** | `assembleAndFactor` → opaque `PreparedSystem`; `solveLoad` reuses the LDLᵀ per load/settlement change — the interactive re-solve path |
| **Supernodal direct lane (opt-in, R-line)** | self-built BLAS3 supernodal Cholesky (METIS ordering + OpenBLAS dense panels): stateless `solveLoadSupernodal` and factor-once `SnSession`; `SimplicialLDLT` stays the **default + fallback**, supernodal is selected explicitly. vs LDLT rel < 1e-10; disabled is a bit-exact drop-in; a mechanism defers to the LDLT pivot guard. Multicore factor within ~1.0–1.2× of MKL-CHOLMOD (1.15–1.21× measured on 8940HX, ~1.0× at 17k DOF); single-machine reachable edge ~150 k DOF real-time on 32 GB (extrapolated from 17k–100k measurements). See [`docs/PROGRESS_R_supernodal.md`](docs/PROGRESS_R_supernodal.md) |
| Prescribed support settlement | matches OpenSees `sp()` to 0 |
| Influence lines / moving loads | unit load marched on the shared factorization; Müller-Breslau cross-check |
| Modal analysis | `Kφ=ω²Mφ`, consistent mass; dense default + opt-in sparse path; vs OpenSees `eigen` ~1e-11 |
| Linear buckling | geometric stiffness from axial force → Euler factor; opt-in sparse subspace path (F34) |
| Response spectrum | modal participation + SRSS/CQC (the code spectrum curve is an input) |
| Real-time transient | modal superposition + Newmark-β, O(nModes)/step |

### 3 · Progressive- & dynamic-collapse line

| Capability | Notes |
|---|---|
| Element removal | `Member.active` / `ShellQuad.active`; part of the reuse fingerprint |
| Safety margins | `worstUtilization` (worst D/C) + `pivotMargin` (continuous proximity-to-mechanism) |
| Debris connectivity | grounded vs detached components; each `FragmentCluster` carries closed-form mass/com/inertia — the UE5 **Chaos handoff** (rigid-body fall is the physics engine's job, by design) |
| **Collapse driver** | GSA-style LSP sequential linear analysis: remove the governing element while D/C > threshold, clean up debris, re-solve; `dlf` sudden-removal amplification; deterministic tie-breaks; per-step replay snapshots |
| Shell failure screen | surface von Mises (both faces, centre + corners) vs `Capacity.vm` |
| Plastic hinges (event-to-event) | hinges form at `\|M\| ≥ Mp` until a hinge mechanism; reproduces `w* = 16Mp/L²` to ±2 % |
| **N–M interaction (S10, opt-in)** | `Mp_eff(N) = Mp·max(0, 1−(N/Ny)²)` — exact for rectangles (first-principles neutral-axis shift), conservative for circles; default-off is **bit-identical** to the fixed-`Mp` driver |
| **Dynamic collapse (S2)** | continuous modal-space Newmark across removal events, **cross-event state inheritance** (M-orthonormal projection) + **momentum-preserving debris handoff** (`FragmentCluster.vel/angVel`); load-dependent Ritz basis with the truncation residual reported per event |

### 4 · Reanalysis & nonlinear line

| Capability | Notes |
|---|---|
| **ReSolve ladder (S1)** | three-tier incremental reanalysis for interactive editing: Tier-1 rank-k **Woodbury** (formula-exact; ~1e-12 of fresh — float path, not bit-identical), Tier-2 **stale-LDLᵀ PCG** (tolerance-grade), Tier-3 rebaseline (always-correct fallback); mechanism detection preserved across tiers |
| **P-Delta (S3)** | Theory-II second order: frozen pseudo-load iteration reusing the existing LDLᵀ (zero re-factor) **or** a K_T re-factor reference — the two paths cross-check to ~1e-13; P=0 is bit-identical to linear; past P_cr it reports `diverged` instead of a silent wrong answer |
| **Tension-only members (S4)** | cables / slender X-braces drop out under compression; active-set iteration whose inner re-solves ride the ReSolve ladder (rank-6 per flip); converged state == omitting the slack members; cycle guard + monotone fallback ensure finite termination |
| **Co-rotational large displacement (S9/S9b/S9c)** | geometrically nonlinear beam driver: planar → general 3-D (torsion + biaxial + SO(3) finite rotations, vs OpenSees corotational 1.22e-9) → **arc-length snap-through** path following (limit load vs OpenSees `ArcLength` 6.4e-3), consistent FD tangent, member UDL, prescribed large displacement; elastica benchmarks to ~1e-4 of Mattiasson's tables |

### 5 · Optimization

| Capability | Notes |
|---|---|
| **FSD sizing (S5)** | fully-stressed-design stress-ratio resizing + similar-section scaling + multi-load-case envelope; 10-bar truss lands 1 % above the pin-jointed literature optimum *because* the engine carries real joint bending (documented) |
| **BESO topology (S7)** | evolutionary hard-kill on `Member.active`, sensitivity = element strain energy (`Σα = ½F·u` to ~4e-14 on UDL-free members), compliance-best fallback, mechanism guard |
| **N2 collapse-robustness constraint (S7)** | optional: candidate topologies are screened by the collapse driver; the constrained result survives every single-member removal where the unconstrained one dies to one |

### 6 · Ecosystem (S6)

| Capability | Notes |
|---|---|
| `frame_cli` text bridge | stdin/stdout wire protocol ([`docs/CLI_PROTOCOL.md`](docs/CLI_PROTOCOL.md)) covering statics, shells, dynamics, collapse, tension-only, sizing, co-rotational, arc-length |
| Daemon mode | multi-request block streaming over one process — bit-identical to independent runs |
| C API DLL | `frame_capi.dll` shares the same protocol core; bit-identical to the CLI (ctypes-verified) |
| Grasshopper client | C# reference client (`Plugins/FrameSolver/Grasshopper/`); the packaged `.gha` component is **not** shipped (not gated — stated honestly) |

---

## Why trust it (the point of the project)

Every capability is anchored to an **independent oracle**, not a self-consistent re-run:
closed-form solutions, published benchmarks (independently re-derived before use), OpenSees
cross-validation, an independent dense solver inside the gate, **bit-identity no-op proofs**
for every opt-in feature, and rotation-equivariance checks. The full evidence chain —
five gate legs, oracle taxonomy, capability → fixture → *measured* agreement — lives in
**[`docs/VERIFICATION.md`](docs/VERIFICATION.md)**. Highlights:

- MITC4 shell vs OpenSees' **own `ShellMITC4`**: ~1e-10 (flat/tilted), ~1e-7–1e-8 (skewed+warped).
- 3-D co-rotational vs OpenSees `geomTransf Corotational`: 1.22e-9; arc-length limit load vs
  `integrator ArcLength`: 6.4e-3.
- Every incremental method (ReSolve, P-Delta frozen path, tension-only) is checked against a
  fresh-factorization reference at ~1e-12 or better.
- Measured agreements are reported separately from gate tolerances (which are looser on
  purpose, for float/library headroom).

**Positioning, honestly:** OpenSees is the *reference*, not a competitor — FrameCore's niche
is an embeddable, engine-agnostic core with a POD API, interactive factorization-reuse, and
a physics-engine debris handoff. The S1–S10 line was developed against a **Karamba3D
benchmarking roadmap** (`docs/KARAMBA3D_ROADMAP.md`): some capabilities track it
(second-order analysis, sizing/topology optimization, a parametric-CAD bridge), some lie
outside its documented feature set in specific research directions (collapse dynamics,
interactive reanalysis), and some of Karamba3D's strengths (EC3 design checks, the mature
Grasshopper ecosystem) are explicitly not claimed.

## Scope boundaries (read this — the engine is honest about what it is *not*)

- **D/C is an elastic / allowable-stress screen**, not RC ultimate strength or a design-code
  check. Shear is screened on the peak stress; rectangular torsion uses a conservative
  corner heuristic.
- **The MITC4 shell is a flat 4-node facet**: curved surfaces converge under refinement
  (benchmarks report it); one inherent low-energy element mode is documented and pinned by a
  spectrum oracle. QM6/DKQ help membranes / thin plates respectively; DKQ has deliberately
  **no** transverse shear. The grillage over-estimates transverse moments (~2 % deflection).
- **Dynamics, buckling, response spectrum are linear** (proportional damping; buckling is the
  onset eigenvalue). **P-Delta is a Theory-II linearization** (axial force frozen at first
  order, small sway) — large displacement belongs to the co-rotational driver.
- **The co-rotational driver is beams-only, small-strain / large-rotation**: nodal loads,
  member UDL (initial-configuration equivalent) and prescribed displacement; no shells, no
  hinge/tension-only coupling, no snap-back / bifurcation branching (cylindrical arc-length
  follows the primary path); the consistent tangent is finite-difference, not analytic.
- **The collapse driver is LSP-grade sequential linear analysis** (linear between events, no
  inertia beyond scalar `dlf`, no membrane/catenary; literature places LSP at roughly ±30 %
  on collapse extent — expect conservative results). **Hinges are event-to-event**: no
  unloading/reversal, zero hinge length; S10 adds *uniaxial* N–M interaction only — no
  My–Mz biaxial coupling, no N–M tangent. **No fiber sections / pushover, deliberately.**
- **The dynamic-collapse driver is linear-elastic in modal space between events**; events
  trigger on a whole step (O(dt)); truncated Ritz bases report their residual; the Chaos
  handoff is one-way.
- **ReSolve Tier-1 is formula-exact but not bit-identical** (rank-k Woodbury; ~1e-12 of a
  fresh factor+solve, the residual being floating-point path difference). **Tier-2 is
  tolerance-grade** (stale-factor PCG, ~1e-11 residual class). Only **Tier-3** (full
  rebaseline) is correct by construction. The ladder assumes geometry/supports/sections
  unchanged — anything else rebaselines automatically.
- **FSD is the optimum only for statically determinate structures** (heuristic fixed point
  otherwise); no lateral-torsional buckling, no displacement constraints. **BESO is a
  heuristic** (no global-optimality claim); its sensitivity is linear-elastic and is
  energy-exact only for UDL-free members (for a member under UDL the computed `½Qᵀu` is an
  approximate screen, no quantified oracle — see `PROGRESS_S7.md`).
- The **tension-only** model is an axial-sign active set with **no universal convergence
  guarantee**: a cycle-hash guard detects loops and switches to a monotone (deactivate-only)
  fallback that terminates in ≤ nTO+1 steps. No pretension, no slack length, no cable sag.

Per-stage limitation lists (more detailed than the above) close every
`docs/PROGRESS_S*.md`.

---

## Build & test

**Standalone gate (fastest — seconds):** compiles FrameCore + the oracle fixtures and runs them.

```bat
Plugins\FrameSolver\Standalone\build.bat
```
Expected: `[PASS] Fn …` lines, then `ALL PASS (failures=0)`, exit 0. (Needs Visual Studio
with the C++ toolset, located via `vswhere`; **and** a conda `framecore-direct` env with OpenBLAS +
METIS — the standalone gate now links the opt-in supernodal lane, and `build.bat` exits 1 without it.)

**One-click five-leg gate** (standalone + UE automation + OpenSees + deep audit + CLI):

```powershell
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
```

**Unreal Engine** (the engine as a UE module): open `ArchSim.uproject`, or headless:

```bat
Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development -project=...\ArchSim.uproject
Engine\Binaries\Win64\UnrealEditor-Cmd.exe ...\ArchSim.uproject -ExecCmds="Automation RunTests FrameCore; Quit" -unattended -nullrhi -nopause
```

> `run_gate.ps1` runs the UE automation but does **not** rebuild the UE module — rebuild
> first (command above) after touching engine code, or the automation runs a stale binary.
> The `$ExpectedUeTests = 52` guard catches a silently-missing test.

**Try the engine without writing C++** — the text bridge solves a model from stdin:

```bat
Plugins\FrameSolver\Standalone\build_cli.bat
(
  echo MAT 210000 80769 7850
  echo SEC 10000 8.333e6 8.333e6 1.406e7 50 50 8333 8333
  echo NODE 0 0 0 0  1 1 1 1 1 1
  echo NODE 1 2000 0 0  0 0 0 0 0 0
  echo MEMBER 0 0 1 0 0  0 0 1
  echo NLOAD 1 0 0 -1000 0 0 0
  echo END
) | Plugins\FrameSolver\Standalone\frame_cli.exe
```

A 2 m cantilever with a 1 kN tip load — the `DISP` row for node 1 reports the
`-PL³/3EI` tip deflection.

(Full wire protocol — shells, modal, collapse, sizing, co-rotational, arc-length — in
[`docs/CLI_PROTOCOL.md`](docs/CLI_PROTOCOL.md).)

## Minimal use (C++)

```cpp
#include "FrameCore/FrameSolver.h"
#include "FrameCore/ElasticAllowable.h"
using namespace frame;

Material mat(210000.0, 80769.0, 7850.0);            // E, G (MPa), rho
mat.cap = Capacity::make(300.0, 300.0, 180.0);      // allowable comp/tens/shear (MPa)
Section sec = Section::Rectangular(100.0, 100.0);   // b, d (mm)

FrameModel m;
m.materials = { mat };  m.sections = { sec };       // material index 0, section index 0
Node n0(0, 0,0,0);  n0.fixAll();                    // encastre base
Node n1(1, 2000,0,0);                               // 2 m cantilever
m.nodes = { n0, n1 };
m.members = { Member(0, 0, 1, 0, 0) };              // matIdx = 0, secIdx = 0
NodalLoad p;  p.node = 1;  p.comp[Uz] = -1000.0;    // 1 kN tip load
m.nodalLoads = { p };

SolveResult r = solve(m);                           // SolveOptions optional
if (!r.singular) {
    double tip = r.disp(1, Uz);                     // = -PL^3/3EI
    DemandResult d = ElasticAllowable{}.checkSection(r.memberForces[0].endI, sec, mat.cap);
    // d.risk (D/C), d.mode (governing failure mode)
}
```

> **Material/Section by index:** `Member`/`ShellQuad` reference material & section by
> **index** (`matIdx`/`secIdx`) into `FrameModel::materials`/`sections` — adding
> nodes/members/materials can never dangle them; `validate()` range-checks the indices.

## Repository layout

```
ArchSim.uproject                       UE host project (engine-as-module shell)
Plugins/FrameSolver/
  Source/FrameCore/                     the engine (pure C++17 + Eigen, UE-agnostic)
    Public/FrameCore/*.h                POD-only public API (model, solver, analyses,
                                        collapse, reanalysis, corotational, optimization)
    Private/*.cpp                       implementation (+ Private/FrameEigen.h: the single
                                        Eigen include site, dual-build guarded)
    Private/Tests/*.cpp                 52 UE automation tests (UE-side oracle mirrors)
  Standalone/                           console gates + CLI/C-API drivers (see its README)
  Grasshopper/                          C# reference client for the text bridge
Scripts/run_gate.ps1                    the one-click five-leg gate
Tools/                                  validation tools (drive frame_cli.exe): opensees_compare,
                                        pdelta_compare, cli_roundtrip, precision audits
docs/                                   see docs/README.md — architecture, verification map,
                                        wire protocol, per-stage progress records, specs
```

## Documentation map

| Document | What it is |
|---|---|
| [`docs/VERIFICATION.md`](docs/VERIFICATION.md) | **the evidence chain**: capability → oracle → gate fixture → measured agreement |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | data model, solve pipeline, conventions, element abstraction |
| [`docs/CLI_PROTOCOL.md`](docs/CLI_PROTOCOL.md) | the `frame_cli` wire protocol |
| per-stage records ([`docs/README.md`](docs/README.md) indexes `PROGRESS_S1.md` … `S10.md`) | what each stage built, its oracles, its honest limits |
| [`docs/README.md`](docs/README.md) | full docs index — including which documents are historical records |

## Roadmap (unimplemented, in rough order of value)

- **Visualization data line (C6–C8):** along-member BMD/SFD diagrams, utilization fields,
  redundancy reporting for a UI.
- **UE5 visualization layer:** collapse replay from `CollapseStep` snapshots,
  `FragmentCluster` → Chaos debris, D/C heat-maps — consuming the existing POD results.
- **S11 — MITC9i higher-order shell** (last on purpose: nine engine seams must move first).
- True material nonlinearity (fiber sections / pushover) stays **deliberately excluded**.

## License / use

Graduation-project code. FrameCore's default solver depends only on Eigen (MPL2, header-only); the opt-in supernodal lane additionally uses OpenBLAS (BSD) + METIS (Apache-2.0) via the conda `framecore-direct` env. OpenSees is
used for offline validation only and is **not** redistributed or linked into the engine.
