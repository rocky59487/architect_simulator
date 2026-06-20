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

## 段落 1 — 現況審計 + 候選技術盤點 + 三項小補完 (00:40–02:30,實際 00:11–02:00)

### 目標
1. 找出 150K 即時 wall-clock bottleneck
2. 盤點 5 候選技術
3. 為每個找 oracle 證據
4. 寫研究筆記 `Research/R2_realtime_150k/CANDIDATES.md`
5. 同步做 deferred 清單 small 項

### 完成
- 兩 Explore agent 並行收事實 (bottleneck audit + deferred 項清單)
- **關鍵發現**:現有「150K 即時」是 log-log 外推 [THEORY],底層實測 17/32/64k + 113k + 191k。113k CHOLMOD backsub 67ms 已超 60fps 預算 4×。**真正瓶頸 = backsub** (factor once-then-reuse)。150K 60fps 在混合建築未直接量測,純殼 85K 已達 16.8ms。
- `Research/R2_realtime_150k/CANDIDATES.md` 完成,6 候選 (A 混合精度+IR / B Schur local update / C GPU / D BLR / E reorder / F multi-RHS) 全部評估
- **判決**:✅ A 混合精度+IR (prototype 階段) / ⚠️ B 高效益但風險高 (延後) / ❌ C, D, E, F (違反「不堆奇怪技術」或場景不對)
- **必補**:沒有 100K/150K 直接實測 → 必先做 micro-bench 才能確認 candidate A 預期值

並行做 deferred 小補完 (A1 + A2 + A5):
- A1 `v2_roundtrip.py`: engine_version 從 "non-empty" → "semver-ish" + `FRAMECORE_EXPECTED_ENGINE_VER` env literal pin (catches v2.5-era kEngineVer drift)
- A2 `DynamicCollapse.cpp`: post-event snapshot 後加 isCancelled poll (mirror line 408 inner-loop pattern); cancel during re-configuration 響應加速一個 frameStride
- A5 `Standalone/main.cpp`: F41/F60 跳號 policy 入 source comment

### Gates 全綠
- standalone F1-F64 ALL PASS (failures=0)
- UE automation 57/57 (UE incremental build 23.88s — hot cache 神速,不是 MEMORY 警告的 swap thrash)
- OpenSees PASS
- linear-deep-audit 104/104
- frame_cli round-trip ALL PASS
- v2_roundtrip ALL PASS (含新 engine_version semver + env pin checks)

### Commit
- `a307a0c` "feat: v2.8.2 audit closeouts — A1 engine_version semver+pin / A2 post-event cancel poll / A5 F41/F60 policy"
- pushed to origin/main

### 失敗的嘗試 / 誠實負面
- A5 初版 comment 提 `$ExpectedFixtures` 變數 — `run_gate.ps1` 沒這變數,改成 grep-friendly 描述 `g_fail` 計數器,**踩雷:不要寫不存在的事實**

### 用量
- 主上下文兩 Explore agent + 三個 Edit + 五腿 build/test。無 web fetch (CANDIDATES.md 用既有 ICL/MUMPS/cuSPARSE 公開知識引用,先不查證 paper 細節)

---

## 段落 2 — R2 lane micro-bench + lazy recover 落地 (02:00–04:30,實際進行中)

### 目標
1. 設計實驗 lane `Research/R2_realtime_150k/` (untracked)
2. 對最有希望的 1-2 個技術 micro-bench
3. 量化 speedup vs LDLT-supernodal baseline
4. 誠實記下負面結果

### 完成
- r2_bench.cpp + build_r2.bat (Research lane,untracked)
- 跑 90k / 120k / 160k 三個 frame tower 規模:
  - 90k: factor 3508ms, solveFrame **132 ms** (60fps -115ms / 100ms -32ms 雙 FAIL)
  - 120k: factor 5282ms, solveFrame **218 ms**
  - 160k: factor 9610ms, solveFrame **389 ms**
- **關鍵發現**:既有「150k 即時 backsub<100ms」是純 sn::solveSuper 數字,不是 user-facing solveFrame 體驗時間。**真正瓶頸是 RHS+backsub+SPMV+recover 全棧**。
- RESULTS_round1.md 完整記錄

