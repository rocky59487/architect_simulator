# docs/ 導覽

評審 / 新讀者建議路徑:**先看 [`README.md`](../README.md)(這是什麼、能做什麼)→
[`VERIFICATION.md`](VERIFICATION.md)(為什麼可信:能力 → oracle → gate fixture → 實測精度)→
[`ARCHITECTURE.md`](ARCHITECTURE.md)(怎麼做到:資料模型、求解管線、慣例)**。
之後再依興趣深入各階段紀錄與規格。

## 現行權威文件(隨引擎同步維護)

| 文件 | 內容 |
|---|---|
| [`VERIFICATION.md`](VERIFICATION.md) | **證據鏈總表**:五腿 gate、oracle 分類、每個能力的實測精度 |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | 資料模型、求解管線、符號/單位/DOF 慣例、元素抽象、模組表 |
| [`CLI_PROTOCOL.md`](CLI_PROTOCOL.md) | `frame_cli` 線協議(文字橋 / daemon / C API 共用) |
| [`PERFORMANCE_BASELINE.md`](PERFORMANCE_BASELINE.md) | 效能基線與同機驗收錨點(`frame_perf.exe`) |

## 各階段工程紀錄(完成後凍結;含各自的 oracle 實測與誠實邊界)

| 階段 | 紀錄 | 規格 |
|---|---|---|
| S1 ReSolve 三層重解階梯 + 稀疏屈曲 | [`PROGRESS_S1.md`](PROGRESS_S1.md) | [`specs/S1_resolve_ladder.md`](specs/S1_resolve_ladder.md) |
| S2 連續動力倒塌(動量繼承) | [`PROGRESS_S2.md`](PROGRESS_S2.md) | [`specs/S2_dynamic_collapse.md`](specs/S2_dynamic_collapse.md) |
| S3 P-Delta 二階 | [`PROGRESS_S3.md`](PROGRESS_S3.md) | [`specs/S3_pdelta.md`](specs/S3_pdelta.md) |
| S4 Tension-only 桿 | [`PROGRESS_S4.md`](PROGRESS_S4.md) | [`specs/S4_tension_only.md`](specs/S4_tension_only.md) |
| S5 FSD 尺寸優化 | [`PROGRESS_S5.md`](PROGRESS_S5.md) | [`specs/S5_sizeopt.md`](specs/S5_sizeopt.md) |
| S6 Grasshopper / CLI 橋 | [`PROGRESS_S6.md`](PROGRESS_S6.md) | [`specs/S6_gh_bridge.md`](specs/S6_gh_bridge.md) |
| S7 BESO 拓撲優化 + N2 韌性約束 | [`PROGRESS_S7.md`](PROGRESS_S7.md) | [`specs/S7_beso.md`](specs/S7_beso.md) |
| S8 殼升級(QM6 / DKQ,opt-in) | [`PROGRESS_S8.md`](PROGRESS_S8.md) | [`specs/S8_shell.md`](specs/S8_shell.md) |
| S9 平面 co-rotational | [`PROGRESS_S9.md`](PROGRESS_S9.md) | [`specs/S9_corotational.md`](specs/S9_corotational.md) |
| S9b 3D 通用 co-rotational | [`PROGRESS_S9b.md`](PROGRESS_S9b.md) | [`specs/S9b_corotational3d.md`](specs/S9b_corotational3d.md) |
| S9c 弧長 snap-through 收尾 | [`PROGRESS_S9c.md`](PROGRESS_S9c.md) | [`specs/S9c_arclength.md`](specs/S9c_arclength.md) |
| S10 N–M 互動塑鉸 | [`PROGRESS_S10.md`](PROGRESS_S10.md) | [`specs/S10_nm_interaction.md`](specs/S10_nm_interaction.md) |
| S11 視覺化應力場後處理(v3.1.0;`computeStressField`) | (整入 `VERIFICATION.md` §1.6 + RELEASE notes) | [`specs/S11_stress_field.md`](specs/S11_stress_field.md) |
| FrameCoreUE UE 反射層(v3.2.0;USTRUCT mirror + BP library + Slate panel) | 入手 5-min 指南 [`FrameCoreUE_QuickStart.md`](FrameCoreUE_QuickStart.md) | [`RELEASE_v3.2.0.md`](RELEASE_v3.2.0.md) |
| R 線 自建 supernodal direct lane(opt-in;非 S 主線) | [`PROGRESS_R_supernodal.md`](PROGRESS_R_supernodal.md) | `VERIFICATION.md` §3.8 |

