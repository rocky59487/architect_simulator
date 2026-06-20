# 交接指南 — `v2.8.1` 後接手 owner

> `v2.8.1` 在 2026-06-20 發布。tag `v2.8.1` = commit `(本檔寫於 commit 前;tag 後填入)`。
> 主交接文是 [`HANDOFF.md`](HANDOFF.md)(原始 vX.Y, 不動);前 cycle 交接是
> [`HANDOFF_v2.5.md`](HANDOFF_v2.5.md)。**v2.6 / v2.7 沒有獨立 handoff(E-01 / E-02)**;
> 本檔合補 v2.6 / v2.7 / v2.8.1 三 cycle 的 deferred + workspace state。

## 1. `v2.8.1` = 什麼

一句話:**對 v2.6 + v2.7 兩個 release 做 7-agent 平行 adversarial audit,落實所有
oracle-backed small-fix,把 v2.6 / v2.7 漏掉的 handoff 補上**。使用者明確表達對
v2.6 / v2.7 沒信心 → audit-hardening release。

| | v2.5 → v2.7 (existing) | v2.5 → v2.8.1 (cumulative) |
|---|---|---|
| FrameCore engine .cpp/.h 改動 | 2 檔 / +31 / -1(DynamicCollapse only) | 2 檔 / +47 / -1(同上 + v2.8.1 NaN guard 三處,只在 .cpp) |
| v2 dispatcher 改動 | Dispatcher.{h,cpp} + ModelBuilder.h + frame_capi_v2.{h,cpp} | 同上 + v2.8.1 kEngineVer bump + dead-field 三件套移除 + inbound queue cap |
| C# GH bridge 改動 | 6 檔(D-01..D-10 fix) | 同上 + v2.8.1 CApiV2Transport.DisposeAsync UAF fix |
| docs 改動 | 4 處(RELEASE_v2.5 +4 行 / RELEASE_v2.6 新建 / RELEASE_v2.7 新建 / VERIFICATION +6 行 / HANDOFF_v2.5 errata) | 同上 + RELEASE_v2.8.1 新建 / HANDOFF_v2.8.1 新建 / docs/README.md 索引 / README.md status / CLAUDE.md / E-07 dead-link 兩處修 / E-09 workspace-notes 移轉 / environment.yml 新增 |

### v2.8.1 整入了什麼 audit-driven small-fix(全 oracle-backed)

