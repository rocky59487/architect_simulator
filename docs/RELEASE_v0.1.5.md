# Release `v0.1.5` — Game body — Tick driver loop + smoke test

> Tagged 2026-06-26, branch `main`. Second patch of Sprint S-02 atop
> FrameCore `v4.0.0` (FROZEN) + LevelSim `v1` (FROZEN). Bundle of
> **AS-02b** (Tick driver: registered-count delta → RequestSolve) + **AS-02c**
> (headless smoke test for Tick telemetry + IsTickable filter, opens AS-13
> as the PIE-world fixture follow-on).
>
> AS-02 trilogy complete after this release (AS-02a was bundled into v0.1.4).

---

## 1. What `v0.1.5` is

One sentence: **`UArchSimGameInstance::Tick()` now bridges the unique
`RegisterMember`-without-RequestSolve gap by detecting `GetRegisteredCount()`
delta each frame and emitting an empty `RequestSolve(FFrameModelPatch{})`
that the existing 150 ms debounce collapses into a single solve burst; a new
headless smoke test (`ArchSim.Integration.TickDriver`) pins the
headless-observable telemetry behaviour and explicitly defers the
driver-loop integration to a future PIE-world fixture (AS-13).**

### Files changed (vs `v0.1.4`)

| Path | Type | Delta | Origin |
|---|---|---|---|
| `Source/ArchSim/Public/ArchSimGameInstance.h` | Production (mod) | +26 | AS-02b |
| `Source/ArchSim/Private/ArchSimGameInstance.cpp` | Production (mod) | +49 | AS-02b |
| `Source/ArchSim/Private/Tests/ArchSimGameInstanceTest.cpp` | Test (new) | +147 | AS-02c |
| `Scripts/run_gate.ps1` | Build | +1 (L29 138 → 139 + comment) | AS-02c |
| `docs/ARCHITECTURE_INDEX.md` | Docs | §2/§6/§7/§8 updates | release |
| `docs/RELEASE_v0.1.5.md` | Docs | new | release |
| `docs/HANDOFF_v0.1.5.md` | Docs | new | release |
| `docs/logs/S-02/agent_AS-02b.md` | Sprint log | new | release |
| `docs/logs/S-02/agent_AS-02c.md` | Sprint log | new | release |
| `docs/logs/S-02/manager.md` | Sprint log | append (v0.1.4 + AS-02b/c + AS-13) | release |
| `README.md` | Docs | v0.1.5 status block prepended | release |

**Engine source delta vs v0.1.4 = 0 lines** under `Plugins/FrameSolver/Source/FrameCore/`.
**LevelSim source delta = 0 lines.**
**`UArchSimModelRegistry.{h,cpp}` delta = 0 lines** (production logic byte-identical).
**ArchSim production code delta vs v0.1.4 = 75 lines** (header +26 + cpp +49 of AS-02b).

### What was NOT done

