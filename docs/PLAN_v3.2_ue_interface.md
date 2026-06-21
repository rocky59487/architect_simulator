# PLAN v3.2 — UE 介面 thin slice + direct link to FrameCore.dll

> **Status:** drafted 2026-06-21 night session, awaiting user sign-off.
> **Base tag:** `v3.1.0` (`5da6f56`) + 1 ci patch (`0f32648`). Work tree clean on `main`.
> **Owner:** Claude (night-shift autonomous run after sign-off).
> **Sign-off rule:** anything not pre-authorised in §3 → defer to `docs/NIGHT_SHIFT_2026-06-22.md`,
> never unilaterally push to `main`.

---

## 1. 範圍

**做什麼:** v3.2 = UE5 視覺層 consumer-side thin slice。新增 `FrameCoreUE` plugin
module(reflection layer)+ 1 個 Blueprint node + 1 個 Slate editor utility panel,
direct-link `FrameCore.dll`(消費 `FRAMECORE_API computeStressField`)。

**不做什麼:**
- 引擎側鐵則 #1 不破 — `FrameCore` native module 維持純 C++17 + POD/std public API。
  本 plan **零行** 動到 `Plugins/FrameSolver/Source/FrameCore/`(包含 `FrameCore.Build.cs`)。
- Rhino bridge(out-of-process 另一條 transport)— 不在 scope。
- 引擎 public ABI / SOVERSION 變更 — 不在 scope。
- CUDA path / cuDSS / r2_bench — 不在 scope(v3.2 source delta 在 GPU path = 0 行)。
- 真實 UE5 renderer(spline mesh / Niagara / colour-band shader)— Phase 3 panel 只做數值表 +
  worst-element 標籤,renderer 進 v3.3。
- v3.1.0 deferred 全部(A-13/D-05/E-07/E-13/C-12/F-02/F-03 + v3.0.1 carry-forward 13 項)。
  每項 HANDOFF_v3.1.0 §3 已有 first-action,留給 v3.3 或單獨 ticket。

---

## 2. 軸答覆紀錄(2026-06-21 user)

- **A 軸:** Thin slice 三層都最薄(plugin shell + 1 BP node + 1 Slate panel)
- **B 軸:** Direct link `FrameCore.dll`(in-process POD marshal,零 JSON overhead)
- **C 軸:** 4/4 不可逆改動全勾(FrameCoreUE 新 module / FrameCore.Build.cs UE-only dep /
  ArchSim.uproject plugin module entry / .uplugin Version 29→30 + VersionName 3.1.0→3.2.0)

---

## 3. 不可逆改動 allow-list(user 已簽,plan 內可做;plan 外 defer)

