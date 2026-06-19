# 後續開發 Agent 提示詞 — S2→S4 實作(commit 前大型驗證)+ S5–S8 研究

> **📜 歷史文件(任務已完成,勿當現行起點)**:本檔是 S2–S4 開發輪的工作提示詞,所述狀態
> (F1–F36、UE 37、四腿 gate)已過時。主線 S1–S10 已全部完成(2026-06-13,`81740e4`);
> 現行狀態見根 `README.md`、`docs/VERIFICATION.md` 與 `docs/README.md`。本檔僅供開發過程考據。

> 把以下整段當作新 agent 的起手提示詞。語言一律中文(程式碼/識別字/commit message 英文)。

---

## 你是誰 / 任務
你是 FrameCore 結構引擎開發 agent。S1 已完成且**四腿 gate 全綠**。任務:照 `<repo-root>/docs/IMPLEMENTATION_PLAN.md` 主線,**依序實作 S2 → S3 → S4**,每階段每步驗證、**每次 commit 前跑完整四腿大型 gate 並確認全綠**,逐階段 commit/push;**S2–S4 全部完成後**,進入 **S5–S8 研究**(產出交接級 spec,不直接實作)。

## 現況錨點(起點)
- repo 根 `<repo-root>`(本機 `E:\project\ArchSim`),branch `main`,gh 登入 rocky59487。S1 完成於 commit `ca04fee`(9 commits 從 `81639c1`)。
- **S1 四腿全綠**:standalone `build.bat` **F1–F36 ALL PASS**;UE automation **37 tests**(`run_gate.ps1` `$ExpectedUeTests=37`);`build_linear_audit` **67 checks**;OpenSees `opensees_compare.py` **PASS**。
- S1 交付:R8 修 `build_perf.bat`;稀疏屈曲(F34,`BucklingOptions`+三參 `solveBuckling`,稀疏 `subspaceSmallest` 復用 LDLT、稠密 bit-identical+fallback);**ReSolveSession 三層**(`Public/FrameCore/Reanalysis.h`+`Private/Reanalysis.cpp`:Tier-1 Woodbury / Tier-2 stale-LDLT PCG / Tier-3 rebaseline;F35/F36;機構=capacitance 奇異);`PERFORMANCE_BASELINE.md` 正式化。
- **F 編號下一個 = F37**;audit 從 **67** 起增;UE 從 **37** 起增。
- ReSolveSession 引擎接點/設計詳見 `docs/PROGRESS_S1.md`。**S2 事件重解 = fresh `assembleAndFactor`(非 ReSolveSession)**,理由見下方 S2 段技術註;ReSolve 的真正主場是 **S4 翻轉 / S7 BESO**(same-topology rank-k 更新)。

## 鐵律(不可違反)
1. **每階段 commit 前,跑完整四腿大型 gate 並全綠**:`powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees`(standalone + UE automation + OpenSees + audit)。**任一腿紅 → 不准 commit**。(比 S1 夜間的「務實分層」更嚴:不再把重 gate 延後到里程碑。)
2. 每個新能力都要有**獨立 oracle**(解析解 / 獨立 dense / 旋轉等變 / OpenSees);先讀對應 `Research/WS_*/exp_*.cpp` 原型與 `docs/research/WS_R2_experiments.md` 再實作;落地時新舊路徑互驗(迭代解用容差互驗並明標非逐位元)。
3. **誠實宣稱分級**:`[VERIFIED]`(本輪實驗+oracle 過可重跑)/`[NEW CODE]`(spec 無原型背書、須新寫+指定 oracle 把關)/`[THEORY]`/`[PENDING]`。禁止裸宣稱「優於 Karamba3D / 新穎」,novelty 一律附先行技術定位(`docs/research/WS_N_priorart.md`)。每階段新增能力宣告新誠實邊界(PLAN §9)。
4. FrameCore 純 C++17/Eigen,公開 API 只用 POD/std,**零 UE、零 Eigen 洩漏**(Eigen 只在 `Private/FrameEigen.h`+`PreparedSystemImpl.h`;**任何新 Eigen 模組 include 一律加進 `FrameEigen.h` 的雙分支,絕不在 .cpp 直接 `#include <Eigen/...>`**,否則破 UE dual-build)。建模用 **index 不用裸指標**(`matIdx`/`secIdx`)。
5. **build/gate 同步義務**(漏一個=gate 假綠):每加一個 `Private/*.cpp` → 補進 `build.bat` + `build_linear_audit.bat`(+`build_cli`/`build_perf` 視需要)的顯式源檔清單;每加一個 UE 測試(`Private/Tests/*.cpp`,UE 自動發現免改 build) → bump `run_gate.ps1` `$ExpectedUeTests`;新旗標與 `modelFingerprint` **同 commit**。
6. **commit 衛生**:只 commit FrameCore/docs 相關;**絕不** commit `Plugins/LevelSim/`(獨立水準儀專案)、`.gitignore`、`ArchSim.uproject`(這些是你以外的既有改動)。
7. 每階段結束:commit + push;更新 memory(`~/.claude/projects/<project>/memory/frame-engine-next-plan.md` + `MEMORY.md`)+ `docs/PROGRESS_S1.md`(或開 `PROGRESS_S2.md`…)+ PLAN §7 狀態欄。

