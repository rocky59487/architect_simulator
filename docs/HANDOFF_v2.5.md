# 交接指南 — v2.5 後接手 owner

> `v2.5` 在 2026-06-19 發布, tag `v2.5` = commit (待 `git tag -a` 落地, baseline + Phase 2 small-fixes).
> 主交接文是 `docs/HANDOFF.md`(原始 v2.0, 不動); v2.x release 系列的 follow-up handoff:
> `docs/HANDOFF_v22p1.md` / `docs/HANDOFF_v2.3.md` / `docs/HANDOFF_v2.4.md` 均凍結保留. 本檔只補
> `v2.5` 多出來的內容. v2.4 release docs 在 v2.5 cycle 動過的部分屬 errata-style follow-up
> (HANDOFF_v2.4.md §4 標 ✅, RELEASE_v2.4.md 文末 errata block); 本檔 cross-ref 不再原地改寫.

---

## 1. v2.5 = 什麼

**Dispatcher-layer release**。Engine 源碼(`Plugins/FrameSolver/Source/FrameCore/`)**自 v2.3
(`6be1dac`)起 bit-identical**;LevelSim v1.0.0 不動。所有 v2.5 改動落在:

- `Plugins/FrameSolver/Standalone/v2/` 與 `frame_capi_v2.{h,cpp}` — B3 dispatcher engine wire
  (12 method handler 真接 FrameCore)+ A-01 close UAF + B5 supernodal factor-reuse + P1/P2/P3
  review-round hardening + Phase 2 small-fixes(kEngineVer / Capabilities widening / arcLength
  guard / advancedDiagnostics 誠實分支 / ModelBuilder 驗證強化)。
- `Plugins/FrameSolver/Grasshopper/v2/` — D-03 OpenFrameCore generation race + D-09 P/Invoke audit。
- LevelSim helper scripts + `docs/AGENT_PROMPT_*.md` — reproducibility + privacy 硬化(硬編 path
  → env var + `%~dp0` fallback;session UUID / username 從 docs templatize 出去)。
- README / VERIFICATION / CLAUDE.md / MEMORY.md — v2.4 → v2.5 status 同步。

v2.4 → v2.5 source delta:

```
git diff --stat v2.4..HEAD
# ~20 files changed, +1700+/-180 (含 Phase 2 small-fixes 後 +1900+/-200)
# Engine source (Plugins/FrameSolver/Source/FrameCore/) = 0 lines changed
```

整入的 v2.4 deferred items(對照 RELEASE_v2.4.md):

| audit-id | landing commit | status |
|---|---|---|
| B3.1 `HandleSolveLinear` wire | `180c9e8` | ✅ landed; bit-exact vs v1 rel<1e-11 |
| B3.2 `build_capi_v2.bat` link FrameCore | `180c9e8` | ✅ landed; DLL ~105 KB → ~600 KB |
| B3.3 11 個 method wire | `180c9e8` | ✅ landed for 7 wired analyses + 4 inspect.* |
| A-01 `frame_v2_close` UAF | `3814f58` | ✅ landed; shared_ptr ownership registry |
| B5 supernodal factor-reuse | `214e99f` | ✅ landed; `session.open mode=supernodal` |
| D-03 GH gen race | `214e99f` | ✅ landed; `_openGate` lock |
| D-09 P/Invoke audit | `214e99f` | ✅ landed; 7 Cdecl delegate 線對線 verified |
| E-10 S6b method table | `214e99f` | ✅ landed; `[B3]/[B4]/[B5]` 狀態圖示 |
| P1 fingerprint material/section | `5c526c0` | ✅ landed; Y/Z 滑桿 silent stale cache fix |
| P2 cancel tombstone + ABI doc | `5c526c0` | ✅ landed; `ClearCancelled` 雙路徑 + transport.sync flag |
| P3 MiniJson raw control char + ERANGE | `5c526c0` | ✅ landed |
| transport.sync capability advertising | `b3cea8b` | ✅ landed; `Capabilities()` 含 transport.sync |
| 7 wired analyses 加入 Capabilities | Phase 2 | ✅ landed; `Capabilities()` 從 7 升 18 |
| 4 inspect.* 加入 Capabilities | Phase 2 | ✅ landed |

沒動的子系統:Engine source(0 行)、LevelSim core(0 行)、UE automation(57 tests,
engine unchanged 不需重跑)、OpenSees mega benchmark(engine unchanged → v2.3 的 128/0 result 仍有效)。

