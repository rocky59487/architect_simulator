# 交接指南 — `v3.1.0` 後接手 owner

> `v3.1.0` 在 2026-06-21 發布, tag `v3.1.0` = commit `<see git log after Phase 5>`.
> 主交接文是 `docs/HANDOFF.md`(原始 v2.x history,不動); 前一輪 release-hardening
> follow-up 在 [`docs/HANDOFF_v2.11.1.md`](HANDOFF_v2.11.1.md); 本檔補上 v3.1.0
> 多出來的內容(S11 stress-field + 7-agent audit closeouts + deferred carry-over)。

## 1. `v3.1.0` = 什麼

**一句話:** 新增 S11 視覺化用的應力場 post-process(`computeStressField`),把
`ElasticAllowable` 跟新 `StressField` 兩條 path 合用同一個 `StressKernel.h`
single-source-of-truth header,F70 D/C interlock bit-exact,F1..F66 不動。

**Engine source delta vs v3.0.1:** 5 files / ~350 lines。

- `Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/StressKernel.h` (新)
- `Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/StressField.h` (新, POD result types
  + `[[nodiscard]] FRAMECORE_API computeStressField`)
- `Plugins/FrameSolver/Source/FrameCore/Private/StressField.cpp` (新, impl)
- `Plugins/FrameSolver/Source/FrameCore/Private/ElasticAllowable.cpp` (refactor — 改 call
  `StressKernel.h` 共用函式;F1..F66 bit-identical)
- `Plugins/FrameSolver/Source/FrameCore/Private/Tests/StressFieldTest.cpp` (新 UE F68 mirror)

**Standalone 新 fixture:** `Plugins/FrameSolver/Standalone/main.cpp` +219 行 — F68 cantilever
member field / F69 clamped-plate shell layer recovery + 30° z-rotation invariance / F70 D/C
interlock。三者全綠(worstRel = 0)。

**v2 dispatcher 新 capability:** `inspect.stress_field`。`kEngineVer 3.0.1 → 3.1.0`。
JSON 形狀照 [`docs/specs/S11_stress_field.md`](specs/S11_stress_field.md)。

**Build / harness 改動:**

- 6 個 .bat 各 +1 行加 `StressField.cpp`(build / build_cli / build_capi / build_capi_v2 /
  build_linear_audit / build_sn_cuda)
- `Scripts/run_gate.ps1` `$ExpectedUeTests 59 → 60`(+1 for `FFrameCoreStressFieldTest`)
- `Scripts/run_gpu_gate.ps1` `FRAMECORE_EXPECTED_ENGINE_VER '3.0.1' → '3.1.0'`
- `.github/workflows/release-gate.yml` 同步 `FRAMECORE_EXPECTED_ENGINE_VER` + Leg 1 step
  name 改 "F1..F70"
- `FrameSolver.uplugin` `Version 28 → 29, VersionName "3.0.1" → "3.1.0"`
- `Tools/v2_roundtrip.py` 加 `inspect.stress_field` shape fixture + 2 range guards
- `.gitignore` 加 `output/`, `_audit*.log`, `_scratch*.log` 防止 scratch 誤入

**整入了哪些先前 deferred:** 無(v3.0.1 deferred list 整份 carry-over 到 v3.1.x — 詳見
§3)。

**v3.1.0 沒動:** LevelSim(自 v2.2+1 以來不曾動)、CUDA lane 引擎源(`SnSession` cuDSS
路徑與 v3.0.1 bit-identical)、UE5 視覺層消費端(StressField 是引擎 producer,renderer 是
consumer-side 不在 v3.1.0 scope)。

## 2. 怎麼跑(主要 reproduce paths)

### 五腿基本 gate(秒~分鐘級)
```powershell
# 一鍵全綠驗證(standalone + UE 60 + OpenSees + audit + CLI):
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees

# 最快單腿(秒級):F1..F70 standalone analytic / benchmark
Plugins\FrameSolver\Standalone\build.bat
Plugins\FrameSolver\Standalone\frametest.exe
```

