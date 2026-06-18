# v3 記憶體線痛點偵察 (recon)

> **定位**:純研究偵察文件,不含任何程式碼修改。依據三包實測數據(Phase E 純殼掃描、混合建築 supernodal_sweep + sn_sweep、scale_ladder SimplicialLDLT)與四條候選方向的文獻調查 + 對抗驗證,給出具體方向推薦與下一步基準計畫。

---

## 1. 目標與限制 — v3 記憶體線三角約束

v3 記憶體線目標:在單機工作站(32 GB 等級)同時達成三個約束:

| 約束 | 量化目標 | 備註 |
|---|---|---|
| **即時** | 60 fps → backsub ≤16 ms;互動 → backsub ≤100 ms | SnSession factor-once + 每幀 backsub,factor 攤銷 |
| **百萬 DOF** | K_ff 自由度 ≥ 1,000,000 | 混合 frame+shell 建築或純殼自由曲面 |
| **精度** | 殘差範數 ≤ 1e-9 | 含高 cond 場景 |

**三個約束是同時成立的硬性要求**,缺一即算未達目標。

從現有實測推算,在 32 GB + 混合 frame+shell 建築場景下:

- 實際 backsub ≤100 ms 上界:~150k DOF(CHOLMOD,`PROGRESS_R_supernodal.md:77`)
- 實際記憶體 fit 32 GB 上界:~190k DOF(peak ~10 GB at 191k,`supernodal_sweep.txt:25`)
- 殘差 ≤1e-9 上界:≤113k DOF(191k 時 res=1.57e-9,超標,`supernodal_sweep.txt:24`)

因此**百萬 DOF 目標在混合建築場景與目前技術三重都不可達**:記憶體、backsub 速度、精度皆構成獨立牆。這是單機物理牆,非算法缺陷。

---

## 2. 證據盤點

### 2a. 純殼 clamped 方板(exp_sn_chol.exe + skipLdltFactor,Phase E)

> 來源:`Research/V3_RESEARCH_HANDOFF.md:148-156`。`sn_limit_shell_sweep.txt` 原始輸出檔**不在磁碟**,數字僅見於 HANDOFF 文件。

| 場景 | nf | factorMs | backsubMs | peakMB | res | 備註 |
|---|---|---|---|---|---|---|
| 純殼 clamped 方板 | 37k | 92 | 5.2 | 236 | 1.0e-10 | 實測 |
| 純殼 clamped 方板 | 85k | 237 | 16.8 | 551 | 1.6e-10 | 實測 |
| 純殼 clamped 方板 | 152k | 474 | 31.7 | 962 | 2.3e-10 | 實測 |
| 純殼 clamped 方板 | 238k | 765 | 59.4 | 1541 | 3.1e-10 | 實測 |
| 純殼 clamped 方板 | 343k | 1024 | 77.2 | 2297 | 4.0e-10 | 實測 |

擬合指數(純殼):peak ~1.03、factor ~1.05、backsub ~1.1–1.2。

**外推(純殼,32 GB)**:`HANDOFF.md:157-166`

| 目標 | 推算 DOF | 方法 |
|---|---|---|
| 60 fps(backsub ≤16 ms) | ~85k | backsub 實測插值 |
| 30 fps(backsub ≤33 ms) | ~155k | 實測插值 |
| 互動(backsub ≤100 ms) | ~425k | 指數外推 |
| 記憶體 fit 32 GB(peak exp ~1.03) | ~3.9M | 指數外推 — **推算** |

> 注意:3.9M DOF 是記憶體外推值(基於指數 ~1.03 的線性外推),無對應實測點。

---

### 2b. 混合 frame+shell 建築(supernodal_sweep.txt + sn_sweep.txt,CHOLMOD / 自建)

> 來源:`Research/out/supernodal_sweep.txt`、`Research/out/sn_sweep.txt`。這兩個檔案**在磁碟**,為第一手數據。

