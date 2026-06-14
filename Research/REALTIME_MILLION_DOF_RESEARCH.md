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
