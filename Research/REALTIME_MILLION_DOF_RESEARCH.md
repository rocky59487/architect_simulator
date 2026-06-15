# 即時百萬 DOF 結構求解 — 研究方向:壓低 factor,單一 direct 算法

> **開新視窗用這份無縫接續。** 前一個對話太長。**方向已修正(見「方向轉變」)**:不走 HP 迭代/seed 路線,改**正面壓低 LDLᵀ factor**,讓單一 sparse-direct 算法 scale 到百萬 DOF,廢掉 HP 雙車道。語言一律中文。

## 一句話目標
即時(互動級 ~16–100ms / 重解)求解百萬(~10⁶)DOF 線彈性,精度 `‖u−u_exact‖/‖u_exact‖ ≤ 1e-9`,**用單一 direct(Cholesky/LDLᵀ)算法**(factor 一次 + 每幀 backsub),不要 HP 雙車道。

## 第 0 層:基石質疑(動工前必先回答 — 使用者提出,最上位)
**不要預設 LDLᵀ 再去加速它。** 使用者點破:「要嘛繞開 factor(HP/迭代,脆弱依賴 seed),要嘛 factor 很久(naive direct)」是**假二分法**。先評估基石光譜:
- **(a) naive sparse direct** = 現狀 `Eigen::SimplicialLDLT`(simplicial,無 supernode/BLAS3)。中小最優,大問題 factor 時間+fill 爆。
- **(b) supernodal/multifrontal direct**(CHOLMOD/PARDISO/MUMPS)。比 (a) 快 10–50x 常數倍,但 **fill 仍 O(n^{4/3}),百萬 DOF 記憶體可能仍爆**。
- **(c) rank-structured / low-rank direct**(BLR multifrontal、HSS、H-matrix;MUMPS-BLR / STRUMPACK / PaStiX-BLR)。**仍是 direct(一次 factor + backsub、無 seed、無迭代收斂、任意 RHS 都快 —— 無 HP 脆弱)**,但用 K⁻¹ 的階層低秩結構把 fill 壓到**近 O(n)** → **改變複雜度階,factor 不再貴**。**這最可能同時破解「繞開的脆弱」與「factor 貴」兩個痛點。**
- **(d) model reduction / ROM**(modal/POD/Krylov 離線建基)。固定結構離線建基 → 即時投影小系統。= HP seeded 的「正統、無 per-frame seed」版,無 HP 重傷,但有子空間完整性 tradeoff(載重離開基失準,須界定載重族)。
- **(e) iterative(PCG+AMG / HP)** = 前一版方向,**降為最後手段**(seed 脆弱 + AMG 難 + 任意 RHS 慢)。
- **三重約束的硬張力**:百萬 DOF + **1e-9 精度** + 即時。(c)/(d) 靠低秩/降階近似,1e-9 會逼高 rank(壓縮變少→factor 變回貴)或逼大基(ROM 失優勢)。**可能無單一方法全滿足 → 可行性評估要誠實找出可達的 (規模, 精度, 時間) 邊界,而非硬湊百萬+1e-9+即時。**
- **結論定調**:不是 LDLᵀ 錯,是「naive 實作 + direct-vs-iterative 二分」錯。基石升級往 **(c) low-rank direct 或 (d) ROM**(兩者保留 direct 的無脆弱性),非回脆弱純迭代。

## 方向轉變(承上 — 為何不是 HP/迭代)
- **HP 依賴 seed = 重傷**:out-of-subspace / 單次 / 任意 RHS 慘敗 40–700x(benchmark 實證),與自家 LDLT 形成「窄賽道自我競爭」的雙車道。
- **direct 的 backsub 本來就 ~17x 勝 PCG**(WS_B 硬事實);若選對 direct 基石((c)/(b)),「factor 一次(結構固定,setup 攤銷)+ 每幀 backsub(~ms)」全面勝 —— 無 seed、無 PCG、無雙車道,**單一乾淨算法**,HP 連同 seed 重傷退役。

## factor wall 很可能是「假牆」(誠實技術判斷,待驗)
FrameCore 現用 **`Eigen::SimplicialLDLT`** — simplicial(逐列、無 supernode、不吃 BLAS3 dense kernel),naive 實作。WS_B 實測 factor 48k DOF 已 20.7s,**但這很可能是 Eigen simplicial 太弱,不是 factor 的本質下限**:
- **fill-reducing ordering**(AMD / METIS / nested dissection):3D 問題用 nested dissection 把 fill 壓到近最優;Eigen 預設 AMD,METIS ND 對 3D 通常更好 —— **單這步可能就推牆數倍**。
- **supernodal / multifrontal**(CHOLMOD / PARDISO / MUMPS):factor 組織成 dense supernode blocks 走 BLAS3,典型比 simplicial 快 **10–50x** + 多執行緒 + out-of-core。
→ 換好 ordering + supernodal,factor wall 可能從 48k 推到百萬級(setup 秒–分鐘、記憶體 GB 級,**對固定結構 factor-once 可接受**)。

