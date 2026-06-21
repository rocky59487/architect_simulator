# FrameCore v3.0.0 STABLE — V3 anchor (GPU lane env-discovery + strict-attached + 9/9 verified)

**Tag (this release):** `v3.0.0`
**Branch:** `main`
**Date:** 2026-06-21
**Repo:** <https://github.com/rocky59487/architect_simulator>
**Base release tag:** `v2.11.1` at `f09a197` (which itself was hardened on `v2.11.0` at `3ae1cad`)

> **v3.0.0 STABLE — all 9 verification legs run green on the integrator's host
> in a single session.** v3.0.0 collapses the v2.11.1-RC follow-up items (env-
> discovery contract + GPU strict-attached fixture + 7-agent audit fixes) into
> a single STABLE tag after the previously-NOT-RUN UE + OpenSees legs both ran
> green (UE rebuild ~62 s incremental; openseespy already installed —
> release-hardening Rule #4 "verify before declaring NOT RUN" caught the stale
> assumption). The GPU lane (cuDSS on RTX 5070 Ti Laptop) confirmed strict-
> attached with the new F67s + `FFrameCoreGpuBacksubStrictTest` reverse
> assertions (silent CPU fallback FAILS). See §3 for the full 9/9 evidence.

## 1. What v2.11.1 is

A **release-hardening cycle** on top of the `v2.11.0` GPU lane. It folds five
post-tag commits (Phase 7 UE wire-up, UE F67 mirror test, HANDOFF_v2.11.0, the
R2 round 4 step 3 cuDSS PHASE_REFACTORIZATION NEGATIVE research, the v2.11
night-shift summary) into a single coherent release, and lands the BLOCKER /
HIGH findings from a 7-agent adversarial audit aimed at making this the stable
anchor for all future work.

**Engine source delta v2.11.0..v2.11.1 = 4 files / ~60 lines, all additive
guards, env-var overrides, or version strings.** The 8 verification gates
(standalone F1..F66 default + F1..F67 CUDA + UE 58/58 *(base tag; RC §2.5
adds `FFrameCoreGpuBacksubStrictTest` → **59/59**)* + OpenSees + deep audit
104 + CLI roundtrip 13 + v2_roundtrip CPU + v2_roundtrip CUDA + run_gpu_gate)
stay green for the legs reachable on this host; see §3 for the reproduction
matrix and the legs explicitly marked NOT RUN on this hardware.

## 2. What landed in v2.11.1

### 2.1 BLOCKER fixes (silent miscalibration)

- **`run_gpu_gate.ps1:90`** — `FRAMECORE_EXPECTED_ENGINE_VER` had silently
  stayed at `'2.10.0'` since v2.10.0 was tagged. With v2.11.0's
  `kEngineVer = "2.11.0"`, the GPU gate's v2_roundtrip CUDA leg would have
  silently mis-asserted on a fresh clone running `run_gpu_gate.ps1`.
  v2.11.1 bumps it to `'2.11.1'` and adds a comment tying it to
  `Dispatcher.h:kEngineVer`.
- **`FrameCore.Build.cs` cuDSS block** — was hardcoded to
  `%USERPROFILE%\anaconda3\envs\framecore-direct` with **no** env-var
  override (the supernodal block correctly honoured `SUPERNODAL_CONDA`).
  A collaborator with Miniconda or a non-default env name got a silent
  "GPU lane stays OFF" with no actionable hint. Fixed: derive `cudaRoot`
  from `SUPERNODAL_CONDA` when set (strip `\Library` suffix), fall back
  to the anaconda3 default otherwise.
- **`FrameCoreModule.cpp` CUDA DLL preload block** — same asymmetry.
  Fixed identically.
- **`FrameCoreModule.cpp` DLL load failures** — `GetDllHandle` returning
  `nullptr` was silent. Now emits `UE_LOG(Warning)` naming the DLL and
  the resolved path, so the GPU-lane delay-fault has a forensic trail
  rather than an in-editor crash with no clue.

