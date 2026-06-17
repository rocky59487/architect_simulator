# v3 研究交接 — 曲面線 ✅ 完成 / 記憶體線 待釐清痛點

> 給新對話的自包含起手交接。**v3 曲面線三項全部完成**(本地 commit 未 push,branch
> `feature/shell-geometric-stiffness` 從 main `8fda27d` 開出):①K_σ `c317043` + ②殼 CR `bb82b04` +
> ③warped `e3b66ee`。**v3 記憶體線仍待釐清痛點未動工**。本檔基於本輪實作 + 三輪五腿 gate 實測證據。

## 🎯 當前狀態(2026-06-16)

| 線 | 項目 | 狀態 | commit |
|---|---|---|---|
| 曲面① | 殼幾何剛度 K_σ(線性屈曲) | ✅ 五腿綠 | `c317043` |
| 曲面② | 殼 co-rotational 大變形(EICR,階段1) | ✅ 五腿綠 | `bb82b04` |
| 曲面③ | warped quad 處理(可用性核心 + 誠實精度標) | ✅ 五腿綠 | `e3b66ee` |
| 記憶體 | 痛點釐清 / 方向選擇 | ⏸ 待使用者授權 | — |

**五腿 gate**:standalone F1–F61 / UE 55 / OpenSees / audit 104 / CLI(本地 commit 未 push)。

---

## ★ 動工中的關鍵發現(plan 假設被實測推翻,改變了 ③warped 的交付定義)

原 plan 對 ③warped 假設「best-fit 平面投影 + Sabir-Lock 修正改善 warped 精度」。**實作 + F61 實測推翻**:

- 對常見 warped quad(平行四邊形 + 中心節點抬升),**Newell 平均法線恰好等於對角叉積法線**(代數可證) →
  best-fit 只是把投影原點從 P0 移到 centroid,**剛度逐位元不變、精度零改善**(F61 實測:warp=3% 時 P0 與
  best-fit 的 Nxx 誤差都是 9.006e-4,改善 **1.00x**)。
- 真正的 MacNeal/Sabir 修正耦合「節點旋轉↔面內位移」,但這對純膜場(旋轉固定)無效,需旋轉自由的殼板彎曲
  場景驗證,且可能與 drilling/MITC4 交互破壞既有 gate → **研究級難題,改善幅度不確定**,留作未來。

**③warped 改為「務實完成」**:核心交付 = `SolveOptions::warpTolerance` 放寬 `validate()` 對非共面 quad
的硬拒絕(自由曲面網格能 solve 就是核心需求),輔以 best-fit 投影 + 角點翹曲記錄 `warp_[k]`,並誠實標
「**精度靠網格加密**」(warp 加 O(warp²) 有界誤差,F61c 實測 warp 4%→2%→1% → Nxx err 1.6e-3→4e-4→1e-4
乾淨收斂 → 加密就達精度,非每元素魔法修正)。完整 MacNeal/Sabir 留未來。

---

## 證據 A — 曲面②殼 CR EICR 大變形(2026-06-16, `bb82b04`)

**目標**:解鎖 `CorotationalAnalysis.cpp:215` 殼 reject。對 Zaha 薄殼大變形/穩定性最致命的缺口直接補齊。

**做法**:Element-Independent Co-Rotational(Felippa-Haugen),全部在 `CorotationalAnalysis.cpp` 匿名 ns
(無新 .cpp 免動 build.bat)。`CrShell24D` 類比梁的 `CrBeam3D`、`crComputeShell24D` 對每 facet:
1. 當前 frame `R_cr`(rows = 局部軸,從當前 4 角點建,同 `MITC4ShellElement::prepare()` 構造)。
2. **natural deformation 扣剛體**:`d_k = R_cr·(x_k − x_c) − R0·(X0_k − X0_c)`(centroid 扣平移、
   R_cr/R0 扣轉動)、`θ_k = logSO3(R_cr · Rnode_k · R0ᵀ)`。
3. `f_local = kl0 · u_local` → `fe = blkdiag(R_cr)ᵀ · f_local`;切線用 driver FD 一致切線。

