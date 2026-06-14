# 即時百萬 DOF 結構求解 — 新研究線啟動 + 交接(R-line)

> **開新視窗用這份無縫接續。** 前一個對話太長,此檔自包含:交接前情 + 新目標 + 三方法 + 可行性評估清單 + 鐵則 + 第一步。語言一律中文。

## 一句話目標
**即時(real-time)求解百萬(~10⁶)DOF 線彈性結構,精度 `‖u_HP − u_exact‖/‖u_exact‖ ≤ 1e-9`。**
「即時」初步定義為互動級(每次重解 ≤ ~100ms,理想遊戲幀 ~16–33ms);百萬 DOF 是硬目標規模。這是 ambitious 目標 —— **第一步是可行性評估,不是直接造輪子**。

## 為什麼開新線(前情 + 為何舊的太窄)
前一輪(WS_B,branch `research/hpfem-solver-v1`,HEAD `a327c6b`,**未發布**)完成 opt-in **seeded HpSession**(A2c serial / A2a parallel / A2b banded coarse + release-prep 重構/優化 + 3-agent 超完整審核 + benchmark)。五腿全綠。
**誠實 benchmark 結論(關鍵)**:端到端只 **~1.6x**(完整 SolveResult)/ **~4.4–6.7x**(displacementsOnly,只填 u),out-of-subspace **慘敗 40–700x**。research 的「solve-only ~19x」是只比 solve 步驟,**端到端被 reactions+recover 稀釋**。勝場結構性地窄 —— 鎖在 *seeded 重複解 + 小-中問題 + 只要位移*。
**根本限制(已反覆驗證)**:小-中問題(<~25k DOF)LDLT factor + back-sub 都 ms 級;迭代法跨不過「reused LDLT backsub 比一次 PCG solve 便宜 ~17x」。**在這個框架/規模內沒有「全局更優」出路。**
**使用者判斷(2026-06-14)**:不是失敗,但太窄,需換戰場、開新研究。

## 真正的戰場:LDLT factor wall
LDLT 分解是超線性爆炸的(WS_B NOTES 實測:18.7k→48k DOF 即 1.7s→**20.7s**,記憶體同步爆)。**到百萬 DOF,LDLT 分解的時間/記憶體完全不可行**,而 matrix-free 是 O(n) 記憶體 + 可並行/GPU。那裡 HP 不是「快幾倍」,而是「LDLT 根本跑不動」= **全局更優**。
**唯一前提**:PCG 迭代數必須隨規模不爆(否則百萬 DOF 上千 iter × 每 iter apply = 不可能即時)→ 需要 **scalable preconditioner**。這正是這個窄勝場一直缺的那一塊,也是新研究的核心。

## 三方法(協同達成即時百萬 DOF)
1. **超大規模 + AMG scalable precond(核心)**:AMG-for-elasticity —— rigid-body near-null-space(6 個剛體模態)入 prolongator、Galerkin `R·K·P` 建粗層、逐層 smoother(block-Jacobi/Chebyshev)+ transfer,使 PCG iter ~ O(1) 不隨規模爆。WS_B 既有 banded coarse(樓層聚合)是 2-level 雛形;NOTES 的 V-cycle「暫緩 No-go」是**針對 seeded headline**(每幀已 0-iter,V-cycle 無從加速)——**對百萬 DOF 首解,AMG 是必須,舊評估不適用新場景**。
2. **GPU matrix-free**:element apply + PCG 上 GPU 大規模並行。百萬 DOF 的 apply 在 CPU 可能太慢,GPU 是達「即時」的關鍵槓桿。**需評估與純 C++17/Eigen 雙車道的整合張力**(GPU 後端 = 新依賴,與鐵則衝突,先評估)。
3. **重新框定問題**:確認百萬 DOF 即時的真實使用情境(精細網格大型結構、即時互動分析/變形預覽),據此界定 precond / 規模 / 精度 / 「即時」的精確需求,避免又解錯問題。

## 載體:先只做可行性評估 + 小原型(使用者明確要求)
**不投入大工程。** 先 go/no-go:用小原型 + 文獻 + 粗略 benchmark + 外推,評估「即時百萬 DOF + 1e-9」是否可達、哪個方法組合、瓶頸在哪。**避免又走進窄胡同**(上一輪教訓:先誠實評估端到端,別被 solve-only 數字誤導)。research-only lane(建議新 `Research/WS_D_million/`),不進五腿 gate、不改 default solve、LDLT 永為 oracle。

