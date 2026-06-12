# S9c 交接規格 — Co-rotational 收尾:弧長法 snap-through + 一致切線 + CR UDL + prescribed 大位移

> 接續 S9b 3D 通用 co-rotational(`3dfbfec`)。研究依據:`docs/research/WS_F_corot.md`(§F-4 Williams toggle、
> §F-9 弧長必要性、§F-3 elastica)+ `WS_F2_corot3d_opensees.md`(§6 Ksigma2/3)。
>
> **狀態:實作完成,五腿全綠**(standalone F1-F53 / UE 49 / OpenSees PASS +arc-length leg / audit 101 /
> CLI ALL PASS)。進度與實測見 `docs/PROGRESS_S9c.md`。**實作相對本 spec 的誠實偏離**:一致切線採數值 FD
> (非解析 Ksigma2/3);弧長 `Dl` snap-through 須手動設(auto 對軟方向不準)。S9c = co-rotational 主線的
> **收尾**,把 S9b 誠實標「留 S9c」的四項補齊:
> ① **弧長法**(Riks/Crisfield)→ snap-through 極限點後路徑(S9/S9b 載荷控制 NR 在極限點 `diverged`);
> ② **完整一致切線**(二次收斂;S9b 用主項 `TᵀKlT+Ksigma1`);③ **CR member UDL**(S9b reject);
> ④ **prescribed 大位移**(S9b reject)。**屬「改動引擎重要部件」級別**;每能力獨立 oracle + 階段五腿 + commit。
>
> **誠實定位**:弧長法 = Crisfield 1981 / Riks 1979 成熟方法;一致切線提供**數值一致(FD)**選項(非解析
> OpenSees Ksigma2/3 逐行 — 那留作未來;FD 切線保證二次收斂且 oracle 可直接驗)。對標禁裸宣稱優於 Karamba。

---

## ① 目標 / 不做

**做**(四能力,各自獨立 oracle,可分階段 commit):
1. **弧長法(Crisfield cylindrical)**:載荷因子 λ 成為未知,弧長 constraint `ΔuᵀΔu=Δl²` 約束增量;越過極限點
   追蹤完整 load-displacement 路徑(snap-through)。opt-in(`CorotationalOptions.useArcLength`)。
2. **完整一致切線(數值 FD 選項)**:`consistentTangent` 時 `Kt=∂f_int/∂u`(forward FD,每 free DOF 擾動),
   保證與內力一致 → NR 二次收斂。預設仍主項切線(對稱 LDLT)。
3. **CR member UDL**:移除 UDL reject;UDL → 初始構型等效節點力(`wL/2` 端力 + `wL²/12` 端彎矩,沿初始
   局部軸),施於變形構型(small-strain 下等效保守;follower 留註)。
4. **prescribed 大位移**:移除 prescribed reject;平移 prescribed 直接設增量、旋轉 prescribed 設 `R_node`;
   NR 約束該 DOF。

**不做(誠實邊界)**:
- **解析 Ksigma2/3 逐行**(OpenSees getKs2Matrix):FD 一致切線已達二次收斂;解析版留未來(收斂速度,非正確性)。
- **動力大位移 / 接觸 / 大應變**(仍小應變大旋轉 CR)。
- **bifurcation 分支切換**(弧長法追主路徑;對稱分叉的次分支不切換)。
- **follower UDL 完整**(初版 UDL 用初始構型等效;隨變形的 follower 壓力留註)。

---

## ② 公開 API(`Public/FrameCore/CorotationalAnalysis.h`,POD/std)