| 場景 | nf | solver | factorMs | backsubMs | peakMiB | res | 備註 |
|---|---|---|---|---|---|---|---|
| mixed(10,8,15) | 8,910 | METIS | 397.7 | 4.9 | 112 pre / 144 done | 7.0e-11 | 實測 |
| mixed(10,8,15) | 8,910 | CHOLMOD | 145.6 | 0.883 | 144 done | 3.8e-11 | 實測 |
| mixed(12,10,20) | 17,160 | AMD | 6,057 | 21.3 | — | 2.4e-10 | 實測 |
| mixed(12,10,20) | 17,160 | METIS | 2,264 | 14.0 | — | 1.8e-10 | 實測 |
| mixed(12,10,20) | 17,160 | CHOLMOD | 217.6 | 5.72 | 362 done | 1.0e-10 | 實測 |
| mixed(12,10,20) | 17,160 | sn-par relax=16 | 199.2 | — | — | 5.7e-10 | 實測 vsChol=1.13x |
| mixed(16,12,24) | 31,824 | CHOLMOD | 514.5 | 13.8 | 842 done | 2.1e-10 | 實測 |
| mixed(16,12,24) | 31,824 | sn-par relax=16 | 492.0 | — | — | 8.8e-10 | 實測 vsChol=1.15x |
| mixed(20,16,30) | 64,260 | CHOLMOD | 1,100 | 33.6 | 2,064 done | 4.8e-10 | 實測 |
| mixed(20,16,30) | 64,260 | sn-par relax=16 | 1,205 | — | — | **1.42e-9(cond)** | 實測;res>1e-9 |
| mixed(24,20,36) | 113,400 | CHOLMOD | 2,625 | 66.7 | 4,879 done | 7.5e-10 | 實測 |
| mixed(28,24,44) | 191,400 | CHOLMOD | 6,070 | 132.8 | 10,234 done | **1.57e-9** | 實測;res>1e-9 |

> 注意:64k 混合建築的殘差 1.4e-9 是**未施加人工高 cond 的常規混合模型基準**,非病態特例(sn_sweep.txt:14)。191k 混合建築的 10 GB 為 peak-done,不含 factor 時間窗 peak。

CHOLMOD factor 指數擬合(8910 → 191400):整體 ~1.252,最高局部段(113k→191k)~1.384。

---

### 2c. 混合/純 frame SimplicialLDLT(scale_ladder.txt)

> 來源:`Research/out/scale_ladder.txt`。在磁碟,為第一手數據。

| 場景 | nf | factorMs | peakMiB | 備註 |
|---|---|---|---|---|
| 純 frame tower | 18,720 | 1,546 | 181 | 實測 |
| 純 frame tower | 61,560 | 30,558 | 772 | 實測 |
| 純 frame tower | 186,000 | 522,026 | 3,803 | 實測 |
| 純 frame tower | 389,664 | 3,229,228 | 10,359 | 實測;peak==factor,零餘裕 |
| 混合 frame+shell | 1,007,964 | OOM 或截斷 | — | 無結果行,推測 OOM |

SimplicialLDLT factor 指數約 3.03(go/no-go 期數據,`PROGRESS_R_supernodal.md:25`),CHOLMOD 約 1.56。

---

### 2d. 混合建築 AMD vs METIS 比較(supernodal_sweep.txt 首段)

| nf | AMD factorMs | METIS factorMs | CHOLMOD factorMs | METIS/AMD 加速比 |
|---|---|---|---|---|
| 8,910 | 499.3 | 397.7 | 145.6 | 1.25x |
| 17,160 | 6,057 | 2,264 | 217.6 | 2.68x |
| 31,824 | 23,264 | 10,199 | 514.5 | 2.28x |

**METIS 顯著優於 AMD**,但兩者都遠遜於 CHOLMOD supernodal。

---

## 3. 痛點假說

| 痛點代號 | 描述 | 硬證據 | 假說/缺口 |
|---|---|---|---|
| **A. 混合建築** | frame 非局部耦合使 peak 指數 ~1.36,32 GB 僅能放 ~190k DOF,backsub 超 100 ms | supernodal_sweep.txt 191k peak=10GB,backsub=133ms(第一手) | peak 指數精確值有兩套數字(1.36/1.65 舊數據;CHOLMOD 新數據整體 1.25,局部 1.38) |
| **B. 純殼 >>百萬** | 純殼記憶體 ~1.03 指數,32 GB ~3.9M 理論可達;但 backsub 成為瓶頸 | 343k 實測 backsub=77ms,百萬外推 ~700ms+,第一手 | 3.9M 是指數外推,無直接實測驗證;自由曲面 warped 網格指數未知 |
| **C. 高 cond 精度** | 混合建築 64k 時 res 已達 1.4e-9(無人工 cond),191k 超標至 1.57e-9;Neumaier 補償求和可延伸條件數邊界 | sn_sweep.txt:14 實測 res=1.4e-9(第一手);HANDOFF 文字描述 Neumaier 結果 | Neumaier 原始程式碼與量測檔案(`REALTIME_MILLION_DOF_RESEARCH.md`)不在磁碟 — **生產路徑未驗** |
| **D. 自由曲面雙曲** | Gaudí/Zaha 級別雙曲殼的 fill 指數完全未知 | 無 | 假說:可能接近純殼(2D 流形)或更差(warped 不規則網格) |

