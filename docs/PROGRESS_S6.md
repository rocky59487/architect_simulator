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
- **[本輪後續補齊]**:J1b(MAT/SMAT cap token + DYNC `DFRAME` 逐幀)、J1.5(daemon block-loop + EOR)、J2(C API DLL `frame_capi`)**已在同輪 commit `256699a`/`e7f3b45` 完成並 gate 驗證**(見下方「J1b/J1.5/J2 增補」)。
- **[仍 DEFERRED]**:DYNC 完整 u/v + 碎塊二進位串流(給 UE/Chaos)、daemon 真常駐(持 PreparedSystem 跨請求)、`.gha`/Yak(需 Rhino)。

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

## J1b / J1.5 / J2 增補(同輪 commit `256699a` + `e7f3b45`)
- **J1b cap token**:`MAT/SMAT … [capComp [capTens [capShear]]]`(省略=`make(300,300,180)`;1 值→tens=comp、shear=0.6·comp)。
  → CLI SIZEOPT 用真 σa=25000psi 重現 **standalone F44 的 1608.49 lb**(經文字橋的跨驗證)。
- **J1b DYNC 逐幀**:`DYNC` 分支加 `DFRAME t maxAbsU`(每儲存幀峰值|位移|的回放時間軸);完整 u/v+碎塊串流仍延後。
- **J1.5 daemon**:`frame_cli` 對 model BLOCK 迴圈,每塊 `EOR` flush;**同行程多塊==各自獨立 cli 逐位元**(已驗)。
  現為 **batch 多塊**(每塊 fresh factor);**真常駐**(持 `PreparedSystem`/`ReSolveSession` 跨請求省 factor)留後續。
- **J2 C API DLL**:`frame_cli_core.{h,cpp}`(`processAll` 共用核心,輸出改 std::string)+ `frame_capi.{h,cpp}`
  (`frame_capi_solve_text`/`frame_capi_version`)+ `build_capi.bat`(`/LD`)。**與 `frame_cli.exe` 逐位元相等**(ctypes 驗)。
- **gate 第五腿現 8 checks**:+ MAT-cap 10-bar(weight≥文獻 1593.2 + 6 sized 桿 D/C=1 + ==F44)、DYNC DFRAME、daemon 一致、C API 一致。
- ⚠️ **`build_capi.bat` 產物 `frame_capi.dll/.exp/.lib` 未被 `.gitignore` 排除**(鐵律禁碰 .gitignore)→ 一律顯式 stage 源碼、勿 `git add -A`。

## 誠實邊界
- S6 = **文字橋 plumbing**,非新力學;所有力學正確性沿用 S1–S5 既有 oracle(F1-F44/UE/OpenSees/audit79)。整合層,無 novelty。
- **不自稱「GH 外掛完成」**:GH `.gha` 元件需 Rhino 8 .NET SDK,是唯一未完成項;C# 參考客戶端 NOT GATED;引擎端契約由 CLI round-trip(8 checks)把關。
- **CLI 材料 allowable cap**:已加 optional `MAT … cap` token(J1b);**省略時**預設 `make(300,300,180)`。
- **DYNC**:輸出摘要(`DYNC` 行)+ 逐幀峰值(`DFRAME`,J1b);**完整 u/v + 碎塊交接資料二進位串流**(給 UE/Chaos 回放)仍延後。
- **daemon**:已驗多塊一致;**真常駐跨請求重用**(省 factor)仍延後。
- 已知 MINOR(審核揭露,gated 路徑不觸發):SizeOpt 對 cap=0 材料會靜默假收斂(finalDC=inf);Amin=0+零力桿截面不更新;
  CLI DYNC check 為 plumbing(動力正確性由 F37-F39+audit 驗);FrameCoreClient.cs 未非同步讀 stderr(理論死結,NOT GATED)。

## 下一步(待使用者授權)
F 編號下一個 = **F45**;audit 從 **79** 起;UE 從 **43** 起。**S6 引擎端完成**(J1/J1b/J1.5/J2 全 gate 驗證)。
後續:**S7 BESO 拓撲優化**(純引擎主線首選)/ GH `.gha`(需 Rhino)/ daemon 真常駐 / DYNC 完整幀串流(UE 層)。見 `docs/AGENT_PROMPT_S5_S11.md`。
