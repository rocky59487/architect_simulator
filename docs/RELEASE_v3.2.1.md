# FrameCore v3.2.1 — UE test coverage strengthening + doc grooming patch

**Tag (this release):** `v3.2.1`
**Branch:** `main`
**Date:** 2026-06-22
**Repo:** <https://github.com/rocky59487/architect_simulator>
**Base release tag:** `v3.2.0` at `5e5a68f` (FrameCoreUE thin slice — USTRUCT marshal
+ BP node + Slate panel; engine source frozen at `v3.1.0` bit-identical).

> **v3.2.1 is a patch over v3.2.0.** The FrameCore engine source under
> `Plugins/FrameSolver/Source/FrameCore/` is **bit-identical to v3.2.0** (verified via
> `git diff v3.2.0..HEAD -- Plugins/FrameSolver/Source/FrameCore/` returns empty).
> No API change, no wire-protocol change, no numerical behaviour change. The release
> ships +8 UE automation tests (62 → 70), `bUseUnity = false` for the FrameCoreUE
> module (clean-build safety), 7 doc-grooming fixes across VERIFICATION / ARCHITECTURE
> / HANDOFF / README / FrameCoreUE_QuickStart, three CI / build hygiene fixes, and
> `.gitignore` coverage for `out/` + `framecore-v*.zip`. **Engine source delta vs
> v3.2.0 = 0 lines under FrameCore native module; v3.2.1 delta is 8 new UE test files +
> doc/CI/build hygiene.**
>
> v3.2.1 was driven through the same release-hardening Phase 0..5 pipeline as v3.2.0;
> all 5 reachable gate legs were green on the integrator host before tag (see §3).

## 1. What v3.2.1 ships

### 1.1 +8 FrameCoreUE automation tests (Phase 6 a-h strengthening)

Test surface area on the consumer-side reflection module grew from 2 (v3.2.0 smoke
tests: `BlueprintSmokeTest` + `EditorSmokeTest`) to **10 `FrameCore.UE.*` tests**.
Total UE gate count: **62 → 70** (with cuDSS), **60 → 68** (without). All eight
additions inline-rebuild their fixture (no FrameCore-private header coupling):

| Test (`FrameCore.UE.*`) | Phase | Coverage |
|---|---|---|
| `MarshalSSBeamTest` | 6a | F2 simply-supported beam under UDL — 2-member trace USTRUCT shape, governing member id range, BP `sigmaCompMax` rel<1e-5 vs POD oracle, `governingShellId == -1` for member-only model |
| `MarshalShellPlateTest` | 6a | F69 clamped 2×2 plate (4 shells, n=2, t=10 mm, q=0.01 MPa) — `ShellsTop` + `ShellsBot` 4 layers, Center + 4 Corners, `bIsTopLayer` differentiation, VonMises marshal rel<1e-5, `governingMemberId == -1` for shell-only model |
| `MarshalMultiMemberTest` | 6a | 3-segment cantilever with user-set member IDs 100/200/300 — `MemberId` carries user ID (not array index), `GoverningMemberId == 100` (real ID 0 vs sentinel ambiguity) |
| `EditorTabSpawnerTest` | 6e | Closes v3.2.0 deferred **U-04** — asserts `FGlobalTabmanager::Get()->HasTabSpawner("FrameCoreStressFieldPanel")` post `StartupModule`. Future WorkspaceMenuStructure API drift surfaces as failing test instead of silently-missing menu |
| `RobustnessTest` | 6e | 3 sub-assertions — (a) negative-input contract (`SamplesPerSpan` ∈ {-3, 0, 1} clamp to 11), (b) 20-segment cantilever scaling (`MemberId` preserved 0..19, `governingMemberId = 0`), (c) memory stability (100 repeat calls bit-exact `GlobalMaxFiberSigma`) |
| `ThetaRangeTest` | 6f | Sweeps 40 shell sample points (4 shells × 5 points × 2 layers) on tilted plate fixture, asserts every `ThetaRad` ∈ (-π/2, π/2] (v3.1.0 A-09 audit invariant carry-forward); v3.2.1 audit A-3 HIGH closeout: lower-bound tolerance corrected to reject values *below* `-π/2 + epsTol` (was incorrectly accepting them) |
| `ZeroLoadTest` | 6f | `ComputeCantileverFixture(P=0, L=2000, 11)` verifies all 11 sample sigmas exactly 0, no NaN anywhere, global maxes 0, shell sentinels stay -1 — BP-friendly zero-load contract |
| `AxialColumnTest` | 6 closeout | F4 vertical column (encastre + tip P↓), exercises `refVec(0,0,1)` degeneracy fallback — |N| ≈ P (rel<1e-3), zero bending (Vy/Vz/Mz ≈ 0 at midspan), shell sentinels stay -1 |