## 三方法(達成「壓低 factor → 單一 direct」)
1. **Fill-reducing ordering**:Eigen 現用 ordering vs METIS nested dissection 對 3D 彈性的 fill / factor 時間影響。
2. **Supernodal / multifrontal factor**:(a) 工業庫 CHOLMOD(supernodal)/ PARDISO / MUMPS — 快但**新依賴**(授權 + dual-build + UE 整合,鐵則張力,先評估);(b) 自實作 supernodal Cholesky — 無依賴但大工作。
3. **Factor-once + backsub-many**:固定結構 factor 一次(PreparedSystem 既有設計),每幀只 backsub(O(fill) ~ms)。量百萬 DOF backsub 是否即時。

## 載體:先只做可行性評估 + 小原型(使用者要求)
不投入大工程。先 go/no-go:壓低 factor 能否讓單一 direct scale 到百萬 DOF + 即時 backsub。**成功 → 廢 HP 雙車道,引擎統一單一算法(這才是真目標)。** research-only,評估階段不進五腿 gate、不改 default solve,現有 LDLT / 解析解為 oracle。

## 可行性評估要回答的(go/no-go)
1. **ordering 的威力**:同一 3D 彈性網格,Eigen 預設 vs METIS nested dissection,factor 時間 + fill(記憶體)差多少?(可能單步推牆數倍)
2. **simplicial vs supernodal**:Eigen SimplicialLDLT vs CHOLMOD supernodal(或自實作)在 ~10萬 DOF 的 factor 時間,外推百萬。快幾倍?
3. **百萬 DOF factor 可行性**:最佳 ordering + supernodal 下,百萬 DOF 3D factor setup(秒?分鐘?)+ fill 記憶體(GB?fit RAM?)。
4. **每幀 backsub 即時性**:百萬 DOF backsub 時間(~ms?)→ 達互動 / 幀率?
5. **依賴取捨**:CHOLMOD/PARDISO/MUMPS 的授權 + 純 C++17/Eigen 雙車道 + UE dual-build + 五腿 gate 整合代價 vs 自實作 supernodal 的工作量。
6. **壓低後 HP 是否完全多餘**:確認「factor-once + backsub」在固定結構全面勝 HP → 可廢雙車道(結構**改變**的增量更新是 ReSolve/S1 領域,非 HP seed)。
7. **factor 本質下限 + 低秩/降階出路(承第 0 層)**:若 supernodal 的 O(n^{4/3}) fill 在百萬 DOF 記憶體爆 → 評估 **(c) rank-structured direct**(BLR/HSS/H-matrix,fill 壓近 O(n))的 fill/factor/backsub + 達 1e-9 所需 truncation rank(精度↔壓縮 tradeoff);及 **(d) ROM**(離線基維度 vs 載重族完整性)。這兩者才是「factor 不貴 *且* 無迭代脆弱」的真出路;純迭代 (e) 是最後手段。

## 現狀 / 路徑 / 工具(交接)
- branch `research/hpfem-solver-v1`,HEAD = 本檔最新 commit。**WS_B seeded HpSession 已完成但窄、未發布、本方向下大機率退役**(保留作對照 oracle / 結構變動的 ReSolve 增量場景)。
- LDLT 在 `Source/FrameCore/Private/FrameEigen.h`:`using LDLTSolver = Eigen::SimplicialLDLT<SpMat>`;`Private/FrameSolver.cpp` 的 assembleAndFactor/solveLoad;`Private/PreparedSystemImpl.h` 持 K/fmap/ldlt(factor-once 既有設計)。
- benchmark 範本:`Standalone/hp_bench.cpp`(改成量 factor / backsub vs ordering / solver)。
- 背景:`Research/WS_B_solver/HPFEM_RESEARCH_NOTES.md`(factor wall scale story、reused-backsub ~17x 勝 PCG 的硬事實);記憶 `hpfem-solver-research.md`(durable 教訓 ①–⑨ + 環境踩雷)。

## 鐵則
research-only(評估階段不進五腿 gate、不改 default solve);現有 LDLT / 解析解為正確性 oracle;**誠實:報實測 factor/backsub 時間 + 記憶體,別宣稱未驗證的 scaling**(上一輪最大教訓);過 gate 才 commit;顯式 `git add` 不 `-A`;不碰 `.gitignore`/`ArchSim.uproject`/`Plugins/LevelSim/`/`Research/WS_C`/`Research/WS_N`/build 產物。**新依賴(CHOLMOD/METIS/...)先在評估報告明列授權 + dual-build + 鐵則張力,不擅自引入。** 環境:bash cwd 在 `/e/project` → `git -C /e/project/ArchSim`;UE build 引號/PowerShell 正斜線見記憶。

