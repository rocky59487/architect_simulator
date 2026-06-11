# S6 進度日誌 — Grasshopper 對接橋(J1 `frame_cli` 文字橋 MVP)

> 接續 `PROGRESS_S5.md`(S5 結案於 `0a16f5e`)。本階段把 S2–S5 新增的分析能力接上 `frame_cli` 文字協議,
> 使 GH/任何外部客戶端可 shell-out 驅動引擎(J1 MVP)。spec = `docs/specs/S6_gh_bridge.md`。
> **政策升級:四腿 gate → 五腿 gate**(新增 CLI round-trip 腿)。

## 基準
- 起點 `0a16f5e`(S5)。working tree 既有雜項(`.gitignore`/`ArchSim.uproject`/`Plugins/LevelSim/`)**非本人改動,全程不碰**。
- baseline 四腿綠:standalone F1–F44 / UE 43 / OpenSees PASS / audit 79。
- 使用者「繼續」授權動工 S6(`AskUserQuestion` 選定 S6 GH 對接橋)。

## 範圍決策(誠實)
S6 三步走 **J1 CLI 文字橋 → J1.5 daemon → J2 C API**。GH C# 元件 / `.gha` / Yak 需 **Rhino 8 .NET SDK**,
本機無法建置/測試 → 盲產未驗證 C# 違反「誠實驗證」鐵律。故本 commit:
- **[GATED] J1 CLI 橋核心**:frame_cli 擴充 + `Tools/cli_roundtrip.py` 端到端測試(gate 第五腿)。
- **[NOT GATED 交付]**:`docs/CLI_PROTOCOL.md` + `Plugins/FrameSolver/Grasshopper/`(參考 C# 客戶端 + README,明標需 Rhino/.NET、不入 gate)。
- **[DEFERRED]**:DYNC 完整時間歷程/碎塊串流(J1b,給 UE/Chaos)、daemon(J1.5)、C API(J2)、`.gha`/Yak、MAT cap token。

## 交付內容(單一 S6 commit)
### 新增
- `Tools/cli_roundtrip.py` — CLI 端到端黃金檔測試(純 Python,不需 openseespy):建置 frame_cli → 跑各指令 → 比對不變量。gate 第五腿。
- `docs/CLI_PROTOCOL.md` — 權威線協議(輸入指令/輸出行/範例/限制)。
- `Plugins/FrameSolver/Grasshopper/FrameCoreClient.cs` — **[NOT GATED]** 無 Rhino 依賴的參考客戶端(shell-out frame_cli、解析成 `FrameResult`)。
- `Plugins/FrameSolver/Grasshopper/README.md` — GH 對接計畫 + 誠實邊界(GH 元件不入 gate 之由)。

### 修改
- `Standalone/frame_cli.cpp`:`VERSION <sha>` stdout 握手行(第一行)、`MEMBER … tonly` token、`TONLY`/`SIZEOPT`/`DYNC` 指令
  (各設分析模式,互斥;無命令 → PDELTA→線性,既有行為逐位元不變)、`printState` 共用輸出。協議註解同步更新。
- `Standalone/build_cli.bat`:源檔清單**改為鏡像 `build.bat` 全集**(robust,免 DYNC 傳遞依賴 link error;加
  SelfWeight/Combination/InfluenceLine/Buckling/ResponseSpectrum/ModalDynamics/Connectivity/Collapse/DynamicCollapse/Reanalysis/TensionOnly/SizeOpt)。
- `Scripts/run_gate.ps1`:加 **[5/5] CLI round-trip** 腿;verdict 納入 `$CliRC`;`$ExpectedUeTests` 維持 **43**(S6 無新 UE 測試)。

## 協議擴充摘要
- **輸入新增**:`MEMBER … [active [tonly]]`、`TONLY [maxIter [allowReact]]`、`SIZEOPT Amin maxIter dcTol`、`DYNC dt maxTime [rid…]`。
- **輸出新增**:`VERSION sha`(首行握手)、`TONLY conv cycled iters`+`SLACK id…`、`SIZEOPT conv iters singular`+`AREA id A DC`+`WEIGHTVOL ΣA·L`、`DYNC outcome nEvents nFrames Tend`+`DEVENT t mode nRem nDet`。
- 向後相容:新行皆 optional token / 未知 token;`Tools/opensees_compare.py`(OpenSees 腿)只解析 `SINGULAR/DISP/MF`,**不受影響**(已查證 + gate 實證)。

## 數值證據(本輪實測)
| Oracle | 量測 | 門檻 | 實測 |
|---|---|---|---|
| CLI baseline DISP | 懸臂尖端 vs PL³/3EI | rel<1e-6 | **3.13e-13** |
| CLI VERSION 握手 | stdout 首行 SHA 非空 | pass | **PASS** |
| CLI TONLY | conv + 恰一桿 slack | pass | **PASS**(slack=[4]) |
| CLI SIZEOPT | conv + 10 桿 + vol>0 | pass | **PASS**(22 迭代) |
| CLI DYNC | 跑通 + outcome 非 Invalid | pass | **PASS**(Collapsed) |

## 五腿 gate(commit 前)
`run_gate.ps1 -RequireOpenSees` → **GATE: PASS** — standalone **F1–F44** / UE **43** / OpenSees **PASS** /
deep audit **checks=79** / **CLI round-trip ALL PASS**。
⚠️ 踩雷:① `ensure_built()` 用 `text=True` 解碼 cl.exe 中文(cp950)輸出 → reader thread `UnicodeDecodeError`
traceback 雜訊(不影響 returncode/結果,但不該出現)→ 加 `errors="replace"` 根治。② cwd 漂移:Bash 在
`E:\project`,跑 Tools 腳本要先 `cd /e/project/ArchSim`。

## 誠實邊界
- S6-J1 = **文字橋 plumbing**,非新力學;所有力學正確性沿用 S1–S5 既有 oracle。整合層,無 novelty。
- **不自稱「GH 外掛完成」**:GH C# 是 NOT GATED 骨架(需 Rhino 環境驗證);引擎端契約由 CLI round-trip 把關。
- **CLI 材料 allowable cap 硬編 `make(300,300,180)`**:`SIZEOPT`/D-C 隨之縮放(CLI SIZEOPT 測試只驗 plumbing,
  重量不變量由 standalone F44 嚴格驗);caller 設 cap 的 `MAT … cap` token 延後 J1b。
- **DYNC 只出摘要**;完整時間歷程/碎塊串流延後 J1b(UE 層協議)。

## 下一步(待使用者授權)
F 編號下一個 = **F45**;audit 從 **79** 起;UE 從 **43** 起。S6-J1 完成即停。後續:J1b(DYNC 時間歷程 +
MAT cap token)/ J1.5 daemon / J2 C API / GH `.gha`(需 Rhino),或轉 **S7 BESO 拓撲優化**。見 `docs/AGENT_PROMPT_S5_S11.md`。
