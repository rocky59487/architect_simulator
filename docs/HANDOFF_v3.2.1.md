# 交接指南 — `v3.2.1` 後接手 owner

> `v3.2.1` 在 2026-06-22 發布(release-hardening 整合 v3.2.0..HEAD 10 commits +
> 7-agent 對抗審核 + 4 個 in-place fix),tag `v3.2.1` = commit `<see git log after
> Phase 5>`。主交接文是 [`docs/HANDOFF.md`](HANDOFF.md)(原始 v2.x history,不動);
> 前一輪在 [`docs/HANDOFF_v3.2.0.md`](HANDOFF_v3.2.0.md);本檔補上 v3.2.1 多出來的
> 內容(Phase 6 a-h UE test +8 / clean-build safety / doc grooming / 6 newly-deferred
> V321-01..06)。

## 1. `v3.2.1` = 什麼

**一句話:** 把 v3.2.0 tag 之後的 10 commits(8 個新 UE test 從 Phase 6 a-h + 3 個 doc
+ CI / build_capi_v2.bat hygiene)包成 patch release,加 release-hardening
7-agent audit 抓到的高信心 fix(`bUseUnity = false` clean-build safety + 4 處
stale UE count + 3 個 CI / run_gate 字串)。**FrameCore native module
(`Plugins/FrameSolver/Source/FrameCore/`)零行改動 vs v3.2.0** —
`git diff v3.2.0..v3.2.1 -- Plugins/FrameSolver/Source/FrameCore/` 是空的。

**Engine source delta vs v3.2.0:** 0 lines under FrameCore native module。v3.2.1
整體 delta = 8 新 UE test 檔 + 12 處 doc/CI/build hygiene 字串 + 1 行
`bUseUnity = true → false` + 2 行 `.gitignore` patterns + 1 個新 RELEASE notes +
1 個新 HANDOFF。

**新檔:**

- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEMarshalSSBeamTest.cpp`
  (UE 63rd test;`FrameCore.UE.MarshalSSBeamTest`)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEMarshalShellPlateTest.cpp`
  (UE 64th;`FrameCore.UE.MarshalShellPlateTest`)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEMarshalMultiMemberTest.cpp`
  (UE 65th;`FrameCore.UE.MarshalMultiMemberTest`)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEEditorTabSpawnerTest.cpp`
  (UE 66th;`FrameCore.UE.EditorTabSpawnerTest`,**closes v3.2.0 deferred U-04**)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUERobustnessTest.cpp`
  (UE 67th;`FrameCore.UE.RobustnessTest`)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEThetaRangeTest.cpp`
  (UE 68th;`FrameCore.UE.ThetaRangeTest`;v3.2.1 audit A-3 HIGH closeout: lower-bound
  tolerance corrected from `<= -halfPi - epsTol` → `<= -halfPi + epsTol`)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEZeroLoadTest.cpp`
  (UE 69th;`FrameCore.UE.ZeroLoadTest`)
- `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEAxialColumnTest.cpp`
  (UE 70th;`FrameCore.UE.AxialColumnTest`)
- `docs/FrameCoreUE_QuickStart.md`(v3.2.0 post-tag Phase 6h 寫的 5-min 入手指南;
  v3.2.1 audit E-6 + G-R3 加 v3.2.x anchor + **Prerequisites** 段落)
- `docs/RELEASE_v3.2.1.md`(本 release notes)
- `docs/HANDOFF_v3.2.1.md`(本檔)

**修改檔:**

- `Plugins/FrameSolver/Source/FrameCoreUE/FrameCoreUE.Build.cs` —
  `bUseUnity = true → false`(v3.2.1 audit D-1 HIGH:7/8 新 test 含 anonymous namespace,
  clean build 時 unity TU merge 會 shadow → CLAUDE.md 踩雷 #4)
- `Scripts/run_gate.ps1` — `$ExpectedUeTests 60 → 70`(增量 62→65→67→69→70 per Phase
  6 a/e/f/closeout)+ 註解 non-cuDSS 從 60 → 68 + 4 處 `[1/3]/[2/3]/[3/3]/[4/4]` 改
  `[1/5]/[2/5]/[3/5]/[4/5]`(F-1)
- `Plugins/FrameSolver/Standalone/build_capi_v2.bat` — 註解現代化(B2 stub → B3 wire
  歷史已穩定;加 v3.2 dispatcher state)
- `.github/workflows/release-gate.yml` — header `(F1..F66 default)` → `(F1..F70 default)`
  + footer `HANDOFF_v2.11.1.md` → `HANDOFF_v3.2.0.md(and any newer)`
- `.gitignore` — `dist/` 之外加 `out/` + `framecore-v*.zip`
- `docs/VERIFICATION.md` — 4 處 `62 / 60` → `70 / 68`
- `docs/ARCHITECTURE.md` — 2 處 `62 / 60` → `70 / 68` + v3.2.1 Phase 6 test 名加入
- `docs/HANDOFF_v3.2.0.md:62` — `UE 62/62` 加註「v3.2.0 tag 時;HEAD 含 v3.2.1
  Phase 6 = 70/70」
- `Plugins/FrameSolver/README.md:47` — `57 tests`(stale 從 v2.10 之前!)
  → `60 tests + FrameCoreUE 10 = 70 total / 68 without cuDSS`
- `docs/README.md` — release-notes 歷史表加 v3.0.0 / v3.0.1 / v3.1.0 / v3.2.0 entries
  + v3.1.0 / v3.2.0 HANDOFF cross-links(原本 orphan from README navigation)
- `docs/FrameCoreUE_QuickStart.md` — 標題 `v3.2.0 → v3.2.x` + 新 **Prerequisites**
  section(UE 5.7 install + Build.bat ArchSimEditor + conda env framecore-direct)
- `docs/NIGHT_SHIFT_2026-06-22.md` — Phase 6 row 完整紀錄(已存在,v3.2.1 不再 append)
- `docs/specs/S5_S11_skeletons.md` — disambiguation banner(已存在,v3.2.1 不動)

**沒動的:** `Plugins/FrameSolver/Source/FrameCore/`(全引擎源)/ `Dispatcher.h`
`kEngineVer` 故意停留 `"3.2.0"`(見 RELEASE notes §1.7)/ `.uplugin VersionName`
也停留 `"3.2.0"` / `Scripts/run_gpu_gate.ps1` `FRAMECORE_EXPECTED_ENGINE_VER`
也停留 `'3.2.0'` / `ArchSim.uproject` / CUDA lane / LevelSim / 所有 v3.2.0 / v3.1.0
deferred 項(除 U-04 在 v3.2.1 Phase 6e closed)。

## 2. 怎麼跑(主要 reproduce paths)

### 五腿基本 gate(秒~分鐘級)

```powershell
# 一鍵全綠驗證(standalone F1..F70 + UE 70/70 + OpenSees + audit 104 + CLI):
$env:PATH = "$env:USERPROFILE\anaconda3\envs\framecore-direct\Library\bin;$env:PATH"
$env:UE_ENGINE_ROOT = "E:\project\UE_5.7"   # 替換成你的 UE 5.7 install 路徑
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees

# 非 cuDSS box:
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees -ExpectedUeTests 68

# 最快單腿(秒級):F1..F70 standalone analytic / benchmark
Plugins\FrameSolver\Standalone\build.bat
Plugins\FrameSolver\Standalone\frametest.exe
```

**前置條件**(同 v3.2.0,carry-forward):
- conda env `framecore-direct` 有 `openblas.dll` + `metis.dll`(必),`cudss64_0.dll`(opt)
- `$env:USERPROFILE\anaconda3\envs\framecore-direct` 是 v3.2.x 預期路徑;如果你的 conda
  env 在別處,set `$env:SUPERNODAL_CONDA` 覆寫(v2.11.1 audit fix)
- system Python(Windows Store 3.12)有 `openseespy`(`pip install openseespy`)
- UE 5.7 install at `$env:UE_ENGINE_ROOT`

**踩雷:** `framecore-direct` conda env **不含 python.exe**(libs-only env)。OpenSees
leg 用 system Python。不要 `conda activate framecore-direct`。

### UE module incremental rebuild(改 UE source 必跑)

```bat
%UE_ENGINE_ROOT%\Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development ^
    -project="%cd%\ArchSim.uproject" -waitmutex
