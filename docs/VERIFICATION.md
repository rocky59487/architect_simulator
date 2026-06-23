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
| 1. Standalone | `frametest.exe` (fixtures **F1..F71** default / **F1..F71 + F67s** in CUDA build, built UE-free) | `ALL PASS (failures=0)` | every capability against analytic / literature / invariance oracles, on the pure-C++ build. F67 is a smoke fixture (tolerates silent CPU fallback for dev-box compile tests); F67s is strict (FAILS on silent fallback) and runs only when `FRAMECORE_GPU_STRICT=1` is set in the env — `Scripts\run_gpu_gate.ps1` sets it automatically when the cuDSS DLL resolves. v3.1.0 added **F68/F69/F70** (S11 stress-field oracle ladder); v3.3.0 added **F71** (U-07 sentinel edge case: -1 sentinel on empty / all-inactive / id-0-governing models). |
| 2. UE automation | headless `FrameCore.*` tests | **135** tests w/ cuDSS, **133** without | UE-side mirrors of the standalone oracles + the v3.5 visual surface + the v3.6 along-span data line (C6 InternalForceField 4 tests / C7 UtilField 3 / C8 Redundancy 2) + Hermite 3 / LoadPatch 2 / InfluencePolarity 1. v3.5.0 added 22 `FrameCore.UE.*` tests across the **visual + game-ready surface**: **Phase 1 (3)** DeformedShape.CantileverTip / EmptyField / SectionCount; **Phase 2 (3)** Heatmap.CantileverDC / Unstressed / VertexCount; **Phase 3 (2)** ModalShape.FirstMode / ModeSwitch; **Phase 4 (3)** DynCollapseReplay.PlaybackEndToEnd / InterpolationMidframe / EventDelegate; **Phase 5 (3)** FragmentCluster.SpawnCount / CentroidPosition / ClearAndRespawn (Chaos POD thin slice -- physics-enabled StaticMesh debris, full Chaos GeometryCollection in v3.6); **Phase 6 (1)** InfluenceLine.SSBeamMidspanMoment; **Phase 7 (3)** InteractiveSubsystem.StartEndLifetime / PatchSemantics / PerfBaseline; **Phase 8 (4)** ResponseSpectrum.EnvelopePulse / Static + RealTimeDynamic.Playback / Midstep. None are `#if FRAMECORE_CUDA` gated, so a non-cuDSS build emits 118 (v3.4 baseline 96 + 22 new = 118; the 2 GPU-gated tests stay compiled out). v3.4.0 added 26 (Phase 1-5 input/output USTRUCT + analysis libraries). v3.3.0 added 2 (`FFrameCoreUEActorStressMeshTest` U-03 + `FFrameCoreUEMarshalJsonTest` U-01). v3.2.1 added 8 `FrameCoreUE.*` Phase 6 a-h. v3.2.0 bumped 60→62. v3.1.0 bumped 59→60 with `FFrameCoreStressFieldTest`. v2.11.1-RC bumped 58→59 with `FFrameCoreGpuBacksubStrictTest`. v2.11 Phase 7 bumped 57→58 for `FFrameCoreGpuBacksubTest`. Both GPU tests are `#if FRAMECORE_CUDA` gated. |
| 3. OpenSees | `Tools/opensees_compare.py` + `pdelta_compare.py` | strict `1e-8` default | agreement with an independent, widely-used FEM code (validation only; never linked) |
| 4. Deep audit | `linear_deep_audit.exe` | **104** checks | independent re-derivations (sympy/numpy-sourced constants), bit-identity no-op proofs, element-spectrum oracles |
| 5. CLI round-trip | `Tools/cli_roundtrip.py` | **13** checks | the text/daemon/C-API bridge reproduces engine results bit-for-bit and surfaces modal/dynamic precondition failures |

