# v0.4.0.1 — Scenario widget cross-world hotfix (AS-28)

**Sprint:** S-05 patch
**Date:** 2026-06-28
**Repo:** `architect_simulator` (game-body line)
**Baseline tag:** `v0.4.0` (`899fa15`)
**HEAD at tag time:** see tag plan below

> **Headline.** Patch on top of `v0.4.0`. Fixes the **silent cross-world bug**
> in `UArchSimScenarioWidget::PlaceKSetMember` discovered when the v0.4.0
> USER-DRIVEN PIE 5-minute smoke (`docs/logs/S-05/u3_pie_smoke.md`) was finally
> run against a live PIE session: K-set actors spawned into the editor world
> while the `UArchSimModelRegistry` lived in the PIE `GameInstance`,
> producing `invalid model: no nodes` from the solver and a never-spawned
> `AFrameUtilizationHeatmapActor`. Root cause: `PlaceKSetMember` used
> `GEditor->GetEditorWorldContext().World()` unconditionally; the prior
> in-code comment claiming "During PIE this is the PIE world" was
> incorrect for UE5.7. Fix: prefer `GEditor->PlayWorld.Get()` first, mirror
> the world-resolution helper already present in `RequestSolveAndVisualize`
> at the same file. **`v0.4.0` is marked as prerelease** because the PIE
> end-to-end demo flow was broken in `v0.4.0` even though the headless
> 5-leg gate (148 UE tests) was green — the gate has no equivalent of a
> live cross-world PIE flow. `v0.4.0.1` should be considered the actual
> shippable Scenario MVP for the cross-world wire (P3..P9 + P13 PASS in
> live PIE; P10/P11 still FAIL but on a separate fixture-physics issue,
> see §3). Engine source delta = **0** under both FROZEN paths
> (FrameCore `v4.0.0` + LevelSim `v1` both honoured). UE automation test
> count unchanged from `v0.4.0`: **148** (cuDSS) / **146** (non-cuDSS).

---

## 1. What landed

### 1a. AS-28 — Scenario widget cross-world fix

| File | Lines changed | Reason |
|---|---|---|
| `Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp` | +18 / -7 | Two edits in one .cpp: (a) `PlaceKSetMember` Step-1 world resolution now `GEditor->PlayWorld.Get() ? PlayWorld : EditorWorld`, mirroring `RequestSolveAndVisualize` L445-447; (b) move `#include "Editor/ArchSimScenarioWidget.h"` to be the FIRST `#include` directive (UE5.5+ IWYU first-header rule) — the prior order put `FrameCoreUE/...` headers first, which made UBT report a fatal lint error but still mark the build `Result: Succeeded` while silently leaving a stale `.obj` link into the DLL. The IWYU bug is why the cross-world fix appeared not to take effect across two earlier rebuild attempts; once include order was corrected, the obj truly rebuilt and the fix activated |

### 1b. New manual-test infrastructure (PIE smoke automation helpers)

| File | Lines | Purpose |
|---|---|---|
| `Tools/setup_pie_smoke_widget.py` | +118 | Commandlet (run via `UnrealEditor-Cmd.exe -run=PythonScript`) that creates `/Game/BP_ArchSimScenarioWidget.uasset` — a BP child class of the abstract `UArchSimScenarioWidget` C++ class, required for instantiation in PIE. Uses `EditorUtilityWidgetBlueprintFactory` + `unreal.AssetToolsHelpers.get_asset_tools().create_asset()`. One-shot setup; the asset itself lives under `Content/` (gitignored per project convention) |
| `Tools/run_pie_smoke.py` | +163 | Python script pasted into the Editor's Output Log Python console (mode: Python) while PIE is active. Drives the full PIE smoke flow programmatically: `spawn_and_register_tab` → `advance_tutorial_step` (×4) → `place_k1_column / place_k2_beam / place_k4_brace` → `request_solve_and_visualize` → 1.5s wait → checks `heatmap_actor` UPROPERTY → `reset_widget_state`. Reports PASS / FAIL for the 12 runtime-observable criteria of u3_pie_smoke.md (P3..P14 minus the 4 visual-only criteria P11/P12/P15/P16). The script substitutes for "Open the BP editor, lay out 6 UMG buttons, wire 6 OnClicked handlers" — saves the user ~30 minutes of one-time wiring per Editor session |

Both scripts honour iron rule #5 (no protected-file modification, no `git add -A`); both committed under their own paths.

---

## 2. Verification matrix

