# RELEASE v0.1.3 — Architect Simulator game body (patch: AS-07 + MaxRank semantic correction)

> **Tag:** `v0.1.3` · **Date:** 2026-06-25 · **Repository:** rocky59487/architect_simulator
> **Engine baseline:** FrameCore `v4.0.0` (FROZEN) + LevelSim `v1` (untouched)
> **This release adds:** one new UE automation test (`ArchSim.Persistence.MaxRankCeiling`)
> + corrects a documented misunderstanding about what `MaxRankBeforeRebaseline = 96`
> actually means.

---

## 1. What v0.1.3 is

A patch release with two intertwined deliverables:

1. **AS-07 closure** — `FArchSimMaxRankCeilingTest` (`ArchSim.Persistence.MaxRankCeiling`)
   stress-tests `UArchSimModelRegistry::RegisterMember` at 97 sequential registrations
   (one past A1-07's quoted "96 ceiling"). 7+ sub-assertions pin the **real**
   production semantic in code.
2. **Spec correction (was: 鐵則 #3 honest-verify failure latent in v0.1.1 / v0.1.2 docs)**
   — `MaxRankBeforeRebaseline = 96` had been *informally documented* across A1-07's
   inline comment and v0.1.1 / v0.1.2 HANDOFF as a "register-count ceiling". This
   release explicitly corrects that:

> **What `MaxRankBeforeRebaseline = 96` actually bounds:**
> `PendingRankAccumulation` inside `UArchSimModelRegistry::RequestSolve`
> (`ArchSimModelRegistry.cpp:281-287`), where "rank" = the running count of
> Deactivate/Reactivate toggles on the patch ladder. `RegisterMember`
> (`ArchSimModelRegistry.cpp:133-208`) has **no register-count ceiling whatsoever**.
> A1-07's `RegisteredCount <= 96` assertion was vacuous (it tested 5 members,
> trivially less than 96, against a ceiling that did not gate the surface it tested).

**Two files touched, zero engine source:**

| File | Lines | Notes |
|---|---|---|
| `Source/ArchSim/Private/Tests/ArchSimSaveLoadTest.cpp` | +160 | Appends `FArchSimMaxRankCeilingTest` (lines 261-421); 25-line header comment cites every relevant production line by file:line; defensive self-guard ensures a future refactor that adds a real register-count ceiling fails loud on the 97th register |
| `Scripts/run_gate.ps1` | 2-line edit | line 29 `$ExpectedUeTests` 136 → 137; cumulative-release comment appended with the AS-07 note + corrected MaxRank semantic; non-cuDSS fallback 134 → 135 |

**Source delta vs v0.1.2:**

| Path | Lines changed | Notes |
|---|---|---|
| `Plugins/FrameSolver/Source/FrameCore/` | **0** | 鐵則 #1 FROZEN honoured |
| `Plugins/FrameSolver/Source/FrameCoreUE/` | **0** | |
| `Plugins/LevelSim/` | **0** | 鐵則 #5 honoured |
| `ArchSim.uproject` / `.gitignore` | **0** | 鐵則 #5 honoured |
| `Source/ArchSim/Public/...` / non-test `Private/...` | **0** | **No production code change** — the agent honoured 鐵則 "do not change production to pass a test" and pinned the actual behaviour instead |
| `Source/ArchSim/Private/Tests/ArchSimSaveLoadTest.cpp` | +160 | New test class only; existing `FArchSimSaveLoadRoundTripTest` untouched |
| `Scripts/run_gate.ps1` | 2 surgical edits | Count bump + comment append |
| `Source/ArchSim/ArchSim.Build.cs` | **0** | Test compiles via existing module deps |

---

## 2. AS-07 — what is verified

**Test path:** `ArchSim.Persistence.MaxRankCeiling`

**7+ sub-assertions** in one test, exercised at headless `-nullrhi -unattended`:

| Group | What's checked |
|---|---|
| Setup | Spawn world located from `GEngine->GetWorldContexts()`; fresh `UArchSimModelRegistry` constructed; `RegisteredCount == 0` |
| Loop body (97 iterations) | Per-iteration: spawn `AActor` with `USceneComponent` root + `SetActorLocation` (A1-07 pattern); construct `UArchSimMemberData` component with `RF_Transient` + `RegisterComponent`; `RegisterMember` returns `Idx == i` (contiguous, monotone) |
| Post-loop | `RegisteredCount == 97` (one past the quoted 96 "ceiling"); 97th register's idx is `96` (not `-1`, not rebaselined) |
| Contract pin | Every `FFrameMember::Id` in `[0, 97)` matches its registration order; every `bActive == true` |
| Idempotency | Re-registering an already-registered component returns the same idx without bumping `RegisteredCount` |
| Cleanup | All 97 actors destroyed; no leak |

**Test header** (`ArchSimSaveLoadTest.cpp:261-286`) cites:
- `RegisterMember` definition span: `ArchSimModelRegistry.cpp:133-208`
- `-1` reject conditions: `cpp:135` (nullptr/owner-less), `cpp:161` (zero-length axis), `cpp:139` (idempotent short-circuit)
- `MaxRankBeforeRebaseline = 96` definition: `ArchSimModelRegistry.h:105`
- `PendingRankAccumulation` ceiling check: `cpp:281-287`
- `RequestSolve` early-return without GameInstance: `cpp:274`
- Force-rebaseline branch: `cpp:284` (out of scope for headless transient registry)

**Why no rebaseline branch coverage in this test:** the force-rebaseline path
requires a live `UGameInstance` + `UWorld` + `UFrameInteractiveSubsystem`, which a
transient headless registry lacks (`GetGameInstance()` returns `nullptr` →
`RequestSolve` early-returns at `cpp:274`). Covering that branch needs a different
fixture; deferred as **AS-10** (see §7).

---

## 3. Verification matrix

| Leg | Command | Result | Notes |
|---|---|---|---|
| **5-leg gate (this release)** | `Scripts\run_gate.ps1 -RequireOpenSees` | ✅ `GATE: PASS` | 137 tests run (135 FrameCore + 2 ArchSim), default `-ExpectedUeTests 137` now matches |
| `ArchSim.Persistence.MaxRankCeiling` standalone | `UnrealEditor-Cmd.exe ArchSim.uproject -ExecCmds="Automation RunTests ArchSim.Persistence.MaxRankCeiling; Quit" -unattended -nullrhi -log` | ✅ `Result={成功}` (40 ms wall) | 7+ TestEqual/TestTrue/TestNotNull all green; covers 97-register + idempotent re-call |
| `ArchSim.Persistence.SaveLoadRoundTrip` (carried) | Same harness | ✅ PASS | Unchanged from v0.1.1; the v0.1.1 `RegisteredCount <= 96` line is now **explicitly documented** as vacuous (it was, all along), but not removed — leaving it in place preserves bisect history for anyone re-reading v0.1.1's RELEASE notes against v0.1.3's correction |
| Inherited gates (standalone / OpenSees / deep audit / CLI roundtrip) | Same harness | ✅ inherited PASS from v4.0.0 baseline | Engine unchanged |
| UE Editor → Plugins panel visual confirm | (Open UE Editor → Edit → Plugins) | ⚠️ **NOT RUN (human-action)** | Tracked as AS-04; carried from v0.1 |
| non-cuDSS host re-verify | `Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 135` | ⚠️ **NOT RUN this cycle** | Tracked as AS-09; this cycle on cuDSS host |

**Honest scope:**
- The MaxRank semantic correction was discovered *because* AS-07's implementer
  Read the production code instead of trusting the prompt's expected outcomes.
  **A1-07's vacuous assertion shipped in v0.1.1 because no one Read `RequestSolve`;
  the spec mistake was inherited from my prompt to the agent.** Listed as a
  durable lesson in `HANDOFF_v0.1.3.md §5`.
- `PendingRankAccumulation` (the actual `MaxRankBeforeRebaseline` consumer) is
  **not exercised by any oracle** in v0.1.3 — closing this gap needs a fixture
  with a live GameInstance, deferred as **AS-10**.

---

## 4. Reproducibility

```powershell
git clone https://github.com/rocky59487/architect_simulator.git
cd architect_simulator
git checkout v0.1.3

# Plugin restore — same as v0.1 / v0.1.1 / v0.1.2
git clone --branch 4.17 https://github.com/Sixze/ALS-Refactored           Plugins/ALS
git clone --depth 1   https://github.com/unknownworlds/prefabricator-ue5  Plugins/Prefabricator
git clone --depth 1   https://github.com/sinbad/SPUD                      Plugins/SPUD
git clone --depth 1   https://github.com/sinbad/SUQS                      Plugins/SUQS
# Patch the 3 plugins missing EngineVersion (see SPRINT_NOTES Spike 1)

# Build editor
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" ArchSimEditor Win64 Development `
    -project="$PWD\ArchSim.uproject" -waitmutex

# Single-command verification (gate now defaults to 137 expected)
.\Scripts\run_gate.ps1 -RequireOpenSees
# Expect: GATE: PASS  (standalone OK, UE 137 tests green, ...)

# Non-cuDSS host:
# .\Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 135

# AS-07 alone
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "$PWD\ArchSim.uproject" `
    -ExecCmds="Automation RunTests ArchSim.Persistence.MaxRankCeiling; Quit" `
    -unattended -nullrhi -log
# Expect: Result={成功} + EXIT CODE 0
```

---

## 5. Breaking changes

**None for callers.** The spec correction is a **documentation truth-up**, not a
behaviour change. Production code is bit-identical with v0.1.2. Any external
code reasoning about `MaxRankBeforeRebaseline` based on v0.1.1's misleading
description should now treat the constant as the `PendingRankAccumulation`
threshold inside `RequestSolve`, not a registry-size ceiling.

---

## 6. Spec correction notice (what changed in our understanding)

| Document | Old claim (v0.1.1 / v0.1.2) | Reality (verified v0.1.3) |
|---|---|---|
| `docs/RELEASE_v0.1.1.md §2` | "MaxRank ceiling: `RegisteredCount <= 96` (the `MaxRankBeforeRebaseline` ceiling at `ArchSimModelRegistry.h:105`)" | `RegisterMember` has no register-count ceiling. `MaxRankBeforeRebaseline = 96` bounds `PendingRankAccumulation` (patch on/off rank) inside `RequestSolve`, not registry size |
| `docs/HANDOFF_v0.1.1.md §4 AS-07` first action | "loop 註冊 97 個 fake member; assert 第 97 個觸發 rebaseline 或被拒" | 97th register returns `Idx=96` (not rejected, not rebaselined). Rebaseline branch requires live GameInstance — covered by AS-10 |
| `docs/HANDOFF_v0.1.2.md §4 AS-07` | Same as above | Same correction |
| `docs/SPRINT_NOTES.md` S-01 A1-07 row | "`RegisteredCount <= MaxRank=96`" listed as covered | Listed as covered, but the assertion was vacuous on 5 members. The semantic is now genuinely pinned by AS-07 in this release |
| `ArchSimSaveLoadTest.cpp:184` inline (v0.1.1) | "the test spec quotes 'MaxRank=96' for the contract here, so we verify the equivalent observable: registered count <= 96" | Same line still present in v0.1.3 (left untouched to preserve bisect/blame across the correction); AS-07's new header comment supersedes the explanation |

**Per the skill's frozen-history rule** (`docs/HANDOFF_v<prev>.md` is never
modified), the v0.1.1 / v0.1.2 docs are left as-is and this RELEASE notes file
acts as the public correction record. Anyone reading v0.1.1 in isolation will
not see the correction; they'll see it the moment they look at v0.1.3.

---

## 7. Deferred items (audit IDs)

AS-07 is **closed** by this release. Carrying forward from v0.1.2 backlog with
one new entry (AS-10):

| ID | Item | Deferred to |
|---|---|---|
| ~~AS-01~~ | ~~`run_gate.ps1` ArchSim namespace~~ | **✅ closed in v0.1.2** |
| AS-02 | A1-06 full integration test | S-02 (v0.2) |
| AS-03 | A2-01 ALS pawn integration | S-02 (v0.2) |
| AS-04 | Gate 0 UE Editor → Plugins panel visual confirm | Human action, any time |
| AS-05 | K1-T2 / K4 art assets | S-02 / S-03 |
| AS-06 | SPUD UE5.5 `StructUtils` deprecation | Before any UE5.7 → UE5.8 upgrade |
| ~~AS-07~~ | ~~A1-07 MaxRank stress test~~ | **✅ closed in v0.1.3 (with spec correction)** |
| AS-08 | SPUD orchestration `RF_Transient` audit | When wiring SPUD orchestration |
| AS-09 | Re-verify gate on non-cuDSS host with `-ExpectedUeTests 135` | Next opportunity on non-cuDSS box |
| **AS-10 (new)** | **Genuine `PendingRankAccumulation` ceiling test — exercise the force-rebaseline branch at `ArchSimModelRegistry.cpp:284` by toggling Deactivate/Reactivate on a registered patch 96+ times with a live GameInstance fixture** | S-02+ |

---

## 8. Tag plan

```bash
git add Source/ArchSim/Private/Tests/ArchSimSaveLoadTest.cpp \
        Scripts/run_gate.ps1 \
        docs/RELEASE_v0.1.3.md \
        docs/HANDOFF_v0.1.3.md \
        README.md
git commit -m "release: v0.1.3 -- AS-07 + MaxRank semantic correction (engine FROZEN)"
git tag -a v0.1.3 -m "Architect Simulator game body v0.1.3 -- MaxRankCeiling + spec correction"

git push origin main
git push origin v0.1.3
gh release create v0.1.3 \
    --title "Architect Simulator game body v0.1.3" \
    --notes-file docs/RELEASE_v0.1.3.md
```

---

*v0.1.3 is the fourth game-body release in a single day (v0.1 → v0.1.1 → v0.1.2 → v0.1.3).*
*This is the release where "honest oracle" pays its first dividend: a vacuous assertion*
*from two cycles ago gets caught + replaced with a contract pin, and the documentation*
*adjusts publicly to match reality.*
