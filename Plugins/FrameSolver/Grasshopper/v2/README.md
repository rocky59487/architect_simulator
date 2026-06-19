# FrameCore × Rhino 橋接 v2 — 設計概要 & 接續指南

> **狀態**:2026-06-19 設計 + 骨架完成(B1)。引擎 dispatcher 與 round-trip gate 待 B2 開工。
> 既有 v1 (`frame_cli` J1 / `frame_capi.dll` J2) **不替代、不下架**;v2 並行,獨立 DLL/exe。
> 權威協議規格:`docs/specs/S6b_rhino_bridge_v2.md`。

## 三層架構速覽

```
Layer 3  C# SDK (this dir)      ← Grasshopper .gha 在這之上做 GH_Component 子類
         FrameSession / Model/Builder / Result POCO
                  ▼ frames
Layer 2  Transport (4 種,可換)
         CApiV2InProcess  (frame_capi_v2.dll, P/Invoke,B1-B2 主路徑)
         Stdio            (frame_cli_v2.exe,subprocess,B6)
         NamedPipe        (daemon, B6)
         Tcp              (cloud, B6)
                  ▼ dispatcher
Layer 1  FrameCore Engine (不動,鐵律)
         FrameSolver / SnSession / DynamicCollapse / Reanalysis ...
```

## 為什麼這樣設計(一句話一個原因)

- **frame-based 線協議**(magic + flags + 兩個 u32 長度欄)→ 同一份 frame parser 餵 4 種 transport。
- **JSON header + 二進位 payload**(可審計 + 不損精度)→ 文字協議的可讀性 + 大模型結果不被字串吃掉。
- **`hello` 握手 + `capabilities` 集合**(每增能力 = 加一個字串)→ 新功能不破舊 client,client 用 `HasCapability("solve.dyn_collapse")` 探詢。
- **`session.open` 回 opaque session id**(server side reuse PreparedSystem / SnSession / ReSolveSession)→ GH 滑桿動 → `inspect.disp` 重用 factor,微秒級回應。
- **`request` / `response` / `event` / `error` 四種 kind**(streaming + cancel + structured error)→ DYNC/ArcLen 邊跑邊吐幀,GH 動畫面隨時 cancel。
- **強型別 C# Model + Builder + Result POCO**(從不寫字串)→ IDE intellisense / 編譯期 catch / 不用維護 token 順序。
- **每個 Result 攜 `RawHeader`**(forward-compat)→ 引擎加新欄位,舊 SDK 用 `RawHeader.GetProperty(...)` 也讀得到,不用等 SDK 升級。
- **`schemaVer`(string)+ `frame_v2_abi_version()`(int)**(雙版號)→ schema 演化(JSON 欄位)與 ABI 演化(C 簽名)解耦,各自寬鬆 SLA。

## 本目錄(B1 已產出的骨架)

```
v2/
├─ FrameCore.Bridge.csproj          (net8.0, zero Rhino dep, 引擎 CI 可獨立編譯)
├─ Bridge/
│  ├─ BridgeOptions.cs              transport 選擇
│  ├─ ITransport.cs                 Layer 2 抽象
│  ├─ CApiV2Transport.cs            P/Invoke frame_capi_v2.dll
│  ├─ FrameProtocol.cs              frame parser/serializer (純 BinaryPrimitives + JsonDocument)
│  ├─ FrameSession.cs               handshake / dispatch / id demux,IAsyncDisposable
│  └─ FrameSessionExtensions.cs     高階 method(SolveLinearAsync, SetModelAsync, ...)
├─ Model/
│  ├─ FrameModel.cs                 強型別 immutable model + typed refs
│  └─ FrameModelBuilder.cs          fluent builder,出 typed refs
└─ Result/
   └─ FrameResults.cs               LinearResult / TensionOnlyResult / DynCollapseResult ...
```

引擎側對應檔(B1 已產出 header,impl 在 B2):

```
Plugins/FrameSolver/Standalone/frame_capi_v2.h    (C ABI v2 header,設計完整)
```

## 雙 Profile — Simple(UE5 友好) / Advanced(Rhino 官方 / 學術)