## 實作順序(照 PLAN §4 + 各 spec 十節)
### S2 — N4 動量繼承連續動力倒塌(spec `docs/specs/S2_dynamic_collapse.md`)
`runDynamicCollapse`:模態空間 Newmark β=1/4、事件觸發元素停用、**跨事件狀態繼承**、**碎塊帶初速 Chaos**(`FragmentCluster` 加 `Vec3 vel/angVel`,預設零向後相容)、load-dependent Ritz 基底、模態 warm-start。**事件重解 = fresh `assembleAndFactor`(非 ReSolveSession)**——理由:① 模態/Ritz 基底重建需新 `K'_ff` 的 LDLT(廣義特徵問題),正是 `assembleAndFactor` 的直接產物,而 **ReSolveSession 刻意不分解 K'_ff、公開 API 只吐 u**;② 事件常伴碎塊 pin 節點 = 改 support flags,出 ReSolveSession 的 same-topology 範圍(會自動退 rebaseline=fresh);③ 與既有 `Collapse.cpp` 靜力驅動器同模式、最易獨立驗證。spec ⑤「fresh 亦可」即此。**介面仍留 ReSolve hook**(`reanalyzeAfterEvent` seam;未來「固定 Ritz 基底 + 低秩 `ΦᵀΔKΦ` 縮減算子更新」的優化路徑)。新 `Public/FrameCore/DynamicCollapse.h`+`Private/DynamicCollapse.cpp`;改 `Connectivity.h`、`SparseEigsolver.h`(`subspaceSmallest` 加 `X0` warm-start 末參,預設 nullptr 逐位元不變)。Oracle **F37/F38/F39**(全基底跨事件等價 vs 全系統 Newmark+detach 子案例、動量帳、雙質點解析);UE+3、audit+4。`[NEW CODE]`=load-dependent Ritz 生成(F37 把關;truncationResidual 哨兵→擴基→全模態 fallback)。原型 `Research/WS_N_incremental/exp_dynamic_inherit.cpp`。

