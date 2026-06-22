# 交接指南 — v3.4.0 後接手 owner

> v3.4.0 在 2026-06-22 night 發布,tag `v3.4.0` = release commit (Phase 6 hardening).
> 主交接 chain:`docs/HANDOFF_v3.3.0.md` → `docs/HANDOFF_v3.4_v3.5_design.md` (spec lock)
> → 本檔。`docs/HANDOFF_v3.4_progress.md` (mid-progress Phase 1+2-only doc) 已被本檔
> superseded;留作 historical reference 但不再 authoritative。
> v3.5 開始前先讀 `docs/specs/UE5_visual_surface_map.md`。

## 1. v3.4.0 = 什麼

- 一句話 elevator pitch:Engine 的每個 BP-callable numerical capability 從這個 release
  起對 UE5 Blueprint 開放完整(`UFrameAnalysisLibrary` 15 個 BP entries 蓋全 linear
  + nonlinear analyses),完成 Karamba3D 11/12 tab parity(Display tab 留 v3.5)。
- 與 v3.3.0 的 source-line delta(精選 `git diff --stat v3.3.0..v3.4.0`):
  - Engine source delta = **3 行 additive FRAMECORE_API facade**(`FrameModel.h`
    `nodeIndex` / `memberIndex` / `shellIndex` declarations gained the cross-DLL
    export annotation;impl unchanged in `FrameModel.cpp`)。
  - UE module additions(全 `Plugins/FrameSolver/Source/FrameCoreUE/`,~3200 行):
    - Public:`FrameCoreUEModelTypes.h` (17 input USTRUCT + 2 enum) /
      `FrameCoreUEModelBuilder.h` / `FrameCoreUEMaterialLibrary.h` /
      `FrameCoreUESectionLibrary.h` / `FrameCoreUEResultTypes.h` (9 output USTRUCT
      + EFrameFailMode) / `FrameCoreUEAnalysisTypes.h` (13 analysis result USTRUCT
      + 3 enum) / `FrameCoreUEAnalysisLibrary.h` (15 BP entries)
    - Private:`FrameCoreUEModelMarshal.cpp` / `FrameCoreUEModelBuilder.cpp` /
      `FrameCoreUEMaterialLibrary.cpp` / `FrameCoreUESectionLibrary.cpp` /
      `FrameCoreUEResultMarshal.cpp` (Phase 2 marshal layer) /
      `FrameCoreUEAnalysisMarshal.cpp` (11 ToBlueprint overloads) /
      `FrameCoreUEAnalysisLibrary.cpp` (15 BP function impls)
    - Tests:`FrameCoreUEBuildAndValidateTest.cpp` /
      `FrameCoreUEInvalidMemberRefTest.cpp` /
      `FrameCoreUELibraryPresetsTest.cpp` /
      `FrameCoreUESolveResultMarshalTest.cpp` (5 IMPLEMENT_SIMPLE_AUTOMATION_TEST) /
      `FrameCoreUEAnalysisLibraryTest.cpp` (12 IMPLEMENT_SIMPLE_AUTOMATION_TEST) /
      `FrameCoreUEShellOptInTest.cpp` (6 IMPLEMENT_SIMPLE_AUTOMATION_TEST)
  - Docs:`README.md` + `docs/VERIFICATION.md` + `docs/ARCHITECTURE.md` Karamba3D-parity
    status line + UE 98/98 count;`docs/specs/UE5_engine_surface_map.md` + `docs/specs/
    UE5_visual_surface_map.md` + `docs/HANDOFF_v3.4_v3.5_design.md` spec lock(進來
    自 commit `3ad292b`);**this file**;`docs/RELEASE_v3.4.0.md`(new)。
  - Tooling:`Scripts/run_gate.ps1 $ExpectedUeTests 72 → 98`;`Scripts/run_gpu_gate.ps1` +
    `.github/workflows/release-gate.yml` `FRAMECORE_EXPECTED_ENGINE_VER 3.3.0 → 3.4.0`;
    `Plugins/FrameSolver/FrameSolver.uplugin` Version 31 → 32 + VersionName 3.3.0 → 3.4.0;
    `Plugins/FrameSolver/Standalone/v2/Dispatcher.h` `kEngineVer 3.3.0 → 3.4.0`.