`Scripts/run_gate.ps1` `$ExpectedUeTests` default bumped 62 → **70**; non-cuDSS
recommendation 60 → **68**.

### 1.2 `FrameCoreUE.Build.cs` — `bUseUnity = false` (clean-build safety)

v3.2.1 release-hardening Agent D HIGH finding: 7 of the 8 new test files use
anonymous-namespace helpers (`namespace { ... }`), and `bUseUnity = true` would
collapse them into one unity TU on a cold build, shadowing helpers across files. UE
adaptive build only protects in-flight edits — committed files re-enter unity on the
next clean build. Per CLAUDE.md 踩雷 #4 the canonical fix is `bUseUnity = false` for
modules with test surface area.

### 1.3 Doc-grooming sync (11 docs)

The post-v3.2.0 Phase 6 work pushed UE count 62 → 70, but several authoritative docs
were not updated in lock-step. v3.2.1 release-hardening Agents B / E / G all flagged
the same items (cross-confirmation = high signal). v3.2.1 also ran an **extra
cumulative v3.0.0..HEAD sweep** (user-requested: audit must cover entire v3.x history,
not just v3.2.0..HEAD patch scope) which surfaced 5 more stale "V3 STABLE gates"
descriptions in repo-root `README.md` carrying v3.0/v3.1-era numbers. Fixed:

- `Plugins/FrameSolver/README.md:47` — "57 tests" (stale since ≈ v2.10) → "60 tests + FrameCoreUE 10 = 70 total with cuDSS / 68 without"
- `docs/VERIFICATION.md` — 5 sites: §1 5-leg table, §1 guard-rail prose, §1.5 v3.0.0 5-leg run-command, §3 S8 contributing sentence, §1.5 GPU 6th gate suite (c) frametest_cuda fixture range — all `62/60` → `70/68`, `F1..F67` → `F1..F70 + F67/F67s strict`
- `docs/ARCHITECTURE.md` — 2 sites: `Private/Tests` count + `run_gate.ps1` line — `62/60` → `70/68` with v3.2.1 Phase 6 a-h test names
- `docs/HANDOFF_v3.2.0.md:62` — `UE 62/62` reproduce command annotation now notes "(v3.2.0 tag-time; HEAD = 70/70 含 v3.2.1 Phase 6)"
- `docs/FrameCoreUE_QuickStart.md` — title/header `v3.2.0` → `v3.2.x`; added new **Prerequisites** section (UE 5.7 install + `Build.bat ArchSimEditor` + conda env `framecore-direct`) so a new contributor following the QuickStart alone can reproduce 70/70
- `docs/README.md` — release-notes history table extended with v3.0.0 / v3.0.1 / v3.1.0 / v3.2.0 entries (previously orphaned from the navigation)
- **`README.md` (repo root)** — 6 sites across "V3 STABLE gates" contract + repo tree:
  - line 33: "UE 62/62" → "UE 70/70" + v3.2.1 Phase 6 a-h test note
  - line 86: "UE 60/60 with cuDSS, 60/60 without" + "`-ExpectedUeTests 60`" → "70/70 / 68/68 / `-ExpectedUeTests 68`"
  - line 87: same row fixed (above)
  - line 91: "kEngineVer=3.1.0 pinned" → "kEngineVer=3.2.0 pinned per v3.2.0 wire-ABI contract (v3.2.x patches leave kEngineVer unchanged); 23 capabilities advertised"
  - line 94: "frametest_cuda F1..F67 + F67s strict" → "F1..F70 default + F67 smoke + F67s strict"
  - line 339: `$ExpectedUeTests = 62` → 70 + `-ExpectedUeTests 60` → 68 + v3.2.1 Phase 6 a-h delta annotated
  - line 411: repo tree `Private/Tests/*.cpp` "62 UE tests w/ cuDSS, 60 without" → split into `FrameCore/Private/Tests = 60` + `FrameCoreUE/Private/Tests = 10` + total 70/68
