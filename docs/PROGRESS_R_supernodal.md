# R 線 — 自建 supernodal direct solver(進生產 opt-in lane)

> **狀態(2026-06)**:R 線「即時百萬 DOF」研究的 factor 核心已收斂並收割進主線:自建
> BLAS3 supernodal Cholesky 作為 **opt-in solver lane** 進生產 FrameCore(standalone + UE),
> `SimplicialLDLT` 永遠是 default 與 fallback。同輪**退役**早期的 HP-FEM 迭代雙車道
> (matrix-free PCG + seeded session),統一為單一 direct solver。五腿 gate 全綠
> (standalone F1–F56 / UE 52 / OpenSees / audit 104 / CLI)。
>
> 完整研究歷史(go/no-go、M0–M3b 自建里程碑、HP-FEM 探索與負面結果)凍結保留在
> `research/hpfem-solver-v1` 分支與 `Research/REALTIME_MILLION_DOF_RESEARCH.md` /
> `Research/WS_B_solver/HPFEM_RESEARCH_NOTES.md`(不在 main)。本檔是 main-facing 的提煉。

## 這是什麼 / 為何 supernodal

R 線目標:逼近「百萬 DOF / 1e-9 / 即時」結構求解的單機可達邊界。早期路線是 HP-FEM
迭代 solver(matrix-free PCG + parallel precond + banded coarse + **seeded** recycling),
對遊戲式低維載重可達每幀 ~15x 快於重用 LDLT。但它**依賴 seed**:載重離開 seed 子空間即
慘敗(out-of-subspace 40–700x),且與 direct 自我競爭。判定 niche 太窄,退役。

關鍵洞察(使用者):`reused sparse-LDLT back-substitution` 本就比一次 PCG solve 便宜
~17x,迭代法在此規模跨不過。正解不是繞過 factor,而是**正面壓低 factor**:Eigen
`SimplicialLDLT`(simplicial、無 supernode/BLAS3)是「假牆」——把 factor 組織成 dense
supernode panel 走 BLAS3 即可快一個量級。go/no-go 實測證實:混合建築 K_ff(frame +
MITC4 樓板),CHOLMOD supernodal 對 `SimplicialLDLT` 在 17k–32k DOF factor **27–45x** 快,
factor scaling 指數從 3.03 降到 1.56。**factor 假牆是真假牆。**

→ 結論:**「廢 HP 雙車道、統一單一 direct」**。HP 的 factor-bypass 理由消失,中小建築規模
direct 全面勝;百萬 + 即時 + 1e-9 是單機**物理牆**(記憶體 + back-substitution,非算法)。

## 能力與 API(opt-in,LDLT 永 default + fallback)

公開 API 只露 POD/std(零 Eigen 洩漏,走 `PreparedSystem` PIMPL),與既有 solver 同合約:

- `solveLoadSupernodal(prepared, model, SnSolveOptions)` — stateless 一次解。RHS 組裝、
  prescribed reduction、scatter、reactions(`K·u − F`)、element recovery **逐字複用
  `solveLoad`**,唯一差異是把 `S.ldlt.solve(Ff)` 換成自建 supernodal factor+solve。
  `enabled=false` → drop-in 等於 `solveLoad`(bit-exact)。
- `SnSession`(PIMPL,move-only)— **factor-once + solve-many**:ctor 內 factor 一次,
  `solveFrame` 重用該 factor。大模型 factor 一次攤銷、每幀只 back-substitute = 遊戲每幀主場。

**紅線(誠實)**:`SimplicialLDLT` 是 default `solve()`(完全不動)與所有 fallback 的把關者。
supernodal **顯式 opt-in** 才走;遇 `!fac.spd`(奇異/欠約束)→ 落回 LDLT pivot guard 報
singular(非 NaN/垃圾)。shell/非 frame 或 disabled → LDLT drop-in。

## 自建 supernodal(`sn_chol.h`,vendor 進 `Private/`)

無新二進位依賴鏈進核心邏輯,只依賴 METIS + CBLAS(OpenBLAS):

- **ordering + symbolic**:`METIS_NodeND` nested dissection + elimination tree + symbolic
  factorization(fill 與 Eigen 精確一致)。
- **numeric**:supernode dense panel、col-major left-looking;Schur complement 走
  `cblas_dgemm`、對角 block `LAPACKE_dpotrf`、off-diagonal `cblas_dtrsm`;solve 走
  `dtrsv/dgemv`。對 CHOLMOD oracle 正確(`vsCHOLMOD` 1e-12 ~ 1e-13)。
- **並行**(`factorizeSuperParallel`):etree level-set + 持久 thread pool(純 C++17
  `<thread>/<atomic>`,無 OpenMP)。**混合並行**:寬葉 level → supernode 並行 + 單緒 BLAS;
  窄根 level → 主緒 + 多緒 panel BLAS(打開 etree 根部 Amdahl)。序列版保留作 bit-exact
  交叉 oracle(階段一全鎖單緒 → `bitdiff=0`,證無 race)。