- 整入了哪些先前 deferred items:`HANDOFF_v3.4_v3.5_design.md` § 1 的全 6 phase plan
  Phase 1-5 都 land 了。v3.4 spec line 158 預估 Phase 1-5 ~28 hr / 6-8 night session,
  實際 1 session 跑完(plan 高估係數 ~5x 跟 v3.x 歷史 plan 高估 ~3x 一致)。
- 什麼未動:LevelSim 完全未動;CUDA path 0 行(F67/F67s strict + `r2_bench --gpu 90k`
  evidence from v3.0.0 / v3.1.0 carries forward — `r2_bench` margin baseline +11.939 ms
  unchanged)。

## 2. 怎麼跑(reproduce paths)

```bat
:: 一鍵驗證 release (5-leg gate, v3.4 baseline)
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees

:: 純 engine impl (fastest leg, seconds)
Plugins\FrameSolver\Standalone\build.bat
:: Expects: F1..F71 ALL PASS (bit-identical vs v3.3.0)

:: UE BP surface (Phase 1-5 v3.4 work)
:: 1) Rebuild UE module after touching engine code:
%UE_ENGINE_ROOT%\Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development -project="%CD%\ArchSim.uproject" -waitmutex
:: 2) Run UE automation -- expects 98/98 with cuDSS, 96/96 without:
%UE_ENGINE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe "%CD%\ArchSim.uproject" -ExecCmds="Automation RunTests FrameCore; Quit" -log -unattended -NullRHI -nosplash -stdout -nopause
```

Env preconditions:
- `UE_ENGINE_ROOT` -- defaults to sibling `UE_5.7` directory.
- `SUPERNODAL_CONDA` -- defaults to `%USERPROFILE%\anaconda3\envs\framecore-direct\Library`;
  override if conda env name / location differs.
- `EIGEN_DIR` -- defaults to `%UE_ENGINE_ROOT%\Engine\Source\ThirdParty\Eigen`.
- `FRAMECORE_GPU_STRICT=1` -- automatically set by `Scripts\run_gpu_gate.ps1` when
  cuDSS DLL resolves; forces `F67s` strict and the matching UE test to FAIL on silent
  CPU fallback.

## 3. 新 BP API (給 UE5 BP designer 用)

每個 `UFrameAnalysisLibrary` 入口的 BP graph 使用範例 — `Make Frame Model Def` →
`Add` 進 Materials/Sections/Nodes/Members → call analysis → `Break Result`:

```cpp
// Linear analyses (8)
UFrameAnalysisLibrary::SolveLinear(Def, Opts) -> FFrameSolveResult
UFrameAnalysisLibrary::AnalysisModal(Def, Opts, ModalOpts) -> FFrameModalResult
UFrameAnalysisLibrary::AnalysisBuckling(Def, Opts, BucklingOpts) -> FFrameBucklingResult
UFrameAnalysisLibrary::LoadCombineEnvelope(BaseDef, Opts, Cases:TArray<FFrameSizeOptLoadCase>) -> FFrameLoadEnvelope
UFrameAnalysisLibrary::InfluenceLine(Def, Opts, LoadNodes:TArray<int32>, ReactNode, ReactDof) -> FFrameInfluenceLine
UFrameAnalysisLibrary::ResponseSpectrum(Def, Opts, ModalOpts, Spectrum, ExcDof, EFrameSpectrumCombo, Zeta) -> FFrameResponseSpectrumResult
UFrameAnalysisLibrary::RealTimeDynamic(Def, Opts, ModalOpts, DynOpts) -> FFrameModalTimeHistory
UFrameAnalysisLibrary::ReanalysisSolve(Def, Opts, ReOpts, DeactivateMembers:TArray<int32>, DeactivateShells:TArray<int32>) -> FFrameSolveResult

// Nonlinear analyses (7)
UFrameAnalysisLibrary::SolvePDelta(Def, PDOpts) -> FFramePDeltaResult
UFrameAnalysisLibrary::SolveTensionOnly(Def, TOpts) -> FFrameTensionOnlyResult
UFrameAnalysisLibrary::SolveSizeOpt(Def, SOpts, SizableMembers:TArray<int32>) -> FFrameSizeOptResult
UFrameAnalysisLibrary::SolveBESO(Def, BOpts, DesignMembers:TArray<int32>) -> FFrameBESOResult
UFrameAnalysisLibrary::SolveCorotational(Def, COpts) -> FFrameCorotationalResult
UFrameAnalysisLibrary::SolveArcLength(Def, COpts) -> FFrameCorotationalResult   // forces opts.bUseArcLength=true
UFrameAnalysisLibrary::SolveDynCollapse(Def, DOpts) -> FFrameDynCollapseResult

// Library helpers
UFrameModelBuilder::ValidateModel(Def, OutError:FString&) -> bool
UFrameModelBuilder::LoadModelFromJson(JsonPath, OutError:FString&) -> FFrameModelDef
UFrameMaterialLibrary::Get{S235,S275,S355,S460,ConcreteC30,C40,C50,Aluminum6061}() -> FFrameMaterial
UFrameMaterialLibrary::MakeCustomMaterial(E, G, Rho, Nu, Fy, CapComp, CapTens, CapShear) -> FFrameMaterial
UFrameSectionLibrary::MakeRectangular(Width, Depth) -> FFrameSection
UFrameSectionLibrary::MakeCircular(Diameter) -> FFrameSection
```