**結論**:
- **A** 有第一手證據支撐,是最緊迫的痛點(backsub + 記憶體雙瓶頸)。
- **B** 在純殼路徑上記憶體可達,但 backsub 是即時瓶頸。
- **C** 有第一手 sn_sweep 數據顯示問題實際存在,Neumaier 解法有文字記錄但無生產代碼。
- **D** 完全是假說,無實測數據。

---

## 4. 證據缺口

以下缺口阻礙可信方向選擇:

1. **`Research/REALTIME_MILLION_DOF_RESEARCH.md` 不在磁碟**:凍結於 `research/hpfem-solver-v1` 分支。此檔包含 Neumaier 補償求和的原始量測數據(cond=1e14 64k,1.4e-9→6.9e-10)與百萬 DOF 駐留 ~19GB vs peak ~117GB 的推算。這些數字目前僅存在於工作流程 context 文字,無磁碟原始數據可驗。

2. **`Research/out/sn_limit_shell_sweep.txt` 不在磁碟**:純殼 Phase E 掃描的原始輸出。數字由 HANDOFF 文件轉錄,但無法直接核對原始輸出格式、殘差計算方式與 CLI 旗標。

3. **Neumaier 補償求和無生產代碼**:`grep` 全專案 `.cpp/.h` 無 `neumaier`、`compensat`、`kahan` 等字樣。Phase E 的 exp_sn_chol.cpp 原始碼不在磁碟。若需生產整合,從零開始,非「只剩接線」。

4. **混合建築 supernodal peak 指數無第一手擬合**:scale_ladder.txt(LDLT)與 supernodal_sweep.txt(CHOLMOD)的尺度系列不共用一組大小,目前 CHOLMOD 整體指數 ~1.25,但「1.36-1.65 混合指數」引自舊 Phase E 數據。具體哪組數字適用於 supernodal 路徑,缺直接擬合。

5. **混合建築 sn_chol 自建路徑在 >64k 無量測**:sn_sweep.txt 最大為 64k DOF(已 FAIL 殘差),64k 以上只有 CHOLMOD 數據,自建 supernodal 在大規模混合建築的 factorMs 與 peakMB 完全未知。

6. **自由曲面雙曲殼 fill 指數完全空白**:無任何 doubly-curved 拓撲基準。F15(Scordelis-Lo)、F16(pinched cylinder)規模太小(n≤32),無 peak/factor 掃描。

7. **混合建築高 cond 場景無跨規模掃描**:sn_sweep.txt 只有 64k 一個 FAIL 點,缺 8k-191k 的 res vs nf 曲線。不知殘差超標臨界點是否是連續函數還是跳躍。

8. **`exp_sn_chol.cpp` 原始碼不在磁碟**:只有 `.exe` 與 `.obj`。`SolveOptions::skipLdltFactor` 旗標不在現有 `SolveOptions.h`,無法確認此 research flag 是否仍可編譯。

9. **駐留 MB vs peak MB 無量測**:只有 `peakMiB`(PeakWorkingSetSize),無 factor 完成後的駐留 RSS。out-of-core 方向的核心主張(peak 降至 5-15%)完全依賴外部文獻估算。

---

## 5. 候選方向比較表

