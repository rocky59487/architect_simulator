# v0.5.0 — Sprint S-06 close (v0.4 殘留全部收完;Scenario fixture + ALS character wiring + IWYU validator + LOW cleanup)

**Date:** 2026-06-28
**Commit:** (filled at tag-time release commit)
**Tag:** `v0.5.0`
**Repository:** https://github.com/rocky59487/architect_simulator
**Prior tag:** [v0.4.0.1](RELEASE_v0.4.0.1.md)(S-05 Scenario MVP cross-world hotfix)
**Session log:** [docs/logs/S-06/manager.md](logs/S-06/manager.md)

---

## 1. 一句話 elevator pitch

`v0.5.0` 收完 v0.4 series 全部 known-residual 工作(ALS PIE character spawn / Scenario fixture for solvable model / IWYU silent-stale-obj prevention / hook + ARCH_INDEX cleanup),讓 v0.x 線從**乾淨 baseline 開始**進入 next-gen feature scope(S-07+)。`v0.5.0` 是 **Mode A minor bump direct from `v0.4.0.1`**,沒有中間 patch tags;4 mid-sprint feature commits 直接 land 為 minor。FrameCore engine FROZEN since v4.0.0 持續零變動(`Plugins/FrameSolver/Source/FrameCore/` git diff = 0)。

---

## 2. Engine / subsystem highlights

### 2.1 Scenario MVP — boundary support API + portal-frame fixture (AS-30, v0.5.0 主軸)

- **`UArchSimModelRegistry::RegisterFixedSupport(FVector PosMm) -> int32`** — register a fully-fixed support node;internally calls existing private `FindOrAddNode(PosMm)` (1 mm linear-scan dedupe)+ sets `FFrameNode.Fixed.Init(true, 6)`(全 6 DOF lock)。Idempotent:重複呼叫同 PosMm 回同 NodeIdx + Fixed 仍 all-true。**Does NOT auto-trigger Solve** — caller batches supports + members and relies on RegisterMember's 150 ms debounce timer。
- **`UArchSimScenarioWidget::PlaceFixedSupport(FVector LocationWorld) -> int32`** — WITH_EDITOR BP-callable;world→mm conversion(`Location * 10`)+ PIE-world preference(`GEditor->PlayWorld.Get()` first per v0.4.0.1 cross-world pattern)+ Registry 呼叫。
- **`UArchSimScenarioWidget::SpawnDefaultPortalFrame() -> bool`** — convenience composer:2 fixed supports at base (±100, 0, 0) cm + 2 K1 columns (2 m height) + 1 K2 beam (2 m span)。Node-snap via Registry::FindOrAddNode 1 mm tolerance auto-shares corner nodes(4 unique nodes from 8 endpoint candidates)。回 `true` 若全部 5 sub-placements 成功。
- 新 test class `FArchSimScenarioFixtureTest` at `ArchSim.Gameplay.ScenarioFixture` — 7 sub-checks(reflection / RegisterFixedSupport / node-snap dedupe / idempotent Fixed / transient widget graceful-fail / [NEW CODE, PIE required] AddInfo for PIE smoke oracle)。`$ExpectedUeTests` 148 → 149。
- Closes scope contract hard-requirement「PIE smoke P10/P11 mechanism FAIL」— SpawnDefaultPortalFrame 產 4-node 3-member 12-free-DOF system(2 fixed bases × 6 + 2 top corners × 6),3× statically indeterminate,K matrix 12×12 well-conditioned for LDLT 不再是 mechanism。

### 2.2 ALS character PIE startup fix (U-ALS)

