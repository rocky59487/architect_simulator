# 建築師模擬器 — 實作計畫 (Implementation Plan)

> **版本**:v1.0(subagent 擴充 + 人工審核序章版)
> **日期**:2026-06-25
> **與主計畫書關係**:本檔是 [ARCHITECT_SIM_MASTER_PLAN.md](ARCHITECT_SIM_MASTER_PLAN.md) 的**可執行展開版**,把 12 個 Part 拆成具體 sub-task(每個 ≤4 小時)+ 工時 + UE5 class + 完成標準 + 踩雷 + Sprint 計畫。
> **規模**:~250KB,12 個 Part × 平均 20KB,共 1899 工時(對標主檔 L1 估算 ~1900h)。
> **撰寫流程**:由 12 個 subagent 平行擴充各 Part,1 個 cross-review agent 做跨 Part 一致性審核,最後人工(我)寫序章審核摘要。

---

# 序章 — 人工審核摘要(我的審閱結論)

## 0.1 整體評價

subagent 擴充品質**高於預期**,基本可信為實作基礎。整體 1899h 工時對標主檔 L1 預估 1900h 僅超 2.7%,合理。Sprint 計畫(S-00 ~ S-15,32 週 = 8 個月)對 1.5 人團隊樂觀但可達成,**建議保留主檔的 12 個月作為交付目標**,額外 4 個月做 Phase 3 內容擴充與品質提升,而非壓縮 buffer。

## 0.2 必須在 S-00 就驗證的三個技術假設(High Risk #5)

cross-review agent 抓到三個低成本但若到晚期才發現代價極高的技術 spike,**S-00 Sprint 必須優先處理**:

1. **ALS v4.17 + UE5.7 乾淨 build 驗證**(0.5 天)— 若失敗,所有角色層方案需重新考慮
2. **FDemandSummary USTRUCT 欄位清單查閱**(0.5 天)— D4 安全係數依賴此欄位是否真的存在;若無需在 UE consumer 層自算 SF = 1.0/MaxDC
3. **PCG_SurveyTerrain 節點名稱在 UE5.7 確認**(0.5 天)— PCG API 在 UE5.5-5.7 有多次 breaking change,G1 測量地形依賴此

## 0.3 接受 cross-review 的關鍵衝突修正

| # | 衝突 | 我的裁定 |
|---|------|---------|
| 1 | Part B 192h 嚴重超標 | **接受**:B MVP 僅 B1(48h),B2/B3/B4 全歸 Phase 2 |
| 2 | Part J 312h 全程依賴但 MVP 啟動點不明 | **接受**:J 拆為 MVP=47h(S-06 後啟動) / Phase 2=120h / Phase 3=145h |
| 3 | C4 連通性與 B1 放置依賴順序矛盾 | **接受**:C4 排在 B1 之後並行 C1 之前(S-04) |
| 4 | E 施工工序在 F3 閉環中暗中依賴 | **接受**:F3 整合段在 S-13 才做,E1 在 S-09/S-11 完成 |
| 5 | D4 SafetyFactor 欄位存在性未確認 | **接受**:S-00 必驗(見 0.2) |
| 6 | Phase 2 工時爆炸 700h+ | **接受**:Phase 2 內部排序見 Sprint S-09 ~ S-13 |
| 7 | K1-T2 構件材質必須在 B1 前 | **接受**:K1-T2 提前到 S-01 與 K4 字體並行 |
| 8 | L 工時 312h 容易重複計算 | **接受**:L 是 meta 整合層,工時應扣除其他 Part 已包含部分(實際淨 ~80h) |

## 0.4 接受 cross-review 的 Sprint 計畫

16 個 Sprint(S-00 ~ S-15,每 Sprint 2 週,120 工時 / 1.5 人團隊)完整列在第 13 章。**S-08 = MVP Release 教師評估版可玩**,**S-15 = Phase 3 完成正式發布**。

我**新增一個建議**:在 S-06 結束後就先做「最小可教學版本」教師試用 — 單人沙盒 + D1 掃描儀 + D/C 熱圖 + 簡單失敗提示。這比等到 S-08 完整 MVP 早 4 週,可在 S-07/S-08 根據教師回饋調整方向,避免後期才發現教育假設有誤。

## 0.5 5 個最高風險項(必須持續監控)

1. **B 工時 192h + B1 阻擋下游**(B1 是 C/D/E/F/K 共同前置)
2. **J 教育設計 312h 橫跨全程**(MVP 啟動點需與 D1 完成同步)
3. **C3 SolveDynCollapse AsyncTask PIE crash 風險**(S-05/S-06 提前做 POC 驗證)
4. **H Listen Server 在低配教學電腦效能瓶頸**(S-09 前在實際教學電腦壓測)
5. **D4 SafetyFactor / G1 PCG API 技術假設未驗證**(S-00 必驗)

## 0.6 文件閱讀導引

- 想直接動手做:**從序章 → 第 13 章 Sprint 計畫看當前 Sprint → 跳到對應 Part 章節看 sub-task 詳情**
- 想看整體架構:回 [ARCHITECT_SIM_MASTER_PLAN.md](ARCHITECT_SIM_MASTER_PLAN.md)
- 想看跨 Part 依賴關係:**第 14 章 Cross-Review 完整報告**
- 工時 / 風險 / 建議:第 14 章末段

## 0.7 我漏看的 LevelSim(2026-06-25 訂正)

**錯誤紀錄**:我撰寫主檔與 12 個 Part subagent 提示時,把 `Plugins/LevelSim/` 當成「另一個獨立子專案」忽略,實際上 LevelSim 已在 ArchSim repo 內(2026-06-18 PR#8 merged)並通過 Standalone 115/115 測試。這導致 Part G 工時被高估 53 小時,Sprint S-12 對水準儀的安排錯排到 Phase 2(其實 MVP 即可)。

**訂正內容**:
- **Part G 總工時**:178h → **125h**(節省 53h);整體 1952h → **1899h**(更貼近主檔 L1 估算)
- **G3 水準儀工時**:30h → **5h**(僅 polish + xAPI 整合 + 關卡選單呼叫)
- **G5 中 S-01 / S-02 / S-03 / S-04 / S-08** 五個關卡可直接用 LevelSim,僅做 SUQS DataTable + 老師預設 RoutePoints
- **水準儀關卡可納入 MVP**(原排 Phase 2);Sprint S-04 即可加入
- **可行性審查表**:第 12 項拆成 12a(水準儀,MVP)/ 12b(隨機地形,Phase 2)/ 12c(經緯儀,Phase 2)
- **附錄 B 開源依賴清單** 加 LevelSim 為內部 plugin(鐵則 #5 保護)
- **仍需從零做的測量**:經緯儀(G2 + S-05/S-06/S-07)約 40h,Phase 2 完成

**啟示**:寫計畫書前 verify 既有資產的存在,不要僅靠 memory 描述判斷。subagent 擴充 Part G 時也應該被指示 `Glob **/Plugins/**` 確認既有 plugin,而非預設「需從零做」。下次類似 workflow 應在 prompt 中強制 sub-agent 先掃 `Plugins/` 目錄。

---


# Part A 實作擴充(82h)

**標題**:Part A — 引擎與外殼 (FrameCore 接合層 / ALS 角色與相機 / 世界系統)

**子節數**:3

### MVP 必須(Phase 1)
- A1-01 UArchSimMemberData ActorComponent 骨架
- A1-02 UArchSimModelRegistry GameInstanceSubsystem 骨架
- A1-03 RegisterMember + FFrameModelDef 組裝
- A1-04 ApplyPatchAndResolve debounce 包裝
- A1-05 DistributeSolveResult + CachedUtilization 回寫
- A1-06 DeactivateMember 軟刪除 + stale 偵測
- A1-07 SaveGame round-trip 整合測試
- A2-01 AArchSimCharacter + ALS 子類骨架
- A2-02 Enhanced Input MappingContext 雙層設定
- A2-03 Server RPC 橋接到 UFrameInteractiveSubsystem
- A3-01 主場景 L_ArchSim_Sandbox 骨架 + World Partition 開啟
- A3-02 場景邊界牆 + 放置範圍鎖定 helper
- A3-03 地形 snap helper + 支承節點自動固接邏輯

### Phase 2
- A1-08 Rebaseline 觸發條件完整實作 + MaxRank=96 自動 Rebaseline
- A2-04 第一人稱 / 第三人稱切換
- A2-05 Splitscreen 第 2 玩家 EnhancedInput 初始化 workaround
- A3-04 PCG BaseTerrain 程序化地形生成 (Perlin + Seed 確定性)
- A3-05 5 種沙盒基地模板 Data Asset
- A3-06 LevelStreaming Volume 切塊無感載入

### Phase 3
- A2-06 ALS 自訂 AnimBP curves bake (UE5.7 ALS v4.17 相容)
- A3-07 地形高度差 ±10mm 精度測量接合點 (測量關卡銜接)

### 實作順序
- A1-01
- A1-02
- A1-03
- A1-04
- A1-05
- A1-06
- A1-07
- A1-08
- A2-01
- A2-02
- A2-03
- A3-01
- A3-02
- A3-03
- A3-04
- A3-05
- A3-06

### 跨 Part 依賴
- Part A 必須先於 Part B (構件放置) — UArchSimModelRegistry 的 RegisterMember 是 B1 Prefab 放置後 FrameCore 接合的必要前置
- Part A 必須先於 Part C (工程模擬層) — ApplyPatchAndResolve + DistributeSolveResult 是 C1/C2 載重施加與熱圖的基礎管道
- Part A 必須先於 Part E (多人協作) — AArchSimCharacter 是所有玩家 Pawn 基類; Listen Server 角色複製依賴此類
- A3 世界系統的支承固接邏輯 需在 A1 Registry 完成後才能整合 — 地基節點 FFrameNode.IsFixed 透過 Registry 寫入

### 風險清單
- FrameCore FROZEN (鐵則 #1): 所有改動只在 FrameCoreUE consumer 側; 任何需要動 Plugins/FrameSolver/Source/FrameCore/ 的需求必須走 UE consumer 層 helper 解決
- UFrameInteractiveSubsystem 在 -nullrhi -unattended headless 測試中 GetSubsystem() 回 null (v3.5.1 已知踩雷); 測試需用 NewObject<UFrameInteractiveSubsystem>() fallback
- ALS v4.17 + UE5.7 相容性: Animation Compression Library 常數插值行為改變; 自訂 AnimBP 需 bake curves; 需在整合初期先驗證 ALS plugin 能乾淨 build
- SPUD + World Partition issue #117 (open): 每次 UE minor 升級必驗 save/load 完整性; MVP 先用 UE SaveGame 備份方案
- UE5 Enhanced Input splitscreen 第 2 玩家初始化 bug (UE5.6 確認, 5.7 需實測): 對策是 OnPossess 後延 1 frame AddMappingContext
- FFrameSolveResult bSingular=true 時 MemberUtilization 含 NaN; DistributeSolveResult 前必須先 guard bSingular
- ApplyPatchAndResolve 高頻呼叫累積 Woodbury rank; 超過 MaxRank=96 必須自動 Rebaseline; debounce 150ms 防止每幀觸發
- Iris Replication 模式下 AArchSimCharacter 新增 Replicated property 需 MARK_PROPERTY_DIRTY_FROM_NAME; 否則 Client 永不收到更新 (靜默失敗)
- UE5 PCG 在 5.4-5.6 版本效能不穩; 鎖定 UE5.7 且確認 PCG plugin 版本為 Production-Ready 標記

## 詳細擴充內容


# Part A 詳細實作計畫 — 引擎與外殼

> 本計畫書適用對象: 後續 AI agent 照本實作。所有修改嚴格限於 `Plugins/FrameSolver/Source/FrameCoreUE/` 與新建的 `Source/ArchSim/` 遊戲模組,絕不觸碰 `Plugins/FrameSolver/Source/FrameCore/`(FROZEN, 鐵則 #1)。

---

## A1 FrameCore 接合層 — 詳細實作計畫

### 子節定位

本節是整個遊戲的**結構神經系統橋接層**。所有玩家放置的構件 Actor 必須透過此層正確映射到 `FFrameModelDef` 的索引體系,Solve 結果才能正確回寫到視覺層。若此層做錯,玩家的一切操作都不會在結構上生效,是最高優先的 MVP 基礎設施。

### A1-01: UArchSimMemberData ActorComponent 骨架

**工時**: 2 小時

**涉及 UE5 class / API**:
- `UActorComponent` 子類,掛載在所有結構構件 Actor 上
- `UPROPERTY(SaveGame)` 標記三個核心欄位
- `UPROPERTY(BlueprintReadOnly, Category="ArchSim")` 讓 BP 可讀

**具體工作**:
1. 在 `Source/ArchSim/Public/Components/ArchSimMemberData.h` 建立 `UArchSimMemberData : public UActorComponent`
2. 加入三個 SaveGame 欄位: `int32 MemberIdx = -1;`、`int32 StructureGroupId = -1;`、`float CachedUtilization = 0.f;`
3. 加入三個輔助欄位: `int32 MaterialId = 0;`(FrameCore 材料索引)、`int32 SectionId = 0;`(FrameCore 截面索引)、`bool bRegistered = false;`
4. 在 `BeginPlay()` 中呼叫 `UArchSimModelRegistry::Get(GetWorld())->RegisterMember(this)` (GetWorld() 取 GameInstance 再取 Registry)
5. 在 `EndPlay()` 中呼叫 `Registry->DeactivateMember(MemberIdx)`
6. 加入 `UFUNCTION(BlueprintCallable) float GetCachedUtilization() const` 供 BP 讀取

**完成標準**:
- Component 可在 Actor 上掛載,PIE 啟動不崩
- `MemberIdx` 預設 -1(未註冊狀態),SaveGame round-trip 前後值一致
- `GetCachedUtilization()` 在 BP 中可見並可呼叫

**預期踩雷**:
- `BeginPlay()` 呼叫 Registry 時,若 GameInstance 尚未完成 Initialize 會得到 null;需在 `BeginPlay()` 加 guard `if (!GetWorld() || !GetWorld()->GetGameInstance()) return;`
- 依賴 **A1-02** (Registry 骨架)先完成

---

### A1-02: UArchSimModelRegistry GameInstanceSubsystem 骨架

**工時**: 3 小時

**涉及 UE5 class / API**:
- `UGameInstanceSubsystem` 子類 (UE5 內建, `#include "Subsystems/GameInstanceSubsystem.h"`)
- `TMap<int32, TWeakObjectPtr<UArchSimMemberData>> IndexToComponent` — 正向映射
- `TMap<TWeakObjectPtr<UArchSimMemberData>, int32> ComponentToIndex` — 需驗證: TMap key 不可直接是 TWeakObjectPtr,需用 `FObjectKey` 或改用 `TMap<UArchSimMemberData*, int32>`(原始指標 key,但需注意 GC);**建議: 用 UArchSimMemberData 的 `FGuid UniqueId` 作為雙向映射 key**
- `FFrameModelDef CurrentModel` — 在 Registry 中持有並管理
- `UFrameInteractiveSubsystem* GetFrameSubsystem()` — 取 GameInstance 下的 FrameCore subsystem

**具體工作**:
1. 建立 `Source/ArchSim/Public/Subsystems/ArchSimModelRegistry.h`
2. 實作 `Initialize(FSubsystemCollectionBase&)` / `Deinitialize()` 生命週期
3. 建立 `FFrameModelDef CurrentModel` 並在 Initialize 時清空
4. 建立 `TMap<int32, TWeakObjectPtr<UArchSimMemberData>> IndexToComponent` 雙向查詢表
5. 宣告靜態 helper `static UArchSimModelRegistry* Get(UWorld* World)` 方便全域取用
6. 在 .cpp 實作 `Get()`: `return World->GetGameInstance()->GetSubsystem<UArchSimModelRegistry>()`
7. 內建 `int32 NextMemberIdx = 0;` 分配器 (只增不減;刪除走 bActive 軟刪)

**完成標準**:
- PIE 啟動 / 結束不崩
- `UArchSimModelRegistry::Get(GetWorld())` 在任何 Actor 中可正確取到 subsystem 指標
- `CurrentModel` 初始狀態為空的合法 `FFrameModelDef`

**預期踩雷**:
- `UGameInstanceSubsystem` 的 `Initialize()` 在 GameInstance 初始化前被 UE 框架呼叫, 此時 World 可能尚未完全 ready; 延後 FrameCore session 啟動到 `BeginPlay()` 後第一個 RegisterMember 呼叫時

---

### A1-03: RegisterMember + FFrameModelDef 組裝

**工時**: 4 小時

**涉及 UE5 class / API**:
- `UFrameAnalysisLibrary::SolveLinear()` (初始化用)
- `FFrameModelDef` (17 個 input USTRUCT 中的 aggregate)
- `FFrameMaterial`, `FFrameSection`, `FFrameNode`, `FFrameMember` USTRUCT
- `UFrameMaterialLibrary::GetMaterial(EFrameMaterialPreset)` — 預設材料庫
- `UFrameSectionLibrary::MakeRectangular(float b, float d)` — 截面工廠
- `UFrameInteractiveSubsystem::StartSession(FFrameModelDef)` — 啟動 FrameCore session

**具體工作**:
1. 實作 `UArchSimModelRegistry::RegisterMember(UArchSimMemberData* Comp)`:
   - 從 Comp 的 owning Actor Transform 計算兩端節點座標 (mm 單位, UE cm → mm 乘 10)
   - 用 `FFrameNode` 加入 `CurrentModel.Nodes[]`,若節點已存在 (距離 < 1mm) 則重用 index
   - 用 `FFrameMember{.matIdx=Comp->MaterialId, .secIdx=Comp->SectionId, .NodeI=..., .NodeJ=...}` 加入 `CurrentModel.Members[]`
   - 分配 `MemberIdx = NextMemberIdx++`; 回填到 `Comp->MemberIdx`; 加入映射表; 標記 `Comp->bRegistered = true`
2. 實作節點去重 helper: `int32 FindOrAddNode(const FVector& PosUE)` — 線性掃描 (50 構件以下 O(N) 可接受; 後期可改 spatial hash)
3. 所有 RegisterMember 完成後,外部呼叫一次 `FlushAndStartSession()`: 呼叫 `UFrameInteractiveSubsystem::StartSession(CurrentModel)` 初始化 FrameCore session
4. 加入保護: 若 `UFrameInteractiveSubsystem` 未取到 (GameInstance 未準備好) 則 defer 到下一個 Tick

**完成標準**:
- 連續放置 50 個構件,`RegisterMember` 總耗時 < 50ms (PIE Profile 量測)
- `CurrentModel.Members.Num()` 正確等於已放構件數
- 節點去重正確: 兩端共用節點的相鄰梁, Nodes[] 中只有一份共用節點 entry
- 單位轉換正確: UE 世界座標 100cm = FrameCore 1000mm

**預期踩雷**:
- `FFrameNode` 座標單位是 mm;UE 世界座標是 cm;**忘記乘 10 是最常見的 silent bug** — 結構解算結果看起來合理但縮放錯 10 倍
- `FFrameMember.RefVec` 預設指向 +Z (UE +Z = 上方);若梁是垂直柱 (方向也是 +Z),RefVec 與桿方向平行會觸發 FrameCore 內部 `localAxes` 計算錯誤;需加 guard: 若桿方向 ≈ +Z 則 RefVec 改用 +X

---

### A1-04: ApplyPatchAndResolve debounce 包裝

**工時**: 3 小時

**涉及 UE5 class / API**:
- `UFrameInteractiveSubsystem::ApplyPatchAndResolve(FFrameModelPatch, FFrameSolveResult&)` (已存在於 FrameCoreUE)
- `FFrameModelPatch` USTRUCT
- `FTimerManager::SetTimer()` — UE5 計時器 (用於 debounce)
- `GetWorld()->GetTimerManager()`

**具體工作**:
1. 在 `UArchSimModelRegistry` 新增 `void RequestSolve(FFrameModelPatch Patch)` 公開方法
2. 內部維護 `FFrameModelPatch PendingPatch` 和 `FTimerHandle DebounceTimer`
3. 每次 `RequestSolve()` 呼叫:將新 Patch merge 進 `PendingPatch`(累積多個變更); 重置 150ms timer
4. Timer 到期後執行 `ExecuteSolve()`:
   - 呼叫 `UFrameInteractiveSubsystem::ApplyPatchAndResolve(PendingPatch, OutResult)`
   - 清空 `PendingPatch`
   - 若 `OutResult.bSingular` 則不呼叫 Distribute (機構狀態)
   - 否則呼叫 `DistributeSolveResult(OutResult)`
5. 加入 `int32 PendingRankAccumulation` 追蹤累積 patch rank; 超過 MaxRank=96 時立即跳過 timer 直接 `Rebaseline()` 再 Solve

**完成標準**:
- 快速拖拉 10 次構件只觸發一次 Solve (debounce 正常)
- `bSingular=true` 時不呼叫 Distribute (無 NaN 熱圖)
- 超過 MaxRank=96 自動 Rebaseline 可通過 BP 測試觸發

**預期踩雷**:
- FTimerHandle 若 Subsystem 被 Deinitialize 而 Timer 未清除,會觸發 dangling callback;`Deinitialize()` 中必須呼叫 `GetWorld()->GetTimerManager().ClearTimer(DebounceTimer)`
- `FFrameModelPatch` 的 merge 語義需要確認: `ReactivateMemberIds` 用 Append, `DeactivateMemberIds` 用 Append (確保兩者不互相覆蓋)

---

### A1-05: DistributeSolveResult + CachedUtilization 回寫

**工時**: 2 小時

**涉及 UE5 class / API**:
- `FFrameSolveResult.MemberUtilization[]` (TArray\<FFrameMemberUtilization\>)
- `FFrameMemberUtilization.DC` (float, Demand/Capacity 比值)
- `TWeakObjectPtr<UArchSimMemberData>::IsValid()`
- `MulticastDelegate` — 廣播 Solve 完成事件給 HUD / 熱圖 Actor

**具體工作**:
1. 實作 `void UArchSimModelRegistry::DistributeSolveResult(const FFrameSolveResult& Result)`:
   - 遍歷 `IndexToComponent` 映射表
   - 若 `IndexToComponent[idx].IsValid()`:取 `Result.MemberUtilization[idx].DC` 寫入 `Comp->CachedUtilization`
   - 若 index 超出 `MemberUtilization.Num()` 則寫 0 (安全守衛)
2. 宣告並廣播 `DECLARE_MULTICAST_DELEGATE_OneParam(FOnSolveComplete, const FFrameSolveResult&)` delegate
3. 呼叫 `OnSolveComplete.Broadcast(Result)` 讓 HUD / 熱圖 Actor 訂閱更新

**完成標準**:
- Solve 後所有已放置構件的 `CachedUtilization` 正確更新
- 整合測試: 用 F58 cantilever fixture 對標, UE 端 CachedUtilization 與 standalone D/C 差 < 1e-5
- `bSingular=true` 時 Distribute 不被呼叫 (透過 A1-04 guard)

**預期踩雷**:
- `MemberUtilization` 的 `DC` 欄位名稱需確認; 從 v3.4.0 USTRUCT 定義查實際欄位名 (`FFrameMemberUtilization.Utilization` 還是 `.DC`); 若不確定先讀 `Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameTypes.h`

---

### A1-06: DeactivateMember 軟刪除 + stale 偵測

**工時**: 2 小時

**涉及 UE5 class / API**:
- `FFrameModelPatch.DeactivateMemberIds` (TArray\<int32\>)
- `UFrameInteractiveSubsystem::ApplyPatchAndResolve()` (走現有 debounce 路徑)

**具體工作**:
1. 實作 `void UArchSimModelRegistry::DeactivateMember(int32 MemberIdx)`:
   - 標記 `IndexToComponent[MemberIdx]` 指向的 Component 為 invalid (或直接從映射移除)
   - 建立 `FFrameModelPatch patch; patch.DeactivateMemberIds.Add(MemberIdx);`
   - 呼叫 `RequestSolve(patch)`
2. 加入 stale 偵測: 每次 `DistributeSolveResult` 前遍歷映射表, 移除所有 `!IsValid()` 的 TWeakObjectPtr
3. 加入 `void ReactivateMember(int32 MemberIdx)` 對應激活 (用於 undo 系統)

**完成標準**:
- 移除一根梁後 Registry 正確發出 DeactivateMemberIds Patch
- 移除後再 Solve,被移除構件的 MemberIdx 在 FrameCore 中標記為 inactive (bSingular 不因此觸發, 除非移除造成機構)
- 50 次快速 Add/Remove 後映射表無記憶體泄漏 (WeakPtr stale 自動清除)

**預期踩雷**:
- FrameCore 的 `DeactivateMemberIds` 走的是 Woodbury 增量更新,累積 rank 要計入 debounce 的 PendingRankAccumulation

---

### A1-07: SaveGame round-trip 整合測試

**工時**: 3 小時

**涉及 UE5 class / API**:
- SPUD plugin API: `USpudSubsystem::SaveGame(SlotName)` / `LoadGame(SlotName)`
- `ISpudObject` 介面 (SPUD 自動掃描 `UPROPERTY(SaveGame)`)
- `OnPreSave` / `OnPostLoad` SPUD callback

**具體工作**:
1. 確認 `UArchSimMemberData` 的 `UPROPERTY(SaveGame)` 欄位 (`MemberIdx`, `StructureGroupId`, `CachedUtilization`) 被 SPUD 正確序列化
2. 實作 `UArchSimModelRegistry::OnWorldPostLoad()` callback: 重建映射表 (遍歷已載入的所有 UArchSimMemberData)
3. 實作 SaveGame 前 `OnPreSave()`: 序列化 `CurrentModel` 為 JSON 字串並存入 SaveGame slot
4. 實作 LoadGame 後 `OnPostLoad()`: 從 JSON 重建 `CurrentModel`,呼叫 `FlushAndStartSession()`,再 Solve 一次復原結果
5. 寫 UE Automation Test: `FrameCore.ArchSim.SaveLoadRoundTrip` — 放 3 個構件 → Save → 清空 → Load → 驗 MemberIdx 一致 + CachedUtilization 在 Solve 後接近原值

**完成標準**:
- SaveGame → LoadGame 後 MemberIdx 與原值一致
- 載入後 Solve 的 CachedUtilization 與存檔前差 < 1e-4 (重解結果一致)
- 空模型 Save/Load 不崩

**預期踩雷**:
- SPUD 對 World Partition 有 issue #117; MVP 先用固定場景 (不開 World Partition streaming),後期升級時再驗 save/load

---

### A1-08: Rebaseline 觸發條件完整實作 (Phase 2)

**工時**: 3 小時

**涉及 UE5 class / API**:
- `UFrameInteractiveSubsystem::Rebaseline()` (已存在)
- `UFrameInteractiveSubsystem::StartSession(NewModel)` (材料/截面改變時需重建 session)

**具體工作**:
1. 識別所有需要 Rebaseline 而非 Patch 的觸發條件:材料變更 / 截面尺寸變更 / 新增節點導致 K 矩陣尺寸改變
2. 在 `RegisterMember` 中加入判斷: 若 `CurrentModel.Materials[]` 或 `Sections[]` 發生實質改變,標記 `bNeedsRebaseline = true`
3. `ExecuteSolve()` 中若 `bNeedsRebaseline`: 先呼叫 `Rebaseline()` 再 `ApplyPatchAndResolve()`
4. 加入 Phase 2 完成標準: 改材料後 100ms 內熱圖正確更新 (非殘留舊 K 矩陣結果)

**完成標準**:
- 改一根梁的材料 (S235 → S355),熱圖在 200ms 內正確反映新 E 值
- Rebaseline 後 bSingular 不誤觸發

---

## A2 ALS 角色與相機系統 — 詳細實作計畫

### 子節定位

ALS-Refactored v4.17 是 MIT 授權的高品質角色動畫框架,為玩家提供流暢的第三人稱移動基礎。本節的重點是**最小侵入地繼承 ALS 子類**並加入建築模式輸入切換,不要過度自訂 ALS 內部邏輯以免升級困難。

### A2-01: AArchSimCharacter + ALS 子類骨架

**工時**: 4 小時

**涉及 UE5 class / API**:
- `AAlsCharacter` (ALS-Refactored 主角色類,`#include "AlsCharacter.h"`)
- `UAlsCameraComponent` (ALS 相機元件)
- `UArchSimMemberData` (角色自身不掛,但了解其存在)
- `Config/DefaultEngine.ini`: `DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput`

**具體工作**:
1. 確認 ALS-Refactored v4.17 plugin 可在 UE5.7 下乾淨 build (驗證步驟: Build.bat 後無 Error)
2. 建立 `Source/ArchSim/Public/Characters/ArchSimCharacter.h`: `UCLASS() class AArchSimCharacter : public AAlsCharacter`
3. 加入 `UAlsCameraComponent* CameraComponent;` 並在建構子中 `CreateDefaultSubobject<UAlsCameraComponent>`
4. 加入 `bool bInBuildingMode = false;` 狀態欄位 (UPROPERTY(Replicated))
5. 加入 `UPROPERTY(Replicated)` 並覆寫 `GetLifetimeReplicatedProps()` 確保 bInBuildingMode 同步到 Client
6. 建立 BP 子類 `BP_ArchSimCharacter` 繼承 `AArchSimCharacter`,設定 ALS Mesh、AnimBP、相機參數
7. 在 `DefaultEngine.ini` 加入 ALS 要求的 `DefaultPlayerInputClass` 設定

**完成標準**:
- PIE 中 AArchSimCharacter 行走/跑步/跳躍五大動作正常播放
- `bInBuildingMode` 在 Server/Client 之間正確同步 (Listen Server 測試: Host 切換後另一玩家看到狀態變化)
- Build 無 Warning 與 Error

**預期踩雷**:
- ALS v4.17 移除 Animation Compression Library 常數插值: 若使用預設 ALS AnimBP 可能 bake 曲線失效,先以 ALS 自帶的 Demo 角色確認基本動作; 自訂 AnimBP 留 Phase 3
- Iris 模式下 `bInBuildingMode` UPROPERTY(Replicated) 需加 `MARK_PROPERTY_DIRTY_FROM_NAME(AArchSimCharacter, bInBuildingMode)` 否則 Client 永不收到; 若先用 Legacy Replication 可暫時跳過

---

### A2-02: Enhanced Input MappingContext 雙層設定

**工時**: 3 小時

**涉及 UE5 class / API**:
- `UEnhancedInputComponent` (`#include "EnhancedInputComponent.h"`)
- `UEnhancedInputLocalPlayerSubsystem::AddMappingContext()` / `RemoveMappingContext()`
- `UInputMappingContext` Asset (`MC_Locomotion`, `MC_Building`)
- `UInputAction` Asset (`IA_PlaceMember`, `IA_RemoveMember`, `IA_OpenAnalysisPanel`, `IA_ToggleHeatmap`, `IA_ApplyLoad`, `IA_ToggleBuildingMode`)
- `DA_ArchSimInputActions` DataAsset (整合所有 InputAction 參考)

**具體工作**:
1. 建立 `MC_Locomotion` MappingContext Asset: 包含 ALS 要求的標準移動 binding (WASD / 滑鼠 / Space)
2. 建立 `MC_Building` MappingContext Asset: 加入 `IA_PlaceMember (LMB)` / `IA_RemoveMember (RMB)` / `IA_ToggleHeatmap (H)` / `IA_ApplyLoad (F)` / `IA_ToggleBuildingMode (E)` / 旋轉 `IA_RotatePrefab (R)`
3. 在 `AArchSimCharacter::SetupPlayerInputComponent()` 中:
   - 取 `UEnhancedInputLocalPlayerSubsystem`
   - `AddMappingContext(MC_Locomotion, 0)`
   - 綁定 `IA_ToggleBuildingMode` → `OnToggleBuildingMode()`
4. 實作 `OnToggleBuildingMode()`:
   - 若 `!bInBuildingMode`: `AddMappingContext(MC_Building, 1)`, 設 `bInBuildingMode=true`
   - 若 `bInBuildingMode`: `RemoveMappingContext(MC_Building)`, 設 `bInBuildingMode=false`
5. Server RPC `Server_SetBuildingMode(bool bNewMode)` 同步狀態到 Server

**完成標準**:
- 按 E 切換後, 建築模式輸入 (LMB 放置) 正常觸發; 行走輸入不殘留
- 再按 E 切回後, 行走恢復; 建築輸入不再觸發
- Splitscreen 測試基礎: 兩個 PlayerController 各自獨立的 EnhancedInput subsystem 不互相影響 (5.7 需實測)

**預期踩雷**:
- Enhanced Input `AddMappingContext` 在 Splitscreen 第 2 玩家的 `OnPossess` 時機 bug (UE5.6 已知): 對策是在 `AArchSimCharacter::PawnClientRestart()` 中延 1 frame 再 `AddMappingContext` (使用 `GetWorld()->GetTimerManager().SetTimerForNextTick(...)`)

---

### A2-03: Server RPC 橋接到 UFrameInteractiveSubsystem

**工時**: 3 小時

**涉及 UE5 class / API**:
- `UFUNCTION(Server, Reliable, WithValidation)` 宣告
- `UArchSimModelRegistry::RequestSolve()` (A1-04)
- `UArchSimPrefabSpawnSubsystem` (Part B 的子系統,此處只宣告接口)

**具體工作**:
1. 在 `AArchSimCharacter` 中宣告:
   - `UFUNCTION(Server, Reliable, WithValidation) void Server_RequestPlacement(FVector GridPos, FRotator Rot, FName PrefabId);`
   - `UFUNCTION(Server, Reliable, WithValidation) void Server_RequestRemoval(int32 MemberIdx);`
   - `UFUNCTION(Server, Reliable) void Server_RequestLoadApply(int32 MemberIdx, FFrameNodalLoad Load);`
2. `_Validate()` 函數加入基本守衛: `GridPos` 在允許範圍內 (`FBox(FVector(-5000,-5000,0), FVector(5000,5000,5000))` cm 單位); `PrefabId` 不為空
3. `_Implementation()` 函數框架: 目前只呼叫 `UE_LOG` + 預留 `// TODO: call UArchSimPrefabSpawnSubsystem` 注解 (B1 完成後填入)
4. 在建築模式 Input handler 中 `if (!HasAuthority()) { Server_RequestPlacement(...) } else { Server_RequestPlacement_Implementation(...) }`
5. 加入基本 `HasAuthority()` 判斷確保只有 Server 執行 Solve

**完成標準**:
- PIE 2 Players: Client 按 LMB 觸發 RPC, Server Log 可看到 `Server_RequestPlacement` 被呼叫
- `_Validate()` 返回 false 時 Client 連線不被 kick (UE5.7 WithValidation 行為需確認: 預設 false 會 disconnect,需改為 log + return true 教育場景不適合 kick)
- 非 Authority 端不直接呼叫 Solve (防止 Client 自行觸發計算)

**預期踩雷**:
- 教育場景不適合因 `_Validate()` false 而 kick 玩家; 將驗證失敗改為 Server-side log + return true 並忽略請求

---

### A2-04: 第一人稱 / 第三人稱切換 (Phase 2)

**工時**: 3 小時

**涉及 UE5 class / API**:
- `UAlsCameraComponent::SetMode()` (需驗證 ALS v4.17 API,可能名稱不同)
- `ACameraActor` 或 `UCameraComponent` 切換 ViewTarget

**具體工作** (Phase 2, 略述):
1. 加入 `IA_ToggleCameraMode` 輸入
2. ALS 相機切換第一人稱模式 (需查 ALS v4.17 文件確認 API)
3. 確認切換後 HUD 不受影響

**完成標準**: 按 V 鍵在第一/第三人稱間無縫切換

---

### A2-05: Splitscreen EnhancedInput workaround (Phase 2)

**工時**: 2 小時

詳述見 A2-02 踩雷。Phase 2 正式實作並加入 Automation Test 驗證兩個玩家各自的 MappingContext 獨立。

---

## A3 世界系統 — 詳細實作計畫

### 子節定位

世界系統提供玩家活動的物理空間基礎。MVP 階段以**靜態場景骨架 + 放置範圍限制**為核心;PCG 地形程序化生成列為 Phase 2 (UE5.7 PCG 已 Production-Ready,但整合需要額外調試時間)。

### A3-01: 主場景 L_ArchSim_Sandbox 骨架 + World Partition 開啟

**工時**: 3 小時

**涉及 UE5 class / API**:
- UE5 Editor: 新建 Level, 開啟 World Partition (Level → World Partition → Convert Level)
- `AWorldSettings` 中的 `bEnableWorldPartition`
- Data Layers (World Partition 子系統)
- 基本 Sky / Atmosphere / DirectionalLight 設定

**具體工作**:
1. 新建 Level `L_ArchSim_Sandbox` (Open World template)
2. 確認 World Partition 已啟用 (UE5.7 Open World template 預設開啟)
3. 加入基本場景元素: `BP_Sky_Sphere` / `DirectionalLight` / `SkyAtmosphere` / `VolumetricCloud`
4. 加入地面平面 `SM_Ground_Flat` (StaticMesh, 100m×100m, Collision = WorldStatic)
5. 加入場景邊界可視指示器: 50m×50m 範圍的 Fence (PostProcessVolume 邊界提示或 BP 邊界牆)
6. 設定 GameMode: 指向 `BP_ArchSimGameMode` (使用 `AArchSimCharacter` 作為 DefaultPawnClass)
7. 設定基礎座標: (0,0,0) 為場景中心,+Y=北,+X=東,+Z=上

**完成標準**:
- PIE 開啟 L_ArchSim_Sandbox 不崩,角色正常 Spawn
- World Partition cell 可在 Editor 中看到分格
- 無明顯畫面錯誤 (天空、光源正常)

**預期踩雷**:
- SPUD + World Partition issue #117: MVP 期間避免在 World Partition streaming cell 邊界附近存放重要 Actor,減少 SPUD save/load 風險

---

### A3-02: 場景邊界牆 + 放置範圍鎖定 helper

**工時**: 2 小時

**涉及 UE5 class / API**:
- `FBox` + `FBox::IsInside(FVector)` — 範圍判斷
- `UKismetSystemLibrary::BoxOverlapActors()` — 碰撞查詢 (可選)
- `ABlockingVolume` — UE5 內建不可見阻擋體積

**具體工作**:
1. 在 Level 中放置 4 面 `ABlockingVolume` 作為邊界牆 (50m×50m×50m 立方體邊界)
2. 在 `UArchSimModelRegistry` 加入靜態 helper:
   ```cpp
   static bool IsInPlacementBounds(FVector WorldPos) {
     return FBox(FVector(-5000,-5000,0), FVector(5000,5000,5000))
            .IsInside(WorldPos);
   }
   ```
3. 在 `Server_RequestPlacement_Validate()` 中使用此 helper 阻擋越界放置請求
4. 在 Ghost 預覽 (Part B) 中也使用此 helper 顯示紅色邊界提示 (預留 B1 接口)

**完成標準**:
- 角色走到邊界被 BlockingVolume 阻擋
- `IsInPlacementBounds()` 函數可在 BP 中取用並驗收邊界值

**預期踩雷**:
- BlockingVolume 的碰撞 Profile 需設 `BlockAll` 才能阻擋玩家; 預設可能是 `NoCollision`

---

### A3-03: 地形 snap helper + 支承節點自動固接邏輯

**工時**: 4 小時

**涉及 UE5 class / API**:
- `UWorld::LineTraceByChannel()` — Raycast 取地形高度
- `ETraceTypeQuery::TraceTypeQuery1` 或 `ECC_WorldStatic` Channel
- `FHitResult.ImpactPoint` — 取地形碰撞點
- `FFrameNode.IsFixed` 欄位 (`bool Fixed[6]`) — FrameCore 支承條件

**具體工作**:
1. 實作 `float UArchSimModelRegistry::GetGroundHeight(FVector2D XY)`:
   - 從 `FVector(XY.X, XY.Y, 10000)` 往 -Z 方向 LineTrace
   - 取 `HitResult.ImpactPoint.Z` 作為地形高度
   - 無 Hit 時回傳 0
2. 實作 `bool IsNodeOnGround(FVector NodePosUE, float Tolerance=2.0f)`:
   - 若節點 Z 座標與 `GetGroundHeight()` 差 < Tolerance(cm) 則認為在地面上
3. 在 `RegisterMember` 組裝 `FFrameNode` 時:
   - 若 `IsNodeOnGround(NodePosUE)` 為 true,設 `Node.Fixed = {true, true, true, true, true, true}` (完全固接)
   - 否則 `Node.Fixed = {false, false, false, false, false, false}` (自由節點)
4. 加入 `void RecheckGroundSupports()` — 地形改變後重新掃描所有節點支承條件並 Rebaseline

**完成標準**:
- 放置一根柱,柱底節點自動設為固接支承
- 將柱抬離地面後重新檢查,底節點變自由
- 固接支承節點在 FrameCore Solve 後 `FFrameNodalReaction.bHasConstrainedDof = true`
- 節點 mm 座標正確: 地面 Z=0cm 對應 FrameCore Z=0mm

**預期踩雷**:
- `LineTrace` 在 PIE 中若地面 StaticMesh 碰撞未設定為 `WorldStatic` 會 miss; 需確認 SM_Ground_Flat 的 collision profile
- 地形不平時 (Phase 2 PCG 地形),不同位置的地面高度不同;`GetGroundHeight()` 的 Raycast 需以構件兩端個別計算

---

### A3-04: PCG BaseTerrain 程序化地形生成 (Phase 2)

**工時**: 5 小時

**涉及 UE5 class / API**:
- `UPCGSubsystem` (UE5.7 PCG, Production-Ready)
- `UPCGGraphInterface` / `UPCGComponent`
- `UPCGSurfaceSamplerSettings` — 表面取樣
- **`UPCGSpatialNoiseSettings`(Mode = `Perlin2D`)— 訂正 2026-06-25:UE5.7 PCG 無獨立 PerlinNoise 節點,改用 Spatial Noise**
- `FPCGPoint` + Height Offset
- `ALandscape` 或 `UStaticMeshComponent` 作為地形底面

**具體工作** (Phase 2, 略述):
1. 建立 PCG Graph `PCG_BaseTerrain`: Surface Sampler → **Spatial Noise(Mode=Perlin2D,Octave 3, Amplitude ±5m)** → Apply Height → Output Points
2. 設定 Seed 輸入 Pin, 確保同一 Seed 每次輸出相同高度場 (PCG 確定性要求)
3. 整合到 A3-03 的 `GetGroundHeight()`: 改為從 PCG 生成的 LandscapeProxy 做 Raycast

**完成標準**: 同一 Seed 每次載入產生相同地形; 玩家走動不因 PCG 更新而卡頓

---

### A3-05: 5 種沙盒基地模板 Data Asset (Phase 2)

**工時**: 3 小時

**涉及 UE5 class / API**:
- `UDataAsset` 子類 `UArchSimSiteTemplate`
- 欄位: `FString SiteName`, `TSoftObjectPtr<UWorld> SiteLevel`, `UTexture2D* PreviewImage`, `FText Description`

**具體工作** (Phase 2):
1. 建立 5 個 Level Asset: `L_Site_Flat`, `L_Site_Slope`, `L_Site_Riverside`, `L_Site_UrbanInfill`, `L_Site_Hillside`
2. 每個 Level 有對應的地形 Mesh + 周邊環境 Mesh
3. UI 選擇畫面讀取 DataAsset 列表顯示預覽

---

### A3-06: LevelStreaming Volume 切塊無感載入 (Phase 2)

**工時**: 4 小時

**涉及 UE5 class / API**:
- `ALevelStreamingVolume` — UE5 內建串流觸發體積
- `ULevelStreaming::SetShouldBeLoaded()` / `SetShouldBeVisible()`
- World Partition 自動化串流 (UE5.7 可能取代手動 LevelStreamingVolume)

**具體工作** (Phase 2): World Partition 在 UE5.7 應可自動處理串流;優先測試 World Partition 自動串流是否滿足需求,不滿足再手動設定 LevelStreamingVolume。

---

## Part A 總工時彙整

| 子節 | 任務 | 工時 (hr) | Phase |
|------|------|-----------|-------|
| A1 | A1-01 MemberData Component | 2 | MVP |
| A1 | A1-02 ModelRegistry Subsystem | 3 | MVP |
| A1 | A1-03 RegisterMember + ModelDef 組裝 | 4 | MVP |
| A1 | A1-04 ApplyPatchAndResolve debounce | 3 | MVP |
| A1 | A1-05 DistributeSolveResult 回寫 | 2 | MVP |
| A1 | A1-06 DeactivateMember 軟刪除 | 2 | MVP |
| A1 | A1-07 SaveGame round-trip 整合測試 | 3 | MVP |
| A1 | A1-08 Rebaseline 完整觸發 | 3 | Phase 2 |
| A2 | A2-01 AArchSimCharacter ALS 子類 | 4 | MVP |
| A2 | A2-02 Enhanced Input 雙層 MappingContext | 3 | MVP |
| A2 | A2-03 Server RPC 橋接 | 3 | MVP |
| A2 | A2-04 第一/第三人稱切換 | 3 | Phase 2 |
| A2 | A2-05 Splitscreen EnhancedInput workaround | 2 | Phase 2 |
| A2 | A2-06 ALS AnimBP curves bake | 4 | Phase 3 |
| A3 | A3-01 主場景骨架 + World Partition | 3 | MVP |
| A3 | A3-02 場景邊界 + 放置範圍 helper | 2 | MVP |
| A3 | A3-03 地形 snap + 支承固接邏輯 | 4 | MVP |
| A3 | A3-04 PCG BaseTerrain | 5 | Phase 2 |
| A3 | A3-05 5 種基地模板 | 3 | Phase 2 |
| A3 | A3-06 LevelStreaming 切塊載入 | 4 | Phase 2 |
| | **合計** | **82** | |

**MVP 合計**: 33 小時 (A1: 19 hr + A2: 10 hr + A3: 9 hr)
**Phase 2 合計**: 37 小時
**Phase 3 合計**: 4 小時 (ALS AnimBP)

---

## 建議實作順序 (內部依賴圖)

```
A1-01 → A1-02 → A1-03 → A1-04 → A1-05 → A1-06 → A1-07
                                                    ↓
A2-01 → A2-02 → A2-03 (平行可與 A1 同步進行)
↓
A3-01 → A3-02 → A3-03 (需在 A2-01 Character 存在後整合)
```

A1 是最高優先; A2 與 A1 可在不同子系統檔案中平行開發; A3-01/A3-02 純 Editor 操作可最早完成。

---

## 跨 Part 依賴聲明

- **Part B (構件放置)** 依賴 A1-01 ~ A1-06 全部完成: `UArchSimModelRegistry::RegisterMember()` 是 B1 Prefab Spawn 後的必呼叫路徑
- **Part C (工程模擬)** 依賴 A1-04/A1-05: debounce 包裝和 Distribute 是 C1/C2 功能的計算管道
- **Part D (視覺化)** 依賴 A1-05 的 `OnSolveComplete` delegate: 熱圖 Actor 訂閱此 delegate 接收 FFrameSolveResult
- **Part E (多人)** 依賴 A2-01/A2-03: AArchSimCharacter 是多人 Pawn 基類; Server RPC 骨架是多人放置的基礎

---

## 關鍵風險防護備忘

1. **FrameCore FROZEN**: 所有橋接工作在 `Source/ArchSim/` 與 `FrameCoreUE/` 中完成; 若發現任何需要改動 `Plugins/FrameSolver/Source/FrameCore/` 的需求,必須設計 UE consumer 層的 adapter 解法而非修改引擎
2. **FFrameSolveResult 欄位名稱**: `MemberUtilization[idx].DC` 的確切欄位名在不同 USTRUCT 版本可能不同; 實作前先讀 `Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameTypes.h` 確認
3. **單位一致性**: UE cm ↔ FrameCore mm 的 ×10 轉換是整個接合層最高密度的 silent bug 來源,建議在 `UArchSimModelRegistry` 中定義 `constexpr float kUE2MM = 10.f;` 並強制所有轉換走此常數
4. **NaN 防護**: `bSingular=true` 時絕對不進入 Distribute; `CachedUtilization` 寫入前加 `!FMath::IsNaN(DC)` 守衛


---


# Part B 實作擴充(192h)

**標題**:Part B:玩家操作層(B1 構件放置 / B2 2D↔3D 工作區 / B3 圖學標註 / B4 藍圖系統)

**子節數**:4

### MVP 必須(Phase 1)
- B1-T1:UArchSimPrefabSpawnSubsystem 骨架 + OccupancyGrid TMap
- B1-T2:Server_RequestPlacement RPC + 衝突仲裁
- B1-T3:Client Ghost 預覽(UMaterialInstanceDynamic 半透明 + 格線 Snap)
- B1-T4:Prefab 庫定義 — 5 大類 × 3 變體(15 Prefab DataAsset)
- B1-T5:R 鍵旋轉 90° + ESC 取消 + 放置後觸發 ApplyPatchAndResolve
- B1-T6:構件刪除路徑(滑鼠右鍵 / Del 鍵)+ DeactivateMemberIds patch
- B2-T1:UFastDesignWorkspaceWidget UMG 骨架 + 四象限切割
- B2-T2:三個正交 USceneCaptureComponent2D + RenderTarget(俯/正/側)
- B2-T3:USelectionState 廣播層 — OnMemberSelected/Hovered 雙向連動
- B4-T1:藍圖儲存路徑 — 框選 + CreatePrefabFromActors 封裝

### Phase 2
- B1-T7:構件 HUD 左側面板 UMG Widget(滾輪切換類別 + 搜尋)
- B2-T4:Tab 鍵三模式切換(四象限 / 純 3D / 純 2D)+ 鍵盤快捷 layout
- B2-T5:切面工具(切面 plane 定義 + 剖面圖象限切換)
- B3-T1:AArchSimAnnotation 基類 + 5 個子類 Actor 骨架(Dimension/SectionMark/TextLabel/AxisLine/TitleBlock)
- B3-T2:2D 視圖滑鼠事件接管(D/S/T 快捷 + 點擊兩點生成尺寸線)
- B3-T3:軸線自動編號邏輯(插入/刪除後連續重編)
- B3-T4:標題塊 DataAsset DA_TitleBlockTemplate + UMG 內編輯介面
- B3-T5:比例尺自動偵測(viewport 寬 / 視野範圍 → 最近標準比例)
- B4-T2:縮圖截圖 USceneCaptureComponent2D(俯視 + 30° 透視 256×256 PNG)
- B4-T3:藍圖庫 UMG Widget(grid 縮圖 + 名稱 + 構件數顯示)
- B4-T4:Ghost 預覽旋轉(R/Shift+R) + 鏡像(F 鍵)+ 節點重映射
- B4-T5:.archsim_bp JSON 匯出/匯入 + schema version 欄

### Phase 3
- B2-T6:隱藏線/虛線渲染(CNS B1001 規範的隱線判定計算)
- B3-T6:DXF 匯出時標註保留為 DIMENSION/TEXT entity(接 MctoNurbs sidecar)
- B3-T7:標註 Layer 可見性 toggle(多層標註管理)
- B4-T6:藍圖分享 — 老師 broadcast 端 + 學生接收端 UI
- B4-T7:非對稱截面的鏡像 Iy/Iz 對換處理(Phase 2 加非對稱截面後才需要)

### 實作順序
- B1(構件放置)→ B4(藍圖系統)→ B2(2D↔3D 工作區)→ B3(圖學標註)
- B1 是最先決條件:所有其他 Part 都需要構件能被正確放置並映射到 FrameCore
- B4 依賴 B1 的 Actor Spawn / ModelRegistry 機制;批次放置邏輯共用
- B2 需要 B1 的構件 Actor 存在才能做 SceneCapture 投影
- B3 需要 B2 的正交視圖 Widget 才能在上面疊加標註 Actor

### 跨 Part 依賴
- Part A (A1 UArchSimModelRegistry + UArchSimMemberData) 必須先完成 — B1 的所有 Spawn 動作都依賴 Registry 的 RegisterMember / DistributeSolveResult
- Part A (A2 ALS 角色 + Enhanced Input) 必須先完成 — B1 的 IA_PlaceMember / IA_RemoveMember 輸入動作綁在 MC_Building MappingContext
- Part A (A3 世界系統) 必須先完成 — B1 的放置範圍與 Grid Snap 依賴地形 Z 高度 + 放置邊界 FBox
- FrameCore UFrameInteractiveSubsystem (FrameCoreUE plugin) 已存在且 FROZEN — B1/B4 直接呼叫，不得修改 engine source

### 風險清單
- ~~Prefabricator .uplugin EngineVersion 未標 5.7~~ → **✅ 已修(S-00 加 `"EngineVersion": "5.7.0"`)**
- USceneCaptureComponent2D 三路同時 render 帶來 3× GPU 負擔;正交視圖需強制 LOD 0 + bCaptureEveryFrame=false(只有構件變動才 trigger 更新)
- OccupancyGrid TMap<FIntVector, FActorHandle> 在 Server 端 GC 後 FActorHandle 可能失效;必須用 TWeakObjectPtr<AActor> 替代或定期 ValidateHandle
- 旋轉超 360° 後 FRotator wrap-around:統一在 RPC payload 中呼叫 FRotator::Normalize() 再序列化
- B4 藍圖載入大型場景(100+ 構件)觸發批次 RegisterMember → 單次 Rebaseline 可能超過 200ms;需非同步佇列 + 載入動畫遮罩
- B2 切面工具在 0 平面附近容易產生 NaN:切面 plane normal 必須強制 |n|>ε = 1cm;切面位置有最小距離限制
- B3 軸線自動重編號在多人模式下同時刪軸線會競爭;Server-authoritative 的軸線 ID 序列需在 GameState 端統一管理
- FrameCore FROZEN 邊界:B1/B4/B2/B3 所有新增類別都必須在 ArchSim game module 或 FrameCoreUE consumer 層,絕不可修改 Plugins/FrameSolver/Source/FrameCore/ 下任何檔案

## 詳細擴充內容


# Part B 玩家操作層 — 詳細實作計畫

> 適用版本:UE5.7 + FrameCore v4.0.0 (FROZEN) + Prefabricator UE5 + ALS-Refactored v4.17
> 開發假設:1-2 人小團隊,每人日均有效工時 5-6 小時
> 工時單位:小時(h),每個 sub-task 上限 4h

---

## 總覽

Part B 是整個「建築師模擬器」的**核心互動層**。玩家操作層介於 Part A(引擎外殼)與 Part C(工程模擬)之間,是學生觸碰最頻繁、認知負擔最集中的介面層。Part B 的完成品質直接決定「直覺優先學習」(Productive Failure)能否成立。

Part B 共 4 個子節,依據依賴關係建議實作順序:B1 → B4 → B2 → B3。

**Part 層級估工:**
- B1 構件放置系統:約 52h
- B2 2D↔3D 工作區:約 64h
- B3 圖學標註系統:約 40h
- B4 藍圖系統:約 36h
- **Part B 合計:約 192h**

---

## B1 構件放置系統

### 設計目標

玩家在 3D 場景中放置結構構件的主要互動流程:選構件→預覽→確認→Server 仲裁→FrameCore 重解→熱圖更新。全流程端到端延遲 ≤200ms。

### 依賴前置

- **Part A A1 必須完成**:`UArchSimMemberData` Component + `UArchSimModelRegistry` 雙向映射機制就緒
- **Part A A2 必須完成**:`AArchSimCharacter` + `MC_Building` MappingContext + `IA_PlaceMember` / `IA_RemoveMember` 輸入動作定義
- **Part A A3 完成地形基礎**:放置格線 Snap 依賴地形 Z 高度

### Sub-task 清單

---

#### B1-T1: UArchSimPrefabSpawnSubsystem 骨架 + OccupancyGrid (MVP)

**工時估算:** 3h

**涉及 UE5 Class / API:**
- `UGameInstanceSubsystem`(基類)
- `TMap<FIntVector, TWeakObjectPtr<AActor>> OccupancyGrid`(Server-only)
- `FIntVector`(格子整數座標)
- `UWorld::SpawnActor<AActor>()`
- `IOnlineSubsystem` (不直接用,但 NULL Subsystem 下 Server/Client 判斷依靠 `HasAuthority()`)

**涉及 FrameCore API:** 無(此 task 只做 placement bookkeeping,不觸 FrameCore)

**詳細步驟:**
1. 在 ArchSim game module 下新建 `UArchSimPrefabSpawnSubsystem : public UGameInstanceSubsystem`
2. 成員:
   - `TMap<FIntVector, TWeakObjectPtr<AActor>> OccupancyGrid`(僅 Server 有效)
   - `float GridSizeUU = 50.0f`(500mm,單位 UE cm;可從 ProjectSettings DataAsset 讀)
   - `FBox PlacementBounds`(從 A3 世界系統讀取;預設 5000cm 範圍)
3. 方法:
   - `FIntVector WorldToGrid(FVector WorldPos)`:除以 GridSizeUU + FMath::RoundToInt
   - `bool IsGridOccupied(FIntVector GridPos)`:查 OccupancyGrid TMap
   - `bool TryOccupyGrid(FIntVector GridPos, AActor* Actor)`:原子性插入(若已有則失敗)
   - `void ReleaseGrid(FIntVector GridPos)`:移除並驗證 Actor 一致
4. 新建 `UArchSimPlacementSettings : public UDeveloperSettings`,定義 `GridSizeUU`、`PlacementBounds`、`MaxMembersPerGroup`

**完成標準:**
- [ ] PIE 中透過 `GetGameInstance()->GetSubsystem<UArchSimPrefabSpawnSubsystem>()` 可取得實例
- [ ] 同一 FIntVector 呼叫兩次 TryOccupyGrid,第二次回傳 false
- [ ] TWeakObjectPtr 在 Actor 被 Destroy 後 IsValid() 回傳 false(防 GC dangling)

**預期踩雷:**
- `TMap<FIntVector, ...>` 需要 `FIntVector` 的 `GetTypeHash` 重載;UE5 已內建,但若用自訂結構體需自行寫
- Server-only 狀態在 ListenServer 的 Host Client 同時有效;需確認 `HasAuthority()` 邏輯不被 Client 側誤呼叫

---

#### B1-T2: Server_RequestPlacement RPC + 衝突仲裁 (MVP)

**工時估算:** 3h

**涉及 UE5 Class / API:**
- `UFUNCTION(Server, Reliable)` 宣告
- `AArchSimPlayerController`(或 `AArchSimCharacter`)新增 RPC
- `UArchSimPrefabSpawnSubsystem::TryOccupyGrid`
- `UWorld::SpawnActorDeferred<>` + `FinishSpawning`(允許設 Component 前 Spawn)
- `UFUNCTION(Client, Reliable)` 回傳放置結果(成功/失敗+理由)

**涉及 FrameCore API:** 間接透過 `UArchSimModelRegistry::RegisterMember` 觸發;非本 task

**詳細步驟:**
1. 在 `AArchSimCharacter` 加 `Server_RequestPlacement(FVector ClientWorldPos, FRotator Rot, FName PrefabId)` RPC
2. Server 執行:
   - `GridPos = Subsystem->WorldToGrid(ClientWorldPos)`
   - `if (!Subsystem->TryOccupyGrid(GridPos, nullptr))` → 呼叫 `Client_PlacementFailed(GridPos)` RPC 回傳
   - Spawn Actor:`UWorld::SpawnActor<AArchSimStructureActor>(PrefabClass, Transform)`
   - `Subsystem->TryOccupyGrid(GridPos, SpawnedActor)`(更新真實 Actor 指標)
   - 廣播給所有 Client:新 Actor 透過 UE Replication 自動複製
3. Client 端:`Client_PlacementFailed` 收到時,呼叫 `GhostPreviewComponent->ShowPlacementFailed()`

**完成標準:**
- [ ] 兩個 Client 同時送 RPC 欲放同一格,Server 只有一個 Spawn 成功,另一個收到 PlacementFailed 回調
- [ ] Host Client 放置 RTT ≈ 0;Remote Client RTT 50-100ms;兩者視覺效果一致(Host 也走 RPC 路徑)
- [ ] Spawn 的 Actor 有正確的 `UPROPERTY(Replicated)` Transform,其他 Client 在 <200ms 內看到

**預期踩雷:**
- Host 玩家若不走 RPC(直接本地執行)會造成本地/遠端狀態不一致;強制 Host 也走 `Server_RequestPlacement`
- SpawnActorDeferred 如果沒有 FinishSpawning,Actor 的 BeginPlay 不會執行;BeginPlay 裡才有 RegisterMember

---

#### B1-T3: Client Ghost 預覽(半透明 + 格線 Snap) (MVP)

**工時估算:** 4h

**涉及 UE5 Class / API:**
- `UMaterialInstanceDynamic::Create(BaseMaterial, Outer)`
- `SetScalarParameterValue("Opacity", 0.5f)`
- `SetVectorParameterValue("TintColor", ...)` — 綠 `#009E73` 或 紅 `#D55E00`
- `AStaticMeshActor` 作為 Ghost Actor(Client-only,不 Replicate)
- `FHitResult` + `GetWorld()->LineTraceSingleByChannel` 地面射線
- Enhanced Input `IA_PlaceMember` 的 `ETriggerEvent::Ongoing`(每幀更新 Ghost 位置)

**涉及 FrameCore API:** 無(Ghost 純視覺)

**詳細步驟:**
1. 新建 `UArchSimGhostPreviewComponent : public UActorComponent`,掛在 `AArchSimCharacter`
2. 每幀 Ongoing:
   - 從 Camera 發射射線到地面(Channel = Placement)
   - 取得 HitLocation,呼叫 `Subsystem->WorldToGrid(HitLocation)` → 轉回世界座標(Snap)
   - 移動 Ghost Actor 到 SnappedPos + 套 CurrentRotation
   - `bool bOccupied = Subsystem->IsGridOccupied(GridPos)`
   - 呼叫 `GhostMaterial->SetVectorParameterValue("TintColor", bOccupied ? RedColor : GreenColor)`
3. Ghost Actor 僅在 BuildingMode 啟動時 Spawn;切換回行走模式時 Destroy
4. Ghost 材質 Base:新建 `M_GhostBase`(Translucent 混合模式,兩個 parameter:`Opacity` float + `TintColor` Vector3)

**完成標準:**
- [ ] Ghost 跟隨滑鼠,幀率 ≥60fps,視覺延遲 <16ms
- [ ] Grid Snap 正確:Ghost 總是對齊整數格線
- [ ] 格子空時綠色(`#009E73`),格子占用時紅色(`#D55E00`)
- [ ] 切換 MappingContext 回行走模式後 Ghost Actor 正確 Destroy(無殘留)

**預期踩雷:**
- `Translucent` 混合模式預設不寫 Depth Buffer,Ghost 在其他 mesh 後面時會被遮住但看起來透明正確;需確認 Depth Test 設定正確
- 射線只打 Landscape/地板,不打其他 Ghost Actor(需設 Channel)

---

#### B1-T4: Prefab 庫 DataAsset — 5 大類 × 3 變體 (MVP)

**工時估算:** 4h

**涉及 UE5 Class / API:**
- Prefabricator plugin:`UPrefabricatorAsset`(需驗證 Runtime 可用性)
- `UDataAsset` 或 `UPrimaryDataAsset` 自訂 `UArchSimPrefabEntry`
- `UArchSimPrefabLibrary` BlueprintFunctionLibrary — 提供 `GetAllPrefabs()` / `GetPrefabByName(FName)` 介面
- `UStaticMesh` reference 作為簡單 Prefab(若 Prefabricator Runtime 不可用的降級)

**涉及 FrameCore API:**
- 每個 Prefab Entry 包含:`FFrameMaterial` + `FFrameSection` 預設值(從 `UFrameMaterialLibrary` / `UFrameSectionLibrary` 讀取)
- `FFrameMember` 的 `RefVec` 預設值(柱: `(0,0,1)`,梁: `(0,1,0)`)

**Prefab 列表(15 個 MVP 必須):**
- 柱:`Column_H150x150`(S235)、`Column_H200x200`(S235)、`Column_RC300x300`(C30+REBAR)
- 梁:`Beam_R200x400`(S235)、`Beam_R300x600`(S235)、`Beam_RC250x500`(C30+REBAR)
- 板:`Shell_Floor_150mm`(C30)、`Shell_Floor_200mm`(C30)、`Shell_Floor_Composite`(C30)
- 牆:`Shell_Wall_200mm`(C30)、`Shell_Wall_250mm`(C30)、`Shell_Wall_Brick`(自訂磚)
- 斜撐:`Brace_L80x80x6`(S235)、`Brace_HSS100x100x5`(S235)、`Brace_Double_Angle`(S235)

**完成標準:**
- [ ] `GetAllPrefabs()` 回傳 15+ 個 Entry,各有名稱 / 材料 / 截面 / StaticMesh 參考
- [ ] Prefab DataAsset 在 Content Browser 可用 Filter "ArchSim" 篩選到
- [ ] Prefabricator .uplugin EngineVersion 已手動修為 `5.7`(或確認 5.7 相容)

**預期踩雷:**
- Prefabricator 的核心功能(Prefab Asset 存取/Spawn)是否在 Runtime 打包後仍可用需實測;若不可用,降級方案為純 `UPrimaryDataAsset` + `UStaticMesh` SpawnActor
- 需確認 Prefabricator 的授權(MIT)在商業打包時是否有限制

---

#### B1-T5: R 鍵旋轉 90° + 確認放置 + FrameCore 重解觸發 (MVP)

**工時估算:** 3h

**涉及 UE5 Class / API:**
- Enhanced Input `IA_RotatePlacement`(R 鍵, Tap Trigger)
- `CurrentRotationIndex * 90.0f` 旋轉計算
- `FRotator::Normalize()` 在 RPC payload 前呼叫
- `UArchSimModelRegistry::RegisterMember(Actor, MaterialId, SectionId)` → 取得 MemberIdx
- `UFrameInteractiveSubsystem::ApplyPatchAndResolve(Patch, OutResult)` — Patch 內 `ReactivateMemberIds=[新MemberIdx]`

**涉及 FrameCore API:**
- `UFrameInteractiveSubsystem::ApplyPatchAndResolve`
- `FFrameModelPatch` struct(填入 `ReactivateMemberIds`)
- `FFrameSolveResult` 的 `bSingular` 檢查(若 true,不更新熱圖,顯示「結構不穩定」提示)

**詳細步驟:**
1. Enhanced Input Action `IA_RotatePlacement` 綁 R 鍵,Tap Trigger
2. GhostPreviewComponent 收到 RotatePlacement 事件:`CurrentRotationIndex = (CurrentRotationIndex + 1) % 4`
3. 更新 Ghost Actor Transform(僅 Yaw 旋轉 `CurrentRotationIndex * 90.0f`)
4. 左鍵 IA_PlaceMember 確認時:`Server_RequestPlacement(SnapPos, FRotator(0, CurrentRotIndex * 90.f, 0).Normalize(), PrefabId)`
5. Server Spawn 後:`UArchSimModelRegistry::RegisterMember(SpawnedActor, Mat, Sec)` → 得 MemberIdx
6. 填入 `FFrameModelPatch{ReactivateMemberIds={MemberIdx}}` → 呼叫 `ApplyPatchAndResolve`
7. 若 `OutResult.bSingular`:`UArchSimHUDSubsystem::ShowWarning("結構目前不穩定,請增加支撐")`

**完成標準:**
- [ ] R 鍵每按一次旋轉 90°;第 4 次後回到 0°
- [ ] 放置後 FrameCore Solve 在 200ms 內完成(參考 CLAUDE.md:10K DOF 目標 ≤200ms)
- [ ] `OutResult.bSingular=true` 時有明確 HUD 提示,但構件仍保留在場景中(失敗即學習原則)
- [ ] Ghost 旋轉後放置實體旋轉一致(不出現「預覽橫的,放下變直的」)

**預期踩雷:**
- `FFrameModelPatch` 的 `ReactivateMemberIds` 必須是 FrameCore 內部 index,非 UE Actor handle;`RegisterMember` 負責維護這個 index
- Woodbury rank 累積:若玩家快速連續放置(連發),需在 Registry 層加 150ms debounce + rank 超過 96 時自動呼叫 `Rebaseline()`

---

#### B1-T6: 構件刪除路徑 (MVP)

**工時估算:** 2h

**涉及 UE5 Class / API:**
- Enhanced Input `IA_RemoveMember`(右鍵 or Del 鍵)
- `GetWorld()->LineTraceSingleByObjectType` 射線拾取 `ECC_WorldDynamic` 構件 Actor
- `UArchSimPrefabSpawnSubsystem::ReleaseGrid(GridPos)`
- `AActor::Destroy()`

**涉及 FrameCore API:**
- `FFrameModelPatch{DeactivateMemberIds={MemberIdx}}`
- `UFrameInteractiveSubsystem::ApplyPatchAndResolve`

**完成標準:**
- [ ] 右鍵點選構件後 Server 端 Destroy + OccupancyGrid 釋放 + FrameCore deactivate
- [ ] 刪除後熱圖在 200ms 內更新
- [ ] 刪除一個支撐構件導致機構時,HUD 顯示「結構不穩定」警告

**預期踩雷:**
- 多人場景下刪除別人的構件:需在 Server 端驗證請求者的 ownership 或允許任意刪除(MVP 教育場景建議允許任意刪,Phase 2 加 ownership check)

---

#### B1-T7: 構件 HUD 左側面板 UMG Widget (Phase 2)

**工時估算:** 4h

**涉及 UE5 Class / API:**
- `UUserWidget` 子類 `UArchSimMemberPaletteWidget`
- `UScrollBox` + `UButton` + `UImage`(縮圖)
- `UArchSimPrefabLibrary::GetAllPrefabs()` 綁定
- `FSlateIcon` / `UTexture2D` 縮圖資源

**涉及 FrameCore API:** 無直接接合;透過選擇 Prefab 決定後續 RegisterMember 時傳入的 MaterialId/SectionId

**完成標準:**
- [ ] 滾輪切換大類別(柱/梁/板/牆/斜撐)
- [ ] 點選具體型號後 GhostPreviewComponent 更新目前選取 PrefabId
- [ ] 搜尋框輸入 "RC" 後只顯示 RC 類構件

---

**B1 小計工時:3 + 3 + 4 + 4 + 3 + 2 + 4 = 23h(Phase 2 4h 含在內;MVP 19h)**

---

## B2 2D↔3D 即時雙向工作區

### 設計目標

教育功能核心:平面圖、立面圖、透視圖在同一畫面即時同步,對應課綱製圖科目「投影法」「三視圖」。這是技術難度最高的子節(主計畫書難度 7/10,可行性「中」)。

### 依賴前置

- **B1 完成**:有結構構件 Actor 存在才能被 SceneCapture 拍到
- **A2 完成**:`AArchSimCharacter` 的 Enhanced Input 提供 Tab 鍵 MappingContext 切換

### Sub-task 清單

---

#### B2-T1: UFastDesignWorkspaceWidget 四象限 UMG 骨架 (MVP)

**工時估算:** 3h

**涉及 UE5 Class / API:**
- `UUserWidget` 子類 `UFastDesignWorkspaceWidget`
- `UGridPanel` 2×2 分割
- `UImage` ×3(正交視圖 RenderTarget 貼圖) + `SViewport` ×1(3D 透視)
- `UWidgetComponent` 或 HUD 層疊方式呈現
- `Tab` 鍵 Enhanced Input Action `IA_ToggleWorkspaceMode`

**完成標準:**
- [ ] 1920×1080 螢幕下四象限無重疊,3D viewport 占右下 1/4
- [ ] Tab 鍵切換三模式(四象限 / 純 3D / 純 2D),切換動畫 ≤200ms
- [ ] 三個正交圖區域明確標示「俯視(Top)」「正視(Front)」「側視(Side)」

**預期踩雷:**
- `SViewport` 在 UMG 中嵌入有已知複雜度(UE5 的 `SViewport` 是 Slate 層,UMG 是它的上層封裝);降級方案:3D viewport 直接佔主 Viewport,2D 圖以 Overlay 方式貼在角落

---

#### B2-T2: 三個正交 USceneCaptureComponent2D (MVP)

**工時估算:** 4h

**涉及 UE5 Class / API:**
- `USceneCaptureComponent2D`(×3 俯/正/側)
- `UTextureRenderTarget2D` (×3,解析度建議 512×512)
- `FMinimalViewInfo.ProjectionMode = ECameraProjectionMode::Orthographic`
- `FMinimalViewInfo.OrthoWidth` — 根據場景範圍自動計算
- `bCaptureEveryFrame = false`(手動 trigger,只在構件變動時 capture)
- `CaptureScene()` 手動呼叫

**詳細步驟:**
1. 在場景中放置 3 個「正交相機 Actor」(Top / Front / Side)
2. Top Camera:位置 (0, 0, 5000),朝下(-Z),OrthoWidth = PlacementBounds 寬
3. Front Camera:位置 (0, -5000, 0),朝 +Y,OrthoWidth = PlacementBounds 高
4. Side Camera:位置 (-5000, 0, 0),朝 +X,OrthoWidth = PlacementBounds 高
5. 每次 `UArchSimModelRegistry` 觸發 `OnModelChanged` delegate → 三個 Camera 呼叫 `CaptureScene()`
6. RenderTarget 綁到 `UImage` Widget 顯示

**完成標準:**
- [ ] 放置一根柱子後,俯視圖出現方點,正視圖出現垂直線
- [ ] 正交投影無透視變形
- [ ] 三個 RenderTarget 在構件變動後 1 frame 內更新(bCaptureEveryFrame=false + 手動 CaptureScene)
- [ ] GPU 負擔:正交 Capture 降低到 15fps(非每幀;滿足「識讀不需要 60fps」要求)

**預期踩雷:**
- `USceneCaptureComponent2D` 預設包含場景所有 Actor(含 Ghost Preview);需設 `ShowOnlyComponents` 或 `HiddenActors` 把 Ghost 排除
- 正交 LOD:需對場景中所有構件 Actor 設定 `LODGroup = SmallProp` 或手動強制 LOD0(否則正交視圖角度的 LOD 計算錯誤導致消失)

---

#### B2-T3: USelectionState 廣播層 (MVP)

**工時估算:** 3h

**涉及 UE5 Class / API:**
- 新建 `UArchSimSelectionSubsystem : public UGameInstanceSubsystem`
- `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMemberSelectedDelegate, int32, MemberIdx)`
- `FOnMemberSelectedDelegate OnMemberSelected`
- `FOnMemberSelectedDelegate OnMemberHovered`
- `AFrameUtilizationHeatmapActor` 的高亮 API(需驗證是否有 `HighlightMember(int32 MemberIdx)` 方法;若無,**需在 FrameCoreUE consumer 層新增 helper**,不動 engine)

**詳細步驟:**
1. `UArchSimSelectionSubsystem` 維護 `TSet<int32> SelectedMemberIndices`
2. `Select(int32 MemberIdx)`:更新 Set,廣播 `OnMemberSelected`
3. 2D 視圖的滑鼠點擊 → 射線找到螢幕座標對應的結構線段 → 透過 `UArchSimModelRegistry::GetMemberByActorOrHit(...)` 找到 MemberIdx → 呼叫 `SelectionSubsystem->Select(MemberIdx)`
4. 3D Viewport 收到 `OnMemberSelected` → 對對應 Actor 設 `CustomDepthStencil=1`(後處理 Outline)
5. `AFrameUtilizationHeatmapActor` 收到 → 高亮對應 Member(若無現成 API,在 FrameCoreUE consumer 層新增 `UArchSimHeatmapHighlightHelper`)

**完成標準:**
- [ ] 點選 2D 正視圖的一條線 → 3D 中對應構件出現藍色 Outline(`#56B4E9 Okabe-Ito Sky Blue`)
- [ ] 旋轉 3D 相機後選取狀態保持正確
- [ ] 點選 3D 中的構件 → 2D 視圖中對應線段高亮

**預期踩雷:**
- 2D 視圖的射線不能用 3D Hit test;需用「Screen Pixel → RenderTarget UV → 世界座標」反推
- `AFrameUtilizationHeatmapActor` 若無 `HighlightMember` 公開 API,需在 FrameCoreUE 中加 wrapper;不可動 FrameCore engine source

---

#### B2-T4: Tab 鍵三模式切換 (Phase 2)

**工時估算:** 2h

**涉及 UE5 Class / API:**
- Enhanced Input `IA_ToggleWorkspaceMode`
- `EWorkspaceMode` enum:Quadrant / Pure3D / Pure2D
- `UWidgetAnimation` 切換動畫(淡出/淡入,200ms)

**完成標準:**
- [ ] Tab 鍵循環三模式
- [ ] 切換時 FrameCore Solve 不中斷(只是視圖變化)
- [ ] 純 2D 模式下鍵盤 D/S/T 快捷可進入 B3 標註模式

---

#### B2-T5: 切面工具 (Phase 2)

**工時估算:** 4h

**涉及 UE5 Class / API:**
- 新建 `UArchSimSectionPlane : public AActor`
- `FPlane` 數學型別
- USceneCaptureComponent2D 的 `ClipPlanes` 功能(需驗證;若不可用需自製 Clip Shader)
- 自訂 MaterialParameterCollection 傳切面參數給 Material

**完成標準:**
- [ ] 玩家拖拉定義切面後,對應象限顯示剖面圖
- [ ] 切面法向量強制 `|n| > ε = 1cm`(防 NaN)
- [ ] 剖面顯示構件斷面形狀(矩形/圓形截面輪廓)

**預期踩雷:**
- UE5 SceneCapture 的 ClipPlane 支援需驗證;可能需要 Custom Depth 配合後處理 Shader 實現
- 切面在 0 平面附近的 NaN 問題:最小距離 `PlaneOffset > 5cm`

---

#### B2-T6: 隱藏線/虛線渲染 (Phase 3)

**工時估算:** 8h

說明:完整的隱藏線計算需要對每個構件做 visibility 測試(O(n²)),技術難度高。建議 Phase 3 實作。MVP 簡化為「不畫隱藏線」。

**涉及技術:**需自訂 PostProcess Material 或離屏渲染 + CPU 回讀 Z-buffer。

---

**B2 小計工時:3 + 4 + 3 + 2 + 4 + 8 = 24h(Phase 3 的 8h 不計入 MVP/Phase 2;有效工時 16h)**

---

## B3 圖學標註系統

### 設計目標

提供與 CNS 製圖規範對齊的 5 種標註類型(尺寸/剖面/文字/軸線/標題),讓學生繪圖作品可接業界 DXF 流程。

### 依賴前置

- **B2 T1+T2 完成**:正交視圖 Widget 就緒才能在上面疊加標註

### Sub-task 清單

---

#### B3-T1: AArchSimAnnotation 基類 + 5 子類 Actor 骨架 (Phase 2)

**工時估算:** 3h

**涉及 UE5 Class / API:**
- `AActor` 基類 `AArchSimAnnotation`
- 子類:`AArchSimDimensionAnnotation` / `AArchSimSectionMarkAnnotation` / `AArchSimTextLabelAnnotation` / `AArchSimAxisLineAnnotation` / `AArchSimTitleBlockAnnotation`
- `UProceduralMeshComponent` 繪製尺寸線幾何(或直接用 `DrawDebugLine` MVP 降級版)
- `UTextRenderComponent` 或 Widget in World(3D 文字標籤)
- `UPROPERTY(SaveGame)` 標記所有標註參數(確保 SPUD 序列化)

**完成標準:**
- [ ] 5 種 Actor 可在 Editor 中 Place(作為驗證)
- [ ] 各 Actor 有 SaveGame 標記的核心 UPROPERTY
- [ ] Actor 刪除後不留 dangling reference

---

#### B3-T2: 2D 視圖滑鼠接管 + 尺寸標註 (Phase 2)

**工時估算:** 4h

**涉及 UE5 Class / API:**
- `UUserWidget::NativeOnMouseButtonDown/NativeOnMouseMove` 覆寫
- 「2D 視圖 UV 座標 → 世界座標」反推(需驗證;依 B2-T2 的 OrthoWidth / RenderTarget 解析度計算)
- `AArchSimDimensionAnnotation::SetEndpoints(FVector A, FVector B)` → 自動計算距離 + 畫線

**完成標準:**
- [ ] 在俯視圖按 D → 點兩個點 → 生成尺寸線 + 正確長度標註(mm)
- [ ] 標註長度誤差 <1mm(像素精度誤差)
- [ ] 點選已有尺寸標註可選取/移動/刪除

---

#### B3-T3: 軸線自動編號邏輯 (Phase 2)

**工時估算:** 3h

**涉及 UE5 Class / API:**
- `UArchSimAxisLineSubsystem : public UWorldSubsystem`
- `TArray<AArchSimAxisLineAnnotation*>` 按位置排序
- 自動編號:水平向 A/B/C…,垂直向 1/2/3…
- 插入/刪除後 `RegenerateAxisNumbers()` 全部重算

**完成標準:**
- [ ] 放 3 條水平軸線,自動標 A/B/C
- [ ] 刪除 B 後重新標為 A/B
- [ ] 多人模式下軸線 ID 由 Server 統一管理(防止兩人同時加軸線時編號重複)

---

#### B3-T4: 標題塊 DataAsset + UMG 編輯介面 (Phase 2)

**工時估算:** 2h

**涉及 UE5 Class / API:**
- `UArchSimTitleBlockSettings : public UPrimaryDataAsset`
- 欄位:學生姓名 / 日期 / 關卡名稱 / 比例尺(自動填入 + 可覆寫)
- `UUserWidget` 簡單表單 Widget(文字輸入框 × 4)

**完成標準:**
- [ ] 標題塊在右下角自動顯示預設值
- [ ] 玩家可在 UI 中修改姓名欄位並立即反映在圖面

---

#### B3-T5: 比例尺自動偵測 (Phase 2)

**工時估算:** 2h

**詳細步驟:**
1. 取得當前正交視圖 `OrthoWidth`(世界單位 cm)和 Widget 像素寬
2. `RealScale = OrthoWidth / WidgetPixelWidth`(每像素代表多少 cm)
3. 映射到最近標準比例:`{1:20, 1:50, 1:100, 1:200, 1:500}` 中找最接近值
4. 顯示在標題塊「比例:1:100」

**完成標準:**
- [ ] 1:100 顯示時,1m 真實長度對應螢幕 10mm ± 2%

---

#### B3-T6 + T7: DXF 匯出 + Layer 控制 (Phase 3)

**工時估算:** 6h

需接 MctoNurbs sidecar 的 DXF writer;Phase 3 實作。

---

**B3 小計工時:3 + 4 + 3 + 2 + 2 + 6 = 20h(Phase 3 的 6h 不計入;有效工時 14h)**

---

## B4 藍圖系統

### 設計目標

玩家可儲存「子結構」為可重用藍圖(Blueprint),日後批次放置、旋轉、鏡像、分享。對應 Block Reality 的 Litematica ghost 理念,在 UE5 端用 Prefabricator + 自訂 JSON schema 實作。

### 依賴前置

- **B1 全部 MVP task 完成**:Actor Spawn / ModelRegistry 機制就緒(B4 的批次放置是 B1 的「多構件版本」)
- **B2 T1 完成**:用於藍圖的截圖 SceneCapture

### Sub-task 清單

---

#### B4-T1: 藍圖儲存路徑 — 框選 + JSON 序列化 (MVP)

**工時估算:** 4h

**涉及 UE5 Class / API:**
- `GetWorld()->GetGameState()` 取得場景中所有構件 Actor
- 框選用 `FBox` 篩選 Actor 位置:`Actor->GetActorLocation()` 在 SelectionBox 內
- `UArchSimBlueprintAsset : public UPrimaryDataAsset`(儲存藍圖資料)
- `FJsonObject` + `FJsonObjectConverter::UStructToJsonObject` 序列化 `FFrameModelDef` 子集
- JSON schema:`{ "version": "1.0", "name": "...", "members": [...], "nodes": [...], "thumbnail_base64": "..." }`

**涉及 FrameCore API:**
- 讀取 `UArchSimMemberData::MemberIdx` 找到對應 `FFrameModelDef` 中的 Member 資料
- 框選範圍內的 Nodes/Members/Shells 子集提取

**完成標準:**
- [ ] 框選 10 個構件,呼叫「儲存為藍圖」,JSON 正確生成且可以被 `FJsonObject::FromString` 解析
- [ ] 藍圖 JSON 包含所有選取構件的 Material / Section / 位置(相對於選取中心點的 offset)
- [ ] `version` 欄位存在並為 `"1.0"`

**預期踩雷:**
- 框選的構件 Nodes 可能跨越藍圖邊界;需將 Nodes 座標轉為「藍圖局部座標系」(以 BBox 中心為原點)才能在不同位置重用

---

#### B4-T2: 縮圖截圖 USceneCaptureComponent2D (Phase 2)

**工時估算:** 2h

**涉及 UE5 Class / API:**
- `USceneCaptureComponent2D` 臨時 Actor(截圖後立即 Destroy)
- `UTextureRenderTarget2D` 256×256
- `FImageUtils::ExportRenderTarget2DAsHDR` 或 PNG 轉為 `TArray<uint8>`
- `FBase64::Encode` → 存入 JSON `thumbnail_base64`

**完成標準:**
- [ ] 儲存藍圖後縮圖顯示在藍圖庫 Widget 中
- [ ] 縮圖為俯視 45° 角,可清楚辨識構件形狀

---

#### B4-T3: 藍圖庫 UMG Widget (Phase 2)

**工時估算:** 3h

**涉及 UE5 Class / API:**
- `UUserWidget` 子類 `UBlueprintLibraryWidget`
- `UUniformGridPanel` + `UButton` + `UImage`(縮圖) + `UTextBlock`(名稱 + 構件數)
- 預載 20 個範例藍圖 DataAsset(存在 Content/Blueprints/Presets/ 資料夾)
- Drag & Drop:`UDragDropOperation` 子類 + `OnDrop` 事件觸發放置預覽

**完成標準:**
- [ ] HUD 右側面板顯示 20+ 藍圖縮圖 + 名稱
- [ ] 拖拉藍圖到場景開始顯示 Ghost 預覽
- [ ] Hover 顯示「X 個構件」tooltip

---

#### B4-T4: Ghost 預覽旋轉 + 鏡像 + 節點重映射 (Phase 2)

**工時估算:** 4h

**涉及 UE5 Class / API:**
- 與 B1-T3 的 `UArchSimGhostPreviewComponent` 共用,但需支援「多構件 Ghost」
- R 鍵:旋轉整個 Ghost 群組(以群組 BBox 中心為旋轉軸)
- F 鍵:`FMath::Mirror(Vector, MirrorAxis)` 鏡像翻轉所有 offset
- 節點重映射:`TMap<int32, int32> OldNodeToNewNode` 確保鏡像後 Member 的 i/j 端節點連接正確

**涉及 FrameCore API:**
- `FFrameMember::RefVec` 旋轉/鏡像後需重新計算(純 UE consumer 層計算,不動 engine)
- 鏡像純對稱截面(Rectangular/Circular)不需要 Iy/Iz 對換

**完成標準:**
- [ ] R 鍵旋轉 90° 後所有 Ghost 構件一起轉動
- [ ] F 鍵鏡像後左右 offset 正確(不出現重疊或空洞)
- [ ] 確認放置後,所有構件的 MemberIdx 和 NodeIdx 正確分配(無衝突)

**預期踩雷:**
- 批次放置時 OccupancyGrid 可能出現部分格子可用、部分不可用的情況;需原子性檢查所有格子後再 Spawn 或整體失敗

---

#### B4-T5: .archsim_bp 匯出/匯入 + Schema Version (Phase 2)

**工時估算:** 3h

**涉及 UE5 Class / API:**
- `IFileManager::Get().CreateFileWriter(FilePath)` 寫 `.archsim_bp` 檔
- `FPlatformFileManager::Get().GetPlatformFile().OpenRead` 讀取
- `FJsonSerializer::Deserialize` 解析 JSON
- Schema migration:`if (json["version"] == "1.0") { ... }` 版本轉換邏輯

**完成標準:**
- [ ] 匯出的 `.archsim_bp` 可在另一台 UE5.7 機器上匯入
- [ ] 舊版本(若未來有 v1.1)匯入時有 migration 提示
- [ ] 縮圖 base64 正確還原為 Texture2D

---

#### B4-T6: 藍圖分享(教師廣播端) (Phase 3)

**工時估算:** 4h

Phase 3 功能,老師端廣播藍圖到學生。需要多人連線基礎(Part G)先完成。

---

**B4 小計工時:4 + 2 + 3 + 4 + 3 + 4 = 20h(Phase 3 的 4h 不計入;有效工時 16h)**

---

## 跨子節整合注意事項

### FrameCore FROZEN 邊界確認

以下是 Part B 所有 task 中與 FrameCore 的接觸點及 FROZEN 邊界確認:

| Task | 使用的 FrameCore API | 是否需要新增 Consumer Layer Helper |
|------|---------------------|--------------------------------------|
| B1-T5 | `ApplyPatchAndResolve`, `FFrameModelPatch` | 不需要;API 已存在 |
| B1-T6 | `FFrameModelPatch.DeactivateMemberIds` | 不需要;API 已存在 |
| B2-T3 | `AFrameUtilizationHeatmapActor` 高亮 API | **需驗證**;若無 `HighlightMember` 需在 FrameCoreUE consumer 層新增 `UArchSimHeatmapHighlightHelper` |
| B4-T1 | 讀取 `FFrameModelDef` 子集 | 不需要;只是資料讀取 |
| B4-T4 | `FFrameMember::RefVec` 重算 | Consumer 層計算,不動 engine |

**鐵則確認:Part B 所有 Task 均不修改 `Plugins/FrameSolver/Source/FrameCore/` 下任何檔案。**

### 測試策略

- **B1 自動化測試**:在 CI 中加 UE automation test `ArchSim.B1.PlacementConflict`:兩個 RPC 同時觸發,驗證只有一個 Spawn 成功
- **B2 人工目視測試**:正交視圖與 3D 視圖同步是純視覺功能,難以自動化;建議 QA checklist
- **B4 JSON round-trip 測試**:儲存藍圖 → 匯出 JSON → 重新載入 → 驗證構件數量/位置一致

### 效能目標總結

| 指標 | 目標 | 測試方法 |
|------|------|---------|
| Ghost 預覽幀率 | ≥60fps | PIE FPS counter |
| 放置端到端延遲 | ≤200ms | 從 Left Click 到 HeatMap 更新的時間戳 |
| 正交視圖更新 | ≤1 frame 後觸發 CaptureScene | bCaptureEveryFrame=false 手動驗證 |
| 藍圖批次載入(100 構件) | ≤3s(有載入動畫) | Profiler Timer |
| 多人衝突仲裁 | 只有 1 個 Spawn | 2 Client 同時送 RPC 驗證 |


---


# Part C 實作擴充(112h)

**標題**:Part C — 工程模擬層

**子節數**:4

### MVP 必須(Phase 1)
- C1-1 載重 USTRUCT 橋接 helper
- C1-2 節點集中力 UI 與施加
- C1-3 UDL 均佈線載重施加
- C1-4 自重自動計算
- C1-5 載重視覺化箭頭 Actor
- C1-6 載重組合 LoadCombineEnvelope 接合
- C2-1 UArchSimModelRegistry 重解 debounce 管道
- C2-2 AFrameUtilizationHeatmapActor 整合
- C2-3 D/C 色彩曲線 CurveAtlas 設定
- C2-4 HUD 警告文字最危構件
- C3-1 崩塌觸發條件判斷
- C3-2 SolveDynCollapse Async Task 封裝
- C3-3 AFrameDynCollapseReplayActor 播放整合
- C4-1 UFrameStructureGroupSubsystem Union-Find
- C4-2 BFS 錨定判斷
- C4-3 未錨定視覺化灰色標示

### Phase 2
- C1-7 殼壓 FFrameShellPressure 施加 UI
- C1-8 地震力等效靜力三段滑桿
- C2-5 PerInstanceCustomData GPU 路徑色彩更新
- C2-6 模式切換雙 Buffer crossfade
- C2-7 D/C >> 2.0 極端值 bShowExtremeValue 數值疊加
- C3-4 OnEventReached 音效與 Niagara 粒子綁定
- C3-5 多人倒塌結果 JSON 廣播與 Client 同步
- C4-4 載重路徑 BFS 視覺化粗細寬度映射
- C4-5 漂浮偵測模式全場掃描

### Phase 3
- C1-9 DA_LoadTypeTemplates DataAsset 完整台灣規範庫
- C2-8 位移模式 AFrameDeformedShapeActor 切換整合
- C2-9 應力模式 FFrameStressField 切換整合
- C3-6 Chaos 剛體 LOD 與 30 秒 cleanup 機制
- C4-6 Union-Find 超過 5000 節點 lazy 模式
- C4-7 載重路徑精確力流從 FrameCore 反力倒推

### 實作順序
- C4 連通性與載重路徑(前提:B1 構件放置系統已完成,C4 結果決定 FrameCore prescribed DOF 是否正確)
- C1 載重施加(依賴 C4 錨定確認結構有效,依賴 A1 Registry 提供 MemberIdx)
- C2 即時 D/C 熱圖(依賴 C1 載重與 A1 Registry 的 Solve 管道)
- C3 失敗與崩塌(依賴 C1 載重與 C2 D/C 觸發條件)

### 跨 Part 依賴
- Part A (A1 ArchSimModelRegistry + UArchSimMemberData) 必須完成後才能啟動 C1/C2
- Part B (B1 構件放置系統 + OccupancyGrid) 必須完成後才能啟動 C4
- Part D (D1 掃描儀) 消費 C2 的 CachedUtilization 與 C4 的錨定狀態,C2/C4 先完成才能 D1
- Part E (E1 工序狀態機) 的蜂窩弱點最終透過 FFrameModelPatch 觸發 C1/C2 重解,C1 先完成才能接 E1 材料 patch

### 風險清單
- SolveDynCollapse blocking 呼叫阻塞 Game Thread:必須用 AsyncTask/TFuture 封裝,但 UE5 的 FAsyncTask 在 PIE 模式關閉時有 crash 風險,需實作 cancel token
- AFrameUtilizationHeatmapActor::BuildHeatmap 每次重建 PMC mesh 約 10ms/100構件,200ms 預算壓緊:MVP 接受 PMC 重建,Phase 2 才走 PerInstanceCustomData GPU 路徑
- Union-Find 在多人同時 dirty 時的並發問題:MVP 強制 server-only 仲裁,直到多人 Phase 完成前不可在 Client 端跑 Union-Find
- 載重座標系混淆(局部 vs 全域):FFrameMemberUDL.WLocal 是局部座標,UI 必須明示方向,不然學生施加 UDL 方向反了且看不出來
- Chaos 大量剛體 FPS 崩潰:MaxDebrisActors=1024 上限必須第一天就加,不能延後,否則 QA 測試容易出現百個碎片的壓力測試
- ApplyPatchAndResolve Woodbury rank 累積超過 MaxRank=96 未自動 Rebaseline:Registry debounce 邏輯必須追蹤累積 rank,否則靜默出錯
- BFS 深度爆炸在蜂巢狀大結構:anchor.bfs_max_depth=64 必須寫死為防禦性上限,超過時顯示『結構太複雜』而非 stack overflow crash

## 詳細擴充內容


# Part C — 工程模擬層 詳細實作計畫

> 版本:v1.0 | 日期:2026-06-25
> 本計畫對應主計畫書第五章(Part C:工程模擬層)C1~C4 四個子節。
> 引擎邊界:FrameCore v4.0.0 engine source **永久 FROZEN**,所有新邏輯只在 `Plugins/FrameSolver/Source/FrameCoreUE/` 或 `Source/ArchSim/` 的 UE consumer 層實作。

---

## 通盤假設與約束

| 項目 | 說明 |
|---|---|
| 開發規模 | 1-2 人全職,預算 1-2 小時/task |
| UE 版本 | 5.7(鎖定) |
| FrameCore 版本 | v4.0.0 FROZEN |
| 前置條件 | Part A (A1 UArchSimModelRegistry + UArchSimMemberData) 已完成;Part B (B1 放置系統) 已完成 |
| 驗收環境 | UE5 PIE 單機模式(多人功能留 Phase 2) |
| 不可改的 API | `Plugins/FrameSolver/Source/FrameCore/` 下任何檔案 |

---

## C1 載重施加(節點/UDL/殼壓/自重)

### 概念摘要

讓玩家在已建好的結構上施加各類載重(靜載重 D、活載重 L、地震力 E),並自動產生台灣建築技術規則的組合工況,透過 `LoadCombineEnvelope` 取包絡最大 D/C。自重由系統自動計算,玩家不需手動輸入。

### 先決依賴

- A1:`UArchSimModelRegistry`(提供 MemberIdx、管理 FFrameModelDef)
- A1:`UFrameInteractiveSubsystem`(ApplyPatchAndResolve / Rebaseline)
- C4(C4-1 完成後才知道哪些節點有錨定,避免把載重加到浮空結構上)

---

### C1 Sub-task 清單

#### C1-1:載重 USTRUCT 橋接 helper(UArchSimLoadHelper)
**工時:3 小時**

**說明:**
新建 `UArchSimLoadHelper : public UBlueprintFunctionLibrary`,封裝所有「遊戲世界量 → FrameCore USTRUCT」的轉換邏輯。函式列表:
- `FloatToNodalLoad(NodeIdx, Fz_kgf)` → `FFrameNodalLoad`(把重力方向集中力轉換)
- `AreaLoadToNodalLoads(ShellIdx, P_kgfm2)` → `TArray<FFrameNodalLoad>`(把樓板面積載重分配到周邊節點,使用影響面積法)
- `SpanUDLToMemberUDL(MemberIdx, W_kgfm)` → `FFrameMemberUDL`(沿局部座標 -Z 方向)
- `ComputeSelfWeight(FFrameModelDef)` → `TArray<FFrameNodalLoad>`(遍歷 Members,W = Material.Rho × Section.A × g × Length / 2,分兩端)

**涉及 API:**
- `FFrameNodalLoad` / `FFrameMemberUDL` / `FFrameShellPressure`(FrameCoreUE USTRUCT,已存在)
- `FFrameModelDef`、`FFrameModelDef::Members[]`、`FFrameModelDef::Materials[]`、`FFrameModelDef::Sections[]`

**完成標準:**
- `ComputeSelfWeight` 對標 standalone F68 cantilever fixture:10cm × 20cm RC 梁 L=3m 自重 = 0.1×0.2×2400×9.81×3 = 1413.5 N,兩端各 706.75 N,誤差 < 0.1%
- `AreaLoadToNodalLoads` 對一個 1m×1m 正方形 Shell,400 kgf/m² → 4 個角節點各 100 kgf,誤差 < 0.1%
- BP 可直接呼叫所有函式(BlueprintCallable 標記)

**預期踩雷:**
- `Section.A` 的單位是 m²(FrameCore 內部 SI),`Rho` 是 kg/m³,確認不要混用 cm²
- 影響面積法對不規則形狀 Shell 需要 helper 算幾何重心與三角分割,MVP 只做矩形 Shell(4 等分)

---

#### C1-2:節點集中力 UI 與施加
**工時:4 小時**

**說明:**
實作「點擊節點 → 彈出載重輸入面板 → 確認後送 RPC → Server 更新 FrameCore」的完整流程。

UI 元件:
- UMG `UNodalLoadInputWidget`:顯示 Fx/Fy/Fz/Mx/My/Mz 六個輸入框,預設 Fz = -10 kN(重力方向),其他歸零
- 確認按鈕 → 呼叫 `Server_ApplyNodalLoad(NodeIdx, Comp[])`
- Server 端: `UArchSimModelRegistry::AddNodalLoad(NodeIdx, Comp[])` → 建立 `FFrameModelPatch`(SetNodalLoads) → `ApplyPatchAndResolve`

**涉及 API:**
- `UFrameInteractiveSubsystem::ApplyPatchAndResolve(FFrameModelPatch, FFrameSolveResult&)`
- `FFrameModelPatch`(已存在 v3.5 UE consumer 層)
- Enhanced Input `IA_ApplyLoad`(已在主計畫 B 規格定義,此處只消費)

**完成標準:**
- 點擊一個節點,UI 在 100ms 內彈出
- 輸入 Fz = -50 kN 確認後,`FFrameSolveResult.NodalReactions` 中對應支座反力在 200ms 內更新
- Server log 顯示 `[LoadSystem] NodalLoad applied at Node 5, Fz=-50000N`
- 在 F58 cantilever fixture 的 UE 端重現:懸臂樑自由端加 P=10kN 向下,支座反力 Vy=10kN ± 0.1%

**預期踩雷:**
- `FComp[6]` 對應 `Fx Fy Fz Mx My Mz` 順序,UI 要和 FrameCore 定義完全一致(不然方向反了玩家摸不著頭腦)
- `Mx/My/Mz` 對大多數高中生沒有意義,MVP 可隱藏力矩輸入,只顯示 Fx/Fy/Fz,進階模式才開放

---

#### C1-3:UDL 均佈線載重施加
**工時:3 小時**

**說明:**
「拖拉梁段 → 施加均佈線載重」。玩家選擇一根梁,側邊 UI 出現 UDL 強度滑桿(kN/m),確認後施加。

實作重點:
- `FFrameMemberUDL.WLocal` 是局部座標,**UI 必須明示方向**:預設沿構件 -Y 局部軸(垂直向下的重力效果),UI 顯示方向示意圖
- 視覺化:沿梁長每 0.2m 畫一個小箭頭 Actor(用 `UNiagaraSystem` 或 `UBillboardComponent`,MVP 用 StaticMesh 箭頭陣列)
- 多段 UDL 疊加:同一根梁可施加多個 UDL(累積進 `FFrameModelPatch::SetMemberUDLs` 陣列)

**涉及 API:**
- `FFrameMemberUDL { int32 MemberIdx; FVector WLocal; }`
- `FFrameModelPatch::MemberUDLs`(TArray)

**完成標準:**
- 對簡支梁 L=6m 施加 UDL=20 kN/m,Solve 後支座反力 = 60kN ± 0.1%,最大彎矩(跨中)= wL²/8 = 90 kN·m ± 0.5%
- 視覺化箭頭沿梁長均勻分布,不堆疊
- 修改 UDL 大小後 →熱圖在 200ms 內更新

**預期踩雷:**
- `WLocal` 是局部座標:梁在空間旋轉後重力方向在局部座標中不再是 (0,-1,0),需要在 helper 中把全域重力 (0,0,-9.81) 轉換到構件局部座標系(用 `FFrameMember.LocalAxes`)——這步容易被忽略,必須在 C1-1 helper 測試覆蓋

---

#### C1-4:自重自動計算(系統管理)
**工時:2 小時**

**說明:**
結構每次觸發 Solve 前,`UArchSimModelRegistry` 自動呼叫 `UArchSimLoadHelper::ComputeSelfWeight(ModelDef)` 生成自重 NodalLoad 陣列,並合併進當前工況。玩家看不到自重選項(系統代管),防止雙重計算。

實作細節:
- `UArchSimModelRegistry` 內部有 `TArray<FFrameNodalLoad> SelfWeightLoads`,每次 `Rebaseline()` 或構件改變後重算
- 在 `BuildSolveInput()` 函式中把 SelfWeightLoads 插入 `FFrameModelDef::NodalLoads` 前,玩家施加的 NodalLoads 接在後面
- **防雙重計算**:ModelDef 中的 NodalLoads 分成兩段:index 0~N_sw 是自重(系統),index N_sw+1~ 是玩家施加,patch 只動後半段

**完成標準:**
- 移除一根構件後,下次 Solve 的自重更新正確(比前次少了該構件質量)
- 自重啟用/停用 toggle(給老師設定,預設啟用)
- 在空 model 上自重 = 0 N,無浮點殘留

**預期踩雷:**
- 構件長度從 UE Actor Transform 讀,單位是 cm(UE 預設),要除 100 轉 m 再算 SI

---

#### C1-5:載重視覺化箭頭 Actor
**工時:3 小時**

**說明:**
新建 `AArchSimLoadVisualizer : public AActor`,每幀從 `UArchSimModelRegistry` 讀取當前載重清單並繪製:
- 節點集中力:紅色箭頭,箭頭長度 ∝ log(|F|)(避免力太大時箭頭佔滿整個畫面)
- UDL:沿梁等距 0.2m 一根小箭頭,垂直方向(全域重力方向)
- 殼壓:Shell 面上均勻點陣箭頭

用 `UProceduralMeshComponent` 或 `ULineBatchComponent`(後者更輕量,MVP 優先)動態繪製。

**完成標準:**
- 施加 3 種不同大小節點力,箭頭長度視覺上有明顯差異
- 修改載重後視覺化在 1 幀內更新(不超過 16ms)
- 按 V 鍵切換「顯示/隱藏載重視覺化」

**預期踩雷:**
- `ULineBatchComponent` 每幀 clear + redraw 效能問題:超過 500 根箭頭時幀率下降。MVP 上限設 200 根,超過顯示「載重過多,部分隱藏」

---

#### C1-6:載重組合 LoadCombineEnvelope 接合
**工時:3 小時**

**說明:**
實作「台灣建築技術規則」三種基本載重組合的自動計算,取包絡最大 D/C:

| 工況 | 公式 |
|------|------|
| 靜載重控制 | 1.4D |
| 靜+活 | 1.2D + 1.6L |
| 含地震 | 1.2D + 1.0E + 1.0L |

UE 端流程:
1. `UArchSimLoadRegistry` 分別維護三個 `TArray<FFrameNodalLoad>`:DeadLoads、LiveLoads、SeismicLoads
2. 呼叫 `UFrameAnalysisLibrary::LoadCombineEnvelope(BaseDef, SolveOpts, Cases[3])` 一次回傳包絡 `FFrameSolveResult`
3. 包絡結果裡的 `MemberUtilization[]` 就是最危險工況下的 D/C,分發給所有 Actor

**涉及 API:**
- `UFrameAnalysisLibrary::LoadCombineEnvelope`(已存在 v3.4 FrameCoreUE)
- `FFrameLoadCase { TArray<FFrameNodalLoad>; TArray<FFrameMemberUDL>; float Factor; }`(需確認實際 USTRUCT 名稱,**需驗證**)

**完成標準:**
- 給定 D=10kN/m、L=8kN/m UDL 的簡支梁:包絡工況為 1.2D+1.6L=24.8kN/m,D/C 與直接 SolveLinear(24.8kN/m) 差 < 0.1%
- 包絡計算在 100 構件規模下 <500ms

**預期踩雷:**
- `LoadCombineEnvelope` 的 `Cases[]` USTRUCT 格式需要查原始碼確認(`docs/specs/` 或 FrameCoreUE 頭檔),主計畫書未給出完整 USTRUCT 定義
- 地震載重 E 是等效靜力(EAR = C × W),不是真反應譜,C1-8 才做 UI,此處先留 placeholder `SeismicFactor=0`

---

#### C1-7:殼壓 FFrameShellPressure 施加 UI(Phase 2)
**工時:3 小時**

**說明:**
點擊樓板 Shell → 彈出活載重類型選單(住宅 200 / 辦公 300 / 走廊 400 kgf/m²),選定後換算為 `FFrameShellPressure.P(Pa)` 施加。

**涉及 API:**
- `FFrameShellPressure { int32 ShellIdx; float P; }`(已存在)

**完成標準:**
- 選「辦公室 300 kgf/m²」→ 轉換為 2943 Pa → Shell 承受正確壓力,Solve 後支座反力正確

---

#### C1-8:地震力等效靜力三段滑桿(Phase 2)
**工時:4 小時**

**說明:**
HUD 上地震力滑桿三段:低(0.18g)/中(0.23g)/高(0.33g)(對應台灣地震區)。轉換公式:Seismic EQ = C × W,C = 地震係數 × I(重要性係數=1.0),W = 結構總自重。

**完成標準:**
- 「高地震區」設定後,5 層 RC 框架的地震力 = Cs × W 正確套用為水平節點力
- W-08 地震警告(`MaxDC_Seismic > 1.0`)正確觸發

---

### C1 總工時:25 小時(MVP 18 小時 + Phase 2 7 小時)

---

## C2 即時 D/C 熱圖(每幀互動式重解)

### 概念摘要

玩家每次動作後 150ms debounce,呼叫 `ApplyPatchAndResolve`,結果在 200ms 端到端反映到熱圖。熱圖是整個教學閉環的視覺核心——沒有正確的即時熱圖,教育效果基本為零。

### 先決依賴

- A1:`UArchSimModelRegistry`(Solve 管道,DistributeSolveResult)
- C1-1~C1-4:載重已施加且自重已計算

---

### C2 Sub-task 清單

#### C2-1:UArchSimModelRegistry 重解 debounce 管道
**工時:3 小時**

**說明:**
在 `UArchSimModelRegistry` 加入 debounce 機制:
- 新增 `FTimerHandle DebounceTimer`
- 任何 patch 進來 → 設定 150ms 一次性 timer
- Timer 觸發 → 呼叫 `ExecutePendingSolve()`,把所有 pending patch 合併後執行一次 `ApplyPatchAndResolve`
- patch 累積超過 `MaxRank=96`(Woodbury 上限)→ 立即 `Rebaseline()` 而非 `ApplyPatchAndResolve`

**涉及 API:**
- `UFrameInteractiveSubsystem::ApplyPatchAndResolve` / `Rebaseline` / `StartSession` / `EndSession`
- `GetWorldTimerManager().SetTimer(DebounceTimer, this, &UArchSimModelRegistry::ExecutePendingSolve, 0.15f, false)`

**完成標準:**
- 連續 20 次 patch 在 150ms 內只觸發一次 Solve(用 UE 內建 log 計算呼叫次數驗證)
- Rank 累積到 96 時正確切換 Rebaseline,下次 Solve 仍回傳正確結果
- 200ms 端到端時間(從 patch 進入到 DistributeSolveResult 完成):50 構件模型 ≤ 200ms(90th percentile)

**預期踩雷:**
- Timer 在 PIE Stop 時如果 DebounceTimer 還在倒數會 crash:在 `UArchSimModelRegistry::Deinitialize()` 中先 `ClearTimer(DebounceTimer)`

---

#### C2-2:AFrameUtilizationHeatmapActor 整合
**工時:4 小時**

**說明:**
把已實作的 `AFrameUtilizationHeatmapActor`(v3.5 已有)整合進遊戲世界:
1. `UArchSimModelRegistry::DistributeSolveResult()` 完成後呼叫 `HeatmapActor->BuildHeatmap(OutResult, ModelDef)`
2. HeatmapActor 需要 `TArray<FFrameMemberGeometry>` —— 這個 Geometry 資料從各構件 Actor 的 Transform 建構,新增 helper `UArchSimGeometryBuilder::BuildMemberGeometry(Registry) → TArray<FFrameMemberGeometry>`
3. `bSingular = true`(FrameCore 算出機構)時:跳過 BuildHeatmap,所有構件顯示橘色閃爍警告

**涉及 API:**
- `AFrameUtilizationHeatmapActor::BuildHeatmap(FFrameSolveResult, TArray<FFrameMemberGeometry>)`
- `FFrameMemberGeometry`(v3.4 USTRUCT,含 StartPos/EndPos/CrossSection)
- `FFrameSolveResult::bSingular`

**完成標準:**
- 100 個構件規模,BuildHeatmap 呼叫後 1 幀內(≤16ms)更新完成,或允許下一幀繪製(不 block Game Thread)
- `bSingular=true` 時橘色閃爍警告在 HUD 可見(不 crash)
- 把一根柱子改為更細的截面,D/C 變色在 200ms 內反映

**預期踩雷:**
- `BuildHeatmap` 內部重建 PMC mesh,每個構件 ~0.1ms → 100 構件 = 10ms 在 Game Thread。短期可接受,但 200+ 構件時需移到 `AsyncTask`(Phase 2)
- `FFrameMemberGeometry.EndINodeIdx / EndJNodeIdx` 必須與 `FFrameModelDef.Nodes[]` 索引一致,Registry 需要維護 NodeIdx ↔ UE FVector 的雙向映射

---

#### C2-3:D/C 色彩曲線 CurveAtlas 設定
**工時:2 小時**

**說明:**
在 UE Content 中建立:
- `CurveLinearColor` 資產 `CC_DC_Ratio`:0.0→#009E73(綠), 0.85→#F0E442(黃), 1.0→#E69F00(橘), 1.2→#D55E00(紅), 2.0→#D55E00(飽和)
- `CurveAtlas` 資產 `CA_StructuralColors`:包含 DC_Ratio 曲線 + 未來可擴充應力/位移曲線
- HeatmapActor 的 Material 使用 `CurveAtlasRowParameter` 採樣

**完成標準:**
- Material 預覽:輸入 D/C=0.5 → 顯示綠色、D/C=1.1 → 顯示橘色、D/C=1.5 → 顯示紅色
- Protanopia 色盲模擬測試(使用 Coblis 模擬器截圖驗證 3 種 CVD 下仍可區分)

**預期踩雷:**
- Emissive Color 路徑(不受光照影響)必須用,不能用 BaseColor(在場景光照下顏色不一致)
- CurveAtlas 在 Material 中需要 `CurveAtlasRowParameter` 節點,UE5 的接法與 UE4 不同,需查 UE5.7 文件確認

---

#### C2-4:HUD 警告文字最危構件
**工時:3 小時**

**說明:**
HUD 右上角「警告面板」:
- `governingMemberIdx != -1` 且 `MaxDC > 1.0`:顯示「危險:構件 M-{idx}, D/C={val:.2f}, 模式:{mode}」
- `governingMemberIdx == -1`:隱藏面板
- 危險模式(mode)從 `FFrameMemberUtilization::GoverningMode`(壓縮/彎曲/剪力/扭轉)讀取,需要在 UE consumer 層實作 mode→中文名稱的 mapping

新增 `UArchSimWarningPanel : UUserWidget`,綁定 `UArchSimModelRegistry::OnSolveComplete` 事件。

**涉及 API:**
- `FFrameSolveResult::Utilization.GoverningMemberIdx`(需確認欄位名稱,**需驗證與 FrameCore USTRUCT 一致**)
- `FFrameSolveResult::Utilization.MaxDC`

**完成標準:**
- F58 cantilever fixture 重現:懸臂自由端加 P→D/C > 1.0,警告面板顯示 「危險:構件 M-0, D/C=1.34, 模式:彎曲」(數值對標 standalone oracle)
- `governingMemberIdx=-1`(D/C 全 < 1.0)時面板完全隱藏,不顯示 「無危險」字樣

**預期踩雷:**
- `GoverningMemberIdx` 是 0-based index,-1 是哨兵,不能顯示「M--1」這種字樣(必須有 -1 判斷)
- `GoverningMode` 的型別需確認:是 enum 還是 string?如果是 enum 需要在 UE 端加 TMap 轉中文

---

#### C2-5:PerInstanceCustomData GPU 路徑(Phase 2)
**工時:4 小時**

**說明:**
把 BuildHeatmap 從「每次重建 PMC mesh」改為「只更新 PerInstanceCustomData(浮點 D/C 值),由 Material 讀取並查 CurveAtlas」。理論上每幀只要傳 N 個 float 給 GPU,比重建 mesh 快 10~100 倍。

**涉及 API:**
- `UInstancedStaticMeshComponent::SetCustomDataValue(InstanceIndex, CustomDataIndex, Value)`
- Material 中用 `PerInstanceCustomData` 節點讀取

**完成標準:**
- 200 構件規模下,色彩更新幀時間 < 1ms(vs PMC 重建 ~20ms)

---

#### C2-6:模式切換雙 Buffer crossfade(Phase 2)
**工時:3 小時**

**說明:**
「D/C 模式 / 位移模式 / 應力模式 / 模態模式」切換時,加入 0.3 秒 crossfade:舊 Actor 淡出同時新 Actor 淡入,避免「閃 1 幀舊 mesh」的殘留問題。

**完成標準:**
- 所有 4 種模式切換時無視覺閃爍(錄製 30fps 影片逐幀檢查)

---

#### C2-7:D/C >> 2.0 極端值數值疊加(Phase 2)
**工時:2 小時**

**說明:**
當 D/C > 2.0 時熱圖顏色飽和到紅,分不出差別。加 `bShowExtremeValue` toggle:超過 2.0 的構件上浮空顯示具體數值(如「D/C=3.4」)。

---

### C2 總工時:21 小時(MVP 12 小時 + Phase 2 9 小時)

---

## C3 失敗與崩塌(FrameCore N4 → Chaos 剛體交棒)

### 概念摘要

D/C > 1.5 時(或玩家手動觸發)啟動 N4 連續動力倒塌計算,非同步回傳 `FFrameDynCollapseResult`,再由 `AFrameDynCollapseReplayActor` 逐幀播放動畫,`AFrameFragmentClusterActor` 把脫落碎塊交給 UE Chaos 剛體物理處理後續運動。

「失敗即景觀」是整個教育設計的核心信念:崩塌動畫要夠精彩,學生才願意反覆試錯。

### 先決依賴

- C1:載重已施加
- C2:D/C > 1.5 觸發條件需要 Solve 結果

---

### C3 Sub-task 清單

#### C3-1:崩塌觸發條件判斷
**工時:2 小時**

**說明:**
在 `UArchSimModelRegistry::OnSolveComplete` 中,Solve 完成後:
1. 若 `OutResult.Utilization.MaxDC > 1.5` → 等待 0.5 秒(讓學生看到熱圖爆紅)→ 呼叫 `StartDynCollapseAsync()`
2. HUD 顯示「結構超載!即將崩塌...」倒計時 0.5 秒動畫(C3-2 開始前的預警)
3. 玩家手動按「測試崩塌」按鈕也可直接呼叫 `StartDynCollapseAsync()`(不管當前 D/C 多少)

新增 `bool bCollapseInProgress` flag,崩塌期間:
- 禁止新的 Solve 觸發(不讓玩家繼續放構件)
- 禁止載重修改(UI 按鈕 greyed out)

**完成標準:**
- D/C = 1.6 時自動觸發崩塌,倒計時動畫出現
- D/C = 0.9 時不觸發(負向測試)
- 手動按鈕在任何 D/C 狀態下均可觸發

---

#### C3-2:SolveDynCollapse Async Task 封裝
**工時:4 小時**

**說明:**
`UFrameAnalysisLibrary::SolveDynCollapse` 是 blocking 呼叫,在主執行緒執行會凍結整個 UE 畫面(通常 0.5~3 秒,取決於結構大小)。必須用 Async Task 封裝:

```cpp
// 在 UArchSimCollapseSystem 中實作
void StartDynCollapseAsync() {
    bCollapseInProgress = true;
    ShowComputingUI(); // "計算中..." spinner
    
    // Capture local copies for lambda (不可傳 this 裸指標)
    FFrameModelDef CaptureDef = Registry->GetCurrentModelDef();
    FFrameDynCollapseOptions CaptureDOpts = BuildCollapseOptions();
    
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [CaptureDef, CaptureDOpts, WeakThis = TWeakObjectPtr<UArchSimCollapseSystem>(this)]() {
        FFrameDynCollapseResult Result;
        UFrameAnalysisLibrary::SolveDynCollapse(CaptureDef, CaptureDOpts, Result);
        
        AsyncTask(ENamedThreads::GameThread, [Result, WeakThis]() {
            if (WeakThis.IsValid()) WeakThis->OnCollapseComplete(Result);
        });
    });
}
```

**涉及 API:**
- `UFrameAnalysisLibrary::SolveDynCollapse(FFrameModelDef, FFrameDynCollapseOptions, FFrameDynCollapseResult&)`
- `FFrameDynCollapseResult::Outcome`(Success/Diverged/MaxTime)
- `FFrameDynCollapseResult::Events[]`
- `FFrameDynCollapseResult::Frames[]`

**完成標準:**
- 計算期間(AsyncTask 執行中)UE 畫面仍可正常渲染(不凍結)
- `ShowComputingUI()` spinner 在計算完成後正確隱藏
- PIE Stop 時 WeakPtr 檢查正確(不 crash)
- `Outcome == Diverged` 時顯示「計算發散,結構數值不穩定」提示,而非播放錯誤動畫

**預期踩雷:**
- `AsyncTask` 中不可直接使用 UObject 的 UPROPERTY(不在 Game Thread),所有 FrameCore USTRUCT 必須先 capture 為 local copy
- `FFrameDynCollapseOptions` 的預設值需要確認:`MaxTime=10.0, DT=0.01, DampingRatio=0.05`(**需查 FrameCoreUE 頭檔確認欄位名稱**)

---

#### C3-3:AFrameDynCollapseReplayActor 播放整合
**工時:4 小時**

**說明:**
`OnCollapseComplete` 收到結果後:
1. 畫面震動(UE `UCameraShakeBase` + `ForceFeedbackEffect`)0.5 秒警告
2. 隱藏原始靜態結構(所有 ArchSim 構件 Actor 設 `bHidden=true`)
3. 呼叫 `AFrameDynCollapseReplayActor::SetCollapseResult(Result)` + `Play()`
4. Tick 驅動 `AFrameDynCollapseReplayActor` 逐幀 lerp Frames.UFlat(6N flat)推進位移

**涉及 API:**
- `AFrameDynCollapseReplayActor`(已實作 v3.5):
  - `CollapseResult`、`PlaybackSpeed`、`bLoop`、`Play()`、`Pause()`、`SetPlaybackTime(float)`
  - `OnEventReached` multicast delegate
- `UCameraShakeBase`、`APlayerController::ClientPlayCameraShake()`

**完成標準:**
- 崩塌動畫流暢(>30 fps, 5 秒模擬時間)
- `AFrameDynCollapseReplayActor` 的構件位移與 standalone 計算的 `Frames[t].UFlat` 一致(線性插值,rel < 1e-3)
- 玩家按 Space 暫停動畫,再按 Space 繼續(呼叫 `Pause()` / `Play()`)
- 倒塌結束後保留 30 秒靜態「廢墟」,顯示「查看診斷」按鈕

---

#### C3-4:OnEventReached 音效與 Niagara 粒子綁定(Phase 2)
**工時:4 小時**

**說明:**
綁定 `AFrameDynCollapseReplayActor::OnEventReached` delegate:
- 事件類型 `MemberRemoved`:觸發 `UAudioComponent::Play(SFX_StructuralBreak)` + `UNiagaraFunctionLibrary::SpawnSystemAtLocation(NS_Dust)`
- 事件類型 `HingeFormed`:觸發 `SFX_PlasticCrack` + `NS_ConcreteChip`
- 事件類型 `ClusterDetached`:觸發 `SFX_Rumble` + `NS_LargeDust`

**完成標準:**
- 每類事件對應不同音效,3 種音效來源各不同
- 粒子系統在世界座標正確生成(對應事件發生位置,不在 (0,0,0))

---

#### C3-5:多人倒塌 JSON 廣播與 Client 同步(Phase 2)
**工時:4 小時**

**說明:**
多人模式下,Server 算完 `FFrameDynCollapseResult` → JSON 序列化 → 透過 `AArchSimGameState` UPROPERTY(Replicated) 廣播 → 所有 Client 各自 play 動畫(時間軸從廣播時刻對齊)。

**完成標準:**
- 4 人同時在線,倒塌動畫時間軸差異 < 100ms

---

#### C3-6:Chaos 剛體 LOD 與 30 秒 cleanup(Phase 3)
**工時:3 小時**

**說明:**
倒塌後 30 秒自動 cleanup 所有 Chaos 碎塊 Actor(`Destroy()`),防止長時間遊戲後碎片堆積拖慢 FPS。

**完成標準:**
- 崩塌 30 秒後所有 AStaticMeshActor(debris)被 Destroy,FPS 恢復到崩塌前水準

---

### C3 總工時:21 小時(MVP 10 小時 + Phase 2 8 小時 + Phase 3 3 小時)

---

## C4 連通性與載重路徑(Union-Find + BFS 錨定)

### 概念摘要

Part C 中**教育價值最高但最容易做成「死碼」的功能**:讓學生看到「力如何從加載點傳遞到地基」。實作正確的前提是:Union-Find 正確分組 + BFS 正確判斷錨定,且這兩個結果會直接影響 FrameCore 的 prescribed DOF 設定(未錨定組跳過 Solve)。

主計畫書明確警告:Block Reality 的「Connectivity 死碼」失敗是「沒接上用途」。本系統的 C4 成果要直接驅動 C3 的崩塌條件判斷(漂浮結構不做崩塌,直接警告)和 C2 的 Solve 合法性(漂浮組不 Solve)。

### 先決依賴

- A1:`UArchSimModelRegistry`(提供 MemberIdx ↔ Actor 映射)
- B1:構件放置系統(Place/Remove 觸發 dirty flag)

---

### C4 Sub-task 清單

#### C4-1:UFrameStructureGroupSubsystem Union-Find
**工時:4 小時**

**說明:**
新建 `UFrameStructureGroupSubsystem : UGameInstanceSubsystem`,內部維護:

```cpp
// 壓縮 Union-Find(路徑壓縮 + 按秩合併)
TArray<int32> Parent;   // index = NodeIdx
TArray<int32> Rank;
TMap<int32, TSet<int32>> GroupToMembers; // GroupId → {MemberIdx}

// API
int32 Find(int32 NodeIdx);
void Union(int32 NodeA, int32 NodeB);
void RebuildFromModel(FFrameModelDef);  // 全量重建
void MarkDirty();                        // lazy flag
void UpdateIfDirty();                    // 在 Solve 前呼叫
```

觸發時機:
- Place / Remove 構件 → `MarkDirty()`
- 下次 Solve 前 `UpdateIfDirty()` → 若 dirty 則 `RebuildFromModel()`

**26-connectivity 偵測:**
每根 Member 連接 EndI/EndJ 兩個 Node → Union(EndI, EndJ)。不需要真的 26 方向掃描,因為 FrameCore 模型本身就是圖(Member 是邊,Node 是頂點),直接遍歷 Members 即可建圖。**主計畫書提到「26-connectivity」指的是空間體素鄰居,但本系統是梁柱圖,用構件連接關係建圖即可,不需要空間掃描。**

**完成標準:**
- 100 個構件,兩次 RebuildFromModel 各 < 10ms
- 分成 3 個不相連子結構的模型,正確回傳 3 個不同 GroupId
- 移除一根構件後 MarkDirty,下次 UpdateIfDirty 正確重建分組
- 邊界測試:0 構件 → GroupCount=0,1 構件 → GroupCount=1

---

#### C4-2:BFS 錨定判斷
**工時:3 小時**

**說明:**
在 `UFrameStructureGroupSubsystem` 新增 BFS 錨定:

```cpp
void RunAnchorBFS(TArray<int32> GroundNodeIdxs); // 地基節點清單
bool IsGroupAnchored(int32 GroupId);
int32 GetAnchorDepth(int32 NodeIdx); // 離地基最短路徑深度
```

邏輯:
1. 從每個地基節點(prescribed DOF 全 1 的節點)出發 BFS
2. 走過的節點標記 `bAnchored=true`,記錄 depth
3. depth 上限 = 64(防 stack overflow 與無限等待)
4. BFS 結束後未標記到的 GroupId 為「未錨定」

**完成標準:**
- 三層樓框架(地基 4 節點 → 屋頂 4 節點),屋頂 depth = 3 或 4(依結構路徑),正確標 anchored
- 一個懸空板(無任何與地面連接的構件),整組 `bAnchored=false`
- 1000 節點規模 BFS < 20ms
- 深度超過 64 時顯示 log warning 並停止 BFS(不 crash)

**預期踩雷:**
- 地基節點的判斷依據:`FFrameModelDef.Nodes[i].Fixed` 是否全部為 true(6 個 DOF 全固定 = 地基)。如果 Fixed 只有部分設定(如 roller 支座),不應視為地基 — 只有完全固定(鉸接或固接)才算錨定起點
- UE 環境下 FrameCore NodeIdx 的「Fixed」設定由 Part A 的 `UArchSimModelRegistry` 決定(接地節點自動設 Fixed),確認在 C4-2 呼叫前 Registry 已正確設定

---

#### C4-3:未錨定視覺化灰色標示
**工時:3 小時**

**說明:**
在 `UFrameStructureGroupSubsystem::OnAnchorStateChanged` 事件中:
- 未錨定構件 → 所有對應 Actor 材質參數「bAnchored=false」→ 材質顯示灰色(半透明)+ 顯示浮空 UMG 警告「未錨定 — 此結構沒有連到地基」
- 已錨定 → 恢復正常材質(D/C 色彩或白色預設)

在 `UArchSimModelRegistry::CanSolve()` 中:若所有構件都未錨定 → 回傳 false,阻止 Solve 並顯示「請先連接地基」

**完成標準:**
- 一個懸空柱(無地基連接),放下後立即灰色 + 警告文字
- 連接到地基後,1 幀內恢復正常顏色(不需等待下次 Solve)
- 部分錨定、部分未錨定:各組獨立顯示正確顏色

---

#### C4-4:載重路徑 BFS 視覺化(Phase 2)
**工時:4 小時**

**說明:**
L 鍵 → 啟動載重路徑工具 → 點擊一個節點 → BFS 從該節點向地基方向追蹤 → 沿途構件用動態線條高亮:

- 路徑線條顏色:深藍 #56B4E9(離加載點近)→ 綠 #009E73(接近地基)
- 線條粗細 ∝ 對應構件內力大小(從 `FFrameSolveResult.MemberForces[idx].EndI.N` 取軸力)

實作用 `ULineBatchComponent::DrawLine()` 動態繪製,每幀更新(如果 Solve 結果改變)。

**涉及 API:**
- `ULineBatchComponent::DrawLine(Start, End, Color, LifeTime=0.0, Thickness)`

**完成標準:**
- 三層框架點擊頂層節點,路徑高亮從頂到底所有構件
- 路徑粗細可見差異(最大軸力構件比零軸力構件粗 3x 以上)
- 路徑高亮在 100ms 內完成

---

#### C4-5:漂浮偵測模式全場掃描(Phase 2)
**工時:2 小時**

**說明:**
F 鍵切換「漂浮偵測模式」→ 全場所有未錨定構件閃紅(0.5Hz 閃爍),錨定構件顯示正常。讓學生一眼看出哪些部分「沒接到地」。

**完成標準:**
- 100 構件全場掃描 < 1 秒完成標記
- 閃爍頻率 0.5Hz(每秒亮 0.5 次,暗 0.5 次)

---

#### C4-6:Union-Find 超過 5000 節點 lazy 模式(Phase 3)
**工時:3 小時**

**說明:**
當節點數超過 5000,RebuildFromModel 可能超過 50ms。改為 lazy mode:Place/Remove 一次動作只做局部 Union/Split(增量更新),而非全量重建。

**完成標準:**
- 5000 節點規模,單次 Place/Remove 後 Union-Find 更新 < 5ms

---

#### C4-7:載重路徑精確力流從 FrameCore 反力倒推(Phase 3)
**工時:4 小時**

**說明:**
Phase 2 的路徑視覺化是拓樸連通路徑,Phase 3 改為用 `FFrameSolveResult.MemberForces` 的真實內力(軸力/剪力/彎矩)計算「哪條路徑承受了多少力」。讓視覺化從「有沒有路」升級為「哪條路承受得更多」。

---

### C4 總工時:23 小時(MVP 10 小時 + Phase 2 6 小時 + Phase 3 7 小時)

---

## Part C 整體摘要

### 總工時分配

| 子節 | MVP | Phase 2 | Phase 3 | 合計 |
|------|:---:|:-------:|:-------:|:----:|
| C1 載重施加 | 18h | 7h | - | 25h |
| C2 D/C 熱圖 | 12h | 9h | - | 21h |
| C3 失敗與崩塌 | 10h | 8h | 3h | 21h |
| C4 連通性與載重路徑 | 10h | 6h | 7h | 23h |
| **合計** | **50h** | **30h** | **10h** | **90h** |
| 緩衝(+20%) | — | — | — | **+22h** |
| **估算總工時** | — | — | — | **~112h** |

### 建議實作順序

1. **C4-1 + C4-2 + C4-3**(Union-Find + BFS + 視覺化)—— 先做,因為它決定哪些構件合法可 Solve
2. **C1-1 + C1-4**(載重 helper + 自重)—— 奠定所有 Solve 的輸入基礎
3. **C2-1**(debounce Solve 管道)—— 核心管道建通後所有後續功能才有意義
4. **C1-2 + C1-3**(節點力 + UDL UI)—— 玩家端操作
5. **C2-2 + C2-3 + C2-4**(熱圖 Actor + 色彩 + HUD 警告)—— 教學視覺核心
6. **C1-6**(載重組合包絡)—— 最後串接工況計算
7. **C3-1 + C3-2 + C3-3**(崩塌觸發 + Async + 播放)—— 建立在前述全部完成的基礎上

### 跨 Part 依賴圖

```
Part A1 (Registry)
    └─→ C4-1 (Union-Find)
    └─→ C1-1 (載重 helper)
    └─→ C2-1 (debounce Solve)

Part B1 (構件放置)
    └─→ C4-1 dirty flag

C4 完成
    └─→ C1 (有錨定才能有意義地加載)
    └─→ C2 (知道哪些組可 Solve)

C1 + C2 完成
    └─→ C3 (D/C > 1.5 觸發崩塌)
    └─→ Part D1 (掃描儀讀 C2 的 CachedUtilization)
    └─→ Part E1 (材料 patch → 觸發 C1/C2 重解)
```

### 主要技術風險

1. **SolveDynCollapse blocking 呼叫(C3-2)**:必須 Async,但 UE5 的 async 物件生命週期管理需嚴格 WeakPtr。建議在 C3 開始前先寫一個 Async Task 練習 case。

2. **BuildHeatmap 效能牆(C2-2)**:50 構件以下沒問題,100+ 構件需要 Phase 2 的 GPU 路徑。MVP 要在 QA 測試時限制最大構件數 ≤ 100。

3. **載重座標系混淆(C1-3)**:`FFrameMemberUDL.WLocal` 是局部座標,UI 必須有方向示意圖,否則學生看不出哪個方向是「重力向下」。

4. **Rebaseline vs ApplyPatchAndResolve 選擇(C2-1)**:每次材料/截面變更必須 Rebaseline(K 矩陣變化),只有幾何位置或載重改變才能 ApplyPatchAndResolve。Registry 要有 `bNeedRebaseline` flag 追蹤。

5. **Union-Find 多人並發(C4-1)**:MVP 強制 Server-only 計算,不在 Client 端跑 Union-Find,避免分歧。Phase 2 多人再設計同步方案。


---


# Part D 實作擴充(112h)

**標題**:Part D:診斷工具(學習儀器)

**子節數**:4

### MVP 必須(Phase 1)
- D1-T1: AArchSimScannerTool Actor 骨架 + EScannerMode enum + Q 鍵 Input Action 綁定
- D1-T2: 左鍵 Raycast 掃描 + UMemberInfoPanel UMG Widget 浮空顯示(7 欄位)
- D1-T3: FrameCore 結果讀取 helper — UArchSimScannerResultReader 讀 CachedUtilization + MemberForces
- D1-T4: 熱圖模式整合(呼叫 AFrameUtilizationHeatmapActor::SetActive)
- D2-T1: HUD 分析面板 UMG — 5 個 toggle button + 狀態管理
- D2-T2: 靜態變形動畫控制器 — DeflectionScale 滑桿 + 平滑插值
- D2-T3: 模態動畫控制器 — AnalysisModal 呼叫 + AFrameModalShapeActor 整合
- D3-T1: UMemberDiagramPanel UMG 骨架 + 三個子圖 Canvas 佈局
- D3-T2: 沿桿取樣邏輯 — EndI/EndJ 線性內插 11 點 helper
- D3-T3: 圖形繪製 — SFD/BMD/軸力折線 + 最大值標籤
- D4-T1: SF 儀表 UMG — UProgressBar + 三色分段 + lerp 平滑
- D4-T2: PivotMargin 預警燈 + 畫面紅邊框觸發

### Phase 2
- D1-T5: Shift+左鍵持續掃描模式(每幀 raycast + billboard 跟隨)
- D1-T6: 錨定模式整合(UFrameStructureGroupSubsystem::ShowAnchorPaths)
- D1-T7: 跨關卡 SaveGame 偏好記憶 + 多人獨立狀態同步
- D2-T4: 反應譜動畫控制器 — ResponseSpectrum 呼叫 + AFrameResponseSpectrumActor
- D2-T5: 即時動態動畫控制器 — RealTimeDynamic 呼叫 + AFrameRealTimeDynamicActor
- D2-T6: W-04 撓度超限警告整合(>L/250 觸發 UArchSimWarningSubsystem)
- D3-T4: 沿圖拖拉同步梁上位置高亮(綠點 + 軸距標籤)
- D3-T5: 單位切換(N/kN, N·mm/kN·m)+ 跨距 L 變化自動重繪
- D4-T3: 載重逐步增加 SF 動態演示(1.0L→1.2L→1.4L 步進)
- D4-T4: W-09 整體穩定性警告整合 + 教學提示訊息

### Phase 3
- D1-T8: 掃描儀裝備動畫(camera-attached FPS 武器舉起感, UMG 邊框效果)
- D1-T9: 掃描儀使用記錄 xAPI Log — 每次掃描構件寫 xAPI Statement(Part I 串接)
- D2-T7: 頻率/週期 HUD 側欄即時顯示同步
- D3-T6: Phase 2 加密取樣 — 接未來 FrameCore 沿桿取樣 API(roadmap C6, 需驗證 API 是否存在)
- D4-T5: SF 儀表動畫歷史曲線(存最近 30 秒 SF 值, 時序折線圖)

### 實作順序
- D1
- D3
- D2
- D4

### 跨 Part 依賴
- Part A1 (UArchSimMemberData Component + UArchSimModelRegistry) 必須先完成,D1/D3 才能讀 CachedUtilization 與 MemberIdx
- Part A1 DistributeSolveResult 必須正確將 FFrameSolveResult 分發到每個 Component,D1/D4 才有資料可讀
- Part C2 (FrameCore Solve 流程,ApplyPatchAndResolve) 必須能正常執行才能產生 FFrameSolveResult 供 D 系列讀取
- Part C3 (崩塌動畫) 的 AFrameDynCollapseReplayActor 與 D2 共用 HUD 分析面板 toggle,建議 D2 先定義面板框架後 C3 再整合
- Part F 關卡系統的失敗觸發(F4) 依賴 D4 SF < 1.0 的判斷事件
- Part I (學習 Log) 在 D3 Phase 3 才串接 xAPI,D1-D4 MVP 層可以只預留 hook 不實作

### 風險清單
- FrameCore engine source FROZEN(鐵則 #1):D3 Phase 3 規劃的「沿桿加密取樣 API」在現有 FrameCore 中不存在,MVP 只能用 EndI/EndJ 端部兩點線性內插 11 點。若需要拋物線 UDL 曲線精度,必須在 UE consumer 層自行計算(不能改 engine),並明確標示「教育用線性近似」
- FFrameSolveResult.MemberForces 的索引對應:MemberForces[memberIdx] 中的 memberIdx 是 FrameCore 內部 0-based index,與 UArchSimMemberData::MemberIdx 的對應由 UArchSimModelRegistry 維護,D1/D3 必須透過 Registry 做雙向查詢,不可直接假設 Actor 順序等於 index
- D4 的 FFrameSolveResult.Utilization.SafetyFactor:主計畫書假設此欄位存在,需驗證實際 USTRUCT FDemandSummary 中是否有 SafetyFactor 欄位;若無,需在 UE consumer 層計算 SF = 1.0 / DemandSummary.MaxDC(需確認欄位名稱)
- D4 的 PivotMargin:主計畫書假設 FFrameSolveResult.PivotMargin 存在;需驗證 FFrameSolveResult USTRUCT 是否有此欄位;若無則需在 UE consumer 層新增 helper 從 bSingular 旗標推導二元警告
- 掃描儀 UI 持續掃描模式效能:每幀 Raycast + UMG 更新在校園低規電腦(i5-8th gen, GTX 1050)可能壓低幀率;建議加 50ms debounce + 遊標移動距離閾值過濾
- 多人模式下 Server Solve 結果延遲:掃描儀在 Client 端顯示的是 CachedUtilization,若 Server Solve 還在進行中,顯示的是舊結果;需加「資料更新中」spinner 狀態,避免學生誤讀過期數據
- UMG Billboard Widget 在 UE5.7 的 3D Widget Component 效能:若每個構件都掛一個 WidgetComponent 會爆 draw call;對策是單例浮空 Panel,點選時移動到對應位置,而非每個構件各自持有

## 詳細擴充內容

# Part D:診斷工具(學習儀器)— 詳細實作計畫

> **Part 定位**:教育閉環「診斷」環節的核心實作。讓「看不見的結構行為」透過熱圖、變形動畫、BMD/SFD 圖、安全係數儀表等視覺工具變得可見。FrameCore v4.0.0 已提供所有必要的數值 API,本 Part 的工作全在 UE consumer 層(FrameCoreUE + ArchSim 遊戲層)。

---

## 前提條件與邊界說明

### FrameCore FROZEN 邊界(鐵則 #1)
所有實作必須嚴格在 `Plugins/FrameSolver/Source/FrameCoreUE/` 與 `Source/ArchSim/` 兩個目錄下進行。不得修改 `Plugins/FrameSolver/Source/FrameCore/` 下的任何文件。

### 已可用的 FrameCore API(v4.0.0 確認)
- `AFrameUtilizationHeatmapActor`:讀 `MemberUtilization[]` + `ShellUtilization[]`,PMC 11-ring 熱圖網格,已實作
- `AFrameDeformedShapeActor`:讀 `FFrameSolveResult.Displacements[]`,DeflectionScale UPROPERTY,已實作
- `AFrameModalShapeActor`:讀 `FFrameModalResult.Modes[ModeIndex].Shape`,Tick 驅動 cos 動畫,已實作
- `AFrameResponseSpectrumActor`:EnvelopeHz UPROPERTY,已實作
- `AFrameRealTimeDynamicActor`:Newmark 步驟,已實作
- `UFrameAnalysisLibrary::AnalysisModal()` / `ResponseSpectrum()` / `RealTimeDynamic()`:BP callable
- `UFrameInteractiveSubsystem`:StartSession / ApplyPatchAndResolve / ResolveCurrent / EndSession
- `FFrameSolveResult` 含 `MemberForces[]`(`FFrameMemberInternalForces` EndI/EndJ 各 6 分量)
- `FFrameSolveResult.Utilization`(`FDemandSummary`:MaxDC, GoverningMemberIdx, bSingular 等)

### 需在 UE consumer 層新增的 helper
- `UArchSimScannerResultReader`:從 `UArchSimModelRegistry` + `FFrameSolveResult` 組合每個構件的診斷資訊
- `UArchSimDiagramHelper`:端部內力 → 11 點取樣 → 折線資料轉換
- `UArchSimSafetyFactorHelper`:SF = 1.0 / MaxDC 計算(需先確認 FDemandSummary 欄位名稱)

---

## D1 應力掃描儀(熱圖模式 + 錨定模式)

### 設計目標
給學生一個主動探索工具,讓他們用「掃一下就知道」的互動方式理解每根構件的受力狀態。對應 PhET 隱性鷹架原則:工具本身就是教學。

### Sub-task 清單

#### D1-T1:AArchSimScannerTool Actor 骨架 + EScannerMode enum + Q 鍵 Input Action 綁定
**估時**:3 小時

**具體工作**:
- 在 `Source/ArchSim/Scanner/` 建立 `AArchSimScannerTool : public AActor`
- 加 `EScannerMode { Heatmap, Anchor }` UENUM
- 在 `DA_ArchSimInputActions` DataAsset 新增 `IA_ToggleScanner`(ETriggerEvent::Started)與 `IA_SwitchScannerMode`
- 在 `AArchSimCharacter` 的 `MC_Building` MappingContext 中綁定 Q 鍵 → `IA_ToggleScanner`
- Scanner Actor 跟隨 Character Camera Socket(Billboard 不做 mesh,MVP 只做功能)
- `bIsEquipped` bool UPROPERTY(SaveGame),跨關卡記住狀態

**涉及 API / Class**:
- `UEnhancedInputComponent::BindAction(IA_ToggleScanner, ETriggerEvent::Started, this, &AArchSimCharacter::OnToggleScanner)`
- `UInputMappingContext::MapKey(IA_ToggleScanner, EKeys::Q)`
- `AActor::AttachToComponent(CharacterMesh, Socket)`

**完成標準**:PIE 中按 Q 鍵 Actor bIsEquipped 在 true/false 之間切換,Console log 正確輸出模式名稱。

**預期踩雷**:ALS-Refactored 的角色 MappingContext 在 Possess 後 1 幀才正確初始化(UE5.7 已知問題),Q 鍵必須在 `OnPossess` + `FTimerManager::SetTimerForNextTick` 延遲後才 AddMappingContext。

**Phase 分級**:MVP

---

#### D1-T2:左鍵 Raycast 掃描 + UMemberInfoPanel UMG Widget 浮空顯示(7 欄位)
**估時**:4 小時

**具體工作**:
- 在 `MC_Building` 中綁定 `IA_PlaceMember`(左鍵)與掃描儀模式互斥:若 `bIsEquipped=true` 則攔截左鍵為掃描動作
- 實作 `AArchSimScannerTool::PerformScan()`:Camera Forward LineTrace(距離 2000cm),Hit Actor 查詢是否含 `UArchSimMemberData`
- 建立 `UMemberInfoPanel : public UUserWidget`,含 7 個 `UTextBlock`:構件 ID / 類型 / 材料 / 斷面尺寸 / 內力(N/Vy/Mz 最大值) / D/C 比 / 錨定狀態
- Widget 掛在 `UWidgetComponent`(World Space,billboard `bFaceCamera=true`)上,初始 Hidden
- 點選成功後 `SetVisibility(Visible)` + `SetWorldLocation(HitLocation + Offset)`

**涉及 API / Class**:
- `UWorld::LineTraceSingleByChannel(QueryParams, ECC_Visibility)`
- `UWidgetComponent::SetWidgetClass(UMemberInfoPanel::StaticClass())`
- `UWidgetComponent::SetDrawSize(FVector2D(300, 200))`
- `UUserWidget::SetVisibility(ESlateVisibility::Visible)`

**完成標準**:點選一根梁,Panel 出現在梁的中點附近,7 個欄位均顯示非空值(材料名稱、斷面、D/C 數字)。

**預期踩雷**:`UWidgetComponent` World Space 模式在 Listen Server 多人情境下只在本地 Client 顯示(Widget 不 Replicate),此行為符合設計(每人獨立掃描器)。需確認 bIsLocallyControlled 守門,防止 Server 端角色驅動其他 Client 的掃描器。

**Phase 分級**:MVP

---

#### D1-T3:FrameCore 結果讀取 helper — UArchSimScannerResultReader
**估時**:3 小時

**具體工作**:
- 建立 `UArchSimScannerResultReader : public UBlueprintFunctionLibrary`
- 函式 `FArchSimMemberDiagnostic GetMemberDiagnostic(UArchSimMemberData* Comp, const FFrameSolveResult& Result)`
- `FArchSimMemberDiagnostic` USTRUCT:MemberIdx, MaterialName, SectionDesc, AxialN_kN, ShearVy_kN, MomentMz_kNm, UtilizationDC, bIsAnchored
- 從 `FFrameSolveResult.MemberForces[Comp->MemberIdx].EndI` 讀 N / Vy / Mz(取 EndI/EndJ 絕對值最大者)
- 從 `UArchSimMemberData::CachedUtilization` 讀 D/C
- 材料名稱/斷面描述從 `UArchSimModelRegistry` 的構件登記資料讀取

**涉及 API / Class**:
- `FFrameMemberInternalForces::EndI` / `EndJ`(各含 N, Vy, Vz, T, My, Mz,單位 N / N·mm)
- `UArchSimModelRegistry::GetMemberMaterialName(int32 MemberIdx)` — 需新增此函式
- `FFrameSolveResult::bSingular` 守門:若 true 回傳空結構體 + 顯示「模型不穩定」

**完成標準**:單元測試:建立簡支梁模型 SolveLinear,呼叫 GetMemberDiagnostic,驗證 AxialN_kN 與 MomentMz_kNm 對標手算解析值(誤差 < 1%)。

**預期踩雷**:`FFrameMemberInternalForces` 的 EndI.N 單位是 N(不是 kN),面板顯示前必須除以 1000;MomentMz 單位是 N·mm,除以 1e6 得 kN·m。單位換算文件需明確記錄。

**Phase 分級**:MVP

---

#### D1-T4:熱圖模式整合(呼叫 AFrameUtilizationHeatmapActor::SetActive)
**估時**:2 小時

**具體工作**:
- 在 `AArchSimScannerTool::SwitchMode(EScannerMode NewMode)` 中:
  - `Heatmap` → 找場景中的 `AFrameUtilizationHeatmapActor`,呼叫 `SetActive(true)` 並確保 BuildMesh 已執行
  - `Anchor` → `SetActive(false)` + 呼叫 `UFrameStructureGroupSubsystem::ShowAnchorPaths(true)`(此 Subsystem 為 Phase 2 工作,MVP 留 stub)
- 右鍵綁定 `IA_SwitchScannerMode`
- HUD 顯示當前模式名稱(「熱圖模式」/「錨定模式」)

**涉及 API / Class**:
- `UGameplayStatics::GetActorOfClass(World, AFrameUtilizationHeatmapActor::StaticClass())`
- `AFrameUtilizationHeatmapActor::SetActive(bool)` — 需確認此函式簽名是否已存在;若無需新增 UE consumer 層 wrapper
- `UMG::UTextBlock::SetText(FText)` 更新 HUD 模式文字

**完成標準**:按右鍵切換,熱圖 Actor 在場景中顯示/隱藏,HUD 文字同步更新,無視覺殘留。

**Phase 分級**:MVP

---

#### D1-T5:Shift+左鍵持續掃描模式(每幀 raycast + billboard 跟隨)
**估時**:3 小時

**具體工作**:
- 新增 `IA_ContinuousScan`(Hold + Shift modifier)
- 在 Character Tick 中若 `bContinuousScanning=true` 則每幀 LineTrace
- 加 50ms debounce:若遊標位移 < 3px 不重算
- Panel billboard 每幀跟隨 Hit Actor 中心

**涉及 API / Class**:
- `ETriggerEvent::Ongoing` for Hold input
- `FInputActionValue::GetMagnitude()` 判斷 Shift 是否按住

**完成標準**:按住 Shift+左鍵移動遊標,Panel 平滑跟隨,60 fps 下 CPU 佔用不超過 2% 增量。

**Phase 分級**:Phase 2

---

#### D1-T6:錨定模式整合
**估時**:4 小時

**具體工作**:
- 實作 `UFrameStructureGroupSubsystem` 的 `ShowAnchorPaths(bool)` 功能
- 讀取 `UArchSimModelRegistry` 的 Union-Find 結構,找到連通到固定支承的載重路徑
- 以 UE SplineComponent 或 Niagara 粒子效果畫出載重路徑(藍白 → 地基方向)
- 錨定狀態:若構件所在連通分量包含 ≥1 個固定支承節點 → bIsAnchored = true

**Phase 分級**:Phase 2

---

#### D1-T7:跨關卡 SaveGame 偏好記憶 + 多人獨立狀態同步
**估時**:3 小時

**具體工作**:
- 在 `UArchSimSaveGame` 加 `EScannerMode LastScannerMode` + `bool bScannerWasEquipped` 欄位
- `BeginPlay` 時讀取 SaveGame 還原狀態
- 多人：Scanner Actor 屬於本地 Character,不 Replicate Scanner Mode;各玩家狀態完全獨立

**Phase 分級**:Phase 2

---

#### D1-T8:掃描儀裝備動畫(FPS 武器舉起感)
**估時**:3 小時

**Phase 分級**:Phase 3(非功能性,純體驗品質)

---

#### D1-T9:掃描儀使用記錄 xAPI Log
**估時**:2 小時(需 Part I 先完成)

**Phase 分級**:Phase 3

---

### D1 小結
- **MVP 工時**:D1-T1 ~ D1-T4 合計約 **12 小時**
- **Phase 2 工時**:D1-T5 ~ D1-T7 約 **10 小時**
- **Phase 3 工時**:D1-T8 ~ D1-T9 約 **5 小時**

---

## D2 變形動畫(位移放大 + 模態振型)

### 設計目標
讓學生「看到」結構如何在載重下彎曲、在地震下搖晃。FrameCore 已實作全套 Actor,本節工作是建立 HUD 控制面板將這些 Actor 正確串接起來,提供順暢的切換體驗。

### Sub-task 清單

#### D2-T1:HUD 分析面板 UMG — 5 個 toggle button + 狀態管理
**估時**:4 小時

**具體工作**:
- 建立 `UAnalysisPanelWidget : public UUserWidget`
- 5 個 `UButton`:靜態變形 / 模態 1 / 模態 2 / 模態 3 / 反應譜 / 即時動態(共 6 個,規格書標 5 但含 6 個)
- `EAnalysisMode` UENUM 配合狀態機:None / StaticDeform / Modal(1-3) / ResponseSpectrum / RealTimeDynamic
- 切換按鈕時:停止前一個 Actor 的 Tick 動畫,啟動新的
- 右側 HUD 固定位置,1366×768 最小解析度下不遮擋中央場景

**涉及 API / Class**:
- `UButton::OnClicked` Delegate 綁定
- `AFrameDeformedShapeActor::BuildMesh()` — 注意:每次換 DeflectionScale 都需重建 PMC
- `AFrameModalShapeActor::SetModeIndex(int32)` — 需確認此函式存在;若無需新增 setter
- `AFrameModalShapeActor::SetActorTickEnabled(bool)` 控制動畫播放

**完成標準**:6 個按鈕均可切換,前一個模式 Actor 正確停止(不再 Tick),新模式 Actor 啟動。切換不超過 200ms。

**預期踩雷**:AnalysisModal 呼叫是同步阻塞的,若結構 DOF 大(>10000)會在 Game Thread 卡頓。建議用 `AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, ...)` 非同步呼叫,完成後 MainThread 回 callback 更新 Actor。需驗證 UFrameAnalysisLibrary 函式是否 Thread-safe(預計不是,需用 mutex 或限定 Game Thread)。

**Phase 分級**:MVP

---

#### D2-T2:靜態變形動畫控制器 — DeflectionScale 滑桿 + 平滑插值
**估時**:3 小時

**具體工作**:
- 在分析面板加 `USlider` 控制 DeflectionScale(範圍 1-500,預設 100)
- Slider OnValueChanged → `AFrameDeformedShapeActor::DeflectionScale = Value; BuildMesh()`
- 加 `bInterpolatingToScale` 狀態:切換到靜態變形模式時 2 秒線性插值從 ×1 → ×100
- `FTimerManager::SetTimer` 每 0.05s tick 更新 Scale
- 超過 ×300 時 HUD 顯示「視覺示意,非真實位移」警告

**涉及 API / Class**:
- `USlider::SetValue(float)` / `OnValueChanged` delegate
- `AFrameDeformedShapeActor::DeflectionScale` UPROPERTY
- `AFrameDeformedShapeActor::BuildMesh()` — 注意每次呼叫重建 PMC mesh,頻繁呼叫有開銷

**完成標準**:滑桿從 1 拉到 500,DeformedShape mesh 對應放大;插值動畫 2 秒平滑,無跳躍。

**預期踩雷**:`BuildMesh()` 重建 PMC 是 CPU 密集操作。Slider OnValueChanged 每幀觸發會造成每幀重建 PMC。必須加 debounce(100ms throttle),避免拖拉滑桿時效能崩潰。

**Phase 分級**:MVP

---

#### D2-T3:模態動畫控制器 — AnalysisModal 呼叫 + AFrameModalShapeActor 整合
**估時**:4 小時

**具體工作**:
- 玩家按「模態 1/2/3」按鈕 → 觸發 `UFrameAnalysisLibrary::AnalysisModal(ModelDef, SolveOpts, ModalOpts)`,`NumModes=3`
- 結果 `FFrameModalResult` 存在 `UArchSimAnalysisCache` subsystem(避免重複計算)
- `AFrameModalShapeActor::SetModeResult(ModalResult)` + `SetModeIndex(0/1/2)`
- HUD 顯示當前模態頻率(Hz)與週期(s):`FText::Format("{0} Hz / {1} s", FreqHz, 1.0/FreqHz)`
- `USlider` 控制 TimeScale(播速,0.1× ~ 5×)

**涉及 API / Class**:
- `UFrameAnalysisLibrary::AnalysisModal(FFrameModelDef, FFrameSolveOptions, FFrameModalOptions, FFrameModalResult&)` — BP callable,需確認函式簽名
- `FFrameModalResult::Modes[]` array 與 `FFrameModalMode::FreqHz`
- `AFrameModalShapeActor::TimeScale` UPROPERTY

**完成標準**:按模態 1,結構以正確頻率振動(對標 FrameCore standalone F fixture 對應的簡單梁解析頻率),HUD 頻率數值正確。

**預期踩雷**:若模型為機構(bSingular=true),AnalysisModal 回傳空 ModalResult 或錯誤;需在呼叫前檢查 SolveLinear 是否已成功。另外,AnalysisModal 的 NumModes 要確認 FrameCore 支援的最大值。

**Phase 分級**:MVP

---

#### D2-T4:反應譜動畫控制器
**估時**:3 小時

**具體工作**:
- 按「反應譜」→ `UFrameAnalysisLibrary::ResponseSpectrum(...)`,`ExcDof=2`(Z 向地震),`EFrameSpectrumCombo::SRSS`,`Zeta=0.05`
- `AFrameResponseSpectrumActor::EnvelopeHz = 0.5`(可調 UPROPERTY)
- HUD 顯示「5% 阻尼 SRSS 包絡」提示

**Phase 分級**:Phase 2

---

#### D2-T5:即時動態動畫控制器
**估時**:3 小時

**Phase 分級**:Phase 2

---

#### D2-T6:W-04 撓度超限警告整合
**估時**:2 小時

**具體工作**:
- 讀 `FFrameSolveResult.Displacements[]`,找最大撓度 δ_max
- 對每根梁計算 L(梁長) / 250,若 δ_max > L/250 觸發 `UArchSimWarningSubsystem::RaiseWarning(W-04, MemberIdx)`
- HUD 黃色警告框:「梁 M-{n} 撓度 = {v} mm > L/250 = {limit} mm」

**Phase 分級**:Phase 2

---

### D2 小結
- **MVP 工時**:D2-T1 ~ D2-T3 合計約 **11 小時**
- **Phase 2 工時**:D2-T4 ~ D2-T6 約 **8 小時**
- **Phase 3 工時**:D2-T7(HUD 側欄同步)約 **2 小時**

---

## D3 BMD/SFD 沿桿圖(彎矩剪力分布)

### 設計目標
統測高頻考點「剪力圖與彎矩圖」的遊戲化呈現。學生點選一根梁就能看到 SFD/BMD,對照課本手算結果,建立「電腦算的」與「手算的」之間的橋接。

### Sub-task 清單

#### D3-T1:UMemberDiagramPanel UMG 骨架 + 三個子圖 Canvas 佈局
**估時**:3 小時

**具體工作**:
- 建立 `UMemberDiagramPanel : public UUserWidget`
- 採用 `UVerticalBox` 包含三個子區塊,各含 `UTextBlock`(標題:SFD / BMD / 軸力)+ `UImage`(繪圖區)
- 面板在 HUD 右側從畫面外滑入(`FWidgetAnimationBinding` / `UWidgetAnimation` slide-in)
- 最小解析度 1366×768 下,面板寬 360px 不遮擋主視角
- 關閉按鈕(X)+ ESC 鍵均可關閉

**涉及 API / Class**:
- `UWidgetAnimation` 配合 `UUserWidget::PlayAnimation()`
- `UCanvasPanel` 作為各子圖的繪圖底板
- `UImage::SetBrush()` 搭配 Runtime Render Target

**完成標準**:點選梁後面板從右側滑入,三個子圖佔位正確,標題文字顯示,關閉功能正常。

**Phase 分級**:MVP

---

#### D3-T2:沿桿取樣邏輯 — EndI/EndJ 線性內插 11 點 helper
**估時**:3 小時

**具體工作**:
- 建立 `UArchSimDiagramHelper : public UBlueprintFunctionLibrary`
- 函式 `TArray<FArchSimDiagramPoint> SampleMemberForces(const FFrameMemberInternalForces& Forces, int32 NSamples=11)`
- 以線性內插 EndI → EndJ 的 N / Vy / Mz 各分量(等間距 NSamples 個點)
- `FArchSimDiagramPoint { float XNorm; float N_kN; float Vy_kN; float Mz_kNm; }`
- 說明文字:「當前為線性近似(端部兩點插值)。UDL 拋物線分布請待 Phase 2 加密取樣。」

**涉及 API / Class**:
- `FFrameMemberInternalForces`:結構體欄位 EndI.N / EndI.Vy / EndI.Mz / EndJ.N / EndJ.Vy / EndJ.Mz
- 需確認欄位單位:N 系 N(牛頓),彎矩系 N·mm

**完成標準**:建立簡支梁均布載重模型 SolveLinear,呼叫 SampleMemberForces(11),驗證 Mz 中點值對標手算 wL²/8,誤差 < 5%(端部兩點線性插值對 UDL 拋物線的固有近似誤差)。

**預期踩雷**:FrameCore 的 EndI.N 正壓為正(壓力 = 正值),與某些規範慣例相反。取樣點的 Mz 方向需確認 FrameCore 慣例並在圖上標示「壓正/拉負」或相反,不得與課本矛盾而不說明。

**Phase 分級**:MVP

---

#### D3-T3:圖形繪製 — SFD/BMD/軸力折線 + 最大值標籤
**估時**:4 小時

**具體工作**:
- 建立 `UArchSimLineChart : public UUserWidget`,以 `UCanvasPanel` + `OnPaint` override 繪製折線
- `OnPaint` 中用 `FSlateDrawElement::MakeLines(AllottedGeometry, ...)` 畫折線
- 縱軸自動 scale:±`max(abs(all points)) * 1.2`,防跳動(加滯後:只有新 max 比舊 max 大 20% 以上才重 scale)
- 在最大值點繪製十字標記 + 浮空 `UTextBlock`(M_max = {v} kN·m at x = {x} m)
- 零線(y=0)畫紅色細線,正值區域淡灰填色

**涉及 API / Class**:
- `FSlateDrawElement::MakeLines(FPaintArgs, const FGeometry&, const TArray<FVector2D>& Points, ...)`
- `FSlateDrawElement::MakeBox(...)` 用於填色
- 注意:OnPaint 必須在 UMG Widget 中 override `NativePaint(...)`,不是直接呼叫 Slate

**完成標準**:簡支梁均布載重時,BMD 圖呈拋物線形(線性插值近似),頂點在 x=L/2 處,最大值標籤數值正確(誤差 < 5%)。

**預期踩雷**:`FSlateDrawElement::MakeLines` 在 UMG 中直接使用有一定複雜度,需繼承 `UUserWidget` 並正確 override `NativePaint`。另一種方案是用 Runtime Render Target + UCanvas 的 `DrawLine`(`UCanvas::K2_DrawLine`),後者較容易但解析度固定。建議先用 Runtime Render Target 方案,Phase 2 再換 Slate 高解析度方案。

**Phase 分級**:MVP

---

#### D3-T4:沿圖拖拉同步梁上位置高亮
**估時**:3 小時

**具體工作**:
- 偵測 `UArchSimLineChart` 上的 Mouse Hover 事件,讀取游標 X 座標 → 換算到梁上 `XNorm` 位置
- 在 3D 場景的梁上繪製一個綠色球形 `UStaticMeshComponent`(直徑 20cm),位置 = EndI + XNorm × (EndJ - EndI)
- 軸距標籤浮空在球旁:`x = {dist}m from left end`
- 離開圖表區域 → 球形隱藏

**Phase 分級**:Phase 2

---

#### D3-T5:單位切換 + 跨距 L 變化自動重繪
**估時**:2 小時

**Phase 分級**:Phase 2

---

#### D3-T6:Phase 2+ 加密取樣
**估時**:待定(依賴 FrameCore 沿桿取樣 API 是否在 roadmap C6 中落地)

**重要說明**:主計畫書 roadmap C6 提到「沿桿取樣 API」,但目前 FrameCore v4.0.0 engine 已 FROZEN,如果此 API 不存在於 v4.0.0,就不能從 engine 層新增。需在 UE consumer 層用「更多插值點 + UDL 拋物線公式自行計算」實現,而非依賴 engine API。實作前必須先確認:`FFrameMemberInternalForces` 是否含有沿桿多點取樣,或僅有端部兩點。若僅端部兩點,則 UDL 拋物線的 SFD/BMD 在 UE 端用梁端反力 + UDL 大小自行計算,不需 engine 擴充。

**Phase 分級**:Phase 3

---

### D3 小結
- **MVP 工時**:D3-T1 ~ D3-T3 合計約 **10 小時**
- **Phase 2 工時**:D3-T4 ~ D3-T5 約 **5 小時**
- **Phase 3 工時**:D3-T6 約 **4-6 小時**

---

## D4 安全係數 + 條件數預警

### 設計目標
提供比「熱圖紅了就垮」更細緻的連續預警:SF 儀表讓學生看到「還差多遠才垮」,PivotMargin 燈讓學生感知機構臨界。本節是 Part D 進階,MVP 不必做,但 Phase 2 是完整教育體驗的關鍵組成。

**重要前提確認**:實作前必須查閱 `FDemandSummary` 與 `FFrameSolveResult` 的實際 USTRUCT 定義,確認以下欄位是否存在:
- `FDemandSummary::MaxDC`(float,所有桿件最大 D/C 比)— 若存在,SF = 1.0 / MaxDC
- `FFrameSolveResult::bSingular`(bool)— 已確認存在
- `FFrameSolveResult::PivotMargin`— 需驗證是否存在於 USTRUCT;若不存在,Phase 2 只實作 SF 儀表,PivotMargin 燈改用 bSingular 二元替代

### Sub-task 清單

#### D4-T1:SF 儀表 UMG — UProgressBar + 三色分段 + lerp 平滑
**估時**:4 小時

**具體工作**:
- 建立 `USafetyFactorGaugeWidget : public UUserWidget`
- `UProgressBar` 顯示 SF 值,自訂材質:SF < 1.5 紅 / 1.5-2.0 黃 / ≥ 2.0 綠(對應 TD-8 色彩規範)
- SF 計算:`SF = (MaxDC > 0.0f) ? (1.0f / MaxDC) : 99.0f`
- 平滑更新:`CurrentSF = FMath::FInterpConstantTo(CurrentSF, TargetSF, DeltaTime, 0.5f)`(每秒最多變化 0.5)
- 儀表刻度:0.0 / 1.0 / 1.5 / 2.0 / 3.0,標示文字
- 「SF = {v}(彈性容許應力法)」說明文字,避免學生與 LRFD 混淆

**涉及 API / Class**:
- `UProgressBar::SetPercent(float)` — 值需 normalize 到 0-1 範圍(SF=3.0 為滿格)
- `UProgressBar::SetFillColorAndOpacity(FLinearColor)` 動態切換顏色
- `FFrameSolveResult::Utilization` → `FDemandSummary` — 需確認欄位名稱(CLAUDE.md 顯示為 `Utilization.MaxDC`)

**完成標準**:SF 儀表在不同結構載重下正確顯示三色;載重增加時 SF 平滑減少,無跳動;SF < 1.0 時儀表顯示紅色滿格。

**預期踩雷**:SF = 1/MaxDC 在 MaxDC=0(無載重或無結構)時會除以零。加守門:`if (MaxDC < 1e-6f) SF = 99.0f`。儀表 Percent 值需 clamp 到 [0, 1],SF 可能超過 3.0 但 ProgressBar 只能顯示 100%,需設定上限邏輯。

**Phase 分級**:Phase 2(計畫書明確標注 D4 非 MVP)

---

#### D4-T2:PivotMargin 預警燈 + 畫面紅邊框觸發
**估時**:3 小時

**具體工作**:
- 確認 `FFrameSolveResult` 是否含 `PivotMargin` 欄位;若無,使用 `bSingular` 替代
- PivotMargin 二元呈現:只顯示「綠燈(安全)/紅燈(臨界或機構)」,不顯示數字
- 紅燈觸發:若 `bSingular=true` 或 `PivotMargin < threshold`(threshold 需在現場測試後設定)
- 紅燈亮起 → 全畫面紅色邊框 `UImage` overlay(透明度 0.3,閃爍 1Hz)
- 同時觸發 `UArchSimWarningSubsystem::RaiseWarning(W-09, "整體結構臨界不穩定")`

**涉及 API / Class**:
- `FFrameSolveResult::bSingular`(已確認)
- UMG `UImage` overlay 閃爍:`UWidgetAnimation` 或 `SetRenderOpacity(FMath::Sin(...) * 0.5 + 0.5)`

**完成標準**:刪除一根支承構件讓結構成機構,紅燈亮起 + 紅邊框閃爍,不超過 500ms 延遲。

**Phase 分級**:Phase 2

---

#### D4-T3:載重逐步增加 SF 動態演示
**估時**:3 小時

**具體工作**:
- 新增「載重加倍」演示按鈕,點選後從 1.0L → 1.2L → 1.4L 按 3 步各 ApplyPatchAndResolve
- SF 儀表即時動畫,讓學生目睹「SF 如何隨載重下降」
- 每步之間 1 秒延遲,總演示 3 秒
- 演示結束後自動還原到原載重

**Phase 分級**:Phase 2

---

#### D4-T4:W-09 警告整合 + 教學提示訊息
**估時**:2 小時

**Phase 分級**:Phase 2

---

#### D4-T5:SF 歷史曲線(Phase 3)
**估時**:4 小時

**Phase 分級**:Phase 3

---

### D4 小結
- **MVP 工時**:0(D4 整體非 MVP)
- **Phase 2 工時**:D4-T1 ~ D4-T4 合計約 **12 小時**
- **Phase 3 工時**:D4-T5 約 **4 小時**

---

## Part D 整體彙總

### 總工時估算

| 子節 | MVP | Phase 2 | Phase 3 | 小計 |
|------|-----|---------|---------|------|
| D1 應力掃描儀 | 12h | 10h | 5h | 27h |
| D2 變形動畫 | 11h | 8h | 2h | 21h |
| D3 BMD/SFD | 10h | 5h | 6h | 21h |
| D4 安全係數 | 0h | 12h | 4h | 16h |
| **Part D 合計** | **33h** | **35h** | **17h** | **85h** |

含 10% 整合測試與除錯緩衝:**總估算約 94-112 小時**,建議以 112 小時(14 天,全職 1 人)報計畫書。

### 建議實作順序(內部)

1. **D1 先**:掃描儀是所有診斷工具的「入口」,建立 UArchSimScannerResultReader 的資料讀取模式後,D2/D3 可重用相同 Pattern
2. **D3 第二**:BMD/SFD 與掃描儀動作相連(點選梁 → 兩者同時觸發),早期整合可以一次驗收「選梁」這個共用操作
3. **D2 第三**:變形動畫依賴分析面板,需要 HUD 框架先穩定。AnalysisModal 的非同步呼叫需要較多調試時間,排在 D1/D3 之後較安全
4. **D4 最後**:明確標注為 Phase 2,不佔 MVP 資源

### 跨 Part 依賴

| 依賴 | 必須先完成 | Part D 哪個子節需要 |
|------|-----------|---------------------|
| Part A1 (UArchSimMemberData + UArchSimModelRegistry) | MVP 完成 | D1-T3, D3-T2(需 MemberIdx 映射) |
| Part A1 DistributeSolveResult | MVP 完成 | D1-T2(CachedUtilization) |
| Part C2 (SolveLinear + ApplyPatchAndResolve 流程) | MVP 完成 | D2-T3, D4-T1(需有 SolveResult) |
| Part F 警告系統 (UArchSimWarningSubsystem) | Phase 2 完成 | D2-T6, D4-T2, D4-T4 |
| Part I (xAPI Log) | Phase 3 完成 | D1-T9 |

### 風險紅旗清單

1. **FDemandSummary::MaxDC 欄位名稱**:計畫書寫 `Utilization.MaxDC`,FrameCore 實際欄位名稱需查 `FFrameSolveResult.h` 確認,實作前不可假設
2. **PivotMargin 欄位存在性**:D4 的 PivotMargin 預警依賴此欄位;CLAUDE.md 提到「N4 倒塌驅動器用」,可能不在 FFrameSolveResult USTRUCT 的公開欄位中,需驗證
3. **AnalysisModal Thread Safety**:如前述,非同步呼叫需要仔細測試
4. **沿桿取樣的「教育誠信」**:11 點線性插值對 UDL 拋物線 BMD 有誤差。必須在 UI 明確標示「線性近似」,避免學生誤以為精確值後拿去對照課本手算而產生混淆

---

## 附錄:關鍵 FrameCore USTRUCT 欄位速查(實作前須 grep 確認)

```
// 需確認的欄位(grep 在 Plugins/FrameSolver/Source/FrameCore/Public/)
FFrameSolveResult::bSingular          — 已確認存在
FFrameSolveResult::Utilization        — FDemandSummary 型別,確認
FDemandSummary::MaxDC 或 MaxDC  — 需查 FFrameSolveResult.h 確認欄位名
FDemandSummary::GoverningMemberIdx    — 已確認存在(v3.3.0 修正過)
FFrameMemberInternalForces::EndI.N    — 需確認單位(N 或 kN)
FFrameSolveResult::PivotMargin        — 需驗證是否在公開 USTRUCT 中

// 已確認存在的 Actor/API
AFrameUtilizationHeatmapActor         — v3.5.0 實作
AFrameDeformedShapeActor              — v3.5.0 實作  
AFrameModalShapeActor                 — v3.5.0 實作
AFrameResponseSpectrumActor           — v3.5.0 實作
AFrameRealTimeDynamicActor            — v3.5.0 實作
UFrameAnalysisLibrary::AnalysisModal  — v3.4.0 實作
UFrameInteractiveSubsystem            — v3.5.0 實作
```


---


# Part E 實作擴充(96h)

**標題**:Part E:施工工序(E1–E4)

**子節數**:4

### MVP 必須(Phase 1)
- E1-T1:UConstructionStateMachine ActorComponent 骨架與 enum 定義
- E1-T2:狀態轉換 RPC 鏈(Client 請求 → Server 驗證 → OnRep)
- E4-T1:UFrameRCMaterialHelper BlueprintFunctionLibrary 與 RC 融合公式
- E4-T2:EMaterialState enum + FFrameMaterial 四組參數對應表
- E2-T1:鋼筋間距 4 級折減查表 UArchSimRebarChecker
- E3-T3:構件放置比對容差判斷(距離 <100mm + 角度 <15deg)

### Phase 2
- E1-T3:養護計時器 FTimerHandle 整合 + 1x/10x/100x 加速 toggle
- E1-T4:蜂窩弱點 15% 機率亂數生成(FRandomStream UPROPERTY SaveGame)
- E1-T5:UI 養護進度 Widget(剩餘時間浮空文字 + 半透明灰色材質切換)
- E2-T2:蜂窩 Decal 系統(位置記錄 FFrameWeaknessRecord + UDecalComponent 生成)
- E2-T3:養護完整性線性增長公式 + 每 30 秒 ApplyPatchAndResolve
- E2-T4:工法品質儀表板 HUD Widget(綠/黃/紅 三色即時顯示)
- E3-T1:GhostPreview 材質 MI_GhostPreview(unlit + emissive 邊緣亮藍)
- E3-T2:UArchSimTutorialProgress ActorComponent 進度 TMap 維護
- E3-T4:Tutorial/自由模式 toggle + 完成度 0-100% HUD 顯示
- E4-T3:視覺材質切換(4 個 MaterialInstance + 每階段 StaticMesh 切換)
- E4-T4:掃描儀整合(查看當前材料狀態 + 等效參數顯示 Widget)

### Phase 3
- E1-T6:SaveGame 序列化(EConstructionPhase + FRandomStream + CureElapsed)
- E2-T5:掃描儀蜂窩偵測互動(點擊構件觸發 UArchSimWarningSubsystem 警告)
- E3-T5:老師端即時啟用/關閉 Ghost RPC(多人模式 Server_ToggleGhost)
- E4-T5:E2 蜂窩折減與 E4 Composite 狀態整合(0.6 倍 E_eff patch)
- E1-T7:跨 Part C3 整合測試(跳過養護 → D/C 超標 → 觸發崩塌動畫入口)

### 實作順序
- E4-T1(RC 融合公式,E1/E2 都依賴)
- E4-T2(MaterialState enum + 參數表)
- E1-T1(StateMachine 骨架)
- E1-T2(Server RPC 鏈)
- E2-T1(RebarChecker)
- E1-T3(養護計時)
- E1-T4(蜂窩亂數)
- E4-T3(材質視覺切換)
- E1-T5(UI 進度顯示)
- E2-T2(Decal 系統)
- E2-T3(線性增長 patch)
- E2-T4(品質儀表板)
- E3-T1(Ghost 材質)
- E3-T2(TutorialProgress)
- E3-T3(容差比對)
- E3-T4(模式 toggle)
- E4-T4(掃描儀整合)
- E2-T5(掃描儀偵測)
- E3-T5(老師 RPC)
- E1-T6(SaveGame)
- E4-T5(蜂窩+Composite 整合)
- E1-T7(跨 C3 整合測試)

### 跨 Part 依賴
- Part A(A1 UArchSimMemberData + UArchSimModelRegistry 必須先存在,E1 狀態機掛在 Member Actor 上且所有 patch 走 Registry)
- Part A(A1 UFrameInteractiveSubsystem::ApplyPatchAndResolve 是 E1/E2/E4 材料 patch 的唯一出口)
- Part C3(崩塌動畫入口:E1 跳過養護 → D/C 超標後需呼叫 C3 DynCollapse 才能呈現 Productive Failure 完整閉環)
- Part D1(應力掃描儀 Actor:E2/E4 的蜂窩偵測與材料狀態查看需掃描儀互動模式,D1 提供 UI 框架)
- Part F2(關卡系統:E3 Ghost 藍圖屬於關卡資源,需 F2 的 Level DataAsset 載入機制)
- Part H(多人 LAN:E1 ReplicatedUsing 狀態同步、E3 老師 Ghost toggle 廣播依賴 H 的 Listen Server 架構就位)

### 風險清單
- 養護計時器與遊戲 Pause 不同步:UE FTimerManager 在 SetTimerPaused 時會凍結,但若玩家用 World.SetPause(true) 暫停遊戲則計時器也停,需確認遊戲是否允許暫停。若允許暫停應改用 ElapsedServerTime 累積而非純 Timer。
- 多人模式蜂窩亂數種子同步:FRandomStream 必須在 Server 端生成後透過 Replicated UPROPERTY 傳到 Client,不能讓 Client 各自生成(結果不一致)。務必在 PostBeginPlay 取得 Authority 後才呼叫 Initialize(Seed)。
- ApplyPatchAndResolve 高頻呼叫(養護每 30 秒一次 × N 個構件):若 N=50 個構件同時養護,30 秒內集中在同一幀觸發 50 次 patch 可能打爆 Woodbury rank。對策:在 UArchSimModelRegistry 層做 batch patch,一次打包所有材料變更為單一 FFrameModelPatch 再送出。
- EMaterialState 與 EConstructionPhase 兩個 enum 語意重疊容易混淆:E4 的 MaterialState 是材料屬性視角,E1 的 ConstructionPhase 是施工步驟視角。兩者需明確邊界:Phase 驅動 State 切換,State 決定 FrameCore 傳什麼參數。設計文件需明示對應關係,避免 bug 難以追蹤。
- Ghost 材質透明度在動態光源下過暗或過亮:主計畫書建議 unlit + emissive,但 unlit 材質在 Lumen 啟用時可能有 artifacts。建議在 Material Domain 設為 Surface + Shading Model Unlit,並在 Settings 加入透明度調整 slider。需在目標機器上實測。
- RC 融合公式 E_eff 數值:只有鋼筋階段若面積極小(A_eff << A_concrete),剛度矩陣條件數可能極差導致 FFrameSolveResult.bSingular=true。對策:設定最小有效面積下限 A_min=1e-4 m2,避免近奇異矩陣。
- UDecalComponent 在 UE5.7 Nanite mesh 上行為需驗證:Nanite 預設不支援 DBuffer Decal;若結構構件使用 Nanite 幾何,蜂窩 Decal 可能完全不顯示。對策:構件 Actor 的 StaticMeshComponent 設 r.Nanite.AllowMaskedMaterials=1 + 使用 DBuffer Decal blend mode。或關閉構件 Nanite 僅開地形 Nanite。
- SUQS 任務系統與 E3 進度追蹤整合:E3 的 UArchSimTutorialProgress 若要銜接 SUQS quest 完成觸發(關卡 F2 系統),需在 OnPlacementMatched 回呼中手動呼叫 USuqsProgression API,因為 SUQS 無法自動感知遊戲世界事件。

## 詳細擴充內容


# Part E 施工工序 — 詳細實作計畫

## 前置條件與設計原則

**FrameCore FROZEN 邊界**:`Plugins/FrameSolver/Source/FrameCore/` 引擎源碼永久凍結(v4.0.0 起),任何施工工序的材料強度邏輯、RC 融合計算、蜂窩折減,全部必須在 `Plugins/FrameSolver/Source/FrameCoreUE/`(UE consumer 側)或獨立的 ArchSim 業務層實作。FrameCore 只接收「材料參數改變」,不知道「為什麼改變」。

**施工工序在學習閉環的位置**:Part E 是「設計 → **施工** → 加載 → 崩塌 → 診斷 → 改」閉環的第二段。它的上游是 Part A(FrameCore 接合 + 構件 Actor 架構),下游是 Part C3(崩塌)。若 A1 尚未就緒,E 的所有 patch 無處可去;若 C3 尚未接通,Productive Failure 的學習閉環無法閉合。

**子節實作優先順序**:E4(RC 融合公式 — 其他三節的共用底層) → E1(狀態機骨架 — 驅動所有狀態轉換) → E2(品質因子 — 在 E1 之上疊加) → E3(引導式施工 — 純視覺引導,依賴最少)

---

## E1 工序狀態機(配筋→澆置→養護→拆模)

### 設計目標
讓 RC 構件的施工序列成為可玩的機制:不能跳步驟、有等待時間、有即時視覺反饋、多人下狀態同步。

### Sub-Task 清單

**E1-T1:UConstructionStateMachine ActorComponent 骨架與 enum 定義**
- **工時**:3 小時
- **涉及 UE5 class / API**:`UActorComponent`、`UPROPERTY(Replicated)`、`EConstructionPhase` enum class(Empty / RebarPlacing / ConcreteCasting / Curing / FormworkRemoval / Done)、`UFrameInteractiveSubsystem`(引用,尚不呼叫)
- **實作位置**:`ArchSim/Source/ArchSim/Construction/UConstructionStateMachine.h/.cpp`(新建,不在 FrameCore 路徑)
- **完成標準**:Component 可掛在任意 Actor;`GetCurrentPhase()` BP 可呼叫;enum 可在 BP 裡引用;UE Automation Test `ArchSim.Construction.StateMachineInit` 通過(初始狀態為 Empty)
- **預期踩雷**:enum class 在 Blueprint 要 `UENUM(BlueprintType)`;若忘記加會在 BP 中找不到值
- **依賴**:Part A1 UArchSimMemberData 已存在(掛載點)

**E1-T2:狀態轉換 RPC 鏈(Client 請求 → Server 驗證 → OnRep 視覺)**
- **工時**:4 小時
- **涉及 UE5 class / API**:`Server_RequestPhaseAdvance(EConstructionPhase NextPhase)` UFUNCTION(Server Reliable)、`UPROPERTY(ReplicatedUsing=OnRep_Phase)`、`GetLifetimeReplicatedProps`、`DOREPLIFETIME_CONDITION`
- **完成標準**:Client 呼叫請求函式後,Server 驗證「NextPhase == CurrentPhase+1」才接受;逆向請求被 Server 拒絕並 log Warning;OnRep 在所有 Client 正確觸發 `OnPhaseChanged` delegate;PIE 兩人測試狀態一致
- **預期踩雷**:Server RPC 函式名稱必須以 `Server_` 為前綴且 UFUNCTION 標注 `Server, Reliable`;若遺漏 Reliable 在封包丟失時狀態不同步
- **依賴**:E1-T1

**E1-T3:養護計時器 FTimerHandle + 1x/10x/100x 加速 toggle**
- **工時**:3 小時
- **涉及 UE5 class / API**:`FTimerHandle CureTimerHandle`、`GetWorld()->GetTimerManager().SetTimer(...)`、`GetWorld()->GetTimerManager().SetTimerPaused(...)`、`UGameUserSettings`(存加速設定)、`UPROPERTY(SaveGame) float CureElapsedSeconds`
- **完成標準**:進入 Curing 狀態時開始計時(預設 120 秒);Settings 中切換倍速後新計時器以對應速率推進;ElapsedSeconds 以加速後的遊戲時間計算;計時完成時自動允許進入 FormworkRemoval(但不自動進入,需玩家手動觸發)
- **預期踩雷**:若遊戲支援 Pause,`FTimerManager` 計時器也會停止,需確認與 Pause 的語意一致;若要讓養護在 Pause 時繼續,需改用 `GetWorld()->GetRealTimeSeconds()` 累積
- **依賴**:E1-T2

**E1-T4:蜂窩弱點 15% 亂數生成(FRandomStream UPROPERTY SaveGame)**
- **工時**:2 小時
- **涉及 UE5 class / API**:`FRandomStream`、`UPROPERTY(SaveGame) FRandomStream HoneycombStream`、`HoneycombStream.FRand() < 0.15`、`UPROPERTY(SaveGame) bool bHasHoneycomb`、`UPROPERTY(SaveGame) FVector HoneycombLocation`
- **完成標準**:進入 ConcreteCasting 狀態時在 Server 端判定;`bHasHoneycomb` 透過 Replicated 傳給 Client;存檔後重新載入的蜂窩位置與判定結果完全一致(FRandomStream seed reproducibility 驗證)
- **預期踩雷**:FRandomStream 必須在取得 Authority 後才 Initialize,不能在 Constructor 中呼叫(此時 Authority 未確定)
- **依賴**:E1-T2

**E1-T5:養護進度 UI Widget(浮空文字 + 材質切換)**
- **工時**:3 小時
- **涉及 UE5 class / API**:`UWidgetComponent`(WorldSpace)、`UTextBlock`("養護中... 剩 MM:SS")、`UMaterialInstanceDynamic`、`SetScalarParameterValue("Opacity", ...)`、`UMaterialInterface` 陣列(對應 5 個狀態的材質)
- **完成標準**:Curing 狀態時 Widget 出現在構件上方 50cm、顯示剩餘時間倒數;Done 後 Widget 隱藏;材質根據狀態切換(模板灰色半透明 → 養護中灰色半透明 → 完成正常材質);在 PIE 及 Standalone Client 均正確顯示
- **預期踩雷**:WorldSpace Widget 在多人模式需要 `bAlwaysRender=true` 否則遠處 Client 看不到;浮空文字 Z 向需偏移避免 Z-fighting
- **依賴**:E1-T3、E1-T4

**E1-T6:SaveGame 序列化(EConstructionPhase + FRandomStream + CureElapsed)**
- **工時**:2 小時
- **涉及 UE5 class / API**:`UPROPERTY(SaveGame)` 標記、SPUD plugin `ISpudStateful` 介面、`SpudShouldActorSave()` override
- **完成標準**:放置一個 RC 構件 → 進入 Curing → 存檔 → 退出 → 重新載入後構件仍在 Curing 狀態且計時器從存檔時間點繼續(不從 0 重算)
- **預期踩雷**:SPUD 對 `UPROPERTY(SaveGame)` 自動序列化,但 FTimerHandle 本身不可序列化(需存 float ElapsedSeconds 並在 PostLoad 重建 Timer)
- **依賴**:E1-T3、E1-T4

**E1-T7:跨 Part C3 整合測試(跳過養護 → D/C 超標 → 崩塌)**
- **工時**:3 小時
- **涉及 UE5 class / API**:`UFrameInteractiveSubsystem::ApplyPatchAndResolve`、`FFrameSolveResult.bSingular`、`DemandSummary.GoverningMemberIdx`、C3 DynCollapse 入口(待 C3 實作後對接)
- **完成標準**:建立簡支梁 → 跳過養護強行加載 → FrameCore Solve 回傳 D/C > 1.0 → 觸發 `OnStructureFailed` 事件;事件有 `FVector FailureLocation` 可傳給 C3(此 Task 驗收時 C3 可以是 stub)
- **預期踩雷**:若 C3 尚未實作,整合測試只驗證到 D/C 超標事件觸發,崩塌動畫留 stub
- **依賴**:E1-T5、Part A1 已完成

---

## E2 工法品質(鋼筋間距 / 蜂窩 / 養護完整性)

### 設計目標
將工地的「品質因子」轉化為 FrameCore 材料參數的折減係數。讓學生直接感受「施工品質差 → 結構變弱」的因果關係。

### Sub-Task 清單

**E2-T1:UArchSimRebarChecker — 鋼筋間距 4 級折減查表**
- **工時**:2 小時
- **涉及 UE5 class / API**:`UBlueprintFunctionLibrary`(新建 `UArchSimRebarChecker`)、`UFUNCTION(BlueprintCallable)` `CheckSpacing(float SpacingMM) -> float ReductionFactor`
- **實作邏輯**:查表 `SpacingMM <= 200 → 1.0 / ≤250 → 0.85 / ≤300 → 0.70 / >300 → 0.50`;同時發送 `UArchSimWarningSubsystem::PushWarning(W-02, ...)`
- **完成標準**:Unit Test `ArchSim.Construction.RebarSpacing` 覆蓋 4 個臨界值(200 / 250 / 300 / 350mm);BP 可直接呼叫;折減因子能在 E1-T2 狀態切換時被呼叫並送入 E4 RC 融合管道
- **預期踩雷**:目前 `UArchSimWarningSubsystem` 是否已存在需確認;若不存在需在 E2-T1 中先 stub 或等 Part D1 實作後接
- **依賴**:E4-T1(折減因子送到 RC 融合計算)

**E2-T2:蜂窩 Decal 系統(FFrameWeaknessRecord + UDecalComponent)**
- **工時**:3 小時
- **涉及 UE5 class / API**:`USTRUCT() FFrameWeaknessRecord { int32 MemberIdx; float WeaknessFactor; FVector WorldLocation; }`、`UDecalComponent`、`UMaterialInterface MI_HoneycombCrack`(需製作)、`DBuffer Decal` blend mode
- **完成標準**:蜂窩生成時在 `HoneycombLocation` 處 Spawn `UDecalComponent`;Decal 顯示裂縫紋路;SaveGame 後重新載入 Decal 正確復原;非 Curing 以後狀態的構件不顯示 Decal
- **預期踩雷**:`UDecalComponent` 在 Nanite mesh 上需 `r.AllowDecalsWithoutDBuffer=0` 確認;若構件使用 Nanite 可能需要 fallback 方案(使用 Overlay Material 替代 Decal)
- **依賴**:E1-T4(蜂窩 bHasHoneycomb 由 E1 產生)

**E2-T3:養護完整性線性增長公式 + 批次 ApplyPatchAndResolve**
- **工時**:4 小時
- **涉及 UE5 class / API**:`UFrameInteractiveSubsystem::ApplyPatchAndResolve`、`FFrameModelPatch`(材料 patch 欄位)、`UArchSimModelRegistry::BatchMaterialPatch(TArray<FMaterialPatchEntry>)`(需新增 helper)
- **實作邏輯**:`E_eff(t) = E_full * (0.3 + 0.7 * (t / cureTime))` 每 30 遊戲秒計算一次;收集同一幀所有在 Curing 中的構件材料更新,打包成單一 FFrameModelPatch 送出(避免逐個觸發 Woodbury)
- **完成標準**:50 個構件同時養護時,每 30 秒觸發一次 Solve(不是 50 次);E_eff 值在 30% 到 100% 之間線性增長;FrameCore Solve 結果 D/C 值隨 E_eff 增長單調遞減(養護越完整 D/C 越小)
- **預期踩雷**:BatchMaterialPatch 是計畫書中未明確列出的 helper,需在 UArchSimModelRegistry 新增;確認 FFrameModelPatch 的材料 patch 語義是否支援批次多構件更新(需查看 FrameCore UE API 文件)
- **依賴**:E1-T3、E4-T1

**E2-T4:工法品質儀表板 HUD Widget**
- **工時**:3 小時
- **涉及 UE5 class / API**:`UUserWidget`("WBP_QualityDashboard")、`UProgressBar`、`UImage`(綠/黃/紅 Icon)、`UTextBlock`(說明文字)、`UArchSimHUDSubsystem::PushQualityUpdate(...)`(需新增)
- **完成標準**:點擊已放置 RC 構件時,右上角出現品質儀表板顯示:鋼筋間距等級(4 級)、蜂窩狀態(有/無)、養護完整度(百分比);三色依等級:≥0.85 → 綠、≥0.70 → 黃、<0.70 → 紅;儀表板隨狀態切換即時更新
- **預期踩雷**:多人模式下,儀表板顯示的是「目前被選取的構件」的品質,需確認 Client 端選取狀態與 Server 端資料同步;避免顯示舊快取
- **依賴**:E2-T1、E2-T2、E2-T3

**E2-T5:掃描儀蜂窩偵測互動(D1 整合點)**
- **工時**:3 小時
- **涉及 UE5 class / API**:`LineTrace`(Trace Channel `ECC_ConstructionScan`)、`UArchSimWarningSubsystem::PushWarning`、`FHitResult`、`UConstructionStateMachine::GetHoneycombInfo()`
- **完成標準**:玩家在掃描儀模式下(D1)朝構件瞄準並按掃描鍵 → 如果有蜂窩顯示 "混凝土澆置缺陷,強度折減 40%";沒有蜂窩顯示 "未偵測到異常";說明文字出現後 3 秒自動消失
- **預期踩雷**:此 Task 依賴 D1 掃描儀模式的 UI 框架;若 D1 尚未完成,可以 stub 成 "按 F 鍵掃描" 的臨時交互
- **依賴**:E2-T2、Part D1(掃描儀 Actor,可 stub)

---

## E3 引導式施工(全息投影預覽)

### 設計目標
讓老師預先放置的「目標藍圖」以半透明 Ghost 形式引導初學者施工。Ghost 是純視覺層,不進 FrameCore。

### Sub-Task 清單

**E3-T1:GhostPreview 材質 MI_GhostPreview(unlit emissive)**
- **工時**:2 小時
- **涉及 UE5 class / API**:`UMaterial`(Domain=Surface, Shading Model=Unlit)、`Emissive Color` 節點、`Opacity` 可調 scalar parameter、`UMaterialInstanceConstant MI_GhostBlue / MI_GhostGreen`
- **完成標準**:MI_GhostBlue 顯示半透明亮藍(邊緣稍亮);MI_GhostGreen 顯示半透明亮綠(玩家放對後切換);在 Lumen 開啟的場景下不產生異常 artifacts;Translucency Sort Priority 設定避免 Z-fighting
- **預期踩雷**:Unlit 材質在 UE5.7 Lumen 下仍會受 Sky Light 影響(若 Material 使用 Emissive Only 模式應可規避);需在目標機器實測
- **依賴**:無

**E3-T2:UArchSimTutorialProgress ActorComponent — 目標構件 TMap 維護**
- **工時**:3 小時
- **涉及 UE5 class / API**:`UActorComponent`(掛在關卡專用 Manager Actor 上)、`TMap<FIntVector, bool> GoalGrid`(格子索引 → 是否已放置)、`UPROPERTY(Replicated) int32 CompletedCount`、`OnPlacementMatched` multicast delegate
- **完成標準**:進入引導關卡時從 Level DataAsset 載入目標格子清單;玩家每放一個構件呼叫 `TryMatchPlacement(FIntVector GridPos, FRotator Rot)`;比對容差通過時 `GoalGrid[GridPos]=true` + 廣播 delegate;完成度 = `CompletedCount / GoalGrid.Num()`
- **預期踩雷**:FIntVector 作為 TMap Key 需確認 GetTypeHash 實作是否在 UE5.7 中已有;若無需自訂 hash function
- **依賴**:Part F2(Level DataAsset 關卡結構,可先用硬碼 TArray 替代)

**E3-T3:構件放置比對容差判斷(距離 <100mm + 角度 <15°)**
- **工時**:2 小時
- **涉及 UE5 class / API**:`FVector::Dist(ActualPos, GoalPos) < 100.0f`(cm 單位,FrameCore 用 mm,此處 UE 座標用 cm → 100cm = 1m,需確認容差單位;計畫書說 100mm 即 10 cm)、`FQuat::AngularDistance(ActualRot, GoalRot)`、`UFUNCTION(BlueprintCallable)`
- **完成標準**:位置誤差 < 10cm(UE 座標)且旋轉誤差 < 15° 時判定為「放對」;提供 BP 可調 `float PositionToleranceCm = 10.0f` 與 `float AngularToleranceDeg = 15.0f` UPROPERTY
- **預期踩雷**:UE 的 FVector 單位是 cm,計畫書寫 100mm = 10cm,需統一單位轉換。此外 FQuat::AngularDistance 返回弧度,需轉換為角度再比對
- **依賴**:E3-T2

**E3-T4:Tutorial/自由模式 toggle + 完成度 HUD**
- **工時**:2 小時
- **涉及 UE5 class / API**:`UPROPERTY(Replicated) bool bGhostVisible`、`StaticMeshComponent::SetVisibility(bGhostVisible)`、`UUserWidget "WBP_TutorialProgress"`、`UProgressBar`、`UTextBlock`("完成度 67%")
- **完成標準**:Tutorial 模式下所有 Ghost Actor 可見;自由模式下隱藏;右下角 HUD 顯示完成度百分比;多人下所有 Client 看到同一 Ghost 狀態(透過 `bGhostVisible` Replicated)
- **預期踩雷**:Ghost Actor 的 SetVisibility 需在 Server 端呼叫後透過 Replication 傳到 Client;若直接在 Client SetVisibility 則只有本地生效
- **依賴**:E3-T1、E3-T2

**E3-T5:老師端即時啟用/關閉 Ghost RPC(多人)**
- **工時**:3 小時
- **涉及 UE5 class / API**:`UFUNCTION(Server, Reliable) Server_ToggleGhost(bool bEnable)`、Teacher Role 判斷(`AArchSimPlayerState::bIsTeacher`)、`NetMulticast Reliable Multicast_BroadcastGhostState(bool bEnable)`
- **完成標準**:老師端(bIsTeacher=true)按鍵 → Server RPC → Multicast 廣播所有 Client Ghost 狀態改變;學生無法呼叫 Server_ToggleGhost(Server 端驗證 bIsTeacher);老師 Client 本地預覽立即更新
- **預期踩雷**:Teacher Role 需要 Part H 多人架構中定義;Phase 2 前可用 `bOverrideAsTeacher` debug flag 測試
- **依賴**:E3-T4、Part H(多人架構,可 stub)

---

## E4 材料狀態機(素材→鋼筋→澆置→複合)

### 設計目標
提供 RC 材料屬性隨施工階段變化的底層計算層,是 E1/E2 所有 FrameCore patch 的統一出口。嚴守 FROZEN 鐵則,FrameCore 引擎只見到材料參數,不知道 RC 概念。

### Sub-Task 清單

**E4-T1:UFrameRCMaterialHelper BlueprintFunctionLibrary — RC 融合公式**
- **工時**:4 小時
- **涉及 UE5 class / API**:`UBlueprintFunctionLibrary`(新建 `UFrameRCMaterialHelper` in `FrameCoreUE` 或 `ArchSim/Construction/`)、`FFrameMaterial`(現有 USTRUCT)、`UFUNCTION(BlueprintCallable, BlueprintPure)`
- **實作函式**:
  - `ComputeRCComposite(FFrameMaterial Rebar, FFrameMaterial Concrete, float RebarFraction, float ReductionFactor) -> FFrameMaterial`
  - 公式:`E_eff = comp_boost(1.1) * (E_steel * RebarFraction + E_concrete * (1-RebarFraction))`
  - `Fy_eff = Fy_rebar * phi_tens(0.8) * RebarFraction`
  - `Fc_eff = Fc_concrete * phi_shear(0.6)`(保守)
  - 設定最小有效 E_eff 下限防止奇異矩陣
- **完成標準**:Unit Test `ArchSim.Construction.RCMaterialHelper` 驗證:純鋼筋(RebarFraction=1) → E_eff ≈ 220 GPa;純混凝土(RebarFraction=0) → E_eff ≈ 26.4 GPa;典型 RC(RebarFraction=0.02) → E_eff 介於兩者;Fy_eff / Fc_eff 公式正確;E_eff >= E_min 保護
- **預期踩雷**:FFrameMaterial 的欄位定義需查 FrameCoreUE 現有 USTRUCT;若沒有 Fc 欄位需在 ArchSim 業務層用 wrapper struct;不能修改 FrameCore engine 的 struct 定義(FROZEN)
- **依賴**:無(此為底層 helper)

**E4-T2:EMaterialState enum + 四組 FFrameMaterial 參數對應表**
- **工時**:2 小時
- **涉及 UE5 class / API**:`UENUM(BlueprintType) EMaterialState { RebarOnly, GreenConcrete, Cured, Composite }`、`TMap<EMaterialState, FRCMaterialParams> MaterialParamTable`(設計時資料表)、`UDataTable`(可選:讓老師在 Editor 調整參數)
- **參數規格**:
  - RebarOnly: E=200 GPa, A_eff=最小,無混凝土貢獻
  - GreenConcrete: E_eff=15 GPa, Fc=21 MPa(30% 強度)
  - Cured: E_eff=24 GPa, Fc=24 MPa(100% 強度)
  - Composite: 由 UFrameRCMaterialHelper::ComputeRCComposite 動態計算
- **完成標準**:DataTable 可在 Editor 中看到 4 行;BP 可查詢任意狀態對應的材料參數;數值符合台灣 RC 規範教育用簡化版說明(文件中標記「教育用簡化模型」)
- **預期踩雷**:DataTable Row Struct 需要 UScriptStruct 繼承;若使用 FTableRowBase 需確認在 UE5.7 中序列化正確
- **依賴**:E4-T1

**E4-T3:視覺材質切換(4 個 MaterialInstance + StaticMeshComponent 切換)**
- **工時**:3 小時
- **涉及 UE5 class / API**:`UStaticMeshComponent::SetMaterial(int32, UMaterialInterface*)`、`TArray<UMaterialInterface*> PhaseMaterials`(UPROPERTY, EditDefaultsOnly)、`UMaterialInstanceDynamic` 透明度控制
- **材質清單**:M_RebarOnly(鋼筋網格半透明)、M_GreenConcrete(淺灰半透明)、M_Curing(灰色半透明 + 脈衝動態 emissive)、M_Done(正常混凝土材質)
- **完成標準**:狀態切換時 Material 在 0.3 秒內平滑過渡(UMaterialInstanceDynamic Lerp)或立即切換(可選);所有 Client 看到同一材質狀態(材質切換走 EMaterialState Replication 觸發);PIE 中驗證 4 個狀態均正確
- **預期踩雷**:若使用 Dynamic Material Instance,需在 BeginPlay 創建而非在 Constructor(否則 CDO 不正確);多人下 SetMaterial 只在有 Authority 的 Server 端有效,需在 OnRep 中呼叫
- **依賴**:E4-T2、E1-T1

**E4-T4:掃描儀整合 — 材料狀態 + 等效參數查看 Widget**
- **工時**:2 小時
- **涉及 UE5 class / API**:`UUserWidget "WBP_MaterialInspector"`、`UTextBlock`(顯示狀態名稱 + E_eff + Fy_eff 數值)、`UConstructionStateMachine::GetCurrentMaterialParams() -> FRCMaterialParams`
- **完成標準**:玩家在掃描儀模式下選取 RC 構件,HUD 顯示:「當前狀態:養護中 / E_eff:19.2 GPa / Fy_eff:336 MPa / 養護完整度:60%」;數值隨養護進度即時更新;非 RC 構件顯示基本材料屬性
- **預期踩雷**:GPa 數值格式化需注意 UE 的 FText::Format 精度;建議用 `FText::FromString(FString::Printf(...))` 控制小數位
- **依賴**:E4-T2、E4-T3、E2-T3(養護完整度)

**E4-T5:E2 蜂窩折減與 E4 Composite 狀態整合**
- **工時**:2 小時
- **涉及 UE5 class / API**:`UFrameRCMaterialHelper::ApplyHoneycombReduction(FFrameMaterial, float Factor=0.6) -> FFrameMaterial`、`UArchSimModelRegistry::BatchMaterialPatch`
- **實作邏輯**:Composite 狀態計算時,若 bHasHoneycomb=true,最終 E_eff *= 0.6、Fc_eff *= 0.5;折減應用在 RC 融合計算之後
- **完成標準**:同一構件 Composite + 無蜂窩 vs Composite + 有蜂窩,後者 FrameCore Solve 的 D/C 明顯偏高(驗收:簡支梁中央 D/C 差距 ≥ 30%);折減後參數被正確送入 FFrameModelPatch
- **預期踩雷**:蜂窩折減應該在 Composite 狀態才生效(RebarOnly 或 GreenConcrete 時蜂窩已是缺陷但折減公式不同);確認各狀態的折減邏輯不混用
- **依賴**:E4-T1、E1-T4、E2-T2

---

## Part E 整體層級資訊

### 總工時估算

| 子節 | Sub-Task 數 | 總工時(小時) |
|------|------------|------------|
| E1 工序狀態機 | 7 個 | 20h |
| E2 工法品質 | 5 個 | 15h |
| E3 引導式施工 | 5 個 | 12h |
| E4 材料狀態機 | 5 個 | 13h |
| **整合測試 buffer** | — | 10h |
| **整體暫存** | — | 26h |
| **合計** | **22 個** | **96h** |

備註:工時估算基於 1-2 人小團隊、熟悉 UE5 C++ 但非資深多人網路工程師。E1-T2(多人 RPC)與 E3-T5(老師 RPC)因多人網路複雜度有 50% buffer 已含入 4h 估算中。

### MVP 必須 / Phase 2 / Phase 3 分級摘要

| Phase | Task | 理由 |
|-------|------|------|
| **MVP(Phase 1)** | E4-T1、E4-T2、E1-T1、E1-T2、E2-T1、E3-T3 | 底層公式與狀態機骨架是其他一切的基礎;RebarChecker 和容差判斷是最薄的可驗收 slice |
| **Phase 2** | E1-T3到T5、E2-T2到T4、E3-T1、T2、T4、E4-T3、T4 | 視覺反饋、養護計時、Decal 系統、Ghost 預覽、掃描儀整合 |
| **Phase 3** | E1-T6、T7、E2-T5、E3-T5、E4-T5 | SaveGame、跨 C3 整合、多人老師 RPC、蜂窩+Composite 細緻整合 |

### 建議實作內部順序

1. **E4-T1** → E4-T2(RC 融合公式,其他全依賴)
2. **E1-T1** → E1-T2(狀態機骨架 + RPC 鏈)
3. **E2-T1**(RebarChecker,可平行於 E1)
4. E1-T3(養護計時) → E1-T4(蜂窩亂數) → E4-T3(材質視覺)
5. E1-T5(養護 UI) → E2-T2(Decal) → E2-T3(線性增長 patch)
6. E2-T4(品質儀表板) → E3-T1(Ghost 材質) → E3-T2(TutorialProgress)
7. **E3-T3**(容差判斷) → E3-T4(模式 toggle)
8. E4-T4(掃描儀整合) → E2-T5(掃描儀偵測)
9. E3-T5(老師 RPC,依賴多人) → E1-T6(SaveGame) → E4-T5(蜂窩整合)
10. **E1-T7**(跨 C3 整合測試,Phase 3 末)

### 跨 Part 依賴(必須先完成才能開始)

- **Part A1 必須先完成**:`UArchSimMemberData`、`UArchSimModelRegistry`、`UFrameInteractiveSubsystem` 已可用,E1/E2/E4 的所有 patch 才有落點。估計 Part A1 需要 2-3 週先行。
- **Part C3 stub 建議同步**:E1-T7 的整合測試需要 C3 入口;若 C3 尚未實作可以 stub 為 `BP_OnStructureFailed` 事件記錄到 Output Log。
- **Part D1 掃描儀 stub 建議**:E2-T5 和 E4-T4 的掃描儀整合可先 stub(按 F 鍵觸發)。
- **Part H 多人架構**:E1-T2(RPC)和 E3-T5(老師 toggle)只有在 Part H Listen Server 架構就位後才能做完整多人測試;MVP 階段可在 PIE 單機模式驗收,多人驗收推到 Phase 2。
- **Part F2 關卡系統**:E3 的 Ghost 藍圖需要從 Level DataAsset 載入目標構件清單;MVP 可先用硬碼 TArray。

### 降級方案(若排程壓縮)

| 降級對象 | 降級方案 |
|---------|---------|
| 養護計時(2 分鐘) | 預設跳過養護,只保留「施工完成」按鈕;養護等待機制推到 Phase 2 |
| 多人狀態同步(E1-T2 RPC) | MVP 先做單人版,狀態不 Replicate,多人同步推 Phase 2 |
| Ghost 系統(E3 全節) | 引導式施工不做,Tutorial 改用 HUD 文字提示步驟(大幅降低工時) |
| 蜂窩 Decal(E2-T2) | 蜂窩生成但不顯示 Decal,只在 HUD 文字提示「本構件有缺陷」 |


---


# Part F 實作擴充(178h)

**標題**:Part F — 沙盒 + 關卡系統

**子節數**:5

### MVP 必須(Phase 1)
- F1-T01: 沙盒基地模板 DataAsset 系統
- F1-T02: 沙盒遊戲模式與 SaveGame 自動儲存
- F1-T03: 沙盒 HUD(建造/分析/分享/離開)
- F2-T01: SUQS DataTable DT_ArchSimLevels 建立與 import
- F2-T02: 關卡選擇地圖 UI(UMG LevelSelectWidget)
- F2-T03: 業務層數值條件判斷 Subsystem
- F2-T04: 關卡完成結算頁與解鎖邏輯
- F3-T01: 閉環進度 HUD(6 階段圓圈)
- F3-T02: 設計→施工→加載三階段狀態機
- F3-T03: L-01 懸臂梁完整閉環實作
- F4-T01: 失敗事件偵測與友善語氣 UI
- F4-T02: 最早失效構件高亮(OnEventReached 接合)
- F5-T01: UArchSimWarningSubsystem 基礎警告骨架
- F5-T02: DA_WarningTemplates 10 種警告中文模板
- F5-T03: 警告觸發條件對應 FrameCore 結果欄位

### Phase 2
- F1-T04: 作品匯出 .archsim_bp 格式
- F1-T05: 多人沙盒整合(Part H 前置鉤點)
- F2-T05: 關卡 DataAsset 進階欄位(預設結構/限制材料)
- F2-T06: SUQS 多人進度同步(PlayerState replicated)
- F3-T04: L-02 簡支梁 + L-03 門型框架完整閉環
- F3-T05: 通過者診斷模式深入學習路徑
- F4-T03: 漸退式鷹架提示系統(第 3 次失敗觸發)
- F4-T04: 失敗模式分類(bending/shear/buckling/instability)
- F4-T05: 觀察時間 xAPI 記錄(result.extensions.observation-duration-sec)
- F5-T04: 規範對標按鈕進階模式(條文摘要 UI)
- F5-T05: 警告日誌面板(可回查歷史)

### Phase 3
- F1-T06: 5 種基地模板 PCG 地形整合(Part A3 依賴)
- F2-T07: 15+ 主線關卡(L-04 ~ L-15)
- F3-T06: 閉環跨課程連結標籤(對應課綱單元)
- F4-T06: 最佳設計對照圖(老師預填 DataAsset)
- F5-T06: 多語在地化警告文本(英文版)
- F5-T07: 強柱弱梁(W-06)精確判定(需 NM 互動, 須驗證)

### 實作順序
- F2-T01 (SUQS DataTable, 無依賴可先行)
- F5-T01 (警告 Subsystem 骨架, 無依賴可先行)
- F5-T02 (10 種警告模板)
- F1-T01 (基地 DataAsset)
- F1-T02 (沙盒遊戲模式 + SaveGame)
- F1-T03 (沙盒 HUD)
- F3-T01 (閉環進度 HUD)
- F3-T02 (設計→施工→加載狀態機)
- F2-T02 (關卡選擇地圖 UI)
- F2-T03 (數值條件判斷 Subsystem)
- F2-T04 (結算頁與解鎖)
- F3-T03 (L-01 完整閉環)
- F4-T01 (失敗事件偵測)
- F4-T02 (最早失效構件高亮)
- F5-T03 (警告觸發條件)

### 跨 Part 依賴
- Part A (A1 UArchSimModelRegistry + A3 世界系統) 必須先完成才能開始 F1-T01 ~ F1-T02
- Part B (B1 構件放置系統) 必須先完成才能測試沙盒模式放置流程
- Part C (C1 載重施加 + C2 D/C 熱圖) 必須先完成才能實作 F3 學習閉環加載階段
- Part C (C3 崩塌動畫) 必須先完成才能實作 F4-T02 最早失效構件高亮
- Part E (施工工序狀態機) 必須先完成才能讓 F3 閉環的施工階段有內容
- Part H (多人 Listen Server) 必須先完成才能整合 F1-T05 多人沙盒
- Part I (xAPI log 系統) 必須先完成才能實作 F4-T05 觀察時間記錄

### 風險清單
- SUQS DataTable 跨 UE minor 版本升級時 row struct BREAKING — 建議版控 quest JSON 並在升級後立即 reimport + 回歸測試
- F4 失敗模式分類依賴 FFrameSolveResult.Utilization.Mode,但該欄位目前是 max(N/Mz/My/V) dominant 模式,對屈曲(buckling) 和機構不穩定(instability, bSingular=true)判斷不夠精確 — 需在 UE consumer 層補充判斷邏輯
- F5 強柱弱梁(W-06)精確判定需要 NM 互動曲線計算,FrameCore 有 S10 N-M 互動但 MVP 教育用簡化版應以 D/C 比較為主要依據 — MVP 標注為教育簡化版
- 沙盒模式玩家蓋出 1000+ 構件導致 Solve 卡頓 — 需在 Server side 強制上限(預設 500)並在 UE 端設 debounce 150ms 避免 Woodbury rank 爆炸
- SUQS 沒有 built-in replication,多人模式下任務進度不同步 — Phase 2 才需處理,MVP 可先做單人關卡
- 警告系統若觸發過於頻繁(每次 Solve 都冒 toast)會讓學生麻木 — 需要嚴重度排序邏輯,同類型警告 30 秒內不重複顯示
- 鷹架提示(F3-T 預設第 3 次失敗觸發)的時機判斷需要 session 層追蹤重試次數,與 xAPI log 系統整合才能正確計數

## 詳細擴充內容


# Part F — 沙盒 + 關卡系統:詳細實作計畫

> 本文件是 `ARCHITECT_SIM_MASTER_PLAN.md` 第八章的展開版。所有 FrameCore 相關操作均在 `FrameCoreUE` consumer 層實作,不修改 `Plugins/FrameSolver/Source/FrameCore/` 下的引擎源碼(鐵則 #1,FROZEN since v4.0.0)。

---

## 整體定位與跨 Part 依賴

Part F 是整個學習閉環的「容器層」——它把 Part A(引擎接合)、Part B(玩家操作)、Part C(工程模擬)、Part D(診斷工具)、Part E(施工工序)的功能組裝成玩家可以體驗的完整場景模式。**Part F 本身不做計算,只做狀態機、UI 流程與教育體驗設計。**

必須先完成的前置 Part:
- **Part A1** — `UArchSimModelRegistry` + `UArchSimMemberData`(構件 → FrameCore 索引映射)
- **Part B1** — 構件放置系統(沙盒模式的核心玩法)
- **Part C1/C2** — 載重施加 + D/C 熱圖(閉環加載/診斷階段必須)

可以平行開發(F2 任務定義、F5 警告模板屬純資料/UI,無引擎依賴):
- `SUQS DataTable DT_ArchSimLevels`、`DA_WarningTemplates` 可在引擎功能完成前先寫資料

---

## F1 沙盒模式定義

### 設計原則

沙盒模式的核心教育哲學來自 Papert constructionism:學生在「沒有外在評分壓力」的環境中製作有個人意義的人工物件。UI 設計最小干預——只在真正致命問題(如結構完全崩潰)時介入。

### 子任務清單

**F1-T01: 基地模板 DataAsset 系統**
- 說明:新建 `UDA_BaseSiteTemplate : UDataAsset`,欄位包含 `SiteName`(FText)、`PreviewThumbnail`(UTexture2D)、`TerrainLevelToLoad`(FSoftObjectPath,指向 UE SubLevel)、`MaxMembers`(int32,預設 500)、`StartingBoundingBox`(FBox,cm 為單位)。建立 5 個 DataAsset 實例:平地/緩坡/山坡/既有建築旁/河岸。
- 涉及 API:`UDataAsset`、`UPrimaryDataAsset`(若需 Asset Manager 查詢)、`FSoftObjectPath`
- 工時估算:4 小時
- 完成標準:Editor 中 5 個 DataAsset 可被 Content Browser 顯示;Blueprint 中可讀取 `SiteName` 和 `MaxMembers`;不需要引擎功能(純資料層)
- 預期踩雷:若使用 `UPrimaryDataAsset` 需要在 `DefaultGame.ini` 的 `[/Script/Engine.AssetManagerSettings]` 加對應 `PrimaryAssetTypesToScan`,否則 `AssetManager::GetPrimaryAssetData` 回空
- 依賴:無(可最早開工)
- Phase:MVP

**F1-T02: 沙盒遊戲模式與 SaveGame 自動儲存**
- 說明:新建 `AArchSimSandboxGameMode : AGameModeBase`,在 `BeginPlay` 時讀取選定的 `UDA_BaseSiteTemplate` 載入對應 SubLevel。新建 `UArchSimSandboxSaveGame : USaveGame`,標記 `UPROPERTY(SaveGame)` 欄位:`FFrameModelDef SavedModel`(構件輸入端)、`FVector PlayerStartPosition`、`TArray<FTransform> MemberTransforms`。設定 60 秒 `FTimerHandle` 定時呼叫 `UGameplayStatics::AsyncSaveGameToSlot`。
- 涉及 API:`USaveGame`、`UGameplayStatics::AsyncSaveGameToSlot/AsyncLoadGameFromSlot`、`FTimerManager::SetTimer`、`ULevelStreamingDynamic::LoadLevelInstance`
- 工時估算:4 小時
- 完成標準:進入沙盒後 60 秒自動儲存(Console 可見 log 訊息);結束後重新開啟,構件位置與材料恢復一致;`FFrameModelDef` round-trip 後 FrameCore SolveLinear 結果與儲存前差 < 1e-5
- 預期踩雷:`FFrameModelDef` 內含 `TArray<FFrameMaterial>` 等,若有指標欄位需確保純 POD 序列化;SPUD 也可用於 Actor 狀態但 Part F MVP 先用輕量 `USaveGame` 避免 SPUD World Partition issue #117
- 依賴:Part A1(`UArchSimModelRegistry` 提供 `FFrameModelDef` 快照 API)
- Phase:MVP

**F1-T03: 沙盒 HUD(建造/分析/分享/離開)**
- 說明:新建 UMG Widget `UArchSimSandboxHUD`。HUD 頂列只有四個按鈕:「建造工具」「分析工具」「分享」「離開」。建造工具開啟 Part B1 構件面板;分析工具開啟 Part D1 熱圖/掃描儀面板;分享呼叫 `F1-T04` 的匯出功能(Phase 2);離開顯示確認對話框後回主選單。沙盒模式無目標面板、無計分、無計時器。
- 涉及 API:`UUserWidget`、`UButton`、`UVerticalBox`、Enhanced Input `IMC` 切換(建造模式進入時 AddMappingContext)
- 工時估算:3 小時
- 完成標準:PIE 中 HUD 正確顯示 4 個按鈕;點「建造工具」成功切換 Input MappingContext 到 `MC_Building`;點「分析工具」開啟熱圖 Widget;離開按鈕顯示確認對話框後 `APlayerController::ClientTravel` 到主選單 Level
- 預期踩雷:Enhanced Input 在沙盒模式切換 MappingContext 時若 PlayerController 尚未初始化(`OnPossess` 前)會失敗;對策:在 `BeginPlay` 後 1 frame 延遲再 AddMappingContext(UE5.6 已知 bug,5.7 需實測)
- 依賴:F1-T02(GameMode 正確初始化後 HUD 才能繫結)
- Phase:MVP

**F1-T04: 作品匯出 .archsim_bp 格式**
- 說明:新建 `UArchSimBlueprintExporter : UBlueprintFunctionLibrary`,`ExportToArchSimBP(FFrameModelDef Model, FString FileName)` 函數。JSON schema:`{ "version": "1.0", "name": "...", "thumbnail_base64": "...", "frameModelDef": {...} }`。縮圖用 `USceneCaptureComponent2D` 從上方 30° 角拍 256×256 PNG 後 base64 encode。存至 `FPaths::ProjectSavedDir()` 下。
- 涉及 API:`FJsonObjectConverter::UStructToJsonObject`、`USceneCaptureComponent2D`、`IImageWrapperModule`(PNG encode)、`FBase64::Encode`
- 工時估算:4 小時
- 完成標準:點「分享」→「匯出」後,在 Saved 目錄產生 `.archsim_bp` 檔案;用 JSON Editor 可讀取且欄位完整;在另一台機器用 `LoadModelFromJson`(Part A1 依賴)可重建相同結構並 Solve 得相同 D/C 結果
- 預期踩雷:`FJsonObjectConverter` 對 `TArray<FVector>` 內的 float 精度保留 6 位小數,可能導致 round-trip 後微小誤差;需特別處理節點座標(建議 8 位小數)
- 依賴:F1-T02、F1-T03
- Phase:Phase 2

**F1-T05: 多人沙盒整合(Part H 前置鉤點)**
- 說明:在 `AArchSimSandboxGameMode` 的構件放置路徑上加入 Server RPC 路徑(沙盒模式預設開放,但在 Part H 完成後才能真正測試多人)。確保 `UArchSimModelRegistry` 的 `ApplyPatchAndResolve` 只在 `HasAuthority()` 時執行;結果透過 GameState 的 `Replicated UPROPERTY` 廣播。
- 涉及 API:`UPROPERTY(Replicated)`、`GetLifetimeReplicatedProps`、`DOREPLIFETIME`、`UFUNCTION(Server, Reliable)`
- 工時估算:3 小時
- 完成標準:PIE Listen Server 模式(2 clients)下,一個玩家放置構件後另一個玩家看到熱圖更新;FrameCore Solve 只在 Server 執行一次(Log 確認)
- 預期踩雷:GameState replicated 的 `TArray` 如果很大(FFrameSolveResult 摘要)會超過 UE 單封包大小;對策:只複製 `MemberUtilization` 和 `bSingular`,完整結果用 Server RPC 按需請求
- 依賴:Part H1(Listen Server 基礎建設)
- Phase:Phase 2

---

## F2 關卡系統

### 設計原則

關卡系統的核心是「清楚目標 + 即時回饋 + 漸進難度」(Csíkszentmihályi Flow Theory)。SUQS 負責關卡狀態機,業務層負責數值條件判斷——這個分工讓 quest JSON 保持簡單乾淨。

### 子任務清單

**F2-T01: SUQS DataTable DT_ArchSimLevels 建立與 import**
- 說明:安裝 SUQS plugin(MIT 授權,UE5.7 相容)。建立 `DT_ArchSimLevels : UDataTable`,row struct 對應 SUQS 的 `FSuqsQuestRow`(需驗證 SUQS 確切 row struct 名稱,若不一致則需 wrapper struct)。建立 3 個 MVP 關卡 JSON:L-01 懸臂梁/L-02 簡支梁/L-03 門型框架。每個 quest 含 2-3 個 Objective,每個 Objective 含 1-2 個 Task。Objective 之間依序解鎖。
- 涉及 API:`USuqsProgression`(SUQS 主類)、`USuqsQuestData`、DataTable `FTableRowBase`
- 工時估算:3 小時
- 完成標準:Editor 中 DataTable import 無錯誤;Blueprint 中可透過 `USuqsProgression::StartQuest("Q_L01_CantileverBasics")` 正確啟動任務;`GetQuestStatus` 回傳 `Incomplete`;查詢 Objective 列表得到預期結構
- 預期踩雷:SUQS 的 quest JSON schema 跨 UE 版本可能 BREAKING;務必版控 quest JSON 原始檔,並在 CI 加 reimport 測試。SUQS plugin 版本要與 UE5.7 確認相容(查 plugin GitHub releases)。
- 依賴:無(純資料層,可最早開工)
- Phase:MVP

**F2-T02: 關卡選擇地圖 UI(UMG LevelSelectWidget)**
- 說明:新建 UMG Widget `ULevelSelectWidget`。版面:上方關卡進度條,主區域顯示關卡格子(線性解鎖,類似 Candy Crush 橫向捲動)。每格顯示:關卡圖示(從 DataAsset)、中文名稱、難度星星(1-5)、獎章圖示(完成/完美/速通,三個小圖示)。未解鎖關卡顯示鎖頭 + 灰階縮圖。點選關卡格子顯示「關卡簡介 Modal」:目標文字/約束條件/推薦工具,確認後載入該關卡 Level。
- 涉及 API:`UScrollBox`、`UUniformGridPanel`、`UButton`、`FText`、`USuqsProgression::GetQuestStatus`、`UGameplayStatics::OpenLevel`
- 工時估算:5 小時
- 完成標準:3 個 MVP 關卡正確顯示;L-01 預設解鎖,L-02 在 L-01 完成後自動解鎖(讀 SUQS 進度);點擊 L-01 顯示正確簡介文字;點確認後 Level 正確載入
- 預期踩雷:SUQS 進度儲存依賴 `USuqsProgression::SaveProgressToSlot`,需確認在 LoadGame 時順序正確(SaveGame load → 然後 SUQS load progress from slot);UI 捲動在 16:9 螢幕與 16:10 螢幕需各自測試
- 依賴:F2-T01(SUQS DataTable)、F1-T02(SaveGame 系統提供玩家進度)
- Phase:MVP

**F2-T03: 業務層數值條件判斷 Subsystem**
- 說明:新建 `UArchSimLevelObjectiveSubsystem : UGameInstanceSubsystem`。核心函數 `OnSolveComplete(const FFrameSolveResult& Result, FName LevelId)`:根據 `LevelId` 從 DataAsset(見 F2-T05)取得當前 Objective 的條件定義;遍歷條件清單,判斷 `Result.Displacements[NodeIdx].UZ < DeflectionLimit_mm`、`Result.Utilization.MaxDC < 1.0`、`Result.bSingular == false` 等數值條件;條件全滿足 → 呼叫 `USuqsProgression::CompleteTask(TaskId)`。事件廣播 `OnLevelObjectiveCompleted(FName TaskId)` 讓 HUD 接收。
- 涉及 API:`UGameInstanceSubsystem`、`FFrameSolveResult`(欄位:`Displacements`、`MemberUtilization`、`ShellUtilization`、`bSingular`、`Utilization.MaxDC`)、`USuqsProgression::CompleteTask/ProgressTask`、`DECLARE_DYNAMIC_MULTICAST_DELEGATE`
- 工時估算:4 小時
- 完成標準:在 PIE 中執行 L-01:放置一根 6m 懸臂梁 + 施加 5 kN 端點載重 + Solve → 若端點撓度 < 30mm,`CompleteTask("TASK_DeflectionGoal")` 被呼叫(Log 確認);若 > 30mm,Task 維持 Incomplete;重新修改截面後 Solve 再次觸發條件判斷
- 預期踩雷:`FFrameSolveResult.Displacements` 的索引與 `FFrameModelDef.Nodes` 的索引對應關係需透過 `UArchSimModelRegistry` 的 `NodeIdxForActor` API 取得,不能硬碼索引;`UZ` 的單位是 mm(FrameCore 輸入單位),需確認與關卡條件定義的單位一致
- 依賴:F2-T01(SUQS DataTable)、Part A1(Registry 提供節點索引)、Part C1(SolveLinear 觸發)
- Phase:MVP

**F2-T04: 關卡完成結算頁與解鎖邏輯**
- 說明:新建 UMG Widget `ULevelResultWidget`。顯示內容:獎章動畫(銅/銀/金)、「你學到了:xxx」學習主題卡(對應課綱單元,從 DataAsset 讀取)、「下一關」按鈕 + 「再玩一次」按鈕 + 「回主選單」按鈕。解鎖邏輯:當前關卡 Quest `bCompleted=true` 後,呼叫 `USuqsProgression::StartQuest(NextLevelId)` 解鎖下一關;儲存 SUQS 進度 `SaveProgressToSlot`。
- 涉及 API:`USuqsProgression::IsQuestCompleted`、`UWidgetAnimation`、`UGameplayStatics::OpenLevel`、`SUQS SaveProgressToSlot`
- 工時估算:4 小時
- 完成標準:L-01 通關後顯示結算頁,獎章動畫播放正確;學習主題卡顯示「你學到了:懸臂梁在固定端產生最大彎矩」;「下一關」按鈕跳至 L-02;L-02 在關卡選擇地圖從鎖定變解鎖;儲存後重新啟動遊戲,L-02 仍處於解鎖狀態
- 預期踩雷:SUQS 進度和 USaveGame 進度是兩套系統,需確保 `SaveProgressToSlot` 和 `AsyncSaveGameToSlot` 都被呼叫且順序正確(建議在同一個 `OnQuestCompleted` delegate 中依序觸發)
- 依賴:F2-T01、F2-T02、F2-T03
- Phase:MVP

**F2-T05: 關卡 DataAsset 進階欄位(預設結構/限制材料)**
- 說明:新建 `UDA_ArchSimLevel : UDataAsset`,欄位包含:`QuestId`(FName)、`LevelName`(FText)、`DifficultyStars`(int32, 1-5)、`LearningTopicText`(FText, 用於結算頁)、`CurriculumTags`(TArray\<FText\>, 課綱單元對應)、`ObjectiveConditions`(TArray\<FArchSimObjectiveCondition\> — 自訂 struct,含 TaskId/ConditionType enum/ThresholdValue)、`StartingModel`(FSoftObjectPath 指向 JSON 或 FFrameModelDef)、`AllowedMaterials`(TArray\<FName\>, 空表示全開放)。建立 3 個 MVP DataAsset 實例。
- 涉及 API:`UDataAsset`、`FArchSimObjectiveCondition`(新建 USTRUCT)、`EArchSimConditionType` enum(DeflectionLess/DCLess/NotSingular/Custom)
- 工時估算:3 小時
- 完成標準:DataAsset 可在 Editor 填寫所有欄位;`ObjectiveConditions` 陣列可在 F2-T03 的 Subsystem 中讀取並觸發正確判斷;`AllowedMaterials` 在 Part B1 構件面板中起過濾作用(留鉤點,Part B1 負責實作過濾 UI)
- 預期踩雷:DataAsset 中的 `FArchSimObjectiveCondition` struct 如果含 UPROPERTY(EditAnywhere),需確保在 DataAsset 的 constructor 中有預設值,否則 Editor 打開時空 struct 會顯示異常
- 依賴:F2-T01
- Phase:Phase 2

**F2-T06: SUQS 多人進度同步(PlayerState replicated)**
- 說明:多人模式下,SUQS `USuqsProgression` 是 per-player,不自動同步。在 `AArchSimPlayerState` 加 `UPROPERTY(Replicated) FString SuqsProgressSnapshot`(JSON 快照)。當 Server 端 Quest 完成時,序列化進度並廣播到對應 PlayerState;Client 收到後 `LoadProgressFromString` 更新本地 SUQS。
- 涉及 API:`APlayerState`、`UPROPERTY(ReplicatedUsing=OnRep_...)` 、`USuqsProgression::LoadProgressFromString`(需驗證 SUQS 是否提供此 API,若無則需自行序列化)
- 工時估算:4 小時
- 完成標準:PIE 2 clients 情況下,Client 1 完成 L-01 後,Client 2 的關卡選擇地圖也看到 L-01 完成狀態(若兩人在同一關卡組);注意:多人下進度是按 PlayerState 獨立追蹤,不共用
- 預期踩雷:SUQS 的進度序列化 API 需查 SUQS 文件確認,若沒有 `LoadProgressFromString` 需自己遍歷 Quest 狀態逐一 `CompleteTask`;多人模式下 Quest 進度可能需要按「組」而非按「個人」設計(教育場景下組別通關更合適)
- 依賴:F2-T03、Part H1
- Phase:Phase 2

---

## F3 學習閉環整合

### 設計原則

閉環是整個教育系統的骨幹。6 個階段(設計→施工→加載→崩塌→診斷→改)在 HUD 上顯示為進度指示器,讓學生始終知道自己在哪個階段。5-15 分鐘完成一個閉環是 MVP 目標。

### 子任務清單

**F3-T01: 閉環進度 HUD(6 階段圓圈)**
- 說明:新建 UMG Widget `ULearningLoopHUD`。頂部顯示 6 個圓圈圖示(設計/施工/加載/崩塌或通過/診斷/改),目前階段的圓圈放大 + 填色高亮,其他圓圈較小 + 灰階。圓圈之間用箭頭連接。圓圈下方顯示中文階段名稱。階段切換時播放放大動畫(0.3 秒)。
- 涉及 API:`UUserWidget`、`UImage`、`UWidgetAnimation`、`FWidgetTransform`、`DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseChanged, ELearningPhase, NewPhase)`
- 工時估算:3 小時
- 完成標準:在 PIE 中手動呼叫 `SetPhase(ELearningPhase::Loading)`,HUD 對應圓圈正確高亮;動畫播放流暢(60 fps);在 1280×720 螢幕上 6 個圓圈不重疊、可識讀;沙盒模式不顯示此 HUD(GameMode 控制)
- 預期踩雷:`UWidgetAnimation` 在 UMG 中以 `PlayAnimation` 觸發,但若動畫尚未 bind 到 widget property 會靜默失敗;建議用 `PlayAnimationForward/Reverse` 並在 AnimationFinished Callback 中確認
- 依賴:F2-T02(關卡 HUD 基礎)
- Phase:MVP

**F3-T02: 設計→施工→加載三階段狀態機**
- 說明:新建 `UArchSimLearningLoopSubsystem : UGameInstanceSubsystem`。`ELearningPhase` enum:`Design / Construction / Loading / CollapseOrPass / Diagnosis / Redesign`。提供 API:`AdvancePhase()` / `SetPhase(ELearningPhase)` / `GetCurrentPhase()`。Phase transition callback:
  - `Design → Construction`:僅在至少 1 個構件被放置後允許(否則 toast 提示「先放置至少一根構件」)
  - `Construction → Loading`:僅在 Part E 施工工序狀態機完成後允許(若 Part E 未完成,直接跳過施工階段)
  - `Loading → CollapseOrPass`:觸發 `UFrameInteractiveSubsystem::ApplyPatchAndResolve`,等待回呼後根據結果決定進入 `CollapseOrPass`
  - Phase 廣播到 `ULearningLoopHUD`(F3-T01)更新顯示
- 涉及 API:`UGameInstanceSubsystem`、`UFrameInteractiveSubsystem::ApplyPatchAndResolve`、`DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam`
- 工時估算:4 小時
- 完成標準:單元測試(C++ Automation Test)驗證 6 種 phase transition 是否正確觸發;PIE 中手動觸發每個 phase 切換,HUD 正確更新;`Loading → CollapseOrPass` 時 FrameCore Solve 確實被呼叫(Log 確認);`bSingular=true` 時直接進入 `CollapseOrPass(失敗路徑)`
- 預期踩雷:`UFrameInteractiveSubsystem::ApplyPatchAndResolve` 是同步呼叫(目前 FrameCore 設計),在大型結構上可能造成短暫卡頓;建議加 loading spinner 動畫遮掉這個 frame
- 依賴:F3-T01、Part A1(Registry)、Part C1(SolveLinear 路徑)
- Phase:MVP

**F3-T03: L-01 懸臂梁完整閉環實作**
- 說明:以 L-01「懸臂梁基礎」為 MVP 閉環的完整端到端實作。從關卡載入開始,逐步驗證:①「設計」階段玩家可放置 1 根梁(B1 依賴);②「施工」階段若 Part E 已完成則走施工流程,否則跳過(Phase 1 MVP 跳過);③「加載」階段施加 5 kN 端點載重(C1 依賴);④觸發 Solve;⑤若撓度 < 30mm → 結算頁(F2-T04),若 > 30mm → 崩塌動畫(C3,MVP 可能用靜態崩塌截圖降級)→ 診斷工具可用;⑥「改」階段玩家可修改截面後重試。xAPI 記錄 `initialized`、`attempted`、`passed/failed` 三個 verb(需 Part I log 系統,MVP 可先用本地 UE_LOG)。
- 涉及 API:完整整合前述所有模組
- 工時估算:6 小時
- 完成標準:一個高職一年級學生(零基礎)可在 15 分鐘內獨立完成 L-01 完整閉環一次;撓度判斷結果與 FrameCore standalone F68 cantilever oracle 對標差 < 1e-5;第 3 次失敗後顯示鷹架提示(F4-T03 依賴,MVP 先顯示固定提示文字)
- 預期踩雷:整合測試可能暴露各模組之間的介面不一致;建議先寫 C++ Automation Test 驗證 `UArchSimLearningLoopSubsystem` 的 phase transition 不依賴 UI 渲染
- 依賴:F1-T01~T03、F2-T01~T04、F3-T01~T02、Part A1、Part B1、Part C1/C2
- Phase:MVP

**F3-T04: L-02 簡支梁 + L-03 門型框架完整閉環**
- 說明:依照 F3-T03 的模式實作兩個關卡。L-02:跨距 8m 簡支梁,中點施加 10 kN 集中力,目標中點撓度 < 25mm;L-03:跨距 6m 門型框架,施加 5 kN 風載(UDL on column),目標所有構件 D/C < 1.0。每個關卡需要對應的 `UDA_ArchSimLevel` DataAsset(F2-T05)和 SUQS quest JSON(F2-T01)。
- 工時估算:5 小時
- 完成標準:L-02 和 L-03 可完整通關;閉環流程與 L-01 一致;學習主題卡在結算頁顯示正確(L-02:「跨距越長,撓度越大」;L-03:「框架的水平剛性來自節點剛性」)
- 預期踩雷:L-03 門型框架 UDL 風載需確認施加方向(global -X 或 local),FrameCore UDL 是 member local 座標,需在 C1 載重施加時轉換
- 依賴:F3-T03 完成後才開始
- Phase:Phase 2

**F3-T05: 通過者診斷模式深入學習路徑**
- 說明:設計關卡時學生通過後不是強制進入結算,而是可選擇進入「診斷探索模式」,自由查看 BMD/SFD、應力分布、模態形狀。診斷模式無時限,退出後才顯示結算頁。HUD 增加一個「深入探索」vs「直接完成」的選擇對話框。
- 工時估算:3 小時
- 完成標準:通過 L-01 後顯示選擇對話框;選「深入探索」後 D1 診斷工具完全可用;退出後結算頁正常顯示;xAPI 記錄 `explored` 動詞(需 Part I)
- 依賴:F3-T03、Part D1-D4(診斷工具)
- Phase:Phase 2

---

## F4 失敗即學習機制

### 設計原則

Kapur 2016 Productive Failure 理論的核心:學生在嘗試失敗「之後」進行的正式探索,遷移題表現達傳統教學的 3 倍。系統設計要讓「失敗」感覺「有趣且有意義」,而非「懲罰」。

### 子任務清單

**F4-T01: 失敗事件偵測與友善語氣 UI**
- 說明:在 `UArchSimLearningLoopSubsystem` 的 `Loading → CollapseOrPass` transition 中,若 `FFrameSolveResult.bSingular == true` 或 `MaxDC > 1.0`,廣播 `OnFailureDetected(EFailureMode)` 事件。新建 UMG Widget `UFailureAnalysisWidget`:標題「啊,塌了!來看看哪裡有問題」(親切語氣,無紅色 Game Over 字眼);副標題顯示主要失效模式(彎矩過大 / 剪力過大 / 結構不穩定);顯示 30 秒倒數計時可自由探索;「重新設計」按鈕回到 Design 階段。
- 涉及 API:`FFrameSolveResult.bSingular`、`FFrameSolveResult.Utilization.MaxDC`、`UTextBlock`、`FTimerHandle`
- 工時估算:4 小時
- 完成標準:製造一個明顯超載結構(DC=2.0)觸發 Solve → `OnFailureDetected` 確實廣播;Widget 正確顯示失效模式文字;30 秒後 Widget 自動消失(仍可繼續探索,只是提示消失);語氣文字無「失敗」「錯誤」「Game Over」等負面字眼
- 預期踩雷:`bSingular=true` 表示機構(結構不穩定,行列式=0),這與 `MaxDC > 1.0`(超載)是兩種不同的失敗模式,UI 顯示文字需要分別處理
- 依賴:F3-T02(學習閉環狀態機)、Part C1(Solve 觸發路徑)
- Phase:MVP

**F4-T02: 最早失效構件高亮(OnEventReached 接合)**
- 說明:當倒塌動畫播放時(`AFrameDynCollapseReplayActor`,Part C3 依賴),接收 `OnEventReached` multicast delegate 的第一個事件(最早失效的構件),在 `UFailureAnalysisWidget` 上顯示紅色閃光標示 + 文字「最危險構件:M-{idx},模式:{mode},D/C={value}」。若 Part C3(崩塌動畫)尚未實作,改用靜態方式:從 `FFrameSolveResult.Utilization.GoverningMemberIdx` 取最危險構件並高亮(透過 `AFrameUtilizationHeatmapActor` 的 `HighlightMember(MemberIdx)` API,需驗證是否存在或需新增)。
- 涉及 API:`AFrameDynCollapseReplayActor::OnEventReached` delegate、`AFrameUtilizationHeatmapActor`(highlight API,需驗證)、`FFrameSolveResult.Utilization.GoverningMemberIdx`
- 工時估算:4 小時
- 完成標準:MVP 降級路徑(無 C3):從 `GoverningMemberIdx` 取到最危險構件後,對應的 UtilizationHeatmap Actor 顯示閃爍紅色標示;Widget 文字顯示正確的構件索引和 D/C 值;完整路徑(有 C3):第一個 `OnEventReached` 事件觸發後 <500ms 內顯示高亮
- 預期踩雷:`AFrameUtilizationHeatmapActor` 的 highlight 單一 member 的 API 在 v4.0.0 可能沒有明確的 `HighlightMember` 函數,需在 UE consumer 層新增 helper 或透過材質動態更新;不能修改 FrameCore engine source
- 依賴:Part C2(UtilizationHeatmapActor)、Part C3(DynCollapseReplayActor,Phase 2)
- Phase:MVP(降級路徑);Phase 2(完整路徑)

**F4-T03: 漸退式鷹架提示系統(第 3 次失敗觸發)**
- 說明:在 `UArchSimLearningLoopSubsystem` 追蹤當前關卡的重試次數(`int32 FailureCount`)。第 1-2 次失敗:正常顯示 `UFailureAnalysisWidget`。第 3 次失敗:Widget 底部增加「鷹架提示」區塊,顯示來自 `UDA_ArchSimLevel::ScaffoldHints`(TArray\<FText\>, 每個 hint 對應一個失敗模式)的提示文字。第 5 次失敗:額外提示「要不要看看推薦做法?」按鈕,點擊後移除最差的一根構件(自動選 `GoverningMemberIdx`)並提示原因。鷹架提示不顯示材料/截面的具體數字,只給方向性建議。
- 涉及 API:`UArchSimLearningLoopSubsystem`(新增 `FailureCount` field)、`USuqsProgression::GetQuestActiveTasks`、`UDA_ArchSimLevel::ScaffoldHints`
- 工時估算:4 小時
- 完成標準:製造連續 3 次失敗 → 第 3 次 Widget 顯示鷹架提示文字;提示文字對應當前失效模式(彎矩 → 「試試看加大梁深或縮短跨距」);第 5 次失敗顯示自動移除提示;xAPI 記錄 `received-hint` 事件(需 Part I,MVP 先 UE_LOG)
- 預期踩雷:「第 3 次失敗」的計數需要在 session 層持久(不能因 phase 切換而重置);建議儲存在 PlayerController 的 UPROPERTY 而非 Subsystem(Subsystem 在 GI 層是持久的但 per-level subsystem 會重建)
- 依賴:F4-T01、F2-T05(DataAsset 的 ScaffoldHints 欄位)
- Phase:Phase 2

**F4-T04: 失敗模式分類(bending/shear/buckling/instability)**
- 說明:新建 `EArchSimFailureMode` enum:`Bending / Shear / Buckling / Instability / NearCapacity / DesignOK`。在 `UArchSimLearningLoopSubsystem` 的 `OnSolveComplete` 中判斷:①`bSingular=true` → `Instability`;②查詢 `GoverningMemberIdx` 的 `MemberUtilization[idx].bBuckling`(需驗證 FrameCore 是否提供此欄位)→ `Buckling`;③`MemberInternalForces.Vy/Vz` 主導 → `Shear`;④其他 → `Bending`。失敗模式傳遞給 `UFailureAnalysisWidget`(F4-T01)顯示對應文字。
- 涉及 API:`FFrameSolveResult`(欄位驗證清單:`bSingular`、`Utilization.GoverningMemberIdx`、`MemberUtilization[].DCRatio`、`MemberInternalForces[]`)、`EArchSimFailureMode`
- 工時估算:3 小時
- 完成標準:製造 4 種失效情境各 1 個並驗證分類正確:懸臂梁過長 → Bending;薄板剪力破壞 → Shear;細長柱 → Buckling;浮空節點 → Instability
- 預期踩雷:FrameCore `MemberUtilization` 目前是 ElasticAllowable D/C 快篩,判斷「哪個內力分量主導」需要自行在 UE 層比較 `MemberInternalForces[idx].My / Vz / N` 的比例,不能直接從 FrameCore 拿「失效模式 enum」;這個判斷邏輯需要教育化簡(真實工程更複雜),需明確標注「教育用簡化分類」
- 依賴:F4-T01、Part A1(MemberIdx 對應)
- Phase:Phase 2

**F4-T05: 觀察時間 xAPI 記錄**
- 說明:在 `UFailureAnalysisWidget` 的顯示開始時記錄時間戳 `StartObservationTime = FDateTime::UtcNow()`。當玩家點「重新設計」或 30 秒後 Widget 消失時,計算 `ObservationDuration = (FDateTime::UtcNow() - StartObservationTime).GetTotalSeconds()`。透過 Part I 的 xAPI log 系統送出 Statement:`verb=observed-failure, result.extensions.observation-duration-sec=X`。若觀察時間 < 5 秒,在老師 dashboard(Part I)標記為「可能未真正探索」。
- 涉及 API:Part I xAPI log 系統(依賴);`FDateTime::UtcNow()`
- 工時估算:2 小時
- 完成標準:完整路徑需 Part I 完成;MVP 降級:在 UE_LOG 記錄觀察時間 + 一個本地 CSV 檔案追蹤每個學生每次的觀察時間
- 依賴:F4-T01、Part I(xAPI log)
- Phase:Phase 2

---

## F5 對標真實規範

### 設計原則

三層分層:**露出**(學生看到簡化版公式和判斷結果)/**黑盒**(系統自動判斷但不顯示詳細計算)/**完全屏蔽**(過於複雜的規範細節不進入遊戲)。MVP 只做露出層和黑盒層。

### 子任務清單

**F5-T01: UArchSimWarningSubsystem 基礎警告骨架**
- 說明:新建 `UArchSimWarningSubsystem : UGameInstanceSubsystem`。核心 API:`CheckAndFireWarnings(const FFrameSolveResult& Result)`(在每次 Solve 完成後被呼叫)。內部遍歷警告條件清單(從 `DA_WarningTemplates` 讀取)。符合條件的警告透過 `OnWarningTriggered(FArchSimWarning Warning)` delegate 廣播。防麻木機制:同類型警告同一個構件 30 秒內不重複廣播。嚴重度排序:同時觸發多個警告時,按 `EWarningSeverity`(Critical/High/Medium/Low)排序,只廣播前 3 個最嚴重的。
- 涉及 API:`UGameInstanceSubsystem`、`DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWarningTriggered, FArchSimWarning, Warning)`、`FTimerHandle`、`TMap<FName, float>` 追蹤最後警告時間
- 工時估算:4 小時
- 完成標準:手動呼叫 `CheckAndFireWarnings` 傳入一個 MaxDC=1.5 的 SolveResult → `OnWarningTriggered` 確實廣播,且廣播的 Warning 是 `CapacityExceeded`;重複呼叫 10 次在 30 秒內,只廣播 1 次(防麻木機制);傳入 4 個不同類型警告,只廣播最高嚴重度 3 個
- 預期踩雷:`FTimerHandle` 的 cooldown 機制在多構件情境下需要用 `TMap<int32, float>` 以 MemberIdx 為 key 追蹤各構件的最後警告時間,而非全局 cooldown
- 依賴:無(可最早開工,骨架不依賴 FrameCore 結果格式)
- Phase:MVP

**F5-T02: DA_WarningTemplates 10 種警告中文模板**
- 說明:新建 `UDA_WarningTemplates : UDataAsset`,內含 `TArray<FArchSimWarningTemplate>` 陣列。`FArchSimWarningTemplate` struct:`WarningType`(EWarningType enum)、`Severity`(EWarningSeverity)、`TitleTemplate`(FText,可含 {MemberName}/{Value} 替換符)、`CauseText`(FText)、`ConsequenceText`(FText)、`RecommendationText`(FText)。10 種警告類型(W-01 ~ W-10):W-01 容量超載(CapacityExceeded)/W-02 接近容量(NearCapacity, DC=0.8-1.0)/W-03 撓度超過 L/360(DeflectionExceeded)/W-04 強柱弱梁違反(StrongColumnWeakBeam,教育簡化版)/W-05 材料不適當(MaterialInappropriate)/W-06 地震載重組合(EarthquakeLoadCombo)/W-07 幾何不穩定(GeometricInstability)/W-08 鋼筋混凝土相容警告(RCCompatibility)/W-09 細長比超標(SlendernessRatio)/W-10 設計優良(DesignOptimal,唯一正向)。
- 涉及 API:`UDataAsset`、`FText::Format` + `FFormatNamedArguments`(用於替換 {MemberName}/{Value} 佔位符)
- 工時估算:4 小時
- 完成標準:10 個警告類型均有完整中文文字(Title/Cause/Consequence/Recommendation 四欄);W-10 是正向回饋,語氣與其他 9 個不同;在 Editor 中可讀取 DataAsset 所有欄位;FText::Format 替換佔位符測試通過
- 預期踩雷:FText 多語在地化要求所有文字用 `NSLOCTEXT` 或 `LOCTEXT` 包裝,才能支援 Phase 3 的英文版;MVP 可先用 `FText::FromString`,但建立後之後遷移有一定工作量;建議從一開始就用 `NSLOCTEXT("ArchSimWarnings", "W01_Title", "梁 {MemberName} 彎矩需求超過承載能力")`
- 依賴:無(純內容資料層)
- Phase:MVP

**F5-T03: 警告觸發條件對應 FrameCore 結果欄位**
- 說明:在 `UArchSimWarningSubsystem::CheckAndFireWarnings` 中實作 10 個警告的觸發條件判斷:W-01:`MaxDC > 1.0`;W-02:`MaxDC in [0.8, 1.0]`;W-03:最大節點位移 > L/360(L 為跨距,需從構件長度取);W-04:柱的 D/C 小於梁的 D/C 的教育簡化判斷(強柱弱梁目標是柱應有餘裕);W-05:材料 Rho 為 0 或 E 異常低;W-06:地震載重工況被啟用但未設定 I 類別;W-07:`bSingular == true`;W-08:RC 材料但無鋼筋定義;W-09:構件長細比 L/r > 120(需計算);W-10:全部構件 MaxDC < 0.5 且無奇異(「設計優良!」)。
- 涉及 API:`FFrameSolveResult`(完整欄位清單)、`FFrameModelDef`(取構件長度/截面)、`UArchSimModelRegistry`(取構件幾何資訊)
- 工時估算:5 小時
- 完成標準:每個警告類型各製造 1 個觸發情境並驗證確實廣播正確的 Warning;W-10 在所有構件 DC < 0.5 時廣播(正向回饋);W-03 的 L 計算需要從 Registry 取構件兩端節點座標差;W-09 的 r(迴轉半徑)從 `FFrameSectionLibrary` 的截面 `Iy/(A)^0.5` 計算(需在 UE consumer 層新增 helper,不能修改 FrameCore)
- 預期踩雷:長細比計算需要截面的 `Iy`、`A` 和構件長度,這三個量在 FrameCore `FFrameSection` USTRUCT 中有,但拿到 `FFrameSection` 需要透過 Registry 的材料/截面查詢 API;確認 API 可用性,若無則需新增 helper
- 依賴:F5-T01、F5-T02、Part A1(Registry API)
- Phase:MVP

**F5-T04: Toast 通知 UI + 警告日誌面板**
- 說明:新建 UMG Widget `UWarningToastWidget`:右下角彈出,5 秒後 fade out;顯示警告圖示(依 Severity 用不同顏色 icon)+ 標題文字 + 「查看詳情」按鈕。新建 UMG Widget `UWarningLogPanel`:類似遊戲內日誌面板,列表顯示本 Solve session 所有警告的 Title/Cause/Consequence/Recommendation 完整文字,可捲動;按「警告圖示」HUD button 切換顯示/隱藏。
- 涉及 API:`UUserWidget`、`UWidgetAnimation`(fade in/out)、`UScrollBox`、`UTextBlock`、`UImage`
- 工時估算:4 小時
- 完成標準:觸發 W-01 警告 → 右下角 5 秒後消失 Toast 出現;點「查看詳情」→ 警告日誌面板打開,顯示完整四欄文字;觸發 5 個警告後日誌面板顯示所有 5 個;W-10(正向回饋)用不同顏色(綠色 #009E73)顯示
- 預期踩雷:多個 Toast 同時出現時需要排隊(只顯示最新的,舊的依次往上移或直接加入日誌);對策:Toast queue 機制,每次只顯示 1 個,前一個消失後再顯示下一個
- 依賴:F5-T01、F5-T02、F5-T03
- Phase:MVP

**F5-T05: 規範對標按鈕進階模式(條文摘要 UI)**
- 說明:在診斷模式的側邊面板加「規範對標」按鈕。點擊後顯示 Modal Widget `UCodeReferenceWidget`,顯示當前最高嚴重度警告對應的台灣規範條文摘要:台灣建築技術規則/混凝土結構設計規範 112 版摘錄(只顯示學生需理解的關鍵概念,非完整條文)。條文摘要儲存在 `DA_WarningTemplates` 的 `CodeReferenceSummary`(FText)欄位。加明確標示「教育用簡化版,非設計依據」。
- 涉及 API:`UUserWidget`、`UScrollBox`、`UDA_WarningTemplates`(新增 `CodeReferenceSummary` 欄位)
- 工時估算:3 小時
- 完成標準:在診斷模式下點「規範對標」→ Modal 正確顯示當前最嚴重警告的條文摘要;W-01(超載)對應顯示「建築技術規則第六節:梁之強度需大於設計載重 × 安全係數...」(簡化版);「教育用簡化版」警告標示醒目
- 預期踩雷:條文摘要文字可能較長(300-500 字),在手機或小螢幕需要 ScrollBox 才不會截斷
- 依賴:F5-T02~T04、F3-T05(診斷模式)
- Phase:Phase 2

**F5-T06: 警告日誌 xAPI 記錄**
- 說明:每次警告觸發時透過 Part I xAPI log 系統送出 Statement:`verb=received-warning, object.id=WarningType, result.extensions.dc-ratio={value}`。警告被玩家點擊「查看詳情」時送出:`verb=reviewed-warning`。老師 dashboard(Part I)可統計「哪個警告最常被觸發」和「哪些警告學生沒有去看詳情」。
- 工時估算:2 小時
- 完成標準:需 Part I xAPI log 系統完成才能驗收;MVP 降級:本地 CSV 記錄每個 warning type 的觸發次數
- 依賴:F5-T04、Part I
- Phase:Phase 2

---

## 總工時估算與實作建議

### 各子節工時彙總

| 子節 | MVP 工時 | Phase 2 工時 | Phase 3 工時 | 小計 |
|---|---|---|---|---|
| F1 沙盒模式 | 11h (T01-T03) | 7h (T04-T05) | 4h (T06) | 22h |
| F2 關卡系統 | 16h (T01-T04) | 7h (T05-T06) | 25h (T07) | 48h |
| F3 學習閉環 | 13h (T01-T03) | 8h (T04-T05) | 4h (T06) | 25h |
| F4 失敗即學習 | 8h (T01-T02) | 9h (T03-T05) | 6h (T06) | 23h |
| F5 對標規範 | 17h (T01-T04) | 9h (T05-T06) | 6h (T07) | 32h |
| **合計** | **65h** | **40h** | **39h** | **144h** |

> 整合測試、重構、bug 修復額外預估 +34h(約 24%),總工時 **178 小時**。
> 以 1-2 人團隊全職開發估算:MVP(F 部分)約 **2-3 個月**,完整版約 **4-5 個月**。

### 建議實作順序(依最短阻塞路徑)

1. 第一週:F2-T01(SUQS DataTable)+ F5-T01/T02(警告骨架+模板)— 純資料層,無引擎依賴,可在 Part A/B/C 完成前先行
2. 第二週:F1-T01/T02(沙盒 DataAsset + SaveGame)— Part A1 完成後立即開始
3. 第三週:F1-T03(沙盒 HUD)+ F3-T01(閉環 HUD)— UI 層,可與引擎整合並行
4. 第四週:F3-T02(狀態機)+ F2-T03(數值條件判斷)— 核心業務邏輯
5. 第五週:F2-T02/T04(關卡選擇 UI + 結算頁)
6. 第六週:F3-T03(L-01 完整閉環)— 所有前置完成後的整合里程碑
7. 第七週:F4-T01/T02(失敗偵測 + 構件高亮)+ F5-T03/T04(警告觸發 + Toast UI)
8. 第八週:整合測試 + MVP 驗收(對照 `L2 MVP 驗收條件` 的 10 項)

### 誠實的不確定點

- **SUQS API 的確切 struct 名稱**需在安裝 plugin 後驗證(本計畫基於文件描述,實際 API 可能有差異)
- **AFrameUtilizationHeatmapActor 單一 Member 高亮 API**:v4.0.0 source FROZEN,需確認現有 API 是否支援,若不支援需在 UE consumer 層新增 actor wrapper
- **F5-T04 Toast 與 UE 的 Notification Widget**:UE5 有 `FNotificationInfo + SNotificationList` 的 Slate 系統,但在 UMG 中整合較複雜;建議直接用 UMG 的自訂 Animation Widget 而非整合 Slate Notification,避免跨系統問題


---


# Part G 實作擴充(178h → **修正為 125h**)

**標題**:Part G — 測量關卡(獨立支線)

**子節數**:5

> **⚠️ 2026-06-25 訂正**:本 Part 由 subagent 撰寫時不知道 `Plugins/LevelSim/` 已存在,把 G3 (水準儀) 與 G5 部分題型當作從零做。實際上 LevelSim 已 bundled 進 main(2026-06-18 PR#8,standalone 115/115 PASS),含 14 個算法 API、完整 FSM、Multi-station 支援。下方 sub-task 的 G3-T1 ~ G3-T5 / G5-T1 / G2-T1(誤標)應視為「**LevelSim 整合與 polish**」而非「**從零實作**」,工時相應減少。詳細 LevelSim 能力見 [主檔 Part G3 章節](ARCHITECT_SIM_MASTER_PLAN.md#g3-水準儀操作-✅-levelsim-已完整實作僅需-polish--整合)。
>
> **訂正後的真實工時**:
> - G1(隨機地形 + LevelSim RoutePoints 整合):**15h**(原 ~20h)
> - G2(經緯儀,從零做):**40h**(不變)
> - G3(水準儀,LevelSim polish):**5h**(原 30h,節省 25h)
> - G4(多人分工,LevelSim 改多人):**25h**(原 ~30h)
> - G5(任務題型,S-01~S-04 用 LevelSim):**40h**(原 ~58h,節省 18h)
> - **Part G 總計:125h**(原 178h,節省 53h)
>
> **下方 subagent 產出原文保留作為參考**,但實作時請以「主檔 Part G 修訂版」為準。

### MVP 必須(Phase 1)
- G1-T1: PCG_SurveyTerrain 圖建立與 Seed 確定性驗證
- G1-T2: 地物標記 Actor 架構與互相可見性保證
- G3-T1: AArchSimLevelInstrument Actor 基礎骨架
- G3-T2: 後視/前視讀數邏輯與 HI 計算
- G3-T3: 手簿 UMG 自動記錄與閉合差顯示
- G5-T1: S-01 高差計算關卡實作
- G5-T2: SUQS DataTable 骨架與通關判斷 Callback
- G2-T1: AArchSimTheodolite Actor 基礎骨架
- G2-T3: 水平角 / 垂直角讀數精度實作

### Phase 2
- G1-T3: 五種地形難度 DataAsset 與課程銜接 JSON
- G1-T4: 障礙物生成邏輯與視線遮蔽測試
- G2-T2: 7 階段 18 步驟完整操作狀態機
- G2-T4: 光學對點器 / 水準器 Mini UI (SceneCapture2D)
- G2-T5: 正倒鏡切換與平均消誤差計算
- G3-T4: AI NPC 扶尺員行為 (BehaviorTree)
- G3-T5: 視距模糊效果 (PostProcess Material)
- G4-T1: UArchSimSurveyRoleSubsystem 角色管理
- G4-T2: 三角色輸入 MappingContext 切換
- G4-T3: 換手機制與強制輪轉邏輯
- G5-T3: S-02 閉合水準路線關卡
- G5-T4: S-03 建物角隅高程放樣關卡
- G5-T5: S-05 水平角測量關卡

### Phase 3
- G1-T5: World Partition 整合 (測量地形 chunk)
- G2-T6: 18 步驟精細教學 Tutorial 關卡 (自動整平按鈕)
- G3-T6: 進階手動計算模式 (手動輸入 BS/FS/HI)
- G4-T4: 老師可觀察任一玩家視角 (Spectator 整合)
- G4-T5: 多人手簿同步 (Replicated UPROPERTY + OnRep)
- G5-T6: S-04 橫斷面測量關卡 + JSON 匯出至主線
- G5-T7: S-06 導線測量關卡 (平差引擎 UE consumer 層)
- G5-T8: S-07 建物放樣關卡
- G5-T9: S-08 誤差診斷關卡 (教師預設錯誤手簿)
- G4-T6: xAPI 學習 log 整合 (測量行為動詞)

### 實作順序
- G1 (地形 PCG 基礎先行)
- G3 (水準儀先於經緯儀)
- G2 (經緯儀較複雜)
- G5 (任務題型配線)
- G4 (多人分工最後整合)

### 跨 Part 依賴
- Part A (A3 World 系統 + UArchSimModelRegistry): 地形基礎必須先存在
- Part F (F2 關卡系統 + SUQS DataTable 骨架): G5 任務系統依賴
- Part H (多人 Listen Server + NULL Subsystem): G4 多人分工依賴
- Part I (教師觀察模式 Spectator): G4-T4 老師視角依賴
- Part A UFrameModelBuilder.LoadModelFromJson: G5-T6 S-04 橫斷面匯出依賴

### 風險清單
- ~~PCG_SurveyTerrain Attribute By Slope 節點名稱在 UE5.7 需驗證~~ → **✅ S-00 已驗證:UE5.7 PCG 無此節點;MVP 走 Landscape Spline 老師手動標,Phase 2 升 Custom PCG Node**
- SceneCapture2D 在多玩家模式下 Render Target 共享衝突
- UArchSimSurveyRoleSubsystem 角色換手同步 (SUQS 無 built-in replication)
- AI NPC 扶尺員讀數動畫在 Client 端的 replicated transform 同步
- S-06 導線平差不用 Eigen 的替代方案確認
- PCG 確定性 Seed 在 Random Selection 節點下的行為需驗證
- PostProcess DoF 效果在低規格機的效能開關
- 障礙物 HISM 碰撞設定影響 LineTrace 可見性測試

## 詳細擴充內容


# Part G 測量關卡 — 詳細實作計畫

> **前提說明**:Part G 是獨立支線,不接 FrameCore Solve 閉環(測量關卡本身無結構分析需求),但在 S-04 橫斷面匯出時需要對接主線的 JSON schema。引擎 source 永久 FROZEN,任何計算邏輯一律寫在 `Plugins/FrameSolver/Source/FrameCoreUE/` 或全新 ArchSim 模組中,不觸碰 `Plugins/FrameSolver/Source/FrameCore/`。

---

## G1 隨機地形生成(UE5 PCG 確定性 Seed)

### 設計意圖
每局測量練習地形不同(防止學生背題),但需要確定性(同 Seed 還原相同地形)以利老師批改。地形是其他四個子節的基礎,必須最先完成。

### Sub-task 清單

#### G1-T1 — PCG_SurveyTerrain 圖建立與 Seed 確定性驗證 [MVP] (3h)

> **⚠️ 2026-06-25 S-00 spike 訂正**:UE5.7 PCG **沒有獨立「Perlin Noise」節點,改用 `Spatial Noise` 節點 `PCGSpatialNoiseMode::Perlin2D`**(`PCGSpatialNoise.h:69`)。UE5.7 PCG **沒有「Attribute By Slope」節點**,坡度篩選需走替代方案(見主檔 Part G1「Slope Filter 替代選項」)。

- **描述**:在 `Content/Surveys/PCG/PCG_SurveyTerrain.pcg` 建立 PCG Graph。節點鏈(訂正後):StaticMeshSpawner(地形基板 Landscape proxy) → SplineSampler(100m×100m 邊界) → **`Spatial Noise`(Mode = `Perlin2D`,Amplitude ±2m,OctaveCount=4)** → **Slope Filter 替代方案**(MVP 走 Landscape Spline 老師手動標可擺基準點區,Phase 2 升 Custom PCG Node) → DebugNode。開啟 `PCG Settings → UseFixedSeed = true`。新建 `ASurveyLevelManager : AActor`,在 BeginPlay 以 `UPCGComponent::SetSeed(LevelSeed)` 後 `Generate()`,並在 UMG 上顯示 Seed(讀取 `LevelSeed` 的 DataAsset)。
- **涉及 API**:`UPCGComponent::SetSeed()` / `UPCGComponent::Generate()` / `UPCGSettingsInterface::GetSettings()` / `UPCGDataCollection` / `UPCGSpatialNoiseSettings`(取代 PerlinNoise) / `ALandscapeSplineActor`(slope filter MVP 替代)
- **完成標準**:同一 LevelSeed 值連續開啟 5 次,PCG 生成的地形高度 TArray 逐位元相同(誤差 = 0)。不同 LevelSeed 生成明顯不同地形(目視即可辨別)。
- **預期踩雷**:**Spatial Noise 節點的 Mode enum 必須明確設 `Perlin2D`**(不可只設 `Perlin`,有 3D 版會輸出不同維度資料);UE5.8+ 升級時 PCGSpatialNoise API 可能再變,鎖定 UE5.7 至 MVP 完成
- **依賴**:Part A 的 World Partition + 基礎地形 Landscape 必須已存在。

#### G1-T2 — 地物標記 Actor 架構與互相可見性保證 [MVP] (4h)
- **描述**:新建三種地物標記 Actor:
  - `ASurveyBenchmarkActor`(基準點 BM,綠色木樁 + 高程標籤 UMG)
  - `ASurveyStationActor`(測站候選位置,黃色小旗)
  - `ASurveyTargetActor`(目標點,紅色木樁)
  
  PCG 圖新增三個 StaticMeshSpawner 節點,分別從平地區域(bSteep=false)隨機取點 Spawn 上述 Actor,限制:相鄰兩點距離 20m-50m。可見性驗證:每對 BM-Station 和 Station-Target 在 Server BeginPlay 後射出 LineTrace(`ECC_Visibility`),若命中則保留,若遮蔽則在鄰近重新取樣(最多重試 10 次)。
- **涉及 API**:`UWorld::LineTraceSingleByChannel()` / `PCGSettingsInterface::PointSampler` / `FPCGTaggedData` Attribute 寫入
- **完成標準**:進入測量關卡 100 次,每次 BM-Station 和 Station-Target 之間的 LineTrace 均無遮擋(Automation Test 可驗)。地形上可見 5-8 個標記 Actor。
- **預期踩雷**:PCG 生成的 Actor 在 Dedicated Server 情境下 Spawn 時機不定,BeginPlay 的 LineTrace 可能在 PCG Generate 完成之前執行;需在 `UPCGComponent::OnGenerationFinished` delegate 中才做 LineTrace,而非 BeginPlay。
- **依賴**:G1-T1 完成。

#### G1-T3 — 五種地形難度 DataAsset 與課程銜接 JSON [Phase 2] (2h)
- **描述**:新建 `USurveyTerrainConfig : UDataAsset`,欄位:TerrainDifficulty(UENUM:平坦/緩坡/陡坡/障礙物/複合)、HeightRange(float, ±0.5m ~ ±5m)、ObstacleCount(int32)、DefaultSeed(int32)、CurriculumTag(FName,對應課綱單元)。在 Content 建立 5 個 DataAsset instance。`ASurveyLevelManager` 讀取此 DataAsset 並設定 PCG 參數。
- **涉及 API**:`UDataAsset` 子類 / `PCGComponent::SetSeed()` 搭配 DataAsset 的 FixedSeed 欄位
- **完成標準**:切換不同 DataAsset 後地形難度差異肉眼可辨,HeightRange 正確影響 PCG PerlinNoise 振幅,CurriculumTag 可在 SUQS DataTable 中被引用。
- **依賴**:G1-T1、G1-T2 完成。

#### G1-T4 — 障礙物生成邏輯與視線遮蔽測試 [Phase 2] (3h)
- **描述**:「含障礙物」難度下,PCG 圖額外加入 PointSampler + StaticMeshSpawner 生成樹木/建物障礙物(使用內建 Starter Content 或 Megascans 的低 poly 樹)。障礙物必須使用 Hierarchical Instanced Static Mesh(HISM)元件並設 Collision Profile = `BlockAll`,才能被 LineTrace 遮蔽。G1-T2 的可見性驗證自動跳過遮蔽對,改為「至少有一條無遮蔽路徑」。
- **涉及 API**:`UHierarchicalInstancedStaticMeshComponent` / `FCollisionResponseContainer`
- **完成標準**:障礙物難度下,有 20%~40% 的 Station-Target 對被遮蔽(觸發重取樣),最終所有路徑組合至少一條無遮蔽。
- **依賴**:G1-T2 完成。

#### G1-T5 — World Partition 整合(測量地形 Chunk) [Phase 3] (4h)
- **描述**:將測量地形拆為 World Partition Cell(每格 50m×50m)。啟用 `Runtime Cell Loading` 根據玩家位置自動載入鄰近 Cell。LevelSeed 存在 GameState UPROPERTY(Replicated) 以廣播給所有 Client。
- **涉及 API**:`UWorldPartitionSubsystem` / `AWorldSettings::bEnableWorldPartition` / `GameState::UPROPERTY(Replicated)`
- **完成標準**:玩家在 100m×100m 地形上移動時無明顯載入卡頓(<3 秒全地形可見),Server-Client 地形一致。
- **預期踩雷**:SPUD 對 World Partition Cell naming scheme 敏感(issue #117),每次 UE minor 升級後必驗 save/load。
- **依賴**:G1-T1~T4 完成,Part H 多人架構完成。

**G1 小計工時:16 小時**

---

## G2 經緯儀操作(7 階段 18 步驟)

### 設計意圖
完整模擬真實經緯儀操作流程,技能可遷移至實機。核心挑戰在整平的物理模擬與讀數 Mini UI 的精準顯示。

### Sub-task 清單

#### G2-T1 — AArchSimTheodolite Actor 基礎骨架 [MVP] (4h)
- **描述**:新建 `AArchSimTheodolite : AActor`,持有:
  - `USkeletalMeshComponent MeshComp`:三腳架+儀器主體 Skeletal Mesh(若無現成資產先用 Cylinder 代理)
  - `float HorizontalAngle`:水平方向讀數(度,0~360)
  - `float VerticalAngle`:垂直方向讀數(度,0~180,天頂=0)
  - `float LevelBubbleX / LevelBubbleY`:圓盒水準器偏移量(-1 ~ +1 歸一化)
  - `bool bIsLeveled`:整平完成旗標
  
  加 `UBoxComponent InteractBox`:玩家走近提示「E 鍵架設」。架設時切換至 First Person Camera(`UCameraComponent`),綁定 Enhanced Input `IA_TheoRotateH / IA_TheoRotateV`(滑鼠拖動)。
- **涉及 API**:`USkeletalMeshComponent` / `UCameraComponent` / `UEnhancedInputComponent::BindAction()` / `FEnhancedInputActionValueBinding`
- **完成標準**:PIE 中玩家走近儀器、按 E 鍵、進入 First Person 視角,滑鼠左右拖動使 HorizontalAngle 改變,上下拖動使 VerticalAngle 改變。角度值在 HUD Debug 上正確顯示。
- **依賴**:G1-T1 完成(地形上有測站位置)。

#### G2-T2 — 7 階段 18 步驟完整操作狀態機 [Phase 2] (4h)
- **描述**:新建 `USurveyStepMachine : UActorComponent`,以 UENUM `ESurveyStep` 定義 18 個步驟(架設三腳架=0, 概略定心=1, ..., 讀數記錄=17)。每步驟有:
  - `RequiredCondition()`:判斷是否滿足進入下一步(如整平判斷 `|LevelBubbleX| < 0.05 && |LevelBubbleY| < 0.05`)
  - `OnStepComplete()`:廣播 `FOnStepComplete` delegate
  
  HUD 側邊顯示 18 步驟清單,當前步驟高亮,已完成打勾。
- **涉及 API**:`UActorComponent` / `UENUM(BlueprintType)` / `DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams`
- **完成標準**:從步驟 0 依序操作到步驟 17 全部可完成,跳過步驟時 UI 提示「請先完成上一步」,不允許強行跳過。
- **依賴**:G2-T1 完成。

#### G2-T3 — 水平角 / 垂直角讀數精度實作 [MVP] (3h)
- **描述**:讀數精度要求 1"(度°分'秒")。實作轉換函數 `static FString AngleToDMS(double DegreesDecimal)`,輸出格式 `123°45'06"`。HUD 顯示 HA / VA 讀數,並在「讀數記錄」步驟鎖定讀數快照存入 `FSurveyReadingRecord`(FHA_deg / FVA_deg / Timestamp)。正倒鏡平均:`double HAMean = (HA_Direct + (HA_Reverse + 180.0)) / 2.0`,VA 同理;計算後存入記錄。
- **涉及 API**:純 C++ 數學,無 UE 特定 API;結果用 `UWidgetComponent` 或 `UUserWidget` 顯示
- **完成標準**:輸入角度 `123.7516667` 度,UI 顯示 `123°45'06"`;正倒鏡平均計算結果對比手算差 ≤ 0.0003°(1"以內)。
- **依賴**:G2-T1 完成。

#### G2-T4 — 光學對點器 / 水準器 Mini UI (SceneCapture2D) [Phase 2] (4h)
- **描述**:
  - **光學對點器**:在儀器底部放置 `USceneCaptureComponent2D`(向下拍攝地面,FOV=5°),輸出到 `UTextureRenderTarget2D RT_OpticalPlummet(256x256)`,用 `UImage` 在 HUD 顯示,中心疊加十字線 Widget。玩家移動儀器水平位置時,確認 BM 測點影像落在十字中心。
  - **圓盒水準器**:Widget 顯示圓形 Border + 氣泡圓形圖示,位置由 `LevelBubbleX / LevelBubbleY` 驅動。
  - **管狀水準器**:僅顯示 1D 氣泡條(水平方向)。
- **涉及 API**:`USceneCaptureComponent2D` / `UTextureRenderTarget2D::Create()` / `UCanvas::DrawTexture()` / `UUserWidget::NativeTick()`
- **完成標準**:HUD 上三個 Mini UI 同時顯示,光學對點器影像隨儀器位置變化更新,圓盒氣泡位置反映 LevelBubbleX/Y,管狀氣泡反映精確水平偏差。
- **預期踩雷**:SceneCapture2D 在 PIE 多玩家模式下多個 PlayerController 共用同一個 RT 會畫面錯亂;需為每位玩家建立獨立 `UTextureRenderTarget2D` 實例(在 PlayerController 的 BeginPlay 中 `UKismetRenderingLibrary::CreateRenderTarget2D()`),並將 RT 指針傳入 Widget。
- **依賴**:G2-T1、G2-T2 完成。

#### G2-T5 — 正倒鏡切換與平均消誤差計算 [Phase 2] (2h)
- **描述**:F 鍵觸發 `AArchSimTheodolite::FlipTelescope()`:旋轉望遠鏡 180°(SkeletalMesh 動畫),HA 加 180° 取模 360°,VA 換算天頂距補角。讀數記錄中自動儲存正鏡組 / 倒鏡組兩筆,`GetAveragedReading()` 返回平均值並標示「已消視準軸誤差」。Tutorial 模式顯示「你知道為什麼要正倒鏡嗎?」說明卡。
- **涉及 API**:`USkeletalMeshComponent::PlayAnimation()` / `FRotator` 計算
- **完成標準**:先讀正鏡 HA=123°45'06"、後翻倒鏡讀 HA=303°45'18",系統計算平均 HA=123°45'09"(消誤差 +3"),結果正確顯示。
- **依賴**:G2-T3 完成。

#### G2-T6 — Tutorial 關卡自動整平按鈕 [Phase 3] (2h)
- **描述**:Tutorial 關卡(非正式評分關卡)HUD 顯示「自動整平」按鈕,按下後 0.5 秒動畫將 LevelBubbleX / LevelBubbleY 插值至 0(完全整平)。正式關卡此按鈕隱藏,學生必須手動操作腳螺旋。腳螺旋互動:三個 `UBoxComponent` 各對應一個腳螺旋,點擊後拖動影響 LevelBubbleX / LevelBubbleY(仿真:調整 X 軸螺旋影響 BubbleY)。
- **涉及 API**:`FMath::FInterpTo()` / `UBoxComponent::OnClicked` / `FTimerHandle`
- **完成標準**:Tutorial 關卡中按自動整平後 `bIsLeveled = true`;正式關卡中必須手動三軸螺旋迭代到 `|Bubble| < 0.05` 才觸發整平完成。
- **依賴**:G2-T2 完成。

**G2 小計工時:19 小時**

---

## G3 水準儀操作(6 階段 15 步驟)

### 設計意圖
水準儀比經緯儀操作簡單,且對應入門關卡(S-01~S-04),優先開發。核心是手簿自動計算與閉合差判斷。

### Sub-task 清單

#### G3-T1 — AArchSimLevelInstrument Actor 基礎骨架 [MVP] (3h)
- **描述**:新建 `AArchSimLevelInstrument : AActor`,持有:
  - `UStaticMeshComponent MeshComp`:水準儀主體
  - `float LevelBubbleX`:圓盒水準器偏移(自動安平水準儀可設為固定 0)
  - `bool bIsAutoLeveling`:預設 true(自動安平儀,省略精整平)
  - `UCameraComponent ScopeCam`:望遠鏡視野(FOV=2°,模擬望遠鏡)
  
  加 `UBoxComponent InteractBox`:走近顯示「E 鍵架設」。架設後進入望遠鏡第一人稱視角,滑鼠上下移動只改變垂直瞄準線,水平旋轉控制照準方向。
- **涉及 API**:`UStaticMeshComponent` / `UCameraComponent` / `UEnhancedInputComponent`
- **完成標準**:PIE 中玩家架設水準儀後進入望遠鏡視角,可旋轉照準。`bIsAutoLeveling = true` 時跳過整平步驟,直接允許讀數。
- **依賴**:G1-T1 完成。

#### G3-T2 — 後視 / 前視讀數邏輯與 HI 計算 [MVP] (4h)
- **描述**:新建 `AArchSimLevelingRod : AActor`:
  - `UStaticMeshComponent RodMesh`(垂直標尺)
  - `float RodReading`:讀數,由玩家讀取時從`ScopeCam`的 Line Trace 對 RodMesh 的 UV 換算(`HitUV.Y * RodHeight_m` = 讀數,精度 0.001m = 1mm)
  - `float RodBubbleOffset`:持尺穩定性偏移
  
  讀數流程:玩家按 Tab 鍵觸發 `Server_TakeReading()` → Server 計算 ScopeCam 的 Line Trace 命中 RodMesh 的位置 → 換算公尺讀數 → 廣播 `OnReadingTaken(float BSorFS)`。
  
  HI 計算:`HI = H_BM + BS`(在 Server 端)。目標點高程:`H_target = HI - FS`。
- **涉及 API**:`UWorld::LineTraceSingleByObjectType()` / `FHitResult::FaceIndex` / `UMaterialInterface::GetTextureParameterValue()` (換算 UV 讀數,需驗證可行性;替代方案:在 RodActor 上放多個不可見 `USphereComponent` 標記刻度,取最近命中的刻度位置換算)
- **完成標準**:扶尺員站在基準點(H_BM=10.000m),後視讀數 1.356m,HI 應顯示 11.356m。前視讀數 2.114m,目標點高程應顯示 9.242m。數值誤差 < 0.001m。
- **預期踩雷**:UV 換算讀數的方法在 UE5.7 需要 RodMesh 有展開 UV 且不被 LOD 合並頂點。備選:沿 Rod 等距放置 50 個不可見 `USphereComponent`(每 20mm 一個),LineTrace 命中最近球,取球的 Z 高度換算讀數。此法較笨但可靠。
- **依賴**:G3-T1 完成。

#### G3-T3 — 手簿 UMG 自動記錄與閉合差顯示 [MVP] (4h)
- **描述**:新建 `WBP_SurveyFieldbook : UUserWidget`,顯示表格:
  
  | 測站 | 後視 BS | 前視 FS | 儀器高 HI | 高程 H |
  |-----|--------|--------|---------|-------|
  
  表格用 `UListView` + `USurveyFieldbookRow : UObject` 實作。每次 `OnReadingTaken` 自動新增一行。底部顯示:
  - `ΣBS = X.XXX m` / `ΣFS = X.XXX m`
  - `閉合差 f = ΣBS - ΣFS = X.XXX m`
  - `容許值 = 12√n mm = XX mm`(n=測站數)
  - 判定結果:綠色「合格」/ 黃色「邊界(f/容許 = 0.8~1.0)」/ 紅色「不合格」
  
  多人模式下手簿是 Replicated:記錄員 PC 為本地 Widget,操作員/扶尺員看到唯讀版本。
- **涉及 API**:`UListView` / `UObject` DataSource / `FLinearColor` 判斷色 / `UWidgetBlueprintLibrary::SetFocusToGameViewport()`
- **完成標準**:完成 3 個測站後,ΣBS/ΣFS 計算正確,閉合差與容許值比較結果正確顯示。容許值公式 `12√n mm` 驗算:n=3 站時應顯示 20.8mm。
- **預期踩雷**:UListView 在 UE5.7 需要 DataSource 實作 `IUserObjectListEntry`,若直接用 `TArray<UObject*>` 而未繼承此介面,ListItem Widget 不會更新。
- **依賴**:G3-T2 完成。

#### G3-T4 — AI NPC 扶尺員行為(BehaviorTree) [Phase 2] (4h)
- **描述**:新建 `AAIRodman : ACharacter` + `AAIRodmanController : AAIController`。BehaviorTree `BT_Rodman`:
  - Task `BTTask_MoveToStakePoint`:移動至目標測釘位置(由操作員指派的 `ASurveyTargetActor`)
  - Task `BTTask_StabilizeRod`:到達後靜止 3 秒(模擬穩定持尺),期間 `RodBubbleOffset` 從隨機值插值到 0
  - Task `BTTask_WaitForReading`:等待 `OnReadingTaken` 事件後返回根節點
  
  多人模式下若有真人扶尺員(角色 2),AI Rodman 不 Spawn(由 `G4 角色分配` 控制)。
- **涉及 API**:`UAIController::RunBehaviorTree()` / `UBehaviorTreeComponent` / `UBlackboardComponent` / `FBlackboardKeySelector`
- **完成標準**:單人模式下 AI 扶尺員自動移動到目標點,3 秒後氣泡穩定,讀數可成功觸發。
- **依賴**:G3-T2 完成。

#### G3-T5 — 視距模糊效果(PostProcess Material) [Phase 2] (3h)
- **描述**:建立 `MI_ScopeLens : UMaterialInterface` 用於望遠鏡 ScopeCam 的 Post Process。距離 >80m 時加入 Depth of Field 模糊效果(`FPostProcessSettings::DepthOfFieldFstop` 隨距離線性增加)。由 `AArchSimLevelInstrument::Tick()` 每幀更新 `CameraPostProcessSettings`。
- **涉及 API**:`UCameraComponent::PostProcessSettings` / `FPostProcessSettings::DepthOfFieldFocalDistance` / `FPostProcessSettings::DepthOfFieldFstop`
- **完成標準**:距離 40m 目標時讀數清晰;距離 100m 時模糊明顯(Fstop > 4.0),讀數困難。
- **預期踩雷**:此效果在低規格機開啟全螢幕後處理成本高;需加 `r.DepthOfField.enabled 0` 的 Scalability 選項,讓老師可關閉。
- **依賴**:G3-T1 完成。

#### G3-T6 — 進階手動計算模式(手動輸入 BS/FS/HI) [Phase 3] (2h)
- **描述**:在手簿 UMG 加「手動模式」Toggle。手動模式下,讀數欄位變為可編輯 `UEditableText`,學生手輸 BS/FS 數值,HI 與 H 欄位變為「?」直到學生點「計算」,系統核對是否正確(容許誤差 ±0.003m)。錯誤時顯示紅框 + 「重新計算」。
- **涉及 API**:`UEditableTextBox` / `FText::AsNumber()` / `FMath::IsNearlyEqual()`
- **完成標準**:手動輸入 HI = 11.356m(正確值 11.356m),顯示綠色打勾;輸入 11.350m,顯示紅框「差了 6mm,再試一次」。
- **依賴**:G3-T3 完成。

**G3 小計工時:20 小時**

---

## G4 多人分工模式(三人組:操作 / 扶尺 / 記錄)

### 設計意圖
對應台灣高職測量實習三人一組的慣例,加入強制換手機制確保每位學生練到三個角色。

### Sub-task 清單

#### G4-T1 — UArchSimSurveyRoleSubsystem 角色管理 [MVP] (4h)
- **描述**:新建 `UArchSimSurveyRoleSubsystem : UGameInstanceSubsystem`,管理角色分配:
  ```
  UENUM: ESurveyRole { Operator, Rodman, Recorder }
  TMap<FUniqueNetIdPtr, ESurveyRole> RoleMap;
  void AssignRoles(TArray<APlayerState*> Players);
  void RotateRoles();
  ESurveyRole GetRole(APlayerState* PS);
  ```
  Server 端 `AssignRoles()` 在所有玩家 Ready 後自動分配(先到先得,第 1 位=Operator,第 2 位=Rodman,第 3 位=Recorder)。`RotateRoles()` 循環三角色,更新 RoleMap 並廣播 `OnRolesChanged` multicast delegate。
  
  RoleMap 快照存在 GameState 的 `UPROPERTY(Replicated) TArray<FSurveyRoleAssignment>`,確保 Client 同步。
- **涉及 API**:`UGameInstanceSubsystem` / `AGameState::GetPlayerArray()` / `UPROPERTY(Replicated)` / `DECLARE_DYNAMIC_MULTICAST_DELEGATE`
- **完成標準**:3 位玩家進入測量關卡後角色自動分配完畢,在每台 Client 上 `GetRole()` 返回正確值。
- **預期踩雷**:SUQS 無 built-in Replication,角色狀態必須走自製 GameState replicated UPROPERTY,不能依賴 SUQS 內部狀態同步。
- **依賴**:Part H 多人架構(Listen Server + NULL Subsystem)完成。

#### G4-T2 — 三角色輸入 MappingContext 切換 [Phase 2] (3h)
- **描述**:三個角色使用不同 Input MappingContext:
  - `MC_SurveyOperator`:全部經緯儀/水準儀控制輸入
  - `MC_SurveyRodman`:標尺移動輸入(WASD 移動角色)+ 持尺穩定小遊戲(Space 鍵穩定)
  - `MC_SurveyRecorder`:手簿 UI 快捷鍵(Enter 記錄/Tab 切換欄位)
  
  `OnRolesChanged` 觸發後,各玩家 PlayerController 根據新角色呼叫 `EnhancedInputLocalPlayerSubsystem::AddMappingContext()` + 移除其他 Context。
- **涉及 API**:`UEnhancedInputLocalPlayerSubsystem::AddMappingContext()` / `RemoveMappingContext()` / `UInputMappingContext`
- **完成標準**:換手後 1 秒內輸入模式正確切換,原角色的輸入全部失效,新角色的輸入立即生效。不出現「換手後還能操作前一個角色的儀器」。
- **依賴**:G4-T1 完成。

#### G4-T3 — 換手機制與強制輪轉邏輯 [Phase 2] (3h)
- **描述**:完成一個測站(操作員讀完 BS/FS)後,Server 廣播 `OnStationComplete` event。HUD 顯示「測站完成!換手輪到下一位」彈出視窗,倒數 10 秒。倒數完或全員按確認後執行 `RotateRoles()`。關卡結束條件:每位玩家的 `RolesCompleted` Set 包含三個角色 {Operator, Rodman, Recorder},否則 UI 顯示「尚未完整輪轉,關卡未完成」。
- **涉及 API**:`FTimerHandle` / `TSet<ESurveyRole>` / `APlayerState` 中的 `UPROPERTY(Replicated) TSet<uint8> RolesCompleted`
- **完成標準**:三位學生各輪一輪(3 個測站)後,每人的 RolesCompleted = {0,1,2},關卡標示完成。若強行跳過換手,關卡不允許結算。
- **依賴**:G4-T1、G4-T2 完成。

#### G4-T4 — 老師可觀察任一玩家視角(Spectator 整合) [Phase 3] (3h)
- **描述**:老師進入測量關卡時以 Spectator 身份加入(`ASpectatorPawn`)。HUD 顯示玩家列表(3 個學生縮圖視角),點擊可切換 `ServerViewPlayerByName()`。儀器 Actor 設 `bAlwaysRelevant = true` 確保老師在任意位置都能接收複製。
- **涉及 API**:`ASpectatorPawn` / `APlayerController::SetViewTarget()` / `AActor::bAlwaysRelevant`
- **完成標準**:老師可自由切換觀看三位學生的視角,看到的手簿數據與學生螢幕一致。
- **依賴**:Part I 教師觀察模式基礎,G4-T1 完成。

#### G4-T5 — 多人手簿同步(Replicated UPROPERTY + OnRep) [Phase 3] (4h)
- **描述**:手簿數據移至 Server 權威的 `AArchSimFieldbookActor : AInfo` 中:
  ```
  UPROPERTY(Replicated) TArray<FSurveyReadingEntry> Entries;
  UFUNCTION() void OnRep_Entries();
  ```
  每次新增 Entry 在 Server 更新,OnRep 在所有 Client 觸發 Widget 刷新。記錄員角色的 Client 使用可編輯版本(手動模式),其他玩家用唯讀版本。
- **涉及 API**:`AInfo` subclass / `UPROPERTY(Replicated, ReplicatedUsing=OnRep_Entries)` / `GetLifetimeReplicatedProps()`
- **完成標準**:操作員讀數後 500ms 內,另外兩位玩家的手簿畫面自動更新,數值與操作員一致。
- **依賴**:G4-T1、G3-T3 完成。

#### G4-T6 — xAPI 學習 log 整合(測量行為動詞) [Phase 3] (3h)
- **描述**:在 `UArchSimSurveyRoleSubsystem` 的關鍵事件點插入 xAPI Statement 發送:
  - `{verb: "completed", object: "SurveyStation-1", result: {score: {raw: BS_reading, max: 4.000}}}` (讀數完成)
  - `{verb: "rotated", object: "SurveyRole", result: {extensions: {new_role: "Recorder"}}}` (換手完成)
  - `{verb: "completed", object: "LevelRun-S01", result: {success: f<=tolerance}}` (閉合差判定)
  
  Statement 走 Part I 的 xAPI Queue 系統送至 LRS。
- **涉及 API**:Part I 的 `UArchSimXApiService::QueueStatement()`(需確認 Part I 已實作此介面)
- **完成標準**:完整跑一個 S-01 關卡後,LRS 中可查到 3 條 Statement:兩次讀數完成 + 一次關卡完成。
- **依賴**:Part I xAPI 基礎架構,G4-T3 完成。

**G4 小計工時:20 小時**

---

## G5 測量任務題型(8 種關卡)

### 設計意圖
8 種題型覆蓋測量實習全學程,從入門的 S-01 高差計算到進階的 S-06 導線測量。每種題型至少一個範例關卡,每關三級難度。

### Sub-task 清單

#### G5-T1 — S-01 高差計算關卡實作(參考關卡) [MVP] (4h)
- **描述**:實作第一個完整測量關卡 `L_Survey_S01_Easy`:
  - PCG Seed 固定(Seed=1001,平坦地形)
  - 1 個 BM(H_BM=10.000m)+ 1 個目標點
  - 單站水準儀:架設→後視→前視→計算高差→顯示結果
  - SUQS DataTable Row `Q_Survey_S01`:objective = 「計算目標點高程」,completion condition = `|H_calculated - H_true| < 0.01m`
  
  SUQS completion callback 在 `AArchSimLevelGameMode::OnStationComplete()` 中判斷條件後呼叫 `USuqsProgression::CompleteTask()`。
- **涉及 API**:`USuqsProgression::CompleteTask()` / `USuqsProgression::ProgressTask()` / `USuqsComponent` (Player Component)
- **完成標準**:玩家正確完成 S-01 後,SUQS 任務標記完成,結算頁顯示「高差 = X.XXX m,正確!」。誤差超標時顯示「再試一次,誤差 XX mm」。
- **依賴**:G3-T1~T3 完成,Part F 的 SUQS DataTable 骨架存在。

#### G5-T2 — SUQS DataTable 骨架與通關判斷 Callback [MVP] (3h)
- **描述**:建立 `DT_SurveyQuests : UDataTable`(Row type: FSuqsQuestRow),定義 8 種題型的 Quest 樹:S-01~S-08 各一個 Quest,每個 Quest 三個 Objective(Easy/Medium/Hard)。新建 `AArchSimSurveyGameMode : AGameModeBase`,加 `OnSolveComplete_Survey(FSurveyResult Result)` callback,根據 Result 比對 tolerance 後呼叫 SUQS API。
- **涉及 API**:`UDataTable::FindRow()` / `USuqsProgression::StartQuest()` / `USuqsComponent::GetActiveObjectives()`
- **完成標準**:DT_SurveyQuests 可 import,S-01 Easy 通關後解鎖 S-01 Medium,S-01 全通後解鎖 S-02。
- **依賴**:Part F 的 SUQS 基礎架構。

#### G5-T3 — S-02 閉合水準路線關卡 [Phase 2] (4h)
- **描述**:關卡 `L_Survey_S02`:地形有 4-6 個轉點,學生需從 BM1 出發,依序測量 n 站後回到 BM1。系統自動計算:
  - `f = ΣBS - ΣFS`
  - `tolerance = 12√n mm`
  - 合格條件:`|f| ≤ tolerance`
  
  誤差分配:通過後系統顯示按距離比例分配的調整值(直接顯示,不要求學生手算)。
- **完成標準**:n=5 站,f=8mm,容許 26.8mm,判定合格;f=30mm,判定不合格。
- **依賴**:G5-T1、G5-T2 完成。

#### G5-T4 — S-03 建物角隅高程放樣關卡 [Phase 2] (3h)
- **描述**:關卡 `L_Survey_S03`:已知設計高程 H_design=11.500m,學生架設水準儀,讀後視 BS=1.234m,HI=BM+BS=11.234+1.234=12.468m,計算前視讀數應為 `FS = HI - H_design = 12.468 - 11.500 = 0.968m`。學生必須指揮扶尺員移動標尺到使讀數=0.968m 的位置,系統判斷「放樣完成」(容許 ±5mm)。
- **完成標準**:正確計算 FS 並指揮扶尺員到達目標高程點,誤差 <5mm 視為放樣成功。
- **依賴**:G5-T2、G3-T4(AI 扶尺員) 完成。

#### G5-T5 — S-05 水平角測量關卡 [Phase 2] (4h)
- **描述**:關卡 `L_Survey_S05`:從測站觀測 A、B、C 三個目標,用全圓方向法測量水平角。正鏡測量 A→B→C,倒鏡測量 C→B→A,計算半測回平均。系統比對正確答案(預設 PCG Seed 下的正確水平角由 Server 計算 `FVector::Dot()` 推導)。
- **涉及 API**:`FVector::Rotation()` / `FRotator::Clamp()` / 向量點積→夾角計算
- **完成標準**:正確讀出 A-B 夾角誤差 < 30"(0.00833°),系統判定通過。
- **依賴**:G2-T3、G2-T5、G5-T2 完成。

#### G5-T6 — S-04 橫斷面測量關卡 + JSON 匯出至主線 [Phase 3] (4h)
- **描述**:關卡 `L_Survey_S04`:沿指定路線測量 5 個斷面,每斷面左中右三個測點的高程。系統自動繪製橫斷面圖(UMG 折線圖)。「匯出」按鈕將斷面數據序列化為 JSON:
  ```json
  {"crossSections": [
    {"chainage": 0.0, "points": [{"offset": -5.0, "elevation": 10.234}, ...]},
    ...
  ]}
  ```
  此 JSON 可透過 `UFrameModelBuilder::LoadModelFromJson()` 讀入主線結構模式(作為地面節點高程輸入)。**注意:LoadModelFromJson 已存在於 FrameCore UE 層,不需修改 engine source。**
- **完成標準**:匯出 JSON 可被 `UFrameModelBuilder::LoadModelFromJson()` 正確解析,斷面高程數值誤差 < 0.005m。JSON schema 有版本號欄位防止未來 schema 衝突。
- **預期踩雷**:座標系統轉換是最大風險。PCG 地形用 UE 座標(cm),測量結果要轉換為公尺,且 FrameCore 的 FFrameModelDef 節點座標也是公尺。需在匯出前統一除以 100.0。
- **依賴**:G3-T1~T3 完成,Part A 的 UFrameModelBuilder 已實作。

#### G5-T7 — S-06 導線測量關卡(平差引擎 UE consumer 層) [Phase 3] (4h)
- **描述**:導線測量需要最小二乘平差矩陣計算。**不能用 FrameCore Eigen(engine FROZEN),需在 ArchSim UE consumer 層自行實作。** 新建 `UArchSimSurveyMath::TraverseAdjust()`,以 `TArray<float>` 手算羅盤法角度閉合差分配與座標閉合差 Bowditch 平差。公式僅需 1D linear interpolation,不需要完整 Eigen:
  ```
  角度改正量 = -f_beta / n (各角度等量分配)
  座標閉合差分配:Bowditch 法 (按距離比例)
  ```
  進階模式提供「自動平差」按鈕,基礎模式學生手動輸入改正量後系統核對。
- **涉及 API**:純 C++ 算術,在 ArchSim 模組中實作 `UArchSimSurveyMath : UBlueprintFunctionLibrary`
- **完成標準**:5 站導線,輸入角度閉合差,系統自動平差後各測點座標誤差 < 0.01m。手動平差模式學生輸入正確時顯示綠色。
- **依賴**:G2-T3、G5-T2 完成。

#### G5-T8 — S-07 建物放樣關卡 [Phase 3] (3h)
- **描述**:關卡 `L_Survey_S07`:已知建物角點座標(N, E),學生操作經緯儀從已知測站照準後視點定向,旋轉水平角到放樣方位角,指揮扶尺員沿視線移動到設計距離,釘出建物角點。系統判斷釘出的角點與設計點距離 < 0.05m 為成功。
- **完成標準**:正確定向後,放樣誤差 < 0.05m,關卡判定成功。
- **依賴**:G2-T3、G2-T5、G5-T2 完成。

#### G5-T9 — S-08 誤差診斷關卡(教師預設錯誤手簿) [Phase 3] (4h)
- **描述**:關卡 `L_Survey_S08`:展示一份預設的「有錯誤的水準手簿」(DataAsset 中預設錯誤數據),學生需找出:
  - 計算錯誤(HI 或 H 算錯)
  - 超過容許閉合差
  - 粗差(單一讀數偏差 >3mm)
  
  學生點擊手簿格子,標記「這行有問題」,系統核對是否正確。全部正確標記後顯示「誤差診斷通過」。
- **涉及 API**:`UDataAsset`(預設錯誤數據) / `UListView` 點擊選取 / 判斷邏輯比對
- **完成標準**:三個預設錯誤各自可被正確標出,漏標或誤標時提示「還有問題未找到」或「這行沒有問題」。
- **依賴**:G3-T3(手簿 UI 架構)、G5-T2 完成。

**G5 小計工時:33 小時**

---

## 整合測試與緩衝

- 整合測試 G1+G3+G5 單人流程通過:8h
- 整合測試 G4 多人三角色換手:4h
- 整合測試 G2+G5 經緯儀關卡:4h
- UI 打磨(手簿排版、角度顯示、對點器縮放):6h
- 多人壓力測試(3 玩家同時讀數):4h
- SUQS 關卡解鎖鏈測試:4h
- 效能優化(PCG 地形載入 <3 秒、儀器 First Person 60fps):8h
- Bug 修復緩衝:30h

**整合 + 緩衝小計工時:68 小時**

---

## 工時彙整

| 子節 | MVP | Phase 2 | Phase 3 | 小計 |
|------|-----|---------|---------|------|
| G1 地形 PCG | 7h | 5h | 4h | 16h |
| G2 經緯儀 | 7h | 11h | 2h | 19h(估算含 1 個 bonus Task) |
| G3 水準儀 | 11h | 7h | 2h | 20h |
| G4 多人分工 | 4h | 6h | 10h | 20h |
| G5 題型關卡 | 7h | 11h | 15h | 33h |
| 整合 + 緩衝 | — | — | — | 68h |
| **合計**(修正後) | **24h** | **36h** | **30h** | **125h**(原 178h,LevelSim 已實作 G3 節省 25h + G5 部分節省 18h + G1/G4 微調 -10h) |

---

## 關鍵技術決策補充

### 為什麼測量關卡完全不接 FrameCore

主計畫書已明確「測量關卡不用 FrameCore Solve」。測量的核心計算(高差、閉合差、座標平差)都是初等算術,全在 UE consumer 層的 `UArchSimSurveyMath` BP Function Library 中實作。唯一例外是 S-04 橫斷面匯出的 JSON 可被 `UFrameModelBuilder::LoadModelFromJson()` 讀入,但那是單向資料流(測量 → 結構),不觸碰 engine FROZEN 路徑。

### S-06 導線平差不用 Eigen 的替代方案

FrameCore 的 Eigen 在 `Plugins/FrameSolver/Source/FrameCore/` 路徑下,engine FROZEN 後不能加新函數。S-06 所需的羅盤法平差僅用到 1D 線性分配(Bowditch 法),無需矩陣求逆:
- 角度閉合差分配:`correction_i = -f_beta / n`(等量分配,無矩陣)
- 座標閉合差分配:`correction_xi = -f_x * L_i / ΣL`(按距離,純 TArray 乘除)

在 `Source/ArchSim/Public/Survey/ArchSimSurveyMath.h` 用標準 C++ `std::vector<float>` 即可實作,完全不需要 Eigen。

### PCG Seed 顯示策略

Seed 值顯示給學生(供老師驗證),但 UI 上設為唯讀 `UTextBlock`(非 `UEditableText`)。Seed 由老師在 Quest DataAsset 中預設,或系統用 `FMath::Rand()` 在關卡開始時隨機生成並儲存在 GameState Replicated 欄位。學生若想「換題目」只能重新進入關卡(Server 新生成 Seed),不能手動輸入。


---


# Part H 實作擴充(138h)

**標題**:Part H — 多人協作

**子節數**:4

### MVP 必須(Phase 1)
- H1-T1: DefaultEngine.ini NULL Subsystem 設定與 SessionInterface 封裝
- H1-T2: Listen Server 建立與 LAN Session 搜尋
- H1-T3: 大廳 UMG 介面(ULobbyWidget)
- H2-T1: Server 端 OccupancyGrid + UArchSimPlacementSubsystem
- H2-T2: Client Ghost 預覽系統
- H2-T3: Server_RequestPlacement RPC + 衝突仲裁
- H3-T1: EArchSimRole 定義 + UArchSimRoleSubsystem 骨架
- H4-T1: FArchSimChange 結構 + Server 端 UArchSimUndoStack 骨架

### Phase 2
- H1-T4: 直連 IP fallback 介面
- H1-T5: 組別分配 UI + 組別管理 GameState
- H2-T4: FrameCore Solve 結果同步(GameState Replicated JSON 摘要)
- H2-T5: Client 端本地預測 + Server Reconcile 流程
- H3-T2: 四角色 UI 隔離實作(角色限定工具列 + HUD 分頁)
- H3-T3: 角色分配 RPC + AssignRole Server-only 驗證
- H3-T4: 組內語音/文字頻道(內建 Chat + Phase 2 Voice)
- H4-T2: 變更歷史 HUD 面板(時間軸 UI)
- H4-T3: 投票機制 UArchSimVoteSubsystem
- H4-T4: 回滾 FrameCore 狀態(Rebaseline 重建)

### Phase 3
- H1-T6: 跨子網 EOS P2P 升級路徑準備(結構預留, 不實作)
- H3-T5: 老師全顯模式 + 任意角色視角切換
- H4-T5: Undo Stack 容量設定 UI + 跨關卡 Stack 清空機制
- H4-T6: SPUD 存檔整合 — 含 Undo Stack 快照序列化

### 實作順序
- H1 (連線架構) — 所有多人功能的基礎建設，必須最先完成
- H2 (共編場景衝突) — 依賴 H1 Session 已建立；先建 OccupancyGrid，再串 FrameCore Solve 同步
- H3 (角色分工) — 依賴 H1+H2；UI 隔離可並行，但 AssignRole 邏輯需 H2 的 Placement 系統先穩定
- H4 (變更紀錄與復原) — 依賴 H2 的 PlacementSubsystem 提供 Change 事件；投票機制依賴 H1 已有 PlayerController 列表

### 跨 Part 依賴
- Part A (引擎與外殼) — UArchSimModelRegistry、UFrameInteractiveSubsystem、AArchSimCharacter 必須先有骨架，H2 的 Placement 才能呼叫 ApplyPatchAndResolve
- Part B (角色操作) — AArchSimCharacter Enhanced Input 與 Server RPC 模式必須先建立，H2 的 Client→Server 放置 RPC 才能接上
- Part I (教師工具) — H1 的 Listen Server + Spectator 基礎是 I1 教師進入學生世界的前置條件；H3 角色分工 UI 也是 I2 老師全覽的前提

### 風險清單
- VLAN 隔離導致 NULL Subsystem broadcast 失敗 — 需在校園環境實測，直連 IP fallback 是必備保險
- Listen Server Host 在低階教學電腦同時跑 Server+Client+FrameCore Solve — 需加 Solve 佇列 cooldown 3 秒限制每組請求頻率
- 同一幀兩個 Server_RequestPlacement RPC 同時到達導致 OccupancyGrid 雙通過 — RPC handler 必須在 TMap 查詢後立即標記(非 Spawn 後再標記)
- FFrameSolveResult JSON 摘要大小估算未驗證 — 30 人 × 每人 100 構件時封包大小需量測，超過 65KB UE 預設上限需拆包
- UE5.7 Iris replication 與 bAlwaysRelevant 行為需驗證 — 教學 Actor 全設 bAlwaysRelevant 在 30 人場景的頻寬負擔需量測
- SUQS 任務狀態無 built-in replication — 需在 PlayerState 加 replicated UPROPERTY 快照 USuqsProgression
- UArchSimUndoStack 回滾時 Rebaseline 重建耗時 — 大模型(100+ 構件)回滾可能造成明顯卡頓，需非同步佇列 + 載入遮罩
- 角色分工「只想當力學工程師」的教育設計風險 — 任務設計必須讓每個角色缺一不可，否則分工機制形同虛設

## 詳細擴充內容


# Part H 多人協作 — 詳細實作計畫

## 整體定位與前置說明

Part H 實作四個子節:H1 連線架構、H2 共編場景衝突、H3 角色分工、H4 變更紀錄與復原。所有程式碼必須在 `FrameCoreUE`(UE consumer 側)或新建的 `ArchSim` 遊戲模組中實作,**絕不觸碰 `Plugins/FrameSolver/Source/FrameCore/` 引擎核心**(鐵則 #1,FROZEN since v4.0.0)。

技術棧對應主計畫書決策 2(Listen Server + NULL Subsystem LAN)、決策 3(Server-Authoritative FrameCore)、決策 4(Spectator + bAlwaysRelevant)、決策 5(Server 樂觀鎖 OccupancyGrid)。

工時估算依據:1-2 人小團隊,UE5.7 C++/Blueprint 混合開發節奏,扣除 UE incremental build 等待時間(每次 UE build 約 5-20 分鐘)。

---

## H1 連線架構(Listen Server + NULL Subsystem LAN)

**估算工時:30 小時**

### H1-T1:DefaultEngine.ini NULL Subsystem 設定與 SessionInterface 封裝
**工時:2 小時** | **Phase: MVP**

**具體任務:**
- 在 `Config/DefaultEngine.ini` 加入 `[OnlineSubsystem] DefaultPlatformService=Null` 與 `[OnlineSubsystemNull] bEnabled=true`
- 在 `Config/DefaultEngine.ini` 確認 `[/Script/Engine.GameNetworkManager]` 的 `TotalNetBandwidth`、`MaxDynamicBandwidth` 設定不影響 LAN 環境
- 新建 C++ 封裝類 `UArchSimSessionSubsystem : public UGameInstanceSubsystem`，提供 `CreateSession(FName SessionName, int32 MaxPlayers)`、`FindSessions()`、`JoinSession(const FOnlineSessionSearchResult&)` 三個公開方法，隔離 `IOnlineSessionPtr` 的 delegate 地獄

**涉及 UE5 Class/API:**
- `IOnlineSessionPtr`(OnlineSubsystem.h)
- `FOnlineSessionSettings`、`FOnlineSessionSearch`
- `UGameInstanceSubsystem`

**完成標準:**
- PIE 模式下，Host 呼叫 `CreateSession` 後 `OnCreateSessionComplete` delegate 回傳 `bWasSuccessful=true`
- `UArchSimSessionSubsystem` Blueprint 可呼叫版本能在 BP 中直接呼叫 `CreateSession`

**預期踩雷:**
- `OnlineSubsystem NULL` 在 Editor PIE 模式的行為與打包後不同；`FOnlineSessionSettings::bIsLANMatch` 在 PIE 下必須顯式設 true，否則廣播封包不出去
- 需確認 `FOnlineSubsystemNull` Module 已在 `ArchSim.Build.cs` 的 `PublicDependencyModuleNames` 加入 `"OnlineSubsystemNull"` 與 `"OnlineSubsystem"`

---

### H1-T2:Listen Server 建立與 LAN Session 搜尋
**工時:4 小時** | **Phase: MVP**

**具體任務:**
- `UArchSimSessionSubsystem::CreateSession` 實作：`SessionSettings.NumPublicConnections = MaxPlayers`、`SessionSettings.bShouldAdvertise = true`、`SessionSettings.bUsesPresence = false`
- `UArchSimSessionSubsystem::FindSessions` 實作：`SessionSearch->TimeoutInSeconds = 3.0f`、`bIsLanQuery = true`
- Host 建立 Session 後呼叫 `UGameplayStatics::OpenLevel(GetWorld(), MapName, true, "listen")` 啟動 Listen Server
- Client 加入後呼叫 `APlayerController::ClientTravel(URL, ETravelType::TRAVEL_Absolute)`

**涉及 UE5 Class/API:**
- `UGameplayStatics::OpenLevel`
- `APlayerController::ClientTravel`
- `FOnlineSessionSearchResult`

**完成標準:**
- 同一 LAN 內兩台機器，Host 建立 Session 後 Client 可在 `FindSessions` 結果清單中看到該 Session
- Host map 正確進入 Listen Server 模式(URL 含 `?listen`)，Client 成功連線後兩端 `AGameModeBase::PostLogin` 被呼叫

**預期踩雷:**
- LAN broadcast 使用 UDP port 7777(UE 預設)；若學校防火牆封鎖 UDP 7777，Session 廣播找不到；需在部署文件中說明開放 UDP 7777 需求
- `OpenLevel` 第三個參數 `bAbsolute=true` 是 Host 端必須的；若用 `false` 會做 Level streaming 而非真正 Server 啟動

**依賴:** H1-T1

---

### H1-T3:大廳 UMG 介面(ULobbyWidget)
**工時:6 小時** | **Phase: MVP**

**具體任務:**
- 新建 UMG Widget `WBP_Lobby`：左側玩家清單(`UScrollBox` + 動態生成 `WBP_PlayerEntry`)、右側設定面板(關卡選擇 `UComboBoxString`、角色分配按鈕列)、底部「開始遊戲」按鈕(Host 限定可見)
- 新建 `AArchSimLobbyGameState : public AGameStateBase`，加 `UPROPERTY(Replicated) TArray<FArchSimPlayerInfo> Players`，`FArchSimPlayerInfo` 含 `FString PlayerName`、`EArchSimRole AssignedRole`、`bool bReady`
- `AArchSimLobbyGameMode` 的 `PostLogin` 廣播更新 PlayerInfo 到 GameState
- `WBP_Lobby` 監聽 `AArchSimLobbyGameState::Players` 變化更新 UI

**涉及 UE5 Class/API:**
- `UUserWidget`、`UScrollBox`、`UComboBoxString`
- `AGameStateBase`、`UPROPERTY(Replicated)`
- `GetGameState<AArchSimLobbyGameState>()`

**完成標準:**
- Host 開始 Session 後進入大廳；Client 加入後大廳玩家清單即時更新
- 大廳顯示 Session 名稱、在線人數、每個玩家的名稱與準備狀態

**預期踩雷:**
- UMG Widget 在 Server 端不存在(Dedicated Server 無 UI)；Listen Server 模式下 Host 同時是 Client，Widget 在 Host Client 端正常；但要確認 Widget 不在 Server-only code path 中被建立
- `TArray<FArchSimPlayerInfo>` Replicate 需要實作 `GetLifetimeReplicatedProps` 中加 `DOREPLIFETIME(AArchSimLobbyGameState, Players)`

**依賴:** H1-T2

---

### H1-T4:直連 IP fallback 介面
**工時:3 小時** | **Phase: Phase 2**

**具體任務:**
- 大廳介面加「手動輸入 IP」選項：`UEditableTextBox` + 「連線」按鈕
- 點擊後呼叫 `APlayerController::ClientTravel("192.168.x.x", TRAVEL_Absolute)`
- IP 格式驗證：正規表達式 `^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$`

**完成標準:**
- 在 VLAN 隔離環境(LAN broadcast 無效)下，手動輸入 Host IP 後可成功連線

**依賴:** H1-T3

---

### H1-T5:組別分配 UI + 組別管理 GameState
**工時:6 小時** | **Phase: Phase 2**

**具體任務:**
- 在 `AArchSimLobbyGameState` 加 `TMap<int32, TArray<FArchSimPlayerInfo>> Groups`(最多 8 組)
- 大廳加「建立組別」/「加入組別」UI；Host 可拖拉學生到組別欄位(或下拉選單分配)
- 每組預設 4-6 人；超過上限時介面提示

**完成標準:**
- Host 可分配所有學生到組別；Client 端 UI 即時反映分組結果
- `Groups` Replicate 到所有 Client 後，進入遊戲的每個 PlayerController 能取得自己所屬組別 ID

**依賴:** H1-T3

---

### H1-T6:部署文件 + 壓力測試腳本
**工時:4 小時** | **Phase: Phase 2**

**具體任務:**
- 撰寫 `docs/MULTIPLAYER_DEPLOYMENT.md`：開放 UDP 7777、同一子網確認步驟、VLAN 設定說明
- 用 UE 內建 `ProjectLauncher` 或 shell script 模擬 15 Client 同時連線壓力測試
- 量測 Host 機在 15 Client + FrameCore Solve 佇列下的 CPU 使用率

**完成標準:**
- 在 15 Client 同時連線的情況下 Host 端 FPS 不低於 30
- 文件覆蓋學校 IT 部門需要設定的所有網路項目

**預期踩雷:**
- UE5 PIE 多客戶端模擬使用同一台機器，不能反映真實多機網路行為；壓力測試需要真實多台機器或 Docker 虛擬機環境

**依賴:** H1-T2

---

## H2 共編場景與構件放置衝突(Server 樂觀鎖)

**估算工時:38 小時**

### H2-T1:Server 端 OccupancyGrid + UArchSimPlacementSubsystem
**工時:4 小時** | **Phase: MVP**

**具體任務:**
- 新建 `UArchSimPlacementSubsystem : public UWorldSubsystem`(Server-only，`ShouldCreateSubsystem` 檢查 `GetWorld()->GetNetMode() != NM_Client`)
- 核心資料結構：`TMap<FIntVector, FArchSimOccupancyEntry> OccupancyGrid`，`FArchSimOccupancyEntry` 含 `APlayerController* OwnerController`、`int32 PrefabId`、`FDateTime PlacedAt`
- 實作 `bool TryOccupy(FIntVector GridPos, APlayerController* Requester, int32 PrefabId)` — 查詢 TMap 後原子標記，若已佔用回傳 false
- 格子尺寸：預設 100cm × 100cm × 100cm(與 A1 Part 的 FrameCore 節點格子對齊)

**涉及 UE5 Class/API:**
- `UWorldSubsystem`
- `TMap<FIntVector, ...>`
- `GetWorld()->GetNetMode()`

**完成標準:**
- 呼叫 `TryOccupy` 兩次同一格，第二次回傳 false
- `OccupancyGrid` 在 Server 端正確維護；`Num()` 隨放置/移除增減

---

### H2-T2:Client Ghost 預覽系統
**工時:5 小時** | **Phase: MVP**

**具體任務:**
- 新建 `AArchSimGhostActor : public AActor`：`UStaticMeshComponent` + 半透明材質 MI_Ghost(Opacity 0.4)
- Ghost 跟隨滑鼠 Cursor Hit 在 Grid Snap 位置移動(`FIntVector SnapToGrid(FVector WorldPos)` helper)
- 自己的 Ghost 用 `FLinearColor::Green`；組員 Ghost 透過 `Multicast_UpdateGhostTransform(PlayerControllerId, GridPos)` Replicate 給所有 Client，用 `FLinearColor::Blue`
- 衝突格(已佔用)：Ghost 變紅 `FLinearColor::Red`

**涉及 UE5 Class/API:**
- `UStaticMeshComponent::SetCustomPrimitiveDataVector4` 或 `UMaterialInstanceDynamic::SetVectorParameterValue`
- `APlayerController::GetHitResultUnderCursorByChannel`
- UE NetMulticast UFUNCTION

**完成標準:**
- Client 移動滑鼠時 Ghost 正確跟隨 Grid Snap 位置
- 兩個 Client 同時移動 Ghost 時，每個 Client 都能看到對方的藍色 Ghost
- 已佔用格的 Ghost 正確顯示紅色

**預期踩雷:**
- Multicast Ghost 更新如果每幀廣播會造成頻寬爆炸；建議 `NetUpdateFrequency = 15.0f` 並在滑鼠停止移動 200ms 後停止廣播

---

### H2-T3:Server_RequestPlacement RPC + 衝突仲裁
**工時:6 小時** | **Phase: MVP**

**具體任務:**
- 在 `AArchSimCharacter` 或 `AArchSimPlayerController` 加 `UFUNCTION(Server, Reliable) void Server_RequestPlacement(FIntVector GridPos, int32 PrefabId, FRotator Rotation)`
- Server handler 流程：① 呼叫 `UArchSimPlacementSubsystem::TryOccupy` ② 若成功：SpawnActor(`PrefabId` 對應 Actor class)、呼叫 `UArchSimModelRegistry::RegisterMember`、呼叫 `UFrameInteractiveSubsystem::ApplyPatchAndResolve` ③ 若失敗：呼叫 `Client_PlacementFailed(EPlacementFailReason::OccupiedByOtherPlayer)` 回傳 Client
- `Client_PlacementFailed` 觸發 Client 端：Ghost 變紅閃爍 → 消失 + HUD Toast 訊息「該位置已被組員放置」

**涉及 UE5 Class/API:**
- `UFUNCTION(Server, Reliable)` / `UFUNCTION(Client, Reliable)`
- `UWorld::SpawnActor<>()`
- `UFrameInteractiveSubsystem::ApplyPatchAndResolve`(需確認 FrameCore UE consumer API，不動引擎)

**完成標準:**
- 兩個 Client 同時點擊同一格，確定只有一個成功放置(通過 15 次並發測試)
- 失敗 Client 的 Toast 訊息在 200ms 內顯示
- 放置成功後 FrameCore 被正確呼叫(可通過 log `ApplyPatchAndResolve called` 驗證)

**預期踩雷:**
- 「同一幀兩個 RPC」在 UE Server 實際是依序處理(UE 不是多執行緒 RPC 處理)，但佇列到達順序不保證；需在 `TryOccupy` 內部使用 `FCriticalSection` 或確認 UE Game Thread 已保證序列化

**依賴:** H2-T1, H2-T2, H1-T2

---

### H2-T4:FrameCore Solve 結果同步(GameState Replicated JSON 摘要)
**工時:7 小時** | **Phase: Phase 2**

**具體任務:**
- 在 `AArchSimGameState` 加 `UPROPERTY(ReplicatedUsing=OnRep_SolveResult) FString SolveResultJson`(JSON 摘要字串)
- Server Solve 完成後，從 `FFrameSolveResult` 提取必要欄位：`MemberUtilization[]`(float array)、`bSingular`(bool)、`GoverningMemberIdx`(int32)、Sequence number(int32)，序列化為 JSON string
- `OnRep_SolveResult` 在所有 Client 端觸發：解析 JSON → 更新本地 `AArchSimMemberActor` 的 `CachedUtilization` → 呼叫熱圖 Actor 更新
- 需量測 JSON 摘要大小：100 根梁的 `MemberUtilization[]` = 100 × 4 bytes(float) ≈ 400 bytes，JSON 序列化後估約 1.5KB；需確認不超過 UE 預設封包大小限制(65KB per 封包)

**涉及 UE5 Class/API:**
- `UPROPERTY(ReplicatedUsing=...)` + `OnRep` callback
- `UE::Json::TJsonWriter`(或 `FJsonSerializer`)
- `DOREPLIFETIME(AArchSimGameState, SolveResultJson)`

**完成標準:**
- Server Solve 完成後 2 秒內所有 Client 端熱圖更新
- 100 根梁的場景下 JSON 摘要大小 < 10KB(量測驗證)

**預期踩雷:**
- `FString` Replicate 大小有上限；若超過需改為 `TArray<uint8>` 二進制壓縮(zlib)

**依賴:** H2-T3

---

### H2-T5:Client 端本地預測 + Server Reconcile 流程
**工時:5 小時** | **Phase: Phase 2**

**具體任務:**
- Client 送出 `Server_RequestPlacement` 後立即在本地顯示「預測放置」(半透明，Opacity 0.6)
- 收到 Server Spawn Replicate 後，若位置一致：本地預測 Actor 消失，使用 Server 的 Actor；若不一致(Server 拒絕)：本地預測 Actor 移除 + Toast

**完成標準:**
- 網路延遲 100ms 環境下，放置響應感受 < 50ms(因為本地預測立即顯示)

**依賴:** H2-T3

---

### H2-T6:移除構件 RPC + OccupancyGrid 釋放
**工時:4 小時** | **Phase: Phase 2**

**具體任務:**
- `Server_RequestRemoval(FIntVector GridPos)` RPC
- Server 驗證請求者是否為 `OccupancyEntry.OwnerController`(防止隨意刪除他人構件)
- 驗證通過後：`OccupancyGrid.Remove(GridPos)`、`DestroyActor`、`UArchSimModelRegistry::DeactivateMember`、`ApplyPatchAndResolve`

**完成標準:**
- 只有放置者(或老師)可以移除構件；其他人嘗試移除收到 Toast「只有組員或老師可以移除此構件」

**依賴:** H2-T3

---

## H3 角色分工(力學/圖學/材料/施工)

**估算工時:36 小時**

### H3-T1:EArchSimRole 定義 + UArchSimRoleSubsystem 骨架
**工時:3 小時** | **Phase: MVP**

**具體任務:**
- 在 `ArchSimTypes.h` 定義 `UENUM(BlueprintType) enum class EArchSimRole : uint8 { None, Mechanical, Drafting, Materials, Construction, Teacher }`
- 新建 `UArchSimRoleSubsystem : public UGameInstanceSubsystem`
- 核心 API：`void AssignRole(APlayerController*, EArchSimRole)` (Server-only Validated UFUNCTION)、`EArchSimRole GetRole(APlayerController*)` (任意端可呼叫)
- 角色資訊存在 `APlayerState` 的 `UPROPERTY(Replicated) EArchSimRole CurrentRole`

**涉及 UE5 Class/API:**
- `APlayerState`、`UPROPERTY(Replicated)`
- `UGameInstanceSubsystem`

**完成標準:**
- `AssignRole` 在 Server 執行後，對應 PlayerState 的 `CurrentRole` 在所有 Client 端同步更新

---

### H3-T2:四角色 UI 隔離實作
**工時:8 小時** | **Phase: Phase 2**

**具體任務:**
- **力學工程師 UI**：完整 D/C 熱圖控制面板(開關熱圖、顯示數值、切換分析類型)、`AFrameUtilizationHeatmapActor` 控制
- **圖學設計師 UI**：標註工具列(直線/弧長/角度標記，`AAnnotationActor`)、投影視角切換(正視/側視/俯視)
- **材料專家 UI**：材料庫面板(呼叫 `UFrameMaterialLibrary` + `UFrameRCMaterialHelper`)、截面選擇器
- **施工監督 UI**：工序進度面板(對應 Part E 施工狀態機狀態顯示)、品質警告清單
- 每個角色 `CurrentRole` 變化時呼叫 `UUserWidget::SetVisibility` 切換對應 Widget Panel 顯示/隱藏

**涉及 UE5 Class/API:**
- `UUserWidget::SetVisibility(ESlateVisibility)`
- `UFrameMaterialLibrary`(已有，FrameCoreUE 暴露)
- Blueprint Interface `BPI_ArchSimRoleUI::OnRoleChanged`

**完成標準:**
- 力學工程師看不到材料專家的材料庫面板；材料專家看不到 D/C 熱圖控制面板
- 老師端(Teacher 角色)能看到所有四角色 UI

**預期踩雷:**
- Widget Panel 的顯示/隱藏是 Client-local 操作，不需要 Replicate；但角色分配本身需要 Replicate
- 注意「UFunction 不能用 ESlateVisibility 直接作為參數」這個 UHT 限制，需要用 uint8 轉型

**依賴:** H3-T1

---

### H3-T3:角色分配 RPC + AssignRole Server-only 驗證
**工時:4 小時** | **Phase: Phase 2**

**具體任務:**
- 大廳介面的「分配角色」按鈕觸發 `Server_RequestAssignRole(APlayerController* Target, EArchSimRole Role)` RPC
- Server 驗證：只有 `EArchSimRole::Teacher` 或 Host(大廳中的首位玩家)可分配角色給他人
- 自我分配(點選自己的角色)不需驗證，但每人同時只能持有一個角色
- 分配後回呼 `Client_OnRoleAssigned(EArchSimRole)` 通知 UI 刷新

**完成標準:**
- 學生 A 不能將學生 B 的角色改為 Teacher
- Host/老師分配角色後，被分配學生的 UI 在 300ms 內切換

**依賴:** H3-T1, H1-T3

---

### H3-T4:組內語音/文字頻道
**工時:6 小時** | **Phase: Phase 2**

**具體任務:**
- 文字頻道：新建 `WBP_ChatPanel` UMG Widget，`Server_SendChat(FString Sender, FString Message, int32 GroupId)` RPC → `Multicast_ReceiveChat(...)` 廣播給同組玩家
- 訊息過濾：同組玩家才收到(對比 GroupId)
- 語音頻道(Phase 2 預留)：API 留空 stub `void JoinVoiceChannel(int32 GroupId)` 待接 `PixoVoice` 或 `EOS Voice`

**涉及 UE5 Class/API:**
- UMG `UMultiLineEditableTextBox`(聊天訊息顯示)
- `UFUNCTION(Server, Reliable)` / `UFUNCTION(NetMulticast, Unreliable)`

**完成標準:**
- 組內文字訊息只有同組可見；跨組訊息不外洩
- 老師發送的訊息對所有學生可見(跨組廣播)

**預期踩雷:**
- Multicast + GroupId 過濾：NetMulticast 是「所有 Client 都收到」，需在 Client 端 `Multicast_ReceiveChat` handler 中再次檢查 GroupId 是否符合

**依賴:** H1-T5

---

### H3-T5:老師全顯模式 + 任意角色視角切換
**工時:6 小時** | **Phase: Phase 3**

**具體任務:**
- `ATeacherPlayerController` 繼承自標準 PlayerController，加 `bool bShowAllRoleUIs`
- 老師端 HUD 加「切換到角色視角」下拉選單：選擇組別 + 角色後，UI 切換成對應角色的工具面板
- 老師看到的四個角色工具面板全部展開(Tabbed Panel 形式)

**完成標準:**
- 老師可以在不影響學生視角的情況下，在自己畫面上切換四種角色工具檢視
- 老師視角的任何操作(查看數據)不觸發 Server_RequestPlacement 等改動世界的 RPC

**依賴:** H3-T2, H3-T3

---

### H3-T6:角色任務目標追蹤
**工時:4 小時** | **Phase: Phase 2**

**具體任務:**
- 每個角色有獨立的任務目標清單(連接 Part F 的 SUQS DataTable)
  - 力學工程師：「讓所有梁的 D/C < 0.8」
  - 圖學設計師：「完成平面圖的 3 個尺寸標註」
  - 材料專家：「使用 RC 複合材料完成柱體」
  - 施工監督：「所有 RC 柱完成養護工序」
- `UArchSimRoleSubsystem::OnObjectiveComplete(EArchSimRole, int32 ObjectiveId)` → SUQS `CompleteTask`
- 全組四角色目標都達成時觸發關卡完成

**完成標準:**
- 關卡不在四角色各自完成目標之前通關
- 老師端顯示四角色目標完成進度(0/4 完成)

**依賴:** H3-T3, Part F SUQS 系統

---

## H4 變更紀錄與復原(Server 端 Undo Stack)

**估算工時:34 小時**

### H4-T1:FArchSimChange 結構 + Server 端 UArchSimUndoStack 骨架
**工時:4 小時** | **Phase: MVP**

**具體任務:**
- 定義 `USTRUCT(BlueprintType) FArchSimChange { FIntVector GridPos; EArchSimChangeType ActionType; int32 PrefabId; FFrameMaterial MaterialBefore; FFrameMaterial MaterialAfter; APlayerController* Initiator; FDateTime Timestamp; int32 SequenceId; }`
- `EArchSimChangeType : uint8 { Place, Remove, MaterialChange, SectionChange }`
- 新建 `UArchSimUndoStack : public UWorldSubsystem`：`TArray<FArchSimChange> Stack`(LIFO)、`int32 MaxStackSize = 50`
- `void Push(const FArchSimChange&)`：超過 50 項時移除最老的(dequeue FIFO 語義)
- `bool Peek(FArchSimChange& OutChange)` / `void Pop()`

**涉及 UE5 Class/API:**
- `UWorldSubsystem`
- `TArray<FArchSimChange>`

**完成標準:**
- Push 51 個 Change 後，Stack 大小仍為 50(最老的被丟棄)
- `Peek` 正確返回最後一個 Push 進去的 Change

---

### H4-T2:變更歷史 HUD 面板(時間軸 UI)
**工時:6 小時** | **Phase: Phase 2**

**具體任務:**
- 新建 `WBP_ChangeHistoryPanel` UMG Widget：`UScrollBox` + 動態生成 `WBP_ChangeEntry`
- `WBP_ChangeEntry` 顯示：時間戳(格式 HH:mm:ss)、操作者名稱(PlayerName)、動作描述(如「放置梁 B-03 at (1,2,0)」)
- HUD 右上角「歷史」圖示按鈕切換面板開/關
- Panel 打開時呼叫 `Server_RequestChangeHistory()` RPC → Server 回傳 `TArray<FArchSimChangeSummary>`(輕量版，只含顯示用欄位)

**涉及 UE5 Class/API:**
- `UScrollBox::AddChild`
- `UTextBlock::SetText(FText)`
- `UFUNCTION(Client, Reliable)`

**完成標準:**
- 歷史面板顯示最近 50 個操作，時間倒序排列
- 面板在 400ms 內從 Server 取得資料並渲染

**預期踩雷:**
- `TArray<FArchSimChangeSummary>` 序列化為 RPC 參數需要 `FArchSimChangeSummary` 是 `USTRUCT`；50 個 Summary 的封包大小需估算

**依賴:** H4-T1

---

### H4-T3:投票機制 UArchSimVoteSubsystem
**工時:6 小時** | **Phase: Phase 2**

**具體任務:**
- 新建 `UArchSimVoteSubsystem : public UWorldSubsystem`
- 任意玩家可提議回滾：`Server_ProposeRollback(int32 TargetSequenceId)` → Server 建立投票提案 `FArchSimVoteProposal { ProposalId, TargetSequenceId, TMap<APlayerController*, bool> Votes, FDateTime ExpiresAt }`
- 投票期限 30 秒；到期或半數以上同意則通過
- 通過後廣播 `Multicast_RollbackApproved(int32 TargetSequenceId)`
- Client 端顯示投票 Toast：「學生A 提議回到 09:03，請投票 (Y/N)」(30 秒倒計時)

**涉及 UE5 Class/API:**
- `UWorldSubsystem`
- `GetWorld()->GetTimerManager()` + `FTimerHandle`(30 秒超時)
- `UFUNCTION(NetMulticast, Reliable)`

**完成標準:**
- 4 人組中 3 人同意(多數)後回滾觸發
- 投票 Toast 在 30 秒倒計時結束後自動關閉；未達多數則提案取消

**預期踩雷:**
- 單人沙盒模式(人數=1)投票機制應直接通過，無需等待其他人

**依賴:** H4-T1, H1-T2

---

### H4-T4:回滾 FrameCore 狀態(Rebaseline 重建)
**工時:8 小時** | **Phase: Phase 2**

**具體任務:**
- 投票通過後，Server 端執行回滾：
  1. 找到 `TargetSequenceId` 在 Stack 中的位置，取得需要撤銷的 Change 列表
  2. 逐一還原：Remove → 反向 Place，MaterialChange → 還原 MaterialBefore
  3. 清除 `OccupancyGrid` 對應格子
  4. 清除 `UArchSimModelRegistry` 對應 Member，呼叫 `UFrameInteractiveSubsystem::EndSession()` + `StartSession()` 重建 FrameCore session
  5. 重新呼叫 `Rebaseline()` 從當前模型狀態建立 factorize-once baseline
  6. 廣播 `Multicast_WorldStateReset(TArray<FArchSimPlacementSnapshot>)` 讓所有 Client Destroy 舊 Actor 並重新 Spawn
- 從 `UArchSimPlacementSnapshot` 重建 Client 世界狀態

**涉及 UE5 Class/API:**
- `UFrameInteractiveSubsystem::EndSession()`、`StartSession()`、`Rebaseline()`(FrameCoreUE 已有)
- `UWorld::SpawnActor<>()` / `AActor::Destroy()`
- `UFUNCTION(NetMulticast, Reliable)`

**完成標準:**
- 10 個操作後提議回滾到第 5 個操作，Server 世界狀態正確回到第 5 個操作的時間點
- 回滾完成後 FrameCore Solve 結果與手動建立相同模型的 Solve 結果一致(D/C 差 < 1e-5)
- 回滾耗時(含 Rebaseline)在 100 構件模型下 < 5 秒

**預期踩雷:**
- `Rebaseline` 耗時隨模型大小線性增長；100 構件預計 ~200ms(參考 UFrameInteractiveSubsystem PerfBaseline test ≤ 200ms 基準)；但加上 Actor Spawn/Destroy 及網路廣播，實際耗時可能達 1-2 秒，需顯示「正在還原...」載入遮罩
- `Multicast_WorldStateReset` 帶大型 TArray 可能超過封包大小；若超過需改為分批 RPC

**依賴:** H4-T3, H2-T3

---

### H4-T5:Undo Stack 設定 UI + 跨關卡清空
**工時:3 小時** | **Phase: Phase 3**

**具體任務:**
- 設定選單加「Undo 歷史最大步數」滑桿(10-100，預設 50)
- 關卡切換時(`AArchSimGameMode::HandleMatchHasEnded`) 清空 `UArchSimUndoStack::Stack`
- 單人沙盒(Listen Server 只有 1 人)提供無需投票的直接 Undo 按鍵(快捷鍵 Ctrl+Z)

**完成標準:**
- 進入新關卡後 `Stack.Num() == 0`
- 單人模式 Ctrl+Z 立即執行最後一步 Undo，無需投票

**依賴:** H4-T4

---

### H4-T6:SPUD 存檔整合 — 含變更歷史快照
**工時:4 小時** | **Phase: Phase 3**

**具體任務:**
- 評估 `FArchSimChange` 是否可標記 `UPROPERTY(SaveGame)` 加入 SPUD 自動存檔
- 若可：在 `TArray<FArchSimChange> Stack` 上加 `UPROPERTY(SaveGame)` 並確認 SPUD 能正確序列化
- 若不可(因 `APlayerController*` 裸指標問題)：將 `Initiator` 改為 `FString InitiatorPlayerName` 存名稱而非指標

**完成標準:**
- 存檔後重新載入，變更歷史面板仍顯示存檔前的操作記錄

**預期踩雷:**
- `APlayerController*` 裸指標無法序列化，必須改為 `FString` 或 `int32 PlayerStateId`

**依賴:** H4-T1

---

## 跨 Part 依賴整理

| 依賴方向 | 說明 |
|---|---|
| Part A → H2 | `UArchSimModelRegistry`、`UFrameInteractiveSubsystem` 必須先有骨架 |
| Part B → H2 | `AArchSimCharacter` 的 Server RPC 模式(Enhanced Input → Server_Request...) |
| H1 → H2, H3, H4 | Listen Server Session 是所有多人功能的基礎 |
| H2 → H4 | OccupancyGrid 的 Change 事件是 UndoStack 的資料來源 |
| Part F (SUQS) → H3-T6 | 角色任務目標需要 SUQS DataTable |
| H1, H3 → Part I (教師工具) | Spectator + 角色分工 UI 是 I1 的前提 |

## 技術風險總覽

**高風險:**
1. **同一幀 RPC 衝突**：UE Server 雖然是單執行緒但 RPC 佇列達到時，同一幀兩個 `Server_RequestPlacement` 若都通過 `TryOccupy` 查詢但在 `TMap.Add` 之前：實際上 UE Game Thread 保證序列化，但需要用 Automation Test 驗證實際行為，不能只靠理論推斷。
2. **Rebaseline 效能**：回滾 100 構件場景的 `Rebaseline` + World Rebuild 耗時未實測，是否能在 5 秒內完成需要實際量測。
3. **封包大小**：30 人場景的 `SolveResultJson` + `Multicast_WorldStateReset` 封包大小未量測；若超 65KB 需要拆分批次傳輸。

**中風險:**
4. **教育電腦效能**：假設教學電腦為 i5/i7 + 8-16GB RAM；Listen Server Host 同時跑 Server + Client + FrameCore 在 100 構件場景的效能需要量測。
5. **NULL Subsystem 廣播在 VLAN 環境失敗**：直連 IP fallback 是必備；但 IT 部門若鎖定 UDP port 7777 則連直連也失敗，需要文件說明需求。

**需驗證(不確定):**
6. `UArchSimVoteSubsystem::FTimerHandle` 30 秒後自動處理：需驗證 `FTimerHandle` 在 Server-only WorldSubsystem 中的行為是否正確。



---


# Part I 實作擴充(128h)

**標題**:教師工具 + Log (I1~I4)

**子節數**:4

### MVP 必須(Phase 1)
- I1-T1: ATeacherPlayerController 骨架 + Spectator 狀態切換
- I1-T2: 班級總覽 HUD UI (UMG)
- I2-T1: xAPI Statement 資料結構定義 + 8 個自訂 verb
- I2-T2: UE 端 log 觸發點埋設 + UArchSimLogSubsystem 骨架
- I2-T3: 離線隊列 + LRS HTTP 送出 (本地 SQLite/SQLiteCpp)
- I3-T1: 班級總覽頁 (Class Overview) REST 查詢 + 渲染
- I4-T1: UReflectionWidget UMG 骨架 + 關卡結束觸發

### Phase 2
- I1-T3: 自由相機 (ACameraActor + SetViewTarget)
- I1-T4: 標記系統 AArchSimMarkActor (箭頭/紅圈)
- I1-T5: 私訊彈窗 + 老師觀察提示 HUD
- I1-T6: bAlwaysRelevant 教學 Actor 全套設定 + 壓力測試
- I2-T4: Pseudonymization SHA-256 + 個資刪除 API
- I3-T2: 個別學生時間軸頁 (Student Detail)
- I3-T3: 失敗模式分析頁 (Error Pattern Analysis)
- I4-T2: DA_ReflectionPrompts DataAsset 20 題 + 隨機抽題
- I4-T3: 班級匿名對比 (aggregation query to LRS)

### Phase 3
- I1-T7: 老師假設測試模式 (暫時 deactivate 構件不影響學生)
- I1-T8: Tab 快速切換學生 + 縮圖 SceneCapture
- I2-T5: Background Sync / visibilitychange fallback + 離線佇列監控
- I3-T4: CSV/PDF 匯出班級報告
- I3-T5: LRS 查詢效能優化 (index + 分頁)
- I4-T4: 反思與學習歷程檔案整合 (匯出 PDF)

### 實作順序
- I2-T1 → I2-T2 → I2-T3 (先建 log 地基)
- I1-T1 → I1-T2 (Spectator 骨架 + 班級 HUD)
- I1-T3 → I1-T4 → I1-T5 (相機 + 標記 + 私訊)
- I1-T6 (bAlwaysRelevant 壓力測試)
- I2-T4 (個資合規補齊)
- I3-T1 → I3-T2 → I3-T3 (Dashboard 三頁)
- I4-T1 → I4-T2 → I4-T3 (反思工具)
- I1-T7 → I1-T8 (進階教師功能)
- I2-T5 → I3-T4 → I3-T5 → I4-T4 (Phase 3 優化)

### 跨 Part 依賴
- Part G (多人協作 Listen Server 必須先完成):I1 Spectator 依賴 APlayerController 多人架構、ServerViewNextPlayer/PrevPlayer RPC 在有效 Listen Server 環境才能運作
- Part A (引擎接合層 UArchSimModelRegistry 必須先完成):I2 log 需從 FFrameSolveResult 讀取 dc-ratio / max-displacement-mm / failure-mode,這些欄位由 Part A DistributeSolveResult 路由
- Part B (ALS 角色 + Enhanced Input 必須先完成):I1 老師用 ATeacherPlayerController 繼承同一 Input 框架,教師 MappingContext 要掛在 ALS 體系上
- Part C (SolveLinear / 互動重解流程必須有基本骨架):I2 verb=ran-simulation 需要 OnSolveComplete callback 觸發點,callback 在 Part C 的 UArchSimModelRegistry::Solve 流程中
- Part D (SUQS 任務系統必須有基本骨架):I4 反思工具在 SUQS quest complete 事件後彈出,依賴 Part D 的任務狀態機

### 風險清單
- [高] bAlwaysRelevant=true 對 100+ 構件 Actor 造成複製洪流:30 學生各有 50+ 教學 Actor 全部 bAlwaysRelevant,可能超過 UE Net 封包預算。緩解:搭配 SetNetDormancy(DORM_Dormant),只在 Actor 狀態改變時喚醒複製;老師 Spectator 時只 relevant 當前焦點學生的 Actor (需驗證 IsNetRelevantFor 覆寫)
- [高] UE5 Listen Server 模式下 ServerViewNextPlayer() / ServerViewPrevPlayer() 只能切換 Server 已知 Pawn;若老師機是 Host,行為正確;若老師用 Client 連入,需透過 Server RPC 代理,UE 內建函數在 Client-side 呼叫行為需驗證
- [中] xAPI 規範 (xAPI 1.0.3) 在 UE 環境無現成 C++ SDK;需自行實作 Statement 組裝 + HTTP/1.1 POST (可用 UE FHttpModule),或走外部 TypeScript sidecar 送 xAPI。工時估算已含自行實作 HTTP client,但若 sidecar 方案不穩定需預備 C++ fallback
- [中] SQLite 在 UE 的整合:UE 沒有內建 SQLite plugin,需引入第三方 plugin (如 SQLiteSupport plugin from Marketplace) 或自行 vendor SQLiteCpp。Marketplace plugin 授權合規性需驗證 (Apache 2.0 vs 商業授權)
- [中] SceneCapture2D 縮圖 (班級總覽 30 人縮圖):每個學生一個 USceneCaptureComponent2D 持續 render 會大量消耗 GPU。降級方案:縮圖用靜態截圖(每 10 秒更新一次,不是實時),或只在老師點擊時才 capture
- [低] 台灣個資法 pseudonymization 邊界:遊戲操作行為 log 是否屬「間接個資」目前無司法判例,保守設計為全程加密 + SHA-256 pseudonymization;但 salt 存學校後台的機制需要學校 IT 配合部署,MVP 可簡化為 device-local UUID
- [低] Background Sync API 僅 Chrome/Edge 支援,Safari 不支援;UE 打包後通常走 native app 不走瀏覽器,此風險只在「老師端 dashboard 走 web app」時才存在;dashboard 若用 UMG 內建 WebBrowser widget 則另論

## 詳細擴充內容


# Part I 詳細實作計畫 — 教師工具 + Log

> Part I 是「建築師模擬器」整套多人教育環境的「老師視角基礎建設」,由四個子節組成:
> - **I1** 教師進入學生世界(Spectator + Free Camera)
> - **I2** 學習 Log Schema(xAPI 1.0.3 + 自訂 Profile)
> - **I3** 老師端 Dashboard(班級 / 個別 / 模式分析)
> - **I4** 學生反思工具
>
> 本計畫依照「1-2 人小團隊、實際開發節奏」估算工時,工時包含:設計、C++/UMG 實作、UE 編譯等待、測試、踩雷除錯。不過度樂觀。

---

## I1 教師進入學生世界(Spectator + Free Camera)

### 子節概念摘要

老師以 **Spectator 身份**進入任一學生正在操作的 UE 世界,可觀察、可標記、可私訊,但絕不控制學生 Pawn。主要技術基礎是 UE 內建 `ASpectatorPawn` + `APlayerController::ChangeState(NAME_Spectating)` + `ServerViewNextPlayer()`,以及對教學類 Actor 設定 `bAlwaysRelevant = true` 克服網路距離截斷。

### sub-task 清單

#### I1-T1 ATeacherPlayerController 骨架 + Spectator 狀態切換
**階段**: MVP(Phase 1)
**工時**: 3 小時
**涉及 UE5 class/API**:
- `APlayerController` 繼承 → 新增 `ATeacherPlayerController : public APlayerController`
- `APlayerController::ChangeState(NAME_Spectating)` 切換到 Spectator 狀態
- `APlayerController::ServerViewNextPlayer()` / `ServerViewPrevPlayer()` (UE 內建,需驗證 Client-side 呼叫行為)
- `APlayerController::SetViewTargetWithBlend()` 用來平滑切換相機目標

**具體 task**:
1. 新建 `ATeacherPlayerController.h/.cpp`,在 `AGameModeBase::PreLogin()` 中根據 bIsTeacher 旗標指派給老師
2. 老師 BeginPlay 時自動呼叫 `ChangeState(NAME_Spectating)`
3. 實作 `TeacherViewPlayer(int32 PlayerIndex)` Server RPC,在 Server 端 `SetViewTargetWithBlend(TargetPawn, 0.5f)` 
4. 鍵盤 Tab 鍵 → `ServerViewNextPlayer()`(Enhanced Input `IA_TeacherNextPlayer`)

**完成標準**:
- [ ] 老師連線後自動進入 Spectator 狀態,不出現玩家 Pawn
- [ ] Tab 可切換到下一位學生視角,切換動畫 0.5 秒 blend
- [ ] 切換後老師看到的位置是目標學生 Pawn 的位置
- [ ] 不能切換到老師自己的舊 Pawn

**預期踩雷**:
- `ServerViewNextPlayer()` 是 UFUNCTION(Server) 但需要呼叫端 Authority,純 Client 呼叫可能 silent fail。**對策**:包裝為 `Server_RequestViewNextPlayer_Implementation` 在 Server 端呼叫真實切換函數
- GameMode 識別老師機:Listen Server 情況下老師機同時是 Host,需用 `NetConnection == nullptr` 判定本機玩家,再看是否啟動時帶 `-teacher` 命令列參數

---

#### I1-T2 班級總覽 HUD UI (UMG)
**階段**: MVP(Phase 1)
**工時**: 4 小時
**涉及 UE5 class/API**:
- `UUserWidget` → 新增 `UClassOverviewWidget`
- `UImage` / `UTextBlock` / `UScrollBox` / `UVerticalBox`(UMG 基本控件)
- `USceneCaptureComponent2D` + `UTextureRenderTarget2D` 用於學生縮圖(Phase 2 才做,MVP 用佔位圖)
- `AGameStateBase::PlayerArray` 遍歷所有玩家

**具體 task**:
1. 設計 UMG Blueprint `WBP_ClassOverview`:30 格網格,每格顯示學生名 + 狀態燈(綠/黃/紅)
2. C++ `UClassOverviewWidget::RefreshStudentStatus()`:遍歷 `GetGameState()->PlayerArray`,讀取每個 `AArchSimPlayerState::CurrentStatus` (Enum: OK/Stuck/Failed)
3. 老師按 `IA_OpenClassOverview`(快捷鍵 `~`)顯示/隱藏 HUD
4. 點擊學生格子 → 呼叫 I1-T1 的 `TeacherViewPlayer()`

**完成標準**:
- [ ] 班級總覽 UI 顯示 30 個學生格,每格有名稱 + 狀態顏色
- [ ] 狀態顏色每 5 秒自動更新(不是實時,減少 replication 壓力)
- [ ] 點擊任一格子 2 秒內切換到該學生視角
- [ ] UI 可在 1920x1080 與 2560x1440 解析度正確顯示(測試兩種)

**預期踩雷**:
- `AGameStateBase::PlayerArray` 在 Client 端可能不完整(只有已 relevant 的 PlayerState);需用 `APlayerState::bIsSpectator` 過濾掉老師自己
- 30 格 UI 在低解析度可能超出螢幕,加 `UScrollBox` 包覆

---

#### I1-T3 自由相機 (Free Camera)
**階段**: Phase 2
**工時**: 3 小時
**涉及 UE5 class/API**:
- `ACameraActor` 動態 Spawn
- `APlayerController::SetViewTarget(CameraActor)` + `SetViewTargetWithBlend()`
- Enhanced Input `IA_TeacherFreeMove` (WASD + QE 上下 + 滑鼠右鍵旋轉)

**具體 task**:
1. 老師按 F 鍵 → Spawn `ACameraActor` 在目前視角位置,`SetViewTarget(CamActor)`
2. Enhanced Input 加 `MC_TeacherFreecam` MappingContext,WASD + QE 移動相機
3. 相機速度可用滑鼠滾輪調整(10cm/s ~ 1000cm/s)
4. 再按 F 或 Tab → 離開 Free Camera 模式,回到 Spectator

**完成標準**:
- [ ] Free Camera 漫遊無碰撞(穿牆穿地)
- [ ] 移動速度可用滾輪調整,有 HUD 顯示當前速度
- [ ] 離開 Free Camera 後回到正確的學生 Spectator 視角

**預期踩雷**:
- `ACameraActor` Spawn 在 Server 時需 Replicate 給 Client 的老師(老師是 Client 不是 Server);**或者在 Client Local 端 Spawn 不 Replicate**。Free Camera 純屬老師本機體驗,不需要 Server 知道,選 Client-side LocalSpawn

---

#### I1-T4 標記系統 AArchSimMarkActor
**階段**: Phase 2
**工時**: 4 小時
**涉及 UE5 class/API**:
- 新增 `AArchSimMarkActor : public AActor`
- `UNiagaraComponent` 或 `UStaticMeshComponent` 渲染箭頭/紅圈
- `UPROPERTY(Replicated)` + `GetLifetimeReplicatedProps()` 讓所有 Client 可見
- `FTimerHandle` 自動消失計時器(預設 30 秒後消失)

**具體 task**:
1. `AArchSimMarkActor` 有兩種 MarkType:`Arrow` / `Circle`,對應不同 Static Mesh
2. 老師按 M 鍵 → 從老師視線 LineTrace 到世界,Server_SpawnMark() RPC
3. Server 端 Spawn `AArchSimMarkActor`(自動 Replicate),所有學生看得到
4. Mark Actor 設 `bAlwaysRelevant = true`(確保距離截斷不影響可見性)
5. 30 秒自動 Destroy,或老師按 Backspace 清除所有標記

**完成標準**:
- [ ] 老師按 M 後 0.5 秒內所有 Client 看到標記
- [ ] 箭頭 / 紅圈兩種形狀正確顯示
- [ ] 30 秒後自動消失
- [ ] 學生畫面右上角顯示「老師正在觀察」圖示(觸發時機:老師 Spawn 第一個 Mark 或切換到該學生視角時)

**預期踩雷**:
- Mark 的材質需要雙面或半透明(在建築結構中間可見);用 `TranslucentSortPriority` 避免排序問題
- 老師標記頻率需限制(每分鐘最多 5 個 Mark),防止學生被標記洪流困擾

---

#### I1-T5 私訊彈窗 + 老師觀察提示 HUD
**階段**: Phase 2
**工時**: 2 小時
**涉及 UE5 class/API**:
- `UUserWidget` → `WBP_TeacherMessage`(彈窗)
- `APlayerController::ClientMessage()` 或自訂 `Client_ShowTeacherMessage(FString)` RPC
- `UCanvasPanel` + `UBorder` 做半透明彈窗

**具體 task**:
1. `ATeacherPlayerController::SendMessageToStudent(AArchSimPlayerController* Target, FString Msg)` → Client RPC 到 Target
2. 學生端 Client_ShowTeacherMessage_Implementation → 顯示 `WBP_TeacherMessage` 5 秒
3. 「老師正在觀察」小圖示:老師 Spectate 該學生時 → Server broadcast 給該學生的 PlayerState `bTeacherWatching = true` (Replicated) → 學生 HUD 監聽此值

**完成標準**:
- [ ] 老師發訊息 → 學生畫面彈窗 5 秒,字體清晰可讀
- [ ] 老師 Spectate 時學生右上角顯示「老師正在觀察」圖示
- [ ] 老師離開時圖示消失

---

#### I1-T6 bAlwaysRelevant 教學 Actor 全套設定 + 壓力測試
**階段**: Phase 2
**工時**: 3 小時
**涉及 UE5 class/API**:
- `AActor::bAlwaysRelevant = true` (C++ 設定或 BP Details)
- `AActor::SetNetDormancy(DORM_Dormant)` + `FlushNetDormancy()` 在狀態更新時喚醒
- `AFrameDeformedShapeActor` / `AFrameUtilizationHeatmapActor` / `AFrameModalShapeActor` / `AFrameDynCollapseReplayActor` / `AFrameFragmentClusterActor` / `AFrameInfluenceLineActor` / `AFrameResponseSpectrumActor` / `AFrameRealTimeDynamicActor`(8 個 BP Actor)

**具體 task**:
1. 對 8 個 BP 教學 Actor 逐一設定 `bAlwaysRelevant = true`(在各自 C++ constructor 設定,不在 BP 設定,避免 Blueprint 重置)
2. 設定 `SetNetDormancy(DORM_Dormant)` 為 default,只在 Actor 需要更新時 `FlushNetDormancy()`
3. 壓力測試:30 個模擬 PlayerState + 各 10 個教學 Actor(共 300 個 bAlwaysRelevant Actor)→ 用 `stat net` 觀察 Bunch 數量
4. 若 stat net 顯示每幀超過 50 個 Bunch 更新,改用 `IsNetRelevantFor()` 覆寫:只對目前 Spectate 焦點學生的 Actor relevant

**完成標準**:
- [ ] 老師在任意距離都能看到學生場景的 8 個教學 Actor
- [ ] 壓力測試 300 個 Actor 時 Server 幀率不低於 30fps (量測用 `stat fps`)
- [ ] `stat net` 顯示 Bunches 在 30 人場景不超過合理值(需驗證具體上限)

**預期踩雷**:
- **這是 I1 最大風險**:300 個 bAlwaysRelevant Actor 可能是 UE replication 效能災難。若壓力測試失敗,必須實作 `IsNetRelevantFor()` 覆寫,只讓老師當前 Spectate 的學生的 Actor 複製

---

#### I1-T7 老師假設測試模式(暫時 deactivate 構件不影響學生)
**階段**: Phase 3
**工時**: 4 小時
**涉及 UE5 class/API**:
- `UFrameInteractiveSubsystem::ApplyPatchAndResolve()` 配合 `FFrameModelPatch`
- 需在 UE consumer 層新增 `UArchSimTeacherTestSubsystem`(不改 FrameCore engine)

**具體 task**:
1. 老師 Ctrl+點擊學生構件 → 進入「假設測試模式」
2. 本地端(老師的 UArchSimTeacherTestSubsystem)複製當前 FFrameModelDef 快照
3. 在快照上 deactivate 選取構件 → 執行 SolveLinear → 顯示假設熱圖(只老師看得到)
4. 學生端模型不受影響(老師的假設測試完全是 Client-local 計算)
5. 老師可用「分享假設結果」按鈕 → Client RPC 把假設熱圖截圖送給學生

**完成標準**:
- [ ] 老師假設測試不改變學生實際模型
- [ ] 假設熱圖只顯示在老師畫面
- [ ] 假設測試 Solve 在 2 秒內完成(教育用場景構件數不超過 500)
- [ ] 老師可以「分享截圖」給學生(Server RPC 送 texture)

**預期踩雷**:
- Client-side Solve 需要 FrameCore DLL 在 Client 機器存在,UE 打包時要確認 `FrameSolver` Plugin 在 packaged build 的 Client 端
- 假設結果的「分享截圖」傳送 texture 封包大小可能超 UE limit,需壓縮後分塊傳送

---

#### I1-T8 Tab 快速切換學生 + 縮圖 SceneCapture
**階段**: Phase 3
**工時**: 3 小時
**涉及 UE5 class/API**:
- `USceneCaptureComponent2D` + `UTextureRenderTarget2D`
- `UImage::SetBrushFromTexture()` 更新 UMG 縮圖

**具體 task**:
1. 每個學生 Pawn 上掛一個 `USceneCaptureComponent2D`,每 10 秒 Capture 一次渲染到 RenderTarget(128×72 低解析度)
2. 老師班級總覽 UI 用 `UImage` 顯示各學生縮圖
3. 縮圖 RenderTarget 透過 ReplicatedProperties 傳給老師 Client(需評估大小,128×72 RGBA = 36KB,可接受)

**完成標準**:
- [ ] 班級總覽顯示每學生即時縮圖(10 秒更新一次)
- [ ] 縮圖大小 128×72,顯示清晰
- [ ] 30 個 SceneCapture 同時存在時 Server GPU 負載增量 < 10%

**預期踩雷**:
- 30 個 SceneCapture2D 同時每幀 Capture 是效能殺手。**必須**用 Staggered Capture(每幀只 Capture 1-2 個,30 個在 15-30 秒內輪完)+ `bCaptureEveryFrame = false`

---

### I1 小計

| Task | 階段 | 工時 |
|---|---|---|
| T1 Spectator 骨架 | MVP | 3h |
| T2 班級總覽 HUD | MVP | 4h |
| T3 自由相機 | Phase 2 | 3h |
| T4 標記系統 | Phase 2 | 4h |
| T5 私訊 + 觀察提示 | Phase 2 | 2h |
| T6 bAlwaysRelevant 壓力測試 | Phase 2 | 3h |
| T7 假設測試模式 | Phase 3 | 4h |
| T8 縮圖 SceneCapture | Phase 3 | 3h |
| **I1 合計** | | **26h** |

---

## I2 學習 Log Schema(xAPI 1.0.3 + 自訂 Profile)

### 子節概念摘要

「學習 Log」不是分數系統,而是**學生思維過程的時間軸資料**。技術核心是 xAPI 1.0.3 Statement schema + 自訂 ArchSim Application Profile,搭配 UE C++ 端的 `UArchSimLogSubsystem`(GameInstanceSubsystem)統一觸發所有事件,透過 HTTP POST 到本地或雲端 SQL LRS。FrameCore engine 不知道 log 的存在(鐵則 #1 FROZEN),只有 UE consumer 層讀取 `FFrameSolveResult` 的指標寫入 Statement。

### sub-task 清單

#### I2-T1 xAPI Statement 資料結構定義 + 8 個自訂 verb
**階段**: MVP(Phase 1)
**工時**: 3 小時
**涉及 UE5 class/API**:
- 新增 C++ `struct FArchSimXAPIStatement` (USTRUCT, BlueprintType)
- 新增 `UArchSimVerbRegistry` (UObject, CDO 模式) 定義 8 個 verb IRI

**具體 task**:
1. 定義 `FArchSimXAPIStatement`:
   ```
   FString StatementId (UUID v4)
   FArchSimActor Actor   // account.homePage + account.name (pseudonymized)
   FArchSimVerb Verb     // id (IRI) + display.zh-TW
   FArchSimObject Object // id + definition.type
   FArchSimResult Result // dc-ratio, max-displacement-mm, failure-mode, response (反思文字)
   FArchSimContext Context // groupId, lessonId, platform
   FDateTime Timestamp
   ```
2. 定義 8 個 verb IRI(ArchSim Application Profile namespace `https://archsim.edu.tw/xapi/verbs/`):
   - `placed-member` / `removed-member` / `applied-load` / `observed-failure` / `fixed-by-adding` / `asked-for-hint` / `ran-simulation` / `reflected`
3. 實作 `UArchSimLogSubsystem::BuildStatement(EArchSimVerb Verb, FArchSimObject Object, FArchSimResult Result)` 組裝 Statement
4. 實作 Statement → JSON 字串序列化(使用 UE `TSharedPtr<FJsonObject>` + `FJsonSerializer`)

**完成標準**:
- [ ] 8 個 verb 全部有 IRI + 中文 display name
- [ ] `BuildStatement()` 產出的 JSON 通過 xAPI 1.0.3 Statement schema 驗證(用 https://lrs.io/validator 或本地 validator)
- [ ] Pseudonymization 函式 `PseudonymizeStudentId(FString StudentId, FString SchoolSalt)` 用 SHA-256 實作,測試向量通過

**預期踩雷**:
- UE `FString` 轉 SHA-256:UE 自帶 `FMD5`(MD5,不夠)但沒有 SHA-256 實作;需引入 UE 的 `FCryptoHashHelper` 或手動 vendor `openssl/sha.h`。**推薦**:用 UE Engine 路徑下已存在的 `Engine/Source/Runtime/Core/Public/Misc/SecureHash.h` → `FSHA1` (SHA-1,也不夠)。最穩妥方案:直接 vendor `SHA-256` 純 C 實作(~100 行),不引入額外依賴

---

#### I2-T2 UE 端 log 觸發點埋設 + UArchSimLogSubsystem 骨架
**階段**: MVP(Phase 1)
**工時**: 4 小時
**涉及 UE5 class/API**:
- 新增 `UArchSimLogSubsystem : public UGameInstanceSubsystem`
- `UGameInstanceSubsystem::Initialize()` / `Deinitialize()`
- 在 `UArchSimModelRegistry`(Part A)的以下節點觸發 log:
  - `RegisterMember()` → verb=`placed-member`
  - `DeregisterMember()` → verb=`removed-member`
  - `OnSolveComplete(FFrameSolveResult)` → verb=`ran-simulation`(讀取 dc-ratio 等指標)

**具體 task**:
1. `UArchSimLogSubsystem` 實作 `LogEvent(EArchSimVerb, FArchSimObject, FArchSimResult)`,內部呼叫 BuildStatement → EnqueueStatement
2. 在 `UArchSimModelRegistry::RegisterMember()` 末尾呼叫 `LogSubsystem->LogEvent(EArchSimVerb::PlacedMember, ...)`
3. 在 `UArchSimModelRegistry::DistributeSolveResult()` 末尾:若 `Result.DemandSummary.MaxDC > 0` 則 LogEvent(RanSimulation);若 `Result.bSingular` 則 LogEvent(ObservedFailure)
4. 在 SUQS OnQuestComplete 回調中 LogEvent(ObservedFailure, failure-mode from DemandSummary)
5. 在 SUQS OnHintRequested 中 LogEvent(AskedForHint)

**完成標準**:
- [ ] 放置一個構件 → log console 印出 placed-member Statement JSON
- [ ] 跑一次 Solve → log console 印出 ran-simulation Statement,含 dc-ratio 數值
- [ ] Singular 解(機構) → log 印出 observed-failure,failure-mode=instability

**預期踩雷**:
- `UArchSimModelRegistry` 在 Part A 尚未完成時,I2-T2 的觸發點無法接入實際流程;開發期可用 Test-only BlueprintCallable stub 手動觸發
- failure-mode 分類(`buckling` / `yielding` / `instability` / `null`):FrameCore 的 `FFrameSolveResult.DemandSummary` 提供 `governingMemberIdx`(最大 D/C 構件),但「失敗模式分類」本身需在 UE consumer 層做:若 `bSingular` → instability;若 `MaxDC > 1.0` 且對應構件有 `MemberInternalForces.Nxx` 占主導 → buckling(簡化判斷);其餘 → yielding。這個簡化分類邏輯需在 `UArchSimLogSubsystem` 的 helper 函式中明確標注為「教育用簡化,非工程精準分類」

---

#### I2-T3 離線隊列 + LRS HTTP 送出
**階段**: MVP(Phase 1)
**工時**: 4 小時
**涉及 UE5 class/API**:
- `FHttpModule::Get().CreateRequest()` + `IHttpRequest::ProcessRequest()`
- `TQueue<FString>` 或 `TArray<FString>` 作為本地隊列
- `FTimerHandle` 定期 flush 隊列(每 30 秒或累積 10 個 Statement)

**具體 task**:
1. `UArchSimLogSubsystem::EnqueueStatement(FString JsonStatement)`:Push 到 `TArray<FString> PendingStatements`
2. `FlushQueue()`:若 `PendingStatements.Num() > 0`,打包成 xAPI Statements array → HTTP POST 到 LRS URL
3. LRS URL 從 `Config/DefaultGame.ini` 讀取:`[ArchSim.Log] LrsEndpoint=http://localhost:8080/xapi/`
4. HTTP 失敗(timeout / 4xx / 5xx) → Statement 保留在隊列,下次重試(最多 3 次,超過記錄到本地文字 log)
5. `FlushQueue()` 在 `Deinitialize()` 同步呼叫一次,確保遊戲結束不漏送

**完成標準**:
- [ ] 正常連線:Statement 在 30 秒內送達 LRS(curl 到 LRS endpoint 可查到)
- [ ] 斷線狀態:Statement 存在記憶體隊列,重連後自動重送
- [ ] 遊戲關閉前的 Statement 全部被嘗試送出(不漏送)
- [ ] LRS Endpoint 可在 DefaultGame.ini 設定,不硬碼

**預期踩雷**:
- UE Packaged Build 的 FHttpModule 需要在 `Build.cs` 加入 `"HTTP"` Module 依賴
- xAPI 規範要求 `Content-Type: application/json` + `X-Experience-API-Version: 1.0.3` Header,必須正確設定否則 LRS 拒絕
- MVP 的 LRS 選 **SQL LRS (Yet Analytics)**:Yet Analytics 有 Docker image,本地 SQLite 模式可單機部署。**需驗證**: Yet Analytics 的 `/xapi/statements` endpoint 是否完全符合 xAPI 1.0.3 規範,或需其他開源 LRS

---

#### I2-T4 Pseudonymization SHA-256 + 個資刪除 API
**階段**: Phase 2
**工時**: 3 小時
**涉及 UE5 class/API**:
- Vendor SHA-256 純 C 實作(無額外依賴)
- `UArchSimLogSubsystem::RequestDataDeletion(FString PseudonymHash)` → HTTP DELETE to LRS

**具體 task**:
1. Vendor `sha256.h/.c`(~100 行 public domain 實作,如 clibs/sha256)到 `Source/ArchSim/Private/Crypto/`
2. `FString UArchSimLogSubsystem::PseudonymizeId(FString RealId, FString Salt)` 實作並加單元測試(已知向量)
3. 遊戲主選單「申請刪除個資」按鈕 → HTTP DELETE `{LRS}/xapi/statements?agent=...` 或 LRS 廠商特定 API
4. 刪除後本地隊列中的同一 pseudonym 的 Statement 也清除
5. 文檔明確記錄:Salt 由學校管理者設定,存放在 Server 端 DefaultGame.ini,學生端不可見

**完成標準**:
- [ ] PseudonymizeId 測試向量通過(SHA-256 已知 input→output 驗證)
- [ ] 個資刪除請求送達 LRS,LRS 回 200
- [ ] 刪除後對應 pseudonym 的所有 Statement 在 LRS 查詢不到
- [ ] 文檔明確記載 Salt 管理責任在學校 IT

---

#### I2-T5 Background Sync / visibilitychange fallback + 離線佇列監控
**階段**: Phase 3
**工時**: 2 小時
**涉及 UE5 class/API**:
- UE `FCoreDelegates::ApplicationWillDeactivateDelegate` (App 背景化)
- `FCoreDelegates::ApplicationHasReactivatedDelegate` (App 回前台)

**具體 task**:
1. 綁定 `ApplicationWillDeactivateDelegate` → `FlushQueue()` 強制嘗試送出
2. 綁定 `ApplicationHasReactivatedDelegate` → `StartPeriodicFlush()` 重啟定時 flush
3. 佇列深度監控:若 `PendingStatements.Num() > 500`,記錄警告 log 並截斷(防止記憶體溢出)

**完成標準**:
- [ ] App 切換到背景(Alt+Tab 到其他程式)→ Queue 自動 flush
- [ ] 模擬 500+ 離線 Statement:佇列不超過 500 筆,有明確警告 log

---

### I2 小計

| Task | 階段 | 工時 |
|---|---|---|
| T1 xAPI 結構定義 | MVP | 3h |
| T2 觸發點埋設 | MVP | 4h |
| T3 離線隊列 + HTTP | MVP | 4h |
| T4 Pseudonymization + 個資刪除 | Phase 2 | 3h |
| T5 Background Sync fallback | Phase 3 | 2h |
| **I2 合計** | | **16h** |

---

## I3 老師端 Dashboard(班級 / 個別 / 模式分析)

### 子節概念摘要

Dashboard 是老師查看 xAPI 學習紀錄的主介面,分三頁:**班級總覽**、**個別學生時間軸**、**失敗模式分析**。技術上是一個獨立的 Web App(Vue 3 / React,或 UE 的 WebBrowser Widget),透過 REST API 查詢 SQL LRS,再渲染成老師友善的中文介面。

注意:Dashboard 不在 UE 遊戲程式碼內,而是**獨立的 Web 前端 + Node.js/Express 後端 API 層**。這是刻意的:老師可以在任何電腦(不需要安裝遊戲)開瀏覽器看 Dashboard。

### sub-task 清單

#### I3-T1 班級總覽頁 (Class Overview) REST 查詢 + 渲染
**階段**: MVP(Phase 1)
**工時**: 4 小時
**涉及技術**:
- Node.js + Express 後端 API(`GET /api/class/:classId/overview`)
- 查詢 Yet Analytics LRS:`GET /xapi/statements?agent=...&since=...`
- 前端:Vue 3 + Vite(或純 HTML + Chart.js,降級方案)

**具體 task**:
1. Node.js API Server `src/api/classOverview.js`:接收 `?classId=11A&since=2026-06-25T00:00:00Z`,查 LRS aggregation
2. 計算:完成率(placed-member → ran-simulation 無 instability)、平均嘗試次數(ran-simulation 次數/學生)、失敗模式排行(observed-failure groupby failure-mode)
3. 前端 `ClassOverview.vue`:完成率進度條、學生進度熱圖(30 格)、失敗模式 Bar Chart
4. 老師篩選:班級下拉、日期範圍選擇器

**完成標準**:
- [ ] API 在 30 學生 × 1000 Statement 規模下 < 2 秒回傳
- [ ] 三個數據:完成率 / 平均嘗試 / 失敗模式排行 正確計算
- [ ] UI 顯示在 1280x800 (老師筆電常見解析度)

**預期踩雷**:
- LRS 的 aggregation query API 各家不同:Yet Analytics 支援 xAPI 1.0.3 standard `/statements` endpoint,但不支援 Statement Aggregation API (ADL/IMS);需在 Node.js 後端自行做 aggregation(拉全部 Statement 再 group by)。30 學生 × 1000 Statement = 30K Statement,JSON parse 在 Node.js 應在 100ms 內

---

#### I3-T2 個別學生時間軸頁 (Student Detail)
**階段**: Phase 2
**工時**: 4 小時
**涉及技術**:
- `GET /xapi/statements?agent=<pseudonym>&ascending=true`
- 前端時間軸元件(D3.js 或 Chart.js)

**具體 task**:
1. API `GET /api/student/:hash/timeline`:拉該 pseudonym 所有 Statement,按時間排序
2. 前端時間軸:橫軸=時間、每個 Statement 一個事件點,hover 顯示詳情
3. D/C before/after 折線圖:找所有 `ran-simulation` Statement,連線 dc-ratio 值
4. 老師備註欄:純 local 存 `localStorage`(MVP),不存 LRS(避免個資混入)

**完成標準**:
- [ ] 時間軸正確顯示 8 種 verb 的事件點,顏色區分
- [ ] D/C 折線圖顯示求解歷史,可看出趨勢
- [ ] 老師備註欄可輸入並在下次開啟時仍存在(localStorage)

---

#### I3-T3 失敗模式分析頁 (Error Pattern Analysis)
**階段**: Phase 2
**工時**: 3 小時
**涉及技術**:
- SQL aggregation:`SELECT failure_mode, COUNT(*) FROM statements WHERE verb='observed-failure' GROUP BY failure_mode`
- 需從 xAPI Statement JSON 中提取 `result.extensions.failure-mode` 欄位

**具體 task**:
1. API `GET /api/class/:classId/errorPatterns`:統計失敗模式 × 關卡交叉表
2. 「頻繁被移除的構件」:從 removed-member Statement 中統計 Object ID 頻率
3. 前端表格:失敗模式行、關卡列、數字填格子(熱圖樣式,顏色深淺 = 次數)

**完成標準**:
- [ ] 失敗模式交叉表正確顯示
- [ ] 「卡關熱點構件」排行正確(和學生操作記錄一致)

---

#### I3-T4 CSV/PDF 匯出班級報告
**階段**: Phase 3
**工時**: 3 小時
**涉及技術**:
- `json2csv` npm 套件(CSV 匯出)
- `puppeteer` 或 `html-pdf` 套件(PDF 匯出,截圖方式)

**具體 task**:
1. 班級總覽頁「匯出」按鈕 → 呼叫 `GET /api/class/:classId/report.csv`
2. CSV 欄位:學生 pseudonym / 完成率 / 平均嘗試 / 主要失敗模式 / 反思完成率
3. PDF 版:用 Puppeteer 截圖 Dashboard 頁面,生成 PDF

**完成標準**:
- [ ] CSV 可用 Excel 直接開啟,中文欄位不亂碼(UTF-8 BOM)
- [ ] PDF 包含班級總覽三頁摘要

---

#### I3-T5 LRS 查詢效能優化 (index + 分頁)
**階段**: Phase 3
**工時**: 2 小時

**具體 task**:
1. 確認 Yet Analytics SQLite/PostgreSQL 的 statements 表有 `(agent, timestamp)` composite index
2. 所有 API 加分頁 `?page=1&limit=100`
3. 班級總覽 API 結果加 Cache-Control: max-age=60(1 分鐘快取)

**完成標準**:
- [ ] 30 學生 × 5000 Statement 規模下 API < 3 秒
- [ ] 分頁正確返回

---

### I3 小計

| Task | 階段 | 工時 |
|---|---|---|
| T1 班級總覽頁 | MVP | 4h |
| T2 個別學生時間軸 | Phase 2 | 4h |
| T3 失敗模式分析 | Phase 2 | 3h |
| T4 CSV/PDF 匯出 | Phase 3 | 3h |
| T5 效能優化 | Phase 3 | 2h |
| **I3 合計** | | **16h** |

---

## I4 學生反思工具

### 子節概念摘要

關卡結束後(成功或失敗)彈出「這次挑戰回顧」頁,顯示統計數據 + 引導問題,提交後以 xAPI `verb=reflected` 寫入 LRS。核心是 `UReflectionWidget`(UMG)+ `DA_ReflectionPrompts`(DataAsset)。本子節純粹是 UE UMG + log,完全不涉及 FrameCore。

### sub-task 清單

#### I4-T1 UReflectionWidget UMG 骨架 + 關卡結束觸發
**階段**: MVP(Phase 1)
**工時**: 3 小時
**涉及 UE5 class/API**:
- `UUserWidget` → `WBP_ReflectionWidget`
- SUQS `USuqsProgression::OnQuestCompleted` delegate(或 BP Event)
- `UGameplayStatics::OpenLevel()` 前顯示反思頁

**具體 task**:
1. `WBP_ReflectionWidget` Layout:三欄 — 左邊統計數據(嘗試次數 / 最大 D/C / 失敗模式)、中間引導問題(文字框或選項)、右側班級匿名對比
2. 綁定 SUQS `OnQuestCompleted` → `AArchSimHUD::ShowReflectionWidget(FArchSimSessionSummary)`
3. `FArchSimSessionSummary`:嘗試次數、最大 D/C(從 log 隊列取最近一次 ran-simulation)、失敗模式

**完成標準**:
- [ ] 關卡完成(SUQS quest complete)後 2 秒內 Reflection Widget 出現
- [ ] 統計數據:嘗試次數 / 最大 D/C / 主要失敗模式 正確顯示
- [ ] 「跳過」按鈕可關閉,不強制填寫(第一次強制,此限制在 T2 實作)

---

#### I4-T2 DA_ReflectionPrompts DataAsset 20 題 + 隨機抽題
**階段**: Phase 2
**工時**: 3 小時
**涉及 UE5 class/API**:
- 新增 `UArchSimReflectionPromptsAsset : public UDataAsset` 
- `UPROPERTY(EditAnywhere) TArray<FArchSimReflectionPrompt>` 
- `FArchSimReflectionPrompt` 含:問題文字、類型(開放式/選擇題)、對應的 Learning Objective 標籤

**具體 task**:
1. 定義 `FArchSimReflectionPrompt` USTRUCT:PromptText(FString)、PromptType(Enum:OpenEnded/MultipleChoice)、Tags(TArray<FName>)
2. 建立 `DA_ReflectionPrompts` DataAsset,填入 20+ 題中文引導問題(覆蓋 buckling / deflection / instability / material 四個主題)
3. 題目範例:
   - 「為什麼你選擇升級斷面而不是加斜撐?這兩種方法有什麼不同?」
   - 「如果把梁的支撐點從兩端改為一端,撓度會怎麼變?」
   - 「你的結構哪個部分最容易失敗?你是怎麼判斷的?」
4. 抽題邏輯:根據關卡的失敗模式 Tag 抽 1-2 題相關問題(若本次 buckling 失敗 → 優先抽 buckling Tag)
5. 首次遊戲:強制回答 1 題(不可跳過);之後可跳過

**完成標準**:
- [ ] DataAsset 在 Editor 中可填寫 20+ 題,熱重載
- [ ] 抽題邏輯依失敗模式 Tag 篩選,測試:buckling 失敗 → 50% 機率抽到 buckling 標記的題目
- [ ] 首次強制:遊戲第一次完成關卡時反思 Widget 無「跳過」按鈕

---

#### I4-T3 班級匿名對比 (aggregation query to LRS)
**階段**: Phase 2
**工時**: 3 小時
**涉及技術**:
- API `GET /api/class/:classId/aggregate?failureMode=buckling&lesson=L01`
- 回傳:該班級遇到相同失敗模式的比例 + 常用修正策略

**具體 task**:
1. Dashboard API 新增 `GET /api/class/:classId/peerSummary`:查 LRS 同班同學的 observed-failure + fixed-by-adding 統計
2. 回傳:`{ sameFailurePercent: 32, topFix: "add-brace", fixPercent: 51 }`
3. `WBP_ReflectionWidget` 右欄顯示:「班上 32% 同學也遇到 buckling,其中一半用加斜撐解決」(文字樣板)
4. **隱私保護**:API 只回傳聚合數據,不回傳任何個人 pseudonym

**完成標準**:
- [ ] 班級對比文字顯示正確(數字來自 LRS 查詢,不是假數據)
- [ ] API 不回傳任何可識別個人的資料
- [ ] 單人或少於 5 人班時顯示「班級人數不足,無法統計對比」(防止逆向識別)

**預期踩雷**:
- 「少於 5 人班不顯示聚合」這個隱私保護閾值是 K-anonymity 的基本原則,實作時需明確文檔記錄此設計決策
- API 需要班級 ID 參數,但 LRS 中的 Statement 是以 pseudonym 存,不含班級 ID 明文。**對策**:Statement 的 `context.groupId` 欄位存班級代碼(不含班級真名,只存 SHA-256 hash 的班級 ID)

---

#### I4-T4 反思與學習歷程檔案整合 (匯出 PDF)
**階段**: Phase 3
**工時**: 2 小時

**具體 task**:
1. 學生主選單「我的學習紀錄」頁 → 顯示自己的反思歷史(從 LRS 查 `verb=reflected` 的 Statement)
2. 「匯出學習歷程」按鈕 → 生成 PDF:關卡名稱 / 嘗試次數 / D/C 最佳值 / 反思文字
3. PDF 格式符合 108 課綱「學習歷程檔案」的自由度要求(不需要特殊格式,一般 A4)

**完成標準**:
- [ ] PDF 匯出包含至少 3 個關卡的反思內容
- [ ] PDF 格式整齊,可直接列印或上傳

---

### I4 小計

| Task | 階段 | 工時 |
|---|---|---|
| T1 Widget 骨架 + 觸發 | MVP | 3h |
| T2 DataAsset 20 題 | Phase 2 | 3h |
| T3 班級匿名對比 | Phase 2 | 3h |
| T4 學習歷程匯出 | Phase 3 | 2h |
| **I4 合計** | | **11h** |

---

## Part I 整體總工時估算

| 子節 | MVP (Phase 1) | Phase 2 | Phase 3 | 合計 |
|---|---|---|---|---|
| I1 教師進入學生世界 | 7h | 12h | 7h | 26h |
| I2 學習 Log Schema | 11h | 3h | 2h | 16h |
| I3 老師端 Dashboard | 4h | 7h | 5h | 16h |
| I4 學生反思工具 | 3h | 6h | 2h | 11h |
| **Part I 合計** | **25h** | **28h** | **16h** | **69h** |

> 注意:以上工時為「純開發+踩雷除錯」時間,不含 UE 冷編譯等待(第一次 UE 完整 Build 約 60-90 分鐘)。加入 UE Build 等待與整合測試後,實際日曆時間約 **128 小時**。

---

## 建議實作順序(考量依賴關係)

```
Sprint 1 (MVP, 25h):
  I2-T1 → I2-T2 → I2-T3   (先建 log 地基,後面所有功能都能驗證)
  I1-T1 → I1-T2             (Spectator 骨架 + 班級總覽,讓老師有基本視角)
  I3-T1                      (Dashboard 班級總覽頁,老師有地方看 log)
  I4-T1                      (反思 Widget 最簡版,關卡結束可顯示)

Sprint 2 (Phase 2, 28h):
  I1-T3 → I1-T4 → I1-T5    (相機 + 標記 + 私訊 + 觀察提示)
  I1-T6                      (bAlwaysRelevant 壓力測試 — 必須在 Phase 2 驗證,不能留 Phase 3)
  I2-T4                      (Pseudonymization + 個資刪除)
  I3-T2 → I3-T3             (Dashboard 後兩頁)
  I4-T2 → I4-T3             (反思題庫 + 班級對比)

Sprint 3 (Phase 3, 16h):
  I1-T7 → I1-T8             (假設測試 + 縮圖)
  I2-T5                      (Background Sync fallback)
  I3-T4 → I3-T5             (匯出 + 效能優化)
  I4-T4                      (學習歷程 PDF)
```

---

## 跨 Part 依賴說明

| 前置 Part | 依賴原因 | 若未完成的降級方案 |
|---|---|---|
| **Part G 多人協作** (Listen Server) | I1 Spectator 依賴多人 Session 存在;ServerViewNextPlayer() 只在有效 Listen Server 中運作 | 降級:單機模式下老師只能看本機狀態;不支援遠端觀察 |
| **Part A 引擎接合層** (UArchSimModelRegistry) | I2 的 ran-simulation / placed-member log 觸發點掛在 Registry 的 Solve 完成回調上 | 降級:I2-T2 用 Test-only BP stub 手動觸發,不接真實 Solve 流程 |
| **Part B ALS 角色系統** | I1 的 ATeacherPlayerController 加入 MC_TeacherFreecam MappingContext 要在 ALS Input 框架上 | 降級:不用 Enhanced Input,改 Classic Input 系統 |
| **Part D SUQS 任務系統** | I4-T1 反思 Widget 在 OnQuestCompleted 觸發;I2 的 asked-for-hint verb 在 OnHintRequested 觸發 | 降級:用計時器在關卡時間到 / 手動按「結束」時觸發 |

---

## 注意事項(誠實聲明)

1. **FrameCore engine FROZEN**:I1~I4 所有工作均在 UE consumer 層(`Plugins/FrameSolver/Source/FrameCoreUE/` 或 ArchSim 遊戲邏輯層),完全不觸碰 `Plugins/FrameSolver/Source/FrameCore/`
2. **failure-mode 分類是教育用簡化**:xAPI Statement 中的 `failure-mode` 欄位(buckling/yielding/instability)是 UE consumer 層的啟發式分類,非 FrameCore 原生提供的失敗模式枚舉。分類邏輯需在程式碼中清楚標注
3. **I3 Dashboard 是獨立 Web App**:工時估算包含 Node.js + Vue 3 前端開發,這是 UE 遊戲本體之外的獨立服務。部署時老師需要有 Node.js 環境或使用 Docker
4. **xAPI SDK 需自行實作**:UE 沒有現成 xAPI C++ SDK,HTTP POST 邏輯由 `UArchSimLogSubsystem` 自行實作;Statement 格式合規性需用外部 validator 驗證
5. **bAlwaysRelevant 風險是本 Part 最大技術賭注**:I1-T6 的壓力測試結果可能迫使整個 I1 降級(從「老師同時 Spectate 多學生」降為「老師一次只看一位學生,且只 relevant 該學生的 Actor」)


---


# Part J 實作擴充(312h)

**標題**:Part J — 教育設計總綱

**子節數**:6

### MVP 必須(Phase 1)
- J1-T01 閉環 HUD 進度指示器
- J1-T02 閉環狀態機 C++ Subsystem
- J1-T03 失敗觸發自動進入診斷模式 30 秒
- J2-T01 直覺優先 — 視覺回饋延遲門控器
- J2-T02 失敗語言包(排除 Game Over 文字)
- J3-T01 關卡 DataTable schema 設計
- J3-T02 MVP 三關卡 L-01/L-02/L-03 內容製作
- J4-T01 三星獎章系統 UArchSimAchievementSubsystem
- J5-T01 課綱對應 metadata 嵌入關卡 DataTable
- J6-T01 TeacherGuide.md 骨架文件 + 一頁速查卡

### Phase 2
- J1-T04 閉環分析報告(Solve 結果摘要 → PDF)
- J2-T03 鷹架漸退演算法(Hint 隨嘗試次數遞減)
- J2-T04 老師放大 — 班級失敗模式熱點排行
- J3-T03 關卡 L-04 ~ L-10 完整製作(7 關)
- J3-T04 測量支線 S-01 ~ S-05 銜接關卡清單生成器
- J4-T02 xAPI Statement 送出層(placed-member / ran-simulation 等 8 個 verb)
- J4-T03 作品集匯出 .archsim_bp + 截圖
- J5-T02 課綱覆蓋率 Dashboard 面板
- J6-T02 教師進入學生世界完整流程(Spectator + bAlwaysRelevant)
- J6-T03 課中流程計時器 + 老師提示推送

### Phase 3
- J1-T05 閉環跨關卡持久化(SPUD 整合)
- J2-T05 Flow 心流難度自動調整(動態調整補充提示)
- J3-T05 關卡 L-11 ~ L-15 完整製作(5 關) + S-06 ~ S-08 測量支線
- J4-T04 LRS 後端整合 + 老師端班級報告匯出
- J5-T03 統測題型對照表 + 練習題生成器
- J6-T04 TeacherGuide.md 完整版 + 培訓影片腳本

### 實作順序
- J1 (閉環狀態機先行,其他所有子節依賴它)
- J3 (關卡 schema 確定後才能做評量)
- J4 (評量機制依賴 J3 關卡結構)
- J2 (教育原則落地依賴 J4 評量基礎設施)
- J5 (課綱對應嵌入 J3 DataTable)
- J6 (教師備課依賴所有其他子節完整)

### 跨 Part 依賴
- Part A (A1 FrameCore 接合層) — J1 閉環狀態機需要 Solve 結果
- Part C (C2 D/C 熱圖、C3 崩塌) — J1 崩塌段 + J2 直覺優先視覺核心
- Part D (D1 應力掃描儀、D2 變形圖) — J1 診斷段 + J2 失覺優先教學工具
- Part F (F2 關卡系統 SUQS) — J3 關卡 DataTable 架在 SUQS 上
- Part I (I1 教師 Spectator、I3 dashboard) — J6 教師備課依賴 Part I 完整實作
- Part G (Part G 測量關卡) — J3 S-XX 測量支線關卡清單

### 風險清單
- J1-R1 閉環 HUD 若 Solve 非同步返回,進度指示器可能在解算中顯示舊狀態 — 需要 Solve 完成 delegate 嚴格觸發 HUD 更新
- J2-R1 直覺優先設計若關卡太難,學生直接放棄而非 Productive Failure — 需要設計至少 3 次免懲失敗緩衝
- J3-R1 DataTable schema 若與 SUQS row struct 不兼容,升 UE 版本時可能爆破 — 需版本鎖定並在升版時 reimport 測試
- J4-R1 xAPI Statement 中文字元 JSON 編碼問題 — 需 UE TJsonWriter 指定 UTF-8 不依賴預設
- J5-R1 108 課綱條目是教育部官方條文,直接引用若有版權疑慮 — 使用概念對應不完整引用原文
- J6-R1 TeacherGuide.md 若只有文字沒有實際操作示範,老師接受度低 — Phase 2 加入螢幕截圖引導 + 影片連結

## 詳細擴充內容


# Part J — 教育設計總綱：詳細實作計畫

> 本文件是 Part J 的可執行擴充計畫。每個子節包含具體 sub-task、工時估算、UE5 class/API 對應、完成標準、預期踩雷、以及 Phase 分級。
> 前提：FrameCore v4.0.0 engine source FROZEN，所有實作都在 FrameCoreUE consumer 側或新的 ArchSim game 模組中進行。

---

## J1 教學主軸閉環（再次定錨）

### 設計意圖

主軸閉環「設計 → 施工 → 加載 → 崩塌 → 診斷 → 改」不是一個概念，而是需要 C++ 狀態機、HUD、Solve 回呼、自動模式切換共同支撐的**技術實體**。每個 Part 都是閉環上的一段；J1 的任務是讓**閉環本身可觀測、可追蹤、可在整個遊戲週期中保持一致**。

### Sub-task 清單

#### J1-T01：閉環 HUD 進度指示器（Phase 1 — MVP 必須）

**工作內容**：新建 `WBP_LearningLoopHUD`（UMG Widget），頂部固定顯示六段進度條（設計/施工/加載/崩塌/診斷/改），每段有激活色（藍）和完成色（綠），目前所在段以 pulse 動畫標記。

**UE5 class/API**：`UUserWidget`、`UProgressBar`、`UImage`（SVG 六角符號）、`UTextBlock`（段落標題）、`FTimerHandle`（pulse 動畫）。

**完成標準**：
- [ ] 進度條在 PIE 中正確顯示六段
- [ ] 切換至「崩塌」段時，「施工」與「加載」段顯示為完成色
- [ ] 進度條本身不阻擋 3D 視角（固定在螢幕上方 40px 高帶，透明背景）

**工時估算**：3 小時

**預期踩雷**：UMG 在多人模式下 Widget 是 Local 的，不需 replicate；但若由 GameState replicated 資料驅動，需注意 Client 端收到資料的時機點可能落後一幀。

---

#### J1-T02：閉環狀態機 C++ Subsystem（Phase 1 — MVP 必須）

**工作內容**：新建 `UArchSimLoopSubsystem : UGameInstanceSubsystem`，持有 `ELearningLoopPhase` enum（Design / Construction / Loading / Collapse / Diagnosis / Revision），提供 `AdvancePhaseTo(Phase)` + `OnPhaseChanged` multicast delegate。

**UE5 class/API**：`UGameInstanceSubsystem`、`DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam`、`ELearningLoopPhase`（自訂 UENUM）。

**FrameCore 接合**：無需直接呼叫 FrameCore；由 `OnSolveComplete`（來自 A1 接合層）通知 Subsystem 推進至 Loading 段完成。

**完成標準**：
- [ ] Subsystem 可在 Blueprint 中呼叫 `AdvancePhaseTo`
- [ ] `OnPhaseChanged` delegate 正確廣播新狀態
- [ ] J1-T01 HUD 綁定此 delegate 並即時更新

**工時估算**：4 小時

**預期踩雷**：`UGameInstanceSubsystem` 在 PIE 結束時自動清理，但若未做好 delegate 解綁可能觸發懸空指標。需在 `Deinitialize()` 中 `OnPhaseChanged.Clear()`。

---

#### J1-T03：失敗觸發自動進入診斷模式 30 秒（Phase 1 — MVP 必須）

**工作內容**：在崩塌動畫播放結束後（`AFrameDynCollapseReplayActor::OnEventReached` delegate），自動呼叫 `AdvancePhaseTo(Diagnosis)` 並啟動 30 秒計時器。計時器顯示在 HUD 右下角。30 秒後顯示「返回設計」提示（不強制跳轉）。

**UE5 class/API**：`AFrameDynCollapseReplayActor`（已有 `OnEventReached`）、`FTimerManager`、`UArchSimLoopSubsystem`。

**完成標準**：
- [ ] 崩塌動畫結束後 <1 秒進入診斷模式
- [ ] HUD 倒計時 30 秒可見
- [ ] 30 秒後顯示非強制性「返回設計」浮動提示

**工時估算**：2 小時

**預期踩雷**：`AFrameDynCollapseReplayActor` 的 `OnEventReached` 是 multicast delegate，若關卡中有多個 Actor 都綁定此事件，需要確認只有「當前活躍結構」的崩塌才觸發診斷模式切換。

---

#### J1-T04：閉環分析報告（Phase 2）

**工作內容**：診斷模式結束時，生成一份輕量結構摘要 JSON（Max D/C、最大撓度、失敗構件 ID 清單），並在 `WBP_DiagnosisReport` Widget 中顯示摘要面板。同時送出 xAPI `observed-failure` Statement（與 J4-T02 共享基礎設施）。

**UE5 class/API**：`UArchSimReportGenerator`（需新增 helper class）、`FFrameSolveResult`（由 A1 接合層取得）、`TJsonWriter`。

**完成標準**：
- [ ] 診斷報告 Widget 在診斷模式中正確顯示 Max D/C 數值
- [ ] 報告 JSON 寫入 `FPaths::ProjectSavedDir()` 可讀取

**工時估算**：4 小時

**預期踩雷**：`FFrameSolveResult` 含有複雜 TArray，直接序列化可能觸發 SPUD 相容問題（決策 6 已指出）。只取 MemberUtilization + MaxDeflection 欄位即可。

---

#### J1-T05：閉環跨關卡持久化（Phase 3）

**工作內容**：整合 SPUD，在每次 `AdvancePhaseTo` 時紀錄時間戳與段落完成狀態，讓學生下次進入時可從斷點繼續，而非從頭開始。

**UE5 class/API**：`ISPUDStateful`、`SPUD_PROPERTY` macro、`USPUDSubsystem`。

**完成標準**：
- [ ] 關閉遊戲並重啟後，閉環進度正確恢復至上次位置
- [ ] SPUD round-trip 在 50 構件規模下正確（對應 L2 MVP 驗收條件 5）

**工時估算**：4 小時

**預期踩雷**：SPUD World Partition issue #117（見決策 6）。Phase 3 若升級 UE 版本需重新驗證 save/load 完整性。

---

**J1 小計工時**：17 小時

---

## J2 三大教育原則的學理基礎

### 設計意圖

三大原則（直覺優先、失敗即學習、老師放大）不能只停留在設計文件層，每一條都需要對應的技術實作讓它「真的發生」。J2 的目標是把這三條原則轉化為可測試、可驗收的程式行為。

### Sub-task 清單

#### J2-T01：直覺優先 — 視覺回饋延遲門控器（Phase 1 — MVP 必須）

**工作內容**：確保每次構件修改後 D/C 熱圖更新延遲 < 500ms（MVP 驗收條件 2）。建立 `UArchSimSolveThrottleSubsystem`，接收構件修改事件後以 200ms debounce 觸發 `ApplyPatchAndResolve`，避免連續快速修改造成 Server 過載。

**UE5 class/API**：`UGameInstanceSubsystem`（`UArchSimSolveThrottleSubsystem`）、`FTimerHandle`（debounce）、`UFrameInteractiveSubsystem::ApplyPatchAndResolve`（已有）。

**FrameCore 接合**：直接呼叫 `UFrameInteractiveSubsystem::ApplyPatchAndResolve`，不修改引擎。

**完成標準**：
- [ ] 連續快速拖拉 10 個構件時，Server 不累積超過 2 個 pending Solve 請求
- [ ] 從修改到熱圖更新的端到端延遲在 50 構件規模下 <500ms
- [ ] 測試工具：在 PIE 中 `stat unitgraph` 確認 GameThread 不卡頓

**工時估算**：3 小時

**預期踩雷**：`ApplyPatchAndResolve` 是 Woodbury 低秩更新，但大量連續 patch 累積後可能需要 `Rebaseline`（Tier-3 重分解）。需在 debounce 計數 ≥ 10 時自動呼叫 `Rebaseline`。

---

#### J2-T02：失敗語言包（排除 Game Over 文字）（Phase 1 — MVP 必須）

**工作內容**：建立 `UArchSimFailureMessageBank`（DataAsset），存放 20+ 條正向失敗訊息（中文），按失敗類型（超載 / 撓度 / 機構 / 崩塌）分類。顯示邏輯嵌入 `WBP_FailureOverlay`：崩塌後 0.5 秒顯示，字體大、強對比、2 秒後自動淡出。

**UE5 class/API**：`UDataAsset`、`UUserWidget`（`WBP_FailureOverlay`）、`UTextBlock`、`FText`（in-table localization 準備）。

**完成標準**：
- [ ] 崩塌後顯示「來看看哪裡出了問題」類型訊息，不出現「失敗」「Game Over」「錯誤」
- [ ] 至少 5 種不同失敗類型各有 4 條變化訊息（避免重複感）
- [ ] 字體符合 Part K 規格（Noto Sans CJK TC，最小 24pt）

**工時估算**：3 小時

**預期踩雷**：`FText` localization 需要 LocRes 文件才能在 shipping build 正常，開發階段用 `NSLOCTEXT` macro；正式版需設定 `.locres` 流程。

---

#### J2-T03：鷹架漸退演算法（Phase 2）

**工作內容**：在關卡中追蹤每個學生的失敗次數（`UArchSimProgressTracker`），第 1-2 次失敗顯示完整提示，第 3 次縮短提示，第 5 次後只顯示箭頭方向提示（極簡）。同時，若學生超過 3 次同類型失敗，觸發 `UArchSimWarningSubsystem` 通知老師 dashboard。

**UE5 class/API**：`UGameInstanceSubsystem`（`UArchSimProgressTracker`）、`UArchSimWarningSubsystem`（需新增）、xAPI `asked-for-hint` verb（與 J4-T02 連接）。

**完成標準**：
- [ ] 第 1/3/5 次失敗顯示不同層次提示，可在 QA checklist 中人工驗收
- [ ] `UArchSimProgressTracker` 有 `GetFailureCount(LevelId)` 可在 Blueprint 中呼叫

**工時估算**：4 小時

**預期踩雷**：鷹架漸退邏輯若放在 Client 端，多人模式下每個學生有獨立計數沒問題；但若老師需要在 dashboard 看到計數，需要 Server 端同步（與 Part I 整合）。

---

#### J2-T04：老師放大 — 班級失敗模式熱點排行（Phase 2）

**工作內容**：統計全班失敗最多的構件位置（grid 座標），在 `WBP_TeacherDashboard` 上顯示「熱點構件 Top 5」。這需要 Server 端收集每次崩塌的 `GoverningMemberIdx` 並彙整。

**UE5 class/API**：`AArchSimGameState`（Server 端彙整 TMap<int32, int32>）、`WBP_TeacherDashboard`（Part I 實作）、`UFrameSolveResult::DemandSummary.GoverningMemberIdx`（已有）。

**完成標準**：
- [ ] 老師端 dashboard 顯示當節課失敗最多的構件位置
- [ ] 彙整資料在 30 人規模下 <1 秒更新（單次 Solve 後計算）

**工時估算**：4 小時

**預期踩雷**：`GoverningMemberIdx` 是以 index 表示，需要對應到玩家可識別的構件名稱（如「第 3 根柱」）。需在 A1 接合層的 `UArchSimModelRegistry` 中維護 index → 顯示名稱的映射。

---

#### J2-T05：Flow 心流難度自動調整（Phase 3）

**工作內容**：追蹤學生完成關卡時間。若連續 3 關 <預設時間的 50%（過快），自動提升下一關的通關標準（如 D/C 目標從 <1.0 降至 <0.8）。若連續 3 關 >200%（過慢），降低難度並推送提示。此機制參考 Csíkszentmihályi Flow 理論。

**UE5 class/API**：`UArchSimDifficultyAdapter`（需新增）、`USuqsProgression`（SUQS）、關卡 DataTable 中的 `DifficultyMultiplier` 欄位。

**完成標準**：
- [ ] 難度調整在 DataTable 中有可設定範圍（不允許調整超過 ±30%）
- [ ] 難度調整後的通關條件在 HUD 中明確顯示

**工時估算**：6 小時

**預期踩雷**：難度調整若影響 xAPI 記錄的基準值，需要在 Statement 中附帶難度 multiplier 欄位，否則老師無法解讀記錄數據。

---

**J2 小計工時**：20 小時

---

## J3 完整關卡清單（主線 + 測量）

### 設計意圖

J3 的輸出是兩樣東西：一是**關卡 DataTable schema**（決定所有關卡的統一資料格式），二是**各關卡的實際內容製作**（L-00 至 L-15、S-01 至 S-08）。Schema 是架構決策，內容製作是重複性工作。

### Sub-task 清單

#### J3-T01：關卡 DataTable Schema 設計（Phase 1 — MVP 必須）

**工作內容**：設計 `FArchSimLevelRowData` USTRUCT（用於 UE DataTable），欄位包含：`LevelId`（FName）、`DisplayName`（FText）、`CorrespondingPart`（FString）、`LearningObjective`（FText）、`ExpectedFailureMode`（FText）、`CompletionConditions`（TArray<FArchSimCompletionCondition>）、`CurriculumUnit`（FString）、`EstimatedMinutes`（int32）、`DifficultyBase`（float）、`IsMainline`（bool）、`xAPIObjectId`（FString）。

`FArchSimCompletionCondition` 子結構：`ConditionType`（UENUM：MaxDC / MaxDeflection / SafetyFactor / ObserveOnly / PhaseComplete）、`ThresholdValue`（float）、`EvaluatedBySubsystem`（FString，指向評量 Subsystem class 名稱）。

**UE5 class/API**：`FTableRowBase`（繼承自此）、UE DataTable Editor。

**完成標準**：
- [ ] DataTable 可在 UE Editor 中匯入，所有欄位正確顯示
- [ ] `FArchSimCompletionCondition::ConditionType` 覆蓋 L-00 至 L-15 所有通關條件型態
- [ ] Schema 有 C++ 頭文件 `ArchSimLevelRow.h`，其他 Subsystem 可 `#include` 引用

**工時估算**：4 小時

**預期踩雷**：DataTable 的 `TArray<FArchSimCompletionCondition>` 欄位在舊版 UE Editor 中顯示為空（UE5.4 以前的 bug）。UE5.7 已修，但需驗證匯入後陣列元素是否正確序列化。

---

#### J3-T02：MVP 三關卡 L-01/L-02/L-03 內容製作（Phase 1 — MVP 必須）

**工作內容**：依 J3-T01 schema 填寫三個關卡的完整內容，並在 UE 中建立對應的關卡地圖（Level）：

- **L-01 懸臂梁初步**：預設一個固端支承 + 3m 跨距空間，學生放 1 根梁。通關條件：`MaxDeflection < 30mm`。預期失敗：梁太細導致撓度超標。
- **L-02 簡支梁與荷載**：預設兩個鉸支承相距 6m，學生放 1 根梁 + 一個均布載重。通關條件：`MaxDC < 1.0`。
- **L-03 簡單門型框架**：預設兩個固端節點，學生放 2 柱 + 1 梁形成門型。通關條件：`MaxDC < 0.85`。

每關需製作：DataTable 一列、Level 場景（地形 + 支承 Actor 預設位置）、SUQS Quest DataTable 一列、關卡說明 UMG Widget。

**UE5 class/API**：`UDataTable`（關卡資料）、`USuqsQuestDataTable`（SUQS）、`WBP_LevelBriefing`（UMG）、`AFrameUtilizationHeatmapActor`（已有，整合進關卡）。

**完成標準**：
- [ ] 三關卡可在 PIE 中完整通關（不中途崩潰）
- [ ] 通關條件由 `UArchSimLevelEvaluator` 自動判定（不需手動觸發）
- [ ] 每關卡有 30-60 秒可完成的操作量（符合 MVP 學生試玩需求）

**工時估算**：8 小時

**預期踩雷**：SUQS Quest 狀態在多人模式下需 replicate（決策 7），MVP 單機版先省略，Phase 2 補。

---

#### J3-T03：關卡 L-04 至 L-10 完整製作（Phase 2）

**工作內容**：依 J3-T01 schema 製作 7 個進階關卡，每關含場景設計、通關條件、DataTable 列：
- L-04 材料選擇關（需 `UFrameMaterialLibrary` 展示各材料 D/C 差異）
- L-05 載重路徑追蹤（需 `UFrameStructureGroupSubsystem` 顯示力流）
- L-06 載重組合（需 `LoadCombineEnvelope` 分析）
- L-07 撓度限制（L/250 規範，需 `FFrameNodalDisplacement` 提取最大位移）
- L-08 強柱弱梁（需 W-06 警告系統 trigger）
- L-09 模態振型（需 `AFrameModalShapeActor`）
- L-10 反應譜地震（需 `ResponseSpectrum` + `AFrameResponseSpectrumActor`）

**工時估算**：24 小時（每關約 3.5 小時）

**完成標準**：每關可在 PIE 中完整通關，且對應的 xAPI verb `completed` 有正確 Statement 送出。

**預期踩雷**：L-09 模態振型探索是「觀察性」目標（`ObserveOnly` 條件類型），需要確認 `UArchSimLevelEvaluator` 能處理「學生打開 ModalShapeActor 並停留 X 秒」這類行為觸發型通關條件。

---

#### J3-T04：測量支線 S-01 至 S-05 關卡清單生成器（Phase 2）

**工作內容**：測量支線使用不同的關卡 schema（`FArchSimSurveyLevelRowData`），欄位包含：儀器類型（水準儀 / 經緯儀）、地形模板 ID、數值範圍（自動生成題目用）、容許誤差。建立 `UArchSimSurveyLevelGenerator`，可根據難度參數動態生成 S-XX 題目（而非固定內容），每次開啟關卡數值不同（防止背答案）。

**UE5 class/API**：`FMath::RandRange`（UE 隨機）、`UArchSimSurveyLevelGenerator`（需新增）、`UDataTable`（`FArchSimSurveyLevelRowData`）。

**工時估算**：10 小時

**完成標準**：S-01 至 S-05 各有動態生成題目，同一關卡連續開啟 5 次數值各不同；容許誤差範圍符合台灣測量實習教科書標準（S-02 閉合差 ≤ ±12√n mm）。

**預期踩雷**：動態生成題目若涉及角度計算（S-05 水平角），需注意 UE 座標系 Y 軸與課堂習慣 North-East 的對應關係（UE X 軸朝前 ≠ 課堂北方）。需建立明確座標轉換 helper。

---

#### J3-T05：關卡 L-11 至 L-15 完整製作 + S-06 至 S-08（Phase 3）

**工作內容**：製作最後 5 個主線關卡（含 RC 工序、蜂窩偵測、倒塌預測、結構優化、期末綜合設計）和 3 個進階測量關卡（導線測量含矩陣平差、建物放樣、誤差診斷）。

**工時估算**：20 小時

**完成標準**：L-15 期末關卡可在 30 分鐘內完成，且可生成作品集截圖（J4-T03）。

---

**J3 小計工時**：66 小時

---

## J4 評量機制

### 設計意圖

評量不是「考試打分數」，而是「學習歷程證據」。技術上需要三層：(1) 即時成就系統（輕量）、(2) xAPI Statement 串流（深度）、(3) 作品集匯出（展示用）。三層彼此獨立，分 Phase 實作。

### Sub-task 清單

#### J4-T01：三星獎章系統 UArchSimAchievementSubsystem（Phase 1 — MVP 必須）

**工作內容**：新建 `UArchSimAchievementSubsystem : UGameInstanceSubsystem`，對每個完成的關卡評 1-3 顆星：
- 1 顆星：完成（通過 CompletionCondition）
- 2 顆星：完美（Max D/C < 0.7 且無規範警告）
- 3 顆星：速通（完成時間 < 70% EstimatedMinutes）

成就解鎖時顯示 `WBP_AchievementToast`（右上角彈出，2 秒動畫）。

**UE5 class/API**：`UGameInstanceSubsystem`、`UUserWidget`（`WBP_AchievementToast`）、`FArchSimLevelRowData::EstimatedMinutes`（J3-T01）、`FFrameSolveResult::MemberUtilization`。

**完成標準**：
- [ ] L-01、L-02、L-03 三關均可觸發三星評分
- [ ] Toast 動畫流暢，不阻擋主視角操作
- [ ] 星數存入 SPUD 持久化（Phase 1 暫存 `UGameInstance` 變數即可）

**工時估算**：4 小時

**預期踩雷**：「速通」判定需要精確的關卡開始時間戳。若學生中途退出再重進，需要從 SPUD 恢復或從 SUQS Quest 的 `GetQuestStartTime()` 取得（需確認 SUQS 是否提供此 API；若無，需在 Subsystem 自行記錄）。

---

#### J4-T02：xAPI Statement 送出層（Phase 2）

**工作內容**：建立 `UArchSimXAPISubsystem`，實作 8 個自訂 verb 的 Statement 組裝與 HTTP POST 送出。Statement 包含：actor（pseudonymized SHA256 hash）、verb（8 個自訂 + 繼承 SG Profile）、object（關卡或構件 URL）、result（score / duration / extensions）。離線時暫存到 `UGameInstance` 記憶體佇列，重連後批次送出。

**UE5 class/API**：`UArchSimXAPISubsystem`（需新增）、`FHttpModule::Get().CreateRequest()`（UE HTTP）、`TJsonWriter`（JSON 組裝）、`FPlatformMisc::GetUniqueDeviceId()`（學生識別符 seed）。

**完成標準**：
- [ ] `ran-simulation` Statement 在 Solve 完成後 <100ms 送出
- [ ] `placed-member` Statement 含 member type、position、material 三個 extension 欄位
- [ ] SHA256 pseudonymized ID 在同一機器同一學生 ID 下每次相同（deterministic）

**工時估算**：10 小時

**預期踩雷**：UE HTTP module 的 `FHttpRequest` 在 packaging 時需要在 `Build.cs` 中加入 `"HTTP"` 模組依賴，否則連結失敗。另：JSON 中文字元若用 `TCHAR_TO_UTF8` 轉換，需注意 Escape 處理；建議直接用 `TJsonWriter<TCHAR>` 並讓 HTTP Header 指定 `Content-Type: application/json; charset=utf-8`。

---

#### J4-T03：作品集匯出 .archsim_bp + 截圖（Phase 2）

**工作內容**：提供「儲存為藍圖」功能（複用 B4 Prefabricator 藍圖系統），以及一個「生成作品集截圖」功能（`UKismetRenderingLibrary::RenderSceneToTexture2D` + 存 PNG）。截圖自動附帶 Max D/C、材料清單、完成時間 metadata（嵌入 PNG EXIF 或附帶同名 JSON）。

**UE5 class/API**：`UKismetRenderingLibrary::RenderSceneToTexture2D`（需驗證 UE5.7 是否直接可用；若不可，改用 `FHighResScreenshotConfig`）、`FImageUtils::SaveImageAsJPEG`、`IFileManager`。

**完成標準**：
- [ ] 沙盒模式可一鍵匯出截圖 + JSON metadata
- [ ] 截圖分辨率 ≥ 1920×1080（適合學習歷程檔案展示）
- [ ] JSON metadata 包含 LevelId、MaxDC、CompletionTime、MemberCount

**工時估算**：6 小時

**預期踩雷**：`RenderSceneToTexture2D` 在 headless / dedicated server 上可能失敗（需渲染上下文）。MVP 只在 Client 端執行截圖，不在 Server 端。

---

#### J4-T04：LRS 後端整合 + 老師端班級報告匯出（Phase 3）

**工作內容**：整合 SQL LRS（Yet Analytics，Docker 部署），老師端可透過 REST API 查詢特定班級在某關卡的 Statement，匯出為 CSV/Excel。需要一個簡易的 Node.js 代理層轉換 LRS 回傳格式。

**工時估算**：16 小時

**完成標準**：老師可在 dashboard 中匯出班級報告 CSV，含每位學生的完成時間、失敗次數、最終 Max D/C。

**預期踩雷**：SQL LRS 的 xAPI Statement 查詢是 SQL WHERE 篩選，台灣教育環境可能不允許安裝 Docker；需提供 SQLite fallback（Yet Analytics 支援 SQLite）。

---

**J4 小計工時**：36 小時

---

## J5 對標 108 課綱（完整對應表）

### 設計意圖

J5 的技術實作目的是讓課綱對應不只是文件，而是**嵌入在關卡 DataTable 的 metadata**，讓老師可以在 dashboard 中看到「今天上的課對應了哪些課綱條目」，以及「哪些課綱條目還沒有對應的關卡」。

### Sub-task 清單

#### J5-T01：課綱對應 Metadata 嵌入關卡 DataTable（Phase 1 — MVP 必須）

**工作內容**：在 `FArchSimLevelRowData`（J3-T01）中加入 `CurriculumTags`（TArray<FString>），每個字串對應一個課綱條目代碼（例如 `"TW_TECH_HIGH_ARCHITECTURE_MECHANICS_1_SHEAR"`）。建立 `FArchSimCurriculumTagRegistry` DataAsset，存放所有有效 tag 及其中文完整名稱與年級對應。

**UE5 class/API**：`UDataAsset`（`FArchSimCurriculumTagRegistry`）、`UDataTable`（擴充 `FArchSimLevelRowData`）。

**完成標準**：
- [ ] L-01 至 L-03 三個關卡各有 1-3 個課綱 tag
- [ ] `FArchSimCurriculumTagRegistry` 涵蓋主計畫書 J5 節所列的全部課綱科目

**工時估算**：3 小時

**預期踩雷**：課綱 tag 字串若在不同版本課綱之間有更名，DataTable 中的舊 tag 不會自動更新。需設計 migration helper 或在 tag Registry 中支援 alias 機制。

---

#### J5-T02：課綱覆蓋率 Dashboard 面板（Phase 2）

**工作內容**：在老師端 dashboard（`WBP_TeacherDashboard`，Part I 實作）中加入一個「課綱覆蓋率」分頁，顯示：哪些課綱條目已有對應關卡（已覆蓋）、哪些尚未（缺口）、學生在每個條目下的平均通關率。

**UE5 class/API**：`WBP_CurriculumCoveragePanel`（需新增 UMG Widget）、`FArchSimCurriculumTagRegistry`、`UArchSimAchievementSubsystem`（取星數 / 通關率）。

**完成標準**：
- [ ] Dashboard 顯示至少 10 個課綱條目的覆蓋狀況
- [ ] 「未覆蓋」條目以紅色高亮，讓老師知道需要補充傳統教案

**工時估算**：6 小時

**預期踩雷**：「學生通關率」需要 Server 端彙整所有 Client 的成就資料。Phase 2 可先用 xAPI Statement 離線分析；即時版需在 `AArchSimGameState` 中維護 per-tag 統計 Map。

---

#### J5-T03：統測題型對照表 + 練習題生成器（Phase 3）

**工作內容**：針對統測必考的「基礎工程力學」與「測量實習」，建立一個「統測練習模式」，根據公布的統測題型範圍，從現有關卡庫中動態抽取對應關卡，組成一組 10 題的練習序列，並在完成後顯示得分（按統測評分邏輯換算）。

**UE5 class/API**：`UArchSimExamModeSubsystem`（需新增）、`FArchSimCurriculumTagRegistry`（篩選統測 tag）、`FMath::RandRange`（隨機抽題）。

**工時估算**：8 小時

**完成標準**：練習模式可生成 10 題序列，涵蓋力學與測量兩大主題；得分計算邏輯有文件說明（不宣稱等同真實統測）。

**預期踩雷**：統測題型每年可能調整，需將對照邏輯設計為可更新的 DataAsset，不硬碼進 C++。

---

**J5 小計工時**：17 小時

---

## J6 教師備課指南（精簡版）

### 設計意圖

J6 的產出不是純文件，而是**遊戲內嵌入的教師支援系統**（備課模式、課中計時器、提示推送）加上**外部文件**（`TeacherGuide.md`、一頁速查卡）。兩者必須保持同步。

### Sub-task 清單

#### J6-T01：TeacherGuide.md 骨架文件 + 一頁速查卡（Phase 1 — MVP 必須）

**工作內容**：撰寫 `docs/TeacherGuide.md`，包含：
1. 系統需求（LAN、電腦規格、UE 安裝步驟）
2. 課前準備 step-by-step（老師機 Host 啟動 → 學生機加入）
3. 45 分鐘課中流程腳本（10+20+10+5 分鐘分配）
4. 關卡選擇建議表（哪節課用哪個關卡）
5. 常見問題 FAQ（前 10 個最可能遇到的技術問題）

同時製作 `docs/TeacherQuickCard.pdf`（一頁，可列印），含 QR code 連結到完整 Guide。

**工時估算**：6 小時

**完成標準**：
- [ ] 文件由至少一位非開發者（例如測試老師）閱讀後，可獨立完成第一次課程設置
- [ ] FAQ 涵蓋「學生無法連線」「Solve 很慢」「崩塌動畫沒出現」三個最常見問題

**預期踩雷**：TeacherGuide.md 若缺少對應遊戲版本號，未來更新後指南與實際操作不符。需在文件頂部明確標注對應的遊戲版本與 UE 版本。

---

#### J6-T02：教師進入學生世界完整流程（Phase 2）

**工作內容**：確認 Part I（I1 教師 Spectator）的實作覆蓋以下教學場景：老師可在不中斷學生操作的情況下進入其視角、標記一個構件（`UArchSimTeacherMarkerSubsystem`）讓全班看到老師的紅色標記箭頭、離開後標記自動消失。

**UE5 class/API**：`ASpectatorPawn`、`APlayerController::SetViewTarget`、`UArchSimTeacherMarkerSubsystem`（需新增，或整合進 Part I）、`UDecalComponent`（紅色圓形地板標記）。

**完成標準**：
- [ ] 老師進入學生視角後，學生的操作不受影響
- [ ] 老師紅色標記箭頭在全班所有 Client 端可見（bAlwaysRelevant = true）
- [ ] 老師離開後標記在 3 秒內消失

**工時估算**：8 小時

**預期踩雷**：決策 4 已指出 `bAlwaysRelevant` 對 100+ 構件有複製負擔。教師標記 Actor 本身 mesh 要極簡（僅一個 `UStaticMeshComponent` 箭頭），且數量限制 ≤ 5 個同時存在。

---

#### J6-T03：課中流程計時器 + 老師提示推送（Phase 2）

**工作內容**：老師端（教師 HUD）有一個 45 分鐘課堂計時器，依照 10+20+10+5 分鐘分割顯示，並在每個時段結束前 2 分鐘顯示提示。老師可主動向全班發送文字訊息（`UArchSimTeacherBroadcastSubsystem`），訊息以「老師說：」前綴顯示在學生 HUD 頂部，5 秒後消失。

**UE5 class/API**：`UArchSimTeacherBroadcastSubsystem`（需新增）、`APlayerController::ClientReceiveLocalizedMessage`（或自訂 RPC）、`FTimerHandle`（課程計時器）、`WBP_TeacherBroadcastOverlay`（學生端 UMG）。

**完成標準**：
- [ ] 老師發送訊息後 <500ms 在所有學生端顯示
- [ ] 訊息最長 100 字元（防止老師 HUD 佔版面過大）
- [ ] 課程計時器在老師切換 Spectator 模式時不停止

**工時估算**：6 小時

**預期踩雷**：Server 端向所有 Client 廣播文字訊息若用 `GameState::Multicast` RPC，需確認 RPC 的可靠性（`Reliable` vs `Unreliable`）。教育場景中訊息遺失不可接受，需用 `Reliable`，但 Reliable RPC 在 30 人同時廣播時有 HOL blocking 風險；對策：老師廣播走 `NetMulticast(Reliable)` + 學生端有視覺 ack（訊息顯示時短暫閃爍確認）。

---

#### J6-T04：TeacherGuide.md 完整版 + 培訓影片腳本（Phase 3）

**工作內容**：擴充 J6-T01 的骨架至完整版，加入：
- 關卡設計指南（老師如何創建自訂關卡，若 Phase 3 開放此功能）
- 個資法合規流程（家長同意書模板 + LRS 刪除申請流程）
- 班級報告解讀指南（如何閱讀 xAPI 報告，哪些 Statement 代表什麼學習行為）
- 培訓影片腳本（10 分鐘快速上手 + 30 分鐘進階教學）

**工時估算**：12 小時

**完成標準**：
- [ ] 個資法合規流程由法律顧問（或教育部 PDPA 指引）審閱
- [ ] 培訓影片腳本可交由非開發者錄製

**預期踩雷**：台灣個資法在學校環境的適用細節與教育部《學生個人資料保護實施要點》有差異；TeacherGuide 必須參照最新版（2025 年修訂），不能只引用通則。

---

**J6 小計工時**：32 小時（含跨 Part I 的整合確認）

---

## Part J 整體總結

### 工時彙整

| 子節 | Phase 1 MVP | Phase 2 | Phase 3 | 合計 |
|------|------------|---------|---------|------|
| J1 閉環 | 9h | 4h | 4h | 17h |
| J2 教育原則 | 6h | 8h | 6h | 20h |
| J3 關卡清單 | 12h | 34h | 20h | 66h |
| J4 評量機制 | 4h | 16h | 16h | 36h |
| J5 課綱對標 | 3h | 6h | 8h | 17h |
| J6 教師指南 | 6h | 14h | 12h | 32h |
| **合計** | **40h** | **82h** | **66h** | **188h** |

> 注意：此工時為純 Part J 相關實作估算。Part J 的多個子節高度依賴 Part I（教師工具）、Part F（關卡系統 SUQS）、Part A（引擎接合），這些依賴 Part 的工時計算在各自 Part 的計畫書中，不重複計入。若 1-2 人團隊同時負責 Part J + Part I + Part F，實際周期會因並行開發效率而拉長。

### 建議實作順序

1. J3-T01（關卡 schema）— 架構決策，最早鎖定
2. J1-T01 + J1-T02（閉環 HUD + Subsystem）— 其他所有教學機制的骨架
3. J4-T01（三星獎章）— 簡單且 MVP 必須，讓學生有即時回饋
4. J3-T02（三個 MVP 關卡）— 填充內容
5. J2-T01 + J2-T02（視覺回饋門控 + 失敗語言包）— 確認「直覺優先」和「失敗即學習」技術上可行
6. J5-T01（課綱 metadata）— 搭順風車嵌入 J3 DataTable
7. J6-T01（TeacherGuide 骨架）— 文件工作，可並行
8. Phase 2 以後按工時排列

### 跨 Part 依賴（必須先完成才能開始 J 子節）

| J 子節 | 必須先完成的 Part | 說明 |
|--------|-----------------|------|
| J1-T02 閉環 Subsystem | A1 FrameCore 接合 | `OnSolveComplete` 事件源頭 |
| J1-T03 失敗觸發診斷 | C3 崩塌動畫 | `AFrameDynCollapseReplayActor::OnEventReached` |
| J2-T01 視覺回饋門控 | A1 FrameCore 接合 | `UFrameInteractiveSubsystem::ApplyPatchAndResolve` |
| J3-T02 關卡製作 | F2 關卡系統 SUQS | SUQS DataTable 結構依賴 |
| J4-T02 xAPI | I2 學習 Log（Part I）| LRS 連線設定依賴 |
| J6-T02 教師進入 | I1 教師 Spectator（Part I）| Spectator 模式實作依賴 |

### 最關鍵風險（不可忽視）

1. **J3-T01 DataTable schema 需在 Sprint 1 第一週鎖定**：後續所有關卡製作、評量機制、課綱對應都依賴此 schema。schema 更改後需要遷移已有 DataTable，代價高昂。
2. **J4-T02 xAPI 個資保護必須在第一個 Statement 送出前到位**：Pseudonymization 邏輯若事後補，有暴露真實學生 ID 的隱患（違反台灣個資法）。
3. **J6-T02 bAlwaysRelevant 在 30 人規模的壓力測試**：教師標記 Actor 若設錯 relevancy，在大班情境下造成複製風暴，影響授課體驗。必須在 Phase 2 完成後、試教前做 30 人壓測。


---


# Part K 實作擴充(112h)

**標題**:Part K:風格與美術

**子節數**:4

### MVP 必須(Phase 1)
- K1-T1: PostProcessVolume 全域後處理材質設定(基準線)
- K1-T2: 構件材質三件套(混凝土/鋼材/木材)UMaterialInstanceDynamic
- K1-T3: 照明系統 DirectionalLight + SkyAtmosphere 鎖定
- K2-T1: 實作 UArchSimColorMap BlueprintFunctionLibrary(D/C 綠黃橘紅 Okabe-Ito)
- K2-T2: 接 AFrameUtilizationHeatmapActor 色彩管道(SaturationDC + 色表切換)
- K2-T3: D/C 警告 HUD 色彩凡例 Widget(WBP_ColorLegend)
- K3-T1: 核心操作音效(構件放置/移除/Solve完成/D/C警告 4 個 SFX)
- K4-T1: 字體資源匯入(Noto Sans CJK TC + JetBrains Mono)
- K4-T2: UI Style Guide DataAsset(UArchSimUIStyle)定義字級/間距/基礎色票
- K4-T3: 核心術語表 DataTable(DT_TermDictionary)中英 90 詞

### Phase 2
- K1-T4: 構件材質 Unlit 模式切換(熱圖覆蓋時自動關閉 Lit)
- K1-T5: 環境風格包(室外極簡 + 室內灰白場景美術)
- K2-T4: Cividis 色表實作(位移 / von Mises)(CLUT Texture 1D + sampler)
- K2-T5: 連續發散色表(模態振型 / 軸力正負值)(藍→白→橘)
- K2-T6: 色盲輔助模式設定頁(Settings → 輔助功能 → 視覺輔助)
- K3-T2: 倒塌音效群組(低頻轟鳴 + Chaos 落地聲 + 環境音暫停)
- K3-T3: 環境音 Ambient 系統(室外風聲/鳥鳴、室內白噪音,MetaSound Graph)
- K3-T4: 通關/失敗音效 + 教師訊息鐘聲 + Tutorial 提示音
- K4-T4: HUD 響應式縮放系統(UUserWidget::GetViewportScale 自動根據解析度縮放)
- K4-T5: 術語 Tooltip 系統(hovering over 術語 → 彈出中英解釋)

### Phase 3
- K1-T6: Nanite/Lumen 輕量化設定(距離 LOD + 低 Lumen quality tier)
- K1-T7: 品質選項選單(效能/平衡/品質 三擋,覆蓋 PostProcess + Shadow 設定)
- K2-T7: 動態色表混合動畫(從灰階 → Cividis 漸入,Solve 完成時播放)
- K3-T5: 自適應音效混音(MetaSound Quartz 同步倒塌事件時間軸與音效時間軸)
- K3-T6: 音效音量 Settings 分軌(主音量/SFX/音樂/環境音 四軌 + 記憶 SaveGame)
- K4-T6: 多語在地化框架(FText + LocText 字串表, 預留英文 locale 切換)
- K4-T7: 術語 Flashcard 迷你遊戲(教師可從設定頁開啟,強化術語記憶)

### 實作順序
- K4-T1 (字體匯入)
- K4-T2 (UIStyle DataAsset)
- K2-T1 (ColorMap Library)
- K1-T1 (PostProcess 基準線)
- K1-T2 (構件材質)
- K1-T3 (照明鎖定)
- K2-T2 (Heatmap 色彩管道)
- K2-T3 (HUD 色彩凡例)
- K3-T1 (核心音效)
- K4-T3 (術語表)

### 跨 Part 依賴
- Part A (A1 FrameCore 接合層):K2-T2 依賴 UArchSimModelRegistry 的 DistributeSolveResult 才能取到 D/C 值驅動色彩
- Part B (B1 構件放置):K1-T2 構件材質必須在 B1 Prefab Spawn 流程建立前定稿,否則 Ghost 預覽材質無法對齊
- Part C (C2 D/C 熱圖):K2-T2/T4/T5 色彩管道是 AFrameUtilizationHeatmapActor 的直接依存;C2 必須先有工作的熱圖 Actor 才能接色彩
- Part F (F2 Tutorial 關卡):K4-T3 術語表需要在第一個 Tutorial 對話出現前就匯入完畢
- Part H (多人協作):K3 音效系統在多人環境需確認 Sound Attenuation 只在本機播放(非 Replicated 聲音),與 H 的網路架構協調

### 風險清單
- [HIGH] UMaterialInstanceDynamic 在 Unlit 模式的熱圖 Actor 上設定 VectorParameterValue 須確認 Render Thread 安全;熱圖 BuildHeatmap() 是 Game Thread 方法,必須在同幀或下幀設定 MID,不可跨幀 dangling pointer
- [HIGH] Noto Sans CJK TC 字型資源體積巨大(繁體子集約 3-5MB TTF);UE5 Font Asset 若不做子集化會大幅增加 pak 包體積,需在 Phase 3 做 Font Atlas 子集化
- [MEDIUM] MetaSound 在 UE5.7 的 API 尚在演進中(Quartz 時鐘 API 在 5.6 → 5.7 有 minor breaking);K3-T5 的 Quartz 同步功能需在 5.7 驗證,建議 MVP 先用傳統 USoundCue 避免風險
- [MEDIUM] AFrameUtilizationHeatmapActor.SaturationDC 是 float UPROPERTY,目前色彩計算在 C++ BuildHeatmap() 內部 hardcode Lerp;K2-T2 若要換色表必須在 FrameCoreUE consumer 層新增 helper(不能改 engine source,FROZEN 鐵則 #1)
- [MEDIUM] 色盲輔助模式需要在 Runtime 動態替換整套色表;若以 CLUT 1D Texture 實作,需確認 UTexture 的 UpdateTextureRegions 在 packaged build 的行為與 PIE 一致
- [LOW] JetBrains Mono 等寬字型在 UMG TextBlock 的 Monospace 渲染品質依賴 UE5 的 freetype Hinting 設定;在中文混排場景字型 fallback chain 需驗證不出現方框替代字

## 詳細擴充內容

# Part K:風格與美術 — 詳細實作計畫

> 版本:v1.0 | 日期:2026-06-25 | 對應主計畫書:第十三章 Part K
> 前提:FrameCore v4.0.0 engine source 永久 FROZEN;所有實作均在 FrameCoreUE consumer 層或 ArchSim 遊戲層完成。

---

## 概述

Part K 負責建立整個建築師模擬器的**視覺語言、色彩語意系統、音效框架、字體與術語規範**。它不是「裝飾」層,而是**學習支架的視覺編碼**:學生透過顏色立即理解 D/C 比的安全等級,透過音效獲得操作事件回饋,透過一致的字體與術語建立跨科目的概念橋樑。

Part K 的所有工作都發生在三個層次:

1. **UE5 Asset 層**:材質 Material、材質實例 MID、字型 Font Asset、音效資產 SoundWave/SoundCue/MetaSound
2. **UE5 C++ helper 層**:新增 `UArchSimColorMap`、`UArchSimUIStyle`、`UArchSimSoundBank` 等純 consumer-side helper class
3. **UMG Widget 層**:色彩凡例 HUD Widget、術語 Tooltip Widget、Settings 音量分軌 Widget

**FrameCore 無任何修改**:所有色彩計算在 `UArchSimColorMap` 或材質中完成,`AFrameUtilizationHeatmapActor` 的 `SaturationDC` 屬性作為色彩飽和點輸入。

---

## K1 視覺風格(寫實 vs 卡通 vs 工程圖)

### 設計原則回顧

主計畫書確立的風格定位:「簡潔工程感,類 Karamba3D / Poly Bridge」。構件幾何清晰,材質簡化不寫實;照明固定方向不動態;後處理輕度。`PostProcessVolume` 上的熱圖材質走 **Unlit**,避免動態光照污染資料讀取。

### 詳細 Sub-Task 清單

#### K1-T1:PostProcessVolume 全域後處理材質設定(基準線)
- **描述**:建立 `PP_ArchSimBase` Post Process Material(Tone Mapper 後,Blendable Weight=1.0),套用輕度 Vignette(Intensity 0.3)、輕度 Chromatic Aberration(Intensity 0.05)關閉、Bloom(Intensity 0.2)、Lens Flare 關閉。所有參數以 `UPostProcessComponent` UPROPERTY 暴露,讓 Phase 3 品質選單可覆蓋。
- **涉及 UE5 API**:`UPostProcessComponent`、`FPostProcessSettings`、`FWeightedBlendable`
- **工時**:2h
- **完成標準**:PIE 開啟後 PostProcess 設定生效;截圖驗收:構件輪廓清晰,無過曝/過暗;`FPostProcessSettings::bOverride_VignetteIntensity = true`
- **預期踩雷**:UE5.7 的 `FPostProcessSettings` 部分欄位在 Blueprint 不可設定,需 C++ `GetWorld()->GetFirstPlayerController()->PlayerCameraManager` 取 CameraManager 再設定;建議改由 Level 內放一個 `APostProcessVolume` 設為 Infinite Extent 的方式,避開 C++ API。
- **依賴**:無(首個基礎設定)
- **Phase**:MVP

#### K1-T2:構件材質三件套(混凝土/鋼材/木材)UMaterialInstanceDynamic
- **描述**:建立三個 Master Material `M_Member_Concrete`、`M_Member_Steel`、`M_Member_Wood`,各含兩個 Parameter:`BaseColor`(FLinearColor)、`OpacityMask`(float, 0=不透明/1=Ghost)。從 Master Material 建立 MID,並封裝為 `UArchSimMaterialManager` BlueprintFunctionLibrary,提供 `GetConcreteMID() / GetSteelMID() / GetWoodMID()` 靜態方法。Ghost 預覽用 `SetScalarParameterValue("OpacityMask", 0.5f)` + `SetVectorParameterValue("TintColor", GreenColor/RedColor)` 覆蓋色調。
- **涉及 UE5 API**:`UMaterialInstanceDynamic::Create()`、`SetScalarParameterValue()`、`SetVectorParameterValue()`、`UStaticMeshComponent::SetMaterial()`
- **工時**:3h
- **完成標準**:三種材質在 PIE 中可見;Ghost 預覽呼叫 `SetOpacity(0.5)` 後透明度正確;D/C 熱圖覆蓋時不受這三個材質影響(Heatmap Actor 走獨立 PMC mesh,不是修改原 Actor 材質)
- **預期踩雷**:Master Material 若設為 Translucent blend mode,`bCastShadows` 必須關閉否則影子消失;Ghost 材質的 Masked 模式在 Nanite mesh 上不支援 − 若 Phase 3 啟用 Nanite,需切換為 Dithered LOD Transition 方案
- **依賴**:K1-T1 PostProcess 基準線
- **Phase**:MVP

#### K1-T3:照明系統 DirectionalLight + SkyAtmosphere 鎖定
- **描述**:場景照明以一盞 `ADirectionalLight`(Lux=100000, Rotation Pitch=-45, Yaw=60 模擬下午 2pm 西南方向)+ `ASkyAtmosphere` + `ASkyLight`(Real Time Capture=false,固定 Cubemap)組成。**照明角度鎖定不跟隨時間軸**,避免影子方向變化干擾視線。所有結構 Actor 的 `StaticMesh.CastShadow = true`,熱圖 PMC mesh 的 `CastShadow = false`(避免資料渲染被影子遮蔽)。
- **涉及 UE5 API**:`ADirectionalLight`、`ASkyAtmosphere`、`ASkyLight`、`UProceduralMeshComponent::SetCastShadow(false)`
- **工時**:2h
- **完成標準**:不同時段(早/午/晚)下照明角度不變;熱圖 PMC mesh 無影子;Ambient Occlusion 輕度可見
- **預期踩雷**:UE5 的 SkyAtmosphere 在 World Partition 場景需設定 `bSupportStationarySkylight=false` 否則 lightmap 編譯耗時暴增
- **依賴**:K1-T1
- **Phase**:MVP

#### K1-T4:構件材質 Unlit 模式切換(熱圖覆蓋時)
- **描述**:當 D/C 熱圖 Actor 啟用時,背後的原始構件 Actor 材質應切換至 Unlit(或降低透明度至 0.1)避免視覺衝突。建立 `UArchSimHeatmapController` ActorComponent,持有 `bHeatmapActive` 旗標,當旗標設為 true 時對所有已登錄構件呼叫 `SetMaterial(0, M_Member_Unlit_Instance)`;旗標設為 false 時還原原始材質。
- **涉及 UE5 API**:`UMaterialInterface`、`UStaticMeshComponent::SetMaterial()`、`TWeakObjectPtr<UMaterialInterface>` 原始材質快取
- **工時**:3h
- **完成標準**:按 Tab 切換熱圖模式時,構件從 Lit → Unlit → Lit 無 artifact;不影響 FrameCore 計算(純 UE 視覺層)
- **預期踩雷**:材質切換如果在 Tick 內每幀呼叫會觸發 RHI 大量 SetShader 調用導致卡頓;必須只在旗標變更時呼叫一次,加 `bDirty` guard
- **依賴**:K1-T2、K2-T2
- **Phase**:Phase 2

#### K1-T5:環境風格包(室外極簡 + 室內灰白場景美術)
- **描述**:建立兩套「情境包」。室外:草地 StaticMesh(Landscape 降級為 StaticMesh Plane 簡化版)+ 遠山剪影 Billboard Mesh + 天空盒。室內:灰白牆面材質 `M_Wall_Grey`、木地板 `M_Floor_Wood`、頂棚 `M_Ceiling_White`。每套包含 1 個 `LevelStreamingVolume` chunk,讓關卡按需載入。
- **涉及 UE5 API**:`ULandscapeComponent`(或降級為 `UStaticMesh`)、`ALandscapeStreamingProxy`、`UMaterialInterface`
- **工時**:6h
- **完成標準**:室外/室內各有對應的 Level chunk;草地/地板材質 tiling 無縫;PIE 無明顯 LOD pop-in
- **預期踩雷**:從 Landscape 降為 StaticMesh 可能丟失地形高度精度,影響 A1 的「地基節點 Z 座標跟隨地形」邏輯;若降級需在 A3 接合點加驗證
- **依賴**:K1-T1、K1-T3
- **Phase**:Phase 2

#### K1-T6:Nanite/Lumen 輕量化設定
- **描述**:在 `DefaultEngine.ini` 設定 Lumen quality tier 為 Low;構件 Procedural Mesh(`UProceduralMeshComponent`)不支援 Nanite,維持傳統管線。僅對環境 StaticMesh(草地、牆面)啟用 Nanite。提供三擋 `UArchSimQualitySettings`:Performance / Balanced / Quality,各別覆蓋 `PostProcessQualityLevel`、`ShadowQuality`、`GlobalIllumination`。
- **涉及 UE5 API**:`Scalability::SetQualityLevels()`、`GEngine->Exec(GetWorld(), TEXT("r.Lumen.Reflections.Allow 0"))` 等 console var 覆蓋
- **工時**:3h
- **完成標準**:三擋設定在 PIE 可切換;Performance 擋在 Intel UHD 730 (校園常見廉價機) 30fps 以上
- **預期踩雷**:`UProceduralMeshComponent` 走傳統 rendering path,與 Nanite mesh 混用時需確認 RHI batch 不衝突
- **依賴**:K1-T1、K1-T3、K1-T5
- **Phase**:Phase 3

#### K1-T7:品質選項選單
- **描述**:在 `WBP_SettingsPanel` 加「畫面品質」頁籤,提供 ComboBox 選擇 Performance / Balanced / Quality,選擇後呼叫 `UArchSimQualitySettings::ApplyPreset()`。存入 `UArchSimSaveGame` 的 `QualityLevel` 欄位(int32)。
- **涉及 UE5 API**:`UComboBoxString`、`UGameUserSettings`、`ApplySettings(false)` 不重啟
- **工時**:2h
- **完成標準**:設定頁選擇 → 即時生效 → 重開遊戲後設定保留
- **依賴**:K1-T6、K4-T2(UIStyle)
- **Phase**:Phase 3

**K1 子節小計工時**:2+3+2+3+6+3+2 = **21h**

---

## K2 色彩語意(完整對照表)

### 設計原則回顧

主計畫書決策 8 確立:D/C 用 Okabe-Ito 色盲友善的交通號誌語意(綠→黃→橘→紅),位移/von Mises 用 Cividis,模態/軸力用藍→白→橘發散色表。**永遠不用 Jet 彩虹色表**。FrameCore 的 `AFrameUtilizationHeatmapActor` 現有 `SaturationDC` 控制飽和點,但色表硬碼在 C++ 端的線性 Lerp,需在 consumer 層包一層可換色表的 helper。

### 詳細 Sub-Task 清單

#### K2-T1:實作 UArchSimColorMap BlueprintFunctionLibrary
- **描述**:新建 `UArchSimColorMap : UBlueprintFunctionLibrary`,在 `FrameCoreUE` consumer 層(ArchSim 遊戲模組,不動 plugin source)實作:
  - `static FLinearColor GetDCColor(float DC, float SaturationDC=1.0f)`:輸入 D/C 值,返回 Okabe-Ito 四色漸層(#009E73→#F0E442→#E69F00→#D55E00)
  - `static FLinearColor GetCividisColor(float T)`:輸入 [0,1] 歸一化值,返回 Cividis CLUT 取樣結果(預先儲存 256 entry FLinearColor 陣列作為 CLUT)
  - `static FLinearColor GetDivergingColor(float SignedT)`:輸入 [-1,1],返回藍→白→橘發散值
  - 所有方法標 `BlueprintPure`、`Category="ArchSim|Color"`
- **涉及 UE5 API**:`UBlueprintFunctionLibrary`、`FLinearColor`、`FMath::Lerp()`
- **工時**:3h
- **完成標準**:Blueprint 可呼叫;用 Python sympy 驗算 `GetDCColor(0.0)` → `#009E73`、`GetDCColor(1.0)` → `#D55E00`、`GetCividisColor(0.5)` → 接近 Cividis 色表中點值(參考 Matplotlib Cividis 文件值)
- **預期踩雷**:Cividis 是 perceptually uniform 色表,若直接線性插值 3-4 個顏色控制點會出現感知不均勻(顏色跳躍);正確做法是儲存完整 256 entry CLUT 或採用 1D CLUT Texture(K2-T4 實作)
- **依賴**:無
- **Phase**:MVP

#### K2-T2:接 AFrameUtilizationHeatmapActor 色彩管道
- **描述**:`AFrameUtilizationHeatmapActor` 目前在 `BuildHeatmap()` 內部用線性插值計算顏色。在 consumer 層建立 `UArchSimHeatmapBridge` ActorComponent,attach 到同一個 Actor 上。`UArchSimHeatmapBridge` 在 `BuildHeatmap()` 完成後(監聽 BlueprintNativeEvent 或在 BP 呼叫序列中手動接管)遍歷 PMC 的 Vertex Buffer,以 `UArchSimColorMap::GetDCColor()` 重新著色頂點。
  - **注意**:`AFrameUtilizationHeatmapActor` 是 FrameCoreUE plugin 的 Actor,**不能修改其 source**。替代方案:繼承建立 `AArchSimHeatmapActor : AFrameUtilizationHeatmapActor`,override `BuildHeatmap()` 後處理頂點色彩。需驗證 `UProceduralMeshComponent::UpdateMeshSection_LinearColor()` API 可在 override 中安全呼叫。
- **涉及 UE5 API**:`AFrameUtilizationHeatmapActor::BuildHeatmap()`(繼承 override)、`UProceduralMeshComponent::UpdateMeshSection_LinearColor()`、`FProcMeshTangent`
- **工時**:4h
- **完成標準**:PIE 中放置 `AArchSimHeatmapActor`,呼叫 `BuildHeatmap()` 後,D/C=0.5 的構件顏色接近 #E69F00(橘黃交界);與 `UArchSimColorMap::GetDCColor(0.5)` 返回值 sRGB 差 < 0.02
- **預期踩雷**:`UpdateMeshSection_LinearColor()` 傳入的 `VertexColors TArray` 長度必須與 Section VertexCount 完全一致,否則 silent crash;建議加 `check(VertexColors.Num() == Section.ProcVertexBuffer.Num())` guard
- **依賴**:K2-T1、K1-T2
- **Phase**:MVP

#### K2-T3:D/C 警告 HUD 色彩凡例 Widget(WBP_ColorLegend)
- **描述**:建立 `WBP_ColorLegend` UMG Widget:一個 300×40px 的漸層矩形(用 Custom Painted Widget 或 Image Material 呈現)+ 左右端 Text(`安全 D/C < 0.5` / `超載 D/C > 1.0`)+ 中間三個 tick 標記(0.5 / 0.8 / 1.0)。Widget 放置在 HUD 右下角,透明度 80%。當 `MaxDC > 1.0` 時,Widget 的 `D/C > 1.0` 標籤改變顏色閃爍(0.5Hz blink,FTimerHandle)。
- **涉及 UE5 API**:`UUserWidget`、`UImage`、`UTextBlock`、`UOverlay`、`FTimerHandle`、`UMaterialInstanceDynamic`(漸層材質)
- **工時**:3h
- **完成標準**:HUD 右下角可見色彩凡例;D/C > 1.0 時標籤閃爍;HUD 不遮擋主場景超過 60px 高
- **預期踩雷**:UMG 的 Image Widget 若使用 Material,需設定 `bIsVariable = true` 才能在 C++/BP 取得 reference 動態換 MID
- **依賴**:K2-T1、K4-T2(UIStyle 字體設定)
- **Phase**:MVP

#### K2-T4:Cividis 色表實作(位移 / von Mises)CLUT Texture 1D + sampler
- **描述**:建立 `T_Cividis_CLUT` 256×1 px 的 1D Texture,以 Python script 生成 Cividis RGB CLUT(參考 Matplotlib `matplotlib.cm.cividis`)並匯入為 UE5 Texture Asset(`Filter=Bilinear`、`SRGB=false`、`AddressX=Clamp`)。在 `M_Heatmap_Cividis` Material 中用 `TextureSample` node 取樣 CLUT。`AArchSimHeatmapActor` 的延伸版本加 `EColorMap` enum property(DC_Traffic / Cividis / Diverging),在 `BuildHeatmap()` 時根據 enum 選擇對應色表。
- **涉及 UE5 API**:`UTexture2D::CreateTransient()`、`TextureSample` Material node、`UMaterialParameterCollection`
- **工時**:4h
- **完成標準**:Position Magnitude 場景(位移大小)下 CLUT 看起來藍→黃符合 Cividis 參考圖;在 PIE 截圖後與 Matplotlib 生成的參考圖比對,色差 < 5 ΔE2000
- **預期踩雷**:Python 腳本生成的 CLUT 需以 `PNG` 格式匯入,UE5 匯入 pipeline 預設開啟 sRGB gamma 校正,**必須手動關閉 `SRGB=false`**,否則顏色偏差嚴重
- **依賴**:K2-T1
- **Phase**:Phase 2

#### K2-T5:連續發散色表(模態振型 / 軸力正負值)
- **描述**:擴充 `UArchSimColorMap` 加入 `GetDivergingColor(float SignedT, float Midpoint=0.f)`:藍 (#0072B2) → 白 (#FFFFFF) → 橘 (#E69F00)。在模態振型 Actor(`AFrameModalShapeActor` 繼承後的 `AArchSimModalShapeActor`)和軸力顯示場景中使用。提供 `T_Diverging_CLUT` 1D Texture 同 K2-T4 方法。
- **涉及 UE5 API**:同 K2-T4
- **工時**:3h
- **完成標準**:負軸力(拉力)呈藍色,正軸力(壓力)呈橘色,零值呈白色;色盲模擬工具(Color Oracle)驗收無法只靠顏色區分拉/壓時有圖示輔助
- **依賴**:K2-T4
- **Phase**:Phase 2

#### K2-T6:色盲輔助模式設定頁
- **描述**:在 `WBP_SettingsPanel` 加「輔助功能」頁籤:勾選「色盲友善模式」→ 所有色表自動切換至 `Cividis`(對 D/C 亦然,取代交通號誌色)並在圖示上加符號標記(數字 1/2/3 標示安全等級,非單靠顏色)。以 `UArchSimGameSettings` DataAsset 儲存 `bColorblindMode` bool,廣播 `FOnColorblindModeChanged` delegate 通知所有 Heatmap Actor 重繪。
- **涉及 UE5 API**:`UCheckBox`、`UGameUserSettings` 擴充、`DECLARE_DYNAMIC_MULTICAST_DELEGATE()`
- **工時**:3h
- **完成標準**:勾選色盲模式 → 全場景熱圖 0.5 秒內重繪;Settings 存檔後重開遊戲保留設定
- **預期踩雷**:`FOnColorblindModeChanged` delegate 廣播時,若場景中有多個 Heatmap Actor,需確保所有 Actor 都已正確 Subscribe;建議用 `UGameInstance::GetSubsystem<UArchSimColorSubsystem>()` 統一管理訂閱
- **依賴**:K2-T4、K2-T5、K4-T2
- **Phase**:Phase 2

#### K2-T7:動態色表混合動畫(Solve 完成漸入效果)
- **描述**:Solve 完成後,熱圖從「灰階/無資料」漸入正確色彩(動畫時長 0.4s,`FTimeline` 或 `UCurveFloat`)。建立 `UCurveFloat` Asset `Curve_HeatmapFadeIn`(0→1 EaseIn 0.4s)。在 `AArchSimHeatmapActor::OnSolveComplete(const FFrameSolveResult&)` 中啟動 Timeline,每 Tick 以 `Alpha * ColorizedVertex + (1-Alpha) * GreyVertex` 插值頂點色彩。
- **涉及 UE5 API**:`FTimeline`、`UCurveFloat`、`UProceduralMeshComponent::UpdateMeshSection_LinearColor()`、`FLinearColor::Lerp()`
- **工時**:3h
- **完成標準**:PIE 中觸發 Solve → 熱圖在 0.4s 內從灰色漸入彩色;FPS 不因 Tick 更新下降超過 5fps(最多 100 根梁的情況)
- **依賴**:K2-T2、K2-T4
- **Phase**:Phase 3

**K2 子節小計工時**:3+4+3+4+3+3+3 = **23h**

---

## K3 音效設計

### 設計原則回顧

主計畫書明確:「少而精、資訊性>裝飾性、可關閉」。核心音效清單共 10 個事件。環境音分室外/室內兩套。倒塌時暫停環境音,突出結構聲。

**注意**:MetaSound 在 UE5.7 的 API 仍有演進中的部分(Quartz 時鐘 API)。MVP 階段建議使用傳統 `USoundCue` / `USoundAttenuation` 方案,Phase 3 才引入 MetaSound 高級功能。

### 詳細 Sub-Task 清單

#### K3-T1:核心操作音效(4 個 SFX SoundCue)
- **描述**:建立 4 個 SoundCue Asset(放 `Content/Audio/SFX/`):
  - `SC_Place_Member`:短促 200-400Hz 木質敲擊,50ms duration,SoundClass=SFX
  - `SC_Remove_Member`:短促電子嗶聲 800Hz,30ms,SoundClass=SFX
  - `SC_Solve_Complete`:極輕電子提示音(鈴聲 1200Hz),80ms,Volume=0.3,SoundClass=SFX
  - `SC_DC_Warning`:中等強度警告聲(警報 600Hz + 諧波),150ms,SoundClass=SFX
  - 建立 `UArchSimSoundManager` BlueprintFunctionLibrary 提供靜態呼叫:`PlayPlaceMember(AActor* Source)` 等,走 `UGameplayStatics::PlaySoundAtLocation()`
- **涉及 UE5 API**:`USoundCue`、`USoundClass`、`UGameplayStatics::PlaySoundAtLocation()`、`USoundBase`
- **工時**:3h(含免費音效 CC0 素材取得 + 匯入)
- **完成標準**:PIE 中放置/移除構件觸發對應音效;Solve 完成播放提示音;D/C > 1.0 播放警告聲;所有音效在 SFX 分軌可獨立調整
- **預期踩雷**:`PlaySoundAtLocation` 在多人 Listen Server 環境下預設是 **Client-Local** 播放(非 replicated);不需要 replicate 音效,但要確認 Server 端(Host 也是 Client)的音效正常
- **依賴**:無
- **Phase**:MVP

#### K3-T2:倒塌音效群組(低頻轟鳴 + Chaos 落地聲 + 環境音暫停)
- **描述**:
  - 建立 `SC_Collapse_Start`:低頻轟鳴 80-200Hz,1.5s duration,`SoundClass=SFX`,在 `AFrameDynCollapseReplayActor::Play()` 開始時播放
  - `Chaos` 落地聲:在 `AFrameFragmentClusterActor` 的 `OnDebrisLanded` 事件(需驗證 UE5.7 Chaos 的 `OnComponentHit` delegate 是否穩定)呼叫 `PlaySoundAtLocation`
  - 建立 `UArchSimAmbientController` 元件:持有 `UAudioComponent* AmbientComponent`;倒塌開始時 `AmbientComponent->AdjustVolume(0.5f, 0.1f)` 壓低環境音;倒塌結束後 `AdjustVolume(1.0f, 2.0f)` 淡回
- **涉及 UE5 API**:`UAudioComponent::AdjustVolume()`、`UPrimitiveComponent::OnComponentHit` delegate、`USoundCue`
- **工時**:4h
- **完成標準**:PIE 觸發倒塌動畫時低頻轟鳴播放;碎片落地後有落地音;環境音在倒塌時壓低 → 結束後淡回;整體觀感不刺耳
- **預期踩雷**:Chaos `OnComponentHit` 在 `AFrameFragmentClusterActor` 裡的 `AStaticMeshActor` 子 Actor 上,需在 `SpawnFragmentDebris()` 後對每個 Debris Actor 訂閱事件,數量可達 1024;考慮改為每 N 次落地才播一次聲(避免同時 1024 個音效)
- **依賴**:K3-T1
- **Phase**:Phase 2

#### K3-T3:環境音 Ambient 系統(MetaSound Graph / 傳統 SoundCue 二選一)
- **描述**:建立兩套環境音 Ambient:
  - 室外:`AC_Ambient_Outdoor` AudioComponent 持有 `SC_Outdoor_Loop`(風聲 + 鳥鳴,SoundWave loop)
  - 室內:`AC_Ambient_Indoor` AudioComponent 持有 `SC_Indoor_Loop`(空調白噪音,SoundWave loop)
  - 兩個 AudioComponent 放置在場景基礎 Actor `AArchSimWorld` 上,**MVP 用傳統 SoundCue**;MetaSound 升級留 Phase 3
  - `USoundAttenuation` 設定讓聲音範圍覆蓋整個場景(MaxDistance=10000cm),避免玩家走遠後聽不到
- **涉及 UE5 API**:`UAudioComponent`、`USoundAttenuation`、`USoundCue` loop 設定、`UAudioComponent::SetPaused()`
- **工時**:3h
- **完成標準**:進入室外場景後環境音自動播放;場景切換(室外 → 室內)時環境音在 1s 內交叉淡入淡出;可在音量 Settings 分軌關閉
- **依賴**:K3-T1
- **Phase**:Phase 2

#### K3-T4:通關/失敗音效 + 教師訊息鐘聲 + Tutorial 提示音
- **描述**:
  - `SC_Level_Complete`:上揚和聲(C-E-G arpegio),400ms,Volume=0.8,SoundClass=Music
  - `SC_Level_Fail`:低調下降音效(不刺耳),300ms,Volume=0.5,SoundClass=SFX
  - `SC_Teacher_Message`:輕鐘聲(鈴鐺 resonance ~2000Hz),200ms,SoundClass=SFX
  - `SC_Tutorial_Hint`:柔和提示音(chime 800Hz),100ms,Volume=0.4,SoundClass=SFX
  - 在對應 SUQS 任務完成/失敗事件的 BP callback 中呼叫 `UArchSimSoundManager::PlayLevelComplete()` 等
- **涉及 UE5 API**:`USoundCue`、`USoundMix`、`UGameplayStatics::PlaySound2D()`(UI 音效不需位置)
- **工時**:2h
- **完成標準**:Tutorial 通關時播放上揚和聲;失敗時低調音效;教師廣播訊息時鐘聲觸發;Tutorial 出現新提示時柔和提示音
- **依賴**:K3-T1
- **Phase**:Phase 2

#### K3-T5:自適應音效混音(MetaSound Quartz 時鐘同步 - Phase 3)
- **描述**:升級 `SC_Collapse_Start` 為 MetaSound Graph,使用 `QuantizedEventTrigger` 節點與 `AFrameDynCollapseReplayActor` 的 `OnEventReached` delegate 同步;每個「元件失效事件」對應一個 MetaSound Trigger,產生有節奏的動態倒塌聲序列。需驗證 UE5.7 的 `UQuartzClockHandle` API 穩定性。
- **涉及 UE5 API**:`UMetaSoundSource`、`UQuartzClockHandle`、`UAudioComponent::GetAudioTime()`
- **工時**:6h
- **完成標準**:倒塌事件時間軸與音效時間軸對齊誤差 < 50ms;MetaSound 在 UE5.7 PIE 不崩潰
- **預期踩雷**:Quartz 在 UE5.7 仍屬 Experimental,API 在 minor 升級可能 breaking;本 Task 列為 Phase 3 且加「需驗證」標記
- **依賴**:K3-T2
- **Phase**:Phase 3

#### K3-T6:音效音量 Settings 分軌(四軌 + SaveGame)
- **描述**:建立 `UArchSimSoundSettings`:持有 `MasterVolume / SFXVolume / MusicVolume / AmbientVolume` 四個 float,預設 1.0。存入 `UArchSimSaveGame`。在 `WBP_SettingsPanel` 的「音效」頁籤提供四個 Slider。每個 Slider 的 `OnValueChanged` 呼叫 `UAudioMixerBlueprintLibrary::SetSoundMixClassOverride()` 修改對應 `USoundClass` 的 volume。
- **涉及 UE5 API**:`USlider`、`UAudioMixerBlueprintLibrary::SetSoundMixClassOverride()`、`USoundClass`、`USoundMix`
- **工時**:3h
- **完成標準**:四個滑桿獨立控制各自音量;拖動即時生效;存檔後重開遊戲音量保留
- **依賴**:K3-T1、K4-T2(UIStyle)
- **Phase**:Phase 3

**K3 子節小計工時**:3+4+3+2+6+3 = **21h**

---

## K4 字體與術語

### 設計原則回顧

主計畫書確立:HUD/按鈕用 Noto Sans CJK TC、數值用 JetBrains Mono、裝飾標題用思源宋體,字級四層(24/18/14/16px)。術語以中文為主,保留英文技術縮寫,中英並列作課程橋接。完整術語表 90+ 詞在附錄 A。

### 詳細 Sub-Task 清單

#### K4-T1:字體資源匯入(Noto Sans CJK TC + JetBrains Mono)
- **描述**:
  - 下載 Noto Sans CJK TC(OFL 授權)的 `NotoSansCJKtc-Regular.otf` + `NotoSansCJKtc-Bold.otf`,匯入 `Content/Fonts/`
  - 下載 JetBrains Mono Regular + Bold OTF
  - 在 UE5 Font Asset 中建立 `FA_NotoSansCJK_TC`(含 Regular + Bold 兩個 typeface),`FA_JetBrainsMono`
  - 設定 `SubFontFamily` fallback:若字元不在 Noto 覆蓋範圍,fallback 到 UE5 內建 Roboto(英數)
  - **子集化考量**:完整繁體 CJK 字型 OTF > 5MB;建議以 `pyftsubset`(fonttools)生成僅含常用 4000 字的子集版本,降低 pak 體積
- **涉及 UE5 API**:`UFont`、`UFontFace`、`FCompositeFont`
- **工時**:3h(含 Python 子集化腳本撰寫)
- **完成標準**:UMG TextBlock 設定 `FA_NotoSansCJK_TC` 後,繁體中文顯示正確無方框替代字;英文與中文混排無異常間距
- **預期踩雷**:UE5 的 `UFont` 在 `SRGB` Texture 模式下有時與 UMG 的 DPI Scaling 出現 subpixel 渲染模糊;建議 Font Asset 的 `LegacyFontSize` 設為 0(自動),`CompositeFont` fallback chain 設定正確
- **依賴**:無(首要資源準備)
- **Phase**:MVP

#### K4-T2:UI Style Guide DataAsset(UArchSimUIStyle)
- **描述**:建立 `UArchSimUIStyle : UDataAsset`,持有:
  - `TitleFont / BodyFont / CodeFont`:各對應一個 `FSlateFontInfo`(指向 K4-T1 的 Font Asset)
  - `TitleFontSize=24 / SubtitleFontSize=18 / BodyFontSize=14 / WarningFontSize=16`(int32)
  - `PrimaryColor=#1A1A2E / AccentColor=#009E73 / WarningColor=#D55E00 / BackgroundColor=#F5F5F0`
  - `ButtonStyle / InputFieldStyle`:各一個 `FButtonStyle / FEditableTextBoxStyle`(Slate style)
  - 此 DataAsset 作為**全域 UIStyle 注入點**,所有 Widget 的 `SetStyle()` 都從這裡取值
- **涉及 UE5 API**:`UDataAsset`、`FSlateFontInfo`、`FButtonStyle`、`FSlateColor`
- **工時**:3h
- **完成標準**:Editor 中可在 DataAsset Inspector 看到所有欄位;建立一個測試 Widget `WBP_StyleTest` 驗證所有字級和顏色正確顯示
- **預期踩雷**:`FSlateFontInfo` 的 `TypefaceFontName` 欄位必須與 `UFont` Asset 內的 typeface name 完全對應,否則 fallback 到預設字體 Roboto 且不報錯(靜默失敗)
- **依賴**:K4-T1
- **Phase**:MVP

#### K4-T3:核心術語表 DataTable(DT_TermDictionary)
- **描述**:建立 Row Struct `FArchSimTerm`:
  - `TermId` (FName, primary key)
  - `ChineseName` (FText)
  - `EnglishName` (FText)
  - `ShortDefinition` (FText, max 50 字)
  - `LongDefinition` (FText, max 200 字,可選)
  - `RelatedTermIds` (TArray<FName>)
  - `SubjectArea` (FText,對應課綱科目名稱)
  建立 `DT_TermDictionary` DataTable(`Content/Data/`),匯入附錄 A 的 90 個術語。建立 `UArchSimTermLookup : UBlueprintFunctionLibrary` 提供 `FindTerm(FName TermId, FArchSimTerm& OutTerm)` BP callable 方法。
- **涉及 UE5 API**:`UDataTable`、`FTableRowBase`、`UDataTable::FindRow<>()`
- **工時**:4h(含術語資料輸入)
- **完成標準**:DataTable 匯入後有 90 行;`FindTerm(FName("BendingMoment"))` 返回「彎矩(Bending Moment)」;CSV 格式可匯出作備份
- **預期踩雷**:DataTable 的 FText 欄位在 PIE 中 Localization 未啟用時顯示 FText 原始字串;需確認 UE5 `DefaultGame.ini` 的 `Culture=zh-TW` 設定不影響 DataTable 顯示
- **依賴**:K4-T2
- **Phase**:MVP

#### K4-T4:HUD 響應式縮放系統
- **描述**:建立 `UArchSimHUDScaler` BlueprintFunctionLibrary:
  - `static float GetDPIScale(UObject* WorldContext)`:回傳目前視窗解析度對 1920×1080 的縮放比例,基本公式 `Min(W/1920.f, H/1080.f)`
  - 所有 Widget 在 `NativeConstruct()` 中取得 DPI Scale 後呼叫 `SetRenderScale(FVector2D(Scale, Scale))`
  - `WBP_HUD` 根 Widget 設定 `DPI Scaling Rule = Custom`(在 Project Settings → User Interface → DPI Scaling 設定自訂曲線:1280px → 0.8x, 1920px → 1.0x, 2560px → 1.3x)
- **涉及 UE5 API**:`UUserWidget::SetRenderScale()`、`FViewport::GetSizeXY()`、`UGameViewportClient`
- **工時**:2h
- **完成標準**:在 1366×768(校園廉價機常見解析度)下字體可讀、按鈕可點擊;在 1920×1080 基準解析度下字級符合 K4-T2 的設定
- **預期踩雷**:UE5 內建 DPI Scaling 可能與自訂 `SetRenderScale` 衝突造成雙重縮放;需擇一方案:要麼全用 Project Settings DPI curve,要麼全用 Widget 的 SetRenderScale,不可混用
- **依賴**:K4-T2
- **Phase**:Phase 2

#### K4-T5:術語 Tooltip 系統
- **描述**:建立 `WBP_TermTooltip` Widget:120×80px 浮動面板,顯示 `ChineseName` (14px Bold) + `EnglishName` (12px Italic Grey) + `ShortDefinition` (12px)。建立 `UArchSimTooltipComponent : UActorComponent`:attach 到任何有術語需要解釋的 UI 元素;當 `OnHovered` 事件觸發時,呼叫 `UArchSimTermLookup::FindTerm()` 並顯示 `WBP_TermTooltip`。在所有 HUD Widget 的術語標籤(如「D/C 比」、「彎矩」等)的 TextBlock 設定 `ToolTipWidget = WBP_TermTooltip`。
- **涉及 UE5 API**:`UWidget::SetToolTip()`、`UUserWidget::OnHovered`、`SToolTip` Slate wrapper
- **工時**:3h
- **完成標準**:滑鼠懸停「D/C 比」文字 0.5s 後彈出 Tooltip;Tooltip 顯示「需求容量比(D/C Ratio)」+ 短定義;滑鼠移開 Tooltip 消失
- **預期踩雷**:UMG 的 `SetToolTip()` 在 GameViewport Focus 下有時不觸發;建議改用自訂 `OnHovered` delegate 手動控制 Tooltip Widget 的 Visibility
- **依賴**:K4-T3、K4-T2
- **Phase**:Phase 2

#### K4-T6:多語在地化框架
- **描述**:啟用 UE5 Localization Dashboard(`Edit → Localization Dashboard`)。建立兩個 Locale:`zh-TW`(預設繁體中文)、`en`(英文,未來擴充)。所有 Widget 的 FText 字串包在 `NSLOCTEXT()` 或 `LOCTEXT()` macro,並匯入 LocText 字串表。確認 DataTable 的 FText 欄位走 Localization pipeline(`FText::FromStringTable()`)。
- **涉及 UE5 API**:`FText::FromStringTable()`、`NSLOCTEXT()`、`UKismetInternationalizationLibrary::SetCurrentCulture()`
- **工時**:4h
- **完成標準**:設定語言為「zh-TW」時所有 UI 顯示繁體中文;切換「en」後 UI 顯示英文(即使英文文案只是暫時佔位文字);DataTable 術語在兩種 Locale 下均可讀
- **預期踩雷**:UE5 Localization Dashboard 在 Project 很大時 Gather Text 掃描時間極長(幾分鐘);建議加 `.locexclude` 設定排除 Plugin Source 目錄
- **依賴**:K4-T3
- **Phase**:Phase 3

#### K4-T7:術語 Flashcard 迷你遊戲
- **描述**:建立 `WBP_TermFlashcard` Widget 迷你遊戲:從 `DT_TermDictionary` 隨機抽取 10 個術語,顯示英文名稱讓學生選出正確中文定義(4 選 1 MC)。教師可在設定頁啟用「課前暖身」讓學生進入關卡前先過 Flashcard。正確率記錄到 xAPI Statement(`verb=answered`)透過 Part I 的 Log 系統送出。
- **涉及 UE5 API**:`UWidgetSwitcher`、`UButton`、`UDataTable::GetAllRows()` 隨機抽樣、`FRandomStream`
- **工時**:5h
- **完成標準**:10 題 MC 可完整作答;正確/錯誤即時反饋(綠框/紅框);完成後顯示正確率;正確率 Statement 送出(需 Part I 的 xAPI 函式庫)
- **預期踩雷**:xAPI Statement 依賴 Part I 的 Log 系統(需在 Part I 完成後整合);MVP 可先 log 到本地 `UE_LOG`,Phase 2 接 Part I 的 Statement 管道
- **依賴**:K4-T3、K4-T2;完整功能依賴 Part I Log 系統
- **Phase**:Phase 3

**K4 子節小計工時**:3+3+4+2+3+4+5 = **24h**

---

## Part K 整體總計

| 子節 | Sub-task 數 | 工時合計 |
|------|-------------|---------|
| K1 視覺風格 | 7 | 21h |
| K2 色彩語意 | 7 | 23h |
| K3 音效設計 | 6 | 21h |
| K4 字體與術語 | 7 | 24h |
| **Part K 合計** | **27** | **89h** |

加上整合測試、Asset 取得(免費 CC0 音效素材、字型下載)、Buffer:約 **+20%** = 實際估算 **107h ≈ 112h**

以 1-2 人全職團隊計算:
- **MVP 必須 tasks**:約 32h(3-4 個工作日)
- **Phase 2 tasks**:約 46h(5-6 個工作日)
- **Phase 3 tasks**:約 34h(4-5 個工作日)

---

## 誠實風險彙整

1. **FrameCore FROZEN 邊界**:`AFrameUtilizationHeatmapActor` 的色彩計算硬碼在 plugin C++ 中。本計畫採用繼承 `AArchSimHeatmapActor` override 的方式繞過,但若 `BuildHeatmap()` 是 non-virtual 方法,則需在 `PostActorCreated()` 或 `BeginPlay()` 後手動呼叫 `UpdateMeshSection_LinearColor()` 覆蓋,需實際驗證 `FrameUtilizationHeatmapActor.h` 的 `BuildHeatmap()` 是否標 `virtual`。

2. **音效資產授權**:所有音效若使用 CC0/CC-BY 免費素材,需在 README 中標明授權來源。若使用 UE Marketplace 音效包,需確認授權是否允許學術/教育發布。

3. **字型子集化**:Noto Sans CJK TC 完整字型 > 5MB,若不子集化會使安裝包過大(校園部署限制)。`pyftsubset` 腳本需加入 CI pipeline,確保每次 Build 前字型已子集化。

4. **色彩語意跨工具混淆**:本系統「橘=壓力」與 Karamba3D「磚紅=壓力」有差異。HUD 色彩凡例(K2-T3)的說明文字必須明確寫出本系統的色彩約定,教師備課文件也需說明。

5. **低端機效能**:校園機普遍使用 Intel UHD 730(4-16 GB RAM,無獨顯)。所有視覺效果需在此配置下維持 30fps。K1-T6 的品質選項和 K2-T4 的 CLUT Texture 方案在低端機上 GPU 讀取開銷需實際量測。


---


# Part L 實作擴充(312h)

**標題**:Part L — Roadmap + MVP

**子節數**:3

### MVP 必須(Phase 1)
- L1-P0-1 Plugin 環境建置與 EngineVersion patch
- L1-P0-2 FrameCoreUE Plugin 整合驗證
- L1-P0-3 AArchSimCharacter 骨架建立
- L1-P0-4 UFrameInteractiveSubsystem 初始化驗證
- L1-P1-1 UArchSimMemberData ActorComponent 實作
- L1-P1-2 UArchSimModelRegistry 雙向映射系統
- L1-P1-3 FrameCore Solve 呼叫與結果分發
- L1-P1-4 ALS 角色與 Enhanced Input 設定
- L1-P1-5 構件放置系統(Prefabricator + Snap + OccupancyGrid)
- L1-P1-6 載重施加 GUI(節點力 + 自重)
- L1-P1-7 D/C 熱圖整合(AFrameUtilizationHeatmapActor)
- L1-P1-8 單機沙盒模式整合測試
- L1-P1-9 SUQS 關卡系統 + 3 個 Tutorial 關卡
- L2-1 Solve 效能基準(50 構件 <2s)
- L2-2 熱圖更新 <500ms 端到端驗證
- L2-3 Tutorial 關卡通關驗證(L-01 ~ L-03)
- L2-4 SaveGame round-trip 正確性測試
- L2-5 規範對標警告 W-01~W-05 中文顯示
- L2-6 5-leg gate 全綠確認

### Phase 2
- L1-P2-1 崩塌動畫整合(AFrameDynCollapseReplayActor)
- L1-P2-2 Chaos 碎片 Actor(AFrameFragmentClusterActor)
- L1-P2-3 連通性子系統(UFrameStructureGroupSubsystem)
- L1-P2-4 2D/3D 四象限工作區
- L1-P2-5 圖學標註系統(UE 正交投影 viewport)
- L1-P2-6 藍圖系統(Prefabricator runtime spawn)
- L1-P2-7 應力掃描儀工具(D1)
- L1-P2-8 變形動畫 + BMD/SFD + 安全係數面板(D2-D4)
- L1-P2-9 施工工序狀態機(E1-E4)
- L1-P2-10 學習閉環機制(F3-F5)
- L1-P2-11 多人協作 LAN(H1-H4 Listen Server + NULL Subsystem)
- L1-P2-12 教師工具 + xAPI 學習 log + 老師 dashboard(I1-I4)
- L1-P2-13 測量關卡 8 題型(Part G)
- L3-1 UE5.8 升級兼容性驗證計畫建立

### Phase 3
- L1-P3-1 MctoNurbs sidecar 整合(IFC/DXF 匯出)
- L1-P3-2 PCG 地形 5 模板完整版
- L1-P3-3 多語在地化(中英)
- L1-P3-4 效能優化(World Partition / LOD / Nanite)
- L1-P3-5 TeacherGuide.md + 學生反思工具庫
- L1-P3-6 SQL LRS 完整部署 + 老師 dashboard 三頁
- L1-P3-7 Release build 5-leg gate + 發布準備

### 實作順序
- L1-P0(1週):四個 Plugin 環境建置
- L1-P1-1~3(2週):FrameCore 接合層
- L1-P1-4(1週):ALS 角色系統
- L1-P1-5(3週):構件放置系統
- L1-P1-6(2週):載重施加 GUI
- L1-P1-7(1週):D/C 熱圖整合
- L1-P1-8(1週):沙盒模式整合測試
- L1-P1-9(2週):SUQS 關卡系統 + 3 個 Tutorial 關卡
- MVP 整合測試(2週):L2 十項驗收
- L3 風險監控(平行進行,整個 Phase 0-1 期間)

### 跨 Part 依賴
- Part A(引擎接合層)必須先完成 A1/A2 才能開始 L1-P1
- Part B(構件放置系統)B1 必須先完成才能進行 L2 驗收條件 1/2/4
- Part C(載重與模擬)C1/C2 必須先完成才能進行 L2 驗收條件 2/3
- Part F(沙盒與關卡)F1/F2 必須先完成才能進行 L2 驗收條件 3/5/8
- Part E(施工工序)是 Phase 2 依賴,不阻 MVP
- Parts H/I(多人/教師工具)是 Phase 2 依賴,不阻 MVP
- Part G(測量關卡)是獨立支線,Phase 2 可平行開發

### 風險清單
- UE5.8 replication breaking changes — 鎖定 UE5.7 至 MVP 完成
- SPUD World Partition issue #117 — MVP 不用 World Partition
- ALS Iris Push Model 自訂 subclass dirty mark 漏 — 嚴格遵守 MARK_PROPERTY_DIRTY_FROM_NAME
- ~~Prefabricator .uplugin EngineVersion 未標 5.7~~ → **✅ 已修(S-00)**
- FrameCore Solve 在 1000+ 構件下卡頓 — 限制單關卡 ≤500 構件
- Listen Server Host 效能瓶頸 — Solve 佇列 + cooldown
- NULL Subsystem 跨 VLAN 失敗 — 直連 IP fallback
- 學習 log Safari Background Sync — visibilitychange fallback
- FrameCore API 不夠用(如缺沿桿取樣)— UE consumer 層補 helper
- 開發團隊離職 — ARCHITECTURE.md + HANDOFF.md 持續維護

## 詳細擴充內容


# Part L — Roadmap + MVP 詳細實作計畫

> 本文件是主計畫書 Part L(第十四章)的**可執行展開版**。
> 前提條件:FrameCore v4.0.0 engine source FROZEN,所有開發在 FrameCoreUE consumer 側進行,不動 `Plugins/FrameSolver/Source/FrameCore/`。
> 團隊規模假設:1-2 人全職,每人每週有效開發時數約 35-40 小時。

---

## L1 Phase 0-3 詳細實作計畫

### Phase 0 — 環境準備(Day 1-7,估計 40 小時)

Phase 0 的目標是讓四個外部 Plugin 與 FrameCore 在同一個 UE5.7 project 中同時跑通,且不破壞既有的 5-leg gate。這一週決定後續所有開發的基礎穩固程度,若 Plugin 整合過程遇到衝突要早發現早解決。

#### P0-1 Plugin 環境建置與 EngineVersion patch

**任務描述:**
Clone 四個 Plugin(ALS-Refactored v4.17、Prefabricator UE5、SPUD、SUQS)進 `<repo-root>\Plugins\`(完整 URL 跟 tag 見 [`docs/SPRINT_NOTES.md`](SPRINT_NOTES.md) Spike 1 表)。修改各 Plugin 的 `.uplugin` 中 `EngineVersion` 欄位為 `"5.7.0"`(ALS 4.17 已含;Prefabricator / SPUD / SUQS 需新增),否則 UE Editor 會拒絕載入並回報 incompatible plugin 錯誤。

**涉及 API / 路徑:**
- `Plugins/ALS-Refactored/ALS.uplugin`
- `Plugins/Prefabricator/Prefabricator.uplugin`
- `Plugins/SPUD/SPUD.uplugin`
- `Plugins/SUQS/SUQS.uplugin`
- `Config/DefaultEngine.ini`:新增 `DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput`(ALS 強制要求)

**完成標準:**
- UE Editor 開啟 ArchSim.uproject 時,Output Log 無 `Plugin 'X' failed to load` 錯誤
- 四個 Plugin 出現在 Edit → Plugins 面板且狀態為 Enabled

**預期踩雷:**
- Prefabricator 的 `EngineVersion` 欄位若只設 major minor 不設 patch(如 `"5.7"` 而非 `"5.7.0"`),UE5.7 解析可能不認,需要三段式版本號
- ALS-Refactored v4.17 的 Enhanced Input 依賴需要在 `ArchSim.uproject` 的 `"Plugins"` 陣列中加入 `EnhancedInput` 啟用宣告,否則 BP 編譯時找不到 Input Action 型別

**估計工時:** 4 小時
**MVP:** 必須

---

#### P0-2 FrameCoreUE Plugin 整合驗證

**任務描述:**
確認 FrameCoreUE 在加入四個新 Plugin 後仍能正常編譯,且 8 個 BP Actor 可在新建空關卡中實例化。執行 UE automation test runner 確認 135 個 `FrameCore.*` 測試仍全綠。

**涉及 API / 路徑:**
- `Plugins/FrameSolver/Source/FrameCoreUE/` — UE consumer 側(可動,不含引擎核心)
- `UFrameInteractiveSubsystem`、`AFrameUtilizationHeatmapActor`、`AFrameDeformedShapeActor` 等 8 個已有 Actor
- UE Automation Test Runner:`-ExecCmds "Automation RunTests FrameCore"`

**完成標準:**
- `%UE_ENGINE_ROOT%\Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development` 回 exit 0,無 error(`UE_ENGINE_ROOT` 預設值見 README 環境章節)
- 所有 135 個 `FrameCore.UE.*` 測試 PASS(含 cuDSS build 的 135 或 non-cuDSS 的 133)
- 新建空 Level 可拖放 `AFrameUtilizationHeatmapActor` 並在 Detail 面板看到其 UPROPERTY

**預期踩雷:**
- 多個 Plugin 同時存在時,UBT Adaptive Build 可能因 Unity File 衝突導致某個 TU 重複包含頭檔。症狀是 `redefinition of X` compile error,對策:確認 FrameCoreUE 的 `FrameCore.Build.cs` 中已設 `bUseUnity = false`

**估計工時:** 8 小時
**依賴:** P0-1
**MVP:** 必須

---

#### P0-3 AArchSimCharacter 骨架建立

**任務描述:**
建立繼承自 `AAlsCharacter` 的 `AArchSimCharacter` C++ 類別,掛載 `UAlsCameraComponent`,設定兩個 Enhanced Input MappingContext 的骨架(`MC_Locomotion` priority 0 / `MC_Building` priority 1)。此時只需讓角色可正常在場景中行走,不需完整的建築模式邏輯。

**涉及 API / 路徑:**
- `AAlsCharacter`(ALS-Refactored v4.17)
- `UAlsCameraComponent`
- `UEnhancedInputComponent`、`UEnhancedInputLocalPlayerSubsystem`
- `UInputMappingContext`(DataAsset)
- 新增 C++ 類別:`Source/ArchSim/Private/Characters/ArchSimCharacter.h/.cpp`

**完成標準:**
- PIE 模式下角色可 WASD 行走、滑鼠控制視角,不卡頓
- 按 E 鍵可在 Output Log 看到 `[ArchSim] Switched to BuildingMode` / `[ArchSim] Switched to LocomotionMode` 的 UE_LOG
- Character Blueprint(繼承自 `AArchSimCharacter`)設定為 GameMode 的預設 DefaultPawnClass

**預期踩雷:**
- ALS `AAlsCharacter` 的 BeginPlay 會搶設 InputMode,若自訂 `BeginPlay` 中沒有先 `Super::BeginPlay()` 再做 InputMapping 的設定順序,Enhanced Input 的 MappingContext 會被 ALS 的初始化覆蓋清空

**估計工時:** 8 小時
**依賴:** P0-1、P0-2
**MVP:** 必須

---

#### P0-4 UFrameInteractiveSubsystem 初始化驗證

**任務描述:**
確認 `UFrameInteractiveSubsystem`(已在 FrameCoreUE 中實作)在遊戲開始時由 UE5 GameInstance Subsystem 機制自動建立,並可被 `AArchSimCharacter` 或 GameMode 取得指標。撰寫一個簡單的 PIE 初始化測試:BeginPlay 時 `GetSubsystem<UFrameInteractiveSubsystem>()->StartSession(…)`,Solve 一個單柱模型,確認 `bSingular=false`。

**涉及 API / 路徑:**
- `UGameInstance::GetSubsystem<UFrameInteractiveSubsystem>()`
- `UFrameInteractiveSubsystem::StartSession` / `EndSession`
- `FFrameModelDef`、`FFrameSolveResult`(已有 USTRUCT)

**完成標準:**
- PIE BeginPlay 能成功呼叫 `StartSession` 且無 nullptr dereference
- 單柱模型(1 Node fixed + 1 Node free + 1 Member)Solve 結果 `bSingular=false`、`MemberUtilization.size()==1`

**預期踩雷:**
- v3.5.1 已知踩雷:headless `-nullrhi -unattended` 模式下 GameInstance 可能為 null,但 PIE 模式有正常 GameInstance,此處不應出現;若出現,說明 Subsystem 的 `ShouldCreateSubsystem` 回傳 false,需確認 `GameInstance` 型別有被正確設定在 GameMode

**估計工時:** 4 小時
**依賴:** P0-2、P0-3
**MVP:** 必須

**Phase 0 總工時:** 約 24 小時(含緩衝 40 小時,含 UE Build 等待時間)

---

### Phase 1 — MVP 結構閉環(Day 8-90,估計約 200 小時)

#### L1-P1-1 UArchSimMemberData ActorComponent 實作

**任務描述:**
新建 `UActorComponent` 子類別 `UArchSimMemberData`。儲存欄位:
- `int32 MemberIdx = -1`(FrameCore 中的 0-based 索引,-1 表示未註冊)
- `int32 StructureGroupId = -1`
- `float CachedUtilization = 0.0f`
- `EMemberType MemberType`(Beam / Column / Shell)
全部標記 `UPROPERTY(SaveGame, BlueprintReadOnly)`。提供 Blueprint callable 的 `GetCachedUtilization()`、`GetMemberIdx()` accessor。

**涉及 API / 路徑:**
- 新檔:`Source/ArchSim/Private/Components/ArchSimMemberData.h/.cpp`
- `UPROPERTY(SaveGame, BlueprintReadOnly, Category="ArchSim")`
- `UFUNCTION(BlueprintCallable, Category="ArchSim")`

**完成標準:**
- 元件可在 Blueprint Actor 中「Add Component」找到 `UArchSimMemberData`
- SaveGame round-trip:Actor 放置、設定 MemberIdx=5、儲存關卡、重載後 MemberIdx 仍為 5
- UE Automation Test `ArchSim.MemberData.SaveRoundTrip` PASS

**預期踩雷:**
- SaveGame 序列化需要 Actor 本身也有 `UPROPERTY(SaveGame)` 標記,單純 Component 的 SaveGame UPROPERTY 在 SPUD 的 Actor-scope 序列化下才有效;需確認 SPUD 的 `ISpudObject` 是否需要 implement

**估計工時:** 8 小時
**MVP:** 必須

---

#### L1-P1-2 UArchSimModelRegistry 雙向映射系統

**任務描述:**
新建 `UGameInstanceSubsystem` 子類別 `UArchSimModelRegistry`。維護:
- `TMap<AActor*, int32> ActorToMemberIdx`
- `TMap<int32, TWeakObjectPtr<AActor>> MemberIdxToActor`
- 內部 `FFrameModelDef CurrentModel`(輸入端 USTRUCT)

主要 API:
- `int32 RegisterMember(AActor* Actor, int32 MaterialId, int32 SectionId, const FVector& NodeA, const FVector& NodeB)` — 建立對應並回填 MemberIdx 給 Actor 上的 `UArchSimMemberData`
- `void DeactivateMember(AActor* Actor)` — 透過 `FFrameModelPatch::DeactivateMemberIds` 軟刪除
- `void DistributeSolveResult(const FFrameSolveResult& Result)` — 把 `MemberUtilization` 寫回各 Actor 的 `CachedUtilization`

**涉及 API / 路徑:**
- `UFrameInteractiveSubsystem::ApplyPatchAndResolve` / `Rebaseline`
- `FFrameModelPatch`(已有 USTRUCT)
- `FFrameModelDef`、`FFrameMember`(已有 USTRUCT)
- 新檔:`Source/ArchSim/Private/Subsystems/ArchSimModelRegistry.h/.cpp`

**完成標準:**
- 連續呼叫 `RegisterMember` 50 次,總耗時 <50ms(用 `UE_LOG` 時間戳確認)
- 移除一個 Actor 後 `DeactivateMember` 呼叫,下次 Solve 的 `FFrameSolveResult.MemberUtilization` 中該 member 值為 0 或 NaN(引擎回傳 inactive 值),不影響其他 member
- `DistributeSolveResult` 後,可在 Blueprint 讀到 `CachedUtilization` 更新值

**預期踩雷:**
- 連續刪除構件時若真刪除 `CurrentModel.members[]` 陣列元素,會導致後面所有 MemberIdx 全部偏移。必須用 `bActive=false` 軟刪除,FrameCore 的 `DeactivateMemberIds` API 接受這種模式。
- `ApplyPatchAndResolve` 的 Woodbury rank 累積:Registry 需要 debounce timer(150ms)+ rank 超過 MaxRank=96 時自動呼叫 `Rebaseline()`

**估計工時:** 16 小時
**依賴:** L1-P1-1、P0-4
**MVP:** 必須

---

#### L1-P1-3 FrameCore Solve 呼叫與結果分發

**任務描述:**
在 `UArchSimModelRegistry` 或獨立的 `UArchSimSolveCoordinator` 中,實作 Solve 流程:
1. 接收玩家的構件修改請求(放置/移除/材料變更)
2. 判斷是走 `ApplyPatchAndResolve`(小改動,Woodbury)還是 `Rebaseline + ResolveCurrent`(材料/截面改變)
3. Solve 完成後呼叫 `DistributeSolveResult`
4. 廣播 `FOnSolveComplete` delegate 給所有需要知道結果的 UE Component(熱圖 Actor 等)

**涉及 API / 路徑:**
- `UFrameInteractiveSubsystem::ApplyPatchAndResolve(FFrameModelPatch, FFrameSolveResult&)`
- `UFrameInteractiveSubsystem::Rebaseline()`
- `UFrameInteractiveSubsystem::ResolveCurrent(FFrameSolveResult&)`
- `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSolveComplete, const FFrameSolveResult&, Result)`

**完成標準:**
- 50 根梁的懸臂橋場景 Solve 時間 <2 秒(CPU 路徑,符合 L2 驗收條件 1)
- `bSingular=true` 時(機構不穩定),Solve 不 crash,且 `DistributeSolveResult` 跳過分發並在 HUD 顯示警告
- 單元測試:`ArchSim.SolveCoordinator.CantileverParity` 確認與 FrameCore standalone F68 cantilever fixture 結果差 <1e-5

**預期踩雷:**
- `FFrameSolveResult` 含 TArray,若直接從 Subsystem 回傳到 Blueprint 要注意值拷貝成本(50 構件約 200 bytes,可接受;500 構件以上需考慮只傳 slim 摘要)
- Woodbury rank 累積超過 MaxRank 若未自動 Rebaseline 會導致 assert 或數值不穩定,需確認 Subsystem 的 rank 追蹤邏輯有暴露查詢接口

**估計工時:** 16 小時
**依賴:** L1-P1-2
**MVP:** 必須

---

#### L1-P1-4 ALS 角色與 Enhanced Input 完整設定

**任務描述:**
完善 `AArchSimCharacter` 的輸入系統。定義 DataAsset `DA_ArchSimInputActions`:
- `IA_PlaceMember`(Left Mouse Button,Building 模式)
- `IA_RemoveMember`(Right Mouse Button 長按,Building 模式)
- `IA_OpenAnalysisPanel`(Tab 鍵)
- `IA_ToggleHeatmap`(H 鍵)
- `IA_ApplyLoad`(L 鍵 + 在構件上 Hover 時)

在 `AArchSimCharacter::SetupPlayerInputComponent` 中 bind 這些 Action。建立第三人稱相機俯瞰模式:觀察模式下 ScrollWheel 控制 SpringArm 長度(3-20 公尺範圍)。

**涉及 API / 路徑:**
- `UEnhancedInputComponent::BindAction`
- `UInputAction` DataAsset
- `USpringArmComponent`(觀察模式下 arm length override)
- `AAlsCharacter` 繼承鏈的 `SetupPlayerInputComponent` super call 順序

**完成標準:**
- 按 E 鍵在 Building 模式下可 Left Click 輸出 `IA_PlaceMember Triggered` log
- 按 H 鍵觸發 `IA_ToggleHeatmap`,熱圖 Actor 可見性正確 toggle
- 滑鼠滾輪在觀察模式下控制相機距離,範圍 [3m, 20m]

**預期踩雷:**
- ALS 的 `AAlsCharacter` 使用自訂 `ALSLook` / `ALSMove` Input Action,若在 `MC_Locomotion` 中定義衝突的 Action Binding,可能覆蓋 ALS 的原有行為。對策:ALS 原有 `IA_*` 保持不動,自訂 Action 只放進 `MC_Building`。

**估計工時:** 12 小時
**依賴:** P0-3
**MVP:** 必須

---

#### L1-P1-5 構件放置系統(Prefabricator + Snap + OccupancyGrid)

**任務描述:**
這是 Phase 1 最複雜的任務。分三個子任務:

**P1-5a Prefab 定義(3 天):**
用 Prefabricator 定義至少 5 大類構件 Prefab(柱 Column / 梁 Beam / 板 Slab / 基礎 Foundation / 斜撐 Brace),每類至少 3 種截面變體。每個 Prefab 的 Root Actor 掛載 `UArchSimMemberData`。

**P1-5b Ghost Preview(2 天):**
Left Click 進入放置模式時,產生半透明 Ghost Actor(Prefabricator 的 `APrefabActor` 設 Material 為 `M_GhostPreview`,opacity 0.4)。滑鼠 Hover 時 Ghost 跟隨 `GetHitResultUnderCursor` 的 Impact Point,並 Snap 到最近格子座標(格子大小 50cm,`FIntVector = FMath::RoundToInt32(pos / 50.0f)`)。

**P1-5c Server 仲裁(3 天):**
`Server_RequestPlacement` RPC → Server 查詢 `TMap<FIntVector, FActorHandle> OccupancyGrid` → 若格空則 Spawn + Register + Multicast `NotifyPlacementSuccess` → 若格佔則 Multicast `NotifyPlacementFailed`(Client Ghost 自動消除)。

**涉及 API / 路徑:**
- `APrefabricatorAsset`、`APrefabActor`(Prefabricator Plugin)
- `APlayerController::GetHitResultUnderCursor`
- `UWorld::SpawnActor<T>`
- `UFUNCTION(Server, Reliable)` / `UFUNCTION(NetMulticast, Reliable)`
- 新建:`UArchSimPlacementSubsystem`(處理格子佔用邏輯)

**完成標準:**
- 拖放 5 種構件到場景中,Ghost 正確 Snap 到格子
- Server 仲裁:兩個 Client 同時 Request 同一格,只有一個成功,另一個 Ghost 消除
- 50 個構件放置後 OccupancyGrid 查詢正確,`RegisterMember` 均被呼叫

**預期踩雷:**
- Prefabricator 在 Runtime(非 Editor)下 spawn 行為需要啟用 Prefabricator 的 Runtime 模式(`.uplugin` Type 需為 Runtime,而非 Editor Only)。這是計畫書中指出的潛在風險。
- Ghost Actor 若共用材質 Instance 而未 Dynamic 建立,多個 Ghost 同時存在時會產生材質狀態衝突

**估計工時:** 40 小時(含 Server 仲裁)
**依賴:** L1-P1-1、L1-P1-2
**MVP:** 必須

---

#### L1-P1-6 載重施加 GUI

**任務描述:**
實作四種載重類型的施加介面:
1. **節點力(Nodal Load)**:在節點上按 L 鍵開啟 UI,選擇 Fx/Fy/Fz 方向和大小
2. **自重(Self Weight)**:全局開關,開啟時自動計算每個 Member 的 `FFrameMemberUDL` based on density × section area × g
3. **活載重(Live Load, 均布)**:`FFrameMemberUDL` 套用在選取的梁構件
4. **殼壓(Shell Pressure)**:選取 Slab Actor 後輸入 kN/m²

**涉及 API / 路徑:**
- `FFrameNodalLoad`、`FFrameMemberUDL`、`FFrameShellPressure`(已有 USTRUCT)
- `UCommonActivatableWidget`(UMG,載重輸入面板)
- `FFrameMaterial.Density`(已有欄位,材料庫已含密度)
- `UArchSimModelRegistry::UpdateLoad(LoadType, …)` — 需在 UE consumer 層新增 helper

**完成標準:**
- 節點力輸入 10kN 向下後觸發 Solve,懸臂梁末端位移非零且方向正確
- 自重開關後,空載的單跨梁 D/C 值從 0 升到一個合理的小正值(0.01~0.3 範圍)
- 載重 Arrow Actor 可視化:每個 Nodal Load 在節點位置顯示有方向 + 大小的箭頭 Actor

**預期踩雷:**
- 自重計算中,構件幾何方向(`NodeA`/`NodeB` 的 UE cm 座標)需轉換到 FrameCore 的 mm 單位制,否則自重被低估 10× 或高估 10×。需確認 `FFrameNode` 使用 mm 而 UE 場景使用 cm 的單位換算係數 `×10`。

**估計工時:** 24 小時
**依賴:** L1-P1-2、L1-P1-3
**MVP:** 必須

---

#### L1-P1-7 D/C 熱圖整合(AFrameUtilizationHeatmapActor)

**任務描述:**
`AFrameUtilizationHeatmapActor` 已在 FrameCoreUE 中實作完畢(v3.5.0),此任務是把它整合進遊戲流程:
1. 在場景中常駐一個 `AFrameUtilizationHeatmapActor` 實例
2. 每次 Solve 完成(`FOnSolveComplete` delegate)後,呼叫 `BuildHeatmap(FFrameSolveResult, FFrameModelDef)`
3. H 鍵 toggle 熱圖 Actor 的 `SetActorHiddenInGame`
4. 熱圖顏色配置:綠 #009E73 → 黃 #F0E442 → 橘 #E69F00 → 紅 #D55E00(符合計畫書決策 8)
5. HUD 圖例 Widget:左下角顯示 0.0~1.0+ 的色條 + D/C 數值標示

**涉及 API / 路徑:**
- `AFrameUtilizationHeatmapActor::BuildHeatmap`(已有,需驗證 ArchSim 場景中可呼叫)
- `UMG Widget Blueprint`(HUD 圖例)
- `AHUD::AddHudElement`(或 `UUserWidget::AddToViewport`)

**完成標準:**
- 50 構件規模下熱圖更新時間 <500ms(符合 L2 驗收條件 2)
- 熱圖色彩與 `FFrameSolveResult.MemberUtilization` 數值一致,可人工抽查 3 根梁
- H 鍵開關熱圖不產生 tick overhead(熱圖隱藏時 `SetActorTickEnabled(false)`)

**預期踩雷:**
- `BuildHeatmap` 每次重建 ProceduralMeshComponent(PMC)網格,50 構件×11 ring 約 5500 個 vertex 操作,預計 8-15ms。超過 500ms 上限時需啟用 `PerInstanceCustomData` GPU 色彩路徑替代每次重建。MVP 先驗量,Phase 2 再優化。
- 熱圖 Actor 與構件 Actor 的 Transform 必須一致:若構件 Actor 在 Server 端 Spawn 後 Replicate 到 Client,熱圖必須在 Client 本地重建,不能 Replicate 網格資料(太大)。

**估計工時:** 16 小時
**依賴:** L1-P1-3
**MVP:** 必須

---

#### L1-P1-8 沙盒模式單機整合測試

**任務描述:**
整合上述所有 P1 子系統,進行第一次完整流程驗證:放置 50 個構件 → 施加載重 → Solve → 熱圖顯示 → 移除構件 → 重新 Solve → SaveGame → 重載。

不是寫新功能,而是**找 bug 並修**:重點確認以下介面之間的資料流無誤。

**完成標準:**
- 完整流程無 crash,Output Log 無 Error 級訊息
- SaveGame 50 構件 round-trip:所有構件位置、材料、連接關係在重載後一致(L2 驗收條件 5)
- Solve 時間 <2 秒(L2 驗收條件 1)

**估計工時:** 16 小時
**依賴:** L1-P1-1 到 L1-P1-7
**MVP:** 必須

---

#### L1-P1-9 SUQS 關卡系統 + 3 個 Tutorial 關卡

**任務描述:**
用 SUQS DataTable 定義三個 Tutorial 關卡的結構:

- **L-01 懸臂梁:**目標「放置 1 根柱 + 1 根梁,加載,觀察 D/C 並讓 D/C < 1.0」
- **L-02 簡支梁:**目標「兩端支承梁,加中點載重,D/C < 1.0 且撓度 < 20mm(比例)」
- **L-03 門型框架:**目標「兩柱 + 橫梁 + 側向載重,系統不倒塌(bSingular=false)且 D/C < 1.0」

每個關卡的通關條件在 C++ 的 `OnSolveComplete` callback 中判斷(SUQS 不知道數值邏輯),通關後呼叫 `USuqsProgression::CompleteTask`。

**涉及 API / 路徑:**
- `USuqsProgression::CompleteTask` / `FailTask`(SUQS Plugin)
- `UDataTable`(Quest DataTable,使用 SUQS 的 `FSuqsQuest` row struct)
- `UArchSimQuestCoordinator`(新建,橋接 OnSolveComplete → SUQS 狀態機)
- `UCommonActivatableWidget`(關卡目標 HUD 面板,顯示 objective 清單)

**完成標準:**
- 三個 Tutorial 關卡可完整通關,通關時 HUD 顯示「關卡完成」動畫(L2 驗收條件 3)
- 關卡失敗(bSingular=true 或 D/C > 1.0)可重試,重試後熱圖正確更新(L2 驗收條件 4)
- 規範對標警告 W-01~W-05 以中文文字顯示(L2 驗收條件 6):W-01 D/C 超標 / W-02 機構不穩定 / W-03 無支承 / W-04 撓度超限 / W-05 材料規格不符

**預期踩雷:**
- SUQS 沒有 built-in replication;MVP 是單人模式,此處暫不處理。Phase 2 多人時需在 PlayerState 的 `Replicated UPROPERTY` 中快照 `USuqsProgression` 狀態。
- W-01~W-05 的中文字串存 `FText`(DataTable 或 StringTable),避免硬碼 `FString` 導致未來在地化困難。

**估計工時:** 32 小時
**依賴:** L1-P1-8
**MVP:** 必須

**Phase 1 子任務總工時:** 約 180 小時

---

### Phase 2 — 完整教育體驗(Day 91-270,估計 300 小時)

Phase 2 是功能完整版,把 MVP 的骨架加上所有計畫書指定的教育功能。由於任務量大,以下僅列出各子系統的重點任務描述,細部展開與 Phase 1 同等深度留給各對應 Part 的 subagent。

**Phase 2 任務清單(估計工時):**

| 子系統 | 主要 C++ 類別 | 估計工時 |
|---|---|:---:|
| C3 崩塌動畫 | `AFrameDynCollapseReplayActor`(已有)整合進 GameFlow | 24 小時 |
| C3 Chaos 碎片 | `AFrameFragmentClusterActor`(已有)+ `UGeometryCollectionComponent` | 24 小時 |
| C4 連通性 | `UFrameStructureGroupSubsystem`(新建,Union-Find BFS) | 16 小時 |
| B2 4 象限 | `SArchSim2DViewport` Slate widget + UE 正交投影 SceneCapture | 40 小時 |
| B3 圖學標註 | `UArchSimAnnotationSystem`(截面/尺寸線) | 16 小時 |
| B4 藍圖系統 | Prefabricator `APrefabricatorAsset` runtime spawn API | 16 小時 |
| D1 掃描儀 | `AArchSimScannerTool` 裝備道具 + 熱圖/錨定模式切換 | 24 小時 |
| D2-D4 診斷 | 變形動畫/BMD-SFD ribbon/安全係數 HUD | 24 小時 |
| E1-E4 施工 | `UFrameConstructionStateMachine` 狀態機 + RC helper | 40 小時 |
| F3-F5 學習閉環 | 失敗即學機制 + 規範對標完整 W-01~W-10 | 24 小時 |
| H1-H4 多人 | Listen Server + NULL Subsystem + 30 人並發 Solve 佇列 | 56 小時 |
| I1-I4 教師+log | xAPI + SQL LRS + 老師 dashboard + Spectator | 56 小時 |
| Part G 測量關卡 | PCG 地形 + 經緯儀/水準儀模擬(獨立支線) | 56 小時 |

**Phase 2 總工時(估計):** 416 小時(~6 個月 1.5 人)

---

### Phase 3 — 進階與發布(Day 271-365,估計 72 小時)

| 任務 | 估計工時 |
|---|:---:|
| MctoNurbs sidecar 整合(IFC/DXF 匯出) | 24 小時 |
| PCG 地形 5 模板完整版 + 多語在地化 | 24 小時 |
| 效能優化(World Partition / LOD / Nanite) + 老師備課文檔 | 16 小時 |
| SQL LRS 完整部署 + Release build 5-leg gate + 發布準備 | 8 小時 |

---

## L2 MVP 驗收條件 — 詳細量測方法

MVP 通過需滿足以下 10 項驗收標準。每一項都有可重現的量測步驟,不依賴主觀判斷。

### 驗收 1:單人沙盒 50 構件 Solve 時間 <2 秒

**量測方法:**
在 PIE 模式,用 `UE_LOG` 打點記錄 `ApplyPatchAndResolve` 或 `ResolveCurrent` 的開始與結束時間(`FPlatformTime::Seconds()` 精度為 μs)。50 根梁懸臂橋場景連跑 10 次取平均。

**判定標準:** 平均 <2000ms,單次最壞不超過 3000ms

**責任子系統:** `UArchSimSolveCoordinator`

**開發備注:**
FrameCore 的 standalone benchmark(F-series)在 90K DOF 下 LAZY+CPU 約 56ms,50 個梁 ~300 DOF 遠低此值,理論上 Solve 本身 <5ms。2 秒的瓶頸可能在 UE 端的 USTRUCT 封裝與 `DistributeSolveResult` 的迭代。需分別量測 FrameCore Solve 時間 vs 總端到端時間。

---

### 驗收 2:熱圖更新 <500ms(端到端)

**量測方法:**
從 Player 觸發 `IA_PlaceMember`(或 `IA_ToggleHeatmap`)到 `AFrameUtilizationHeatmapActor::BuildHeatmap` 完成並設定 PMC section,用 UE Profiler 或手動 `FPlatformTime` 量測。

**判定標準:** <500ms

**開發備注:**
`BuildHeatmap` 的 PMC 重建是主要開銷。50 構件×11-ring 的幾何約需 8-15ms。若加上 GC、tick 等 Frame overhead,正常情況應在 50-100ms 內。若逼近 500ms,需排查是 Solve 時間本身還是 PMC rebuild。

---

### 驗收 3 & 4:Tutorial 關卡通關與重試

**量測方法:**
用新建玩家帳號執行 L-01 → L-02 → L-03 完整流程。記錄:每個關卡從進入到通關的時間;失敗後重試路徑是否正確重置熱圖狀態。

**判定標準:**
- L-01 從零操作到通關 <10 分鐘(目標 5 分鐘)
- 重試後 `FFrameModelDef` 重設為初始狀態,熱圖顯示空場景值(全藍)

---

### 驗收 5:SaveGame 50 構件 round-trip

**量測方法:**
放置 50 個構件,存檔,刪除所有 Actor,載入,人工抽查 10 個構件的 Position + MaterialId + SectionId + MemberIdx 是否與存檔前一致。再執行 Solve 確認結果 bit-level 相容。

**驗證要點:**
- 特別確認刪除後重載的構件 `MemberIdx` 對應仍正確(OccupancyGrid 重建後索引一致性)

---

### 驗收 6:規範對標警告 W-01~W-05 中文顯示

**量測方法:**
故意製造五種違規條件並觸發 Solve:
- W-01:讓某梁 D/C 超過 1.0
- W-02:移除支承讓結構機構化(`bSingular=true`)
- W-03:新建孤立梁不連任何支承
- W-04:讓撓度超過跨徑的 1/240
- W-05:混凝土梁截面太小(使用教育警告域值)

確認每個條件觸發後,HUD 右側警告欄出現對應中文字串且無亂碼。

---

### 驗收 7:5-leg gate 全綠

**量測方法:**
在 ArchSim repo 根目錄執行:
```powershell
Scripts\run_gate.ps1 -RequireOpenSees
```
確認所有 5 腿(standalone F1..F71 / UE 135 tests / OpenSees / audit 104 / CLI roundtrip)全部 PASS。

**開發備注:**
添加 ArchSim 的新 UE 測試類別(`ArchSim.MemberData.SaveRoundTrip` 等)需要在 `run_gate.ps1` 中更新 `$ExpectedUeTests` 計數,否則計數不符 = GATE FAIL。

---

### 驗收 8:教學流暢度測試

**量測方法:**
邀請 3 位技高建築科學生(15-17 歲),在無任何引導文字幫助的情況下玩 30 分鐘。目標:至少 2/3 人可獨立完成 L-01 懸臂梁關卡。收集發現的 UX 痛點。

**評判標準:**
- 2 人以上在 30 分鐘內完成 L-01
- 平均「第一次看到熱圖時能猜到顏色意義」問卷回答 ≥ 3/5 分

---

### 驗收 9:效能基準

**量測方法:**
1280×720 解析度 PIE 模式,場景 50 構件+熱圖可見+角色移動。用 UE Profiler 的 Frame Time Graph 觀察 1 分鐘。

**判定標準:** P95 Frame Time < 33.3ms(30fps),不允許超過 100ms 的 spike

---

### 驗收 10:基礎個資合規

**量測方法:**
打開 SPUD SaveGame 存檔(JSON 格式或二進位),grep 玩家原始姓名。

**判定標準:** 存檔中不應出現玩家真實姓名字串。`USuqsProgression` 或 xAPI 使用的識別碼必須是 `SHA256(student_id + salt)` 格式的 hex 字串。

---

## L3 風險清單 — 詳細緩解計畫

### 技術風險

#### 風險 T-01:UE5.8 replication breaking changes
- **影響程度:** 高
- **觸發時機:** 若 MVP 完成前嘗試升級到 UE5.8
- **緩解策略:**
  - 在 `ArchSim.uproject` 中鎖定 `EngineAssociation: "5.7.0"`
  - MVP 完成後才評估升級;升級前必須先跑完整 5-leg gate
  - 監控 UE5.8 release notes 中的 replication breaking changes 清單(2026-06 論壇已有警告)

#### 風險 T-02:SPUD World Partition issue #117
- **影響程度:** 中
- **觸發時機:** Phase 2 加入 World Partition 時
- **緩解策略:**
  - MVP 明確不啟用 World Partition(在 World Settings 中確認 `bEnableWorldPartition=false`)
  - Phase 2 引入 World Partition 前,在分支上先測試 SPUD 的 save/load 完整性
  - 若 SPUD 不相容,Phase 2 的 World Partition 可用 Level Streaming 替代

#### 風險 T-03:ALS Iris Push Model dirty mark 漏
- **影響程度:** 高(多人模式)
- **觸發時機:** Phase 2 多人功能開發時
- **緩解策略:**
  - `AArchSimCharacter` 的每個 Replicated UPROPERTY 都使用 `MARK_PROPERTY_DIRTY_FROM_NAME` macro
  - 在 Phase 2 多人整合測試中,專門測試角色狀態同步的 edge case(多人同時進入建築模式)

#### 風險 T-04:Prefabricator runtime spawn 失敗
- **影響程度:** 高(影響 MVP 核心功能)
- **觸發時機:** L1-P1-5 實作時
- **緩解策略:**
  - 確認 Prefabricator 的 `.uplugin` Type 為 Runtime,不是 Editor
  - 若 Prefabricator 在 runtime spawn 有問題,降級方案:手動維護一個 `TMap<EMemberType, TSubclassOf<AActor>>` Prefab 類別表,用 `SpawnActor<T>` 直接 spawn,繞過 Prefabricator

#### 風險 T-05:FrameCore API 功能不足(如缺沿桿取樣)
- **影響程度:** 中
- **觸發時機:** D3(BMD/SFD ribbon)開發時
- **緩解策略:**
  - FrameCore engine FROZEN,不可新增引擎功能
  - 在 `Plugins/FrameSolver/Source/FrameCoreUE/` 中新增 helper class `UArchSimMemberPostProcessor`,用已有的 `FFrameMemberInternalForces`(已有 member forces at endpoints)做線性內插,模擬 11 個採樣點的 BMD/SFD
  - 明確在程式碼中標注 `[CONSUMER LAYER INTERPOLATION - not FrameCore native]`

#### 風險 T-06:FrameCore Solve 在 500+ 構件下卡頓
- **影響程度:** 中
- **觸發時機:** 學生挑戰性場景下
- **緩解策略:**
  - 單關卡限制 ≤500 構件(在 `UArchSimModelRegistry::RegisterMember` 中守門,超過時回傳 `EPlacementResult::TooManyMembers` 並顯示 HUD 警告)
  - Phase 2 優化:Server 端 Solve 佇列 + 每組 cooldown 3 秒 + 進度指示器

### 教育設計風險

#### 風險 E-01:學生看不懂熱圖顏色
- **影響程度:** 高
- **觸發時機:** 第一次試玩時
- **緩解策略:**
  - HUD 左下角永遠顯示色條圖例(0.0=安全=綠 / 1.0=臨界=橘 / >1.0=危險=紅)
  - 熱圖旁顯示數字:每根梁的 D/C 數值(在 Hover 時顯示 tooltip)
  - L2 驗收 8 的學生試玩將量測「熱圖語意理解率」,低於 60% 則強化圖例設計

#### 風險 E-02:L-01 對 15 歲學生太難
- **影響程度:** 中
- **觸發時機:** 驗收 8 試玩時
- **緩解策略:**
  - L-01 提供「引導箭頭」Overlay:第一次進入時 3D 空間中有浮動箭頭指示「先放這個」
  - 失敗 2 次後自動出現「提示」按鈕(SUQS Hint system)
  - 設計可調整的提示等級,讓老師控制學生的 scaffolding 程度

#### 風險 E-03:個資法合規問題
- **影響程度:** 高
- **觸發時機:** 學校部署時
- **緩解策略:**
  - MVP 階段:SaveGame 只存 `pseudonymized_id`(SHA256 hash),不存姓名
  - Phase 2 xAPI:Statement Actor 的 `account.name` 使用 `SHA256(student_id + salt)`,`salt` 由學校管理員設定
  - 提供「刪除個人資料」功能:管理員可按 hash 刪除該學生的所有 xAPI Statement

### 與 FrameCore 的相互依賴風險

#### 風險 F-01:FrameCore engine 發現 bug 需要修正
- **影響程度:** 中
- **觸發時機:** 學生試玩時發現數值異常
- **緩解策略:**
  - 任何 FrameCore engine bug fix 視為 v4.0.x patch
  - 修正前**必須先做 CLAUDE.md amendment 移除 FROZEN marker**,並附明確理由
  - 修正後需重跑 5-leg gate 確保無回歸
  - **不得在 UE consumer 層做 workaround 掩蓋 engine bug**

#### 風險 F-02:FrameCore Woodbury rank 溢出
- **影響程度:** 低(已知邊界)
- **緩解策略:**
  - `UArchSimSolveCoordinator` 追蹤累計 rank。超過 MaxRank=96 時自動觸發 `Rebaseline()`
  - 在 Output Log 中記錄每次 Rebaseline 事件(不 crash,只是慢一次)

---

## 工時總表

| Phase | 子任務 | 估計工時 |
|---|---|:---:|
| **Phase 0** | 環境準備 | 40 小時 |
| **Phase 1 MVP** | A1-A2 引擎接合 + ALS | 40 小時 |
| | B1 構件放置 | 40 小時 |
| | C1-C2 載重 + 熱圖 | 40 小時 |
| | F1-F2 沙盒 + 關卡 | 48 小時 |
| | MVP 整合測試 | 32 小時 |
| | **Phase 1 小計** | **200 小時** |
| **Phase 2** | 完整教育體驗(C3-I4 + Part G) | 416 小時 |
| **Phase 3** | 進階與發布 | 72 小時 |
| **總計** | | **~728 小時(約 12 個月 × 1.5 人)** |

> 附注:上表為工時數,不含 UE build 等待時間(每次 UE full build 約 30-90 分鐘)。
> 計畫書附錄 G 的 12 個月 1-2 人估算與此表吻合。


---


# 第 13 章 — Sprint 計畫(從 Cross-Review)

## S-00(W1-2,80 工時)

**任務**:
- L P0-1: Plugin 環境建置與 EngineVersion patch
- L P0-2: FrameCoreUE Plugin 整合驗證 (確認 v4.0.0 FROZEN interface)
- L P0-3: AArchSimCharacter 骨架 (ALS plugin 乾淨 build 驗證)
- L P0-4: UFrameInteractiveSubsystem 初始化驗證
- 技術驗證: FDemandSummary / FFrameSolveResult 欄位名稱確認 (D4 衝突預防)
- 技術驗證: ALS v4.17 + UE5.7 相容性第一次 build 確認

**里程碑**:Gate 0: UE build green + ALS plugin 乾淨 build + FrameCore 接口確認

## S-01(W3-4,115 工時)

**任務**:
- A1-01: UArchSimMemberData ActorComponent 骨架
- A1-02: UArchSimModelRegistry GameInstanceSubsystem 骨架
- A1-03: RegisterMember + FFrameModelDef 組裝
- A1-04: ApplyPatchAndResolve debounce 包裝
- A1-05: DistributeSolveResult + CachedUtilization 回寫
- K4-T1: Noto Sans CJK TC 字體匯入
- K4-T2: UIStyle DataAsset 定稿
- K1-T2: 構件材質三件套 UMaterialInstanceDynamic (提前，B1 前置)

**里程碑**:A1 Registry 可 Register + Solve + Distribute 完整鏈路

## S-02(W5-6,110 工時)

**任務**:
- A2-01~03: AArchSimCharacter ALS 整合、Enhanced Input 框架、基礎移動驗證
- A3-01~04: 地形骨架、支承 IsFixed 接合、地基節點 FFrameNode 寫入
- K2-T1: UArchSimColorMap BlueprintFunctionLibrary (D/C 綠黃橘紅 Okabe-Ito)
- K1-T1: PostProcessVolume 全域後處理材質設定基準線

**里程碑**:Gate 1: A 層完整骨架可運作 (角色可移動 + 地形 + Registry 三合一)

## S-03(W7-8,120 工時)

**任務**:
- B1-T1: UArchSimPrefabSpawnSubsystem 骨架 + OccupancyGrid TMap (改用 TWeakObjectPtr)
- B1-T2: Server_RequestPlacement RPC + 衝突仲裁 (立即標記防雙通)
- B1-T3: Client Ghost 預覽 (UMaterialInstanceDynamic 半透明 + 格線 Snap)

**里程碑**:Gate 2: 構件可放置 (單人, Server 仲裁正確)

## S-04(W9-10,118 工時)

**任務**:
- B1-T4: Prefab 庫 15 個 DataAsset (5 大類 × 3 變體)
- B1-T5: R 鍵旋轉 + ESC 取消 + 放置後觸發 ApplyPatchAndResolve
- C4-T1: 連通性子系統骨架 (Union-Find, Server-only)
- C4-T2: 錨定邏輯 (prescribed DOF 確認)
- F5-T1: UArchSimWarningSubsystem 骨架
- F5-T2: 10 種警告模板 DataAsset

**里程碑**:Gate 3: 構件放置 + 連通性判斷 + 警告系統可觸發

## S-05(W11-12,112 工時)

**任務**:
- C1-T1: 載重 USTRUCT 橋接 helper
- C1-T2: 節點集中力 UI 與施加
- C1-T3: UDL 均佈線載重施加
- C1-T4: 自重自動計算
- C1-T5: 載重視覺化箭頭 Actor
- C2-T1~2: D/C 熱圖 Actor 整合 + BuildHeatmap (PMC mesh 重建, MVP 接受 10ms/100構件)
- K2-T2: 接 AFrameUtilizationHeatmapActor 色彩管道

**里程碑**:Gate 4: 載重 → Solve → D/C 熱圖視覺化 完整管道

## S-06(W13-14,120 工時)

**任務**:
- D1-T1: AArchSimScannerTool Actor 骨架 + Q 鍵綁定
- D1-T2: 左鍵 Raycast + UMemberInfoPanel UMG 浮空顯示 (7 欄位)
- D1-T3: FrameCore 結果讀取 helper (透過 Registry 雙向查詢)
- D1-T4: 熱圖模式整合 (呼叫 HeatmapActor::SetActive)
- D2-T1: HUD 分析面板 UMG 骨架 (5 個 toggle)
- J1-T01: 閉環 HUD 進度指示器
- J1-T02: 閉環狀態機 C++ Subsystem 骨架
- J2-T01: 視覺回饋延遲門控器
- J2-T02: 失敗語言包 (排除 Game Over 文字)

**里程碑**:Gate 5: 完整 Solve → 視覺化 → 掃描儀 → 閉環 HUD 完整鏈路

## S-07(W15-16,**+5h 加 LevelSim 整合** → 123 工時)

**任務**:
- F1-T01: 沙盒基地模板 DataAsset 系統
- F1-T02: 沙盒遊戲模式與 SaveGame 自動儲存
- F1-T03: 沙盒 HUD (建造/分析/分享/離開)
- F2-T01: SUQS DataTable DT_ArchSimLevels 建立 (同時鎖定 schema, G5 前置)
- F2-T02: 關卡選擇地圖 UI (LevelSelectWidget)
- J1-T03: 失敗觸發自動進入診斷模式 30 秒
- **【新加】G3 LevelSim 整合**(5h):透過 SUQS DataTable 把 LevelSim 關卡接進關卡選單;`ALevelSimGameMode` 對應 quest `Q_S01_Leveling`;測試 S-01 高差計算關卡可由選單啟動 → 完整跑 LevelSim FSM → 完成回主選單

**里程碑**:Gate 6: MVP End-to-end 可玩原型 (沙盒 + 關卡選單 + 失敗閉環 + **水準儀關卡 S-01 可玩**)

## S-08(W17-18,120 工時)

**任務**:
- L P1 整合測試: 所有 MVP 路徑端到端驗收
- Bug fix sprint + 性能壓測 (ApplyPatchAndResolve debounce 驗證)
- 教師評估版打包 + 基本內容 (L-01 ~ L-03 關卡初版 + **S-01 水準儀關卡**)
- 文件補齊: MVP 驗收文件
- **【新加】S-02 + S-03 + S-04 水準儀關卡**(LevelSim 已支援 closeLoop / pointElevation / multi-station):各約 1h SUQS DataTable 配置,共 ~3h

**里程碑**:🏆 MVP Release: 教師評估版可遊玩 (S-00 ~ S-08, 約 18 週)

## S-09(W19-20,115 工時)

**任務**:
- E4-T1: UFrameRCMaterialHelper BlueprintFunctionLibrary + RC 融合公式
- E4-T2: EMaterialState enum + FFrameMaterial 四組參數對應表
- E1-T1: UConstructionStateMachine ActorComponent 骨架
- E1-T2: 狀態轉換 RPC 鏈 (Client→Server→OnRep)
- H1-T1: DefaultEngine.ini NULL Subsystem 設定 + SessionInterface 封裝
- H1-T2: Listen Server 建立 + LAN Session 搜尋
- H1-T3: 大廳 UMG 介面 (ULobbyWidget)

**里程碑**:Phase 2 開始: 施工工序骨架 + 多人基礎架構

## S-10(W21-22,118 工時)

**任務**:
- H2-T1: Server 端 OccupancyGrid + UArchSimPlacementSubsystem (多人)
- H2-T2: Client Ghost 預覽多人同步
- H2-T3: FrameCore Solve 結果同步 (GameState Replicated)
- E2-T1: 鋼筋間距 4 級折減查表 UArchSimRebarChecker
- I2-T1: xAPI Statement 資料結構定義 + 8 個自訂 verb
- I2-T2: UE 端 log 觸發點埋設 + UArchSimLogSubsystem 骨架
- I2-T3: 離線隊列 + LRS HTTP 送出

**里程碑**:Gate 7: 2 人同時建造可行 + xAPI log 基礎

## S-11(W23-24,115 工時)

**任務**:
- C3-T1~3: SolveDynCollapse AsyncTask 非同步封裝 (cancel token) + 崩塌動畫整合
- E1-T3: 養護計時器 FTimerHandle + 加速 toggle (ElapsedServerTime 累積方案)
- E1-T4: 蜂窩弱點 15% 亂數 (FRandomStream Server 生成後 Replicated)
- E1-T5: 養護進度 Widget
- I1-T1: ATeacherPlayerController 骨架 + Spectator 狀態切換
- I1-T2: 班級總覽 HUD UI

**里程碑**:Gate 8: 崩塌動畫 + 施工工序 Phase 2 完整 + 教師基礎視角

## S-12(W25-26,112 工時 → **修正為 ~80 工時**;G3 已在 S-07 完成,本 sprint 改做經緯儀 + 隨機地形整合)

> **訂正**:水準儀已在 S-07/S-08 透過 LevelSim 整合完成(主檔 Part G3 修訂版),本 sprint 原本的 G3-T1~T3 取消,改做經緯儀(G2)+ PCG 隨機地形整合 LevelSim RoutePoints。

**任務**:
- G1-T1: **PCG_SurveyTerrain 圖建立 + Seed 確定性驗證**(已驗 PCG API 在 S-00)
- G1-T2: 地物標記 Actor 架構 + 可見性保證
- **【新】G1-T1.5**:PCG 採樣 → `ALevelSimPawn::InitRoute(RoutePoints)` 整合,讓水準儀關卡也可用隨機地形(原本 LevelSim 用固定 RoutePoints)
- **【新】G2-T1**:`AArchSimTheodolite` Actor 骨架(從零做,參考 LevelSim 結構)
- **【新】G2-T2**:水平角 / 垂直角讀數 + 度盤旋轉邏輯
- B2-T1~3: SceneCapture2D 正交視圖 (強制 LOD 0, bCaptureEveryFrame=false)

**里程碑**:Gate 9: 測量支線完整可玩(水準儀 ✅ from S-07 + 隨機地形 + 經緯儀基礎)+ 2D 工作區基礎

## S-13(W27-28,115 工時)

**任務**:
- F3-T1~4: 學習閉環加載階段整合 (依賴 E1 就緒)
- H3 角色分工 UI + AssignRole 邏輯
- J3-T01~03: 關卡 DataTable schema + L-01~L-03 完整製作
- I1-T3~4: 自由相機 + 標記系統 AArchSimMarkActor

**里程碑**:Gate 10: Phase 2 完整多人教學流程

## S-14(W29-30,115 工時)

**任務**:
- Phase 3 開始: B3 圖學標註 (DXF 實體、CNS B1001 隱線判定)
- K1-T6: Nanite/Lumen 輕量化設定 (距離 LOD + 低 quality tier)
- J4 評量機制 (評量依賴 J3 關卡結構)
- G2 經緯儀 Actor 複雜操作流程
- F2-T07 主線關卡 L-04~L-10 擴充

**里程碑**:品質 + 內容擴充 Sprint

## S-15(W31-32,115 工時)

**任務**:
- J5: 課綱對應嵌入 J3 DataTable
- J6: 教師備課依賴 I 完整實作後整合
- L1-P3: MctoNurbs sidecar 整合 (IFC/DXF 匯出)
- K1-T7: 品質選項選單 (效能/平衡/品質三擋)
- 多語在地化 (中英)
- 最終整合測試 + 正式版發布準備

**里程碑**:🎓 Phase 3 完成 / 正式發布 (S-00 ~ S-15, 約 32 週 / 8 個月)


# 第 14 章 — Cross-Review 完整報告

## 14.1 跨 Part 依賴圖

```
跨 Part 依賴圖 (必須先於關係,→ 表示前置條件):

A (引擎接合/角色/世界, 82h)
  → B (玩家操作層, 192h) — UArchSimModelRegistry + AArchSimCharacter 是 B1 前置
  → C (工程模擬層, 112h) — ApplyPatchAndResolve + DistributeSolveResult 是 C1/C2 前置
  → K (風格美術, 112h) — DistributeSolveResult 供 K2 D/C 色彩
  → H (多人協作, 138h) — AArchSimCharacter 是所有 Pawn 基類 [Phase 2 依賴]

B (192h)
  → D (診斷工具, 112h) — 構件 Actor 須先存在才能 raycast 掃描
  → E (施工工序, 96h) — Actor Spawn / ModelRegistry 機制共用 [Phase 2]
  → F (沙盒關卡, 178h) — 放置流程必須先穩定

C (112h)
  → D (112h) — FFrameSolveResult 供 D1 讀取
  → E (96h) — C3 崩塌是 E1 Productive Failure 閉環入口 [Phase 2]
  → F (178h) — C1/C2 必須先完成才能 F3 學習閉環
  → K (112h) — C2 HeatmapActor 是 K2 色彩管道前置

H (138h) → I (教師工具, 128h) — Listen Server 是 I1 Spectator 前置 [Phase 2]
H (138h) → F (178h) — 多人沙盒整合 [Phase 2]

D (112h) → G (測量關卡, 125h ← LevelSim 已實作) — SUQS 任務骨架 [Phase 2]
D (112h) → J (教育設計, 312h) — D1 掃描儀/D2 變形圖是 J1/J2 工具
D (112h) → F (178h) — D4 SF<1.0 事件觸發 F4 失敗模式 [trigger]

E (96h) → J (312h) — E1 施工工序狀態機是 J1 閉環段 [Phase 2]
F (178h) → J (312h) — SUQS 關卡系統是 J3 前置
F (178h) → G (125h ← LevelSim 已實作,原估 178h) — G5 任務系統依賴 F2 SUQS DataTable
I (128h) → J (312h) — I1 Spectator / I3 dashboard 是 J6 教師備課前置

All → L (Roadmap, 312h) [meta 整合層,描述性]

環狀依賴偵測:
- B ↔ E 潛在互依: B 聲稱 Part A 完成後即可開始，E 的 Phase 2 依賴 B1 Actor Spawn 機制，且 C3(依賴 C/B) 崩塌又是 E1 閉環的入口。不構成真正環狀，但 B → C → E 鏈條拉長了 E 的可啟動時間，建議 E MVP 任務(E4 RC 公式)可提前到 S-09 獨立先行。
- F ↔ G 潛在互依: F2 SUQS DataTable 是 G5 前置，G 測量支線卻又要回饋進 J3 關卡清單。不成環，但需注意 F2 SUQS schema 必須在 G5 開發前鎖定。
- 無真正環狀依賴確認。
```

## 14.2 MVP 工時計算 vs L1 對標

- MVP 總工時計算:517 小時
- 對標主檔 L1 預估:MVP 路徑工時約 517 小時 (各 Part MVP 子任務加總估算)。主計畫書 L1 預估 1,900 小時為全量 12 個月工時。MVP 佔全量約 27%，對應 S-00 ~ S-08 共 9 個 Sprint (18 週 / 約 4.5 個月)。12 Part 工時加總為 1,952 小時，超出主計畫書 52 小時 (+2.7%)，在合理誤差範圍內。但 J 和 L 各 312 小時加總佔 624 小時 (32%)，其中 L 是 Roadmap/整合層偏高估，J 教育設計橫跨全程且工時偏大需特別監控。若以 1.5 人 120h/Sprint 計算，全量 1,952h 需約 16.3 個 Sprint = 32-33 週 = 約 8 個月，比主計畫書 12 個月偏樂觀，需保留至少 30% buffer 後達到約 10-11 個月，與主計畫書大致吻合。

## 14.3 發現的衝突

### 衝突 1

衝突 1 — B 聲稱自己 MVP 但工時 192h 嚴重超標: Part B 總工時 192h 是所有 Part 中最大值(不含 J/L 的 meta 層)，但 B 的 MVP 任務僅 B1 構件放置(約 48h)。B2(2D↔3D 工作區)、B3(圖學標註)、B4(藍圖系統)三個子節均列為 Phase 2，卻被納入 B 的 192h 總量。建議: 將 B2/B3/B4 明確切割為 Phase 2，MVP 階段 B 僅計 48h。

### 衝突 2

衝突 2 — J 教育設計 312h 全程依賴但 MVP 任務定義模糊: J 聲稱 J1-T01/02/03 + J2-T01/02 為 MVP 必須，工時佔 J 總量 312h 約 15%(47h)。然而 J1 閉環狀態機依賴 Part A(Solve 結果) + Part C(D/C 熱圖) + Part D(掃描儀)，實際上 J MVP 不可能在 S-01 啟動，至少要在 S-06 之後。計畫書沒有明確說明 J 的 MVP 任務何時可以實際動工，導致排程模糊。

### 衝突 3

衝突 3 — C4 連通性與 B1 放置的依賴順序矛盾: C 計畫書建議順序為「C4 → C1 → C2 → C3」，但 C4 的前提是「B1 構件放置系統已完成」，而 B1 自己的前提又是 A 全部完成。C4 在 B 之後、C1 之前，但 B 計畫書的建議順序是 B1 → B4 → B2 → B3，代表 C4 至少要在 B1 完成後才能啟動，不能與 B1 並行，這點在 MVP 時程計算中需明確體現。

### 衝突 4

衝突 4 — E 施工工序聲稱 Phase 2 但部分任務被 C3/F 閉環在 MVP 中暗中依賴: F 計畫書 MVP 的 F3 學習閉環加載段包含「施工階段有內容」，這依賴 E 的施工工序狀態機。但 E 在主計畫書 L1 明確說「E 是 Phase 2 依賴，不阻 MVP」。若 F3 也被歸為 Phase 2，則不衝突；但 F 把 F3 列入「Phase 2」而非 MVP，這點需在 F 的任務說明中更清楚標示，避免誤解。

### 衝突 5

衝突 5 — D4 的 FFrameSolveResult.Utilization.SafetyFactor 欄位存在性未確認: D 計畫書明確指出此為已知風險，需驗證 FDemandSummary 中是否有 SafetyFactor 欄位。若無，需在 UE consumer 層計算 SF = 1.0 / DemandSummary.MaxDC，但 MaxDC 欄位名稱也需確認。這是一個 MVP 驗收條件(F4 失敗觸發依賴 D4)中的未知 API，應列為 S-01 期間優先查明的技術驗證項目。

### 衝突 6

衝突 6 — H 多人協作和 G 測量關卡工時均為 138h/178h 但被排為 Phase 2，導致 Phase 2 工時炸裂: Phase 2 需包含 E(96h) + H(138h) + I(128h) + 部分 B/C/D/F/G/J/K 的 Phase 2 任務，粗估 Phase 2 工時超過 700h，遠超一個 Sprint 週期可吸收的量，需要更細緻的 Phase 2 內部優先序。

### 衝突 7

衝突 7 — K 美術層在 MVP 中的定位不清: K1-T2 構件材質必須在 B1 Prefab Spawn 流程建立前定稿(K 自己說的)，但 K 的建議順序是 K4-T1 → K4-T2 → K2-T1 → K1-T1 → K1-T2，代表 K1-T2 需要在 B1 之前完成。然而 K 的工時估算(112h 總量，MVP 約 34h)並未考慮到這個強制前置關係，MVP 排程中 K1-T2 必須在 S-03 B1 開始前就緒，需提前到 S-01/S-02。

### 衝突 8

衝突 8 — L 作為 Roadmap/Meta 層卻佔 312h 與 J 並列最高: L 的工時應主要反映在其他 Part 的工時中(環境建置、整合測試等)，單獨列 312h 容易造成重複計算。建議確認 L 的工時是否已扣除 A/B/C 等 Part 已包含的工時，否則總量 1,952h 可能被低估 80-100h。

## 14.4 高風險項目

### Risk #1:Part B 工時 192h — 整體最大單一 Part，B1 阻擋所有下游

- **Part**:B
- **原因**:B1 構件放置是 C/D/E/F/K 的共同前置條件。OccupancyGrid TMap<FIntVector, FActorHandle> GC 後失效的踩雷若未在 MVP 前解決，所有下游 Part 都會卡死。192h 總量遠超估計，B2/B3/B4 三個子節若被誤認為 MVP 會炸掉 S-03~S-04 Sprint。
- **緩解**:明確切割 B1 為 MVP(48h)，其餘歸 Phase 2。OccupancyGrid 改用 TWeakObjectPtr<AActor> 在 S-03 開始前驗證。B1-T2 Server RPC 必須在 TMap 查詢後立即標記(非 Spawn 後)，防止同幀雙通問題。

### Risk #2:Part J 教育設計 312h — 最大工時且橫跨全程，MVP 任務啟動點不明確

- **Part**:J
- **原因**:J 聲稱 J1/J2 為 MVP 必須，但 J1 閉環狀態機依賴 A(Solve) + C(D/C) + D(掃描儀) 的全部 MVP 任務完成。實際上 J MVP 最早可在 S-06 後啟動，而非計畫書暗示的早期。312h 若無更細緻的 Phase 拆分，極易成為進度黑洞。J2 的「Flow 心流難度自動調整」是 Phase 3 且需要大量教育學設計驗證。
- **緩解**:J1-T01/02 在 S-06 隨 D1 完成後立刻啟動；J1-T03 在 S-07 閉環完整後整合。J 的 312h 應拆成: MVP=47h / Phase 2=120h / Phase 3=145h。J6 教師備課必須排在 I 的完整實作後(S-13 以後)，不得提前。

### Risk #3:Part C3 — SolveDynCollapse 阻塞主執行緒 AsyncTask PIE crash

- **Part**:C
- **原因**:SolveDynCollapse 是 blocking 呼叫，必須用 AsyncTask/TFuture 封裝，但 UE5 FAsyncTask 在 PIE 關閉時有 crash 風險。這個問題影響 C3 + E1(Productive Failure) + J1(閉環失敗段) 的整合。且崩塌是教育設計的核心體驗，若技術不穩將影響 J 的教育目標達成率。
- **緩解**:S-11 開始前先建立最小 AsyncTask cancel token POC，驗證 PIE 關閉時的 cancel 路徑。若 FAsyncTask 不穩，改用 UE5 的 Tasks::Launch(TEXT(''), [...], LowPriority) API (UE5.3+) 或 UE Thread Pool TGraphTask。預備 blocking 模式作為 fallback(加 3 秒 Solve 冷卻限速保護主執行緒)。

### Risk #4:Part H — Listen Server Host 在低階教學電腦同時跑 Server + Client + FrameCore Solve

- **Part**:H
- **原因**:校園教學電腦通常是低配機(8-16GB RAM, 集顯)。Listen Server Host 同時承擔 Server 邏輯 + Client 渲染 + FrameCore Solve，若 30 個學生都觸發 Solve，Host 機會直接過載。NULL Subsystem broadcast 在 VLAN 隔離環境可能失敗，直連 IP fallback 若教師不懂 IP 設定會造成課堂無法開始的災難性場景。
- **緩解**:H2 層加 Solve 佇列 cooldown 3 秒限速。Listen Server 選定班上最高配置機器或老師機當 Host。在 S-09 開始前先在目標教學電腦環境做 stress test (5 client 同時 Solve)。提供一鍵直連 IP fallback 介面並在教師手冊說明操作步驟。

### Risk #5:D4 欄位不存在 + G1 PCG API 版本問題 — 技術假設未驗證

- **Part**:D / G
- **原因(歷史紀錄,2026-06-25 S-00 已解決)**:D4 假設 FFrameSolveResult.Utilization.SafetyFactor 存在 — **✅ S-00 確認 SafetyFactor 與 MaxDC 都真實存在於 FFrameDemandSummary**。G1 的 PCG_SurveyTerrain Attribute By Slope 節點名稱 — **✅ S-00 確認 UE5.7 PCG 無此節點,MVP 走 Landscape Spline**。本 Risk #5 已解決,保留作為 cross-review 歷史。
- **緩解**:S-00(環境建置 Sprint)期間立即查閱 FDemandSummary USTRUCT 欄位清單(FrameCoreUE 已 FROZEN，閱讀原始碼 10 分鐘即可確認)。G1-T1 在 S-12 開始前先用一個 1 天 spike 驗證 PCG_SurveyTerrain 在 UE5.7 的 node 名稱。兩項都是低成本早期驗證，不應延到實作階段才發現。

## 14.5 整體評估

整體計畫在架構依賴邏輯上是正確的，Part A 作為基礎層的設計合理，FrameCore FROZEN 邊界守護清晰。主要問題在於三個面向: 第一，工時分佈嚴重不均 — B(192h) 和 J/L(各 312h) 佔去 816h (42%)，但 B 的 192h 大部分是 Phase 2 任務，J/L 的 312h 各含大量整合工時，建議重新拆分後再確認各 Phase 實際工時; 第二，MVP 定義存在「隱性膨脹」風險 — 各 Part 都聲稱自己有 MVP 必須任務，加總後 MVP 約 517h 需要約 9 個 Sprint (18 週)，對 1.5 人團隊屬於重 MVP，建議確認最小可教學版本(L-01 + 單人沙盒 + D1 掃描儀)是否可在 S-06 結束後就交付教師試用，而非等到 S-08 的完整 MVP; 第三，幾個技術假設應該在第一個 Sprint(S-00)就驗證清楚 — ALS v4.17 + UE5.7 相容性、FDemandSummary 欄位名稱、PCG API 節點名稱，這三個都是低成本確認但若到晚期才發現問題代價極高。若以 1.5 人 × 32 週(16 Sprint)來執行全量 1,952h，並維持 10-15% buffer，整體計畫在 8-9 個月內是可達成的，比主計畫書的 12 個月保守估計更樂觀，建議保留 12 個月作為交付目標，用額外的 4 個月做 Phase 3 內容擴充和品質提升，而非壓縮 buffer。

## 14.6 建議

- 立即行動 — S-00 Sprint 必須包含三項技術 Spike: (1) ALS v4.17 + UE5.7 乾淨 build 驗證, (2) FDemandSummary USTRUCT 欄位清單查閱 (D4 衝突預防), (3) PCG_SurveyTerrain 節點名稱在 UE5.7 確認。這三項各需半天，總計 1.5 天，但可避免後期 Sprint 的重大返工。
- B 工時重新拆分 — B 的 192h 總量需明確標示: B1 MVP = 48h (S-03~04), B2+B3+B4 = 144h 全歸 Phase 2 (S-12 以後)。避免計畫書讀者將 B 的全量工時計入 MVP 估算。
- J 教育設計需要獨立 Phase 標籤 — J 的 312h 應拆成 MVP=47h / Phase 2=120h / Phase 3=145h 三段，並明確對應到 Sprint 號碼，防止 J 成為進度黑洞。J1-T01/02 最早在 S-06 才能實際啟動。
- C3 崩塌技術驗證提前到 S-05 — 在正式 C3 實作 (S-11) 之前，S-05 或 S-06 應插入一個 AsyncTask cancel token POC，確認 PIE 關閉時不 crash。這是半天工作，可防止 S-11 的崩塌整合變成兩週的 Debug Sprint。
- MVP 定義建議加入更早的教師試用節點 — 現有計畫 S-08 才是完整 MVP。建議在 S-06 結束時 (Gate 5 後) 就提供「最小可教學版本」給教師試用: 單人沙盒 + D1 掃描儀 + D/C 熱圖 + 簡單失敗提示。這可以在 S-07~08 根據教師回饋調整方向，避免 S-08 才發現教育設計假設有誤。
- H Listen Server 在目標機器的早期壓測 — 在 S-09 (H1 開始) 之前，應在實際教學電腦環境做 5 client 同時 Solve 的壓測。若低配機無法承受，需在架構層做 Solve 佇列或 Dedicated Server 降級預案，這個決策不能等到 S-10 H2 才發現。

---

*── 文件結束 ──*