```

**v3.2.1 注意**:`FrameCoreUE.Build.cs` 改 `bUseUnity = false`。第一次 pull v3.2.1
之後第一個 `Build.bat ArchSimEditor` 會觸發 FrameCoreUE module 全重編(unity 設定改);
後續 incremental 正常(只重編 changed .cpp)。

**踩雷:** `run_gate.ps1` 不會 build UE。改 UE source 後必須先跑上面 Build.bat,否則
`$ExpectedUeTests` count guard 會 short-fall。v3.2.1 加了 8 個新 UE test → 從 v3.2.0
checkout pull v3.2.1 後若沒 rebuild,UE leg 會 fail(只跑舊 62 tests)。

### v2 dispatcher round-trip(CPU)

```bat
:: 建 v2 DLL + 跑 round-trip
Plugins\FrameSolver\Standalone\build_capi_v2.bat
set FRAMECORE_EXPECTED_ENGINE_VER=3.2.0
python Tools\v2_roundtrip.py
:: 期望: "=== summary: ALL PASS ==="
```

**v3.2.1 注意**:`FRAMECORE_EXPECTED_ENGINE_VER=3.2.0` 不變(見 RELEASE notes §1.7
為什麼 v3.2.1 patch 不 bump kEngineVer)。Capabilities list 23 條不變。

### GPU + r2_bench 90k 互動 perf(opt-in)

```powershell
Scripts\run_gpu_gate.ps1 -Strict
```

`FRAMECORE_EXPECTED_ENGINE_VER` 仍 `3.2.0`。v3.2.1 source delta 在 CUDA path = 0 lines,
數字 vs v3.0.0/v3.1.0/v3.2.0 不變。

### 環境前置(fresh clone)

跟 v3.2.0 一樣:MSVC 18 (Community/Preview) / Eigen 3.4.0 / UE 5.7 / conda env
`framecore-direct`(`environment.yml`)/ `SUPERNODAL_CONDA` env-var override /
`FRAMECORE_EXPECTED_ENGINE_VER='3.2.0'` v2_roundtrip pin。

## 3. Deferred items + first-action-on-day-1

### 3.1 v3.2.0 carry-forward(原 U-01..U-07 + 所有 v3.1.0 / v3.0.x carryforward)

所有 v3.2.0 carry-forwards 持續到 v3.2.x / v3.3,除 **U-04 在 v3.2.1 Phase 6e
CLOSED**:
- U-01 BP load JSON model entrypoint(v3.3 主軸)
- U-02 Slate fixture dropdown(trivial follow-up)
- U-03 真實 renderer(spline mesh / Niagara / colour-band shader)
- **U-04 CLOSED** in v3.2.1(`FFrameCoreUEEditorTabSpawnerTest`)
- U-05 float-only USTRUCT 精度
- U-06 VS2026 not preferred version warning
- U-07 sentinel mismatch(engine 0 vs USTRUCT -1)— 鐵則 #1 blocked

詳見 [`docs/HANDOFF_v3.2.0.md` §3.2](HANDOFF_v3.2.0.md#32-v320-newly-deferred) 每項
first action。v3.1.0 carry: A-13 F71, D-05 CLI STRESS, E-07 v2 inspect spec, E-13 S11
naming, C-12 cancel poll, F-02 findUdl hashmap, F-03 clamps invariant doc → 詳見
[`docs/HANDOFF_v3.1.0.md` §3](HANDOFF_v3.1.0.md)。

### 3.2 v3.2.1 newly deferred(從本 release-hardening 7-agent audit)

6 個 item Phase 1 audit 抓到但超出 v3.2.1 patch 範圍(< 30 LOC + oracle-backed + no
public-API change)或需要 UE rebuild 矩陣再驗證:

1. **V321-01 (A-1/A-2) — `MarshalSSBeamTest` 加 analytic Vy oracle + tighten gov id 範圍**
   currently asserts `bpVy ≈ podVy`(self-referential float-cast)+ `GoverningMemberId >= 0`
   (vacuous)。
   **first action:** 在 `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUEMarshalSSBeamTest.cpp`
   line 113 之前加 `const double VyAnalytic = w * (L/2 - x_mid) / 2.0;` 配 midspan
   sample;line 124 之後 assert `TestTrue("SSBeam Vy analytic", FMath::Abs(podVy0 - VyAnalytic) < 1e-3 * FMath::Abs(VyAnalytic))`;再把 `>= 0` 改 `== 0 || == 1`。

2. **V321-02 (A-4) — `RobustnessTest` per-sample bit-exact compare**
   100 repeat loop 只比 `GlobalMaxFiberSigma` + 計數;state leak 改 sample 內值不會被抓。
   **first action:** 在 `FrameCoreUERobustnessTest.cpp` line 139-150 改 loop 抓 first
   iteration 的 `r0.Members[0].Samples[5]` snapshot,後續 99 次 iteration 對 N/Vy/Mz
   做 bit-exact compare(double::operator==)。

3. **V321-03 (A-5) — `AxialColumnTest` 加 N > 0 sign assertion**
   現在用 `FMath::Abs(absN0 - P)`,看不出 N 的正負(F4 standalone 是 compression-positive
   contract `N > 0`)。
   **first action:** `FrameCoreUEAxialColumnTest.cpp` 在 `|N| ≈ P` 後面加
   `TestTrue("Axial column N sign (compression-positive)", s0.N > 0.f);`。

4. **V321-04 (A-8) — `EditorTabSpawnerTest` 加 module load guard**
   `HasTabSpawner` 在 `StartupModule` 還沒跑時可能 false-negative。
   **first action:** `FrameCoreUEEditorTabSpawnerTest.cpp` line 22 之前加
   `FModuleManager::Get().LoadModuleChecked<IModuleInterface>(TEXT("FrameCoreUE"));`。

5. **V321-05 (F-4) — extract `Tests/FrameCoreUETestHelpers.h` 共用 forward decl**
   6 test files 都 copy-paste `namespace FrameCoreUE { FFrameStressField ToBlueprint(...); }`。
   **first action:** 新建 `Plugins/FrameSolver/Source/FrameCoreUE/Private/Tests/FrameCoreUETestHelpers.h`
   含 1 個 forward decl;6 個 test files swap `namespace { FFrameStressField ToBlueprint(...); }`
   改 `#include "FrameCoreUETestHelpers.h"`;UE incremental rebuild + UE leg green 驗證。

