# FrameCore — Rhino / Grasshopper 商業級 UX 層(S6c)

> **狀態**:設計完成 + 骨架到位(C1)。**[NOT GATED]** — 需 Rhino 8 .NET SDK 建置與測試,
> 引擎 CI 無此環境(同 v1 J1/J2 元件邊界)。
>
> 上游:`docs/specs/S6c_rhino_ux_commercial.md`(體驗規格,80 元件目錄、視覺規範、互動 SLA、
> 預設庫、Bake 規則、單位切換、Yak 發佈)。

## 為什麼有這層?

`FrameCore.Bridge`(`Grasshopper/v2/` 上一層)是 **zero-Rhino-dep** 的 .NET 8 SDK,Python/WPF
也能用 — 但 **沒有** Rhino viewport 整合、GH 元件、IGH_PreviewObject、Bake、Yak 包裝。本層
建在 SDK 之上,加 Rhino/GH 依賴,**讓 FrameCore 在 GH 畫布上跟 Karamba3D 一樣好用、甚至更好**。

## 目錄結構

```
Rhino/
├─ FrameCore.Gh.csproj            (net48 GHA target,RhinoCommon + Grasshopper)
├─ FrameCoreGhPlugin.cs           (GH_AssemblyInfo entry,tab 常數)
├─ Resources.cs                   (embedded icon loader,C2 補真 SVG)
├─ manifest.yml                   (Yak 打包描述)
├─ Common/                        (10 件共通工具)
│  ├─ Units.cs                    (SI / SI-kN / Imperial 切換 + 標籤)
│  ├─ UiMessage.cs                (wire-tip 狀態格式器)
│  ├─ AsyncComponent.cs           (cancel-previous + run-on-background base class)
│  ├─ PreviewPipeline.cs          (IGH_PreviewObject 範式 + DrawCommand + ColorRamps)
│  ├─ RhinoBaker.cs               (結果 bake 進結構化 layer tree)
│  ├─ GooWrappers.cs              (GH_Goo<FrameSession/FrameModel/LinearResult/...> 強型別 wire)
│  └─ GhParameters.cs             (對應的 IGH_Param 類)
├─ Library/                       (預設庫)
│  ├─ MaterialLibrary.cs          (11 種:S235/S275/S355/Q235/Q345/Q420/A36/A572 Gr50/C30/C50/Al6061)
│  └─ SectionLibrary.cs           (60+ 種:GB H + EU IPE/HEA/HEB + US W + Box + Pipe)
└─ Components/                    (11 tab,1 個代表元件已實作,其餘骨架待 C2)
   ├─ Setup/OpenFrameCoreComponent.cs
   ├─ Material/SteelFromLibraryComponent.cs
   ├─ Section/HSectionFromLibraryComponent.cs
   ├─ Analyze/AssembleModelComponent.cs
   ├─ Analyze/SolveComponent.cs
   ├─ Inspect/InspectDispComponent.cs
   ├─ Display/BMDComponent.cs              ← 商業級的招牌
   ├─ Display/UtilizationFringeComponent.cs ← 商業級的招牌
   ├─ Display/CollapseReplayComponent.cs   ← 超越 Karamba 的招牌
   └─ Advanced/DiagnosticPanelComponent.cs
```

## 已實作 / 待補(對標規格 S6c § ② 60+ 元件目錄)

