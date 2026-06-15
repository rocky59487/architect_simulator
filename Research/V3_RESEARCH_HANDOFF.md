# v3 研究交接 — 記憶體使用量 + 曲面問題

> 給新對話的自包含起手交接。v2.0.0 已發布(`8fda27d`,tag `v2.0.0`,GitHub release)。
> v3 目標(使用者定):**(1) 記憶體使用量、(2) 曲面問題**——針對實際自由曲面建築(Gaudí 受壓殼 /
> Zaha Hadid 高度雙曲薄殼)。本檔基於兩份實測證據:純殼即時/記憶體 sweep + 4-agent 自由曲面能力審查。

## ★ 動工前最該知道的一個修正

v3 命名為「記憶體 + 曲面」,但實測**推翻了「純殼記憶體是瓶頸」的假設**:

- **純殼模型的記憶體幾乎不是問題**。純殼 = 2D 流形嵌 3D,supernodal fill-in 近線性(peak 指數 **~1.03**),
  32GB 機可放 **~3.9M DOF** 純殼(百萬 DOF peak 僅 ~7GB)。對比混合建築(frame 非局部耦合 → peak 指數 1.36)
  32GB 只能 ~360k DOF。**純殼記憶體比混合好 ~10x**。
- 所以「記憶體問題」若指**純殼自由曲面建築**,其實已經很好;真正的記憶體瓶頸在 **(a) 混合 frame+shell
  建築(frame 耦合)、(b) 超大規模 >數百萬、(c) 高 cond 需 extended-precision**。**v3 第一步應與使用者
  釐清「記憶體使用量」具體痛點**(哪個模型、哪個規模、實測還是觀察?)。
- **曲面的真正瓶頸不是記憶體,是精度(flat-facet)+ 結構行為(殼無幾何非線性/屈曲)**。見證據 B。

---

## 證據 A — 純殼即時/記憶體實測(2026-06-15,research,8940HX)

工具:`exp_sn_chol.exe --limit --shell --nx N`(純殼 clamped 方板,全 MITC4,`makeShellGrid`)
+ `SolveOptions::skipLdltFactor`(research-only flag,跳過 `assembleAndFactor` 的 SimplicialLDLT,讓
supernodal 自己 factor → 繞過 Phase E 卡住的 SimplicialLDLT 牆)。數據 `Research/out/sn_limit_shell_sweep.txt`。

| nf | factorMs | per-frame backsubMs | peakMB | res0 |
|---|---|---|---|---|
| 37k | 92 | 5.2 | 236 | 1.0e-10 |
| 85k | 237 | 16.8 | 551 | 1.6e-10 |
| 152k | 474 | 31.7 | 962 | 2.3e-10 |
| 238k | 765 | 59.4 | 1541 | 3.1e-10 |
| 343k | 1024 | 77.2 | 2297 | 4.0e-10 |

**擬合指數(純殼,遠低於混合建築)**:backsub **~1.1–1.2**(混合 1.65)、factor **~1.05**、peak **~1.03**。
**純殼即時規模(SnSession factor-once + 每幀 backsub,玩家風/活載重)**:

| 幀率 | DOF | 節點 | 對應建築 |
|---|---|---|---|
| 60fps(16ms) | ~85k | ~14k | 中型自由曲面展館 |
| 30fps(33ms) | ~155k(實測) | ~26k | 大一點的展館 |
| 互動(100ms) | ~425k | ~71k | 大型細網格建築 |
| 1s 後台 | ~2.7M | — | 即時是限制,**非記憶體** |

→ **比之前用混合指數的外推樂觀 ~2x**(60fps 從 ~7k 節點 → ~14k;互動從 ~21k → ~71k 節點)。
純殼 res0 ~1e-10(良態,不卡混合建築的 ~1.4e-9 cond 底限)。**caveat**:clamped 平板,代表 2D 流形稀疏結構;
真實曲面的曲率/warp 可能略升 fill;實測止 343k,百萬+是穩定 ~1.1 指數外推。

---

## 證據 B — 自由曲面能力審查(4-agent Workflow + 第一手碼讀)

審查 main `8fda27d` 對 Gaudí/Zaha 的能力。**結論:曲面的瓶頸是精度 + 結構行為,不是速度/記憶體。**

### B1. 殼幾何非線性 / 屈曲:完全缺失(最致命,Zaha 薄殼直接破功)
| 缺口 | 證據(file:line) |
|---|---|
| 殼大位移/co-rotational | `CorotationalAnalysis.cpp:215-216` 遇殼 `reject("...beam-column only; model contains shells")` |
| 殼幾何勁度 K_σ | `MITC4ShellElement` 不覆寫 `IElement::assembleGeometric`(空函式) |
| 殼線性屈曲 | `BucklingAnalysis.h:26`「Beam-column geometric stiffness only (shell ... future addition)」→ 純殼 λ_cr **無意義** |
| 殼 snap-through/P-Delta/塑鉸 | arc-length 入口 reject 殼;塑鉸只在梁(殼只能脆性移除) |