| # | 改動 | C 軸對應 | Phase | 備註 |
|---|---|---|---|---|
| 1 | 新增 `Plugins/FrameSolver/Source/FrameCoreUE/`(reflection layer) | C-1 | Phase 1 | 獨立於 FrameCore native module |
| 2 | `FrameCoreUE.Build.cs` depend Core/CoreUObject/Engine/Slate/SlateCore/UnrealEd | C-2 | Phase 1 | **澄清:** FrameCore.Build.cs **不動**(維持鐵則 #1)。Build.cs 改動全在新檔 |
| 3 | `FrameSolver.uplugin` Modules 加第二 entry `"FrameCoreUE"` | C-3 | Phase 1 | **澄清:** `ArchSim.uproject` 不動(`FrameSolver` plugin 已 EnabledByDefault=false 但 uproject 已 explicit enable;plugin 內 modules 是 .uplugin schema,不是 .uproject schema) |
| 4 | `FrameSolver.uplugin` Version 29→30 + VersionName "3.1.0"→"3.2.0" | C-4 | Phase 5 | + `kEngineVer "3.1.0"→"3.2.0"`(Dispatcher.h)+ `FRAMECORE_EXPECTED_ENGINE_VER` env-pin + `$ExpectedUeTests 60→62` |

**Plan 外浮現的不可逆改動 → 一律 defer:**
- FrameCore public API 加新 entry / 改 signature
- v2 dispatcher schema / capability 改
- 新 module SOVERSION / 改 wire ABI(`kAbiVersion`)
- Eigen 升版 / conda env 新依賴
- 任何 `Plugins/FrameSolver/Source/FrameCore/` 內 .cpp/.h 改

→ 浮現就寫 `docs/NIGHT_SHIFT_2026-06-22.md` 等簽,**不 unilateral push main**。

---

## 4. Phase 安排

每 phase 標 **ROI / 風險 / hour budget / 可逆性 / PASS-NEGATIVE-DEFERRED threshold**。

### Phase 0 — pre-flight 五腿 gate(**可逆**,30 min,ROI 高)

**目的:** 確認 v3.1.0 base 在 integrator host 是綠的,避免 Phase 1+ 建在不穩基礎上。

**動作:**
```powershell
conda activate framecore-direct
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
```
- Standalone F1..F70 / UE 60/60 / OpenSees / audit 104 / CLI roundtrip 五腿
- `git status` clean
- log 全 reproduce 到 NIGHT_SHIFT 日誌

**Threshold:**
- ✅ PASS: 五腿全綠 + work tree clean → 進 Phase 1
- ❌ NEGATIVE: 任一腿掉 → halt,寫 NIGHT_SHIFT 抓 root cause,等 user 簽核才決定要 fix 還是 abort plan
- ⏭️ DEFERRED: N/A(Phase 0 是必須的)

**風險:** OpenSees env 在 conda 內可能未 activate → 預先 `conda activate framecore-direct` 確保 openseespy import 過。

---

### Phase 1 — FrameCoreUE module shell + USTRUCT marshal(**不可逆**,3-4 hr,ROI 高)

**目的:** 開新 reflection module,把 `frame::StressField` POD 用 USTRUCT mirror 暴露給 UE。Phase
2/3 全部依賴此。

**動作:**

1. 新檔結構:
   ```
   Plugins/FrameSolver/Source/FrameCoreUE/
   ├── FrameCoreUE.Build.cs
   ├── Public/
   │   └── FrameCoreUE/
   │       ├── FrameCoreUETypes.h          (USTRUCT mirror)
   │       ├── FrameCoreUELibrary.h        (UBlueprintFunctionLibrary)
   │       └── FrameCoreUEModule.h         (IModuleInterface)
   └── Private/
       ├── FrameCoreUEModule.cpp           (module load)
       ├── FrameCoreUETypes.cpp            (POD↔USTRUCT marshal helper, hidden)
       └── FrameCoreUELibrary.cpp          (BP entrypoints, stub for Phase 2)
   ```

2. USTRUCT mirror(Public/FrameCoreUE/FrameCoreUETypes.h):
   ```cpp
   USTRUCT(BlueprintType) struct ARCHSIM_API FFrameStressFieldSample {
       GENERATED_BODY()
       UPROPERTY(BlueprintReadOnly) float X = 0.f;
       UPROPERTY(BlueprintReadOnly) float SigmaCompMax = 0.f;
       UPROPERTY(BlueprintReadOnly) float SigmaTensMax = 0.f;
       UPROPERTY(BlueprintReadOnly) float TauShear     = 0.f;
       UPROPERTY(BlueprintReadOnly) float TauTorsion   = 0.f;
       UPROPERTY(BlueprintReadOnly) float N=0.f, Vy=0.f, Vz=0.f, T=0.f, My=0.f, Mz=0.f;
       UPROPERTY(BlueprintReadOnly) float SigmaTopY=0.f, SigmaBotY=0.f, SigmaPlusZ=0.f, SigmaMinusZ=0.f;
   };
   USTRUCT(BlueprintType) struct ARCHSIM_API FFrameMemberStressTrace { ... };
   USTRUCT(BlueprintType) struct ARCHSIM_API FFrameShellStressPoint  { ... };
   USTRUCT(BlueprintType) struct ARCHSIM_API FFrameShellStressLayer  { ... };
   USTRUCT(BlueprintType) struct ARCHSIM_API FFrameStressField {
       GENERATED_BODY()
       UPROPERTY(BlueprintReadOnly) TArray<FFrameMemberStressTrace> Members;
       UPROPERTY(BlueprintReadOnly) TArray<FFrameShellStressLayer>  ShellsTop;
       UPROPERTY(BlueprintReadOnly) TArray<FFrameShellStressLayer>  ShellsBot;
       UPROPERTY(BlueprintReadOnly) float GlobalMaxFiberSigma  = 0.f;
       UPROPERTY(BlueprintReadOnly) float GlobalMaxVonMises    = 0.f;
       UPROPERTY(BlueprintReadOnly) int32 GoverningMemberId    = -1;
       UPROPERTY(BlueprintReadOnly) int32 GoverningShellId     = -1;
       UPROPERTY(BlueprintReadOnly) int32 GoverningShellCorner = -1;
       UPROPERTY(BlueprintReadOnly) bool  GoverningShellLayerIsTop = true;
   };
   ```
   **注意:** `governingMemberId`/`governingShellId` 用 `-1` sentinel 跟 v3.1.0 C-07/C-08 對齊,別退回 `0`。

3. Marshal helper(Private/FrameCoreUETypes.cpp,non-Blueprint pure function):
   ```cpp
   namespace FrameCoreUE {
       FFrameStressField ToBlueprint(const frame::StressField& field);
       // POD → USTRUCT copy. real(double) → float lossy cast 是設計(BP designer 用 float)。
       // bit-identity 不可能保留,但 rel<1e-6 是 acceptable visualisation 等級。
   }
   ```
   **lossy cast 紀錄:** Phase 2 smoke test 驗 rel<1e-5(visualisation tolerance,not bit-exact)。
   若 designer 之後要更高精度可在 v3.3 加 double USTRUCT 版本。

4. `FrameCoreUE.Build.cs`:
   ```csharp
   PublicDependencyModuleNames.AddRange(new string[] {
       "Core", "CoreUObject", "Engine", "FrameCore"
   });
   PrivateDependencyModuleNames.AddRange(new string[] {
       // Slate/SlateCore/UnrealEd 留 Phase 3 #if WITH_EDITOR 加入,Phase 1 不加
   });
   PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
   CppStandard = CppStandardVersion.Cpp20;
   bUseUnity = true;
   // FRAMECORE_UE 已由 FrameCore 模組 PublicDefinitions 傳遞下來
   ```

5. `FrameSolver.uplugin` Modules 加:
   ```json
   {
       "Name": "FrameCoreUE",
       "Type": "Runtime",
       "LoadingPhase": "Default"
   }
   ```

6. incremental rebuild:
   ```bat
   E:\project\UE_5.7\Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development ^
       -project="E:\project\ArchSim\ArchSim.uproject" -waitmutex
   ```

**Threshold:**
- ✅ PASS: UE compile clean + `Plugins/FrameSolver/Binaries/Win64/UnrealEditor-FrameCoreUE.dll` 存在 +
  Live-coding `IsModuleLoaded("FrameCoreUE")` 為真
- ❌ NEGATIVE: compile error → 看是 reflection schema(UHT 失敗,通常 USTRUCT 語法問題)還是 dep
  graph(missing module dependency)→ in-place 修;若 30 min 內修不掉 halt 寫 NIGHT_SHIFT
- ⏭️ DEFERRED: 無(Phase 1 是 Phase 2/3 必要)

**風險:**
- UHT reflection 卡:踩雷 #2(`IN`/`OUT` 巨集)注意 USTRUCT field 命名;`UE_LOG` 開夠看 UHT 輸出
- unity-build conflict:踩雷 #4 提醒,FrameCoreUE 是 fresh module 第一次編,風險低
- LoadingPhase `Default` 可能讓 module 在 `CoreUObject` 還沒 ready 前 load → 若見 startup crash 切
  `PostDefault`

---

### Phase 2 — Blueprint node + BP smoke test(**可逆**,1-2 hr,ROI 中)

**目的:** 暴露 BP API + 寫一個 BP-flavoured smoke test 證 USTRUCT marshal 沒丟失含義。

**動作:**

1. `UFrameCoreStressFieldLibrary : UBlueprintFunctionLibrary`(Public/FrameCoreUE/FrameCoreUELibrary.h):
   ```cpp
   UCLASS()
   class ARCHSIM_API UFrameCoreStressFieldLibrary : public UBlueprintFunctionLibrary {
       GENERATED_BODY()
   public:
       // Build a cantilever fixture in-memory and compute its stress field.
       // 這個 entrypoint 是 BP 範例用;真實 production 路徑 v3.3 加 "load JSON model" path。
       UFUNCTION(BlueprintCallable, Category="FrameCore|StressField")
       static FFrameStressField ComputeCantileverFixture(int32 SamplesPerSpan = 11);

       UFUNCTION(BlueprintPure, Category="FrameCore|StressField")
       static int32 GetGoverningMemberId(const FFrameStressField& Field);

       UFUNCTION(BlueprintPure, Category="FrameCore|StressField")
       static const TArray<FFrameStressFieldSample>& GetMemberSamples(
           const FFrameStressField& Field, int32 MemberIdx);
   };
   ```

2. UE automation test `FFrameCoreUEBlueprintSmokeTest`(Private/Tests/):
   ```cpp
   // Drive ComputeCantileverFixture via the library; compare USTRUCT result.GlobalMaxFiberSigma
   // against the C++ POD reference (call frame::computeStressField on the same fixture and
   // pull max sigma) at rel<1e-5 (lossy double→float).
   // Sample 11 along the cantilever, assert analytic |P|·(L-x)/Wz at rel<1e-4 (visualisation
   // tolerance — F68 standalone is rel<1e-9 in double, this is the float-lossy budget).
   IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrameCoreUEBlueprintSmokeTest,
       "FrameCore.UE.BlueprintSmokeTest", ...)
   ```

3. bump `$ExpectedUeTests` 60 → 61(`Scripts/run_gate.ps1` line 29)

**Threshold:**
- ✅ PASS: BP test 綠 + standalone F1..F70 仍綠(零引擎改動 sanity check)
- ❌ NEGATIVE: marshal value drift > rel 1e-4 → 抓 const& copy / float cast / `samplesPerSpan`
  off-by-one。30 min 內修不掉 halt
- ⏭️ DEFERRED: 可。Phase 2 卡 → 跳到 Phase 3(Slate panel 直接 call C++ API 不依賴 BP),release
  v3.2.0-rc1 不算 BP exposure。把 Phase 2 失敗紀錄寫進 NIGHT_SHIFT

**風險:**
- Float lossy cast 在 small-load 場景(stress 量級 ~ MPa)可能出現 visible relative error >
  1e-4 — Phase 1 預計 tolerance 已放寬到 1e-4,實際看 fixture 尺度
- BP smoke test 在 UE automation 容器內可能 BP runtime 還沒 init → fallback 用 C++ 直接 call
  library static methods 不走真 BP graph(等於 testing marshal layer 而非 BP runtime)

---

### Phase 3 — Slate editor utility panel(**不可逆**,3-5 hr,ROI 中-高)

**目的:** Editor utility panel 讓 dev 在 Editor 內 load model + compute + 看 worst element。
Phase 3 是 "在 PIE 內驗證可視化邏輯" 的第一步。

**動作:**

1. `FrameCoreUE` module 內 `#if WITH_EDITOR` 區段(不單獨開 FrameCoreUEEditor module,降低 .uplugin
   複雜度;若 Slate 跟 Engine module type 衝突再切):
   - `FrameCoreUE.Build.cs` `if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(...)`
     加 `"Slate"`, `"SlateCore"`, `"UnrealEd"`, `"EditorStyle"`, `"EditorSubsystem"`,
     `"ToolMenus"`
   - Private/SFrameCoreStressFieldPanel.{h,cpp} — `SCompoundWidget`
   - Private/FrameCoreUEEditorCommands.cpp — register tool menu entry under
     `LevelEditor.MainMenu.Tools` → "Open Stress Field Panel"

2. Panel UI(minimal):
   - "Load fixture" button(下拉 Cantilever / Plate / Cross / Truss)→ build in-memory model
   - "Samples per span" int slider(2..32 default 11)
   - "Compute" button → call `computeStressField` → 填 result section
   - Result section:文字 "Global max fiber sigma: %f MPa" / "Governing member: id=%d" /
     "Worst sample x=%f L (sigmaCompMax=%f, sigmaTensMax=%f)"
   - Sample table:`SListView<TSharedPtr<FFrameStressFieldSample>>` 4 column

3. UE automation test `FFrameCoreUEEditorSmokeTest`:`#if WITH_EDITOR` 區段,只驗 widget 可 construct
   且 OnCompute() 不 crash;不點 button(automation 跟 Slate 互動很 fragile,留 Phase 3 真實 manual
   PIE 驗證即可)

4. bump `$ExpectedUeTests` 61 → 62

**Threshold:**
- ✅ PASS: panel 在 Editor 開得起來 + automation editor smoke test 綠 + 五腿 UE 62/62
- ❌ NEGATIVE: Slate API drift(UE 5.7 vs 範例)→ in-place 修;若 1 hr 內修不掉 halt 寫 NIGHT_SHIFT
- ⏭️ DEFERRED: 可。Phase 3 卡 → release v3.2.0-rc1(只含 Phase 1+2);Phase 3 移到 v3.2.1。
  把 panel 半成品 commit 到 branch `wip/v3.2-phase3` 不進 main

**風險:**
- `#if WITH_EDITOR` 區段在 packaged build 必須完全 elide;若 link error 表示 forgot
  `#if WITH_EDITOR` guard 某個 Engine module include
- Tool menu register timing:LoadingPhase `Default` 可能太早,改 `PostEngineInit`(已標 Phase 1
  風險)
- 如果 panel UX 想要超過「文字 + table」(spline mesh / colour band),那是 v3.3 範圍 — Phase 3
  維持「dev 自驗 minimum viable panel」

---

### Phase 4 — 五腿 gate 全綠 + bump ExpectedUeTests(**可逆**,1-2 hr,ROI 高)

**目的:** Phase 1-3 後完整跑五腿驗 v3.2 沒帶入 regression。

**動作:**
```powershell
conda activate framecore-direct
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 62
```
- Standalone F1..F70 應 bit-identical 過(零引擎改動)
- UE 62/62(60 base + Phase 2 BP + Phase 3 editor)
- OpenSees / audit 104 / CLI 應 bit-identical 過
- v2 dispatcher round-trip(`build_capi_v2.bat` + `Tools/v2_roundtrip.py`)也跑一次驗 v2 capability
  list 沒被 Phase 1-3 動到

**Threshold:**
- ✅ PASS: 五腿 + v2_roundtrip 全綠 → 進 Phase 5
- ❌ NEGATIVE: 某腿掉 → 排查是 Phase 1 / 2 / 3 哪步;halt 寫 NIGHT_SHIFT
- ⏭️ DEFERRED: N/A

**風險:**
- 若 standalone F1..F70 突然掉 → 表示 plan 違反「零引擎改動」承諾(rebuild artifact path /
  Build.cs side effect)→ 立刻 halt
- OpenSees env 切換失敗 → 同 Phase 0 風險

---

### Phase 5 — release-hardening + tag v3.2.0(**不可逆**,3-4 hr,ROI 高)

**目的:** 7-agent 對抗審核,closeout BLOCKER/HIGH,bump 版本,tag + GitHub release + binary bundle。

**動作:**

1. 開 `release-hardening` skill(plan 內已 user 隱式授權 — task 列表第 8 項就是這個)。預期 5-7
   個 finding 跟 v3.1.0 同規模。Phase 2 small-fixes 全部 fold 進 v3.2.0 commit
2. Bump 版本:
   - `Plugins/FrameSolver/FrameSolver.uplugin`:Version 29 → 30,VersionName "3.1.0" → "3.2.0"
   - `Plugins/FrameSolver/Standalone/v2/Dispatcher.h`:`kEngineVer "3.1.0"` → `"3.2.0"`
   - `Scripts/run_gpu_gate.ps1`:`FRAMECORE_EXPECTED_ENGINE_VER='3.1.0'` → `'3.2.0'`
   - `.github/workflows/release-gate.yml`:`FRAMECORE_EXPECTED_ENGINE_VER` 同步
   - `Scripts/run_gate.ps1`:`-ExpectedUeTests 60` → `62`(若 Phase 3 deferred 則 61)
3. 寫 `docs/RELEASE_v3.2.0.md`(範本參 v3.1.0,sections: §1 What ships / §2 What stayed bit-identical /
   §3 Repro matrix / §4 Tag plan / §5 Deferred / §6 Honest limitations / §7 Breaking changes)
4. 寫 `docs/HANDOFF_v3.2.0.md`(範本參 v3.1.0,sections: §1 What it is / §2 How to run / §3 Deferred +
   first-action / §4 Durable lessons / §5 Next directions)
5. Final 五腿 + v2_roundtrip(再跑一次確認 bump 後仍綠)
6. Git ops:
   ```bash
   git add -- <enumerated file list>
   git commit -m "release: v3.2.0 -- FrameCoreUE thin slice + BP node + Slate panel"
   git tag -a v3.2.0 -m "v3.2.0 -- UE consumer-side visualisation layer"
   git push origin main
   git push origin v3.2.0
   ```
7. GitHub release:
   ```bash
   gh release create v3.2.0 \
       --title "v3.2.0 -- FrameCoreUE thin slice + BP node + Slate panel" \
       --notes-file docs/RELEASE_v3.2.0.md \
       --latest
   ```
8. Binary bundle `framecore-v3.2.0-win64.zip` 包:
   - `frame_capi.dll` (v1) + `frame_capi_v2.dll` + `frame_cli.exe` + `frametest.exe`
   - **新增** `UnrealEditor-FrameCoreUE.dll`(若已產生且 < 10 MB)
   - 標明 "Engine binaries; UE plugin module included for reference but built against UE 5.7"

**Threshold:**
- ✅ PASS: release URL 上線 + binary 上傳 + tag = Latest + GitHub release page 載得起 markdown
- ✅ PASS-rc1: Phase 3 deferred 場景,tag `v3.2.0-rc1` 而非 `v3.2.0`,release marked `--prerelease`,
  HANDOFF 寫明 Phase 3 移到 v3.2.1
- ❌ NEGATIVE: audit BLOCKER 無法 in-place 修 → halt,把 finding 寫 NIGHT_SHIFT 等簽核
- ⏭️ DEFERRED: 把整個 Phase 5 移到 user 起床後執行 — 這是最不可逆的一步,user 可能想自己看完
  audit findings 才 tag。Plan 預設執行,但若 audit 出現 surprising finding 就 halt

**風險:**
- audit 抓出 Phase 1-3 預料外的 BLOCKER → Phase 5 budget 爆;留 1 hr buffer
- `gh release create` 需要 gh CLI auth(已 login rocky59487);若 token 過期 halt 寫 NIGHT_SHIFT
- Binary bundle 含未 tracked artifact path → 預先 dry-run `gh release upload --dry-run`

---

## 5. Hour budget 統計

| Phase | 預估 | 累計 |
|---|---|---|
| Phase 0 | 0.5 hr | 0.5 |
| Phase 1 | 3-4 hr | 3.5-4.5 |
| Phase 2 | 1-2 hr | 4.5-6.5 |
| Phase 3 | 3-5 hr | 7.5-11.5 |
| Phase 4 | 1-2 hr | 8.5-13.5 |
| Phase 5 | 3-4 hr | 11.5-17.5 |
| **合計** | **11.5-17.5 hr** | |

**最壞 case(Phase 3 卡住 + 不可逆改動需簽 → halt):** 完成 Phase 0+1+2+4(部分),
release `v3.2.0-rc1` 含 FrameCoreUE module + BP node,Slate panel 移到 v3.2.1。
這是 acceptable fallback,HANDOFF 紀錄交代清楚。

---

## 6. 沒事做時優先序(夜班閒置策略)

按 user 指示:
1. **Deeper research lane prototype** — 若 Phase 5 ~ 03:00 完成,開 HANDOFF deferred A-12
   (cuDSS PHASE_REFACTORIZATION P-Delta revisit)或 C-01(pinned host memory)做 throwaway
   prototype 在 `Research/`,不入 gate 不改 default
2. **文獻摘要** — `deep-research` skill;題目從 user MEMORY 抓 "百萬+1e-9+即時" R-line research 線
3. **Negative 結果驗證** — Phase 4 五腿 repeat 100 次跑 stability stress,確認 v3.2 穩定
4. **Docs grooming** — `S11_stress_field.md` 對 v3.2 補 BP exposure 段;`README.md` UE plugin
   段加 FrameCoreUE 說明
5. **Release-hardening deep audit on whole tree** — `release-hardening` skill 對整 repo 跑(非
   v3.2 specific),抓 stale comment / hardcoded path / doc drift

**禁止做的閒置行為:**
- wire 新 ABI / 新 v2 capability / 改 Build.cs dep — 違反 §3 allow-list
- 任何 `Plugins/FrameSolver/Source/FrameCore/` 內 source 改動 — 違反鐵則 #1

---

## 7. 起床檢查清單(user 醒來看的東西)

按 user 指示:
1. ✅ `docs/PLAN_v3.2_ue_interface.md` — **本檔** + 你的 sign-off commit
2. ✅ `docs/NIGHT_SHIFT_2026-06-22.md` — 工作日誌(每 phase 完成 / 卡住 / 浮現 unilateral
   decision 都 append)
3. ✅ `v3.2.0` tag(或 `v3.2.0-rc1` 若範圍縮減)+ GitHub release page + binary bundle
4. ✅ 每 phase 標 ✅/❌/⏭️ 狀態 — 在 NIGHT_SHIFT 表格 + PLAN 本檔 §4 phase header 各加 status badge
5. ✅ `docs/HANDOFF_v3.2.0.md` — 下次 session 接手指南

---

## 8. Sign-off

User 簽核這份 plan(commit + push 後 review) → 進 Phase 0。
Plan 內未明列的不可逆改動 → defer 到 NIGHT_SHIFT 紀錄等簽。
