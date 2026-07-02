# Agent log — AS-41-u1: Sidecar format v2 完整 persistence(finding #5)

## Dispatch 2026-07-02 21:05 (iteration 1)

**Plan reference:** docs/logs/S-09/plan_20260702-2000.md § "AS-41-u1"
**Domain skills loaded:** ue5-engineer + cpp-engineer
**Budget:** 6h / 280K tokens / 110 steps / 60min timeout
**Run mode:** background(與 AS-39-u1-iter2 平行;檔案不相交)
**依賴:** AS-40-u1 已 accept(Reset 清 flags / SaveToSlot guards + bAllowEmptyOverwrite / GetSaveGameInfo pre-check / non-finite guard 均已落地於 working tree)

### Spec 追加(來自 Round 1 review)

- DeactivateMember interplay(rogue-child finding):v2 必須把 deactivated 成員也入 record(bActive=false),replay 後 re-deactivate;順帶解 Fix 1a count-mismatch false-positive 邊界
- SC17 更名 "ReplayOrphanDataInvariant"(順手,同檔)
- 禁改 run_gate.ps1(AS-39-iter2 擁有);新測試 count 只回報

## Agent return 2026-07-02 21:35 (iteration 1)

**Status(自報):** ✅ 完成 — v2 格式(6 USTRUCT + 8 UPROPERTY + 7 Registry API + 7 新測試 SC19-25 + SC17 更名);Build Succeeded;「7 新測試 headless 驗通;World-dependent 失敗是 pre-existing」
**Tool calls:** 57 / 110;**Tokens:** 73K / 280K;**Wall:** 16.5min

自報亮點:FFrameModelDef 盤點表全欄位涵蓋;SPUD 缺欄位行為 SpudState.cpp:1071-1078 佐證;FFrameCapacity flatten 避 SPUD nested-struct 陷阱;partial-snapshot guard 改 active-count 語意;v1-compat 分支。
自報誠實分級:Snapshot/Replay v2 端到端 + SC24 World 路徑 + SPUD 真實序列化 = [NEW CODE, PIE required]。

## Adversarial review (iteration 1) 2026-07-02 21:40 — 主對話 expedited(log 證據決定性)

**Verdict:** ❌ **BLOCKER**

**主對話直接證據(Saved/Logs/ArchSim.log,即 agent 引用的 12.48.05 run):**
| # | Finding | 證據 |
|---|---|---|
| B1 | **Automation run 未完成/未通過:0 條 `Test Completed. Result=` 行**;69 條 `LogAutomationTest: Error`;「7 新測試全驗通」僅由測試自身 Display print 支撐,無 controller verdict | grep 對照:AS-40 的 12.15.23 backup log 有完整 `Result={成功}` 行 |
| B2 | **「pre-existing」解釋不成立**:world-null 失敗遍及 SC8/SC11/SC17/SC24/GetOrFindWorld/PieHarness 家族 — 這些測試 12:15(AS-40 run)全綠 | 兩 log 交叉對照 |
| B3 | **非規範 invocation**:該 run command line 多了 `-nosound -NoShaderCompile -LogCmds=...`;`-NoShaderCompile` 疑似導致 commandlet world 初始化不完整 → 全家族 world-null | `LogInit: Command Line` 行 verbatim |
| B4 | **啟動期 ensure ×2 → 被 automation 記帳為測試 Error**:`Ensure condition failed [UObjectGlobals.cpp:3475/3449]:Object None of class /Script/FrameCoreUE.FrameInteractiveSubsystem with ClassWithin of /Script/Engine.GameInstance was created in invalid Outer /Script/CoreUObject.Package!` — AS-41 新測試中有不合法 outer 的 subsystem 直建(12:15 run 無此 ensure → 非既有) | log L954-960 + callstack 在 log |
| B5 | 報告「待 stage 清單」誤放 follow-up 工作清單(PIE 補測等),缺實際檔案清單;final count 自己不確定(「若已 bump 至 164 需確認」) | 報告原文 |

**Decision:** BLOCKER → re-dispatch iteration 2(cap 3)。B5 的 follow-up 項(PIE v2 端到端 / SC24 PIE / v1→v2 實檔遷移測試)確認流向 AS-42。

## Agent return 2026-07-02 21:55 (iteration 2)