| Audit ID | Category | 修了什麼 | LOC |
|---|---|---|---|
| **A-01 / B-04 / E-03 / F-01** ★4-agent | engine version drift | `kEngineVer` 從 v2.5 起停在 `"2.5.0"` 兩個 release → bump 到 `"2.8.1"`(`Dispatcher.h:75`)。註解明示這是 4-agent confirmed。 | 4 |
| **B-05 / E-04** | engine version drift | `FrameSolver.uplugin VersionName` 同樣停在 `"2.5.0"` → bump `"2.8.1"`。 | 1 |
| **A-02 / A-03 / A-04** | engine NaN guard | `runDynamicCollapse` Newmark loop 之前**沒有**對 `uN`/`vN` 的 `isFinite` 檢查。任何 standalone caller(F-fixtures、直接 C++ user)收到 NaN frame 卻看到 `outcome=MaxSteps`,因為 loop-end 的 unconditional 寫覆蓋了 dispatcher onFrameEmitted 設的 Invalid。修法:`storeFrame` lambda 內 `isFinite.all()` short-circuit,設 `nanAbort` flag 給 main loop 三個 `storeFrame` 點 bail-out。 | 16 |
| **C-02** | dispatcher OOM | `frame_v2_send` 的 inbound 隊列無上限。長 handler(`solve.dyn_collapse`)blocks worker 數秒,快 client 期間可塞 1000+ frames。加 256 frame 上限,超過回 `FRAME_V2_OUT_OF_MEMORY`。 | 8 |
| **C-11** | dispatcher dead code | `Dispatcher::lastError_` + `errMtx_` + `LastError()` 三件套全 declared 但**從不寫入**(grep `lastError_ =` 0 hits)。`frame_v2_last_error` 也只讀 ctx-level `lastError`,從不 fallback 到 dispatcher 端。移除整套 dead code。 | -10 |
| **D-10b** | C# bridge UAF | `CApiV2Transport.DisposeAsync` 直接 `_closeDelegate(_ctx)` → `NativeLibrary.Free(_libHandle)`,**沒有先**喚醒可能阻塞在 `frame_v2_recv(blockingMs=-1)` 的線程。一旦有 recv 在 wait,Free 後 `_ctx` 變懸空。修法:close 前先 `_cancelRecvDelegate?.Invoke(_ctx)`(非阻塞,設旗 + cv.notify)。 | 3 |
| **E-07** | docs dead link | `POST_V2_5_HARDENING.md` 從未被 author 但 `RELEASE_v2.5.md:4` + `VERIFICATION.md:62` 兩處 cite,陌生人跟 trail 走入 404。兩處改 cite `RELEASE_v2.6.md` / `v2.7` / `v2.8.1` 真實 release 鏈。 | 9 |
| **E-09 / G-01** | docs privacy leak | `RELEASE_v2.6.md:145` 內有 `E:\project\v2-audit` hardcoded 路徑 + `stash@{0}` 已 drop 但 release 還寫 "safe to drop after v2.6 ships"。 In-place errata:整段移轉到本 HANDOFF §6 workspace state。 | 8 |
| **E-05** | README status | README.md L19 + L21-30 寫 "bundled v2.5 release" + "Status (2026-06, v2.5 — B3 ...)",凍結兩 release 沒更新。整段改寫為 v2.8.1 release truth。 | 18 |
| **E-06** | CLAUDE.md staleness | `E:\project\CLAUDE.md:13` 寫「現況(2026-06-19, tag `v2.4` + v2.5 build-up 7 commits)」— 新 session 開出來就以為現況是 v2.5 build-up。更新到 v2.8.1。**註**:CLAUDE.md 在 repo 外不入 git commit。 | 2 |
| **E-10** | docs/README.md index | docs/README.md Rhino bridge 區段寫「B3-B7 待辦(見 HANDOFF_v2.4)」,五個 RELEASE/HANDOFF 沒入索引。補。 | 14 |
| **G-02** | env-doc reproducibility | `environment.yml` 從未存在。陌生人無法重建 `framecore-direct` conda env。`conda env export -n framecore-direct --no-builds` 後手動清 prefix / 鎖 OpenSeesPy 版本上界 / 加 README setup 註解。 | new file (74) |
| **B-01** | F-fixture 號誤導 | RELEASE_v2.6/v2.7 + README 寫 "F1..F64" 暗示 64 fixtures,真實 62 個(F41 + F60 跳號,合理但未文件化)。README + 本 HANDOFF + RELEASE_v2.8.1 都改寫「62 individual F-fixtures spanning F1..F64」。 | 3 |

### 引擎源碼 delta vs v2.7(byte-level)

```
Plugins/FrameSolver/Source/FrameCore/Private/DynamicCollapse.cpp     | +16 -0   (A-02/A-03/A-04 NaN guard, 3 sites)
Plugins/FrameSolver/Standalone/v2/Dispatcher.h                       |  +9 -6   (kEngineVer "2.5.0"→"2.8.1" + dead-field trio removal + LastError 註解)
Plugins/FrameSolver/Standalone/v2/Dispatcher.cpp                     |  +4 -3   (移除 LastError() body, 留 audit-comment marker)
Plugins/FrameSolver/Standalone/frame_capi_v2.cpp                     |  +9 -0   (C-02 inbound queue cap 256)
Plugins/FrameSolver/FrameSolver.uplugin                              |  +1 -1   (VersionName "2.5.0"→"2.8.1")
Plugins/FrameSolver/Grasshopper/v2/Bridge/CApiV2Transport.cs         |  +8 -0   (D-10b cancel-recv before close+Free)
```

ABI: `abi_version = 2` **不變**。

### 沒入 v2.8.1 的(intentional)

LevelSim 完全不動(v2.6/v2.7 / v2.8.1 都未動)。standalone fixture 命名也不動(F41/F60 補回是 next-cycle 動作,本 release 只 clarify 文檔)。`run_gate.ps1` `$ExpectedUeTests=57` 不變(UE source delta = 0)。

## 2. 怎麼跑 (verification reproduce)

