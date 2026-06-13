# FrameCore — Verification Map

This document traces **every capability claim to its evidence**: the independent oracle(s)
that check it, the gate fixture that re-runs them on every commit, the *measured* agreement,
and where its honest limitations are written down. It is the companion to the
[README](../README.md) capability overview and [`ARCHITECTURE.md`](ARCHITECTURE.md).

Reproduce everything with:

```powershell
# full five-leg gate (standalone + UE automation + OpenSees + deep audit + CLI round-trip)
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
```

```bat
:: fastest single leg (seconds): standalone analytic/benchmark fixtures
Plugins\FrameSolver\Standalone\build.bat
```

---

## 1. The five-leg gate

| Leg | What runs | Count | What it proves |
|---|---|---|---|
| 1. Standalone | `frametest.exe` (fixtures **F1–F54**, built UE-free) | `ALL PASS (failures=0)` | every capability against analytic / literature / invariance oracles, on the pure-C++ build |
| 2. UE automation | headless `FrameCore.*` tests | **50** tests | the *same* fixtures compiled as a UE module — the dual-build contract holds |
| 3. OpenSees | `Tools/opensees_compare.py` + `pdelta_compare.py` | strict `1e-8` default | agreement with an independent, widely-used FEM code (validation only; never linked) |
| 4. Deep audit | `linear_deep_audit.exe` | **104** checks | independent re-derivations (sympy/numpy-sourced constants), bit-identity no-op proofs, element-spectrum oracles |
| 5. CLI round-trip | `Tools/cli_roundtrip.py` | 11 checks | the text/daemon/C-API bridge reproduces engine results bit-for-bit |

Guard rails: `run_gate.ps1` hard-fails if fewer than `$ExpectedUeTests = 50` UE tests run
(catches "new test silently not compiled"); the audit prints its own check count rather than
a hard-coded number; `-RequireOpenSees` turns a missing OpenSeesPy into a failure instead of
a soft skip. Fixture numbering is append-only; **F41 is unassigned** (S3 ended at F40, S4
started at F42) — a numbering gap, not a missing test.

## 2. Oracle taxonomy

Every fixture is anchored to at least one oracle that is *independent of the code under test*:

- **Analytic closed form** — textbook solutions (`PL³/3EI`, `5wL⁴/384EI`, `θ=TL/GJ`, Euler
  buckling, Kirchhoff plate coefficients, plastic collapse `w*=16Mp/L²`, …).
- **Literature benchmark** — published reference values (MacNeal–Harder Scordelis-Lo roof and
  pinched cylinder, Cook's membrane, the 10-bar truss `1593.2 lb` optimum, Mattiasson's
  elastica tables — independently re-derived with scipy shooting integration).
- **OpenSees cross-validation** — the same model solved by OpenSees (§4).
- **Independent recompute** — a second, structurally different path inside the gate: an
  independent dense Gaussian solver (not Eigen), fresh-factorization references for every
  incremental method, sympy/numpy re-derivations in the deep audit.
- **Bit-identity (no-op proof)** — every opt-in feature must leave the default path
  *bit-for-bit* unchanged (QM6/DKQ off, `nmInteraction` off, P=0 P-Delta, factorize-once vs
  monolithic solve). This is how new capability is proven not to perturb verified behaviour.
- **Invariance** — rotate the whole model by an arbitrary rotation; displacements must
  transform with it (catches transform errors a norm-only check cannot see).

Where a "measured" number is quoted below it is the *observed* agreement; gate **tolerances
are deliberately looser** (e.g. shell `1e-7`, modal `1e-4`) to leave float/library headroom —
the gap between measured and gated is stated, not hidden.

## 3. Capability → evidence map

### 3.1 Linear core (F1–F16)

