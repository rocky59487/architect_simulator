# 建築師模擬器 v1.0 主計畫書

> **版本**：v1.0(主計畫書骨幹版)
> **日期**:2026-06-24
> **目標平台**:Unreal Engine 5.7 + C++17 + Blueprint
> **語言**:繁體中文(技術術語保留英文)
> **適用對象**:技術型高中(技高)建築/土木/室內設計群科一二年級
> **撰寫對象**:後續 AI agent 照本實作 + 指導老師/評審 review

---

## 文件閱讀指南

本計畫書是「建築師模擬器」教育遊戲的設計主檔。它的職責是把分散在三個來源 — **FrameCore v4.0.0 引擎工程文檔**、**Block Reality v3.0 製作手冊(已棄用平台,理念保留)**、**教育需求書 v3 + 10 天設計筆記** — 統一成單一可實作的計畫書。

- 主軸:**設計→施工→加載→崩塌→診斷→改** 的學習閉環
- 不寫程式碼骨架(那是後續 agent 的工作);**只寫設計層:玩法、UX、介面、教學、接合點**
- 每節格式統一:**概念 → 玩家視角 → 介面與操作規格 → FrameCore 接合 → 教學落點 → 完成標準 → 預期踩雷**
- 引擎相關詳情參考 [FrameCore CLAUDE.md](../CLAUDE.md) 跟 [docs/ARCHITECTURE.md](ARCHITECTURE.md)
- 附錄 D 是 FrameCore BP API 速查,附錄 E 是完整關卡清單

---

# 序章 — 為什麼做這個

## 0.1 專案定位

**這份計畫書描述的不是 Minecraft 模組,也不是 Rhino/AutoCAD/Revit 的廉價替代品。**

「建築師模擬器」是一個面向**技高建築/土木/室內設計群科一二年級**(15-17 歲)的**多人協作建築學習環境**,具備以下五個核心定位:

| 維度 | 定位 |
|---|---|
| **受眾** | 高中職一二年級,術語深度壓低、重直覺、重體驗 |
| **學習方式** | **直覺優先**:憑直覺蓋,透過熱圖/變形/崩塌的視覺反饋學習,**不是先算後蓋** |
| **內容形式** | **沙盒 + 關卡並行**:自由建造的沙盒與一系列引導關卡同時存在 |
| **協作模式** | **多人核心**:設計時即支援同班同學共同操作,不是後期附加 |
| **教育者角色** | **老師放大、不是取代**:老師可進入學生世界即時介入,完整學習 log 提供診斷依據 |

它不取代專業軟體,也不否定老師的教學角色,而是**補上學生從「分散科目」走向「完整建築理解」之間最缺少的那一層**。

## 0.2 學習主軸閉環

整個系統的骨架是同一條教育閉環:

```
       ┌─────────────────────────────────────────────┐
       │                                             │
       ▼                                             │
   [設計] ───→ [施工] ───→ [加載] ───→ [崩塌] ──→ [診斷] ──→ [改]
   學生在 2D    遵循正確    施加自重     失敗即學    應力熱圖   回到設計
   或 3D 介    工序蓋出     /活載重/    Productive  + 載重路   階段修正
   面放置構    結構         地震                Failure   徑可視化
   件,選材    (RC 工序     系統自動                       (Diagnosis
   料                       生成載重組合                      工具)
                            並 Solve
```

每個模組(Part)都是這條閉環上的一段,單獨抽出來看也應該講得通。

## 0.3 與 Block Reality 的關係(理念繼承表)

**Block Reality 是本專案的精神前身**(2025-2026 年,Minecraft Forge 1.20.1 路線,使用自研 PFSF 物理引擎)。在執行過程中發現 PFSF 在工程嚴謹度上有不可繞過的數學限制(梯度的旋度恆為 0,算不出純剪力與扭轉),於是整個工程引擎重做為 FrameCore(基於有限元素法 FEM,Karamba3D 對標)。Block Reality 的**設計理念與教育閉環設計**完整保留,但**平台、語言、引擎、模組系統全部更換**:

| 層級 | Block Reality(已棄用) | Architect Simulator(本專案) |
|---|---|---|
| 遊戲引擎 | Minecraft Forge 1.20.1 | **Unreal Engine 5.7** |
| 開發語言 | Java 17 + TypeScript Sidecar | **C++17 + Blueprint + TypeScript Sidecar 保留** |
| 物理引擎 | PFSF(自研 GPU 細胞機式擴散場) | **FrameCore v4.0.0(FEM, Karamba-parity)** |
| 結構單位 | 體素方塊(RBlock + R氏材料) | **梁柱(EB+Timoshenko)+ MITC4 板殼 + 構件 Actor** |
| 模組組成 | 三層架構(API / Fast Design / Construction Intern) | **12 Part 設計層 + FrameCore 結構引擎層** |
| 持久化 | NBT + GZIP 自訂格式 | **SPUD + JSON 模型** |
| 多人 | Minecraft 內建多人 | **UE5 Listen Server + NULL Subsystem LAN** |
| CAD 匯出 | TypeScript Sidecar(DC+PCA+NURBS) | **MctoNurbs Sidecar 保留(體素 → NURBS → DXF/IFC)** |

**繼承自 Block Reality 的核心設計理念**(完整 17 項對應見附錄 H):

- 工序狀態機(配筋→澆置→養護→拆模,蜂窩弱點與養護計時)
- 應力熱圖渲染 + 應力掃描儀(熱圖模式 / 錨定模式)
- 載重路徑追蹤 + 連通性 BFS 判定錨定
- RC 節點融合公式(鋼筋抗拉 + 混凝土抗壓 → 複合節點)
- 全息投影預覽(藍圖半透明)
- 學習閉環:設計 → 施工 → 加載 → 崩塌 → 診斷 → 改

## 0.4 與既有工程軟體的關係

本系統**不取代**業界 BIM/CAD 工具,也不要求老師重新學習一套全新軟體。我們把「初學者過早面對專業軟體 90% 不必要功能」這個問題抽出來解決:

| 工具 | 用途 | 跟本系統的關係 |
|---|---|---|
| **AutoCAD / Rhino / Revit** | 業界專業繪圖/BIM | 本系統不取代;高三/大學以後仍需學 |
| **Karamba3D / SAP2000 / ETABS** | 結構工程設計 | 本系統借用 D/C 熱圖視覺語言,但教學深度不到工程師等級 |
| **Poly Bridge / Kerbal Space Program** | 教育類沙盒物理遊戲 | 直接啟發來源:**失敗即景觀、工程概念內嵌於遊戲機制**(見 Part J 學理基礎) |
| **PhET Interactive Simulations** | 物理/化學教育模擬 | 設計哲學參考:**讓看不見的可見、隱性鷹架、最小文字** |
| **OpenSees** | 結構分析 oracle | FrameCore 引擎驗證的對標基準(已有 OpenSees 整合) |
| **IFC / glTF** | 業界互通格式 | MctoNurbs sidecar 已支援匯出,未來連接 BIM 流程 |

## 0.5 與既有引擎的關係(FrameCore + LevelSim)

本專案有**兩個既有 C++17 引擎**作為基底,**遊戲開發完全不修改任一引擎核心**:

### 0.5.1 FrameCore v4.0.0 — 結構大腦

