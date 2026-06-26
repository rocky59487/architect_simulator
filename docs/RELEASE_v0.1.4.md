# Release `v0.1.4` — Game body — Tick driver foundation + real rebaseline-ceiling test

> Tagged 2026-06-26, branch `main`. Continuation of the `v0.1.x` game-body
> patch line atop FrameCore `v4.0.0` (FROZEN) + LevelSim `v1` (FROZEN).
> Bundle of two units: **AS-02a** (production-only GameInstance foundation)
> + **AS-10** (honest `PendingRankAccumulation` ceiling test, closes
> v0.1.3 deferred item).

---

## 1. What `v0.1.4` is

One sentence: **the `UArchSimGameInstance` foundation for the per-frame Tick
driver lands as a thin skeleton, and the `MaxRankBeforeRebaseline=96` ceiling
that v0.1.3's `MaxRankCeiling` test deliberately could NOT verify gets a real
(but honestly partial) test in `ArchSim.Persistence.RebaselineCeiling`.**

### Files changed (vs `v0.1.3`)

| Path | Type | Delta | Origin |
|---|---|---|---|
| `Source/ArchSim/Public/ArchSimGameInstance.h` | Production (new) | +75 | AS-02a |
| `Source/ArchSim/Private/ArchSimGameInstance.cpp` | Production (new) | +68 | AS-02a |
| `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h` | Production (mod) | +24 | AS-10 |
| `Source/ArchSim/Private/Tests/ArchSimRebaselineTest.cpp` | Test (new) | +220 | AS-10 |
| `Config/DefaultEngine.ini` | Config | +8 | AS-02a |
| `Scripts/run_gate.ps1` | Build | +1 (L29 bump + comment) | AS-10 |
| `docs/ARCHITECTURE_INDEX.md` | Docs | +5 + L232 fix | AS-10 + NITS fix |
| `docs/RELEASE_v0.1.4.md` | Docs | new | release |
| `docs/HANDOFF_v0.1.4.md` | Docs | new | release |
| `docs/logs/S-02/*.md` | Sprint logs | new dir | release |
| `README.md` | Docs | v0.1.4 status block prepended | release |

**Engine source delta vs v0.1.3 = 0 lines** under `Plugins/FrameSolver/Source/FrameCore/`.
**FrameCoreUE source delta = 0 lines.**
**LevelSim source delta = 0 lines.**
**ArchSim production code delta = 167 lines** (3 new production files + 24 LOC
header amendment for telemetry getters).

### What was NOT done

- FrameCore engine (`v4.0.0` FROZEN)
- LevelSim engine (`v1` FROZEN)
- 4 external plugin clones (ALS / Prefabricator / SPUD / SUQS — untouched since
  v0.1; their dirs remain untracked from S-00 spike, deliberate)