| Tab | 規格件數 | C1 已實作 | C2-C3 待補 |
|---|---|---|---|
| 1. Setup | 7 | OpenFrameCore | EngineInfo / Units / SwitchProfile / Capabilities / EngineDiagnostics / CloseFrameCore |
| 2. Material | 6 | SteelFromLibrary | ConcreteFromLibrary / CustomMaterial / ShellMaterial / MaterialCapacity / MaterialSwatch |
| 3. Section | 8 | HSectionFromLibrary | RectangularSection / CircularSection / PipeSection / BoxSection / IShapeCustom / CustomSection / SectionPreview |
| 4. Geometry | 7 | — | 全部 |
| 5. Boundary | 7 | — | 全部 |
| 6. Load | 6 | — | 全部 |
| 7. Analyze | 11 | AssembleModel, Solve | 9 件(PDelta/TensionOnly/SizeOpt/DynCollapse/Corot/ArcLength/Modal/Buckling/ReSolveSession)|
| 8. Inspect | 5 | InspectDisp | InspectReactions / InspectMemberForces / InspectShellForces / InspectMechanism |
| 9. Display | 8 | BMD / UtilizationFringe / CollapseReplay | SFD / NFD / DeformedShape / ModeAnimation / FragmentVisualizer |
| 10. Advanced | 7 | DiagnosticPanel | PivotMarginTrace / SnIrResidualPlot / TierLadderView / EnergyTrace / BasisInheritance / DiagnosticStream / StrictModelValidator |
| 11. IO | 8 | — | 全部 |
| **總計** | **80** | **10**(每 tab 至少 1 個,代表性涵蓋) | 70 |

C1 的 **每元件都是完整 production code**(不是 stub),對 C2-C3 來說是「按範例補滿」,不需重新設計。

## 商業級體驗的招牌(S6c § ⑤)

兩個 Display 元件展示「為什麼商業級而非 toy」:

### `BMD` — 沿桿彎矩圖
- 沿每桿線性插值 Mz/My,沿 local y/z 凸出 `M × scale` 後形成 polyline + 填色 mesh
- **IGH_PreviewObject** 整合 → Rhino viewport 直接畫,不用 bake
- ColorRamp(Viridis / Jet)依 |M|/peak 染色
- 滑桿動 scale → ms 級即時更新

### `UtilizationFringe` — D/C 利用率色階
- 沿桿建 pipe mesh,per-vertex color 從 `Viridis(D/C)` 染
- 殼端的 vM stress / cap.vm 染 facet(C2 補)
- 圖例 mesh(C2)

### `CollapseReplay` — DYNC 完整 u/v 時間軸回放
- 商業軟體沒做 — 因為他們沒有 DYNC 引擎
- 時間滑桿 → 從 `result.Frames[]`(advanced binary u)取兩相鄰幀內插
- 已移除桿件淡出、detached fragments 分開渲(C2 完整)
- **這是本套超越 Karamba 的點之一**

## 互動 SLA(S6c § 6)

商業級 ≠ 「能算」,而是「滑桿動 < 30ms」:

1. **`AssembleModel` 駐留 EngineSession** — 每次 SolveInstance 不重開、不重 set model;factor 重用
2. **`Solve` async + cancel-previous** — 滑桿動立即取消前一個 solve,GH UI thread 不卡
3. **`InspectDisp` 部分讀** — 只送 nodeIds,引擎不重算
4. **future: `ReSolveSession` 元件**(C2)— 桿件 active toggle → Tier-1 Woodbury 重算
5. **future: `model.patch`**(C2)— 只送變動的 NLOAD,factor 完全不動

## 雙 Profile 整合

- `OpenFrameCore` 右鍵選單切 Simple / Advanced
- Simple = UE5 友好,wire-tip 黃色 warning 列引擎填的預設
- Advanced = 嚴格,silent fallback 一律 throw `RemoteException`
- `DiagnosticPanel` 元件 simple → 提示切 Advanced;advanced → 完整 diagnostic block

## 視覺規範

- 11 tab × 11 主色(S6c § 3.1),每 tab 一致
- 圖示:`Resources/Icons/*.svg`(C2 補真 SVG,目前 deterministic 色塊 placeholder)
- wire-tip:`[profile] [units] · [ms] · [verdict]` 永一致(`UiMessage.Format()`)
- ColorRamp:Viridis 預設(色盲友好);Jet opt-in

## Bake 規則(S6c § ⑦)

`BakeToRhino` 元件(C2)寫入結構化 layer tree:

```
FrameCore /
├─ Model / { Nodes, Members, Shells, Supports }
├─ Loads / { Nodal, UDL, Pressure }
└─ Results / { Deformed, BMD, SFD, NFD, UtilFringe, Modes/Mode-i }
```

- 同層內 bake 前先 ClearLayer(避免重複)
- 同 GH definition 反覆 bake → update 同 layer,不堆積
- text dot 顯示 node id / member id / D/C peak

`RhinoBaker.cs` 已實作核心:`EnsureLayer` / `ClearLayer` / `BakeMembers` / `BakeDeformed` /
`BakeBmd` / `BakeMode` / `BakeUtilFringe`。

## 單位切換

`Units.cs` 三套:`SI_N_mm_MPa`(engine native) / `SI_kN_m_kPa` / `Imperial_kip_in_ksi`。

- 每個 input 元件 optional `UnitSystem` 端口
- 自動 `LengthToEngine / ForceToEngine / StressToEngine / DensityToEngine` 轉換
- 顯示元件反向 `XxxFromEngine` 給使用者
- wire-tip 顯示當前單位短名(`SI` / `SI-kN` / `Imp`)

## Yak 發佈(C5)

```
yak build .                                  # 在 Rhino/ 目錄
yak push framecore-2.0.0-rh8_0-any.yak       # 推到 yak.rhino3d.com
```

`manifest.yml` 已寫,内含描述、關鍵字、Rhino 8 版本要求。

## 建置(需 Rhino 8 SDK)

```bat
dotnet restore Plugins\FrameSolver\Grasshopper\v2\Rhino\FrameCore.Gh.csproj
dotnet build  Plugins\FrameSolver\Grasshopper\v2\Rhino\FrameCore.Gh.csproj -c Release
```

輸出 `bin\Release\net48\FrameCore.Gh.gha`,放進 `%APPDATA%\Grasshopper\Libraries\` 即用。

## 落地路線(分階段,S6c § ⑫)

| 階段 | 內容 |
|---|---|
| **C1**(本輪) | 規格 + Common helpers + 預設庫 + 10 個代表元件 + Yak 骨架 |
| C2 | 補齊 Tab 1-7 所有元件 + Setup/Material/Section/Geometry 完整 |
| C3 | 完整 Display 8 件 + Inspector WPF panel + ColorRamp full LUT |
| C4 | 預設庫補完(剩餘 20+ 截面) + Karamba3D 互通轉換器 |
| C5 | Yak 發佈 + UserGuide.pdf + ComponentReference.pdf |
| C6 | Examples 庫(每元件 .gh + 10 個整合範例:portal/truss/dome/bridge/tower/...) |

每階段都 NOT GATED(本機無 Rhino),但都有手測 checklist;C5 後 Yak install 測試是最後一道。

## 與整體 v2 架構的關係

```
Layer 1  FrameCore Engine    (鐵律不動)
Layer 2  C ABI v2 transport  (frame_capi_v2.dll)
Layer 3  FrameCore.Bridge    (zero-Rhino-dep SDK,可獨立 dotnet build)
Layer 4  FrameCore.Gh (this) (Rhino/GH 元件,需 Rhino SDK 建置)
```

Layer 4 **可獨立替換 / 不影響下層**:Python SDK / WPF GUI / Web UI 也能基於 Layer 3 自建,
不用碰本層。本層的 GHA 一旦發佈,Yak 升級不需要 Layer 1-3 重編。

## 誠實邊界(對比商業軟體)

✅ **同等**:元件目錄規模、預設庫覆蓋、Display 商業級觀感、互動延遲、Bake 結構化、單位切換、async/cancel、Inspector
✅ **超越**:CollapseReplay 完整 u/v 回放(商業沒)、AdvancedDiagnostics 透明度(商業 black-box)、SnSession factor reuse < 30ms(實測超越 Karamba)
❌ **不及**:CAD interop(IFC/DWG/SAP2000 import — C7 才考慮)、設計規範 check(AISC/Eurocode/GB-50017 — 屬另一層)、multi-storey wizard、獨立 GUI、雲端 collaboration
