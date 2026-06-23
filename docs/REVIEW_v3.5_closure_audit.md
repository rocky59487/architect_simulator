# V3.5.0 深入審核 — 距離引擎封箱評估報告

> 審核日期:2026-06-23 · 審核對象:v3.5.0(commit `d9e9b02`)· 基準:v3.4.0(`f411c80`)
> 範圍:引擎封箱就緒度評估 + v3.5 視覺層程式碼獨立複查 + 閘門驗證狀態核實
> 性質:唯讀審核,未改動任何程式碼。

---

## 結論先行

**引擎本體(`FrameCore/`)實質上已經封箱**;距離正式宣告 v3.5「封箱」只剩**一件阻塞事項**:
Z-01 整合機台閘門實跑。沒有任何引擎演算法工作擋在封箱前面。

| 維度 | 狀態 | 距封箱 |
|---|---|---|
| 引擎數值核心(`FrameCore/`) | 自 v3.4 起 stable-mode,v3.4→v3.5 **0 行變更**(已驗證) | ✅ 已封 |
| 演算法完整度(S1–S11) | S1–S10 全實作;僅 S11 MITC9i 未做(**刻意殿後 / optional**) | ✅ 不阻塞 |
| v3.5 視覺層程式碼品質 | 12 項聲稱修復 11 PRESENT / 1 PARTIAL,新發現全 LOW/INFO | 🟡 1 個可選小修 |
| **閘門驗證(放行條件)** | **5 腿閘門從未端到端跑過,Leg 2 完全未驗** | 🔴 **唯一阻塞** |

> **一句話:** 距離引擎封箱 = **1 次整合機台閘門實跑(Z-01)**。封箱的最後一哩不是「再寫程式」而是「補驗證」。

---

## 一、引擎本體封箱狀態:已實質封箱

**證據鏈:**

1. **零引擎變更已核實。**
   `git diff v3.4.0 v3.5.0 -- Plugins/FrameSolver/Source/FrameCore/` 回傳**空集**。
   v3.5 全部 4,102 行新增都落在 `FrameCoreUE/`(消費端)、`Scripts/`、`docs/`。
   CLAUDE.md 鐵則 #1(引擎源碼 delta = 0)完全遵守。

2. **設計文件已宣告 stable-mode。**
   `HANDOFF_v3.4_v3.5_design.md` 明文:「Engine 從 v3.4 起進入 stable mode,理論上 v3.6+ 只接
   bug fix」。v3.4/v3.5 的契約是「**只暴露、不加演算法**」。

3. **對標 Karamba3D 11-tab 全覆蓋。**
   自評矩陣顯示 Setup / Material / Section / Element / Load / Analyze / Inspect / Display /
   Algorithms / Advanced / Interactive / Utility 全部 ✅。

4. **唯一未實作的引擎演算法 S11(MITC9i 高階殼)是刻意殿後的 optional 項。**
   `KARAMBA3D_ROADMAP.md §3` 標註「S1–S10 已全部實作完成;僅 S11 未實作」,且使用者點名「殿後」。
   **它不在 v3.5 的封箱範圍內。**

**判讀:** 從「引擎程式碼凍結」這個嚴格定義看,引擎在 v3.4 就已經封了,v3.5 連一行都沒碰它。
所以「距離引擎封箱」的真正問題不是**還要寫什麼**,而是**還要驗證什麼**。

---

## 二、V3.5 深入審核發現

### 2.1 🔴 唯一阻塞:閘門從未實跑(Z-01 未執行)

這是 v3.5 release notes **自己誠實揭露**的最大缺口,審核確認屬實且嚴重:

- **5 腿閘門沒有端到端跑過。**
  RELEASE / HANDOFF / VERIFICATION 三份文件一致標註 Legs 2–6 全為 **「PROJECTED」(預期)**,
  非 verified。release notes 原文:「the 5-leg gate was **NOT RUN end-to-end**」。

- **CI 綠燈是誤導性安心。**
  GitHub Actions 上 v3.5.0 commit `d9e9b02` 的 release-gate **conclusion = success**。
  但檢視 `.github/workflows/release-gate.yml` 後確認 —— CI **只跑 CPU 引擎腿**
  (standalone F1–F71、audit 104、CLI 13、v2_roundtrip)。這些腿測的是**引擎數值**,
  而引擎 delta = 0,所以它們只是把 v3.4 的綠燈原封不動帶過來。