> **動機**:現有引擎 API 為了 UE5 直接用,在許多地方做了 silent 預設(material cap 預設
> `300/300/180` MPa、supernodal SPD 失敗自動 fallback LDLT、singular 只 flag 不 throw、DYNC 只回
> 摘要+峰值而非完整 u/v + 碎塊細節 ...)。UE5 場景對的;Rhino 官方 / 學術精度 / CI 嚴格驗證
> 場景需要「**silent 一律暴露**」。

v2 用 **session-level profile** 切兩套:**同引擎、同 ABI、同 wire frame**,在 dispatcher 層攔截
silent 路徑。權威規格見 `docs/specs/S6b_rhino_bridge_v2.md` § ⑭。

| | Simple(預設,UE5 友好) | Advanced(官方 Rhino / Rhino-UE5 橋接) |
|---|---|---|
| 缺欄位 option | 用引擎預設,結果列出 `defaultsApplied[]` | error `VALIDATION_FAILED` |
| material cap 省略 | `make(300,300,180)` | error |
| supernodal SPD 失敗 | silent fallback LDLT | error `NOT_SPD` |
| singular system | `result.Singular = true`(不 throw) | `RemoteException("SINGULAR_SYSTEM")` |
| DYNC 結果 | 摘要 + `DFRAME t maxAbsU` | 預設 binary u/v 每幀 + 碎塊 cluster 細節 |
| 額外診斷 | 無 | 每 result 帶 `AdvancedDiagnostics`(pivot trace、IR 殘差、tier ladder、energy trace ...) |
| diagnostic event 通道 | n/a | opt-in `DiagnosticStream` push 每階段 progress event |
| C# 入口 | `FrameSession.OpenSimpleAsync(...)` | `FrameSession.OpenAdvancedAsync(...)` |
| Option record | `EngineSessionOptions`(有預設) | `AdvancedEngineSessionOptions`(全 `required`) |
| Method 簽名 | 同一份 extension method | 同一份 extension method;`Advanced` 後綴的補上額外 strict overload |

**選擇路徑**:UE5 plugin / 一般 GH 使用者 → simple;官方 .gha 進階元件 / 雲端 Rhino-UE5 橋接 /
`Tools/v2_roundtrip.py` gate / 學術精度驗證 → advanced。

## 用法範例(B2 dispatcher 上線後可用)

### Simple profile — UE5 / 一般 GH 元件

```csharp
using FrameCore.Bridge;
using FrameCore.Bridge.Model;

await using var session = await FrameSession.OpenSimpleAsync(new BridgeOptions
{
    Kind = TransportKind.CApiV2InProcess,
    FrameCapiV2DllPath = @"C:\...\Plugins\FrameSolver\Standalone\frame_capi_v2.dll"
});
Console.WriteLine($"Engine {session.EngineVersion} (sha={session.EngineBuildSha}), schema={session.SchemaVersion}, profile={session.Profile}");

// 2. 探詢能力(SLA:server 加新 capability 不影響舊 client)
if (!session.HasCapability("solve.tension_only"))
    throw new InvalidOperationException("engine too old");

// 3. 建模型(強型別,流暢)
var b = new FrameModelBuilder();
var steel = b.AddSteel();
var sec   = b.AddRectSection(100, 100);
var n0    = b.AddFixedNode(0, 0, 0, 0);
var n1    = b.AddFreeNode (1, 2000, 0, 0);
var m     = b.AddMember(0, n0, n1, steel, sec);
b.AddNodalLoad(n1, fz: 1000);

// 4. server side 開引擎 session 並送模型
var engineSession = await session.OpenEngineSessionAsync();
var dofCount = await session.SetModelAsync(engineSession, b.Build());

// 5. 解線性(可帶 CancellationToken)
var linear = await session.SolveLinearAsync(engineSession, ct: ct);
Console.WriteLine($"tip Uz = {linear.Disp[1][(int)Dof.Uz]:G6}");

// 6. 部分讀(GH 高頻互動專用,只傳 ids,不重新傳整個結果)
var partial = await session.InspectDispAsync(engineSession, new[] { 1 }, ct);

// 7. simple 也能查 silent default:UE5 友好不 throw,但仍然「告訴你」填了什麼
if (linear.DefaultsApplied is { Count: > 0 })
    foreach (var d in linear.DefaultsApplied)
        Console.WriteLine($"engine silently filled: {d}");
```

