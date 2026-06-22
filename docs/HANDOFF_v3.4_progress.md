# 交接指南 — v3.4 mid-progress(Phase 1+2 完整,Phase 3-5 待續)

> 寫於 2026-06-22 night,HEAD `476d866` ahead-of-`origin/main` 3 commits。
> **v3.4 尚未 release**(`v3.3.0` 仍是 Latest GitHub release tag)。本檔為 v3.4 6-phase plan 跑到一半的 mid-progress handoff;v3.4 完整 ship 的最終 HANDOFF 由 Phase 6 release-hardening 在 v3.4.0 tag 時寫(`docs/HANDOFF_v3.4.0.md`)。
> 主交接 chain:`docs/HANDOFF_v3.3.0.md` → `docs/HANDOFF_v3.4_v3.5_design.md` → 本檔 → 將來的 `docs/HANDOFF_v3.4.0.md`。

## 1. mid-progress = 什麼

v3.4 6-phase plan(`docs/specs/UE5_engine_surface_map.md`)的 **Phase 1 + Phase 2 落地**;Phase 3-5 與 Phase 6 release-hardening 全部 pending。Engine source delta vs `v3.3.0`(`dbdedb1`)= **3 行 FRAMECORE_API additive facade**(`Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/FrameModel.h:33-35` 上的 `nodeIndex`/`memberIndex`/`shellIndex`)+ comment 1 行;`impl` 不動,標準 spec line 47 允許 trivial cross-DLL facade。算法 / 結構 / 公開 POD shape 0 行變動,所有 standalone fixture F1..F71 bit-identical with v3.3.0。

兩 commit 共 ~3146 insertion (含 19 docs spec / 16 source / 1 gate-count bump):

| Commit | 主軸 |
|---|---|
| `3ad292b` | v3.4 + v3.5 design spec lock(本檔的前置;不動 source) |
| `60f95b4` | **Phase 1** input USTRUCT + ModelBuilder + library presets(0 engine delta) |
| `476d866` | **Phase 2** output USTRUCT + SolveResult marshal(3 FRAMECORE_API facade) |

新檔(全 `Plugins/FrameSolver/Source/FrameCoreUE/` 下):
- Public/FrameCoreUE/
  - `FrameCoreUEModelTypes.h` — 17 USTRUCT(Material/Section/Node/Member/Shell/Load 3 種/SolveOptions/8 種 analysis options + FFrameModelDef aggregate)+ 2 enum(EFrameSectionShape / EFrameReleasePreset)
  - `FrameCoreUEModelBuilder.h` — `UFrameModelBuilder::{ValidateModel, LoadModelFromJson}`
  - `FrameCoreUEMaterialLibrary.h` — `UFrameMaterialLibrary` 8 presets + `MakeCustomMaterial`
  - `FrameCoreUESectionLibrary.h` — `UFrameSectionLibrary::{MakeRectangular, MakeCircular}`(IBeam/HSS/CircularHollow deferred:engine 沒對應 `Section::Shape` factory,需想清楚是 conservative-treat-as-Rectangular 還是擴 enum)
  - `FrameCoreUEResultTypes.h` — 9 USTRUCT(EFrameFailMode / NodalDisplacement / NodalReaction / MemberEndForces+InternalForces / DemandResult+MemberUtilization / ShellInternalForces+ShellUtilization / DemandSummary / FFrameSolveResult top-level)
- Private/
  - `FrameCoreUEModelMarshal.cpp` — `FromBlueprint(FFrameModelDef → frame::FrameModel)` + `FromBlueprint(FFrameSolveOptions → frame::SolveOptions)` 長度 6/12/4 enforce + idx OOB 拒絕 + diagnostic
  - `FrameCoreUEModelBuilder.cpp` — ValidateModel + LoadModelFromJson(獨立 JSON parser,不 reuse v3.3 `ComputeFromJsonModel` 的 anonymous-namespace helper,避免 inter-cpp coupling)
  - `FrameCoreUEMaterialLibrary.cpp` — `MakeSteel(fy)` / `MakeConcrete(Ecm, fck)` helper + 8 presets
  - `FrameCoreUESectionLibrary.cpp` — delegate `frame::Section::Rectangular/Circular` 保 bit-equal
  - `FrameCoreUEResultMarshal.cpp` — `ToBlueprint(FrameModel, SolveResult) → FFrameSolveResult` 含 NaN-free-DOF guard / per-member ElasticAllowable D/C / per-shell checkShellSurface / worstUtilization+worstShellUtilization 跑 governing idx 解析
- Private/Tests/(8 個新 .cpp test;UE automation +8)
  - Phase 1: `FrameCoreUEBuildAndValidateTest.cpp` / `FrameCoreUEInvalidMemberRefTest.cpp` / `FrameCoreUELibraryPresetsTest.cpp`
  - Phase 2: `FrameCoreUESolveResultMarshalTest.cpp` 5 個 `IMPLEMENT_SIMPLE_AUTOMATION_TEST`

