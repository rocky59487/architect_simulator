# S7 進度日誌 — 拓撲優化 BESO + N2 倒塌韌性約束

> 接續 `PROGRESS_S6.md`(S6 引擎端結案於 `e7f3b45`,起點錨點 `7894d1e`)。本階段加入 evolutionary
> hard-kill 拓撲優化(`runBESO`)+ N2 倒塌韌性約束,在 `Member.active`(S1 移除設施)上**零侵入**。
> spec = `docs/specs/S7_beso.md`。研究依據 WS_I(BESO)+ WS_N §N2(先行技術定位)+ 原型 `Research/WS_I_beso/`。

## 基準
- 起點 `7894d1e`(S6)。working tree 既有雜項(`.gitignore`/`ArchSim.uproject`/`Plugins/LevelSim/`)**非本人改動,全程不碰**。
- baseline 五腿綠:standalone F1–F44 / UE 43 / OpenSees PASS / audit 79 / CLI round-trip 8 checks(動工前 `run_gate.ps1 -RequireOpenSees` 實證 GATE: PASS)。
- 使用者授權「S7 一次做完」(spec + BESO 核心 + N2 單一 commit,全綠才停)。

## 範圍決策(誠實)
- **做**:`runBESO` evolutionary hard-kill BESO(零侵入 `Member.active`)+ **必修三項 + compliance-best 回退 + N2 約束**(見下)。
- **不做**:soft-kill SIMP(另一族)、**雙向加回**(hard-kill 移除桿無內力可估敏感度 → 初版單向 ESO,誠實標)、應力約束 BESO、殼 BESO(`ShellQuad.active` 機制就緒、Michell oracle 是桁架 → oracle 待補)、LP/SDP 全局最優、Karamba GroupIds。

## 交付內容(單一 S7 commit)
### 新增
- `Public/FrameCore/Topology.h` + `Private/Topology.cpp` — `runBESO` + public `memberStrainEnergy`(sensitivity = 元素應變能,也供 C7 能量場可視化)。
- `Private/Tests/TopologyTest.cpp` — UE automation `FrameCore.Topology.BESO`(能量平衡 + BESO 移除/best 非奇異 + N2 對比)。
- `docs/specs/S7_beso.md` — 十項交接規格。
- `main.cpp` F45–F47;`linear_deep_audit.cpp` `testBESO()`(+3 checks)。

### 修改
- 四 build 腳本(`build.bat`/`build_linear_audit.bat`/`build_cli.bat`/`build_capi.bat`)補 `Topology.cpp`(維持「鏡像 build.bat 全集」不變量;cli/capi 初版不調 runBESO,純鏈接全集)。
- `Scripts/run_gate.ps1`:`$ExpectedUeTests` 43→44(+1 Topology BESO)。
- **不動** `modelFingerprint`(複用 `Member.active`,已在 fingerprint;BESO 不加 model 旗標,同 S5 SizeOpt)。

## 演算法摘要
- **sensitivity** `α_e = ½ u_eᵀK_e u_e` = 元素應變能,經 recover 的 local 端力·端位移重建。Q=k_local·u_local
  (無 UDL),`recover()` 存 endI=Q[0..5]、endJ=(−Q[6],Q[7..11])(壓正翻號**僅** endJ.N)→ end-J 軸項取
  −endJ.N。**Σα_e == ½·F·u 能量平衡硬 oracle**(全 1 權重)。可暴露 4 分量權重(Karamba WTension/.../WMoment)。
- **必修①** 敏感度兩步歷史平均 `ᾱ=(α_k+α_{k−1})/2`(Huang & Xie,抑振盪)。
- **必修②** compliance 跳變停機 `C_k > complianceJumpTol·C_{k−1}`(原型尾段暴增 52×)。
- **必修③** 機構守門 = connectivity(載重點/碎塊)+ **N1 ReSolveSession capacitance 快篩 + fresh `solve` 確認**
  (ReSolve `mechPivotTol` 與 fresh `pivotTol` 容差不同,planar 邊緣機構由 fresh 兜底)。
- **必修④** compliance-best 回退:輸出 `bestActive`(暴增前可行拓撲)。
- **N2**:每 `redundancyCheckEvery` 步以 `runProgressiveCollapse` 抽查單桿移除情境,任一 `Collapsed` →
  回滾該步 batch + 鎖該批桿(protected);FNV-1a 狀態哈希守門(回滾循環)保證有限終止。

## 數值證據(本輪實測)
| Oracle | 量測 | 門檻 | 實測 |
|---|---|---|---|
| F45 能量平衡 | Σα_e vs ½·F·u(3D L-frame) | rel<1e-9 | **4.13e-14** |
| F45 懸臂退化 | α_single vs ½·F·u | rel<1e-9 | **4.48e-16** |
| F45 逐分量解析 | 純軸/彎/扭 vs P²L/2EA、M²L/2EIz、T²L/2GJ | rel<1e-5 | **rel=0**(機器精度) |
| F46 BESO 移除 | ground structure(81 桿)移除 ≥25% | ≤75% active | **81→40** |
| F46 體積達標 | volFrac 至 target 附近 + 乾淨終止 | ≤target+evolRate | **0.503**(Stalled) |
| F46 best 非奇異 | bestActive fresh solve | !singular | **PASS** |
| F46 compliance | 移除半體積變柔 | C_f≥1.15·C_0 | **354→528(1.49×)** |
| F47 無約束脆弱 | ∃單桿移除 Collapsed | true | **PASS** |
| F47 約束韌性 | ∀單桿移除 Stable + protected≠∅ | true | **PASS**(protected=1,unc=2 rob=3) |
| audit BESO 能量平衡 | portal frame Σα=½C | rel<1e-9 | **5.68e-14** |
| audit N2 對比 | robust-safe ∧ unc-fragile | =1 | **PASS** |
| audit 機構守門 | best fresh 非奇異 | 0=ok | **PASS** |

