# FrameCore v3.0.1 — post-v3.0.0 release-hardening patch (6 findings, 9/9 verified)

**Tag (this release):** `v3.0.1`
**Branch:** `main`
**Date:** 2026-06-21
**Repo:** <https://github.com/rocky59487/architect_simulator>
**Base release tag:** `v3.0.0` at `b57cf99` (which collapsed v2.11.1 + the 5 v2.11.1-RC
follow-up items + the 7-agent audit fixes into the V3 STABLE anchor)

> **v3.0.1 closes 6 consistency findings the user raised against the just-shipped v3.0.0
> tag** (2 BLOCKER, 2 HIGH, 2 MEDIUM). The fixes are surgical (no engine numerics
> touched, no public API broken), the same 9/9 gate suites stay green, and the release
> ships with a `.github/workflows/release-gate.yml` that re-runs the CPU-only legs on
> every push to `main` plus the actual gate logs from this session in
> `docs/gate-logs/v3.0.1/` as audit evidence.

## 1. The 6 findings v3.0.1 closes

### 1.1 BLOCKER 1 — version surface inconsistent with v3.0.0 tag

v3.0.0 shipped but the runtime / dispatcher / UE plugin still reported `2.11.1`:

| Surface | Pre-v3.0.1 | v3.0.1 | File |
|---|---|---|---|
| Dispatcher `kEngineVer` | `"2.11.1"` | **`"3.0.1"`** | `Plugins/FrameSolver/Standalone/v2/Dispatcher.h:79` |
| `FrameSolver.uplugin` `VersionName` | `"2.11.1"` | **`"3.0.1"`** | `Plugins/FrameSolver/FrameSolver.uplugin` |
| `FrameSolver.uplugin` `Version` (int) | `26` | **`28`** | same |
| `FrameSolver.uplugin` `IsBetaVersion` | `true` | **`false`** | same |
| `run_gpu_gate.ps1` `FRAMECORE_EXPECTED_ENGINE_VER` pin | `'2.11.1'` | **`'3.0.1'`** | `Scripts/run_gpu_gate.ps1:232` |

The v2.11.1-era `v2_roundtrip.py` already pinned via env var (no hard-coded literal),
so it picks up the new pin automatically.

### 1.2 BLOCKER 2 — strict GPU test could SKIP and still PASS

`FFrameCoreGpuBacksubStrictTest` and standalone F67s both treat
`FRAMECORE_GPU_STRICT != "1"` as a clean SKIP (PASS, by design for dev-box compile
tests). But `run_gate.ps1` had no way to enforce that the strict branch actually ran
under CI-strict mode — a silent SKIP under `-Strict` would still show 9/9 green.

v3.0.1 fix: both tests now emit a literal fingerprint string in their executed and
skipped paths:

| Path | Fingerprint emitted |
|---|---|
| Standalone F67s strict-executed | `[F67s] STRICT_EXECUTED gpu_attach=cuDSS env=FRAMECORE_GPU_STRICT=1` |
| Standalone F67s strict-skipped | `[F67s] STRICT_SKIPPED reason=env_unset_or_not_1` |
| UE strict-executed | `[F67s_UE] STRICT_EXECUTED gpu_attach=cuDSS env=FRAMECORE_GPU_STRICT=1` |
| UE strict-skipped | `[F67s_UE] STRICT_SKIPPED reason=env_unset_or_not_1` |

`run_gpu_gate.ps1` greps frametest_cuda stdout for the EXECUTED fingerprint when
`FRAMECORE_GPU_STRICT=1` is set; missing fingerprint OR present SKIPPED fingerprint
under strict mode → gate FAIL. `run_gate.ps1` does the same grep against
`Saved/Logs/ArchSim.log` for the UE-side fingerprint when strict mode is on.

### 1.3 HIGH 1 — r2_bench --gpu 90k was a soft warning, not a hard gate

The bench's own exit code only enforces "margin ≥ 0 vs the 16.67 ms / frame 60-fps
budget." A GPU lane that silently regresses to ~12 ms (still inside the 60-fps budget
but 2.5× the v2.11.0 baseline of ~4.7 ms / frame) would PASS the gate.