### v2 dispatcher round-trip(CPU)— 驗 inspect.stress_field
```bat
:: 建 v2 DLL + 跑 round-trip (含新 inspect.stress_field 11 個 check)
Plugins\FrameSolver\Standalone\build_capi_v2.bat
set FRAMECORE_EXPECTED_ENGINE_VER=3.1.0
python Tools\v2_roundtrip.py
:: 期望: "=== summary: ALL PASS ==="
```

### GPU + r2_bench 90k 互動 perf(opt-in, 需 cuDSS DLL)
```powershell
Scripts\run_gpu_gate.ps1 -Strict
:: 內含 frametest_cuda F1..F67 + F67s strict + v2_roundtrip CUDA + r2_bench --gpu 90k
:: 自動 set FRAMECORE_GPU_STRICT=1 並驗 STRICT_EXECUTED fingerprint
```

### 環境前置(fresh clone)

- **MSVC 18 (Community/Preview)** 在預期路徑或 vswhere 找得到
- **Eigen 3.4.0** — 自動從 UE_5.7 sibling tree 取(或 `set EIGEN_DIR=...`)
- **UE 5.7** 在 `E:\project\UE_5.7\` 或 `$env:UE_ENGINE_ROOT` override
- **conda env `framecore-direct`** 含 OpenBLAS / METIS / openseespy(`environment.yml` 重現)
- **`SUPERNODAL_CONDA`** override env-var:接受 env-root 或 `\Library`-suffixed,build_*.bat
  與 `FrameCore.Build.cs` 全部 normalize 一致(v3.0.1 audit fix)
- **`FRAMECORE_EXPECTED_ENGINE_VER='3.1.0'`** — v2_roundtrip 自動 pin

## 3. Deferred items + first-action-on-day-1

### 3.1 v3.0.1 carry-forward(原 v2.11.1 / v3.0.0 audit IDs)

1. **A-02 CUDA RAII guards** — first action: 建 `Plugins/FrameSolver/Source/FrameCore/Private/CudaRaii.h`
   含 `CuStreamGuard`/`CuMallocGuard<T>`/`CuDssHandleGuard`;替換 `SnSession.cpp Impl` 內裸指
   標 + null-check;`build.bat` 重建;`run_gate.ps1 -RequireOpenSees` 確認無 regression。

2. **A-05/F-14 OpenMP thread heuristic** — first action: 編 `SnSession.cpp` 改 `std::min(8, ...)`
   先尊重 `opts.numThreads > 0`,fallback `hardware_concurrency/2`;跑 F55/F56/F62/F63
   supernodal fixtures。

3. **A-12/D-2 cuDSS PHASE_REFACTORIZATION P-Delta revisit** — first action: 設計非均勻軸力
   pattern 的 P-Delta fixture,測 PHASE_REFACTORIZATION 容差;documentation 在
   `docs/PROGRESS_R_supernodal.md`。

4. **C-01 Pinned host memory** — first action: 在 `SnSession.cpp Impl` 加 4 個
   `cudaMallocHost` buffer(`spmvU_h`/`spmvF_h`/`spmvR_h`/`reactionBuf_h`)替換 Phase-2
   async path 的 `cudaMemcpy`。

5. **C-06 UDL + parallelRhs+GPU fixtures** — first action: `main.cpp` F67 之後加 F67b cantilever
   + `MemberUDL`,assert GPU reactions == CPU reactions rel<1e-9。

6. **C-07 DynamicCollapse GPU limitation doc** — first action: `SnSession.h` 在 class 上加
   comment block 釐清 "60 fps @ 200K DOF 是 static re-solve,DynamicCollapse 不走 SnSession,
   GPU speedup 不適用 collapse lane"。

7. **D-02/D-03 UE bCudaEnabled + packaging recipe** — first action:
   `FrameSolver.uplugin` 加 `"PluginSettings": { "bCudaEnabled": true }`;Build.cs 讀並 gate
   `FRAMECORE_CUDA`;確認 non-CUDA UE build 仍編譯。

8. **D-08 gpuRelInf rename** — first action: rename `gpuRelInf` 為 `GpuBacksubRelInf` 避免
   unity-build collision。

9. **D-10/D-11/F-16 mat67 ctor + std namespace + bat 重複** — first action: D-11 萃取
   `Standalone/_cuda_env.bat` 共用 `:derive_cuda_root`;D-10 修 mat67 constructor;F-16 補
   `std::` prefix。

10. **E-07/E-08 NIGHT_SHIFT docs** — first action: 把 `docs/NIGHT_SHIFT_*.md` 移到
    `Research/` 或加 header "Engineering scratch — not peer-reviewed"。

11. **F-02/F-03/F-04/F-10 Hoist per-frame SnSession allocations** — first action: `SnSession.cpp`
    Impl 把 `spmvU_h`/`spmvF_h`/`spmvR_h` 改成 persistent members;確認 r2_bench --gpu 90k
    margin 改善或不退步。

12. **F-08 Hoist nodeIndex cache** — first action: `FrameModel.h` 暴露 `nodeIndex(int id) const`
    public method,內部 lazy unordered_map<int,int>;替換所有 `std::find_if`;F1..F70 ALL PASS
    確認。

13. **D-06 r2_bench `--baseline` flag** — first action: `Research/R2_realtime_150k/r2_bench.cpp`
    加 `--baseline <ms>` CLI flag;`run_gpu_gate.ps1` 傳 `--baseline 4.56`。

### 3.2 v3.1.0 newly deferred

14. **A-13 F71 cantilever +Z load(`My(x)` formula 沒 fixture 蓋)** — first action:
    `main.cpp` 在 F70 後加 F71 block,`fixtures::cantileverTipLoad` 改 +Z load,assert
    `analytic My(x) = P·(L-x)` 11-sample worst rel<1e-9。

15. **D-05 CLI roundtrip `inspect.stress_field`** — first action: 決定 v1 CLI 是否加
    `STRESS` token。若加,`frame_cli_core.cpp` 增 token 處理,`cli_roundtrip.py` 加 check;
    若不加,在 `docs/specs/S11_stress_field.md` 補 "v1 CLI 不暴露 stress_field;只 v2
    dispatcher 暴露"。

16. **E-07 CLI_PROTOCOL.md v2 inspect.* doc** — first action: 建
    `docs/specs/S6b_v2_inspect_protocol.md` 把所有 v2 inspect.* JSON schemas 集中描述,
    含 `inspect.stress_field`;README docs map 加連結。

17. **E-13 S11 / MITC9i 命名衝突** — first action: 改名 stress-field spec 為
    `S11a_stress_field.md` 並 update README + legacy docs;或 MITC9i 改 S12 並 update 5 個
    legacy 引用。決定後 grep `S11` 全 repo 同步。

18. **C-12 `submitMtx_` 在 computeStressField 內 long-hold** — first action: 加 cancel
    poll hook 進 `computeStressField`(mirroring `HandleDynCollapse`);`Dispatcher.h` 加
    `isCancelled` callback param 或 thread-local 機制。

19. **F-02 `findUdl` O(N·M)** — first action: 把 `findUdl` lambda 改用一個
    `unordered_map<MemberId, Vec3>`,在 sweep 前 build 一次。實測 r2_bench 90k 看是否有 perf
    改善(目前 M ≪ N 應該無感)。

20. **F-03 StressKernel 1e-12 clamps** — first action: 在 `StressKernel.h` 開頭加 invariant
    block "all entries assumed validated; clamps are last-resort backstop";或 v3.2 直接
    刪 clamps + 加 `assert`。

## 4. 過程留下的教訓(durable, v3.1.0-specific)

1. **`StressKernel.h` 共用 header 模式** — 把同一公式同時被 ElasticAllowable D/C screen 與
   StressField visualisation 兩條 path 用,單一 source of truth header 是正確 pattern。F70
   bit-exact interlock 是「兩路徑不可 drift」的 oracle。未來的 C6 BMD/SFD data line / C7
   utilization field 都該套用同樣模式,呼叫 `internalForcesAtX` / `memberCornerSigmaMax`
   而不要重寫。

2. **「transitively OpenSees-verified」是合法 defer 姿態 — 但要明示** — F71(direct sigma_xx
   on OSPy) 故意不加是因為 `Nxx`/`Mxx` 已被現有 shell milestone fixtures 蓋,而 `Nxx/t ±
   6Mxx/t²` 是 textbook 恆等式,F70 bit-exact 接到 ElasticAllowable 自動繼承 OSPy 驗證鏈。
   這套推理在 v3.1.0 deferred list 寫清楚;類似邏輯下次 release 用要明 link 證據鏈,別
   只說 "transitively"。

3. **Sentinel `-1` 對「無 governing element」的 schema 明文** — `governingMemberId: 0` 跟真
   element ID 0 ambiguous(C-07/C-08)。改 sentinel `-1` 是 protocol-level lie 不再發生;
   未來新 inspect.* 加 `governingXxxId` 字段都該用 `-1` sentinel 並在 spec 明示。

4. **Audit BLOCKER `findUdl` 第一筆 match — solver 改變慢於 post-process** — 引擎 solver
   aggregates 所有 UDL 給 nodal equivalents,但 post-process `findUdl` 只取第一筆,silent
   divergence。未來任何 post-process 跟 solver 共用 `MemberUDL` / `NodalLoad` / `ShellPressure`
   都該用 accumulator pattern,不是 first-match。

5. **`run_gate.ps1` 不 build UE — `$ExpectedUeTests` 是 build 沒漏編的最後一道防線** —
   v3.1.0 加 UE test 必須同步 bump `$ExpectedUeTests`(59 → 60),否則 gate `-ge` 計數
   guard 會檢出 short-fall。新 UE test 加完 commit 前必先 incremental rebuild 一次。

6. **`-Strict` GPU mode `FRAMECORE_GPU_STRICT=1` 對 `silent fallback` 是 hard** — v3.0.1
   landed 的 `STRICT_EXECUTED` fingerprint 對 v3.1.0 保持有效。v3.1.0 source delta 0 行 in
   CUDA path,所以 r2_bench 90k margin 應該保持 v2.11.0 baseline +11.94 ms,不會 regress
   到 +8 ms 警戒線。

## 5. 後續方向(無排序)

**最近 + 高價值:**

1. **可視化 data line C6/C7/C8(完整 BMD/SFD 渲染端)** — `computeStressField` 已 expose 全
   StressField POD;C6 BMD/SFD 沿桿渲染、C7 利用率場、C8 贅餘度等都是 renderer-side 消費者。
   引擎側無動作,UE5 spline mesh / Niagara / colour-band shader 是下一步主軸。

2. **UE5 視覺層 — 吃 `inspect.stress_field` JSON / 直接 link `computeStressField`** — D/C
   熱圖、stress cloud、fragment 倒塌回放(吃 `CollapseStep.u`)。`FrameCore.dll` 已 export
   `FRAMECORE_API computeStressField`,UE 模組可直接 call。不需引擎改動。

3. **F71 OpenSees sigma 直接 cross-check(close S11 最後 defer)** — `opensees_compare.py`
   加 sigma_xx column 比較。低成本但補完 S11 OSPy lane。

4. **`model.patch` schema(v2 dispatcher 最後一個 NOT_IMPLEMENTED 動詞)** — schema 確定 +
   wire B6 phase。

5. **MITC9i 高階殼(原本 S11 skeleton; 9 處引擎修改先決)** — 真的需要的話,disambiguation
   完成後(E-13)再啟動。

**v3.1.1 / v3.2 候選:**

- A-13 F71 +Z load fixture
- D-05 v1 CLI `STRESS` token 決定
- C-12 cancel poll in computeStressField
- F-02 findUdl 改 hash map
- F-03 clamps invariant doc

---

接手有問題:
- `docs/HANDOFF.md` → `docs/HANDOFF_v2.11.1.md` → 本檔 → `docs/RELEASE_v3.1.0.md`
- S11-specific 問題讀 [`docs/specs/S11_stress_field.md`](specs/S11_stress_field.md)
- Engine 全圖 [`docs/ARCHITECTURE.md`](ARCHITECTURE.md)
- 驗證 / 證據鏈 [`docs/VERIFICATION.md`](VERIFICATION.md)
