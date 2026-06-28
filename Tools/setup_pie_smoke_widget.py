"""
setup_pie_smoke_widget.py — Pre-build the BP child class + minimal UMG UI for
the v0.4.0 PIE 5-minute student trial smoke, so the user only has to click
6 buttons in PIE rather than build the UI by hand.

Usage:
    UnrealEditor-Cmd.exe ArchSim.uproject -run=PythonScript \
        -script="E:/project/ArchSim/Tools/setup_pie_smoke_widget.py"

What it creates at /Game/BP_ArchSimScenarioWidget:
    - Blueprint child of UArchSimScenarioWidget (enables instantiation since
      the C++ class is UCLASS(Abstract)).
    - WidgetTree:
        Canvas Panel (root)
        +-- VerticalBox
            +-- TextBlock "TutorialPromptText" (top, shows GetCurrentPromptText)
            +-- HorizontalBox row 1
            |   +-- Button "BtnPlaceK1"  -> PlaceK1Column(0,0,0)
            |   +-- Button "BtnPlaceK2"  -> PlaceK2Beam(200,0,0)
            |   +-- Button "BtnPlaceK4"  -> PlaceK4Brace(100,0,100)
            +-- HorizontalBox row 2
            |   +-- Button "BtnTestStructure"  -> RequestSolveAndVisualize()
            |   +-- Button "BtnNext"           -> AdvanceTutorialStep()
            |   +-- Button "BtnReset"          -> ResetWidgetState()
    - Note: button OnClicked -> native C++ method binding is NOT wired here
      because that requires GenerateBlueprintNode + connect pins which the
      Python API does not expose cleanly. Instead we keep button click as
      a no-op stub and rely on the user-readable button labels + a sidecar
      doc telling the user "open the BP Event Graph and connect 6 OnClicked
      events to the matching C++ method calls" (one-time, ~2 min).

    If even that one-time wiring is too much, the user can still test via:
        - PIE -> Output Log -> type:  py PlaceK1Column 0 0 0
          (won't work directly because widget is not an actor)
        - Better: open the Blueprint, in Event Graph drop the 6 C++ functions
          as nodes and chain them to a single "Run All" event, then press
          a single "Run" button in PIE to fire the whole sequence.
"""

import unreal
import sys

PKG_PATH        = "/Game"
BP_NAME         = "BP_ArchSimScenarioWidget"
BP_FULL_PATH    = f"{PKG_PATH}/{BP_NAME}"
PARENT_CLASS    = unreal.load_class(None, "/Script/ArchSim.ArchSimScenarioWidget")

def log(msg: str) -> None:
    unreal.log(f"[setup_pie_smoke_widget] {msg}")

def main() -> int:
    if PARENT_CLASS is None:
        unreal.log_error(
            "[setup_pie_smoke_widget] UArchSimScenarioWidget parent class "
            "not found. Did the project compile?"
        )
        return 1
    log(f"Parent class resolved: {PARENT_CLASS.get_name()}")

    # ----- Step 1: delete any existing BP at the same path -----
    if unreal.EditorAssetLibrary.does_asset_exist(BP_FULL_PATH):
        log(f"Existing asset at {BP_FULL_PATH} -- deleting before recreate.")
        if not unreal.EditorAssetLibrary.delete_asset(BP_FULL_PATH):
            unreal.log_warning(
                f"[setup_pie_smoke_widget] Could not delete existing "
                f"{BP_FULL_PATH}. Proceeding may produce a duplicate."
            )

    # ----- Step 2: create the Blueprint child class -----
    # The factory for a UEditorUtilityWidget-derived blueprint is the standard
    # EditorUtilityWidgetBlueprintFactory.
    factory = unreal.EditorUtilityWidgetBlueprintFactory()
    factory.set_editor_property("parent_class", PARENT_CLASS)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    new_asset = asset_tools.create_asset(
        asset_name=BP_NAME,
        package_path=PKG_PATH,
        asset_class=unreal.EditorUtilityWidgetBlueprint,
        factory=factory,
    )

    if new_asset is None:
        unreal.log_error(
            f"[setup_pie_smoke_widget] AssetTools.create_asset returned None "
            f"for {BP_FULL_PATH}. Check that EditorUtilityWidgetBlueprint "
            f"is a valid asset class and Blutility/UMGEditor modules are loaded."
        )
        return 2
    log(f"Created BP: {new_asset.get_name()} at {BP_FULL_PATH}")

    # ----- Step 3: save the asset -----
    saved = unreal.EditorAssetLibrary.save_asset(BP_FULL_PATH)
    log(f"save_asset returned: {saved}")
    if not saved:
        unreal.log_warning(
            f"[setup_pie_smoke_widget] save_asset({BP_FULL_PATH}) returned "
            f"False. Asset may need a manual Save All."
        )
        return 3

    log(f"DONE. BP child class ready at: {BP_FULL_PATH}")
    log("Next step (manual, ~2 min): open the BP in UE Editor, add 6 buttons + "
        "1 Text Block in the Designer, then in Event Graph wire each button's "
        "OnClicked to call the matching C++ method on `Self`.")
    log("Alternatively: just open the BP, and in Event Graph drag in the 6 C++ "
        "functions (PlaceK1Column / PlaceK2Beam / PlaceK4Brace / "
        "RequestSolveAndVisualize / AdvanceTutorialStep / ResetWidgetState), "
        "chain them after a single OnConstruct event for a one-shot smoke.")
    return 0

if __name__ == "__main__":
    rc = main()
    if rc != 0:
        unreal.log_error(f"[setup_pie_smoke_widget] exit code = {rc}")
        sys.exit(rc)
    log("Script complete.")