## 五腿 gate(commit 前)
`run_gate.ps1 -RequireOpenSees` → **GATE: PASS** — standalone **F1–F47** / UE **44**(+`FrameCore.Topology.BESO`)/
OpenSees **PASS** / deep audit **checks=82**(+3 BESO)/ **CLI round-trip ALL PASS**。
⚠️ 踩雷:① UE 測試常數勿用 `IN/OUT`(SAL 巨集)→ 直接值;② F47/N2「脫地→Collapsed」機制需**完全 free**
的中間/載重節點(任何 fixed DOF 都被 `analyzeConnectivity` 當 grounded,破壞脫地測試)→ 用兩真支座 + free
M/C 靠 3D beam 自穩;③ planar 邊緣機構 ReSolve capacitance 與 fresh solve 容差分歧 → 守門加 fresh 確認。

## 誠實邊界 / novelty 定位
- BESO = **heuristic 近似**(非全局最優;桁架全局最優 = LP/SDP ground structure);線彈性 sensitivity;
  **初版單向 hard-kill**(雙向加回留後續);殼 BESO 機制就緒、oracle 待補。
- **sensitivity 對 UDL-free 桿精確(能量平衡 + 逐分量 [VERIFIED]);有 UDL 桿 `memberStrainEnergy` 算
  ½Qᵀu≠½uᵀKu 為近似 screen,偏差無定量 oracle [UNKNOWN]**(header 已警告)。
- **收斂準則**:`converged`/`TargetReached` 僅表示體積達標,**非** Huang & Xie 2N 窗 compliance 穩定準則
  (**未實作** [NOT IMPLEMENTED];離散桿常卡 target 上方一點 Stalled)。
- compliance 跳變門檻是工程啟發值(暴增現象有文獻佐證,具體 52× 數字無對應文獻 [UNKNOWN])。
- **N2 = LSP 級韌性評估,非真非線性倒塌;勿自稱 "fail-safe"**。定位(WS_N §N2):**最近先行技術 = fail-safe
  桁架 TO(Stolpe 2019 / Zhu 2023 離散構件單桿移除損傷;Jansen 2014 連續 SIMP 原版)**;差異**僅在評估器**
  ——LSP 序列線性倒塌驅動器(Stable/Collapsed + dlf + 碎塊連通)取代塑性/conic LP。**非新方法、非全局最優保證**。
- **分級**:`[VERIFIED]`(文獻方法實作)= 敏感度歷史平均(Huang & Xie 2007)+ compliance-best 回退(WS_I 做法 A);
  `[NEW CODE]`(工程整合非原創)= N2 抽查回滾鎖保護 + ReSolve capacitance 守門整合。

## 效能(誠實)
- 每步重解走 ReSolve(Tier-1 Woodbury;累積 rank>96 自動 Tier-3 rebaseline,BESO 移除多 → 頻繁 rebaseline)。
- 機構守門每候選 capacitance 快篩(快),存活者付一次 fresh `solve(work)`(最壞每候選一次全分解)。
- ⚠️ bad() 的 tentative 移除→回滾在 ReSolve ladder 留 false→true 兩個 rank-update(無法撤銷),高密度移除
  加速逼近 maxRank 觸發 rebaseline(效能退化,非正確性;審核揭露,記著)。
- **N2 抽查每次 O(samples × collapse_steps × n_member) 次 fresh solve,昂貴**(WS_N §N2 [THEORY]);預設關閉
  (純 BESO),`redundancyCheckEvery`/`redundancySamples` 可調。

## 已知 MINOR(可順手修或記著)
- BESO 殼路徑未實作(`ShellQuad.active` 機制就緒,sensitivity/oracle 待補)。
- N2 抽查情境集預設全 active design(成本高);`redundancySamples` 限 top-D/C 可降本但犧牲覆蓋。
- compliance-best 回退在「全程無暴增」時 bestActive == finalActive(達標步)。

## 下一步(待使用者授權)
F 編號下一個 = **F48**;audit 從 **82** 起;UE 從 **44** 起。**S7 完成**。
後續主線:**S8 殼路線**(QM6 opt-in 膜 → DKQ 薄板)/ S9 co-rotational / S10 N-M 塑鉸(S9 後)/ S11 MITC9i(殿後)。
並行:殼 BESO、BESO 雙向加回、C6–C8 可視化(複用 `memberStrainEnergy` 能量場)。見 `docs/AGENT_PROMPT_S5_S11.md`。