**為何旋轉不變**:純剛體運動下 `R_cr = R0 · R_rigᵀ` → `d_k = θ_k = 0` → `fe = 0`(CR 定義性質)。
**這是本輪實作最高風險點**,實測通過 → formulation 正確。

**Oracle 數據**(三條獨立 oracle):

| Oracle | 結果 | 容差 |
|---|---|---|
| F58a 小位移殼 CR == 線性 solve | rel **3.8e-11** | 1e-3 |
| F58b 任意軸 SO(3) 旋轉不變(w/L=0.69 真大變形) | rel **2.6e-14** | 1e-9 |
| F59 nu=0 殼條對 Mattiasson elastica 表(alpha 1/5/10) | rel **1e-4 ~ 2e-3** | 3e-3~5e-3 |

**外部 OpenSees oracle 評估**:實測 `openseespy` 的 `CorotShellMITC4` **不可用**(unknown element);
`ShellNLDKGQ` 可用但是不同 formulation(DKGQ Kirchhoff vs MITC4 Mindlin)→ Mattiasson elastica 解析
oracle **更強**,本階段不納入 OpenSees 殼 CR leg。

**借的 API**:`MITC4ShellElement::localKForAudit()`(已有,取 kl0)+ 新加的 `localFrameForAudit()`(取 R0)。

**留階段 2**(未做,誠實標):
- arc-length 殼後屈曲(snap-through;driver 已有 arc-length 路徑,殼接入需測試)
- 完整解析切線(本輪 FD 一致切線足夠,解析 spin/moment 項只影響收斂速度)
- **CR 殼力 recover**(shellForces 目前 id-tag + 零填,線性 recover 會把大轉動誤判為應變 → 寧零不錯)
- 殼壓力 follower load(現用初始構形等效)
- 大應變(EICR 假設小應變,kl0 保持有效線性元素)

---

## 證據 B — 曲面③ warped quad(2026-06-16, `e3b66ee`)

**目標**:自由曲面建築分網必然產生 warped quad,但 `FrameModel.cpp:120` 在 `maxWarp > 1e-6·maxEdge`
**硬拒絕** → Gaudí/Zaha 網格根本進不了 solver。

**做法**:
- `SolveOptions::warpTolerance`(預設 1e-6 = 今天 strict)放寬 validate;`useWarpingCorrection` opt-in
  best-fit 投影(Newell 法線 + centroid origin)+ 記錄 `warp_[k]`。
- `validate(std::string& why, real warpTol = 1e-6)` 加預設參數(呼叫處不變,FrameSolver / Corotational
  透傳 `opts.warpTolerance` / `opts.solve.warpTolerance`)。

**F61 Oracle**:

| 分項 | 結果 |
|---|---|
| F61a warped patch 可 solve + 誤差有界(warp 3%, Nxx rel<10%) | ✅ warpTolerance 放寬有效;P0 與 best-fit 都 9.0e-4(best-fit 對規則 warped 零改善) |
| F61b flat patch 兩路徑都精確(預設不破) | ✅ rel < 1e-10 |
| F61c warp 誤差隨 warp 減小(加密就收斂) | warp 4%→2%→1% Nxx err **1.6e-3 → 4e-4 → 1e-4**(O(warp²) 收斂) |

**留未來**(誠實標):
- **完整 MacNeal/Sabir 旋轉耦合修正**:對純膜無效需旋轉自由場景驗證,且可能破 drilling/flat-patch gate,
  改善幅度不確定 → 未對齊路線前不做。
- 強烈翹曲(>5%)精度:Sabir 是小翹曲假設;主要靠加密。

**MITC4 仍是 flat facet**(O(1/N²) faceting 不變)。warped 網格達精度的方法 = **加密**(每元素 warp 隨網格
加密自動減小 → F61c 數據支撐),非單元素魔法修正。

---

## 證據 C — 曲面① K_σ 殼線性屈曲(`c317043`,2026-06-16)

**目標**:`BucklingAnalysis.h:26` 註明「Beam-column geometric stiffness only」→ 含殼模型 λ_cr 無意義。