FrameCore 是本專案的**結構力學大腦**,v4.0.0 起 engine source 永久 FROZEN(詳見 [CLAUDE.md 鐵則 #1](../CLAUDE.md))。所有遊戲開發都在 **FrameCoreUE(consumer 側)**進行,不修改 `Plugins/FrameSolver/Source/FrameCore/` 引擎核心。

**FrameCore 已提供且本計畫直接使用的能力**(完整 API 速查見附錄 D):

- **15 個分析 BP entry**:SolveLinear / Modal / Buckling / LoadCombineEnvelope / InfluenceLine / ResponseSpectrum / RealTimeDynamic / ReanalysisSolve / PDelta / TensionOnly / SizeOpt / BESO / Corotational / ArcLength / DynCollapse
- **8 個 BP 視覺 Actor**:DeformedShape / UtilizationHeatmap / ModalShape / DynCollapseReplay / FragmentCluster / InfluenceLine / ResponseSpectrum / RealTimeDynamic
- **UFrameInteractiveSubsystem**(StartSession / ApplyPatchAndResolve / Rebaseline / ResolveCurrent / EndSession):支援 60 fps 互動式重解
- **材料庫**:S235/S275/S355/S460 鋼 + C30/C40/C50 混凝土 + Al6061 + 自訂
- **截面庫**:Rectangular + Circular(可擴充)
- **23 dispatcher capabilities**(v2 wire protocol)、`kAbiVersion=2`
- **5-leg gate 驗證**:standalone F1..F71 / UE 135 tests / OpenSees / linear_deep_audit 104 / CLI roundtrip

### 0.5.2 LevelSim v1 — 水準儀測量大腦

LevelSim 是本專案的**水準儀(Level)測量引擎**,2026-06-18 PR#8 已 merged 進 ArchSim main(`33cbd57`,v2.2+1),受 [CLAUDE.md 鐵則 #5](../CLAUDE.md) 同等保護(`絕不碰 Plugins/LevelSim/`)。

**LevelSim 已提供且本計畫直接使用的能力**(完整 API 速查見 [Part G 章節](#g3-水準儀操作-✅-levelsim-已完整實作僅需-polish--整合)):

- **算法層** `levelsim::LevelCore.h`(純 C++17,零依賴):14 個 API 函式(`tiltFromScrews` / `bubbleFromTilt` / `sightTilt` / `measure` / `trueThreeHair` / `stadiaDistance` / `curvatureRefraction` / `parallaxJitter` / `heightOfInstrument` / `pointElevation` / `closeLoop` / `recoverIAngleTwoPeg` / `scoreReading`)
- **真值/誤差模型**:補償器殘餘安平誤差、視準軸 i 角、視差、地球曲率+大氣折光 opt-in
- **體驗層** `ALevelSimPawn`:完整 FSM(Overview / Leveling / Telescope / Booking / RouteSummary)、三個 camera、Multi-station 完整支援
- **附屬 Actor**:`ALevelStaffActor`(扶尺員/標尺)、`ALevelSimGameMode`、`ALevelSimHUD`
- **驗證**:Standalone 115/115 PASS / 27 個對抗式審核發現修正(含 critical bug 圓氣泡方向反置)/ smoke test hooks

### 0.5.3 兩個引擎的可發表性

**兩個引擎都是 C++17 純物理引擎,邏輯上各自是獨立可發表的程式**。本計畫書描述的「建築師模擬器」是兩者共同的**第一個 UE5 consumer 應用**。未來:

- FrameCore 可被 Grasshopper bridge、獨立 CAD 工具、商業結構分析軟體 reuse
- LevelSim 可被獨立的測量教學軟體、職訓中心模擬器 reuse
- 兩者**沒有耦合**(LevelSim 不接 FrameCore,FrameCore 不接 LevelSim),只是同一個遊戲 consumer 把兩者整合在同一個 UE5 場景

### 0.5.4 仍需從零開發的引擎

- **經緯儀(Theodolite)**:無對應現成引擎,Part G2 需從零實作(可參考 LevelSim 結構設計)
- 其他 12 個 Part 大多是 UE consumer 層 logic,不算「引擎」

---

# 第一章 — 可行性審查表

> 評審身份:UE5 C++ 工程師 × 結構工程教育研究者 × 高中職建築科課綱專家 × 多人遊戲架構師 × 教育心理學家
> 評估條件:全職 1-2 人開發團隊 + 12 個月開發週期(MVP 6 個月,完整版 12 個月)

評估標準:
- **技術可行性**:高/中/低(基於 UE5.7 API 能力與已驗證社群實作)
- **實作難度**:1-10(1 = 簡單 routine,10 = 需博士級演算法或數月調試)
- **MVP 必須?**:✅ 必須 / 🟡 重要 / 🟢 進階 / ⚪ 未來
- **降級方案**:若不可行或建議降級時提供

## 1.1 可行性審查表(共 15 項)

| # | 功能項目 | 技術可行性 | 難度 | MVP 必須? | 若不可行的降級方案 |
|---|----------|:---------:|:---:|:--------:|-------------------|
| 1 | **FrameCore 引擎接合**(已實作,僅需 UE consumer 整合) | 高 | 3 | ✅ | — |
| 2 | **構件放置系統**(Prefabricator + 自訂格子放置 + Snap) | 高 | 5 | ✅ | 降級為固定 Prefab 庫 + 預設位置選單 |
| 3 | **D/C 熱圖即時視覺化**(AFrameUtilizationHeatmapActor 已有) | 高 | 2 | ✅ | — |
| 4 | **載重施加**(節點/UDL/殼壓,GUI 拖拉施加) | 高 | 4 | ✅ | 降級為預設載重列表選擇 |
| 5 | **互動式重解**(ApplyPatchAndResolve, Woodbury 低秩更新) | 高 | 4 | ✅ | 已實作,直接呼叫 |
| 6 | **倒塌動畫 + Chaos 碎片**(DynCollapseReplay + FragmentCluster) | 高 | 5 | 🟡 | 降級為靜態崩塌截圖 + Chaos 碎裂特效不接結構數據 |
| 7 | **2D ↔ 3D 即時連動工作區** | 中 | 7 | 🟡 | 降級為三視角獨立切換(不雙向)、或單視角 + 透視預覽 |
| 8 | **施工工序狀態機**(配筋→澆置→養護→拆模) | 高 | 5 | 🟡 | 降級為兩階段(配筋 / 完成),不做養護時間 |
| 9 | **多人協作**(Listen Server LAN + Prefab 衝突仲裁) | 中 | 6 | 🟡 | 降級為單機模式,多人留 Phase 2 |
| 10 | **教師進入學生世界**(Spectator + Free Camera) | 中 | 5 | 🟡 | 降級為老師端僅看靜態截圖匯出 |
| 11 | **學習 Log 系統**(xAPI + 本地 IndexedDB Queue + SQL LRS) | 中 | 6 | 🟡 | 降級為純本地 CSV 匯出,不上 LRS |
| 12a | **水準儀關卡**(LevelSim plugin) | **高** | **3**(僅整合) | **✅**(LevelSim 已存在) | — |
| 12b | **隨機地形(UE5 PCG)+ LevelSim RoutePoints 整合** | 中 | 5 | 🟡 | 降級為固定 3 張練習地形 |
| 12c | **經緯儀關卡**(從零實作) | 中 | 7 | 🟡 | 延後至 Phase 2,MVP 只做水準儀 |
| 13 | **規範對標警告系統**(W-01 ~ W-10) | 高 | 4 | 🟡 | MVP 只做 5 個核心警告 |
| 14 | **BIM/CAD 匯出**(MctoNurbs sidecar 已存在,接 IFC/DXF) | 中 | 7 | 🟢 | MVP 不做匯出,先做 SaveGame |
| 15 | **AR/VR 模式** | 中 | 9 | ⚪ | 完全延後,MVP 僅 PC 滑鼠鍵盤 |

## 1.2 可行性彙總

| 結論 | 功能編號 | 數量 |
|------|---------|:----:|
| ✅ MVP 必須(Phase 0-1) | 1, 2, 3, 4, 5, **12a** | 6 |
| 🟡 重要(Phase 2)| 6, 7, 8, 9, 10, 11, **12b, 12c**, 13 | 9 |
| 🟢 進階(Phase 3)| 14 | 1 |
| ⚪ 未來(Phase 4+) | 15 | 1 |

**MVP 6 個月路徑建議**:1 → 2 → 3 → 4 → 5(可以蓋出簡單結構、加載、看 D/C 熱圖、互動式重解)+ **12a 水準儀關卡**(LevelSim 已實作,僅需整合)。**完整版 12 個月路徑**:加 6 → 7 → 8(視覺化崩塌、2D↔3D、施工工序),再加 9 → 10 → 11(多人 + 教師 + log),最後加 12b/12c(隨機地形 + 經緯儀)。測量水準儀部分(12a)直接 MVP 即可,因 LevelSim 已有現成 5-leg gate 通過的 plugin。

---

# 第二章 — 核心技術決策

> 本章列出 10 個會深遠影響整個系統架構的關鍵決策,每個決策附:推薦方案、理由、潛在風險、替代方案(若有)。風格參考 Block Reality manual 第二部分。

## 決策 1:遊戲引擎選擇 — UE5.7 + FrameCore v4.0.0 (FROZEN)

**【推薦方案】** UE5.7 + 既有 FrameCore v4.0.0 + 4 個 MIT plugin (ALS-Refactored / Prefabricator UE5 / SPUD / SUQS)

**【理由】** FrameCore 已經是 Karamba3D-parity 的成熟工程引擎(5-leg gate 已綠),整套 17 個 input USTRUCT + 9 個 result sub-USTRUCT + 8 個 BP Actor + UFrameInteractiveSubsystem 都已實作完畢。UE5.7 PCG 框架已 Production-Ready(Epic GDC 2026 確認),Iris Replication + Push Model 已穩定。4 個輔助 plugin 都是 MIT 授權、UE5.7 相容(ALS v4.17 2025-06 已正式支援 5.7),覆蓋角色控制、構件放置、持久化、任務系統四大基礎建設,各自無結構領域的競合。

**【潛在風險】**
- UE5.8 已有「replication breaking changes」論壇警告(2026-06),建議**鎖定 UE5.7** 至 MVP 完成
- SPUD 對 World Partition 有未修復 issue #117,需在 minor 版本升級後驗證 save/load 完整性
- ~~Prefabricator 的 `.uplugin` EngineVersion 欄可能需手動修為 5.7~~ → **✅ 已修(S-00 加 `"EngineVersion": "5.7.0"`,SPRINT_NOTES Spike 1 確認)**

## 決策 2:多人架構 — Listen Server + OnlineSubsystem NULL (LAN)

**【推薦方案】** Listen Server (老師機或任一學生機當 Host) + NULL Subsystem (LAN 廣播搜尋)

**【理由】** 教育場景 4-30 人規模(一個班),純校園 LAN,**不需要 Steam/EOS 帳號體系**。NULL Subsystem 依賴 LAN broadcast 尋找 Session,完全不需要網際網路,符合「開箱即用」的教育部署要求。Listen Server 的額外複雜度遠低於 Dedicated Server,且老師機可同時當 Host 與管理端,簡化部署。Replication Graph 與 Iris 對 4-30 人是過度工程,預設 UE5 Replication 配合 `NetUpdateFrequency` + `bAlwaysRelevant` + `SetNetDormancy(DORM_Dormant)` 已綽綽有餘。

**【潛在風險】**
- NULL Subsystem 依賴 UDP 廣播,**VLAN 隔離或不同子網段會失敗**;需確認班級機器同一子網
- Listen Server Host 同時跑 Server + Client + FrameCore Solve,30 人邊界時 Host 機效能瓶頸明顯;對策:加 Server 端 Solve 佇列(每幀最多執行一個)+ 每組 cooldown 3 秒
- 若未來需跨校,改用 EOS P2P 或 Dedicated,核心邏輯不變

**【替代方案】** 規模超過 30 人或跨校場景 → Dedicated Server + EOS。MVP 不採用。

## 決策 3:結構大腦分發 — Server-Authoritative + FFrameSolveResult JSON 摘要

**【推薦方案】** FrameCore Solve 只在 Server 執行,**結果以 JSON 摘要透過 GameState UPROPERTY(Replicated) 廣播**;Client 不存原始 FFrameSolveResult,只接收視覺化所需子集

**【理由】**
1. **確定性**:多機同算可能因浮點數排程微差異產生不同結果,Client 之間熱圖會不一致
2. **防止作弊**:Client 不可能在本機篡改結構計算結果假裝合格
3. **計算集中**:Solve 是 CPU 密集單次任務,集中算一次比 N 台機器同算更節省
4. **封包大小**:FFrameSolveResult 含 Displacements/MemberInternalForces 等大型 TArray,1000 根梁可能超過單封包大小;只送視覺化必要欄位(MemberUtilization + ShellUtilization + GoverningMemberIdx + bSingular)

**【潛在風險】**
- 30 人同時觸發 Solve → Server 排隊;對策:Solve 佇列 + 每組 cooldown
- JSON 摘要與完整 result 不同步;對策:摘要要包含 sequence number,Client 需要全量時用 Server RPC 請求

**【替代方案】** 若 FFrameSolveResult 大小可接受 → 實作 `bool FFrameSolveResult::NetSerialize()` 走原生 UE 序列化(MVP 不採用,JSON 較容易迭代)

## 決策 4:教師觀察模式 — Spectator + bAlwaysRelevant 教學 Actor

**【推薦方案】** `ASpectatorPawn` + `APlayerController::ChangeState(NAME_Spectating)` + `ServerViewNextPlayer/PrevPlayer`;教學 Actor (DeformedShape/Heatmap 等) 全部設 `bAlwaysRelevant=true` 避免距離截斷

**【理由】** UE5 內建 Spectator 系統是最低侵入度的方案,老師不會搶走學生的 Pawn 控制權(對比 Possess),自由相機(`ACameraActor` + `SetViewTarget`)讓老師可以漫遊整個班級。SpectatorPawn 是 local PlayerController 層,不跨網複製,無效能代價。**關鍵踩雷:預設 net relevancy 是距離判斷,老師遠處看不到學生建築 Actor,必須對教學類 Actor 設 `bAlwaysRelevant=true`**,或重載 `IsNetRelevantFor()` 以 ViewTarget 位置計算距離。

**【潛在風險】**
- bAlwaysRelevant 對 100+ 構件可能造成複製負擔;對策:設 `SetNetDormancy(DORM_Dormant)`,只在變更時複製
- 老師看的是即時狀態,若學生 Solve 還在 Server 排隊中,熱圖會「閃一下」舊資料

## 決策 5:Prefab 放置多人衝突 — Server 樂觀鎖 + 格子佔用 TMap

**【推薦方案】** Server-side `TMap<FIntVector, FActorHandle> OccupancyGrid` + Client 送 `Server_RequestPlacement` RPC + Server 查詢後才 Spawn + Replicate

**【理由】** Prefabricator 本身是 Editor-side 工具,Runtime 多人沒有內建衝突處理。Server 是唯一世界狀態真相,Client 本地的「預覽 ghost」純屬 UI。**樂觀鎖 + TMap O(1) 查詢**對 4-30 人並發放置完全足夠,不需要 Link-Cut Tree 之類複雜結構。格子化座標(整數 voxel grid)符合建築沙盒邏輯,也方便碰撞查詢。

**【潛在風險】**
- 兩個學生同時送 RPC 欲放同一格,Server 在同一幀收到;對策:RPC 進入時先查詢並標記,Spawn 之後再廣播
- 失敗時 Client 端的預覽 ghost 要正確收回,避免「我看到我放了但 Server 拒絕」的混亂

## 決策 6:持久化策略 — SPUD + JSON 模型雙軌

**【推薦方案】** 走 SPUD(用於場景 Actor 狀態) + 自訂 SaveGame UPROPERTY(用於 `FFrameModelDef` 輸入); **不直接序列化 FFrameSolveResult**,載入後重新 SolveLinear() 復原

**【理由】**
1. SPUD 自動處理 Actor Transform + 標記為 SaveGame 的 UPROPERTY,Listen Server 模式下 Host 本地存檔即是 Server 存檔
2. FFrameSolveResult 含複雜 TArray,直接存有 `TObjectPtr` 序列化陷阱(SPUD issue #56);**只存輸入端 POD-friendly 的 FFrameModelDef**,結果可重算
3. 老師發題與分發關卡走 JSON(用 UFrameModelBuilder::LoadModelFromJson),不走 SaveGame

**【潛在風險】**
- SPUD World Partition 在 UE 版本升級時可能爆炸(issue #117 open);**每次 UE minor 升級必驗 save/load 完整性**
- 重新 Solve 載入時可能卡頓;對策:背景執行 + 載入畫面 + 第一次 Solve 完成才釋放遊戲輸入

## 決策 7:任務系統 — SUQS + 業務層條件判斷

**【推薦方案】** SUQS DataTable 定義關卡結構(quest/objective/task 樹) + C++ OnSolveComplete callback 中業務層判斷數值條件 → CompleteTask/ProgressTask

**【理由】** SUQS 是純資料驅動 + 業務層驅動的設計。**數值條件(如「撓度 < 10mm」「D/C < 1.0」)無法在 quest JSON 中設定**,需要在 `OnSolveComplete` callback 中由業務層用 FrameCore 結果判斷後呼叫 SUQS API。這分離反而比硬塞進 SUQS 更乾淨 — quest 系統只管「狀態機」,業務邏輯在自己的 ArchSim Subsystem。

**【潛在風險】**
- SUQS 沒有 built-in replication,多人模式下任務狀態同步需自己設計;對策:將 `USuqsProgression` 快照存在 PlayerState 的 replicated UPROPERTY
- Quest JSON 升 UE 版本時 row struct 可能 BREAKING;對策:版本控制 quest JSON + 升級時 reimport 測試

## 決策 8:色彩配色 — D/C 用綠/黃/紅 + 其他連續量用 Cividis(色盲友善)

**【推薦方案】**
- **D/C 比值**:綠 #009E73(Bluish Green) → 黃 #F0E442 → 橘 #E69F00 → 紅 #D55E00(Okabe-Ito 色盲友善版,語意對應交通號誌)
- **位移/應力大小**:Cividis 色表(深藍 #00224E → 亮黃 #F6D645),感知均勻 + 對三型 CVD 近乎相同外觀
- **雙向量(軸力/彎矩/模態)**:藍 #0072B2 → 白 → 橘 #E69F00 發散色表
- **永遠不用 Jet 彩虹色表**

**【理由】**
- 高職男生族群 CVD 流行率 ~8%,**絕不能讓教育材料對 1/12 學生不可讀**
- D/C 是核心教學指標,綠/黃/紅交通號誌語意對 15-17 歲學生最直覺(比工程慣用「藍=低、紅=高」更快認知)
- Cividis 是 2018 PLOS One 論文發表的 CVD-optimized 色表,對 deuteranopia/protanopia/tritanopia 三型呈現近乎相同外觀
- 連續漸層比步階離散色更快被遊戲世代理解(血量條、地圖熱區的語意移植)

**【潛在風險】**
- 工程師習慣 Karamba3D 的「磚紅=壓、鋼藍=拉」,本系統的「藍=拉、橘=壓」會造成跨工具混淆;對策:HUD 標明色彩約定 + 圖示作為冗餘語意管道

## 決策 9:學習 Log — xAPI + SQL LRS (Yet Analytics) + IndexedDB 離線隊列

**【推薦方案】** xAPI 1.0.3 Statement schema + 自訂 ArchSim Application Profile + SQL LRS (Apache 2.0, Docker 部署) + Client 端 IndexedDB sync queue + Background Sync 自動重送

**【理由】**
- xAPI 比 IMS Caliper 更適合遊戲場景(xAPI-SG Profile 已有 e-UCM/ADL 標準動詞),Caliper 的 Metric Profile 對自訂動作擴充困難
- SQL LRS 是目前唯一活躍的開源 LRS(Learning Locker 2021 停止維護),Apache 2.0,支援 SQLite(學校自建)/PostgreSQL(雲端)
- xAPI Statement 天然是不可變事件流(append-only),idempotency UUID 完美對應離線重送
- **學生姓名永不進入 Statement**,用 `account.name = SHA256(student_id + salt)`,符合台灣個資法 pseudonymization

**【潛在風險】**
- Safari 不支援 Background Sync API;對策:`visibilitychange` fallback 或在 UE 層做重試
- 老師端 dashboard 需要自己寫 REST API 查 LRS;對策:見 Part I

## 決策 10:RC 融合策略 — UE 端 helper 算等效材料,FrameCore 不知道融合存在

**【推薦方案】** 新建 `UFrameRCMaterialHelper` BlueprintFunctionLibrary,實作 φ 公式(`φ_tens=0.8 / φ_shear=0.6 / comp_boost=1.1`);REBAR + CONCRETE 相鄰偵測 → 計算融合後等效 `FFrameMaterial`(`E_eff` / `Fy_eff`) → 透過 `FFrameModelPatch` 局部更新到 FrameCore;**FrameCore engine 完全不知道「RC 融合」這個概念**

**【理由】**
- 符合 CLAUDE.md 鐵則 #1(engine source FROZEN);所有教育化簡的概念都封在 UE consumer 層
- 學生在遊戲中體驗到「加鋼筋讓結構變強」的直覺感受,引擎只看到一個「強一點的材料」
- 蜂窩弱點(15% 機率 R×0.6)也走相同管道:UE 端亂數判定 → patch 材料參數

**【潛在風險】**
- 老師若要驗算公式,要知道 UE 端做了 alias 轉換;對策:R 氏材料系統明示對應關係於教師備課文檔
- φ 公式跟真實 ACI 318 規範細節有出入(實際 RC 設計更複雜);明確標示為「教育用簡化模型」,不可移植真實工程使用

---

# 第三章 — Part A:引擎與外殼

## A1 FrameCore 接合層(USTRUCT 橋接,不動 engine)

### 概念
FrameCore 是已凍結的純 C++17 引擎,UE 端只能透過 `FrameCoreUE` plugin 暴露的 17 個 input USTRUCT + 9 個 result sub-USTRUCT + 8 個 BP Actor + UFrameInteractiveSubsystem + 15 個 Analysis BP entry 與引擎互動。本層的職責是**讓所有遊戲世界的構件 Actor 都正確映射到 FFrameModelDef 的 Materials/Sections/Nodes/Members/Shells 索引**。

### 玩家視角
玩家**完全感受不到**這一層。它是橋接層,但若橋接做錯,玩家會看到「我蓋了東西但熱圖不變」「我改了材料但結構行為不變」這類最致命的學習體驗破壞。

### 介面與操作規格
- 新建 ActorComponent `UArchSimMemberData`:每個結構構件 Actor 持有一個,儲存 `MemberIdx`(int32, FrameCore 中的索引)、`StructureGroupId`(int32, Union-Find 分組)、`CachedUtilization`(float, 上次 solve 的 D/C),三者都標記 `UPROPERTY(SaveGame)`。
- 新建 GameInstanceSubsystem `UArchSimModelRegistry`:統一管理「Actor ↔ FrameCore index」雙向映射,所有 Place/Remove/Modify 構件動作都走這層。
- 構件 Actor BeginPlay 時呼叫 `UArchSimModelRegistry::RegisterMember(this, MaterialId, SectionId)`,Registry 維護 `FFrameModelDef` 並回填 `MemberIdx` 給 Component。
- Solve 完成後 `UArchSimModelRegistry::DistributeSolveResult(const FFrameSolveResult&)` 把結果寫回每個 Component 的 `CachedUtilization`。

### FrameCore 接合
- 寫入引擎走 `UFrameInteractiveSubsystem::ApplyPatchAndResolve(FFrameModelPatch, OutResult)`(Woodbury 低秩更新,適合每幀微小改動)
- 讀回結果走 `FFrameSolveResult.MemberUtilization[]`、`ShellUtilization[]`、`Utilization`(DemandSummary)
- 材料/截面變更必須呼叫 `Rebaseline()`,因為改變 K 矩陣本身

### 教學落點
不直接教學;但這層的正確性決定**所有教學能否成立**。

### 完成標準
- [ ] `UArchSimMemberData` 可在 Actor 上掛載,SaveGame round-trip(放下 → 存檔 → 載入)後 MemberIdx 一致
- [ ] `UArchSimModelRegistry::RegisterMember` 在連續放置 50 個構件後耗時 <50ms 總計
- [ ] FrameCore Solve 後 `CachedUtilization` 在所有 Component 上正確更新,可用 BP `GetCachedUtilization` 讀取
- [ ] 移除一根梁後 Registry 正確 deactivate FrameCore Member(透過 `FFrameModelPatch::DeactivateMemberIds`)
- [ ] 整合測試:用 [F58 cantilever fixture](../Plugins/FrameSolver/Standalone/) 對標,UE 端 D/C 與 standalone 結果差 < 1e-5

### 預期踩雷
- **Actor 持有 raw pointer 到 FrameCore 構件**會違反鐵則 #4(用 index 不用裸指標),修正:Component 只存 `int32 MemberIdx`
- 連續刪除構件時 MemberIdx 重編號:Registry 必須有 stale 偵測機制(透過 `bActive` flag 軟刪除,而非真刪除 array element)
- `FFrameSolveResult` 可能 `bSingular=true`(機構/不穩定),Distribute 前必須檢查;否則熱圖會吃到 NaN
- ApplyPatchAndResolve 高頻呼叫(每幀拖拉)會打爆 Woodbury rank 累積;Registry 需要 debounce(建議 150ms)+ 累積超過 `MaxRank=96` 時自動 Rebaseline

---

## A2 ALS 角色與相機系統

### 概念
玩家在世界中走動、操作構件、觀察結構行為的所有人類介面,都建立在 ALS-Refactored v4.17 的角色控制與相機系統之上。MVP 階段只用第三人稱,Phase 2 加第一人稱切換。

### 玩家視角
- 像一般 3D 遊戲一樣 WASD 移動、滑鼠看向、Space 跳躍
- 滑鼠左鍵主互動(放置/選取構件)、右鍵副互動(取消/旋轉預覽)
- E 鍵切換「建築模式 ↔ 觀察模式」(Enhanced Input MappingContext 切換)
- 觀察模式下相機可拉遠俯瞰整個結構

### 介面與操作規格
- Character 類別 `AArchSimCharacter : public AAlsCharacter`,加 `UAlsCameraComponent`
- `Config/DefaultEngine.ini` 設定 `DefaultPlayerInputClass=EnhancedPlayerInput`(ALS README 強制要求)
- 兩個 MappingContext:`MC_Locomotion`(ALS 預設行走) + `MC_Building`(自訂建築模式);Priority 分別為 0 / 1
- 按 E 切換時呼叫 `AddMappingContext(MC_Building, 1)` 或 `RemoveMappingContext(MC_Building)`
- DataAsset `DA_ArchSimInputActions` 定義:`IA_PlaceMember` / `IA_RemoveMember` / `IA_OpenAnalysisPanel` / `IA_ToggleHeatmap` / `IA_ApplyLoad`

### FrameCore 接合
角色不直接呼叫 FrameCore,而是透過 Server RPC → UFrameInteractiveSubsystem(GameInstance Subsystem)。Character 的 Input handler 範例:
```
OnPlaceMemberAction:
  if !HasAuthority: Server_RequestPlacement(Pos, Material, Section)
  else: UFrameInteractiveSubsystem::ApplyPatchAndResolve(...)
```

### 教學落點
低層基礎建設,**直接教學落點 0**。但好的角色操作降低外在認知負荷(Sweller CLT),讓學生把注意力放在結構學習而非「為什麼我跳不過去」。

### 完成標準
- [ ] PIE 中 `AArchSimCharacter` 行走/跑步/跳躍/坐下/趴下五大動作正常
- [ ] Enhanced Input MappingContext 切換建築模式 ↔ 行走模式無輸入殘留
- [ ] Splitscreen 2 P 各自正確初始化 Enhanced Input subsystem(UE5.7 須實測,UE5.6 有已知 bug)
- [ ] Input handler 中呼叫 `UFrameInteractiveSubsystem::ApplyPatchAndResolve`,Server 端正確觸發求解

### 預期踩雷
- ALS v4.17 移除 Animation Compression Library 常數插值,自訂 AnimBP 需重新 bake curves
- LocalMultiplayer Enhanced Input `AddMappingContext` 在 splitscreen 第 2 玩家初始化時機 bug(UE5.6 確認,5.7 須測);workaround 是 `OnPossess` 後延 1 frame 再 AddMappingContext
- 自訂 ALS subclass 新增 Replicated property 必須加 `MARK_PROPERTY_DIRTY_FROM_NAME`,否則 Iris 模式下 client 永不收到更新(靜默失敗,最難 debug)

---

## A3 世界系統(地形 + Level + World Partition)

### 概念
每個關卡都在一個獨立的 UE Level 內運行,沙盒模式可選不同的「基地模板」(平地 / 緩坡 / 既有建築旁等)。地形由 UE5 PCG 框架程序化生成(支援 Seed 確定性 + 每局不同),靜態場景用 World Partition 切塊載入。

### 玩家視角
- 進入關卡後看到一片地、可能有遠景的城市/山景作為氛圍
- 場景邊界明確(視覺上以柵欄或邊界線標明),不會掉出地圖
- 沙盒模式可以選 3-5 種基地類型,每種有不同地形挑戰

### 介面與操作規格
- 主場景 `L_ArchSim_Sandbox` 啟用 World Partition + Data Layers
- PCG 圖 `PCG_BaseTerrain`:Surface Sampler + **`Spatial Noise`(Mode = `Perlin2D`)** + 高度範圍 ±0.5m~±5m 可調(UE5.7 PCG 無獨立「Perlin Noise」節點,2026-06-25 S-00 spike 確認;見 Part G1 相同訂正)
- 每個關卡用獨立 LevelStreaming Volume 載入特定地形 chunk
- 基地中心預設座標 (0, 0, 0),向北為 +Y、向東為 +X、向上為 +Z(UE5 預設左手座標)
- 構件放置範圍限制:`FBox(FVector(-5000, -5000, 0), FVector(5000, 5000, 5000))`(以 cm 為單位,即 50m × 50m × 50m 範圍)

### FrameCore 接合
基地不直接接 FrameCore,但**地基節點的 prescribed DOF(支承固定)** 由地形決定 — 凡是與地面接觸(Z=0)的構件節點自動設 `Fixed=[true, true, true, true, true, true]`(完全固接基礎)。地形若不平,各支座 Z 座標跟隨地形高度,**這是測量關卡的銜接點**(放樣結果可直接作為節點座標輸入)。

### 教學落點
- **基地與測量**:對應「土木建築工程與技術概論」「測量實習」科目;讓學生感受「真實地形不平 → 結構設計要考慮基礎高程」
- **空間尺度感**:對應「建築造型實習」「室內設計概論」尺度感培養

### 完成標準
- [ ] 5 種沙盒基地模板(平地 / 緩坡 / 河岸 / 既有建築旁 / 山坡)
- [ ] PCG Seed 鎖定後,同一 Seed 每次載入產生相同地形(確定性)
- [ ] World Partition 切塊載入無感(玩家走動不卡頓)
- [ ] 放置構件碰到地形時自動 snap 到地面 Z 高度
- [ ] 地形高度差透過測量關卡可正確讀出(精度 ±10mm 內)

### 預期踩雷
- UE5 PCG 在 UE5.7 才正式 Production-Ready;5.4-5.6 版本有效能不穩定問題,**鎖定 5.7+**
- SPUD 對 World Partition cell naming scheme 改變敏感(issue #117 open),UE minor 升級必驗 save/load
- Listen Server 的 Host 機載入 World Partition + 同時跑 FrameCore Solve 可能爆 RAM;對策:Server 用簡化 collision proxy,Client 用完整 mesh

---

# 第四章 — Part B:玩家操作層

## B1 構件放置系統(Prefabricator + 格子 Snap)

### 概念
玩家在 3D 場景中放置「結構構件」的主要互動。每個構件對應一個 Prefab(柱、梁、牆殼、樓板、斜撐 5 大類),放置時即時預覽,確認後 Server 仲裁 + Replicate。

### 玩家視角
1. 從 HUD 左側構件面板選一個構件(滾輪切換大類別 / 點擊選擇具體型號)
2. 滑鼠在場景中移動,看到半透明的「ghost 預覽」跟著游標,自動 snap 到格線
3. ghost 變綠 = 可放置,變紅 = 衝突(已有構件 / 出界 / 不合理位置)
4. 滑鼠左鍵確認放置;R 鍵旋轉 90°;ESC 取消選擇
5. 放置後即時觸發 FrameCore 重解,熱圖更新

### 介面與操作規格
- Prefab 庫對齊 `UFrameSectionLibrary` 命名:`Prefab_Column_H200x200`、`Prefab_Beam_Rectangular_300x500`、`Prefab_Shell_Wall_200mm`...
- 格子大小預設 500mm(0.5m,合理建築模數);可在 Project Settings 調整
- Ghost 預覽走 `UMaterialInstanceDynamic` 半透明材質,透明度 0.5,綠色 #009E73 / 紅色 #D55E00
- Server 仲裁:`Server_RequestPlacement(FVector GridPos, FRotator Rot, FName PrefabId)` RPC
- Server 端 `UArchSimPrefabSpawnSubsystem::SpawnPrefab()` 查詢 `OccupancyGrid` TMap<FIntVector, FActorHandle> → 若空則 Spawn 並廣播,若占則回傳失敗
- Spawn 後 Actor 自動 attach `UArchSimMemberData` Component(見 A1)

### FrameCore 接合
- 放置成功後 Registry 呼叫 `UFrameInteractiveSubsystem::ApplyPatchAndResolve(Patch, OutResult)`,Patch 內含 `ReactivateMemberIds=[新 MemberIdx]`
- 若新構件造成超載(`OutResult.Utilization.MaxDC > 1.0`),熱圖立即顯示紅色,但放置動作仍成功 — **失敗即學習,不阻擋操作**

### 教學落點
- **構造與施工法**(課綱):學生選擇柱/梁/板的位置與斷面,理解結構系統組成
- **設計與技術實習**:Pre-design intuition,玩家會自然發現「沒有柱子的牆會塌」「太細的梁會撓」
- **直覺優先原則**:不強迫先計算才能放,先放再學(Productive Failure - Kapur 2016)

### 完成標準
- [ ] 5 大類構件(柱/梁/板/牆/斜撐)各至少 3 個 Prefab 變體(小/中/大),共 15+ Prefab
- [ ] Ghost 預覽 60 fps 跟隨滑鼠,延遲 <16ms
- [ ] 衝突偵測:2 個玩家同時送 RPC 欲放同一格,Server 正確序列化,只有一個成功
- [ ] 放置後熱圖在 200ms 內更新(端到端:Client RPC → Server Solve → Replicate → Client 重渲)
- [ ] R 鍵旋轉 90° 後 ghost 跟構件本身視覺一致(不出現「預覽是橫的,放下變直的」)

### 預期踩雷
- Prefabricator 的 `.uplugin EngineVersion` 可能未標 5.7,需手動編輯(否則 plugin whitelist 拒絕載入)
- 旋轉超過 360° 後 FRotator wrap-around 處理錯誤:統一在 RPC 內 `FRotator::Normalize()`
- Listen Server 的 Host 玩家放置時 RTT=0,其他人 RTT=50-100ms;Host 容易有「我放下了但其他人慢半拍」的不公平感;對策:Host 玩家本地也走 RPC 模式(視覺一致性 > RTT 公平)

---

## B2 2D ↔ 3D 即時雙向工作區

### 概念
這是教育需求書 v3 中最有教學價值的單一功能:**讓平面圖、立面圖、3D 透視在同一個畫面上即時同步**,學生在 2D 畫一根梁,3D 立即出現;在 3D 旋轉視角,2D 圖立即重算。對應圖學課的「投影法」「三視圖」核心概念。

### 玩家視角
- 螢幕分四象限:左上=俯視圖(Top)、左下=正視圖(Front)、右上=側視圖(Side)、右下=3D 透視
- 在任一 2D 視圖中拖拉構件 → 3D 跟著動
- 旋轉 3D 視角 → 三視圖不變(因為這是正投影,不隨相機轉)
- 點選 2D 中的一條線 → 3D 中對應的構件高亮泛藍 #56B4E9(Okabe-Ito Sky Blue)
- Tab 鍵切換「四象限模式 ↔ 純 3D 模式 ↔ 純 2D 模式」(對應 PhET「漸進式揭露」原則)

### 介面與操作規格
- 主 Screen 類別 `UFastDesignWorkspaceWidget`(UMG)
- 三個正交 Viewport 走 `USceneCaptureComponent2D` + Render Target,正交投影矩陣 (`ProjectionMatrix = FMath::OrthoMatrix(...)`)
- 視圖之間的關聯透過 `USelectionState` (Slate 共用狀態) 廣播 `OnMemberSelected`/`OnMemberHovered` 事件
- 3D Viewport 用 UE 原生 ViewportClient,2D Viewport 用 Render Target + 自訂 SWidget 接收滑鼠輸入
- 三視圖隱藏線/虛線規則:看不見的構件邊用虛線(目前先做不畫,Phase 2 完整化)
- 切面工具:玩家拉一條切面平面,4 個視圖之一變為「該切面剖面圖」

### FrameCore 接合
與 FrameCore 無直接接合 — 只是同一份結構模型的不同視覺投影。但 **selection state 觸發 `AFrameUtilizationHeatmapActor` 高亮指定 Member**(透過 MemberIdx 對應)。

### 教學落點
- **製圖實習(課綱必修 8 學分)**:平面圖、立面圖、剖面圖、投影法
- **電腦輔助製圖實習(高二 6 學分)**:CAD 操作邏輯熟悉
- **空間思維能力**:對應 108 課綱核心素養 B1「符號運用與溝通表達」

### 完成標準
- [ ] 四象限同步:拖一個構件,所有四個視圖立即更新(延遲 <33ms,即 30 fps 同步)
- [ ] 2D 視圖中選取的線段,3D 視圖正確高亮對應構件(包含旋轉相機後仍正確)
- [ ] 切面工具:玩家定義切面後,該切面剖面圖正確生成,顯示對應構件斷面
- [ ] 三視圖在 1920×1080 螢幕下無視覺重疊或滾動條
- [ ] UE5 第三角法 / 第一角法切換 toggle(預設第三角法 — 美規,符合台灣建築界慣例)

### 預期踩雷
- 三個正交 Viewport 同時 render 對 GPU 是 3x 負擔;對策:正交視圖用低 LOD 簡化 mesh + 降低 render 頻率(15 fps 已足夠識讀)
- `USceneCaptureComponent2D` 預設不複製到 client,須設 `bCaptureEveryFrame=true`(client local)
- 切面工具如果定義在 0 平面附近,容易切到 NaN/Inf 區域;對策:切面 plane 強制有最小厚度 ε=1cm
- 隱藏線判定要計算 visibility,完整版實作很重;MVP 簡化為「不畫隱藏線」(虛線留 Phase 2)

---

## B3 圖學標註系統(尺寸/剖面/標題)

### 概念
建築圖學的「靈魂」是標註 — 尺寸線、引線、剖面符號、軸線編號、標題塊。本系統提供與 CNS / ISO 製圖規範對齊的標註工具,讓學生繪製出來的圖可以接到業界正式工具(透過 DXF/IFC 匯出 - 見 B4 與附錄)。

### 玩家視角
- 在 2D 視圖中按 D 鍵啟動尺寸標註模式 → 點兩個點,自動畫出尺寸線 + 標註長度
- 按 S 鍵啟動剖面符號模式 → 拉一條線,自動產生剖面記號 A-A、B-B...
- 按 T 鍵啟動文字標註模式 → 點一個位置,輸入文字
- 軸線編號自動生成:水平方向 A/B/C...,垂直方向 1/2/3...,可在 Settings 切換為阿拉伯數字或羅馬數字
- 標題塊在每張圖右下角,自動填入學生姓名、日期、關卡名稱、比例尺

### 介面與操作規格
- 標註 Actor `AArchSimAnnotation`(基類)+ 子類:`Dimension` / `SectionMark` / `TextLabel` / `AxisLine` / `TitleBlock`
- 尺寸標註自動單位:預設 mm,可在 Settings 切換為 cm/m
- 比例尺自動偵測:viewport 寬度 / 視野範圍 = 當前比例,顯示為 1:100 / 1:50 / 1:20 等標準比例
- 標題塊用 DataAsset `DA_TitleBlockTemplate`,內容可在 Settings 修改

### FrameCore 接合
標註系統與 FrameCore 完全解耦,純粹是視覺層。但**標註會在匯出時與 FrameCore 模型一起寫入 DXF/IFC**(見附錄 B 的匯出管道)。

### 教學落點
- **製圖實習**:對應圖01「能以正投影法繪製三視圖」、圖02「能識讀施工圖材料標示與尺寸」
- **電腦輔助製圖實習**:對應圖03「能操作 CAD 完成基本建築圖」

### 完成標準
- [ ] 5 種標註類型(尺寸/剖面/文字/軸線/標題)均可在 2D 視圖中放置
- [ ] 尺寸自動單位切換(mm/cm/m)
- [ ] 軸線自動編號連續正確(刪除中間一軸後重新編號)
- [ ] 標題塊內容可在遊戲內 UI 編輯
- [ ] 比例尺 1:100 顯示時,真實長度 1m = 螢幕 10mm(±2% 容差)
- [ ] 匯出 DXF 時所有標註保留為 DIMENSION / TEXT entity

### 預期踩雷
- CNS B1001(建築製圖)與 ISO 128(製圖總則)的虛線比例規範略不同;預設用 CNS,Settings 提供切換
- 第一角 vs 第三角法投影學生易混淆;MVP 鎖定第三角法(美規,符合台灣建築界慣例)
- 標註太多會視覺塞車;對策:Layer 控制(MVP 不做,Phase 2 加標註可見性 toggle)

---

## B4 藍圖系統(Prefab 存取 + 旋轉鏡像)

### 概念
玩家可以把自己設計的「子結構」(如一個門框、一組桁架、一個典型樓層)儲存為**藍圖(Blueprint)**,日後可重複使用、旋轉、鏡像。對應 Block Reality 的「Litematica 藍圖系統 + 全息投影」理念,在 UE5 端用 Prefabricator 的 Prefab 機制實作。

### 玩家視角
- 框選一群構件 → 右鍵「儲存為藍圖」→ 命名 → 圖示自動截圖
- 藍圖庫顯示為 HUD 右側面板,類似 Minecraft inventory
- 拖拉藍圖到場景 → 顯示半透明預覽(Litematica 風格 ghost) → 確認後實體化所有構件
- R/Shift+R 旋轉 ghost,F 鏡像翻轉
- 藍圖可分享:輸出為 `.archsim_bp` 檔(JSON + 縮圖),老師可分發或學生互相交換

### 介面與操作規格
- Prefab 儲存:呼叫 Prefabricator API `CreatePrefabFromActors(SelectedActors)` → `UPrefabricatorAsset`
- 縮圖自動截圖:`USceneCaptureComponent2D` 從上方俯視 + 透視 30° 角拍一張 256×256 PNG
- 藍圖庫 UMG widget `UBlueprintLibraryWidget`,grid 顯示縮圖 + 名稱 + 構件數
- 全息投影預覽:Prefabricator 內建 ghost preview material + 額外疊加 `AFrameUtilizationHeatmapActor` 預測(若效能允許,MVP 不做預測)
- `.archsim_bp` 檔 JSON schema 範例:`{ "version": "1.0", "name": "Standard 3x3 Frame", "thumbnail_base64": "...", "members": [...], "nodes": [...] }`

### FrameCore 接合
藍圖載入後,所有構件被 `UArchSimModelRegistry::RegisterMember` 一次性註冊(批次),最後呼叫一次 `Rebaseline()` 重新建立 FrameCore session。**載入大型藍圖會卡頓**(100+ 構件),對策:背景執行 + 載入畫面。

### 教學落點
- **設計與技術實習**:鼓勵學生建立「個人元件庫」,培養模組化設計思維
- **多人協作**:組員可互傳藍圖,觀察彼此設計策略(對應 Papert constructionism「公開可見的人工物件」)
- **學習歷程檔案**:藍圖匯出可作為 108 課綱要求的「實作作品」上傳項目

### 完成標準
- [ ] 框選 10 個構件儲存為藍圖,縮圖正確生成
- [ ] 藍圖庫顯示 20+ 範例藍圖(預載 + 玩家自製)
- [ ] 載入藍圖到場景,所有構件位置、材料、斷面均正確還原
- [ ] 旋轉 90° 後構件間連接關係保持(節點 ID 重新映射)
- [ ] 鏡像翻轉後左右手坐標系正確處理
- [ ] `.archsim_bp` 檔可在不同學生機器間互相讀取(同 UE5 版本)

### 預期踩雷
- Prefabricator 不支援 Instanced Static Mesh,對 100+ 構件渲染負擔大;對策:LOD + Nanite
- 旋轉導致 `FFrameMember::RefVec` 失效:每次旋轉重新計算 RefVec(預設指向 +Z 或 +Y,依構件方向)
- 鏡像導致 Iy/Iz 對換(若是非對稱截面):純對稱截面(rectangular/circular)不受影響,Phase 2 加非對稱截面時要處理
- 跨 UE 版本載入舊藍圖:JSON 加 `version` 欄位,升級時 schema migration

---

# 第五章 — Part C:工程模擬層

## C1 載重施加(節點/UDL/殼壓/自重)

### 概念
玩家在已蓋好的結構上施加載重,觀察結構行為。對應課綱「基礎工程力學」核心概念:**靜載重(D)、活載重(L)、地震力(E)**,以及它們的組合 1.4D / 1.2D+1.6L / 1.2D+1.0E+1.0L。

### 玩家視角
- HUD 上有「載重工具」(綁定 IA_ApplyLoad)
- 點擊樓板 → 拉出滑桿選擇活載重類型(住宅 200 / 辦公 300 / 走廊 400 kgf/m²,對應台灣建築技術規則)
- 點擊節點 → 輸入集中力大小與方向(預設往下重力方向)
- 拖拉梁段 → 施加均佈線載重(UDL)
- 地震力按鈕:從 HUD 拉出滑桿選「低 / 中 / 高」三段(對應台灣地震區係數簡化)
- 自重自動計算(從 Material.Rho × Section.A × Length),不需手動

### 介面與操作規格
- 載重 USTRUCT 對應 FrameCore:
  - `FFrameNodalLoad { Node, Comp[6: Fx Fy Fz Mx My Mz] }`(節點集中力 + 力矩)
  - `FFrameMemberUDL { Member, WLocal: FVector }`(沿桿局部座標力/長)
  - `FFrameShellPressure { Shell, P }`(沿殼法向壓力)
- 載重視覺化:節點力用紅色箭頭、UDL 用紅色等距小箭頭、Shell 壓力用紅色等距點陣
- 載重編輯:點選已施加的載重 → 修改大小或刪除
- 載重類型範本庫:DataAsset `DA_LoadTypeTemplates` 包含常用載重(住宅/辦公/倉庫/車道等),每個對應 N/m² 數值

### FrameCore 接合
- 載重透過 `FFrameModelPatch::SetNodalLoads[] / AddNodalLoads[]`(v3.6 U-12)增量更新
- 載重組合自動產生:`UFrameAnalysisLibrary::LoadCombineEnvelope(BaseDef, Opts, Cases[])` 一次跑 3-5 組合,取最大 D/C
- 自重自動計算:在 `FFrameModelDef` 組裝時遍歷 Members 計算 W = Material.Rho × Section.A × g,加到 NodalLoads

### 教學落點
- **基礎工程力學(課綱必修 6 學分)**:對應力01「靜力平衡」、力03「容許應力設計」、力04「斷面尺寸合理性」
- **載重組合概念**:1.2D + 1.6L 的係數差異反映不確定性(活載重不可預測 → 係數更高)
- **規範對標**:對應建築技術規則建築構造編,活載重住宅 200 / 辦公 300 等台灣標準值

### 完成標準
- [ ] 4 種載重類型(節點力 / UDL / 殼壓 / 自重)均可施加
- [ ] 載重視覺化(箭頭)隨大小縮放,不會堆疊到看不見
- [ ] 載重組合 1.2D + 1.6L 自動產生並計算包絡
- [ ] 自重從 Material+Section 自動計算,玩家不需手動輸入
- [ ] 載重編輯:修改數值 → 熱圖立即更新(<200ms)
- [ ] 地震力簡化版:三段強度對應台灣四個地震區(0.18g / 0.23g / 0.28g / 0.33g)

### 預期踩雷
- 載重方向座標系混淆:**局部座標(沿桿)** vs **全域座標(沿世界)**;UI 要明確標示
- 活載重「樓板上每平方米」 vs FrameCore「節點/桿件離散載重」轉換:UE 端 helper 把樓板載重平均分配到周邊節點與梁
- 地震力簡化為等效靜力(EAR = C × W),不做反應譜分析(留 Part D2/D3 進階)
- 自重重複計算:若手動輸入自重 + 自動自重,會 double-counting;對策:自重永遠由系統管,玩家不可手動加自重

---

## C2 即時 D/C 熱圖(每幀互動式重解)

### 概念
這是整個教育遊戲的**核心視覺語言**。玩家每一個動作(放構件 / 改材料 / 改載重)後,結構的 D/C 比值(Demand/Capacity)在 200ms 內更新熱圖,讓「直覺式學習」(Part J)成立。對應 Karamba3D 在大學建築系教育的成功模式。

### 玩家視角
- 結構構件本身用半透明材質呈現,**表面色彩隨 D/C 比值變化**
- 預設顯示熱圖,可按 H 切換 ON/OFF
- 配色:綠 #009E73(D/C<0.85) → 黃 #F0E442(0.85~1.0) → 橘 #E69F00(1.0~1.2) → 紅 #D55E00(>1.2);Color blind friendly (Okabe-Ito)
- D/C > 1.0 時構件邊緣加紅色閃爍提示
- HUD 右上角即時顯示「最危險構件:M-12, D/C = 1.34, 模式:bending」(對應 W-01 警告)

### 介面與操作規格
- 使用 `AFrameUtilizationHeatmapActor`(已實作),設定:
  - `SaturationDC = 1.0`(D/C=1.0 對應純紅;玩家進階模式可設 0.5 早期預警)
  - `MemberGeometry` 從 `UArchSimModelRegistry` 提供
- 配色實作:UE5 Curve Atlas(`CurveLinearColor` + `CurveAtlas` + `CurveAtlasRowParameter`),儲存 D/C 0-2 範圍的色彩
- 變更模式:HUD 切換按鈕「D/C 模式 / 位移模式 / 應力模式 / 模態模式」,各模式呼叫不同 BP Actor 顯示
- 重解 debounce:玩家連續修改時,150ms 內只觸發一次 Solve(避免 Server 過載)

### FrameCore 接合
核心 API 序列:
```
1. 玩家動作 → UArchSimModelRegistry.RequestUpdate(patch)
2. 150ms debounce timer
3. UFrameInteractiveSubsystem::ApplyPatchAndResolve(patch, OutResult)
4. UArchSimModelRegistry::DistributeSolveResult(OutResult)
5. AFrameUtilizationHeatmapActor::BuildHeatmap() — 重建 PMC mesh + 配色
6. HUD 從 OutResult.Utilization 讀取 MaxDC + GoverningMemberIdx → 更新警告文字
```

### 教學落點
- **基礎工程力學**:核心對應點;學生看到「我加根斜撐 → 紅色變綠」的因果關係
- **構造與施工法**:理解不同構件的承載能力差異(柱抗壓 vs 梁抗彎 vs 板抗剪)
- **直覺優先 + 即時回饋**(PhET 設計原則 + Csíkszentmihályi Flow 條件)

### 完成標準
- [ ] 熱圖在 200ms 內反映玩家修改(端到端,90% percentile)
- [ ] 100 個構件規模下,熱圖渲染 >30 fps(60 fps 為目標)
- [ ] D/C > 1.0 構件邊緣紅色閃爍可見性測試(視角從遠到近都明顯)
- [ ] HUD 警告文字正確識別 `governingMemberIdx`(-1 = 無危險時隱藏)
- [ ] 模式切換(D/C / 位移 / 應力 / 模態)無視覺殘留

### 預期踩雷
- `AFrameUtilizationHeatmapActor::BuildHeatmap` 每次重建 PMC 耗時 ~10ms/100 構件;對策:用 `PerInstanceCustomData` 走 GPU 路徑只更新色彩(MVP 不做,Phase 2 優化)
- 模式切換時舊 mesh 還在閃 1 幀;對策:雙 buffer + crossfade
- 色彩飽和度在 HDR 顯示器與 SDR 顯示器看起來不同;對策:Material 走 Emissive Color 不走 BaseColor(不受光照影響)
- D/C >> 2.0 時(嚴重超載)熱圖就「飽和到紅」分不出來;對策:加 `bShowExtremeValue` toggle 顯示具體數值

---

## C3 失敗與崩塌(FrameCore N4 → Chaos 剛體交棒)

### 概念
當結構嚴重超載或玩家主動「壓壞它」(觸發崩塌測試)時,系統不只顯示「失敗」紅字,而是**動態演出崩塌過程**:FrameCore N4 連續動力倒塌計算每個時間步的位移場 + 構件脫落事件,UE 端的 `AFrameDynCollapseReplayActor` 逐幀回放,`AFrameFragmentClusterActor` 把脫落的構件交給 UE Chaos 剛體物理做後續滾動 / 碰撞。這是 **「失敗即景觀」**(Failure as Spectacle)的核心實作(對應 Poly Bridge / KSP 教育研究的關鍵成功因子)。

職責切分(嚴守 PROJECT.txt day2 設計):
- **FrameCore 算**:何時斷、斷成幾塊、交棒時的線速度 V + 角速度 ω + 質心 + 慣量
- **UE Chaos 算**:斷後的剛體運動、碰撞、堆積

### 玩家視角
- 結構超載到某個門檻(由老師設定,或玩家主動按「測試崩塌」按鈕)
- 畫面震動 0.5 秒(警告)→ 倒塌動畫開始
- 構件依序失效(斷裂處紅色閃光 + 音效),整體下落
- 失效構件變成 Chaos 剛體,落地滾動、堆積
- 倒塌結束後,畫面定格在「廢墟狀態」,旁邊出現「診斷模式」按鈕

### 介面與操作規格
- 觸發條件:`OutResult.Utilization.MaxDC > 1.5` 自動觸發 OR 玩家手動按鈕
- `UFrameAnalysisLibrary::SolveDynCollapse(Def, DOpts)` 跑 N4 連續動力倒塌
- `AFrameDynCollapseReplayActor`:
  - `CollapseResult = Result; PlaybackSpeed = 1.0; bLoop = false`
  - `OnEventReached` 綁定 → 每個事件(構件移除/塑鉸形成/碎塊分離)觸發 UE 特效(`UNiagaraSystem` 粒子)+ 音效
  - `Play()` 後 Tick 自動推進
- `AFrameFragmentClusterActor`:
  - `CollapseResult = Result; ChunkMesh = StaticMesh'Concrete_Block_1m'`
  - `SpawnFragmentDebris()` 在每個 Cluster.COM 位置生成 AStaticMeshActor + `bSimulatePhysics=true` + 初速 LinearVelocity
  - `MaxDebrisActors = 1024`(v3.5 U-14 安全閥防止無限生成)
- 倒塌期間 FrameCore Solve 暫停(因為結構已不再合法),熱圖凍結在最後狀態

### FrameCore 接合
- `FFrameDynCollapseResult` 含 `Outcome`(成功/發散/最大時間), `Events[]`(每個時間點的失效事件), `Frames[]`(逐幀位移)
- `Events[i].Detached: TArray<FFrameFragmentCluster>` 每個 Cluster 含 COM, V, omega, Mass, Inertia → 直接餵 UE Chaos 剛體初值

### 教學落點
- **基礎工程力學(高二)**:`力04` 斷面尺寸合理性的後果體驗
- **結構耐震行為**:對應「強柱弱梁」哲學(W-06 警告)、地震連續倒塌的視覺化
- **Productive Failure(Kapur)**:失敗變成主要學習事件,而非懲罰
- **K3 Failure as Spectacle**:倒塌動畫做得有娛樂價值,鼓勵重試

### 完成標準
- [ ] D/C > 1.5 自動觸發崩塌動畫
- [ ] 玩家手動「測試崩塌」按鈕可主動觸發
- [ ] 倒塌動畫流暢(>30 fps, 5 秒模擬時間)
- [ ] 斷裂構件變 Chaos 剛體,正確落地堆積(不穿透地面)
- [ ] OnEventReached 觸發音效(每個事件至少 3 種音效:結構聲 / 落地聲 / 塵土聲)
- [ ] 倒塌後保留 30 秒檢視時間(讓學生觀察 - PF 強調的「失敗觀察」)

### 預期踩雷
- `SolveDynCollapse` 是 blocking 呼叫,可能阻塞 Game Thread 數秒;對策:Async Task,主執行緒顯示「計算中」UI
- Chaos 剛體大量(>100)會嚴重影響 FPS;對策:`MaxDebrisActors=1024` 上限 + LOD + 30 秒後自動 cleanup
- 倒塌動畫播放途中玩家想暫停 → `AFrameDynCollapseReplayActor::Pause()`(已實作,直接用)
- 多人模式下倒塌動畫不同步:用 Server 算完後廣播 `FFrameDynCollapseResult` JSON 摘要,所有 Client 各自 lerp Frames[];確保時間軸同步

---

## C4 連通性與載重路徑(Union-Find + BFS 錨定)

### 概念
這是 Block Reality 設計中**最被忽視但教育價值最高的視覺化**:讓學生看到「力如何從加載點傳遞到地基」。實作上是兩個東西:**Union-Find 維護結構分組**(哪些構件屬於同一個結構),**BFS 從地基往上追**(每個節點到地基的路徑)。如果結構漂浮(沒接到地基),整組顯示為灰色「未錨定」。

### 玩家視角
- HUD 上有「載重路徑工具」(L 鍵)
- 點擊一個節點 → 該節點往地基的路徑高亮顯示(連續構件以藍 → 綠漸層,深度越淺越藍 #56B4E9 → 越接近地基越綠 #009E73)
- 路徑寬度隨力大小變化:粗 = 大力、細 = 小力
- 孤立結構(沒連到地基)整組變灰色 + 顯示「未錨定」警告
- 切換到「漂浮偵測模式」:整個場景所有未錨定構件全部閃紅,讓學生一眼看到問題

### 介面與操作規格
- 新建 `UFrameStructureGroupSubsystem`(GameInstanceSubsystem)
- Union-Find 資料結構:
  - `TMap<int32 MemberIdx, int32 GroupId> MemberToGroup`
  - `TMap<int32 GroupId, FStructureGroupInfo> Groups`
  - 26-connectivity 偵測:每個構件 26 個方向鄰居檢查
- BFS 錨定:從每個 `Fixed=[true,true,true,true,true,true]` 節點(地基)往外走,標記可達構件為 `bAnchored=true`
- 載重路徑視覺化用 `AFrameInfluenceLineActor`(已實作,本來給影響線用),套到「從加載點往地基」反推路徑
- 重算時機:Place/Remove 構件後增量更新(dirty flag,如 Block Reality manual 決策 6)

### FrameCore 接合
- 不直接呼叫 FrameCore solver;但**錨定狀態決定 FrameCore prescribedDOF**:已錨定組的地基節點 `Fixed=[1,1,1,1,1,1]`,未錨定組整個跳過 solve(顯示「漂浮」)
- 載重路徑可選擇性用 FrameCore 算力流(從反力倒推),但簡化版只看拓樸連通

### 教學落點
- **構造與施工法**:對應構03「能識別建築平、立、剖面圖中的結構構件」
- **基礎工程力學**:**載重路徑是最直觀的「為什麼會垮」框架**(Block Reality 設計者語)
- **「漂浮會垮」**直覺:對應 Vygotsky ZPD 鷹架,學生第一次看到「我蓋的牆沒接到柱子,系統說它會掉」就建立空間直覺
- **設01「能運用尺度感」**:室內設計科適用

### 完成標準
- [ ] L 鍵啟動載重路徑工具,點選節點後路徑高亮 <100ms
- [ ] 26-connectivity Union-Find 100 構件規模 <50ms 重算
- [ ] BFS 錨定深度 <64(避免 stack overflow)
- [ ] 未錨定組正確標灰色 + 警告文字
- [ ] 移除一根構件後即時更新分組與錨定(增量更新,dirty flag)
- [ ] 漂浮偵測模式 1 秒內掃描全場標紅

### 預期踩雷
- 100% 連通的單一大型結構:Union-Find 每次改動仍 O(N) 全重算;對策:component size 上限 5000,超過 lazy mode
- BFS 在大型蜂巢結構深度爆炸:`anchor.bfs_max_depth = 64` 強制上限
- 多人同時改同一 component 的並發 dirty flag:`AtomicBoolean` 或 server-only 仲裁(MVP 走 server-only)
- Block Reality 的「Connectivity 死碼」歷史:那次失敗是「沒接上用途」;**本系統 connectivity 直接用於 BFS 錨定 + 載重路徑視覺化 + 倒塌交棒(C3),用途明確,避免重蹈覆轍**

---

# 第六章 — Part D:診斷工具(學習儀器)

## D1 應力掃描儀(熱圖模式 + 錨定模式)

### 概念
這是教育需求書 v3 中的**新支柱**:給學生一個**工具**,讓「看不見的結構行為」變得可見。對應 Block Reality 的應力掃描儀設計,但實作層改為 UE5 互動工具。掃描儀本身是一個玩家可裝備的 item,有兩個切換模式:**熱圖模式**(D/C 視覺化,沿用 C2)與**錨定模式**(載重路徑視覺化,沿用 C4)。

### 玩家視角
- HUD 左下角顯示「掃描儀」item icon,按 Q 鍵裝備/收起
- 裝備時相機微距變化(類似 FPS 武器舉起感),畫面邊緣加白色框
- 左鍵點擊構件 → 觸發「掃描」動作:該構件高亮閃光 + 旁邊浮空 UI 顯示細節(材料、斷面、當前內力、D/C)
- 右鍵切換模式(熱圖 ↔ 錨定),HUD 角落顯示當前模式
- Shift+左鍵 = 「持續掃描」模式,游標所到構件即時顯示資訊面板
- 模式狀態存在玩家 SaveGame(跨關卡記住偏好)

### 介面與操作規格
- 新建 Actor `AArchSimScannerTool`,類似 FPS 持有物(camera-attached)
- 模式 enum:`EScannerMode { Heatmap, Anchor }`
- 浮空資訊面板用 UMG `UMemberInfoPanel`,自動跟隨選中構件 attached(billboard 永遠面向相機)
- 顯示欄位:構件 ID / 類型(柱/梁/板)/ 材料(S355 / C30 / ...)/ 斷面(尺寸或型號)/ 內力(壓拉/彎矩/剪力)/ D/C / 錨定狀態(綠勾或紅叉)
- 切換熱圖 ↔ 錨定時呼叫 `AFrameUtilizationHeatmapActor::SetActive(true/false)` 與 `UFrameStructureGroupSubsystem::ShowAnchorPaths(true/false)`

### FrameCore 接合
- 掃描儀讀 `UArchSimMemberData::CachedUtilization`(已從 Solve 結果分發)
- 內力數值從 `FFrameMemberInternalForces`(在 `FFrameSolveResult.MemberForces[memberIdx]`)讀取 EndI/EndJ 的 N/Vy/Vz/T/My/Mz
- 不額外觸發 Solve,純粹是 result viewer

### 教學落點
- **基礎工程力學**:力02「能繪製剪力圖彎矩圖」對應內力顯示
- **直覺優先**:掃描儀是讓學生「主動探索」的工具,符合 PhET 隱性鷹架原則
- **個性化探索**:不同學生會在不同構件停留,Log 系統(Part I)記錄掃描模式使用情況可作為老師端教學診斷依據

### 完成標準
- [ ] Q 鍵裝備/收起切換正常
- [ ] 左鍵掃描構件顯示完整資訊面板(7 個欄位)
- [ ] Shift+左鍵持續掃描,60 fps 跟隨游標
- [ ] 模式切換無視覺殘留
- [ ] 跨關卡 SaveGame 記住模式偏好
- [ ] 多人模式下每位玩家獨立掃描器狀態(不被其他人影響)

### 預期踩雷
- 掃描儀 UI 在小螢幕(1366×768 校園電腦常見解析度)會擠;對策:UMG `WidgetSwitcher` 動態縮放
- 持續掃描模式對效能有影響(每幀重新計算射線交點 + 更新 UMG);對策:60 fps 限制,移動 <1 像素時不重算
- 多人模式下若 Server Solve 結果還沒到,Client 顯示舊資料;對策:資訊面板顯示「資料更新中」spinner

---

## D2 變形動畫(位移放大 + 模態振型)

### 概念
結構在載重下的**位移**通常是次毫米級(肉眼看不見),需要放大才能教學。本節介紹兩個動畫:**靜態變形動畫**(位移按 ×100 放大,可調)與**模態振型動畫**(顯示前 3 模態的振動形狀,自動播放)。

### 玩家視角
- HUD 右側「分析面板」有 toggle:「靜態變形」「模態 1」「模態 2」「模態 3」「反應譜峰值」「即時動態」
- 切換到變形模式 → 結構慢慢「彎下去」(從 ×1 到 ×100 線性插值 2 秒),停留在放大狀態
- 切換到模態 1 → 結構依該模態頻率振動(可見振動),可滑桿調速度
- 點 Play/Pause 控制動畫
- 滑桿調 DeflectionScale(1×~500×)

### 介面與操作規格
- 變形動畫:`AFrameDeformedShapeActor`(已實作)
  - `Solution = FrameSolveResult; DeflectionScale = 100.0(預設); bUseHermiteInterpolation = true(三次 Hermite 內插)`
  - `BuildMesh()` 重建 PMC
- 模態動畫:`AFrameModalShapeActor`(已實作)
  - `Modes = FrameModalResult; ModeIndex = 0; Amplitude = 100; TimeScale = 1.0`
  - Tick 自動驅動 `Amplitude * cos(2π * FreqHz * t)`
- 反應譜:`AFrameResponseSpectrumActor`(已實作),`EnvelopeHz = 0.5` 包絡顯示
- 即時動態:`AFrameRealTimeDynamicActor`(已實作),Newmark 步驟時程
- HUD 同步顯示:當前模式名稱 / 頻率(Hz)/ 模態週期(s)

### FrameCore 接合
- 靜態變形:讀 `FFrameSolveResult.Displacements[]`
- 模態:呼叫 `UFrameAnalysisLibrary::AnalysisModal(Def, Opts, ModalOpts)`,`NumModes=3`
- 反應譜:`UFrameAnalysisLibrary::ResponseSpectrum(Def, Opts, ModalOpts, Spectrum, ExcDof=2, EFrameSpectrumCombo::SRSS, Zeta=0.05)`
- 即時動態:`UFrameAnalysisLibrary::RealTimeDynamic(Def, Opts, ModalOpts, DynOpts)`

### 教學落點
- **基礎工程力學(高二)**:對應「應力應變」「樑的彎曲撓度」核心單元
- **動力學概念**:雖然高職不深入動力,但**看到一棟樓在地震中如何搖晃**對學生衝擊極大(對應「結構耐震」直覺)
- **W-04 撓度超限警告**:當 `撓度 > L/250` 時自動觸發警告(L = 跨距)

### 完成標準
- [ ] 5 種動畫模式(變形/模態1/模態2/模態3/反應譜/即時動態)切換無錯
- [ ] DeflectionScale 滑桿 1× ~ 500× 平滑變化
- [ ] 模態動畫頻率與 `FrameModalResult.Modes[].FreqHz` 一致
- [ ] CurrentPhase 在 ~193 天後不會出現精度懸崖(v3.5 U-13 已修)
- [ ] 反應譜包絡 0.5 Hz 脈動可見

### 預期踩雷
- DeflectionScale 過大時構件可能穿透地面 / 其他構件;對策:預設 100,警告超過 1000 時顯示「視覺示意,非真實位移」
- 模態動畫長時間播放(>1 小時)精度懸崖:v3.5 U-13 已修(`FMath::Fmod(1/FreqHz)`)
- 反應譜 EnvelopeHz 0.5 不一定符合所有結構自然頻率;對策:設成 UPROPERTY 可調

---

## D3 BMD/SFD 沿桿圖(彎矩剪力分布)

### 概念
**剪力圖(Shear Force Diagram, SFD)** 與 **彎矩圖(Bending Moment Diagram, BMD)** 是結構力學教學的「核心兩張圖」。本節讓玩家點選一根梁,旁邊立刻畫出該梁的 BMD/SFD,並可與課本對照。

### 玩家視角
- 點選一根梁(或用掃描儀模式)→ HUD 旁邊滑出「沿桿分析」面板
- 面板顯示三張小圖:**SFD(剪力)、BMD(彎矩)、軸力**
- 滑鼠在圖上拖拉 → 對應到梁上的位置高亮(綠 #009E73 點)
- 圖上標示最大值 + 位置(M_max = 25.6 kN·m at x = L/2)

### 介面與操作規格
- 新建 UMG `UMemberDiagramPanel`,寬度 400px,顯示三張子圖(各 400×120)
- 子圖用 `UCanvasPanel` + `UImage` + `UCurve`(自製繪圖元件)
- 沿桿取樣:根據 FrameCore 提供的 `FFrameMemberInternalForces.EndI / EndJ` 端部內力,**沿梁長度線性內插**(線彈性簡支梁可如此;UDL 拋物線需更密取樣)
- MVP 取 11 個取樣點(對應 FrameCore 慣例),Phase 2 加密
- 圖中標示:最大 Mz / Vy 位置 + 數值,單位 kN·m / kN
- 點擊圖上任一位置 → 高亮梁上對應實體位置(綠點 + 軸距標籤)

### FrameCore 接合
- 從 `FFrameSolveResult.MemberForces[memberIdx]` 讀取 EndI(N, Vy, Vz, T, My, Mz)與 EndJ
- 11 取樣點線性內插(MVP);Phase 2 改 FrameCore 沿桿取樣 API(對應 C6 工程化路線圖)
- 壓力為正(`FFrameMemberEndForces::N`),圖中標示

### 教學落點
- **基礎工程力學**:對應力02「能繪製簡支梁的剪力圖和彎矩圖」(統測高頻考點)
- **靜定/靜不定區分**:靜定結構的 BMD 可手算驗證,靜不定結構手算困難,讓學生體驗「為什麼要用電腦」
- **設計觀察**:看到 BMD 最大彎矩位置 → 知道哪裡需要加強鋼筋

### 完成標準
- [ ] 點選一根梁,BMD/SFD/軸力 三張圖在 100ms 內生成
- [ ] 最大值標示正確(對標簡支梁 ML²/8 等手算公式)
- [ ] 沿圖拖拉與梁上位置同步高亮
- [ ] 單位切換(N → kN,N·mm → kN·m)
- [ ] 跨距 L 變化後圖自動重繪

### 預期踩雷
- 11 取樣點對 UDL 拋物線可能精度不夠;對策:Phase 2 接 FrameCore 沿桿取樣 API(roadmap C6)
- 圖縱軸自動 scale 容易跳動;對策:固定縱軸 = `max(abs(max), abs(min)) * 1.2`
- 軸力(N)壓拉慣例與規範不同;明確標示「壓力 → 正值」

---

## D4 安全係數 + 條件數預警

### 概念
比單純 D/C 更細緻的指標:**安全係數**(SF = 1 / max(D/C),離垮還有多遠)+ **臨界餘裕**(來自 LDLᵀ pivot 比值,作為「快垮了」的連續預警)。本節是 Part D 進階,MVP 不必做,但放在計畫書讓 Phase 2 有依據。

### 玩家視角
- HUD 中央上方有「安全係數儀表」(類似汽車儀表)
- 顯示當前 SF(綠色 SF>2.0,黃色 1.5~2.0,紅色 <1.5)
- 旁邊有「臨界餘裕」(pivot margin)指示燈:0 = 機構/不穩定,>0 = 安全
- 載重逐步上升(如 1.0L → 1.2L → 1.4L)時 SF 動態變化,讓學生「感受到」載重越大、SF 越小

### 介面與操作規格
- SF 從 `FFrameSolveResult.Utilization.SafetyFactor` 直接讀
- Pivot margin 從 `FFrameSolveResult.PivotMargin` 讀(已實作,N4 倒塌驅動器用)
- 儀表用 UMG `UProgressBar` + 自製刻度
- 數值跳動採用 lerp(`InterpToConstant`,每秒最多變化 0.5),避免「閃爍」

### FrameCore 接合
直接讀 result,不需額外 Solve。

### 教學落點
- **基礎工程力學**:對應力03「能理解容許應力設計之安全係數意涵」
- **「快垮了」的連續預警**:Block Reality 設計者強調的非二元結果(穩/不穩),而是有預警
- **W-09 整體穩定性警告**:`PivotMargin < epsilon` 時觸發

### 完成標準
- [ ] 儀表顯示 SF,綠/黃/紅三段顏色正確
- [ ] Pivot margin <0 時整體警告(畫面紅邊框)
- [ ] 載重逐步增加時 SF 平滑變化,無跳動
- [ ] 單位明確(SF 無量綱)

### 預期踩雷
- SF 在不同 D/C 計算法(ASD 容許應力 vs LRFD 極限狀態)下意義不同;明確標示「彈性容許應力法」
- Pivot margin 是引擎內部數值,數量級對玩家無意義;只用「綠燈/紅燈」二元呈現,不顯示數字

---

# 第七章 — Part E:施工工序

## E1 工序狀態機(配筋→澆置→養護→拆模)

### 概念
從 Block Reality manual 直接搬到 UE 的核心教育機制:**蓋一個 RC 構件不是「放方塊」,是有先後順序的施工序列**。順序錯了結果就錯。讓玩家親身經歷:綁鋼筋 → 立模板 → 澆置混凝土 → 養護(等待強度發展)→ 拆模 → 完工。這把「會建模」升級為「會蓋」,直接對應「營建技術實習」「施工圖實習」課程。

### 玩家視角
- 在沙盒模式或關卡模式選擇「施工模式」(預設關)
- 構件不是直接「放下就完成」,而是按以下序列推進:
  1. **REBAR_PLACING**(綁鋼筋):放置鋼筋網,鋼筋間距檢查
  2. **CONCRETE_CASTING**(立模板 + 澆置):立模板 + 澆置混凝土,15% 機率生成蜂窩弱點
  3. **CURING**(養護):等待 2 分鐘遊戲時間(可加速),強度從 30% 線性增長到 100%
  4. **FORMWORK_REMOVAL**(拆模):拆掉模板,構件「正式啟用」
- 養護期間構件呈半透明灰色,旁邊浮空「養護中... 剩 1:23」
- 蜂窩弱點以裂縫 decal 視覺呈現(玩家可用掃描儀「發現」)
- 強行在養護完成前加載 → D/C 立即超標 → 結構失效(Productive Failure)

### 介面與操作規格
- 新建 ActorComponent `UFrameConstructionStateMachine`,掛在每個 RC 構件上
- 狀態 enum:`EConstructionPhase { Empty, RebarPlacing, ConcreteCasting, Curing, FormworkRemoval, Done }`
- 狀態轉換需玩家手動觸發(按按鈕),不自動進階
- 養護計時:`FTimerHandle CureTimer`,2400 tick = 2 分鐘(可在 Settings 加速為 30 秒測試)
- 蜂窩生成:`if (FMath::FRand() < HoneycombProb) { ... }`,亂數種子可設為 `UPROPERTY(SaveGame)` 確保 reproducible
- 視覺呈現:UE Material 切換(模板 → 灰色混凝土 → 養護中半透明 → 完成正常材質)

### FrameCore 接合
- **狀態切換時呼叫 `UFrameInteractiveSubsystem::ApplyPatchAndResolve(Patch)` 更新材料強度**:
  - `Empty`: 構件 `bActive=false`(不存在於模型中)
  - `RebarPlacing`: 只有鋼筋(`Fy=420 MPa`)但無混凝土
  - `CONCRETE_CASTING`: 添加混凝土材料(`f'c=21 MPa, E_eff=15000 MPa` 初始值)
  - `CURING`: 每隔 30 秒(遊戲時間)呼叫 patch,線性增長 `E_eff: 15000 → 30000 MPa, f'c: 21 → 70%~100%`
  - `Done`: 完整強度
- 蜂窩弱點:`E_eff *= 0.6` 同時加 `bHasDefect=true`

### 教學落點
- **構造與施工法(高一-高二必修)**:對應構02「能理解施工序列對結構完整性的影響」
- **營建技術實習(高三必修)**:整套 RC 工序的視覺化前奏
- **學習歷程**:Productive Failure 完美應用 — 學生強行跳過養護必失敗,學到「等待是工程的一部分」

### 完成標準
- [ ] 4 階段狀態機可依序推進,逆向則拒絕
- [ ] 養護計時可加速(Settings 提供 1×/10×/100× toggle)
- [ ] 蜂窩弱點 15% 機率,正確視覺呈現裂縫 decal
- [ ] 養護完成後構件正式啟用,FrameCore Solve 包含該構件
- [ ] 跳過養護加載 → D/C 超標 → 觸發崩塌動畫(對應 C3)
- [ ] 多人模式下狀態同步(透過 ReplicatedUsing OnRep)

### 預期踩雷
- 養護計時與遊戲時間(可暫停)同步:用 UE `FTimerManager` 而非 wall-clock
- 多人模式下狀態切換競態:Server-authoritative,Client 只請求轉換
- 蜂窩生成亂數種子重要(reproducibility);存 `FRandomStream UPROPERTY(SaveGame)`
- 玩家覺得養護 2 分鐘太久:預設提供「跳過養護(學習模式)」 toggle,正式模式要等

---

## E2 工法品質(鋼筋間距 / 蜂窩 / 養護完整性)

### 概念
RC 工法的「品質因子」是真實工地的關鍵教學內容。本節定義三個品質檢查:**鋼筋間距(spacing)、蜂窩弱點(honeycomb)、養護完整性(curing duration)**,每個都對應一個材料強度折減係數,直接影響 FrameCore 計算結果。

### 玩家視角
- 構件處於 `REBAR_PLACING` 狀態時,玩家放鋼筋網,**間距太大會被警告**:「鋼筋間距 = 350mm,規範建議 ≤ 200mm,目前折減 70%」
- 蜂窩弱點以裂縫 decal 隨機生成,**玩家可用掃描儀偵測**:點擊有蜂窩的構件 → 顯示「檢測到混凝土澆置缺陷,強度折減 40%」
- 養護期間若強行使用 → 強度 < 100% 進入 FrameCore solve,結構行為對應變弱
- HUD 右上角「工法品質」儀表板:綠 / 黃 / 紅 三色顯示當前構件品質等級

### 介面與操作規格
- 鋼筋間距檢查:`UArchSimRebarChecker::CheckSpacing(SpacingMM)` → 返回折減係數 1.0 / 0.85 / 0.70 / 0.50
- 蜂窩生成位置記錄:`FFrameWeaknessRecord { MemberIdx, WeaknessFactor=0.6, Location=FVector }`
- 養護完整性:`E_eff(t) = E_full * (0.3 + 0.7 * (t / cureTime))`,線性增長
- 警告訊息:對應 W-XX 警告系統(見 Part J),透過 `UArchSimWarningSubsystem` 發送

### FrameCore 接合
所有品質因子最終都轉化為材料強度折減,透過 `FFrameModelPatch::SetNodalLoads / UFrameInteractiveSubsystem::ApplyPatchAndResolve`:
- 鋼筋間距 350mm → 等效鋼筋面積折減 70% → `Fy_eff = Fy * 0.7`
- 蜂窩 → `E_eff *= 0.6, Fc_eff *= 0.5`
- 養護未完成 → 動態 lerp

### 教學落點
- **營建技術實習 + 構造與施工法**:工地真實品質管控的視覺化
- **規範對標**:鋼筋間距規範參考 ACI 318-19 / 台灣 RC 規範(混凝土結構設計規範 112 版)
- **「為什麼工地要監造」**直覺:學生理解「同一份設計,施工差 → 結構安全度大降」

### 完成標準
- [ ] 鋼筋間距 4 級檢查(200 / 250 / 300 / 350mm)正確折減
- [ ] 蜂窩 15% 機率生成,折減 60%
- [ ] 養護完整性線性增長公式正確,30% 起始,100% 終點
- [ ] 工法品質儀表板即時顯示
- [ ] 掃描儀可偵測蜂窩位置與折減量

### 預期踩雷
- 鋼筋間距規範細節在實務上更複雜(不只「越密越強」,還有最小淨距、保護層等);**MVP 簡化為單一間距檢查**,明確標示「教育用簡化版」
- 蜂窩位置不固定 → 多次 Save/Load 後可能變動;對策:存 `FRandomStream`
- 養護期間 FrameCore 反覆 re-solve(每 30 秒)可能造成熱圖閃爍;對策:Solve 結果 lerp 過渡 2 秒

---

## E3 引導式施工(全息投影預覽)

### 概念
對應 Block Reality 的「Litematica 全息投影」概念。**老師可在關卡中放置「目標藍圖」**(教師預先設計的標準解答),學生看到半透明的 ghost 預覽,跟著放置即可學習正確的施工步驟。對應 PhET 的「隱性鷹架」與 Vygotsky ZPD scaffolding 漸退原則。

### 玩家視角
- 引導關卡進入時,場景中有一個半透明藍色 ghost 結構(目標形狀)
- 玩家把實體構件放到 ghost 對應位置 → 該位置 ghost 變綠 + 播放正面音效
- 放錯位置 → 該構件不消失但不被認可,HUD 提示「該位置不是設計目標,要繼續嗎?」
- 引導關卡可選「自由模式」(關閉 ghost,完全靠直覺)或「Tutorial 模式」(完整 ghost)
- 老師可在多人模式下臨時開啟 ghost(緊急救援卡關學生)

### 介面與操作規格
- Ghost 預覽用 Prefabricator 的 prefab ghost(複用 B1 構件放置 ghost 機制)
- Ghost 材質:`MI_GhostPreview` 半透明 + 邊緣亮藍色(對應 Litematica 視覺風格)
- 進度追蹤:`UArchSimTutorialProgress`(ActorComponent on level),維護「目標構件 vs 已放置構件」對應 TMap
- 完成度 0-100%:每放對一個構件 +1,顯示在 HUD 右下角
- 老師端「啟用 / 關閉 ghost」按鈕,廣播給所有 client

### FrameCore 接合
Ghost 不接 FrameCore(它只是視覺指引);**完成度 100% 時觸發一次完整 Solve**,讓學生看到「完工後熱圖」。

### 教學落點
- **設計與技術實習**:對應教師預先設計典範作品供學生臨摹
- **漸退式鷹架**(Vygotsky):Tutorial 關卡用完整 ghost,後期關卡逐步減少 ghost 提示
- **K2 設計-反饋-設計迴圈**:玩家在 ghost 引導下蓋,看到熱圖,再去自由模式自己蓋

### 完成標準
- [ ] 老師可在關卡編輯時放置「ghost 藍圖」
- [ ] 學生模式下 ghost 半透明可見
- [ ] 構件放對位置時 ghost 變綠,放錯不變
- [ ] 完成度即時更新
- [ ] Tutorial / 自由模式 toggle
- [ ] 多人下老師可隨時啟用 / 關閉 ghost

### 預期踩雷
- Ghost 透明度與場景光源衝突(過暗 / 過亮);對策:Material 用 unlit + 固定 emissive
- 學生覺得「ghost 模式像照本宣科,無趣」;對策:ghost 模式只是 Tutorial 用,Phase 2+ 自由模式為主
- 「放對位置」判定容差:預設 `< 100mm 距離 + 角度差 < 15°` 算對

---

## E4 材料狀態機(素材 → 鋼筋 → 澆置 → 複合)

### 概念
Block Reality 設計者特別強調的概念:**同一個構件會隨施工「轉變身份」**,從素材 → 配筋(只有鋼筋)→ 澆置(混凝土包裹鋼筋)→ 養護完成(RC 複合節點)。每階段材料屬性不同,FrameCore 計算結果也不同。

### 玩家視角
- 與 E1 工序狀態機緊密整合,但聚焦在「材料屬性如何變化」
- 玩家在 RC 構件上能感受到:
  - 只有鋼筋階段:超細長、超撓(學生視覺看到「鋼筋網沒澆混凝土很軟」)
  - 澆置初期:剛變硬但脆(早齡期 Fc 低)
  - 養護完成:正式 RC 強度(混凝土抗壓 + 鋼筋抗拉合力)
- 構件視覺隨階段變化:鋼筋網 → 加模板 → 灰色混凝土塊 → 完工材質

### 介面與操作規格
- 材料狀態 enum:`EMaterialState { RebarOnly, GreenConcrete, Cured, Composite }`
- 每個狀態對應一組 `FFrameMaterial` 參數:
  - `RebarOnly`: E=200 GPa, A_eff = rebar 面積(很小)
  - `GreenConcrete`: E=15 GPa, Fc=21 MPa, 早齡期
  - `Cured`: E=24 GPa, Fc=24 MPa, 完整混凝土
  - `Composite`: E_eff(透過 RC 融合公式), Fy=420 MPa(鋼筋抗拉貢獻)
- 視覺切換用 UE Material 切換(預先做好 4 個 MaterialInstance)
- RC 融合公式在 `UFrameRCMaterialHelper`(見決策 10)

### FrameCore 接合
每次狀態切換呼叫 `UFrameInteractiveSubsystem::ApplyPatchAndResolve` 更新材料 → 重解。**FrameCore engine 不知道材料正在切換,它只看到「材料參數改變」**(嚴守鐵則 #1)。

### 教學落點
- **材料與試驗(高一 4 學分)**:對應理解材料屬性如何影響結構
- **RC 複合材料概念**:對應 Block Reality 教學重點(混凝土抗壓 + 鋼筋抗拉 = 強過任何單一材料)
- **直覺破除迷思**:學生原本以為「混凝土很強」,看到只有混凝土時抗拉斷掉、配上鋼筋後就強很多,完成「為什麼 RC 是工程主流」的直覺建立

### 完成標準
- [ ] 4 個材料狀態正確切換,視覺與材料參數同步
- [ ] 各狀態下 FrameCore 結果合理(只有鋼筋時極撓,Composite 時最強)
- [ ] RC 融合公式 `comp_boost=1.1 / φ_tens=0.8 / φ_shear=0.6` 在 UFrameRCMaterialHelper 內實作
- [ ] 學生可在掃描儀中看到「當前材料狀態 + 等效參數」
- [ ] 蜂窩弱點(E2)與本層整合(蜂窩 = Composite 狀態下加 0.6 折減)

### 預期踩雷
- RC 融合公式只是教育簡化(實際 ACI 318 規範遠複雜);明確標示「教育用簡化模型,不可移植真實工程」
- 玩家可能誤以為「只放鋼筋就有 RC 強度」;對策:HUD 明確標示當前材料狀態(視覺+文字)
- 多人下狀態同步:每個構件的 `EMaterialState` 走 `UPROPERTY(Replicated)`

---

# 第八章 — Part F:沙盒 + 關卡

## F1 沙盒模式定義(自由設計、無評分)

### 概念
沙盒模式是「無壓力創作場域」,讓玩家在沒有目標、沒有時間限制、沒有評分的環境中**自由建造任何結構**,系統不打斷、不警告(除了致命錯誤如崩塌),log 系統照常記錄但只用於老師回顧。對應 Papert constructionism「製作有意義人工物件」的核心精神。

### 玩家視角
- 進入沙盒模式 → 選擇基地模板(平地 / 緩坡 / 山坡 / 既有建築旁,5 種)
- 構件庫完全開放,所有材料、斷面、藍圖均可用
- 沒有時間限制、沒有計分
- 可隨時切換熱圖 / 變形 / 模態 / 倒塌測試
- 完成作品可儲存 + 分享(`.archsim_bp` 檔)
- 可邀請組員協作(對應 Part H 多人模式)

### 介面與操作規格
- 主選單「沙盒模式」按鈕進入
- 基地選擇 UMG widget,5 個縮圖選項
- 沒有 HUD「目標」面板,只有「建造工具」 + 「分析工具」 + 「分享」 + 「離開」
- 自動 SaveGame:每 60 秒背景儲存,離開時最後一次存

### FrameCore 接合
與關卡模式無差別,都呼叫同一套 `UFrameInteractiveSubsystem::ApplyPatchAndResolve`。

### 教學落點
- **Papert 建構主義**:作品的個人意義與可分享性
- **Csíkszentmihályi Flow**:無壓力環境讓學生「玩進去」,沒有外在動機壓力
- **設計與技術實習(高二)**:對應創作自由發揮
- **跨課程作品**:沙盒作品可作為「專題實作(高三 8 學分)」前置嘗試

### 完成標準
- [ ] 5 種基地模板可選
- [ ] 沙盒進入後完全自由,無 UI 干擾
- [ ] 自動 SaveGame 60 秒間隔
- [ ] 作品可匯出為 `.archsim_bp`
- [ ] 多人沙盒(Part H)整合

### 預期踩雷
- 玩家可能蓋出超巨大結構(1000+ 構件),Solve 時間爆;對策:Server-side 構件數上限(預設 500,老師可調)
- 沙盒模式無評分但有 log;**必須明確告知學生**(對應個資法 / 教育倫理):log 用於老師回顧,可申請刪除

---

## F2 關卡系統(SUQS DataTable + 漸進難度)

### 概念
關卡是有目標、有約束的引導性場景,讓學生在受控環境中學習特定概念。每個關卡有:**起始狀態 / 目標 / 約束 / 評分**。對應 Csíkszentmihályi Flow 的「清楚目標 + 即時回饋」與 Kapur Productive Failure 的「結構化失敗」。

### 玩家視角
- 主選單「關卡」按鈕 → 關卡選擇地圖(類似 Candy Crush 風格,線性解鎖)
- 每關有圖示 + 中文名稱 + 難度星星 + 三個獎章(完成 / 完美 / 速通)
- 進入關卡前看簡介:目標 / 約束 / 推薦工具
- 關卡開始 → HUD 顯示目標進度,即時更新
- 完成 → 結算頁:獎章 + 學習主題回顧 + 解鎖下一關

### 介面與操作規格
- SUQS DataTable 定義關卡結構:`DT_ArchSimLevels`,每 row = 1 個關卡
- Quest JSON 範例(以 SUQS 格式):
  ```json
  {
    "Identifier": "Q_L01_CantileverBasics",
    "Title": "懸臂梁基礎",
    "Description": "蓋一根 6m 跨距的懸臂梁,撓度不超過 30mm",
    "Objectives": [
      { "Identifier": "OBJ_Build", "Tasks": [
          { "Identifier": "TASK_PlaceMember", "Title": "放置一根梁" }
      ]},
      { "Identifier": "OBJ_LoadTest", "Tasks": [
          { "Identifier": "TASK_ApplyLoad", "Title": "施加 5 kN 端點荷載" },
          { "Identifier": "TASK_DeflectionGoal", "Title": "端點撓度 < 30mm",
            "TargetCount": 1 }
      ]}
    ]
  }
  ```
- 業務層條件判斷:`OnSolveComplete(FrameSolveResult)` → 取 `Displacements.Last().UZ` 判斷是否 < 30mm → `CompleteTask`(見決策 7)
- 關卡進度存 SUQS `USuqsProgression`,Save 到 SPUD

### FrameCore 接合
關卡完成判定依賴 FrameCore 結果(撓度、D/C、SF 等);在業務層判斷而非 SUQS JSON 直接寫條件(SUQS 不支援數值比較)。

### 教學落點
- **基礎工程力學**:每關對應特定力學概念
- **完整關卡清單**見附錄 E(15+ 主線關卡 + 5+ 測量關卡)
- **K2 設計-反饋-設計迴圈**:每關設計成可多次失敗、修正,符合 Kapur PF

### 完成標準
- [ ] SUQS DataTable 正確 import
- [ ] 關卡選擇地圖 UI 完整
- [ ] 數值條件正確判斷(撓度 / D/C / SF)
- [ ] 完成後解鎖下一關
- [ ] 結算頁顯示學習主題回顧(對應課綱單元連結)

### 預期踩雷
- SUQS quest JSON schema 變動 BREAKING(跨 UE 版本);版控 + reimport 測試
- 多人下任務進度不同步(SUQS 無 built-in replication):自製方案存 PlayerState replicated UPROPERTY
- 學生卡關時氣餒;對策:卡 3 次後自動提供「鷹架提示」(對應 Vygotsky scaffolding)

---

## F3 學習閉環整合(設計 → 施工 → 加載 → 崩塌 → 診斷 → 改)

### 概念
整個系統的骨架,所有 12 個 Part 都掛在這條閉環上。本節定義閉環如何在單一關卡中**完整跑一遍**,讓學生在 5-15 分鐘的關卡時間內經歷完整循環,而非碎片化學習。

### 玩家視角(以「L-03 簡單門型框架」為例)
1. **設計階段**(2 分鐘):看到任務「設計一個跨距 6m 的門型框架,承受 5 kN 風載」,玩家自由放置兩根柱 + 一根梁
2. **施工階段**(1 分鐘):進入 Construction 模式,依序綁鋼筋 → 澆置 → 養護(加速 10×)→ 拆模
3. **加載階段**(30 秒):施加風載荷,FrameCore Solve
4. **崩塌或通過**(視結果):
   - 若 D/C < 1.0 → HUD 顯示「通過!」,獎章解鎖
   - 若 D/C > 1.0 → 倒塌動畫播放(C3)+ HUD 顯示「失敗,結構超載」
5. **診斷階段**(2-5 分鐘):
   - 失敗者:用掃描儀觀察哪些構件最先壞、為什麼,看載重路徑
   - 通過者:看 BMD / SFD / 應力分布,加深理解
6. **改設計階段**(1-3 分鐘):回到設計階段,根據診斷做修改,重新跑閉環

### 介面與操作規格
- 關卡 HUD 上方顯示「閉環進度」:6 個圓圈標示當前階段(設計/施工/加載/崩塌/診斷/改)
- 每階段切換時動畫(放大當前圓圈)
- 「改」階段可選擇:回到設計階段(重做)或結束關卡(放棄/接受結果)

### FrameCore 接合
**Solve 在加載階段觸發,結果驅動崩塌或通過判斷**。診斷階段不額外 Solve,純粹是 result viewer(D1/D2/D3)。

### 教學落點
**這是整個教育設計的最終匯流**:
- 設計 → 對應「設計與技術實習」
- 施工 → 對應「構造與施工法 + 營建技術實習」
- 加載 → 對應「基礎工程力學 + 載重組合概念」
- 崩塌 → 對應「Productive Failure 學習機制」
- 診斷 → 對應「PhET 隱性鷹架 + 應力掃描儀」
- 改 → 對應「設計迭代思維」

### 完成標準
- [ ] 至少 3 個關卡(L-01, L-02, L-03)完整實作學習閉環
- [ ] 閉環進度 HUD 直觀顯示
- [ ] 失敗 → 診斷 → 改設計流程順暢(不卡頓、不混淆)
- [ ] 通過者也能進入診斷模式深入學習
- [ ] 完整流程 5-15 分鐘可完成(MVP 第一關 5 分鐘為目標)

### 預期踩雷
- 學生可能跳過「診斷階段」直接改設計;對策:強制查看 1 個構件詳細資料才能進「改」
- 多次重試後玩家失去耐心;對策:第 3 次失敗開始提供漸退式鷹架提示
- 「改」階段保留前次設計太久 → 玩家不想動;對策:第 2 次重試自動移除最差構件 1 根(顯示「給你個提示:這根可能有問題」)

---

## F4 失敗即學習機制(Productive Failure 設計)

### 概念
Kapur 2016 的 Productive Failure 理論直接落地:**先讓學生面對超出能力的問題,嘗試失敗,再進行正式教學**。本系統的設計關鍵是「失敗免費、即時、可重試、可視化值得觀看」。

### 玩家視角
- 失敗時不顯示「Game Over」紅字,而是顯示「啊,塌了!來看看哪裡有問題」(語氣親切)
- 倒塌動畫播完後自動進入「失敗分析模式」:
  - 紅色閃光標示最早失效構件
  - HUD 顯示「最危險構件:M-12, 模式:bending, D/C=1.42」
  - 推薦診斷工具按鈕(load path / heatmap / BMD)
- 旁邊顯示「最佳設計 vs 你的設計」對照(若已通關過,有解答)
- 鼓勵性訊息:「失敗是工程的一部分,真實工程師失敗 100 次才設計出 1 個成功作品」

### 介面與操作規格
- 失敗事件記錄到 xAPI:`verb=observed-failure, result.failure-mode=buckling / yielding / instability`
- 失敗分析模式 UMG `UFailureAnalysisWidget`,持續 30 秒可探索
- 「最佳設計對照」需老師預先提供(關卡 DataAsset 中可選填),MVP 預設無對照
- 觀察時間記錄:`result.extensions.observation-duration-sec`(若 <5 秒,可能沒真懂)

### FrameCore 接合
失敗判定來自 `FFrameSolveResult.bSingular = true` 或 `MaxDC > 1.5`。失敗模式分類來自 `FFrameSolveResult.Utilization.Mode`(bending / shear / buckling / instability)。

### 教學落點
- **Kapur Productive Failure(學理基礎在 Part J)**:160 個對照實驗驗證
- **遷移題表現**:PF 組在遷移題得分達傳統教學 3 倍
- **「失敗免費」優勢**:對比真實工地或建模考試,遊戲讓 PF 循環從一週壓縮到 3 分鐘

### 完成標準
- [ ] 失敗時無懲罰訊息(語氣親切)
- [ ] 自動進入失敗分析模式 30 秒
- [ ] 最早失效構件正確高亮(`OnEventReached` first event)
- [ ] 失敗模式正確分類(bending / shear / buckling / instability)
- [ ] 觀察時間記錄到 log
- [ ] 通關率隨關卡推進逐步提高(學習效果驗證)

### 預期踩雷
- 學生不看分析直接重試:對策:第 3 次失敗強制觀察 ≥5 秒才能重試
- 失敗模式分類不準確:依賴 FrameCore Utilization.Mode 識別,該欄位本身有侷限(目前是 max(N/Mz/My/V) 的 dominant 模式)
- 「最佳設計」對照可能讓學生抄答案:對策:對照只給結構拓樸示意圖,不給材料/斷面詳情

---

## F5 對標真實規範(教學分層 + 警告系統)

### 概念
完整對應 Block Reality manual 中的「對標真實工程標準(如 RC 的 ACI 318)」理念。將台灣建築技術規則 / 混凝土結構設計規範 112 版 / 鋼結構設計規範的核心概念用三層分層方式露給學生:**露出 / 黑盒 / 完全屏蔽**。詳細警告系統(W-01 ~ W-10)的中文範本見 spec research。

### 玩家視角
- 構件超載時不只顯示紅色,還顯示**警告對話框**:
  - 標題:「梁 B-12 彎矩需求超過承載能力(D/C = 1.34)」
  - 原因:「梁受到太大的彎矩 — 跨距過長或載重過重」
  - 後果:「此梁可能在正常使用時就開裂破壞」
  - 建議:「加大梁深 d、縮短跨距、或提高混凝土等級」
- 全部結構通過 → 顯示「設計優良!」正向回饋(W-10)
- 「規範對標」按鈕(進階):顯示當前設計對應台灣規範條文摘要

### 介面與操作規格
- 警告系統 Subsystem `UArchSimWarningSubsystem`
- 警告類型 enum:`EWarningType { CapacityExceeded, NearCapacity, DeflectionExceeded, StrongColumnWeakBeam, MaterialInappropriate, EarthquakeLoadCombo, GeometricInstability, DesignOptimal }`
- 警告 DataAsset `DA_WarningTemplates`,每個 entry 含:Title / Cause / Consequence / Recommendation(全中文)
- 觸發條件對應 FrameCore 結果欄位(見 spec research 7.3)
- 警告顯示為 toast notification(右下角彈出,5 秒消失)+ 完整內容可在「警告日誌」面板查看

### FrameCore 接合
警告判斷邏輯純在 UE 層,根據 FrameCore 提供的 `MemberUtilization` / `ShellUtilization` / `Displacements` / `bSingular` 等 result 欄位觸發。

### 教學落點
- **建築法規(校訂選修)+ 構造與施工法**:對標台灣建築技術規則
- **規範意識**:培養「設計要符合法規」的職業素養
- **學習表現**:對應力03「能理解容許應力設計之安全係數」、構01「能辨識不同結構系統之組構邏輯」

### 完成標準
- [ ] 10 種警告類型(W-01 ~ W-10)中文模板完整
- [ ] 警告觸發條件正確(對應 spec research 7.3 表)
- [ ] 「規範對標」按鈕進階模式顯示條文摘要
- [ ] 正向回饋(W-10)在無問題時顯示
- [ ] 警告日誌可回查歷史

### 預期踩雷
- 警告太多 → 玩家麻木;對策:嚴重度排序,只顯示前 3 個
- 規範細節在實務上更複雜(W-06 強柱弱梁的真實判定需要 NM 互動曲線等);明確標示「教育用簡化版」
- 跨規範系統(ACI 318 vs Eurocode 2)的選擇:MVP 鎖定 ACI 318(對應台灣規範基礎)

---

# 第九章 — Part G:測量關卡(獨立支線)

> **Part G 是獨立支線**,不接學習閉環,但對應「測量實習」課程(土木與建築群統測專業科目二必考)。可在任何 Phase 平行開發,甚至可獨立發布展示。
>
> **⚠️ 關鍵現況(2026-06-25 更新)**:**水準儀部分(LevelSim)已在 `ArchSim/Plugins/LevelSim/` 完整實作並 bundled 進 main**(v2.2+1 / `33cbd57` / 2026-06-18 PR#8 merge)。Standalone 115/115 PASS,CLAUDE.md 鐵則 #5 對 LevelSim 採跟 FrameCore 同等保護(`絕不碰 Plugins/LevelSim/`)。原計畫書估算 Part G 178 工時是錯誤的(我撰寫時漏看 LevelSim),**修正後實際工時約 125 小時**(節省 53h):G3 從 30h 降到 5h(僅 polish),G5 中 S-01~S-04 水準儀題型 LevelSim 已支援。**水準儀關卡可納入 MVP**(原排 Phase 2),經緯儀(G2)仍需從零做。
>
> **LevelSim 提供的能力**:
> - **`levelsim::LevelCore.h`**(純 C++17,14 個 API):`tiltFromScrews / bubbleFromTilt / sightTilt / measure(含地球曲率+大氣折光 opt-in) / trueThreeHair(三絲視距) / stadiaDistance / parallaxJitter / heightOfInstrument / pointElevation / closeLoop(多站閉合+平差) / recoverIAngleTwoPeg(兩樁法回推 i 角) / scoreReading`
> - **`ALevelSimPawn`**(UE5 體驗層):完整 FSM(Overview / Leveling / Telescope / Booking / RouteSummary)、三個 camera、Multi-station RoutePoints + FLegRecord、smoke test hooks
> - **`ALevelStaffActor`**(扶尺員/標尺)、`ALevelSimGameMode`、`ALevelSimHUD` 全套
> - **27 個對抗式審核發現已修正**(2026-06-08),含 critical bug 圓氣泡方向反置

## G1 隨機地形生成(UE5 PCG 確定性 seed + LevelSim RoutePoints 整合)

### 概念
每局練習地形不同(避免學生背題),但需要**確定性**(同一個 Seed 生成相同地形)以利老師批改與回放。對應 UE5.7 已 Production-Ready 的 PCG 框架。

### 玩家視角
- 進入測量關卡 → 顯示「本題 Seed: ABC123」(學生記下供老師驗證)
- 場景生成一塊 100m × 100m 的地形,起伏 ±2m
- 有 5-8 個可見地物標記:基準點 BM(綠色木樁)、測站候選位置(黃色小旗)、目標點(紅色木樁)
- 地形難度依關卡升級:平坦 → 緩坡 → 陡坡 → 含障礙物

### 介面與操作規格
- PCG Graph `PCG_SurveyTerrain`(**2026-06-25 S-00 spike 後修正**):
  - Surface Sampler → **`Spatial Noise`(Mode = `Perlin2D`)** — UE5.7 PCG 無獨立「Perlin Noise」節點,改用 `PCGSpatialNoise.h` 的 `PCGSpatialNoiseMode::Perlin2D` 選項(高度範圍 ±2m,可調)
  - **⚠️ 平地篩選**:UE5.7 PCG **無「Attribute By Slope」節點**(完整搜過 `Engine/Plugins/PCG` / `PCGInterops` / `Experimental/PCGBiomeCore` / `Experimental/PCGBiomeSample` 確認)。替代方案見下方 **Slope Filter 替代選項**
  - 基準點/測站/目標點隨機分布(但保證互相可見)
- Seed 鎖定:`PCG Settings.UseFixedSeed = true; FixedSeed = LevelSeed`
- 關卡 DataAsset 預定義 Seeds(老師可選擇 / 隨機)

#### Slope Filter 替代選項(S-00 spike #3 結果,需 architectural decision)

| 方案 | 工作量 | 優點 | 缺點 |
|---|---|---|---|
| (a) **Custom PCG Node** 用 C++ 寫 `UPCGSlopeFilterSettings` | ~8h | 跟 PCG graph 無縫整合;可重用 | 需懂 PCG API,UE5.7 學習成本 |
| (b) **GeometryScript** 算 face normal · Z 軸 → 篩選 sample 點 | ~5h | 不依賴 PCG,直接走 GeometryScript BP 可調 | 從 PCG graph 抽出,流程切兩段 |
| (c) **Landscape Spline** 預先在 editor 標示「可擺基準點區」 | ~3h | 最簡單,老師可手動畫 | 失去確定性隨機;每張地圖要手畫 |
| (d) **取消 slope filter**(隨機放,讓玩家自己挑) | 0h | 零工作 | 學生可能放在陡坡無法整平,失敗體驗差 |

**建議**:MVP 走 (c) Landscape Spline 老師手動標(3h);Phase 2 升級到 (a) Custom PCG Node 換成程序化。

### FrameCore / LevelSim 接合
- **不接 FrameCore Solve**(測量不用結構力學引擎)
- **接 LevelSim**:PCG 生地形後,把地形高程取樣轉成 `levelsim::Staff` 的 baseZ(物件鉛直高度),傳給 `ALevelSimPawn::InitRoute(RoutePoints)`。RoutePoints 是 LevelSim 已有的 multi-station API(`FRoutePoint{ WorldXm, WorldYm, WorldZm, Name }`),整合工作只是 PCG 採樣 → world Z → metres 換算

### 教學落點
- **測量實習(高一-高三必修 6 學分)**:對應實際工地多變地形練習
- **空間定位能力**:對應群科素養指標「測量繪製營造能力」

### 完成標準
- [ ] 5 種地形難度
- [ ] Seed 鎖定後可重現相同地形(±0 公差)
- [ ] 基準點/測站/目標點互相可見(視線無遮擋)
- [ ] 地形載入 <3 秒
- [ ] PCG 生成的 RoutePoints 可正確餵入 `ALevelSimPawn::InitRoute`

### 預期踩雷
- PCG 在 UE5.4-5.6 效能不穩;**鎖定 UE5.7+**
- Seed 顯示給學生但學生無法手動輸入(避免作弊改 seed);Seed 用於老師端對標,UI 上顯示但不可改
- LevelSim 的 staff/setup 座標系是「儀器在 (0,0,opticsHeightZ)」單站相對;PCG 生地形是 absolute world,**需在 `ALevelSimPawn::BuildRelativeTargets()` 之前完成座標換算**(LevelSim 已有此機制,只需正確餵 RoutePoints)

**工時估算**:~15h(PCG 整合 + RoutePoint 換算)

---

## G2 經緯儀操作(7 階段 18 步驟)— 從零實作

### 概念
完整模擬經緯儀(Theodolite)的標準操作流程,讓學生在虛擬環境練習真實儀器操作。對應 SurReal / VRISE 研究的數位測量教學模式(技能可遷移至實機)。

### 玩家視角(7 階段流程)
1. **架設三腳架**:展開腳架、目視概略對準測點
2. **概略定心**:安裝儀器、光學對點器確認測點
3. **概略定平**:調整腳架長度使圓盒水準氣泡居中
4. **精確定心**:鬆基座螺旋、平移儀器使測點影像落在光學對點器中心
5. **精確定平**:旋轉照準部 + 雙手反向旋轉腳螺旋,反覆迭代到任意方向氣泡居中
6. **照準目標**:對光調焦、粗瞄、精瞄
7. **讀數記錄**:讀水平度盤(HA) + 垂直度盤(VA),正倒鏡取平均

### 介面與操作規格
- 主 Actor `AArchSimTheodolite`,持有 SkeletalMesh + 旋轉狀態
- 操作介面:第一人稱視角,滑鼠拖拉旋轉儀器(水平 / 垂直分離),滾輪調焦
- 三個關鍵 mini UI:
  - 光學對點器:看到地面影像,中心十字標
  - 圓盒水準器:看到氣泡,反映儀器水平度
  - 管狀水準器:更精細的水平判斷
- 讀數顯示:HA / VA 度°分'秒",精確到 1"
- 正倒鏡切換:F 鍵翻轉望遠鏡

### 教學落點
- **測量實習(必修)**:對應水平角 / 垂直角測量、放樣作業
- **誤差消除概念**:正倒鏡平均消除視準軸誤差(實機真實技巧)

### 完成標準
- [ ] 18 步驟全可操作
- [ ] 整平迭代真實感(調一個螺旋影響其他軸氣泡)
- [ ] 讀數精度 1"(虛擬感知)
- [ ] 正倒鏡平均自動計算消除誤差

### 預期踩雷
- 第一人稱小視窗(對點器 / 水準器)在小螢幕難讀;對策:可放大顯示
- 整平迭代過程過於繁瑣 → 學生不耐;對策:Tutorial 關卡可加速「自動整平」按鈕,正式關卡要手動

**工時估算**:~40h(從零實作,可參考 LevelSim 結構但水平/垂直度盤、正倒鏡、放樣邏輯需新做)

---

## G3 水準儀操作 ✅ **(LevelSim 已完整實作,僅需 polish + 整合)**

### 現況
**已完整 bundled 在 `ArchSim/Plugins/LevelSim/`**(2026-06-18 PR#8 merged)。Standalone 115/115 PASS,5-leg gate 已綠,符合 CLAUDE.md 鐵則 #5 保護等級(`絕不碰 Plugins/LevelSim/`)。

### LevelSim 提供的能力

| 層級 | 內容 |
|---|---|
| **算法核心** `levelsim::LevelCore.h` | 純 C++17 / 零 UE 依賴 / 零 Eigen / 14 個 API 函式 + 9 個結構 |
| **真值/誤差模型** | 補償器殘餘安平誤差 / 視準軸 i 角 / 視差 / 地球曲率+大氣折光 opt-in |
| **體驗層** `ALevelSimPawn` | 完整 FSM:Overview / Leveling / Telescope / Booking / RouteSummary;三個 camera;Multi-station 完整支援 |
| **附屬 Actor** | `ALevelStaffActor`(扶尺員/標尺)、`ALevelSimGameMode`、`ALevelSimHUD` |
| **驗證** | 115/115 standalone test pass / 27 個對抗式審核發現修正(2026-06-08)/ smoke test hooks |

### 主要 API(來自 `LevelCore.h`)

| 函式 | 用途 |
|---|---|
| `tiltFromScrews(P, S)` | 三支點平面法線 → 傾角(TiltState) |
| `bubbleFromTilt(P, t)` | 圓水準器氣泡(BubbleState,offset 浮向高側) |
| `sightTilt(P, t, residual)` | 視線殘餘傾角(SightState,範圍內 losTilt = i 角 + clamp 殘餘) |
| `measure(P, S, st, residual)` | **完整真值量測**:reading = (opticsZ + D·tanε) − baseZ(+ 可選曲率折光) |
| `trueThreeHair(...)` | 三絲幾何視距(可選/考試模式) |
| `stadiaDistance(P, upper, lower)` | 視距 D = K·(上絲 − 下絲) |
| `curvatureRefraction(P, horizDist)` | 地球曲率 + 大氣折光合併修正 c = (1−k)·D²/(2R) |
| `parallaxJitter(P, S, D)` | 視差抖動 |
| `heightOfInstrument(BM, BS)` | HI = BM + BS |
| `pointElevation(HI, FS)` | H = HI − FS |
| `closeLoop(legs, C, byDistance)` | **多站閉合 + 平差**(byDistance / byStationCount) |
| `recoverIAngleTwoPeg(...)` | 兩樁法回推視準軸 i 角 |
| `scoreReading(P, truth, player)` | 評分(±readTol 滿分,±readPartial 之外 0,之間線性) |

### 玩家視角(LevelSim 已實作流程)
1. **Overview**:綜觀地形 + 標尺位置
2. **Leveling**:三腳螺旋升降,圓氣泡置中(roughLevel → fineLevel)
3. **Telescope**:對焦 + 照準(消視差),自動補償器啟動
4. **Booking**:輸入讀數 → 即時 scoreReading 評分
5. **RouteSummary**:Multi-station 閉合差 + 平差結果(byDistance / byStation 雙軌)

### 整合工作(本計畫書層級)
- **無需重寫 G3 任何算法**
- 把 LevelSim 整合進主遊戲的關卡選單(透過 SUQS quest 觸發 LevelSim level)
- HUD 風格統一(LevelSim 自帶 ALevelSimHUD,Phase 2 可換成跟主遊戲一致的 UMG 風格)
- xAPI log 整合(LevelSim 操作要寫進 Part I 學習 log)

### 完成標準
- [x] **算法層全綠**(115/115 standalone)
- [x] **體驗層可玩**(FSM + 三 camera + multi-station)
- [ ] 主遊戲關卡選單可呼叫 LevelSim level
- [ ] xAPI log 在 LevelSim 操作中寫入(verb: `selected / interacted / completed`)
- [ ] HUD 風格統一(Phase 2)

### 預期踩雷
- **不要試圖修改 LevelSim core**(鐵則 #5);任何 LevelSim 行為調整都走 UE consumer 包裝層
- LevelSim 是單人 Pawn 設計,多人需要在 G4 加入 angle(可用 Replicated UPROPERTY 同步 ScrewTravel / PlayerBS / PlayerFS 等 Pawn state)
- LevelSim 的座標單位是 metres(POD-friendly);UE world 是 cm,**轉換點集中在 `ALevelSimPawn::MakeSetup()`**,不要在其他地方做轉換

**工時估算**:~5h(polish + xAPI 整合 + 關卡選單呼叫)— 從原估 30h 降到 5h

---

## G4 多人分工模式(三人組:操作 / 扶尺 / 記錄)

### 概念
台灣高職測量實習慣例是**三人一組輪流**(操作員 / 扶尺員 / 記錄員)。本系統的多人模式設計直接對應這個慣例,且加入「換手機制」確保每位學生練到三個角色。

### 玩家視角
- 多人房間進入測量關卡 → 系統自動分配「角色 1 / 2 / 3」(可手動互換)
- **角色 1 操作員**:控制儀器,第一人稱視角,負責整平、照準、讀數
- **角色 2 扶尺員**:控制標尺位置,需通過「持尺穩定性挑戰」(圓盒氣泡居中 3 秒)才算成功讀數
- **角色 3 記錄員**:控制手簿 UI,負責填寫讀數、計算高差(高級模式手動,基礎模式自動)
- 完成一個測站後 HUD 提示「換手」,三個角色循環
- 一個關卡每位學生必須擔任過三個角色才算完成

### 介面與操作規格
- 多人 Session 4 人模式(三角色 + 1 老師觀察)
- 角色管理 Subsystem `UArchSimSurveyRoleSubsystem`(GameInstanceSubsystem)
- 換手機制:`USurveyRoleSubsystem::RotateRoles(SessionId)`,廣播 OnRotate event
- 評分依組整體閉合差,非個人

### FrameCore / LevelSim 接合
- **不接 FrameCore**
- **接 LevelSim**:`ALevelSimPawn` 是單人設計,多人模式需把 Pawn state(`ScrewTravel[3]`、`PlayerBS`、`PlayerFS`、`PlayerHI`、`PlayerElev`、`Phase`、`Job`)轉為 Replicated UPROPERTY 由 Server authoritative;扶尺員的 `ALevelStaffActor` 位置由 Player B 直接 Possess,記錄員的手簿 UMG 由 Player C 編輯(透過 RPC 同步)

### 教學落點
- **多人協作 + 測量實務**:對應實際工地測量隊分工
- **C2 人際關係與團隊合作素養**

### 完成標準
- [ ] 三角色可分配 + 互換
- [ ] 換手機制三角色都練到
- [ ] 多人下手簿同步顯示
- [ ] 老師可觀察任一玩家視角(整合 Part I1 Spectator)
- [ ] LevelSim Pawn state 正確 replicate

### 預期踩雷
- 學生只想玩操作員,不想當扶尺;對策:強制換手 + 計分按全組
- 持尺穩定性挑戰太簡單(失去意義)或太難(挫敗);MVP 簡化為「保持氣泡居中 3 秒」
- LevelSim Pawn 原設計是單一玩家 client-side 推算,改多人需要 Server-authoritative;**操作員的 ScrewTravel 變動要走 RPC 不能直接寫 local state**

**工時估算**:~25h(LevelSim 改多人 + 三角色 UI + 換手機制)— 從原估 30h 微調

---

## G5 測量任務題型(8 種關卡)

### 概念
完整關卡清單見附錄 E,本節列出 8 種主要題型 + 學習目標,提供關卡設計骨架。

### 題型清單

| # | 題型 | 對應儀器 | 學習目標 | 難度 |
|---|------|---------|---------|------|
| **S-01** | 高差計算 | 水準儀 ✅ LevelSim | 完整一次架設→後視→前視→計算流程 | ★ |
| **S-02** | 閉合水準路線 | 水準儀 ✅ LevelSim closeLoop | 理解閉合差意義、容許誤差、誤差分配 | ★★ |
| **S-03** | 建物角隅高程放樣 | 水準儀 ✅ LevelSim pointElevation 反算 | 高程放樣(從 HI 反算前視讀數) | ★★ |
| **S-04** | 橫斷面測量 | 水準儀 ✅ LevelSim multi-station | 斷面測量、填挖方計算 | ★★★ |
| **S-05** | 水平角測量 | 經緯儀 ❌ 需新做 | 全圓方向法、正倒鏡平均 | ★★ |
| **S-06** | 導線測量 | 經緯儀 ❌ 需新做 | 角度閉合差、座標閉合差、平差 | ★★★★ |
| **S-07** | 建物放樣 | 經緯儀 ❌ 需新做 | 座標放樣、儀器定向 | ★★★ |
| **S-08** | 誤差診斷關 | 水準儀手簿 ✅ LevelSim scoreReading | 誤差診斷、品質管控 | ★★★★ |

> **MVP 可玩部分**:S-01、S-02、S-03、S-04、S-08 五個關卡(LevelSim 已支援核心算法),只需在 SUQS DataTable 中定義 quest schema + 老師預設的 RoutePoints / 容許閉合差參數即可。**經緯儀 S-05~S-07 三題留 Phase 2**(G2 從零做完之後才能上)。

### 教學落點
- **測量實習(統測必考科目)**:S-01 ~ S-05 對應一般測量單元,S-06 ~ S-08 對應進階單元
- **學習表現**:對應群科素養指標「測量繪製營造能力」、「實務操作能力」
- **跨課程整合**:S-04 橫斷面結果可直接輸入 architect sim 結構模式(對應 G→主線銜接)

### 完成標準
- [ ] 8 種題型至少各 1 個範例關卡
- [ ] S-04 橫斷面結果可匯出 JSON → 主線結構模式 LoadModelFromJson 讀入
- [ ] 每題型有 3 級難度變化

### 預期踩雷
- 導線測量(S-06)需要矩陣運算平差,對高職偏難;MVP 提供「自動平差」按鈕,進階關卡手動
- 與主線結構整合容易出問題(座標系統 / 單位轉換);明確的 JSON schema 防錯

---

# 第十章 — Part H:多人協作

## H1 連線架構(Listen Server + NULL Subsystem LAN)

### 概念
完整實作 [核心技術決策 2](#決策-2多人架構--listen-server--onlinesubsystem-null-lan):**Listen Server + OnlineSubsystem NULL (LAN 模式)**,適合校園 4-30 人規模,不依賴 Steam/EOS。老師機作 Host 兼觀察端。

### 玩家視角
- 老師機:主選單 → 「主持遊戲」 → 選關卡 → 「Listen Server 啟動,Session 名稱:Class_11A」
- 學生機:主選單 → 「加入遊戲」 → 看到 LAN 列表中的「Class_11A」 → 點擊加入
- 連線成功後進入大廳,看到組員列表,可分組(預設每組 4-6 人)
- 老師可在大廳設定關卡 / 模式 / 規則

### 介面與操作規格
- `Config/DefaultEngine.ini`:
  ```ini
  [OnlineSubsystem]
  DefaultPlatformService=Null
  
  [OnlineSubsystemNull]
  bEnabled=true
  ```
- Session 建立:`SessionSettings.bIsLANMatch = true`
- Session 搜尋:`SessionSearch->bIsLanQuery = true`
- 大廳 UMG `ULobbyWidget`:顯示組員 + 設定按鈕
- 直連 IP fallback:若 LAN broadcast 失敗(VLAN 隔離),提供「直接輸入 IP」選項(`open 192.168.x.x`)

### FrameCore 接合
無(連線層基礎建設)。

### 教學落點
無直接教學(基礎建設)。

### 完成標準
- [ ] LAN 模式可建立 Session
- [ ] 4-30 個 Client 可加入
- [ ] 直連 IP fallback 正常
- [ ] 大廳介面完整

### 預期踩雷
- VLAN 隔離 / 不同子網 → broadcast 失敗;對策:直連 IP fallback + 學校 IT 開放 UDP port 7777
- Host 同時跑 Server + Client 在低階教學電腦上效能瓶頸;預設關卡規模 ≤ 100 構件

---

## H2 共編場景與構件放置衝突(Server 樂觀鎖)

### 概念
多人下兩位學生同時放構件在同一位置的處理,完整實作 [決策 5 Server 樂觀鎖](#決策-5prefab-放置多人衝突--server-樂觀鎖--格子佔用-tmap)。

### 玩家視角
- 多人模式下,看到組員的 ghost 預覽(藍色,不影響自己操作)
- 兩人同時點同一格 → Server 仲裁,只有一個成功
- 失敗者收到 toast:「該位置已被組員放置」,ghost 變紅消失

### 介面與操作規格
- Server 端 `UArchSimPlacementSubsystem::Server_RequestPlacement(GridPos, Rot, PrefabId)` RPC
- `TMap<FIntVector, FActorHandle> OccupancyGrid`(Server-only)
- Client 預覽 ghost 顯示為自己的色 / 組員的色(不同顏色避免誤判)
- 失敗反饋:Server 回傳 `EPlacementResult::OccupiedByOtherPlayer`

### FrameCore 接合
放置成功後,Server 呼叫 `UFrameInteractiveSubsystem::ApplyPatchAndResolve`,結果透過 GameState replicated 廣播到所有 Client(對應 [決策 3](#決策-3結構大腦分發--server-authoritative--fframesolveresult-json-摘要))。

### 教學落點
- **C2 人際關係與團隊合作素養**:衝突發生與解決
- **C2「協同設計」**:對應實際工地工作面分配

### 完成標準
- [ ] 同時放置只有一個成功
- [ ] 失敗者正確收到 toast
- [ ] 組員的 ghost 預覽顯示為不同顏色
- [ ] Server FrameCore 結果同步到所有 Client

### 預期踩雷
- Server 排程同一幀兩個 RPC,可能兩個都通過 Spawn;對策:RPC handler 內 TMap 查詢 + 標記 + Spawn 必須在同一 critical section
- 多人下熱圖更新慢半拍(Server Solve 延遲);對策:Client 端先本地預測再 reconcile

---

## H3 角色分工(力學 / 圖學 / 材料 / 施工)

### 概念
教育需求書 v3 中的「真合作」設計:**讓不熟悉軟體的學生也能用直觀方式表達想法**。不是同一份模型大家拖拉同一個構件(會吵架),而是每人負責一個面向 — 力學、圖學、材料、施工 — 分工協作。

### 玩家視角
- 多人組進入沙盒/關卡 → 系統可選「分工模式」
- 老師或組長分配 4 個角色:**力學工程師**(看熱圖 / 加強構件)、**圖學設計師**(畫尺寸標註 / 投影)、**材料專家**(選擇材料 / 改善 RC 工法)、**施工監督**(管理工序 / 檢查品質)
- 每個角色有專屬工具與 UI(其他角色看不到 / 看不細),但能看到結果
- 完成任務需要 4 個角色都各自完成自己的部分(模擬真實工程設計流程)

### 介面與操作規格
- 角色 enum:`EArchSimRole { Mechanical, Drafting, Materials, Construction }`
- 角色管理 Subsystem `UArchSimRoleSubsystem`(GameInstanceSubsystem)
- UI 顯示模式:預設「角色限定 UI」(自己看到自己的工具),老師可切換「全顯模式」
- 任務分配:`UArchSimRoleSubsystem::AssignRole(PlayerController, Role)` Server-only

### FrameCore 接合
不同角色看到不同資料子集:力學工程師看完整 FFrameSolveResult,圖學設計師只看幾何 + 標註,材料專家看 Material/Section 詳情。

### 教學落點
- **跨領域整合**:對應 108 課綱「跨科目整合」精神
- **真實工程分工**:讓學生體驗「工程設計不是一個人從頭做到尾」
- **室內設計科 vs 建築科 vs 土木科**:同一遊戲不同角色可由不同群科學生擔任

### 完成標準
- [ ] 4 角色可分配
- [ ] 角色限定 UI 正確隔離
- [ ] 完成任務需四角色協作
- [ ] 老師可即時看任一角色視角

### 預期踩雷
- 學生只想當「最酷的角色」(力學工程師有熱圖,看起來最威);對策:任務設計讓每個角色都有獨特貢獻(沒有圖學沒有施工順序,結構就無法完成)
- 角色限定 UI 過嚴 → 學生無法討論;對策:組內語音 / 文字頻道公開

---

## H4 變更紀錄與復原(Server 端 Undo Stack)

### 概念
多人下「誰改了什麼、可不可以還原」是教育場景的重要考量。Server 端維護 Undo Stack,每個操作記錄 actor + timestamp,允許指定步驟回滾。

### 玩家視角
- HUD 右上角「變更歷史」按鈕,展開後顯示時間軸:
  - 09:02 學生A 放置梁 B-03
  - 09:03 學生B 改變柱 C-01 材料
  - 09:05 學生A 移除梁 B-03
- 任一學生可請求「回到 09:03」,**需要組員多數同意**(避免單方面破壞)
- 回滾後該時點之後的所有變更被撤銷

### 介面與操作規格
- Server 端 `UArchSimUndoStack` Subsystem
- 每個操作記錄:`FArchSimChange { Actor, Action, BeforeState, AfterState, Timestamp }`
- 回滾需多數投票(`UArchSimVoteSubsystem::RequestVote(ProposalId)`)
- 預設保留 50 個操作的歷史

### FrameCore 接合
回滾時 Server 重建 FFrameModelDef 到該時點狀態,呼叫 `Rebaseline()` 重新建立 session。

### 教學落點
- **C2「設計迭代與反思」**:對應 108 課綱 A3「規劃執行與創新應變」
- **協作倫理**:多數投票機制讓學生學習尊重組員決定

### 完成標準
- [ ] 50 步驟 Undo Stack
- [ ] 變更歷史 UI 完整
- [ ] 投票機制正常
- [ ] 回滾後 FrameCore 狀態正確

### 預期踩雷
- 投票阻擋遊戲流暢度;對策:單人沙盒模式無投票,自由 Undo
- 50 步驟可能不夠(大關卡)/ 太多(記憶體);可在 Settings 調整
- 跨關卡 Undo 不可行;每進新關卡 Stack 清空

---

# 第十一章 — Part I:教師工具 + Log

## I1 教師進入學生世界(Spectator + Free Camera)

### 概念
完整實作 [決策 4 Spectator + bAlwaysRelevant](#決策-4教師觀察模式--spectator--balwaysrelevant-教學-actor)。**老師不是切走全班畫面示範,而是進入個別學生世界即時介入**(對應教育需求書 v3「老師放大、不取代」原則)。

### 玩家視角(老師端)
- 老師主畫面 HUD「班級總覽」面板:顯示所有學生縮圖 + 即時狀態(綠 = 順利 / 黃 = 卡關 / 紅 = 失敗)
- 點擊任一學生 → 老師相機切換為該學生 Spectator,看到該學生視角
- 按 F 切換為「自由相機」:老師可在該學生場景中自由漫遊
- 按 Tab 切換到下一位學生
- 老師可在學生畫面上「標記」(放虛擬箭頭、畫紅圈),學生看得到
- 老師有「私訊」工具,可單獨給某學生發語音/文字
- 老師絕不能直接控制學生 Pawn(避免侵入),只能標記與訊息

### 玩家視角(學生端)
- 老師進入時看到「老師正在觀察」標誌(右上角小圖示)
- 老師標記時看到場景中浮現箭頭/紅圈
- 老師私訊以彈窗顯示

### 介面與操作規格
- 老師 PlayerController 自訂 class `ATeacherPlayerController`
- 切學生:`ServerViewNextPlayer() / ServerViewPrevPlayer()`(UE 內建)
- 自由相機:`ACameraActor` + `SetViewTarget()`
- 教學 Actor(8 個 BP Actor + ghost preview)全部設 `bAlwaysRelevant = true`(關鍵!見決策 4)
- 標記系統:`AArchSimMarkActor`,3D 浮空箭頭 / 紅圈,Owner 為老師,但對所有 Client 可見
- 私訊用 UE Chat(可內建 Voice Chat,Phase 2)

### FrameCore 接合
無直接(老師只是觀察者)。但**老師可在學生場景施加「假設測試」**:暫時 deactivate 一根構件、看會怎樣 → 不影響學生實際模型;對應 [api research 場景 6](#)。

### 教學落點
- **Vygotsky ZPD + Scaffolding**:**老師作為人類鷹架**的最直接實作
- **班級管理效率**:老師不再依賴「切走畫面示範」,而是針對性介入
- **隱性鷹架配合顯性指導**:標記 + 私訊讓老師在不打斷整班的情況下精準幫助個別學生

### 完成標準
- [ ] 班級總覽 UI 顯示 30 學生即時狀態
- [ ] 老師可在 2 秒內切換到任一學生視角
- [ ] 自由相機在學生場景內無遮擋移動
- [ ] 標記系統(箭頭/紅圈)正確顯示給所有 client
- [ ] 私訊功能正常
- [ ] 學生明確知道老師在觀察

### 預期踩雷
- Spectator 距離截斷 → 老師遠處看不到學生建築:**必須對教學 Actor 設 `bAlwaysRelevant = true`**(關鍵踩雷!)
- 30 個學生視角同時 spectate 對 Server 壓力大;對策:只 spectate 焦點學生
- 學生可能因「被監視感」不自在;對策:老師標記明顯但不過度頻繁(設定每分鐘最多 3 次提醒)

---

## I2 學習 Log Schema(xAPI 1.0.3 + 自訂 Profile)

### 概念
完整實作 [決策 9 xAPI + SQL LRS](#決策-9學習-log--xapi--sql-lrs-yet-analytics--indexeddb-離線隊列)。Log 不是「分數紀錄」,而是「學生思維過程的可追蹤證據」 — 操作時間軸、失敗模式、修正路徑、求助次數,讓老師看見學生**腦中的錯誤**而非只是看最後成品。

### 玩家視角(學生)
- 預設 log 自動發送,玩家無感
- 主選單「我的學習紀錄」可看自己的時間軸 + 統計
- 「申請刪除個資」按鈕,符合台灣個資法

### 玩家視角(老師)
- 老師端 dashboard(見 I3)讀取 LRS
- 看到「班級總覽 / 個別學生時間軸 / 失敗模式排行」三層

### 介面與操作規格
- xAPI Statement schema 完整定義見 [log research](#) 章節
- 自訂 verb 8 個:`placed-member / removed-member / applied-load / observed-failure / fixed-by-adding / asked-for-hint / ran-simulation / reflected`
- LRS 採 SQL LRS (Yet Analytics, Apache 2.0)
  - 本地 SQLite 用於老師單機,生產用 PostgreSQL
  - Docker 部署
- Client 端 IndexedDB sync queue + Background Sync API
- Pseudonymization:`actor.account.name = SHA256(student_id + school_salt)`,salt 存學校後台
- TLS 必開(HTTPS)
- 老師端可申請刪除學生個資(對應個資法)

### FrameCore 接合
log 從 FrameCore 結果取出關鍵指標寫入 `result.extensions`:
- `dc-ratio`(MaxDC)
- `max-displacement-mm`
- `failure-mode`(buckling / yielding / instability / null)

### 教學落點
- **學習診斷工具**:老師看見學生卡關位置,精準介入
- **108 課綱學習歷程**:log 可作為學習歷程檔案的「實作作品」上傳依據
- **個資法合規**:Pseudonymization + 刪除權設計

### 完成標準
- [ ] xAPI Statement 自動發送,離線時隊列正確緩存
- [ ] SQL LRS 本地與雲端皆可部署
- [ ] 8 個自訂 verb 正確映射遊戲事件
- [ ] Pseudonymization 學生姓名永不外洩
- [ ] 老師可申請刪除學生個資
- [ ] 多人下個人 actor 不相互可見(只看自己 + 老師可看全班)

### 預期踩雷
- Safari 不支援 Background Sync API;對策:`visibilitychange` fallback
- 離線隊列累積太多(學生帶 log 離線一週);對策:LRS 端監控同步延遲,主動推送提醒
- 個資法第 5 條「必要範圍」邊界:遊戲操作 log 是否算個資?**保守處理為「間接個資」全程加密**

---

## I3 老師端 Dashboard(班級 / 個別 / 模式分析)

### 概念
老師查看 LRS 資料的主要介面,完整實作 [log research 第六節 dashboard wireframe](#)。三層視角:**班級總覽 / 個別學生時間軸 / 失敗模式分析**。

### 介面與操作規格

**6.1 班級總覽頁(Class Overview)**
```
+--------------------------------------------------+
| [班級: 11A] [課程: 結構入門] [日期: 本週] [匯出] |
+--------------------------------------------------+
| 完成率  ████████░░  78% (28/36人)                |
| 平均嘗試次數  4.2 次   平均求助次數  1.8 次       |
+--------------------------------------------------+
| 學生進度熱圖(每人一格,顏色 = 通關狀態)         |
| [s01 綠] [s02 紅] [s03 黃] [s04 綠] ...         |
|  綠 = 通關  黃 = 進行中  紅 = 卡關 >3 次         |
+--------------------------------------------------+
| 常見失敗模式排行:                                |
|  1. buckling         (43 次, 32%)               |
|  2. excessive-deflect (28 次, 21%)              |
|  3. instability      (19 次, 14%)               |
+--------------------------------------------------+
| 卡關熱點:構件 M_015 被移除超過 5 次(12 人共現) |
+--------------------------------------------------+
```

**6.2 個別學生細看頁(Student Detail)**
- 時間軸顯示每個動作:09:02 initialized / 09:03 placed-member / 09:05 ran-simulation → D/C 0.72 / ...
- 修正路徑圖:D/C before/after 折線
- 求助次數、觀察失敗秒數
- 老師備註欄(老師可寫教學介入紀錄)

**6.3 常見錯誤分析頁(Error Pattern Analysis)**
- 失敗模式 × 關卡交叉分析表格
- 頻繁被移除的構件(可能教學問題點)
- 修正策略分佈

### 教學落點
- **教師專業成長**:dashboard 是老師的「教學診斷工具」
- **回饋教學改進**:看到班級共同失敗模式 → 老師調整教學重點

### 完成標準
- [ ] 3 個 dashboard 頁完整
- [ ] LRS 查詢 API 正確
- [ ] 老師可匯出班級報告(CSV/PDF)
- [ ] 老師可篩選班級 / 課程 / 日期

### 預期踩雷
- LRS 查詢效能(30 學生 × 1000 statements):需要適當 index + 分頁
- 老師非 IT 專業,介面要極簡;對策:不顯示 xAPI 原始 JSON,只顯示中文敘述

---

## I4 學生反思工具

### 概念
完整實作 [log research 第七節學生反思工具](#)。反思不是「給數字」,而是用**與學習目標對齊的引導問題**促進深度思考(Crystal Island 研究)。

### 玩家視角
- 關卡結束(成功或失敗)後彈出「這次挑戰回顧」頁
- 顯示:
  - 嘗試次數
  - 最大挑戰(失敗模式)
  - 怎麼解決(修正策略)
  - 引導問題(選一題或都答):「為什麼選擇升級斷面而不是加斜撐?」「下次會在哪一步停下來思考?」
  - 班級對比(匿名):「32% 的同學也遇到 buckling,其中一半用加斜撐解決」
- 完成反思 +10 經驗值(輕微獎勵,鼓勵但不強制)

### 介面與操作規格
- UMG `UReflectionWidget`,關卡結束自動顯示
- 引導問題從 DataAsset `DA_ReflectionPrompts` 隨機抽 1-2 題
- 文字框 + 選擇題混合
- 提交反思 → 寫入 xAPI:`verb=reflected, result.response=文字`

### FrameCore 接合
無(純 UI/log)。

### 教學落點
- **元認知能力**:對應 108 課綱核心素養 A1「身心素質與自我精進」
- **個性化深度學習**:不同學生反思方向不同,老師可從反思看到思維特徵
- **學習歷程檔案**:反思可作為「多元表現」上傳

### 完成標準
- [ ] 反思頁正確顯示
- [ ] 引導問題 20+ 個範本
- [ ] 班級匿名對比(不暴露個人)
- [ ] 跳過反思 / 完成反思皆可
- [ ] 反思內容寫入 LRS

### 預期踩雷
- 學生不想寫反思(覺得麻煩);對策:第一次強制 + 後續可跳過,但通關獎章看完成反思率
- 反思內容隱私:預設僅老師可見,學生可選「公開」分享
- 文字輸入在遊戲手把上難打;MVP 鎖定 PC 滑鼠鍵盤

---

# 第十二章 — Part J:教育設計總綱

## J1 教學主軸閉環(再次定錨)

整個系統的教育骨架是一條閉環:**設計 → 施工 → 加載 → 崩塌 → 診斷 → 改**(見 [0.2 學習主軸閉環](#02-學習主軸閉環))。每個 Part(A 到 L)都是這條閉環的一部分,單獨抽出不能成立教育目標,只有閉合成完整迴圈才能達成 Productive Failure 的學習效果。

本節不重複前面內容,只強調**主軸的不可分割性**:Part A 引擎 + Part B 玩家操作對應「設計」;Part E 工序對應「施工」;Part C1 載重對應「加載」;Part C3 崩塌對應「崩塌」;Part D 診斷工具對應「診斷」;Part F2 關卡系統的「改設計」循環對應「改」。**任何一段缺失,閉環即斷,學習效果大降**。

## J2 三大教育原則的學理基礎

完整 7 個教育理論的整理見 [edu-theory research](#);此處以三大原則收斂:

### 原則 1:直覺優先(Intuition First)
**學理基礎**:PhET「讓看不見的可見」+ Sweller CLT「降低外在認知負荷」+ Csíkszentmihályi Flow「清楚即時回饋是心流前提」三者匯流。

**核心命題**:工程力學教學的最大障礙不是「公式難」,而是「公式對應的物理現象看不見」。先建立視覺直覺,公式才有可附著的基模。

**具體體現**:
- 遊戲中**不強迫學生輸入方程式**(除進階關卡)
- D/C 熱圖、變形動畫、崩塌景觀是教學主管道
- 數值顯示是「確認直覺」的可選工具,不是必修

### 原則 2:失敗即學習(Failure as Learning)
**學理基礎**:Kapur Productive Failure(160 個對照實驗)+ Papert constructionism「錯誤是建構過程」+ Vygotsky ZPD「ZPD 工作必然包含當下做不到的任務」。

**核心命題**:失敗是學習資源,不是懲罰。系統的優勢在於**崩塌免費 / 即時 / 可重試 / 視覺值得觀看**,把 PF 循環從一週壓縮到三分鐘。

**具體體現**:
- 不顯示「Game Over」紅字,顯示「來看看哪裡有問題」
- 崩塌動畫做得有娛樂價值(K3「Failure as Spectacle」)
- 失敗後自動進入診斷模式 30 秒
- 多次失敗不扣分,鼓勵重試

### 原則 3:老師放大、不取代(Teacher as Amplifier, Not Substitute)
**學理基礎**:Vygotsky ZPD + Scaffolding 將老師定位為「幫助學習者進入發展近側的引導者」+ Kapur PF 框架中老師的「鞏固整合(Consolidation)」教學時機。

**核心命題**:一個沒有老師觀察與介入能力的建築模擬遊戲,只是強化了個體學習。加入老師可見度後,老師能識別班級共同概念錯誤,精準在恰當時機提供 PF 鞏固教學。

**具體體現**:
- I1 教師進入學生世界(Spectator + 標記)
- I3 dashboard 顯示班級失敗模式排行
- 老師「不切走畫面」,而是「進入學生畫面」

## J3 完整關卡清單(主線 + 測量)

詳見[附錄 E:完整關卡清單](#附錄-e完整關卡清單)。MVP 目標 15+ 關卡(主線 10 + 測量 5),完整版 30+ 關卡。

每個關卡的標準格式:
- ID(L-XX / S-XX)
- 中文名稱
- 對應 Part / 模組
- 學習目標
- 預期失敗模式
- 通關條件(D/C / 撓度 / 安全係數等具體數值)
- 對應課綱單元
- 預估時長

## J4 評量機制

### 評量原則
**不是用考試打分數,而是用學習歷程證據佐證**。對應 108 課綱「學習歷程檔案」制度,學生在遊戲中的操作 log、崩塌診斷報告、迭代紀錄,均可作為「實作作品」與「多元表現」上傳。

### 評量管道
1. **遊戲內成就**(輕量、即時):每關 3 個獎章(完成 / 完美 / 速通)
2. **log 系統**(深度、累積):xAPI 完整時間軸,老師可匯出個別/班級報告
3. **反思內容**(質性、深度):I4 學生反思工具的文字回應
4. **作品集**(展示用):沙盒模式作品可匯出 `.archsim_bp` + 截圖

### 評量到課綱對應
- **形成性評量(老師持續觀察)**:I1 + I3 dashboard
- **總結性評量(關卡完成度)**:F2 SUQS quest progression
- **學習歷程檔案(108 課綱要求)**:作品集 + 反思 + log 報告

### 不適合用本系統評量的
- **真實工程能力**:本系統是教育簡化,不能取代專業執照考試
- **個人創意分數**:沙盒作品的「美感」「創意」不可自動化評分,留給老師主觀評定

## J5 對標 108 課綱(完整對應表)

詳見[附錄 C:108 課綱對應](#附錄-c108-課綱對應);此處列出核心對應:

### 高一(基礎概念建立期)
| 課綱科目 | 對應 Part | 對應關卡 |
|---|---|---|
| 土木建築工程與技術概論 | 0.1 定位、F1 沙盒 | L-00 序章介紹 |
| 製圖實習(一) | B2 2D↔3D、B3 標註 | L-01 平面圖識讀 |
| 材料與試驗 | E4 材料狀態機 | L-02 材料比較關 |
| 基礎工程力學(一) | C1 載重、D1 掃描儀 | L-03 ~ L-05 靜力平衡關卡 |
| 測量實習(一) | Part G | S-01 ~ S-03 水準儀關卡 |

### 高二(核心力學與設計整合期)
| 課綱科目 | 對應 Part | 對應關卡 |
|---|---|---|
| 基礎工程力學(二) | C2 D/C 熱圖、D3 BMD/SFD | L-06 ~ L-10 應力應變關卡 |
| 構造與施工法 | B1 構件放置、E1 工序狀態機 | L-11 RC 工序關卡 |
| 電腦輔助製圖實習 | B2 2D↔3D + B4 藍圖 | L-12 CAD 介面關卡 |
| 設計與技術實習 | F1 沙盒 | 沙盒自由創作 |
| 測量實習(二) | G2 經緯儀、G5 任務 | S-05 ~ S-08 進階測量 |

### 高三(整合應用期)
| 課綱科目 | 對應 Part | 對應關卡 |
|---|---|---|
| 建築結構實習(校訂) | C3 崩塌、F4 失敗即學 | L-13 ~ L-15 崩塌診斷關 |
| 營建技術實習 | E1 ~ E4 工序 + F5 規範 | 完整 RC 工序大關卡 |
| 專題實作 | 沙盒 + 多人 + 教師介入 | 期末大型設計專題 |

### 108 課綱核心素養三面九項對應
| 素養 | 對應遊戲機制 |
|---|---|
| A1 身心素質與自我精進 | I4 學生反思工具(元認知) |
| A2 系統思考與解決問題 | F3 學習閉環、D2 + D3 + D4 診斷工具組 |
| A3 規劃執行與創新應變 | F1 沙盒 + F4 失敗即學 + F5 規範 |
| B1 符號運用與溝通表達 | B2 2D↔3D + B3 標註 |
| B2 科技資訊與媒體素養 | A1 FrameCore + Part D 視覺化工具 |
| B3 藝術涵養與美感素養 | F1 沙盒 + Part K 風格 |
| C2 人際關係與團隊合作 | Part H 多人協作 |

## J6 教師備課指南(精簡版)

完整教師備課文檔將獨立成 `TeacherGuide.md`,本節只列骨架:

### 課前準備
- 確認電腦教室 LAN 可通(VLAN 不隔離)
- 老師機作 Host,啟動 Session
- 學生機開遊戲 → 加入 Session
- 班級分組(預設 4-6 人 / 組)

### 課中流程(45 分鐘範本)
- **10 分鐘**:老師介紹本節學習目標 + 對應課綱單元
- **20 分鐘**:學生自由操作 / 完成指定關卡(老師 dashboard 觀察)
- **10 分鐘**:老師根據 dashboard 看到的共同失敗點,集中講解(Kapur PF 的鞏固教學時機)
- **5 分鐘**:學生完成反思

### 課後分析
- 老師端 dashboard 匯出班級報告
- 識別常見錯誤 → 下次課程調整
- 學生反思內容回饋 → 個別追蹤

### 與既有課程整合建議
- 第一週引入「土木建築工程與技術概論」課程
- 後續每週搭配「基礎工程力學」單元,每單元對應 2-3 個關卡
- 期末「專題實作」可用沙盒模式完成大型設計

### 注意事項
- 本系統是**輔助工具**,不取代教師授課
- 「失敗即學習」是設計原則,不是教學藉口 — 老師仍應介入卡關 3 次以上的學生
- 個資法合規:課前發放家長同意書 → 學生 log 才能上傳 LRS
- 系統 bug 或無法載入時,提供 fallback 教案(傳統手算題目)

---

# 第十三章 — Part K:風格與美術

## K1 視覺風格(寫實 vs 卡通 vs 工程圖)

### 概念
本系統的視覺定位:**簡潔工程感**(類似 Karamba3D / Poly Bridge 風格),不走 AAA 寫實,也不走純卡通。目標是讓「結構行為」成為視覺焦點,避免裝飾性元素干擾學習。

### 具體選擇
- **構件**:幾何造型清晰,材質寫實但簡化(混凝土用淡灰、鋼材用淡藍灰、木材用淡黃褐)
- **環境**:極簡(室外 = 草地 + 遠山剪影 / 室內 = 灰白牆 + 木地板),避免過度裝飾
- **照明**:固定方向太陽光(2 pm 角度,陰影清晰但不過強)+ ambient occlusion
- **後處理**:輕度,不過曝、不過 vignette
- **PostProcess Volume**:Material 走 Unlit 為主(尤其熱圖 Actor),避免動態光照影響資料讀取

### 對比決策
| 風格 | 優點 | 缺點 | 採用? |
|---|---|---|---|
| 寫實(類 Twinmotion) | 視覺震撼 | 干擾學習焦點、效能負擔 | ❌ |
| 卡通(類 SimCity) | 親和性高 | 工程感不足 | ❌ |
| **工程圖簡潔(類 Karamba)** | **聚焦結構行為、效能低、易讀** | 視覺較單調 | ✅ |

## K2 色彩語意(完整對照表)

完整色彩規範見 [viz research](#);此處列出核心對照:

| 場景 | 主色表 | RGB Hex | 色盲友善替代 |
|---|---|---|---|
| **D/C 比值** | 綠→黃→橘→紅(交通號誌語意) | #009E73 → #F0E442 → #E69F00 → #D55E00 | 同(Okabe-Ito 已色盲友善) |
| **位移大小** | Viridis 或 Cividis(感知均勻) | Cividis #00224E → #F6D645 | Cividis(本身色盲安全) |
| **模態振型(±振幅)** | 藍→白→紅 發散 | #0072B2 → #FFFFFF → #D55E00 | 藍橘(Okabe-Ito) |
| **軸力(壓拉雙向)** | 藍 = 拉力 / 橘 = 壓力 / 白 = 零 | 同上 | 同上 |
| **應力場 von Mises** | Plasma | #0D0887 → #F0F921 | Cividis 替代 |
| **載重路徑** | 藍→綠漸層(力流深淺) | #56B4E9 → #009E73 | 同 |
| **崩塌動態速度** | inferno-style | #000000 → #8B0000 → #FF4500 → #FFFFFF | 同 |
| **邊界條件 / 支承** | Okabe-Ito 分類色 | Sky Blue #56B4E9(鉸支)/ Purple #CC79A7(固支)| 同 |

### 通用規則
- **永遠不用 Jet 彩虹色表**(學術已批判 20 年)
- **連續漸層為主**(15-17 歲玩家熟悉血量條語言)
- **加圖示作冗餘語意**(避免單靠顏色)
- **HUD 中說明色彩約定**(防跨工具混淆)

## K3 音效設計

### 設計原則
- **少而精**:過多音效干擾學習,只在「事件性」時刻播放
- **資訊性 > 裝飾性**:每個音效對應一個明確事件
- **可關閉**:Settings 提供音量分軌(主音量 / 音效 / 音樂)

### 核心音效清單

| 事件 | 音效類型 | 用意 |
|---|---|---|
| 放置構件 | 短促木質敲擊 | 操作回饋 |
| 移除構件 | 短促電子嗶聲 | 操作回饋 |
| Solve 完成 | 極輕電子提示 | 系統處理完畢 |
| D/C > 1.0 警告 | 中等強度警告聲 | 注意警告 |
| 倒塌開始 | 低頻轟鳴 + 結構聲 | 戲劇張力 |
| 倒塌構件落地 | 物理碰撞聲(Chaos 內建) | 真實感 |
| 通關成功 | 上揚和聲 | 正向回饋 |
| 失敗 | 低調音效(不刺耳) | 鼓勵重試 |
| Tutorial 提示 | 柔和提示音 | 引導 |
| 老師訊息 | 鐘聲 | 注意老師 |

### 環境音
- 室外:輕度風聲 + 鳥鳴(可關)
- 室內:空調白噪音(可關)
- 倒塌進行中:暫停環境音,突出結構聲

## K4 字體與術語

### 字體選擇
- **HUD 標題 / 按鈕**:Noto Sans CJK TC(思源黑體繁體中文,免費 OFL)
- **內文 / 數值**:同上,小一級
- **程式碼 / 數值顯示**:JetBrains Mono(等寬,免費 OFL)
- **裝飾標題**(關卡名稱):思源宋體繁體中文(免費 OFL)

### 字級
- 標題 24px / 副標題 18px / 內文 14px / 警告 16px
- 響應式縮放(1920×1080 為基準,1366×768 自動縮小到 80%)

### 中英對照原則
- **中文為主**:所有 UI、警告、教學文字均中文
- **保留英文**:技術術語、規範代號、單位(D/C、MPa、kN·m、Fy、f'c)
- **中英並列**(課程對應):「彎矩(Bending Moment)」「容許應力(Allowable Stress)」
- 避免直譯:「Demand/Capacity」翻為「需求/容量比」較難懂 → 簡稱「D/C 比」並輔以圖示

### 數字格式
- 力 / 長度 / 應力都明確標單位
- 預設單位:N-mm-MPa(FrameCore 內部慣例)
- UI 顯示可切換:kN-m-MPa(高職常用)或 N-mm-MPa
- 角度用「度°分'秒"」(測量關卡)

### 術語表
完整 90+ 詞彙的中英對照見[附錄 A:術語表](#附錄-a術語表)

---

# 第十四章 — Part L:Roadmap + MVP

## L1 Phase 0-3 規劃(12 個月路徑)

> 本 Roadmap 以 1-2 人全職開發團隊為前提。實際時程取決於團隊規模、Plugin 整合順利程度、FrameCore consumer 端 binding 速度。

### Phase 0 — 環境準備(Day 1-7, 1 週)

**目標**:四個 plugin 與 FrameCore 同時跑通,UE Editor 可正常開啟

**任務**:
- [ ] Clone 4 個 plugin 進 `Plugins/`:ALS-Refactored v4.17 / Prefabricator UE5 / SPUD / SUQS
- [ ] 啟用 Enhanced Input(`DefaultEngine.ini` 設定)
- [ ] 建立 `AArchSimCharacter` 基本骨架
- [ ] FrameCoreUE 已有的 8 BP Actor 在新場景中可實例化
- [ ] `UFrameInteractiveSubsystem` 在 GameInstance 啟動時正確初始化
- [ ] 5-leg gate 仍綠(integ research 整合測試清單 Phase 0)

**完成條件**:Editor 開啟 ArchSim.uproject,console 無 plugin load error

### Phase 1 — MVP 結構閉環(Day 8-90, ~3 個月)

**目標**:**最簡可玩版本** — 學生可以蓋出簡單結構、加載、看 D/C 熱圖、即時互動式重解

**任務**:
- [ ] A1 FrameCore 接合層(UArchSimMemberData + UArchSimModelRegistry)
- [ ] A2 ALS 角色與相機(行走 + Enhanced Input 切換建築模式)
- [ ] B1 構件放置系統(5 大類 × 3 變體 = 15 Prefab)
- [ ] C1 載重施加(節點力 + 自重)
- [ ] C2 即時 D/C 熱圖(`AFrameUtilizationHeatmapActor`)
- [ ] F1 沙盒模式單機版
- [ ] F2 關卡系統最簡版(3 個 Tutorial 關卡 L-01 ~ L-03)

**完成條件**:單人沙盒可放 50 個構件 + 加載 + 看熱圖,3 個 Tutorial 關卡可完整通關

### Phase 2 — 完整教育體驗(Day 91-270, ~6 個月)

**目標**:加入多人、教師工具、學習 log、崩塌動畫、施工工序、診斷工具完整版

**任務**:
- [ ] C3 崩塌動畫 + Chaos 碎片(`AFrameDynCollapseReplayActor` + `AFrameFragmentClusterActor`)
- [ ] C4 連通性與載重路徑(`UFrameStructureGroupSubsystem`)
- [ ] B2 2D ↔ 3D 即時連動工作區(4 viewport)
- [ ] B3 圖學標註系統
- [ ] B4 藍圖系統(Prefabricator runtime spawn)
- [ ] D1 應力掃描儀(熱圖 + 錨定模式)
- [ ] D2 變形動畫 + D3 BMD/SFD + D4 安全係數
- [ ] E1 ~ E4 施工工序完整版
- [ ] F3 ~ F5 學習閉環 + 失敗即學 + 對標規範
- [ ] H1 ~ H4 多人協作 LAN
- [ ] I1 ~ I4 教師工具 + log + dashboard + 反思
- [ ] Part G 測量關卡完整 8 題型

**完成條件**:完整 Phase 1 + 30 + 個關卡 + 多人 + 教師端 dashboard 全功能

### Phase 3 — 進階與發布(Day 271-365, ~3 個月)

**目標**:打磨、優化、CAD 匯出、發布準備

**任務**:
- [ ] 14 號功能:CAD 匯出(MctoNurbs sidecar 整合,IFC/DXF)
- [ ] PCG 地形完整 5 模板
- [ ] 多語在地化(中英並存)
- [ ] 效能優化(World Partition / LOD / Nanite)
- [ ] 老師備課文檔 `TeacherGuide.md`
- [ ] 學生反思工具完整資料庫(20+ 反思問題)
- [ ] 完整 LRS 部署 + 老師端 dashboard 完整三頁
- [ ] 5-leg gate 在 release build 全綠
- [ ] **Steam / itch.io 發布**或**學校直接分發**

**完成條件**:可發表的 v1.0 版本

## L2 MVP 驗收條件(Phase 1 結束時)

MVP 必須通過以下 10 項驗收:

1. **單人沙盒可建造 50 個構件,Solve 時間 <2 秒**
2. **熱圖在玩家修改後 <500ms 更新**(端到端)
3. **3 個 Tutorial 關卡可完整通關**(L-01 懸臂梁、L-02 簡支梁、L-03 門型框架)
4. **失敗的關卡可重試,且改設計後熱圖正確更新**
5. **SaveGame 在 50 構件規模下 round-trip 正確**(放下 → 存檔 → 載入後位置與材料一致)
6. **規範對標警告 W-01 至 W-05 中文文字正確顯示**
7. **5-leg gate(standalone + UE + OpenSees + audit + CLI roundtrip)全綠**
8. **教學流暢度測試**:邀請 3 位高職建築科學生試玩 30 分鐘,可獨立完成 L-01
9. **效能基準**:1280×720 解析度下保持 30 fps(50 構件規模)
10. **基礎個資合規**:玩家姓名不存在 UE SaveGame,只存 pseudonymized hash

## L3 風險清單

### 技術風險

| 風險 | 影響 | 緩解策略 |
|---|---|---|
| UE5.8 replication breaking changes | 高 | **鎖定 UE5.7 至 MVP 完成**,升級前完整 5-leg gate 驗證 |
| SPUD 對 World Partition issue #117 | 中 | MVP 不用 World Partition,Phase 2 加入時測試 |
| **SPUD 依賴 `StructUtils` plugin 在 UE5.5 已 deprecated**(2026-06-25 S-00 build log L10-11 確認) | 中(UE5.8+ 升級風險) | MVP 不擋,UE5.7 仍可用;**UE5.8+ 升級前必須先驗證 SPUD 是否已換 StructUtils 替代方案,否則 SPUD 無法載入,持久化整層失效** |
| ALS Iris Push Model 自訂 subclass dirty mark 漏 | 高(多人) | 自訂 subclass 嚴格遵守 `MARK_PROPERTY_DIRTY_FROM_NAME` |
| ~~Prefabricator `.uplugin EngineVersion` 未標 5.7~~ | ✅ 已修(S-00) | 加 `"EngineVersion": "5.7.0"` |
| FrameCore Solve 在 1000+ 構件下卡頓 | 中 | 限制單關卡 ≤500 構件;Phase 2 加入優化 |
| Listen Server Host 機效能瓶頸(30 人邊界) | 中 | Server 端 Solve 佇列 + 構件 cooldown;30+ 人改 Dedicated Server |
| NULL Subsystem 跨 VLAN 失敗 | 中 | 直連 IP fallback + 學校 IT 開放 UDP 7777 |

### 教育設計風險

| 風險 | 影響 | 緩解策略 |
|---|---|---|
| 學生覺得「失敗即學習」是藉口 | 中 | 老師端 dashboard 看到卡關 3 次主動介入 |
| 熱圖顏色學生看不懂 | 高 | 加圖示作冗餘語意 + HUD 圖例 |
| 「直覺優先」導致學生忽略公式 | 中 | 進階關卡引入數值驗證 + 老師補充公式教學 |
| 老師不願意學習 dashboard | 高 | 老師備課文檔 + 一頁式速查卡 + 在地化培訓 |
| 個資法合規問題 | 高 | Pseudonymization + 家長同意書 + 刪除權設計 |

### 商業 / 推廣風險

| 風險 | 影響 | 緩解策略 |
|---|---|---|
| 學校 IT 環境不允許新軟體 | 高 | 提供 portable 版本(不需安裝)+ 靜態 SaveGame 模式 |
| 校內網路頻寬不足 | 低(LAN 即可) | 純 LAN 不需網際網路 |
| 開發團隊離職 | 高 | 詳細 ARCHITECTURE.md + HANDOFF.md + CLAUDE.md 文檔(已有先例) |
| FrameCore 開源 license 變動 | 低(已 MIT 內部) | MIT 授權保持 |

### 與 FrameCore 的相互依賴風險

| 風險 | 影響 | 緩解策略 |
|---|---|---|
| FrameCore engine 因 bug 需要修正 | 中 | 修正視為 v4.0.x patch,**先做 CLAUDE.md amendment**(鐵則 #1 FROZEN marker) |
| FrameCore API 不夠用(如缺沿桿取樣) | 中 | C6/C7 roadmap 已規劃,以 UE consumer 層補(MVP 簡化版) |
| FrameCore 結果不準確(校驗失敗) | 低(5-leg gate 已綠) | 持續對標 OpenSees |

---

# 附錄

## 附錄 A:術語表

| 中文 | English | 解釋 |
|---|---|---|
| 結構大腦 | Structural Engine | 指 FrameCore,負責所有結構力學計算 |
| 體驗層 | Experience Layer | 指 UE5 端的玩家互動 / 視覺化 / 教學機制 |
| 構件 | Member / Element | 結構單元(柱、梁、板、殼) |
| 節點 | Node | 構件連接點,可能含支承條件 |
| 利用率 / D/C 比 | Utilization / Demand-Capacity Ratio | 需求 ÷ 容量,>1.0 = 失效 |
| 安全係數 | Safety Factor | 1 / max(D/C) |
| 彎矩 | Bending Moment | 截面承受的彎曲力矩 |
| 剪力 | Shear Force | 截面承受的剪切力 |
| 軸力 | Axial Force | 沿桿長度方向的力(壓/拉) |
| 撓度 | Deflection | 構件變形的位移量 |
| 模態 | Mode / Modal Shape | 結構自然振動的特定形狀 |
| 屈曲 | Buckling | 細長構件受壓失穩 |
| 反應譜 | Response Spectrum | 地震載重的頻率域表示 |
| 等強壓力 | von Mises Stress | 三維應力等效純量(殼用) |
| 學習閉環 | Learning Loop | 設計→施工→加載→崩塌→診斷→改 |
| 直覺優先 | Intuition First | 教育原則 1,先建立視覺直覺再公式 |
| 失敗即學習 | Productive Failure | Kapur 2016 教育理論 |
| 老師放大 | Teacher as Amplifier | 教育原則 3,老師作為人類鷹架 |
| 鷹架 | Scaffolding | Vygotsky 提出的教學支持機制 |
| 近側發展區 | ZPD(Zone of Proximal Development) | 學習者「獨立做」與「協助下做」之間的學習帶 |
| 認知負荷 | Cognitive Load | Sweller 提出的工作記憶負擔分類 |
| 心流 | Flow | Csíkszentmihályi 提出的最優學習狀態 |
| 沙盒模式 | Sandbox Mode | 無目標自由創作模式 |
| 關卡 | Level / Quest | 有目標、有約束的引導性場景 |
| 藍圖(系統) | Blueprint System | 構件群儲存與重用功能(Prefabricator) |
| 全息投影 | Hologram / Ghost Preview | 半透明藍圖預覽,源自 Block Reality |
| 工序狀態機 | Construction State Machine | 配筋→澆置→養護→拆模序列 |
| RC 融合 | RC Composite Fusion | 鋼筋 + 混凝土合成複合節點 |
| 蜂窩弱點 | Honeycomb Defect | 澆置不實造成的混凝土缺陷 |
| 養護 | Curing | 混凝土凝固強度發展期 |
| 應力掃描儀 | Stress Scanner | 玩家可裝備的診斷工具 |
| 載重路徑 | Load Path | 力從加載點傳到地基的路徑 |
| 連通性 | Connectivity | 構件間是否屬於同一結構(Union-Find 維護) |
| 錨定 | Anchoring | 結構是否連到地基的判定 |
| 教師進入 | Teacher Spectator | 老師進入學生世界觀察的機制 |
| 學習 log | Learning Log | xAPI Statement 序列 |
| LRS | Learning Record Store | 儲存 xAPI 紀錄的後端 |
| 反思 | Reflection | 學生對自己學習過程的元認知活動 |
| 個資 pseudonymization | Pseudonymization | 用 hash 取代真實姓名的去識別化 |
| 規範對標 | Code Compliance | 對照建築結構規範的警告系統 |
| 自由相機 | Free Camera | 老師可在學生場景中漫遊的相機 |
| Listen Server | Listen Server | UE 內建的「同時是 Server 與 Client」架構 |
| Dedicated Server | Dedicated Server | 純伺服器(沒有渲染)架構 |
| 樂觀鎖 | Optimistic Lock | 多人並發處理策略,先查詢後標記 |
| 動力倒塌 | Dynamic Collapse | N4 連續動力倒塌分析 |
| 碎片 | Fragment | 倒塌後分離的剛體塊 |
| Chaos(物理) | Chaos Physics | UE5 內建剛體物理引擎 |
| 影響線 | Influence Line | 載重位置變化對某反力影響的圖 |
| FROZEN(凍結) | Frozen | FrameCore engine 源 v4.0.0 起永久不動 |

(完整 90+ 詞彙;此處列關鍵 50)

---

## 附錄 B:開源依賴清單

| 名稱 | License | 用途 | 整合方式 | 風險 |
|---|---|---|---|---|
| **FrameCore v4.0.0** | (內部 MIT) | 結構大腦 | 已內建於 ArchSim repo,Plugins/FrameSolver | 鐵則 #1 FROZEN |
| **LevelSim v1**(水準儀模擬器) | (內部 MIT) | **測量大腦**(水準儀完整實作) | 已內建於 ArchSim repo,Plugins/LevelSim;2026-06-18 PR#8 merged | **鐵則 #5 同等保護**(絕不碰),Standalone 115/115 PASS |
| **ALS-Refactored v4.17** | MIT | 角色 + 相機 | Plugins/ALS | UE5.7 已正式支援 |
| **Prefabricator UE5** | MIT | 構件放置 + 藍圖 | Plugins/Prefabricator | ✅ `.uplugin EngineVersion` 已修為 `5.7.0`(S-00) |
| **SPUD** | MIT | 持久化 | Plugins/SPUD | World Partition issue #117 |
| **SUQS** | MIT | 任務系統 | Plugins/SUQS | 無 built-in replication,自製方案 |
| **OpenBLAS / METIS / cuDSS**(已內建於 FrameCore) | BSD / OpenBSD / NVIDIA | 線性代數 | 已內建 | 不影響本系統 |
| **OpenSees**(僅 oracle) | BSD | 結構分析對標 | 開發測試用,不打包到 release | 無 |
| **SQL LRS(Yet Analytics)** | Apache 2.0 | 學習紀錄儲存 | Docker 部署,外部服務 | 學校自建 vs 雲端 |
| **Noto Sans CJK TC** | OFL(免費) | 中文字體 | Content/Fonts/ | 無 |
| **JetBrains Mono** | OFL | 等寬字體 | Content/Fonts/ | 無 |
| **Okabe-Ito 色板** | Public Domain | 色盲友善色彩 | 內部使用 | 無 |
| **Cividis colormap** | BSD | 連續量色表 | 內部使用 | 無 |
| **MctoNurbs sidecar** | (內部) | CAD 匯出(Phase 3) | TypeScript / Node.js | 子模組,獨立 repo |

---

## 附錄 C:108 課綱對應

詳見 [cur research](#);此處列核心對應表(完整詳細表見 cur research 第七節)。

### 部定必修科目對應(土木與建築群建築科)

| 課綱科目 | 學分 | 年級 | 對應 Part / 模組 |
|---|---|---|---|
| 土木建築工程與技術概論 | 2 | 高一 | 序章 + F1 沙盒 + L-00 序章關卡 |
| 製圖實習 | 8 | 高一-高二 | B2 + B3 + B4 |
| 材料與試驗 | 4 | 高一 | E4 材料狀態機 + 材料選擇系統 |
| **基礎工程力學** | **6** | **高一-高二** | **C1 載重 + C2 D/C 熱圖 + D1-D4 全套** |
| 構造與施工法 | 2 | 高一-高二 | B1 構件放置 + E1 ~ E4 工序 |
| 電腦輔助製圖實習 | 6 | 高二 | B2 2D↔3D + B4 藍圖 |
| 設計與技術實習 | 4 | 高二 | F1 沙盒自由創作 |
| 測量實習 | 6 | 高一-高三 | Part G(整支線) |
| 建築結構實習(校訂) | 2 | — | C3 崩塌 + F4 失敗即學 |
| 營建技術實習 | 6 | 高三 | E1-E4 + F5 規範 |
| 專題實作 | 8 | 高三 | 沙盒 + 多人 + 教師工具 |

### 統測對接(土木與建築群)

| 統測科目 | 範圍 | 對應遊戲機制 |
|---|---|---|
| 專業科目(一) | 基礎工程力學 + 材料與試驗 | C 全部 + D 全部 + E4 材料 |
| 專業科目(二) | 測量實習 + 製圖實習 | Part G + B2/B3/B4 |

### 108 課綱核心素養

詳見 [J5 對標 108 課綱](#j5-對標-108-課綱完整對應表)。

---

## 附錄 D:FrameCore BP API 速查

完整 API 詳見 [api research](#);本附錄是條目級速查表。

### UFrameAnalysisLibrary(15 個分析 BP entry)

| BP 節點 | 輸入 | 輸出 | 用途 |
|---|---|---|---|
| SolveLinear | FFrameModelDef + FFrameSolveOptions | FFrameSolveResult | 線性靜力分析 |
| AnalysisModal | + FFrameModalOptions(NumModes=3) | FFrameModalResult | 模態分析 |
| AnalysisBuckling | + FFrameBucklingOptions | FFrameBucklingResult | 線性屈曲分析 |
| LoadCombineEnvelope | + TArray<FFrameSizeOptLoadCase> | FFrameLoadEnvelope | 載重組合包絡 |
| InfluenceLine | + LoadNodes + ReactNode + ReactDof | FFrameInfluenceLine | 反力影響線 |
| ResponseSpectrum | + FFrameSpectrum + EFrameSpectrumCombo | FFrameResponseSpectrumResult | 反應譜分析 |
| RealTimeDynamic | + FFrameModalDynamicsOptions | FFrameModalTimeHistory | 即時動態時程 |
| ReanalysisSolve | + FFrameReanalysisOptions + DeactivateMemberIds | FFrameSolveResult | 拓樸變更重解 |
| SolvePDelta | + FFramePDeltaOptions | FFramePDeltaResult | P-Delta 幾何非線性 |
| SolveTensionOnly | + FFrameTensionOnlyOptions | FFrameTensionOnlyResult | 只受拉桿 |
| SolveSizeOpt | + FFrameSizeOptOptions + SizableMembers | FFrameSizeOptResult | FSD 尺寸優化 |
| SolveBESO | + FFrameBESOOptions + DesignMembers | FFrameBESOResult | BESO 拓撲優化 |
| SolveCorotational | + FFrameCorotationalOptions | FFrameCorotationalResult | 共轉大位移 |
| SolveArcLength | (同上,強制 bUseArcLength=true) | FFrameCorotationalResult | 弧長法 snap-through |
| SolveDynCollapse | + FFrameDynCollapseOptions | FFrameDynCollapseResult | N4 連續動力倒塌 |

### UFrameInteractiveSubsystem(互動子系統)

| 方法 | 功能 |
|---|---|
| StartSession | 建立 ReSolve Session |
| EndSession | 釋放 Session |
| IsSessionActive | 檢查是否運作中 |
| ApplyPatchAndResolve | 套用 Patch + 重解(Woodbury) |
| Rebaseline | Tier-3 重分解 |
| ResolveCurrent | 不套 Patch 直接重解 |

### 8 個 BP Actor

| Actor | 用途 |
|---|---|
| AFrameDeformedShapeActor | 幾何變形視覺化 |
| AFrameUtilizationHeatmapActor | D/C 熱圖渲染 |
| AFrameModalShapeActor | 模態振型動畫 |
| AFrameDynCollapseReplayActor | 倒塌動畫播放 + OnEventReached delegate |
| AFrameFragmentClusterActor | Chaos 碎片物理生成 |
| AFrameInfluenceLineActor | 影響線 ribbon |
| AFrameResponseSpectrumActor | 反應譜峰值包絡 |
| AFrameRealTimeDynamicActor | 即時動態 Newmark 時程 |

### UFrameModelBuilder + Material/Section Libraries

| 函式 | 用途 |
|---|---|
| ValidateModel | 模型合法性檢查 |
| LoadModelFromJson | 從 JSON 載入完整模型 |
| GetS235 / GetS275 / GetS355 / GetS460 | EN10025 鋼材 |
| GetConcreteC30 / C40 / C50 | EN1992 混凝土 |
| GetAluminum6061 | 鋁材 |
| MakeCustomMaterial | 自訂材料 |
| MakeRectangular(b, d) | 矩形截面 |
| MakeCircular(D) | 圓形截面 |

---

## 附錄 E:完整關卡清單

> MVP 目標 15 + 關卡;完整版 30+。每個關卡標準格式:**ID / 名稱 / 對應 Part / 學習目標 / 預期失敗 / 通關條件 / 課綱單元 / 預估時長**

### 主線關卡(L-XX)

| ID | 名稱 | 對應 Part | 學習目標 | 通關條件 | 課綱 | 時長 |
|---|---|---|---|---|---|---|
| L-00 | 序章:你的第一棟建築 | A2/B1 | 熟悉操作 | 蓋一根梁 | 概論 | 5 min |
| L-01 | 懸臂梁初步 | B1/C1/D2 | 撓度概念 | 撓度 < 30mm | 力學一 | 5 min |
| L-02 | 簡支梁與荷載 | B1/C1/D3 | BMD 概念 | D/C < 1.0 | 力學一 | 5 min |
| L-03 | 簡單門型框架 | B1/C1/C2 | 框架分析 | D/C < 0.85 | 力學一 | 7 min |
| L-04 | 材料選擇關 | E4 | 材料屬性 | 用最便宜材料通過 | 材料與試驗 | 8 min |
| L-05 | 載重路徑追蹤 | C4 | 力傳遞 | 識別載重路徑 | 力學一 | 8 min |
| L-06 | 載重組合 1.2D+1.6L | C1/F5 | 規範概念 | 各組合下都 < 1.0 | 力學二 | 10 min |
| L-07 | 撓度限制 L/250 | D2/F5 | 規範撓度 | 滿足撓度限制 | 力學二 | 10 min |
| L-08 | 強柱弱梁 | F5(W-06) | 耐震哲學 | 通過強柱弱梁檢查 | 力學二 | 12 min |
| L-09 | 模態振型探索 | D2 | 動力概念 | 觀察前 3 模態 | 力學二 | 8 min |
| L-10 | 反應譜地震分析 | D2 | 耐震直覺 | 地震下 < 1.0 | 力學二 + 結構實習 | 12 min |
| L-11 | RC 工序完整版 | E1-E4 | 工序紀律 | 正確完成所有 4 階段 | 構造 + 營建 | 15 min |
| L-12 | 蜂窩偵測修繕 | E2/D1 | 工法品質 | 找出所有蜂窩並修繕 | 營建 | 10 min |
| L-13 | 預測倒塌方向 | C3/F4 | 失效預測 | 觀察倒塌動畫 + 反思 | 結構實習 | 10 min |
| L-14 | 結構優化挑戰 | F5/D4 | 設計優化 | D/C 在 0.7-0.9 區間 | 設計實習 | 15 min |
| L-15 | 期末綜合設計 | 全部 | 整合應用 | 完成多層框架 + 通過所有檢查 | 專題實作 | 30+ min |

### 測量支線關卡(S-XX)

詳見 [G5 測量任務題型](#g5-測量任務題型8-種關卡)。簡列:

| ID | 名稱 | 儀器 | 課綱 |
|---|---|---|---|
| S-01 | 高差計算 | 水準儀 | 測量實習 |
| S-02 | 閉合水準路線 | 水準儀 | 測量實習 |
| S-03 | 建物角隅放樣 | 水準儀 | 測量實習 |
| S-04 | 橫斷面測量(可串主線) | 水準儀 | 測量實習 |
| S-05 | 水平角測量 | 經緯儀 | 測量實習 |
| S-06 | 導線測量 | 經緯儀 | 測量實習 |
| S-07 | 建物放樣 | 經緯儀 | 測量實習 |
| S-08 | 誤差診斷關 | 水準儀手簿 | 測量實習 |

---

## 附錄 F:xAPI Verb 詞彙表

### 繼承自 xAPI-SG Profile

| Verb | URI | 使用情境 |
|---|---|---|
| initialized | http://adlnet.gov/expapi/verbs/initialized | 進入關卡/沙盒 |
| completed | http://adlnet.gov/expapi/verbs/completed | 完成關卡 |
| progressed | http://adlnet.gov/expapi/verbs/progressed | 關卡進度推進 |
| interacted | http://adlnet.gov/expapi/verbs/interacted | 點擊構件查看細節 |
| accessed | https://w3id.org/xapi/seriousgames/verbs/accessed | 查看熱圖/變形圖 |
| selected | https://w3id.org/xapi/adb/verbs/selected | 選取構件類型/材料 |

### 自訂(ArchSim Application Profile)

| Verb | URI | 記錄重點 |
|---|---|---|
| placed-member | https://archsim.edu.tw/verbs/placed-member | type, position, section, material |
| removed-member | https://archsim.edu.tw/verbs/removed-member | 移除原因 |
| applied-load | https://archsim.edu.tw/verbs/applied-load | 大小、方向、位置 |
| observed-failure | https://archsim.edu.tw/verbs/observed-failure | 失敗模式 + 觀察秒數 |
| fixed-by-adding | https://archsim.edu.tw/verbs/fixed-by-adding | 修正策略 + D/C 改善量 |
| asked-for-hint | https://archsim.edu.tw/verbs/asked-for-hint | 第幾次求助 + 提示類型 |
| ran-simulation | https://archsim.edu.tw/verbs/ran-simulation | Solve 耗時 + 是否收斂 + Max D/C |
| reflected | https://archsim.edu.tw/verbs/reflected | 反思文字內容 |

### 完整 xAPI Statement schema 範例見 [log research 第一節](#)

---

## 附錄 G:工時總表(MVP 6 個月路徑)

| Phase | 內容 | 預估 |
|---|---|---|
| Phase 0 | 環境準備 + plugin 整合 | 1 週 |
| **Phase 1 — MVP**(以下子任務皆 Phase 1 內) | | **~3 個月** |
| A1 FrameCore 接合 | UArchSimMemberData + Registry | 2 週 |
| A2 ALS 角色 | Character + Enhanced Input | 1 週 |
| B1 構件放置 | Prefab + Snap + Server 仲裁 | 3 週 |
| C1 載重施加 | 4 種載重類型 | 2 週 |
| C2 即時 D/C 熱圖 | AFrameUtilizationHeatmapActor 整合 | 1 週 |
| F1 沙盒模式 | 單機版 | 1 週 |
| F2 關卡系統 | SUQS DataTable + 3 個關卡 | 2 週 |
| MVP 整合測試 + 學生試玩 | | 2 週 |
| **Phase 2 — 完整教育體驗** | | **~6 個月** |
| C3 崩塌 + Chaos | DynCollapseReplay + FragmentCluster | 3 週 |
| C4 連通性 + 載重路徑 | StructureGroupSubsystem | 2 週 |
| B2 2D↔3D 四象限 | | 4 週 |
| B3 圖學標註 | | 2 週 |
| B4 藍圖系統 | Prefabricator runtime | 2 週 |
| D1-D4 診斷工具 | | 3 週 |
| E1-E4 施工工序 | | 4 週 |
| F3-F5 學習閉環 | | 3 週 |
| H1-H4 多人協作 | | 5 週 |
| I1-I4 教師工具 + Log | SQL LRS + dashboard | 6 週 |
| Part G 測量關卡(LevelSim 整合 + 經緯儀新做)| 水準儀已有 LevelSim;經緯儀從零;PCG 整合 | 4 週(原 6 週,LevelSim 節省 53h)|
| Phase 2 整合測試 + 試教 | | 2 週 |
| **Phase 3 — 進階與發布** | | **~3 個月** |
| CAD 匯出整合(MctoNurbs sidecar) | | 3 週 |
| PCG 地形完整 + 多語在地化 | | 3 週 |
| 效能優化 + Nanite | | 4 週 |
| 老師備課文檔 + LRS 完整部署 | | 2 週 |
| Release build 5-leg gate + 發布準備 | | 2 週 |
| **總計** | | **~12 個月**(1-2 人團隊) |

---

## 附錄 H:Block Reality → UE5+FrameCore 完整對應表

完整 17 個概念對應見 [mapping research](#附錄-h-block-reality--ue5framecore-完整對應表);此處列摘要,完整詳細策略見 mapping research 中各 ADAPT 案例。

| # | Block Reality 概念 | 是否保留 | UE5+FrameCore 對應 | FrameCore 已實作 |
|---|------|---|---|---|
| A | R氏材料(Rcomp/Rtens/Rshear/density) | ADAPT | FFrameMaterial USTRUCT + UFrameMaterialLibrary | ✅ |
| B | RBlock + BlockEntity | ADAPT | UArchSimMemberData + AFrameUtilizationHeatmapActor | 部分(熱圖已實作) |
| C | 26-connectivity Union-Find | ADAPT | UFrameStructureGroupSubsystem | ⚪(待實作) |
| D | 錨定連續性 BFS | ADAPT | UFrameStructureGroupSubsystem::BFS + prescribedDOF | 部分(prescribedDOF 已有) |
| E | RC 節點融合 | ADAPT | UFrameRCMaterialHelper | ⚪(UE 端 helper) |
| F | SPH 應力場 | ADAPT(改 FEM) | UFrameCoreStressFieldLibrary | ✅(已實作) |
| **G** | **應力熱圖渲染** | **KEEP** | **AFrameUtilizationHeatmapActor** | ✅ |
| H | 應力掃描儀 | ADAPT | AArchSimScannerTool(D1) | 部分(資料來源已有) |
| I | 工序狀態機 | ADAPT | UFrameConstructionStateMachine(E1) | ⚪ |
| **J** | **全息投影** | **KEEP** | **Prefabricator ghost preview** | ✅(via plugin) |
| K | 鋼索物理 | **DROP** | (UE5 Cable Component 視覺裝飾,不接結構) | — |
| L | CLI 指令 | ADAPT | Grasshopper bridge + UE console + SUQS | ✅(Grasshopper 已有) |
| M | 藍圖打包 NBT+GZIP | ADAPT | SPUD SaveGame + JSON 模型 | — |
| N | TypeScript Sidecar | KEEP | MctoNurbs sidecar 保留 | ✅(子模組) |
| O | 快照層(Snapshot) | ADAPT | UFrameInteractiveSubsystem ReSolveSession | ✅(已實作) |
| P | 蜂窩弱點 | ADAPT | UFrameConstructionStateMachine + FFrameModelPatch | ⚪ |
| Q | 養護計時 | ADAPT | UFrameConstructionStateMachine + FTimerHandle | ⚪ |

(完整 17 row + 6 個 ADAPT 詳細整合策略見 [mapping research](#附錄-h-block-reality--ue5framecore-完整對應表))

---

## 附錄 I:重要設計理念紀錄

### I.1 為什麼從 PFSF 轉到 FrameCore(Karamba-parity FEM)?

Block Reality 設計者(本作者)在 2026 年初開始 PFSF(Phase-Field Stress Field)的研究,試圖用 GPU 細胞機式擴散場做結構模擬。**PFSF 在概念上很美**(從電壓場、梯度向量推導力傳導),也成功在 Minecraft Forge 1.20.1 環境跑通約 30 個 commits、6500 行新代碼、7 個測試類別。

但在深入研究後發現**數學上的根本限制**:純量場(`φ` 值)的梯度永遠是無旋場(`curl(grad φ) = 0`),這意味著 PFSF **算不出純剪力與扭轉**。對於建築結構模擬(尤其鋼結構與板殼)而言,扭轉與剪力是常見且重要的內力,PFSF 在數學上**無法成為工程嚴謹的結構引擎**。

於是專案路線整個重做:
- **棄用** PFSF + Minecraft Forge + Java 整個工程堆疊
- **採用** FrameCore(基於有限元素法 FEM,完整 EB+Timoshenko 梁 + MITC4 板殼)+ UE5 + C++
- **保留** 教育閉環、學習設計、視覺化方案等 Block Reality 設計理念

這個決定符合 CLAUDE.md 鐵則 #3「誠實驗證、不過度宣稱」 — 寧可走更嚴謹的路徑,也不要用炫但不對的方法誤導學習者。

### I.2 為什麼用 FrameCore 自己寫 supernodal,而不用 CHOLMOD?

CHOLMOD(SuiteSparse)是業界最成熟的稀疏 Cholesky 求解器,效能極好。但它的 license 是 LGPL/GPL,**會感染整個 FrameCore 為 GPL**,違反「保持 license 乾淨」的目標。MUMPS 與 PARDISO 同樣有依賴或授權問題。

解法:**自建 supernodal Cholesky**(sn_chol.h),利用 OpenBLAS(BSD)做 BLAS3 kernel,把 CHOLMOD 當作 oracle 對標而非產品依賴。整套自建程式碼 ~2-3 個月開發成本,但**換來 license 乾淨 + 可發表 + 知識完全在內**。完整 R-line 研究紀錄見專案內部 memory(離線文件,非 repo 一部分)。

### I.3 為什麼把 connectivity 從「死碼」變成有用?

FrameCore 早期版本曾經實作過 connectivity 模組,但當時沒接上任何用途,被當作死碼刪除。

**本系統的 C4 連通性與載重路徑** 是給 connectivity 一個真實用途(BFS 錨定判定 + 載重路徑視覺化 + 倒塌交棒)。這正好對應 PROJECT.txt day2 設計的「同樣的技術,差別只在於有沒有真實用途 + oracle」原則。**避免重蹈覆轍的關鍵不是「不做」,而是「做之前先想好用途」**。

### I.4 「特洛伊木馬」戰略遺產

Block Reality 開發期曾考慮「50% 外包 C++ 戰略」 — 把計算核心做成 C++ dll,用 Minecraft 當免費測試外殼,最終長出獨立 OS / 引擎。這個想法在 architect simulator 路線上以另一種形式實現:

- **FrameCore 本身是獨立可發表的純 C++17 引擎**(已 FROZEN,5-leg gate 已綠)
- **UE5 architect simulator 是 FrameCore 的第一個 consumer 應用**
- **未來可有更多 consumer**:Grasshopper bridge、獨立 CAD 工具、其他教育產品、商業結構分析軟體

「特洛伊木馬」的戰略目標已達成 — 結構大腦獨立於體驗外殼,可分別演進。

---

*本主計畫書共 12 個 Part(A 到 L)+ 序章 + 可行性審查 + 10 個技術決策 + 9 個附錄,總共 ~25,000 字。整體實作工時 1899h(2026-06-25 v0.1 release-hardening 後修正,原估 1952h,因 LevelSim 已實作 Part G 節省 53h)。*

*版本控制:`docs/ARCHITECT_SIM_MASTER_PLAN.md`(repo-relative),持續更新。本主檔是設計層權威來源,實作層細節見 [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md)。*

---

## 配套文件

- **[IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md)** — 實作計畫(實作層展開,~440KB / 7648 行)
  - 12 個 Part 的 sub-task 詳細拆解(每個 ≤4 小時,共 1899 工時)
  - 16 個 Sprint 計畫(S-00 ~ S-15,32 週 / 8 個月路徑)
  - Cross-Review 8 個衝突 + 5 個高風險 + 整體評估
  - 人工審核序章(我對 cross-review 的接受/裁定)

- **[CLAUDE.md](../CLAUDE.md)** — FrameCore 引擎現況與鐵則
- **[ARCHITECTURE.md](ARCHITECTURE.md)** — FrameCore 引擎架構文檔
- **[FrameCoreUE_QuickStart.md](FrameCoreUE_QuickStart.md)** — UE consumer 端 quick start

*── 文件結束 ──*

---

*本文件由 architect-sim-master-research workflow(10 個維度平行 deep research,~11M tokens)+ 人工統整撰寫,版本控制於 git。*
