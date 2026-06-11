# FrameCore × Grasshopper 對接(S6 J1)

> **[NOT GATED]** 本資料夾的 C# **不由引擎驗證 gate 建置/測試**——它需要 **.NET / Rhino 8 SDK**,
> 而引擎 CI 無此環境。它是 **參考客戶端骨架**,給 Grasshopper 元件包裝用。引擎端真正被 gate 驗證的
> 是 CLI 文字橋本身(`Tools/cli_roundtrip.py`,gate 第五腿)+ 協議文檔 `docs/CLI_PROTOCOL.md`。

## 這是什麼
S6 對接橋採三步走:**J1 CLI 文字橋(MVP,已做)→ J1.5 daemon 常駐 → J2 C API DLL**。
本資料夾是 J1 的 GH 端起點:

- `FrameCoreClient.cs` — 無 Rhino 依賴的參考客戶端(`System.Diagnostics` shell-out `frame_cli.exe`、解析協議
  成 `FrameResult`)。可直接丟進 GHA 專案,或用 `dotnet`/`csc` 單獨編譯。**本機未建置/未測試**(無 SDK)。

## 為何 GH 元件不入引擎 gate(誠實邊界)
- GH/`.gha` 是 Rhino 8 .NET 生態,與引擎的 C++17/Eigen + UE 驗證鏈分離(WS_J)。盲產未經編譯的 GHA
  元件違反專案「每能力獨立 oracle、誠實驗證」鐵律,故本輪只交付**可被外部環境驗證的骨架 + 權威協議**,
  不假裝「GH 外掛完成」。
- 引擎端契約(CLI 協議)由 `Tools/cli_roundtrip.py` 端到端把關:VERSION 握手、`TONLY`/`SIZEOPT`/`DYNC`
  指令、`MEMBER … tonly` token 全部 round-trip 驗證。GH 端只要遵循 `docs/CLI_PROTOCOL.md` 即與引擎一致。

## 建議的 GH 元件組(待 Rhino 環境實作)
依 WS_J,封裝成讀取器元件:`Assemble`(組模型文字)、`Analyze`(線性 solve)、`BucklingModes`、
`PDelta`、`TensionOnly`、`SizeOpt`、`DynCollapse`。每個都用 `FrameCoreClient.Solve(...)` 驅動同一支
`frame_cli.exe`,讀回 `FrameResult` 對應欄位。

## 待辦(後續階段)
- **J1b**:`DYNC` 完整時間歷程幀(`DynCollapseFrame.u/v`)+ 碎塊交接資料協議(給 UE/Chaos 回放)。
- **J1.5 daemon**:常駐行程持有 `PreparedSystem`/`ReSolveSession` 跨請求(省重複 factor);stale 偵測沿用
  `modelFingerprint`;加「同連線多次 solve == 各自獨立 cli」一致性測。
- **J2**:C API DLL(長期;WS_R2 §10:100k+ DOF 受 factor 支配才催)。
- **MAT cap token**:目前 CLI 材料 allowable 硬編 `make(300,300,180)`;`SIZEOPT`/D-C 需 caller 設 cap 時,
  協議加 `MAT … cap_comp cap_tens cap_shear` optional token。
- **`.gha` 建置 + Yak 發佈**:Rhino 8 .NET、版本握手沿用 CLI 的 `VERSION <sha>` 行(WS_J 工具鏈)。