6. **V321-06 (E-9) — `Dispatcher.h:44,145` `model.patch — schema TBD` carryforward**
   open 從 v2.4。
   **first action:** 給 audit ID `B-06`;若 v3.3 / v3.4 沒 plan 就移除 capability
   announce 並更新 `docs/specs/S6b_rhino_bridge_v2.md` 列為 "out of v3.x scope"。

## 3.3 v3.x out-of-tree maintenance(本 release 之外但接手 owner 應知)

- **`E:/project/CLAUDE.md` 第 13 行 HEAD anchor 還停在 `v2.11.1`** — 這份 project-level
  CLAUDE.md(`ArchSim/` 的 parent 目錄)是接手 owner 開新 Claude session 時自動載入的
  context,但**不在 git tracking 範圍內**(repo root = `E:/project/ArchSim/`)。
  v3.0.0 / v3.0.1 / v3.1.0 / v3.2.0 / v3.2.1 五個 release 都沒有同步它。
  **下次接手 owner 應該手動編輯**:line 13 改成 `## 現況(YYYY-MM-DD,HEAD `v3.2.1`,
  ...)`,line 14 之後加 v3.0.x / v3.1.0 / v3.2.0 / v3.2.1 narrative。
  release-hardening 流程因為只 audit git tracking 範圍而漏掉 — 未來 release 看到
  out-of-tree CLAUDE.md 也應該檢查。

