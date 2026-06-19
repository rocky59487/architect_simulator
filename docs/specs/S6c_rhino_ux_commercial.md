# S6c — Rhino / Grasshopper 商業級 UX 規格

> **目標**:把 FrameCore 的 Rhino/Grasshopper 元件套件做到 **Karamba3D / SOFiSTiK Bridge Modeler /
> Robot Structural Analysis** 級別 — 完整元件目錄、預設庫、顯示組件、bake、單位切換、互動 SLA、
> Yak 發佈。**[NOT GATED]**(需 Rhino 8 .NET SDK 建置;引擎 CI 無此環境,本文是設計與骨架)。
>
> 上游依賴:
> - 引擎 = `Plugins/FrameSolver/Source/FrameCore`(鐵律 1 不動)
> - 協議 = `docs/specs/S6b_rhino_bridge_v2.md` § ① ~ ⑬(framed 線協議)
> - profile = `docs/specs/S6b_rhino_bridge_v2.md` § ⑭(simple / advanced)
> - SDK = `Plugins/FrameSolver/Grasshopper/v2/` C# 骨架(zero Rhino dep,本文加 Rhino layer)
> - 對標 = Karamba3D 1.3 元件目錄、SOFiSTiK GH 9.0、Robot SA、Tekla Structural Designer

## ⓪ 商業級的衡量標準(誠實基線)

「跟商業軟體同等」≠ 全等功能,而是 **體驗 + 完整性 + 可靠性 + 文檔** 同等:

| 商業級維度 | 對標 | 本套要達到 |
|---|---|---|
| 元件數 | Karamba3D 1.3 ≈ 50+,SOFiSTiK ≈ 80+ | **60+ 件**(本文 § ② 完整目錄) |
| 預設材料庫 | Karamba 17 種,SOFiSTiK 50+ | **11 種**(常用鋼/混凝土/鋁/木) |
| 預設截面庫 | Karamba ~200 種,SOFiSTiK 數千 | **80+ 種**(GB H-section + 歐標 IPE/HEA + 美標 W) |
| 顯示組件 | Karamba: BeamDisplay/BeamForce/UtilDisplay | **本套同名 + Collapse Replay + Fragment Visualizer + Mode Animation**(獨家) |
| 互動延遲 | Karamba 滑桿動 ~50ms / Robot ~100ms | **目標 < 30ms**(SnSession factor reuse) |
| 文檔 | Karamba 250 頁手冊 + 範例庫 | tooltip + 範例 .gh + `docs/karamba_compat.md` 互通指引 |
| Yak 發佈 | Karamba 全球 Yak | `framecore-2.x-rh8-any.yak` |
| 商業授權 | Karamba 商用 1100 EUR | **本套 MIT / 商用免費**(畢專學術產出) |

**誠實邊界 — 本套 NOT 跟商業同等的維度**(寫出來不騙人):
- ❌ 無 CAD interop(IFC / DWG / SAP2000 binary import)
- ❌ 無設計規範自動 check(AISC / Eurocode / GB-50017 — 屬於 D/C 之上的另一層)
- ❌ 無 multi-storey 樓板 wizard / Building Information Modeling
- ❌ 無 GUI 模式(纯 Grasshopper definition;商業軟體有獨立 GUI)
- ❌ 無雲端 collaboration

## ① UX 設計原則(寫死在元件設計裡)

1. **可預測 — 一秒判讀**:元件名 = 名詞短語(`AssembleModel` 而非 `MakeAModelFromTheseInputs`);輸入順序 = 模型 → 載重 → 選項;輸出順序 = 主要 → 次要 → diagnostics。
2. **可組合 — 強型別 wire**:每元件的輸入/輸出都是 IGH_Goo wrapper 的強型別 `FrameModel` / `LinearResult` 等 — 端口拖錯型別 GH 紅線,不會等到 SolveInstance 才報。
3. **永不卡 Rhino**:任何 solve / set 都走 async,UI thread 不阻塞;`Schedule` callback 完成才 ExpireSolution。
4. **靜默零容忍**:任何引擎填預設都顯示在元件下方 **wire-tip** 黃色 warning(advanced profile 直接 red error)。
5. **滑桿動 → < 30ms**:同元件實例 reuse `FrameSession` 與引擎 `SnSession` / `ReSolveSession`,只重算受影響部分。
6. **GH 預覽 = Rhino 預覽**:結果 `IGH_PreviewObject.DrawViewportMeshes/Wires` 同步;停元件 = 停預覽。
7. **Bake 可重做**:bake 一律進結構化 layer tree,bake 後再 bake 不重複 — 而是 update 同 layer 內物件。
8. **單位顯式**:每個元件右下角顯示當前單位系(`N·mm·MPa` / `kN·m·kPa` / `kip·in·ksi`);全套支援切換。
9. **故障可恢復**:engine session crash → 元件自動重 Open + 重 set model + 提示使用者繼續;不掉資料。
10. **每元件範例 .gh**:Yak 包含 `Examples/` 目錄,每元件對應一個最小範例。

