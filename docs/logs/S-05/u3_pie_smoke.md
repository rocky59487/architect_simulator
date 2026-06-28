# v0.4.0 PIE 5-Minute Student Trial Smoke Gate

> **Gate ID:** u3_pie_smoke  
> **Sprint:** S-05 / SPIKE-Scenario-u3  
> **Version:** v0.4.0 Path A Scenario MVP FINAL  
> **Status:** USER-DRIVEN — this document is the instructions; Claude cannot run PIE.  
> **Headless coverage:** `ArchSim.Gameplay.ScenarioTutorial` (8 sub-checks) + prior u1/u2 tests.  
> **This doc covers:** the PIE runtime that headless cannot reach.

---

## §1 Pre-requisites

Before starting the smoke run, verify all of the following:

1. **UE Build complete** — run the build command and confirm exit 0:
   ```powershell
   & "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" `
       ArchSimEditor Win64 Development `
       -project="E:\project\ArchSim\ArchSim.uproject" -waitmutex
   ```
   Expected: `BUILD SUCCEEDED` or `0 error(s)` in output, process exit code 0.

2. **Headless gate green** — run the full 5-leg gate and confirm `GATE: PASS`:
   ```powershell
   .\Scripts\run_gate.ps1 -RequireOpenSees
   ```
   Expected: `[2/5] UE headless automation: 148/148 PASS` (cuDSS) or `146/146 PASS` (non-cuDSS).
   If gate fails, do NOT proceed with PIE smoke — fix the headless failures first.

3. **BP child class exists** — you must have a Blueprint child of `UArchSimScenarioWidget`
   in the Content Browser. If not created yet:
   - In the UE Content Browser, right-click → Blueprint Class.
   - Search for `ArchSimScenarioWidget` in the parent class picker.
   - Name it `BP_ArchSimScenarioWidget` (or any name you prefer).
   - Save the asset.
   - *(Optional but recommended for tutorial overlay)* Add a Canvas Panel + Text Block
     named `TutorialPromptText` and wire `OnTutorialStateChanged` event to update it.
     `OnVoicePromptShouldPlay` can be left empty (text-only is acceptable).

4. **Default level open** — open any level in the UE Editor (the default ALS starter level
   or an empty level both work). The widget places actors relative to the Editor world origin.

---

## §2 Step 1 — Open the Editor Utility Widget

1. In the UE Editor menu bar: **Window → Editor Utility Widgets**.
2. Double-click `BP_ArchSimScenarioWidget` (or your child class name) in the list.
3. The widget panel opens. You should see (if BP overlay is wired):
   - Tutorial prompt text showing **"Welcome! You are about to test structural ideas."**
   - Buttons for Place Column / Place Beam / Place Brace / Test Structure (as wired in BP graph).

**Expected result:** widget opens without crash, log shows no `Error` or `Fatal` lines in
`Saved/Logs/ArchSim.log`.

---

## §3 Step 2 — Enter PIE

1. Click the **Play** button in the UE Editor toolbar (or press **Alt+P**).
2. Wait for the PIE world to load (ALS pawn spawns, viewport activates).
3. The UE log should show:
   - `UArchSimModelRegistry::Initialize` (Registry ready).
   - `UFrameInteractiveSubsystem::StartSession` (engine solver ready).

**Expected result:** PIE world running, ALS pawn controllable, no crash.

**If PIE fails to launch:** check `Saved/Logs/ArchSim.log` for the error. Common causes:
- ALS plugin missing assets → re-download ALS-Refactored content.
- FrameSolver DLL not compiled → re-run the UE Build step.

---

## §4 Step 3 — Tutorial K1→K2→K4 Placement Loop + Test Structure + Heatmap Observe

While **PIE is active** and the widget is open:

### 3a. Advance Tutorial to PromptPlaceK1
1. In the widget, press **Next** (or call `AdvanceTutorialStep` from the BP button).
2. Tutorial overlay should show: **"Place a K1 Column..."**
3. BP `OnTutorialStateChanged` event fires (state = PromptPlaceK1).

### 3b. Place K1 Column
1. In the widget, input a world-space XYZ location (e.g., (0, 0, 0)) OR click a spot
   in the viewport if your BP wired a click-to-place handler.
2. Press **Place Column** button (calls `PlaceK1Column`).
3. **Expected:** A new Actor appears in the PIE viewport. Log shows:
   ```
   UArchSimScenarioWidget::PlaceKSetMember[K1] — registered at MemberIdx=0
   ```