- FrameCore engine source (FROZEN)
- LevelSim engine (FROZEN)
- 4 external plugin clones (ALS / Prefabricator / SPUD / SUQS)
- `ArchSim.uproject` / `.gitignore` / build artifacts
- `UArchSimModelRegistry` production logic (only AS-10's 3 telemetry getters from v0.1.4 stand; this release does not amend the registry)
- v0.1.x already-shipped tests (`SaveLoadRoundTrip` + `MaxRankCeiling` + `RebaselineCeiling` all preserved)
- Actor position-change sync (deliberately deferred to future AS-XX; demo MVP has static buildings)

---

## 2. AS-02b — Tick driver loop (registered-count delta)

The key insight (verified by Read of `ArchSimModelRegistry.cpp`):

- `RegisterMember` (cpp:131-208) mutates `CurrentModel` + `IndexToComponent`
  but does NOT call `RequestSolve` anywhere in its body
- `DeactivateMember` (cpp:377-394) DOES call `RequestSolve` internally (cpp:391-393)
- Therefore: registrations are the ONLY model-change event that fails to
  auto-trigger a solve. AS-02b's Tick driver fills exactly this gap.

### Tick body design

```cpp
void UArchSimGameInstance::Tick(float DeltaSeconds)
{
    // Telemetry (preserved from AS-02a; AS-02c smoke test reads these).
    ++TickCount;
    AccumulatedSeconds += DeltaSeconds;

    // AS-02b: registered-count delta -> RequestSolve(empty patch).
    UArchSimModelRegistry* Registry = GetSubsystem<UArchSimModelRegistry>();
    if (!Registry) return;  // subsystem teardown edge cases

    const int32 CurrentCount = Registry->GetRegisteredCount();
    if (CurrentCount != LastSeenRegisteredCount)
    {
        Registry->RequestSolve(FFrameModelPatch{});  // PatchRank=0, never trips ceiling
        ++SolveTriggerCount;
        LastSeenRegisteredCount = CurrentCount;
    }
}
```

### Why `!=` instead of `<` or `>`

Symmetric on add+remove. Removes are handled by `DeactivateMember`'s own
internal `RequestSolve` (so the Tick path doesn't need to re-trigger), but
the count check still uses `!=` so that `LastSeenRegisteredCount` updates
after a remove and prevents redundant Tick re-trigger on subsequent idle frames.

### Why initial `LastSeenRegisteredCount = -1`

Forces the first Tick after Init() to emit `RequestSolve(empty)` regardless
of starting count. Even with 0 placed members, the empty solve cascade is
harmless: `RequestSolve` → debounce → `ExecuteSolve` → `FlushAndStartSession`
on empty model → `Sub->StartSession` fails → patch dropped, single Warning log.

### Position-change sync explicitly deferred

Demo MVP places static buildings — no position-dirty case fires. Adding a
per-Tick actor-position scan would require either:
- A new `MoveMemberEndpoint` API on the registry (not added; production
  logic preserved byte-identical), OR
- A `Deactivate + Re-register` pattern that renumbers `MemberIdx` and
  breaks the SaveGame contract for moved members.

The plan deliberately scoped AS-02b to "registration delta" only. Position
sync is a future AS-XX item to be picked up when dynamic buildings become a
gameplay feature.

---

## 3. AS-02c — Headless smoke test

`Source/ArchSim/Private/Tests/ArchSimGameInstanceTest.cpp` (new, 147 lines).
`IMPLEMENT_SIMPLE_AUTOMATION_TEST FArchSimTickDriverSmokeTest`. **7 sub-checks**.
Test path: `ArchSim.Integration.TickDriver`.

### What this test verifies in headless `-nullrhi -unattended`

- ✅ `UArchSimGameInstance` constructs via `NewObject<>()` with zeroed telemetry
- ✅ Manual `Tick(1/60)` increments `TickCount` deterministically
- ✅ Manual `Tick(1/60)` accumulates `AccumulatedSeconds` to expected total
- ✅ `LastSeenRegisteredCount` stays at -1 after Tick (because `GetSubsystem`
  returns null in this fixture, so the driver branch early-bails BEFORE
  the count compare)
- ✅ `SolveTriggerCount` stays at 0 for the same reason
- ✅ `IsTickable()` returns false on the Class Default Object (`IsTemplate()==true`)
- ✅ `IsTickable()` returns false on a fresh `NewObject` instance (`bIsActive==false`)
- ✅ 100-tick burst leaves no aliasing (pure integer counter discipline)
- ✅ Const-getter is observably pure (two consecutive reads return identical)

### What this test CANNOT verify in headless (deferred to AS-13)

`GetSubsystem<UArchSimModelRegistry>()` returns null in a headless fixture
because the GameInstance subsystem pipeline is not engaged by a real PIE
session. Therefore the driver-loop branch at `ArchSimGameInstance.cpp:90-103`
(detect delta → emit RequestSolve → increment counters) is unreachable in
this fixture. To verify that branch end-to-end:

- **AS-13 (new backlog)** — Spawn a PIE world via `FAutomationEditorCommonUtils`,
  place 96+ `UArchSimMemberData` actors, tick the world, then assert
  `Registry->IsRebaselineDue()` and `GameInstance->GetSolveTriggerCount()`.

This mirrors the v0.1.4 AS-10 honesty pattern: the test pins what IS
observable in the fixture it can construct, and the limitation is loudly
documented in the test header rather than papered over with a mocked subsystem.

### Convention note (verified by reviewer)

The agent chose `EAutomationTestFlags_ApplicationContextMask | SmokeFilter`
matching the precedent of `ArchSimSaveLoadTest.cpp:82` /
`ArchSimRebaselineTest.cpp:73` / `FArchSimMaxRankCeilingTest:L288`. Adversarial
reviewer cross-checked all three precedents to confirm this is NOT a
fabricated justification.

---

## 4. NITS / new backlog

| ID | Title | Origin | Severity |
|---|---|---|---|
| AS-13 | PIE-world fixture for driver-loop + trip-path | AS-02c smoke deferred + AS-10 trip-path deferred | MEDIUM |
| AS-11 | Header comment precision for rebaseline reset points | v0.1.4 (carried) | LOW |
| AS-12 | `GetMaxRankBeforeRebaseline()` production consumer | v0.1.4 (carried) | LOW |

No NITS findings on AS-02b or AS-02c reviews themselves — both CLEAN.

---

## 5. 5-leg gate evidence

```
[1/5] standalone FrameCore gate (build.bat)...
       standalone: ALL PASS  (failures=0) (exit 0)
[2/5] UE headless automation...
       UE automation: 139 tests run, exit code 0 (process exit 0; expected >= 139)
[3/5] OpenSees offline cross-validation...
       OpenSees compare: PASS (exit 0)
[4/5] linear-analysis deep audit...
       linear deep audit: PASS failures=0 checks=104 (exit 0)
[5/5] CLI round-trip (frame_cli J1 bridge)...
       CLI round-trip: ALL PASS  (failures=0) (exit 0)
======================================================
 GATE: PASS  (standalone OK, UE 139 tests green, OpenSees PASS, deep audit OK, CLI round-trip OK)
```

Run by AS-02c subagent on cuDSS host. AS-02b's earlier gate run reported `UE 138 tests green`.

**Non-cuDSS expectation:** `.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 137`.

---

## 6. Process narrative

`v0.1.5` continues Sprint S-02:

- Phase 2 dispatched AS-02b (budget 220K / 45 / 25min; used 119K / 27 / 8.8min)
- Phase 3 reviewer (95K / 26 tool calls / 2.3min) → CLEAN
- AS-02b commit deferred (no standalone tag; bundles with AS-02c)
- Phase 2 dispatched AS-02c (budget 150K / 30 / 20min; used 129K / 20 / 9.5min)
- Phase 3 reviewer (91K / 14 tool calls / 1.4min) → CLEAN
- Phase 4 (this) commits AS-02b production + AS-02c test as v0.1.5 bundle

Sprint logs preserved under `docs/logs/S-02/`:

- [`agent_AS-02b.md`](logs/S-02/agent_AS-02b.md)
- [`agent_AS-02c.md`](logs/S-02/agent_AS-02c.md)
- [`manager.md`](logs/S-02/manager.md) (events appended)

---

## 7. Continuation pointer

- **Handoff:** [`docs/HANDOFF_v0.1.5.md`](HANDOFF_v0.1.5.md) — Z-01 first action for AS-03a (ALS pawn subclass)
- **Architecture index:** [`docs/ARCHITECTURE_INDEX.md`](ARCHITECTURE_INDEX.md) — updated §2/§6/§7/§8
- **Sprint S-02 continues** in the same session toward v0.2.0 minor at AS-03d
- **Prior anchor:** [`docs/RELEASE_v0.1.4.md`](RELEASE_v0.1.4.md) | [`docs/HANDOFF_v0.1.4.md`](HANDOFF_v0.1.4.md)
