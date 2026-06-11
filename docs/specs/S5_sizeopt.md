# S5 交接級 spec — 尺寸優化(Fully-Stressed Design,`runSizeOptimization`)

> 對齊 S1–S4 同款十項。研究依據:`docs/research/WS_H_sizeopt.md` + `Research/WS_H_sizeopt/exp_size_opt.cpp`
> (10-bar truss 收斂 1593.16 lb)。誠實分級 `[VERIFIED]`/`[NEW CODE]`/`[THEORY]`。
> 起點錨點 `6defe28`(S4),四腿 baseline 全綠(F1–F43 / UE 42 / OpenSees PASS / audit 76)。

## ① 目標 / 不做
- **做**:全應力設計(FSD)`runSizeOptimization` — stress-ratio resizing `A ← max(Amin, A·maxDC)`,
  以彈性 D/C(`ElasticAllowable::checkSection` 的 `risk`,涵蓋軸力+雙軸彎曲組合應力)驅動,
  多工況取每桿跨工況最大 D/C(envelope)。相似截面縮放(形狀保持)使任意初始截面可用。
  離散標準截面表為 **opt-in** 後處理:round-up(向上取整,保守)+ 狀態哈希震盪守門(有限終止)。
- **不做**:① 真數學最佳化(KKT / MMA / SQP)— FSD 是啟發式;② LTB / 局部挫屈(`Section` 缺翹曲常數
  Iw/Cw 與剪力中心距,無法做 EC3 6.3.2 / AISC F 章完整側扭挫曲,見 WS_H §5 gap 清單);
  ③ 位移約束 FSD(初版只應力 D/C,位移約束屬 SLS 虛載重法,留後續);④ ReSolve 重用(改截面值
  超出 same-topology 假設,見 ⑥)。

## ② 公開 API(`Public/FrameCore/SizeOpt.h`,純 POD/std,零 Eigen/UE)
```cpp
struct SizeOptLoadCase {            // 一個工況的載重集合
    std::vector<NodalLoad> nodalLoads;
    std::vector<MemberUDL> memberUDLs;
};
struct SizeOptOptions {
    int  maxIter = 40;              // 迭代上限
    real dcTol   = 1e-8;           // 全應力收斂:resized 桿 max|D/C − 1| < dcTol
    real Amin    = 0;              // 面積下界(夾);0 時細弱桿可能趨零 → 機構,須慎設
    SolveOptions solve;            // threaded 進每次 assembleAndFactor
    std::vector<real> sectionTable;            // 升序離散面積表;空=連續 A
    std::vector<SizeOptLoadCase> cases;        // 多工況;空=單工況(用 model 自身載重)
};
struct SizeOptResult {
    std::vector<real>    finalAreas;     // 每桿(model.members 序);非 sizable 維持原值
    std::vector<Section> finalSections;  // 每桿縮放後截面(非 sizable=原截面拷貝)
    std::vector<real>    finalDC;        // 每桿收斂時 D/C(FS 桿≈1;非 sizable=量測值)
    std::vector<real>    dcHistory;      // 每迭代 sizable 桿最差 D/C
    std::vector<real>    weightHistory;  // 每迭代材料體積 Σ A·L(密度無關重量代理,mm³)
    bool converged = false;
    bool cycled    = false;        // 離散表震盪守門觸發 → 有限終止(非平滑收斂)
    bool singular  = false;        // 某次 solve 機構(如 Amin 過小)
    int  iterations = 0;
};
FRAMECORE_API SizeOptResult runSizeOptimization(const FrameModel& model,
                                                const SizeOptOptions& opts,
                                                const std::vector<int>& sizableMembers = {});
```
- `sizableMembers` = 參與尺寸調整的 member **index** 清單(空=全部 active 且 mat/sec 有效的桿)。
  **刻意用參數而非 Member 旗標** → 不動 `modelFingerprint`(改截面值已在 fingerprint,fresh re-factor
  自然發生;`runSizeOptimization` 內走工作副本,不污染 caller model)。

