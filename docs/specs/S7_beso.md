# S7 交接規格 — 拓撲優化 BESO + N2 倒塌韌性約束(實作就緒)

> 研究輪驗證:hard-kill BESO 在 `Member.active`(S1 移除設施)上零侵入成立;原型
> `Research/WS_I_beso/exp_beso_truss.cpp`(12×6 ground structure、軸力 sensitivity、connectivity
> 守門 + per-member fresh re-factor 機構檢查)。文獻 WS_I(Huang & Xie 2007/2010、Zuo & Xie 2016
> 連通約束、compliance 暴增已知問題)、WS_N §N2(先行技術定位:類 fail-safe TO 但離散框架 + LSP 評估器)。
> 數據 WS_R2 §8。**原型實測踩雷:砍到 30% 體積時 compliance 暴增 52× → 沒停機會劣化**。

## ① 目標 / 不做

**做**:`runBESO` evolutionary hard-kill 拓撲優化(逐步停用低敏感度 member,零侵入 `Member.active`),
含**必修三項 + 研究強烈建議的回退**:
- ① **敏感度兩步歷史平均** `ᾱ_e^k = (α_e^k + α_e^{k-1})/2`(Huang & Xie,抑離散 0/1 振盪)。
- ② **compliance 跳變停機** `C_k > complianceJumpTol · C_{k-1}` → 停機(原型尾段暴增 52× 守門)。
- ③ **機構守門走 N1 capacitance(快篩)+ fresh 確認**:移除後 `ReSolveSession::solve()` 的 `singular`
  (capacitance 奇異 = 機構,Woodbury 增量)為**快篩**;因 ReSolve 的 `mechPivotTol` 與 fresh `solve`
  的 `pivotTol` 容差不同,planar 邊緣機構(pivot 落在兩容差之間)可能滑過 capacitance,故對快篩存活者
  再以一次 fresh `solve` **確認**(原型是每候選都 fresh;此處快篩擋掉多數,僅存活者付 fresh 成本)。
- ④ **compliance-best 回退**:輸出歷史「未發生暴增跳變」的拓撲(`bestActive`),非最終拓撲(WS_I 做法 A)。
- **N2 倒塌韌性約束**(opt-in):每 `redundancyCheckEvery` 步以 `runProgressiveCollapse` 抽查單桿移除
  情境,任一情境 `Collapsed` → 回滾該步移除 + 鎖該批桿為 protected(永不可刪)。

**不做**:
- **soft-kill SIMP**(連續密度 + 罰函數,屬另一族;Karamba BESO-for-Beams 用 soft-kill 降剛度而非刪,
  本初版用 hard-kill `active` 旗標保零侵入,雙向加回因此不做 → 見下)。
- **雙向加回(Bi-directional 的「+」)**:hard-kill 移除桿後不在 K 內、無內力可估敏感度,加回需周圍場
  插值或 soft-kill;**初版為單向 evolutionary removal(ESO 式)**,雙向留後續(誠實標,名稱仍循專案沿用「BESO」)。
- **應力約束 BESO**(初版剛度/compliance 目標)、**殼 BESO**(`ShellQuad.active` 機制就緒、Michell oracle
  是桁架 → 殼 sensitivity oracle 待補,留後續)、**LP/SDP 全局最優**(BESO 是 heuristic 近似)、
  **Karamba GroupIds 群組約束**(需額外 group→member 映射)。

## ② 公開 API