| Fixture | Capability | Oracle → measured |
|---|---|---|
| F1, F2, F4 | beam statics (cantilever, SS-UDL, axial column) | analytic `PL³/3EI`, `5wL⁴/384EI`, `wL²/8` |
| F3, F7 | mechanism / ill-conditioning detection | negative test: LDLᵀ pivot screen must flag SINGULAR |
| F5, F9 | torsion + circular section, biaxial D/C | analytic `T·r/J`, `I=πr⁴/4`, resultant bending |
| F6 | end releases (static condensation incl. fixed-end forces) | analytic propped cantilever `5wL/8, 3wL/8, wL²/8` |
| F8 | Timoshenko shear flexibility | analytic `PL³/3EI + PL/GAₛ`; reduces to E-B for slender |
| F10 | piecewise/refined meshing convergence | analytic (quarter-circle arch convergence) |
| F11 | **rotation equivariance** | rigid rotation of model+loads → `u' = R·u` |
| F12 | grillage plate idealization | Kirchhoff plate theory, centre deflection ~2 % (moments over-estimated — documented) |
| F13 | MITC4 plate bending | Kirchhoff thin plate ~0.1–0.3 % (Mindlin model gap, non-monotone — documented); OpenSees `ShellMITC4` ~1e-10 |
| F14, F14a | membrane + drilling + 3-D facet rotation; patch tests | constant strain/curvature to machine precision (regular + parallelogram meshes) |
| F15 | Scordelis-Lo roof | MacNeal–Harder reference: 0.83 % at N=24 |
| F16 | pinched cylinder | 98.8 % of the `1.8248e-5` reference at N=32, converging from below |

### 3.2 Linear analysis suite (F17–F25, F34)

| Fixture | Capability | Oracle → measured |
|---|---|---|
| F17 | load combination primitive | superposition identity `u(A)+u(B) == u(A+B)` |
| F18 | self-weight (`ρ` unit bridge) | analytic `wL²/8` — only passes if the ×1e-12 tonne/mm³ bridge is right |
| F19 | factorize-once `PreparedSystem` + prescribed settlement | **bit-identity** with the monolithic solve; settlement matches OpenSees `sp()` to 0 |
| F20 | pattern loading + envelopes | component-wise max/min vs enumerated cases |
| F21 | influence lines / moving loads | Müller-Breslau cross-check |
| F22 | modal analysis | analytic `ωₙ=(nπ/L)²√(EI/ρA)`; OpenSees `eigen -cMass` ~1e-11 |
| F23 | linear buckling | Euler column loads |
| F24 | response spectrum (SRSS/CQC) | hand-derived modal combination |
| F25 | modal-superposition transient (Newmark-β) | analytic SDOF response |
| F34 | **opt-in sparse buckling** (subspace iteration) | sparse == dense to 2e-14–1e-12; vs Euler 4.1e-7–1.35e-5 (finest mesh best) |

### 3.3 Progressive-collapse line (F26–F33, F37–F39; F54 = S10)

F54 (the S10 N–M interaction hinge) lives here because it extends the hinge subsystem of the
collapse driver rather than the reanalysis line.

| Fixture | Capability | Oracle → measured |
|---|---|---|
| F26, F28 | element removal (member / shell `active`) | redistribution vs analytic + mechanism oracle; OpenSees omission model: 2.2e-13 (beam), 2.2e-10 (shell facet) |
| F27 | safety factor + `pivotMargin` | C3/C4 margins vs hand-computed D/C; margin crosses `pivotTol` exactly at the mechanism |
| F29 | debris connectivity + `FragmentCluster` | closed-form mass / com / inertia of detached fragments |
| F30 | LSP progressive-collapse driver | scripted removal sequence, dual terminal Stable/Collapsed, deterministic tie-breaks |
| F31 | shell von Mises failure screen | surface σ = N/t ± 6M/t² vs hand values |
| F32 | plastic hinge mechanics (solver layer) | formed-hinge state vs an *independent OpenSees formulation* ~1e-12 |
| F33 | event-to-event hinge driver | classic plastic collapse load `w* = 16Mp/L²` to ±2 % |
| F37–F39 | **continuous dynamic collapse (S2)** | full-basis Ritz == pure modes bit-for-bit; cross-event inheritance vs analytic static `PL/EA` 2.26e-15; audit cross-event vs full-system Newmark 5.55e-13; translational momentum closure exact (p=mv); angular momentum from a **rod-model** approximation (FE cross-section polar inertia omitted — accurate for slender members); x-z symmetry `vel_y` ~1e-13 |
| F54 | **N–M interaction hinges (S10)** | `Mp_eff(N)` formula rel 0 (tol 1e-15); rectangle == first-principles neutral-axis shift rel 0; driver bracket: 0.99 w* Stable / 1.01 w* Collapsed; audit: `off` == stage-4b **bit-identical**; same load `on`=Collapsed vs `off`=Stable — the interaction decides the outcome |