## 4. 過程留下的教訓(durable, v3.2.1-specific)

1. **release-hardening 7-agent parallel audit on patch release** — v3.2.1 是 test +
   docs + CI patch,scope 比 v3.2.0 窄但 audit 還是 catch 8 + 8 + 5 + 15 + 14 + 13 = 63
   findings。**cross-confirmation 才是高信號**:UE count stale 被 B + E + G 三 agent
   各從不同角度 flag(B 從 oracle 對齊 / E 從 docs cartography / G 從 reproducibility),
   = 高信心必修;反之獨家 finding 要先驗證(D-1 unity collision + E-1 README.md "57 tests"
   都驗證後成立)。

2. **`Plugins/FrameSolver/README.md:47` "57 tests" stale 一年以上** — 自 v2.10 之前
   沒人改;v2.11.x → v3.0.x → v3.1.x → v3.2.0 都沒人 catch。**這是新貢獻者讀的第一份檔**。
   未來 release-hardening 必跑「外部入手檔 first-touch sweep」— 不只 README, 還包括
   Plugin 各自 README + QuickStart docs(它們有自己的數字)。

3. **`bUseUnity = false` 是 module-with-tests 的鐵律** — `FrameCoreUE.Build.cs` v3.2.0
   是 `bUseUnity = true`(沿用 default),v3.2.0 release 通過 audit 是因為 incremental
   build 在 working set 內 adaptive excludes 新 .cpp。但 commit 後 fresh clone 跑 clean
   build 會 unity merge → anon namespace 衝突。CLAUDE.md 踩雷 #4 寫明 "UE
   `bUseUnity=false` / Adaptive Build 自動排除新 .cpp 出 unity",當時是
   `FrameCore.Build.cs`(已 `bUseUnity = false`);v3.2.0 加新 module 漏 carry 同樣設定。
   未來加新 module 時 default 就 `bUseUnity = false`(尤其有 `Private/Tests/`)。

4. **post-tag in-place RELEASE notes edits 是 docs-discipline 問題** — `RELEASE_v3.2.0.md`
   被 commit `6d1fdb0` 把 U-04 從 deferred flip 成 CLOSED(post-tag);GitHub release
   page 還是舊版 → divergence。v3.2.1 決定:**RELEASE_vX.md 一旦 tag 就凍結**;
   post-tag 工作的新數字 / 新 closed 放到下一個 RELEASE_vX+.md 與下一個 HANDOFF。
   未來 release-hardening 嚴守:每個 release 一個 freezing 點,after-tag 改動進下一個
   release。HANDOFF 可以 append(這是 active doc),但不能 rewrite tag-time 歷史。

5. **`kEngineVer` / `.uplugin VersionName` bump 條件** — v3.2.1 audit Agent B 提出
   policy 建議,v3.0.1 → v3.1.0 bump 因為有 protocol change(`inspect.stress_field`
   capability),v3.0.0 → v3.0.1 bump 因為有 CI / strict fingerprint 改動。但 v3.2.1
   patch 純 test + docs + CI hygiene + clean-build safety,**沒有 engine / 協議 / capability
   改動**,所以 `kEngineVer` 不 bump。Clients pin `3.2.0` 看 bit-identical 行為。
   未來 patch:engine source delta = 0 且 capability list 不變 → 不 bump pins;否則 bump。

6. **`run_gate.ps1` 不 build UE 的 count guard 是金線** — v3.2.1 加了 8 個新 UE test;
   接手 owner pull v3.2.1 後若忘 rebuild,UE leg 跑舊 binary `Total tests = 62 < 70`
   → GATE FAIL。這是 intended 行為(per `-ge` guard)。Release notes §7 寫明
   「必須 Build.bat ArchSimEditor 再跑 gate」。