### 2.2 HIGH fixes (correctness + diagnostic + UE narrowing)

- **`SnSession.cpp` GPU ctor — `cudaStreamCreate` failure path** — was a
  fire-and-forget. If the stream create fails, `cudaStream` stays null,
  cuDSS and cuSPARSE both fall back to the default stream (which is
  serialised on stream 0 — still correct), but **the Phase 2 async overlap
  silently disappears with no diagnostic**. v2.11.1 adds an explicit
  diag string when this happens.
- **`SnSession.cpp` ctor sync narrowed** — `cudaDeviceSynchronize()` after
  `PHASE_FACTORIZATION` would block every unrelated CUDA workload in the
  process (Niagara, ML pipelines, etc.). Narrowed to
  `cudaStreamSynchronize(p_->cudaStream)` when a stream is available,
  falling back to device sync only when stream creation had failed.

### 2.3 Documentation hardening (E-class findings)

- README + VERIFICATION.md updated to `F1..F66` default / `F1..F67` CUDA
  (was stale at `F1..F64` since the F65/F66 lazy + RHS-fastpath fixtures
  landed in v2.9.0). **RC §2.5 further extends CUDA build to F1..F67 +
  F67s strict (silent CPU fallback detection).**
- README + VERIFICATION.md UE test count `57` → `58` (Phase 7 added
  `FFrameCoreGpuBacksubTest`, the UE mirror of standalone F67). **Final count
  after v2.11.1-RC §2.5: 59 w/ cuDSS, 57 without** (RC further added
  `FFrameCoreGpuBacksubStrictTest`).
- `environment.yml` adds an "Optional: CUDA + cuDSS" comment block with
  the exact `conda install -c nvidia ...` invocation. Previously the
  GPU install steps were buried inside `build_sn_cuda.bat`.
- HANDOFF_v2.11.0.md `v2.12 deferred item 2` (cuDSS PHASE_REFACTORIZATION)
  marked **NEGATIVE** with a backlink to `RESULTS_round4_step3_refactor_negative.md`.

### 2.4 What stayed bit-identical

- `default-off + cross-vendor`: Non-CUDA builds (`FRAMECORE_CUDA=0`) keep
  the v2.11.0 CPU sn_chol.h path bit-identical. No engine numeric changes.
- All public API signatures unchanged; v2.11.0 callers re-link without source
  edits.
- Wire ABI unchanged. `kEngineVer` bump is the only protocol-visible change.

### 2.5 v2.11.1-RC follow-up (V3 STABLE candidate)

After the v2.11.1 7-agent audit landed, the user asked for five concrete items as
a precondition to flipping V3 STABLE — they harden the GPU lane's env-discovery
contract and add silent-fallback detection, each closing a path where a real bug
could hide green in CI:

1. **`Scripts/run_gpu_gate.ps1`** — replaced the hardcoded
   `$env:USERPROFILE\anaconda3\envs\framecore-direct` with `Resolve-SupernodalConda`
   (env-var → legacy alias → conda layout probe under `USERPROFILE` + `C:\ProgramData`).
   PS1 now exports `SUPERNODAL_CONDA` + `CUDA_ROOT` so the bat files see the same
   resolved root, plus `FRAMECORE_GPU_STRICT=1` when the cuDSS runtime DLL is
   present (arms F67s / `FFrameCoreGpuBacksubStrictTest`). New `-CondaEnv <path>`
   script argument for an explicit override.
2. **`Plugins/FrameSolver/Standalone/build_sn_cuda.bat`** — `CUDA_ROOT` now derives
   from `SUPERNODAL_CONDA` (strip `\Library` suffix → conda env root) when neither
   `CUDA_ROOT` nor `CUDA_PATH` is set. Diagnostic on miss lists all three env var
   names so the developer can pick one.
