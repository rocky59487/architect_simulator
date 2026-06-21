# 交接指南 — `v3.2.0` 後接手 owner

> `v3.2.0` 在 2026-06-22 night-shift autonomous run 發布(commit 在 Phase 5 tag 後填),
> tag `v3.2.0` = commit `<see git log after Phase 5>`。主交接文是
> [`docs/HANDOFF.md`](HANDOFF.md)(原始 v2.x history,不動);前一輪在
> [`docs/HANDOFF_v3.1.0.md`](HANDOFF_v3.1.0.md);本檔補上 v3.2.0 多出來的內容
> (FrameCoreUE thin-slice + 6 newly-deferred U-01..U-06)。

## 1. `v3.2.0` = 什麼

**一句話:** 在 v3.1.0 之上做 UE-side reflection,新增獨立 `FrameCoreUE` plugin module
host USTRUCT 鏡像 / UBlueprintFunctionLibrary / 一個 Slate editor utility panel。
**FrameCore native module(`Plugins/FrameSolver/Source/FrameCore/`)零行改動 — `git diff
v3.1.0..HEAD -- Plugins/FrameSolver/Source/FrameCore/` 是空的。**

**Engine source delta vs v3.1.0:** 0 lines under FrameCore native module。v3.2 整體 delta
= 10 新檔 + 4 處版本字串 bump(.uplugin / Dispatcher.h / run_gpu_gate.ps1 / release-gate.yml)
+ run_gate.ps1 `$ExpectedUeTests 60 -> 62`。

**新檔:**

- `Plugins/FrameSolver/Source/FrameCoreUE/FrameCoreUE.Build.cs`(新 module Build.cs;
  depend Core/CoreUObject/Engine/FrameCore;`bBuildEditor` 加 Slate/SlateCore/UnrealEd/
  EditorStyle/EditorSubsystem/ToolMenus/InputCore/WorkspaceMenuStructure)
- `Public/FrameCoreUE/FrameCoreUEModule.h` + `Private/FrameCoreUEModule.cpp`
  (`FFrameCoreUEModule : IModuleInterface`;`StartupModule` 在 `#if WITH_EDITOR` 內註冊
  nomad tab spawner)
- `Public/FrameCoreUE/FrameCoreUETypes.h`(5 USTRUCT(BlueprintType): FFrameStressFieldSample
  / FFrameMemberStressTrace / FFrameShellStressPoint / FFrameShellStressLayer /
  FFrameStressField;全部 `BlueprintReadOnly`;`-1` sentinel for governing IDs)
- `Private/FrameCoreUETypes.cpp`(`FrameCoreUE::ToBlueprint(frame::StressField)` marshal
  helper;double->float lossy cast)
- `Public/FrameCoreUE/FrameCoreUELibrary.h` + `Private/FrameCoreUELibrary.cpp`
  (`UFrameCoreStressFieldLibrary` UBlueprintFunctionLibrary;`ComputeCantileverFixture` BP demo
  entry + 5 BP pure accessors)
- `Public/FrameCoreUE/SFrameCoreStressFieldPanel.h` + `Private/SFrameCoreStressFieldPanel.cpp`
  (`#if WITH_EDITOR` Slate `SCompoundWidget`;Compute button + result text + sample
  `SListView`)
- `Private/Tests/FrameCoreUEBlueprintSmokeTest.cpp`(UE 61st test;`FrameCore.UE.BlueprintSmokeTest`)
- `Private/Tests/FrameCoreUEEditorSmokeTest.cpp`(UE 62nd test;`#if WITH_EDITOR`;
  `FrameCore.UE.EditorSmokeTest`)

**修改檔:**

- `Plugins/FrameSolver/FrameSolver.uplugin` Version 29 → 30 + VersionName "3.1.0" → "3.2.0"
  + Modules[] 第二 entry `"Name": "FrameCoreUE"`
- `Plugins/FrameSolver/Standalone/v2/Dispatcher.h` kEngineVer "3.1.0" → "3.2.0"
- `Scripts/run_gate.ps1` `[int]$ExpectedUeTests = 60 → 62`
- `Scripts/run_gpu_gate.ps1` `FRAMECORE_EXPECTED_ENGINE_VER '3.1.0' → '3.2.0'`
- `.github/workflows/release-gate.yml` `FRAMECORE_EXPECTED_ENGINE_VER` 同步 + Leg 1 step
  name 改

