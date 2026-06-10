# S1 進度日誌(無人監督夜間執行 — 2026-06-10 23:35 起)

> 睡前授權:照 `docs/IMPLEMENTATION_PLAN.md` 從 S1 起盡量往後推、不半成品、務實分層 gate、持續 commit/push。
> 本檔記錄每個交付點的決策與假設,供使用者醒來檢視。gate 政策:**standalone(build.bat + build_linear_audit.bat)綠就 commit**;UE automation + OpenSees 重 gate 留到 **S1 整體完成的里程碑** 跑一次。

## 基準
- 起點 commit `81639c1`(研究輪)。working tree 既有雜項(`.gitignore`/`ArchSim.uproject` 改動、`Plugins/LevelSim/`)**非本人改動,全程不碰**。
- baseline standalone gate 確認綠:`ALL PASS (failures=0)`,F1..F34(原 F1..F33;F33=event-to-event 塑鉸驅動,與 F32 共用 `[F32]` printf 表頭)。

## 已完成交付點

### 1. `docs/IMPLEMENTATION_PLAN.md` 入庫 — commit `63b80bc` ✅
單獨 commit,嚴守 commit 衛生(未掃入雜項)。

### 2. R8:修 `build_perf.bat` 連結失敗 — commit `0e2e500` ✅
- 源檔清單補 `MITC4ShellElement.cpp`(FrameSolver 的 assembleAndFactor 即使純梁柱模型也引用殼 dispatch 符號 → LNK2001)。
- 順手補齊 `vswhere` 目錄上 PATH 的一致性修復(build.bat/build_cli/build_linear_audit 都有,build_perf 漏了 → vcvars 裸 vswhere 警告)。
- 驗證:`frame_perf.exe` 連結乾淨無警告。獨立 bugfix,與 S1 新增無耦合。

### 3. 稀疏屈曲 overload + F34 oracle — commit `(本次)` ✅
- **`BucklingOptions`**(POD:`denseThreshold=500`/`nev=1`/`maxIter=300`/`tol=1e-11`)+ `solveBuckling` 三參 overload;舊兩參版委派 `BucklingOptions{}`。
- `BucklingAnalysis.cpp` 重構:共用步驟(參考解→軸力→Kg)→ 依 `nf` vs `denseThreshold` 分派 **稀疏**(`subspaceSmallest(Kff, S.ldlt, negKgff, nev)`,復用既有 LDLT,`lambda(0)`=criticalFactor)或 **稠密**(GES,**bit-identical** 保 F23 不變);稀疏失敗→稠密 fallback(永遠正確)。
- **決策/假設(誠實標)**:
  - **F 編號**:稀疏屈曲=**F34**(實作順序;spec ⑥ 暫定 F36,但 baseline 最高 logical fixture=F33,故依 PLAN §7「以實際新增為準」順序編號;ReSolve tier1/tier2 接 F35/F36)。
  - **殘差閘**:在 spec ④ 之外加一道寬鬆 pencil 殘差閘(`‖(−Kg)φ−γKφ‖/(γ‖Kφ‖) < 1e-3` 才採信;研究輪實測 1e-7~1e-10,故對良態模型永不誤觸,只擋「收斂但不準」→ 退稠密)。屬保守安全網,文檔已標。
  - **退化護衛**:全拉(`gtrips.empty()`)→ 稠密路徑,沿用既有「no compression」診斷;強制稀疏在全拉時亦退稠密(`lambda≤0` 或 Kg 空),不偽造正屈曲因子。
  - **F34 用矩形斷面**(`Rectangular(60,100)`):避免方斷面 y/z 簡併模態,讓最低屈曲值單一、稀疏迭代乾淨收斂。
- **驗證**:
  - standalone F34:pinned-pinned n=10/fixed-free n=10/pinned-pinned n=24,**sparse==dense rel = 2e-14 / 2e-13 / 1e-12**(非零→確認走稀疏非 fallback)、sparse==Euler rel = 1.35e-5 / 8.4e-7 / 4.1e-7、全拉護衛三檢全過。`ALL PASS (failures=0)`。
  - linear_deep_audit +1 check「sparse path agrees with dense」rel=5.48e-13 → **checks 62→63 PASS**。
  - 原型參照:`Research/WS_B_solver/exp_sparse_buckling.cpp`(同款 reduceFF + subspaceSmallest 映射)。
- **無新 `.cpp`** → build 腳本源檔清單不需改。

## 待補(S1 里程碑重 gate 一次處理)
- **UE automation**:加 `FrameCore.Buckling.SparseAgreesDense`(+ 之後 ReSolve 的 `FrameCore.Reanalysis.LadderAgreesFresh`/`MechanismDetection`)→ bump `run_gate.ps1` `$ExpectedUeTests`(34→實際數)→ 跑 headless UE 測試。
- **OpenSees strict**:既有「移除態」逐位移場景改走 ReSolveSession 重跑一次(同容差)。
- 理由:務實分層 gate 政策(額度受限),重 gate 集中里程碑跑。

## 下一個交付點
- **ReSolveSession 三層重分析階梯(F35/F36)** — S1 最硬核:Tier-1 Woodbury 低秩 + Tier-2 stale-LDLT PCG + Tier-3 全重分解;F 增量記帳 `[NEW CODE]`(F35 須含 UDL/殼子案例安全網);新 `Reanalysis.{h,cpp}` → build 腳本補源檔。原型 `Research/WS_N_incremental/exp_incremental_refactor.cpp`。