```bash
# 0. (一次) 重建 conda env (G-02 close)
conda env create -f environment.yml
conda activate framecore-direct
# 或環境變數 override (G-04 / G-05 半 close):
set FRAMECORE_LIB_DIR=D:\miniconda3\envs\framecore-direct\Library
set SUPERNODAL_CONDA=%FRAMECORE_LIB_DIR%

# 1. standalone gate (秒級, 含 supernodal lane)
cd Plugins/FrameSolver/Standalone && build.bat        # F1..F64 範圍 62 fixtures, ALL PASS

# 2. linear deep audit
cd Plugins/FrameSolver/Standalone && build_linear_audit.bat && linear_deep_audit.exe   # 104 checks PASS

# 3. v2 dispatcher round-trip (第 6 leg)
cd Plugins/FrameSolver/Standalone && build_capi_v2.bat
python Tools/v2_roundtrip.py                          # 含 P1-3 live_frames == nFrames

# 4. OpenSees cross-validation (leg 3)
python Tools/opensees_compare.py                      # PASS, 須 env 內 openseespy

# 5. CLI round-trip (leg 5)
python Tools/cli_roundtrip.py                         # ALL PASS

# 6. UE automation (leg 2)
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
# 預期: 五腿全綠, $ExpectedUeTests=57
```

## 3. 新 API surface(v2.8.1)

無 — v2.8.1 全部是 audit-driven 小修正,沒加新 method / capability / token / flag。`frame_v2_engine_version()` 現在誠實回傳 `"2.8.1"`(之前 v2.6 / v2.7 都回傳 `"2.5.0"`)。

## 4. 仍 deferred 的 items(v2.8.1 + 繼承 v2.6 / v2.7)

對每項加 **First action on day 1** 一行 —— 隔日打開 repo 第一個動作。

### 從 v2.6 / v2.7 繼承

1. **B5.2 ReSolveSession session-cache routing** — `analysis.reanalysis_solve` 現每次重建 session,不享 Woodbury → stale-LDLT PCG → rebaseline ladder 的長壽 cache。 
   **First action**: `Plugins/FrameSolver/Standalone/v2/Dispatcher.cpp` HandleReanalysisSolve 改先 `sess->resolveSession` 是否 valid;invalid 就 `sess->resolveSession = std::make_shared<ReSolveSession>(...)`。schema fingerprint(model + sec/mat hash)入 `SessionContext` 用於 invalidate。

2. **model.patch schema 未定** — Dispatcher.h L40 / L122 / Dispatcher.cpp L21 三處 comment 寫 "schema TBD",handler return NOT_IMPLEMENTED。
   **First action**: 新建 `docs/specs/S6d_model_patch.md`,定義 add/remove/update verb,key-by-id semantics,以及與 `model.set` 的差異(全替換 vs 增量)。然後 Dispatcher.cpp 解 patch + apply 到 `sess->model`。

3. **C-09 / C-10 widening** — 現是 NOT_IMPLEMENTED reject;ergonomic 改進是讓 supernodal session 開 transient LDLT side-system 跑 modal/buckling 而不丟掉 supernodal factor。
   **First action**: `Dispatcher.cpp:1119` HandleModal 內 `sess->useSnSession` 路徑加分支 — 用 `sess->sn->K_ff` 重建一個 transient `PreparedSystem` 跑 modal,advertise 新 capability `analysis.modal.ldlt_fallback`。

4. **Live `dyn_collapse.event` channel** — v2.7 只 stream frames live;structural events(碎塊脫離)仍 post-run。 
   **First action**: `DynamicCollapse.h` 加第二個 callback `onEventEmitted(const DynCollapseEvent&)`;`DynamicCollapse.cpp:H.events.push_back` 後 invoke。`Dispatcher.cpp:1248` HandleDynCollapse 把 callback 接上 `d.Emit("dyn_collapse.event", ...)`,並 advertise `dyn_collapse.live.events`。

5. **C# `dotnet build` 驗證** — 本機沒 .NET SDK,v2.6 / v2.7 / v2.8.1 都沒跑 `dotnet build`。
   **First action**: `winget install Microsoft.DotNet.SDK.8`(或 7);`cd Plugins/FrameSolver/Grasshopper && dotnet build -c Release`;補 GHA `.github/workflows/csharp.yml`(matrix net7.0 / net8.0)。 

### v2.8.1 audit 新 deferred(MAJOR 但無 in-cycle oracle)

6. **G-07 wire-order breaking** — v2.7 `streamFrames=true` 把 frames 從 post-final-response 移到 mid-run。v2.7 RELEASE 寫 "Breaking changes: None" 對 buffer-then-process 的 client 是誤導。
   **First action**: `gh release edit v2.7 --notes-file -` 把 wire-order change 補進 v2.7 release body 的 "Client-visible behavior change" 段(不重 tag);v2.8.1 RELEASE 同步 acknowledge。

7. **G-06 RELEASE 缺 Repo URL** — RELEASE_v2.6 / v2.7 缺 `**Repo:** https://github.com/rocky59487/architect_simulator` 行(v2.3-v2.5 / v2.8.1 都有)。
   **First action**: `gh release edit v2.6 --notes-file ...`(同上,加 Repo 行,不重 tag)。