### 3.4 Reanalysis & nonlinear line (F35–F36, F40, F42–F43, F50–F53)

| Fixture | Capability | Oracle → measured |
|---|---|---|
| F35 | **ReSolve ladder Tier-1 (Woodbury) + Tier-3 rebaseline (S1)** | vs fresh factor+solve: 2.43e-12 (member), 1.70e-12 (shell facet); restore drift 1.3e-16; audit 4e-14 |
| F36 | ReSolve Tier-2 (stale-LDLᵀ PCG) | vs fresh: 5.3e-15 in 3 PCG iterations; audit 7e-16 |
| F40 | **P-Delta second order (S3)** | frozen pseudo-load == K_T-refactor reference: 1.62e-13 (P/Pcr=0.3), 1.45e-12 (0.95); reference vs analytic beam-column: 2.64e-7 / 3.72e-5; P=0 **bit-identical** to linear, 0 iterations; P=1.05 Pcr → both paths report `diverged` |
| F42–F43 | **tension-only members (S4)** | converged state == omitting slack members (fixture rel<1e-10; audit 1.135e-15); monotone fallback terminates in ≤ nTO+1 iterations; `tensionOnly` in the reuse fingerprint |
| F50 | **planar co-rotational (S9)** | elastica vs Mattiasson shooting tables: N16 1.1e-4–6.5e-4; planar rigid-rotation invariance 0.0; small-displacement degenerate == `runPDelta` 5.0e-3 |
| F51 | **3-D general co-rotational (S9b)** | arbitrary-axis rigid rotation (φ=2 rad) 4.10e-16; spatial elastica == planar values; pure torsion `θ=TL/GJ` 2.60e-16 (1.41e-15 at 162°); biaxial decoupling ~1e-9; **OpenSees `geomTransf Corotational`: 1.22e-9** |
| F52 | **arc-length snap-through (S9c)** | shallow-arch limit load 0.00585833 vs OpenSees `ArcLength` 0.0058962 (rel 6.4e-3, tol 3e-2); load control past the limit point correctly diverges |
| F53 | CR member UDL / prescribed displacement / consistent tangent (S9c) | UDL tip `wL⁴/8EI` 1.25e-8; prescribed reaction `3EIδ/L³` 1.02e-8; prescribed rotation `EIθ/L` 2.65e-16; FD consistent tangent reproduces the main-term limit load to 4.44e-16 |

### 3.5 Optimization (F44–F47)

| Fixture | Capability | Oracle → measured |
|---|---|---|
| F44 | **FSD sizing (S5)** | 10-bar truss: 1608.49 lb vs the pin-jointed literature optimum 1593.2 lb (rel 9.6e-3 — *heavier* because the engine carries real joint bending; documented, not hidden); sized members `|D/C−1| < 1e-6`; audit one-step `A = P/σ_a` rel 1.14e-16 |
| F45 | BESO sensitivity = element strain energy | energy balance `Σα = ½F·u` 4.13e-14; pure axial/bending/torsion components at machine precision |
| F46 | **BESO topology optimization (S7)** | ground structure 81→40 members at volume target 0.503; best topology re-solves non-singular; compliance rises 1.49× (sanity direction) |
| F47 | N2 collapse-robustness constraint | unconstrained result dies to a single removal; constrained result survives *every* single-member removal and locks the protective members |