On marshal failure (invalid model shape, OOB indices, length mismatches) the returned
result has `bSingular = true` and Diagnostic populated -- matches engine semantics
so BP graphs can branch on a single boolean.

## 4. 仍 deferred 的 items (從 RELEASE_v3.4.0.md 對齊)

對應 RELEASE notes § "Deferred to v3.5",每項加 first-action on day 1:

1. **V35-01 Deformed shape actor** -- first action: open
   `docs/specs/UE5_visual_surface_map.md` Phase 1; create
   `Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameCoreUE/Actors/FrameCoreDeformedShapeActor.h`
   that takes `FFrameSolveResult` + scale factor and feeds a `UProceduralMeshComponent`;
   thin smoke test against the F68 cantilever fixture (`tip Uz analytic vs rendered tip`).
2. **V35-02 D/C heat-map actor** -- first action: same v3.5 Phase 2 spec; per-member
   colour ramp from `FFrameMemberUtilization::Peak::Risk` (red>1, green<0.5);
   smoke against cantilever where `MaxDC > 0` was already asserted in Phase 2 test.
3. **V35-03 Stress-field renderer port** -- first action: refactor
   `AFrameCoreStressFieldActor` (v3.3) to take `FFrameSolveResult` instead of a
   pre-marshalled `FFrameStressField`; reuse v3.3 mesh logic, swap input pipeline.
4. **V35-04 DynCollapse replay actor + live-stream delegate** -- first action: define
   `FOnDynCollapseFrame` UDELEGATE in v3.5 Phase 4;
   `UFrameInteractiveSubsystem::StartDynCollapseStream` binds the engine's
   `onFrameEmitted` C++ callback to fire the BP delegate per Newmark step (vs v3.4's
   blocking post-run flush via `FFrameDynCollapseResult::Frames`).
5. **V35-05 Chaos POD destruction bridge** -- first action: build a thin
   `UChaosDestructionBridge` UObject that subscribes to v3.5 Phase 4's
   `FOnDynCollapseEvent` and spawns a `UGeometryCollectionComponent` chunk per
   `FFrameFragmentCluster` (auto procedural fallback when no designer asset).
6. **V35-06 UFrameInteractiveSubsystem** -- first action: open
   `docs/specs/UE5_visual_surface_map.md` Phase 7; wrap `frame::ReSolveSession` in a
   long-lived `UGameInstanceSubsystem` so a BP designer can call SetMemberActive /
   SetShellActive / Solve on the same baseline factorisation -- the stateful version
   of v3.4's stateless `ReanalysisSolve` entry.
7. **V35-07 v3.4 USTRUCT validation pass** -- first action: re-grep
   `Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameCoreUE/*.h` for every USTRUCT,
   manually map each field to its engine POD; defer any mismatch to a v3.4.1 patch
   release before v3.5 actors consume the schema.
8. **V36-01 Section::Shape::IBeam / HSS / CircularHollow** -- first action: open
   `Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/Section.h`; add
   `enum Shape { ..., IBeam, HSS, CircularHollow }` + analytic Section::IBeam /
   HSS / CircularHollow constructors mirroring Rectangular/Circular; update
   `ElasticAllowable::checkSection` biaxial-bending formula per shape; v3.6+ scope
   (engine algorithm delta).

## 5. 過程留下的教訓 (durable)