## ② 完整元件目錄(60 件,11 個 GH tab)

GH 元件分 **tab → category** 兩層。tab 用阿拉伯數字前綴強制排序(GH 字母序)。

### Tab 1 — `1. Setup`(7 件)

| Component | Inputs | Outputs | 對應 method |
|---|---|---|---|
| `OpenFrameCore` | `dllPath`(opt)、`profile`(Simple/Advanced) | `Session` | `session.open` |
| `EngineInfo` | `Session` | `buildSha` `version` `schemaVer` `caps[]` | `hello` (cached) |
| `Units` | `system`("SI"/"SI-kN"/"Imperial") | `UnitSystem` | client only |
| `SwitchProfile` | `Session` | `simpleSession` `advancedSession`(autoload pair) | dual-session |
| `Capabilities` | `Session` | `caps[]` `has(cap)` | hello (cached) |
| `EngineDiagnostics`*(advanced)* | `Session` | `factorBackend` `openblasThreads` `recommended` | session.status |
| `CloseFrameCore` | `Session` | `closed` | `session.close` |

\*`*` = advanced profile 專用。

### Tab 2 — `2. Material`(6 件)

| Component | Inputs | Outputs | 備註 |
|---|---|---|---|
| `SteelFromLibrary` | `grade`("S355"/"Q345"/...) | `Material` | 內建 11 種 |
| `ConcreteFromLibrary` | `grade`("C30"/"C50"/...) | `Material` | |
| `CustomMaterial` | `E`,`G`,`ρ`,`fy` | `Material` | beam (nu=0) |
| `ShellMaterial` | `E`,`ν`,`G` | `Material` | SMAT |
| `MaterialCapacity` | `Material`,`comp`,`tens`,`shear` | `Material'` | 補 allowable |
| `MaterialSwatch` | `Material` | (preview color) | viewport color preview |

### Tab 3 — `3. Section`(8 件)

| Component | Inputs | Outputs |
|---|---|---|
| `HSectionFromLibrary` | `name`("HW400x400"/"IPE300"/"W14x82"/...) | `Section` |
| `RectangularSection` | `b`,`d` | `Section` |
| `CircularSection` | `r` | `Section` |
| `PipeSection` | `od`,`t` | `Section` |
| `BoxSection` | `b`,`d`,`t` | `Section` |
| `IShapeCustom` | `bf`,`tf`,`d`,`tw` | `Section` |
| `CustomSection` | `A`,`Iy`,`Iz`,`J`,`cy`,`cz`,`Asy`,`Asz` | `Section` |
| `SectionPreview` | `Section`,`location` | (Rhino curve 預覽) |

### Tab 4 — `4. Geometry`(7 件)

| Component | Inputs | Outputs |
|---|---|---|
| `NodeFromPoint` | `Point3d`,`id`(opt) | `Node` |
| `NodeGrid` | `box`,`nx`,`ny`,`nz` | `Node[]` |
| `MemberFromCurve` | `Curve`,`Material`,`Section`,`refVec`(opt) | `Member` |
| `MemberFromCurves` | `Curve[]`,`Material`,`Section` | `Member[]` |
| `ShellFromMesh` | `Mesh`,`Material`,`t` | `Shell[]` |
| `ShellFromBrep` | `Brep`,`Material`,`t`,`maxEdge` | `Shell[]` |
| `LocalAxesPreview` | `Member` | viewport-only(顯示 x/y/z 三色 frame) |

### Tab 5 — `5. Boundary`(7 件)