`CorotationalOptions` 加 4 個欄位(皆 solve 參數,**不入 `modelFingerprint`**):
```cpp
struct CorotationalOptions {
    int  loadSteps = 10;
    int  maxIter   = 50;
    real tolR = 1e-9, tolU = 1e-12;
    SolveOptions solve;
    // --- S9c ---
    bool useArcLength      = false;   // Crisfield cylindrical arc-length (snap-through; ignores loadSteps)
    real arcLength         = 0;       // arc-length increment Dl (0 -> auto from first tangent + loadSteps)
    int  arcSteps          = 50;      // max arc-length increments
    bool consistentTangent = false;   // numerical (FD) consistent tangent for quadratic NR convergence
};
```
`CorotationalResult` 加 load-displacement 路徑(snap-through 回放/驗證):
```cpp
struct CorotationalResult {
    bool converged=false, diverged=false;
    int  loadStepsCompleted=0, totalIterations=0;
    real lastResidual=0;
    SolveResult finalState;
    // --- S9c (arc-length path) ---
    std::vector<real> pathLambda;     // load factor at each completed arc-length step (empty if not arc-length)
    std::vector<real> pathDisp;       // a representative monitored displacement at each step (caller picks DOF)
    int  monitorDof = -1;             // global DOF whose displacement is recorded in pathDisp (-1 -> tip auto)
};
```
**API 向後相容**:預設 `useArcLength=false` → S9b 行為逐位元不變(F50/F51 不受影響)。

---

## ③ 資料流 / 架構(沿用 S9b 獨立 driver)

- **弧長 driver**:`useArcLength` 時 `runCorotational` 走 arc-length 分支(取代固定 λ load stepping)。狀態
  增 `lambda`(載荷因子)。參考載重 `F_ext`(全量)。每弧長步 predictor(切線)+ corrector(NR + constraint)。
  `Rnode` 更新同 S9b(spatial 左乘)。記錄 `pathLambda/pathDisp`。
- **一致切線**:`consistentTangent` 時 `assembleTangentFD`(對每 free DOF `g`:擾動 `u/Rnode` by `eps`,重算
  `f_int`,`Kt[:,g]=(f_int⁺−f_int⁰)/eps`),取代解析 `Ke`。旋轉 DOF 擾動經 `Rnode←expSO3(eps·ê)Rnode`。
- **UDL**:build 階段每元素算初始局部等效節點力 `Qf`(同 `BeamColumnElement` 的 UDL Qf),轉全域(用初始
  框架 `E0col`),加進 `F_ext`(隨 λ 縮放;small-strain 等效)。
- **prescribed**:`fmap` 仍把 prescribed DOF 當 fixed(不在 free set);但 NR 前把 prescribed 值施加到 `u`
  (平移)/`Rnode`(旋轉,`expSO3`),殘差自然含 prescribed 反力。recover 反力含 prescribed DOF。

**守門調整**:移除 UDL/prescribed reject;保留 殼/塑鉸/release/tonly reject。弧長 + UDL/prescribed 可組合。

---

## ④ 演算法

**(A) 弧長法(Crisfield cylindrical,ψ=0)**:
- 參考載重 `F`(=全量 `F_ext_f` on free DOF)。狀態 `u, λ`(初 0)。
- **predictor**(每弧長步起):`δu_t = Kt⁻¹ F`;`Δλ = ±Δl/√(δu_tᵀδu_t)`;符號 = `sign(δu_{t,prev}ᵀδu_t)`
  (GSP,首步 +);`Δu = Δλ·δu_t`;`u+=Δu, λ+=Δλ`(更新 Rnode)。
- **corrector**(NR until `‖r‖/‖λF‖<tolR`):`r=λF−f_int`;`δu_bar=Kt⁻¹r, δu_t=Kt⁻¹F`;cylindrical
  constraint 二次 `a δλ²+b δλ+c=0`:`a=δu_tᵀδu_t`、`b=2(Δu+δu_bar)ᵀδu_t`、`c=(Δu+δu_bar)ᵀ(Δu+δu_bar)−Δl²`;
  **選根** `cosθ=Δuᵀ(Δu+δu_bar+δλ δu_t)` 最大(避折返);`δu=δu_bar+δλ δu_t`;`Δu+=δu, Δλ+=δλ`;
  `u+=δu, λ+=δλ`(更新 Rnode)。