```cpp
// Public/FrameCore/Topology.h(新檔)。POD/std only — 零 Eigen、零 UE。複用 Member.active(已在
// fingerprint),不加 model 旗標 → 不動 modelFingerprint(同 S5 SizeOpt 不動 fingerprint)。
namespace frame {

struct BESOOptions {
    real targetVolFrac = 0.5;       // 目標殘留體積/初始體積(Σ A_e·L_e,僅 design members)
    real evolRate      = 0.02;      // 每步移除體積配額 = evolRate · 當前體積(典型 0.02~0.05)
    int  maxIter       = 200;

    bool sensHistory          = true;  // 必修①:敏感度兩步歷史平均
    real complianceJumpTol    = 2.0;   // 必修②:C_k/C_{k-1} 超此 → 停機(單步暴增守門)
    bool complianceBestRollback = true; // 必修④:輸出 bestActive(暴增前)而非 finalActive

    // sensitivity 分量權重(Karamba WTension/WCompr/WShear/WMoment 對應;全 1 = 全元素應變能)。
    // ⚠️ 非全 1 時能量平衡 Σα = ½C 不再成立(oracle 用全 1)。
    real wAxial = 1, wBending = 1, wShear = 1, wTorsion = 1;

    // N2 倒塌韌性約束(0 = 關閉,純 BESO)。
    int  redundancyCheckEvery = 0;     // 每 N 步抽查;0 關閉
    int  redundancySamples    = 0;     // 抽查情境數 = 當前 D/C 最高的前 m 根 design 桿;0 = 全 active design
    CollapseOptions redundancy;        // 抽查用 collapse 選項(dlf / removeThreshold 等透傳)

    SolveOptions solve;                // 透傳 baseline assembleAndFactor / ReSolve / collapse
};

enum class BESOStop { TargetReached, ComplianceJump, Stalled, Mechanism, MaxIter, Invalid };

struct BESOResult {
    std::vector<char>     finalActive;     // 停機時 member active(model.members order,1/0)
    std::vector<char>     bestActive;      // compliance-best 回退拓撲(== finalActive 若全程無暴增)
    std::vector<real>     volFracHistory;  // 每迭代殘留體積比
    std::vector<real>     complianceHistory; // 每迭代 C = Σ_nodalLoads F·u(外力功)
    std::vector<MemberId> protectedMembers; // N2 鎖定保護的桿(ascending)
    int      bestIter   = -1;              // bestActive 對應迭代(1-based)
    int      iterations = 0;
    BESOStop reason     = BESOStop::Invalid;
    bool     converged  = false;           // true 當 reason==TargetReached(乾淨達標)
};

// Evolutionary hard-kill 拓撲優化。designMembers = 可移除 member 索引(空 = 全 active screenable
// member);不在此集的桿固定保留(支座連桿等)。caller model 永不 mutate(內部工作副本)。
FRAMECORE_API BESOResult runBESO(const FrameModel& model, const BESOOptions& opts,
                                 const std::vector<int>& designMembers = {});

} // namespace frame
```

## ③ 資料流 / ④ 演算法

```
工作副本 work = model;  ReSolveSession sess(work)        // 同 topology 增量 + capacitance 機構守門
design = designMembers(空→全 active screenable);  protected = {}(N2 鎖定)
totalVol0 = Σ_{e∈design active} A_e·L_e
prevAlpha = ∅                                            // 歷史平均用
best = { active 快照, C, iter }(C 最小且未暴增者)

迴圈 it = 1..maxIter:
  r = sess.solve()                                       // Woodbury 增量(累積 rank>maxRank→Tier-3 rebaseline)
  若 r.singular → reason=Mechanism;break(回退 best)
  C = Σ_nodalLoads Σ_d load.comp[d]·u[gdof(node,d)]      // 外力功(compliance)
  記錄 (volFrac, C)

  // 必修②:單步暴增 → 停機,輸出暴增前拓撲
  若 it>1 且 C > complianceJumpTol·C_{prev} → reason=ComplianceJump;break(best=上一可行步)
  else 更新 best(此步可行 → bestActive=當前 active,bestIter=it)

  若 volFrac ≤ targetVolFrac → reason=TargetReached;converged=true;break

  // 1) sensitivity:元素應變能 α_e = ½ d_e^T K_e d_e,經 local 端力·端位移重建(分量加權)
  for e∈design active:  α_e = ½·Σ_end (wAxial·N·dx + wShear·(Vy·dy+Vz·dz)
                                       + wTorsion·T·rx + wBending·(My·ry+Mz·rz))_local
  // 必修①:兩步歷史平均
  若 sensHistory 且 prevAlpha 存在:  ᾱ_e = (α_e + prevAlpha_e)/2;  prevAlpha = α_e

  // 2) 移除配額:按 ᾱ 升冪,移除最低敏感度桿直到體積配額 quota=min(evolRate·vol, vol−target·vol0)
  排序 design active by ᾱ 升冪
  batch = ∅
  for (a,e) in 排序:
    若 removedVol ≥ quota break
    若 e∈protected continue
    tentatively: work.active[e]=false; sess.setMemberActive(id,false)
    // ③ 機構守門 = connectivity(載重點/碎塊)+ capacitance 快篩 + fresh 確認(彎曲/邊緣機構)
    cr = analyzeConnectivity(work)
    bad = (載重節點 ∈ cr.looseNodes 或 ∈ 任何 cr.detached)
          或 sess.solve().singular          // capacitance 快篩(快)
          或 solve(work).singular            // fresh 確認(容差邊緣)
    若 bad: 回滾(work.active[e]=true; sess.setMemberActive(id,true)); continue
    removedVol += A_e·L_e; batch.push(e)
  若 batch 空 → reason=Stalled;break

  // N2:每 k 步抽查倒塌韌性
  若 redundancyCheckEvery>0 且 it%redundancyCheckEvery==0:
    情境集 = 全 active design(或 D/C top redundancySamples)
    不韌性 = ∃ s∈情境集: runProgressiveCollapse(work, {initialRemovals={s}, redundancy}).outcome==Collapsed
    若 不韌性:
      回滾整個 batch(work.active=true; sess.setMemberActive true);  protected ∪= batch
      // FNV-1a 狀態哈希守門:回滾+鎖可能造成 active 狀態循環 → 重現即 reason=Stalled 有限終止
      若 hashState(work.active) 已見 → reason=Stalled;break

reason 未定 → MaxIter
回填 finalActive / bestActive(complianceBestRollback ? best : final)/ protected(ascending)
```