## 建議第一步
小原型:可調規模 3D structured 彈性 grid(~1萬→10萬 DOF),量:(a) Eigen SimplicialLDLT factor 時間 + 記憶體 vs 規模 + ordering(natural / AMD / 若可接 METIS);(b) backsub 時間 vs 規模;(c) 外推百萬 DOF 的 factor setup / 記憶體 / backsub。若 ordering 單步就大幅推牆 → 可能不需工業庫。據此寫 **go/no-go:單一 direct 能否 scale 到百萬 DOF 即時** → 能則 HP 退役、引擎統一。**先評估,別造大輪子。** 用 Workflow / 平行 agent + 對抗式查核維持誠實。

---

## go/no-go 結果(2026-06-14)— factor 假牆擊穿,判定「部分 GO」

**原型**:`Research/WS_B_solver/exp_supernodal_compare.cpp`(+ `exp_cholmod_smoke.cpp` link smoke / `build_supernodal.bat` 自包含 build / `run_supernodal.ps1` 掃描 driver);conda env `framecore-direct`(conda-forge `suitesparse`+`metis`,Eigen 自帶 `CholmodSupport`/`MetisSupport` wrapper)。混合建築 = `makeTower` frame + 每層每 bay 一 MITC4 shell 樓板(載重仍 nodal-only,過 `assertNodalOnly`);同一 K_ff 上比 `SimplicialLDLT`(AMD) / `SimplicialLDLT`(METIS) / `CholmodSupernodalLLT`。機器 Ryzen 9 8940HX / 31.2 GiB。

**實測(6 規模 nf 8.9k→191k)**:

| nf | AMD factor | CHOLMOD factor | CHOLMOD/AMD | CHOLMOD backsub | peak |
|---|---|---|---|---|---|
| 17,160 | 6.1 s | 0.22 s | 27.8x | 5.7 ms | 362 MiB |
| 31,824 | 23.3 s | 0.51 s | 45.2x | 13.8 ms | 842 MiB |
| 191,400 | (略,太慢) | 6.07 s | — | 132.8 ms | 10.0 GiB |

**外推百萬(numpy log-log,高端段)**:AMD factor exp **3.03** → ~262 h;CHOLMOD factor exp **1.56** → ~80 s;backsub exp 1.26 → ~1.05 s;factor 駐留 ~19 GiB;peak ~117 GiB。

**五判準**:① factor ≥10x ✓✓✓(假牆是真假牆) ② exp ≤1.7 ✓(~1.4) ③ backsub ≤100ms **✗**(百萬 ~1s) ④ 記憶體 fit 32GB **✗**(百萬 peak ~117GB;實測這台上限 ~190k) ⑤ res ≤1e-9 ◐(≤113k OK,191k=1.6e-9 需 iterative refinement)。

**可達邊界(32GB + 混合建築)**:每幀即時(backsub ≤100ms)~150k;互動(~1s)~390k(peak 限)/ 近百萬(駐留,需優化臨時記憶體);factor 百萬 ~80s 可行。

**判定**:**部分 GO**。factor 核心問題**解決**(HP 的 factor-bypass 理由消失 → 中小規模 CHOLMOD 全面勝 HP → 對實際建築規模「廢雙車道、統一單一 direct」成立);百萬 + 即時 + 1e-9 是單機**物理牆**(記憶體 + backsub,非算法問題)。

## 下一步:自建 BLAS3-accelerated supernodal(新大階段)

**決策(使用者)**:CHOLMOD 當**參考**(效能對標 + 正確性 oracle),自建 supernodal direct、dense block 走 **OpenBLAS 之類 BLAS3 kernel**(`dpotrf`/`dsyrk`/`dgemm`/`dtrsm`)逼近甚至超越 CHOLMOD。動機:避開 CHOLMOD supernodal 模組 GPL,改用 BSD 的 OpenBLAS;拿到 supernodal 的 BLAS3 速度。
- 核心:supernode 偵測(elimination tree + 合併)+ fill-reducing ordering(METIS or 自寫 ND)+ left/right-looking supernodal Cholesky,dense panel 交 BLAS3。
- 鐵則張力:OpenBLAS 是依賴(BSD,較乾淨);Eigen 仍 Private;production 整合需 dual-build。
- research-only 起步,過五腿 gate 才入生產。HP 雙車道退役。

## 進度(2026-06-14,research-only,未 commit)