- `ArchSim.uproject` (鐵則 #5)
- `.gitignore` (鐵則 #5)
- `UArchSimModelRegistry` production logic (`RequestSolve` / `ExecuteSolve` /
  `PatchRank` / `MaxRankBeforeRebaseline=96` — all unchanged; only 3 `const
  noexcept` telemetry getters added to header)
- v0.1.x already-shipped tests (`SaveLoadRoundTrip` + `MaxRankCeiling`
  vacuous-assertion preserved per HANDOFF v0.1.3 §1)

---

## 2. AS-02a — `UArchSimGameInstance` skeleton

Sprint S-02 first dispatch unit. Lands the `UGameInstance` + `FTickableGameObject`
mixin that subsequent units (AS-02b/c) will fill with the actual member-sync +
`RequestSolve` driver loop. This unit deliberately keeps the `Tick()` body to
two telemetry counter increments only.

### Class shape

```cpp
UCLASS()
class ARCHSIM_API UArchSimGameInstance : public UGameInstance,
                                          public FTickableGameObject
```

- `Init()` calls `Super::Init()` first, then sets `bIsActive = true` LAST.
  Order matters: any `IsTickable()` query during the super-chain sees inactive.
- `Shutdown()` sets `bIsActive = false` FIRST, then `Super::Shutdown()`. A racing
  Tick callback during teardown bails immediately.
- `IsTickable()` is the three-condition AND `bIsActive && GetWorld() != nullptr && !IsTemplate()`.
  CDOs (`IsTemplate()` true) and during-teardown contexts (`GetWorld()` null) are
  filtered. `IsTickableInEditor()` and `IsTickableWhenPaused()` are both `false`.
- `GetStatId()` uses the standard `STATGROUP_Tickables` so Unreal Insights groups
  this with other Tickables.
- BP-pure accessors: `GetTickCount()` and `GetAccumulatedTime()` (the smoke
  test for AS-02c will assert these increment).

### Config wire

```ini
; in Config/DefaultEngine.ini (post-v0.1.4)
[/Script/EngineSettings.GameMapsSettings]
GameInstanceClass=/Script/ArchSim.ArchSimGameInstance
```

Changing this triggers `Invalidating makefile for ArchSimEditor (DefaultEngine.ini
modified)` per SPRINT_NOTES Spike 1 — expected, not a regression.

### LogArchSim category

Pre-v0.1.4, `LogArchSimRegistry` was declared via `DEFINE_LOG_CATEGORY_STATIC`
(TU-local) in `ArchSimModelRegistry.cpp`. AS-02a adds a separate module-wide
`LogArchSim` via `DECLARE_LOG_CATEGORY_EXTERN` (public header) + `DEFINE_LOG_CATEGORY`
(`.cpp`) so AS-02b/c TUs can use it without redeclaration.

---

## 3. AS-10 — Real `PendingRankAccumulation` ceiling test

Closes the deferred AS-10 item from `HANDOFF_v0.1.3.md` §4 #8. The v0.1.3
`MaxRankCeiling` test pinned that **`RegisterMember` has no register-count
ceiling**; AS-10 turns the spotlight on the **actual** ceiling site:
`RequestSolve` in `ArchSimModelRegistry.cpp:269-292`.

### Production-side: 3 telemetry getters

```cpp
// New in ArchSimModelRegistry.h:
[[nodiscard]] int32 GetPendingRankAccumulation() const noexcept;
[[nodiscard]] bool  IsRebaselineDue() const noexcept;
[[nodiscard]] static constexpr int32 GetMaxRankBeforeRebaseline() noexcept;
```

All three are pure `const noexcept` observers. No production logic touched
(`RequestSolve` / `ExecuteSolve` / `PatchRank` / `MaxRankBeforeRebaseline = 96`
all unchanged, byte-identical to v0.1.3). Adversarial review confirmed
`git diff HEAD -- Source/ArchSim/Private/Subsystems/ArchSimModelRegistry.cpp`
returns zero bytes.

### Test — `ArchSim.Persistence.RebaselineCeiling`

`Source/ArchSim/Private/Tests/ArchSimRebaselineTest.cpp` (new, 220 lines).
`IMPLEMENT_SIMPLE_AUTOMATION_TEST FArchSimRebaselineCeilingTest`. **7 sub-checks**.

### Two honest spec corrections

#### Correction 1 — Strict `>` semantic (off-by-one direction)

`HANDOFF_v0.1.3.md` §4 #8 said: *"對它呼叫 96 次 DeactivateMember + RegisterMember
toggle 循環 ... assert 第 96 次 toggle 後 force-rebaseline branch 觸發"*.

**Reality, per `ArchSimModelRegistry.cpp:281`:**

```cpp
if (PendingRankAccumulation > MaxRankBeforeRebaseline)  // STRICT >, not >=
```

So the **97-th** cumulative rank-1 patch trips `bNeedsRebaseline = true`, not
the 96-th. 96 itself is the boundary (last non-tripping value).

This is the same class of off-by-one that v0.1.3 caught in AS-07's spec; the
test pins the real production behaviour and the comment block in the test
header (`ArchSimRebaselineTest.cpp` near the IMPLEMENT macro) cite
`ArchSimModelRegistry.cpp:281` directly.

#### Correction 2 — GI guard pre-empts trip path in headless

Subagent's deeper read of `RequestSolve` found:

```cpp
// ArchSimModelRegistry.cpp:271-275
MergePatch(PendingPatch, Patch);
PendingRankAccumulation += PatchRank(Patch);   // accumulator updated FIRST
UGameInstance* GI = GetGameInstance();
if (!GI) return;                                // early-return BEFORE trip check
UWorld* World = GI->GetWorld();
if (!World) return;
```

In a headless `-nullrhi -unattended` test fixture, `NewObject<UArchSimModelRegistry>()`
returns an instance with no `GameInstance` outer (this matches the v0.1.3
`FArchSimMaxRankCeilingTest` precedent). `GetGameInstance()` returns null →
`RequestSolve` early-returns at L275 → **the trip check at L281 is never
reached, the timer is never scheduled, and `ExecuteSolve` is never called**.

But the accumulator update at L272 IS unconditional. So in headless we can
verify:

- ✅ `MaxRankBeforeRebaseline = 96` constant via `GetMaxRankBeforeRebaseline()`
- ✅ `PatchRank` math via repeated single-rank-1 calls (96 calls → accum = 96)
- ✅ The direction of the strict `>` boundary at 96 (boundary stays, doesn't trip)
- ✅ Multi-rank single-patch increments correctly (one `DeactivateMemberIds` array
  with 97 entries → accum = 97 in one call)
- ✅ `IsRebaselineDue()` getter is `const noexcept`-pure
- ✅ Empty patch (`PatchRank == 0`) does not perturb accum
- ❌ **Cannot** verify in headless: `bNeedsRebaseline = true` flag set,
  `ExecuteSolve()` invoked, `Sub->Rebaseline()` call, accum reset by trip path

The test's 7 sub-checks are designed around what IS observable. Sub-check 4
explicitly asserts the headless-observable behaviour (accum = 97, flag = false
because trip code never ran), and a comment block at the test header documents
**why** this is the honest pinning rather than the spec's wish.

### Why this still closes AS-10

The unit's stated goal was to "真實 PendingRankAccumulation ceiling test". The
test verifies the accumulator semantics, the constant value, the strict-`>`
direction, and the headless trip-path obstruction — all real production
properties. The full trip-path verification (the `Sub->Rebaseline()` call site)
requires a live `UGameInstance` + PIE world, which is a separate testing
modality (PIE world fixture, deferred to a future sprint as AS-13).

---

## 4. NITS opened during AS-10's adversarial review (logged in `docs/logs/S-02/manager.md`)

| ID | Title | Why deferred | Severity |
|---|---|---|---|
| AS-11 | Header comment precision for rebaseline reset points | cosmetic doc, ≤ 1h | LOW |
| AS-12 | `GetMaxRankBeforeRebaseline()` static constexpr — production consumer | borderline anti-goal (test-only consumer today); resolution path is to add HUD/heatmap "rank budget" indicator OR document TODO | LOW |

A third LOW finding (`ARCHITECTURE_INDEX.md` §8 cheat-sheet stale `137/135` →
should be `138/136`) was **fixed inline** before this release commit; it is
not a deferred item.

---

## 5. 5-leg gate evidence

```
[1/5] standalone FrameCore gate (build.bat)...
       standalone: ALL PASS  (failures=0) (exit 0)
[2/5] UE headless automation...
       UE automation: 138 tests run, exit code 0 (process exit 0; expected >= 138)
[3/5] OpenSees offline cross-validation...
       OpenSees compare: PASS (exit 0)
[4/5] linear-analysis deep audit...
       linear deep audit: PASS failures=0 checks=104 (exit 0)
[5/5] CLI round-trip (frame_cli J1 bridge)...
       CLI round-trip: ALL PASS  (failures=0) (exit 0)
======================================================
 GATE: PASS  (standalone OK, UE 138 tests green, OpenSees PASS, deep audit OK, CLI round-trip OK)
```

Run by AS-10 subagent on cuDSS host immediately after the `$ExpectedUeTests`
bump landed. AS-02a's earlier gate run (before AS-10 work) also reported
`UE 137 tests green` — both runs honest.

**Non-cuDSS expectation:** `.\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 136`
on a host without cuDSS (F67 / F67s skipped at build time when `FRAMECORE_CUDA=0`).

---

## 6. Process narrative (`/work` sprint S-02 first two units)

`v0.1.4` was produced through Sprint S-02 of the `/work` 7-phase chain:

- **Phase 0** locked the scope contract for the whole session (8 dispatch
  units, demo-ready target, v0.2.0 minor at AS-03d close).
- **Phase 1** decomposed into 8 units with a DAG; AS-02a → AS-10 → AS-02b ...
- **Phase 2** dispatched AS-02a (subagent budget 200K / 40 steps / 25min;
  used 114K / 37 / 14.4min)
- **Phase 3** adversarial reviewer Read 5 files, grep'd 9 patterns,
  cross-checked 7 [VERIFIED] claims → CLEAN
- **Phase 4** (this) lightweight commit-only (no tag, bundles)
- **Phase 2** dispatched AS-10 (budget 180K / 35 / 25min; used 147K / 45 /
  15.5min — tool-call overshoot 28% from deeper code reading)
- **Phase 3** adversarial reviewer found GI-guard pre-emption → NITS verdict
  (3 LOW, no BLOCKER)
- **Phase 4** (this release) bundles both into `v0.1.4`

Sprint logs preserved under `docs/logs/S-02/`:

- [`scope_2026-06-26T1033.md`](logs/S-02/scope_2026-06-26T1033.md) — 6-axis contract
- [`plan_2026-06-26T1033.md`](logs/S-02/plan_2026-06-26T1033.md) — 8-unit DAG + budgets
- [`agent_AS-02a.md`](logs/S-02/agent_AS-02a.md) — dispatch + return + review
- [`agent_AS-10.md`](logs/S-02/agent_AS-10.md) — dispatch + return + review + spec correction
- [`manager.md`](logs/S-02/manager.md) — append-only event log

---

## 7. Continuation pointer

- **Handoff:** [`docs/HANDOFF_v0.1.4.md`](HANDOFF_v0.1.4.md) — Z-01 first action for AS-02b
- **Architecture index:** [`docs/ARCHITECTURE_INDEX.md`](ARCHITECTURE_INDEX.md) — updated §2/§6/§7
- **Sprint S-02 continues** in the same session (next unit: AS-02b Tick driver loop)
- **Prior anchor:** [`docs/RELEASE_v0.1.3.md`](RELEASE_v0.1.3.md) | [`docs/HANDOFF_v0.1.3.md`](HANDOFF_v0.1.3.md)