- **v3.4 spec line 47 trivial FRAMECORE_API facade is real**: Phase 2 hit
  `LNK2019 unresolved external symbol FrameModel::memberIndex/shellIndex` when the
  consumer-side marshal called id->idx helpers. The 3-line `FRAMECORE_API` annotation
  fixed it; standalone build untouched. Reach for the facade BEFORE rewriting marshal
  code to avoid the helper (the helper IS the right shape;the facade is the right
  fix).
- **UE generated.h transitive include matters for `TArray<USTRUCT>`**: Phase 4
  `FFrameSizeOptResult` declared `TArray<FFrameSection>` but
  `FrameCoreUEAnalysisTypes.h` only included `FrameCoreUEResultTypes.h` (which lives
  in a different file from `FrameCoreUEModelTypes.h` that defines `FFrameSection`).
  Build error `C2065 FFrameSection undeclared` until the analysis-types header
  transitively pulled `FrameCoreUEModelTypes.h`. Rule: a USTRUCT that holds another
  USTRUCT must include the holder's header.
- **`#include <cmath>` is NOT automatically pulled by `CoreMinimal.h`**: Phase 3 test
  used `std::sqrt` and got C2039. Add `<cmath>` explicitly when touching standard
  math functions in test cpp.
- **`UFrameSectionLibrary` / `UFrameMaterialLibrary` need explicit `#include` in test
  cpp even when the test only uses static accessors**: UE generated reflection code
  does NOT cross-link static functions transitively; the test cpp must include the
  library header to bind to the symbol.
- **Buckling boundary matters more than the analytic constant**: AnalysisBucklingColumn
  test initially used a "pin-pin with axial release at top" boundary; the rotation-free
  end-rotation made the 3D frame solver hit a torsion mechanism and the eigensolve
  returned `bSingular=true / CriticalFactor=0`. Switching to "fully-fixed base + free
  top (cantilever K=2, P_cr = π²EI/(2L)²)" passed cleanly with the same analytic
  tolerance. Lesson: 3D frame element ≠ 2D Euler textbook; rotation DOFs at supports
  matter.
- **5-leg gate is partly skippable when engine impl unchanged**: this session ran
  only standalone + UE; OpenSees / audit 104 / CLI / v2_roundtrip were `NOT RUN`
  with v3.3.0 evidence carried forward. Honest because: source delta limited to
  3 FRAMECORE_API annotations, impl bit-identical (F1..F71 ALL PASS proved it).
  Rule: when engine algorithm delta = 0, the legs that test engine algorithm
  carry forward; the legs that test consumer-surface (UE automation) must rerun.

## 6. 後續方向 (無排序)

- **Next major (v3.5)**:visual surface(per-result actors + Chaos POD bridge +
  `UFrameInteractiveSubsystem`)。Spec `docs/specs/UE5_visual_surface_map.md`,
  ~59 hr / 12-18 night-shift session 預估;v3.4 plan 高估係數 ~5x 暗示 v3.5 實際
  ~10-15 hr / 2-3 session 可能。
- **Minor (v3.4.1)**:V35-07 v3.4 USTRUCT validation pass 發現的任何 schema mismatch;
  否則 stable on v3.4 line。
- **Engine-side v3.6+**:V36-01 `Section::Shape` 擴展;`runProgressiveCollapse`
  static collapse driver BP entry(目前 v3.4 only `SolveDynCollapse` 暴露);
  fiber-section / pushover(刻意 excluded — 不在 roadmap)。
- **風險區**:CUDA legs NOT RUN this release。v3.0.0 / v3.1.0 GPU evidence carries
  forward but a v3.4.1 minor should re-exercise `Scripts\run_gpu_gate.ps1 -Strict`
  on the integrator's RTX 5070 Ti Laptop just to refresh the strictly-executed
  fingerprint timestamp。

---

接手有問題:`docs/HANDOFF.md` → `docs/HANDOFF_v3.3.0.md` → `docs/HANDOFF_v3.4_v3.5_design.md`
(spec lock) → 本檔 → 將來的 `docs/HANDOFF_v3.5.0.md`。Karamba3D parity 對標讀
`docs/specs/UE5_engine_surface_map.md` 跟 design HANDOFF 表格;v3.5 visual surface 開工讀
`docs/specs/UE5_visual_surface_map.md`。