Guard rails: `run_gate.ps1` hard-fails if fewer than `$ExpectedUeTests = 120` UE tests run (v3.5.0 bump for the 22 new `FrameCore.UE.*` Phase 1-8 visual-surface tests stacked on top of the v3.4 98-test baseline; 118 on non-cuDSS builds — pass `-ExpectedUeTests 118`). **v3.5.1 update: the 5-leg gate (Legs 1-5 + Leg 6 v2 round-trip) was actually run end-to-end on the integrator host; the v3.5.0 "Projected" disclosure no longer applies. See [`docs/RELEASE_v3.5.1.md`](RELEASE_v3.5.1.md).**
(catches "new test silently not compiled"); the audit prints its own check count rather than
a hard-coded number; `-RequireOpenSees` turns a missing OpenSeesPy into a failure instead of
a soft skip. Fixture numbering is append-only; **F41 and F60 are unassigned** (F41: S3 ended
at F40, S4 started at F42; F60: numbering jump between F59 — `S9c`'s consistent-tangent FD
check — and F61 — v3 warped quads — when the v3 surface line was bundled in) — both are
numbering gaps, not missing tests.

**v2.5 supplementary — manual 6th leg (v2 transport line)**. The Rhino bridge v2 dispatcher
(`frame_capi_v2.dll`, B3 engine-wired in v2.5) has its own smoke-test harness
`Tools/v2_roundtrip.py` (**ALL PASS, 0 SKIP / 0 FAIL** in v2.5; the v2.4 SKIP
`solve.linear bit-exact vs v1` became a PASS in commit `180c9e8`, rel<1e-11 vs
`frame_capi.dll` v1 on the cantilever fixture; additional checks cover B3-wired
analyses and the `transport.sync` capability advertised in v2.4 review-round commit
`b3cea8b`). It is **NOT** wired into `run_gate.ps1` for v2.5 because (i) it depends on
the build step `build_capi_v2.bat` which is itself outside the 5-leg gate's scope,
(ii) the v2 wire protocol is a separate transport line and rebuilding it on every
five-leg run gates the FrameCore engine on a downstream artefact that does not gate it
back. Run manually:
```bat
Plugins\FrameSolver\Standalone\build_capi_v2.bat
python Tools\v2_roundtrip.py
```
Inclusion in `run_gate.ps1` is re-evaluated when B4 (streaming dispatcher) lands; the
v2.5 contract is "if you touched the dispatcher, you ran this manually before push".

**Post-v2.5 update (2026-06-20, refreshed in v2.8.1 audit E-07).**
`Tools/v2_roundtrip.py` now also covers the post-tag transport/dispatcher hardening:
`transport.async` advertised with `transport.sync` absent, native `solve.linear wantDC`
utilization output, duplicate-id rejection, `analysis.reanalysis_solve` response shape,
`solve.dyn_collapse` final summary shape, the C-09 / C-10 supernodal-session modal /
buckling reject path, and (v2.7) `dyn_collapse.live` mid-run frame streaming. The full
state-of-the-dispatcher narrative lives in [`RELEASE_v2.6.md`](RELEASE_v2.6.md) (B4 async +
GH bridge + C-09/C-10), [`RELEASE_v2.7.md`](RELEASE_v2.7.md) (live frames + cancel), and
[`RELEASE_v2.8.1.md`](RELEASE_v2.8.1.md) (audit-driven hardening). The previously cited
`docs/POST_V2_5_HARDENING.md` was never authored; the v2.8.1 audit pass replaces both
dead-link sites with the canonical release trail.

## 1.6. v3.1.0 — S11 stress field post-process

**v3.1.0 shipped 2026-06-21 with the 6 CPU-only legs all green on the integrator host.**
Engine source delta v3.0.1..v3.1.0 = 5 files / ~350 lines; `ElasticAllowable.cpp` refactored
to call `StressKernel.h` shared math (F1..F66 bit-identical). New oracle ladder:

| Fixture | Capability | Oracle → measured |
|---|---|---|
| **F68** | StressField member stress field (cantilever oracle) | analytic `sigma(x) = \|P\|·(L-x)/Wz` at 11 samples worstRel=0; bit-exact vs `ElasticAllowable(endI/endJ)` rel=0 (shared `StressKernel`); tip sigma vanishes; root TopY/BotY \|sigma\| == sComp |
| **F69** | StressField shell layer recovery + rotation invariance | `(top+bot)/2 == Nxx/t` worstRel=0; `(top-bot)/2 == 6·Mxx/t²` worstRel=0; 30° z-rotation `globalMaxVonMises` invariance rel=0 |
| **F70** | StressField ↔ ElasticAllowable D/C interlock | `governingMemberId` match (fld=0 ea=0); `governingShellId` match (fld=59 ea=59); max fiber sigma == max ElasticAllowable end sigma rel=0; `globalMaxVonMises` == `ds.maxDC * cap.vm` rel=0 |
| **UE FFrameCoreStressFieldTest** | UE-build mirror of F68 (member side) | interlock + analytic, same tolerances as standalone F68 |

`inspect.stress_field` v2 dispatcher capability covered by `Tools/v2_roundtrip.py` (11 new
shape + range-guard checks); the gate's `=== summary: ALL PASS ===` confirms wire-level
schema integrity (samplesPerSpan=11, members list, governing-element sentinels, and
range guards reject `samplesPerSpan=1` / `=2048`).

## 1.5. v3.0.0 STABLE gate suites (and the v3.0.1 patch hardening on top)

**v3.0.0 STABLE shipped 2026-06-21 with all 9 legs green in one integrator session,
and v3.0.1 raised the bar with strict-execution fingerprints + perf regression
threshold + CI workflow.** The release-time matrix (kept here as historical
evidence — see [`docs/RELEASE_v3.0.0.md`](RELEASE_v3.0.0.md) for the v3.0.0 narrative
and [`docs/RELEASE_v3.0.1.md`](RELEASE_v3.0.1.md) for v3.0.1):

On the integrator's host, all 9 legs ran green against the v3.0.0 source
(standalone F1..F66 + audit 104 + CLI
13 + v2_roundtrip CPU + standalone F1..F67 + F67s CUDA strict + v2_roundtrip CUDA +
r2_bench --gpu 90k margin +11.946 ms + UE 59/59 + OpenSees strict). v3.0.1 added
post-v3.0.0 hardening (kEngineVer 2.11.1→3.0.1 sync, strict-test STRICT_EXECUTED
fingerprint enforcement, r2_bench regression threshold, UE Build.cs normalisation
matching the bat resolver, CI workflow). The same three gate suites stay green:

| Gate | Command | What it covers |
|---|---|---|
| **a.** 5-leg | `Scripts\run_gate.ps1 -RequireOpenSees` | standalone + UE (default 98 with cuDSS — pass `-ExpectedUeTests 96` without) + OpenSees + deep audit + CLI |
| **b.** v2 CPU | `build_capi_v2.bat` + `python Tools\v2_roundtrip.py` | dispatcher round-trip without GPU |
| **c.** GPU 6th | `Scripts\run_gpu_gate.ps1 -Strict` (use `-CondaEnv <path>` to override the auto-probe) | frametest_cuda F1..F71 default + F67 smoke + F67s strict + v2_roundtrip CUDA + r2_bench --gpu 90k ≤ 16.67 ms. Requires cuDSS DLLs on PATH for `-Strict`; soft-skips otherwise. |

The `-Strict` flag on (c) is the key add: when `FRAMECORE_GPU_STRICT=1` reaches
the test binaries, **silent CPU fallback FAILS** instead of green-washing as a
PASS. Strict is auto-armed by `run_gpu_gate.ps1` whenever the cuDSS runtime DLL
resolves on the box; passing `-Strict` to the script itself fails hard if any of
the env / DLL prerequisites are missing.

