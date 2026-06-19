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

對應 RELEASE_v2.4.md 的 Deferred section。每行格式:`audit-id | 一句話 | first action on day 1`。

### B3 — 接引擎 wire(優先;v2.4 → v2.5 的主軸)
- **B3.1 `HandleSolveLinear` wire** | 引擎沒接,client 拿 `_stub:true` | 開 `Plugins/FrameSolver/Standalone/v2/Dispatcher.cpp`,搜 `[TODO B3]` 在 `HandleSolveLinear` 區塊;新建 `v2/ModelBuilder.h` helper(`buildModelFromJson(json) -> FrameModel`,鏡像 `frame_cli_core.cpp::buildModel`),呼叫 `frame::solve(model, opts)`,把 `SolveResult` 序列化回 JSON;`Tools/v2_roundtrip.py` 把 SKIP `solve.linear bit-exact vs v1` 改為 PASS,oracle = `frame_cli.exe` 同模型輸出的 `DISP` 行 bit-exact 比對。
- **B3.2 `build_capi_v2.bat` 擴 link FrameCore** | 目前 transport-only 不 link 引擎 | 在 `build_capi_v2.bat` cl 命令的 `Standalone\v2\Dispatcher.cpp Standalone\frame_capi_v2.cpp` 後增列 FrameCore 全 TU(鏡像 `build_capi.bat` 的源檔清單),加 `/DFRAMECORE_SUPERNODAL=1` 與 conda OpenBLAS+METIS link;預期增加 ~30 s build 時間。
- **B3.3 其他 11 個 method wire** | `solve.pdelta/tension_only/size_opt/corotational/arclength/analysis.modal/analysis.buckling/inspect.*` 仍 NOT_IMPLEMENTED | 按 `[TODO B3]` 順序每 method 補 30-50 LOC 的 dispatcher.cpp handler + JSON 序列化;每寫完一個就在 `v2_roundtrip.py` 加一個 bit-exact-vs-v1 check。
- **A-04 (B3 tier) — 引擎側 propagation 守 isfinite** | MiniJson 已守住 input,B3 wire 後 output 也得守 | 在 B3.1 的 `SolveResult → JSON` 序列化路徑,對每個 `disp[i]` / `member_force[i]` 序列化前加 `if (!std::isfinite(v))` reject + `code: NON_FINITE_RESULT`。

### B4 — streaming + binary + per-handler cancel(`solve.dyn_collapse`)
- **C-06 / C-07 — dispatcher 並發架構重設計** | cv/outbound mutex 分離 + IsCancelled TOCTOU | B4 開工先重寫 `Dispatcher.cpp::Submit/Recv` 並發模型:每 session 一個專用 worker thread,`Submit` 入 queue 不等執行,worker pop 後執行 + signal recv;`HandleSolveDynCollapse` 內部每 N 步 poll `ctx_.IsCancelled(reqId)`;binary payload 走 `FLAG_HAS_PAYLOAD | FLAG_BINARY_PAYLOAD` 分幀。
- **B4 first-frame** | `solve.dyn_collapse` 仍 NOT_IMPLEMENTED stub | 在 `HandleSolveDynCollapse`(B4 主入口)裡 enqueue 每幀 96 bytes/node `(x,y,z,vx,vy,vz)` 的 binary payload,header JSON 帶 `kind: "dyn_collapse.frame"` + `t`;最後一幀 `FLAG_END_OF_RESPONSE`;`v2_roundtrip.py` 加 streaming case + cancel-mid-stream case。

### B5 — session factor-reuse
- **B5 wire** | mode=`resolve` / `supernodal` 未接 | 在 `Dispatcher.cpp::EngineSession` struct 加 `std::optional<frame::SnSession>` + `std::optional<frame::ReSolveSession>`;`session.open` 若帶 `mode: "supernodal"` 則 build SnSession,後續 `solve.linear` 直接走 `snSession->solveFrame(...)`(factor-once);`v2_roundtrip.py` 加 「同模型連 100 次 solve.linear 比第 1 次快 X 倍」 latency check。