## 可行性評估要回答的關鍵問題(go/no-go 依據)
1. **LDLT 在百萬 DOF 真不可行?** 量 LDLT factor 在 ~10萬 / 50萬 / 100萬 DOF 的時間 + 記憶體(預期爆)→ 確立 HP 的*必要性*(賣點是「LDLT 做不到」,不是「比 LDLT 快」)。
2. **PCG iter vs 規模(無 AMG)**:Jacobi/banded-coarse PCG 的 iter 隨規模如何爆(條件數 ~ O(h⁻²))→ 量化 AMG 的必要性。
3. **AMG-for-elasticity 收斂率(成敗關鍵)**:小原型(3D 彈性,structured grid 起步)AMG 的 iter 數是否 ~ O(1) 不隨規模增長?達不到 → 即時百萬 DOF 不可行。
4. **即時可達性**:百萬 DOF,每 PCG iter 的 apply 成本(CPU matrix-free vs GPU)× 收斂 iter 數,能否 ≤ 100ms(互動)/ ~16–33ms(幀)?
5. **1e-9 精度的代價**:PCG `tol=1e-9`(相對殘差)在 AMG precond 下需多少 iter?是否與即時衝突(放寬到 1e-6 是否就夠精度目標)。
6. **GPU 的角色與代價**:apply 上 GPU 的加速比;與純 C++17/Eigen / 五腿 gate / FRAMECORE_UE dual-build 的整合張力與取捨。
7. **記憶體**:百萬 DOF 的 element data + AMG 階層 + PCG 向量,是否 fit(CPU RAM / GPU VRAM)。

## 現狀 / 路徑 / 可複用工具(交接)
- branch `research/hpfem-solver-v1`,HEAD `a327c6b`,五腿全綠,**未發布**(seeded HpSession 已是乾淨可用的 opt-in lane,但窄;不要再投入擴它)。
- 可複用:
  - `Plugins/FrameSolver/Standalone/hp_bench.cpp` + `build_hp_bench.bat` — per-frame benchmark harness 範本(交錯重複取 median)。
  - `Research/WS_B_solver/exp_parallel_pcg.cpp` — matrix-free element apply / ThreadApplyPool / **banded block-tridiagonal coarse**(2-level AMG 雛形)/ 既有 deflation·coarse 實驗(多為負面,看 NOTES)。
  - `Plugins/.../Private/HpSession.cpp` — production seeded session(ElementBlock12 / CoarseOperator / RecycleBasis)。
  - 大問題 fixture:`Standalone/hp_bench.cpp` 的可擴展 cantilever;真實大網格需自建 3D structured grid(每節點 6 DOF,nf=6·nodes)。
- 權威背景:`Research/WS_B_solver/HPFEM_RESEARCH_NOTES.md`(**Session 4–7**,含 V-cycle 行前評估、根本限制、scale story、factor-bypass 17→40x)。
- 記憶:`hpfem-solver-research.md`(durable 教訓 ①–⑨:UE build 引號、PowerShell 正斜線、bash cwd 在 `/e/project` 用 `git -C`、threaded reduction tolerance 非 bit-identical、誠實報端到端等)。

## 鐵則(沿用)
research-only(不進五腿 gate、不改 default `solve`/`solveLoad`/`solveLoadHP`);**LDLT 永為 oracle / 正確性對標**(百萬 DOF 若 LDLT 跑不動 → 用較小規模 LDLT 對標 + 解析解 / 真實殘差 `‖Ku−f‖`);**誠實驗證:報端到端非 solve-only、報實測非宣稱、niche 邊界寫清楚**(上一輪最大教訓);過 gate 才 commit;顯式 `git add` 個別檔(絕不 `-A`);不碰 `.gitignore`/`ArchSim.uproject`/`Plugins/LevelSim/`/`Research/WS_C`/`Research/WS_N`/build 產物。**GPU/新依賴若引入,先在可行性評估明確列出鐵則張力與取捨,不擅自破壞純 C++17/Eigen 雙車道。**

## 建議第一步
開 `Research/WS_D_million/`(或先暫存原型),寫一個最小可行性原型:3D 彈性 structured grid(可調規模到 ~10萬 DOF 起,外推百萬),量:
(a) LDLT factor 時間 + 記憶體 vs 規模 → 找撞牆點;
(b) Jacobi / 既有 banded-coarse PCG 的 iter vs 規模 → AMG 必要性;
(c) 每 iter apply 成本 → 外推百萬 DOF 的即時可達性。
據此寫 **go/no-go 報告 + AMG/GPU 必要性判斷**。**先評估,別直接造大輪子。** 用 Workflow / 平行 agent + 對抗式查核(三方重算)維持誠實。