→ **Zaha 受壓自由薄殼的屈曲穩定性 + 後屈曲 + 大變形完全算不了**。引擎目前只能對殼做線彈性小位移。
Gaudí 受壓 funicular(線性小變形)夠用。

### B2. MITC4 是 flat-facet,曲面靠網格逼近(精度可控但有真實成本)
- 元素是 `flat-shell facet`(`MITC4ShellElement.cpp:11`);**warped quad(4 節點不共面)投影到平均平面,
  無 warping correction、無誤差警告**(`cpp:500/505/515` 平面投影)。高度翹曲(>1e-6×邊長)被 `validate()` 拒絕
  → **自由曲面分網必須先把每個 quad 投影到 best-fit 平面**(硬前置約束)。
- benchmark 實測(agent 實跑 `shell_mitc4_deep_audit.py`):Scordelis-Lo(溫和圓柱)N=8 誤差 3.6%/N=24 0.83%;
  **pinched cylinder(彎曲主導)N=8 誤差 27%/N=32 才 1.2%**。收斂 O(1/N²)。
- **Gaudí**(曲率溫和、受壓):N≈20 網格可達 1–3%,可用。**Zaha**(高雙曲、扭曲):粗網格誤差 20%+,
  需每曲率特徵 10–20 元素,且 warped 投影誤差無警告 → 難知是否收斂。

### B3. 非正交/厚薄(可承受)
- 厚薄:殼厚度每元素獨立;MITC4 assumed shear 在 t/L 1/5~1/100 無 locking。可承受。
- 非正交:仿射(平行四邊形)過 strong patch test 機器精度;**一般四邊形只過 weak patch test(O(h) 殘差,
  靠加密)**;**無 aspect ratio 守門、無極端內角警告**。

---

## v3 研究線

### 曲面線(真正瓶頸,Zaha 薄殼正確性的先決)
按優先序:
1. **殼幾何勁度矩陣 K_σ_shell**(殼線性屈曲)——覆寫 `MITC4ShellElement::assembleGeometric`,接進
   `BucklingAnalysis`。讓含殼模型的屈曲因子有意義。**這是 Zaha 薄殼穩定性的最低門檻**。
2. **殼 co-rotational**(殼大變形/後屈曲)——MITC4 facet 的有限轉動 formulation,接進 `runCorotational`
   (目前 reject 殼)。工程大(殼 CR facet + 一致切線)。
3. **warped quad / 曲面精度**——warping correction,或更高階殼(如 MITC9 二次殼,memory 提過 S11 殿後),
   或為 warped 投影誤差加顯式 inter-facet 法向跳躍警告(讓使用者知道網格夠不夠密)。

### 記憶體線(先釐清,再選方向)
- **先與使用者確認痛點**:純殼記憶體已很好(~3.9M DOF @ 32GB)。痛點是混合建築?超大規模?還是別的觀察?
- 候選方向(視痛點):
  - **混合建築 reordering / frame 耦合**:frame 的非局部耦合拉高 fill(指數 1.36 vs 純殼 1.03);
    nested-dissection 對混合拓撲的優化、或 frame/shell 分離求解。
  - **rank-structured direct(BLR/HSS/H-matrix)**:memory「第 0 層基石質疑」提過——fill 壓近 O(n)、
    仍 direct 無 seed。對超大規模(>百萬)是真出路,1e-9 精度有張力(低秩逼高 rank)。
  - **out-of-core / 駐留優化**:supernodal factor 駐留 vs peak 的差(Phase E:百萬駐留 ~19GB vs peak ~117GB
    是混合;純殼低很多)。
  - **extended-precision residual**(已驗 Neumaier 補償求和破 cond 底限,Phase E)——用於高 cond 大模型。

---

## 工具 / 環境 / 踩雷

- **純殼 benchmark**:`Research/WS_B_solver/build_supernodal.bat {compare|sn}`(改 FrameSolver/SolveOptions
  這類 header 後須刪 `Research/obj_sn_core/*.obj` 重編全部 core,否則 SolveOptions struct ABI 不一致);
  `exp_sn_chol.exe --limit --shell --nx N`(一 process/規模,peak 乾淨);跑前 PATH 加 conda
  `framecore-direct\Library\bin`。
- **skipLdltFactor**(`SolveOptions.h`,research-only):繞過 `assembleAndFactor` 的 SimplicialLDLT
  (`FrameSolver.cpp:88`,Phase E 130k+ 卡死的牆);supernodal 自己 factor K_ff。**只在 research benchmark 用**,
  跳過 mechanism detection(pivotMargin=0)。
- **OOM**:純殼 peak 低(343k 才 2.3GB),可安全跑到百萬+;混合建築 peak 高(100k 4.9GB),>130k 小心。
- **分支**:這些都在 `research/hpfem-solver-v1`(research-only,不進 main/gate)。main = v2.0.0 純淨。
- **背景跑大 sweep**:`run_in_background` + 讀 output;每規模一 process。
- 權威:Phase E 數據 `Research/REALTIME_MILLION_DOF_RESEARCH.md`;純殼 `Research/out/sn_limit_shell_sweep.txt`;
  自由曲面審查證據在本檔 B 節。
