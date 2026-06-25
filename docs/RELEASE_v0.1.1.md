# RELEASE v0.1.1 — Architect Simulator game body (patch: A1-07 SaveLoadRoundTrip test)

> **Tag:** `v0.1.1` · **Date:** 2026-06-25 · **Repository:** rocky59487/architect_simulator
> **Engine baseline:** FrameCore `v4.0.0` (FROZEN) + LevelSim `v1` (untouched)
> **This release adds:** one new UE automation test covering the v0.1 ArchSim registry
> SaveGame contract. **No source-code behavior change; no engine touch.**

---

## 1. What v0.1.1 is

A patch release that closes the only un-exercised contract from v0.1:
`UArchSimMemberData::MemberIdx` ↔ `ArchSimModelRegistry::Members[i].Id` stability
across a UE SaveGame reflection roundtrip. v0.1 documented the contract in
`ArchSimModelRegistry.cpp:178`/`:389`; v0.1.1 adds the oracle.

**One new file:**

| Path | Lines | Notes |
|---|---|---|
| `Source/ArchSim/Private/Tests/ArchSimSaveLoadTest.cpp` | +262 | `IMPLEMENT_SIMPLE_AUTOMATION_TEST FArchSimSaveLoadRoundTripTest`, test path `ArchSim.Persistence.SaveLoadRoundTrip` |

**Source delta vs v0.1:**

| Path | Lines changed | Notes |
|---|---|---|
| `Plugins/FrameSolver/Source/FrameCore/` | **0** | 鐵則 #1 FROZEN honoured |
| `Plugins/FrameSolver/Source/FrameCoreUE/` | **0** | Engine consumer surface untouched |
| `Plugins/LevelSim/` | **0** | 鐵則 #5 honoured |
| `ArchSim.uproject` / `.gitignore` | **0** | 鐵則 #5 honoured |
| `Source/ArchSim/ArchSim.Build.cs` | **0** | Test compiles via existing module deps |
| `Source/ArchSim/Public/...` / `Source/ArchSim/Private/...` (non-test) | **0** | No production behaviour change |

---

## 2. A1-07 — what is verified

**Test path:** `ArchSim.Persistence.SaveLoadRoundTrip`

**21+ sub-assertions** in one test, exercised at headless `-nullrhi -unattended`:

| Group | What's checked |
|---|---|
| Pre-state (a/b) | 5 fake actors spawned with root + scene component (the `SetActorLocation` silent-eat bug — see §5); each `UArchSimMemberData` component registered; `RegisterMember` returns valid idx; `MemberIdx` in-order assignment; `bRegistered` flag set; `FFrameModelDef` has 10 unique nodes (5 disjoint 1 m beams, 200 cm actor separation, well above the 1 mm node-merge tolerance) and 5 members |
| Pre-contract | `Members[i].Id == Components[i].MemberIdx` for all `i ∈ [0, 5)` |
| MaxRank ceiling | `RegisteredCount <= 96` (the `MaxRankBeforeRebaseline` ceiling at `ArchSimModelRegistry.h:105`) |
| Save (c) | Per-component `FObjectAndNameAsStringProxyArchive` capture (`ArIsSaveGame = true`) into `TArray<uint8>` buffers; one buffer per component |
| Tear-down (d) | All three `UPROPERTY(SaveGame)` fields (`MemberIdx` / `StructureGroupId` / `CachedUtilization`) scribbled to `-1` / `-1f` sentinel values — proves (e) genuinely restores from buffer rather than no-op |
| Load (e) | Per-component proxy-archive apply from buffer |
| Post-contract (f) | Registry-side model unchanged (node count / member count / registered count); `Members[i].Id` list bit-identical to pre-snapshot; each component's `MemberIdx` restored from buffer (not -1); `Members[i].Id == Components[i].MemberIdx` still holds; MaxRank ceiling preserved |

**Why proxy archive instead of driving `USpudSubsystem`:** SPUD is enabled by default
(`Plugins/SPUD/SPUD.uplugin`, `EngineVersion 5.7.0`) but `USpudSubsystem` requires a
live `World` + `Level` + `GameInstance` and produces disk artefacts that are fragile
under `-nullrhi -unattended`. `FObjectAndNameAsStringProxyArchive` is the **exact
reflection-based serializer SPUD layers on top of** — the three
`UPROPERTY(SaveGame)` fields on `UArchSimMemberData` are tested through their actual
save path. Only the SPUD orchestration layer (slot management, world tear-down,
disk I/O) is stubbed; the UE save contract for the component is genuinely covered.

