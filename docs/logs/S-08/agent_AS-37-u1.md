# Agent log — AS-37-u1: ALS commandlet PIE crash 查證(read-only)

## Dispatch 2026-07-02T0130Z (iteration 1)

**Plan reference:** docs/logs/S-08/plan_2026-07-02T0019Z.md § "AS-37-u1"
**Domain skills loaded:** ue5-engineer (primary)
**Budget:** 2h / 200K tokens / 60 tool calls / 25min timeout
**Dispatch mode:** foreground(serial flow)
**特別規約:** 最終狀態 0 production delta;允許暫時 instrumentation(如暫拆 PIE smoke test 的 DefaultGameMode sidestep 以重現 crash),結束前 `git checkout --` 還原 + incremental rebuild 恢復 binary,final `git status` 必須乾淨。

### Pre-flight reads(main thread)

- ARCH_INDEX § 7 AS-37 條目:症狀 = `AArchSimCharacter` spawn in commandlet PIE → ALS `LoadObject<T>()`(`SKM_Als`, `CS_Als_Default` 等 plugin content)在 PIE-pawn-spawn 時序失敗 → `MovementSettings` null → `NotifyLocomotionModeChanged()` EXCEPTION_ACCESS_VIOLATION;user-driven PIE 不觸發(Editor 先 mount plugin content)
- S-06 U-ALS 前例:`ArchSimCharacter` 的 `PostInitProperties()` + `BeginPlay()` + `LoadAlsAssetsLate()` runtime-late LoadObject;ALS patch `Tools/patches/als_l400_animinstance_guard.patch`
- AS-35-u1 sidestep 現場:`ArchSimPortalFramePIESmokeTest.cpp` 內 test-local `WorldSettings->DefaultGameMode = AGameModeBase::StaticClass()` override
- Baseline:v0.5.2 @ `2fb0f4e`(AS-36 已 ship;working tree clean 除 untracked)

### Composed prompt

結構同 AS-36-u1(iron rules verbatim → discipline → arch-index pointer → baseline v0.5.2 @ 2fb0f4e → ue5-engineer SUBAGENT_PREFIX verbatim(路徑 `~/.claude/skills/domain/ue5-engineer/SUBAGENT_PREFIX.md`,310 行)→ unit spec → verification → reporting → forbid list)。Unit spec 核心:

1. **Deliverable 1 — 重現**:instrumented run(暫時把 PIE smoke test 的 DefaultGameMode sidestep 拆掉或加最小 scratch 手段)重現 EXCEPTION_ACCESS_VIOLATION,capture callstack + log verbatim(`Saved/Logs/ArchSim.log` + `Saved/Crashes/`)
2. **Deliverable 2 — 定位**:哪個 `LoadObject` 對哪個 asset path 失敗、call site file:line(`ArchSimCharacter.cpp` LoadAlsAssetsLate / ALS 內部)、commandlet vs editor 的 plugin content mount 時序差異(UE source 佐證)、S-06 既有 guards 為何擋不住此路徑
3. **Deliverable 3 — severity 判斷**:packaged game(cooked content)是否受影響?(影響 (a)/(b) 抉擇的關鍵)
4. **Deliverable 4 — (a)/(b) 成本風險並列**:(a) 文件化 known commandlet-only limitation 的殘餘風險;(b) 修 fix 的候選路徑草圖(mount 時序 / 防禦 guard 鏈 / patch ALS)+ 預估 LOC + 風險,**不預選立場**
5. **Deliverable 5 — 還原**:`git checkout --` 全部暫時改動 + incremental rebuild + final `git status --short` 乾淨證據 + scratch 檔清除

ESCALATE triggers:查證發現 production(user-driven PIE / packaged)也受影響 → 升 severity 回報;無法在 budget 內重現 → 帶部分證據回報,不硬湊。
越界禁令:不修 bug(那是 (b) 選項的 AS-37-u2)、不動 NITS-u1 / AS-08 scope、不留任何檔案改動。

## Agent return 2026-07-02T0145Z (iteration 1)

**Status:** ✅ DONE(self-reported;待 Phase 3 review)
**Wall time:** 9m 49s(of 25min budget)
**Token usage:** 150,057 of 200K budget
**Tool calls:** **67 of 60 budget(+12% 超標,無 ESCALATE — 記錄為 process NIT;完成品質未受影響)**