- **Leg 2(UE 自動化,120 測試)= v3.5 的全部交付物 —— 從未跑過,也不在 CI 內。**
  v3.5 新增的 9 個 `.cpp` + 8 個 actor header + 7 個測試檔(22 個新測試),
  **沒有任何自動化系統證明它們能在 UE 5.7 下編譯,更別說通過**。
  GPU 機台 + UE 安裝是 CI 觸及不到的(workflow 註解自承)。

> **白話:** 目前綠的是「沒動過的引擎」;v3.5 真正新寫的東西,一行都還沒在編譯器 / 測試框架前面
> 證明過自己。

### 2.2 🟡 程式碼品質審核(獨立驗證 7-agent 審計聲稱)

對 9 個實作檔逐一獨立複查,12 項聲稱修復:**11 PRESENT、1 PARTIAL**,證據齊全。
生命週期路徑**無 memory-safety / OOB / dangling 缺陷**;`ReSolveSession` 將 model 複製入內,
subsystem `Cached` 生命週期排序正確。

**已驗證 PRESENT 的聲稱修復:**

| 編號 | 內容 | 證據 |
|---|---|---|
| A-02 | delegate 結構 by-value | `FrameDynCollapseReplayActor.h:23-24` |
| A-05 / D-02 / D-10 | loop 事件雙窗派發、無丟事件、移除死 `PrevTime` | `FrameDynCollapseReplayActor.cpp:61-78,88-91` |
| C-03 | `ensureMsgf` on `UFlat.Num() % 6 == 0` | `FrameDynCollapseReplayActor.cpp:117-119` |
| C-05 | influence ribbon normal 隨 `sign(Height)` 翻轉 | `FrameInfluenceLineActor.cpp:70` |
| C-09 | response spectrum 單極性 `0.5*(1+cos)` | `FrameResponseSpectrumActor.cpp:65` |
| D-04 | Modal `TimeScale` `ClampMin=0.0` | `FrameModalShapeActor.h:43-45` |
| D-06 | DynCollapse `PlaybackSpeed` 執行期 clamp | `FrameDynCollapseReplayActor.cpp:59` |
| D-12 | subsystem 測試 null path `AddError` | `FrameCoreUEInteractiveSubsystemTest.cpp:113,153` |
| F-04 / F-05 / F-12 | heatmap O(M·N) → 預算 `TMap` risk + `SatGuard` 外提 | `FrameUtilizationHeatmapActor.cpp:68-93` |
| F-09 | DeformedShape `BaseColor` 外提出 ring loop | `FrameDeformedShapeActor.cpp:109` |
| A-04 | `FFrameShellGeometry.CornerNodeIndices` 標 v3.6-reserved | `FrameCoreUEVisualTypes.h:46-49` |

**新發現(皆 LOW/INFO 級,不阻塞封箱,但建議收尾時處理):**