- **M0 PASS**:OpenBLAS 裝進 conda env `framecore-direct`(headers 在 `Library/include/openblas/`、`openblas.lib` 為 MSVC 可用 ar 格式、DLL + MinGW runtime(libgcc/quadmath/winpthread)在 bin)。`exp_openblas_smoke.cpp` 四個 API(dgemm/dpotrf/dtrsm/dsyrk)link + 正確,**dgemm 378 GFLOPS**(Ryzen 9 8940HX Zen4;這是超越 MKL-backed CHOLMOD 的依據)。**durable**:`lapacke.h`→`lapack.h` 用 C99 `_Complex`,MSVC C++ 不支援 → 不 include lapacke.h,自宣告 `extern "C" int LAPACKE_dpotrf(int,char,int,double*,int)` + `#define LAPACK_COL_MAJOR 102`;`metis.h` C4005 用 `/wd4005` 壓。
- **M1 symbolic PASS**:`sn_chol.h`(header-only,raw CSC 輸入,只依賴 metis):`analyze` = extractLower → `METIS_NodeND` → permuteFull → etree(Davis cs_etree,用 strict-upper i<k)→ symbolic factorization(L pattern per col,etree children merge)。`exp_sn_chol.cpp`:natural & METIS 的 L fill 對 Eigen `SimplicialLDLT`(Natural/Metis)**EXACT MATCH**(ratio 1.000,連 METIS perm/iperm 約定都對);METIS 砍 fill 50–67%。**durable**:Eigen `matrixL()` 是單位下三角,對角隱含不存,`nonZeros()` 已是 strict-lower(對比別多減 n)。build:`build_supernodal.bat sn`(重用 `obj_sn_core`,需先 `compare` 建 core)。
- **M1 numeric (column-based) PASS**:`sn_chol.h` 加 `SnFactor`/`factorize`(left-looking column:dense accumulator + `rowOf` 轉置當左看來源 + permuted lower A values,取 `iperm[i]>=iperm[j]` 一次避對稱重複)+ `solve`(forward/backward + permute)。`exp_sn_chol` numeric 測試對 `CholmodSupernodalLLT` oracle:poisson20/40 + tower-Kff(nf=3780)全 **PASS**(res 1e-11~1e-14、vsCHOLMOD 1e-13~1e-15)。**自建完整 sparse Cholesky direct solver 正確運作**(METIS+symbolic+numeric+solve)。注意這是**正確性參考**(scalar/sparse,無 BLAS3,不對標速度)。
- **M1 step 2b supernodal BLAS3 PASS** = M1 完整完成:`sn_chol.h` 加 `SnSuper`/`factorizeSuper`(fundamental supernode = 連續列巢狀 pattern,relaxed 無 postorder → 不利 ordering 只是 supernode 變小、仍正確;dense panel col-major left-looking;`cblas_dgemm` 算 Schur complement `RowMat*ColMat^T` + scatter-gather 相對索引 + `assert`;`LAPACKE_dpotrf` 對角 block + `cblas_dtrsm` off-diag)+ `solveSuper`(dtrsv/dgemv 前後代換)。對 CHOLMOD + column-based 雙驗:poisson40 res 3.4e-14、tower-Kff(3780) res 4.4e-11 **PASS**。**durable**:backward 代換 `y_diag -= L_odᵀ·x_below` 的 `cblas_dgemv` alpha 必須 **-1.0**(forward 手動 `-=` 對;backward 改用 dgemv 易漏負號 → 解全錯但 assert 不觸發,靠 CHOLMOD oracle 抓)。**效能(誠實,小規模)**:poisson40 0.58x(我快,size-1 多、CHOLMOD overhead 相對大)、tower-3780 **3.43x(我慢,落在 fundamental supernode 慢 2–5x 預期)**。**M1 gate 通過**(正確 + 合理倍數 + OpenBLAS 路線可行)。
- **M2 大規模對標完成**(`exp_sn_chol --bigSweep` + `run_sn_sweep.ps1`,混合建築):sn supernodal vs CHOLMOD factor — 17k 2.8x、32k 3.1x、**64k 2.35x**(兩次跑都 ~2.2–2.35x,穩定;單次 factor 有噪音)。**vsCHOLMOD 1e-12~1e-13**(自建解 == CHOLMOD,正確性無虞)。**res 判讀(誠實,重要)**:raw res0 ~1e-9(17k 1.1e-9 → 64k 3.0e-9)是混合建築 K 高條件數(shell drilling + 大跨)的 direct-solve 固有底限,**CHOLMOD 在同 K 一樣**(非 solver bug,vsCHOLMOD 1e-12 為證);一步 iterative refinement(重用 factor)砍 ~2x(res1:64k 1.4e-9,仍略超 1e-9 因 cond 限制)。**residual 1e-9 工程足夠**;但「相對誤差 ≤1e-9」= cond×res,高 cond 下難達 —— 這是 direct solver 本質(go/no-go 第 0 層的精度張力),非自建問題。fundamental supernode 平均 ~7 列(64k nsn=8840,偏碎)。
- **M3a amalgamation 完成(誠實:無淨加速)**:`factorizeSuper(amalgRelax, amalgMaxCol)` 貪婪合併連續 supernode(addedRows ≤ relax 且 ncol ≤ maxcol),panel rows = member pattern 的 union。**durable fix**:amalgamation 的 zero-pad below row 可能不在目標 R_J,但其 Schur 貢獻**可證為零**(否則該 K 列會把它 fill 進 R_J)→ scatter `if(rp<0) continue` 跳過(原本是 hard assert,大規模才觸發)。掃 relax 0/16/48(17k–64k):nsn 大降(64k 8840→4217,panel 放大)、vsCHOLMOD 仍 1e-12(正確),**但 facMs 幾乎不變/略升**(64k 2.42→2.30→2.43x)。原因:zero-pad 多餘 flops + 每-factor 重算 union-loop 的開銷,抵消 panel 增益。**結論:amalgamation 單獨非縮小 2.3x 的槓桿。**
- **瓶頸重新定位 = 單執行緒**:自建 left-looking 序列(僅 panel 內 OpenBLAS 多緒,panel 小時多緒效益低),CHOLMOD 走 MKL 多執行緒(8940HX 16C/32T)。**M3b 並行(etree level-set 平行 supernode,thread pool)才是縮小 2.3x 的關鍵。**
- **單對單診斷(2026-06-15,確認 M3b 必中)**:OpenBLAS + MKL 都鎖單執行緒(`OMP_NUM_THREADS=OPENBLAS_NUM_THREADS=MKL_NUM_THREADS=1`),自建 vs CHOLMOD:**32k 1.13–1.20x、64k 1.08–1.19x** —— 單執行緒下自建算法**幾乎追平 CHOLMOD**(多執行緒的 2.3–2.6x 差距**全在多核 supernode 並行**)。自建「多執行緒」版 ≈ 單執行緒(64k 2660 vs 2747ms),因 panel 內 OpenBLAS 多緒對小 panel 幾乎無效;CHOLMOD 多核是 **supernode 級**並行(64k 多緒 1099 vs 單緒 2552ms ≈ 2.3x)。**結論:補 supernode 級並行即可打平,OpenBLAS Zen4 378 GFLOPS 有望超越 MKL。**
- **下一步 M3b(綠燈)**:etree(supernode-level)level-set 並行 factor — 同 level 無依賴的 supernode 用 thread pool 平行(依賴 = updaters[J])。amalgamation 在並行下才划算(夠大 panel 餵 worker);多步 iterative refinement 補 res 1e-9;union-loop 開銷移進 analyze(算一次)。column-based 版留作交叉驗 oracle。