S 系列之前的線性套件(8 段)與崩塌 C 線(6 階段)沒有獨立 PROGRESS 檔——其能力、oracle 與
邊界已整併入根 README 與 `VERIFICATION.md` §3.1–§3.3,演進史在 git log。

## 史料(point-in-time 定稿,僅供考據,不反映現況)

| 文件 | 性質 |
|---|---|
| [`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md) | 2026-06-10 研究輪收斂出的開發計畫(S1–S11 的原始目標/驗收定義);主線 S1–S10 已全部完成 |
| [`KARAMBA3D_ROADMAP.md`](KARAMBA3D_ROADMAP.md) | 對標研究主報告(對標事實、novelty 先行技術定位、宣稱紀律) |
| [`research/WS_*.md`](research/) | 研究輪文獻查證與可重跑實驗(`WS_R2_experiments.md`) |
| [`specs/S5_S11_skeletons.md`](specs/S5_S11_skeletons.md) | S5–S11 規格骨架(S5–S10 + S11 stress field 都已被正式 spec 取代:S11 走 `specs/S11_stress_field.md`,v3.1.0 落地) |
| `AGENT_PROMPT_S2_S4.md` / `AGENT_PROMPT_S9.md` / `AGENT_PROMPT_S5_S11.md` | 各開發輪的 agent 工作提示詞(檔首已標歷史狀態) |

> 慣例:史料**不回頭改寫內文**(保留當時的狀態數字與判斷),只在檔首加狀態 banner。
> 宣稱分級 `[VERIFIED]` / `[NEW CODE]` / `[THEORY]` / `[UNKNOWN]` 的定義見
> `KARAMBA3D_ROADMAP.md` §0,沿用於全部 specs 與 PROGRESS。

## 其他引擎(同 repo,獨立子系統)

| 子系統 | 入口 | 說明 |
|--------|------|------|
| LevelSim — 水準儀模擬器 | [`Plugins/LevelSim/README.md`](../Plugins/LevelSim/README.md) | 純 C++17 測量核心 + UE5 可玩 MVP;**與 FrameCore 完全零耦合**(無共享 header / 無共享 Build.cs 相依),可獨立 build/test/release。本 docs/ 目錄維持 FrameCore 專屬;LevelSim 自有 README、gate、smoke pipeline 與 release notes 段。bundled release `v2.4` = FrameCore v2.4 + LevelSim v1.0.0(v2.4 內容見 [`RELEASE_v2.4.md`](RELEASE_v2.4.md))。 |

## 外部橋接 / Rhino bridge v2(B 線,v2.4 加入,v2.5→v2.8.1 已 fully wired)

> v2.4 引入第二條 client-side bridge,並行於既有的 v1 `frame_cli` / `frame_capi.dll`(永久保留)。
> v2 = **framed JSON 線協議 + opt-in C# SDK**。階段史:B1 (規格,v2.4) → B2 (DLL + dispatcher 骨架,v2.4)
> → **B3 (engine wire,v2.5,12 method handler 接 FrameCore)** → **B4 (async dispatcher,v2.6)** →
> **B5 / B5.2 (factor reuse + reanalysis_solve,v2.5/v2.6 shape;session-cache routing 仍 deferred 至 v2.9)**
> → **B6 placeholder (model.patch schema 待定)** → **B7 GHA dotnet build (環境依賴,deferred)**。
> 當前 v3.2.0 audit 確認:**23 advertised capabilities**(`hello.capabilities` 廣告含 `transport.async`、
> `dyn_collapse.live`、`inspect.stress_field` (v3.1.0 加)、所有 analysis.* / solve.* / inspect.* /
> session / cancel / model.set / profile.*)+ C-09/C-10 supernodal guard 仍生效;
> `Tools/v2_roundtrip.py` 為 6th 個 CI gate leg。

| 文件 | 性質 |
|---|---|
| [`specs/S6b_rhino_bridge_v2.md`](specs/S6b_rhino_bridge_v2.md) | 權威協議規格:framed JSON、雙 profile(simple/advanced)、19 method 目錄、forward-compat 規則 |
| [`specs/S6c_rhino_ux_commercial.md`](specs/S6c_rhino_ux_commercial.md) | 商業級 UX 規格:80 GH 元件目錄、Display 範式、預設庫、Bake |
| [`HANDOFF_rhino_bridge_v2.md`](HANDOFF_rhino_bridge_v2.md) | B1 階段交接(設計 + 骨架完成,被下一輪 superseded — 史料) |
| [`HANDOFF_rhino_bridge_v2_final.md`](HANDOFF_rhino_bridge_v2_final.md) | B2 階段最終交接(B2 是 stub dispatcher;B3/B4 將其 wire 起來) |
| [`PROGRESS_B2.md`](PROGRESS_B2.md) | B2 階段進度紀錄(dispatcher 骨架交付明細) |
| [`HANDOFF_v2.4.md`](HANDOFF_v2.4.md) | v2.4 cycle 交接概要 + B3 第一行動指引(已落實於 v2.5) |
| [`RELEASE_v2.4.md`](RELEASE_v2.4.md) | v2.4 release notes — Rhino bridge v2 + B2 dispatcher 骨架 |
| [`RELEASE_v2.5.md`](RELEASE_v2.5.md) | v2.5 release notes — B3 dispatcher engine wire + 7-agent audit |
| [`HANDOFF_v2.5.md`](HANDOFF_v2.5.md) | v2.5 後接手 owner 交接(B4/B5.2/C-09/C-10 第一行動指引) |
| [`RELEASE_v2.6.md`](RELEASE_v2.6.md) | v2.6 release notes — B4 async dispatcher + C# bridge 7 fix + C-09/C-10 guard |
| [`RELEASE_v2.7.md`](RELEASE_v2.7.md) | v2.7 release notes — P1-3 `dyn_collapse.live` mid-run streaming + cancel |
| [`RELEASE_v2.8.1.md`](RELEASE_v2.8.1.md) | **v2.8.1 release notes** — audit-hardening pass (7-agent finding,版本字串 / engine NaN / queue cap / dead field / DisposeAsync UAF / dead-link / handoff debt) |
| [`HANDOFF_v2.8.1.md`](HANDOFF_v2.8.1.md) | **v2.8.1 後接手 owner 交接** — 補齊 v2.6/v2.7 漏的 handoff,含每項 deferred 的「第一個動作」 |
| [`RELEASE_v3.0.0.md`](RELEASE_v3.0.0.md) | v3.0.0 STABLE — 9/9 gate legs green anchor + r2_bench 90k 60-fps margin |
| [`RELEASE_v3.0.1.md`](RELEASE_v3.0.1.md) | v3.0.1 — post-3.0.0 hardening (version sync + strict fingerprint + regression gate + CI workflow) |
| [`RELEASE_v3.1.0.md`](RELEASE_v3.1.0.md) | v3.1.0 — S11 stress-field post-process + 7-agent audit closeouts (F68/F69/F70) |
| [`RELEASE_v3.2.0.md`](RELEASE_v3.2.0.md) | v3.2.0 — FrameCoreUE thin slice (USTRUCT marshal + BP node + Slate panel) |
| [`HANDOFF_v3.1.0.md`](HANDOFF_v3.1.0.md) / [`HANDOFF_v3.2.0.md`](HANDOFF_v3.2.0.md) | 對應 v3.1.0 / v3.2.0 接手 owner 交接(每項 deferred 含 first action) |

## 課程素材 / Learning(v2.4 加入)

> 26 課白板教材 + 39 k 行深入課程,涵蓋從 FEM 基礎到 FrameCore S1-S10 內部機制。
> 這是隨 v2.4 一起 bundled 的教學素材,**非引擎本身的一部分**,可不影響 build/gate 獨立讀。

| 文件 | 性質 |
|---|---|
| [`learning/framecore_v2_course_lesson1.md`](learning/framecore_v2_course_lesson1.md) | 白板課第 1 課獨立檔(完整 998 行) |
| [`learning/deep_course/`](learning/deep_course/) | 25 課白板課(`lesson_02..lesson_26.md`)+ deep_course combined 39645 行 + 生成腳本(`generate_deep_course.py`, `generate_freeform_pdf.py`)+ `FREEFORM_IMPORT_GUIDE.md`、`README.md` |
| [`scripts/generate_framecore_whiteboard_course.py`](scripts/generate_framecore_whiteboard_course.py) | 白板課 PDF 生成器(reportlab,本機字型;非 CI 必跑) |
| [`AGENT_PROMPT_OPENSEES_MEGA_BENCHMARK.md`](AGENT_PROMPT_OPENSEES_MEGA_BENCHMARK.md) | v2.3 cycle 寫的 mega benchmark 任務提示詞(史料,結果見 [`benchmarks/opensees_mega/`](../benchmarks/opensees_mega/)) |
