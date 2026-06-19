# 交接指南 — `v2.4` 後接手 owner

> `v2.4` 在 2026-06-19 發布,tag `v2.4` = (commit 見 release notes / `git show v2.4`)。
> HANDOFF chain:`docs/HANDOFF.md`(v2.1 原始) → `docs/HANDOFF_v22p1.md` → `docs/HANDOFF_v2.3.md` → 本檔。
> v2.4 在 transport line 加了大量新東西(Rhino bridge v2 + B2 dispatcher + 6th gate leg + 課程素材);
> **本檔是 wrapper**,真正的 cycle 細節在:
> - 設計階段:[`docs/HANDOFF_rhino_bridge_v2.md`](HANDOFF_rhino_bridge_v2.md)([SUPERSEDED] 史料)
> - B2 完成 + 三輪 P0/P1/P2 修補:[`docs/HANDOFF_rhino_bridge_v2_final.md`](HANDOFF_rhino_bridge_v2_final.md)
> - B2 進度日誌:[`docs/PROGRESS_B2.md`](PROGRESS_B2.md)
> - 線協議規格:[`docs/specs/S6b_rhino_bridge_v2.md`](specs/S6b_rhino_bridge_v2.md)
> - UX 規格:[`docs/specs/S6c_rhino_ux_commercial.md`](specs/S6c_rhino_ux_commercial.md)
> - 完整 release notes:[`docs/RELEASE_v2.4.md`](RELEASE_v2.4.md)

---

## 1. v2.4 = 什麼(elevator pitch)

- **FrameCore 引擎程式碼**:**完全不動**。`Plugins/FrameSolver/Source/FrameCore/` 自 v2.3 commit `6be1dac` 至本 release commit bit-identical;五腿 gate(F1-F64 / UE 57 / OpenSees / audit 104 / CLI)行為等價。
- **新增 transport line**:Rhino bridge v2 完整三層架構交付,**B2 stub level** 真實 DLL + framed JSON 線協議 + 雙 simple/advanced profile,連結真實 `frame_capi_v2.dll`(~105 KB 經 release-hardening MiniJson/FrameWire 安全修補後;HANDOFF_rhino_bridge_v2_final 與 PROGRESS_B2 的「109 KB」是修補前的 snapshot)。
- **新增第 6 gate leg(手動)**:`Tools/v2_roundtrip.py` 13 PASS / 1 SKIP,SKIP 是 `solve.linear bit-exact vs v1`(B3 wire 後升 PASS)。
- **新增 53 檔 C# SDK 骨架**:Layer 3 `FrameCore.Bridge`(net7.0 zero Rhino dep)+ Layer 4 `FrameCore.Gh`(net7.0 Rhino 8 GHA,10 個代表元件)。**未 `dotnet build` 驗**(需 Rhino 8 .NET SDK 環境)。
- **新增 26 課白板教材**(`docs/learning/`,46k 行 deep_course):**bundled courseware**,非引擎本身,不入任何 gate。
- **`.gitignore` 補上**:`*.dll`、`*.lib`、`*.exp`、`obj_capi_v2/`,build 產物不再外洩。

### 與 v2.3 的 source-line delta(從 `git diff --stat v2.3..v2.4` 精選)

| 區塊 | 行數 | 性質 |
|---|---:|---|
| FrameCore 引擎 source | **0** | 0 |
| v2 transport(`Standalone/v2/` + `frame_capi_v2.{h,cpp,bat}`)| ~1 600 | B2 骨架 |
| C# SDK(`Grasshopper/v2/`)| ~5 800 | 設計 + 骨架,zero `dotnet build` 驗 |
| Tooling + 規格 + HANDOFF | ~2 200 | `Tools/v2_roundtrip.py` + S6b/S6c + 3 份 HANDOFF + PROGRESS_B2 |
| Courseware(`docs/learning/`)| ~45 600 | 26 課白板教材 + deep_course,**不入 gate** |
| Release-hardening 修補(P1/P2 corner-case)| ~80 | A-04/C-01/C-02/C-05/D-15/H-01 + 一系列 docs 對齊 |

