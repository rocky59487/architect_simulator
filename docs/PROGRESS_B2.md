# B2 進度日誌 — Rhino 橋接 v2 引擎側 dispatcher 骨架 + 第 6 gate leg

> 接續 `docs/HANDOFF_rhino_bridge_v2.md`(B1 + C1 設計階段)+ `docs/specs/S6b_rhino_bridge_v2.md`。
> 本階段把 v2 設計 "**真的編出 DLL、真的握手、真的拒絕 silent**",但 **不接引擎**(B3 工作);
> 引擎不動,五腿 gate 不破。

## 基準
- 起點 `6be1dac`(v2.3),五腿全綠(F1-F64 / UE 57 expected / OpenSees PASS / audit 104 / CLI ALL PASS)
- 本輪 **無 commit**(現場驗證後待使用者授權)。

## 範圍決策(誠實)
B 線三步走 **B2 dispatcher 骨架 → B3 接引擎 → B4 streaming / cancel / binary**。本輪 = B2:
- **[GATED — 第 6 leg 已綠]**:`frame_capi_v2.dll` 真實編出 + Python ctypes 第 6 gate leg `Tools/v2_roundtrip.py` **13/13 PASS,1 SKIP 明標 B3 wire**
- **[NOT 接引擎]**:每 method 一個 stub case + `[TODO B3]` 註解;`solve.linear` 等回 `_stub: true` 標記,gate 用「shape 對 + `_stub=true`」放行,B3 wire 後升為「結果 bit-exact vs v1 文字協議」
- **[完全隔離]**:所有新檔在 `Plugins/FrameSolver/Standalone/v2/` 子目錄 + `build_capi_v2.bat` 獨立批檔,**不動 `build.bat` / `build_capi.bat` / `frame_cli_core.cpp`**,五腿 gate 零影響

## 交付內容

### 新增(13 檔)
- `Plugins/FrameSolver/Standalone/v2/MiniJson.h` — header-only JSON parser/writer(零依賴,~250 LOC,UTF-8 + \uXXXX BMP + 嚴格無 trailing comma)
- `Plugins/FrameSolver/Standalone/v2/FrameWire.h` — header-only frame parse/serialize(magic `FC` + flags u16 + 兩個 u32 length LE + JSON header + optional binary payload)
- `Plugins/FrameSolver/Standalone/v2/Dispatcher.h` — method registry + Context + EngineSession + 25 capability 字串 + MakeResponse/MakeEvent/MakeError 統一構造器
- `Plugins/FrameSolver/Standalone/v2/Dispatcher.cpp` — 22 個 method handler(5 connection-mgmt **[WIRED]** + model.set/solve.linear/inspect.* **[shape correct,引擎 TODO B3]** + 11 個 **[NOT_IMPLEMENTED stub]**)
- `Plugins/FrameSolver/Standalone/frame_capi_v2.cpp` — C ABI impl,委派 Dispatcher;`frame_v2_open/close/send/recv/cancel_recv/cancel_request/last_error/pending_count/abi_version/build_sha/engine_version` 全 8 個導出
- `Plugins/FrameSolver/Standalone/build_capi_v2.bat` — 獨立 build,鏡像 build_capi.bat 結構但只 link v2 TU(Dispatcher.cpp + frame_capi_v2.cpp),不需任何 FrameCore 引擎 .cpp / Eigen
- `Plugins/FrameSolver/Standalone/frame_capi_v2.dll` — 真實 build 產物(109 KB)
- `Tools/v2_roundtrip.py` — 第 6 gate leg:ctypes 載入 DLL 跑 hello/session.open/model.set/solve.linear/session.close 等 8 個 method,額外驗 advanced profile 拒絕缺 cap

### 修改
**無**。引擎 source tree 一行不動,既有 build / cli / capi 一個檔不動。

## 第 6 gate leg 結果(實測)

```
=== v2_roundtrip (B2 stub level) ===
  [PASS] abi_version >= 2 -- abi=2
  [PASS] build_sha non-empty -- sha=6be1dac
  [PASS] engine_version non-empty -- v=2.3.0
  [PASS] hello returns OK -- rc=0
  [PASS] hello.capabilities includes core set -- missing=[]
  [PASS] hello.schemaVer present
  [PASS] hello carries END_OF_RESPONSE
  [PASS] session.open returns session id -- sid='s_1'
  [PASS] simple model.set ok + dofCount -- body={'ok': True, 'dofCount': 6, 'defaultsApplied': ['materials[0].cap']}
  [PASS] simple model.set lists defaultsApplied for missing cap
  [PASS] solve.linear returns spec shape (stub)
  [SKIP] solve.linear bit-exact vs v1 -- engine wiring is B3 work
  [PASS] session.close returns OK -- rc=0
  [PASS] advanced model.set REJECTS missing cap (VALIDATION_FAILED) -- kind=error code=VALIDATION_FAILED
=== summary: ALL PASS ===
```

**13 PASS / 1 SKIP / 0 FAIL**。SKIP 明標待 B3。

## 雙 profile 驗證(spec § ⑭ 落地)