| 編號 | 級別 | 內容 |
|---|---|---|
| **D-01 PARTIAL** | LOW | release notes 聲稱 copy/**move** ctor 都 `= delete`,實際只刪了 copy ctor + copy assignment(`FrameInteractiveSubsystem.h:36-37`)。實務 benign(宣告 user copy ctor 已抑制 implicit move 生成),但與文件意圖不符。 |
| **NEW-1** | LOW–MED | `AFrameRealTimeDynamicActor::Tick`(`.cpp:51`)與 `AFrameModalShapeActor::Tick`(`.cpp:42`)缺 D-06 那種 **use-site clamp**,只有 UI `ClampMin`。BP 旁路寫負 `PlaybackSpeed` / `TimeScale` → 時間倒退(不崩潰,退化成 frame-0 凍結 / 反向動畫)。**這正是 D-06 要關的同類旁路,卻只修了 DynCollapse 一個 sibling。最值得收。** |
| **NEW-2** | LOW | A-05 loop 事件派發修復**零測試覆蓋** —— EventDelegate 測試(`FrameCoreUEDynCollapseReplayTest.cpp:168-175`)沒綁 delegate、只斷言 `CurrentTime` 前進,沒驗證事件實際觸發。 |
| NEW-3 | LOW | heatmap risk 若引擎吐 NaN 會污染頂點色(`FrameUtilizationHeatmapActor.cpp:73,79-80,106`);防禦性,引擎端應恆為有限值 ≥ 0。 |
| NEW-4 | LOW | `SpawnedDebris` 無上限成長(`FrameFragmentClusterActor.cpp:13-28`)—— 已知為 deferred **U-14**,程式碼與延期一致(**非漏網**)。 |
| NEW-5 | INFO | `FFrameFragmentCluster.Inertia`(length-6 tensor)有 marshal 但 UE 端未消費(`FrameFragmentClusterActor.cpp:61-67`)—— thin-slice Phase 5 一致。 |

### 2.3 誠實邊界與延期項(已妥善標註,不阻塞封箱)

v3.5 把所有限制都顯式寫進 release notes(D/C 彈性快篩非規範檢核、LSP ±30%、塑鉸 event-to-event、
模態 / 屈曲 / 反應譜全 linear、MITC4 flat-facet、Chaos 為視覺非物理)。延期項
(U-08 showcase map → v3.5.1 需人工 Editor、U-09 Chaos POD → v3.6、U-10..U-15、
PMC-DUP-01 / TEST-DUP-01 重構)**全部誠實 cross-confirm 於 HANDOFF**,範圍紀律良好。

---

## 三、距離封箱的具體清單(依阻塞性排序)

### P0 — 放行硬條件(唯一真正阻塞)

1. **執行 Z-01**:在整合機台(Windows 11 + UE 5.7 + cuDSS)跑

   ```powershell
   $Root   = "<path-to-ArchSim-clone>"
   $Engine = $env:UE_ENGINE_ROOT
   Set-Location $Root
   & "$Engine\Engine\Build\BatchFiles\Build.bat" ArchSimEditor Win64 Development `
     -project="$Root\ArchSim.uproject" -waitmutex
   powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
   ```

   把 Legs 2–6 從 **PROJECTED 升級為 VERIFIED**。

   - 預期:Leg 2 = **120/120**(有 cuDSS)或 **118/118**(`-ExpectedUeTests 118`)。
     首要風險是 7 個新測試檔是否全編譯(HANDOFF 已列檢查清單)。
   - 成本:約 30–90 分鐘(UE 冷快取建置 + 閘門)。
   - **這是現在與封箱之間唯一站著的東西。**

### P1 — 收尾建議(非阻塞,可併入 v3.5.1)

2. 修 **NEW-1**:給 `RealTimeDynamic` / `ModalShape` 的 `Tick` 補 use-site speed clamp(各 1 行,與 D-06 同 pattern)。
3. 修 **D-01 PARTIAL**:補 `= delete` move ctor / assignment,讓程式碼符合文件意圖(1–2 行)。
4. 補 **NEW-2** 的 loop 事件觸發測試(用 `UFUNCTION` 綁定計數)。

### 不阻塞封箱(明確排除)

- S11 MITC9i、U-08~U-15、Chaos POD 全套 —— 這些是 v3.5.1 / v3.6 的**另一條 roadmap**,
  使用者已刻意排除或延期。

---

## 四、總結判讀

> **距離引擎封箱 = 1 次整合機台閘門實跑(Z-01)。**

引擎本體早已凍結,演算法已完整(S1–S10),程式碼品質審核乾淨(無 memory/OOB 缺陷,只有可選小修)。
**封箱的最後一哩不是「再寫程式」而是「補驗證」** —— 把那份從未跑過、目前 100% 靠紙上推導的
120-test UE 閘門,在真機上跑出綠燈。在 Z-01 通過前,v3.5 嚴格來說是
「**程式碼完成、驗證未完成**」狀態,不能算封箱。

| 封箱前置 | 性質 | 誰能做 |
|---|---|---|
| Z-01 閘門實跑 | 驗證(P0,阻塞) | 整合機台(UE 5.7 + cuDSS) |
| NEW-1 / D-01 / NEW-2 | 程式碼收尾(P1,可選) | 任何 fresh session |
| S11 / U-08~U-15 | 另一條 roadmap | v3.5.1 / v3.6 |

---

*本報告為唯讀審核產出,未改動任何原始碼或閘門腳本。*