---

## 2. 怎麼跑 v2.4(快速 reproduce)

```powershell
# 1) 三條一定能跑的(無 conda 也行) — pure-Python + standalone CLI build
python Tools/cli_roundtrip.py                    # L5: ALL PASS
Plugins\LevelSim\Standalone\level_gate.exe       # LevelSim: 115/115 PASS

# 2) standalone 引擎 gate(需 conda framecore-direct env 提供 OpenBLAS + METIS)
Plugins\FrameSolver\Standalone\build.bat              # L1: F1-F64 ALL PASS
Plugins\FrameSolver\Standalone\build_linear_audit.bat # L4: failures=0 checks=104

# 3) 第 6 leg(v2 bridge smoke test;手動,無需 openseespy)
Plugins\FrameSolver\Standalone\build_capi_v2.bat      # build frame_capi_v2.dll
python Tools/v2_roundtrip.py                          # 13 PASS / 1 SKIP

# 4) NOT RUN 此 release(reproduce 命令,需各自前置)
#    L2 UE automation(需 UE 5.7 build):
#      set UE_ENGINE_ROOT=E:\project\UE_5.7
#      Engine\Build\BatchFiles\Build.bat ArchSimEditor Win64 Development -project=...\ArchSim.uproject
#      powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
#    L3 OpenSees(需 openseespy):
#      pip install openseespy>=3.5
#      python Tools/opensees_compare.py
#      python Tools/pdelta_compare.py
#    Mega benchmark(需 openseespy):
#      powershell -ExecutionPolicy Bypass -File benchmarks\opensees_mega\rerun.ps1
```

如果 conda 不在 `%USERPROFILE%\anaconda3\envs\framecore-direct`,設定
`SUPERNODAL_CONDA=<conda-root>\envs\framecore-direct\Library` 再跑 build.bat。

---

## 3. 新的 client-side API(B2 stub level)

### C ABI v2(8 個導出)
規格在 [`Plugins/FrameSolver/Standalone/frame_capi_v2.h`](../Plugins/FrameSolver/Standalone/frame_capi_v2.h):

```c
int  frame_v2_open(const char* opts_json, frame_v2_ctx_t** out);
int  frame_v2_close(frame_v2_ctx_t* ctx);
int  frame_v2_send(frame_v2_ctx_t* ctx, const uint8_t* buf, size_t len);
int  frame_v2_recv(frame_v2_ctx_t* ctx, uint8_t* buf, size_t cap,
                   size_t* out_len, size_t* out_needed, int blocking_ms);
int  frame_v2_cancel_recv(frame_v2_ctx_t* ctx);
int  frame_v2_cancel_request(frame_v2_ctx_t* ctx, const char* req_id);
const char* frame_v2_last_error(frame_v2_ctx_t* ctx);
uint32_t    frame_v2_abi_version(void);            // = 2
const char* frame_v2_build_sha(void);              // git rev-parse --short HEAD at build
const char* frame_v2_engine_version(void);         // = "2.4.0"
```

線協議規格:[`docs/specs/S6b_rhino_bridge_v2.md`](specs/S6b_rhino_bridge_v2.md) §2.x

### C# SDK 入口
```csharp
using FrameCore.Bridge;

await using var session = await FrameSession.OpenSimpleAsync(new BridgeOptions {
    Kind = TransportKind.CApiV2InProcess,
    FrameCapiV2DllPath = @"<path>\frame_capi_v2.dll"
});
// 探詢能力 — 真實可用 6 個:cancel / profile.advanced / profile.simple / session / model.set / solve.linear
if (!session.HasCapability("solve.linear")) throw ...;
var b = new FrameModelBuilder();
// ... 建模 ...
var es = await session.OpenEngineSessionAsync();
var dofCount = await session.SetModelAsync(es, b.Build());
var linear = await session.SolveLinearAsync(es);  // B2 stub 回 _stub=true;B3 wire 後真實求解
```