## M3b 完成(2026-06-15)— supernode 級 level-set 並行,基本打平 MKL-CHOLMOD ✅

**實作**:`sn_chol.h` 新增 `factorizeSuperParallel`(序列 `factorizeSuper` 保留作 bit-exact 交叉 oracle)。抽出共用 `detail::buildSuperStructure`(supernode 劃分+amalg+panel+acol+updaters)+ `detail::processSupernode`(單 J factor;rowpos RAII 清零防殘留污染;`spdOk` 用 `std::atomic<bool>`)→ 序列/並行零邏輯漂移(bit-exact 前提)。`level[J]=1+max(level[K∈updaters[J]])`(updaters 全 < J)正序一次掃出 level-set;持久 thread pool(generation+cv barrier + `std::atomic<int>` 動態搶任務,純 C++17 `<thread>/<atomic>/<mutex>/<condition_variable>`,無 OpenMP/新依賴)。**混合並行(達標關鍵)**:寬(葉)level → supernode 並行 + 每 worker 單緒 BLAS;窄(根,`size≤nt/2`)level → 主緒序列 + 多緒 panel BLAS(打開 etree 根部 Amdahl 瓶頸,CHOLMOD 正是對大 supernode 開多緒)。OpenBLAS 緒數由主緒每 level dispatch 前設一次(全域狀態,worker 不可碰),結束保存/恢復。

**實測(8940HX 16C/32T,混合建築 frame+MITC4,relax=16,預設 nt=physical=16)**:

| nf | 並行 sp(vs 序列) | 並行 vsCHOLMOD | bit-exact(階段一) | res |
|---|---|---|---|---|
| 17,160 | 3.1x | **1.15x**(nt=8 達 **1.03x**) | `bitdiff=0 valdiff=0` | 5.7e-10 |
| 32,448 | 2.3x | **1.21x** | `bitdiff=0 valdiff=0` | 8.8e-10 |
| 64,260 | 2.1x | **1.16x** | `bitdiff=0 valdiff=0` | 1.4e-9(cond) |

**判定 ✅**:自建從序列的 ~2.5–3.6x 落後 CHOLMOD(ser/cholmod),並行後**基本打平 MKL-backed CHOLMOD(vsCHOLMOD 1.15–1.21x,17k nt=8 達 1.03x)**。**正確性兩階段 gate 全綠**:階段一(序列 oracle + 並行皆鎖 OpenBLAS 單緒)逐位元相同(`bitdiff=0` = 無 race/排程 bug,排除 BLAS reassociation 假失敗);vsCHOLMOD ≤ 2.4e-12(並行解匹配獨立 oracle)。speedup 非線性(8–16 核 2–3x),受 Amdahl 根部 + 記憶體頻寬限,但 CHOLMOD 同受限故打平。

