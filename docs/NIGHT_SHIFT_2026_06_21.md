# Night-Shift Working Log — 2026-06-21 (00:11 → 09:00)

> 約 9 小時夜班,使用者要求:持續工作到早上 9 點 / 用 SUBAGENT / 控制用量 / 維持 commit / 工程管理紀律 / 深入思考設計 / 大量查資料測試 / 一段落用 release-hardening / 避免上下文壓縮失憶 / 先實驗再進引擎 / 不堆奇怪技術 / 不靠想像 / 不為部分功能犧牲主體 / 沒驗證不做。
>
> **目標**:讓引擎使用前沿科技或改良版達到「真正實時 150K」(現況可達邊界 ~150k 即時/~390k 互動但未穩),並擇優補完現有缺陷。
>
> **HEAD 起點**:`69c16e0` v2.8.1 — 7-agent adversarial audit pass on v2.6/v2.7,五腿閘門全綠 (F1..F64, UE 57, OpenSees, audit 104, CLI)。

---

## 工作節奏

每 90 分鐘 checkpoint,確保夜班不歪、上下文不錯亂。每個段落結束:
1. grep 確認 Edit 真落地 (踩雷:Edit 顯示成功 ≠ 真落地)
2. 跑五腿 gate
3. 顯式 `git add` 單檔 (絕不 `-A`)
4. 個別 commit 推 main
5. 更新本日誌、tasks、可能更新 MEMORY.md

---

## 段落 0 — 開機 + 計畫 (00:11–00:40)

### 目標
- 確認 git 在 v2.8.1 乾淨狀態
- 建立 task list + 本日誌
- 開始現況審計

### 完成
- HEAD `69c16e0` 確認 clean
- TaskCreate × 8
- 本日誌建立

### 決策
- 不直接動 main,先在 Research/R2_realtime_150k 跑實驗 (沿用 HpFEM research lane 模式,負面結果也誠實記)
- 先實驗瓶頸再選技術,不堆 BLR/HSS/GPU/AMP 等流行詞,先看 profile 結果再決定

---

## 段落 1 — 現況審計 + 候選技術盤點 (預計 00:40–02:30)

### 目標
1. 找出 150K 即時下的真正 wall-clock bottleneck (factor / backsub / Newmark / 殼裝配 / D/C 各佔比)
2. 盤點 5 個候選技術:
   - (a) BLR/HSS rank-structured Cholesky (進一步降 factor cost)
   - (b) GPU offload backsub (BLAS3 重型運算搬 GPU)
   - (c) 局部更新 (Schur complement / Sherman-Morrison) — 元素移除 → low-rank update
   - (d) 混合精度 (FP32 backsub + FP64 iterative refinement)
   - (e) reorder / blocking / NUMA pinning
3. 為每個候選找 oracle 證據 (paper benchmark / 開源實作數字)
4. 寫研究筆記 `Research/R2_realtime_150k/CANDIDATES.md`

### 完成
(待填)

### 失敗的嘗試 / 誠實負面
(待填)

### Commit
(待填)

---

## 段落 2 — 實驗 lane + 微基準 (預計 02:30–05:00)

### 目標
1. 設計實驗 lane 目錄 `Research/R2_realtime_150k/` (untracked,不破 main)
2. 對最有希望的 1-2 個技術做 micro-bench
3. 量化 speedup vs LDLT-supernodal baseline
4. 誠實記下負面結果

### 完成
(待填)

---

## 段落 3 — 缺陷補完 (預計 05:00–07:00)

### 目標
從 v2.6/v2.7/v2.8.1 deferred 清單擇優實作低風險項:
- `model.patch` schema
- `dyn_collapse.event` channel (S2 跨事件)
- B5.2 ReSolveSession session-cache 路由
- C# bridge 漏項
- F65/F66 warped-shell fixtures (擴大 OpenSees oracle)

選 1-2 項,每項先 oracle 再 code。

### 完成
(待填)

---

## 段落 4 — Gate + Release (預計 07:00–09:00)

### 目標
- 完整跑 `Scripts\run_gate.ps1 -RequireOpenSees` 五腿閘門
- 如有可發布實質改動 → release-hardening skill → tag v2.8.2 or v2.9
- 推 main + GitHub release
- 更新 MEMORY 索引

### 完成
(待填)

---

## 踩雷記錄 (本班新增,durable 入 memory)
(待填)

---

## 對 main 的承諾 (commit hygiene)
- ❌ 絕不 `git add -A`
- ❌ 絕不碰 `.gitignore` / `ArchSim.uproject` / `Plugins/LevelSim/` / build 產物
- ✅ 每個段落結束 grep + git status 雙確認
- ✅ 五腿 gate 全綠才推 main
- ✅ commit message 中文 ok 但技術詞保留英文 (project 慣例)
- ✅ 顯式 `git add` 個別源檔

## 上下文管理
- 每段落結束做一次 self-summary,寫進本日誌
- 一旦進入第 3 段,主動評估是否需要請 user 壓縮 (或用 ScheduleWakeup pause/resume) — 但夜班無 user 在,只能自管
- 用 Explore / general-purpose subagent 處理大量 grep/讀檔,主上下文只保留決策摘要

## 用量控制
- 大量 file-read 走 Explore subagent,不在主 context 展開
- benchmark log 不全文回讀,只留關鍵數字
- 文獻搜尋走 WebFetch 摘要,不存原文
