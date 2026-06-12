# S9c 進度 — Co-rotational 收尾:弧長法 snap-through + 一致切線 + CR UDL + prescribed 大位移

> 接續 S9b 3D 通用 co-rotational(`3dfbfec`)。規格 `docs/specs/S9c_arclength.md`,研究依據
> `docs/research/WS_F_corot.md`(§F-4 Williams、§F-9 弧長)。**S9c = co-rotational 主線收尾**,把 S9b 誠實標
> 「留 S9c」的四項補齊。完成後**下一步即 S10**(N-M 互動塑鉸)。

## 摘要

S9c 在 `runCorotational` 補齊四項能力(皆 opt-in / 不動既有預設行為,S9b F50/F51 逐位元不變):
1. **弧長法(Crisfield cylindrical)**:`useArcLength` → 載荷因子 λ 成為未知,弧長 constraint `ΔuᵀΔu=Δl²`
   約束增量,越過極限點追蹤完整 load-displacement 路徑(snap-through);S9/S9b 載荷控制在極限點 `diverged`。
2. **一致切線(數值 FD)**:`consistentTangent` → `Kt=∂f_int/∂u`(forward FD + 對稱化),保證與內力一致。
3. **CR member UDL**:UDL → 初始構型等效節點力(同 `BeamColumnElement` 的 consistent Qf),加進 `F_ext`。
4. **prescribed 大位移**:λ-ramped Dirichlet BC(平移直接設、旋轉 `R_node=expSO3(λ·presc)`)。

**架構**:沿用 S9b 獨立 driver。抽共用 `assemble(useFD)`/`applyInc`/`recoverState`/`applyPrescribed` helper
(load-stepping 與 arc-length 共用);**不新增 `.cpp`** → 四 build 腳本免動。API 加 4 opts + 2 result 欄位
(`pathLambda`/`pathDisp`),**不入 `modelFingerprint`**,預設全 off → S9b 行為不變。

**五腿全綠**:standalone **F1-F53** / UE **49**(+1 `Corotational.SnapThrough`)/ OpenSees PASS(+arc-length
leg)/ audit **101**(+3)/ CLI round-trip **ALL PASS**(+ARCL)。`$ExpectedUeTests` 48→49;audit 98→101。

## 數學 / 演算法

- **弧長法**:predictor `δu_t=Kt⁻¹F`、`Δλ=±Δl/‖δu_t‖`(GSP 符號避折返);corrector cylindrical 二次
  `a δλ²+b δλ+c=0`(`a=δu_tᵀδu_t`、`b=2(Δu+δu_bar)ᵀδu_t`、`c=‖Δu+δu_bar‖²−Δl²`),選 `cosθ` 最大根;
  越過極限點(`Kt` 奇異是 snap-through 本質,弧長 constraint 跨過)。記錄 `pathLambda/pathDisp`。
- **一致切線(FD)**:`Kt[:,j]=(f_int(u⊕eps·ê_j)−f_int(u))/eps`(平移 `u[g]+=eps`、旋轉 `Rnode←expSO3(eps ê)Rnode`),
  `eps=1e-7`,對稱化 `(Kt+Ktᵀ)/2` 續用 LDLT。O(nf) 組裝/迭代(oracle / 小模型;大模型用主項切線)。
- **UDL**:`Qf`(局部 consistent,`wL/2`軸/橫 + `±wL²/12`端彎)→ `peq=−transform12(localAxes)ᵀ·Qf`(全域)→
  加 `F_ext`(隨 λ)。初始構型等效(small-strain;follower 隨變形壓力留後續)。
- **prescribed**:`applyPrescribed(λ)` 設 prescribed DOF(FIXED,不在 free set)= `λ·presc`(平移)/
  `expSO3(λ·prescVec)`(旋轉);殘差/反力自然含。load-stepping(每步)+ arc-length(每次 applyInc 後)。

## Oracle(實測)

- **F52a/b snap-through**:淺人字 von Mises 拱,弧長法 `λ_peak=0.00586` @step3(極限載荷 ≈ von Mises 閉式
  0.0057,rigid-jointed 略高合理),λ 升後降(後屈曲路徑);載荷控制到 1.5×極限 → `diverged`(證明弧長必要)。
- **F53a UDL**:X 懸臂橫向 UDL,tip `δ=wL⁴/8EI` rel **1.25e-08**。
- **F53b prescribed**:導向懸臂 tip Uy=δ(Rz free),tip Uy==δ rel **0**(BC 精確)、base reaction
  `=−3EIδ/L³` rel **1.02e-08**。
- **F53c 一致切線**:淺拱弧長 snap-through,FD 切線極限載荷 == 主項切線 rel **4.44e-16**(FD 切線正確)。
- **OpenSees arc-length leg**:淺拱極限載荷,我方 `0.00585833` vs OpenSees `integrator ArcLength` `0.0058962`
  rel **6.42e-03**(`TOL_SNAP` 3e-2;CR + 弧長同物理,跨工具吻合)。