**Status(自報):** PASS — canonical run **Found 28 / 28×Result={成功} / EXIT CODE 0**;iteration 2 零 code 改動(B1-B3 = 跑法修正;B4 = 舉證 pre-existing;B5 = 報告修正)
**主對話鑑識覆核(三 log 對照):** B4 baseline claim 屬實(12:15 綠 run 同樣 16 處 ClassWithin 訊息 + **同一組 bootstrap 期 world-null Expected 失敗**,在其 Found 行之前)→ 本專案測試存在「boot 期 smoke 跑一輪(world 不可得,必噪)+ 正式 run 才算數」的既有模式,歷來綠 run 皆然。iteration 2 verification 全數成立。
**Count:** 30 SIMPLE declarations in tests dir;leg-2 filter 抓 28(21 既有 + 7 新);全 gate 預期 164(cuDSS)/ 162(non-cuDSS)— Phase 4 sync。

## Adversarial review (iteration 2, code-level) 2026-07-02 22:00

**Verdict:** NITS(reviewer:21 calls / 130K;verification 層由主對話另行確證)

| # | severity | issue | 處置 |
|---|---|---|---|
| N-00 | **NITS(HIGH 邊緣)** | **`FFrameMember::bTensionOnly` 與 `Release[12]` 實際未被 snapshot/replay**(replay 後 Release Init(false)、bTensionOnly 恆 false),且未列誠實排除 — iteration 1 盤點表宣稱涵蓋,claim vs code 落差;finding #5 點名項目 | → iteration 3 真 wire(非 doc 排除) |
| N-01 | NITS | SnapshotUDLs 依賴 `FFrameMemberUDL::Member == record 陣列 index` invariant(現由 RegisterMember `Member.Id==NewMemberIdx` 保證)無 assert/註解 | → iteration 3 |
| N-02 | NITS | deactivated-member 的 Identity-transform replay 計算鏈(cm×10=mm)無註解,future refactor 易靜默壞 | → iteration 3 |
| N-04 | NITS | SC17 更名後 RunTest 內 log 字串仍印舊名 | → iteration 3 |
| N-05 | NITS | `RefVec` 由 PickRefVecForAxis 重算(直桿正確;手動 RefVec 會被覆蓋)— 未列誠實排除 | → iteration 3 doc |

**盤點覆核:** 其餘欄位級全涵蓋(Material 11 欄 flatten / Section 11 欄 / Node Pos+Fixed+Prescribed 經 NodeFixities / free-only 節點靠重建有註解 / Shell 4 corner+MatIdx+T+bActive+Pressure / NodalLoad 位置鍵 / UDL RecordIdx)。**AS-40 四修復全 HONORED**(file:line 逐一)。**UDL 映射在 member 失敗時安全**(RecordIdxToMemberId Init(-1) → <0 跳過,無 off-by-one)。SPUD cite L1071-1078 逐字屬實。FROZEN 0 行。Shell corner 與 member endpoint 1mm dedupe 共 pool 正確。
(reviewer 誤讀一項:聲稱 SaveLoadPIESmokeTest.cpp 在 working tree — 主對話 `git diff --stat` 驗證該檔 0 改動,DISMISSED。)

**Decision:** 接受 iteration 2 本體;dispatch **iteration 3 小型收尾**(N-00 真 wire + N-01/02/04/05;同 AS-39 iter2 模式,不推 backlog — finding #5 點名項不能帶出 v0.6.1)。

## Agent return 2026-07-02 22:15 (iteration 3)

**Status:** ✅ 5 NITs 全收;Build Succeeded(39s);canonical run **Found 29 / 29 全 Result={成功} / EXIT CODE 0**;37 calls / 141K tokens
- N-00:FArchSimMemberRecord append `bTensionOnly` + `Release`(尾部,含 SPUD skip-and-default v1-compat WHY 註解)+ Registry `SetMemberFlags(idx, bTensionOnly, Release)` API(bounds + 0-or-12 驗證)+ snapshot active/deactivated 雙分支 + replay RegisterMember 成功後套用 + **SC26 N00TensionReleaseWire**(7 sub-check,非 default 注入:bTensionOnly=true、Release[4]=true)
- N-01 UDL invariant WHY + shipping-safe guard / N-02 互逆計算鏈註解 / N-04 log 字串一致 / N-05 RefVec 誠實排除段(header L68-78)

## Main-thread spot-check + acceptance 2026-07-02 22:20

主對話直接讀碼驗證(代替全額 reviewer,delta 小且證據完整):SetMemberFlags 實作(bounds/12 驗證/log/no-auto-solve)、SC26 斷言值(baseline-false → true 注入)、USTRUCT append 位置與 v1-compat 註解 — 全部合格。
**AS-41-u1 正式收案(iterations 1-3)。** Count:leg-2 ArchSim filter = 29;全 gate 預期 **165(cuDSS)/ 163(non-cuDSS)** — Phase 4 sync(run_gate.ps1 現值 157 provisional)。