| 方向 | peak 影響 | factor 影響 | backsub 影響 | 精度影響 | 對應痛點 | 主要風險 | 預估週數 |
|---|---|---|---|---|---|---|---|
| **rank-structured(BLR/HSS)** | 對純殼幾乎無效(指數已近 1.0);對混合 ~1.3-1.7x 降低(估算,非測量) | 在 <500k DOF 可能 **增加** 10-50%(JOREK 實測);>1M 有意義 | **零改善**(BLR 不壓縮三角求解) | 精度衝突:tight epsilon 抵消壓縮;loose epsilon + IR 在 cond>1e10 不可靠 | B(有限);A(無效—backsub 不變) | 混合建築 frame coupling 抵抗壓縮;Windows MSVC Fortran ABI 問題 | 8-13 週(in-house);4-8 週(STRUMPACK oracle) |
| **nested-dissection 調優** | 3-10% fill 降低(文獻);對混合建築的效果未知—可能零 | 與 fill 同比例降低;一次性攤銷 | 同比例 3-10% 改善(微小) | 零影響(排列置換不改計算精度) | A(邊際) | 混合建築 fill 提升可能是物理耦合造成,非排列問題;最多移動 ~10% 邊界 | 0.5-1 週(benchmark only;無程式碼改動) |
| **out-of-core** | 理論降至 working-panel (~5-15%);解決記憶體牆 | +10-30% overhead(NVMe overlap,估算) | **每幀讀 2×|L| bytes from NVMe:~15-30x 更慢**(100ms 互動目標在 64k 即達到極限) | 零影響(bit-identical 計算) | A(記憶體部分);但 backsub 瓶頸不解決 | backsub 速度與 NVMe 頻寬從根本衝突;Windows OVERLAPPED I/O 複雜性 | 12-16 週 |
| **Neumaier-refinement** | O(nf) 附加(可忽略) | 零 | 每步 +~2x(一個 IR 步驟加一次 matvec + backsub);64k 混合 backsub ~37ms → IR 後 ~94ms,逼近 100ms 上界 | **直接解決 C**:64k cond 邊界 ~2x 條件數延伸(文字記載);不解決 A/B | C(有效) | Neumaier 程式碼從零開始(無生產代碼);IR 在 cond>1e12 停滯;混合建築殘差超標是結構性而非精度偶發—可能需在 backsub 預算已滿時開火 | 0.5-1 週(生產整合) |

**核心結論**:

- BLR/rank-structured:對本專案規模無效;backsub 零改善是根本性缺陷。**不推薦**。
- nested-dissection 調優:代價最低,但效果上界是 10% fill 降低,無法解決量級差距。**邊際,可作前期探索**。
- out-of-core:記憶體問題技術上可解,但代價是 backsub 速度倒退 15-30x,破壞 SnSession 互動使用案例。**不推薦**。
- Neumaier-refinement:窄範圍有效(痛點 C),實作代價最低,且已有原型佐證(Phase E 文字記載)。**推薦**。

---

## 6. 對抗驗證結果摘要

### rank-structured(BLR/HSS)

**被推翻的主張:**
- "BLR 對混合建築可降 peak 1.3-1.7x" — 即使如此,backsub 不改善;191k DOF backsub 已 132ms,400k 外推 ~350ms,超過 100ms 互動預算 3.5x。BLR 完全不壓縮三角求解(MUMPS/PaStiX 均確認)。
- "BLR + IR 可達 1e-9 for cond~1e14" — cond=1e14 時 O(epsilon × cond) 誤差在 epsilon=1e-7 下約 O(10^7),數學上不可能收斂至 1e-9。
- "純殼 fill exp ~1.03 有可壓縮空間" — fill 已近線性,BLR 理論漸近也是 O(n log n),實際無可壓縮空間。

**存活的主張:**
- BLR 確實不壓縮三角求解相(JOREK 研究第一手確認)。
- >1M DOF 3D 實體問題 BLR 有效 2-3x 記憶體降低(但超出本專案 32GB 可達範圍)。
- frame 1D 鏈拓撲無法被 BLR 壓縮。

---

### nested-dissection-mixed(排序調優)

**被推翻的主張:**
- "METIS 已在混合建築達最優" — 從未測試過 METIS vs 自然排序在 sn_chol 路徑上的混合建築;scale_ladder.txt 是 SimplicialLDLT 生成的,不是 supernodal 路徑。
- "3-10% fill 降低可移動 360k 邊界至 390k" — 要從互動 ~150k 移到 ~360k 需要 fill 降低 (360/150)^{4/3} ≈ 3.5x。10% 遠遠不夠。

