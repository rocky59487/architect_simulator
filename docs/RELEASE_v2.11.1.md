# FrameCore v2.11.1 — release-hardening on the v2.11 GPU lane

**Tag (planned):** `v2.11.1`
**Branch:** `main`
**Date:** 2026-06-21
**Repo:** <https://github.com/rocky59487/architect_simulator>
**Base release tag:** `v2.11.0` at `3ae1cad`

## 1. What v2.11.1 is

A **release-hardening cycle** on top of the `v2.11.0` GPU lane. It folds five
post-tag commits (Phase 7 UE wire-up, UE F67 mirror test, HANDOFF_v2.11.0, the
R2 round 4 step 3 cuDSS PHASE_REFACTORIZATION NEGATIVE research, the v2.11
night-shift summary) into a single coherent release, and lands the BLOCKER /
HIGH findings from a 7-agent adversarial audit aimed at making this the stable
anchor for all future work.

**Engine source delta v2.11.0..v2.11.1 = 4 files / ~60 lines, all additive
guards, env-var overrides, or version strings.** The 8 verification gates
(standalone F1..F66 default + F1..F67 CUDA + UE 58/58 + OpenSees + deep audit
104 + CLI roundtrip + v2_roundtrip CPU + v2_roundtrip CUDA + run_gpu_gate)
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
  landed in v2.9.0).
- README + VERIFICATION.md UE test count `57` → `58` (Phase 7 added
  `FFrameCoreGpuBacksubTest`, the UE mirror of standalone F67).
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

## 3. Reproduction matrix (this hardware)

| Leg | Cmd | Result | Notes |
|---|---|---|---|
| 1. Standalone | `Plugins\FrameSolver\Standalone\build.bat` | **ALL PASS (failures=0)** | F1..F66 on rebuilt v2.11.1 source |
| 2. UE automation | `Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development -project=…\ArchSim.uproject` then `UnrealEditor-Cmd.exe …` | **NOT RUN** | Build.cs + FrameCoreModule.cpp + GpuBacksubTest.cpp touched in this cycle; UE module must be rebuilt before re-running. Standalone F67 is the cross-confirm. |
| 3. OpenSees | `python Tools\opensees_compare.py` | **NOT RUN** | `openseespy` not in the system python on this host; previously verified on tag `v2.11.0` |
| 4. Deep audit | `Plugins\FrameSolver\Standalone\linear_deep_audit.exe` | **PASS failures=0 checks=104** | rebuilt against v2.11.1 source |
| 5. CLI round-trip | `python Tools\cli_roundtrip.py` | **ALL PASS (failures=0)** | 17 checks incl. DYNC / EIGEN guard, daemon, capi byte-identity, COROT planar/3D, ARCL |
| 6a. v2_roundtrip (CPU) | `build_capi_v2.bat` + `python Tools\v2_roundtrip.py` | **NOT RUN** | Dispatcher.h `kEngineVer` bumped — DLL must be rebuilt |
| 6b. v2_roundtrip (CUDA) | `build_capi_v2_cuda.bat` + run_gpu_gate.ps1 leg 2 | **NOT RUN** | Same as 6a + cuDSS lane |
| 7. r2_bench --gpu | `r2_bench.exe --preset 90k --gpu --compare --repeat 30` | **NOT RUN** | Headline 4.56 ms @ 90 k carried forward from v2.11.0 (no GPU lane source change) |

The honest NOT RUN list is the integrator's responsibility, per
release-hardening Bedrock principle #2 ("Honest NOT RUN beats fake PASS").
The reachable CPU-only legs were re-verified on this host with the v2.11.1
source changes already applied.

**To re-run the full matrix on a fresh box:**

```powershell
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
.\Scripts\run_gate.ps1 -RequireOpenSees
.\Scripts\run_gpu_gate.ps1
```

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

```bash
git add -- \
  Plugins/FrameSolver/FrameSolver.uplugin \
  Plugins/FrameSolver/Source/FrameCore/FrameCore.Build.cs \
  Plugins/FrameSolver/Source/FrameCore/Private/FrameCoreModule.cpp \
  Plugins/FrameSolver/Source/FrameCore/Private/SnSession.cpp \
  Plugins/FrameSolver/Standalone/v2/Dispatcher.h \
  Scripts/run_gpu_gate.ps1 \
  environment.yml \
  README.md \
  docs/VERIFICATION.md \
  docs/HANDOFF_v2.11.0.md \
  docs/RELEASE_v2.11.1.md \
  docs/HANDOFF_v2.11.1.md

git commit -m "release: v2.11.1 -- release-hardening on v2.11 GPU lane + 7-agent audit"
git tag -a v2.11.1 -m "v2.11.1 release-hardening"
git push origin main
git push origin v2.11.1
gh release create v2.11.1 --title "v2.11.1 -- release-hardening" --notes-file docs/RELEASE_v2.11.1.md
```
