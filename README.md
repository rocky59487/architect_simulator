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

> **This repository ships two independent engines:** FrameCore (this document, `Plugins/FrameSolver/`)
> and **LevelSim** — a surveying-level simulator — at [`Plugins/LevelSim/`](Plugins/LevelSim/README.md).
> They share no code and can be built, tested, and released independently; the bundled
> `v2.2+1` release packaged them together (FrameCore v2.2 + LevelSim v1.0.0). Every release
> from `v2.3` onwards is FrameCore-only — LevelSim has not changed since `v2.2+1`.

> **Status (2026-06-22, v3.2.0 — FrameCoreUE thin-slice UE reflection module):** the v3.0.0
> STABLE anchor + v3.0.1 hardening + v3.1.0 S11 visualisation numerical layer + v3.2.0
> UE-side reflection layer. v3.2.0 adds a new `FrameCoreUE` plugin module that exposes
> `FRAMECORE_API computeStressField` to Blueprint designers (USTRUCT mirrors of
> `frame::StressField`, `UFrameCoreStressFieldLibrary`) and editor dev tools (a Slate
> utility panel `SFrameCoreStressFieldPanel` registered as a nomad tab under
> WorkspaceMenu/Tools). **Engine source delta v3.1.0..v3.2.0 = 0 lines under
> `Plugins/FrameSolver/Source/FrameCore/`** (engine rule #1 preserved; verified by
> standalone F1..F70 bit-identical); v3.2 net delta is 10 new files (`FrameCoreUE/`
> module shell + USTRUCT + library + panel + 2 smoke tests) + 4 lockstep version pins.
> On the integrator's host the **6 CPU-only legs run green against the rebuilt v3.2.0
> source**: standalone F1..F70 ALL PASS, **UE 70/70 ALL PASS** (v3.2.1 adds 8
> `FrameCore.UE.*` Phase 6 a-h tests on top of v3.2.0's BlueprintSmokeTest +
> EditorSmokeTest pair), **OpenSees strict
> PASS**, deep audit 104 PASS, CLI round-trip 13 ALL PASS, v2_roundtrip CPU ALL PASS
> (`kEngineVer=3.2.0` pin enforced; capability list unchanged). The 3 CUDA legs
> (`run_gpu_gate.ps1 -Strict`) are reachable but were not exercised in this release
> session — v3.2.0 has zero source delta in the CUDA path, so the v3.0.0 / v3.1.0 GPU
> evidence (`r2_bench --gpu 90k margin +11.939 ms`, `F67s STRICT_EXECUTED` fingerprint)
> carries forward unchanged. See
> [docs/RELEASE_v3.2.0.md](docs/RELEASE_v3.2.0.md) for the full reproduction matrix and
> [docs/HANDOFF_v3.2.0.md](docs/HANDOFF_v3.2.0.md) for the next-cycle pickup guide.
>
> v3.1.0 (S11 stress-field post-process) shipped `StressKernel.h` as the single source
> of truth shared between `ElasticAllowable` (D/C screen) and the new `StressField`
> (visualisation post-process), plus three standalone fixtures (F68 cantilever member
> field, F69 clamped-plate shell layer recovery + 30° z-rotation invariance, F70 D/C
> interlock — all bit-exact through the shared kernel) and the `inspect.stress_field`
> v2 dispatcher capability. v3.2.0 builds the UE5 consumer-side surface on top of
> v3.1.0's `FRAMECORE_API computeStressField`.
>
> v3.0.0 STABLE folded five hardening items + 7-agent audit fixes on top of v2.11.1
> (`f09a197`); v3.0.1 patches the six follow-up findings from the post-release
> consistency review:
> (1) `Scripts/run_gpu_gate.ps1` resolves `SUPERNODAL_CONDA` through one canonical
>     resolver (env var → legacy alias → conda layout probe);
> (2) `build_sn_cuda.bat` derives `CUDA_ROOT` from `SUPERNODAL_CONDA` (strip `\Library`
>     suffix → env root), with `CUDA_ROOT` / `CUDA_PATH` explicit overrides;
> (3) `build_capi_v2_cuda.bat` mirrors (2) block-for-block so dispatcher + standalone
>     CUDA builds never silently drift;
> (4) F67 (standalone) and `FFrameCoreGpuBacksubTest` (UE) keep their existing
>     smoke semantics (tolerate silent CPU fallback for dev-box compile tests); two NEW
>     fixtures F67s + `FFrameCoreGpuBacksubStrictTest` enforce real GPU attachment
>     when `FRAMECORE_GPU_STRICT=1` (set automatically by `run_gpu_gate.ps1` when the
>     cuDSS runtime DLL resolves) — silent fallback now FAILS strict CI, and v3.0.1
>     adds a `[F67s] STRICT_EXECUTED` fingerprint that `run_gate.ps1` + `run_gpu_gate.ps1`
>     grep for to catch the case where the strict branch was somehow not run;
> (5) v3.0.1 syncs the version surface: `kEngineVer = "3.0.1"` (was stale at "2.11.1"
>     when v3.0.0 was tagged), `FrameSolver.uplugin` `VersionName = "3.0.1"` +
>     `IsBetaVersion = false`, `FRAMECORE_EXPECTED_ENGINE_VER = '3.0.1'` in
>     `run_gpu_gate.ps1`; `r2_bench --gpu 90k` gains a baseline-regression hard gate
>     (margin ≥ +8 ms vs v2.11.0 baseline ~+11.94 ms, not just "margin ≥ 0 vs the
>     16.67 ms budget"); `FrameCore.Build.cs` normalizes `SUPERNODAL_CONDA` the same
>     way the bat does (accept env-root OR `\Library`); `.github/workflows/release-gate.yml`
>     runs the CPU-only legs on every push to `main` and uploads gate logs as artifacts.
>
<a id="v3-stable-conditions"></a>
> ### V3 STABLE gates (all green; ran in one session on the integrator's host)
>
> The same three gate suites that flipped v3.0.0 STABLE are the contract every release
> on the v3.x line is held to — and v3.0.1 raised the bar with strict-execution
> fingerprints + perf regression threshold:
>
> 1. `Scripts\run_gate.ps1 -RequireOpenSees` exits 0
>    (standalone F1..F70 default / F1..F70 + F67/F67s in CUDA build + UE **70/70** with cuDSS,
>    **68/68** without — pass `-ExpectedUeTests 68` in the latter case; OpenSees
>    strict; deep audit 104; CLI round-trip 13). Under `FRAMECORE_GPU_STRICT=1`
>    additionally requires `[F67s_UE] STRICT_EXECUTED` fingerprint in the UE log.
> 2. `Plugins\FrameSolver\Standalone\build_capi_v2.bat` + `python Tools\v2_roundtrip.py`
>    exits 0 (CPU dispatcher round-trip; `kEngineVer="3.2.0"` pinned per v3.2.0 wire-ABI
>    contract — v3.2.x patches leave kEngineVer unchanged; `inspect.stress_field` shape +
>    range guards exercised; 23 capabilities advertised).
> 3. `Scripts\run_gpu_gate.ps1 -Strict` exits 0 on a box with cuDSS installed
>    (frametest_cuda F1..F70 default + F67 smoke + F67s strict with STRICT_EXECUTED
>    fingerprint, v2_roundtrip CUDA, r2_bench --gpu 90k margin ≥ +8 ms hard regression gate).
>
> v2.10 introduced
> the cuDSS GPU backsub lane as an opt-in production path; v2.11 stacked three GPU phases
> (Phase 1 cuSPARSE SpMV reactions, Phase 2 single CUDA stream + async memcpy, Phase 3'
> Qf-detection cache) and reached **60 fps through 200 K DOF** on RTX 5070 Ti Laptop —
> a 12.3× / 35× speedup at 90 k vs v2.9.0 LAZY / pre-LAZY CPU respectively. v2.11.1
> is the hardening pass on top: `kEngineVer` 2.11.0 → 2.11.1, uplugin `VersionName` bumped,
> `run_gpu_gate.ps1` engine-version pin fixed (had silently stayed at `2.10.0`),
> `FrameCore.Build.cs` + `FrameCoreModule.cpp` cuDSS lane now honour `SUPERNODAL_CONDA` env
> var (previously hardcoded `%USERPROFILE%\anaconda3\envs\framecore-direct`, silently
> broke for Miniconda / custom env names), `cudaStreamCreate` failures emit explicit
> diagnostic instead of silently disabling Phase-2 overlap, `cudaDeviceSynchronize` in
> ctor narrowed to `cudaStreamSynchronize` (no longer blocks unrelated CUDA work in the
> same process), UE DLL preload warns on `GetDllHandle` returning null (no more silent
> delay-fault), `environment.yml` documents the optional CUDA package install, docs
> resynced to the current 58/F67/104/etc. counts.  v2.11.1-RC stacks five extra hardening
> items on top (above), bumping UE test count 58 → **59** when built with cuDSS
> (`FFrameCoreGpuBacksubStrictTest`) and adding F67s (strict) alongside F67 (smoke) on the
> standalone CUDA gate. **FrameCore engine source delta v2.11.0..v2.11.1-RC = ~6 files /
> ~120 lines, all additive guards / env-var overrides / version strings / new test
> fixtures.** The six-leg verification gate stays green for reachable legs —
> standalone `ALL PASS` (62 + 1 individual F-fixtures spanning **F1..F66 default build,
> F1..F67 + F67s in CUDA build** — F41 and F60 are intentionally absent, see [`docs/VERIFICATION.md`](docs/VERIFICATION.md)) ·
> **59** UE automation tests with cuDSS (57 without; v2.11.1-RC added
> `FFrameCoreGpuBacksubStrictTest` for silent-fallback detection on top of v2.11's
> `FFrameCoreGpuBacksubTest` smoke) · **OpenSees** strict cross-validation PASS ·
> deep audit **104** independent checks · CLI round-trip ALL PASS. One repo-relative
> command reproduces it
> (`-Engine` or `UE_ENGINE_ROOT` can point at a non-sibling Unreal install):
> `powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees`.
> The optional 6th + GPU gate legs (v2 dispatcher round-trip CPU + CUDA + production GPU
> perf sanity) live in `Scripts\run_gpu_gate.ps1`, which soft-skips on hosts without cuDSS.
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
| Warped shell quads (v3, opt-in) | `warpTolerance` relaxes `validate()`'s hard rejection of non-coplanar quads (free-surface meshes can solve at all); `useWarpingCorrection` projects corners onto the best-fit (Newell/centroid) plane and records `warp_[k]`. **MITC4 stays a flat facet** — warp adds an O(warp²) bounded error that shrinks as warp shrinks (F61c: warp 4 %→2 %→1 % gives Nxx err 1.6e-3→4e-4→1e-4) → **a warped free-surface mesh reaches accuracy by mesh refinement**, not by a per-element magic fix. Default-off, bit-identical to today |
| Elastic D/C screen | combined axial + biaxial bending + peak-factored shear + torsion vs allowable capacities; reports the governing mode |
| Grillage plate idealization | ν-inflated woven beam grid; kept as a cheap approximation alongside the true shell |
| Member end forces / reactions | local `{N,Vy,Vz,T,My,Mz}` at both ends; `R = K·u − F` |

### 2 · Linear analysis suite

| Analysis | Notes |
|---|---|
| Load cases, combinations, envelopes | `combine` / `envelope`; self-weight from `Material.rho` |
| **Factorize-once, solve-many** | `assembleAndFactor` → opaque `PreparedSystem`; `solveLoad` reuses the LDLᵀ per load/settlement change — the interactive re-solve path |
| **Supernodal direct lane (opt-in, R-line)** | self-built BLAS3 supernodal Cholesky (METIS ordering + OpenBLAS dense panels). Three integration modes: **(a)** `SolveOptions::useSupernodalPrimary` — `assembleAndFactor` builds the supernodal Cholesky as the **primary** factor and **skips the LDLT entirely** on SPD success (R2.1 PERF-01 architectural fix; gate F63 verifies bit-equivalence to the LDLT path and that mechanism detection still works via the L-diagonal pivot screen). Single-solve win at scale: 8× faster factor at 18.7k DOF, **20× at 62k DOF** (`perf_sn.exe` first-hand). **(b)** stateless `solveLoadSupernodal` — always builds its own supernodal factor, falls back to LDLT on SPD failure. **(c)** factor-once `SnSession` — reuses the supernodal factor across many `solveFrame` calls (and *transparently reuses the SnPrimary factor* if the PreparedSystem already holds one, so no double-build). vs LDLT rel < 1e-10 in all modes; default (no flags) is bit-exact drop-in. Multicore factor within ~1.0–1.2× of MKL-CHOLMOD (1.15–1.21× measured on 8940HX with conda OpenBLAS at 32k–64k DOF; ~1.0× at 17k DOF — the ratio drifts with the OpenBLAS build, hardware, and DOF range). Single-machine reachable edge **~150 k DOF interactive on 32 GB** `[THEORY: 外推]` with `useSupernodalPrimary` (extrapolated from 18 k / 62 k measured factor scaling at exponent ~1.5; not yet directly measured at 150 k, but the architectural blocker is gone). Constraint: `useSupernodalPrimary` skips LDLT, so analyses that need it (modal / buckling / P-Delta / ReSolve / dynamic-collapse) refuse on a SnPrimary PreparedSystem with a clear diagnostic; build a default PreparedSystem for those workflows. See [`docs/PROGRESS_R_supernodal.md`](docs/PROGRESS_R_supernodal.md) and [`docs/specs/v3_memory_recon.md`](docs/specs/v3_memory_recon.md). |
| Prescribed support settlement | matches OpenSees `sp()` to 0 |
| Influence lines / moving loads | unit load marched on the shared factorization; Müller-Breslau cross-check |
| Modal analysis | `Kφ=ω²Mφ`, consistent mass; dense default + opt-in sparse path; vs OpenSees `eigen` ~1e-11 |
| Linear buckling | geometric stiffness from axial force → Euler factor; opt-in sparse subspace path (F34). Opt-in **shell** K_σ (`shellGeometricStiffness`): MITC4 membrane stress → transverse stress stiffening → plate buckling `N_cr=4π²D/a²` (F57; w-only, flat-facet O(1/N²)) |
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
| **Co-rotational large displacement (S9/S9b/S9c + shell EICR)** | geometrically nonlinear beam driver: planar → general 3-D (torsion + biaxial + SO(3) finite rotations, vs OpenSees corotational 1.22e-9) → **arc-length snap-through** path following (limit load vs OpenSees `ArcLength` 6.4e-3), consistent FD tangent, member UDL, prescribed large displacement; elastica benchmarks to ~1e-4 of Mattiasson's tables. Opt-in **EICR shells** (`shellCorotational`): large-displacement MITC4 facets, rotation-invariant to 1e-14, strip elastica ~1e-4 of Mattiasson (NR load-control; shell arc-length later phase) |

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
  onset eigenvalue). Shell buckling (opt-in shell K_σ) is **facet-level w-only stress stiffening**
  verified against the analytic plate load (F57); a curved-shell buckling result is that facet K_σ
  plus flat-facet mesh approximation — no curved-shell benchmark yet, and no post-buckling (that is
  the shell co-rotational line). **P-Delta is a Theory-II linearization** (axial force frozen at first
  order, small sway) — large displacement belongs to the co-rotational driver.