This treatment is consistent with `HANDOFF_v0.1.md §4 item #6` ("SPUD UE5.5 risk
deferred"). The deferred-message is logged via `UE_LOG(LogTemp, Display, ...)` so
running the test prints the audit trail.

---

## 3. Verification matrix

| Leg | Command | Result | Notes |
|---|---|---|---|
| **A1-07 (this release)** | `%UE_ENGINE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe ArchSim.uproject -ExecCmds="Automation RunTests ArchSim.Persistence.SaveLoadRoundTrip; Quit" -unattended -nullrhi -log` | ✅ PASS (`EXIT CODE: 0`, 2026-06-25T07:37:49) | 21+ TestEqual/TestTrue/TestNotNull all green |
| Standalone FrameCore gate | `Plugins\FrameSolver\Standalone\build.bat` | ✅ PASS F1..F71 | Inherited from v0.1 — engine unchanged |
| UE headless automation (FrameCore.*) | `Scripts\run_gate.ps1 -RequireOpenSees` leg 2 | ✅ 135 tests PASS | Inherited from v0.1 — FrameCore namespace unchanged |
| OpenSees offline cross-validation | `Scripts\run_gate.ps1 -RequireOpenSees` leg 3 | ✅ PASS | Inherited |
| Linear deep audit | `Scripts\run_gate.ps1 -RequireOpenSees` leg 4 | ✅ 104/104 checks PASS | Inherited |
| CLI round-trip | `Scripts\run_gate.ps1 -RequireOpenSees` leg 5 | ✅ PASS | Inherited |
| **UE Editor → Plugins panel visual confirm** | (Open UE Editor → Edit → Plugins) | ⚠️ **NOT RUN (human-action item)** | Still deferred from v0.1 |

**Honest scope:**
- **`run_gate.ps1` does not currently include the `ArchSim.*` namespace.**
  Line 70 hard-codes `ExecCmds = 'Automation RunTests FrameCore; Quit'`, which only
  matches the `FrameCore.*` namespace. `ArchSim.Persistence.SaveLoadRoundTrip`
  therefore must be run via the standalone command in row 1 above. Folding it into
  the 5-leg gate is deferred — see [`HANDOFF_v0.1.1.md §4 item #1`](HANDOFF_v0.1.1.md).
- MaxRank=96 is verified as `RegisteredCount <= 96`; an explicit "register 97 ⇒
  rebaseline triggers" stress test is not in scope (sub-coverage gap; intentional).
- `UArchSimMemberData` in the test uses `RF_Transient` (acceptable here because the
  proxy-archive is invoked explicitly). When SPUD orchestration is wired in S-02+,
  verify production components are **not** `RF_Transient` or SPUD will skip them.

---

## 4. Reproducibility

```powershell
# Prereqs: UE 5.7 installed at %UE_ENGINE_ROOT%; framecore-direct conda env present.
git clone https://github.com/rocky59487/architect_simulator.git
cd architect_simulator
git checkout v0.1.1

# Restore the 4 plugin clones (NOT committed — they are external repos)
git clone --branch 4.17 https://github.com/Sixze/ALS-Refactored           Plugins/ALS
git clone --depth 1   https://github.com/unknownworlds/prefabricator-ue5  Plugins/Prefabricator
git clone --depth 1   https://github.com/sinbad/SPUD                      Plugins/SPUD
git clone --depth 1   https://github.com/sinbad/SUQS                      Plugins/SUQS

# Patch the 3 plugins missing EngineVersion (see SPRINT_NOTES Spike 1 for exact bytes)
# - Prefabricator/Prefabricator.uplugin: add "EngineVersion": "5.7.0"
# - SPUD/SPUD.uplugin:                   add "EngineVersion" : "5.7.0"
# - SUQS/SUQS.uplugin:                   add "EngineVersion" : "5.7.0"

# Build the editor
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" ArchSimEditor Win64 Development `
    -project="$PWD\ArchSim.uproject" -waitmutex

# Run A1-07 only (5-leg gate inherits unchanged from v0.1)
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests ArchSim.Persistence.SaveLoadRoundTrip; Quit" `
    -unattended -nullrhi -log

# Verify
Select-String -Path Saved\Logs\ArchSim.log `
    -Pattern 'ArchSim\.Persistence\.SaveLoadRoundTrip','EXIT CODE'
# Expect:  Result={成功}  +  **** TEST COMPLETE. EXIT CODE: 0 ****
```

---

## 5. Lessons folded into this release (durable)

1. **`AActor` has no default `RootComponent`** — `SpawnActor(Class, FTransform(Location), Params)`
   on a bare `AActor::StaticClass()` **silently drops the location** because there's no
   transform-bearing root to anchor it. The test caught this on first run with
   `nodeCount expected 10, got 2` (all 5 actors collapsed to identity → 10 endpoints
   merged into 2 unique nodes under the 1 mm tolerance). Fix is **graft a
   `USceneComponent` root + `RegisterComponent` + `SetActorLocation`** explicitly.
   Inline comment at `ArchSimSaveLoadTest.cpp:119-121` preserves this for the next
   maintainer.

2. **MSVC source encoding** — `§` (U+00A7) in a C++ string literal triggers MSVC's
   "`\xA74` escape sequence out of range" parse error under the default codepage.
   Test source uses ASCII `"section 4"` for the HANDOFF backlink. Project-wide
   convention: ASCII-only in test diagnostic strings.

3. **Headless test world acquisition** — `UWorld::CreateWorld` crashes inside an
   Automation commandlet because the editor GameInstance template is not loaded.
   The canonical workaround is `GEngine->GetWorldContexts()` walk — same pattern as
   `FrameCoreUEActorStressMeshTest`. New tests in `Source/ArchSim/Private/Tests/`
   should re-use this idiom (`FindSpawnWorld()` helper).

4. **`UArchSimModelRegistry` Outer warning** — `NewObject<UArchSimModelRegistry>()`
   with no explicit Outer emits a `LogUObjectGlobals: Warning ... was created in
   invalid Outer /Script/CoreUObject.Package!` line. Cosmetic, matches existing
   `FrameCoreUEInteractiveSubsystemTest` fallback pattern, test still passes.
   Documented in `ArchSimSaveLoadTest.cpp:50-55`.

---

## 6. Breaking changes

**None.** v0.1.1 is purely additive (one new test file). All v0.1 contracts hold.

---

## 7. Deferred items (audit IDs)

| ID | Item | Deferred to | First-action sketch |
|---|---|---|---|
| AS-01 | `Scripts/run_gate.ps1` ExecCmds filter `'FrameCore;'` does not include `ArchSim.*` namespace; A1-07 is not in the 5-leg gate yet | S-02 (v0.2) | Edit `run_gate.ps1:70` `ExecCmds = 'Automation RunTests FrameCore+ArchSim; Quit'` (UE Automation supports `+`-separated filters); bump `$ExpectedUeTests` from 135 → 136 with non-cuDSS fallback 133 → 134 |
| AS-02 | A1-06 full integration test (currently stub in v0.1) | S-02 (v0.2) | First action lives in `HANDOFF_v0.1.md §4 item #1`; SaveLoad now done so the dependency chain is unblocked |
| AS-03 | A2-01 ALS pawn integration | S-02 (v0.2) | `HANDOFF_v0.1.md §4 item #3` |
| AS-04 | Gate 0 UE Editor → Plugins panel visual confirmation | Human action, any time | Open UE Editor; tick `SPRINT_NOTES.md L141-142` |
| AS-05 | K1-T2 / K4 art assets | S-02 / S-03 (v0.2 / v0.3) | `HANDOFF_v0.1.md §4 item #5` |
| AS-06 | SPUD UE5.5 `StructUtils` deprecation | Before any UE5.7 → UE5.8 upgrade | `HANDOFF_v0.1.md §4 item #6` |
| AS-07 (new) | A1-07 MaxRank=96 stress test — register 97, verify rebaseline trigger | S-02+ (v0.2+) | Extend `ArchSimSaveLoadTest.cpp` with second `IMPLEMENT_SIMPLE_AUTOMATION_TEST` `FArchSimMaxRankCeilingTest`; register 97 fake members; assert rebaseline triggered or registration rejected |
| AS-08 (new) | SPUD orchestration `RF_Transient` audit | When wiring SPUD orchestration in S-02+ | Grep `Source/ArchSim/` for `RF_Transient` on `UArchSimMemberData`; ensure production components are NOT transient (else SPUD reflection sweep skips them) |

All items trace to a one-line "first action" in `HANDOFF_v0.1.1.md §4`.

---

## 8. Tag plan

```bash
# Already done by integrator at release-hardening commit:
git add Source/ArchSim/Private/Tests/ArchSimSaveLoadTest.cpp
git add docs/SPRINT_NOTES.md docs/RELEASE_v0.1.1.md docs/HANDOFF_v0.1.1.md
git add README.md
git commit -m "release: v0.1.1 -- A1-07 SaveLoadRoundTrip test (game body; engine FROZEN)"
git tag -a v0.1.1 -m "Architect Simulator game body v0.1.1 — A1-07 SaveLoad oracle"

# Pending user authorisation:
git push origin main
git push origin v0.1.1
gh release create v0.1.1 \
    --title "Architect Simulator game body v0.1.1" \
    --notes-file docs/RELEASE_v0.1.1.md
```

---

*v0.1.1 is the second game-body release on top of FrameCore v4.0.0 + LevelSim v1.*
*Next: v0.2 collects the rest of Sprint S-02 (A1-06 integration, A2-01 ALS pawn,*
*ArchSim namespace in the 5-leg gate).*