- **`Scripts/run_gpu_gate.ps1`** — header comment line 21 "runs F1-F67" → "runs F1..F70 default + F67/F67s (CUDA build adds F67/F67s on top of the default F1..F70 set)"

### 1.3a Cross-v3 audit findings (user-requested cumulative sweep)

After v3.2.1's Phase 2 patch-scope fixes landed, the user requested an additional
audit covering **the entire v3.x history** (`v3.0.0..HEAD`, 15 commits, 63 files,
+6510/-117 lines, FrameCore engine source net +576/-37 from v3.0.1 hardening +
v3.1.0 StressKernel/StressField). The cumulative sweep verified:

- **v3.0.0 / v3.0.1 / v3.1.0 RELEASE notes:** 0 post-tag commits each (frozen — skill
  rule respected).
- **v3.2.0 RELEASE notes:** 1 post-tag commit (the E-4 in-place U-04 flip; documented
  but not retroactively reverted to avoid history rewriting).
- **HANDOFF chain v2.11.1 → v3.1.0 → v3.2.0 → v3.2.1:** all reachable, no broken
  links. v3.0.0 and v3.0.1 intentionally have no HANDOFF (hardening releases that
  carry forward from v2.11.1 without adding new deferred items).
- **Cumulative stale references** in repo-root `README.md` "V3 STABLE gates" contract
  + `docs/VERIFICATION.md` gate suite (c) + `Scripts/run_gpu_gate.ps1` header comment
  carrying v3.0/v3.1-era numbers (UE 60/60, kEngineVer "3.1.0", F1..F67) — all fixed
  in §1.3 above.

### 1.4 CI / build hygiene (3 fixes)

- `.github/workflows/release-gate.yml:10` — header comment `(F1..F66 default)` → `(F1..F70 default)` (was stale by 4 fixtures since F68/F69/F70 added in v3.1.0)
- `.github/workflows/release-gate.yml:174` — gate summary footer referencing `docs/HANDOFF_v2.11.1.md` (two majors old) → `docs/HANDOFF_v3.2.0.md` (and any newer HANDOFF_v*.md in chain)
- `Scripts/run_gate.ps1:59,67,105,116` — section header comments `[1/3]/[2/3]/[3/3]/[4/4]` → `[1/5]/[2/5]/[3/5]/[4/5]` to match the printed leg labels (was cosmetic but misled anyone reading the script to add a leg 6)

### 1.5 `.gitignore` — `out/` + `framecore-v*.zip`

`a04c205` added `dist/`. Sibling release-staging paths (`out/`) and release bundle
artefacts dropped at repo root (`framecore-v*.zip`) were not covered. v3.2.1 closes
that gap. Note: agent-scratch patterns (`.codex/`, `.claude/`, `.cursor/`, `*.output`,
`*.transcript`, `*.jsonl`, `output/`, `_audit*.log`, `_scratch*.log`) were already
covered before v3.2.1.

### 1.6 v3.2.0 deferred — U-04 CLOSED