## ③ 演算法(`Private/SizeOpt.cpp`)
1. `validate(model)`;建工作副本 `FrameModel work = model`;對每根 sizable 桿**展開私有截面**
   (append `work.sections` 拷貝 + 重指 `secIdx`),記原始截面 `origSec[e]` 與 `area[e]=origSec.A`。
2. 每迭代:
   - 對每個工況(空=1 個,用 model 自身載重):覆寫 `work` 的 nodalLoads/memberUDLs → `solve(work)`;
     若 `singular` → 記 `result.singular`、停。對每根 sizable 桿 `dc[e] = max(dc[e],
     max(checkSection(endI).risk, checkSection(endJ).risk))`(跨工況、跨兩端取 max)。
   - resize:`Anew = max(Amin, area[e]·dc[e])`;離散表 → round-up 到表中 ≥Anew 的最小值;
     `area[e]=Anew`;`work.sections[secIdx] = scaledFrom(origSec[e], Anew)`(相似縮放,見 ⑤)。
   - 記 `dcHistory`(本迭代最差 dc)、`weightHistory`(Σ A·L)。
   - **收斂**(連續):resized 桿(>Amin)`max|dc−1| < dcTol` 且 `iter≥3` → `converged`。
     **收斂**(離散):無桿面積變動 → `converged`;面積狀態哈希(FNV-1a)重現 → `cycled=true` 且
     視為有限終止結束(同 S4 cycle-guard 模式)。
3. 回填 `finalAreas`/`finalSections`/`finalDC`(末次 solve 的逐桿 D/C)。

## ④ 相似截面縮放(形狀保持;`[NEW CODE]`)
給定面積比 `r = Anew/origA`,線性尺寸比 `λ = √r`(物理相似縮放):
```
A ← origA·r ;  Iy,Iz,J ← ·r²(∝λ⁴) ;  cy,cz ← ·λ ;  Asy,Asz ← ·r(∝λ²) ;  Zy,Zz ← ·r^1.5(∝λ³)
```
→ `Wy=Iy/cy ∝ r^1.5`(正確)。**從 origSec 一次縮放**避免迭代累積浮點漂移。
**已驗**:對方形,此縮放逐位元等於 `Section::Rectangular(√Anew,√Anew)`(原型用 `squareSection(√A)`,
故 10-bar oracle 與原型一致)。

## ⑤ Oracle(每能力獨立)
- **F44(standalone)= 經典 10-bar truss FSD**(Schmit/Berke;`fixtures::tenBarTruss`):
  E=10⁷psi、σ_allow=±25000psi、P=10⁵lbf↓@節點2/4、bay=360in、Amin=0.1in²、初始 A=10in²。
  斷言:`converged`;重量(由 finalAreas 反算 Σ0.1·A_in²·L_in)≥ **1593.2 lb** 且 rel<**1.5e-2**(實測 1608.49,組合應力比 pin-jointed 略重);
  **全應力**:sized 桿 `|D/C−1|<1e-6`(機器精度級 FS 性質);面積樣式比對文獻
  `[7.94,0.10,8.06,3.94,0.10,0.10,5.74,5.57,5.57,0.10]`(關鍵桿 ±2%、Amin 桿貼界);
  彎曲份額 maxBendShare 微小(truss 理想化誠實度量)。
- **audit `testSizeOpt`(+3 checks,76→79)**:
  (1)**靜定一步精確**:單軸壓桿 `axialColumn` → `A → P/σa` 機器精度(rel<1e-12;靜定 FSD=最優,
  `[VERIFIED]`);(2)**多工況 envelope**:兩工況,sized 結構在**兩工況下** D/C≤1+tol(較大工況支配);
  (3)**離散表 round-up 有限終止**:粗表 → `converged||cycled` 終止,且 final 面積為表值且 ≥ 連續最優(保守)。
- **UE `FrameCore.SizeOpt.FullyStressed`**:10-bar 鏡像(converged + 重量 + FS)。