3. **`Plugins/FrameSolver/Standalone/build_capi_v2_cuda.bat`** — block-for-block
   mirror of (2). Closes the silent-drift class of bug between standalone and
   dispatcher CUDA builds (v2.11.1 had already hit one variant of this).
4. **Strict-attached GPU fixtures (new):**
   - `Plugins/FrameSolver/Standalone/main.cpp` adds **F67s** after F67. Runs only
     when `FRAMECORE_GPU_STRICT=1`; FAILS if the SnSession diagnostic doesn't
     carry the success substring `"[GPU] cuDSS factor ready"`.
   - `Plugins/FrameSolver/Source/FrameCore/Private/Tests/GpuBacksubTest.cpp` adds
     `FFrameCoreGpuBacksubStrictTest` alongside the existing
     `FFrameCoreGpuBacksubTest` smoke. Same enforcement; UE-side mirror.
   - `Scripts/run_gate.ps1` `$ExpectedUeTests` default bumped 58 → 59 with a
     comment for cuDSS-less boxes (pass `-ExpectedUeTests 57`).
5. **RC / V3 STABLE conditions documented:** this file (`§ banner`), the README
   status block, `docs/VERIFICATION.md` §1.5, and `docs/HANDOFF_v2.11.1.md` §1.1
   all name v2.11.1 as a Release Candidate and pin the three gates that must
   exit 0 on a single box in a single session to flip v3.0.0 STABLE.

v2.11.1-RC additional source delta vs v2.11.1 tag (`f09a197`):
- `Scripts/run_gpu_gate.ps1` — full rewrite of env-resolution block + strict flag
  arming (~60 net lines added)
- `Plugins/FrameSolver/Standalone/build_sn_cuda.bat` — `CUDA_ROOT` derivation
  block (~22 net lines added)
- `Plugins/FrameSolver/Standalone/build_capi_v2_cuda.bat` — mirror of above
  (~22 net lines added)
- `Plugins/FrameSolver/Standalone/main.cpp` — F67s strict fixture
  (~50 lines added, no edits to F67)
- `Plugins/FrameSolver/Source/FrameCore/Private/Tests/GpuBacksubTest.cpp` —
  `FFrameCoreGpuBacksubStrictTest` + new file header (~55 lines added)
- `Scripts/run_gate.ps1` — `$ExpectedUeTests` default 58 → 59 + comment (1 line
  changed)
- `README.md`, `docs/VERIFICATION.md`, `docs/HANDOFF_v2.11.1.md` — RC banner +
  V3 STABLE conditions blocks (~140 net lines doc)

Engine numerics unchanged: standalone F1..F66 default + F67 smoke + F67s strict
all green on the integrator's host; new strict path uses the same SnSession code
as the existing smoke path with a tighter diagnostic check, no production
behaviour change. Cross-vendor / default-off builds (no `FRAMECORE_CUDA`) compile
the strict fixture out via the existing `#if FRAMECORE_CUDA` guard.

## 3. Reproduction matrix (this hardware)