- **終止**:`λ≥1`(達目標載重)或 `arcSteps` 用盡;每步存 `(λ, u[monitorDof])`。**越過極限點**:`Kt` 在極限點
  奇異 → predictor `Kt⁻¹F` 失敗 → 弧長步法天然以小 `Δl` 跨過(不正好落在奇異點);若 `Kt` 數值奇異則該步
  縮 `Δl` 重試(adaptive)。`Δl` auto:首步用主項切線 `δu_t`,`Δl=‖δu_t‖/loadSteps·(λ_target scale)`。
- **預設 Kt**:主項 `TᵀKlT+Ksigma1`(對稱 LDLT);`consistentTangent` 時 FD。極限點附近主項奇異是 snap-through
  物理本質,弧長 constraint 處理。

**(B) 一致切線(FD)**:`Kt[:,g]=(f_int(u⊕eps·ê_g)−f_int(u))/eps`,`eps=1e-7·max(1,scale)`;旋轉 DOF 擾動
經 `Rnode`。**非對稱** → 用 `Eigen::SparseLU`(或對稱化續 LDLT;FD 對稱化誤差小)。oracle:小位移 FD Kt ==
解析主項 Kt(rel<1e-5)+ NR 二次收斂(殘差 `r_{k+1}∝r_k²`)。

**(C) UDL**:元素初始局部等效 `Qf`(EB:`wL/2`軸/橫、`±wL²/12`端彎),`f_eq=−E0col·Qf` 轉全域(同
`BeamColumnElement` 慣例),累加 `F_ext`(隨 λ)。oracle:UDL 懸臂大撓度 vs 均布載重 elastica(shooting
`d²θ/ds²=−q(L−s)cosθ/EI` 或對比 OpenSees corot + eleLoad... ⚠️ corotTransf 忽略 eleLoad → 用 small-disp
退化 `δ=wL⁴/8EI` + 中撓度 vs Linear)。

**(D) prescribed**:NR 每迭代前 prescribed DOF 設值(平移 `u[g]=presc`、旋轉 `Rnode=expSO3(presc·ê)`),
free set 排除該 DOF;殘差/反力自然含。oracle:固端梁單端 prescribed 轉動 `θ` → 大位移撓度 vs end-rotation
elastica(`α` 由邊界轉角給)或 small-disp `M=4EIθ/L` 退化。

---

## ⑤ 檔案

**改(不新增 `.cpp`)**:`Private/CorotationalAnalysis.cpp`(弧長分支 + FD 切線 + UDL 等效 + prescribed +
移除 UDL/prescribed 守門);`Public/.../CorotationalAnalysis.h`(4 opts + 3 result 欄位 + SCOPE 註解)。
**Gate**:`main.cpp` +F52(a snap-through vs OpenSees ArcLength / b 一致切線二次收斂 / c UDL elastica /
d prescribed);`FrameTestFixtures.h` +snap-through fixture(淺人字 / 淺拱);`Tests/CorotationalTest.cpp`
+`Corotational.SnapThrough`(`$ExpectedUeTests` 48→49);`linear_deep_audit.cpp` testCorotational +S9c checks;
`cli_roundtrip.py` +ARCL/UDL;`opensees_compare.py` +arc-length leg(`integrator ArcLength`)。`SolveOptions.h`
不動。**`.gitignore`/`ArchSim.uproject`/`Plugins/LevelSim/`/`frame_capi.*` 絕不碰。**

---

## ⑥ Oracle(誠實分級)

- **F52a snap-through `[VERIFIED]`(弧長法核心)**:淺人字兩桿(rigid-jointed)頂點下壓,弧長法追蹤完整
  load-displacement 路徑(含極限點 + 下降段),極限載荷 + 路徑 vs **OpenSees `integrator ArcLength`**(獨立
  第三方)逐點 rel<1e-2(cross-tool);載荷控制 NR 在極限點 `diverged`(對照,證明弧長法的必要)。
- **F52b 一致切線二次收斂 `[VERIFIED]`**:小位移 FD `Kt` == 解析主項 `Kt`(rel<1e-5);elastica α=5 NR
  殘差 `consistentTangent` 二次下降(迭代數 << 主項線性)。
