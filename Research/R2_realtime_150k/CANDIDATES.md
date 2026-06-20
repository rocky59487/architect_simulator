# R2 Realtime 150K — Candidate Technology Evaluation

> **Lane**: research-only,**不進 main**,不被五腿 gate 跑。沿用 HpFEM research 模式 (untracked → 證明可行才 opt-in)。
>
> **Question**: 讓 ArchSim FrameCore 在 150K 自由度的混合建築模型達到**真正 60fps 實時** (16.67 ms/frame),目前 supernodal 達到的是「100ms 互動級」(THEORY 外推)。
>
> **Date**: 2026-06-21

## 1. 事實基線 (來自 audit subagent)

### 1.1 已實測點 (混合 frame+shell 建築)

| nf | factorMs | backsubMs | peakMiB | residual | 來源 |
|---|---|---|---|---|---|
| 17,160 | 217 (CHOLMOD) | 5.72 | — | 1.0e-10 | v3_memory_recon.md:62 |
| 31,824 | 514 (CHOLMOD) | 13.8 | 842 | 2.1e-10 | v3_memory_recon.md:62 |
| 64,260 | 1,100 (CHOLMOD) | 33.6 | 2,064 | 4.8e-10 | v3_memory_recon.md:62 |
| 113,400 | 2,625 (CHOLMOD) | **66.7** | 4,879 | 7.5e-10 | v3_memory_recon.md:62 |
| 191,400 | 6,070 (CHOLMOD) | **132.8** | 10,234 | 1.57e-9 | v3_memory_recon.md:62 |

### 1.2 已實測點 (純殼方板,自建 sn)

| nf | factorMs | backsubMs | peakMB | res |
|---|---|---|---|---|
| 85k | 237 | 16.8 | 551 | 1.6e-10 |
| 152k | 474 | 31.7 | 962 | 2.3e-10 |
| 238k | 765 | 59.4 | 1541 | 3.1e-10 |

### 1.3 重要結論

- **factor 是 once-then-reuse**:在 SnSession + same-topology dynamic 路徑,factor 一次後 solveFrame 反覆呼叫,**單一 factor 不在 60fps 預算內,但合理 (loading/event-driven re-factor)**
- **backsub 才是 60fps killer**:113k 混合建築 backsub **67 ms** = 60fps 預算 4×
- **150K 60fps 在混合建築未直接量測**,只在純殼 85K 達到 (16.8ms)
- **元素移除 (collapse 事件) re-factor**:6,070 ms @ 191k → 不可即時 (但可接受,模擬一次崩塌劇本不必每幀重做)
- **真正瓶頸排序**:backsub > re-factor > Newmark loop > 殼裝配 (殼 K_T 凍結重用) > D/C 篩查 (有 throttle)

## 2. 候選技術評估

評估維度 (使用者鐵則):
- ❌ **不堆奇怪技術** — 必須業界成熟 (paper/工業驗證)
- ❌ **不靠想像猜測** — 必須有 oracle 數字
- ❌ **不為部分功能犧牲主體** — 不可破現有 F1..F64 / UE 57 / OpenSees gate
- ✅ **先實驗再放到引擎** — Research lane prototype 通過才談 opt-in

### Candidate A — 混合精度 backsub + Iterative Refinement (FP32+FP64 IR)

**原理**:Cholesky factor FP32 儲存 + FP32 triangular solve + FP64 residual + IR 收斂。LAPACK xPOSRFS-style。

**Oracle 證據**:
- Higham, *Accuracy and Stability* §15.5: SPD + IR 在 κ(A) < 1/u_FP32 ≈ 8e6 內可收斂
- ICL UTK MAGMA mixed-precision SPD report: backsub 1.7-2.1× speedup on Intel CPU (no GPU)
- 本 SnSession 已 cache `Ap/Ai/Ax` (line 91-96) 給 IR 用 (CSC format)

**預期效益**:
- backsub 67ms (113k) → ~32-40ms — **仍未 60fps,但接近 30fps 預算**
- 150k 預期 backsub 80-100ms → **40-50ms with FP32 + IR(2 steps)**
- 不夠 60fps,但離得近;搭配 (B) 或 (C) 才達標

**風險**:
- 混合建築 condition number 高 (殼 drilling + 桿 release 混合可能讓 κ > 1e6) → IR 不收斂 fall back FP64
- 需 cache FP32 factor (2× 記憶體佔用,可接受)
- BLAS 對 FP32 SPD-trsv 已熟,實作可控

**工作量**:M (sn_chol.h FP32 path + IR loop + benchmark)

**判決**:✅ **首選**,風險最低、實作小、可在夜班 prototype

---

### Candidate B — Schur complement local update (Sherman-Morrison-Woodbury)

**原理**:元素移除 → K' = K + UCVᵀ (low-rank perturbation) → 用 cached factor + Woodbury identity 解出 u' 而不重 factor。

**Oracle 證據**:
- 已有 `S1_resolve_ladder.md` Tier1 Woodbury for **same-topology, support-flag-unchanged** 路徑
- DynamicCollapse 明示**改 support flags → 跳 Woodbury 直接 re-factor** (S2_dynamic_collapse.md)
- 文獻:Davis & Hager 2009, *Dynamic Supernodes in Sparse Cholesky Update/Downdate* — 對結構工程崩塌情境特別合適

**預期效益**:
- 元素移除 re-factor 6,070ms → low-rank update 預期 50-150ms (rank ~6 per element)
- 對 dynamic collapse 路徑 = **大躍進**
- 但對純線性 backsub-only loop **無加速** (那是 same-topology)