- **Root cause [INFERRED from ALS L400 code pattern; supported by ConstructorHelpers fail evidence at `Saved/Logs/ArchSim-backup-2026.06.28-03.37.00.log:L916-929`]**:`AArchSimGameMode::DefaultPawnClass = AArchSimCharacter::StaticClass()` 直接 spawn C++ class 沒 BP child wiring AnimBlueprint → `PostInitializeComponents()` 時 `GetMesh()->GetAnimInstance()` 回 null → `PossessedBy()` → `RefreshMeshProperties()` L400 unguarded `AnimationInstance->MarkPendingUpdate()` → EXCEPTION_ACCESS_VIOLATION。
- **Layer 1 fix (tracked source):** `AArchSimCharacter::PostInitProperties()` + `BeginPlay()` overrides + `LoadAlsAssetsLate()` helper 用 `LoadObject<T>()` runtime-late timing 載 4 asset(Settings / MovementSettings / SkeletalMesh / AnimBlueprint generated class)。BeginPlay fallback 必須在 `Super::BeginPlay()` **前**呼叫,因 ALS L138-146 fires `ALS_ENSURE(IsValid(Settings))` inside Super。
- **Layer 2 fix (third-party patch artifact):** `Tools/patches/als_l400_animinstance_guard.patch` — 11 LOC `AnimationInstance.IsValid()` guard at AlsCharacter.cpp:L411,匹配 ALS 自身在 6 處(L141/156/215/260/271/1512)既有 guard pattern。`Plugins/ALS/` 是 untracked-third-party convention(670 MB 不進 git);fresh checkouts must `git apply Tools/patches/als_l400_animinstance_guard.patch`,詳 [Tools/patches/README.md](../Tools/patches/README.md)。
- ARCH_INDEX § 2 `AArchSimCharacter` row 加 S-06 U-ALS sub-paragraph 文件化 runtime-late asset wiring 與 BeginPlay timing trick。

### 2.3 IWYU first-header pre-commit validator (U-IWYU)

- 預防 v0.4.0.1 級「silent stale-obj」災難:UE5.5+ enforce 「each `.cpp` first `#include` must be matching header」,違反時 UBT log 報錯但 target build 仍 `Result: Succeeded` + 沿用 stale `.obj` → 修正 invisible through rebuilds。
- `Tools/check_iwyu_first_header.py`(+194 LOC):CLI validator,scan full `Source/**/*.cpp` < 1 s(暖機後 0.25 s;cold-start subprocess ~1.2 s),stem endswith match 接受 module 前綴,skip Tests/ + FROZEN paths + Marshal cpp without same-stem header。
- `Tools/test_iwyu_validator.py`(+229 LOC):7 fixtures(positive/negative v0.4.0.1 pattern/empty/comment-only/pragma-only/test file skip/FROZEN skip)+ setUp/tearDown reset cache。
- `docs/IWYU_VALIDATOR.md`(+108 LOC):5-section README(What+Why / Usage / Pre-commit hook 安裝 / Exceptions / CI hints)。
- `.git/hooks/pre-commit`(本機 only,not tracked):grep staged `.cpp` via `git diff --cached --name-only`,no-cpp early exit 0,不擋無關 commit。

### 2.4 Hook + ARCH_INDEX cleanup (U-LOW)

- `~/.claude/hooks/work-phase-guard.ps1` L114 `-notmatch` → `-cnotmatch` + WHY block:case-sensitive match;`s-XX` lowercase 現正確被視為 foreign(原 `-notmatch` 預設 case-insensitive 會誤 match `^S-` pattern)。`.bak` regenerated bit-identical;5-scenario stdin test PASS(idle / S-06 uppercase / S-06a suffix / s-06 lowercase fail-open / shop foreign / malformed)。
- ARCH_INDEX § 7 backlog:AS-28 closed(本 unit 自身)/ AS-29 description 精化只留 PS env race / NEW AS-30 row HIGH priority(後由 AS-30 unit closed in same release)。
- HANDOFF_v0.4.0.1.md L49 inline note 雙向 cross-reference 防 future session 混淆 AS-29 vs AS-30 ID collision。

---

## 3. Small-fixes folded into this release (Phase 3 NIT 收尾)

每個 AS-XX accepted with NITS;cosmetic fixes batched into Phase 5 → 本 release commit:

| Source | NIT description | Fix in this release |
|---|---|---|
| AS-30 Phase 3 F1 | `docs/logs/S-05/u3_pie_smoke.md` 3 處 `148/146` stale | 改 `149/147`(L28 expected / L187 P2 / L270 §9 template) |
| AS-30 Phase 3 F2 | `Source/ArchSim/Private/Editor/ArchSimScenarioWidget.cpp:L448-461` DoF comment 「`0 global free DOFs`」與「`12 free DOF`」矛盾 | 刪 2D-static word-salad,統一描述為 3D portal frame 3× statically indeterminate / 12 free DOF on 2 top corners |
| AS-30 Phase 3 F3 | ARCH_INDEX § 2 Registry row 沒提 RegisterFixedSupport「does NOT auto-trigger Solve」 | 加 1 句 clarification + cite header L68-69 |