- **F52c UDL 大位移 `[VERIFIED]`**:UDL 懸臂,small-disp 退化 `δ=wL⁴/8EI`(vs `solve` 線性 rel<1e-3)+
  中等載重大撓度單調合理(vs 無 UDL elastica 趨勢)。
- **F52d prescribed `[VERIFIED]`**:固端梁單端 prescribed 轉動,small-disp `M=4EIθ/L`(端彎矩,vs 解析
  rel<1e-3)+ 大轉動撓度合理。
- **OpenSees arc-length leg**:`opensees_compare.py` snap-through `integrator ArcLength` 路徑對照。
- **平面/3D 退化回歸**:S9b F50/F51 全綠(useArcLength=false 預設,逐位元不變)。
- **誠實**:弧長法 vs OpenSees ArcLength = 獨立第三方;snap-through 極限載荷亦可加 von Mises 閉式(若 fixture
  退化為純軸力桁架)。一致切線 FD 自洽(切線=內力導數,二次收斂是定義性質)。

---

## ⑦ Gate

F52(a/b/c/d);UE `Corotational.SnapThrough`(`$ExpectedUeTests` 48→49);audit testCorotational 擴 S9c;
`cli_roundtrip` ARCL;`opensees_compare.py` arc-length leg。**保留 S9b F50/F51**(useArcLength=false 預設不變)。
**免動四 build 腳本源檔清單**(不新增 `.cpp`)。commit 前五腿全綠。

---

## ⑧ 效能

弧長法 = 迭代(arc steps × NR),每迭代 2 次 solve(`δu_bar` + `δu_t`,同 Kt 一次 factor 兩 solve)。FD 一致
切線 O(N) f_int 評估/迭代(僅 oracle 模型;大模型用主項切線)。UDL/prescribed 無額外迭代成本。

---

## ⑨ 誠實邊界 / novelty

- **弧長法 = Crisfield 1981 / Riks 1979 文獻方法**,FrameCore 為實作。`[NEW CODE]` = arc-length driver 整合 +
  CR 結合。對標附先行技術定位(弧長是業界標準)。
- **一致切線 = 數值 FD**(非解析 OpenSees Ksigma2/3;FD 保證二次收斂,解析版留未來收斂速度優化)。
- **UDL = 初始構型等效**(small-strain 保守;follower 隨變形壓力留未來)。
- **無 bifurcation 分支切換**(追主路徑);snap-back(載荷+位移雙折返)cylindrical 可能需 spherical,初版標明。
- **S10 N-M 塑鉸仍在 S9c 後**(R4)。

---

## ⑩ 風險 / fallback(實作後回填)

1. **弧長 root selection**:cosθ 折返判據;snap-back 嚴重時 cylindrical 失效 → 標明(主 oracle 淺人字單折返)。
2. **極限點 Kt 奇異**:predictor `Kt⁻¹F` 在奇異點失敗 → adaptive `Δl` 縮步重試;弧長 constraint 跨過。
3. **FD 切線非對稱**:用 SparseLU 或對稱化;eps 選 1e-7(平衡截斷/捨入)。
4. **UDL + corotTransf eleLoad**:OpenSees corot 忽略 eleLoad → UDL oracle 用 small-disp 退化 + 趨勢,不對
   OpenSees corot+UDL。
5. **prescribed 旋轉大角度**:`Rnode=expSO3(presc)` 直接設;θ→π 同 S9b logSO3 窄帶(已知無害)。
6. **平面/3D 退化**:useArcLength=false 預設 → S9b F50/F51 逐位元不變(硬保護)。
7. **build/gate 同步**:不新增 `.cpp`;`$ExpectedUeTests` 48→49;新 opts 不入 fingerprint。
8. **commit 衛生**:顯式 `git add`,勿 `-A`。

---

## ⑪ S9c 後續

- 解析 Ksigma2/3 spin/moment 切線(收斂速度);follower UDL(隨變形壓力);spherical arc-length(snap-back);
  bifurcation 分支切換。**S10 N-M 互動塑鉸**(R4;`pb` 餵 N-M 面)。