### Advanced profile — 官方 Rhino / 學術 / Rhino-UE5 橋接

```csharp
using FrameCore.Bridge;
using FrameCore.Bridge.Model;

// 1. 開 advanced session — 引擎需宣告 profile.advanced capability,否則 throw NotSupported
await using var session = await FrameSession.OpenAdvancedAsync(new BridgeOptions
{
    Kind = TransportKind.CApiV2InProcess,
    FrameCapiV2DllPath = dllPath
});

// 2. 引擎 session 用 AdvancedEngineSessionOptions(全 required,編譯期攔下漏設)
var engineSession = await session.OpenEngineSessionAdvancedAsync(new AdvancedEngineSessionOptions
{
    Mode                       = EngineSessionMode.Default,
    PivotTol                   = 1e-12,
    EnableReleases             = false,
    UseTimoshenko              = false,
    UseIncompatibleMembrane    = false,
    UseDKQPlate                = false,
    ShellGeometricStiffness    = false,
    UseWarpingCorrection       = true,
    WarpTolerance              = 1e-6,
    ShellCurvatureMaxAngleDeg  = 22.5,   // 顯式啟用 guard(simple 預設 0 = 隱式關)
    UseSupernodalPrimary       = false,
    SnIrSteps                  = 0,
    SnAmalgMaxCol              = 64,
    SnNumThreads               = 0,
    SnFallbackOnFail           = false,  // 顯式拒絕 silent fallback
    DiagnosticStream           = true    // opt-in 收每階段 progress event
}, ct);

// 3. material 缺 cap 在 advanced 會 reject:
var b = new FrameModelBuilder();
b.AddMaterial(new Material { E=210000, G=80769, Rho=7.85e-9,
                              Cap = new Capacity(300, 300, 180) });  // 必須顯式給 cap
// ... 略 ...

// 4. solve — 結果帶 AdvancedDiagnostics
var linear = await session.SolveLinearAsync(engineSession, ct: ct);
Console.WriteLine($"factor: {linear.AdvancedDiagnostics?.FactorMethod} via {linear.AdvancedDiagnostics?.FactorBackend}, " +
                  $"factorMs={linear.AdvancedDiagnostics?.FactorTimeMs:F2}, " +
                  $"pivotTrace=[{string.Join(',', linear.AdvancedDiagnostics?.PivotMarginTrace ?? Array.Empty<double>())}]");

// 5. singular 不再 silent — throw
try {
    var unstable = await session.SolveLinearAsync(engineSession, ct: ct);
} catch (RemoteException ex) when (ex.Code == "SINGULAR_SYSTEM") {
    Console.WriteLine($"engine refused silent singular: {ex.Message}");
}
```

```csharp
// DYNC 串流範例(B4):
await foreach (var frame in session.StreamDynCollapseAsync(engineSession,
                                                            dt: 1e-3, maxTime: 0.5,
                                                            initialRemovals: new[] { 12, 34 },
                                                            ct: ct))
{
    // frame is a WireFrame; the kind + body fields tell you whether this is a progress
    // event, a dyn_collapse.event, a dyn_collapse.frame, or the final response.
    var kind = frame.Header.RootElement.GetProperty("kind").GetString();
    // ... map to your GH preview ...
    if (frame.IsEndOfResponse) break;
}
```

## 接續者待辦(B2 開始)

1. **B2 — 引擎 dispatcher 雛形**
   - 新檔:`Plugins/FrameSolver/Source/FrameCore/Public/Bridge/Dispatcher.{h,cpp}`(共用 v1 stdio / v1 cli core / v2 dll / v2 cli core)。
   - 新檔:`Plugins/FrameSolver/Standalone/frame_capi_v2.cpp`(impl frame_capi_v2.h 的 8 個導出)。
   - 新檔:`Plugins/FrameSolver/Standalone/frame_cli_v2.cpp`(把 stdin 切 frame、stdout 寫 frame)。
   - 新檔:`Plugins/FrameSolver/Standalone/build_capi_v2.bat` / `build_cli_v2.bat`(mirror 既有的)。
   - **新 gate leg [6/6]**:`Tools/v2_roundtrip.py` 對 6 個樣本(F1 懸臂、F2 tonly portal、F5 殼板、F37 DYNC、模態、屈曲)走 v2 拿到的結果 vs v1 文字協議 **bit-exact**。
   - 第一個可用 method 集合:`hello / session.open / session.close / model.set / solve.linear / inspect.disp`。

