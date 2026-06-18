# 交接指南 — `v2.2+1` 後接手 owner (FrameCore v2.2 + LevelSim v1)

> `v2.2+1` 在 2026-06-18 發布,tag `v2.2+1` = commit (見 release notes)。
> v2.1 的 `docs/HANDOFF.md` 仍是 FrameCore 主交接文(原樣保留,作為史料);
> 本檔僅補上 v2.2+1 release 多出來的兩件事:**LevelSim 入門**與 **bundled release 的衛生規矩**。
> 真正接手前先讀 `docs/HANDOFF.md`,再讀本檔即可。

---

## 1. v2.2+1 = 什麼

- **FrameCore v2.2**:**引擎程式碼與 v2.1.0 (`4e660de`) 完全相同**;v2.2 是版本對齊升號,
  目的是把 FrameCore 與 LevelSim 包成同一個 release。FrameCore 細節仍以 `docs/HANDOFF.md`
  + `docs/PROGRESS_V21.md` + `docs/VERIFICATION.md` 為準。
- **LevelSim v1**:首次正式發布的「水準儀 / 高程站」測量教學模擬器,**與 FrameCore 完全零耦合**。
  - 主檔:[`Plugins/LevelSim/README.md`](../Plugins/LevelSim/README.md)
  - 純 C++17 測量核心:[`Plugins/LevelSim/Core/LevelCore.{h,cpp}`](../Plugins/LevelSim/Core/)
  - 獨立 oracle gate:[`Plugins/LevelSim/Standalone/level_gate.cpp`](../Plugins/LevelSim/Standalone/level_gate.cpp)
    (L1..L16,執行時 115 PASS asserts,跑 `Plugins\LevelSim\Standalone\build.bat`)
  - UE 可玩層:[`Plugins/LevelSim/Source/LevelSimPlay/`](../Plugins/LevelSim/Source/LevelSimPlay/)
  - 像素 oracle:[`Plugins/LevelSim/Tools/verify_smoke_shots.py`](../Plugins/LevelSim/Tools/verify_smoke_shots.py)
  - 設計文件:`E:\project\水準儀模擬器_設計文件.md`(本機路徑;不在 repo 內)

## 2. 兩引擎共生規則(踩過的雷)

- **零耦合是技術契約,不是口號**:LevelSim Core/cpp/cs 從不 `#include` FrameSolver/FrameCore;
  LevelSimPlay.Build.cs 不列 FrameSolver 依賴。任何改動意外破壞這條,就把 release 退化為單體。
  reviewer 可一行 grep 驗:
  ```bash
  grep -rn "FrameSolver\|FrameCore" Plugins/LevelSim/ --include="*.h" --include="*.cpp" --include="*.cs"
  ```
  預期零命中。
- **LevelSim 不需要 conda framecore-direct env**:它純 C++17 + MSVC,`Plugins\LevelSim\Standalone\build.bat`
  只要 `vswhere` 找得到 VS 就能跑。FrameCore 的 conda 依賴(OpenBLAS + METIS)只影響
  FrameCore 五腿 gate,**不影響 LevelSim gate**。新接手者若只想跑 LevelSim,跳過 conda 設定即可。
- **`.uproject` 不需要顯式列 LevelSim**:`LevelSim.uplugin` 已設 `"EnabledByDefault": true`,
  ArchSim host 自動載入。任何修改 `ArchSim.uproject` 把 LevelSim 加進 `Plugins[]`,反而會與
  upstream 的「零 .uproject 衝突」策略(commit `dd8548c`)抵觸。如有人本機加了又卡 merge,
  把那行 stash 掉即可。
- **`.gitignore` 不擋 `Plugins/FrameSolver/Standalone/frame_capi.{dll,exp,lib}`**(故意):
  CLAUDE.md 鐵則禁止改 `.gitignore`,但這些 build 產物 untracked 是定時炸彈。**用顯式
  `git add <file>` 而非 `git add -A` / `git add .`,任何時候都不要例外**。release commit
  的 `git status` 必須出現「Changes not staged」中含 frame_capi.* 是 untracked(非 ignored)
  也是正常的;確定它沒被 staged 即可。

## 3. v2.2+1 發布前掃過一輪 audit,主要 deferred 項

5 個對抗式 subagent(FrameCore numerics/oracle、LevelSim core/UE、docs/release)在 v2.2+1
發布前完整跑過。**無真硬 blocker**;以下兩項刻意 defer 到 v2.2+2(或下一輪 audit):

1. **deep audit "104 vs 109" 差異**(audit B-02):
   `linear_deep_audit.cpp` 內 `addRow(` 靜態計數為 109,但 README / VERIFICATION / PROGRESS_V21
   全寫 "104"。本機無 conda env,沒辦法執行 `linear_deep_audit.exe` 看執行時自報數字。
   - 動作:接手者在 conda env active 後跑
     ```bat
     Plugins\FrameSolver\Standalone\build_linear_audit.bat
     Plugins\FrameSolver\Standalone\linear_deep_audit.exe | findstr checks=
     ```
     確認真實 N,然後同步三份文件。
2. **LevelSim 多站「玩家算錯傳染下一站」**(audit D-08):
   多站路線中 `CurrentKnownElevM` 取上一腿的 `PlayerElev`(玩家自己輸入),若上腿
   `bElevOk=false`,錯誤會傳染到下一站的算術核對基準。**這是真實水準測量本來就會發生
   的事(後站靠玩家算的 elev 起算)**,且 `closeLoop()` 仍用 truth 算最終 misclosure,
   所以閉合分數不被汙染。決定:**保現行邏輯,作為「真實考試行為」記錄**;若未來想改成
   後站用 truth 高程重起,需同步更新 README "誠實邊界" 段並加 oracle。

其他 audit 已就地落地的小修見 release notes 與 git log。

## 4. 下一步候選(無排序)

- v2.2 → v2.3 FrameCore 線:**S11 MITC9i 高階殼**(9 處 seam,主線 last);**C6–C8 可視化資料線**
  (along-member BMD/SFD、利用率場、贅餘度,給 UI 用);**UE5 視覺層**(吃 CollapseStep replay
  + Chaos handoff)。詳見 `docs/HANDOFF.md` 與 `docs/AGENT_PROMPT_S5_S11.md`。
- LevelSim 線:升 PC MVP 為「多站考試流程」(M5/M6 核心 + UE FSM 已備,主玩流程需要 polish);
  HUD CJK 字型;圓窗 telescope;粗平(腳架腿);三絲評分;鏡頭 DOF 真實化;手機 / VR 切片。
- bundled release 線:`v2.2+2` 把 deferred 兩項做完(B-02 確認 + D-08 文件補強);
  考慮把 LevelSim release notes 拆獨立檔(目前合寫在 `docs/RELEASE_v2.2+1.md`)。

---

接手有問題,先讀 `docs/HANDOFF.md`(主線);LevelSim 相關問題讀 `Plugins/LevelSim/README.md`。
