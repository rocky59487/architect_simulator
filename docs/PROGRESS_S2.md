# S2 進度日誌 — N4 動量繼承連續動力倒塌(`runDynamicCollapse`)

> 接續 `PROGRESS_S1.md`(S1 結案於 `ca04fee` + docs `f08941d`)。本階段把靜力 LSP 倒塌
> (`runProgressiveCollapse`)升級為**連續動力模擬**:模態空間 Newmark + 跨拓撲事件狀態繼承 +
> 碎塊帶**初速/角速度**交接,修掉「碎塊零初速」的既有誠實限制。
> 政策(使用者 2026-06-11 定調):**每次 commit 前跑完整四腿 `run_gate.ps1 -RequireOpenSees` 並全綠**。

## 基準
- 起點 `f08941d`。working tree 既有雜項(`.gitignore`/`ArchSim.uproject` 改動、`Plugins/LevelSim/`)**非本人改動,全程不碰**。
- baseline 四腿確認綠:standalone F1–F36 / UE 37 / OpenSees PASS / audit 67。

## 交付內容(單一 S2 commit)
### 新增
- `Public/FrameCore/DynamicCollapse.h` — POD API(`DynCollapseOptions`/`DynCollapseEvent`(含 `detached` 帶 vel/angVel)/`DynCollapseFrame`(6N u,v 快照)/`DynCollapseHistory`)+ `runDynamicCollapse`。零 Eigen。
- `Private/DynamicCollapse.cpp` — 驅動器:鏡像 `Collapse.cpp` 的工作副本/fragment cleanup,事件重解**走 fresh `assembleAndFactor`**(碎塊清理 pin 節點=改 support flags,超出 ReSolve same-topology 範圍;且重建基底需新構型 `K'_ff` 的 LDLT)。
- `Private/FragmentMomentum.h` — header-inline 動量抽取(`fragmentMomentum`/`fillFragmentVelocity`/`fragmentKE`),驅動器與 audit 共用同一份碼(SparseEigsolver.h 模式)。
- `Private/Tests/DynCollapse{Equivalence,Momentum,Outcomes}Test.cpp` — UE 自動化 ×3。

### 修改
- `Connectivity.h`:`FragmentCluster` 加 `Vec3 vel; Vec3 angVel;`(預設零,POD 向後相容;靜力路徑維持零初速,動力路徑回填)。
- `SparseEigsolver.h`:`subspaceSmallest` 加 `const MatX* X0 = nullptr` 末參(模態 warm-start;預設 nullptr 逐位元不變)。
- `Standalone/main.cpp`:F37/F38/F39。`linear_deep_audit.cpp`:`testDynamicCollapse()`(+4 checks,67→71)。
- `build.bat`/`build_linear_audit.bat`:源檔清單 +`DynamicCollapse.cpp`。`run_gate.ps1`:`$ExpectedUeTests` 37→40。

## 演算法 + 對抗審查修正(已納入)
- **時間積分**:per-mode Newmark β=1/4 γ=1/2;模態阻尼 `c_i = α + β·ω²`(=ΦᵀCΦ 對角,**天然避開 ω→0 除法溢位**,c=0 與原型逐位元等價)。
- **跨事件繼承**:事件瞬間在全域 N 空間凍結 (u,v);**順序嚴格**(對抗審查 P0):先抽碎塊動量(pin 之前,v 完整)→ 再 deactivate+pin → fresh re-factor → 用**新 fmap** reduce → `q'=Φ'ᵀM'u'`、`q̇'=Φ'ᵀM'v'`、`q̈'=Φ'ᵀF'−W'²∘q'`;`truncationResidual=‖u'−Φ'q'‖/‖u'‖`。
- **動量抽取**(C1):組 **fragment-local 全局一致質量**(節點重映,共享節點不重複計),`p=Tᵀ_trans M v`、`L=Tᵀ_rot M v`;`vel=p/mass`、`angVel=I⁻¹L`(對稱偽逆,共線碎塊 own-axis→0)。
- **基底**:預設 load-dependent Ritz(`[NEW CODE]`,種子=事件後殘差 `r=F'−K'u'`;退化補向量用固定 seed 20260607u 可重現);對照路徑純特徵模態。scatter constrained DOF 填 prescribed(位移)/0(速度)(A3)。

## 數值證據(本輪實測)
| Oracle | 量測 | 門檻 | 實測 |
|---|---|---|---|
| **F37**(門架,全基底) | Ritz frames == 純模態 frames | 1e-8 | **0.000**(逐位元) |
| F37 | per-event truncationResidual | 1e-9 | **0**(全基底精確) |
| **F38**(門架激振→懸臂鏈動態斷開) | 運動中 detach,handoff vel 非零有限 | — | vel=(3.42,~0,1.49) mm/s,|angVel|=0.084 |
| F38 | x-z 對稱 vel_y~0 | — | ~1e-13 |
| **F39**(軸鏈斷開→留存 SDOF) | 繼承 u1 == 解析靜態 PL/EA | 1e-9 | **2.26e-15**(機器精度) |
| F39 | 振幅守恆(Newmark) / Ritz 路徑繼承 | 1e-2 / 1e-8 | 2.26e-15 / 6.94e-16 |
| **audit** 跨事件等價 | 模態繼承 vs 全系統 Newmark(全基底) | 1e-8 | **5.55e-13** |
| audit 動量平移閉合 | `p_x` vs `m·v0`(FE 一致質量) | 1e-10 | **0.000**(精確) |
| audit transverse 角動量 | `L_y` vs cluster `I_yy·w0` | 1e-2 | **0.000**(細長桿 rod 模型精確) |
| audit 全基底零截斷 | runDynamicCollapse truncationResidual | 1e-9 | 1.00e-15 |

## 四腿 gate(commit 前,全綠)
`run_gate.ps1 -RequireOpenSees` → **GATE: PASS** — standalone **F1–F39** / UE **40 tests** exit 0 / OpenSees **PASS** / deep audit **PASS checks=71**。UE dual-build(`DynamicCollapse.cpp`+`FragmentMomentum.h` 經 `FrameEigen.h` choke point,零 Eigen 洩漏)Link DLL 成功。

## 誠實邊界
- 事件間線彈性模態空間;失效準則 = screening 級 D/C(非規範檢核)。
- 事件**整步觸發**(O(dt));截斷誤差由 `truncationResidual` 顯式報告(全基底=精確;純模態截斷可觀)。
- 碎塊 **own-axis** 角動量用 rod 閉式(FE 截面極慣量項被忽略,細長桿可略);Chaos 交接**單向**。
- `looseNodes`(孤立節點)速度不入碎塊報告。塑鉸動力留 **S2.1**(`formedHinges` 欄位預留空)。
- 事件重解 = **fresh re-factor**(ReSolve hook 留後續效能優化);效能屬定性(小 fixture 毫秒級),大規模量測排 S6/後續。

## 下一步 = S3 P-Delta(`docs/specs/S3_pdelta.md`)
> ⚠️ 使用者定調:**S2 完成即停**,回報後等使用者檢視再授權 S3。
F 編號下一個 = **F40**;audit 從 **71** 起增;UE 從 **40** 起增。