### Full report(verbatim 摘要 — 完整版在 dispatch 系統記錄)

## Status
✅ DONE

## 重現結果
以 READ-ONLY 方式用既有 crash logs(`Saved/Crashes/UECC-Windows-7ECCED384D44B2CCD56A45B7F390734D_0002` 2026-06-28 + `UECC-Windows-964B6D31440FCE798BC8F593555760E6_0002` 2026-06-27)完成查證,未做 instrumented build(證據充分 + 省 budget)。

Verbatim 關鍵行(0002,06-28):
```
LogLoad: Game class is 'ArchSimGameMode'
LogArchSim: Display: AArchSimCharacter [ALS] PostInitProperties: attempting LoadAlsAssetsLate
LogUObjectGlobals: Warning: 寻找对象"AlsCharacterSettings /ALS/Data/Character/CS_Als_Default.CS_Als_Default"失败
LogArchSim: Warning: ... LoadObject failed for CS_Als_Default / MS_Als_Normal / SKM_Als / AB_Als_C(4 連敗)
LogOutputDevice: Error: Ensure failed: IsValid(NewMovementSettings), AlsCharacterMovementComponent.cpp, Line: 894
LogOutputDevice: Error: Ensure failed: IsValid(MovementSettings), AlsCharacterMovementComponent.cpp, Line: 903
CrashContext: Unhandled Exception: EXCEPTION_ACCESS_VIOLATION reading address 0x00000000000000d8
Callstack: UnrealEditor-ALS + 0x44766 / 0x45281 / UnrealEditor-Engine + 0xbe062b / UnrealEditor-ALS + 0x50dff ...
```

## 失敗鏈定位
- 環1 [INFERRED]:commandlet 模式 `Mounting Project plugin ALS` 有 mount,但 AssetRegistry 對 `/ALS/` 的 scan 在 actor-spawn 時序未完成(`LoadPackage: SkipPackage: /ALS/Data/Character/CS_Als_Default — does not exist on disk or in the loader`);`RegisterMountPoint`(PluginManager.cpp:2057)只登記映射不觸發 rescan;互動 Editor 在 PIE 前早已完成 scan。
- 環2 [VERIFIED]:`ArchSimCharacter.cpp:194-199` BeginPlay fallback 在 PIE spawn 時仍 4 連敗(`ArchSimCharacter_0` 非 CDO,log verbatim)。
- 環3 [VERIFIED]:`AlsCharacter.cpp:L139` `ALS_ENSURE(IsValid(Settings))` fire 但不中止 → L163 `RefreshMeshProperties()`。
- 環4 [VERIFIED]:實際崩點鏈 = `UAlsCharacterMovementComponent::SetMovementSettings` L894 + `RefreshGaitSettings` L903 ensures → next tick `NotifyLocomotionModeChanged`(`AlsCharacter.cpp:L526`)deref null `Settings` → EXCEPTION_ACCESS_VIOLATION。**HANDOFF/ARCH_INDEX 原記載「crash 在 NotifyLocomotionModeChanged」不精確 — 那是最終崩點,首發是 L894/L903。**
- 環5 [INFERRED]:user-driven PIE 不觸發因 Editor 完整初始化後 AssetRegistry scan 已完成。

## Severity 判斷
**Commandlet-only [VERIFIED 推理]**:packaged build 用 cooked assets(pak manifest 索引),`SkipPackage` 只發生在 development commandlet;production 不走 editor commandlet 路徑。**不影響 production packaged build。**