The honesty layer is the NOT-RUN list in
[`docs/HANDOFF_v2.11.1.md` §2](HANDOFF_v2.11.1.md#2-gates-what-ran-what-didnt) —
the V3 STABLE flip is the moment that table reads all green on the same line of
a single owner session. RC has narrowed it to two rows (UE + OpenSees).

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
| `WARP` token (v2.3) | `WARP <warpTolerance> [<useWarpingCorrection>]` exposes the v3 warped-quad opt-in (audit fix for mega benchmark). Omitting it leaves `SolveOptions::warpTolerance=1e-6` (strict, v2.2+1 bit-identical). Forward-compatible — older clients keep working |

### 3.7a OpenSees mega benchmark (v2.3 — 128 cases)

A new external-oracle suite under `benchmarks/opensees_mega/` runs FrameCore via the
text bridge against OpenSees on 128 case × load × mesh combinations:
- A1–A8 building frames (beam-column + faceted shell)
- B1–B4 bridge segments
- C1 quarter dome / C2 hyperbolic paraboloid / C3 pinched cylinder / C4 Scordelis-Lo /
  C5 sinusoidal freeform faceted shell, each at 8×8 / 16×16 / 24×24 mesh × L1/L2/L3/L4 loads
- D1 P-Delta column / D2 release surrogate / D3 modal SDOF / D4 ground-spring beam
- L6 modal frequencies

Run `20260619-001` (v2.3, post-WARP fix): **0 CRITICAL · 0 MAJOR · 64 MINOR (within tolerance) · 64 KNOWN (CLI-coverage / faceted-vs-smooth oracle gap)**.
Per-family worst displacement rel: A_building 3.59e-11, B_bridge 3.86e-12,
C_shell 1.60e-4 (was 1.00 pre-fix), D_special 1.55e-3, L6_modal 0.

Reproduction: `powershell -ExecutionPolicy Bypass -File benchmarks\opensees_mega\rerun.ps1`
(needs `openseespy`; writes a fresh `results/<run-id>/` with `matrix.csv`, `findings.json`,
`report.md`, and per-case convergence plots).

### 3.8 Supernodal direct lane (F55–F56, R-line — opt-in, default LDLᵀ)

A self-built BLAS3 supernodal Cholesky (METIS ordering + OpenBLAS dense panels) offered as an
explicit opt-in solver lane; `SimplicialLDLT` stays the default **and** the fallback. Its oracle
is the LDLᵀ solve itself — the supernodal factor must reproduce the direct solution and stay a
bit-exact drop-in when disabled. Full write-up: [`PROGRESS_R_supernodal.md`](PROGRESS_R_supernodal.md).

| Fixture | Capability | Oracle → measured |
|---|---|---|
| F55 | stateless `solveLoadSupernodal` (frame / SS-UDL / settlement / release / MITC4 shell) | vs LDLᵀ rel < 1e-10; disabled == LDLᵀ bit-exact (rel < 1e-12); mechanism → LDLᵀ pivot-guard fallback (singular, not NaN) |
| F56 | `SnSession` factor-once + solve-many (reused supernodal factor) | each reused-factor frame vs LDLᵀ rel < 1e-10; disabled session == LDLᵀ drop-in |

Correctness is gated against LDLᵀ (and, in research, CHOLMOD) rather than a residual: on
high-condition mixed shell/frame systems the fixed-precision residual floors at ~cond·eps, so a
`vsLDLᵀ` relative check is the honest oracle. The lane needs a conda `framecore-direct` env
(OpenBLAS + METIS) at build time — the standalone gate leg now depends on it.

### 3.9 v3 surface line + v2.1 architectural fixes (F57–F64)

The v3 surface line and the v2.1 audit responses are all opt-in: every flag defaults to off and
the new fixtures lock in bit-identical v2.0 behaviour when the flag is disabled, so the LDLᵀ-only
default lane remains untouched. Full write-ups:
[`docs/specs/shell_geometric_stiffness.md`](specs/shell_geometric_stiffness.md),
[`docs/specs/shell_corotational.md`](specs/shell_corotational.md),
[`docs/specs/shell_warping.md`](specs/shell_warping.md),
[`docs/specs/R2_neumaier_ir.md`](specs/R2_neumaier_ir.md),
[`docs/specs/v3_memory_recon.md`](specs/v3_memory_recon.md),
[`docs/PROGRESS_R2.md`](PROGRESS_R2.md),
[`docs/PROGRESS_V21.md`](PROGRESS_V21.md).

| Fixture | Capability | Oracle → measured |
|---|---|---|
| F57 | shell K_σ stress stiffening (opt-in `shellGeometricStiffness`) → SS plate linear buckling | analytic `N_cr = 4π²D/a²` (Timoshenko); MITC4 facet O(1/N²) — rel < 3 % at n=20; axis invariance 1e-9; sparse==dense 1e-6; opt-in OFF → singular (no Kg source) |
| F58 | EICR shell co-rotational (opt-in `shellCorotational`) — small-disp == linear + arbitrary-axis rotation invariance | F58a small-displacement rel 3.8e-11 (CR == linear); F58b SO(3) arbitrary-axis invariance rel 2.6e-14 (w/L=0.69 large rotation) |
| F59 | EICR shell large-deflection strip vs Mattiasson elastica (ν=0 plate strip == beam EI) | Mattiasson table (α=1/5/10) — strip dv/L 1.32e-4..7e-4 rel, dh/L 7e-4..2e-3 rel (gate 3e-3/5e-3) |
| F61 | warped MITC4 quad (`warpTolerance` + `useWarpingCorrection`, v3 phase B) | warp 4%→2%→1% → Nxx err 1.6e-3 → 4e-4 → 1e-4 (O(warp²) refinement); flat patch bit-identical (eOff/eOn < 1e-10) |
| F62 | R2 Neumaier-compensated iterative refinement on the supernodal lane (`irSteps` / `irTol`) | slender cantilever L/d=200 (~cond 1e6): IR=0 res 6.12e-8 → IR=2 res 5.03e-8 (ratio 0.82, polish); IR solution rel 1.15e-9 (small correction, not redirect); irSteps=0 bit-identical to no-IR |
| F63 | PERF-01 supernodal-primary factor (`useSupernodalPrimary` — supernodal REPLACES LDLᵀ when SPD) | cantilever / SS+UDL / shell plate u-rel < 1e-9 vs LDLᵀ; pivotMargin from L diagonal (1.875e-7 > pivotTol 1e-12); mechanism (no supports) → singular; SnSession on SnPrimary reuses factor (no double-build, rel<1e-12); solveModal/solveBuckling refuse SnPrimary ps with clear diagnostic |
| F64 | AC-06 shell buckling knockdown (`shellBucklingKnockdown`) + AC-07 curved-shell mesh guard (`shellCurvatureMaxAngleDeg`) | F64a alpha=0.65 → design = alpha·raw (rel 1e-12), `reportedCriticalFactor` stable across alphas; F64a-shell SS plate alpha=0.65 also rel 9.2e-3 vs F57 oracle; F64b 22.5° guard rejects 8-facet cylinder (45°/facet), admits 32-facet (11.25°/facet); inactive shells skipped (progressive-collapse false-rejection guard) |

UE mirrors: `FrameCore.Buckling.ShellGeometricStiffness` / `FrameCore.Buckling.ShellKnockdown`
/ `FrameCore.Validation.ShellCurvatureGuard` cover F57 / F64-knockdown / F64-curvature on the UE
side, contributing 3 of the 70-test gate count (v3.2.1).

Honest scope:
- F62 standalone improvement is modest (~18 % at L/d=200, cond ~1e6 — close to the machine-precision
  floor); the IR mechanism's larger payoff appears at 64 k DOF mixed building where the recon's
  `sn_sweep.txt` shows res ~1.40e-9 → expected < 1e-9 (production-scale benchmark pending).
- F63 single-solve win at scale is data-backed from `perf_sn.exe` `[VERIFIED: perf_sn first-hand]`
  (XXL 18.7 k → 8.3 × factor speedup; MEGA 61.5 k → 20 × factor speedup) but the gate fixtures
  themselves are small and only assert correctness; the speedup ratios are not re-checked on
  every commit.
- F64 shell-buckling oracle is the same Kirchhoff plate as F57; curved-shell post-buckling is the
  shell co-rotational arc-length phase (later).

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