不批 NIT 進 backlog:Phase 5 一併修完;非 v0.5.0+1 follow-up 工作。

**Cosmetic deferred(無 fix this release;Phase 6 close 文件化):**

| Origin | Item | Where |
|---|---|---|
| U-IWYU NIT F1 | `docs/IWYU_VALIDATOR.md:L79` `cp ... # already there` NOP self-copy 行 | Phase 5 docs sync 或 next sprint cosmetic backlog |
| U-IWYU NIT F2 | `.git/hooks/pre-commit:L41` exit code 1 vs 2 訊息不分 | hook maintenance window |
| U-IWYU NIT F3 | `.git/hooks/pre-commit:L38` `$STAGED_CPP` unquoted spaces | hook maintenance window |
| U-ALS iter 2 N1 | `AlsCharacter.cpp:L406` comment 列 6 行號實際 7(L411 + L1512 漏)+ subagent 行號 off-by-2(L409/L1510 vs L411/L1512)| ALS patch refresh window |

---

## 4. Verification matrix

| Leg | 跑了嗎 | Result | Reproduction |
|---|---|---|---|
| [1/5] Standalone(F1..F71,FrameCore standalone build)| ✅ PASS | exit 0,F1..F71 ALL PASS(failures=0)| `Plugins\FrameSolver\Standalone\build.bat` (從 repo root;Windows;需 conda env `framecore-direct` 或 SUPERNODAL_CONDA env override)|
| [2/5] UE automation | ✅ PASS | 149 tests run, exit 0(`$ExpectedUeTests = 149`,cuDSS build;非 cuDSS pass `-ExpectedUeTests 147`)| `& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" ArchSimEditor Win64 Development -project="$PWD\ArchSim.uproject" -waitmutex` 先 build,然後 `.\Scripts\run_gate.ps1 -RequireOpenSees` |
| [3/5] OpenSees compare(`Tools/opensees_compare.py`)| ✅ PASS | exit 0 | 需 system Python + `pip install openseespy`;`python Tools\opensees_compare.py` |
| [4/5] Linear deep audit(`linear_deep_audit.exe`,104 checks)| ✅ PASS | failures=0, exit 0 | `Plugins\FrameSolver\Standalone\build_linear_audit.bat` build + run |
| [5/5] CLI round-trip(`Tools/cli_roundtrip.py`)| ✅ PASS | ALL PASS(failures=0), exit 0 | `python Tools\cli_roundtrip.py` |
| 6 (optional) v2 dispatcher roundtrip | ✅ PASS(at v3.x baseline)| inherits from v4.0.0 anchor | `python Tools\v2_roundtrip.py`(需 frame_capi_v2.dll on PATH)|
| 7 (optional) GPU strict gate | NOT RUN(本 session)| inherits from v3.0.1 anchor evidence | `.\Scripts\run_gpu_gate.ps1 -Strict`(需 cuDSS host + RTX 50-series GPU)|
| **PIE smoke**(USER-DRIVEN per scope contract hard gate)| ⏳ **WAITING USER** | **[NEW CODE, PIE required]** | 5-step instruction in [`docs/logs/S-05/u3_pie_smoke.md`](logs/S-05/u3_pie_smoke.md)P1..P15;**v0.5.0 stamped Latest 直到 user run smoke + 回報 PASS 才宣告「ready for student trial」**(若 PIE smoke FAIL 走 post-publish hotfix protocol v0.5.0.1)|

Full gate command(one-liner):
```powershell
.\Scripts\run_gate.ps1 -RequireOpenSees
# Expect: GATE PASS — UE 149 / OpenSees PASS / audit 104 / CLI PASS / standalone ALL PASS
```

`standalone leg PowerShell env race`(AS-29 backlog):AS-29 known issue 在 PS session 偶爾遇 `[1/5] standalone: exit 1`(蓋因 parallel build `.obj` lock race);workaround:直接 `Plugins\FrameSolver\Standalone\build.bat` 從 repo root 跑 standalone 驗證(本 session 中 U-LOW + U-ALS iter 1 都遇此 race;AS-30 + U-ALS iter 2 都 fresh PASS)。