**存活的主張:**
- METIS 顯著優於 AMD(17k: 2.68x factor 加速,supernodal_sweep.txt 第一手)。
- sn_chol.h 目前以 nullptr 傳入 METIS options(即預設),nseps tuning 從未測試。
- 排序對精度無影響(置換是精確等價)。
- 多層樓混合建築的 fill 指數 ~4/3 是結構性,非可排序消除的。

---

### out-of-core

**被推翻的主張:**
- "OOC 可讓 32GB 處理 2-4M 混合建築 DOF" — 技術上成立,但在需要 OOC 的規模上,每幀讀 2×|L| bytes from NVMe 的代價使 backsub ≥7-8 秒,與互動目標不相容。OOC 是批次計算工具,非即時計算工具。
- "10-25% factor overhead(HPC 文獻)" — 本專案 supernode panel 規模(數十至數百 KB)位於 I/O 延遲主導區,非頻寬主導區;HPC 文獻數字過樂觀。

**存活的主張:**
- fill 計數與 in-core 完全相同,OOC 不改變數值計算。
- 對批次單次求解場景(無即時重用 factor 要求)OOC 是有效選項。
- backsub 需要 2×|L| sequential reads — 這是不可回避的物理約束。

---

### neumaier-refinement

**被推翻的主張:**
- "IR 只在 cond>1e8 的病態情況才需要" — sn_sweep.txt:14 實測:64k 常規混合建築 res=1.40e-9,無人工高 cond,是正常建築拓撲的結果。殘差超標是系統性的,非偶發。
- "已部分完成,只剩接 IR 迴圈" — Neumaier 補償求和原始碼不在磁碟,`exp_sn_chol.cpp` 不在工作樹,Phase E 原始量測檔案不在磁碟。生產整合是從零開始。
- "一個 IR 步驟 +~2x backsub,在互動範圍內" — 使用了純殼 backsub 數字。混合建築 64k backsub ~37ms;加一步 IR ~37ms + ~20ms matvec = ~94ms,幾乎填滿 100ms 互動預算,zero headroom。

**存活的主張:**
- 零 fill 增加,factor 完全不觸碰。
- MSVC long double == double 是有效且 durable 的踩雷;Neumaier 補償求和是 MSVC 上正確的平台兼容替代。
- opt-in SnSolveOptions::irSteps 欄位是低風險向後相容擴展。
- Phase E 文字記載 1.4e-9 → 6.9e-10(一步補償求和 IR)的結果,即使原始程式碼不可及,方向正確。

---

## 7. 推薦下一步

### 推薦:生產整合 Neumaier 補償求和 IR(不需程式碼改動前先跑基準)

**理由:**
1. 實作代價最低(0.5-1 週),是四個方向中 risk/reward 最優的。
2. 痛點 C(高 cond / 大規模殘差超標)已被 sn_sweep.txt 第一手數據確認是常規問題,非邊緣案例:64k 混合建築 res=1.40e-9、191k res=1.57e-9。
3. 不改變任何 fill/factor 路徑,不引入新外部依賴。
4. 精確擴展殘差精度上界,與目前 vsCHOLMOD gate 設計兼容(gate 比較解向量,非絕對殘差)。

**前置基準計畫(不需任何程式碼修改):**

| 步驟 | 動作 | 需要的 fixture | 預期輸出 |
|---|---|---|---|
| 1 | 重跑 sn_sweep.txt 混合建築掃描,記錄每個 DOF 的 res 值,確認殘差 >1e-9 的 nf 臨界點 | 現有 `supernodal_sweep` 工具(`exp_sn_chol.exe --mixed`) | res vs nf 曲線;確認 64k 是首個超標點還是更小 nf 已超標 |
| 2 | 計算在 64k 混合建築上手動一步 IR 的預期精度:估算 vsCHOLMOD 目前是 2.57e-12(第一手),代表 solution 幾乎完美;而殘差 1.4e-9 是 K*u 的捨入誤差放大 | 無(算術計算) | 確認 IR 改善目標是「殘差計算精度」而非「解向量精度」;若 vsCHOLMOD 已 <1e-11,IR 後 res 應接近機器精度 × cond |
| 3 | 記錄 64k 混合建築 backsub 時間(CHOLMOD:33.6ms,sn-par:~37ms),計算 IR overhead budget:33.6ms(原始)+ 33.6ms(IR backsub)+ ~17ms(K*u matvec 估算)= ~84ms,確認是否在 100ms 互動預算內 | 現有 sn_sweep.txt 數據 | 確認 IR 能在互動預算內合適運作的 DOF 上界 |
| 4 | 對比 191k DOF 的估算:backsub 132ms 本身已超 100ms,加 IR 達 ~280ms — 確認 191k 無法做互動 IR | 算術 | 為 IR 生產整合確立 DOF 範圍限制(≤~64k 可互動 IR) |

