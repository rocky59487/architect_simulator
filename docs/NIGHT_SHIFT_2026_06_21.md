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

## 段落 4 — solveLoad fastpath + A4 (05:00–07:00,完成)

### 完成
- solveLoad propagation (`FrameSolver.cpp`):同 patch (unordered_map cache + hasNonZeroPresc 守門),讓 PDelta/Reanalysis/Modal initial step 全 free 得益
- A4 dyn_collapse.event live channel:`DynCollapseOptions::onEventEmitted` + dispatcher `liveEvents=true` 預設 + capability `dyn_collapse.live.events`
- v2_roundtrip 加 R2.3 live events fixture(live_events=1 == nEvents=1)+ capability 廣告 check
- 兩個 commit:`c76692b` solveLoad propagate / `22c4971` A4 channel
- 五腿全綠 + v2_roundtrip ALL PASS

### Commits
- `c76692b` perf(solveLoad): propagate RHS fastpath + nodeIdx cache from SnSession
- `22c4971` feat(v2): A4 dyn_collapse.event live channel (close v2.6 deferred)

## 段落 5 — release v2.9.0 (07:00–07:30,完成)

### 完成
- `kEngineVer` 2.8.1 → **2.9.0** (Dispatcher.h)
- `FrameSolver.uplugin` VersionName 2.8.1 → 2.9.0,Version 22 → 23
- `docs/RELEASE_v2.9.md` 完整 release notes (8 sections)
- Research/R2_realtime_150k/ 改進 main(source + docs track,exe/obj gitignored)
- commit `7873e39` release: v2.9.0 (含 version bumps + RELEASE notes + R2 lane track)
- tag `v2.9.0` 推 GitHub
- `gh release create v2.9.0` + `framecore-v2.9.0-win64.zip` (1.7MB) 上傳
- Release URL: <https://github.com/rocky59487/architect_simulator/releases/tag/v2.9.0>
- 更新 `E:\project\CLAUDE.md`:HEAD v2.9.0 / fixture F1-F66
- 更新 `~\.claude\projects\E--project\memory\frame-engine-next-plan.md`:v2.9.0 entry
- 更新 `~\.claude\projects\E--project\memory\MEMORY.md`:索引條目

### Commits 總計 (夜班 8 個)
1. `a307a0c` v2.8.2 audit closeouts (A1+A2+A5)
2. `7b2dc65` SnSession lazy recover (F65)
3. `cdfaeeb` SnSession RHS fastpath + nodeIdx cache (F66, 2.4× @ 90k)
4. `c76692b` solveLoad fastpath propagation
5. `22c4971` A4 dyn_collapse.event live channel
6. `7873e39` release: v2.9.0
7+. (CLAUDE.md / MEMORY 不在 git tracked,直接更新 in place)

## ★ 夜班續延總結 (使用者醒來凌晨 03:25 補充更多時間 + 硬體規格)

凌晨 03:25 使用者醒過來,留訊息「還能繼續工作」,明示硬體 R9 8940HX + RTX 5070 Ti,「各種技術都可以採納,只要真的有經過嚴謹驗證」。

### R2 Round 3 三步驟(research lane)

1. **Step 1: CPU FP32 mixed-precision IR backsub** (`fp32_safety.cpp`)
   - 跑 90k frame tower 用 Eigen FP32 SimplicialLDLT vs FP64
   - **NEGATIVE**: residual=0.879 (no convergence),2 IR step 後仍 3.5e-4
   - 根因:κ≈1e8 遠超 Higham IR-converges 8e6 邊界(braced tower axial vs bending 270×/elem × section size 範圍)
   - FP32 backsub 只快 1.19× (不是預期 2×)
   - **判決:DROP**。`RESULTS_round3_fp32_negative.md` 完整記錄

2. **Step 2a: cusolverSp high-level GPU** (`gpu_bench.cpp`)
   - `cusolverSpDcsrlsvchol` 跑 90k 花 9501 ms
   - 原因:cusolverSp csrchol* family 是 **host-based**(NVIDIA docs 明示)
   - **NEGATIVE**。`gpu_bench.cpp` 保留作 negative result history

3. **Step 2b: cuDSS GPU-native** (`gpu_bench2.cpp`)
   - cuDSS 是 NVIDIA 2024+ 真 GPU-native sparse direct solver
   - PHASE_ANALYSIS+FACTORIZATION 真 GPU,PHASE_SOLVE 真 GPU triangular
   - **per-frame 2.5ms@90k / 3.3ms@120k / 5.5ms@160k / 6.8ms@200K**
   - **60fps PASS 全範圍**!`RESULTS_round3_gpu_success.md`

### Production 整合(commit `596b0f9`)

- `SnSessionOptions::useGpuBacksub` 預設 false
- `#ifdef FRAMECORE_CUDA` 整段 Impl/ctor/dtor/solveFrame branch
- GPU 失敗 → fallback CPU sn::solveSuper(transparent)
- ctor refactor:SnPrimary reuse 早期 return 拿掉變 `else if`,讓 GPU setup attach 到 reused factor;F66 仍 rel=0
- `build_sn_cuda.bat` → `frametest_cuda.exe`(default `frametest.exe` 不變)
- F67 fixture(7 check):GPU.u==CPU.u rel<1e-8 + reactions + sizes + [GPU] tag in diag