| Component | Inputs | Outputs |
|---|---|---|
| `Support` | `Node`,`Ux`,`Uy`,`Uz`,`Rx`,`Ry`,`Rz` | `Node'` |
| `PinnedSupport` | `Node` | `Node'`(平移固定) |
| `FixedSupport` | `Node` | `Node'`(6 全固定) |
| `RollerSupport` | `Node`,`freeDirection` | `Node'` |
| `PrescribedDisplacement` | `Node`,`Δ[6]` | `Node'` |
| `HingeRelease` | `Member`,`releaseFlags[12]` | `Member'` |
| `PlasticHinge` | `Member`,`dof`,`Mp` | `Hinge` |

### Tab 6 — `6. Load`(6 件)

| Component | Inputs | Outputs |
|---|---|---|
| `NodalLoad` | `Node`,`Fx`,`Fy`,`Fz`,`Mx`,`My`,`Mz` | `Load` |
| `MemberUDL` | `Member`,`wx`,`wy`,`wz` | `Load` |
| `ShellPressure` | `Shell`,`p` | `Load` |
| `SelfWeight` | `g`,`direction` | `Load` |
| `LoadCase` | `name`,`Loads[]` | `LoadCase` |
| `LoadCombination` | `cases[]`,`factors[]` | `Combination` |

### Tab 7 — `7. Analyze`(11 件)

| Component | Inputs | Outputs | 對應 method |
|---|---|---|---|
| `AssembleModel` | `Session`,`Nodes`,`Members`,`Shells`,`Materials`,`Sections`,`Loads`,`Hinges` | `EngineSession`,`dofCount` | `model.set` |
| `Solve` | `EngineSession` | `LinearResult` | `solve.linear` |
| `PDelta` | `EngineSession`,`refactor`(bool) | `PDeltaResult` | `solve.pdelta` |
| `TensionOnly` | `EngineSession`,`maxIter`,`allowReact` | `TonlyResult`,`slack[]` | `solve.tension_only` |
| `SizeOpt` | `EngineSession`,`Amin`,`maxIter`,`dcTol` | `SizeOptResult` | `solve.size_opt` |
| `DynCollapse` | `EngineSession`,`dt`,`tMax`,`initialRemovals` | `DynCollapseResult` | `solve.dyn_collapse` |
| `Corotational` | `EngineSession`,`steps`,`tolR` | `CorotResult` | `solve.corotational` |
| `ArcLength` | `EngineSession`,`arcLen`,`steps` | `ArcLengthResult` | `solve.arclength` |
| `Modal` | `EngineSession`,`nModes` | `ModalResult` | `analysis.modal` |
| `Buckling` | `EngineSession`,`nModes` | `BucklingResult` | `analysis.buckling` |
| `ReSolveSession` | `EngineSession`,`memberToggles[]`,`shellToggles[]` | `LinearResult` | `analysis.reanalysis_solve` |

### Tab 8 — `8. Inspect`(5 件 — 滑桿動畫面最常用)

| Component | Inputs | Outputs |
|---|---|---|
| `InspectDisp` | `Result`,`nodeIds[]` | `disp{id → [Ux..Rz]}` |
| `InspectReactions` | `Result`,`nodeIds[]` | `reactions` |
| `InspectMemberForces` | `Result`,`memberIds[]` | `MF{id → endI/endJ}` |
| `InspectShellForces` | `Result`,`shellIds[]` | `SF{id → Mxx/.../Nxy}` |
| `InspectMechanism`*(adv)* | `Result` | `mechCandidates[]` |

### Tab 9 — `9. Display`(8 件 — 商業級體驗的招牌)

| Component | Inputs | Outputs(Preview / Goo) | 描述 |
|---|---|---|---|
| `BMD` | `Result`,`Members[]`,`axis`("Mz"/"My"),`scale` | `IGH_PreviewObject` Polyline + filled shape | 沿桿 BMD 線性插值 + 填色 |
| `SFD` | `Result`,`Members[]`,`axis`("Vy"/"Vz"),`scale` | 同上 | 剪力圖 |
| `NFD` | `Result`,`Members[]`,`scale` | 同上 | 軸力圖 |
| `UtilizationFringe` | `Result`,`Members[]`,`Shells[]`,`ramp`("Viridis"/"Jet"/"Cool") | colored Mesh + legend | D/C 色階 |
| `DeformedShape` | `Result`,`scale`,`asMesh`(bool) | mesh / line | 變形預覽 |
| `ModeAnimation` | `ModalResult`,`modeIdx`,`time`(0..1),`scale` | mesh | 模態形狀動畫 |
| `CollapseReplay` | `DynCollapseResult`,`time`,`scale`,`showFragments` | mesh + fragment shards | DYNC 完整 u/v 回放(advanced) |
| `FragmentVisualizer` | `DynCollapseResult`,`time` | shards + velocity arrows | 碎塊細節(advanced) |