收斂性質誠實定位:單向 evolutionary removal **無全局最優保證**(heuristic);敏感度兩步平均抑振盪;
**有限終止**由「體積單調下降 + FNV-1a 狀態守門(N2 回滾循環)+ maxIter」三重保證。compliance 在
單向移除中**單調緩升**(材料漸少更柔),暴增點由跳變守門攔截(非「下降」——AGENT_PROMPT 措辭按
bi-directional 寫,本初版單向故 oracle 改驗「緩升段 + 暴增攔截 + 能量平衡」,見 ⑥)。

## ⑤ 檔案

- 新 `Public/FrameCore/Topology.h` + `Private/Topology.cpp`。
- `Private/Topology.cpp` include:`FrameSolver.h`(solve/PreparedSystem)、`Reanalysis.h`(ReSolveSession)、
  `Connectivity.h`(analyzeConnectivity)、`Collapse.h`(runProgressiveCollapse,N2)、`ElasticAllowable.h`
  (worstUtilization / memberDC,N2 情境排序)、`MemberGeometry.h`(memberLocalAxes,sensitivity local 轉換)。
  **Eigen 零洩漏**(只走既有 POD API,無 `#include <Eigen/...>`)。
- **不動** `modelFingerprint`(複用 `Member.active`,已在 fingerprint;BESO 不加 model 旗標)。
- **不動** `FrameModel.cpp validate`(無新 model 欄位)。
- **build 同步**:`Topology.cpp` 加進 `build.bat` + `build_linear_audit.bat`(SizeOpt.cpp 之後);為維持
  「鏡像 build.bat 全集」不變量,亦加進 `build_cli.bat` + `build_capi.bat`(cli 初版不調 runBESO,純鏈接全集)。
- **gate 同步**:F45–F47;`run_gate.ps1 $ExpectedUeTests` 43→44(+1 UE 測試);audit +3~4 checks(79→82~83)。

## ⑥ Oracle(誠實分級)

- **F45 — 能量平衡 + sensitivity 正確性 `[VERIFIED]`**:3D L-frame(載重含力+力矩,使軸/剪/彎/扭
  四分量全非零),對 public `memberStrainEnergy` 驗 **Σα_e == ½·Σ_nodalLoads F·u**(全應變能恆等)。
  這是 sensitivity 重建公式的硬獨立 oracle(α 從內力+位移,F·u 從外載+位移)。**實測 rel=4.13e-14**
  (機器精度)+ 單軸懸臂退化 α_single == ½·F·u(rel=4.48e-16)+ **逐分量解析 oracle**:純軸/彎/扭懸臂
  各對閉式 P²L/2EA、M²L/2EIz、T²L/2GJ(rel=0 機器精度)——**釘死各權重通道係數,破能量平衡總和無法偵測的
  對稱符號錯誤**(總和守恆是必要非充分條件)。
- **F46 — evolutionary 移除 + 量化體積/compliance + best 回退 `[VERIFIED]`**:NX=6,NZ=3 ground
  structure(**81 members**;原型研究輪為 12×6)跑 runBESO 到 targetVolFrac=0.5;**量化**檢查:① 體積驅到
  target 附近(volFrac ≤ target+evolRate)且乾淨終止(TargetReached,或離散桿卡 target 上方一點 Stalled —
  皆合理)② 移除 ≥25% 桿(實測 81→40)③ `bestActive` fresh solve 非奇異(載重路徑完整)④ compliance 上升
  **≥15%**(移除半體積 354→528,結構確實變柔;原 0.999 容差連「沒變」都過,太弱)。**「對角斜交 Michell
  骨架」是人工視覺觀察,非 gate 項目 [NOT GATED]**。
- **F47 — N2 倒塌韌性對比(可構造、確定性)`[VERIFIED]`**:小框架「主傳力桿 + 平時不受力的備援桿」。
  - 無約束 `runBESO`(redundancyCheckEvery=0)→ 砍掉備援 → 最終拓撲在 `initialRemovals={主桿}` 情境下
    `runProgressiveCollapse.outcome == Collapsed`(**不通過**抽查)。
  - 韌性約束 `runBESO`(redundancyCheckEvery=1)→ 鎖備援(protectedMembers 含之)→ 同情境
    `outcome == Stable`(**通過**抽查)。