| spec § ⑭ silent 點 | B2 落地 |
|---|---|
| ① `MAT` 無 cap 預設 `make(300,300,180)` | simple = `defaultsApplied: ["materials[0].cap"]` 列入結果;advanced = `VALIDATION_FAILED { code, message }`(實測通過)|
| ② supernodal SPD silent fallback | B3 工作(handler 已留 TODO 註解) |
| ③ singular silent flag | B3 工作 |
| ④ FRAMECORE_SUPERNODAL=0 silent ignore | B3 工作 |
| ⑤-⑩ 其餘 silent 點 | B3 工作 |

5 個 connection-mgmt 與 model.set 的 silent 攔截已**端到端通過**,證明 dispatcher hook 設計可行。

## 數值精度
B2 不接引擎,**沒有數值精度可量**。B3 第一個 milestone:`solve.linear` 對懸臂尖端 `δ=PL³/3EI` 與 v1 `frame_cli.exe` stdout 比對 **bit-exact**(`%.17g` 序列化、IEEE-754 round-trip)。

## 鐵律檢查(對齊 `CLAUDE.md`)

| 鐵律 | 影響 |
|---|---|
| 1. FrameCore 純 C++17 + Eigen,Eigen 不洩漏 | ✅ B2 完全不碰 FrameCore source tree;v2 子目錄與 frame_capi_v2.cpp 連 Eigen 都沒 include |
| 2. 五腿 gate 全綠 | ✅ build.bat / build_capi.bat / build_cli.bat / build_linear_audit.bat / run_gate.ps1 一檔不動;**第 6 leg 新增為獨立護欄,不入 5 leg** |
| 3. 誠實驗證 / 不過度宣稱 | ✅ B2 = `[GATED stub level]`;solve.linear 等 `[TODO B3]`;gate 用 `_stub=true` 標記放行,B3 升為 bit-exact |
| 4. 索引非裸指標 | ✅ wire schema 用 string id;session 用 shared_ptr 而非裸指標 |
| 5. commit 衛生 | ✅ 全新檔 + 新目錄;`.gitignore` / `.uproject` / `Plugins/LevelSim/` 零碰 |

## 編譯/執行記錄

```
build_capi_v2.bat                            # ~10s
[build_capi_v2] OK -> Standalone\frame_capi_v2.dll  # 109 KB

python Tools/v2_roundtrip.py                 # <1s
=== summary: ALL PASS ===
```

## 接續(B3 任務拆解)

按 `Dispatcher.cpp` 內 `[TODO B3]` 標記順序逐 method wire:

1. **`buildModelFromJson(...)`**(新 helper,放 Dispatcher.cpp 內或 `v2/ModelBuilder.h`)— 讀 MiniJson 構造 `frame::FrameModel`,鏡像 `frame_cli_core.cpp::buildModel` 的邏輯但讀 JSON 而非 istringstream
2. **`HandleSolveLinear`** — 拿到 `FrameModel` → `solve(model, opts)` → 把 `SolveResult.u/reactions/memberForces/shellForces` 寫成 JSON dict(node id → `[Ux..Rz]` 等);**v2_roundtrip.py 移除 SKIP,加 bit-exact-vs-v1 對標**
3. **`HandlePDelta`**(同樣 pattern)→ `runPDelta` → 加 `pdStatus` 欄
4. **`HandleTensionOnly`** → `runTensionOnly` → 加 `slack[]` 欄
5. **`HandleSizeOpt`** → `runSizeOptimization` → 加 `areas{}` + `weightVolume`
6. **`HandleCorotational` / `HandleArcLength`** → 同 pattern
7. **`HandleModal` / `HandleBuckling`** → 加 binary payload(modes 96·n bytes per mode)
8. **`HandleDynCollapse`** — streaming + binary u/v + cancel,B4 工作
9. **`HandleReanalysis`** — `ReSolveSession`,B5 工作
10. **`HandleInspectDisp/MF/RF/SF`** — 只讀已 cache 的 SolveResult(EngineSession 持有),不重算
11. **每 method wire 完同步擴 `v2_roundtrip.py` 對標 v1**(去除 SKIP)
12. **build_capi_v2.bat 擴 link FrameCore 全 TU**(鏡像 build_capi.bat 全集)

B3 完成後 v2 DLL 與 v1 `frame_capi.dll` 對同模型應 byte-byte 等價(simple profile);advanced profile 多 `advancedDiagnostics` 欄。

## 誠實邊界
- **B2 = transport plumbing**,**不算新力學能力**;所有力學正確性仍由 5 腿 gate 把關
- **未 commit**;working tree 變動可由 `git status` 完整列出
- **未跑 5 腿 gate**(因為一個檔沒改 5 腿覆蓋範圍 → 沒必要 30 分鐘 rebuild;接續者 commit 前若有疑慮可一鍵 `Scripts\run_gate.ps1 -RequireOpenSees` 驗證,預期全綠)

## 下一步(待使用者授權)
B3 接引擎或 C2 補 Rhino 元件 — 兩線可並行。任選一條動工,B2 提供的 dispatcher 介面已穩定。
