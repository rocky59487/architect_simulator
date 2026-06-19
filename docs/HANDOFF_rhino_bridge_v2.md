# 交接 — Rhino 橋接 v2 + 商業級 UX(設計階段 B1 + C1)

> **[SUPERSEDED by B2]** — B2 完成 + 三輪 P0/P1/P2 修補後的權威交接是
> [`HANDOFF_rhino_bridge_v2_final.md`](HANDOFF_rhino_bridge_v2_final.md);本檔保留作為 B1 階段史料,
> 不反映現況(B2 已 wire 真實 `frame_capi_v2.dll` + 第 6 gate leg 13/13 + 53 檔 C# 骨架,v2.4 release ship)。
> **日期**:2026-06-19。**範圍**:設計 + 骨架,引擎與 gate **未動**(鐵律 1)。
> **狀態**:`[DESIGN]` `[NOT GATED]` — 引擎 dispatcher 未實作、Rhino .gha 未建置。

---

## 一句話交付

完整三份規格 + 四層架構骨架,讓 Rhino .gha 元件可以**像 Karamba3D 一樣好用**(且超越),
同時提供 **simple / advanced 雙 profile**:UE5 直接用 + 官方 Rhino 學術精度路徑並行。

## 四層架構

```
Layer 1  FrameCore 引擎          (鐵律不動)
Layer 2  C ABI v2 + 線協議        (frame_capi_v2.h + 規格 S6b)
Layer 3  FrameCore.Bridge SDK    (zero Rhino dep,可獨立 dotnet build)
Layer 4  FrameCore.Gh GH 元件    (Rhino 8 SDK,規格 S6c)
```

每層上層可獨立替換 / 不影響下層;v1(J1/J2)永久保留並行。

## 三份權威規格(讀這裡)

| 規格 | 內容 |
|---|---|
| `docs/specs/S6b_rhino_bridge_v2.md` | 線協議(framed message + JSON header + binary payload)、20+ method、forward-compat 規則、雙 profile § ⑭ |
| `docs/specs/S6c_rhino_ux_commercial.md` | 80 元件目錄(11 tab)、視覺規範、預設庫、Display 商業級體驗、Bake/單位/Yak |
| `Plugins/FrameSolver/Standalone/frame_capi_v2.h` | C ABI v2:opaque ctx + framed send/recv + cancel + ABI 版本 |

## 骨架已產出(C# 共 36 檔、2200+ lines)

```
Plugins/FrameSolver/Grasshopper/v2/
├─ FrameCore.Bridge.csproj           [Layer 3, zero-Rhino, net8.0]
├─ Bridge/                            ★ 線協議 + transport + session
│  ├─ FrameProtocol.cs               (frame parser/serializer)
│  ├─ ITransport.cs + CApiV2Transport.cs
│  ├─ BridgeOptions.cs + Profiles.cs
│  ├─ FrameSession.cs                (OpenSimpleAsync/OpenAdvancedAsync)
│  └─ FrameSessionExtensions.cs
├─ Model/                             (FrameModel + Builder,強型別 ref)
├─ Result/                            (LinearResult/TonlyResult/...+AdvancedDiagnostics)
│
└─ Rhino/                             [Layer 4, 需 Rhino 8 SDK]
   ├─ FrameCore.Gh.csproj + manifest.yml + plugin entry + Resources
   ├─ Common/                         (Units, UiMessage, AsyncComponent,
   │                                   PreviewPipeline+ColorRamps, RhinoBaker,
   │                                   GooWrappers, GhParameters)
   ├─ Library/                        (MaterialLibrary 11種 + SectionLibrary 40+)
   └─ Components/                     (10 代表性元件,production code)
      ├─ Setup/OpenFrameCoreComponent
      ├─ Material/SteelFromLibrary
      ├─ Section/HSectionFromLibrary
      ├─ Analyze/AssembleModel + Solve
      ├─ Inspect/InspectDisp
      ├─ Display/BMD + UtilizationFringe + CollapseReplay   ★ 商業級招牌
      └─ Advanced/DiagnosticPanel
```

## 雙 profile 摘要(S6b § ⑭)

| | Simple(UE5 友好) | Advanced(官方 Rhino/學術) |
|---|---|---|
| 缺欄位 option | 用引擎預設,結果列 `defaultsApplied[]` | error `VALIDATION_FAILED` |
| supernodal SPD 失敗 | silent fallback LDLT | error `NOT_SPD` |
| singular | `result.Singular = true` | `RemoteException("SINGULAR_SYSTEM")` |
| DYNC 結果 | 摘要 + 峰值 | 完整 binary u/v + 碎塊 cluster 細節 |
| 額外診斷 | 無 | `AdvancedDiagnostics`(pivot trace、IR 殘差、tier ladder、energy trace) |
| C# 入口 | `FrameSession.OpenSimpleAsync` | `FrameSession.OpenAdvancedAsync` |

引擎一行不動,strict 行為在 dispatcher hook 層攔截。

---

## 接續者怎麼開工(分階段、互不阻塞)

### B 線(引擎 dispatcher,**先做這條**)

| | 工作 | 產出 | 影響 |
|---|---|---|---|
| **B2** | `Source/FrameCore/Public/Bridge/Dispatcher.{h,cpp}` + `frame_capi_v2.cpp` + `build_capi_v2.bat`;hello/session.open/close/model.set/solve.linear/inspect.disp(simple full + advanced 部分) | **新 gate leg [6/6]** `Tools/v2_roundtrip.py`:v2 結果 vs v1 文字協議 bit-exact | 五腿仍綠,新第 6 腿護欄 |
| B3 | 補 pdelta/tension_only/size_opt/corot/arclength/modal/buckling | round-trip 覆蓋 8+ method | |
| B4 | DYNC streaming + cancel + binary u/v + 碎塊 | gate 含 streaming 重組驗 | |
| B5 | session.factor_reuse(SnSession + ReSolveSession 整合到 wire) | benchmark:100 patch → 1 factor | |
| B6 | named-pipe / TCP transport(雲端,可選)| 不入主 gate | |
| B7 | Rhino 8 .gha 真實作 + Yak 發佈 | 外部 CI | |

### C 線(Rhino UX 元件,**B2 完成後可並行**)

| | 工作 | 規格 |
|---|---|---|
| **C2** | 補齊 Tab 1-7(Setup→Analyze)所有元件 | S6c § ② 30+ 件 |
| C3 | 完整 Display 8 件 + Inspector WPF panel + ColorRamp full LUT | |
| C4 | 預設庫補完(40+ 截面) + Karamba3D 互通轉換器 | |
| C5 | Yak 發佈 + UserGuide.pdf + ComponentReference.pdf | |
| C6 | Examples 庫(每元件 .gh + 10 整合範例) | |

---

## 鐵律檢查(對齊 `CLAUDE.md`)

| 鐵律 | 影響 |
|---|---|
| 1. FrameCore 純 C++17+Eigen,Eigen 不洩漏 | ✅ Dispatcher 用自帶 mini JSON,零新依賴;header 只用 stdint/stddef |
| 2. 五腿 gate 全綠 | ✅ v2 不改 v1;**B2 新增第 6 腿** v2 round-trip |
| 3. 誠實驗證、不過度宣稱 | ✅ B1/C1 都標 `[DESIGN]`/`[NOT GATED]`;B2 完成才標 `[VERIFIED]` |
| 4. 索引非裸指標 | ✅ wire schema 用 id,Model/Result 用 typed ref |
| 5. commit 衛生 | ✅ 全新檔 + 新目錄,不碰 `.gitignore`/`.uproject`/`LevelSim/`/build 產物 |

---

## 已知 / 不做(誠實邊界)

- 本機只有 .NET 8 runtime、無 SDK → C# 骨架 `dotnet build` 驗證留待裝 SDK 的環境
- 引擎側 v2 impl 與第 6 leg gate 屬 **B2 工作**,本輪未動
- v1 `frame_cli.exe`/`frame_capi.dll` 永久保留,不被替代
- 不引入 gRPC/Protobuf/Cap'n Proto/FlatBuffers(零依賴鐵律)
- Rhino .gha 編譯需 Rhino 8 SDK(本機無),屬 B7 / C5

---

## 一頁式速查

- **想知道協議怎麼演進**:讀 `docs/specs/S6b_rhino_bridge_v2.md` § ⑤(forward-compat 規則)
- **想知道 simple/advanced 差別**:讀 S6b § ⑭(10 silent 點對照表)
- **想開工引擎 dispatcher**:讀 S6b § ⑩(落地路線)+ `frame_capi_v2.h`(C ABI 契約)
- **想知道 Rhino 元件做什麼**:讀 `docs/specs/S6c_rhino_ux_commercial.md` § ②(80 件目錄)
- **想知道商業級體驗的招牌**:讀 S6c § ⑤(Display 元件範式)+ `Components/Display/BMD/Util/CollapseReplay.cs` 三檔
- **想知道怎麼接續**:本文件「接續者怎麼開工」段

設計閉環完整,任何接續者(同一人下次開窗 / 別人)都可以從 B2 或 C2 開工,不需要再決策。