### 3.6 Shell upgrades (F48–F49, S8 — both opt-in, default off)

| Fixture | Capability | Oracle → measured |
|---|---|---|
| F48 | QM6 incompatible membrane | distorted membrane patch 2.2e-16; Cook's membrane N16: Q4 −3.2 % → QM6 −0.9 %; slender in-plane cantilever: Q4 locks at −75.5 % vs QM6 −0.9 %; **default off == baseline bit-for-bit** |
| F49 | DKQ thin plate | constant-curvature patch to machine precision; SS thin plate 0.05 % vs Kirchhoff `0.00406qa⁴/D` (no Mindlin overshoot), clamped 1.52 %; **OpenSees `ShellDKGQ`: 1.69e-12**; default off == baseline bit-for-bit |

### 3.7 Ecosystem bridge (S6 — gate leg 5)

| Check | Oracle → measured |
|---|---|
| baseline `DISP` | cantilever tip vs `PL³/3EI`: 3.13e-13 |
| `TONLY` / `SIZEOPT` / `DYNC` | reproduce engine results through the text bridge (SIZEOPT reproduces the F44 1608.49 lb) |
| `COROT` / `ARCL` | planar elastica `dv/L = 0.714138`; shallow-arch λ_peak 0.00586 through the bridge |
| daemon mode (J1.5) | multi-block single process == independent CLI runs, **bit-identical** |
| C API DLL (J2) | `frame_capi_solve_text` == `frame_cli.exe`, **bit-identical** (ctypes harness) |

## 4. OpenSees cross-validation summary

OpenSees is the external reference (validation only — never shipped or linked). Measured
agreements, tightest first:

| Comparison | Measured | Note |
|---|---|---|
| prescribed settlement vs `sp()` | 0 | exact |
| beam statics suite | strict `1e-8` gate default | |
| DKQ plate vs `ShellDKGQ` | 1.69e-12 | same-element class |
| formed plastic-hinge state | ~1e-12 | vs an independent OpenSees formulation |
| modal frequencies vs `eigen -cMass` | ~1e-11 | |
| MITC4 shell vs OpenSees' own `ShellMITC4` | ~1e-10 flat/tilted; ~1e-7–1e-8 skewed+warped | gate tol 1e-7 (headroom on purpose) |
| 3-D co-rotational vs `geomTransf Corotational` | 1.22e-9 | gate tol 1e-6 |
| element removal (collapse state) | 2.2e-13 beam / 2.2e-10 shell | removal-by-flag vs omission model |
| arc-length limit load vs `integrator ArcLength` | 6.4e-3 | path-following; gate tol 3e-2 |
| P-Delta vs `PDelta` geomTransf | 1.37e-3 | **model gap, not error**: OpenSees' `PDelta` transform ignores internal P-δ; our consistent geometric stiffness sits at 2.64e-7 from the *analytic* beam-column solution, which is the tight oracle |

## 5. Claim discipline

- Statements in docs/specs are tagged **`[VERIFIED]`** (oracle-backed, reproducible),
  **`[NEW CODE]`** (no prototype backing — named, and assigned an oracle before merge),
  **`[THEORY]`** (derivation only), **`[UNKNOWN]`** (stated as such). Textbook methods are
  never claimed as novel; novelty statements carry prior-art positioning
  (`docs/research/WS_N_priorart.md`).
- Every stage S1–S10 went through an **adversarial review pass** (independent sympy / numpy /
  scipy re-derivation of the mathematics; e.g. the Mattiasson elastica table was re-derived
  with an independent DOP853 shooting integration to 6e-11 before being trusted as an oracle).
  Per-stage records: `docs/PROGRESS_S*.md`, each with its own honest-limitations section.
- Known scope boundaries (what the engine deliberately does **not** do) are kept in one
  place: the README *scope boundaries* section. Highlights: the D/C check is an elastic
  screen, the collapse driver is LSP-grade sequential linear analysis (±30 % literature
  envelope), hinges are event-to-event with no unloading, and there are no fiber sections /
  pushover by design.