**做法**:覆寫 `MITC4ShellElement::assembleGeometric` 出薄殼 w-only 應力剛化 `k_w = ∫ Gᵂᵀ S Gᵂ dA`,膜應力
張量 `S = t · Dm · Bm · dm` **每 Gauss point** 從 `prestress.u` 重算(全張量含 Nxy 剪/雙軸,vs 梁純壓
軸力)。`IElement::assembleGeometric` 簽名 `memberAxial` → `const SolveResult& prestress`(梁讀
`memberForces[e].endI.N`=舊值逐位元、殼讀膜場);opt-in `SolveOptions::shellGeometricStiffness`
(預設 off → 屈曲/P-Delta 逐位元同 beam-only)。

**F57 Oracle**:SS 方板 `N_cr = 4π²D/a²`(Timoshenko),n=20 rel **2.8%**(O(1/N²))、軸不變 **8.5e-13**、
sparse==dense **4.3e-12**、opt-in-OFF singular(證 flag 在做工)。

**誠實邊界**:F57 驗 **facet 級平板** K_σ 對解析解;**曲殼屈曲** = facet K_σ + flat-facet 逼近曲面,
無曲殼第三方 benchmark(留未來);w-only(in-plane 二階刻意排除);線性特徵值屈曲(後屈曲屬曲面②殼 co-rot)。

---

## 殘留的 v3 記憶體線(本輪未動,保留 plan)

**關鍵修正(沿用)**:純殼記憶體**不是**瓶頸 —— 32GB 機可放 **~3.9M DOF 純殼**(2D 流形 supernodal peak 指數
~1.03 近線性);真正記憶體瓶頸在:
- (a) **混合 frame+shell 建築**(frame 非局部耦合 → peak 指數 1.36,32GB 只能 ~360k DOF)
- (b) **超大規模 >數百萬**
- (c) **高 cond 需 extended-precision**

**v3 第一步應與使用者釐清「記憶體使用量」具體痛點**:哪個模型、哪個規模、實測還是觀察?

**候選方向**(視痛點):
- **混合建築 reordering / frame 耦合**:nested-dissection 對混合拓撲優化、或 frame/shell 分離求解
- **rank-structured direct(BLR/HSS/H-matrix)**:fill 壓近 O(n)、仍 direct 無 seed;對 >百萬 是真出路,
  1e-9 精度有張力(低秩逼高 rank)
- **out-of-core / 駐留優化**:supernodal factor 駐留 vs peak 的差(Phase E:百萬駐留 ~19GB vs peak ~117GB
  是混合;純殼低很多)
- **extended-precision residual**:已驗 Neumaier 補償求和破 cond 底限(Phase E),用於高 cond 大模型

詳見 Phase E 數據:`Research/REALTIME_MILLION_DOF_RESEARCH.md`。

---

## 純殼即時/記憶體實測(沿用,Phase E 證據)

工具:`exp_sn_chol.exe --limit --shell --nx N`(純殼 clamped 方板,全 MITC4)+ `SolveOptions::skipLdltFactor`
(research-only flag,繞 SimplicialLDLT)。資料 `Research/out/sn_limit_shell_sweep.txt`。

| nf | factorMs | per-frame backsubMs | peakMB | res0 |
|---|---|---|---|---|
| 37k | 92 | 5.2 | 236 | 1.0e-10 |
| 85k | 237 | 16.8 | 551 | 1.6e-10 |
| 152k | 474 | 31.7 | 962 | 2.3e-10 |
| 238k | 765 | 59.4 | 1541 | 3.1e-10 |
| 343k | 1024 | 77.2 | 2297 | 4.0e-10 |

**擬合指數(純殼)**:backsub **~1.1–1.2**、factor **~1.05**、peak **~1.03**(混合 1.65/1.36/—)。

**純殼即時規模**(SnSession factor-once + 每幀 backsub):

| 幀率 | DOF | 節點 |
|---|---|---|
| 60fps(16ms) | ~85k | ~14k |
| 30fps(33ms) | ~155k(實測) | ~26k |
| 互動(100ms) | ~425k | ~71k |
| 1s 後台 | ~2.7M | — |

