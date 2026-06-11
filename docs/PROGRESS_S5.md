# S5 進度日誌 — 尺寸優化(Fully-Stressed Design,`runSizeOptimization`)

> 接續 `PROGRESS_S4.md`(S4 結案於 `6defe28`)。本階段把 Karamba3D 對標主線推進到 S5:全應力設計
> (FSD)stress-ratio resizing 尺寸優化。spec = `docs/specs/S5_sizeopt.md`。政策:每次 commit 前跑
> 完整四腿 `run_gate.ps1 -RequireOpenSees` 全綠。

## 基準
- 起點 `6defe28`(S4)。working tree 既有雜項(`.gitignore`/`ArchSim.uproject`/`Plugins/LevelSim/`)**非本人改動,全程不碰**。
- baseline 四腿綠:standalone F1–F43 / UE 42 / OpenSees PASS / audit 76。
- 使用者「直接做到結束」授權動工 S5。

## 交付內容(單一 S5 commit)
### 新增
- `Public/FrameCore/SizeOpt.h` — POD API(`SizeOptLoadCase`、`SizeOptOptions`(maxIter/dcTol/Amin/
  solve/sectionTable/cases)、`SizeOptResult`(finalAreas/finalSections/finalDC/dcHistory/weightHistory/
  converged/cycled/singular/iterations))+ `runSizeOptimization`。零 Eigen/UE。
- `Private/SizeOpt.cpp` — stress-ratio FSD 驅動器:工作副本 + per-member 私有截面展開 + **相似截面縮放**
  (`A·=r, I/J·=r², c·=√r, As·=r, Z·=r^1.5`,從 origSec 一次縮放免漂移)+ 多工況 envelope D/C
  (`ElasticAllowable::checkSection` 逐桿 risk)+ 離散表 round-up + FNV-1a 狀態哈希震盪守門。
  **內迴圈走 fresh `solve`**(改截面值=改 K 值,超出 ReSolve same-topology;誠實標不享重用)。
- `Private/Tests/SizeOptTest.cpp` — UE 自動化 `FrameCore.SizeOpt.FullyStressed`(10-bar 鏡像)。

### 修改
- `FrameTestFixtures.h`:`tenBarTruss`(經典 Schmit/Berke 10-bar,全桿共享 section 0,driver 內展開)。
- `Standalone/main.cpp`:F44(+ include `SizeOpt.h`)。
- `linear_deep_audit.cpp`:`testSizeOpt()`(+3 checks,76→**79**)+ 註冊。
- `build.bat`/`build_linear_audit.bat`:源檔清單 +`SizeOpt.cpp`(`build_cli.bat` 不加 — frame_cli 的
  sizeopt 指令排 S6)。`run_gate.ps1`:`$ExpectedUeTests` 42→**43**。
- **不加 Member/Section 旗標 → 不動 `modelFingerprint`**(用 `sizableMembers` 參數;改的截面值已在
  fingerprint,driver 內走工作副本,fresh re-factor 自然發生)。

## 演算法 + 誠實定位
- **stress-ratio 不動點迭代**:每迭代 = 對所有工況 fresh solve → 逐桿跨工況跨兩端取最大 D/C(`risk`)
  → `A ← max(Amin, A·maxDC)` → 相似縮放重建截面 → 收斂 `max|D/C−1|<dcTol`(只計 >Amin 的 sized 桿)。
- **相似縮放**(`[NEW CODE]`):對方形**逐位元等於** `Section::Rectangular(√A,√A)`(原型 `squareSection(√A)`),
  故 10-bar oracle 與原型一致;對任意初始截面為形狀保持的物理相似縮放。
- **離散表**:round-up(向上取整,保守不震盪)+ FNV-1a 面積狀態哈希,重現 → `cycled` 有限終止(同 S4 模式)。
- **ReSolve 不適用**:FSD 改截面值出 K_ff 新值,非 same-topology rank-k(移除/恢復)→ 內迴圈 fresh
  `assembleAndFactor`;誠實效能段標明 FSD 不享 factorization 重用(對標 Karamba OptiCroSec 5 次 ULS 迭代)。

## 數值證據(本輪實測)
| Oracle | 量測 | 門檻 | 實測 |
|---|---|---|---|
| **F44** 10-bar | 收斂迭代數 | converged | **24** |
| F44 | 重量(lb)vs 文獻 1593.2 | ≥1593 且 rel<1.5e-2 | **1608.49**(rel 9.6e-3) |
| F44 | sized 桿(6)max\|D/C−1\| | <1e-6 | **PASS** |
| F44 | 桿2/5/6/10 貼 A_min | rel<1e-9 | **PASS** |
| F44 | 桿1/桿3 面積(in²) | ±3% vs 7.94/8.06 | **7.99 / 8.11** |
| **audit** 靜定一步 | A vs P/σa | rel<1e-12 | **1.14e-16** |
| audit 多工況 | 兩工況 envelope D/C | ≤1+1e-9 | **1.000** |
| audit 離散 | round-up 終止+保守 | 2000→2200 | **2200** |

## 四腿 gate(commit 前)
`run_gate.ps1 -RequireOpenSees` → **GATE: PASS** — standalone **F1–F44** / UE **43 tests** / OpenSees
**PASS** / deep audit **checks=79**。`SizeOpt.cpp` 純 POD(零 Eigen),UE dual-build 乾淨(`bUseUnity=false` 起);
⚠️ 本輪踩雷:UE 測試常數不可命名 `IN`(Windows SAL 巨集,經 CoreMinimal.h 拉入)→ 改 `kIn/kLb/kPsi`。

## 誠實邊界
- **靜定** FSD = 最輕解(`[VERIFIED]` by audit axialColumn,rel 1.14e-16);**靜不定**(10-bar)為啟發式
  固定點,**不保證全域最優**(WS_H 文獻);1608.49 lb 是**組合應力**最優,比 pin-jointed 文獻 1593.2 高 ~1%
  (引擎封閉真鉸,moment-frame 桿帶微小彎曲吃軸向裕度→桿略重略安全;權重永不低於純軸力最優,為不變量)。
- **無 LTB / 無局部挫屈 / 無位移約束**;初版連續 A(離散表 opt-in 後處理,round-up 保守不保證最輕)。
- D/C 用組合應力 risk;`A·risk` 對純軸力一步全應力精確,對含彎曲桿為啟發式(W∝A^1.5 故過修、迭代逼近)。
- **frame_cli sizeopt 指令 + OpenSees 對標**刻意排除(排 S6 GH 橋一起)。

## 下一步(待使用者授權)
F 編號下一個 = **F45**;audit 從 **79** 起增;UE 從 **43** 起增。S5 完成即停,回報後等使用者檢視再授權
下一階段(S6 GH 橋 / S7 BESO / …,順序彈性,見 `docs/AGENT_PROMPT_S5_S11.md`)。