- **audit +3**(checks 9/10/11):snap-through(tracked+diverged)、UDL(rel 1.56e-12)、prescribed(rel 3.07e-15)。
- **CLI ARCL**:淺拱經文字橋 `ARCL` round-trip,`λ_peak=0.00586`。

## 檔案改動

- `Private/CorotationalAnalysis.cpp`:`assemble` 加 `useFD`(FD 一致切線);新增 `applyPrescribed` helper;
  Fext 加 UDL 等效節點力;移除 UDL/prescribed reject 守門;load-stepping 重構用 `assemble` lambda(統一 +
  啟用 consistentTangent;useFD=false 與原逐位元相同);弧長 + load-stepping 呼叫 applyPrescribed。
  `Public/.../CorotationalAnalysis.h`:+4 opts(`useArcLength/arcLength/arcSteps/monitorDof/consistentTangent`)
  +2 result(`pathLambda/pathDisp`);SCOPE 註解更新。
- `main.cpp` +F52(snap-through)+F53(UDL/prescribed/一致切線);`FrameTestFixtures.h` +`shallowArchPair`;
  `Tests/CorotationalTest.cpp` +`Corotational.SnapThrough`;`run_gate.ps1` `$ExpectedUeTests` 48→49;
  `linear_deep_audit.cpp` +checks 9/10/11;`frame_cli_core.cpp` +`ARCL` 指令(struct/parse/exec + APATH);
  `cli_roundtrip.py` +ARCL check;`opensees_compare.py` +arc-length leg(`shallow_arch_model`/`run_frame_cli_arcl`/
  `run_opensees_arcl`)。
- **不動**:`IElement.h` / 既有 element / `FrameSolver.cpp` / `SolveResult.h` / `SolveOptions.h` /
  `FrameModel.cpp`。**四 build 腳本源檔清單免動**(不新增 `.cpp`)。**`.gitignore`/`ArchSim.uproject`/
  `Plugins/LevelSim/`/`frame_capi.*` 未碰**(working tree 前序未 commit 項;commit 顯式 `git add` 排除)。

## 誠實邊界 / novelty

- **弧長法 = Crisfield 1981 / Riks 1979 文獻方法**,FrameCore 為實作;對標附先行技術定位(業界標準)。
- **一致切線 = 數值 FD**(非解析 OpenSees Ksigma2/3 逐行;FD 保證一致 + 二次收斂,解析版留未來收斂速度)。
- **UDL = 初始構型等效**(small-strain;follower 隨變形壓力留後續)。
- **無 snap-back / bifurcation 分支切換**(cylindrical 追主路徑;雙折返 snap-back 需 spherical,標明)。
- **prescribed 旋轉**多軸時合成 `prescVec`,θ→π 同 S9b logSO3 窄帶(已知無害)。
- **S10 N-M 互動塑鉸**在 S9c 後(R4;塑鉸在局部共旋座標,`pb` 餵 N-M 面)。

## ⚠️ 踩雷(durable)

1. **弧長 `Dl` auto 對軟方向不通用**:`Dl=‖δu_t‖/loadSteps` 對淺拱(垂直軟)`δu_t` 大 → `Dl≈1.7` 一步
   跳過整個 snap region(apex 下沉 1.7 >> rise 0.25),λ 暴增無 snap。**snap-through 須手動設 `arcLength`**
   (依 rise/span 尺度,如淺拱 0.03);auto 只是粗略 fallback。F52/audit/cli/OpenSees 都手動設 0.03。
2. **load-stepping 重構用 `assemble` lambda**:`useFD=false` 與原 inline 組裝逐位元相同 → F50/F51 回歸綠
   (硬保護);refactor 後務必驗 F50/F51。
3. **UDL 等效 `peq=−Tᵀ·Qf`**:複用 `BeamColumnElement` 的 `wx=−w_local` Qf 慣例(`transform12(localAxes)`);
   小位移退化 `wL⁴/8EI` 機器精度驗證符號正確。
4. **prescribed DOF 是 FIXED**(不在 free set),`applyPrescribed` 每步**設**(非累加)`λ·presc`;NR 不碰 fixed。
5. **OpenSees `integrator ArcLength` 的 `arcLength` 參數**與我方 `Dl` 尺度不同 → 各自調(我方 0.03 / OS 0.0012)
   到能追過極限點;對比**極限載荷**(物理,robust)非路徑點對齊,`TOL_SNAP` 3e-2。
6. **環境工具錯亂**(本輪踩到):一批 Edit 顯示成功卻沒落地(`git status` / `grep` 核對才發現)→ 重大改動後
   **以 `git status` + `grep` 核對檔案真有改到**,勿只信 Edit 回傳。

## 對抗式審核

(待 3-agent 平行對抗審核回報後補。)

## 下一步

- **S10**:N-M 互動塑鉸(co-rotational 線已收尾,下一步即此;R4 方向耦合;塑鉸局部共旋座標,`pb` 餵 N-M 面;
  WS_G 路線 G1 `Mp_eff=Mp·f(N/Np)`)。
- **S9c 後續**(非阻塞):解析 Ksigma2/3 spin 切線、follower UDL、spherical arc-length(snap-back)、
  bifurcation 分支切換。