**意義**:R-line「廢 HP 雙車道、統一單一 direct」效能拼圖補齊 —— 自建 BLAS3 supernodal(OpenBLAS BSD,避 CHOLMOD supernodal 模組 GPL)多核下 ≈ 工業庫 MKL-CHOLMOD。

**durable 踩雷(M3b)**:
1. **HT 災難**:預設 `nt=hardware_concurrency`(=32 含 16 HT)**最差**(17k vsCHOLMOD 2.53x、sp 僅 1.2x);dense BLAS3 factor 記憶體頻寬/FPU 限,HT 兄弟搶資源。改 `nt=physical`(halve logical)→ vsCHOLMOD 1.15x、sp 2.6x。**邊際:nt=8 在 17k 比 16 更佳(memory-bound),大規模 8/16 相近。**
2. **OpenBLAS 緒數是全域狀態**:`openblas_set_num_threads` 不可每 worker call(race+冗餘);主緒每 level 設一次(葉 1 / 根 N),結束 `openblas_get_num_threads` 保存/恢復。
3. **bit-exact 緒數前提**:序列 oracle 跑預設多緒 BLAS、並行鎖單緒 → dgemm reduction 順序不同 → `memcmp` 假失敗(非 race)。階段一序列 oracle **也須**鎖單緒;階段二(根部多緒)改 tolerance gate。
4. **res~1.4e-9@64k = fixed-precision refinement 固有底限(~cond·eps)**:混合建築高 cond(shell drilling/大跨),iterative refinement 卡 1.4e-9 不收斂(3 步無降),**CHOLMOD 同 K 一樣**。正確性 gate 用 **vsCHOLMOD(對獨立 oracle)非 residual**;res 標 `(cond)`。
5. **參數錯位真實咬人**:`factorizeSuperParallel(...,amalgRelax,amalgMaxCol,numThreads,blasThreadsRoot)`,呼叫漏 amalgMaxCol → nt 餵給它(=0→amalg 失效=fundamental supernode)、numThreads 變 blasRoot → 假 bit-exact 失敗(nsn 結構不同;DIAG 抓出 ser=67 vs par=95)。顯式具名傳參。

## M3b 後續:nt 自動調優(sqrt(nf) 啟發式)— 17k 超越 CHOLMOD(2026-06-15)

系統掃 nt(17k/64k × nt=4..16)揭穿一個反直覺事實:**dense panel factor 是 memory-bandwidth bound,最佳 nt 遠低於核數且隨規模增,過度並行會退化**:

| nf | 最佳 nt | 該 nt vsCHOLMOD | nt=16(舊預設)vsCHOLMOD |
|---|---|---|---|
| 17,160 | 6 | **0.99x(掃描)/ 0.94x(預設跑)→ 超越** | 1.19x |
| 32,448 | ~9 | 1.18x | 1.20x |
| 64,260 | 12 | 1.11x | 1.15x |

最佳 nt ≈ **√nf / 20**(√17160/20=6.5、√64260/20=12.7,擬合極準;物理上 dense panel 算術強度 ∝ panel size ∝ √nf,飽和 thread 數隨之)。預設改為 `recommendedThreads(n)=clamp(√nf/20, 2, physical)`(/20 是頻寬/核校準 8940HX,`--threads` 覆蓋),`factorizeSuperParallel` 與 benchmark 共用此 helper。

**結果**:17k 預設**超越 MKL-CHOLMOD(0.94x)**;32k/64k 接近各自最佳但**仍慢 12–18%**(speedup 大規模僅 2.0–2.4x,根部 Amdahl + 頻寬更吃緊)。

**CCD affinity(已試 = 負面,2026-06-15)**:綁 worker 到物理核 `2*tid`(SMT 兄弟相鄰假設,讓 nt≤8 集中單 CCD L3)在 17k(0.97→1.01x)、64k(1.12→1.14x)**皆略退步**。原因:OS 排程器已置放良好,硬 pin 剝奪其彈性、且與 pool 的**動態搶任務**衝突(綁死的 worker 搶到的 panel 資料可能在另一 CCD),panel 共享也使 pin 無法改變跨 CCD 性。NUMA-aware 資料置放(panel 按 CCD 分配 + 任務-資料親和排程)才是真槓桿,但工程大、收益不確定 → 暫不追(reverted,sn_chol.h 留 NOTE)。

**下一步候選**:大規模超越(更大 amalgamation panel 餵 dgemm 近 peak、union-loop 移進 analyze、並行 setup;NUMA-aware 資料置放);百萬 DOF 實測(記憶體 peak,M2 外推 ~117GB peak/~19GB 駐留,須臨時記憶體優化);extended-precision residual 破 cond 底限達真 1e-9;**production 整合(dual-build + 五腿 gate)入引擎統一 direct,HP 雙車道退役**。