---

## 5. Honest limitations

### 5.1 Scenario fixture P10/P11 PIE smoke 屬 [NEW CODE, PIE required]
v0.5.0 ship 時 `SpawnDefaultPortalFrame` 在 PIE 真實環境的「heatmap actor spawn + non-trivial colour」尚未由 subagent 驗(no GUI access)。Headless 7 sub-checks 全 PASS(reflection / RegisterFixedSupport / node-snap dedupe / idempotent Fixed / transient widget graceful-fail / [NEW CODE, PIE required] AddInfo)。USER PIE smoke 是最後 validation。

### 5.2 ALS L400 patch 是 third-party plugin patch(非 in-tree change)
Fix B(L411 guard)程式碼在 `Plugins/ALS/Source/ALS/Private/AlsCharacter.cpp`,該檔案在 untracked 第三方 plugin 目錄。canonical commit artifact = `Tools/patches/als_l400_animinstance_guard.patch`。Fresh checkouts MUST manually apply patch — ALS upstream not yet notified(considered filing issue against ALS-Refactored v4.17+,deferred to S-07 cleanup)。

### 5.3 ConstructorHelpers ctor-time wiring 確認 non-functional in Editor
U-ALS iter 1 嘗試 ctor `ConstructorHelpers::FObjectFinder` x4 失敗(ALS plugin content not mounted at CDO);iter 2 改 PostInitProperties + BeginPlay runtime-late LoadObject 成功。Headless log `Failed to find /ALS/` count:iter 1 = 4 / iter 2 = 0。**為何不直接走 BP child path:**避免動 GameMode.cpp DefaultPawnClass 改指 BP child(超出 U-ALS 原 scope);S-07 可重評估若 PostInitProperties path 維護成本過高。

### 5.4 IWYU validator cold-start ~1.2 s 邊緣超 < 1 s spec
首次 Python subprocess invocation 1174 ms(超 spec);暖機後 185-191 ms 全 PASS。CI 第一次跑會在邊緣;後續 reruns 安全。Documented in README;deferred 真實優化(Python startup amortize)to next IWYU iteration。

### 5.5 PIE smoke 完整覆蓋仍需 BP child 為 widget instance
SpawnDefaultPortalFrame 是 `UArchSimScenarioWidget` (Abstract) instance method;PIE 要實際呼叫需 BP child class instantiation per existing `Content/BP_ArchSimScenarioWidget.uasset`(gitignored;見 u3_pie_smoke.md § 3)。AS-30 不創 BP child;若 BP child missing user 須先 create(`setup_pie_smoke_widget.py` 已 ship in v0.4.0.1)。

---

## 6. Breaking changes

**None.** v0.5.0 strictly additive:
- 1 new public Registry API(`RegisterFixedSupport`)— additive
- 2 new public Widget BP-callable methods(`PlaceFixedSupport` / `SpawnDefaultPortalFrame`)— additive
- 1 new UE test class(`FArchSimScenarioFixtureTest`)— additive(`$ExpectedUeTests` 148 → 149)
- 1 new Python tool(`Tools/check_iwyu_first_header.py`)— additive
- 1 third-party plugin patch(ALS L411 guard)— additive defensive-only(不改 ALS public API)
- `AArchSimCharacter` 加 2 new override(`PostInitProperties` / `BeginPlay`)+ 1 helper(`LoadAlsAssetsLate`)— additive(do NOT change existing AS-15 Enhanced Input / AS-16 CalcCamera 行為)

**ABI / API stability:**
- FrameCore `kEngineVer 4.0.0` 不變(FROZEN since v4.0.0)
- FrameSolver `.uplugin VersionName` 不變(engine FROZEN 不需 bump)
- FrameCoreUE plugin public USTRUCT / UCLASS 全不變(只用既有 `FFrameNode.Fixed` 欄位)
- `kAbiVersion=2` 不變(v2 dispatcher wire-protocol 全相容)

---

## 7. Deferred items(audit IDs)