8. **G-04 / G-05 env-var docs** — `SUPERNODAL_CONDA` / `FRAMECORE_LIB_DIR` 在 `build_capi_v2.bat:23` + `Tools/v2_roundtrip.py:50` 有 escape hatch 但 README setup 段沒文件化。
   **First action**: README.md 加 "## Setup" 段,列三個 env var(`UE_ENGINE_ROOT` / `SUPERNODAL_CONDA` / `FRAMECORE_LIB_DIR`)及預設值。

9. **G-09 / G-11 / G-12 hardcoded UE / 舊 repo URL** — `docs/HANDOFF.md:37`(`E:\project\UE_5.7\...`)、`docs/AGENT_PROMPT_S2_S4.md:50`(同)、`docs/specs/S6c_rhino_ux_commercial.md:485` + `docs/RELEASE_v2.2+1.md:4`(舊 repo URL `architect-` 404)。
   **First action**: 一次性 `sed -i 's|E:\\project\\UE_5.7|%UE_ENGINE_ROOT%|g'` + repo URL 字面替換。注意 HANDOFF.md 是 frozen,改字面是錯;改在 HANDOFF_v2.8.1 errata 紀錄即可。

10. **F41 / F60 fixture 號** — 真補回 vs 永遠跳號?
    **First action**: 決定 policy。若補回:挑兩個 v2.x 加的新斷面 / 新驗證點(殼 K_σ 旋轉等變、co-rot 弧長 snap-through 的 extreme case 等)當 F41 / F60;若永遠跳號:在 main.cpp 開頭 comment block 明寫"F41 / F60 are intentionally absent — historical numbering preserved"。

11. **A-04 cancel poll latency at post-event storeFrame** — v2.8.1 已修(line 458 post-event `storeFrame` 之後加 `if (nanAbort) return H;` 但**沒**加 isCancelled poll)。
    **First action**: line 458 之後加 `if (opts.isCancelled && opts.isCancelled()) { ... return H; }`(<5 LOC)。

12. **F-02 / F-07 micro-perf** — `abortReason` 從 `shared_ptr<string>` 改 `optional<string>`;`Capabilities()` 改 static const ref。Hot path 微改善,單 release 無 oracle 直接打到。
    **First action**: 直接動,v2_roundtrip 是 oracle。

13. **C-09 binary payload size 驗證** + **C-10 frame_v2_open() thread quota cap** + **C-13 MiniJson depth counter** — 全 dispatcher 細節,defer 到 quality-sweep。

14. **G-08 / G-13** — environment.yml v2.8.1 已 close;`framecore-v2.8.1-win64.zip` asset 名稱列入 RELEASE_v2.8.1.md。

## 5. v2.8.1 audit 過程留下的教訓(durable)

(a) **多 agent 平行 audit 的 cross-confirmation 才是真高信心**。`kEngineVer` 被 A / B / E / F 4 個 agent 獨立提出 — 那不是 noise,是「肯定漏的問題」。如果只跑 1 個 agent,這個 finding 可能還在 list 中段;4-agent 同題就是頂部。Release-hardening 的核心步驟是「7-agent parallel → unified findings table」。

(b) **`grep -c` 不能取代 runtime verification**。Agent B 提了 deep_audit `addRow` 靜態計數 108 vs 文件 104 — 但實際 runtime 是 104。`grep -c 'addRow'` 包含 declaration line + early-exit branches。**任何 numeric claim 都要跑一次 runtime 確認**,不能停在 grep。

(c) **stale comment 也是 shipped surface area**。`RELEASE_v2.5.md:4` 的 `POST_V2_5_HARDENING.md` dead link 跨了 3 個 release(v2.5/v2.6/v2.7),陌生人 follow trail 直接 404。release-hardening Phase 4 必須 grep dead link。

(d) **engine code 改 0 行 ≠ engine 行為不變**。v2.7 號稱「FrameCore engine 0 行動」是真的(0 .cpp 改),但 v2.7 加的 callback hook 通過 `DynCollapseOptions` 影響整個 storeFrame path 行為(NaN abort、cancel timing、frame emit point)。dispatcher 用 callback wire 起來後,engine API surface 已經改了(`onFrameEmitted` / `isCancelled` 是 source-compatible additive 但 semantic 新),standalone caller 也受 NaN 沒 guard 影響。**"engine 0 行動" claim 要 narrow 到 "core algorithm 0 改",不能擴及 "engine semantic 0 影響"。**

