# Handoff — v0.4.0.1 → v0.4.1 / S-06

**From:** `v0.4.0.1` (`<tag-time SHA>` — see `git log --oneline -3`)
**To:** next session owner (any Claude / human)
**Date:** 2026-06-28
**Prior handoff:** [`docs/HANDOFF_v0.4.0.md`](HANDOFF_v0.4.0.md) (still authoritative for v0.4.0 era — read both)

---

## Z-01 first action on day 1

**Verify the cross-world fix is still active in your local DLL.**

```bash
cd E:/project/ArchSim
git log --oneline -3                            # should show v0.4.0.1 patch commit
grep -n "v0.4.0.1 fix" Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp
# Expect line ~182: WHY-comment marker
grep -n "GEditor->PlayWorld.Get()" Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp
# Expect lines 198, 199, 452, 453 (Place + Solve both prefer PlayWorld)

# Rebuild ArchSim only (~20 s if no other source changed):
"E:/project/UE_5.7/Engine/Build/BatchFiles/Build.bat" ArchSimEditor Win64 Development \
    -project="E:/project/ArchSim/ArchSim.uproject" -waitmutex

# Build log MUST NOT contain IWYU error — grep it:
grep -E "Expected.*first header" $LASTBUILDLOG    # expect 0 matches

# Then 5-leg gate:
powershell -ExecutionPolicy Bypass -File Scripts/run_gate.ps1 -RequireOpenSees
# Expect: "GATE PASS" — 148 UE tests cuDSS / 146 non-cuDSS
```

If any of these grep / build / gate steps fail, STOP and investigate before
starting AS-29 work — the v0.4.0.1 fix may have been clobbered by an
uncommitted rebase / merge / WIP.

---

## What v0.4.0.1 closes

- **AS-28** (Scenario widget cross-world bug) — `PlaceKSetMember` now prefers `GEditor->PlayWorld` first, matching `RequestSolveAndVisualize`. Verified live PIE: K1/K2/K4 register at MemberIdx=0/1/2 in the PIE-world Registry (was previously "Registry not available (edit-mode, no PIE)" with stale-obj bug masking the prior fix attempt).
- IWYU first-header rule violation in widget cpp — the silent-stale-obj root cause that defeated two earlier rebuild attempts.

## What v0.4.0.1 does NOT close (carry forward)

| AS-XX | Status | Notes |
|---|---|---|
| **AS-29** | **NEW backlog from v0.4.0.1** | Scenario valid-frame fixture + boundary support API. K1/K2/K4 placed at default smoke coordinates produce a 12-DOF-free mechanism → LDLT rejects model → no HeatmapActor → P10/P11 of u3_pie_smoke.md FAIL. Widget needs `PlaceFixedSupport`, node-snap logic, and `SpawnDefaultPortalFrame`. See `docs/RELEASE_v0.4.0.1.md` §3 for full first-action sketch |
| **AS-28-followup** | NEW backlog | Pre-commit IWYU first-header validator script (`Tools/check_iwyu_first_header.py`) — would have caught the v0.4.0.1 bug class before two failed rebuilds |
| **ALS character mesh null deref** | NEW backlog | `AAlsCharacter::RefreshMeshProperties` crashes on PIE with default OpenWorld template + `ArchSimGameMode`. Workaround in `v0.4.0` PIE smoke: temporarily set `GlobalDefaultGameMode=/Script/Engine.GameModeBase` in `Config/DefaultEngine.ini`. **REVERTED in `v0.4.0.1` release** (ini back to ArchSimGameMode). Real fix: investigate ArchSimCharacter BP mesh assignment OR ALS plugin MovementSettings UPROPERTY default |
| (all v0.4.0 deferred) | carry from prior handoff | See `docs/HANDOFF_v0.4.0.md` for full inventory |

---

## Tips earned during v0.4.0.1 (durable)

1. **Build "Result: Succeeded" ≠ obj rebuilt.** UE5.5+ IWYU first-header rule is fatal-silent — UBT reports the error in the log AND the obj stays stale AND the link uses the stale obj AND the whole target exits 0. Two rebuild rounds went by with the fix invisible. Always grep the build log for `Expected.*first header` before declaring a rebuild done. Alternative: temporarily change a string literal in your fix, rebuild, run, see if the new string appears in the log — if not, obj is stale.

2. **PIE smoke labels must claim PIE-state explicitly, not implicitly.** The `run_pie_smoke.py` P9 label said "(PIE active + Registry valid)" — but only checked the widget method's return value. PIE-state PASS labels should query `unreal.EditorSubsystem` / `unreal.SystemLibrary.is_game_world` for explicit PIE confirmation, not infer it from a method return.

3. **PIE auto-stops on Esc / Stop button / sometimes Alt+Tab.** The smoke flow requires Alt+P → immediately switch to Output Log via mouse-click (NOT keyboard) → paste exec line → Enter. Any keyboard activity while PIE focus is "auto-captured" risks Esc-binding triggering Stop. With `GameModeBase` override, no player controller exists so mouse isn't captured; with `ArchSimGameMode` (post-ALS-fix), mouse capture may return — re-evaluate.

4. **Editor Utility Widget UCLASS(Abstract) requires a BP child class.** You cannot `spawn_and_register_tab` directly on the C++ class. `Tools/setup_pie_smoke_widget.py` creates the BP child programmatically via `EditorUtilityWidgetBlueprintFactory` + `AssetTools.create_asset`. Asset lives at `/Game/BP_ArchSimScenarioWidget` (gitignored under `Content/`).

5. **Headless 148-UE-test gate cannot catch cross-world bugs.** All ScenarioWidgetTest / ScenarioSolveWireTest / ScenarioTutorialTest sub-checks construct the Registry via `NewObject<UArchSimModelRegistry>(GetTransientPackage())`, completely sidestepping the GameInstanceSubsystem-vs-PlayWorld interaction. Real PIE smoke is a SEPARATE verification leg — keep `docs/logs/S-05/u3_pie_smoke.md` (and any future Scenario-feature equivalents) as USER-DRIVEN gates that must be re-run after every Scenario-facing change.

---

## Repository state at v0.4.0.1 tag time

- `git diff v0.4.0..v0.4.0.1 --stat` should show ~3 files: widget .cpp, run_pie_smoke.py (new), setup_pie_smoke_widget.py (new) — plus the two doc files in this directory and ARCH_INDEX latest-tag bump.
- FROZEN guards honoured: 0 lines under `Plugins/FrameSolver/Source/FrameCore/` and 0 lines under `Plugins/LevelSim/Source/LevelCore/`.
- `Config/DefaultEngine.ini` reverted to `GlobalDefaultGameMode=/Script/ArchSim.ArchSimGameMode` (no temporary override).
- UE test count unchanged: 148 (cuDSS) / 146 (non-cuDSS).
- `v0.4.0` GitHub release marked `--prerelease` because its claimed PIE end-to-end demo flow was broken (cross-world bug).

🤖 Generated with [Claude Code](https://claude.com/claude-code)