| ID | Origin | One-line | First action |
|---|---|---|---|
| AS-04 | v0.1.3 carry | UE Editor Plugins panel visual | human / parallel — UE editor manual |
| AS-05 | v0.1.3 carry | K1-T2 / K4 art assets | parallel — 3D modelling |
| AS-06 | v0.1.3 carry | SPUD UE5.5 StructUtils deprecation | 🔵 deferred pre-5.8 upgrade(SPIKE-UE5.8 NO-GO eval 2026-06-27) |
| AS-08 | v0.1.3 carry | SPUD orchestration `RF_Transient` audit | wait until SPUD wiring(currently disabled) |
| AS-09 | v0.1.3 carry | Re-verify gate on non-cuDSS host | 🔵 opportunistic |
| AS-29 | S-05 carry | `run_gate.ps1` standalone PowerShell env race diagnosis | LOW;workaround documented(direct `build.bat`)|
| **AS-31 (NEW)** | S-06 cosmetic NIT bundle | docs/IWYU_VALIDATOR.md L79 NOP self-copy / .git/hooks/pre-commit L41 exit-code message conflation / `$STAGED_CPP` unquoted spaces / AlsCharacter.cpp L406 comment off-by-2 | cosmetic cleanup window — fold into next minor or skip if S-07 has weighty work |
| **AS-32 (NEW)** | S-06 follow-up | ALS L411 guard upstream contribution | file ALS-Refactored issue or PR;Tools/patches/ patch 文件化 issue # |
| **AS-33 (NEW)** | U-ALS iter 2 design choice | Evaluate BP child + GameMode.cpp DefaultPawnClass swap vs current PostInitProperties LoadObject path | review PIE smoke results;若 LoadObject pattern 維護成本高 → 切 BP child |
| **AS-34 (NEW)** | AS-30 honest limitation | PIE smoke P10/P11 「heatmap colour」具體 oracle 化 | run u3_pie_smoke 收集 1 次 healthy heatmap baseline → 寫入 expected colour pattern |

**v0.4.0 prerelease note(carry from v0.4.0.1):**`v0.4.0` 仍 marked `--prerelease`;`v0.4.0.1` + `v0.5.0` 是 canonical Scenario MVP path。

---

## 8. Tag plan + publish commands(USER ACTION)

**主執行緒已執行(local only):**
```powershell
git add <explicit files>           # ✓ 9 tracked files(no -A)
git commit -m "release: v0.5.0 ..." # ✓ single release commit
git tag -a v0.5.0 -m "release v0.5.0 -- Sprint S-06 close ..."  # ✓ annotated tag
```

**USER 須執行(publish to GitHub):**
```powershell
git push origin main
git push origin v0.5.0
gh release create v0.5.0 \
    --title "v0.5.0 — Sprint S-06 close: Scenario fixture + ALS character + IWYU + LOW cleanup" \
    --notes-file docs/RELEASE_v0.5.0.md
```

publish 後:
1. 訪問 https://github.com/rocky59487/architect_simulator/releases/tag/v0.5.0 確認 Latest 標記 + release notes render(尤其中文 + code block)
2. **執行 USER-DRIVEN PIE smoke per docs/logs/S-05/u3_pie_smoke.md P1..P15**:這是 scope contract hard gate;PIE smoke PASS 後才宣告「v0.5.0 ready for student trial」。**若 PIE smoke FAIL:** 走 post-publish hotfix protocol（mark prerelease + ship v0.5.0.1 hotfix per release-hardening skill `Post-publish hotfix protocol`）。
3. 把 release URL 回報主對話以記錄

---

## Untracked / out-of-scope (transparency)

- `Plugins/ALS/Source/ALS/Private/AlsCharacter.cpp` 改動在 working tree;canonical artifact = `Tools/patches/als_l400_animinstance_guard.patch`(committed)
- `.git/hooks/pre-commit`(本機 IWYU validator hook)未 tracked;README 給 install 指引
- `Plugins/ALS/` / `Plugins/SPUD/` / `Plugins/SUQS/` / `Plugins/Prefabricator/` / `Plugins/FrameSolver/Grasshopper/v2/{Rhino/,}/bin/`:third-party plugin 目錄 + build artifact 從未 tracked
- `Content/` / `.claude/` / `Research/ue58_attempt/`:carry from prior sprints,untracked per convention

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
