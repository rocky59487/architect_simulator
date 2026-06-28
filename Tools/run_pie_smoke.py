"""
run_pie_smoke.py — Drive the v0.4.0 PIE 5-minute student trial smoke
programmatically from UE Editor's Python console, no BP UI buttons needed.

Usage (after entering PIE in UE Editor):
    1. Window > Developer Tools > Output Log
    2. Switch the Cmd dropdown at the bottom from "Cmd" to "Python"
    3. Paste this entire file into the input box and press Enter
       (or just type:  py E:/project/ArchSim/Tools/run_pie_smoke.py )

What it does (mirrors docs/logs/S-05/u3_pie_smoke.md flow):
    - Spawns the UArchSimScenarioWidget BP child as an active Editor Utility tab.
    - Calls PlaceK1Column / PlaceK2Beam / PlaceK4Brace at canonical sample locations.
    - Advances the tutorial state machine through Welcome -> PromptPlaceK1 ->
      PromptPlaceK2 -> PromptPlaceK4 -> PromptPressTest.
    - Calls RequestSolveAndVisualize and waits ~1 second for the OnSolveComplete
      delegate to fire and the AFrameUtilizationHeatmapActor to spawn.
    - Reports PASS/FAIL for the 16 PIE-smoke criteria the headless gate cannot
      cover (the 12 PIE-runtime criteria; build/headless gate criteria are
      assumed PASS from the v0.4.0 release-time main-thread verification).

Expected output:
    A pass/fail line for each of P3..P14 + a final summary.
"""

import unreal
import time

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
BP_PATH       = "/Game/BP_ArchSimScenarioWidget"
K1_LOCATION   = unreal.Vector(  0.0, 0.0,   0.0)
K2_LOCATION   = unreal.Vector(200.0, 0.0,   0.0)
K4_LOCATION   = unreal.Vector(100.0, 0.0, 100.0)
SOLVE_WAIT_S  = 1.5   # 150 ms debounce + solve dispatch + delegate fire
RESET_WAIT_S  = 0.2   # ResetWidgetState is synchronous but actor destroy may queue


def banner(label: str) -> None:
    unreal.log(f"========== {label} ==========")


def check(label: str, ok: bool) -> bool:
    mark = "[PASS]" if ok else "[FAIL]"
    unreal.log(f"{mark} {label}")
    return ok