| Leg | What | Status | Reproduction command |
|---|---|---|---|
| 1 | standalone `frametest.exe` (F1..F71) | PASS | `Plugins\FrameSolver\Standalone\build.bat` |
| 2 | UE automation `FrameCore.*` + `ArchSim.*` (148 cuDSS / 146 non-cuDSS) | PASS | `Scripts\run_gate.ps1 -RequireOpenSees` |
| 3 | OpenSees offline cross-validation | PASS | `Tools\opensees_compare.py --relaxed` |
| 4 | linear deep audit (104 checks) | PASS | `Plugins\FrameSolver\Standalone\linear_deep_audit.exe` |
| 5 | CLI round-trip | PASS | `Tools\cli_roundtrip.py` |
| **6** | **USER-DRIVEN PIE 5-minute smoke** (`docs/logs/S-05/u3_pie_smoke.md`) | **P3..P9 + P13 PASS / P10/P11 FAIL (separate fixture issue, see §3)** | Manual PIE + `exec(open(r'E:\project\ArchSim\Tools\run_pie_smoke.py').read())` in Output Log Python console while PIE active |

Engine source delta vs `v0.4.0`: **0 lines** under `Plugins/FrameSolver/Source/FrameCore/` and `Plugins/LevelSim/Source/LevelCore/` — FROZEN paths honoured. Plugin-side binaries (`UnrealEditor-FrameCore.dll`, `UnrealEditor-ArchSim.dll`) rebuilt only on the ArchSim side (DLL mtime 2026-06-28 10:08).

---

## 3. Honest limitations

**PIE smoke P10/P11 FAIL — separately scoped to AS-29, not a v0.4.0.1 regression.**

After the cross-world fix landed and PIE was confirmed active end-to-end, the
PIE smoke run produced:

```
LogArchSim: Display: PlaceKSetMember[K1] — registered at MemberIdx=0, loc=(0.0,0.0,0.0).
LogArchSim: Display: PlaceKSetMember[K2] — registered at MemberIdx=1, loc=(200.0,0.0,0.0).
LogArchSim: Display: PlaceKSetMember[K4] — registered at MemberIdx=2, loc=(100.0,0.0,100.0).
LogArchSim: Display: RequestSolveAndVisualize — Solve requested; awaiting OnSolveComplete delegate.
LogArchSimRegistry: Warning: FlushAndStartSession failed: LDLT factorization failed
  (info != Success): rank-deficient stiffness / mechanism
```

K1/K2/K4 successfully register into the PIE-world Registry (the v0.4.0.1
cross-world fix is doing its job). But the three placed members:

- K1 column at world origin, span ±50 cm along +X — a 100 cm bar in space
- K2 beam at +X 200 cm, span ±100 cm along +X — a 200 cm bar in space
- K4 brace at (+X 100, +Z 100), span ±71 cm along ±X±Z — a 200 cm diagonal in space