## Production 整合階段 1+2 完成(2026-06-15)— opt-in supernodal lane 進生產,五腿綠

把自建 supernodal 接進生產 FrameCore 作 **opt-in solve lane**(仿 HpSolver opt-in 範本),LDLT 永 default + fallback,`default solve()` 一字不動。

- **階段 1(standalone,`bafbb1b`)**:vendor `sn_chol.h`→`Private/` + `FrameSnChol.h` choke point(`FRAMECORE_SUPERNODAL` 守門,standalone=1 / UE=0)+ Public POD `SnSolveOptions.h`/`SnSolver.h`(零 Eigen)+ `Private/SnSolver.cpp`(guard+RHS+scatter+reactions+recover **verbatim 自 solveLoad**;solve step = reduceFF→analyze→factorizeSuperParallel→solveSuper;`!fac.spd`/disabled/not-compiled→LDLT fallback)+ build.bat link conda OpenBLAS/METIS + **F57**(frame/UDL/settlement/release/shell vs LDLT rel<1e-10、disabled drop-in rel<1e-12、mechanism→fallback→singular 非 NaN)。standalone F1-F57 ALL PASS。
- **階段 2(UE wiring,`391a9ba`)**:`FrameCore.Build.cs` link UE 內建 METIS(預備,SUPERNODAL=0 未引用)+ `Private/Tests/SupernodalSolverTest.cpp`(UE drop-in/fallback 合約)+ run_gate `$ExpectedUeTests` 50→51。**UE build Succeeded**(SnSolver.cpp 在 UE `FRAMECORE_SUPERNODAL=0` 走 LDLT,不需 OpenBLAS;metis link 不破壞 UE build)。**五腿全綠**(standalone F57 / UE 51 / OpenSees / audit 104 / CLI)。
- **durable**:① UE 不能用 conda MinGW OpenBLAS(GNU-ar import lib + MinGW/MSVC ABI 混用;**授權其實非阻塞**——pthreads build,libgcc Runtime-Exception / libquadmath LGPL / libwinpthread MIT 皆允許動態鏈接)→ UE supernodal body 由 `FRAMECORE_SUPERNODAL` 編出,UE solveLoadSupernodal 走 LDLT;② `SnSolver.h` 簽章**不**守門(否則 UE test 無法呼叫驗 drop-in),body 在 `.cpp` 內 `#if` 分流;③ standalone gate leg 此後**依賴 conda framecore-direct env**(build.bat link conda);④ UE test 用 local lambda 非 file-local static(unity 安全)。
- **階段 3a 完成(`3caa30c`,UE supernodal LIVE,五腿綠)** — **dumpbin 推翻「需 MSVC-clean OpenBLAS」前提**:`openblas.dll` 只依賴 VCRUNTIME140 + UCRT(MSVC-runtime-clean,**非 MinGW**;env 的 libgcc/libgomp/libquadmath/libwinpthread 是別 conda 套件帶的、**非 openblas 的 import**),conda metis 是 static lib `IDXTYPEWIDTH=32`(配 sn_chol idx_t=int32)。故**直接用 conda OpenBLAS 進 UE**(不需 vcpkg/rebuild,vcpkg 也沒裝):`FrameCore.Build.cs` 指 conda env(openblas include/lib + DLL RuntimeDependency + delay-load、metis static、`FRAMECORE_SUPERNODAL=1` Win64、env 缺則 OFF→LDLT)+ `FrameCoreModule.cpp` StartupModule **`GetDllHandle` 預載 openblas.dll**(讓 delay-load thunk 命中;否則首次 cblas call faults——實測 crash 過)+ FrameSnChol.h 壓 C4005。**五腿綠、UE automation 51 跑真 supernodal vs LDLT(非 fallback)**。**durable**:① UE delay-load DLL 不在 editor search path → module StartupModule 預載 full path;② RuntimeDependencies 對 editor build 只記錄、不 copy DLL(packaging 才 copy)故需預載;③ **dumpbin /dependents 才是 DLL 真實 ABI 的權威——別憑 import lib 是 `!<arch>` 就判 MinGW(MSVC .lib 也是 ar 格式)**。
- **階段 3b 完成(`1807b99`)— SnSession factor-once + solve-many,production 整合 COMPLETE**:`SnSession`(PIMPL,ctor reduceFF+analyze+factorizeSuperParallel **factor 一次** / solveFrame solveSuper **重用**,仿 HpSession:non-owning PreparedSystem ptr + fingerprint guard + move-only + 零 Eigen public)——stateless `solveLoadSupernodal` 每次重 factor,SnSession 發揮 supernodal「factor-once + solve-many」(遊戲每幀 solve 主場;大模型 supernodal factor ~27x 快於 SimplicialLDLT + 重用攤銷)。standalone **F58** + UE **FFrameCoreSnSessionTest**:3× solveFrame 重用同一 factor 各對 LDLT rel<1e-10、disabled drop-in、gate 52,**五腿綠**。
- **★ production 整合四階段全完成**(1 standalone `bafbb1b` / 2 UE wiring `391a9ba` / 3a UE supernodal LIVE `3caa30c` / 3b SnSession `1807b99`):**standalone + UE 都跑自建 supernodal**(stateless `solveLoadSupernodal` + 重用 `SnSession`),LDLT 永 oracle/fallback,`default solve()` 不動,五腿全綠。R-line「廢 HP 雙車道、統一單一 direct」由研究 → 生產落地。