`Scripts/run_gate.ps1` 的 `$ExpectedUeTests 72 → 80`(+3 Phase 1,+5 Phase 2)。

## 2. 怎麼跑(reproduce)

```bash
# Engine bit-identity (應該 ALL PASS, engine source delta 限 facade)
cd E:\project\ArchSim
Plugins\FrameSolver\Standalone\build.bat

# UE incremental build(增 4 .cpp,UE 自動 adaptive non-unity 處理新 .cpp)
E:\project\UE_5.7\Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development -project="E:\project\ArchSim\ArchSim.uproject" -waitmutex

# UE automation, FrameCore.UE.* family only(快;+8 新 test 在這 family)
E:\project\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe "E:\project\ArchSim\ArchSim.uproject" -ExecCmds="Automation RunTests FrameCore.UE; Quit" -log -unattended -NullRHI -nosplash -stdout -nopause

# Full 5-leg gate (未在 mid-progress session 跑;留 Phase 3 session 跑)
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
```

驗證結果 host 2026-06-22 night:
- ✅ standalone F1..F71 ALL PASS(bit-identical vs v3.3.0)
- ✅ UE automation FrameCore.UE.* 20/20 PASS(其中 Phase 1+2 加 8 個新 test 全綠)
- ⏸ OpenSees / linear_deep_audit 104 / CLI roundtrip 13 / v2_roundtrip(CPU + CUDA)/ GPU strict — **NOT RUN this mid-progress session**;engine 算法 0 行變動 → evidence carries forward;Phase 3 session 跑完整 5-leg 再 commit Phase 3 checkpoint。

## 3. 新公開 API(給下個 phase 與 BP designer 用)

每個都在 FrameCoreUE module Public 下,可被任何 dependent module 跟 BP graph 用。

```cpp
// Phase 1 — input plumbing
USTRUCT FFrameMaterial{ E, G, Nu, Rho, Fy, Cap{ Comp, Tens, Shear, Bend, Tors, VM } };
USTRUCT FFrameSection { A, Iy, Iz, J, Cy, Cz, Asy, Asz, Zy, Zz, Shape (Rect/Circ) };
USTRUCT FFrameNode    { Id, Pos, Fixed: TArray<bool> (len 0 or 6), Prescribed: TArray<float> (len 0 or 6) };
USTRUCT FFrameMember  { Id, I, J, MatIdx, SecIdx, RefVec, Release: TArray<bool> (len 0 or 12), bActive, bTensionOnly };
USTRUCT FFrameShellQuad { Id, N: TArray<int32> (len 4), MatIdx, T, bActive };
USTRUCT FFrameNodalLoad { Node, Comp: TArray<float> (len 6) };
USTRUCT FFrameMemberUDL { Member, WLocal };
USTRUCT FFrameShellPressure { Shell, P };
USTRUCT FFrameSolveOptions { PivotTol, bEnableReleases, bUseTimoshenko, bUseIncompatibleMembrane,
                             bUseDKQPlate, bShellGeometricStiffness, bUseWarpingCorrection,
                             WarpTolerance, ShellCurvatureMaxAngleDeg, bUseSupernodalPrimary };
// + 7 analysis-specific options USTRUCT(PDelta / TensionOnly / Collapse / Corotational /
//   DynCollapse / SizeOpt + SizeOptLoadCase / BESO / Reanalysis)
USTRUCT FFrameModelDef { Materials, Sections, Nodes, Members, Shells,
                         NodalLoads, MemberUDLs, ShellPressures };

// Phase 2 — output plumbing
USTRUCT FFrameSolveResult { bSingular, Diagnostic, PivotMargin,
                            Displacements:TArray<FFrameNodalDisplacement>,
                            Reactions:TArray<FFrameNodalReaction>,
                            MemberForces:TArray<FFrameMemberInternalForces>,
                            MemberUtilization:TArray<FFrameMemberUtilization>,
                            ShellForces:TArray<FFrameShellInternalForces>,
                            ShellUtilization:TArray<FFrameShellUtilization>,
                            Utilization:FFrameDemandSummary };

// Library entry points
UFrameModelBuilder::ValidateModel(Def, OutError) -> bool
UFrameModelBuilder::LoadModelFromJson(JsonPath, OutError) -> FFrameModelDef
UFrameMaterialLibrary::GetS235/S275/S355/S460/ConcreteC30/C40/C50/Aluminum6061()
                      ::MakeCustomMaterial(E, G, Rho, Nu, Fy, CapComp, CapTens, CapShear)
UFrameSectionLibrary::MakeRectangular(W, D) / MakeCircular(D)

// Private marshal helpers(test 跟下 phase 用,not BP-exposed)
FrameCoreUE::FromBlueprint(FFrameModelDef, frame::FrameModel&, FString&) -> bool
FrameCoreUE::FromBlueprint(FFrameSolveOptions, frame::SolveOptions&) -> bool
FrameCoreUE::ToBlueprint  (frame::FrameModel, frame::SolveResult) -> FFrameSolveResult
FrameCoreUE::ToBlueprint  (frame::DemandSummary) -> FFrameDemandSummary
FrameCoreUE::ToBlueprint  (frame::DemandSummary, frame::ShellDemandSummary) -> FFrameDemandSummary
```

