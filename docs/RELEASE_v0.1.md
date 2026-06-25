# RELEASE v0.1 — Architect Simulator game body (first UE5 consumer-side release)

> **Tag:** `v0.1` · **Date:** 2026-06-25 · **Repository:** rocky59487/architect_simulator
> **Engine baseline:** FrameCore `v4.0.0` (FROZEN) + LevelSim `v1` (untouched)
> **This release adds:** UE5 game-body consumer layer — first ever non-engine release.

---

## 1. What v0.1 is

The first **game-body** release on top of the two FROZEN structural engines that
shipped before it. v0.1 does not change either engine — it adds the consumer-side
plumbing that the upcoming education-game-play layer will sit on:

- **`Source/ArchSim/`** game module — 4 new files (`UArchSimMemberData` ActorComponent
  + `UArchSimModelRegistry` GameInstanceSubsystem) wiring Actor placement to the
  FrameCore solver via `UFrameInteractiveSubsystem`. Sprint S-01 A1 deliverables
  (A1-01 .. A1-05 done; A1-06 stub).
- **4 new MIT-licensed plugins** under `Plugins/` (clones; `.uplugin` `EngineVersion`
  patched to `5.7.0` where needed):
  - `Plugins/ALS/` — `Sixze/ALS-Refactored` tag `4.17` (advanced locomotion)
  - `Plugins/Prefabricator/` — `unknownworlds/prefabricator-ue5` (placement prefabs)
  - `Plugins/SPUD/` — `sinbad/SPUD` (save / persistence)
  - `Plugins/SUQS/` — `sinbad/SUQS` (quest system)