v3.2.0 shipped six deferred items (U-01..U-06) plus a v3.3-blocked engine-source
BLOCKER (U-07 sentinel). **v3.2.1 closes U-04** via the `EditorTabSpawnerTest`
(Phase 6e). Remaining deferred — see §5.

### 1.7 Version-pin intentionally unchanged

`kEngineVer` stays at `"3.2.0"` in `Plugins/FrameSolver/Standalone/v2/Dispatcher.h`.
The `.uplugin VersionName` stays at `"3.2.0"`. `FRAMECORE_EXPECTED_ENGINE_VER` stays
at `'3.2.0'` in `Scripts/run_gpu_gate.ps1` + `.github/workflows/release-gate.yml`.

**Rationale:** v3.2.1 changes zero engine source, zero wire protocol, zero capability
list. Clients pinned to `kEngineVer=3.2.0` see bit-identical behaviour. Bumping any
of these pins to "3.2.1" would force a regenerated CI cache + dispatcher rebuild
without any observable behavioural reason. This follows the v2.7-style discipline
(test surface area + CI only → patch tag only) rather than v3.0.1-style (which DID
bump pins because of post-tag CI / fingerprint additions). v3.2.1 is the former.

## 2. What stayed bit-identical

- **Engine source under `Plugins/FrameSolver/Source/FrameCore/`** — `git diff v3.2.0..HEAD -- Plugins/FrameSolver/Source/FrameCore/`
  returns nothing. Standalone F1..F70 + UE existing 62 tests + OpenSees + linear deep
  audit 104 + CLI roundtrip all bit-identical to v3.2.0.
- **Public ABI** — `FRAMECORE_API` exports unchanged. `FRAMECOREUE_API` is the v3.2.0
  symbol set; no signature drift.
- **Wire ABI** — `kAbiVersion = 2` unchanged. v2 dispatcher capability list unchanged
  (23 verbs including `inspect.stress_field` from v3.1.0). `kEngineVer` stays at
  `"3.2.0"` (see §1.7).
- **Default-build cross-vendor** — non-CUDA builds (`FRAMECORE_CUDA=0`) and FrameCore
  native module are bit-identical to v3.2.0; only the FrameCoreUE module gains 8 new
  test `.cpp` files (test-only, not linked into runtime).
- **CUDA lane** — v3.2.1 source delta in CUDA path = 0 lines. The v2.11.0 baseline
  r2_bench 90k margin (+11.94 ms over the 16.67 ms 60-fps budget) carries forward
  bit-identical.

## 3. Reproduction matrix (v3.2.1 source on integrator host, 2026-06-22)

| Leg | Cmd | Result | Notes |
|---|---|---|---|
| 1. Standalone F1..F70 | `Plugins\FrameSolver\Standalone\build.bat` then `frametest.exe` | **ALL PASS (failures=0)** | F1..F70 fixtures bit-identical vs v3.2.0; no new standalone fixture (v3.2.1 is UE-only test additions) |
| 2. UE automation 70/70 | `Build.bat ArchSimEditor Win64 Development` then `Scripts\run_gate.ps1 -RequireOpenSees` | **ALL PASS** | `-ExpectedUeTests 70` default; 8 Phase 6 tests joined the existing 62; non-cuDSS box use `-ExpectedUeTests 68` |
| 3. OpenSees strict | `python Tools\opensees_compare.py` | **OPENSEES GATE: PASS** | unchanged vs v3.2.0 (engine source unchanged) |
| 4. Deep audit | `Plugins\FrameSolver\Standalone\build_linear_audit.bat` then `linear_deep_audit.exe` | **PASS failures=0 checks=104** | unchanged vs v3.2.0 |
| 5. CLI round-trip | `Tools\cli_roundtrip.py` (after `build_cli.bat`) | **ALL PASS (failures=0)** | 13 checks; unchanged vs v3.2.0 |
| 6a. v2_roundtrip (CPU) | `build_capi_v2.bat` + `FRAMECORE_EXPECTED_ENGINE_VER=3.2.0 python Tools\v2_roundtrip.py` | **=== summary: ALL PASS ===** | `kEngineVer=3.2.0` pin enforced (unchanged); capability list unchanged |
| 6b. v2_roundtrip (CUDA) | `Scripts\run_gpu_gate.ps1 -Strict` | **NOT RUN** (this session) | reachable when cuDSS DLL on PATH; see §3a |
| 7a. F1..F67 + F67s strict | `Scripts\run_gpu_gate.ps1 -Strict` leg 1/3 | **NOT RUN** (this session) | reachable when cuDSS DLL on PATH; see §3a |
| 7b. r2_bench --gpu 90k | `Scripts\run_gpu_gate.ps1 -Strict` leg 3/3 | **NOT RUN** (this session) | reachable when cuDSS DLL on PATH; see §3a |

