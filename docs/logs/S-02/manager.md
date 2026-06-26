# Sprint S-02 — Manager Log

> Append-only log kept by the /work session driver. Each entry is a single
> event (unit accept, unit reject, scope change, retrospective, decision).
> Owned by main thread, not subagents.
>
> **Sprint start:** 2026-06-26
> **Target:** v0.1.3 → v0.2.0 minor (game body demo-ready foundation)
> **Scope contract:** [`scope_2026-06-26T1033.md`](scope_2026-06-26T1033.md)
> **Execution plan:** [`plan_2026-06-26T1033.md`](plan_2026-06-26T1033.md)

---

## 2026-06-26T10:33 — Session 1 opened (Phase 0)

- Scope contract locked (3-tier Q-loop:goal=demo-ready / risk=safe-main+spike / 8 dispatch units)
- Plan approved (8 units sequential, AS-02a → AS-10 → AS-02b → AS-02c → AS-03a → AS-03b → AS-03c → AS-03d)
- v0.2.0 tag target at AS-03d close

## 2026-06-26T11:00 — AS-02a accepted CLEAN (Phase 3 review)

- Adversarial reviewer Read 5 files, grep'd 9 patterns, cross-checked 7 [VERIFIED] claims with file:line — all confirmed
- 8/8 plan adversarial_focus dimensions covered
- 鐵則 compliance: FROZEN 0 行 / Never-touch 0 行 / no stub / no fabricated [VERIFIED]
- 3 hidden assumptions noted (all reasonable;`bIsActive` non-atomic OK because UE convention is GT-only Tick)
- Decision: accept CLEAN, no NITS, advance to Phase 4

## 2026-06-26T11:05 — AS-02a committed `d229140` (no tag)

- 3 files committed explicitly:`ArchSimGameInstance.h/.cpp` + `Config/DefaultEngine.ini`
- **NO tag** (production-only;bundles with AS-10 into v0.1.4)
- Anti-goal compliance verified at commit time: explicit per-file `git add`, no `-A`/`.`, no FROZEN paths, no .uproject/.gitignore touch
- **Side-finding:** `docs/ARCHITECTURE_INDEX.md` is untracked in git (orphan since some earlier session) — flagged for v0.1.4 inclusion

## 2026-06-26T11:35 — AS-10 accepted NITS (Phase 3 review)