完整範例:[`Plugins/FrameSolver/Grasshopper/v2/README.md`](../Plugins/FrameSolver/Grasshopper/v2/README.md)

---

## 4. Deferred items(from release notes,搭配 day-1 actions)

> **2026-06-19 follow-up pass landed.** Commits `a859810` / `180c9e8` /
> `3814f58` / `214e99f` close the v2.4 → v2.5 main axis (B3 wire + A-01 UAF +
> B5 factor-reuse + D-03 GH race + D-09 P/Invoke audit + E-10 method table +
> the six docs/.gitignore/build.bat quick wins). Five-leg verdict after these
> commits: L1 standalone ALL PASS (F1-F64), L4 audit failures=0 checks=104,
> L5 cli_roundtrip ALL PASS, L6 v2_roundtrip 30 PASS / 0 FAIL (including
> supernodal mode bit-exact vs v1), LevelSim 115/115. L2 UE + L3 OpenSees
> NOT RUN this cycle (engine source bit-identical to v2.3 since `6be1dac`;
> openseespy not installed). The list below keeps the original status legend
> and marks which entries the follow-up cleared.

對應 RELEASE_v2.4.md 的 Deferred section。每行格式:`audit-id | 一句話 | first action on day 1`。

### B3 — 接引擎 wire(優先;v2.4 → v2.5 的主軸)
- ✅ **B3.1 `HandleSolveLinear` wire** | 已 wire `180c9e8`,bit-exact vs v1 frame_capi.dll on cantilever (rel<1e-11) | `Plugins/FrameSolver/Standalone/v2/ModelBuilder.h` + `Dispatcher.cpp` `HandleSolveLinear` body;`Tools/v2_roundtrip.py` SKIP → PASS。
- ✅ **B3.2 `build_capi_v2.bat` 擴 link FrameCore** | 已擴 `180c9e8`,鏡像 build_capi.bat + conditional supernodal | DLL ~105 KB → ~600 KB(含 FrameCore + SnSolver/SnSession);`/DFRAMECORE_SUPERNODAL=1` (with conda) or =0 (without)。
- ✅ **B3.3 其他 11 個 method wire** | 已 wire `180c9e8` | `solve.pdelta/tension_only/size_opt/corotational/arclength + analysis.modal/buckling + inspect.{disp,reactions,member_forces,shell_forces}` 全 wire,spec-shape 通過 v2_roundtrip;solve.dyn_collapse + analysis.reanalysis_solve 隨 B4/B5 deferred,model.patch schema 待決。
- ✅ **A-04 (B3 tier) — 引擎側 propagation 守 isfinite** | 已實作 `180c9e8` | `Dispatcher.cpp` `finiteOrFail` + `packDisp/packMemberForces/packShellForces/buckling/modal` 全路徑 isfinite check;首個 non-finite 走 `NON_FINITE_RESULT` 錯誤 frame。

### B4 — streaming + binary + per-handler cancel(`solve.dyn_collapse`)
- 🟡 **C-06 / C-07 — dispatcher 並發架構重設計** | DEFERRED to v2.6 | redesign 跟 B4 streaming 綁:per-session worker thread + cv/outbound mutex 分離 + IsCancelled TOCTOU poll + binary framing。當前 dispatcher 是 single-threaded inline,client 看不到差別;dyn_collapse handler 還是 stub,無 driver 需要這個重設計。Day-1:重寫 `Dispatcher.cpp::Submit/Recv` per-session worker model,handler 內每 N 步 poll IsCancelled。
- 🟡 **B4 first-frame** | DEFERRED to v2.6 | 待 C-06/C-07 worker thread 完成才有 streaming 通道。Day-1:`HandleSolveDynCollapse` enqueue 每幀 96 bytes/node binary payload + `FLAG_END_OF_RESPONSE` 收尾;`v2_roundtrip.py` 加 streaming + cancel-mid-stream case。