- **The co-rotational driver is beams + opt-in EICR shells, small-strain / large-rotation**: nodal
  loads, member UDL (initial-configuration equivalent) and prescribed displacement. Opt-in
  `shellCorotational` adds EICR large-displacement MITC4 shells (NR load-control; rotation-invariant
  to 1e-14, large-deflection strip to ~1e-4 of Mattiasson) — but **shell arc-length post-buckling,
  CR-consistent shell-force recover and an analytic shell tangent are later phases**, and the
  flat-facet O(1/N²) surface approximation is unchanged (the CR frame removes rigid rotation, not the
  faceting). No hinge/tension-only coupling, no snap-back / bifurcation branching (cylindrical
  arc-length follows the primary path); the consistent tangent is finite-difference, not analytic.
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
- **Arc-length snap-through (`CorotationalOptions::arcLength`) needs an explicit step**.
  The `arcLength=0` auto fallback derives an initial step from the first tangent and
  `loadSteps`; for soft-direction structures (shallow arches, thin shells) this can
  jump over the entire snap region in a single step. **Set `arcLength` manually to
  1 %–5 % of the characteristic rise or expected mid-span deflection** (e.g. `0.03` for
  a `rise=1.0` shallow arch). The auto fallback is a coarse starting point only; the
  corrector may still report `converged=true` on a post-snap equilibrium without ever
  flagging the missed limit load. Arc-length also assumes a non-zero reference load
  vector — `arcLengthSolve` rejects an empty `Fext_f` with a clear diagnostic
  (Crisfield's cylindrical constraint is geometrically meaningless at zero load).
- **Curved shells need mesh refinement** — the engine now has an *opt-in guard*. The MITC4
  facet is flat, so a curved shell's membrane / bending forces converge as O(1/N²). Honest
  numbers: an internally pressurised cylinder with N=8 sides around the circumference
  under-predicts the hoop membrane force by **7.6 %**; N=16 gives 1.9 %, N=32 gives 0.48 %,
  N=64 gives 0.12 %. **Use at least 16 facets per 90° of curvature** for a 2 % engineering
  tolerance. Set `SolveOptions::shellCurvatureMaxAngleDeg = 22.5` (16-per-90°) to have
  `assembleAndFactor` *refuse* a too-coarse mesh up front with a diagnostic naming the
  worst-pair shells — R2.1 AC-07 fix (F64b in the standalone gate). Default 0 keeps v2.0
  behaviour bit-identical.
- **Linear-buckling eigenvalues over-predict real buckling loads for thin shells** — the
  engine now produces a *design-grade* number when asked. `BucklingAnalysis` returns both
  the raw eigenvalue (`reportedCriticalFactor`) and a knocked-down design value
  (`criticalFactor = alpha · raw`); `knockdownFactor` records the alpha. Set
  `BucklingOptions::shellBucklingKnockdown` to the relevant code-style alpha (e.g. 0.65
  per NASA SP-8007 for axially compressed cylinders, 0.7 for general fabrication) and the
  result is immediately usable for code checks (R2.1 AC-06 fix, F64a). Default 0 keeps
  the raw eigenvalue (bit-identical to v2.0). The linear analysis still ignores imperfections,
  post-buckling softening, and large-displacement coupling — for a definitive analysis use
  the shell co-rotational arc-length path (later phase).

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
METIS — the standalone gate now links the opt-in supernodal lane, and `build.bat` exits 1 without it.
If conda is installed off `%USERPROFILE%\anaconda3`, set `SUPERNODAL_CONDA=<conda-root>\envs\framecore-direct\Library`
before running so `build.bat` picks up the right OpenBLAS+METIS install.)

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
> The `$ExpectedUeTests = 70` guard catches a silently-missing test (v3.2.1 bumped
> 62→70 with the 8 `FrameCore.UE.*` Phase 6 a-h tests; v3.2.0 bumped 60→62 with
> `BlueprintSmokeTest` + `EditorSmokeTest`; v3.1.0 bumped 59→60 with
> `FFrameCoreStressFieldTest`; v2.11.1-RC bumped 58→59 with
> `FFrameCoreGpuBacksubStrictTest`; v2.11 Phase 7 bumped 57→58 for
> `FFrameCoreGpuBacksubTest`, the UE mirror of standalone F67). On a box without cuDSS
> the two GPU tests compile out via `#if FRAMECORE_CUDA` — pass `-ExpectedUeTests 68`.

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
    Private/Tests/*.cpp                 60 UE automation tests (FrameCore.*, UE-side oracle mirrors)
  Source/FrameCoreUE/                   v3.2.0+ consumer-side BP/USTRUCT reflection module
    Private/Tests/*.cpp                 10 UE automation tests (FrameCore.UE.*, v3.2.1 Phase 6 a-h)
                                        Total UE gate count: 70 w/ cuDSS, 68 without
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

**FrameCore is released under the MIT License** — see [`LICENSE`](LICENSE) for the
full text. Graduation-project code, open to reuse and redistribution.

FrameCore's default solver depends only on **Eigen** (MPL-2.0, header-only, with
`EIGEN_MPL2_ONLY` so the LGPL modules are excluded by the preprocessor).
The opt-in supernodal lane (`SnSolver` / `SnSession` / `solveLoadSupernodal`,
gated by `FRAMECORE_SUPERNODAL=1`) additionally links **OpenBLAS** (BSD-3-Clause)
and **METIS** (Apache-2.0) via the conda `framecore-direct` env; the full
third-party attribution text required by their licenses is collected in
[`third_party/NOTICE.md`](third_party/NOTICE.md) — include it alongside any
redistribution that ships the supernodal binaries.

**OpenSees** is used for offline validation only (Python tooling under `Tools/`)
and is **not** redistributed or linked into the engine.