(e) **frozen release docs 的 in-place errata 是合理的**。v2.6 commit 770b23c 改了 v2.5 docs(VERIFICATION +6 / RELEASE_v2.5 +4 / HANDOFF_v2.5 path errata)— 沒重 tag v2.5,但留下 audit trail。v2.8.1 同精神:in-place 修 RELEASE_v2.6.md workspace notes leak,commit message 明示 "audit E-09 / G-01"。GitHub published release body 用 `gh release edit` 同步,不重 tag。

(f) **Auto Mode 下,4-agent confirmed finding 就直接動工**。使用者「對 v2.6 / v2.7 沒信心」+ 「審核」是明確授權。不問逐項 confirm,跑完 7-agent → 整合 → 動工,並在 RELEASE notes 列每項 audit ID 讓使用者複核。

## 6. Workspace state(從 RELEASE_v2.6.md 移轉,E-09 fix)

這些是 v2.6 / v2.7 / v2.8.1 cycle 期間的本機開發狀態,**不影響什麼被 published**,但下個 session 開出來不會誤踩鬼影:

- **`Plugins/LevelSim/Binaries/Win64/UnrealEditor.modules`** v2.6 從 sibling worktree `<repo-parent>/ArchSim-levelsim` 複製進來,因為主 worktree UE rebuild 在 `Module.FrameCore.cpp` unity TU 卡 1.5 h(cl.exe ~1.6 GB RSS / 0.4 % CPU / 47 GB pagefile = swap thrash,非 compile fail)。複製來的 manifest BuildId `47537391` 與主 `FrameSolver/Binaries/Win64/UnrealEditor.modules` 對齊。五腿 gate 在此配置綠。RAM 夠的機器 clean rebuild 會原地重生 manifest;無 source 改動。v2.7 / v2.8.1 都沿用。

- **`<repo-parent>/v2-audit` worktree** (detached HEAD at `4e660de` = v2.1.0) 是 audit-checkpoint reference snapshot。Do not modify;是 canonical pre-supernodal-lane snapshot。原本路徑在 RELEASE_v2.6.md L145 是 `E:\project\v2-audit`(本機絕對路徑),v2.8.1 G-01 移到這裡並改 portable wording。

- **`stash@{0}`(`pre-release-hardening: stale uproject+DefaultGame.ini local hookups`)** 在 v2.6 cycle 後已 drop(MEMORY.md 也錨點到「stash@{0} dropped」)。內含的 ArchSim.uproject + Config/DefaultGame.ini LevelSim hookups 因 LevelSim 改用 `.uplugin` `EnabledByDefault=true` 而 obsolete。

- **UE incremental build 1h57m swap thrash 教訓**:31 GB RAM / 47 GB pagefile 的機器跑 v2.7 cold-cache 確實**能完成**(不是 fail)。1.5 h 等待是 swap thrash 不是 deadlock。對策:release 前在 RAM 充裕機器 build 一次;cold-cache build 啟動後**不要 abort**(別人 abort 過 1.5 h 等於浪費)。

- **`build_capi_v2.bat` silent exit 0 在錯 cwd** 是 durable 踩雷 ①:在 `E:\project` 跑會「系統找不到指定的路徑」中文 + rc=0 misleading。對策:build 後比 dll mtime > source mtime。`vswhere -prerelease` 路徑是 hardcoded 假設 VS18 Community preview;Stable VS 跑會 fall through 到 error。

## 7. 下一輪方向(無排序)

- **v2.9 candidate**:落實 §4 #1-4 deferred(B5.2 routing / model.patch schema / C-09/C-10 widening / dyn_collapse.event channel),引擎完全無改動。
- **v3.0 candidate**:plastic 反轉/卸載(目前 S10 是 event-to-event 教科書,無真彈塑性),或 fiber 斷面/pushover,或 MITC9i 高階殼(memory 內 "S11" deferred,9 處引擎修改先決)。
- **C# dotnet build CI**:GitHub Actions matrix net7 / net8,close 環境依賴的最後 leg。

---

接手有問題:[`HANDOFF.md`](HANDOFF.md) → [`HANDOFF_v2.5.md`](HANDOFF_v2.5.md) → 本檔。
v2 dispatcher 特定問題:[`specs/S6b_rhino_bridge_v2.md`](specs/S6b_rhino_bridge_v2.md)。
Audit findings 細節:本檔 §1 表 + [`RELEASE_v2.8.1.md`](RELEASE_v2.8.1.md) §Audit-driven fixes。