→ 即時是限制,**非記憶體**。caveat:clamped 平板代表 2D 流形;真實曲面 warp 可能略升 fill,實測止 343k,
百萬+是穩定 ~1.1 指數外推。

---

## 工具 / 環境 / 踩雷

### 接續本分支
**branch `feature/shell-geometric-stiffness`** 從 main `8fda27d` 開出,3 commit 領先 main 本地未 push:
```
e3b66ee feat(FrameCore): warped shell quads -- relax validate + best-fit projection
bb82b04 feat(FrameCore): opt-in EICR shell co-rotational large displacement
c317043 feat(FrameCore): opt-in shell geometric stiffness (K_sigma)
```

### 各階段 spec(本輪新增)
- `docs/specs/shell_geometric_stiffness.md`(曲面①)
- `docs/specs/shell_corotational.md`(曲面②)
- `docs/specs/shell_warping.md`(曲面③,含 plan 推翻發現)

### 純殼 benchmark(記憶體線用)
- `Research/WS_B_solver/build_supernodal.bat {compare|sn}`(改 FrameSolver/SolveOptions 這類 header 後須刪
  `Research/obj_sn_core/*.obj` 重編全部 core,否則 SolveOptions struct ABI 不一致)
- `exp_sn_chol.exe --limit --shell --nx N`(一 process/規模,peak 乾淨);跑前 PATH 加 conda
  `framecore-direct\Library\bin`
- `skipLdltFactor`(`SolveOptions.h`,research-only):繞過 `assembleAndFactor` 的 SimplicialLDLT
  (Phase E 130k+ 卡死的牆);supernodal 自己 factor K_ff。**只在 research benchmark 用**

### 踩雷(durable,本輪實證或新增)
- **接續未提交工作**:**先 `git diff` 全讀核對**(Edit 假落地;本輪初接續曲面①時踩到)
- **UE build 用 PowerShell `&` call operator**(git-bash 下 `cmd //c '"…Build.bat" -project="…"'` 巢狀引號
  失敗報「不是命令」,本輪實證)
- **CLI/OpenSees 兩腿自呼 `build_cli.bat` 重編 frame_cli**(故 frame_cli.exe 舊時間戳無妨)
- **OpenSees `CorotShellMITC4` 不可用**(實測 openseespy unknown element);`ShellNLDKGQ` 可用但 DKGQ vs
  MITC4 不同 formulation → 殼大變形用 Mattiasson 解析 oracle 更強
- **plan 假設可能被實測推翻**:③warped 的 best-fit 對規則 warped quad 零改善(本輪實測);**改動 hook 後
  先快速跑一個診斷 fixture 量改善幅度**,再定 oracle 判據
- **OOM**:純殼 peak 低(343k 才 2.3GB),可安全跑到百萬+;混合建築 peak 高(100k 4.9GB),>130k 小心

### 一鍵驗證
```
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
```
期 `GATE: PASS`(standalone F1-F61 / UE 55 / OpenSees / audit 104 / CLI)。改 FrameCore → 先用 PowerShell
`& "...\Build.bat" ArchSimEditor Win64 Development -project="...\ArchSim.uproject" -waitmutex` 重編 UE。

---

## 下一步(待使用者授權)

1. **v3 記憶體線**(本檔上半部):先釐清痛點再選方向(混合 reordering / rank-structured / out-of-core /
   extended-precision)
2. **殼 CR 階段 2**:arc-length 殼後屈曲(driver 已有路徑)+ CR 殼力 recover + 解析切線
3. **完整 MacNeal/Sabir warped 修正**:對曲面③進一步真改善(研究級,風險高)
4. **S11 MITC9i 高階殼**(殿後,9 處引擎修改先決)
5. **C6–C8 可視化資料線**(沿桿 BMD/SFD、利用率場、贅餘度)
6. **UE5 視覺層**(吃 CollapseStep.u 回放、FragmentCluster→Chaos、D/C 熱圖)

每階段動工前先補滿 spec、完成即停待授權。權威現況:`E:\project\CLAUDE.md` + memory
`frame-engine-next-plan` + 各 PROGRESS_*.md / specs/*.md。
