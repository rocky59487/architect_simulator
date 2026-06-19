# 交接指南 — `v2.3` 後接手 owner

> `v2.3` 在 2026-06-19 發布,tag `v2.3` = (commit 見 release notes)。
> 主交接文仍是 `docs/HANDOFF.md`(v2.1 寫的、原樣保留作為史料);
> `docs/HANDOFF_v22p1.md` 紀錄 v2.2+1 bundled release 多出的兩件事;
> 本檔只補上 v2.3 多出的兩個跨 cycle 主題:**OpenSees mega benchmark** 與 **CLI `WARP` token**。
> 接手前先讀 `docs/HANDOFF.md` → `docs/HANDOFF_v22p1.md` → 本檔。
>
> **下一輪 v2.4(同 2026-06-19 發布)新增**:Rhino bridge v2 + B2 dispatcher + 第 6 gate leg
> + 26 課白板教材。v2.4 細節見 [`HANDOFF_v2.4.md`](HANDOFF_v2.4.md) + [`RELEASE_v2.4.md`](RELEASE_v2.4.md);
> 引擎程式碼未動,本檔的所有資訊在 v2.4 仍然有效。

---

## 1. v2.3 = 什麼

- **FrameCore 引擎程式碼:vs v2.2+1 = 微改 7 行**
  - `FrameModel::validate()`:reject `mat.rho < 0`(v2.2+1 deferred A-06,本輪驗證落地;對 beam + shell 兩條都加)。
  - `frame_cli_core.cpp`:新 `WARP <warpTolerance> [<useWarpingCorrection:0|1>]` token,把 v3 warped-quad opt-in 暴露給 CLI(這是 24 CRITICAL 的根因 fix:CLI 無 token → `Block::opt` 永 default → `warpTolerance=1e-6` → C2/C5 warped quad 全 reject)。
- **OpenSees mega benchmark 整入**:`benchmarks/opensees_mega/`(c212aec 進 main 後加 v2.3 的 harness 補強)。**128 case × load × mesh** 對標 `ShellMITC4`/`elasticBeamColumn`,結果在 `results/<run-id>/{matrix.csv,findings.json,report.md}`。
- **harness 自動 emit `WARP 0.02 1`**:`benchmarks/opensees_mega/harness/{model.py,io.py,builders/shells.py}` — 為 `_c2_hypar` / `_c5_freeform_surrogate` 設 `warp_tol=0.02`,其他 case 仍 None → 不 emit WARP,完全 v2.2+1 backward-compat。
- LevelSim 無改動。

## 2. 怎麼跑 mega benchmark

```powershell
# 需要 conda env framecore-direct(供 supernodal lane build 用,雖然 mega 用 CLI 不直接需要)
# 與 openseespy(pip install openseespy)
powershell -ExecutionPolicy Bypass -File benchmarks\opensees_mega\rerun.ps1
```

`--run-id <stem>` 可指定 results 目錄名。預設下次序號自動產生。

新 run 寫入 `benchmarks/opensees_mega/results/<run-id>/`:
- `matrix.csv` 128 列 × per-node 比對
- `findings.json` 每個 case 的 severity / oracle / reason
- `report.md` 執行摘要 + 每結構類型小結 + CRITICAL 清單
- `plots/C*_convergence.{csv,svg}` 殼網格收斂曲線

## 3. WARP token 用法(給 CLI 客戶端)

```
WARP <warpTolerance> [<useWarpingCorrection:0|1>]
```

- `warpTolerance`:relative,典型 0.02 ~ 0.1。0 = 嚴格(等於 default 1e-6)。
- `useWarpingCorrection`:1 = 啟用 best-fit 平面投影(預設);0 = 不投影,只放寬 validate。

**不發此 token = v2.2+1 位元同**(`SolveOptions::warpTolerance=1e-6` 嚴格)。

OpenSees `ShellMITC4` 不檢查 warp,直接接受任意 quad。FrameCore 預設嚴格 = 給 user 一個告警提醒(curvature shell 需要 mesh refinement、warp correction 等)。要對標 OpenSees 必須顯式給 token。

## 4. 仍 deferred 的 items

1. **F65 / F66 standalone fixtures** — Phase A subagent 提了兩個 4-element warped fixture(mini-C2 + mini-C5)的草案。mega benchmark 已 act 作為 regression test(24→0),但 standalone fast-feedback 還沒收割。v2.3.1 可補。
2. **standalone `build.bat` 加 `FC_NO_SUPERNODAL=1` skip 路徑** — 給 vanilla MSVC 機(無 conda)用。本機這次的 audit 證實 conda env 已存在,所以沒打到痛點;但 contributor 仍需(目前 `build.bat` 硬綁 conda)。
3. **LevelSim D-08 (multi-station player elev propagation)** — 真實測量行為,沒 code 改;但 `Plugins/LevelSim/README.md` "誠實邊界" 應加一段明文敘述。

## 5. v2.3 過程留下的教訓 (durable)

- **不要假設環境跑不了**:v2.2+1 把 FrameCore standalone gate 標 NOT RUN,實際 conda env 早已在 `~/anaconda3/envs/framecore-direct/`,只是 `where conda` 不見。本輪先確認 env 才省一輪 deferred → 這個 audit-step「環境探查」要列為 release-hardening workflow 的必跑 phase 0。
- **CLI bridge 不暴露 opt-in flag = 引擎能力等於零**:`warpTolerance`、`useWarpingCorrection`、`useIncompatibleMembrane`、`useDKQPlate` 等 SolveOptions 旗標,如果 CLI 沒有 token,對所有 CLI 客戶端(含 mega benchmark)就等同不存在。新增 SolveOptions flag 時,**同步檢查 CLI/UE/gh-bridge 三個入口**。
- **mega benchmark 是強力 oracle 也是強力陷阱**:128 case 一次 surface 出單一 root cause(24 CRITICAL 全是 warpTolerance);但若 root cause 沒收斂,容易誤把 1 個問題報成 24 個改動需求。先做根因分析、再決定 fix scope。

## 6. 後續方向(無排序)

- v2.3 → v2.4:S11 MITC9i 高階殼(主線 last,9 處 seam)、C6–C8 視覺化資料線(BMD/SFD、utilization、redundancy)、UE5 視覺層(CollapseStep replay + Chaos handoff + D/C heatmap)。
- v2.3.1 / v2.4:LevelSim "多站考試流程" UI polish(M5/M6 核心已備、只欠 UX)、HUD CJK 字型、圓窗 telescope、粗平(腳架腿)、三絲評分。
- mega benchmark 擴充:NURBS 原生 primitive(取代 faceted surrogate)、release token 給 D2、SF 比對校準。

---

接手有問題:`docs/HANDOFF.md` → `docs/HANDOFF_v22p1.md` → 本檔。LevelSim 相關問題讀 `Plugins/LevelSim/README.md`。Mega benchmark 細節讀 `benchmarks/opensees_mega/README.md`。