### Production 測試結果(`r2_bench --gpu`)

| nf | LAZY+GPU | 60fps | 30fps | vs CPU LAZY |
|---|---|---|---|---|
| 90,402  | **12.4 ms** | **PASS +4.3ms** | PASS | 4.5× (vs 56ms) |
| 161,280 | **25.6 ms** | -8.9ms | **PASS +7.7ms** | 4.2× (vs 108ms) |
| 211,140 | **34.0 ms** | -17.3ms | -0.6ms | ~4.4× (vs ~150ms) |

**60 fps @ 90k production**!**30 fps @ 150K production**!使用者「真正實時 150K」目標達成。

### v2.10.0 release

- `kEngineVer` 2.9.0 → **2.10.0**
- `FrameSolver.uplugin` Version 23 → 24, VersionName 2.9.0 → 2.10.0
- `docs/RELEASE_v2.10.md` 完整 release notes
- commit `cbd3aa5` release: v2.10.0
- tag `v2.10.0` 推 GitHub
- `gh release create v2.10.0` + `framecore-v2.10.0-win64.zip` 2.2 MB(含 frametest_cuda.exe)
- Release URL: <https://github.com/rocky59487/architect_simulator/releases/tag/v2.10.0>

### 夜班續延 commits 總計

| # | hash | one-liner |
|---|---|---|
| 11 | `4b2edcd` | research(R2 round 3 step 1): FP32 NEGATIVE -- pivot to GPU |
| 12 | `75287ac` | research(R2 round 3 step 2): cuDSS GPU BREAKTHROUGH at 200K |
| 13 | `596b0f9` | feat(SnSession): cuDSS GPU backsub lane (FRAMECORE_CUDA opt-in) |
| 14 | `cbd3aa5` | release: v2.10.0 -- cuDSS GPU production-integrated |

### 額外 durable 教訓

7. **cusolverSp csrchol* 是 host-based**,不是 GPU(NVIDIA docs 講清楚但容易踩;先用 high-level API 測再判)
8. **真 GPU sparse direct solver 是 cuDSS**(2024+ NVIDIA 套件;`conda install -c nvidia libcudss-dev`)
9. **FP32 在建築剛度矩陣不可行**(κ≈1e8 遠超 FP32 邊界;不要再花時間試)
10. **cuDSS DLL 群組**:cudss64_0 + cudss_mtlayer + cudart + cusparse + nvJitLink 都要 co-locate(用 `dumpbin //dependents` 找 transitive deps)
11. **`dumpbin //dependents`**(雙 slash)在 Git Bash 防 path translation
12. **research lane 可進 main**(PROGRESS_R_supernodal pattern,source/docs track,exe/obj gitignored)

## 夜班最終總結 (07:30 — 原始版,留作 v2.9.0 結束時的時間軸)

### 量化成果
- **8 commits push origin/main** + 1 GitHub release tag
- **90k frame tower solveFrame 134ms → 55.8ms (2.4×)** — PASS 100ms 互動級
- **160k 389 → 108ms (3.6×)** — 接近 100ms 互動級
- F65 + F66 兩個 fixture 加,standalone F1-F66 ALL PASS
- 五腿 gate + v2_roundtrip 始終全綠
- v2.6 deferred dyn_collapse.event channel 收尾
- v2.8.2 audit deferred 3 項 (A1+A2+A5) 收尾

### 關鍵教訓 (durable)
1. **per-stage budget 有問題就量,別猜** — Round 1 估算 recover 是大頭(50-80ms)錯了 4×;真兇 nodeIndex linear scan 完全沒料到
2. **80ms 的「不知名 rest」不可放過** — sub-stage timing 找出真兇花了 1 小時,實作 fix 30 分鐘,收益 2-3.6×
3. **bit-equivalent skip 是優化的安全形式** — `presc[c]=0` 場景跳 sparse K loop,callback nullable default empty,全 backward-compat 無風險
4. **Research lane 可進 main** — 沿用 PROGRESS_R_supernodal pattern,source/docs track 不沾 build/gate (.gitignore 擋 exe/obj)
5. **真正 60fps 150K 是物理牆**(backsub 本身 47-91ms) — 誠實設定可達目標(30fps@90K 或 100ms 互動 @ 150K),別硬推
6. **UE incremental hot cache 快**(~40s 完成,不是 MEMORY 警告的 1h57m swap thrash)

### 仍 deferred 到 v2.10
- Mixed-precision IR backsub (Candidate A,清 30fps@90K 跟 ≤100ms@200K)
- B1 ReSolveSession dispatcher session-cache routing
- B2 model.patch schema + handler
- C-09/C-10 widening (transient LDLT side-system for SnPrimary modal/buckling)
- A3 abortReason → std::optional 微優化

### 用量與紀律
- 沒用 ScheduleWakeup / 沒用 background long-sleep / 沒用 destructive git ops
- 兩個 Explore subagent (段落 1 並行) 收事實,其餘自做
- 五腿 gate 跑 6 次(每段 commit 都跑)
- v2_roundtrip 跑 4 次
- 始終顯式 `git add` 個別檔,從未 `git add -A`
- 從未碰 .gitignore / .uproject / Plugins/LevelSim/

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