### Tab 10 — `10. Advanced`(7 件 — advanced profile only)

| Component | Inputs | Outputs |
|---|---|---|
| `PivotMarginTrace` | `Result` | plot (line) |
| `SnIrResidualPlot` | `Result` | plot (line) |
| `TierLadderView` | `ReSolveResult` | bar chart |
| `EnergyTrace` | `DynCollapseResult` | line chart |
| `BasisInheritance` | `DynCollapseResult` | residual histogram |
| `DiagnosticStream` | `EngineSession` | live event log |
| `StrictModelValidator` | `AssembledModel` | `errors[]` `warnings[]` |

### Tab 11 — `11. IO`(8 件)

| Component | Inputs | Outputs |
|---|---|---|
| `ImportFromJson` | `path` | `Model` |
| `ImportFromIFC` | `path` | `Model` (deferred) |
| `ExportResultsCsv` | `Result`,`path` | `bytesWritten` |
| `ExportResultsJson` | `Result`,`path` | |
| `ExportResultsXlsx` | `Result`,`path`,`Material[]`,`Section[]` | (XLSX with multi-sheet) |
| `BakeToRhino` | `Result`,`options` | `objectsCreated` |
| `ScreenshotReport` | `Result`,`Views[]`,`path` | (PDF + PNGs) |
| `Karamba3DCompat` | `Karamba.Model` | `FrameModel`(轉換器) |

**總計**:7 + 6 + 8 + 7 + 7 + 6 + 11 + 5 + 8 + 7 + 8 = **80 件**(實際在 60–80 區間,逐個逐個上)。

## ③ 元件視覺規範

### 3.1 圖示

每元件一個 24x24 SVG,放 `Plugins/FrameSolver/Grasshopper/v2/Rhino/Resources/Icons/`。

| Tab | 主色 | 圖示風格 |
|---|---|---|
| 1 Setup | 淺藍 #4A90E2 | 齒輪、插頭 |
| 2 Material | 鋼灰 #7B8A8B | 金屬條 / 立方體 |
| 3 Section | 橙黃 #F5A623 | 斷面剪影 |
| 4 Geometry | 綠 #7ED321 | 桿線 / mesh wireframe |
| 5 Boundary | 紫 #9013FE | 支座箭頭 / 鉸 |
| 6 Load | 紅 #D0021B | 向下箭頭 |
| 7 Analyze | 深藍 #003F87 | 公式 / sigma |
| 8 Inspect | 黃 #FFD700 | 放大鏡 |
| 9 Display | 漸層彩 | 等高線 / 色帶 |
| 10 Advanced | 黑 #1E1E1E | 示波器 / 監控 |
| 11 IO | 灰 #95A5A6 | 檔案 / 雲端 |

### 3.2 wire-tip 狀態顯示

GH 元件正下方 wire-tip(GH 標準的 `Component.Message`)固定格式:

```
[profile] [units] | [time]ms | [errors]/[warnings]
```

範例:
- 綠色 `simple SI · 12ms · ✓` — 正常完成
- 黃色 `simple SI · 18ms · 2 defaults` — 引擎填了 2 個 default(simple 友好)
- 紅色 `advanced SI · ❌ NOT_SPD` — advanced 拒絕 silent fallback
- 灰色 `simple SI · solving...` — async 中

### 3.3 端口命名規範

- 輸入端口名 = **單詞 PascalCase**,單獨節點 = 單數,集合 = 複數(`Node` vs `Nodes`)
- access:單個用 `GH_ParamAccess.item`,集合用 `.list`,巢狀(LoadCases)用 `.tree`
- optional 輸入後加 `?` 後綴(GH 標示風格):`refVec?`
- 預設值在 tooltip 標明:`refVec? (default = (0,0,1))`

## ④ 預設庫(MaterialLibrary + SectionLibrary)

### 4.1 材料(11 種)