### 3c. Advance Tutorial to PromptPlaceK2 + Place K2 Beam
1. Press **Next** → tutorial shows: **"Place a K2 Beam..."**
2. Input location (e.g., (200, 0, 0)) and press **Place Beam** (calls `PlaceK2Beam`).
3. **Expected:** A 2 m horizontal beam actor spawns. Log shows:
   ```
   UArchSimScenarioWidget::PlaceKSetMember[K2] — registered at MemberIdx=1
   ```

### 3d. Advance Tutorial to PromptPlaceK4 + Place K4 Brace
1. Press **Next** → tutorial shows: **"Place a K4 Brace..."**
2. Input location (e.g., (100, 0, 100)) and press **Place Brace** (calls `PlaceK4Brace`).
3. **Expected:** A 2 m diagonal brace actor spawns. Log shows:
   ```
   UArchSimScenarioWidget::PlaceKSetMember[K4] — registered at MemberIdx=2
   ```

### 3e. Advance Tutorial to PromptPressTest + Test Structure
1. Press **Next** → tutorial shows: **"Press Test Structure to run the structural analysis."**
2. Press **Test Structure** button (calls `RequestSolveAndVisualize`).
3. **Expected within ~500 ms:**
   - Log shows:
     ```
     UArchSimScenarioWidget::RequestSolveAndVisualize — Solve requested; awaiting OnSolveComplete delegate.
     UArchSimScenarioWidget::OnSolveComplete — HeatmapActor spawned in PIE world.
     UArchSimScenarioWidget::OnSolveComplete — BuildHeatmap returned true; N member geometries provided.
     ```
   - In the PIE viewport: coloured mesh appears on or near the placed actors showing
     D/C utilization (blue = low stress, red = high stress or over-capacity).
   - **No PASS/FAIL grade is shown** — the heatmap is data, not judgment (ZPD Level 1 scope).

### 3f. Advance Tutorial to FreeExplore
1. Press **Next** → tutorial shows: **"Great work! Explore freely."**
2. Pressing **Next** again does nothing (FreeExplore is terminal — no-op).

---

## §5 Step 4 — Free Explore (≥5 min, ≥5 K-set actors, ≥5 successive Test Structure calls)

This step verifies stability under sustained student interaction.

1. Place **at least 5 more** K-set actors total (any combination of K1/K2/K4) at various
   world-space locations.
2. Press **Test Structure** at least **5 more times** (successive calls, rapid or spaced).
3. Each call must:
   - Return within 2 seconds (150 ms debounce + solve latency).
   - Update the heatmap colours without crash.
   - Not cause a "singular system" warning UNLESS you intentionally placed a mechanism
     (e.g., disconnected free-floating actors with no supports).
4. **Run at minimum 5 continuous minutes** of PIE without stopping. Watch for:
   - Float accumulation / silent freezes (U-13 fix targets ~193 days; 5 min is nominal).
   - Memory growth beyond OS baseline (debris leak — U-14 cap is 1024; no actors at u3).
   - Log `Error` or `Fatal` lines.

**Minimum pass criteria for free explore:**
- 5+ K-set actors placed, each appearing in viewport.
- 5+ Test Structure calls completed, each updating heatmap.
- No crash, no deadlock, no `Fatal` log in 5 min window.

---

## §6 Step 5 — Close Widget Cleanly (Reload Smoke)

1. In the widget, press **Reset** button (calls `ResetWidgetState`).
   - **Expected:** Log shows:
     ```
     UArchSimScenarioWidget::ResetWidgetState — resetting widget state.
     UArchSimScenarioWidget::ResetWidgetState — done. TutorialState=Welcome, HeatmapActor=null, PlacedActors cleared.
     ```
   - Tutorial overlay resets to Welcome prompt.
   - HeatmapActor procedural mesh disappears from PIE viewport.
   - K-set actors (columns/beams/braces) remain in the world (by design — not destroyed).

2. Close the widget panel (click X or use Window menu).

3. Stop PIE (press **Stop** button or Escape).

4. Verify no crash and no leaked editor state:
   - UE Editor does not hang on PIE stop.
   - `Saved/Logs/ArchSim.log` has no `Ensure`, `Check`, or `Fatal` in the shutdown sequence.
   - Outliner no longer shows the HeatmapActor (it was in PIE world, which is torn down).

5. Reopen the widget (Window → Editor Utility Widgets → BP_ArchSimScenarioWidget).
   - Tutorial overlay must show **Welcome** prompt (state reset worked).

6. Re-enter PIE and place 1 K1 + press Test Structure once more.
   - Heatmap must appear again (delegate re-subscription on fresh PIE session works).