2. **B3 — 補齊 method**
   - `solve.pdelta` / `solve.tension_only` / `solve.size_opt` / `solve.corotational` / `solve.arclength` / `analysis.modal` / `analysis.buckling`。
   - 對應 client extension(SolvePDeltaAsync...)補進 `FrameSessionExtensions.cs`。

3. **B4 — streaming + cancel + binary payload**
   - `solve.dyn_collapse` 邊跑邊吐 `dyn_collapse.event` / `dyn_collapse.frame`,每幀 u/v 走 binary payload(96 bytes / node)。
   - 引擎內部 cancel token(per session 一個 atomic flag,collapse driver 每 N 步檢查)。

4. **B5 — session factor-reuse**
   - mode=`resolve` → `ReSolveSession` 包進來,`model.patch` method 走 toggle active。
   - mode=`supernodal` → `SnSession` 包進來。
   - benchmark:同模型 100 次 patch → factor 只跑一次。

5. **B6 — 其他 transport**(可選)
   - `Stdio` / `NamedPipe` / `Tcp`(共用 dispatcher,只實作 ITransport 三個 method)。

6. **B7 — Rhino 8 .gha 元件實作**(需 Rhino 環境,不入引擎 gate)
   - `OpenFrameCore`、`Material`、`RectSection`、`MemberFromCurve`、`Solve`、`InspectDisp`、`BMD`、`DCFringe` 等(詳見 spec § ⑧ 表)。

## 與 v1 的關係

| 路徑 | 狀態 | 客戶端 |
|---|---|---|
| `frame_cli.exe` stdin/stdout 文字(J1/J1.5) | ✅ 永久保留 | 既有 GH 元件、`Tools/opensees_compare.py`、`cli_roundtrip.py` |
| `frame_capi.dll` `frame_capi_solve_text`(J2) | ✅ 永久保留 | 既有 P/Invoke 客戶端 |
| `frame_capi_v2.dll`(本協議) | 🚧 設計完成,impl B2 | 新 .gha、新 Python SDK |
| `frame_cli_v2.exe`(本協議,stdio framed) | 🚧 B6 | 雲端 / split-machine |

**永不破舊**:v1 兩個入口完全獨立,不會因為 v2 演進而動。

## 鐵律檢查(對齊 `CLAUDE.md`)

- ✅ FrameCore 純 C++17 + Eigen,Eigen 不洩漏 — v2 header 只用 stdint/stddef,impl 用自帶 mini JSON。
- ✅ 五腿 gate 不破 — v2 不改 v1;新增第 6 腿護欄。
- ✅ 索引而非裸指標 — Model/Result 全用 id;wire schema 不出現指標。
- ✅ commit 衛生 — 新檔 + 新目錄,不碰 `.gitignore` / `.uproject` / `Plugins/LevelSim`。
- ✅ 誠實驗證 — B1 只交付設計 + 骨架,引擎側 impl 與 gate 列為 B2 工作;**本目錄 C# 可獨立 `dotnet build` 驗,但引擎側對接尚未存在**(誠實分級 `[DESIGN]`,不是 `[VERIFIED]`)。

## 已知 / 不做

- **不重寫力學引擎**,純 transport + schema 重設計。
- **不引入 gRPC / Protobuf / Cap'n Proto / FlatBuffers**(零依賴鐵律)。
- **不解決 Rhino SDK 編譯**,B7 由 Rhino 環境的人接手。
- **不做 v1↔v2 自動橋接** — 客戶端二選一,server 同時開兩個 DLL/exe 入口。

---

延伸閱讀:`docs/specs/S6b_rhino_bridge_v2.md`(權威協議規格)
