"""
v3.5 Phase 9 — FrameCoreShowcase.umap builder.

Run this script from the UE Editor (Window > Output Log > Console > "Python (Script)" mode):

    py "<path-to-repo>/Tools/build_v3_5_showcase_map.py"

Replace `<path-to-repo>` with the absolute path to your local ArchSim clone (e.g.
`E:/project/ArchSim`, `~/projects/ArchSim`). Or open this file directly in the Editor's
Python tab.

What it builds: one map at Content/Maps/FrameCoreShowcase.umap with one instance of every
v3.5 renderer actor laid out in a labelled grid:

  * AFrameDeformedShapeActor       at (0,   0,    0)
  * AFrameUtilizationHeatmapActor  at (2000, 0,   0)
  * AFrameModalShapeActor          at (4000, 0,   0)
  * AFrameDynCollapseReplayActor   at (0,    2000, 0)
  * AFrameFragmentClusterActor     at (2000, 2000, 0)
  * AFrameInfluenceLineActor       at (4000, 2000, 0)
  * AFrameResponseSpectrumActor    at (0,    4000, 0)
  * AFrameRealTimeDynamicActor     at (2000, 4000, 0)
  * AFrameCoreStressFieldActor     at (4000, 4000, 0)  (v3.3 retained)

HONEST: Phase 9 ships this script + the placeholder map; the BP examples (drag-and-drop
parameter wiring for "hello cantilever" / "tower under wind" / "pull a column, watch
collapse" / "real-time slider") are deferred to v3.5.1 because they require interactive
designer iteration. The map gives a one-click starting point; subsequent BP authoring
builds on it.
"""

try:
    import unreal
except ImportError as e:
    raise SystemExit(
        "This script must be run inside the UE Editor — `unreal` module is "
        "Editor-only."
    ) from e


PLUGIN_PATH = "/FrameSolver"  # adjust if your plugin Content mount differs
MAP_PATH    = "/Game/Maps/FrameCoreShowcase"


def _spawn_at(world, actor_class, location, label):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class, unreal.Vector(*location), unreal.Rotator(0.0, 0.0, 0.0)
    )
    if actor is None:
        unreal.log_warning(f"Failed to spawn {label}")
        return None
    actor.set_actor_label(label)
    return actor


def main():
    eal = unreal.EditorAssetLibrary
    if not eal.does_asset_exist(MAP_PATH):
        unreal.log(f"Creating new map at {MAP_PATH}")
        new_world = unreal.EditorLevelLibrary.new_level(MAP_PATH)
    else:
        unreal.log(f"Loading existing map at {MAP_PATH}")
        unreal.EditorLevelLibrary.load_level(MAP_PATH)

    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        unreal.log_error("Editor world unavailable; aborting")
        return

    # Resolve each FrameCoreUE actor class by name (UE Python looks them up via the
    # reflection registry under the global namespace once the plugin is loaded).
    actor_specs = [
        ("AFrameDeformedShapeActor",      (    0.0,    0.0, 0.0), "DeformedShape"),
        ("AFrameUtilizationHeatmapActor", ( 2000.0,    0.0, 0.0), "Heatmap"),
        ("AFrameModalShapeActor",         ( 4000.0,    0.0, 0.0), "ModalShape"),
        ("AFrameDynCollapseReplayActor",  (    0.0, 2000.0, 0.0), "DynCollapseReplay"),
        ("AFrameFragmentClusterActor",    ( 2000.0, 2000.0, 0.0), "FragmentCluster"),
        ("AFrameInfluenceLineActor",      ( 4000.0, 2000.0, 0.0), "InfluenceLine"),
        ("AFrameResponseSpectrumActor",   (    0.0, 4000.0, 0.0), "ResponseSpectrum"),
        ("AFrameRealTimeDynamicActor",    ( 2000.0, 4000.0, 0.0), "RealTimeDynamic"),
        ("AFrameCoreStressFieldActor",    ( 4000.0, 4000.0, 0.0), "StressField"),
    ]
    for cls_name, loc, label in actor_specs:
        cls = unreal.find_class(cls_name)
        if cls is None:
            unreal.log_warning(
                f"Class {cls_name} not found — make sure FrameCoreUE plugin is loaded"
            )
            continue
        _spawn_at(world, cls, loc, label)

    unreal.EditorLevelLibrary.save_current_level()
    unreal.log(f"FrameCoreShowcase.umap built and saved to {MAP_PATH}")


if __name__ == "__main__":
    main()