**若基準計畫確認可行,後續整合步驟(估計 0.5-1 週):**

1. `sn_chol.h` 或 `SnSolver.cpp` 新增 Neumaier 補償稀疏矩陣-向量乘積 helper(~30 行)。
2. `SnSession::solveFrame` 加 IR 迴圈(irSteps 次迭代,收斂範數判斷,~20 行)。
3. `SnSolveOptions` 加 `irSteps`(預設 0)與 `irTol`(預設 1e-9)欄位。
4. `Standalone/main.cpp` 新增 F-test:高 cond 混合建築案例,irSteps=1 → res 相較 irSteps=0 改善。

**注意:需要程式碼變動。無法純靠 re-run 現有 binary 驗證生產路徑。**

---

## 8. 不推薦 / 留未來

### rank-structured(BLR/HSS/H-matrix) — 明確不推薦

**原因:**
- backsub 為零改善是根本性缺陷,而 backsub 是 SnSession 互動模式的每幀瓶頸。
- 在 <500k DOF(本專案互動範圍)factor 時間可能劣化(JOREK 36-50% slowdown)。
- 混合建築 frame-coupling 拓撲(1D beam chains + 2D shell patches 共用節點)對 BLR off-diagonal rank 壓縮抗拒;METIS nested dissection 的分隔面穿越 frame+shell 共用節點,無法選擇性壓縮。
- 精度要求(1e-9)需 epsilon~1e-10,抵消大部分壓縮效益;高 cond 場景 BLR + IR 數學上不可行(O(epsilon×cond) 誤差)。
- 純殼 fill 指數 ~1.03 已近最優,無可壓縮空間。

若未來問題規模穩定超過 1M DOF 且由 3D 實體 FEM 主導(非本專案目前場景),STRUMPACK 作為外部 oracle 可重新評估。

### out-of-core — 明確不推薦(互動場景)

**原因:**
- OOC 與 SnSession factor-once + solve-many 架構從根本不相容。SnSession 的設計假設是「factor 一次,每幀 backsub」;OOC 使每幀 backsub 讀取 2×|L| from NVMe,在 64k 混合建築規模即達 ~280ms per frame。
- 對批次計算(無互動要求、每個模型求解一次)OOC 是有意義的選項,但超出 v3 「real-time + 百萬 DOF」的指定目標。
- 實作複雜度估計 12-16 週(Windows OVERLAPPED I/O + sn_chol.h 並行排程整合),成本超過所有其他方向。

### nested-dissection 調優 — 低優先級,可選前期探索

**不推薦作為主要方向**,理由:
- 最大效益上界是 3-10% fill 降低,對移動量級差距(目標 360k vs 目前 ~150k 互動上界)是至少 3.5x 差距,10% 遠不夠。
- 多層樓建築的 fill 指數 ~4/3 是多層間非局部耦合的結構性結果,無法透過排序消除。

**可選探索**:若想確認 METIS 已達混合建築排序最優(消除疑問),唯一需要的是:對 sn_chol.h 的 `metisOrder()` 傳入 `nseps=3` 的選項陣列(修改一行,約 5 行 code),然後對現有 sn_sweep.txt 的混合建築各規模重跑,比較 L_nnz 是否下降 >5%。這是 0.5 天的投資。若 L_nnz 變化 <5%,方向關閉。

---

## 9. 踩雷與認識限制

以下是本次偵察本身的誠實限制:

1. **Phase E 純殼數字來自不可再現的 binary**:`exp_sn_chol.cpp` 不在工作樹,`skipLdltFactor` 旗標不在現有 `SolveOptions.h`。純殼 factor/backsub/peak 的 5 個數據點無法直接重跑確認。數字可信(由 HANDOFF 轉錄),但屬「引用數字」非「當前可執行代碼驗證」。