share **no nodes** (no two members touch) and the structure has **no boundary
supports** (no `PlaceFixedSupport` API exists yet). All 12 DOF (or 18 DOF if
nodes don't coalesce) are free. The global stiffness matrix is therefore
rank-deficient; LDLT factorization correctly rejects the system as a
"mechanism" (engine behaviour is correct — rejecting an unsolvable model is
correct, not a bug). With no valid `Solution`, `OnSolveComplete` fires with
`bSuccess=false` and `RequestSolveAndVisualize`'s post-solve handler does not
spawn `AFrameUtilizationHeatmapActor` — hence P10/P11 FAIL.

This is a **fixture / API design** problem, not a widget-code bug. The widget
did exactly what it should: place actors, register them, dispatch solve,
receive the solver's verdict, decide not to spawn a heatmap for an
unsolvable model.

**Filed as AS-29** for v0.4.1 / S-06 backlog. First-action sketch:

1. Add `UArchSimScenarioWidget::PlaceFixedSupport(FVector LocationWorld)` BlueprintCallable that registers a `MemberData` with `bIsBoundarySupport = true` (or similar) into the Registry.
2. Add a node-snap pass inside `UArchSimModelRegistry::RegisterMember` so that two members whose endpoints are within a tolerance (~1 cm in PIE space) share a single Registry node index — eliminates the "no shared nodes" half of the mechanism.
3. Add a `UArchSimScenarioWidget::SpawnDefaultPortalFrame()` BlueprintCallable that places K1 (column) at (0,0,0) and (0,0,300), K2 (beam) along the top (0,0,300)→(400,0,300), and a `PlaceFixedSupport` at the column base (0,0,0) — gives the student a known-solvable starter frame on day one.
4. Update `Tools/run_pie_smoke.py` to call `SpawnDefaultPortalFrame()` first, then `RequestSolveAndVisualize()` — P10/P11 will PASS.

Also note: the in-PIE BP UI buttons / voice prompt assets / 5+ student-asset
art assets remain backlog (none are required for the cross-world fix release).

---

## 4. Breaking changes

**None.** Widget public-method signatures unchanged from `v0.4.0`. BP child
class `/Game/BP_ArchSimScenarioWidget` from `v0.4.0` continues to work
(was generated against the same UCLASS layout; only method body of
`PlaceKSetMember` changed). Engine API + wire ABI unchanged
(`Plugins/FrameSolver/Source/FrameCore/` FROZEN since `v4.0.0`).

---

## 5. Deferred items

| Audit ID | Title | Defer to | First action |
|---|---|---|---|
| **AS-28** (this release closes) | Scenario widget cross-world bug fix | — (closed in `v0.4.0.1`) | — |
| **AS-29** | Scenario valid-frame fixture + boundary support API | v0.4.1 / S-06 | See §3 first-action sketch |
| **AS-28-followup** | UE5.5+ IWYU first-header rule enforcement in pre-commit | v0.4.1 / S-06 | Add a `Tools/check_iwyu_first_header.py` that grep-validates every `Source/**/Private/**/*.cpp` puts the matching `.h` as the first `#include` — would have caught this bug locally before two failed rebuild attempts |
| **ALS character mesh null deref** | `AAlsCharacter::RefreshMeshProperties` crashes on PIE with default empty world (workaround during smoke: temporarily set `GlobalDefaultGameMode=/Script/Engine.GameModeBase` in `Config/DefaultEngine.ini`) | v0.4.1 / S-06 | Investigate why `ArchSimCharacter` BP's `Mesh` component is null at `PossessedBy` time; likely missing default `SkeletalMesh` asset assignment OR ALS plugin needs `MovementSettings` UPROPERTY set at construction |
| (carried from v0.4.0) | All v0.4.0 deferred items | v0.4.1 / S-06 | See `docs/HANDOFF_v0.4.0.md` |

---

## 6. Tag plan

```bash
cd E:/project/ArchSim
git add Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp \
        Tools/run_pie_smoke.py \
        Tools/setup_pie_smoke_widget.py \
        docs/RELEASE_v0.4.0.1.md \
        docs/HANDOFF_v0.4.0.1.md \
        docs/ARCHITECTURE_INDEX.md
git commit -m "fix(S-05): AS-28 v0.4.0.1 -- Scenario widget cross-world bug + IWYU rebuild fix"
git tag -a v0.4.0.1 -m "v0.4.0.1 -- Scenario widget cross-world hotfix (AS-28)"
git push origin main
git push origin v0.4.0.1
gh release create v0.4.0.1 \
    --title "v0.4.0.1 -- Scenario widget cross-world hotfix" \
    --notes-file docs/RELEASE_v0.4.0.1.md
gh release edit v0.4.0 --prerelease
```

---

## 7. Lessons added to project memory

- **UE5.5+ IWYU first-header rule is fatal-silent** — UBT reports `error: Expected X.h to be first header included` and continues with `Result: Succeeded`, but the `.obj` is not actually re-built; the stale obj links into the DLL. Two consecutive `Build.bat` runs both reported Succeeded with the bug still present. Detection: source-mtime > obj-mtime is NOT a reliable rebuild-confirm; the only reliable confirm is `grep -E "Expected.*first header"` on the build log. Mitigation: always check build log for IWYU errors before declaring a rebuild done. Long-term: pre-commit hook (AS-28-followup).
- **PIE-state assertion in headless smoke fixtures must be explicit, not name-based** — the smoke runner's P9 PASS label "PIE active + Registry valid" claimed PIE was active while only checking the widget method's return value; the user-visible label misled triage twice. Smoke fixtures should query `unreal.EditorSubsystem` for explicit `is_in_pie()` (or equivalent) before any PASS-with-PIE-claim is printed.
- **Iron rule #5 (no FROZEN-path modification) does NOT cover plugin assets that are project-private (Content/, Saved/)** — the BP child class `Content/BP_ArchSimScenarioWidget.uasset` is allowed and necessary (UCLASS(Abstract) cannot be instantiated directly). When deciding what counts as a "protected file", apply the test: is it under `Plugins/FrameSolver/Source/FrameCore/` or `Plugins/LevelSim/Source/LevelCore/` or named in `.gitignore`/`ArchSim.uproject`? If no, it's fair game.
- **End-to-end PIE smoke is a separate verification leg from the 5-leg headless gate**. The 148-test UE automation suite passes entirely in headless-nullrhi mode; the cross-world bug was invisible there because every test uses `NewObject<UArchSimModelRegistry>(GetTransientPackage())` to construct a Registry directly, side-stepping the GameInstanceSubsystem-vs-PlayWorld interaction that real PIE goes through. Future Scenario features that involve real PIE actor + Registry interactions need a USER-DRIVEN smoke leg analogous to `u3_pie_smoke.md`.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