| 名稱 | 規範 | E (MPa) | G (MPa) | ν | ρ (t/mm³) | fy (MPa) | cap.comp/tens/shear (MPa) |
|---|---|---|---|---|---|---|---|
| S235 | EN 10025 | 210000 | 80769 | 0.3 | 7.85e-9 | 235 | 156/156/93 |
| S275 | EN 10025 | 210000 | 80769 | 0.3 | 7.85e-9 | 275 | 183/183/110 |
| S355 | EN 10025 | 210000 | 80769 | 0.3 | 7.85e-9 | 355 | 236/236/142 |
| Q235 | GB/T 1591 | 206000 | 79231 | 0.3 | 7.85e-9 | 235 | 156/156/93 |
| Q345 | GB/T 1591 | 206000 | 79231 | 0.3 | 7.85e-9 | 345 | 229/229/138 |
| Q420 | GB/T 1591 | 206000 | 79231 | 0.3 | 7.85e-9 | 420 | 279/279/167 |
| A36 | ASTM A36 | 200000 | 76923 | 0.3 | 7.85e-9 | 250 | 166/166/100 |
| A572 Gr50 | ASTM A572 | 200000 | 76923 | 0.3 | 7.85e-9 | 345 | 229/229/138 |
| C30 | GB 50010 | 30000 | 12500 | 0.2 | 2.5e-9 | 14.3 (fc) | 14.3/1.43/1.5 |
| C50 | GB 50010 | 34500 | 14375 | 0.2 | 2.5e-9 | 23.1 (fc) | 23.1/1.89/1.83 |
| Al6061-T6 | AA | 69000 | 26000 | 0.33 | 2.7e-9 | 276 | 183/183/110 |