§3a footnote — **NOT RUN legs (CUDA / `-Strict` mode)**: v3.2.1 source delta in CUDA
lane = 0 lines (FrameCoreUE is UE-only; `SnSession` / cuDSS Impl unchanged from v3.2.0
which was unchanged from v3.1.0 which was unchanged from v3.0.1). The r2_bench 90k
margin from v3.0.0/v3.1.0 (`+11.94 ms` over the 16.67 ms 60-fps budget, v2.11.0
baseline still in effect) carries forward bit-identical. To reproduce all 9/9 V3
STABLE legs:

```powershell
Scripts\run_gpu_gate.ps1 -Strict
```

With `cudss64_0.dll` on PATH the `-Strict` flag enforces the `STRICT_EXECUTED`
fingerprint on F67s + the UE `FFrameCoreGpuBacksubStrictTest`, and pins
`FRAMECORE_EXPECTED_ENGINE_VER='3.2.0'` (unchanged — see §1.7).

## 4. Tag plan

```bash
git add -- \
  .github/workflows/release-gate.yml \
  .gitignore \
  Plugins/FrameSolver/README.md \
  Plugins/FrameSolver/Source/FrameCoreUE/FrameCoreUE.Build.cs \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEAxialColumnTest.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEEditorTabSpawnerTest.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEMarshalMultiMemberTest.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEMarshalSSBeamTest.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEMarshalShellPlateTest.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUERobustnessTest.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEThetaRangeTest.cpp \
  Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEZeroLoadTest.cpp \
  Plugins/FrameSolver/Standalone/build_capi_v2.bat \
  Scripts/run_gate.ps1 \
  docs/ARCHITECTURE.md \
  docs/FrameCoreUE_QuickStart.md \
  docs/HANDOFF_v3.2.0.md \
  docs/HANDOFF_v3.2.1.md \
  docs/NIGHT_SHIFT_2026-06-22.md \
  docs/README.md \
  docs/RELEASE_v3.2.1.md \
  docs/VERIFICATION.md \
  docs/specs/S5_S11_skeletons.md

git commit -m "release: v3.2.1 -- UE test coverage 62->70 + doc grooming + Build.cs unity-off"
git tag -a v3.2.1 -m "v3.2.1 -- patch: +8 UE tests, doc sync, clean-build safety"
git push origin main
git push origin v3.2.1

gh release create v3.2.1 \
  --title "v3.2.1 -- UE test coverage 62->70 + clean-build safety + doc grooming" \
  --notes-file docs/RELEASE_v3.2.1.md \
  --latest
```

## 5. Deferred

### 5.1 v3.2.0 deferred (carry-forward, minus the now-closed U-04)

