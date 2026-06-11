# S6 交接級 spec — Grasshopper 對接橋(J1 `frame_cli` 文字橋 MVP)

> 對齊 S1–S5 同款十項。依據:`docs/AGENT_PROMPT_S5_S11.md` §S6、`docs/research/WS_J_gh.md`、
> `WS_K_ecosystem.md`、`WS_R2_experiments.md §10`。起點錨點 `0a16f5e`(S5),五腿前 baseline 四腿全綠
> (F1-F44 / UE 43 / OpenSees PASS / audit 79)。

## ⓪ 範圍邊界(本 commit 做什麼、為何如此切)
S6 三步走 **J1 CLI 文字橋(MVP)→ J1.5 daemon 模式 → J2 C API DLL**。GH C# 元件 / `.gha` / Yak 需
**Rhino 8 .NET SDK**,本機無法建置/測試 → 盲產未驗證 C# 違反「誠實驗證」鐵律。故:
- **[GATED] 本 commit = J1 CLI 橋核心**:`frame_cli` 擴充(`TONLY`/`SIZEOPT`/`DYNC` 指令 + `MEMBER … tonly`
  token + `VERSION` 握手行)+ **CLI 端到端黃金檔測試 `Tools/cli_roundtrip.py` 進 gate(第五腿)**。
- **[NOT GATED 交付]**:`docs/CLI_PROTOCOL.md`(線協議)+ `Grasshopper/` 參考 C# 客戶端骨架
  (純 `System.Diagnostics`,無 Rhino 依賴的解析核心,明標需 .NET/Rhino 建置、不入 gate)。
- **[DEFERRED]**:① DYNC 完整時間歷程 + 碎塊串流協議(給 UE/Chaos 回放,屬 U 層;本輪只出 DYNC **摘要**)
  → J1b;② daemon 常駐(持有 `PreparedSystem`/`ReSolveSession` 跨請求)→ J1.5;③ C API DLL → J2;
  ④ GH `.gha` 元件組 + Yak 發佈 → 需 Rhino 環境。

## ① 目標 / 不做
- **做**:把 S2–S5 新增的分析能力接上 `frame_cli` 文字協議(GH/任何外部客戶端可 shell-out 驅動),
  使 J1 MVP 成立(WS_R2 §10:行程開銷僅 6.7ms,~20k DOF 端到端 2.1s 可用)。版本握手行讓客戶端驗證引擎 build。
- **不做**:一步到位 C API(WS_R2 §10:J1 先夠,100k+ 受 factor 支配才催 daemon);GH UI 元件本身
  (在引擎之上、需 Rhino);DYNC 完整時間歷程串流(本輪摘要;full frames 給 UE 層另設協議)。

## ② CLI 協議擴充(`Standalone/frame_cli.cpp`;stdin 文字、stdout 解析)
既有輸入指令(S1–S3 已有):`MAT/SMAT/SEC/NODE/MEMBER(+active)/SHELL(+active)/NLOAD/UDL/SPRESS/HINGE/OPT/EIGEN/PDELTA/END`。
既有輸出:`SINGULAR/DISP/RF/MF/SF/FREQ/PDSTATUS`。**S6 新增**:
- **輸入**:
  - `MEMBER … rx ry rz [active [tonly]]` — 在 active 之後再加 optional `tonly`(0/1,預設 0)。
  - `TONLY [maxIter [allowReact]]` — 設分析模式=tension-only(主動集 eliminator,讀 `Member.tonly`)。
  - `SIZEOPT <Amin> <maxIter> <dcTol>` — 設模式=FSD 尺寸優化(全 active 桿)。
  - `DYNC <dt> <maxTime> [rid…]` — 設模式=連續動力倒塌;行尾 ids = `initialRemovals`。
- **輸出**(均在 `VERSION` 之後):
  - `VERSION <sha>` — **stdout 第一行握手**(既有 stderr provenance 保留);舊解析器忽略未知 token,**不破壞 OpenSees 腿**。
  - `TONLY <conv> <cycled> <iters>` + `SLACK <id>…`(鬆弛桿)+ 標準 `SINGULAR/DISP/RF/MF/SF`(finalState)。
  - `SIZEOPT <conv> <iters> <singular>` + 每桿 `AREA <id> <A> <DC>` + `WEIGHTVOL <ΣA·L>`。
  - `DYNC <outcome> <nEvents> <nFrames> <Tend>` + 每事件 `DEVENT <t> <mode> <nRemoved> <nDetached>`(摘要)。
- 模式互斥:命令出現即設模式(後者覆蓋);無命令 → PDELTA(若設)→ 線性 solve(維持既有行為逐位元)。

## ③ build / 鏈結
`build_cli.bat` 源檔清單**改為鏡像 `build.bat` 全集**(robust,免 DYNC 傳遞依賴漏檔 link error):
加入 `SelfWeight/Combination/InfluenceLine/BucklingAnalysis/ResponseSpectrum/ModalDynamics/Connectivity/
Collapse/DynamicCollapse/Reanalysis/TensionOnly/SizeOpt`(原本只到 PDeltaAnalysis)。`frame_cli.cpp` 取代 `main.cpp`。