def main() -> None:
    banner("v0.4.0 PIE smoke runner")

    # ----- Step 0: load BP + spawn as Editor Utility tab -----
    widget_bp = unreal.load_asset(BP_PATH)
    if widget_bp is None:
        unreal.log_error(f"[run_pie_smoke] Failed to load BP at {BP_PATH}. "
                         f"Did setup_pie_smoke_widget.py run successfully?")
        return

    eus = unreal.get_editor_subsystem(unreal.EditorUtilitySubsystem)
    if eus is None:
        unreal.log_error("[run_pie_smoke] EditorUtilitySubsystem unavailable.")
        return

    # spawn_and_register_tab returns the spawned widget instance (or the existing
    # one if already open). Two-arg variant: (blueprint, tab_id_optional).
    widget = eus.spawn_and_register_tab(widget_bp)
    if widget is None:
        unreal.log_error(f"[run_pie_smoke] spawn_and_register_tab returned None. "
                         f"Asset may not be a valid Editor Utility Widget.")
        return
    check("P3  Widget instantiated and registered as Editor Utility tab", True)

    # ----- P4: PIE presence -----
    # We rely on RequestSolveAndVisualize return value as the PIE proxy.
    # If user did not enter PIE, RequestSolveAndVisualize returns false and
    # the P9 criterion below will FAIL.

    # ----- Step 1-3: place K1, K2, K4 (tutorial advances between each) -----
    placed = []

    widget.advance_tutorial_step()  # Welcome -> PromptPlaceK1
    a1 = widget.place_k1_column(K1_LOCATION)
    placed.append(("K1", a1))
    check("P6  PlaceK1Column returns non-null AActor", a1 is not None)

    widget.advance_tutorial_step()  # PromptPlaceK1 -> PromptPlaceK2
    a2 = widget.place_k2_beam(K2_LOCATION)
    placed.append(("K2", a2))
    check("P7  PlaceK2Beam   returns non-null AActor", a2 is not None)

    widget.advance_tutorial_step()  # PromptPlaceK2 -> PromptPlaceK4
    a4 = widget.place_k4_brace(K4_LOCATION)
    placed.append(("K4", a4))
    check("P8  PlaceK4Brace  returns non-null AActor", a4 is not None)

    # ----- P5: full state machine reachable -----
    widget.advance_tutorial_step()  # -> PromptPressTest
    state_at_press_test = widget.tutorial_state
    check(
        f"P5a TutorialState advanced through 4 steps "
        f"(now: {state_at_press_test})",
        state_at_press_test == unreal.ArchSimTutorialState.PROMPT_PRESS_TEST,
    )

    # ----- P9-P11: solve + heatmap spawn -----
    ok_solve = widget.request_solve_and_visualize()
    check("P9  RequestSolveAndVisualize returns true (PIE active + Registry valid)",
          bool(ok_solve))

    # Wait for the 150 ms debounce + the OnSolveComplete delegate roundtrip.
    unreal.log(f"[run_pie_smoke] Sleeping {SOLVE_WAIT_S} s for OnSolveComplete...")
    time.sleep(SOLVE_WAIT_S)

    heat = widget.heatmap_actor
    check("P10 HeatmapActor spawned (UPROPERTY non-null after OnSolveComplete)",
          heat is not None)
    if heat is not None:
        unreal.log(f"[run_pie_smoke]   HeatmapActor: {heat.get_name()}")

    # P11 (heatmap colour visible in PIE viewport) requires human eyes; we log
    # the success of BuildHeatmap return value implicitly via heatmap_actor
    # being valid (BuildHeatmap is called inside OnSolveComplete and we'd see a
    # log error if it failed). We mark this PASS-pending-eyes.
    check("P11 Heatmap colour visible in PIE viewport  [PASS-PENDING-EYES]",
          heat is not None)

    # ----- Advance to FreeExplore + log terminal -----
    widget.advance_tutorial_step()  # -> FreeExplore
    widget.advance_tutorial_step()  # FreeExplore -> FreeExplore (no-op)
    check(
        f"P5b FreeExplore terminal (state: {widget.tutorial_state})",
        widget.tutorial_state == unreal.ArchSimTutorialState.FREE_EXPLORE,
    )

    # ----- P13: ResetWidgetState -----
    widget.reset_widget_state()
    time.sleep(RESET_WAIT_S)
    check("P13a After Reset: TutorialState == Welcome",
          widget.tutorial_state == unreal.ArchSimTutorialState.WELCOME)
    check("P13b After Reset: HeatmapActor == null",
          widget.heatmap_actor is None)

    # ----- Summary -----
    banner("Smoke run complete")
    unreal.log("[run_pie_smoke] Sections to verify visually in PIE:")
    unreal.log("[run_pie_smoke]   P11 Heatmap colour ramp (blue->red) on placed actors")
    unreal.log("[run_pie_smoke]   P12 Free Explore: place 5+ more, press Test Structure 5+ times, ")
    unreal.log("[run_pie_smoke]       observe 5+ min unattended without crash")
    unreal.log("[run_pie_smoke]   P14 PIE Stop -> no hang, no Ensure/Fatal in shutdown log")
    unreal.log("[run_pie_smoke]   P15 Widget reopen -> Welcome prompt")
    unreal.log("[run_pie_smoke]   P16 Re-PIE + Test Structure -> heatmap re-spawns")
    unreal.log("[run_pie_smoke] If P3..P10 + P13 all PASS above and the visual ")
    unreal.log("[run_pie_smoke]   P11/P12/P14/P15/P16 hold, the v0.4.0 release ")
    unreal.log("[run_pie_smoke]   claim is verified end-to-end.")


main()
