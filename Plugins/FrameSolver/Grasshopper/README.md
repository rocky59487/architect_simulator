# FrameCore × Grasshopper 對接(S6)

> **[NOT GATED]** 本資料夾的 C# **不由引擎驗證 gate 建置/測試**(需 .NET / Rhino 8 SDK,引擎 CI 無此環境)。
> 引擎端被 gate 驗證的是 **CLI 文字橋 + C API DLL**(`Tools/cli_roundtrip.py`,gate 第五腿)+ 協議
> `docs/CLI_PROTOCOL.md`。GH 端只要遵循該協議即與引擎一致。

## 引擎端進度(已 gate 驗證)
S6 對接橋三步走,引擎端**全部完成並驗證**:
- **J1 CLI 文字橋**:`frame_cli.exe` stdin/stdout 協議(LINEAR/PDELTA/TONLY/SIZEOPT/DYNC + VERSION 握手)。
- **J1b**:`MAT … cap` token(真 allowable → CLI SIZEOPT 重現 F44 的 1608.49 lb)+ DYNC `DFRAME` 逐幀時間軸。
- **J1.5 daemon**:`frame_cli` 對 model BLOCK 迴圈,每塊 `EOR` flush;**同行程多塊 == 各自獨立 cli 逐位元**(已驗)。
- **J2 C API DLL**:`frame_capi.dll`(`frame_capi_solve_text` / `frame_capi_version`,`frame_capi.h`);
  **與 `frame_cli.exe` 輸出逐位元相等**(ctypes 驗於 gate)。免行程開銷的長期 transport。

> 唯一**未完成**的是 GH `.gha` 元件本身(需 Rhino 8 .NET SDK 建置 + Yak 發佈)——見下方接線指南。

## 兩種 transport(GH 元件二選一)
1. **`frame_cli.exe`(行程)**:用 `FrameCoreClient.cs`(本資料夾;無 Rhino 依賴,`System.Diagnostics`
   shell-out + 解析成 `FrameResult`)。行程開銷僅 ~6.7ms(WS_R2 §10),~20k DOF 端到端 2.1s 可用。
2. **`frame_capi.dll`(P/Invoke)**:`[DllImport("frame_capi.dll")] int frame_capi_solve_text(string in, byte[] out, int cap);`
   兩段呼叫(先傳 cap=0 取長度、再填)。免行程啟動,適合 100k+ DOF 或高頻互動。協議文字與 CLI 完全相同。

## 建議的 GH 元件組(待 Rhino 環境實作)
依 WS_J,封裝成讀取器元件,各自用上面任一 transport 驅動同一協議:

| GH 元件 | 送出(輸入塊) | 讀回(`FrameResult`) |
|---|---|---|
| `Assemble` | 把 GH 幾何/載重轉成 MAT/SEC/NODE/MEMBER/SHELL/NLOAD/... 文字塊 | (模型文字,餵下游) |
| `Analyze` | 模型塊 + `END` | `Disp` / `Reactions` / `MemberForces` |
| `BucklingModes` | 模型塊 + `EIGEN n` | `FREQ`(擴 `FrameResult` 解析) |
| `PDelta` | 模型塊 + `PDELTA 0/1` | `PDSTATUS` + 二階 `Disp` |
| `TensionOnly` | 模型塊(`MEMBER … tonly`)+ `TONLY` | `TensionOnly` / `Slack` |
| `SizeOpt` | 模型塊(`MAT … cap`)+ `SIZEOPT Amin maxIter dcTol` | `Areas` / `WeightVolume` |
| `DynCollapse` | 模型塊 + `DYNC dt maxTime [rid…]` | `DynCollapse` + `DEVENT`/`DFRAME` 時間軸 |

每個元件 = 一個 `GH_Component` 子類(`Grasshopper.Kernel`),`SolveInstance` 內呼叫
`FrameCoreClient.Solve(text)`(或 P/Invoke DLL),把 `FrameResult` 欄位輸出到 GH 端口。
`Assemble` 是純文字組裝(不呼叫引擎),其餘是讀取器。

## 待辦(後續)
- **`.gha` 建置**:Rhino 8 .NET 專案參考 `Grasshopper`/`RhinoCommon`,實作上表 7 元件 + 圖示;
  版本握手用協議的 `VERSION <sha>` 行;Yak 打包發佈(WS_J 工具鏈)。**需 Rhino 環境,不在引擎 CI。**
- **J1b+**:DYNC 完整 u/v 幀 + 碎塊交接資料(`DynCollapseFrame`/`FragmentCluster`)的二進位/分塊協議
  → UE/Chaos 回放(屬 U 層,非 GH)。
- **J1.5+ 真常駐**:daemon 持有 `PreparedSystem`/`ReSolveSession` 跨請求省 factor(現為 batch 多塊;
  stale 偵測沿用 `modelFingerprint`)。
- **C API 擴充**:flat 數值 ABI(免文字解析)若 profiling 顯示文字成為瓶頸再加。