- **執行緒數調優**:dense panel factor 是 memory-bandwidth bound,最佳 `nt ≈ √nf/20`
  **遠低於核數**(過度並行退化、HT 災難);預設 `recommendedThreads(n)`,`--threads` 覆蓋。

**對標(8940HX 16C/32T,混合建築)**:並行自建 vs MKL-backed CHOLMOD **1.15–1.21x**
(17k `nt=8` 達 0.94–1.03x,基本打平工業庫);自建 BLAS3 supernodal 用 **OpenBLAS(BSD)**,
避開 CHOLMOD supernodal 模組的 GPL。意義:R 線「統一單一 direct」效能拼圖補齊。

## 可達邊界(誠實 — 單機物理牆,非算法)

go/no-go 三重約束(百萬 + 即時 + 1e-9)外推(numpy log-log,混合建築):

| 判準 | 結果 |
|---|---|
| factor ≥10x(假牆擊穿) | ✓✓✓(中小規模 27–45x vs `SimplicialLDLT`) |
| factor scaling 指數 ≤1.7 | ✓(~1.56) |
| back-substitution ≤100ms / 幀 | ✗(百萬外推 ~1s) |
| 記憶體 fit 32GB | ✗(百萬 peak 外推 ~117GB;此機實測上限 ~190k) |
| 相對誤差 ≤1e-9 | ◐(≤~113k OK;191k 1.6e-9,受高 cond 限) |

**可達邊界(32GB + 混合建築)**:每幀即時(back-sub ≤100ms)~**150k** DOF;互動(~1s)
~**390k**(peak 限);factor 百萬 ~80s 可行。百萬 + 即時 + 1e-9 同時達成是**單機物理牆**
(記憶體 + back-substitution 頻寬),非演算法問題。後續候選見 research handoff
(extended-precision residual 破 cond 底限、記憶體實測)。

## 驗證(五腿 gate)

- **standalone**:**F55**(stateless `solveLoadSupernodal` vs LDLT:frame / SS-UDL /
  settlement / release / MITC4 shell rel<1e-10、disabled bit-exact rel<1e-12、mechanism→
  fallback singular)、**F56**(`SnSession` 重用 factor 3 幀各 vs LDLT rel<1e-10、disabled
  drop-in)。
- **UE**:`FrameCore.Solver.Supernodal` + `FrameCore.SnSession`(`$ExpectedUeTests` 50→52)。
- **正確性 gate 用 `vsLDLᵀ`(及研究中 `vsCHOLMOD`)而非 residual**:混合建築高 cond
  (shell drilling + 大跨)使固定精度 residual 底限 ~cond·eps,iterative refinement 不收斂,
  **CHOLMOD 在同 K 一樣** → 對獨立 direct oracle 的相對誤差才是誠實判準。
- audit 104 / CLI / OpenSees 不受影響(supernodal 是 opt-in lane,不碰 default 路徑)。

## 依賴與整合邊界(誠實)

- **conda 依賴**:standalone `build.bat` 與 UE `FrameCore.Build.cs` 連結 conda env
  `framecore-direct` 的 OpenBLAS + METIS。**standalone gate leg 不再自包含**,需此 env
  (`SUPERNODAL_CONDA` 可覆蓋路徑)。
- **dual-build 守門**:`FRAMECORE_SUPERNODAL`(standalone=1)。supernodal body 在
  `FrameSnChol.h` choke point 後;UE 經 `FrameCoreModule.cpp` `StartupModule` 預載
  `openblas.dll`(解 delay-load search,否則首次 cblas faults)。
- **ABI**:`dumpbin /dependents openblas.dll` 證它只依賴 VCRUNTIME140 + UCRT(MSVC-clean,
  非 MinGW;import lib `!<arch>` 格式不代表 MinGW)→ conda OpenBLAS 直接進 UE。
- threaded reduction 是 tolerance-grade(非 bit-identical serial);F55/F56 用 tolerance
  gate 而非 default 路徑的 bit-exact oracle。

## 退役的 HP-FEM 雙車道(研究紀錄)

HP-FEM 迭代 lane(`HpSolver`/`HpSession`、matrix-free PCG、Galerkin seeded projection、
parallel element apply、banded coarse)已從生產樹移除。它在遊戲式低維載重下的 seeded 加速
(每幀 ~15x)是真實的研究成果,但 niche 太窄(out-of-subspace fallback、雙車道自我競爭),
**統一單一 direct 後不再需要**。完整實作、benchmark 與負面結果(spectral deflation /
sparse-finer-coarse / symApply 皆中性或負面)保留在 `research/hpfem-solver-v1` 分支與
`Research/WS_B_solver/HPFEM_RESEARCH_NOTES.md`,作為「曾探索過、誠實判定退役」的紀錄。