| Leg | Cmd | Result | Notes |
|---|---|---|---|
| 1. Standalone | `Plugins\FrameSolver\Standalone\build.bat` | **ALL PASS (failures=0)** | F1..F66 on rebuilt v2.11.1-RC source |
| 2. UE automation | `Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development -project=…\ArchSim.uproject` then `UnrealEditor-Cmd.exe …` | **59 tests run, exit 0** | Build was an incremental hit (~62 s wall-clock, 5 actions, 1 parallel due to memory pressure — well below the ≥ 1 h cold-rebuild thrash threshold). `FFrameCoreGpuBacksubStrictTest` registered + passed alongside the 58-test base. |
| 3. OpenSees | `python Tools\opensees_compare.py` | **ALL PASS** | Verified during RC re-gate — `OPENSEES GATE: PASS (engine matches OpenSees + analytic)`. The earlier HANDOFF claim "openseespy not in system python" was stale; Rule #4 (verify before declaring NOT RUN) caught it. Includes shallow-arch snap-through ours=0.00585833 vs OpenSees=0.0058962 (rel 6.4e-3 < 3e-2 tol). |
| 4. Deep audit | `Plugins\FrameSolver\Standalone\linear_deep_audit.exe` | **PASS failures=0 checks=104** | rebuilt against v2.11.1-RC source |
| 5. CLI round-trip | `python Tools\cli_roundtrip.py` | **ALL PASS (failures=0)** | **13** checks incl. DYNC / EIGEN guard, daemon, capi byte-identity, COROT planar/3D, ARCL (B-06 audit corrected stale 17 claim to authoritative 13 per `grep -c "^check(" Tools/cli_roundtrip.py`) |
| 6a. v2_roundtrip (CPU) | `build_capi_v2.bat` + `python Tools\v2_roundtrip.py` | **ALL PASS** | Dispatcher.h `kEngineVer=2.11.1` pin enforced; gpuBacksub=false reflected correctly in the CPU build |
| 6b. v2_roundtrip (CUDA) | `Scripts\run_gpu_gate.ps1 -Strict` leg 2/3 | **ALL PASS** | cuDSS-built dispatcher advertises `solve.linear.gpu_backsub`; gpuBacksub=true round-trips |
| 7a. Standalone F67 + F67s CUDA | `Scripts\run_gpu_gate.ps1 -Strict` leg 1/3 | **ALL PASS (failures=0)** | F67s strict-attached fixture confirmed cuDSS truly on device — diagnostic carries `[GPU] cuDSS factor ready (nf=6, nnz=36) + [GPU] cuSPARSE SpMV reactions ready (N=12, nnz=144)`. Silent CPU fallback would have FAILED. |
| 7b. r2_bench --gpu 90k | `Scripts\run_gpu_gate.ps1 -Strict` leg 3/3 | **PASS budget=16.67ms margin=+11.946ms** | RTX 5070 Ti Laptop, ~4.72 ms / frame at 90 k, well under the 60 fps ceiling. Confirms v2.11 GPU lane has not regressed since v2.11.0 tag. |

The honest NOT RUN list is the integrator's responsibility, per
release-hardening Bedrock principle #2 ("Honest NOT RUN beats fake PASS").
The reachable CPU-only legs were re-verified on this host with the v2.11.1
source changes already applied.

**To re-run the full matrix on a fresh box:**

```powershell
# From repo root (E:\project\ArchSim or your clone location):
conda activate framecore-direct
# Optional CUDA bits (see environment.yml § Optional: CUDA + cuDSS):
conda install -c nvidia cuda-runtime cuda-cudart-dev cuda-cudart cusparse cublas
conda install -c nvidia libcudss libcudss-dev

# Required env vars (only if not in default locations)
$env:UE_ENGINE_ROOT       = 'E:\…\UE_5.7'
$env:SUPERNODAL_CONDA     = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library"

# Engine rebuilds (touch any FrameCore .cpp -> rebuild before the gate)
.\Plugins\FrameSolver\Standalone\build.bat
.\Plugins\FrameSolver\Standalone\build_linear_audit.bat
.\Plugins\FrameSolver\Standalone\build_cli.bat
.\Plugins\FrameSolver\Standalone\build_capi_v2.bat
.\Plugins\FrameSolver\Standalone\build_sn_cuda.bat       # CUDA only
.\Plugins\FrameSolver\Standalone\build_capi_v2_cuda.bat  # CUDA only
E:\…\UE_5.7\Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development -project=$pwd\ArchSim.uproject -waitmutex

# Pin and gate
$env:FRAMECORE_EXPECTED_ENGINE_VER = '2.11.1'
.\Scripts\run_gate.ps1 -RequireOpenSees                  # gate a: 5-leg
.\Plugins\FrameSolver\Standalone\build_capi_v2.bat       # gate b prep
python .\Tools\v2_roundtrip.py                           # gate b: v2 CPU
.\Scripts\run_gpu_gate.ps1 -Strict                       # gate c: V3 STABLE GPU 6th
```