### B5 — session factor-reuse
- ✅ **B5 wire** | 已 wire `214e99f`,supernodal mode 路徑 bit-exact vs v1 | `EngineSession` 加 `useSnSession` + `std::unique_ptr<frame::SnSession> sn`;`session.open` 讀 `body.mode` 並回應同 mode;`model.set` eagerly factor + build SnSession;`solve.linear` 走 `sn->solveFrame`(LDLT 永遠 fallback)。`resolve` mode 留給 ReSolveSession follow-up(B5.2)。

### B7 — Rhino 8 GHA 實 build
- ✅ **D-09 — P/Invoke signature audit** | 完成 `214e99f` | 7 個 Cdecl delegate 對 `frame_capi_v2.h` prototype 線對線 verify(IntPtr/UIntPtr/byte*/sbyte*/uint/int 寬度全合 x86_64 Windows ABI);audit 結論落在 `CApiV2Transport.cs` 內供未來 header 擴增 re-audit。
- 🟡 **B7 build** | DEFERRED (ENV) | 本 cycle host 無 dotnet SDK / Rhino 8 SDK,無法 dotnet build。Source 自 v2.4 release 起未動;steps 同 RELEASE_v2.4.md (yak install RhinoCommon → dotnet build FrameCore.Gh.csproj -c Release → yak build .)。Day-1: 在 Rhino 8 dev machine 跑這條指令鏈,verify GHA load 進 Grasshopper。
- ✅ **A-01 — `frame_v2_close` race** | 完成 `3814f58` | shared_ptr ownership registry + per-call acquire();close 釋放 owner ref,recv 持自己 ref 直到退出;UAF window 已關。
- ✅ **D-03 — GH OpenFrameCore generation race** | 完成 `214e99f` | `_openGate` lock object 包住 gen-check + field-write;ResetSession.InvalidateCurrentSession 也走同一 gate。

### Docs / 雜項 follow-up(v2.4.1 或下個 docs cycle 處理)
- ✅ **B-12** | 完成 `a859810` | README.md:61 `20× at 61.5k DOF` → `20× at 62k DOF`(對齊 PROGRESS_V21.md perf_sn 數值)。
- ✅ **E-10** | 完成 `214e99f` | docs/specs/S6b_rhino_bridge_v2.md §③ method table 加 ✅/🟡/⚪ 狀態標籤 + 圖示說明。
- 🟡 **F65 / F66** | DEFERRED to v2.5 | F61 已驗證 membrane-warp O(1/N²) 收斂;F65 (warped MITC4 stiffness vs regular) + F66 (warped 旋轉等變) 是 net-new fixture 設計,需 numerically robust 模板才不會 flake。
- ✅ **`build.bat` conditional supernodal skip** | 完成 `a859810` | `build.bat` 偵測 conda OpenBLAS 不存在時走 `:build_sn_off` 分支,`/DFRAMECORE_SUPERNODAL=0`,main.cpp 的 F55/F56/F62/F63 `#if FRAMECORE_SUPERNODAL` 自動 compile out;無 conda 機器也可跑 L1 leg。
- ✅ **H-02 / H-03 / H-04** | 完成 `a859810` | `docs/AGENT_PROMPT_OPENSEES_MEGA_BENCHMARK.md` + `docs/AGENT_PROMPT_S2_S4.md` 字面 `E:\project\ArchSim` / `C:\Users\wmc02\.claude\...` / `E:\project\UE_5.7` → `<repo-root>` / `~/.claude/projects/<project>/memory/` / `%UE_ENGINE_ROOT%`(local hint 保留 inline)。`docs/HANDOFF_rhino_bridge_v2_final.md` 經 grep 確認已無字面路徑(可能 P1/P2 修補時已 templatize)。
- ✅ **H-09 / H-10** | 完成 `a859810` | `docs/learning/generate_framecore_whiteboard_course.py:28-32` 改用 `NOTOSANS_TC_PATH` / `INKFREE_PATH` env var fallback;`.gitignore` 補上 `obj_capi/` + `obj_linear_audit/`。
- 🟡 **F-1..F-10** | DEFERRED | 留給獨立 `code-quality-sweep` cycle,不入本 release。