## 2. 怎麼 reproduce(v2.5 release verification)

```bat
:: 最快:standalone gate L1
Plugins\FrameSolver\Standalone\build.bat
:: 預期: ALL PASS (failures=0)

:: 五腿主 gate(需 conda + openseespy + UE 5.7)
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees

:: 第六腿(v2 dispatcher round-trip,手動)
Plugins\FrameSolver\Standalone\build_capi_v2.bat
python Tools\v2_roundtrip.py
:: 預期: 34 PASS / 0 SKIP / 0 FAIL; v=2.5.0; build_sha=<HEAD short SHA>

:: LevelSim 子 gate
Plugins\LevelSim\Standalone\build.bat
:: 預期: ALL PASS (failures=0) 115/115
```

環境前置(release-hardening reproducibility checklist):
- VS18 Community **preview**(cl.exe via `vswhere -prerelease` + `vcvars64.bat`)
- conda env `framecore-direct`(OpenBLAS + METIS;`SUPERNODAL_CONDA` env var 可指向非預設位置)
- UE 5.7 引擎(L2 only;`UE_ENGINE_ROOT` env var,LevelSim run_*.bat 也吃這個)
- Python 3.10+ with `openseespy>=3.5`(L3 only)

## 3. v2.5 新加的 token / API / capability

### `frame_capi_v2.h` ABI 不變(`abi_version=2`),但行為層:

- `kEngineVer = "2.5.0"`(was `"2.4.0"`)— `hello.response.engineVersion` 跟著升。
- `hello.response.capabilities` 從 v2.4 的 7 項升到 **18 項**:
  - 既有 7:`cancel`, `profile.{simple,advanced}`, `session`, `model.set`, `solve.linear`, `transport.sync`
  - 新加 7 analyses:`solve.pdelta`, `solve.tension_only`, `solve.size_opt`, `solve.corotational`, `solve.arclength`, `analysis.modal`, `analysis.buckling`
  - 新加 4 inspect:`inspect.disp`, `inspect.reactions`, `inspect.member_forces`, `inspect.shell_forces`
- `session.open body.mode = "supernodal"` — opt-in B5 路徑;`model.set` 會 eagerly factor SnSession,
  `solve.linear` 走 `sn->solveFrame` 重用 factor。LDLT 永遠 fallback。