## ④ Oracle / gate(第五腿)
- `Tools/cli_roundtrip.py`(純 Python,**不需 openseespy**):建置 frame_cli(call `build_cli.bat`)→ 對已知模型跑各指令
  → 比對不變量。檢查:
  1. **baseline DISP**:懸臂尖端 `δ=PL³/3EI`(rel<1e-6;驗協議本身 round-trip)。
  2. **VERSION 握手**:stdout 首行為 `VERSION` 且 SHA 非空。
  3. **TONLY**:X 斜撐 portal 純橫向 → `conv=1`、恰一桿 `SLACK`(鏡像 F42 不變量)。
  4. **SIZEOPT**:10-bar truss → `conv=1`、`WEIGHTVOL>0`、桿數=10(鏡像 F44 plumbing;重量不變量已由 F44 嚴格驗)。
  5. **DYNC**:帶 `initialRemoval` 的模型 → 跑通、`outcome ∈ {Stable,Collapsed,MaxSteps}`、`nFrames≥0`(驗 plumbing;
     動力正確性已由 F37–F39 + audit 嚴格驗)。
- **run_gate.ps1 加 [5/5] CLI round-trip 腿**;四腿→**五腿**(standalone / UE / OpenSees / audit / CLI)。
- GH C# 元件**不入 gate**(獨立 repo/CI/Rhino 議題,WS_J)。

## ⑤ 誠實邊界 / 分級
- `[VERIFIED]`(進 gate):CLI round-trip baseline DISP 對解析、TONLY/SIZEOPT/DYNC plumbing 不變量。
- `[NOT GATED]`:`Grasshopper/` C# 參考客戶端 + 協議文檔(本機無 Rhino/.NET SDK 建置;標明需外部環境驗證)。
- `[DEFERRED]`:DYNC 完整時間歷程/碎塊串流(UE 層協議)、daemon(J1.5)、C API(J2)、`.gha`/Yak。
- CLI 是 POD 文字邊界(引擎之上);輸出精度 `%.12g`;模型輸入格式向後相容(新 token 皆 optional)。

## ⑥ 與既有設施關係
- frame_cli 已被 `Tools/opensees_compare.py`(gate OpenSees 腿)建置 + 驅動;新 token/行皆 optional 或未知 token,
  opensees_compare 只解析 `SINGULAR/DISP/MF` → **不受影響**(已查證:其解析迴圈忽略未知 t[0])。
- daemon(J1.5)未來持有 `PreparedSystem`/`ReSolveSession` 跨請求;stale 偵測沿用 `modelFingerprint`。

## ⑦ build/gate 同步義務
- `build_cli.bat` 全集(本輪改)。**不動** `build.bat`/`build_linear_audit.bat`(standalone/audit 不變)。
- `run_gate.ps1`:加第五腿;`$ExpectedUeTests` 維持 **43**(S6 無新 UE 測試 — CLI 屬 standalone 生態)。
- 不加 Member 旗標到 K?**`Member.tonly` 不是新引擎旗標**——`Member.tensionOnly` 早於 S4 存在且已入 fingerprint;
  CLI 的 `tonly` token 只是把它接到文字協議,**引擎零變更、fingerprint 不動**。

## ⑧ 誠實宣稱
- S6-J1 = **文字橋 plumbing**,非新力學;所有力學正確性沿用 S1–S5 既有 oracle。novelty 無(整合層)。
- 不自稱「GH 外掛完成」——GH 元件是 NOT GATED 骨架;誠實標 J1 CLI 橋 = MVP enabler。

## ⑨ 數值證據(實測,五腿 gate 全綠)
| Oracle | 量測 | 門檻 | 實測 |
|---|---|---|---|
| CLI baseline DISP | 懸臂尖端 vs PL³/3EI | rel<1e-6 | **3.13e-13** |
| CLI VERSION 握手 | stdout 首行 SHA 非空 | pass | **PASS**(首行=VERSION,sha 非空) |
| CLI TONLY | conv + 恰一桿 slack | pass | **PASS**(conv=1,slack=[4]) |
| CLI SIZEOPT | conv + 10 桿 + WEIGHTVOL>0 | pass | **PASS**(conv=1,22 迭代,10 AREA,vol>0) |
| CLI DYNC | 跑通 + outcome 非 Invalid | pass | **PASS**(outcome=1 Collapsed) |

> 五腿 gate:standalone **F1–F44** / UE **43** / OpenSees **PASS** / audit **79** / **CLI round-trip ALL PASS**。

## ⑩ 下一步
S6-J1 完成即停,五腿 gate 全綠後 commit/push。後續:J1b DYNC 時間歷程協議 / J1.5 daemon / J2 C API /
GH `.gha`(需 Rhino)。或轉 S7 BESO。見 `docs/AGENT_PROMPT_S5_S11.md`。