- Adversarial reviewer Read 7 files, grep'd 3 patterns, cross-checked 6 claims
- 8/8 plan adversarial_focus dimensions covered (8 partial: cite line numbers changed from `cpp:298-304` to `cpp:303/315/324/331` — equivalent semantics)
- 鐵則 compliance: FROZEN 0 行 / cpp 0 行動 (key check, adversarial #7) / Never-touch 0 行
- **Critical finding by reviewer:** test is honest partial coverage in headless mode
  - ✅ Pins: `MaxRankBeforeRebaseline=96` constexpr, accumulator math (`+= PatchRank`), off-by-one direction
  - ⚠️ Cannot verify in headless: `bNeedsRebaseline=true` flag set, `ExecuteSolve()` invoked, `Sub->Rebaseline()` call
  - Reason: `RequestSolve` GI-null guard at `cpp:274-275` fires BEFORE trip check at `cpp:281` → headless `NewObject` fixture early-returns before trip
  - Honest limitation documented in test header + `ARCHITECTURE_INDEX.md` § 7 AS-10 closure
  - Acceptable per AS-07 lesson #1 (test pins production reality, not spec wish)
- **NITS findings (3 LOW):**
  - Finding #1 (`ARCHITECTURE_INDEX.md` L232 stale 137/135 in cheat-sheet) → **FIXED INLINE** before release (138/136)
  - Finding #2 (header L96 comment cites cpp:303/315/324/331 — borderline precision) → **AS-11 backlog**
  - Finding #3 (`GetMaxRankBeforeRebaseline()` static constexpr has no production consumer yet) → **AS-12 backlog**
- Decision: accept NITS, advance to Phase 4 v0.1.4 release

### NITS backlog items opened

#### AS-11 — Header comment precision for rebaseline reset points

- File:`Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h` L96-area
- Issue:comment cites `cpp:303/315/324/331` for `PendingRankAccumulation = 0` reset points — technically correct but reviewers say maintainability would benefit from per-branch labels (e.g. "L303 = no-Sub path / L315 = session-start-fail path / L324 = post-rebaseline / L331 = end-of-ExecuteSolve")
- Sprint:next available cleanup window (≤ 1h cosmetic)
- Priority:LOW
- Origin:S-02 AS-10 Phase 3 review finding #2

#### AS-12 — `GetMaxRankBeforeRebaseline()` production consumer

- File:`Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h` (new static constexpr getter from AS-10)
- Issue:getter has no production consumer yet (test-only); reviewer notes borderline anti-goal (production code 配合 test)
- Resolution path:
  - (a) Add HUD/heatmap consumer that displays "已用 rank / 總 rank 上限" → telemetry value confirmed
  - (b) Or document expected consumer in `// TODO: consumed by HUD rank-budget indicator (AS-XX)` comment
  - (c) Or remove the getter and let test use magic-96 (但會喪失「one source of truth」property)
- Sprint:bundle into AS-02c (smoke test phase) — could add HUD consumer there if scope allows
- Priority:LOW
- Origin:S-02 AS-10 Phase 3 review finding #3

---

## Open backlog (mid-S-02)

| ID | Title | Origin | Status |
|---|---|---|---|
| AS-02b | Tick driver loop | plan §AS-02b | 🟡 next |
| AS-02c | Smoke test + BP accessors | plan §AS-02c | 🟡 queued |
| AS-03a..d | ALS pawn + Input + Camera + test | plan §AS-03 | 🟡 queued |
| AS-04 | Plugins panel visual check (human) | plan stretch | 🔵 deferred next-session |
| AS-05 | K1-T2 / K4 art | plan stretch | 🔵 deferred next-session |
| AS-11 | Header comment precision | this session (AS-10 NITS #2) | 🟡 backlog |
| AS-12 | GetMaxRankBeforeRebaseline consumer | this session (AS-10 NITS #3) | 🟡 backlog |

---

*Append below this line for each new event.*

## 2026-06-26T11:08 — v0.1.4 tagged (`abf131a`)

- Bundle: AS-02a (UArchSimGameInstance skeleton) + AS-10 (Real PendingRankAccumulation ceiling test)
- Tag annotated locally; publish pending (user action)
- 5-leg gate PASS 138 (cuDSS) / 136 (non-cuDSS)
- README v0.1.4 status block prepended
- ARCHITECTURE_INDEX §6/7/8 synced
- HANDOFF_v0.1.4.md + RELEASE_v0.1.4.md written
- NITS opened: AS-11 (header comment precision) + AS-12 (GetMaxRankBeforeRebaseline production consumer)

## 2026-06-26T12:05 — AS-02b accepted CLEAN

- Tick body extended to detect registered-count delta and emit RequestSolve(empty patch)
- Production logic byte-identical (ArchSimModelRegistry.cpp 0 行動)
- +75 LOC across header (+26) and cpp (+49)
- Adversarial reviewer: 8/8 adversarial_focus CONFIRMED with file:line evidence
- 3 hidden assumptions + 3 missed edge cases logged but all LOW, no action
- Scope narrowing pre-agreed: dirty = registered-count delta (not actor position movement)

## 2026-06-26T12:21 — AS-02c accepted CLEAN

- New ArchSim.Integration.TickDriver test (147 LOC, 7 sub-checks)
- $ExpectedUeTests bump 138 → 139 (cuDSS) / 136 → 137 (non-cuDSS)
- Adversarial reviewer: flag deviation (SmokeFilter vs prompt template's ProductionFilter) verified as CORRECT precedent match (SaveLoadTest L82 + RebaselineTest L73 + MaxRankCeiling L288 all use SmokeFilter)
- Headless limitation honest: driver-loop branch unreachable because GetSubsystem returns null in NewObject fixture — deferred to PIE-world AS-13

### AS-13 opened (new backlog)

#### AS-13 — PIE-world fixture for driver-loop + trip-path observability

- Need: AS-10 trip-path (`bNeedsRebaseline=true` + `ExecuteSolve()` + `Sub->Rebaseline()`) AND AS-02 driver-loop (`Tick() → GetSubsystem → Registry`) both require a live GameInstance pipeline that headless `NewObject<T>()` does NOT provide
- Resolution path:
  - Spawn a PIE world via `FAutomationEditorCommonUtils::CreateNewMap` or equivalent
  - Place 96+ `UArchSimMemberData` actors; trigger BeginPlay
  - Tick the world for several frames
  - Assert `Registry->IsRebaselineDue()` flipped + `GameInstance->GetSolveTriggerCount()` incremented
- Sprint: defer to S-03 or later (needs PIE automation expertise)
- Priority: MEDIUM (test gap, not a production bug)
- Origin: S-02 AS-10 closure (v0.1.4) + AS-02c CLEAN review note (v0.1.5)

## 2026-06-26T12:25 — v0.1.5 tagged (bundles AS-02b + AS-02c)

v0.1.5 tagged `2935e71`. Bundles AS-02b + AS-02c. 5-leg gate PASS 139.

## 2026-06-26T12:42 — AS-03a accepted CLEAN

- AArchSimCharacter subclass AAlsCharacter (foundation)
- 3 files: header / cpp / Build.cs (ALS dep added Public)
- UE build 8.11s OK (ALS link verified)
- Phase 3 reviewer: 8/8 adversarial dimensions verified file:line
- Feature commit `ea44014`, no tag (bundles to v0.2.0)

## 2026-06-26T13:00 — AS-03b accepted CLEAN+3 LOW NITS

- Enhanced Input: 5 IA + 1 IMC TObjectPtr slots + 7 handler bodies
- HandleMove view-space (matches AAlsCharacterExample::Input_OnMove)
- docs/INPUT_MAPPING.md UAsset spec for user to create in Editor
- UE build 4.54s OK / gate PASS 139
- NITS:
  - N-01: HandleMove missing `ClampMagnitude012D` (LOW — analog stick prep) → **AS-14 backlog opened**
  - N-02: Sprint edge-trigger vs ALS example every-frame (LOW — design choice, no backlog)
  - N-03: include order style (LOW — no backlog)
- Feature commit pending (will commit after this entry)

### AS-14 opened (new backlog)

#### AS-14 — Analog stick / gamepad input normalization

- File: `Source/ArchSim/Private/Characters/ArchSimCharacter.cpp` HandleMove
- Issue: missing `UAlsVector::ClampMagnitude012D(Value.Get<FVector2D>())` clamp before passing to view-space movement; ALS example does this in `Input_OnMove` L1
- Impact: keyboard W+D gives raw `(1,1)` norm 1.41, but `AddMovementInput` normalizes internally so functionally OK. Becomes load-bearing for analog stick / gamepad — corner stick travel would over-drive.
- Sprint: AS-03d Phase 4 cleanup OR next gameplay polish window
- Priority: LOW
- Origin: S-02 AS-03b Phase 3 NITS #1

## 2026-06-26T13:25 — AS-03c accepted NITS+inline fix

- Camera DefaultSubobject (UAlsCameraComponent attach to Mesh) + AArchSimGameMode (AGameModeBase, DefaultPawnClass wire) + Config GlobalDefaultGameMode
- 6 files: char .h/.cpp / Build.cs (ALSCamera dep) / GameMode .h/.cpp / DefaultEngine.ini
- UE build 8.1s zero warning, gate PASS 139, UHT reflection confirmed
- Phase 3 NITS-01: agent claim "_Direct not on USkeletalMeshComponent" was incorrect; ALS example uses `SetRelativeRotation_Direct({0,90,0})`. Roll drift -90 vs 0 — fixed inline before commit
- Phase 3 NITS-02 dismissed (Build.cs diff context confusion, not real)
- Tool calls 59/40 = +47% overshoot. 50-step hard cap not enforced — **hook configuration gap** logged for Phase 6 retrospective
- Feature commit `c116760`

## 2026-06-26T13:46 — AS-03d accepted CLEAN

- Final AS-03 unit. New `ArchSimCharacterTest.cpp` (130 LOC, 7 sub-checks, 24 assertions) in `ArchSim.Gameplay.CharacterInput`
- Verifies class hierarchy (AAlsCharacter → ACharacter → APawn), GameMode inherits AGameModeBase, DefaultPawnClass wire (TSubclassOf workaround via TestTrue), AS-03a controller-rotation flags, AS-03c UAlsCameraComponent subobject, AS-03b 6 Enhanced Input UPROPERTY null slots, LogArchSim link symbol, reflection names
- $ExpectedUeTests bump 139 → 140 (cuDSS) / 137 → 138 (non-cuDSS)
- Honest headless limitation: ALS state machine + Enhanced Input runtime + camera attachment runtime + actor movement all deferred to AS-13 PIE fixture
- UE build 2.71s, gate PASS 140, test single-run `Result={成功}` exit 0
- Phase 3 NITS-01 (extra IMC verification = positive) + NITS-02 (sub-check 6 link-time signal) both LOW, accepted

## 2026-06-26T13:48 — v0.2.0 minor bump tagged

- **AS-02 + AS-03 + AS-10 trilogy complete this Sprint S-02**
- Bundle: 4 AS-03 feature commits (a/b/c/d) atop v0.1.5
- Production code delta across S-02: ~530 LOC (ArchSimGameInstance + ArchSimCharacter + ArchSimGameMode + 3 telemetry getters)
- Test delta: 3 new tests (RebaselineCeiling / TickDriver / CharacterInput) = +3 sub-checks namespace 4→5, gate count 137→140
- Engine source delta this Sprint = 0 (FrameCore v4.0.0 FROZEN honored; LevelSim v1 FROZEN honored)
- ArchSimModelRegistry.cpp delta = 0 (production logic byte-identical throughout)
- 8 dispatch units fired: AS-02a / AS-10 / AS-02b / AS-02c / AS-03a / AS-03b / AS-03c / AS-03d
- Adversarial reviews: 6 CLEAN + 2 NITS (1 fixed inline, 1 deferred to backlog)
- Backlog opened during sprint: AS-11 (header comment precision) + AS-12 (GetMaxRankBeforeRebaseline consumer) + AS-13 (PIE-world fixture) + AS-14 (analog stick clamp)
- 5-leg gate PASS 140 on cuDSS host

## 2026-06-26T14:30 — release-hardening pass on v0.2.0 (Phase 0-5)

Triggered by user `/release-hardening` invocation immediately after the v0.2.0
tag was cut locally (still unpushed). Phase 1 fired 7 parallel adversarial
audit subagents (A numerics / B claim cross-check / C bridge consumer / D
UI-FSM glue / E docs / F cleanliness / G repro+privacy+handoff). 87 raw
findings.

**Findings disposition:**

- 2 dismissed as false positives (B-01 cascade — unanchored grep matched a
  comment line; B-03/B-12 fold)
- 5 INFO / no-action
- 23 acted on inline in this hardening pass (16 doc + 5 small-fix code +
  2 sanitisation sweeps)
- 14 deferred to backlog as AS-15..AS-19 (real follow-ups but not
  release-scope; require ≥30 LOC or new test harness)

**Privacy / reproducibility cleanup actions:**

- G-05 (BLOCKER): reviewer agent ID `[redacted]` removed from
  `RELEASE_v0.2.0.md`
- G-06: 16 agent IDs across 8 sprint logs replaced with `[sanitized]`
- G-13: 4 scratch logs (`_as03b_build.log` / `_as03b_gate.log` /
  `build_as03c.log` / `gate_as03c.log`) deleted from the working tree
  (untracked, no git history impact). `.gitignore` left unchanged per
  iron rule #5 (`.gitignore` is a never-touch path)
- G-01/G-02: `ARCHITECTURE_INDEX.md` § 8 cheat-sheet hardcoded
  `E:\project\ArchSim` → `$PWD`
- G-03: § 9 title `verbatim from E:\project\CLAUDE.md` → "from the
  project root CLAUDE.md"
- G-04: `agent_AS-02b.md` Gotcha sanitized
- G-07 + G-08: `RELEASE_v0.2.0.md` § 5 gained an explicit Reproduce block
  with conda + UE_ENGINE_ROOT prerequisites named on the line that uses
  them
- G-12: HANDOFF Z-02/Z-04 first-actions sharpened from vague phrasing to
  concrete "open UE Editor → ..." steps

**Doc consistency fixes:**

- E-01: § 10 Quick links advanced v0.1.3 → v0.2.0
- E-02: § 5 Data-flow "Gaps as of v0.1.3" rewritten to "Wire status as of
  v0.2.0"
- E-03: § 9 iron rule #2 UE count 137/135 → 140/138
- B-02: `RELEASE_v0.2.0.md` + HANDOFF + README LOC delta 287 → 394
  (table per-file numbers also corrected)
- E-05: this section (Sprint S-02 high-level pointer in
  `docs/SPRINT_NOTES.md`)
- E-06: this entry in `manager.md`

**Small production fixes (this hardening pass, additive guards only):**

- **C-01** (real bug) `ArchSimModelRegistry.cpp` DeactivateMember now
  resets `Comp->bRegistered = false; Comp->MemberIdx = -1;` so a PIE
  restart can re-register fresh members instead of short-circuiting on
  the `bRegistered` idempotency guard with a stale MemberIdx.
- **C-05** (defensive) RegisterMember's session-restart path now drops
  the queued `PendingPatch` and resets `PendingRankAccumulation`. The
  existing `bSessionStarted = false` only invalidated the engine session;
  the patch queue would otherwise carry stale Deactivate ids into the
  next ExecuteSolve against a freshly-rebuilt model.
- C-03 / C-07 comment corrections in `ArchSimGameInstance.cpp` Tick
  body — the cited cpp line ranges and the `<` edge-case description
  now match production behavior.

**5-leg gate** re-run after the production fixes: GATE: PASS (UE 140
tests, standalone F1..F71 ALL PASS, OpenSees PASS, deep audit 104,
CLI round-trip ALL PASS). Confirms the additive guards do not perturb
any oracle.

**Deferred to Sprint S-03 backlog (AS-15..AS-19):**

- AS-15 (HIGH): refit Enhanced Input lifecycle via
  `NotifyControllerChanged` + `RemoveMappingContext` + `Canceled` event
  bindings + `bNotifyUserSettings`. Bundles A-02 / D-01 / D-02 / D-03 /
  D-06. ~30-50 LOC.
- AS-16 (HIGH): override `CalcCamera` on `AArchSimCharacter` to route
  through `UAlsCameraComponent->GetViewInfo` per the ALS example.
- AS-17 (MEDIUM): audit and either prove or guard the
  empty-`CurrentModel` `StartSession` path (C-02).
- AS-18 (LOW): document the cross-subsystem teardown ordering between
  `UArchSimModelRegistry` and `UFrameInteractiveSubsystem` (C-04).
- AS-19 (LOW): retry / explicit-warning path when `MemberData::BeginPlay`
  fires before the `GameInstance` is ready (C-06).

## 2026-06-26T14:40 — v0.2.0 retagged at hardened commit

Per release-hardening doctrine (local-only tags can be moved before the
first publish without violating the "no force overwrite without
authorisation" rule), the v0.2.0 tag was moved from `bd507a2` (original
sprint-close commit) to the hardened HEAD that includes the privacy
sanitisation + 2 small production fixes + doc consistency repairs. The
`bd507a2` commit remains in the history as the AS-03d feature commit,
but the v0.2.0 release tag now points at the cleaned release commit.

Net result: the published `v0.2.0` carries zero agent-session fingerprints,
zero hardcoded user paths in shipped docs, correct LOC numbers, and 2
small bug fixes for stale-state edge cases discovered during the audit.

Sprint S-02 closed.