- `solve.linear` advanced profile response 的 `advancedDiagnostics.factorMethod` / `.factorBackend`
  根據 session 實際走的 path 真實分支:
  - supernodal session:`"Supernodal" / "SnChol_selfbuilt"`
  - default session:  `"LDLT" / "SimplicialLDLT"`
  - `factorTimeMs` / `solveTimeMs` 仍 `0.0`(B4 才實際計時;CLAUDE.md 鐵則 #3 禁止編造數字)。
- `solve.arclength` arcLength 缺失或 0 的契約變化:
  - simple profile:silent 接受 engine 的 auto-estimate,但記在 `defaultsApplied: ["arcLength=auto (engine first-tangent estimate)"]`
  - advanced profile:回 `VALIDATION_FAILED`(避免 silent 跳過 snap-through limit point)
- `nodalLoads[k].comp` 在 `model.set` 必填(v2.4 silent 接受缺失 → 零載荷;v2.5 回 `VALIDATION_FAILED`)。
- `hinges[k].dof` 在 `model.set` 必須 ∈ {4, 5, 10, 11}(v2.4 default 0 silent miscompile;v2.5 回 `VALIDATION_FAILED`)。

### 仍 NOT_IMPLEMENTED(不在 capabilities):

- `solve.dyn_collapse` — 等 B4 streaming + binary payload
- `analysis.reanalysis_solve` — 等 B5.2 ReSolveSession wire
- `model.patch` — schema 未決

## 4. 仍 deferred 的 items(對齊 RELEASE_v2.5.md "Deferred to v2.6")

每項一行 first-action sketch — day-1 owner 打開 repo 第一個動作該做什麼。

### Threading / async(B4 cycle)

1. **C-06 / C-07 per-session worker thread**
   First action: rewrite `Plugins/FrameSolver/Standalone/v2/Dispatcher.{h,cpp}` `Submit/Recv` —
   `EngineSession` 持自己的 worker thread + `boost::asio::strand` 或 `std::condition_variable` +
   分離的 cv/outbound mutex;`handler` 內每 N 步呼叫 `ctx.IsCancelled(reqId)` 並提前 return;
   rebuild via `build_capi_v2.bat`;新增 `v2_roundtrip.py` cancel-mid-long-solve case 驗證
   stuck thread 不存在。

2. **B4 streaming `solve.dyn_collapse`**
   First action: gated on (1) 完成。`HandleSolveDynCollapse` enqueue 每幀 96 bytes/node binary
   payload 走 `FRAME_V2_PAYLOAD_BINARY` flag,最後幀帶 `FLAG_END_OF_RESPONSE`;
   `v2_roundtrip.py` 加 N4 dynamic-collapse driver fixture 驗證連續幀序 + 取消半路。

3. **A-05 / F-06 `frame_v2_last_error` race**
   First action: `frame_capi_v2.cpp` `frame_v2_last_error` 改為 thread_local `static
   thread_local std::string` 緩存,在 mutex 內 copy `ctx->lastError` 後放鎖再回 `c_str()`;
   ASAN 跑 `Tools/v2_roundtrip.py` 確認 UB 消失。

### Engine / dispatcher physics

4. **B5.2 `analysis.reanalysis_solve` wire**
   First action: `Dispatcher.cpp` `HandleReanalysis` 取代 `notImpl` — 用 `ReSolveSession`
   (`frame::ReSolveOptions` JSON-parse,參考 S1 ladder spec);F49 fixture 驗證 vs fresh solve
   ~1e-12;`v2_roundtrip.py` 加 reanalysis case。

5. **C-09 / C-10 supernodal vs modal/buckling 分流**
   First action: `Dispatcher.cpp` `HandleModal` / `HandleBuckling` 在 `sess->prepared` 存在
   但 `usingSupernodalPrimary()` 為 true 時,**建一個 fresh default PreparedSystem** 給 solveModal /
   solveBuckling(不重用 SnPrimary);在 docs/specs/S6b_rhino_bridge_v2.md 註明 dual-prepared 規則。

6. **C-03 per-member D/C output in `solve.linear`**
   First action: 設計 `solve.linear` response schema 擴充 — body `wantDC: true` opt-in 後回
   `dc: { "<memberId>": {comb, axial, bendingY, bendingZ, shear, vm} }`;Dispatcher.cpp
   `HandleSolveLinear` 從 `frame::ElasticAllowable` 取 `worstUtilization`;v2_roundtrip 加 oracle。

7. **C-05 ModelBuilder duplicate-ID rejection**
   First action: `ModelBuilder.h` `buildModelFromJson` 每個 vector(nodes/members/shells)
   build 過程中維持 `std::unordered_set<int>` id-seen,duplicate → 立即 `VALIDATION_FAILED`;
   v2_roundtrip 加 duplicate-id fixture。

### C# Grasshopper hardening

8. **D-01 UtilizationFringeComponent placeholder D/C**
   First action: `Plugins/FrameSolver/Grasshopper/v2/Rhino/Components/Visualize/UtilizationFringeComponent.cs:69`
   把 `sigmaI = Math.Abs(mf.Ni) / 1.0` 改成 `sigmaI = double.NaN` + 加 `AddRuntimeMessage(GH_RuntimeMessageLevel.Warning,
   "Native D/C output deferred to v2.6 (audit C-03); rendering as NaN until then.")`;wait until
   C-03 lands 再恢復真實值。

9. **D-02 `_opening = null` 入鎖**
   First action: `OpenFrameCoreComponent.cs:172` 把 `_opening = null` 移進 `lock (_openGate) {...}`
   緊接 `_session = newSession` 之後;手測 reset + open 同時並 verify 沒看到 partial state。

10. **D-06 zero-size frame guard**
    First action: `CApiV2Transport.cs:130` 探測 recv 後若 `size == 0` 直接 `return
    ReadOnlyMemory<byte>.Empty`;加 header-only ack frame fixture。

11. **D-07 `_cachedFs volatile`**
    First action: `AssembleModelComponent.cs:71` 改 `private FrameSession? _cachedFs` → `private volatile FrameSession? _cachedFs`。

12. **D-08 end-to-end C# cancel test**
    First action: 新建 `Plugins/FrameSolver/Grasshopper/v2/Tests/CancelE2ETest.cs`(or `Tools/v2_csharp_cancel_test.py`
    用 pythonnet 呼叫),建 FrameSession,送長 solve,取消 CTS,驗 OperationCanceledException。

13. **D-10 `DisposeAsync` ValueTask.CompletedTask**
    First action: `CApiV2Transport.cs:184` 移除 `async` 關鍵字 + `await Task.CompletedTask`,改 `return ValueTask.CompletedTask`。

### Docs / infrastructure

14. **B-11 B5 latency oracle**
    First action: `Tools/v2_roundtrip.py` 加新 fixture:同 model.set 後跑 N=100 次 solve.linear,
    用 `time.perf_counter()` 量總 elapsed,assert `elapsed < threshold_ms`(supernodal mode 對
    LDLT mode 應 factor-amortise 顯著)。release notes 改標 `[VERIFIED: latency speedup XxN]`。

15. **B7 Rhino 8 GHA dotnet build**
    First action: 在 Rhino 8 dev machine 跑 `yak install RhinoCommon → dotnet build
    Plugins\FrameSolver\Grasshopper\v2\FrameCore.Gh.csproj -c Release → yak build .` 並
    verify `.gha` 載入 Grasshopper。Source 自 v2.4 起未動,build 應該乾淨。

16. **F65 / F66 warped-shell fixtures**
    First action: 設計 numerically robust template — warp 4%/2%/1% 三層、加 controlled E/G/t
    讓 K_e 條件數可預測;`Plugins/FrameSolver/Standalone/frametest.cpp` 加 F65 / F66。

17. **G-08 `environment.yml`**
    First action: `conda env export -n framecore-direct --no-builds > environment.yml`,手工裁
    剪到 OpenBLAS + METIS 必要套件、加版本 pin 上界;commit + 在 README "## Build & test" 加
    `conda env create -f environment.yml`。

18. **G-09 OpenSeesPy 版本上界**
    First action: 跑 mega benchmark 確認當前 openseespy 版本,在 `docs/RELEASE_v2.5.md` 補
    `Tested with: openseespy 3.x.y`;之後升 openseespy 前先重跑 mega benchmark。

19. **G-13 .gitignore 補丁**
    First action: `.gitignore` 末尾加 `*.codex/`、`*.cursor/`、`*.transcript`、`*.jsonl` —
    防火牆性質(目前 git ls-files 確認無洩漏)。

20. **F-1..F-10 code-quality sweep**
    First action: 在獨立 release cycle 用 `/code-quality-sweep` skill,不入 v2.6 release scope。

## 5. 過程留下的教訓(durable,僅本 cycle)

1. **七 agent parallel audit 統整 ONE landing plan 是 release-hardening 的核心。**
   findings 在多 agent 間重疊代表高信心(`kEngineVer` 被 4 agent 飆過、`advancedDiagnostics` 硬碼被 3 個飆過),
   非 noise。Phase 1 用 7 個 agent 跑出 ~80 個 raw findings,但合併去重後本 release window 只
   留 ~15 個 small-fix 真落地。每個 agent 的 findings table 要直接收進整合者 notes,不要 paraphrase。

2. **Capabilities() 廣告漏列 wired handler = 協議層不可見 = 等於沒實作。**
   v2.4 → b3cea8b 7 個 analysis handler 已 wire,但 `Capabilities()` 只列 7 項(漏 7 analyses + 4 inspect.*)。
   任何用 `hello.capabilities.includes(...)` gating 的 SDK(C# bridge / Python script / Web client)
   都會把這些 verb 當「不存在」而 silently bypass。release-hardening Phase 1 該檢查
   `Capabilities()` vs 實際 wired handler 集合,而非看 dispatcher 註冊清單。

3. **advancedDiagnostics 硬碼 backend 違反誠實驗證鐵則。**
   `factorMethod="LDLT"` 即使 session 走 supernodal 也回 "LDLT" — 是 silent lie。advanced profile
   存在的意義就是給 client 真實診斷資訊,硬編字串等於在 advanced 模式上撒謊。修是 1 行
   condition,但這個錯誤模式(「歷史欄位沒人 review,留下後人不該信的數據」)會反覆出現。

4. **`arcLength=0` advanced profile 必 reject、不然 silent snap-through skip。**
   PROGRESS_S9c.md 已知 durable lesson:auto-estimate 可能 step over limit point。dispatcher 層
   silent 接受 0 default → 把這個歷史 wisdom 廢掉。advanced profile 的契約是「強制 explicit
   intent」;default-on-zero 違反這個契約。

5. **frozen release docs 原則有灰色地帶。**
   `RELEASE_v2.4.md` / `HANDOFF_v2.4.md` 在 v2.5 cycle 被加了 ERRATA block(`a139a12` + 早些 commit),
   標明「post-v2.4 follow-up, 2026-06-19」屬可接受(讀者看到標頭就知道不是原始 v2.4 release 內容);
   但若直接改寫原文 deferred 的狀態為 ✅,則是「重寫歷史」,該移到下版本 handoff 而非原地改。
   v2.5 cycle 接受了 v2.4 docs 的 errata 形式,但 v2.5 自己不再追加 v2.4 docs。

6. **MEMORY.md / CLAUDE.md 數字漂移是 release cycle 的隱形債。**
   `UE 50 → 57` 的更新從 v2.1 audit 落地,但 CLAUDE.md 一直停在 50,memory `frame-engine-next-plan.md`
   也 stale。release-hardening Phase 4 該把這些長壽 docs 一併同步,不然下次開新 session 第一個讀
   到的就是錯數字。

7. **L2 UE NOT RUN 在 engine 0-line release 是 valid;但要寫對 reproduction command。**
   v2.5 engine 源碼 0 行動,L2 UE 重編 5-10 分鐘無意義,誠實 NOT RUN 比硬上節省 cycle。但
   RELEASE_v2.5.md / VERIFICATION.md 的 Reproduce 欄位必須給 fresh-clone reader 一句指令而非
   一段背景說明。

8. **LevelSim 三個 .bat / .py 硬編路徑是 v2.4 release-hardening 漏掉的尾巴。**
   v2.4 Phase 4.5 sanitize pass 修了 docs path,但 `run_game.bat` / `run_smoke.bat` /
   `verify_smoke_shots.py` 全是硬編 `E:\project\…`。release-hardening sanitize sweep 該 grep
   `*.bat` / `*.py` / `*.cs` 三類同樣嚴格,而非只 docs。

## 6. 後續方向

### v2.6 主軸候選

**主線 A: B4 streaming dispatcher**(per-session worker thread + binary payload + cancel-mid-stream)
— 解鎖 `solve.dyn_collapse` 與長 solve 不阻塞 transport。對 C# SDK 是 transport.sync → transport.async
的能力升級。

**主線 B: GH bridge production**(B7 GHA dotnet build + D-01 fringe D/C 真值 + D-02/D-06/D-07/D-08/D-10
C# hardening 全清)— 對 Rhino 8 user 從 alpha 到 beta-grade。需要 dev machine 有 Rhino 8 / .NET 8 SDK。

**主線 C: Supernodal 分流完整化**(C-09/C-10 modal/buckling 自動建 default PreparedSystem + B5.2
reanalysis wire + C-03 per-member D/C 在 solve.linear)— 對 advanced 結構 engineer 把 v2 dispatcher
功能集填到接近 v1 frame_cli。

### v2.5.x minor 候選

- F65/F66 warped-shell fixtures(F61 已驗 membrane-warp 收斂,只缺 fixture entry 而已)
- environment.yml conda export(G-08)+ OpenSeesPy 版本上界(G-09)
- B-11 supernodal latency oracle 補 v2_roundtrip

### 風險區(下次 release 必看)

- **`solve.linear` 沒回 D/C** 是 C# SDK 端目前 hack `UtilizationFringeComponent.cs` 自己除以 1.0
  的根因。C-03 + D-01 一起做,不然 D-01 改 NaN 後使用者看到 NaN 體驗差。
- **supernodal session 對 modal/buckling 的 dispatcher 路徑沒測。** v2.5 沒暴露
  `useSupernodalPrimary` 經 `session.open`,所以 trip 不到 C-09/C-10 bug;一旦暴露,**沒有 gate**
  捕捉。B5.2 wire 時必須同 PR 加 fixture。
- **`docs/HANDOFF.md` §10「審核遺產」的 worktree 位置**(`<repo-root>/../v2-audit`)假設 ArchSim 是
  二級資料夾;不同 user 機器上不一定成立。若該 worktree 仍有用,該寫個 retrieval script。

---

接手有問題:`docs/HANDOFF.md`(原始) → `docs/HANDOFF_v2.3.md` → `docs/HANDOFF_v2.4.md` → 本檔
HANDOFF_v2.5.md。v2 dispatcher specific 看 `docs/specs/S6b_rhino_bridge_v2.md`。 mega-benchmark
specific 看 `benchmarks/opensees_mega/README.md`。