## 收割回 main + 退役 HP(2026-06-15,main `7030321`)

production 整合四階段(全在本 research 分支)後,supernodal 成果**收割回 main 並退役 HP 雙車道**:收尾分支 `chore/supernodal-to-main` 從 main checkout research 的 supernodal 15 檔 → 去 HP → 單 commit `--ff-only` 進 main → push。HP 生產碼 9 檔(HpSolver/HpSession/hp_bench/hp_stress)移除,完整研究歷史(含 HP 探索與負面結果)**凍結保留本 research 分支**。main = 乾淨 single-direct(LDLT default + supernodal opt-in **F55** stateless / **F56** SnSession,UE 52,五腿 F1-F56 綠);新增 main-facing `docs/PROGRESS_R_supernodal.md` + VERIFICATION §3.8。

## Phase E:可達邊界極限實測 + 精度破底限(2026-06-15,research-only)

`exp_sn_chol.cpp` 加 `--limit`(`testSuperLimit`):單規模實測 parallel factor / per-frame backsub / peak working-set(`K32GetProcessMemoryInfo`)/ residual at double-vs-Neumaier-compensated refinement;一 process/規模(peak 乾淨)。混合建築 17k–100k:

| nf | factorMs | backsubMs/幀 | peakMB | resD(1x) | resComp(3x) |
|---|---|---|---|---|---|
| 17160 | 199 | 5.9 | 448 | 5.7e-10 | 2.9e-10 |
| 31824 | 524 | 13.6 | 1036 | 8.8e-10 | 4.8e-10 |
| 64260 | 1250 | 34.1 | 2772 | 1.40e-9 | **6.9e-10** |
| 99750 | 2561 | 61.6 | 4931 | 1.95e-9 | 1.07e-9 |

**發現 1 — 可達邊界宣稱驗證(純外推 → 17k–100k 實測 + 擬合外推)**:factor exp **≈1.57**(≈ go/no-go CHOLMOD exp 1.56)→ 百萬 ~120s(同量級於宣稱 ~80s,自建略高);peak working-set exp **≈1.36** → 百萬 **~110GB**(✓ 驗證宣稱 ~117GB);backsub exp **≈1.65** → 每幀 100ms 即時邊界 **~120k**、1s 互動邊界 **~500k**(量級對齊宣稱 ~150k 即時 / ~390k 互動;backsub 比 factor 緩 → 互動邊界更樂觀)。32GB working-set 外推 ~360k,但 factor 臨時 commit 更高 → 實際單機 OOM 估 ~200–250k,與 ~190k 量級一致。

**發現 2 — 精度破 cond 底限(Neumaier 補償 residual)**:double 1-step refinement(resD)在高 cond 混合建築卡 fixed-precision 底限 ~cond·eps(64k 1.40e-9 / 100k 1.95e-9 > 真 1e-9);**改用 Neumaier 補償求和算 residual `r = F − K x`(double 硬體上真高精度)+ 3-step refinement → resComp 破底限**(64k **6.92e-10** / 100k 1.07e-9),真 1e-9 精度規模從 double 的 **~40k 推到 ~90k**(~2x)。**durable 踩雷**:**MSVC `long double` == `double`(64-bit)** → 初版用 long double 算 residual 的 resLD ≈ resD(看似「extended-precision 不破底限」)是**假象**;補償求和(Kahan/Neumaier)才是 double 硬體上做高精度 residual 的正確手段。底限的真實成因是 **residual cancellation**(可由補償求和破),要更低需 quad factor(超範圍)。

**測試框架邊界(誠實)**:`--limit` 受 `main` 前置 `assembleAndFactor`(SimplicialLDLT,為拿 K_ff 先跑 default factor)限,130k+ 即撞 SimplicialLDLT 慢路徑(整個 research 流程拿 K_ff 都經它,故 M2 sweep 也止於 64k),**非 supernodal 限**;實測止於 100k,130k+ 用 exp 外推。資料 `Research/out/sn_limit_sweep.txt`。

**Phase E 結論**:三重約束(百萬 + 即時 + 1e-9)可達邊界由純外推升級為 17k–100k 實測擬合,量級全部對齊原宣稱;補償-residual refinement 把即時 1e-9 精度規模推 ~2x。**R-line 研究收尾。**
