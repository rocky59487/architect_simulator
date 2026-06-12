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

- **F52a/b snap-through**:淺人字拱(**rigid-jointed**),弧長法 `λ_peak=0.00586` @step3,λ 升後降(後屈曲
  路徑);載荷控制到 1.5×極限 → `diverged`(證明弧長必要)。**極限載荷驗證(誠實)**:pin-jointed von Mises
  閉式 = **0.00566**(scipy 獨立;**與 F52 的剛接框架是不同模型**,剛接 + 彎曲剛度使其略高 ~3.5%);F52 的剛接
  `0.00586` 是由 **OpenSees `integrator ArcLength` 互驗(0.0058962,rel 0.64%)**,非剛接框架的解析閉式。
  F52a 已 `checkClose` `λ_peak`(非僅 bool 存在性)。
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

## 對抗式審核(2026-06-12,3-agent 平行)

S9c 五腿全綠後做 **3-agent 平行對抗審核**(弧長法+一致切線數學 / UDL+prescribed+衛生 / oracle 強度),各用
numpy/scipy 獨立重算 + 既有 `frametest.exe`,不跑 build。**零 CRITICAL**。核心經獨立確認正確:

- **弧長法數學(numpy)**:Crisfield cylindrical `a/b/c` 係數(兩根滿足約束 3.5e-18)、predictor 落在圓柱面、
  root selection 符合 Crisfield Vol.1 eq.5.65、GSP 符號、FD 擾動空間左乘——全對。
- **UDL/prescribed**:UDL `Qf` 符號與 `BeamColumnElement` **逐行一致**、`peq=−TᵀQf` 慣例相同;prescribed DOF
  為 FIXED 每步**設**非累加、平移/旋轉分支正確;守門移除後殼/塑鉸/release/tonly 守門仍在(無 silent-wrong)。
- **oracle**:exe vs OpenSees 0.64%、UDL `wL⁴/8EI`、prescribed `3EIδ/L³` 公式正確。

**findings 處理**:
- **[MAJOR→REJECTED] `dutPrev` raw vs signed**(弧長 agent):agent 建議 `dutPrev=sgn·dut`(Crisfield Alg.5.5
  純桁架版)。**實證:改 signed 反而破壞 F52**(λ 單調升到 2.36 不 snap)——本式用 consecutive raw tangent
  點積 GSP 準則(`dut^i·dut^{i-1}` 在極限點反號)才正確追過極限點。**原碼正確**,僅把該行註解改寫說明為何 raw。
- **[MAJOR→修] F52「von Mises 0.0057」誤導**(oracle agent):0.0057 是 **pin-jointed** 閉式(scipy 0.00566),
  F52 是剛接框架(不同模型)→ 改誠實標:剛接 0.00586 由 OpenSees ArcLength 互驗 0.64%,非剛接解析閉式。
- **[MAJOR→修] header SCOPE 過時**(衛生 agent):header 仍寫「NO snap-through / UDLs/prescribed REJECTED」
  (S9c 已實作)→ 重寫 SCOPE 反映 arc-length/UDL/prescribed 已支援(公開 API 誠實)。
- **[MAJOR→修] 旋轉 prescribed 無 oracle**(oracle agent):新增 **F53b2**(tip Rz=θ → base moment EIθ/L,
  rel **2.65e-16**)覆蓋 `R_node=expSO3(λ·presc)` 旋轉路徑(原只測平移 prescribed)。
- **[MINOR→修] F52a λ_peak 未斷言**:加 `checkClose(λ_peak,0.00586,2e-2)`(非僅 bool 存在性)。
- **[MINOR→修] F53a UDL 容差過鬆**:2e-3→1e-4(實測 1.25e-8)。
- **[MINOR→修] cpp/header「Ksigma2/3→S9c」措辭**:S9c 用 FD 一致切線,解析 Ksigma2/3 留未來。
- **[誠實標,未補測] 已知覆蓋缺口**:① **auto `Dl` 路徑無測**(已知對軟方向失效,snap-through 須手動 arcLength,
  踩雷①);② F53b 平移 `tip Uy==δ` check 是機械強制(反力 check 才是真 oracle,已註解);③ F53c FD==main 是
  自洽(main 正確性由 F50/F51/OpenSees 1.22e-9 獨立確立);④ FD 迭代數較多(160 vs 80)因每步 N 次額外組裝非
  收斂較慢;⑤ snap-through 單一幾何 / 大位移 UDL / arc-length+UDL 組合未測(誠實標,留後續)。

審核後重跑五腿全綠(standalone F1-F53 / UE 49 / OpenSees PASS / audit 101 / CLI ALL PASS),**零 CRITICAL 殘留**。

## 下一步

- **S10**:N-M 互動塑鉸(co-rotational 線已收尾,下一步即此;R4 方向耦合;塑鉸局部共旋座標,`pb` 餵 N-M 面;
  WS_G 路線 G1 `Mp_eff=Mp·f(N/Np)`)。
- **S9c 後續**(非阻塞):解析 Ksigma2/3 spin 切線、follower UDL、spherical arc-length(snap-back)、
  bifurcation 分支切換。