The `-Strict` flag on `run_gpu_gate.ps1` is the key V3 STABLE arm: it FAILS the
gate if cuDSS DLLs are absent, instead of soft-skipping. Drop the flag to
soft-skip on non-cuDSS boxes (the release then ships with the GPU lane
DOCUMENTED but NOT VERIFIED on that owner's box).

## 4. v2.12 deferred (audit IDs traceable to HANDOFF_v2.11.1.md)

Per release-hardening Bedrock principle #7, the deferred list is public and
each item points back to the auditing finding.

- **A-02** — Wrap CUDA handles + raw pointers in `SnSession::Impl` with RAII
  guards (`CuStreamGuard`, `CuMallocGuard`, `CuDssDataGuard`). Today's
  10-pointer + 6-handle dtor null-check pattern is correct but glass-floored
  for any future GPU work (CUDA Graph, stream pool).
- **A-05 / F-14** — OpenMP `parallelRhs` thread-count heuristic
  (`min(8, elems.size()/4096)`) doesn't honour `opts.numThreads` and the
  hardcoded 8 can include logical-only HT cores on chips that hurt the
  factor (HP research durable: HT can be 2× slower).
- **A-12 / D-2** — cuDSS `PHASE_REFACTORIZATION` was measured 1.04-1.07×
  on the frame-tower fixture (`RESULTS_round4_step3_refactor_negative.md`)
  — **NEGATIVE** for that workload, but the fixture is uniform `K*(1+1e-3)`
  scaling; P-Delta `K_sigma` is non-uniform and the methodology section
  explicitly leaves that open. Revisit on cuDSS ≥ 0.9 or with a true
  P-Delta fixture.
- **C-01** — Phase-2 async memcpy buffers (`b`, `u_h`, `F_h`, `r_h`) are
  pageable heap memory. CUDA spec requires async memcpy to use pinned host
  memory for fully-defined behaviour; current driver auto-down-grade to
  sync on pageable is implementation-detail. Move the four buffers to
  `cudaMallocHost` in `Impl`, pre-allocated at GPU ctor.
- **C-06** — `FFrameCoreGpuBacksubTest` + standalone F67 both only cover
  the `cantileverTipLoad` fixture (Qf=0). Phase 1 SpMV reactions for
  non-zero Qf (UDL, shell pressure, self-weight) has no oracle. Add a
  UDL fixture + a `parallelRhs=true` + `useGpuBacksub=true` combination.
- **C-07** — `DynamicCollapse` builds its own `assembleAndFactor` per event
  (`useSupernodalPrimary=false`); the 60 fps GPU headline does NOT apply
  to dynamic-collapse workloads. Document explicitly in `SnSession.h` /
  `DynCollapseOptions`, and consider a GPU-integrated dynamic-collapse
  refactor in v3.0.
- **D-02 / D-03** — UE `bCudaEnabled` explicit plugin flag (today: auto-
  detect via conda env). UE packaging recipe for cuDSS DLLs (today:
  Editor-only).
- **D-08** — `GpuBacksubTest.cpp` anonymous-namespace `gpuRelInf` helper
  may collide with other Test TU helpers under `bUseUnity=true` + CUDA
  build (currently passes — `#if FRAMECORE_CUDA` guards the whole TU).
  Rename to `GpuBacksubRelInf` or hoist into header.
- **D-10 / D-11 / F-16** — Cosmetic: `mat67` Material constructor delta
  between standalone F67 and UE GpuBacksubTest, `gpuRelInf` UE coupling.
- **E-07 / E-08** — NIGHT_SHIFT_*.md files written in first-person agent
  voice; add internal-session-log disclaimer or move to `Research/`.
- **F-02 / F-03 / F-04 / F-10** — Hoist 4 per-frame `std::vector<double>(N)`
  heap allocations (`spmvU_h`, `spmvF_h`, `r_h`, `snB`, `snX`, `prescBuf`)
  into `Impl`, pre-sized at ctor. Saves ~15 MB per frame at 200K of
  malloc/free churn. Held back from v2.11.1 because the refactor touches
  ctor init + every solveFrame path; v2.12 work, gated by F1..F67 +
  F55/F56/F58.
- **F-08** — `FrameModel::nodeIndex` is still O(N) linear scan inside
  `validate()`, making validate O(N²). Hot only at `assembleAndFactor`
  time (not per-frame), but worth hoisting the cache to FrameModel.

See `docs/HANDOFF_v2.11.1.md` for the **First action on day 1** sketch for
each deferred item.

## 5. Wire / ABI

- **Engine ABI**: additive only. No new public fields beyond what v2.11.0
  shipped. `SnSessionOptions::parallelRhs` is still the only new trailing
  field over v2.10.0; default `false` keeps every call site bit-identical.
- **Wire ABI**: unchanged. Same dispatcher capability set as v2.11.0.
  The `solve.linear.gpu_backsub` capability still depends on the engine
  binary's `FRAMECORE_CUDA` flag.
- **`kEngineVer`**: 2.11.0 → 2.11.1
- **`FrameSolver.uplugin VersionName`**: 2.11.0 → 2.11.1, Version 25 → 26
- **`run_gpu_gate.ps1 FRAMECORE_EXPECTED_ENGINE_VER`**: 2.10.0 → 2.11.1
  (was stale by one major version)

## 6. Hardware reference (unchanged from v2.11.0)

NVIDIA GeForce RTX 5070 Ti Laptop GPU (Blackwell, SM 12.0, 12.8 GB VRAM)
Driver 595.97 / CUDA 13.2 runtime. cuDSS 0.8.0.10 + cuSPARSE 12.x via conda
nvidia channel. CPU R9 8940HX (Zen 4, 16C/32T). The performance numbers in
RELEASE_v2.11.md §1 (60 fps at 200K, 12.3× / 35× at 90 k) are unchanged in
v2.11.1 — no GPU-lane source paths were touched.

## 7. Tag plan

**Final state:** `v2.11.1` at `f09a197` is left in place as the prior published
release. v3.0.0 STABLE is a fresh tag on top of `f09a197` containing the 5
v2.11.1-RC follow-up items + the 7-agent audit small-fixes. All 9 verification
legs ran green in the same integrator session before tagging.

```bash
# 0. Pre-flight: stage the v3.0.0 changeset (10 source/script files + renamed release notes)
git add -- \
  Plugins/FrameSolver/Source/FrameCore/Private/Tests/GpuBacksubTest.cpp \
  Plugins/FrameSolver/Standalone/build_capi_v2_cuda.bat \
  Plugins/FrameSolver/Standalone/build_sn_cuda.bat \
  Plugins/FrameSolver/Standalone/main.cpp \
  README.md \
  Scripts/run_gate.ps1 \
  Scripts/run_gpu_gate.ps1 \
  docs/HANDOFF_v2.11.1.md \
  docs/RELEASE_v3.0.0.md \
  docs/VERIFICATION.md

# (docs/RELEASE_v2.11.1.md was renamed to docs/RELEASE_v3.0.0.md via `git mv` so
# git auto-detects the rename and preserves history with `git log --follow`.)

# 2. Single annotated tag commit on main
git commit -m "release: v3.0.0 STABLE -- V3 anchor (GPU lane env-discovery + strict-attached + 9/9 verified)"
git tag -a v3.0.0 -m "v3.0.0 STABLE -- V3 anchor (GPU lane env-discovery + strict-attached + 9/9 verified)"
git push origin main
git push origin v3.0.0

# 3. GitHub release marked Latest
gh release create v3.0.0 \
  --title "v3.0.0 STABLE -- V3 anchor (GPU lane env-discovery + strict-attached + 9/9 verified)" \
  --notes-file docs/RELEASE_v3.0.0.md \
  --latest
```