2. **Neumaier 結果(1.4e-9→6.9e-10)目前無原始數據**:`REALTIME_MILLION_DOF_RESEARCH.md` 不在 main 分支磁碟,Neumaier 程式碼不在生產樹。此數字依賴 workflow context 文字,屬「有文字記錄的 Phase E 研究結果」而非「可從磁碟重現的測試」。

3. **混合建築 supernodal peak 指數擬合不完整**:scale_ladder.txt 是 SimplicialLDLT 路徑(AMD ordering),supernodal_sweep.txt 是 CHOLMOD 路徑,兩者用的模型尺寸序列不同。「混合建築 peak exp 1.36-1.65」引自 HANDOFF 舊 Phase E,不是本輪量測的擬合值。本輪 CHOLMOD 數據擬合整體 ~1.25,但模型尺寸序列不夠大(最大 191k)。

4. **BLR 文獻均為 HPC 場景**:JOREK(熱核聚變 FEM)、3D EM FWI 等問題的拓撲與 FrameCore 混合建築顯著不同。BLR 在建築 FEM 混合 frame+shell 拓撲的實際壓縮比完全未量測。

5. **自建 sn_chol 在 >64k 混合建築未量測**:sn_sweep.txt 的 64k 混合建築已達 FAIL(res=1.40e-9),更大規模只有 CHOLMOD 數據。若要比較自建路徑在 113k、191k 的性能,需要對自建 sn_chol 補跑,目前缺數據。

6. **backsub 瓶頸的雙重性容易被遺漏**:純殼路徑的 backsub 已很快(37k: 5.2ms),會誤導認為「backsub 不是問題」。混合建築路徑的 backsub 在 64k 已達 33.6ms(CHOLMOD),191k 達 132ms。任何改善混合建築記憶體的方向都必須同時考量 backsub 代價,否則延伸出的「可解 DOF 範圍」在即時性上毫無意義。

7. **本次偵察所有方向評估均基於文獻 + 現有數據**:無新實驗。因此對 rank-structured 和 out-of-core 的「不推薦」結論有 medium-to-high 信心(物理論證充分),但 nested-dissection 的實際 fill 改善量在混合建築場景上未量測;Neumaier 的 cond 延伸範圍在混合建築(非純殼)的確切邊界也未量測。

---

## 關鍵參考檔案

- `E:\project\ArchSim\Research\V3_RESEARCH_HANDOFF.md`(Phase E 數字轉錄源)
- `E:\project\ArchSim\Research\out\supernodal_sweep.txt`(混合建築 CHOLMOD 第一手數據)
- `E:\project\ArchSim\Research\out\sn_sweep.txt`(自建 sn_chol vs CHOLMOD 第一手數據)
- `E:\project\ArchSim\Research\out\scale_ladder.txt`(SimplicialLDLT 尺度梯比第一手數據)
- `E:\project\ArchSim\docs\PROGRESS_R_supernodal.md`(R 線 supernodal 整合記錄)
- `E:\project\ArchSim\Plugins\FrameSolver\Source\FrameCore\Public\FrameCore\SolveOptions.h`(公開 API 現況,含 SnSolveOptions)
- `E:\project\ArchSim\Plugins\FrameSolver\Source\FrameCore\Private\sn_chol.h`(自建 supernodal 核心,含 metisOrder 入口)


---

## Workflow 推薦下一步

重跑 sn_sweep.txt 混合建築掃描以確認 res>1e-9 的 nf 臨界點,然後整合 Neumaier 補償求和 IR 至 SnSession::solveFrame(irSteps 欄位 opt-in)

### Workflow 提案的 benchmark fixtures

- 混合建築 sn_sweep 全尺寸 (8k-191k) 殘差 vs nf 掃描 — 確認殘差超標臨界點與 res 連續性
- 自建 sn_chol 混合建築 >64k 量測 — 補全 113k、191k 的 factorMs/peakMB/res(目前只有 CHOLMOD 數據)
- sn_chol metisOrder nseps=3 vs 預設 — 混合建築各規模 L_nnz 比較(確認 METIS 調優上界)
- 純殼 Phase E 重現 — 用目前可編譯路徑確認 37k-343k backsub/peak 數字(exp_sn_chol.cpp 需從 research branch 取回)
- Neumaier IR 整合後的 F-test — 64k 混合建築 irSteps=1 vs 0 的殘差改善與 backsub overhead 量測