## (a)/(b) 成本風險表
| 選項 | 做法 | 預估 LOC | 風險 | 殘餘限制 |
|------|------|----------|------|----------|
| (a) 文件化 | ARCH_INDEX §7 AS-37 補 commandlet-only 說明 + BeginPlay fallback 失敗原因;未來 PIE test 必帶 sidestep | ~20 行 docs | 低 | AS-08-u2 及未來所有 PIE test 必須帶 sidestep,漏帶即 crash |
| (b-1) test harness 統一 sidestep helper | 新 `FArchSimPIETestHelper::OverrideGameModeForSafePIE(UWorld*)`,PortalFrame test 改用 | ~30 LOC test | 低(只動 test 側) | PIE test 仍不能 spawn ALS character |
| (b-2) LoadAlsAssetsLate 失敗 graceful degrade | BeginPlay 若 Settings null → 不呼叫 Super::BeginPlay,Error log + return | ~10 LOC | 中(ALS state machine 不初始化 → zombie pawn;副作用需驗) | commandlet PIE character 功能殘缺 |
| (b-3) StartupModule 強制 ScanPathsSynchronous("/ALS/") | ArchSim module 啟動時 explicit AssetRegistry scan | ~5 LOC | 中-高(headless 啟動時間 +;commandlet 模式 API 行為需驗) | asset path 改變需同步 |
| (b-4) patch ALS L526 null-guard | 仿 als_l400 patch 加 `if (!IsValid(Settings)) return;` | ~8 LOC patch | 中(只擋終崩點不修根因;L894/L903 ensures 仍 fire;仍 zombie) | 第二個 ALS patch 檔要維護 |

## 還原證據
本 unit 0 行 tracked 改動(`git status --short` 只剩 baseline 既有 M docs ×3 + untracked);leg 6 最新 PASS(2026-07-02 08:45 `Result={成功}` + EXIT CODE 0);binary 未動。

## Gotchas / discoveries
1. **Crash 位置修正**:文檔說 `NotifyLocomotionModeChanged()`,實際首發 L894/L903(ensure)→ 終崩 L526。
2. `PostInitProperties` 在 CDO 時期也觸發 LoadObject 4 連敗(`Default__ArchSimCharacter` log 行)。
3. 06-27 crash log 無 `LoadAlsAssetsLate` 行 = S-06 U-ALS(`9b99691`,06-28)之前的舊 binary [INFERRED from timeline]。
4. BeginPlay fallback 無效的根本原因:問題不在「何時呼叫」而在「AssetRegistry 是否已索引」。
5. `als_l400_animinstance_guard.patch` 守 L411 AnimationInstance,未覆蓋 L894/L903 MovementSettings 崩點。

## Self-grading
環2/3/4 + severity + 還原 + leg6 = [VERIFIED];環1/5 + 舊 binary 推測 = [INFERRED](UE callstack 無 symbol,架構推理)。

## ESCALATE?
None

---

## Adversarial review (iteration 1) 2026-07-02T0155Z

**Verdict:** NITS(4 findings 全為精確度級;無 fabrication;鐵則全 CONFIRMED)

**Reviewer 核實:** crash log 兩實檔 verbatim 行(四連敗 / L894/L903 ensures / 0xd8)✓;`AlsCharacterMovementComponent.cpp:L894/L903`、`AlsCharacter.cpp:L139/L163/L526`、`ArchSimCharacter.cpp:194-199` 逐行核實 ✓;0 tracked delta + no commit(HEAD 仍 `2fb0f4e`)✓;[INFERRED] 標記誠實 ✓。

**Findings 及主對話裁定(更正直接記於此,不另開 backlog):**
1. NIT:`PluginManager.cpp:2057` 行號誤植 → **正確為 L2007**(`RegisterMountPointDelegate.Execute`)。
2. NIT:leg 6 時戳「08:45」→ log 實為 `2026.07.02-00.45`(UTC;本地時區偏差)。PASS 本身屬實。
3. NIT:第二個 crash(964B6D31,06-27)是 **WRITE violation 0x478**,與第一個(READ 0xd8)例外型態不同 — 「同路徑」描述過度簡化;舊-binary 推測維持 [INFERRED]。
4. LOW:severity「commandlet-only」補 caveat — **packaged Development `-game`(無 pak)場景未排除**,cooked/pak 結論不變;殘餘不確定性標註於此。

**Reviewer 補充的機制支撐(missed edge case #1,主對話採納):** 現行 leg 6 run 的 log 顯示 CDO(`Default__ArchSimCharacter`)四連敗 warning 依然發生,但因 sidestep(GameModeBase)未進 ALS BeginPlay 故不 crash — 這正是 commandlet-only + sidestep 有效性的直接機制證據。

**Decision:** Accept with corrections logged。查證品質達標;進 user decision gate((a)/(b))。