v3.0.1 fix: `run_gpu_gate.ps1` parses the `margin=+X.XXX ms` line from the bench
output and enforces `margin ≥ +8.0 ms` (baseline +11.94 ms; 8.0 ms is the 2×-baseline-
frame-time alarm point). Below the threshold the gate FAILs with an explicit message
naming the baseline. The threshold sits in `run_gpu_gate.ps1` as a named constant so
future baseline shifts only require one edit.

### 1.4 HIGH 2 — no CI workflow / no archived gate logs

v3.0.0 shipped 9/9 green but the evidence was only in the docs (HANDOFF / RELEASE).
No GitHub Actions run, no archived gate logs, no externally verifiable record.

v3.0.1 fix:

- `.github/workflows/release-gate.yml` runs on every push to `main`, every tag, every
  PR, and on `workflow_dispatch`. It runs the CPU-only legs (standalone F1..F66 +
  linear deep audit 104 + CLI 13 + v2_roundtrip CPU + OpenSees opt-in via the
  framecore-direct conda env) on `windows-latest` with miniconda + MSVC, and uploads
  each leg's stdout as the `gate-logs-<sha>` artifact with 90-day retention.
- The GPU + UE legs require a self-hosted runner with cuDSS + UE 5.7 and stay run on
  the integrator's box; their logs from the v3.0.1 session are archived to
  `docs/gate-logs/v3.0.1/` so anyone reading the repo gets reproducible evidence.

### 1.5 MEDIUM 1 — UE `FrameCore.Build.cs` SUPERNODAL_CONDA semantics inconsistent

PS1 `Resolve-SupernodalConda` and bat `:normalize_conda_ss` both accept
`SUPERNODAL_CONDA` as either the env root OR the `\Library`-suffixed form. UE
`FrameCore.Build.cs` only ever assumed `\Library` — so a developer who set
`SUPERNODAL_CONDA=<env-root>` directly (e.g. for the `Resolve-SupernodalConda` probe)
saw the bat work but the UE build silently miss OpenBLAS / METIS.

v3.0.1 fix: `FrameCore.Build.cs` now normalises `SUPERNODAL_CONDA` the same way the
bat does — strip a trailing `\Library` or `/Library`, then re-append `\Library` so all
downstream `Path.Combine(condaSS, ...)` calls hit the right tree. The `cuDSS` block
(which wanted the env root, not the `\Library` form) already strips correctly per the
v2.11.1 D-01 audit; the supernodal block now matches.

### 1.6 MEDIUM 2 — docs mixed v2.11.1-RC + v3.0.0 STABLE narrative

After v3.0.0 shipped, the README / VERIFICATION / HANDOFF docs still carried RC-era
phrases ("v2.11.1-RC adds five hardening items", "→ V3 STABLE candidate",
"the four NOT-RUN legs") that made future maintainers parse the release as still
in-flight.

v3.0.1 fix: README status block, the V3 STABLE conditions section, VERIFICATION §1.5,
HANDOFF_v2.11.1.md lede now all read as "v3.0.0 STABLE shipped; v3.0.1 is the patch
on top." Inline historical references (`"rebuilt v2.11.1-RC"` in evidence cells)
are preserved as time-stamped audit trail.

## 2. What stayed bit-identical

- **Engine numerics:** zero source delta in
  `Plugins/FrameSolver/Source/FrameCore/Private/*.{cpp,h}` (compare:
  `git diff v3.0.0..v3.0.1 -- 'Plugins/FrameSolver/Source/FrameCore/**/*.cpp' '...*.h'`
  excludes the `Tests/` subtree, where `GpuBacksubTest.cpp` got the strict
  fingerprint emit).
- **Public API:** unchanged.
- **Wire ABI:** `kAbiVersion = 2` unchanged. `kEngineVer` bump 2.11.1→3.0.1 is the
  only protocol-visible change.
- **Default-build cross-vendor:** non-CUDA builds (`FRAMECORE_CUDA=0`) compile the
  strict variants out via the existing `#if FRAMECORE_CUDA` guard; behaviour matches
  v3.0.0 bit-for-bit.

## 3. Reproduction matrix (v3.0.1 source on integrator host)