實作 Candidate G (lazy force recovery):
- SnSessionOptions::skipForceRecovery 預設 false (backward-compat)
- SnSession::solveFrame skip member/shell recover,留 u + reactions
- r2_bench 加 --compare 模式
- standalone F65 fixture: 8 個 check 驗 bit-equivalence + sizes + diagnostic tag
- **measured**: 90k 從 132.9 → 115.9 ms (1.15× / -16.9ms)

### 失敗的嘗試 / 誠實負面
- **預測錯**:RESULTS_round1.md 估 recover 50-80ms,實測只 17ms (-13% solveFrame)。BeamColumnElement::recover 對 46k members 估 6.6M flops ≈ 1ms 應該想得到,被「46k member 很多」的錯覺誤導
- lazy recover 是 ABI-friendly mini-win,**不是「真正實時 150K」突破**。真正大頭 (115ms) 在 RHS+backsub+SPMV 沒解。
- 60fps @ 150K 在當前架構是物理牆 (RESULTS 結論)

### Gates 全綠
- standalone F1-F65 ALL PASS (failures=0)
- UE automation 57/57
- OpenSees PASS
- linear-deep-audit 104/104
- frame_cli round-trip ALL PASS
- v2_roundtrip ALL PASS (env pin 2.8.1)

### Commit (準備中)
- 4 modified files (SnSession.h/cpp/main.cpp/NIGHT_SHIFT)
- Research/ 仍 untracked (research-only)

---

## 段落 3 — sub-stage instrumentation + 真兇 fix (04:00–05:00,實際進行中)

### 目標
1. 加 SnSession sub-stage timing (SN_SESSION_TIMING opt-in)
2. 找出 RHS 內 80ms 「rest」隱藏成本來源
3. 設計 fix + 加 oracle fixture
4. gate 全綠 commit

### 完成 ★ 重大突破
- SnSession.h 加 `SnSessionTimings`/`lastTimings()` API
- SnSession.cpp 加 `#ifdef SN_SESSION_TIMING` block,主 lane 預設 zero cost
- r2_bench 跑 90k 看到 sub-stage:rhs=87 ms (eq=2.5 / Kloop=N/A / **rest=85**)
- **真兇**:`FrameModel::nodeIndex` 是 O(N) linear search,14k loads × 15k nodes = **110M iter/frame ≈ 74ms**
- **第二兇**:sparse-K loop 在 prescribed-all-zero 場景仍跑 O(nnz),雖然 inner branch 都 skip 但 outer iter 還是付了
- 兩個 patch (additive,backward-compat):
  - SnSession 加 `std::unordered_map<NodeId, int>` lazy cache,fingerprint guard 保證 stable
  - RHS sparse-K loop 加 `hasNonZeroPresc` 守門,base-fixed-at-0 場景全 skip
- **measured**: 90k LAZY 從 134 ms → **55.8 ms (2.4× speedup)**;**PASS 100ms 互動級** (從 fail)
- 加 F66 fixture (cantilever + prescribed tip rotation): 8 check 全 PASS,rel=0 bit-equivalent vs LDLT oracle
- RESULTS_round2.md 完整記錄
- **教訓**:Round 1 估算錯一個量級。Lesson: **per-stage budget 有問題就量,不靠想**

### Gates 全綠
- standalone F1-F66 ALL PASS (新加 F66 8 check)
- UE automation 57/57 (UE rebuild 39s)
- OpenSees PASS
- linear-deep-audit 104/104
- frame_cli round-trip ALL PASS
- v2_roundtrip ALL PASS (env pin 2.8.1)

### 失敗的嘗試 / 誠實負面
- Round 1 估算 recover 50-80ms,實際 21ms (-13%)
- Round 1 估算 backsub 50-80ms,實際 47ms (within range,但不是最大頭)
- Round 1 完全沒猜到 `nodeIndex` linear scan 是 74ms 大頭
- 60fps@90K 還是物理牆 (-39ms)
- 30fps@90K 仍 fail (-22ms) — 需要 backsub 改進才能達

### Commit (準備中)
- 3 files (SnSession.h/cpp/main.cpp)
- Research/ 仍 untracked

## 段落 4 — TBD (05:00–07:00)
候選:
- 把 fastpath + nodeIdx cache 也 propagate 到 solveLoad (FrameSolver.cpp:309-322 同 pattern)
- A4 dyn_collapse.event channel
- mixed-precision IR backsub prototype (clear 30fps@90K)
- A3 abortReason → optional micro-perf

## 段落 5 — release-hardening (07:00–09:00)

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