---

## §7 PASS Criteria (verbatim)

All of the following must be true for the smoke to PASS:

- [ ] **P1** UE Build exits 0 before smoke.
- [ ] **P2** Headless gate reports `148/148` (cuDSS) or `146/146` (non-cuDSS) PASS.
- [ ] **P3** Widget opens without crash; log has no `Fatal` at open.
- [ ] **P4** PIE launches; Registry + InteractiveSubsystem initialize (log confirms).
- [ ] **P5** Tutorial advances: Welcome → PromptPlaceK1 → PromptPlaceK2 → PromptPlaceK4 → PromptPressTest → FreeExplore without crash.
- [ ] **P6** PlaceK1Column log shows `registered at MemberIdx=N` (N is whatever the Registry's next free index is — typically 0 in a fresh PIE session, but higher if the Registry already contains members from prior placements).
- [ ] **P7** PlaceK2Beam log shows `registered at MemberIdx=M` where M = N+1 (monotonic increment vs P6).
- [ ] **P8** PlaceK4Brace log shows `registered at MemberIdx=K` where K = M+1 (monotonic; the three placement actions form a contiguous sequence).
- [ ] **P9** RequestSolveAndVisualize returns true (log confirms `Solve requested`).
- [ ] **P10** OnSolveComplete fires; HeatmapActor spawned; BuildHeatmap returns true.
  **AS-30 update (S-06):** To trigger a solvable model reliably, call `SpawnDefaultPortalFrame()`
  before `RequestSolveAndVisualize()`. In the widget Output Log you can execute:
  `KismetSystemLibrary.ExecuteConsoleCommand("ke * SpawnDefaultPortalFrame")` or wire a
  BP button to `SpawnDefaultPortalFrame`. Expected: 3 actors spawn (2 K1 columns + 1 K2 beam;
  plus 2 fixed support nodes registered in the model — no separate support marker actors in the
  default implementation). The model has 4 nodes (2 fully-fixed base + 2 free top corners) and
  3 members — LDLT can resolve the 12 free-DOF system without the "Solve is singular" warning.
- [ ] **P11** Heatmap colour visible in PIE viewport (blue→red spectrum on placed actors).
  **AS-30 update (S-06):** With the portal frame fixture, P11 is relaxed to:
  HeatmapActor spawns successfully AND at least 1 member visualization shows non-trivial colour
  (any non-default-white). Portal frame under self-weight: both columns carry vertical compression,
  beam carries bending; D/C ratios are non-zero even with no explicit loads. Specific colour values
  are not pinned (depend on section/material defaults S275 + 200×200 mm rect and solver rounding).
- [ ] **P12** Free explore: 5+ actors + 5+ Test Structure calls in 5+ min, no crash.
- [ ] **P13** ResetWidgetState: log confirms `done. TutorialState=Welcome`; HeatmapActor gone from viewport.
- [ ] **P14** Widget close + PIE stop: no hang, no Ensure/Fatal in shutdown.
- [ ] **P15** Widget reopen: Welcome prompt shown (reload smoke).
- [ ] **P16** Re-PIE + Test Structure: heatmap appears again (re-subscription works).
- [ ] **P14 (AS-30, S-06)** Headless automation: `ArchSim.Gameplay.ScenarioFixture` reports
  6+ sub-checks PASS in `Saved/Logs/ArchSim.log` after running:
  ```powershell
  & "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
      "E:\project\ArchSim\ArchSim.uproject" `
      -ExecCmds="Automation RunTests ArchSim.Gameplay.ScenarioFixture; Quit" `
      -unattended -nullrhi -log
  ```
  Covers: PlaceFixedSupport + SpawnDefaultPortalFrame UFunction reflection (SC1/SC2);
  Registry headless `RegisterFixedSupport` at FVector(0,0,0) → NodeIdx >= 0 + Fixed.Num()==6
  + all-true (SC3); node-snap dedupe — second call same position returns same idx (SC4);
  idempotent Fixed after second call (SC5); transient widget graceful-fail (SC6).
  No PIE required; runs in the standard 5-leg headless gate.
- [ ] **P15 (AS-30, S-06)** Transient widget graceful-fail (headless verification):
  `SpawnDefaultPortalFrame()` on a `NewObject<UArchSimScenarioWidget>` (no PIE) returns
  false and does NOT crash. Verified by SC6 in `ArchSim.Gameplay.ScenarioFixture` — no
  separate manual step needed if P14 passes.

---

## §8 FAIL Recovery

If the smoke fails, use the following triage table to determine whether to fall back to v0.3.2 (Path B only):

| Symptom | Timestamp to check | Likely cause | Action |
|---|---|---|---|
| Build exits non-0 | `Build.bat` output | New header include broke unity build | Check ArchSimScenarioWidget.h for missing `#if WITH_EDITOR` guard; rebuild |
| Headless gate shows `N/148` mismatch | `run_gate.ps1` output `[2/5]` | New test file not compiled into binary | Re-run UE Build, then gate |
| Widget opens blank / no overlay | Widget open | BP child class missing event wiring | Wire `OnTutorialStateChanged` in BP graph; re-open widget |
| PIE fails to launch | Startup log | ALS asset missing or FrameSolver DLL not found | Re-run Build.bat; verify DLL path in `FrameCore.Build.cs` |
| `PlaceK2Beam` / `PlaceK4Brace` returns null | PIE log | GEditor->GetEditorWorldContext().World() null | Ensure a level is open before PIE; check Editor World Context |
| `Registry not available` log during placement | PIE log (placement) | Called PlaceKXxx before PIE started | Enter PIE first, then place actors |
| `RequestSolveAndVisualize — Registry null` | PIE log (Test Structure) | PIE not active when pressing Test Structure | Verify PIE is running (play button active) |
| `Solve is singular` warning | PIE log (OnSolveComplete) | No structural supports in model (all actors are free-floating) | Place actors at fixed support points, or add a `FixedNodeLoad` via Registry |
| Heatmap not visible | PIE viewport | BuildHeatmap returned false (zero members) | Verify at least 1 K-set actor was placed with MemberIdx >= 0 in log |
| HeatmapActor not destroyed after Reset | PIE viewport after Reset | `IsValid(HeatmapActor)` false before Destroy | Confirm PIE is still running when pressing Reset |
| Widget hangs on close | UE Editor | Delegate not unsubscribed | Check `BeginDestroy` log; if hang is reproducible, file AS-XX for investigation |
| **ANY crash / Fatal** | `Saved/Logs/ArchSim.log` | Unknown | Attach full log + callstack; fall back to v0.3.2 Path B release |

**Fall-back v0.3.2 (Path B only):** if any P1–P16 criterion fails and root cause is not identified within 30 min, tag the current commit as `v0.4.0-smoke-FAIL` and continue with v0.3.2 Path B release scope. Do not ship v0.4.0 until the smoke passes.

---

## §9 Smoke Evidence Template

After completing the smoke run, fill in this template and attach to the session log:

```
## v0.4.0 PIE 5min Smoke Evidence

Date: ____-__-__
Engineer: ________________
Host OS: Windows 11 (or other)
UE version: UE_5.7 (or specify)
GPU: (e.g., RTX 5070 Ti)
cuDSS build: yes / no
$ExpectedUeTests: 148 / 146

### P1 UE Build
Exit code: ___
Output snippet: [paste last 3 lines]

### P2 Headless gate
run_gate.ps1 output (last 10 lines):
[paste]
Total UE tests reported: ___/148 (or 146)

### P3–P4 Widget open + PIE launch
Log lines:
[paste UArchSimModelRegistry::Initialize + UFrameInteractiveSubsystem::StartSession lines]

### P5 Tutorial state machine
Transitions observed in log (paste AdvanceTutorialStep log lines):
[paste]
OnTutorialStateChanged visible in BP viewport: yes / no

### P6–P8 K-set placement
PlaceK1Column log: [paste MemberIdx line]
PlaceK2Beam log: [paste MemberIdx line]
PlaceK4Brace log: [paste MemberIdx line]

### P9–P11 Solve + heatmap
RequestSolveAndVisualize log: [paste "Solve requested" line]
OnSolveComplete log: [paste "HeatmapActor spawned" + "BuildHeatmap returned true" lines]
Heatmap visible in viewport: yes / no
Screenshot: [attach filename or paste description]

### P12 Free explore
Total K-set actors placed: ___
Total Test Structure calls: ___
PIE duration: ___ min ___ sec
Any crashes or Errors: none / [describe]

### P13–P16 Reset + reload smoke
ResetWidgetState log: [paste "done" line]
HeatmapActor gone after reset: yes / no
Widget reopen: Welcome prompt shown: yes / no
Re-PIE + Test Structure heatmap: appears: yes / no

### Overall verdict
PASS / FAIL

If FAIL, list criteria that failed (P-numbers): _______________
```

---

*This document is the v0.4.0 hard gate for Path A Scenario MVP FINAL.*  
*Generated: Sprint S-05 SPIKE-Scenario-u3 (2026-06-27).*