**沒動的:** `Plugins/FrameSolver/Source/FrameCore/`(全引擎源)/ `ArchSim.uproject`(因為
plugin-level enable 早已存在,plugin's modules 是 .uplugin schema 不是 .uproject schema)/
CUDA lane / LevelSim / 所有 v3.1.0 deferred 項。

## 2. 怎麼跑(主要 reproduce paths)

### 五腿基本 gate(秒~分鐘級)

```powershell
# 一鍵全綠驗證(standalone F1..F70 + UE 62/62 + OpenSees + audit 104 + CLI):
$env:PATH = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;$env:PATH"
$env:UE_ENGINE_ROOT = "E:\project\UE_5.7"
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees

# 最快單腿(秒級):F1..F70 standalone analytic / benchmark
Plugins\FrameSolver\Standalone\build.bat
Plugins\FrameSolver\Standalone\frametest.exe
```

**踩雷:** `framecore-direct` conda env **不含 python.exe**(libs-only env:OpenBLAS /
METIS / cuDSS)。OpenSees leg 用 system Python(Windows Store 3.12)裡的 openseespy。
不要 `conda activate framecore-direct` — 那只 prepend PATH 而已,等同上面的單行 `$env:PATH`
prepend。

### UE module incremental rebuild

```bat
E:\project\UE_5.7\Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development ^
    -project="E:\project\ArchSim\ArchSim.uproject" -waitmutex
```

新增 .cpp 或 .h 在 `Plugins/FrameSolver/Source/FrameCoreUE/` 下,UBT 自動 detect。注意
**`run_gate.ps1` 不會 build UE**,改 UE source 後必須先跑上面 Build.bat,否則 UE leg 跑舊
binary(`$ExpectedUeTests` 短少會偵測到)。

### v2 dispatcher round-trip(CPU)

```bat
:: 建 v2 DLL + 跑 round-trip(含 inspect.stress_field 11 個 check)
Plugins\FrameSolver\Standalone\build_capi_v2.bat
set FRAMECORE_EXPECTED_ENGINE_VER=3.2.0
python Tools\v2_roundtrip.py
:: 期望: "=== summary: ALL PASS ==="
```

### GPU + r2_bench 90k 互動 perf(opt-in, 需 cuDSS DLL)

```powershell
Scripts\run_gpu_gate.ps1 -Strict
:: 內含 frametest_cuda F1..F67 + F67s strict + v2_roundtrip CUDA + r2_bench --gpu 90k
:: 自動 set FRAMECORE_GPU_STRICT=1 並驗 STRICT_EXECUTED fingerprint
:: v3.2.0 source delta 在 CUDA path = 0 lines, 數字 vs v3.0.0/v3.1.0 不變
```

### 環境前置(fresh clone)

跟 v3.1.0 一樣:MSVC 18 (Community/Preview) / Eigen 3.4.0 / UE 5.7 / conda env
`framecore-direct`(`environment.yml`)/ `SUPERNODAL_CONDA` env-var override /
`FRAMECORE_EXPECTED_ENGINE_VER='3.2.0'` v2_roundtrip pin。

## 3. Deferred items + first-action-on-day-1

### 3.1 v3.1.0 carry-forward(原 v2.11.1 / v3.0.0 audit IDs + v3.1.0 newly)

所有 v3.1.0 carry-forwards 持續到 v3.2.x / v3.3:A-02 CUDA RAII, A-05/F-14 OpenMP heuristic,
A-12/D-2 cuDSS PHASE_REFACTORIZATION P-Delta revisit, C-01 pinned host memory, C-06 UDL +
parallelRhs+GPU fixtures, C-07 DynamicCollapse GPU limitation doc, D-02/D-03 UE
bCudaEnabled flag + packaging recipe, D-08 gpuRelInf rename, D-10/D-11/F-16, E-07/E-08
docs scratch, F-02/F-03/F-04/F-10, F-08 nodeIndex cache, D-06 r2_bench `--baseline` flag。
v3.1.0 newly: A-13 F71 +Z, D-05 v1 CLI STRESS, E-07 v2 inspect protocol spec, E-13 S11
naming, C-12 cancel poll, F-02 findUdl hash map, F-03 clamps invariant doc。

詳見 [`docs/HANDOFF_v3.1.0.md` §3](HANDOFF_v3.1.0.md#3-deferred-items--first-action-on-day-1)
每項 first-action 草圖。

### 3.2 v3.2.0 newly deferred

1. **U-01 BP "load JSON model" entrypoint** — 目前只有 ComputeCantileverFixture。 first
   action: 在 `UFrameCoreStressFieldLibrary` 加 `UFUNCTION(BlueprintCallable) static
   FFrameStressField ComputeFromJsonString(const FString& JsonText, int32 SamplesPerSpan = 11)`;
   parse JSON via v1 CLI text format 或 v2 wire JSON 二擇一(建議走 v2 wire JSON,跟
   `inspect.stress_field` 形式一致),呼 frame::computeStressField。

2. **U-02 Slate panel fixture dropdown** — plan §4 Phase 3 提過,實作 minimum viable
   path 沒做。first action: panel 加 `SComboBox<TSharedPtr<FString>>` next to Compute
   button;options = "Cantilever (F68)" / "Plate (F69)" / "Truss" / "Cross";dispatch
   on selection。

3. **U-03 真實 renderer(spline mesh / Niagara / colour-band)** — v3.3 主軸。first
   action: 新 `AFrameCoreStressFieldActor : AActor` + `UProceduralMeshComponent`,
   ingest `FFrameStressField` 並 emit 沿桿 colour-band mesh(根據 `Samples[i].SigmaCompMax`
   normalised to `Field.GlobalMaxFiberSigma`)。

4. **U-04 nomad tab spawner sanity-check** — **✅ CLOSED in Phase 6e (commit pending)**:
   added `FFrameCoreUEEditorTabSpawnerTest` in
   `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEEditorTabSpawnerTest.cpp`
   which asserts `FGlobalTabmanager::Get()->HasTabSpawner("FrameCoreStressFieldPanel")` is
   true after `StartupModule`. UE 5.7 build today: PASS. If WorkspaceMenuStructure ever
   moves API in a future UE version, this test will fail instead of the menu silently
   disappearing.

5. **U-05 float-only USTRUCT 精度** — 對 P << 1 N 場景失精。first action: 決定是否
   加 `FFrameStressFieldSampleD`(double 版本);或在 RELEASE notes 明示 lossy budget
   1e-4 visualisation only。

6. **U-06 VS2026 not preferred version 警告** — 從 v3.1.0 build 帶過來。Build.bat
   一行 warning 沒 fail。first action: 在 `Setup.bat` pin 偏好版本 14.44.35207
   或 sanitize warning(non-blocking)。

7. **U-07 `governingMemberId` / `governingShellId` sentinel 不一致(engine 0 vs USTRUCT -1)** —
   3-agent audit A-1 BLOCKER。引擎 POD `frame::StressField` 用 `0` 表示「無 governing
   element」(`StressField.h` L78-79: `int governingMemberId = 0; // 0 if no member governs`),
   但 USTRUCT `FFrameStressField` 預設用 `-1`(對齊 v3.1.0 C-07/C-08 audit pattern)。
   `FrameCoreUE::ToBlueprint` 是直通拷貝,所以 USTRUCT 在「no governing」場景下會帶 0
   值(被 BP 消費者誤解為「member id 0 是 worst」)。 對 v3.2 cantilever fixture 因為
   member id 0 真的 governs,smoke test 通過是「碰巧對」。v3.2 沒修因為 fix 需要動
   `Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/StressField.h`(engine source,違反
   鐵則 #1);marshal-layer 啟發式檢測(`globalMaxFiberSigma == 0` 譯成 -1)易脆。
   first action:在 `StressField.h` 把 `governingMemberId = 0` 改成 `-1`,並在
   `StressField.cpp computeStressField` 結尾若 worst 未更新就保留 -1(目前 `idxWorst`
   永遠會被設成 0 if any member exists,需顯式 guard `if (!members.empty() && worstFound)`);
   Dispatcher.cpp `packStressField` 已對齊 -1(v3.1.0 C-07/C-08 audit),所以 JSON path
   不變;BP path 自動跟上。同時 `FFrameCoreUEBlueprintSmokeTest` 已 v3.2 改成 robust
   assertion(`GoverningMemberId >= 0`)— 加完 -1 sentinel 後也安全。

## 4. 過程留下的教訓(durable, v3.2.0-specific)

1. **`framecore-direct` conda env 不是 Python env** — 它是 build-time native libs-only env
   (OpenBLAS / METIS / cuDSS)。`conda activate` 之後 `python` 還是用 system Python
   (Windows Store 3.12)。OpenSees leg 用的是 system Python 裡的 openseespy。Phase 0 我
   一開始嘗試 `& "$envRoot\python.exe"` 失敗才注意到 env 沒裝 python — 對接手 owner 而
   言這是「啟動環境只需 PATH prepend $env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin
   就夠了」的 corollary,不需要 `conda activate`。

2. **新 module 第一次 build 後,UE 自動會在每次新 .cpp 加入時 trigger adaptive non-unity** —
   `[Adaptive Build] Excluded from FrameCoreUE unity file: ...` 是好現象(踩雷 #4)。
   新 .cpp 自己 TU 編譯,匿名 namespace 不衝突。但小心:大量新增 .cpp 時 unity 變得很小、
   非 unity TU 數量多,連結變慢。`Plugins/FrameSolver/Source/FrameCoreUE/` 目前 8 個
   .cpp / 對 FrameCore 27 個 .cpp 比起來還小,無感。但 v3.3 加 renderer / actor / component
   會多很多 .cpp,屆時要監控 build time。

3. **USTRUCT `BlueprintReadOnly` 是 marshal 設計,別誤加 BlueprintReadWrite** — `ToBlueprint`
   是 one-way 結果生產;BP 改寫 USTRUCT 內容並不會反向修改 engine state。若未來 v3.3
   要 BP designer 改 model 再呼叫 `ComputeStressField`,加新的 input USTRUCT (e.g.
   `FFrameModelLoadOptions`) 不是把 result USTRUCT 變 BlueprintReadWrite。

4. **`-1` sentinel for governing IDs 跟 v3.1.0 C-07/C-08 一致** — `0` 跟 real ID 0
   ambiguous。任何未來 inspect.* USTRUCT 加 governingXxxId 都用 `-1`,並在 spec 明示。

5. **lossy double->float USTRUCT 是 trade-off,測試 budget 要記 1e-5 而非 1e-9** —
   `FFrameCoreUEBlueprintSmokeTest` oracle (a) rel<1e-5 是因為唯一 divergence path 是
   engine-side double → USTRUCT float 的 cast;oracle (b) 1e-4 因為加上 analytic 的
   roundoff。比 standalone F68 / UE FFrameCoreStressFieldTest 的 1e-9 / 1e-12 寬鬆 4-7
   個 order,別誤以為 BP test 失敗代表 engine numerics 退步;先看 standalone F68 還綠
   不綠。

6. **`run_gate.ps1` 不 build UE — `$ExpectedUeTests` 是 build 沒漏編的最後一道防線** —
   v3.2.0 加 2 個 UE test → bump `$ExpectedUeTests` 60→62;同 v3.1.0 鐵則 #4 提醒
   (5)。新 UE test 加完 commit 前必先 incremental rebuild 一次,否則 gate `-ge` count
   guard 會 short-fall。

## 5. 後續方向(無排序)

**最近 + 高價值:**

1. **真實 renderer(U-03)** — v3.2 出 USTRUCT data,v3.3 出 visual。
   `AFrameCoreStressFieldActor` + `UProceduralMeshComponent` 沿桿 colour-band 是最低
   風險;然後 Niagara particle stress cloud / shell heat-map 等等。

2. **BP "load JSON model"(U-01)** — production code path。v3.2 ComputeCantileverFixture
   是 demo;真實使用者要 load BIM JSON / Rhino transport JSON。

3. **F71 OpenSees sigma 直接 cross-check(close S11 最後 defer)** — `opensees_compare.py`
   加 sigma_xx column。

4. **`model.patch` schema(v2 dispatcher 最後一個 NOT_IMPLEMENTED 動詞)** — 跟 v3.1.0
   一樣 carry-forward。

**v3.2.1 / v3.3 候選:**

- U-01..U-06 全部(BP load JSON / Slate fixture dropdown / renderer / spawner sanity / 
  double USTRUCT / VS toolchain pin)
- F71 +Z, D-05 v1 CLI STRESS, E-07 v2 inspect protocol spec, E-13 S11 naming 解決
- v3.1.0 deferred 整批還沒動的

---

接手有問題:
- `docs/HANDOFF.md` → `docs/HANDOFF_v2.11.1.md` → `docs/HANDOFF_v3.1.0.md` → 本檔 →
  `docs/RELEASE_v3.2.0.md`
- S11 / StressField 問題讀 [`docs/specs/S11_stress_field.md`](specs/S11_stress_field.md)
- v3.2 UE 介面 plan 讀 [`docs/PLAN_v3.2_ue_interface.md`](PLAN_v3.2_ue_interface.md)
- 夜班 log [`docs/NIGHT_SHIFT_2026-06-22.md`](NIGHT_SHIFT_2026-06-22.md)
- Engine 全圖 [`docs/ARCHITECTURE.md`](ARCHITECTURE.md)
- 驗證 / 證據鏈 [`docs/VERIFICATION.md`](VERIFICATION.md)