## ⑥ ReSolve 不適用說明(誠實效能定位)
S1 `ReSolveSession`(Woodbury / rebaseline)只省「**移除/恢復**元素」(same-topology rank-k);FSD 每迭代
改的是**截面值**(A/I/J…)→ K_ff 的**值**改變,**超出 same-topology 假設**。故內迴圈走 **fresh
`assembleAndFactor`**(`solve()` = assembleAndFactor+solveLoad,每迭代全新分解)。**誠實標**:FSD 不享
factorization 重用;規模成本 = maxIter × (每工況一次完整分解)。對標 Karamba OptiCroSec(預設 5 次 ULS 迭代)。

## ⑦ build / gate 同步義務
- `build.bat` + `build_linear_audit.bat`:源檔清單 +`Source\FrameCore\Private\SizeOpt.cpp`
  (`build_cli.bat` 不加 — frame_cli 的 sizeopt 指令排 S6)。
- `run_gate.ps1`:`$ExpectedUeTests` 42→**43**。
- UE 模組自動收錄 `Private/**.cpp` 與 `Private/Tests/**.cpp`,無需改 `.Build.cs`。
- **不加 Member/Section 旗標 → 不動 `modelFingerprint`**(本 spec 關鍵決策)。

## ⑧ 誠實邊界 / 分級
- **靜定**結構 FSD = 最輕解(嚴格,`[VERIFIED]` by F44 axialColumn check)。**靜不定**結構 FSD 為
  啟發式固定點,**不保證全域最優**(WS_H §1 文獻明確;10-bar 是靜不定,1593.16≈文獻 1593.2 但
  屬「接近最優」非「證明最優」)。
- **無 LTB / 無局部挫屈 / 無位移約束**(初版只截面強度 D/C + 既有線性屈曲另走 S3);連續 A(離散表
  opt-in 後處理,round-up 保守不保證最輕)。
- D/C 用組合應力 `risk`(軸+雙軸彎);`A·risk` 對純軸力一步全應力精確,對含彎曲桿為啟發式
  (彎曲應力 ∝ M/W、W∝A^1.5,故過修,迭代逼近;truss 彎曲份額≈0 故收斂到軸力 FS 解)。
- `[NEW CODE]` = stress-ratio 迭代 + 相似縮放 + 多工況 envelope + 離散 round-up 震盪守門;
  `[VERIFIED]` = 10-bar 對文獻、靜定一步精確、相似縮放==方形重建。

## ⑨ 數值證據(實測,gate 全綠)
| Oracle | 量測 | 門檻 | 實測 |
|---|---|---|---|
| F44 10-bar | 收斂迭代數 | converged | **24 iters** |
| F44 10-bar | 重量(lb) vs 文獻 1593.2 | ≥1593 且 rel<1.5e-2 | **1608.49**(rel 9.6e-3;組合應力略重略安全) |
| F44 全應力 | sized 桿(6 根)max\|D/C−1\| | <1e-6 | **PASS**(nSized=6) |
| F44 樣式 | 桿2/5/6/10 貼 A_min | rel<1e-9 | **PASS**(精確夾界) |
| F44 樣式 | 桿1/桿3 面積(in²) | ±3% vs 7.94/8.06 | **7.99 / 8.11** |
| audit 靜定一步 | A vs P/σa | rel<1e-12 | **1.14e-16**(機器精度) |
| audit 多工況 | 兩工況 envelope D/C | ≤1+1e-9 | **1.000** |
| audit 離散 | round-up 終止 + 保守 | 2000→2200 | **2200**(表值,≥連續最優) |

> 四腿 gate:standalone **F1–F44** / UE **43** / OpenSees **PASS** / audit **checks=79**(76→79)。

## ⑩ 下一步
S5 完成即停,四腿 gate 全綠後 commit/push。F 編號下一個=**F45**;audit 從 **79** 起;UE 從 **43** 起。
後續 S6(GH 橋)/S7(BESO)/… 見 `docs/AGENT_PROMPT_S5_S11.md`。