- **3 new design docs** under `docs/`:
  - [`ARCHITECT_SIM_MASTER_PLAN.md`](ARCHITECT_SIM_MASTER_PLAN.md) — 12 Parts × 5 sub-sections,
    feasibility table, 10 key technical decisions, full education-game master plan
  - [`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md) — sub-task expansion of every
    Part (1899h total over 16 sprints S-00 .. S-15)
  - [`SPRINT_NOTES.md`](SPRINT_NOTES.md) — per-sprint execution notes; S-00 spike
    conclusions live here
- **Config / build glue**:
  - `Config/DefaultEngine.ini` adds Enhanced Input default classes (ALS requirement)
  - `Source/ArchSim/ArchSim.Build.cs` adds `FrameCoreUE` + `EnhancedInput` dependencies

**Engine source delta** vs `v4.0.0`:

| Path | Lines changed | Notes |
|---|---|---|
| `Plugins/FrameSolver/Source/FrameCore/` | **0** | 鐵則 #1 FROZEN honoured |
| `Plugins/FrameSolver/Source/FrameCoreUE/` | **0** | Consumer-side public API stable |
| `Plugins/LevelSim/` | **0** | 鐵則 #5 honoured |
| `ArchSim.uproject` | **0** | 鐵則 #5 honoured (plugins auto-load via `.uplugin` flags) |

---

## 2. S-00 three-spike conclusions (folded into this release)

The release-gating spikes from Sprint S-00 are all resolved; see [`SPRINT_NOTES.md`](SPRINT_NOTES.md)
for full details. Quick summary:

| # | Spike | Result | Doc impact |
|---|---|---|---|
| 1 | ALS-Refactored v4.17 + UE5.7 build | ✅ PASS (build 63.97s) | Plugins clone table in SPRINT_NOTES Spike 1 |
| 2 | `FFrameDemandSummary` field-name verification | ✅ `MaxDC` and `SafetyFactor` both exist | `MaxMemberDC` typo fixed in 9 places across IMPL_PLAN |
| 3 | UE5.7 PCG node-name verification | ⚠️ 2 findings: (a) no independent "Perlin Noise" node → use `Spatial Noise` `Mode=Perlin2D`; (b) no "Attribute By Slope" node | Master plan Part G1 + IMPL_PLAN G1-T1 + A3 corrected; Slope filter alternatives table added; MVP decision: Landscape Spline (3h), Phase 2 upgrade: Custom PCG Node (8h) |

Bonus observation (added to Part L3 risk register):
**SPUD depends on `StructUtils` which UE5.5 deprecated**. Not blocking for UE5.7 MVP,
will break on UE5.8+ upgrade — verify SPUD has replaced `StructUtils` before any
UE-major upgrade.

---

## 3. Verification matrix

| Leg | Command | Result | Notes |
|---|---|---|---|
| Standalone FrameCore gate | `Plugins\FrameSolver\Standalone\build.bat` | ✅ PASS F1..F71 | Run 2026-06-25 by S-01 agent |
| UE headless automation | `Scripts\run_gate.ps1 -RequireOpenSees` (leg 2) | ✅ 135 tests PASS | Same matches v4.0.0 baseline — no automation regression from 4 new plugins |
| OpenSees offline cross-validation | `Scripts\run_gate.ps1 -RequireOpenSees` (leg 3) | ✅ PASS | Conda env `framecore-direct` |
| Linear deep audit | `Scripts\run_gate.ps1 -RequireOpenSees` (leg 4) | ✅ 104/104 checks PASS | |
| CLI round-trip | `Scripts\run_gate.ps1 -RequireOpenSees` (leg 5) | ✅ PASS | |
| **UE Editor build** | `%UE_ENGINE_ROOT%\Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development` | ✅ PASS (incremental 7.31s after v0.1 doc-only fixes) | Initial build at S-00: 63.97s (4 plugins + ArchSim module link) |
| **UE Editor → Plugins panel visual confirm** | (Open UE Editor → Edit → Plugins) | ⚠️ **NOT RUN (human-action item)** | Deferred to user; SPRINT_NOTES Gate 0 checklist L141-142 |

**Honest scope:**
- The 5-leg gate verifies the FrameCore + LevelSim engines are not regressed by the
  game-body additions. It does **not** verify `Source/ArchSim/` runtime behaviour —
  there is no `FrameCore.ArchSim.*` UE automation test yet. **Deferred to Sprint S-02
  task A1-07** (SaveGame round-trip test, adds 1 new UE test → 136 total).
- The `MemberIdx` / `Member.Id` stable-across-SaveGame contract is **documented**
  (`ArchSimModelRegistry.cpp:176-178`) but **not exercised by any oracle**. A1-07 will
  fix this.

---

## 4. Reproducibility (fresh-clone smoke test)

```powershell
# Prereqs: UE 5.7 installed at %UE_ENGINE_ROOT%; framecore-direct conda env present.
git clone https://github.com/rocky59487/architect_simulator.git
cd architect_simulator

# Restore the 4 plugin clones (NOT committed — they are external repos)
git clone --branch 4.17 https://github.com/Sixze/ALS-Refactored Plugins/ALS
git clone --depth 1 https://github.com/unknownworlds/prefabricator-ue5 Plugins/Prefabricator
git clone --depth 1 https://github.com/sinbad/SPUD                       Plugins/SPUD
git clone --depth 1 https://github.com/sinbad/SUQS                       Plugins/SUQS

# Patch .uplugin EngineVersion (Prefabricator + SPUD + SUQS): add "EngineVersion": "5.7.0"
# (ALS already has it.) See SPRINT_NOTES Spike 1 for exact patches.

# Build
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" ArchSimEditor Win64 Development `
    -project="$PWD\ArchSim.uproject" -waitmutex

# 5-leg gate
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
```

Expected end-state: 5-leg gate `GATE: PASS`, UE Editor opens with 4 new plugins
loaded (visual confirmation required).

---

## 5. Honest limitations

- **`Source/ArchSim/` is not yet exercised by automation.** 4 new files compile clean
  but have no green oracle. A1-07 in S-02 lands the first test.
- **A1-06 `DeactivateMember` is a stub** (`ArchSimModelRegistry.h:74-77`). It marks
  the model row inactive and queues a Deactivate patch via `RequestSolve`, but does
  not implement the full sweep-stale-weak-ptr + Reactivate flow. Sufficient for the
  current placement test path; full impl deferred.
- **No multiplayer testing.** Master plan Part H lists multiplayer goals; v0.1 ships
  the single-player Subsystem only. `UArchSimModelRegistry` is `UGameInstanceSubsystem`
  (per-instance, not per-server) — multiplayer replication is a deferred subsystem.
- **UE Editor → Plugins panel visual confirm pending.** Programmatic build is green;
  visual state of the Plugins panel requires a human eye on the UE Editor UI.

---

## 6. Breaking changes

**None.** This is the first game-body release; there is no prior game-body API to
break. The FrameCore engine API (`FFrameModelDef`, `UFrameInteractiveSubsystem`,
the 17 input + 9 result USTRUCT types, the 15 BP analysis entries) is unchanged
from `v4.0.0`.

---

## 7. Deferred to future releases (audit-traceable)

Every entry has a first-action sketch in [`HANDOFF_v0.1.md`](HANDOFF_v0.1.md) §4.

| ID | Title | Why deferred | Lands in |
|---|---|---|---|
| **A1-06 full** | Full sweep + Reactivate in `DeactivateMember` | S-01 stub sufficient for the placement test path | S-02 (~2h) |
| **A1-07** | SaveGame round-trip UE automation test | Needs A1 stable first | S-02 (~3h) — adds `FrameCore.ArchSim.SaveLoadRoundTrip` → 136 UE tests |
| **A2-01** | `AArchSimCharacter` ALS subclass | ALS plugin only built clean today | S-02 (~4h) |
| **Gate 0 visual confirm** | UE Editor → Plugins panel screenshot | Human-action only | Next user session |
| **G1 Slope Filter** | Landscape Spline (MVP) vs Custom PCG Node (Phase 2) decision | S-00 spike confirmed UE5.7 PCG has no `Attribute By Slope` node | MVP via Landscape Spline; Phase 2 ~8h |
| **K1-T2 / K4-T1 T2** | Constituent material + font + UIStyle DataAssets | Pre-B1 art assets, non-C++ work | S-02/S-03 in parallel |
| **SPUD UE5.5 StructUtils deprecation** | Verify SPUD has migration path before any UE5.8+ upgrade | UE5.7 currently works | Pre-UE5.8 upgrade gate |
| **`Plugins/FrameSolver/Grasshopper/v2/` 6 modified files** | Trivial build-env fixes (LF/CRLF + namespace import cleanup + GH API casing fix) | Not architectural; ship with v0.1 as `chore:` | Folded into v0.1 commit |

---

## 8. Grasshopper bridge yellow-flag verdict

`Plugins/FrameSolver/Grasshopper/v2/Rhino/*.cs` + `*.csproj` show 6 modified files
(13 insertions / 8 deletions, mostly LF/CRLF + unused-import + casing):

- `GhParameters.cs` — removed unused `using Grasshopper.Kernel.Types;`
- `FrameCoreGhPlugin.cs` — `GH_LibraryLicense.OpenSource` → `.opensource` (Rhino API casing fix)
- 4 others — `.csproj` / `AsyncComponent.cs` / `OpenFrameCoreComponent.cs` line-ending + minor

**Decision: ship with v0.1** as a `chore:` change. Rationale: these are trivial
build-environment fixes that surfaced during S-00 environment preparation; they do
**not** modify Grasshopper-bridge architectural surface, do not break v2 dispatcher
ABI (`kAbiVersion=2`), and are too small to justify a separate `v4.0.1` patch tag
on the FrameCore line. The release commit message will note them as `chore: GH
bridge build-env fixes (LF/CRLF + .cs casing + unused import; non-architectural)`.

---

## 9. Tag plan

```bash
cd <repo-root>      # e.g. cd $env:ARCHSIM_ROOT or cd ~/code/architect_simulator

# Stage explicitly (鐵則 #5: never -A or .)
git add Config/DefaultEngine.ini
git add Source/ArchSim/ArchSim.Build.cs
git add Source/ArchSim/Public/Components/ArchSimMemberData.h
git add Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h
git add Source/ArchSim/Private/Components/ArchSimMemberData.cpp
git add Source/ArchSim/Private/Subsystems/ArchSimModelRegistry.cpp
git add docs/ARCHITECT_SIM_MASTER_PLAN.md
git add docs/IMPLEMENTATION_PLAN.md
git add docs/SPRINT_NOTES.md
git add docs/RELEASE_v0.1.md
git add docs/HANDOFF_v0.1.md
git add README.md
# Plugin clones (Plugins/ALS/, Plugins/Prefabricator/, Plugins/SPUD/, Plugins/SUQS/)
# are NOT committed — they remain external clones documented in §4 above.
# Grasshopper chore fixes:
git add Plugins/FrameSolver/Grasshopper/v2/FrameCore.Bridge.csproj
git add Plugins/FrameSolver/Grasshopper/v2/Rhino/Common/AsyncComponent.cs
git add Plugins/FrameSolver/Grasshopper/v2/Rhino/Common/GhParameters.cs
git add Plugins/FrameSolver/Grasshopper/v2/Rhino/Components/Setup/OpenFrameCoreComponent.cs
git add Plugins/FrameSolver/Grasshopper/v2/Rhino/FrameCore.Gh.csproj
git add Plugins/FrameSolver/Grasshopper/v2/Rhino/FrameCoreGhPlugin.cs

git commit -m "$(cat <<'EOF'
release: v0.1 -- game body first release (UE5 consumer on FrameCore v4.0.0 + LevelSim v1)

Adds Source/ArchSim/ game module (UArchSimMemberData + UArchSimModelRegistry,
Sprint S-01 A1 deliverables), 4 MIT plugin clones documented for reproducible
restore, and the architect-simulator design corpus (master plan + implementation
plan + sprint log).

FrameCore engine source delta = 0 (鐵則 #1); LevelSim source delta = 0 (鐵則 #5);
ArchSim.uproject untouched (鐵則 #5).

5-leg gate PASS at S-00 close (standalone F1..F71 / UE 135 tests / OpenSees /
audit 104 / CLI roundtrip). UE incremental build 7.31s post v0.1 fixes.

chore: Grasshopper bridge build-env fixes (LF/CRLF + .cs casing + unused
import; non-architectural).

Deferred to S-02: A1-06 full sweep, A1-07 SaveLoadRoundTrip automation test,
A2-01 ALS character subclass. See docs/RELEASE_v0.1.md + docs/HANDOFF_v0.1.md.
EOF
)"

git tag -a "v0.1" -m "Architect Simulator game body first release (on FrameCore v4.0.0)"
git push origin main
git push origin v0.1
gh release create v0.1 --title "Architect Simulator game body v0.1" --notes-file docs/RELEASE_v0.1.md
```

---

*Release prepared by release-hardening skill (Phase 0–5 workflow). Audit: 3 parallel
deep-audit subagents (G/E+B/F+A); cross-doc consistency, sanitize, ABI verification.*