cap 由 fy 除以材料安全係數(鋼 1.5,混凝土用 fc'),這是 D/C **彈性篩**的層級,非 RC ultimate(誠實邊界 — 對標 cap.bend 註解)。

### 4.2 截面(代表性 80+,分 4 套)

- **GB H-section**(`HW150x150` ~ `HN700x300`):16 種
- **EU IPE/HEA/HEB**(`IPE100` ~ `HEB1000`):30 種
- **US W-shape**(`W6x12` ~ `W36x300`):20 種
- **Hollow**:Box `□50x4` ~ `□400x16`(8 種)、Pipe `φ48.6x3.2` ~ `φ406x14`(8 種)
- **Channel/Angle**:`C10x15` ~ `C15x50`(代表 5 種)、`L75x6` ~ `L150x12`(5 種)

每筆 = `(name, A, Iy, Iz, J, cy, cz, Asy, Asz, w_per_m)`。

## ⑤ Display 組件設計範式(IGH_PreviewObject)

商業級的「殺手鐧」 — Karamba 也是靠 BeamDisplay/UtilDisplay 紅起來的。

### 5.1 共通介面

```csharp
public abstract class FrameCorePreviewComponent : GH_Component, IGH_PreviewObject
{
    protected IDisposable? _previewSubscription;
    protected List<DrawCommand> _cachedDraws = new();

    // GH preview pipeline -- called every viewport refresh
    public override BoundingBox ClippingBox => /* aggregated bbox */;
    public override void DrawViewportMeshes(IGH_PreviewArgs args) { /* iterate cachedDraws */ }
    public override void DrawViewportWires(IGH_PreviewArgs args)  { /* iterate cachedDraws */ }
    public bool Hidden { get; set; }
    public bool IsPreviewCapable => true;

    // Lifecycle -- cache draws on SolveInstance, clear on RemovedFromDocument
    protected override void SolveInstance(IGH_DataAccess da) { /* fill _cachedDraws */ }
    public override void RemovedFromDocument(GH_Document doc) { _cachedDraws.Clear(); }
}
```

### 5.2 範式 — `BMD`(沿桿彎矩圖)

```csharp
public sealed class BMDComponent : FrameCorePreviewComponent {
    protected override void RegisterInputParams(GH_Component.GH_InputParamManager p) {
        p.AddParameter(new Param_LinearResult(), "Result", "R", "Linear result", item);
        p.AddParameter(new Param_Member(), "Members", "M", "Members to draw on", list);
        p.AddTextParameter("Axis", "A", "Mz / My", item, "Mz");
        p.AddNumberParameter("Scale", "S", "Drawing scale (mm BMD per mm length)", item, 0.001);
    }
    protected override void RegisterOutputParams(GH_Component.GH_OutputParamManager p) {
        p.AddCurveParameter("BMD", "B", "Polyline along each beam", list);
        p.AddMeshParameter("Filled", "F", "Filled BMD shape", list);
    }

    protected override async void SolveInstance(IGH_DataAccess da) {
        var r = ReadResult(da, 0); var members = ReadMembers(da, 1);
        var axis = ReadAxis(da, 2); var scale = ReadScale(da, 3);

        // 沿每桿 N+1 點線性插值 (endI..endJ)、取 Mz/My,沿 local y/z 凸出
        var polylines = new List<Polyline>();
        var meshes    = new List<Mesh>();
        foreach (var m in members) {
            var pair = r.MemberForces[m.Id];
            var n = 20;
            var poly = new Polyline();
            for (int i = 0; i <= n; ++i) {
                double t = (double)i / n;
                double M = axis == "Mz"
                    ? Lerp(pair.Mzi, -pair.Mzj, t)
                    : Lerp(pair.Myi, -pair.Myj, t);
                var basePt = LerpPoint(m.StartPt, m.EndPt, t);
                var normalLocal = axis == "Mz" ? m.LocalY : m.LocalZ;
                poly.Add(basePt + normalLocal * M * scale);
            }
            polylines.Add(poly);
            meshes.Add(BuildFilledMesh(poly, m));
        }
        da.SetDataList(0, polylines);
        da.SetDataList(1, meshes);
        CacheDrawsForPreview(polylines, meshes);  // viewport refresh 用
        UpdateMessage($"BMD on {members.Count} members · {Watch.ElapsedMs}ms");
    }
}
```

### 5.3 範式 — `UtilizationFringe`(D/C 色階)

- 輸入 `Result` + `Members[]` + `Shells[]` + `ramp` enum
- 沿每桿線性 sample D/C(從 endI/endJ 內插),用 colorRamp 染色 tube mesh
- 殼用 corner Mxx/Myy/Mxy 算 vM stress / cap.vm → 染色 facet
- 圖例 mesh:在 viewport 右下角 draw legend bar(0..1)

### 5.4 範式 — `CollapseReplay`(DYNC 完整 u/v 回放)

- 輸入 `DynCollapseResult` + `time` 滑桿(0..tEnd)
- 從 `result.Frames[]`(advanced profile 有 binary u),用兩相鄰幀內插
- 變形原始 mesh,殼斷裂時(`removedShells` 之後)隱藏
- 碎塊(`fragmentClusters[]`)分別繪 shard mesh + 速度向量
- 動畫:GH `Galapagos`-style timer 自動播放,可選 `play/pause/loop`

## ⑥ 互動 SLA — async + cancel + factor reuse

### 6.1 async 模式

GH `GH_Component.SolveInstance` 是同步的。在 wire 上 → 主執行緒。要 async:

```csharp
protected override void SolveInstance(IGH_DataAccess da) {
    // 1. 讀完輸入後立刻 release 主執行緒
    var session = ReadSession(da, 0);
    var model   = ReadModel(da, 1);
    var ct      = _cts.Token;

    _ = Task.Run(async () => {
        try {
            var result = await session.SolveLinearAsync(EngineSession, ct: ct);
            await OnUIThread(() => {
                _result = result;
                ExpireSolution(true);   // 重新觸發 SolveInstance 把 result 寫進 DA
            });
        } catch (OperationCanceledException) { /* 滑桿已動,跳過 */ }
        catch (RemoteException ex) { AddRuntimeMessage(GH_RuntimeMessageLevel.Error, ex.Message); }
    });
    if (_result is null) { AddRuntimeMessage(GH_RuntimeMessageLevel.Remark, "solving..."); return; }
    da.SetData(0, _result);
}
```

### 6.2 cancel — 滑桿動立即取消前一個

每元件實例維持一個 `_cts`,SolveInstance 進來先 `_cts.Cancel(); _cts = new();`。

```csharp
private CancellationTokenSource _cts = new();
protected override void SolveInstance(IGH_DataAccess da) {
    _cts.Cancel();
    _cts = new CancellationTokenSource();
    // ... 用 _cts.Token ...
}
```

### 6.3 factor reuse — `ReSolveSession` 駕馭 toggle

`AssembleModel` 內部把模型送入後保留 `engineSessionId`;當下游 toggle 桿件 active flag 時,走 `ReSolveSession`(spec § B5),引擎 LDLᵀ factor reuse:**滑桿動 → < 30ms**(spec § ⓪ 目標)。

```csharp
// AssembleModelComponent 持有 engineSessionId
// ToggleActiveComponent 收 [memberId, active] tree → 發 analysis.reanalysis_solve
```

## ⑦ Bake 規則

`BakeToRhino` 元件預設 layer 結構:

```
FrameCore /
├─ Model /
│   ├─ Nodes        (point clouds,id 顯示 text dot)
│   ├─ Members      (curves,layer color = D/C)
│   ├─ Shells       (meshes,layer color = D/C)
│   └─ Supports     (anchor blocks)
├─ Loads /
│   ├─ Nodal        (arrows)
│   ├─ UDL          (distributed arrows)
│   └─ Pressure     (mesh normals)
└─ Results /
    ├─ Deformed     (deformed mesh,opt-in 半透明)
    ├─ BMD          (polylines + filled meshes)
    ├─ SFD          (polylines + filled meshes)
    ├─ NFD          (polylines + filled meshes)
    ├─ UtilFringe   (colored mesh)
    └─ Modes /
        ├─ Mode-1   (mode shape 1)
        ├─ Mode-2
        └─ ...
```

bake 行為:
- 同 layer 內,bake 前先**清空**該 layer(避免重複 bake 堆積)
- text dot 顯示 node id / member id / D/C peak / Mz peak 等
- bake 後輸出 `objectsCreated`(int),供下游 GH 元件驗

## ⑧ 單位切換(SI / SI-kN / Imperial)

引擎是 N-mm-MPa(固定)。SDK 層暴露轉換器:

```csharp
public enum UnitSystem { SI_N_mm_MPa, SI_kN_m_kPa, Imperial_kip_in_ksi }

public static class Units {
    public static double LengthToEngine(double v, UnitSystem u) => u switch {
        UnitSystem.SI_kN_m_kPa => v * 1000.0,
        UnitSystem.Imperial_kip_in_ksi => v * 25.4,
        _ => v
    };
    public static double ForceToEngine(double v, UnitSystem u) => u switch {
        UnitSystem.SI_kN_m_kPa => v * 1000.0,
        UnitSystem.Imperial_kip_in_ksi => v * 4448.222,
        _ => v
    };
    public static double StressToEngine(double v, UnitSystem u) => u switch {
        UnitSystem.SI_kN_m_kPa => v * 1e-3,
        UnitSystem.Imperial_kip_in_ksi => v * 6.89476,
        _ => v
    };
    // ... DensityToEngine / MomentToEngine / etc.
}
```

`Units` 元件輸出 `UnitSystem` 物件,每個 input-style 元件(`NodalLoad` / `CustomMaterial` / `RectangularSection`)接受可選 `UnitSystem`(預設 SI_N_mm_MPa);輸入值自動轉成 engine 單位。

顯示元件反向轉換(engine → user-selected display unit)。

## ⑨ Inspector Panel(advanced 殺手鐧)

獨立 `WPF` window(Rhino 8 .NET 7 ok),從 `EngineDiagnostics` 元件右鍵 → "Open Inspector":

- 左欄樹:`Session` → `EngineSession` → `Solve calls`(每 solve 一節點)
- 右欄分頁:
  - **Diagnostics**:`factorBackend`、`factorTimeMs`、`solveTimeMs`、OpenBLAS threads、recommended threads
  - **Pivot trace**:折線 + log scale
  - **IR residual**:折線(supernodal IR)
  - **Tier ladder**:bar chart(ReSolve 每 solve 走 1/2/3)
  - **Energy**:DYNC 能量演化
  - **Stream**:即時 diagnostic event log(advanced + diagnosticStream)

對應 Robot SA 的 "Solver Status" 面板,Karamba 沒這個 — 這是本套 **超越 Karamba 的點**。

## ⑩ Yak 發佈(包裝)

```yaml
# manifest.yml
name: framecore
version: 2.0.0
authors: [rocky59487]
description: FrameCore — 3D elastic FEA engine with progressive collapse, supernodal solver, and a Karamba3D-compatible bridge.
url: https://github.com/rocky59487/architect-
keywords: [structure, fea, analysis, beam, shell, collapse]
icon: framecore-icon.png
```

包含:
- `bin/net48/FrameCore.Bridge.dll`(SDK)
- `bin/net48/FrameCore.Gh.dll`(GH 元件,含 .gha)
- `bin/net48/frame_capi_v2.dll`(in-process C ABI)
- `bin/Standalone/frame_cli_v2.exe`(備用 stdio transport)
- `Examples/`(每元件一個 .gh)
- `Docs/`(`UserGuide.pdf`、`CompComponentReference.pdf`)
- `Resources/Icons/*.svg`
- `Karamba3D_compat.md`(對標互通指引)

發佈到 `https://yak.rhino3d.com/packages/framecore`。

## ⑪ 與 Karamba3D 的互通(`11. IO / Karamba3DCompat`)

提供轉換器:
- `Karamba.Model → FrameCore.Model.FrameModel`(讀 Karamba 元件輸出)
- `LinearResult → Karamba.ModelResult`(回寫,讓 Karamba 視覺化元件可繼續用)

這讓既有 Karamba 用戶**逐元件遷移**而非全 GH 重畫,降低遷移成本(對標 Robot ↔ ETABS 互通策略)。

## ⑫ 落地路線(分階段)

| 階段 | 工作 | gate |
|---|---|---|
| **C1**(本輪) | 規格 + 目錄 + 11 tab namespace + 5-8 個代表元件骨架(NOT GATED,需 Rhino SDK) | C# 標準語法檢查(本機 .NET runtime / 之後 Rhino SDK) |
| C2 | 補齊 Tab 1-7(Setup → Analyze)所有元件 | Rhino 環境手測 |
| C3 | 完整 Display 組件(8 件)+ Inspector Panel | Rhino 環境手測 + screenshot 對標 Karamba |
| C4 | 預設庫(材料 11 種、截面 80+)+ Karamba3D 互通 | 與 Karamba `.3dm` 互測 |
| C5 | Yak 包裝 + Docs(UserGuide.pdf + ComponentReference.pdf) | Yak install 測試 |
| C6 | Examples 庫(每元件一 .gh + 10 個整合範例:portal/truss/dome/bridge/tower/...) | 視覺對標 Karamba example library |

每階段都 **NOT GATED**(本機無 Rhino),但都有獨立可重現的手測 checklist。

## ⑬ 與既有 v2 設計的關係

| 層 | 檔案 | 狀態 |
|---|---|---|
| Layer 1 引擎 | `Source/FrameCore/*` | 不動(鐵律 1)|
| Layer 2 transport / wire | `frame_capi_v2.h` + 將來 `frame_capi_v2.cpp` | 不動 |
| Layer 3 SDK(zero Rhino) | `Grasshopper/v2/Bridge/` `Model/` `Result/` | 不動 |
| **Layer 4 Rhino UX(本文)** | `Grasshopper/v2/Rhino/`(新)| 本輪建立 |

Layer 4 **建在 Layer 3 之上**,引用 `FrameCore.Bridge`,加 Rhino/Grasshopper SDK 依賴。
**FrameCore.Bridge 仍然 zero-Rhino-dep**,讓 Python / Node / WPF host 也能用。

---

## ⑭ 開工檢核(C1 階段)

- [x] `docs/specs/S6c_rhino_ux_commercial.md`(本文)
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/FrameCore.Gh.csproj`(Rhino 8 SDK 引用)
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/FrameCoreGhPlugin.cs`(GH 入口 + Loader)
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/manifest.yml`(Yak 打包)
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/Components/Setup/OpenFrameCoreComponent.cs`
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/Components/Analyze/AssembleModelComponent.cs`
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/Components/Analyze/SolveComponent.cs`
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/Components/Inspect/InspectDispComponent.cs`
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/Components/Display/BMDComponent.cs`
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/Components/Display/UtilizationFringeComponent.cs`
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/Components/Display/CollapseReplayComponent.cs`
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/Components/Advanced/DiagnosticPanelComponent.cs`
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/Library/MaterialLibrary.cs`(11 種)
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/Library/SectionLibrary.cs`(80+)
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/Common/PreviewPipeline.cs`(IGH_PreviewObject 範式)
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/Common/Units.cs`
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/Common/RhinoBaker.cs`
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/Common/UiMessage.cs`(wire-tip status helper)
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Rhino/README.md`(Rhino layer 用法)