### LevelSim 子系統(獨立 worktree)
- **D-08** | player-elev propagation doc 未寫 | 在 `E:\project\ArchSim-levelsim\docs\` 新建 `D-08-player-elevation-propagation.md`,描述 PlayerController ↔ LevelSim elevation 同步的 tick-order 與 authority 規則。

---

## 5. 過程中學到的(durable,本 cycle 教訓)

- **第 6 gate leg 不強行併入 run_gate.ps1** — `Tools/v2_roundtrip.py` 需要 `build_capi_v2.bat` 先跑、需要 conda 不一定有的 ctypes 設定;v2.4 選擇「手動 leg + README 明標」而非「自動 leg 但 conda 缺時 fail」。哪天 B3 wire 完成、`v2_roundtrip` 從 stub 升 bit-exact-vs-v1,再評估自動化。
- **Engine source `0 lines changed` 是 release 一個強 signal** — 五腿 gate 全綠不是巧合,是 transport-only release 本來就該如此;若哪天 transport release 順手改了 engine,要立刻警覺是不是真該改。
- **`docs/learning/` 的取捨**:39k 行 courseware 在 commit 前是「待決定要不要進 release」的灰色項;v2.4 release-hardening pass 判斷「**刻意 bundle**」(generate 腳本也進來、檔案結構完整、單獨可讀),所以納入 release narrative 並在 docs/README.md 加索引,而非排除。下次有類似大型素材,沿用「intentional courseware bundle」這個分類概念。
- **release 同日同 commit 多輪改進的時序**:`10b767c` 一個 commit 把(v2 設計 + B2 骨架 + 三輪 P0/P1/P2 修補 + .gitignore patch + courseware + benchmark output 重生)全打包進去,handoff 文件描述「未 commit」的狀態被一次性翻成「已 commit」;**release-hardening pass 必須 spot-check 每份 handoff 與當前 git 是否一致**(本 cycle 抓到 § ⑪ `.gitignore patch 待 apply` 是過時描述就是案例)。
- **OpenSeesPy 在 conda env 的位置不可假設** — v2.2+1 假設「沒裝」放掉 A-06,v2.3 才發現裝在 base anaconda3;v2.4 真的沒裝(`autogenstudio` / `framecore-direct` 都沒)就誠實 NOT RUN,不再硬上 pip install 影響使用者環境。

---

## 6. 一頁式速查

- **想看完整 release 內容** → [`RELEASE_v2.4.md`](RELEASE_v2.4.md)
- **想動工 B3 wire** → § 4 B3.1-B3.3 + `Dispatcher.cpp` 搜 `[TODO B3]`
- **想看 P0/P1/P2 三輪修補對照** → [`HANDOFF_rhino_bridge_v2_final.md`](HANDOFF_rhino_bridge_v2_final.md) § ②
- **想看 80 個 Rhino 元件 catalogue** → [`docs/specs/S6c_rhino_ux_commercial.md`](specs/S6c_rhino_ux_commercial.md) § ②
- **想看 v2 線協議 wire layout** → [`docs/specs/S6b_rhino_bridge_v2.md`](specs/S6b_rhino_bridge_v2.md) § 2.1
- **想跑 release verification** → § 2 quick-reproduce 指令塊
- **想跑 6th gate leg(手動)** → `Plugins\FrameSolver\Standalone\build_capi_v2.bat && python Tools/v2_roundtrip.py`
- **想看課程素材** → [`docs/learning/`](learning/);第 1 課獨立檔 [`framecore_v2_course_lesson1.md`](learning/framecore_v2_course_lesson1.md)
- **想知道下一輪重點** → § 4 B3 list 的前三條(method wire + build.bat 擴 link + isfinite propagation)
