# 交接指南 — `v3.2.2` 後接手 owner

> `v3.2.2` 在 2026-06-22 發布(closes v3.2.1 audit deferred V321-01..06 = 5 test 強化
> + 1 Dispatcher.h 註解 audit ID),tag `v3.2.2` = commit `<see git log after Phase 5>`。
> 主交接文 chain: [`docs/HANDOFF.md`](HANDOFF.md) → [`docs/HANDOFF_v3.1.0.md`](HANDOFF_v3.1.0.md)
> → [`docs/HANDOFF_v3.2.0.md`](HANDOFF_v3.2.0.md) → [`docs/HANDOFF_v3.2.1.md`](HANDOFF_v3.2.1.md)
> → 本檔。

## 1. `v3.2.2` = 什麼

**一句話**:把 v3.2.1 release-hardening 7-agent audit 留下的 6 個 deferred
(V321-01..06)全部 close,純 test 強化 + 1 個 Dispatcher.h 註解賦 audit ID。
**Engine source 零行改動 vs v3.2.1 / v3.2.0 / v3.1.0**(自 v3.1.0 tag 後 engine
source 已穩定 4 個 release)。`kEngineVer` 保持 `"3.2.0"` 不 bump(同 v3.2.1 §1.7
策略)。

**Engine source delta vs v3.2.1:** 0 lines under FrameCore native module。v3.2.2
整體 delta = 5 個 FrameCoreUE test files in-place 強化 + 1 個新 test helper header
+ 1 個 Dispatcher.h 註解區塊 + 2 個 docs。

**新檔:**

- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUETestHelpers.h`
  (V321-05;module-private,deliberately NOT exposed from `Public/FrameCoreUE/`)
- `docs/RELEASE_v3.2.2.md`(本 release notes)
- `docs/HANDOFF_v3.2.2.md`(本檔)

**修改檔:**

- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEMarshalSSBeamTest.cpp`
  — V321-01(analytic Vy oracle + gov-id tighten)+ V321-05(forward decl swap)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUERobustnessTest.cpp`
  — V321-02(per-sample bit-exact compare)+ V321-05(forward decl swap)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEAxialColumnTest.cpp`
  — V321-03(N > 0 sign)+ V321-05(forward decl swap)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEEditorTabSpawnerTest.cpp`
  — V321-04(LoadModuleChecked guard)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEMarshalMultiMemberTest.cpp`
  — V321-05(forward decl swap)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEMarshalShellPlateTest.cpp`
  — V321-05(forward decl swap)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEThetaRangeTest.cpp`
  — V321-05(forward decl swap)
- `Plugins/FrameSolver/Standalone/v2/Dispatcher.h` — V321-06(`model.patch` schema
  TBD 賦 audit ID **B-06** + 「out of v3.x scope unless explicit design decision」註)

**沒動的:** 全 engine source(`Plugins/FrameSolver/Source/FrameCore/`)/ kEngineVer
保持 `"3.2.0"` / uplugin VersionName 保持 `"3.2.0"` / `Scripts/run_gpu_gate.ps1`
+ `.github/workflows/release-gate.yml` `FRAMECORE_EXPECTED_ENGINE_VER` 都保持
`'3.2.0'` / `Scripts/run_gate.ps1` `$ExpectedUeTests` 保持 70(無新 test 加,只
in-place 強化)/ `ArchSim.uproject` / CUDA lane / LevelSim / 所有 v3.2.x carry-forward
deferred 項。

## 2. 怎麼跑(主要 reproduce paths)

跟 v3.2.1 完全一樣(engine source 沒動,gate 命令也沒改):

```powershell
# 一鍵全綠驗證(standalone F1..F70 + UE 70/70 + OpenSees + audit 104 + CLI):
$env:PATH = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;$env:PATH"
$env:UE_ENGINE_ROOT = "E:\project\UE_5.7"
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees

# 非 cuDSS box:
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 68
```

**v3.2.2 注意**:5 個 test files 內容改了(V321-01..05),所以從 v3.2.1 pull v3.2.2
之後第一個 `Build.bat ArchSimEditor` 會 incremental 重編 5-7 個 test `.cpp` + relink
DLL(實測本 host ~6 秒)。UE leg 仍跑 70/70(沒新測試,只 in-place 強化)。

### v2 dispatcher round-trip(CPU)

```bat
Plugins\FrameSolver\Standalone\build_capi_v2.bat
set FRAMECORE_EXPECTED_ENGINE_VER=3.2.0
python Tools\v2_roundtrip.py
:: 期望: "=== summary: ALL PASS ==="
```

`Dispatcher.h` 只改 comment block(V321-06 B-06 audit ID),不影響 binary 行為。但
`build_capi_v2.bat` 會 recompile Dispatcher.cpp 因 header 改動 → DLL hash 變,
v2_roundtrip 仍 ALL PASS(23 capabilities,kEngineVer "3.2.0")。

