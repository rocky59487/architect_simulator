# Handoff — v3.0.0 STABLE shipped + v3.0.1 patch (2026-06-21)

> **Historical note (this file's original framing was `v2.11.1-RC → V3 STABLE
> candidate`):** v3.0.0 was tagged the same day after UE rebuild + OpenSees re-ran
> green, and v3.0.1 followed within the same session to close 6 post-release
> consistency findings (version surface sync, strict-test executed fingerprint
> enforcement, r2_bench regression hard gate, CI workflow + gate-log artifacts,
> UE Build.cs normalisation, doc scrub). The §-numbered content below still
> reflects the v2.11.1-RC → STABLE flow because it's the right narrative for the
> incoming maintainer; the §2 reproduction matrix shows the as-run green state.

## Legacy lede (preserved for context)


> Supplements `HANDOFF_v2.11.0.md`. v2.11.1 is the release-hardening pass
> on the v2.11 GPU lane: integrates 5 post-tag commits + closes 6 BLOCKER /
> HIGH findings from a 7-agent adversarial audit. Engine code delta vs
> v2.11.0 = 4 files / ~60 lines, all additive guards / env-var overrides /
> version strings.
>
> **v2.11.1-RC supplement (this section's 5 follow-up items, same session):**
> 5 hardening items land the GPU env-discovery contract and add a strict
> GPU-attached fixture so silent CPU fallback in CI is a FAIL, not a green-
> washed PASS. v2.11.1 is a Release Candidate until UE 59/59 (w/ cuDSS),
> v2_roundtrip CPU + CUDA, and `run_gpu_gate.ps1 -Strict` all run green
> on the same box in the same session — that is the v3.0.0 STABLE flip.

## 1. v2.11.1 = what

A 1-day release-hardening cycle. The user asked for "硬體調用、資源分配、
程式碼強度全方位推到最好,讓它成為後續工作穩定的錨點" — long-form, deep,
release-as-anchor. The deliverable:

- **Engine numerics:** unchanged (default-off + cross-vendor still
  bit-identical to v2.11.0; GPU lane source paths untouched, so 60 fps @ 200 K
  carries forward).
- **Engine diagnostics:** `cudaStreamCreate` failure no longer silent (Phase 2
  overlap loss now reported); UE DLL preload failures emit `UE_LOG(Warning)`.
- **Engine resource lifecycle:** ctor `cudaDeviceSynchronize` narrowed to
  `cudaStreamSynchronize` so unrelated CUDA workloads in the same process
  (Niagara, ML pipelines) aren't blocked during factor build.
- **Build portability:** `FrameCore.Build.cs` + `FrameCoreModule.cpp`
  cuDSS detection now honours `SUPERNODAL_CONDA` (was hardcoded to
  `%USERPROFILE%\anaconda3\envs\framecore-direct` — broke Miniconda or
  custom env-name boxes silently).
- **Gate calibration:** `run_gpu_gate.ps1 FRAMECORE_EXPECTED_ENGINE_VER`
  bumped 2.10.0 → 2.11.1 (was stale by one major; v2_roundtrip CUDA leg
  on a fresh clone would have silently mis-asserted).
- **Version strings:** `kEngineVer` 2.11.0 → 2.11.1; uplugin 25/2.11.0 →
  26/2.11.1; README/VERIFICATION/HANDOFF docs UE-count 57 → 58,
  F-fixture range 64 → 66/67.
- **Documentation:** RELEASE_v2.11.1.md + this handoff; environment.yml
  documents the optional CUDA install; HANDOFF_v2.11.0 item 2 (cuDSS
  PHASE_REFACTORIZATION) marked NEGATIVE with research backlink.

`git diff --stat v2.11.0..HEAD` after Phase 2:
- `Plugins/FrameSolver/FrameSolver.uplugin`              | 4 lines
- `Plugins/FrameSolver/Source/FrameCore/FrameCore.Build.cs` | ~28 lines (new SUPERNODAL_CONDA-aware cudaRoot block)
- `Plugins/FrameSolver/Source/FrameCore/Private/FrameCoreModule.cpp` | ~30 lines (env-var override + UE_LOG warns)
- `Plugins/FrameSolver/Source/FrameCore/Private/SnSession.cpp` | ~10 lines (stream-fail diagnostic + stream sync)
- `Plugins/FrameSolver/Standalone/v2/Dispatcher.h` | 1 line
- `Scripts/run_gpu_gate.ps1` | 4 lines
- `environment.yml` | ~18 lines (CUDA install comment block)
- docs/* | ~330 lines

## 1.1. v2.11.1-RC follow-up (post-tag, V3 STABLE candidate)

Five additional items landed in the same session after the v2.11.1 7-agent audit, in
response to the user's "do these then ship V3 STABLE" request. They tighten the GPU
lane's discovery contract and add silent-fallback detection — each one closes a path
where a real bug could hide green in CI.

1. **`Scripts/run_gpu_gate.ps1` — canonical `SUPERNODAL_CONDA` resolver.** The script
   used to hardcode `$env:USERPROFILE\anaconda3\envs\framecore-direct`. Replaced with
   `Resolve-SupernodalConda`: env var (`SUPERNODAL_CONDA` → `FRAMECORE_LIB_DIR` legacy
   alias) → conda layout probe (`anaconda3` / `miniconda3` / `mambaforge` /
   `miniforge3` × `USERPROFILE` + `C:\ProgramData`), preferring envs that actually
   carry `cudss.h`. New `-CondaEnv <path>` script arg for explicit override. The
   resolved root is **exported** into `SUPERNODAL_CONDA` and `CUDA_ROOT` before the
   bat files run, so PS1 and bat agree on a single env-root contract.

2. **`Plugins/FrameSolver/Standalone/build_sn_cuda.bat` — derive `CUDA_ROOT` from
   `SUPERNODAL_CONDA`.** Precedence top-to-bottom: `CUDA_ROOT` override → strip
   `\Library` off `SUPERNODAL_CONDA` to get the conda env root → `CUDA_PATH` standard
   CUDA Toolkit install → legacy anaconda3 default. Diagnostic on miss lists all
   three env vars by name so a developer can pick one.

3. **`Plugins/FrameSolver/Standalone/build_capi_v2_cuda.bat` — block-for-block mirror
   of (2).** Silent drift between the standalone and dispatcher CUDA builds was the
   class of bug v2.11.1 had already hit (different code paths each rolled their own
   conda discovery); v2.11.1-RC closes the same gap on the second bat.

4. **GPU strict-attached vs smoke split (new fixtures).**
   - **F67 (standalone) + `FFrameCoreGpuBacksubTest` (UE)** — kept as SMOKE. Tolerate
     silent CPU fallback so devs can compile-test the CUDA lane on any box.
   - **F67s (standalone, new) + `FFrameCoreGpuBacksubStrictTest` (UE, new)** — STRICT.
     Run only when `FRAMECORE_GPU_STRICT=1` in the env (set automatically by
     `run_gpu_gate.ps1` when the cuDSS runtime DLL resolves). Diagnostic must carry
     the success substring `"[GPU] cuDSS factor ready"`; any fallback path emits a
     different substring (`"[GPU] cuDSS context create failed; CPU lane used"`, etc.)
     and FAILS. UE `$ExpectedUeTests` default bumped 58 → 59; cuDSS-less boxes pass
     `-ExpectedUeTests 57` because both GPU tests `#if FRAMECORE_CUDA` compile out.

5. **v2.11.1 marked RC; V3 STABLE flip conditions enumerated.** README status,
   VERIFICATION §1.5, and this handoff now name v2.11.1 as a Release Candidate and
   pin three commands that must all return exit 0 on one box, one session, before
   `v3.0.0` STABLE can be tagged:

   ```powershell
   # On a box with framecore-direct conda env + UE_5.7 + cuDSS DLLs:
   $env:UE_ENGINE_ROOT     = 'E:\…\UE_5.7'
   $env:SUPERNODAL_CONDA   = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library"
   # (or pass -CondaEnv to run_gpu_gate.ps1, or rely on probe)

   # 1) rebuild everything the new gates depend on
   .\Plugins\FrameSolver\Standalone\build.bat              # standalone default
   .\Plugins\FrameSolver\Standalone\build_linear_audit.bat # leg 4
   .\Plugins\FrameSolver\Standalone\build_cli.bat          # leg 5
   .\Plugins\FrameSolver\Standalone\build_capi_v2.bat      # gate b
   .\Plugins\FrameSolver\Standalone\build_sn_cuda.bat      # gate c (CUDA only)
   .\Plugins\FrameSolver\Standalone\build_capi_v2_cuda.bat # gate c (CUDA only)
   & "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" `
       ArchSimEditor Win64 Development -project="$pwd\ArchSim.uproject" -waitmutex

   # 2) the three V3 STABLE gates (all must exit 0 in this session)
   .\Scripts\run_gate.ps1 -RequireOpenSees                          # a
   .\Plugins\FrameSolver\Standalone\build_capi_v2.bat; `
     python .\Tools\v2_roundtrip.py                                 # b
   .\Scripts\run_gpu_gate.ps1 -Strict                               # c
   ```

   If gate (c)'s `-Strict` fails on `cudss64_0.dll missing`, the box doesn't have
   the GPU lane; either install cuDSS via `conda install -c nvidia libcudss-dev`
   and re-run, or drop the `-Strict` flag and accept the soft-skip (v3.0.0 then
   ships with the GPU lane DOCUMENTED but NOT VERIFIED on that owner's box, which
   is the same posture as v2.11.0 — i.e. legitimately downgrading the release
   from "GPU lane verified" to "CPU lane verified, GPU lane present"). The
   user's stated goal is the verified posture.

## 2. Gates (what ran, what didn't)

The v2.11.1-RC follow-up items closed the GPU-lane discovery contract sufficiently
that this session was able to run the GPU 6th gate to completion on the
integrator's host (RTX 5070 Ti Laptop + cuDSS 0.8 in conda framecore-direct).
After the v2.11.1-RC re-gate session, OpenSees strict re-ran green (the earlier
"not in system python" claim was stale; openseespy was already installed —
release-hardening Bedrock #4 caught it). **The one remaining NOT-RUN leg is
UE 59/59**, which needs UE 5.7 module rebuild (≥ 1 h swap-thrash on 31 GB RAM
per HANDOFF_v2.11.0 durable). All other 8 legs ran green on the integrator's
host this session.

| Leg | Status | Reproduce |
|---|---|---|
| Standalone F1..F66 default | **ALL PASS (failures=0)** rebuilt v2.11.1-RC | `Plugins\FrameSolver\Standalone\build.bat` |
| Linear deep audit 104 checks | **PASS failures=0** rebuilt v2.11.1-RC | `Plugins\FrameSolver\Standalone\linear_deep_audit.exe` |
| CLI roundtrip **13** checks | **ALL PASS (failures=0)** rebuilt v2.11.1-RC (B-06 audit corrected stale "17" claim to authoritative 13 per `grep -c "^check(" Tools/cli_roundtrip.py`) | `python Tools\cli_roundtrip.py` |
| v2_roundtrip CPU | **ALL PASS** rebuilt v2.11.1-RC (kEngineVer 2.11.1 pinned) | `build_capi_v2.bat` then `python Tools\v2_roundtrip.py` |
| Standalone F1..F67 + F67s CUDA | **ALL PASS (failures=0)** rebuilt v2.11.1-RC under `FRAMECORE_GPU_STRICT=1`; cuDSS truly attached on device (diagnostic carries `[GPU] cuDSS factor ready (nf=6, nnz=36) + [GPU] cuSPARSE SpMV reactions ready`) | `Scripts\run_gpu_gate.ps1 -Strict` |
| v2_roundtrip CUDA | **ALL PASS** rebuilt v2.11.1-RC (FRAMECORE_EXPECTED_GPU_CAP enforced) | `Scripts\run_gpu_gate.ps1 -Strict` (leg 2/3) |
| r2_bench --gpu 90k | **PASS** 60 fps budget margin **+11.946 ms** (i.e. ~4.72 ms / frame on RTX 5070 Ti Laptop, well under the 16.67 ms 60-fps ceiling) | `Scripts\run_gpu_gate.ps1 -Strict` (leg 3/3) |
| UE automation 59/59 w/ cuDSS | **ALL PASS — 59 tests run, exit 0** (rebuild was incremental ~62 s wall-clock vs ≥ 1 h cold-rebuild estimate; FFrameCoreGpuBacksubStrictTest registered + passed) | `…\UE_5.7\Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development …` then `Scripts\run_gate.ps1 -RequireOpenSees` |
| OpenSees strict | **ALL PASS** on this host (verified post-RC; system python had openseespy installed -- the HANDOFF's earlier "not in system python" claim was stale; rebuild + `python Tools\opensees_compare.py` returned `OPENSEES GATE: PASS (engine matches OpenSees + analytic)`) | `python Tools\opensees_compare.py` |

The NOT RUN list is explicit and reproducible. CPU-only legs were re-verified
on this host with v2.11.1 source already applied. UE + CUDA legs require a
rebuild step that wasn't done in this session — first action on day 1 below.

## 3. First action on day 1 (deferred items, traceable to audit IDs)

Each item is concrete: file:line + command, not "investigate".

### Immediate (re-verify on owner's box, ~30 min)

**Z-01 — Rebuild UE module and confirm UE 59/59 w/ cuDSS (57/57 without) + GPU leg green**
```powershell
# From repo root (E:\project\ArchSim or your clone location):
$env:UE_ENGINE_ROOT = 'E:\…\UE_5.7'  # set to your UE 5.7 install path
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" `
    ArchSimEditor Win64 Development -project="$pwd\ArchSim.uproject" -waitmutex
$env:FRAMECORE_EXPECTED_ENGINE_VER = '2.11.1'
.\Scripts\run_gate.ps1 -RequireOpenSees
.\Scripts\run_gpu_gate.ps1 -Strict   # -CondaEnv <path> if conda is in a non-standard location
```
Expected: 5-leg gate green (UE = 59/59 w/ cuDSS or 57/57 without, audit 104), 6th gate (run_gpu_gate)
either green or soft-skip if cuDSS isn't on the box. If `run_gpu_gate.ps1`
errors on `FRAMECORE_EXPECTED_ENGINE_VER`, the v2.11.1 fix worked
(previously would have errored on `2.10.0`).

### v2.12 priority 1 (correctness)

**C-01 — Pinned host memory for Phase-2 async memcpy**
First action: In `Plugins/FrameSolver/Source/FrameCore/Private/SnSession.cpp`,
in the `#ifdef FRAMECORE_CUDA` `Impl` struct, add four pinned buffers:
```cpp
double* h_b_pinned    = nullptr;  // size: max gpuNf
double* h_x_pinned    = nullptr;  // size: max gpuNf
double* h_u_pinned    = nullptr;  // size: Nfull (SpMV input)
double* h_F_pinned    = nullptr;  // size: Nfull (SpMV F)
```
Allocate with `cudaHostAlloc(..., cudaHostAllocDefault)` at GPU ctor (right
after the cudaMalloc batch around line 238); free in dtor before
`cudaStreamDestroy`. Then in solveFrame (line 515 onwards), replace
`b.data()` and `x.data()` with the pinned pointers, copying from Eigen
vectors only once per frame. Bit-equivalent to today; eliminates the
"pageable host memory + async memcpy = undefined-per-CUDA-spec" risk.
Run `frametest_cuda.exe` F67 + `r2_bench.exe --gpu --preset 90k` to verify.

**C-06 — Add UDL + parallelRhs+GPU fixtures**
First action: In `Plugins/FrameSolver/Standalone/main.cpp`, find the `[F67]`
block (around line 3960) and add `[F67b]` after it: same cantilever but with
a `MemberUDL` instead of nodal tip load. Verify GPU.reactions == CPU.reactions
at rel<1e-8 (current F67 only tests Qf=0). Then add `[F67c]` with
`SnSessionOptions{useGpuBacksub=true, parallelRhs=true}` on a 64-element
distributed-load frame. Update `Plugins/FrameSolver/Source/FrameCore/Private/Tests/GpuBacksubTest.cpp`
to mirror.

### v2.12 priority 2 (perf)

**F-02/F-03/F-04/F-10 — Hoist per-frame allocations into Impl**
First action: In `SnSession.cpp`'s `Impl` struct, declare:
```cpp
std::vector<double> spmvU_h, spmvF_h, spmvR_h;  // Phase 1 SpMV bufs
std::vector<double> snB_, snX_;                  // CPU backsub bufs
std::vector<real>   prescBuf_;                   // RHS scatter buf
```
In ctor after `S.nf` / `S.K.rows()` are known, `resize` them once. In
solveFrame, replace `std::vector<double> b(n), x(n);` (line 515),
`std::vector<double> u_h(Nfull)` (line 620), `std::vector<double> r_h(N)`
(line 637), and `std::vector<real> presc(N, 0.0)` (line 480) with the
pre-sized members + `std::fill` reset. Bit-identical (oracle: F55/F56/F58
+ F67). Eliminates ~15 MB/frame of malloc/free at 200K.

**A-05/F-14 — OpenMP thread heuristic**
First action: In `SnSession.cpp` around line 455, change
`std::max(1, std::min(8, (int)S.elems.size() / 4096))` to honour
`opts.numThreads` (when >0) and use `std::thread::hardware_concurrency()/2`
as the upper bound rather than hardcoded 8 (avoids HT degradation per HP
research durable). Extract `kRhsMinElemsPerThread = 4096` and
`kRhsMaxThreadsDefault = 8` as named constants.

**A-12/D-2 — cuDSS PHASE_REFACTORIZATION revisit on P-Delta fixture**
First action: NEGATIVE on uniform `K*(1+1e-3)` scaling
(`RESULTS_round4_step3_refactor_negative.md`); the methodology section
explicitly excludes P-Delta `K_sigma` non-uniform updates. To revisit:
extend `gpu_bench3_refactor.cpp` with a `K_sigma`-style perturbation
(only stiffness rows of axial-loaded members; diagonal-dominant update).
If still NEGATIVE on cuDSS 0.8, drop from v2.12; if POSITIVE, wire into
P-Delta path.

### v2.12 priority 3 (correctness + safety net)

**A-02 — CUDA RAII guards**
First action: Create
`Plugins/FrameSolver/Source/FrameCore/Private/CudaRaii.h` with
`CuStreamGuard`, `CuMallocGuard<T>`, `CuDssHandleGuard`,
`CuDssDataGuard`, `CuDssMatrixGuard`. Convert `SnSession::Impl`'s 10
raw `cudaMalloc` pointers + 6 cuDSS/cuSPARSE handles + 1 `cudaStream` to
RAII. Dtor becomes ~5 lines instead of 30 null-check lines. Pre-requisite
for v2.12 CUDA Graph work — the raw-pointer pattern doesn't survive Graph
capture safely.

**C-07 — Document DynamicCollapse GPU limitation**
First action: In `Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/SnSession.h`
add a comment paragraph above the SnSession class: "Performance numbers
(60 fps @ 200K) reference static re-solve workloads via SnSession only.
DynamicCollapse builds its own per-event factor and does NOT use the GPU
lane; per-event factor cost is ~1.7 s @ 90k CPU. v3.0 may add a
GPU-integrated dynamic-collapse pipeline." Same paragraph in
`DynCollapseOptions` doc block.

### v2.12 priority 4 (UE packaging + ergonomics)

**D-03 — UE explicit `bCudaEnabled` plugin flag**
First action: In `Plugins/FrameSolver/FrameSolver.uplugin` add
`"PluginSettings": { "bCudaEnabled": true }` or per-target
`Modules[0].CudaEnabled`. In `FrameCore.Build.cs`, read this via
`Target.ProjectFile`-based JSON parse before falling through to the
auto-detect heuristic; let the user explicitly opt out even when cuDSS
is present in the env (useful for build-bots that want CPU-only).

**D-02 — UE packaging recipe (cuDSS DLLs in staged build)**
First action: In `FrameCore.Build.cs` after the existing
`RuntimeDependencies.Add` loop, add a `Target.Type == TargetType.Game`
branch that calls `Target.GlobalDefinitions.Add("WITH_FRAMECORE_CUDA=1")`
and emits `Stage` directives. Verify with
`Engine\Build\BatchFiles\RunUAT.bat BuildCookRun -project=...uproject
-platform=Win64 -clientconfig=Development -package`.

### Non-priority (cosmetic / doc)

- **E-07/E-08** — NIGHT_SHIFT_*.md disclaimer or move to `Research/`
- **D-10/D-11/F-16** — F67 `mat67` constructor + `gpuRelInf` std namespace
- **F-08** — Hoist `FrameModel::nodeIndex` cache to FrameModel public API
- **D-08 (RC audit)** — rename `gpuRelInf` to `GpuBacksubRelInf` in
  `Plugins/FrameSolver/Source/FrameCore/Private/Tests/GpuBacksubTest.cpp` to
  remove the latent unity-build collision risk (compile-only fix, no oracle
  needed beyond `run_gate.ps1` UE leg).
- **D-11 (RC audit)** — `:derive_cuda_root` duplicated verbatim in
  `build_sn_cuda.bat` and `build_capi_v2_cuda.bat`. First action: extract to a
  shared `Plugins/FrameSolver/Standalone/_cuda_env.bat` and `call` from both.
- **D-06 (RC audit)** — `r2_bench --gpu` perf gate only enforces "margin ≥ 0"
  vs 16.67 ms budget, not regression vs v2.11.0 baseline (~4.56 ms). First
  action: add `--baseline <ms>` flag to `Research/R2_realtime_150k/r2_bench.cpp`
  and have `run_gpu_gate.ps1` pass `4.56` so a silent GPU-disable regression
  to ~12 ms still trips the gate.

## 4. Durable lessons (extra to HANDOFF_v2.10.0 / v2.11.0)

18. **`SUPERNODAL_CONDA` is the canonical env-root contract.** Three
    different code paths used to derive their own conda root differently
    (`FrameCore.Build.cs` supernodal vs cuDSS, `FrameCoreModule.cpp`,
    `build_sn_cuda.bat`). All four now derive from the same env var
    (with the same `\Library`-suffix-strip rule). Future GPU library
    additions should follow the same pattern, never `Environment.GetFolderPath`.

19. **A version-string env var that isn't tested fires only at release.**
    `FRAMECORE_EXPECTED_ENGINE_VER` in `run_gpu_gate.ps1` had been stale at
    `2.10.0` from the moment v2.11.0 was tagged; nobody noticed because
    the GPU gate isn't part of the 5-leg gate `run_gate.ps1`. The fix is
    a 1-liner; the lesson is that **the 6th gate's env-var pins need to
    move when `kEngineVer` moves**, and the comment in `run_gpu_gate.ps1`
    now reminds future maintainers.

20. **`cudaStreamCreate` failure is a perf cliff, not a crash.** Both
    cuDSS and cuSPARSE accept a `nullptr` stream (= default stream),
    which serialises but works. The Phase-2 async overlap silently
    disappears with no diagnostic — i.e. the lane appears to work but
    is back at v2.10.0 perf with no clue. Always emit an explicit
    diagnostic on `cudaStreamCreate` failure even when downstream calls
    survive a null handle.

21. **`cudaDeviceSynchronize` in a multi-CUDA-user process is a bug.**
    UE 5.7 doesn't ship CUDA, but UE projects that integrate ML / Niagara
    GPU sim / third-party plugins frequently do. `cudaDeviceSynchronize`
    blocks every CUDA workload in the process; `cudaStreamSynchronize`
    only blocks our stream. Always prefer the latter when a stream is
    available. This was a latent bug in v2.11.0 that v2.11.1 fixes
    pre-emptively.

22. **Release-hardening exposes the post-tag commits as the real
    deliverable.** v2.11.0 tag was at `3ae1cad` 11:45; HEAD at the start
    of v2.11.1 was 5 commits later (Phase 7 UE wire + UE F67 + 3 docs).
    The 5 commits were the difference between "engine perf release" and
    "stable anchor for all future work" — UE integration + cross-platform
    detection + the negative-result research artefact that closes a v2.12
    priority. A patch release was the right shape.

## 5. v2.12 open questions (carried over + new)

1. Should `parallelRhs` move from opt-in to detect-and-enable using the
   Qf detection? When `qfAllZero=false`, the Phase 3 OpenMP path wins;
   when `qfAllZero=true`, Phase 3' kills the whole stage anyway. The
   detect-and-enable wins both cases without an exposed flag.
2. Should the v2 dispatcher GPU flag default `true` when the engine
   binary was built with `FRAMECORE_CUDA=1`? (Carried over from v2.10 —
   still open, no audit-driven push either way in v2.11.1.)
3. Should `SnSession::Impl` move to RAII (A-02) before v2.12 GPU work,
   or after? Argument for before: every v2.12 GPU change rests on the
   current null-check pattern. Argument for after: needs a measured
   regression budget on the ctor cost.
4. Is the NIGHT_SHIFT_*.md doc series a public artefact or an internal
   session log? E-07/E-08 surfaced this as ambiguous; the call for v2.12
   is "either harden them into engineering specs or relocate".

## 6. Pointers for the next session

- Authority on engine state: `RELEASE_v2.11.1.md` (this cycle) + this
  HANDOFF.
- Previous handoffs (frozen, do not edit): `HANDOFF.md` (v2.1 era) →
  `HANDOFF_v2.10.0.md` → `HANDOFF_v2.11.0.md` → this file.
- Performance reference: `Research/R2_realtime_150k/RESULTS_round4_scaling.md`.
- 7-agent audit findings (not committed; lived in the v2.11.1 release-
  hardening session). Top-3 concerns for forward planning:
  (a) pinned-host-memory (C-01) is the most subtle production risk;
  (b) UE 59/59 w/ cuDSS (57/57 without) + GPU leg re-verify is the first owner action
  (Z-01 above) because Build.cs / Module / GpuBacksubTest were all
  touched this cycle; (c) cuDSS PHASE_REFACTORIZATION P-Delta fixture is
  the cleanest "is this dead or just under-tested" question to settle.