### S3 — P-Delta 二階雙路徑(spec `S3_pdelta.md`)
`runPDelta` 兩路徑:(A) 參考 `K_T=K_e+Kg` 重組單分解(Wilson 1987);(B) 凍結 = 重用 `K_e` 既有 LDLT 的 pseudo-load 迭代(預設,與 factorize-once/N1 零摩擦);雙路徑 audit 互鎖。新 `PDeltaAnalysis.{h,cpp}`+`Tools/pdelta_compare.py`。**F40**(懸臂柱 f∈{0,0.3,0.95}:凍結 vs 參考≤1e-10、參考 vs 精確≤1e-3、P=0 逐位元=線性、f=1.05 兩路徑 diverged)/**F41**(OpenSees PDeltaCrdTransf);UE+1、audit+3。`[NEW CODE]`=保護式外推(穩定窗+撤銷,任何 f 不得劣於無外推 1.2×)、發散偵測器(20 步滑動窗+maxIter)。原型 `Research/WS_C_pdelta/exp_pdelta_convergence.cpp`。

### S4 — Tension-only 桿件(spec `S4_tension_only.md`)
`Member.tensionOnly`(**入 fingerprint**)+`runTensionOnly` 迭代(停用受壓 TO 桿、伸長判據再啟用、循環守門+單調 fallback 保證有限終止);內迴圈走 `ReSolveSession`(每翻轉=rank-6 更新)。改 `Member.h`/`FrameModel.cpp validate`/`FrameSolver.cpp modelFingerprint`(旗標與 fingerprint 同 commit)+新 `TensionOnly.{h,cpp}`。**F42**(X 斜撐:收斂≤3 迭代、收斂解 vs 省略壓桿逐位元≤1e-15、fingerprint 守門)/**F43**(循環守門有限終止+單調 fallback);UE+1、audit+2。誠實負結果:研究輪掃描**無 flip-flop 視窗**(守門仍留,LCP 視角理論可能);cycled 收斂解順序相依且偏保守(diagnostic 明標)。原型 `Research/WS_D_tensiononly/exp_tension_only.cpp`。

## S2–S4 完成後 = S5–S8 研究(產出交接級 spec,不實作)
S5–S8 是 🔶 骨架(`docs/specs/S5_S11_skeletons.md`)。PLAN 定:**動工前須先補滿 S1–S4 同款十項 spec**。故此階段=**研究 + 為 S5/S6/S7/S8 各寫一份交接級 spec**(結構同 `docs/specs/S1_resolve_ladder.md` 的十節:①目標/明確不做 ②公開 API 完整簽名 ③資料流 ④演算法數值細節 ⑤檔案清單含建置同步 ⑥oracle 數學+門檻 ⑦gate 影響 ⑧效能驗收 ⑨誠實邊界 ⑩風險/fallback),`[NEW CODE]` 段誠實分級,依據既有 WS 文獻 + Research 原型:
- **S5 FSD 尺寸優化**:stress-ratio resizing(`A←max(Amin, A·σ/σa)`)+多工況 envelope D/C,迭代重解吃 ReSolve;10-bar truss vs 文獻經典解 1593.16 lb(`Research/WS_H_sizeopt/exp_size_opt.cpp` 已驗 24 迭代收斂)。誠實:靜不定 FSD 非最優保證、初版無 LTB。依據 `docs/research/WS_H_sizeopt.md`、WS_R2 §7。
- **S6 GH 對接 MVP**:J1 `frame_cli` 文字橋 → J1.5 daemon → J2 C API DLL;研究依據 WS_R2 §10(行程開銷 6.7ms、~20k DOF 2.06s、100k+ 受 factor 支配→催 daemon)。依據 `WS_J_gh.md`/`WS_K_ecosystem.md`。
- **S7 BESO 拓撲優化 + N2 倒塌韌性**:hard-kill BESO(`active` flag 零侵入)+敏感度歷史平均(Huang&Xie)+compliance 跳變停機+機構守門用 N1 capacitance;N2 韌性約束版每 k 步 `runProgressiveCollapse` 抽查。`Research/WS_I_beso/exp_beso_truss.cpp` 已驗 29 迭代到 vol 30%(**尾段 compliance 暴增 52×→必加停機準則**);novelty 定位=fail-safe TO 親緣但「離散框架×序列線性倒塌×Stable/Collapsed 終點」方法整合,勿自稱 fail-safe(WS_N §N2)。依據 `WS_I_beso.md`/`WS_N_priorart.md`。
- **S8 殼 QM6 opt-in 膜 → DKQ 薄板快路**:**最大風險=OpenSees `ShellMITC4` 是原始雙線性膜,改進膜必須 opt-in、預設 Q4 才不破壞 OpenSees 閘門**。依據 `WS_E_shell.md` + 既有 P5 膜鎖定探索(膜鎖定是 MITC4 曲面慢收斂主因)。

## 一鍵指令
- **完整四腿 gate(commit 前必跑、必須全綠)**:`powershell -ExecutionPolicy Bypass -File <repo-root>\Scripts\run_gate.ps1 -RequireOpenSees`
- 快速 standalone(開發中迭代):`Plugins\FrameSolver\Standalone\build.bat`(期望 `ALL PASS (failures=0)`)
- UE 模組 build(改 FrameCore 後驗 UE 端):`%UE_ENGINE_ROOT%\Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development -project="<repo-root>\ArchSim.uproject" -waitmutex`(本機 `UE_ENGINE_ROOT=E:\project\UE_5.7`)
- 完整脈絡:`docs/KARAMBA3D_ROADMAP.md`、`docs/PROGRESS_S1.md`、各 `docs/specs/S*.md`、`docs/research/WS_*.md`、`<repo-parent>\CLAUDE.md`