- **audit(linear_deep_audit)+3**:(a) BESO 能量平衡 Σα=½C 獨立重算;(b) N2 約束解 vs 無約束解的
  protected 差異 + 倒塌抽查對比;(c) sensitivity 歷史平均單調性 / 機構守門(移除致機構必回滾)。

## ⑦ Gate

F45–F47;UE `FrameCore.Topology.BESO`(能量平衡 + Michell 定性,**勿用 `IN`/`OUT` 常數名 = SAL 巨集**);
audit +3(79→82);四 build 腳本補 `Topology.cpp`;`$ExpectedUeTests` 43→44。**commit 前五腿全綠**。

## ⑧ 效能驗收

每步重解走 ReSolve:Tier-1 Woodbury 增量(累積 rank ≤ maxRank=96 ≈16 beams),超出自動 Tier-3 rebaseline
(BESO 移除多 → 頻繁 rebaseline,誠實標)。**機構守門:capacitance 快篩(ReSolve,快)→ 快篩存活者再付一次
完整 `solve(work)`(full LDLT);最壞每候選一次全分解(比「Woodbury 快」貴)**。⚠️ **bad() 的 tentative
移除→回滾在 ReSolve ladder 留下 false→true 兩個 rank-update(無清晰語意可撤銷),高密度移除加速逼近 maxRank
觸發 rebaseline(效能退化,非正確性)**。**N2 抽查每次 O(samples × collapse_steps × n_member) 次 fresh
solve,昂貴**;整體 O(n_iter/k × samples × n_member) 量級(WS_N §N2 [THEORY]),大型問題需快取/近似 →
誠實記錄,redundancyCheckEvery/Samples 可調。

## ⑨ 誠實邊界 / novelty 定位

- BESO = **heuristic 近似**,非全局最優(桁架全局最優 = LP/SDP ground structure;WS_I);線彈性
  sensitivity(無塑後重分布);**初版單向 hard-kill**(雙向加回留後續);殼 BESO 機制就緒、oracle 待補。
- **sensitivity 對純節點載重 / UDL-free 桿是精確元素應變能(能量平衡 + 逐分量解析 [VERIFIED]);有 UDL 桿
  `recover` 含固端力 → `memberStrainEnergy` 算 ½Qᵀu ≠ ½uᵀKu,為近似 screen,偏差量無定量 oracle**
  [UNKNOWN](header 已警告)。
- **收斂準則**:`TargetReached`/`converged` 僅表示**體積目標達到**,**非** Huang & Xie 2N 窗 compliance
  穩定準則(**未實作** [NOT IMPLEMENTED];呼叫者自 `complianceHistory` 評估穩定性)。離散桿常卡 target 上方
  一點(Stalled,皆合理終止)。
- compliance 跳變停機門檻 `complianceJumpTol` 是工程啟發值(暴增現象有充分文獻佐證,**具體 52× 數字無對應
  文獻** [UNKNOWN],compliance-best 回退做法 A 有文獻支持)。
- **N2 = LSP 級韌性評估,非真非線性倒塌;勿自稱 "fail-safe"**。定位(WS_N §N2):**最近先行技術 = fail-safe
  桁架 TO(Stolpe 2019 / Zhu 2023,離散構件單桿移除損傷情境;Jansen 2014 為連續 SIMP 原版)**;FrameCore
  與其差異**僅在評估器**——LSP 序列線性倒塌驅動器(Stable/Collapsed + dlf + 碎塊連通)取代塑性/conic LP。
  **非新方法、非全局最優保證**。
- **分級**:`[VERIFIED]`(文獻方法之 FrameCore 實作)= 敏感度歷史平均(Huang & Xie 2007)+ compliance-best
  回退(WS_I 做法 A);`[NEW CODE]`(工程整合,非算法原創)= N2 抽查回滾鎖保護(LSP 評估器嵌入 BESO 迴圈)
  + ReSolve capacitance 機構守門整合。

## ⑩ 風險 / fallback

- 機構(移除致 K 奇異)→ ReSolve capacitance 快篩 + fresh `solve` 確認 → 回滾候選桿(必修③;兩容差
  邊緣案例由 fresh 兜底,F46 best 拓撲 fresh 非奇異即此保證)。
- compliance 暴增(移除關鍵桿)→ 跳變守門停機 + best 回退(必修②④)。
- 狀態循環(N2 回滾+鎖反覆)→ FNV-1a `hashState` 守門 → `Stalled` 有限終止(同 S4/S5 模式)。
- 配額移除全被守門擋下(無可移除桿)→ `Stalled` break。
- N2 抽查昂貴 → `redundancyCheckEvery`/`redundancySamples` 調節;預設關閉(純 BESO)保持輕量。
- 殼 / 雙向加回 / group 約束 / 應力約束目標 = 後續(本階段 runBESO 為 member 單向 evolutionary 驅動器)。