| Leg | Cmd | Result | Notes |
|---|---|---|---|
| 1. Standalone F1..F66 | `Plugins\FrameSolver\Standalone\build.bat` | **ALL PASS (failures=0)** | rebuilt v3.0.1 |
| 2. UE automation 59/59 | `…\Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development …` then `Scripts\run_gate.ps1 -RequireOpenSees` | **ALL PASS** (incremental rebuild ~60 s; STRICT_EXECUTED fingerprint verified in `Saved/Logs/ArchSim.log`) | strict mode enforced |
| 3. OpenSees strict | `python Tools\opensees_compare.py` | **PASS** | matches OpenSees + analytic |
| 4. Deep audit | `Plugins\FrameSolver\Standalone\linear_deep_audit.exe` | **PASS failures=0 checks=104** | rebuilt v3.0.1 |
| 5. CLI round-trip | `python Tools\cli_roundtrip.py` | **ALL PASS (failures=0)** | 13 checks |
| 6a. v2_roundtrip (CPU) | `build_capi_v2.bat` + `python Tools\v2_roundtrip.py` | **ALL PASS** | `kEngineVer=3.0.1` pin enforced |
| 6b. v2_roundtrip (CUDA) | `Scripts\run_gpu_gate.ps1 -Strict` leg 2/3 | **ALL PASS** | `gpu_backsub` cap advertised |
| 7a. Standalone F1..F67 + F67s strict | `Scripts\run_gpu_gate.ps1 -Strict` leg 1/3 | **ALL PASS** | STRICT_EXECUTED fingerprint observed, regression-hard r2_bench gate passed |
| 7b. r2_bench --gpu 90k (regression hard gate) | `Scripts\run_gpu_gate.ps1 -Strict` leg 3/3 | **PASS margin ≥ +8 ms threshold** | actual margin reported in `docs/gate-logs/v3.0.1/` |

`docs/gate-logs/v3.0.1/` contains the literal stdout from each of these legs as run
on the integrator's host (`b57cf99..v3.0.1`).

## 4. Tag plan

```bash
git add -- \
  Plugins/FrameSolver/FrameSolver.uplugin \
  Plugins/FrameSolver/Source/FrameCore/FrameCore.Build.cs \
  Plugins/FrameSolver/Source/FrameCore/Private/Tests/GpuBacksubTest.cpp \
  Plugins/FrameSolver/Standalone/main.cpp \
  Plugins/FrameSolver/Standalone/v2/Dispatcher.h \
  README.md \
  Scripts/run_gate.ps1 \
  Scripts/run_gpu_gate.ps1 \
  docs/HANDOFF_v2.11.1.md \
  docs/RELEASE_v3.0.1.md \
  docs/VERIFICATION.md \
  .github/workflows/release-gate.yml \
  docs/gate-logs/v3.0.1/

git commit -m "release: v3.0.1 -- post-v3.0.0 hardening (version sync + strict fingerprint + regression gate + CI workflow)"
git tag -a v3.0.1 -m "v3.0.1 -- v3.0.0 STABLE + 6 post-release findings closed"
git push origin main
git push origin v3.0.1

gh release create v3.0.1 \
  --title "v3.0.1 -- post-v3.0.0 hardening (version sync + strict fingerprint + regression gate + CI workflow)" \
  --notes-file docs/RELEASE_v3.0.1.md \
  --latest
```

## 5. Deferred (carried over from v3.0.0)

All v2.11.1 / v3.0.0 deferred items (A-02 CUDA RAII, A-05/F-14 OpenMP heuristic,
A-12/D-2 cuDSS PHASE_REFACTORIZATION P-Delta revisit, C-01 pinned host memory,
C-06 UDL + parallelRhs+GPU fixtures, C-07 DynamicCollapse GPU limitation doc,
D-02/D-03 UE bCudaEnabled flag + packaging recipe, D-08 gpuRelInf rename,
D-10/D-11/F-16, E-07/E-08, F-02/F-03/F-04/F-10, F-08, D-06 r2_bench `--baseline`
flag) carry forward to v3.1.0 / v3.0.2 unchanged. See
[`docs/HANDOFF_v2.11.1.md` §3](HANDOFF_v2.11.1.md#3-first-action-on-day-1-deferred-items-traceable-to-audit-ids)
for the first-action sketches.