## 4. 仍 deferred 的 items + first-action on day 1

| ID | 內容 | First action on day 1 |
|---|---|---|
| P3-01 | Phase 3 SolveLinear BP entry | Edit `Plugins/FrameSolver/Source/FrameCoreUE/Public/FrameCoreUE/FrameCoreUEAnalysisLibrary.h`(新檔): `UFrameAnalysisLibrary::SolveLinear(const FFrameModelDef&, const FFrameSolveOptions&) -> FFrameSolveResult`,內部 `FromBlueprint → frame::solve → ToBlueprint`。第一個 test = `FrameCore.UE.SolveLinear.Cantilever`,F68 fixture rel<1e-5 mirror Phase 2 `CantileverDisplacement`(這個應該 trivially pass — Phase 2 已驗 marshal layer)。 |
| P3-02 | InfluenceLine entry point grep | Spec line 214 標 TBD。`grep -rn "influence" Plugins/FrameSolver/Source/FrameCore/Public/` 找對應的 `FRAMECORE_API` function;若不存在 → spec line 215「locate the function at Phase 3 start」flag,可能要降 InfluenceLine 為 Phase 4 或 v3.5。 |
| P3-03 | AnalysisModal / AnalysisBuckling / ResponseSpectrum / RealTimeDynamic | grep `analysisModal` / `analysisBuckling` / `responseSpectrum` / `realTimeDynamic` in FrameCore Public,Phase 3 加 result USTRUCT(FFrameModalResult / FFrameBucklingResult / FFrameResponseSpectrumResult / FFrameRealTimeDynamicResult)+ marshal。 |
| P3-04 | ReanalysisSolve stateless library form | `frame::ReSolveSession` 是 class with PIMPL ctor;stateless library form = build SnSession per call,**O(n³) per call** —— 但 v3.5 InteractiveSubsystem 才做 stateful。spec line 220 確認 stateless form 在 Phase 3。 |
| P4-01 | DynCollapse marshal sub-types(7 USTRUCT) | spec line 250-256:FFrameDynCollapseFrame + FFrameDynCollapseEvent + EFrameDynCollapseOutcome + FFrameFragmentCluster + FFrameDynCollapseResult。Block-mode 不接 onFrameEmitted/isCancelled callback(留 v3.5 InteractiveSubsystem)。 |
| P4-02 | SizeOpt / BESO result USTRUCT | spec line 243-244:FFrameSizeOptResult + FFrameBESOResult。BESO `finalActive` / `bestActive` 是 `std::vector<char>` → TArray<bool>(char 0/1 直接 → bool)。 |
| P4-03 | Corotational + ArcLength | Engine `frame::CorotationalOptions::useArcLength` 內嵌 arc-length;spec line 246 提 SolveArcLength 為獨立 BP entry 但 engine 無獨立 `frame::ArcLengthOptions`。決策 Phase 4 start: 兩條路 — (a) BP 用同一 `SolveCorotational(opts)` + opts.bUseArcLength;(b) 加 BP-side facade `SolveArcLength(opts)` 內部 force opts.bUseArcLength=true。推 (b) 更 BP-friendly。 |
| P5-01 | Shell opt-ins parity 6 tests | spec line 295-310:DKQ vs MITC4 / QM6 in-plane / Shell K_sigma buckling / warped quad admit / Shell co-rotational rotation invariance / NM interaction opt-in。每個 standalone 已有 fixture(F50/F57/F58b 等)→ UE thin mirror。 |
| P6-01 | release-hardening v3.4.0 ship | `kEngineVer 3.3.0 → 3.4.0` / uplugin Version 31 → 32 + VersionName 3.3.0 → 3.4.0 / `FRAMECORE_EXPECTED_ENGINE_VER` in `run_gpu_gate.ps1` + `release-gate.yml` / `run_gate.ps1 $ExpectedUeTests 80 → ~98`(or 實際 count after Phase 3-5)/ docs sync(README + VERIFICATION + ARCHITECTURE + new HANDOFF + new RELEASE notes)/ CLAUDE.md line 13 anchor 從 v3.4-in-progress → v3.4.0 released / 3-agent audit / 5-leg gate / tag v3.4.0 + push + gh release create with bundle zip。 |
| P-IBeam | UFrameSectionLibrary::MakeIBeam/HSS/CircularHollow | engine `Section::Shape` 只 `{Rectangular, Circular}`。Option (a)BP-side analytic A/Iy/Iz/J/cy/cz/Asy/Asz/Zy/Zz + 標 Shape=Rectangular(elastic 正確,biaxial conservative);Option (b) 擴 engine `Section::Shape::IBeam/HSS/CircularHollow` + 改 `ElasticAllowable` 跟 `StressKernel` 的 biaxial 公式(違鐵則 #1 — engine 算法變)。推 (a),Phase 5 加 spec line 312 對應 fixture。 |

## 5. 過程留下的教訓(durable)

- **Spec line 47 trivial FRAMECORE_API facade**:`v3.4` engine source delta target 是 0 行,但 spec 明確允許 additive `FRAMECORE_API` annotation for cross-DLL public-API access。Phase 2 link error `LNK2019: FrameModel::memberIndex/shellIndex` 是這個 facade 沒在,impl 在 Private/FrameModel.cpp 不能跨 DLL。fix = +3 行 declaration-side `FRAMECORE_API`(impl 完全不動,standalone bit-identical)。鐵則 #1「跨 DLL 符號標 FRAMECORE_API」明確要求,加 facade 是 fix 過去遺漏而非引入新 risk。**用 FrameCoreUE 暴露 engine private API 時遇 LNK2019 → 先 grep FrameModel.h 看該 API 是否有 FRAMECORE_API**。
- **USTRUCT 嵌 USTRUCT 過 ~3 層 BP Make/Break 變難讀**:FFrameSolveResult 有 `TArray<FFrameMemberUtilization>` 而 FFrameMemberUtilization 嵌 `FFrameDemandResult` 而 FFrameDemandResult 嵌 `EFrameFailMode` — 4 層。BP designer 用 `Get Member Utilization → Get Peak → Get Risk` 順 OK,但 `Get Member Utilization → Make Member Utilization Override` 在 BP 不好操作。Phase 3 / 5 加 `UFrameAnalysisLibrary::GetMemberPeakRisk(SolveResult, MemberIdx)` 等 flat accessor 為 BP ergonomics。
- **Test 用 `EAutomationTestFlags_ApplicationContextMask` (UE 5.7 正確 form)**:`v3.3` 既有 pattern 用底線 form (`_ApplicationContextMask`) 非 v5.6 之前的 `::ApplicationContextMask` member access form。新 test 沿用底線 form。
- **`TestNotEqual(uint8(Mode), uint8(EFrameFailMode::None))`**:UE `TestEqual` 對 enum class 要 cast 到 underlying integer type,否則 ambiguous overload。
- **Inline `FFrameSolveOptions` defaults**:USTRUCT field 用 `= xxx` inline default 完全 OK(C++17 default member initialiser),不需要 ctor。Engine `frame::SolveOptions{}` default 跟 USTRUCT default `Make Struct` 一致(field-for-field copy)讓 BP `Make Frame Solve Options` zero-config 跑得起 — Phase 1 LibraryPresets test 隱含驗。
- **Phase 1 「TBD」in spec line 214**:`InfluenceLine` API spec 標 TBD 因為 grep 不確定 entry point 在 FrameCore Public。Phase 3 start 第一動是 grep 確認;若找不到 → Phase 3 spec 動,InfluenceLine 降到 Phase 4 或 v3.5。

## 6. 後續方向

- **Next session(Phase 3)**:5-leg gate 跑 fresh(OpenSees / audit / CLI / v2_roundtrip 確認 v3.3 evidence forward + new UE 80 count),然後 implement P3-01 → P3-04 順序;Phase 3 預估 4-8 hr 實際,完成後 commit checkpoint。
- **Phase 4 (4-6 hr)**:DynCollapse marshal 是最大工作量(7 sub-USTRUCT);SizeOpt + BESO 加 result USTRUCT;Corotational + ArcLength decision(推 (b) 獨立 BP facade)。
- **Phase 5 (1-3 hr)**:Shell opt-ins parity 6 個 thin mirror test。
- **Phase 6 (3-5 hr)**:release-hardening 全套 + v3.4.0 ship + GitHub release。
- **v3.5 開工 (post-v3.4.0)**:讀 `docs/specs/UE5_visual_surface_map.md`;Phase 1 USTRUCT validation quick sanity → Phase 1 deformed shape actor。

---

接手有問題:`docs/HANDOFF_v3.3.0.md` → `docs/HANDOFF_v3.4_v3.5_design.md` → 本檔。Phase-specific 問題讀 `docs/specs/UE5_engine_surface_map.md`;Karamba3D 對標問題讀 design HANDOFF 表格。