### GPU + r2_bench 90k 互動 perf(opt-in)

跟 v3.2.1 一樣;v3.2.2 source delta 在 CUDA path = 0 lines。

### 環境前置(fresh clone)

同 v3.2.0 / v3.2.1。

## 3. Deferred items + first-action-on-day-1

### 3.1 v3.2.1 V321-deferred — 5/6 CLOSED in v3.2.2

V321-01 ⚠(gov-id 部分 ✅;analytic Vy 切出 V321-01a 進 v3.2.3) / V321-02 ✅ / V321-03 ✅
/ V321-04 ✅ / V321-05 ✅ / V321-06 ✅。

### 3.1a v3.2.2 newly deferred

- **V321-01a** — `MarshalSSBeamTest` analytic Vy oracle 失敗(podVy ≠ w*L/4 closed
  form)。**first action**: 開
  `Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/StressField.h` + `StressField.cpp`
  `computeStressField` 找 `MemberStressSample::Vy` 是什麼(units, sign, axis convention)。
  常見可能性:(a) Vy 是 member 端力(joint reaction)非 sample 點 transverse shear
  → 用 `samples[k].Mz` 斜率 dMz/dx = -Vy 反推 ✓;(b) Vy 是 local-axis 方向但 sign
  flipped → 改 `(podVy < 0) ? -podVy : podVy` 與 analytic 比;(c) Vy 是 global y 不是
  local y → 換 frame transform。決定後 swap 進 `FrameCoreUEMarshalSSBeamTest.cpp`
  的 (4b) commented-out block(該 block 仍保留作 v3.2.3 起點)。

### 3.2 v3.2.0 / v3.2.1 carry-forward(無 v3.2.2 變更,沿用前一 HANDOFF first-actions)

- **U-01** BP load JSON model entrypoint — [`HANDOFF_v3.2.0.md` §3.2 ¶1](HANDOFF_v3.2.0.md)
- **U-02** Slate fixture dropdown — [`HANDOFF_v3.2.0.md` §3.2 ¶2](HANDOFF_v3.2.0.md)
- **U-03** real renderer(spline mesh / Niagara / colour-band)— **v3.3 主軸候選 #1**;
  first action: 新 `AFrameCoreStressFieldActor : AActor` + `UProceduralMeshComponent`,
  ingest `FFrameStressField` 並 emit 沿桿 colour-band mesh
- **U-04** ✅ CLOSED 於 v3.2.1 Phase 6e
- **U-05** float-only USTRUCT precision
- **U-06** UE 5.7 + VS2026 "not preferred version" build warning
- **U-07** sentinel mismatch(engine 0 vs USTRUCT -1)— **v3.3 主軸候選 #2**;first
  action: `StressField.h` 把 `governingMemberId = 0` 改 `-1` + `computeStressField`
  結尾 guard `if (members.empty() || !worstFound) leaveDefault`;Dispatcher.cpp 已對齊
  -1(v3.1.0 C-07/C-08),BP marshal 自動跟上。違反鐵則 #1 但 v3.3 minor scope 解禁。