**風險**:
- 需擴 ReSolve ladder 邏輯到 collapse 路徑 — **可能破壞 S1 ↔ S2 介面**
- Davis & Hager 對 supernodal 形式的 update 需特殊資料結構 (cholmod_updown 是參考)
- 高 risk 動引擎主體 — **使用者明示「不為部分功能犧牲主體」**

**判決**:⚠️ **可行但高風險**;延後,先試 (A)

---

### Candidate C — GPU offload backsub (D3D12 ComputeShader / cuSPARSE)

**原理**:把 CSC factor 上傳 GPU,backsub 在 ComputeShader 跑。

**Oracle 證據**:
- cuSPARSE csrsv2 對 100k SPD backsub:CPU 100ms → GPU 5-10ms (RTX 30 系)
- 但傳輸 latency 0.5-1ms,且 sparse triangular solve 在 GPU **平行度差** (依賴鏈)
- 工業界 Algoryx / Project Chrono 真實系統:大規模 SPD 仍走 CPU 因 sparse triangular solve 序列性

**預期效益**:
- 理論 5-20×,實際對 sparse triangular 約 2-3×
- 跨 vendor (NVIDIA/AMD/Intel) ABI 不統一

**風險**:
- UE D3D12 ComputeShader 寫 sparse triangular solve = **巨型工程**,夜班不可做
- 跨硬體相容性 = **長期維護負擔**
- rocky59487 機器 GPU 規格未知 → 可能無 dedicated GPU

**判決**:❌ **不做**,違反「不堆奇怪技術」

---

### Candidate D — BLR (Block Low-Rank) supernodal Cholesky

**原理**:supernodal 的 dense panel 用 low-rank approximation 壓縮,factor 跟 backsub 都降 cost。

**Oracle 證據**:
- MUMPS 5.3 BLR @ 200k SPD: factor 1.5-2× speedup, backsub 1.2-1.5× (Amestoy et al. 2019)
- HSL_MA86 BLR: 類似數字
- 對 **stiffness matrix typical κ ≈ 1e5** 在 ε_BLR = 1e-8 仍 numeric stable

**預期效益**:
- backsub 67ms → 45-55ms — **仍未 60fps**
- factor 2625ms → 1500ms — 有幫助但非突破

**風險**:
- 實作量極大 (~3,000 行新程式碼)
- 需新 ABI / cache 結構
- 1.5× speedup 不夠「真正實時 60fps」突破
- 違反「不堆奇怪技術」如果沒有質的飛躍

**判決**:❌ **不做**,效益不夠突破性

---

### Candidate E — reorder / blocking / NUMA pinning 微調

**原理**:METIS quality 微調、supernode amalgamation 重新算、CCD thread affinity。

**Oracle 證據**:
- 自建 supernodal 已 nt=√nf/20 (PROGRESS_R_supernodal)
- AMD 7000 系 CCX 內 L3 共享,跨 CCX 同步成本高

**預期效益**:
- 5-15% — **無突破性**

**判決**:❌ **不做**,效益不夠夜班代價

---

### Candidate F — 多右側 batched backsub (multiple RHS pipelining)

**原理**:Newmark 模態空間每步 backsub 1 個 RHS,可改成把 N 步 RHS 累積後一次 batch solve。

**Oracle 證據**:
- 但 SnSession 用模態解耦 (DynamicCollapse.cpp:200-206),per-mode Newmark **本來就不需要 K backsub**
- Dynamic collapse 的 backsub 只在事件觸發後做,稀疏
- **靜力即時模式 (UE 互動拖載重點點) 才 per-frame backsub** — 這條才有 multi-RHS 機會
- 但每幀只有 1 個 RHS (user input),無 batch

**判決**:❌ **不適用**,場景不對

---

## 3. 結論與夜班行動

### 3.1 結論

| 候選 | 判決 | 理由 |
|---|---|---|
| A — 混合精度 + IR | ✅ **prototype** | 風險低、實作小、預期 1.7-2× backsub |
| B — Schur local update | ⚠️ 延後 | 高效益但動主體,需更謹慎設計 |
| C — GPU offload | ❌ | 巨型工程、相容性差 |
| D — BLR | ❌ | 1.5× 不突破 |
| E — reorder 微調 | ❌ | 5-15% 不突破 |
| F — multi-RHS batch | ❌ | 場景不對 |

### 3.2 重要前提:先補實測基線

**目前 100K 與 150K 規模混合建築 factor/backsub 時間是外推,不是實測。**

夜班第一個 micro-bench:在 Research/R2_realtime_150k/ 跑 90k / 120k / 150k 真實混合建築 fixture,記錄:
- SnSolver factor / backsub wall clock
- LDLT baseline
- residual
- peak RAM

**如果 150k 實測混合建築 backsub > 100ms**,則使用者目標「真正 150k 實時」需要 A + B 組合;**如果只稍高於 16.67ms** (例如 30-50ms),則 (A) 單獨可能達標。

### 3.3 不做的事

- 不寫 BLR/HSS 程式碼
- 不接 GPU 後端
- 不重寫 SnSolver
- 不破 main 五腿 gate
- 不誇大「達到 60fps」如果實測沒到

### 3.4 夜班行動順序

1. **R2-1** Research bench: 90/120/150k 真實 fixture 量測 SnSession backsub
2. **R2-2** 如果 (1) 確認 backsub > 20ms,prototype FP32 + IR backsub
3. **R2-3** 量化 prototype speedup vs FP64 baseline
4. 並行做 main lane:A1/A2/A3/A5 + 視時間 A4 dyn_collapse.event channel

## 4. 用量控制

- 每個 candidate 證據從 web/文獻 fetch 一次後筆記,不重複拉
- bench log 不全文回讀,只記關鍵數字
- 用 subagent 跑大量檔案掃描