### B7 — Rhino 8 GHA 實 build
- **D-09 — P/Invoke signature audit** | C# 8 delegate 簽名沒線對線 verify against `frame_capi_v2.h` | 開 `Plugins/FrameSolver/Grasshopper/v2/Bridge/CApiV2Transport.cs:179-205`(8 個 delegate)與 `frame_capi_v2.h:122-170` 並列 diff,每 delegate 對應 prototype 比對 calling convention(全 `Cdecl`)、parameter width(`UIntPtr`↔`size_t`、`IntPtr`↔`void*`、`int`↔`int`)、`[MarshalAs]` (`LPUTF8Str`?);發現不符立即修。**這是 GHA 在 release-mode 跑會不會 silent corrupt stack 的關鍵 check**。
- **B7 build** | net7.0 GHA 真 build 沒做 | 安裝 Rhino 8 SDK(`yak install RhinoCommon` 等),`cd Plugins/FrameSolver/Grasshopper/v2/Rhino && dotnet build FrameCore.Gh.csproj -c Release`;輸出 `bin\Release\net7.0\FrameCore.Gh.gha`;`yak build .` 打包 Yak。需要 Rhino 環境,不入引擎 CI。
- **A-01 — `frame_v2_close` race** | close `delete`s ctx while recv 可能在 cv.wait | 改 `frame_v2_open` 把 ctx 用 `std::shared_ptr` 持有,`frame_v2_close` 設 closed=true + wake recv,recv 退出時 shared_ptr 自動 dealloc;**B3 wire 後若無 dispose 順序,Rhino 端 dispose FrameSession 進 cv.wait 同步 close 會 UAF**。
- **D-03 — GH OpenFrameCore generation race** | 二段 check 中間有狹窄寫入窗 | 在 `OpenFrameCoreComponent.cs:144-159` 把 gen-check + field-write 合進一個 `lock (_openGate)` 區塊,或改用 `Interlocked.CompareExchange(ref _openGeneration, thisGen, thisGen)` 比對成功才寫 `_session`。

### Docs / 雜項 follow-up(v2.4.1 或下個 docs cycle 處理)
- **B-12** | README `61.5k` vs `62k` DOF 不一致 | 開 `README.md:57`,把 `61.5k DOF` 改為 `62k DOF`(對齊 `PROGRESS_R_supernodal.md` 的 perf_sn 數值)。
- **E-10** | S6b method table 沒 `[B3]`/`[B4]`/`[B5]` 標籤 | 等 B3 真開工後再開 `docs/specs/S6b_rhino_bridge_v2.md` §③ method table 加標籤,因為標籤要對應實作狀態。
- **F65 / F66** | warped-shell standalone fixtures | 在 `Plugins/FrameSolver/Standalone/frametest.cpp` 尾部加 F65 / F66,F65 = warped MITC4 stiffness vs regular,F66 = warped 旋轉等變;加 `build.bat` 源檔清單。
- **`build.bat` conditional supernodal skip** | 無 conda 機器無法跑 | `build.bat` 偵測 `%CONDA_SS%\include\openblas\cblas.h` 不存在時 `goto :no_supernodal` + `/DFC_NO_SUPERNODAL=1`,跳過 F55-F64 supernodal 相關 fixtures;標 `[SKIP]` 不算 fail。
- **H-02 / H-03 / H-04** | HANDOFF + AGENT_PROMPT 內 `E:\project\ArchSim` / `C:\Users\<user>\.claude\...` 字面 | 開 `docs/HANDOFF_rhino_bridge_v2_final.md:255,285` / `docs/AGENT_PROMPT_OPENSEES_MEGA_BENCHMARK.md:11,39` / `docs/AGENT_PROMPT_S2_S4.md:28`,把字面路徑改為 `<repo-root>` 或 `~/.claude/projects/<project>/memory/` 範本。
- **H-09 / H-10** | font path 硬寫 + gitignore 漏 `obj_capi/` | `docs/learning/generate_framecore_whiteboard_course.py:29-30` 把 `C:\Windows\Fonts\NotoSansTC-VF.ttf` 改 env var fallback;`.gitignore` 加 `obj_capi/` + `obj_linear_audit/`(可選,目前 `*.obj` 已涵蓋無 risk)。
- **F-1..F-10** | C++ / C# 小型 perf / 風格 cleanups | 改在獨立 `code-quality-sweep` cycle,不入本 release。

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