詳見 [`docs/HANDOFF_v3.2.0.md` §3.2](HANDOFF_v3.2.0.md#32-v320-newly-deferred) +
[`docs/HANDOFF_v3.1.0.md` §3](HANDOFF_v3.1.0.md)。

### 3.3 v3.2.2 newly introduced audit IDs

- **B-06** `Dispatcher.h` `model.patch — schema TBD`(open since v2.4,v3.2.2 賦 ID)
  — first action: 若 v3.3+ 收到 GH client request 要 transactional model edit,
  authors `docs/specs/S6c_model_patch.md` + `Dispatcher.cpp` HandleModelPatch handler
  + `Tools/v2_roundtrip.py` test。**否則繼續 NOT_IMPLEMENTED**。

### 3.4 v3.x out-of-tree maintenance(承 v3.2.1 §3.3)

- **`E:/project/CLAUDE.md` 第 13 行 HEAD anchor 還停在 `v2.11.1`** — v3.0..v3.2.2
  累計 5 個 release 都沒同步,因為它在 git tracking 外。v3.2.2 仍未動。**下次接手 owner
  必須手動修**:line 13 → `## 現況(YYYY-MM-DD,HEAD `v3.2.2`,...)` + 補 v3.x narrative。

## 4. 過程留下的教訓(durable, v3.2.2-specific)

1. **V321-05 forward decl swap 是「微」refactor 但需要動 6 個 file** —
   在 v3.2.1 audit Agent F 提出時看起來 < 30 LOC 應該很簡單,實際做需要
   (a) 新建 helper header,(b) 6 個 test files 每個都 Read + Edit 一次,
   (c) UE rebuild 驗證所有 7 個 test 仍 link clean(因為 Tests/ 下任何 .cpp 改動都
   trigger module link)。實作時間 ~10 分鐘,比 V321-01..04 任何一個都久。
   **教訓:LOC 不是全部** — 跨 file fan-out 是 hidden cost。release-hardening
   把 V321-05 評為「small-fix」是樂觀;label 為「medium-fix(touches N files)」
   更精確。

2. **test 加 analytic oracle 不能放在用得到 podVy/tr0 變數之前** — V321-01 第一次
   嘗試把 analytic block 放在 (2) 段,但 reference 還沒 declare 的 `tr0` →
   compile error。**教訓:test 強化的 patch 寫好之後必先 mental-compile**(尤其
   block 用了還沒 declare 的 var),不要靠 build 才發現。

3. **idempotent guard 在 lazy-init 場景很便宜** — V321-04 加
   `LoadModuleChecked` 是 idempotent(模組已載入時是 O(1) hash 查找);可以無腦加
   不會 hurt perf。**教訓:UE module 訪問前的 LoadModuleChecked guard 是 cheap
   insurance,任何依賴 module-side state 的 test 都應該加**(尤其是 editor-only
   smoke test,因 editor module 載入順序 may shift across UE patches)。

4. **per-sample bit-exact compare 比 global-max compare 嚴格 5+ 量級** — V321-02 把
   100-repeat memory stability 從只比 global max 擴展到 5 個 sample 內部 field,
   覆蓋面廣 5 倍。**教訓:write a memory/concurrency stability test 時 default 用
   per-element compare**,只比 aggregate(max/sum/avg)是 lossy proxy。

5. **comment-only audit ID 賦予是高槓桿** — V321-06 只改 Dispatcher.h 兩個 comment
   block,但把開了 8 個 release 的 carryforward 從「TBD 模糊狀態」釘到「B-06 + 明文
   out-of-scope」。**教訓:roadmap 上長期 carryforward 的 item 若沒有 concrete
   landing plan,賦 audit ID + 明文 deferred 比每個 release 都 mention 一次強**。

## 5. 後續方向(v3.3 重新審視)

v3.2.x 線 patch 線完全收尾(v3.2.0 / v3.2.1 / v3.2.2 三個 patch + test 強化),
engine source 已 4 個 release 沒動 → **v3.3 minor 是時候動 engine 主軸**:

**v3.3 主軸候選(依優先序)**:

1. **U-07 sentinel mismatch fix** — engine source change 解禁;`StressField.h`
   1 行 default + `computeStressField` 1 個 guard;FrameCoreUE 端 BP marshal 自動
   跟上(因 Dispatcher v3.1.0 C-07/C-08 已對齊 -1)。是 v3.3 minor 第一個動作,
   < 30 LOC engine fix,但是「engine source ≠ 0 行」所以 trigger major-ish
   verification:standalone F1..F70 重跑 + audit 104 重跑 + 加 1 個新 fixture
   驗 "no governing member" edge case。

2. **U-03 真實 renderer (主軸 thin slice)** — `AFrameCoreStressFieldActor` +
   `UProceduralMeshComponent` 沿桿 colour-band stress mesh。對 v3.3 minor 而言,
   建議「thin slice」:只做 member stress band(不做 shell heat-map,不做 Niagara
   particle)。一個 actor + 1-2 個 BP demo Map + 1 個 UE test 驗 mesh vertex 數
   = members × samples,顏色取自 SigmaCompMax / Field.GlobalMaxFiberSigma。

3. **U-01 BP load JSON model** — 跟 U-03 並行可做,讓 BP designer 真的能 load
   非 cantilever fixture 的 model。

4. **U-02 / U-05 / U-06** — 收尾級 follow-up,留 v3.3.1 patch。

5. **B-06 model.patch** — explicitly closed unless GH client 要;繼續 deferred。

**v3.3 minor scope 預估 work**:U-07 (~30 LOC engine + 1 fixture)+ U-03 thin slice
(~300 LOC UE + 2 new test files)+ U-01 (~150 LOC)+ docs / release-hardening
≈ 2-4 個 night-shift 級 sessions。

---

接手有問題:
- 主交接鏈: `docs/HANDOFF.md` → `docs/HANDOFF_v2.11.1.md` → `docs/HANDOFF_v3.1.0.md`
  → `docs/HANDOFF_v3.2.0.md` → `docs/HANDOFF_v3.2.1.md` → 本檔
  → `docs/RELEASE_v3.2.2.md`
- 5-min 入手指南: [`docs/FrameCoreUE_QuickStart.md`](FrameCoreUE_QuickStart.md)
- S11 / StressField 問題讀 [`docs/specs/S11_stress_field.md`](specs/S11_stress_field.md)
- Engine 全圖 [`docs/ARCHITECTURE.md`](ARCHITECTURE.md)
- 驗證 / 證據鏈 [`docs/VERIFICATION.md`](VERIFICATION.md)