7. **release-hardening Phase 1 audit scope 必須涵蓋 cumulative cross-release**,
   不能只 patch scope。v3.2.1 第一輪 7-agent audit 只看 `v3.2.0..HEAD` patch scope,
   修了 8 處 stale UE counts;但 user 要求擴大到 `v3.0.0..HEAD` 整 v3.x 系列後又找到
   **6 處 cumulative stale**(repo-root `README.md` "V3 STABLE gates" contract 4 處
   + `VERIFICATION.md` gate suite (c) 1 處 + `run_gpu_gate.ps1` header comment 1 處)
   — 都帶 v3.0/v3.1-era 數字(UE 60/60, kEngineVer "3.1.0", F1..F67),累積跨 4 個
   release 沒人 catch。**未來 release-hardening 應該 default 雙層 sweep**:
   (a) `<prev-tag>..HEAD` patch scope(已 covered);
   (b) `<latest-major-anchor>..HEAD` cumulative scope(本 lesson)。
   "V3 STABLE gates" 這種跨多個 release 都不變但內含具體 current state 數字的章節
   是最容易被各個 release 漏修的地方,因為它看起來像 historic anchor。

8. **out-of-tree CLAUDE.md 不會被 release-hardening 自動 catch** — `E:/project/CLAUDE.md`
   是 `ArchSim/` parent 目錄的檔,接手 owner 開新 session 自動載入但不在 git tracking。
   v3.0..v3.2 都沒同步它,line 13 還說 HEAD `v2.11.1`。release-hardening 主要 audit
   git tracking 範圍,out-of-tree 的 user-context 檔屬於 §3.3 「接手 owner 應知」項目。

## 5. 後續方向(無排序)

**最近 + 高價值(v3.2.2 / v3.3):**

1. **V321-01..04 落地** — 把 v3.2.1 audit defer 的 4 個 test 強化在 v3.2.2 串成一個
   小 patch;這 4 個都是 < 30 LOC 但需要 UE rebuild + 全 leg re-verify。

2. **V321-05 test helpers .h** — 抽 forward decl 共用;影響 6 test files,1 個新 header,
   降低未來 marshal API drift 風險。

3. **U-07 sentinel mismatch** — v3.3 主軸第一步。`StressField.h` `governingMemberId = 0
   → -1`,`computeStressField` 結尾 guard `if (!members.empty() && worstFound)`,
   Dispatcher.cpp / FrameCoreUE 都自動跟上。需 engine source change,屬 v3.3 scope
   不是 v3.2.x。

4. **真實 renderer(U-03)** — v3.3 主軸第二步。v3.2 出 USTRUCT data;v3.3 出 visual。

**v3.2.2 / v3.3 candidate(carry-forward from v3.2.0):**

- U-01..U-06 剩餘(BP load JSON / Slate fixture dropdown / renderer / spawner sanity /
  double USTRUCT / VS toolchain pin)
- F71 +Z, D-05 v1 CLI STRESS, E-07 v2 inspect protocol spec, E-13 S11 naming 解決
- V321-01..06(本檔 §3.2)

---

接手有問題:
- 5-min 入手指南: [`docs/FrameCoreUE_QuickStart.md`](FrameCoreUE_QuickStart.md)(v3.2.0
  post-tag Phase 6h 寫,v3.2.1 audit 補 Prerequisites 段;適合新 contributor)
- 主交接鏈: `docs/HANDOFF.md` → `docs/HANDOFF_v2.11.1.md` → `docs/HANDOFF_v3.1.0.md`
  → `docs/HANDOFF_v3.2.0.md` → 本檔 → `docs/RELEASE_v3.2.1.md`
- S11 / StressField 問題讀 [`docs/specs/S11_stress_field.md`](specs/S11_stress_field.md)
- v3.2 UE 介面 plan 讀 [`docs/PLAN_v3.2_ue_interface.md`](PLAN_v3.2_ue_interface.md)
  (v3.2.0 night-shift plan)
- v3.2.0 night-shift log [`docs/NIGHT_SHIFT_2026-06-22.md`](NIGHT_SHIFT_2026-06-22.md)
  (含 Phase 6 a-h 完整 narrative,凍結)
- Engine 全圖 [`docs/ARCHITECTURE.md`](ARCHITECTURE.md)
- 驗證 / 證據鏈 [`docs/VERIFICATION.md`](VERIFICATION.md)