- **U-01** — BP "load JSON model" entrypoint (still deferred to v3.3 / v3.4)
- **U-02** — Slate fixture dropdown (cantilever / plate / cross / truss)
- **U-03** — Real renderer (spline mesh / Niagara / colour-band shader)
- **U-04 — CLOSED in v3.2.1 Phase 6e** (`FFrameCoreUEEditorTabSpawnerTest`)
- **U-05** — `float`-only USTRUCT precision for very-small load cases
- **U-06** — UE 5.7 + VS2026 "not preferred version" build warning
- **U-07** — `governingMemberId` / `governingShellId` sentinel mismatch (engine 0 vs USTRUCT -1) — requires engine source change (rule #1 protected), still v3.3 work
- All v3.1.0 carry-forwards continue: A-13 F71 +Z, D-05 v1 CLI STRESS, E-07 v2 inspect protocol spec, E-13 S11 naming, C-12 cancel poll, F-02 findUdl hash map, F-03 clamps invariant doc; plus the v3.0.1 carry-forwards (A-02 CUDA RAII, A-05/F-14 OpenMP, etc).

See [`docs/HANDOFF_v3.1.0.md` §3](HANDOFF_v3.1.0.md) and [`docs/HANDOFF_v3.2.0.md` §3](HANDOFF_v3.2.0.md) for first-actions on each.

### 5.2 v3.2.1 newly deferred (from this release-hardening audit)

Six items surfaced during Phase 1 (7-agent audit) were deferred — either because the
fix exceeds the < 30 LOC + oracle-backed bound for patch scope, requires a UE rebuild
matrix re-verification, or sits behind rule #1 (engine source frozen). Each is
documented with a first-action sketch in [`docs/HANDOFF_v3.2.1.md` §3.2](HANDOFF_v3.2.1.md).

- **V321-01 (A-1/A-2)** — `MarshalSSBeamTest` add analytic Vy oracle + tighten governingMemberId range check (currently asserts `>= 0` not `0 OR 1`). Strengthens self-referential float-cast check with closed-form `w·L/4` shear.
- **V321-02 (A-4)** — `RobustnessTest` per-sample bit-exact compare over 100 repeats (currently only `GlobalMaxFiberSigma` + counts). Extends to compare `Members[0].Samples[5]` N/Vy/Mz against iteration 0.
- **V321-03 (A-5)** — `AxialColumnTest` add `N > 0` sign assertion (compression-positive, consistent with F4 standalone).
- **V321-04 (A-8)** — `EditorTabSpawnerTest` add explicit `FModuleManager::LoadModuleChecked<>("FrameCoreUE")` pre-`HasTabSpawner` guard to avoid lazy-init false-negative.
- **V321-05 (F-4)** — Extract `Tests/FrameCoreUETestHelpers.h` with shared `FrameCoreUE::ToBlueprint` forward declaration (currently copy-pasted in 6 test files). Saves 18 lines aggregate but requires touching 6 files + verifying all UE tests still green.
- **V321-06 (E-9)** — `Dispatcher.h:44,145` `model.patch — schema TBD` carryforward open since v2.4; needs design decision (write spec or remove).

## 6. Honest limitations (carry-forward from v3.2.0)

All §6 honest limitations from `docs/RELEASE_v3.2.0.md` carry forward unchanged with
one exception: the v3.2.0 §6 bullet "Editor smoke test does NOT cover the nomad tab
spawner" is **superseded by v3.2.1 Phase 6e `EditorTabSpawnerTest`** (U-04 CLOSED).
The remaining four bullets — reflection-only, float lossy cast per-value, no packaged
build smoke test, no renderer — still apply.

## 7. Breaking changes

None at the public ABI level. `kEngineVer` intentionally unchanged from `"3.2.0"`
(see §1.7). `.uplugin VersionName` unchanged. Clients pinned to `3.2.0` see
bit-identical engine behaviour and can adopt v3.2.1 without code changes.

`FrameCoreUE` test count grew from 2 → 10. Anyone running `run_gate.ps1` against
v3.2.1 source MUST rebuild the UE editor (`Build.bat ArchSimEditor Win64 Development`)
before running the gate, otherwise the count guard short-falls (expected 70, found 62).
This is the intended behaviour of the `$ExpectedUeTests` rail and is consistent with
the v3.2.0 / v3.1.0 / v2.11.x pattern.
