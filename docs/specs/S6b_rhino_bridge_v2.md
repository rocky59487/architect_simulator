# S6b — Rhino 橋接協議 v2(長期穩定、可演進)

> **狀態**:設計階段(2026-06-19)。S6 (J1/J1b/J1.5/J2) 已於 v2.0 GA 完成並進 gate;
> 本 spec 為 v2 重設計,目標「**一次做好,之後 N 個 minor/major 版本不用大改**」。
> 既有 J1 文字協議與 J2 `frame_capi.dll` **永遠保留**(舊客戶端零變更),v2 走獨立
> handle (`frame_capi_v2.dll`/`frame_capi.dll@v2`) + 全新訊息協議,雙軌並行。

## ⓪ 為什麼要 v2(現有橋接無法滿足的長期需求)

| 痛點 | J1/J2 現況 | v2 設計回應 |
|---|---|---|
| 新分析每次都要加 token + 解析 | 字串 token,解析散落 client | 統一 RPC schema,新方法只擴 method 名,zero-cost forward-compat |
| 版本/能力協商 | 只有 `VERSION <sha>` 第一行 | `HELLO` handshake → `CAPABILITIES` 集合(method list, feature flags, build SHA, schema ver) |
| 二進位資料(完整 DYNC u/v 幀、模態形狀、屈曲模態、碎塊 cluster) | 不可能塞進文字 | length-prefixed framed message:header (JSON/Bincode) + optional binary blob payload (raw double array) |
| 跨請求 session 重用(factor-once + solve-many,GH 滑桿一動就重跑) | daemon 多塊但每塊 fresh factor | `session/open` 回 handle → `session/solve_load` 重用 PreparedSystem/SnSession/ReSolveSession |
| 部分結果 / streaming(DYNC 邊跑邊推幀給 UE 回放) | 全跑完才回 | `request` + `progress` events + `result` 多訊息序列;client 用 reqId 收集 |
| 取消(GH 動畫面要中斷 long DYNC/Arclen) | 無 | `cancel(reqId)` 訊息 + 引擎 polling cancel token |
| 浮點精度(double round-trip + `%.12g` 損失) | 文字 12 位有效位數 | binary mode 走 bincode raw doubles,bit-exact |
| 結果欄位讀取(GH 元件常只要一條桿一條欄位) | client 解析完整 stdout | 結果 message 用 schema 定義的欄位,client 按需 deserialize |
| 文字 stdin/stdout 大模型慢(20k DOF 2.1s, 100k+ 不可行) | I/O 與字串轉換是瓶頸 | binary 路徑 + in-process C ABI v2 + memory-mapped 大結果(M3 future) |
| 錯誤分級 | 散落 `ERR/FREQERR/DYNERR` tag | 統一 `error { code, severity, message, context }` 結構,vocabulary 固定 |
| 取消式單一 transport 鎖死 | 只有 stdin/stdout | transport 抽象:`stdio` / `C ABI` / `named pipe` / `TCP`(雲端) — 同 schema |

**v2 不做的事**(誠實邊界,**避免過度設計**):
- 不重寫力學引擎 — v2 是純 transport + schema 重設計,引擎核心一行不動。
- 不挑戰 J1 既有 5-leg gate — J1 保留,v2 並行,**新增第 6 leg gate** 驗 v2。
- 不引入第三方 RPC 框架(gRPC / FlatBuffers / Cap'n Proto)— 引擎 C++17 only + 純 Eigen 鐵律不破;
  schema 用**手寫 framed JSON header + raw double payload**(可預測、可審核、零依賴)。
- 不解決 Rhino .gha 編譯 — 仍需 Rhino 8 .NET SDK,C# 客戶端骨架可獨立編譯驗。

---

## ① 三層架構

```
   ┌──────────────────────────────────────────────────────────┐
   │  Layer 3: 客戶端 SDK                                      │
   │    C# (Rhino/GH) — FrameCore.Bridge.dll                  │
   │    Python (Tools/) — frame_bridge.py                     │
   │    強型別 Model / Result / Session,async + cancel        │
   └─────────────────────────┬────────────────────────────────┘
                             │  Protocol Messages (this spec)
   ┌─────────────────────────┴────────────────────────────────┐
   │  Layer 2: Transport (interchangeable)                    │
   │    A. C ABI v2 — frame_capi_v2.dll (in-process)          │
   │    B. stdio    — frame_cli_v2.exe (subprocess)           │
   │    C. named pipe — frame_daemon (long-running, future)   │
   │    D. TCP     — cloud / split-machine (future)           │
   │  全部跑同一 schema(同一份 dispatcher 程式碼)             │
   └─────────────────────────┬────────────────────────────────┘
                             │  C++ engine API (Eigen-free POD)
   ┌─────────────────────────┴────────────────────────────────┐
   │  Layer 1: FrameCore Engine (unchanged — 鐵律)            │
   │    FrameSolver / Reanalysis / SnSession / DynamicCollapse│
   │    Eigen 只在 Private/*.cpp,公開 API 全 POD             │
   └──────────────────────────────────────────────────────────┘
```

關鍵:**Layer 2 的四個 transport 共用 Layer 1 上方的單一 dispatcher 函式** 
`std::string dispatchMessage(SessionRegistry&, std::span<const std::byte> frame)`,
所以新方法**只加一次**就在 stdio / DLL / pipe / TCP 全部生效。

---

## ② 線協議(framed message)

### 2.1 訊息框架(framing)

每個訊息是一個 **frame**,結構:

```
+--------+--------+----------------+------------------+
| MAGIC  | FLAGS  | HEADER_LEN(4)  | PAYLOAD_LEN(4)   |   header
+--------+--------+----------------+------------------+
| HEADER_BYTES (JSON, UTF-8, no NUL terminator)       |   variable
+-----------------------------------------------------+
| PAYLOAD_BYTES (binary blob, often raw doubles)      |   optional
+-----------------------------------------------------+
```

| 欄位 | 大小 | 內容 |
|---|---|---|
| MAGIC | 2 bytes | `FC` (`0x46 0x43`) — frame anchor |
| FLAGS | 2 bytes | bit0 = `END_OF_RESPONSE` (server 推完此 reqId 的最後一塊);bit1 = `HAS_PAYLOAD`;bit2 = `IS_BINARY_PAYLOAD`(0=base64 in header,1=raw bytes);保留 13 bits |
| HEADER_LEN | 4 bytes LE u32 | header JSON 位元組數 |
| PAYLOAD_LEN | 4 bytes LE u32 | payload 位元組數(0 if no payload) |
| HEADER_BYTES | HEADER_LEN | JSON 物件 |
| PAYLOAD_BYTES | PAYLOAD_LEN | 二進位 blob(通常 raw `double` little-endian, IEEE 754) |

**為什麼這個 framing(設計理由)**:
- MAGIC + length-prefix 讓串流可重新同步,壞 frame 不會卡死;
- header 用 JSON 而非 bincode,**讓 v2 自我描述、可審計、可手寫 debug**(犧牲一點空間換可讀性,header 通常 < 1KB);
- payload 與 header 分離,**header schema 演進(加欄位)不影響 payload bit-exactness**;
- LE u32 為 length,跨平台統一,避開 endian 麻煩;
- 32-bit length 上限 4 GiB,單一 frame 夠用;大結果分多 frame 串。

### 2.2 Header schema(JSON)

每個 header 必含:

```json
{
  "v": 2,             // protocol version (固定 2,大改才 bump)
  "kind": "request" | "response" | "event" | "error" | "hello" | "bye",
  "id":   "<uuid-or-int>",   // request id (回應沿用)
  "method": "session.open",  // 僅 request/response 用
  "seq":  0,                 // event/response 多訊息時的序號 (0..n)
  "body": { ... }            // method-specific payload (schema 見 §3)
}
```

未知欄位 client/server **必須忽略**(forward-compat 鐵律)。

### 2.3 Handshake — `hello`

連線(或 C ABI session open)**首訊息必為 `hello`**,雙向交換:

```json
// client → server
{ "v":2, "kind":"hello", "id":"hs", "body": {
    "client": "FrameCore.Bridge.Rhino 1.2.0",
    "preferredSchemas": ["2026.06"],
    "wantsBinary": true
}}

// server → client
// NOTE: this 16-capability list is the *full* v2 roadmap. B2 (v2.4) ships a tighter
//       subset (see Dispatcher.h::Capabilities() = 6 actually wired); B3-B5 unlock the
//       rest. Clients MUST use the engine's actual response, not this template.
{ "v":2, "kind":"hello", "id":"hs", "body": {
    "engine": "FrameCore",
    "buildSha": "10b767c",
    "version":  "2.4.0",
    "schemaVer": "2026.06",
    "capabilities": [
        "session", "session.factor_reuse",
        "solve.linear", "solve.pdelta", "solve.tension_only",
        "solve.size_opt", "solve.dyn_collapse",
        "solve.corotational", "solve.arclength",
        "modal", "buckling",
        "supernodal", "reanalysis",
        "binary_payload", "streaming", "cancel"
    ],
    "limits": { "maxNodes": 5000000, "maxDof": 30000000 },
    "deprecated": []
}}
```

**capability 規則(長期穩定的關鍵)**:
- 客戶端**永遠先查 capabilities 再下指令**;
- 引擎新增能力 → 加一個 capability 字串,**不改舊有 method 簽名**;
- 引擎廢能力 → 加入 `deprecated`,**保留 method 數 N 個版本**(承諾至少 2 minor);
- 客戶端遇到伺服器沒宣告的能力 → 自己 fallback 或讓使用者知道(`ERR/UNSUPPORTED_METHOD`)。

---

## ③ Method 目錄(method catalogue, schema 版本 `2026.06`)

| Method | 語意 | 對應 capability |
|---|---|---|
| `session.open` | 開新 session(可選擇 factor-reuse 模式) | `session` |
| `session.close` | 關閉 session,釋資源 | `session` |
| `session.status` | 查 session 狀態 / 引擎旗標 | `session` |
| `model.set` | 寫入完整模型(取代舊模型) | always |
| `model.patch` | 部分更新(僅 NLOAD/UDL/SPRESS/prescribed/active 翻轉)→ 重用 factor | `session.factor_reuse` |
| `solve.linear` | 線性靜態 | always |
| `solve.pdelta` | P-Delta | always |
| `solve.tension_only` | 主動集 tension-only | `solve.tension_only` |
| `solve.size_opt` | FSD 尺寸優化 | `solve.size_opt` |
| `solve.dyn_collapse` | 連續動力倒塌(可選 stream 幀) | `solve.dyn_collapse` |
| `solve.corotational` | co-rotational 大位移 | `solve.corotational` |
| `solve.arclength` | arc-length snap-through | `solve.arclength` |
| `analysis.modal` | 模態頻率 + 模態形狀(binary payload) | `modal` |
| `analysis.buckling` | 線性屈曲(eigenvalue + buckling mode shape) | `buckling` |
| `analysis.reanalysis_solve` | ReSolve(增量 active toggle 後重算) | `reanalysis` |
| `inspect.disp` | 部分讀:取指定 node ids 的位移 | always |
| `inspect.member_forces` | 部分讀:取指定 member ids 的端力 | always |
| `inspect.shell_forces` | 部分讀:取指定 shell ids 的合力 | always |
| `cancel` | 取消正在執行的 reqId | `cancel` |

`event` kind 用於 server push:
- `progress` — `{ pct: 0.42, stage: "factor", note: "..." }`
- `dyn_collapse.event` — 每個拓撲事件(對應舊 `DEVENT`)
- `dyn_collapse.frame` — 每一儲存幀(對應舊 `DFRAME`,**或** 走 binary payload 推完整 u/v)
- `log` — `{ level: "info"|"warn", message: "..." }`

`error` kind 統一:

```json
{ "v":2, "kind":"error", "id":"<reqId>", "body": {
    "code": "VALIDATION_FAILED" | "UNSUPPORTED_METHOD" | "SINGULAR_SYSTEM" |
            "CANCELLED" | "INTERNAL" | "PROTOCOL" | "OUT_OF_MEMORY",
    "severity": "fatal" | "request" | "warning",
    "message": "...",
    "context": { ... }  // method-specific (e.g., why string from validate())
}}
```

---

## ④ 關鍵 method 範例(每個的 request 與 response)

### 4.1 `session.open`

```json
// req
{ "v":2, "kind":"request", "id":"r1", "method":"session.open",
  "body": {
    "mode": "default" | "supernodal" | "resolve",
    "options": {
        "pivotTol": 1.0e-12,
        "enableReleases": false,
        "useTimoshenko": false,
        "useIncompatibleMembrane": false,
        "useDKQPlate": false,
        "shellGeometricStiffness": false,
        "useWarpingCorrection": false,
        "warpTolerance": 1.0e-6,
        "shellCurvatureMaxAngleDeg": 0.0,
        "useSupernodalPrimary": false
    },
    "snSession": {              // optional, only if mode=supernodal
        "irSteps": 0,
        "amalgMaxCol": 64,
        "numThreads": 0
    }
}}

// rsp
{ "v":2, "kind":"response", "id":"r1",
  "body": { "session": "s_3f2a", "ready": true }}
```

### 4.2 `model.set`

```json
{ "v":2, "kind":"request", "id":"r2", "method":"model.set",
  "body": {
    "session": "s_3f2a",
    "materials": [
        {"E": 210000, "G": 80769, "rho": 7.85e-9,
         "nu": 0.3,
         "cap": {"comp":300, "tens":300, "shear":180}}
    ],
    "sections": [
        {"A":10000, "Iy":8.33e6, "Iz":8.33e6, "J":1.41e7,
         "cy":50, "cz":50, "Asy":8333, "Asz":8333}
    ],
    "nodes": [
        {"id":0, "x":0, "y":0, "z":0, "fixed":[1,1,1,1,1,1]},
        {"id":1, "x":2000, "y":0, "z":0, "fixed":[0,0,0,0,0,0]}
    ],
    "members": [
        {"id":0, "i":0, "j":1, "mat":0, "sec":0,
         "refVec":[0,0,1], "active":true, "tensionOnly":false,
         "release":[false,false,false,false,false,false,
                    false,false,false,false,false,false]}
    ],
    "shells": [],
    "nodalLoads": [
        {"node":1, "comp":[0,0,1000,0,0,0]}
    ],
    "memberUdls": [],
    "shellPressures": [],
    "hinges": []
}}
```

回 `{ "ok": true, "dofCount": 12 }` 或 `error { code: VALIDATION_FAILED, context: {why: "..."}}`.

### 4.3 `solve.linear`

```json
// req
{ "v":2, "kind":"request", "id":"r3", "method":"solve.linear",
  "body": { "session":"s_3f2a", "binaryDisp":true, "wantReactions":true }}

// rsp(可能拆兩個 frame:header 訊息 + binary payload frame)
{ "v":2, "kind":"response", "id":"r3", "seq":0, "body": {
    "singular": false,
    "pivotMargin": 4.3e-7,
    "n":         2,            // nodes
    "nMembers":  1,
    "nShells":   0,
    "payloadLayout": {
        "u":          { "offset": 0,   "len": 96  },  // 2 nodes * 6 dofs * 8 bytes
        "reactions":  { "offset": 96,  "len": 96  },
        "memberForces":{"offset": 192, "len": 96  }   // 1 mem * 12 doubles * 8
    }
}}
// followed by binary frame:
//  HEADER { id:"r3", seq:1, kind:"response", body:{ binary:true } }
//  PAYLOAD: 288 bytes raw doubles in the layout above
```

### 4.4 `inspect.disp`(部分讀,GH 元件最常用)

```json
// req
{ "v":2, "kind":"request", "id":"r4", "method":"inspect.disp",
  "body": { "session":"s_3f2a", "nodes":[1,5,10] }}

// rsp
{ "v":2, "kind":"response", "id":"r4",
  "body": { "disp": {
      "1": [1.52381, 0,0,0,0,0],
      "5": [...], "10": [...]
  }}}
```

只取要的 3 個節點,**省字串解析時間,適合 GH 滑桿動畫面**。

### 4.5 `solve.dyn_collapse` 串流

```json
// req
{ "v":2, "kind":"request", "id":"r5", "method":"solve.dyn_collapse",
  "body": {
    "session":"s_3f2a",
    "dt": 1e-3, "maxTime": 0.5,
    "initialRemovals": [12, 34],
    "streamFrames": true,         // server push every stored frame
    "binaryFrames":  true         // u/v as raw double payload
}}

// server → client (多訊息):
// event: progress {pct:0.05, stage:"factor"}
// event: dyn_collapse.event {t:0.01, mode:"Brittle", removed:[12,34], detached:[...]}
// event: dyn_collapse.frame {t:0.012, maxAbsU:0.4, idx:0}  +  binary payload (96 bytes/node * n)
// ...
// response: { outcome:"Collapsed", nEvents:3, nFrames:43, tEnd:0.5, diagnostic:"" }  END_OF_RESPONSE flag set
```

客戶端按 `id="r5"` 收集所有 events,看到 `END_OF_RESPONSE` 收尾。

### 4.6 `cancel`

```json
{ "v":2, "kind":"request", "id":"c1", "method":"cancel",
  "body": { "targetId": "r5" }}
```

引擎中迴圈每 N 步檢查 cancel token,命中即丟 `error { code:CANCELLED }` 給原 reqId 並 `END_OF_RESPONSE`。

---

## ⑤ Forward-compatibility 規則(承諾)

下列規則保證 v2 在後續 minor 版本不破舊客戶端:

| 規則 | 引擎不可做 | 引擎可做 |
|---|---|---|
| Method 加欄位 | 變更已 release 欄位語意 / 改 type | 新增 optional 欄位(client 忽略未知) |
| Method 刪 | 直接刪 | 標 `deprecated`,保留 ≥ 2 minor 版本 |
| 新 method | — | 任意新增,client 用 capability 探詢 |
| Token / enum 字串 | 改既有 string 值 | 新增 value(client 收到未知 enum → 視為「其它」+ raw 保留) |
| 二進位 layout | 改既有欄位順序 / type | 新增 payload region(`payloadLayout` 多 key) |
| Schema 版號 `schemaVer` | 不 bump 卻偷改 | bump `schemaVer` minor,允許 minor 並存 |
| Protocol `v` | 任意 bump | 大改才 bump 到 3;v=2 必須在 v=3 server 仍可服務 1 ~ 2 個 minor |

**SLA(對 GH 元件作者的承諾)**:
- v2 GA 後,**任何 v2.minor.patch 升級不需要重編 .gha**;
- major bump(v3)時,提供 v2 並行端點至少 6 個月。

---

## ⑥ Layer 2 transport — C ABI v2(`frame_capi_v2.h`)

完整 header 見 `Plugins/FrameSolver/Standalone/frame_capi_v2.h`。要點:

```c
// 不透明 session handle(框架用,不是引擎 session)
typedef struct frame_v2_ctx frame_v2_ctx;

// 開/關 context(可同時多個,每個 context 自己一條 hello/session pipeline)
FCAPI frame_v2_ctx* frame_v2_open(void);
FCAPI void          frame_v2_close(frame_v2_ctx*);

// 送一個 frame(client → engine)
// inFrame:  pointer to raw frame bytes (magic+flags+lens+header+payload)
// inLen:    bytes
// 回 0 = 收下、非 0 = 協議錯
FCAPI int frame_v2_send(frame_v2_ctx*, const uint8_t* inFrame, size_t inLen);

// 收一個 frame(engine → client),阻塞 / non-block 由 flags 決
// outBuf 由 client 配置;若太小,*outNeeded 寫回所需大小,函式回 FRAME_V2_NEED_BIGGER
// 回 FRAME_V2_OK / FRAME_V2_EMPTY / FRAME_V2_NEED_BIGGER / FRAME_V2_PROTOCOL_ERROR
FCAPI int frame_v2_recv(frame_v2_ctx*, uint8_t* outBuf, size_t outCap,
                        size_t* outLen, size_t* outNeeded, int blockingMs);

// Cancel — two entry points; the spec deliberately exposes both so clients can wake a recv
// loop AND drop in-flight engine work in one cycle without composing helper frames.
//
//   frame_v2_cancel_request    — protocol-level cancel of an in-flight reqId. The engine
//                                drops the queued handler and emits an
//                                `error { code: "CANCELLED" }` frame. Equivalent to sending
//                                a `{kind:"request", method:"cancel", body:{targetId}}` frame
//                                but skips the user-space encode + send round-trip.
//   frame_v2_cancel_recv       — interrupt a thread blocked inside frame_v2_recv (returns
//                                FRAME_V2_CANCELLED). Used by the C# SDK to wake its recv
//                                dispatcher when the FrameSession is being disposed.
FCAPI int frame_v2_cancel_request(frame_v2_ctx*, const char* reqId);
FCAPI int frame_v2_cancel_recv   (frame_v2_ctx*);

// ABI 版本 — 嚴格遞增整數,client 第一件事呼叫
FCAPI uint32_t frame_v2_abi_version(void);  // returns 2 for this header

// build SHA(舊 frame_capi_version 同義,沿用)
FCAPI const char* frame_v2_build_sha(void);
```

**設計理由**:
- `frame_v2_open/close` 用 opaque context → 多 session 同時、ABI 不暴露內部結構;
- `send/recv` 完全 frame-based → DLL 與 stdio / pipe / TCP transport **共用 frame parser**,不重複寫;
- `outBuf` two-call dance(先試 cap=0 拿 needed,再配置)+ `NEED_BIGGER` 回傳 → client 不被 buffer 大小卡;
- `blockingMs` 同時支援同步(`-1` 阻塞)與 polling(`0`)→ GH 元件 UI thread 可 polling 避免凍結;
- exception **全在 C++ side caught**,跨 ABI 一律回 error code;
- ABI version 整數 → 新版 DLL 可同時提供 `frame_capi_v2_abi_version()` 返回 3,client 偵測舊 DLL 不致 crash。

---

## ⑦ Layer 3 — C# 客戶端 SDK(`FrameCore.Bridge`)

公開類型:

```csharp
namespace FrameCore.Bridge {

  // 強型別 model builder(流暢介面)
  public sealed class FrameModel { /* materials, sections, nodes, ... */ }
  public sealed class FrameModelBuilder {
      public FrameModelBuilder AddSteel(double E, double G, double rho) => ...;
      public FrameModelBuilder AddRectSection(double b, double d) => ...;
      public NodeRef AddNode(double x, double y, double z, Dof fixed_) => ...;
      public MemberRef AddMember(NodeRef i, NodeRef j, MaterialRef m, SectionRef s) => ...;
      public FrameModel Build();
  }

  // Session — Disposable,封裝 transport + handshake
  public sealed class FrameSession : IAsyncDisposable {
      public static Task<FrameSession> OpenAsync(BridgeOptions opts, CancellationToken ct = default);
      public IReadOnlyCollection<string> Capabilities { get; }
      public string EngineBuildSha { get; }
      public string SchemaVersion { get; }
      public bool HasCapability(string cap) => Capabilities.Contains(cap);

      public Task SetModelAsync(FrameModel m, CancellationToken ct = default);
      public Task<LinearResult> SolveLinearAsync(LinearOptions o, IProgress<Progress> p, CancellationToken ct);
      public Task<TensionOnlyResult> SolveTensionOnlyAsync(...);
      public IAsyncEnumerable<DynCollapseEvent> StreamDynCollapseAsync(DynCollapseOptions o,
                                                                       CancellationToken ct);
      // 部分讀
      public Task<Dictionary<int, double[]>> InspectDispAsync(IEnumerable<int> nodes, CancellationToken ct);
      public ValueTask DisposeAsync();
  }

  // BridgeOptions 選擇 transport
  public sealed class BridgeOptions {
      public TransportKind Kind { get; init; } = TransportKind.CApiV2InProcess;
      public string? FrameCapiV2DllPath { get; init; }   // for CApiV2
      public string? FrameCliExePath    { get; init; }   // for Stdio (legacy compat path)
      public string? NamedPipeName      { get; init; }   // for NamedPipe
      public IPEndPoint? Endpoint       { get; init; }   // for Tcp
  }

  // Forward-compat:結果物件總是攜帶 RawHeader,client 可讀未識別欄位
  public abstract class FrameResult {
      public string ReqId { get; }
      public JsonElement RawHeader { get; }   // server header (含未來新欄位)
      public byte[]?      BinaryPayload { get; }
  }

  // 強型別具體 result
  public sealed class LinearResult : FrameResult {
      public bool Singular { get; }
      public double PivotMargin { get; }
      public IReadOnlyDictionary<int, double[]> Disp { get; }       // node id -> [ux..rz]
      public IReadOnlyDictionary<int, double[]> Reactions { get; }
      public IReadOnlyDictionary<int, MemberEndPair> MemberForces { get; }
      public IReadOnlyDictionary<int, ShellForce> ShellForces { get; }
  }
}
```

關鍵設計選擇:
- **強型別** Model/Result vs 舊 `Dictionary<int, double[]>` — IDE intellisense + 編譯期捕錯;
- **流暢 builder** — 防止漏欄位;Build() 內呼 `validate()` 提早回報;
- **Async 全鏈** — GH 元件 SolveInstance 用 async 避免 UI thread stall;
- **`IAsyncEnumerable` 串流結果** — DYNC 邊跑邊吐幀,client 用 `await foreach`;
- **`IProgress<Progress>`** — GH 元件可動畫進度條;
- **`CancellationToken`** — 配 Layer 2 `cancel` method,GH 滑桿動下一格立即 cancel 前一格;
- **`RawHeader` JsonElement** — server 加新欄位 client 自動拿得到、不用升級 SDK;
- **`HasCapability` 探詢** — GH 元件依能力顯示/隱藏輸入端口。

---

## ⑧ Rhino / Grasshopper 元件設計(指南)

每個 GH 元件 = 一個 `GH_Component` 子類,內部持有**一個 module-level `FrameSession`**(reuse cross-`SolveInstance`),
在元件初始化時 `OpenAsync`,在 element disposal 時 `DisposeAsync`。

| GH 元件 | Input | Output | 用 method |
|---|---|---|---|
| `OpenFrameCore` | (none) | `FrameSession` | `session.open` |
| `Material` | `E, G, rho, cap*` | `Material` | (純 builder) |
| `Section` | `A, Iy, Iz, J, cy, cz, Asy, Asz` | `Section` | (純 builder) |
| `RectSection` | `b, d` | `Section` | (純 builder; 自動算 I/Z) |
| `NodeFromPoint` | `Point3d, Fixed[6]` | `NodeRef` | (純 builder) |
| `MemberFromCurve` | `Curve, Mat, Sec, RefVec, Active, TensionOnly` | `MemberRef` | (純 builder) |
| `ShellFromMesh` | `Mesh, Mat, t, Active` | `ShellRef[]` | (純 builder) |
| `NodalLoad` | `NodeRef, F[6]` | `Load` | (純 builder) |
| `Assemble` | `Nodes, Members, Shells, Mats, Secs, Loads` | `FrameModel` | `model.set` |
| `Solve` | `Session, Model, Options` | `LinearResult` | `solve.linear` |
| `PDelta` | `Session, Model, Options` | `PDeltaResult` | `solve.pdelta` |
| `TensionOnly` | `Session, Model, MaxIter` | `TensionOnlyResult + SlackList` | `solve.tension_only` |
| `SizeOpt` | `Session, Model, Amin, MaxIter, DcTol` | `Areas + WeightVol` | `solve.size_opt` |
| `DynCollapse` | `Session, Model, Dt, MaxTime, Removals` | `DynCollapseResult` | `solve.dyn_collapse` |
| `Corotational` | `Session, Model, Steps, Tol` | `CorotationalResult` | `solve.corotational` |
| `ArcLength` | `Session, Model, ArcLen, Steps` | `ArcLengthResult + Path` | `solve.arclength` |
| `Modal` | `Session, Model, nModes` | `Frequencies + ModeShapes` | `analysis.modal` |
| `Buckling` | `Session, Model, nModes` | `Lambdas + Modes` | `analysis.buckling` |
| `InspectDisp` | `Result, NodeIds` | `Dict<id, [u..r]>` | `inspect.disp`(高頻互動) |
| `InspectMF` | `Result, MemberIds` | `Dict<id, MemberForcePair>` | `inspect.member_forces` |
| `BMD` | `Result, MemberRef, nSamples` | `Curve[]` | (post-process,純 client) |
| `DCFringe` | `Result, ColorRamp` | `Mesh` | (post-process,純 client) |
| `Replay` | `DynCollapseResult, Time` | `Mesh` | (純 client 內插) |

設計鐵律:**Assemble → Solve → Inspect** 三段,讓 GH 元件可以複用同一 `FrameSession` + 同一 `LinearResult`,
inspect 元件只送 ids,不再傳整個模型來回。

---

## ⑨ 向後相容路徑(既有 J1/J2 不死)

| 路徑 | 狀態 | 客戶端 |
|---|---|---|
| `frame_cli.exe` stdin/stdout 文字 (J1/J1.5) | **永久保留**,新 method 不加入此 transport | 既有 GH 元件、Python tools、OpenSees 對標 |
| `frame_capi.dll` `frame_capi_solve_text` (J2) | **永久保留** | 既有 GH/.NET 元件 |
| **新 `frame_capi_v2.dll`** (this spec) | 新檔,不互相替代 | 新 GH 元件、新 Python SDK |
| **新 `frame_cli_v2.exe`** stdin/stdout framed | 與 `.dll` 共用 dispatcher | 雲端 / 跨機 |

**鐵律**:
- 舊 v1 `frame_capi.dll` **不**升級為 v2 schema(避免「升 DLL 就破舊元件」);v2 走獨立 DLL/exe;
- 引擎程式碼共用同一份 `Source/FrameCore/Public/Bridge/Dispatcher.{h,cpp}`,
  讓 v1 stdio / v1 cli core / v2 dll / v2 cli core 全部 delegate 到同一個 method registry;
- gate 第 6 leg(新增)驗 v2 round-trip;v1 gate(第 5 leg)維持不動 → v1 與 v2 兩條獨立護欄。

---

## ⑩ 落地路線(分階段,每階段 5-leg + 1-leg gate)

| 階段 | 範圍 | gate |
|---|---|---|
| **B1** | spec(本文)+ Layer 3 C# 骨架 + Layer 2 C ABI v2 header,**不影響引擎** | 編譯 `Plugins/.../v2/` C# 標準 .NET 8 標的(no Rhino dep)無錯 |
| **B2** | 引擎 dispatcher 雛形:`Bridge/Dispatcher.{h,cpp}` 實作 `session.open/close/status` + `model.set` + `solve.linear` + `inspect.disp` + `hello/capabilities` | 新 gate leg [6/6] CLI v2 round-trip:對既有 F1 懸臂、F2-tonly portal、F5 殼板等樣本走 v2 拿到的結果 vs J1 文字協議 bit-exact |
| **B3** | `solve.pdelta` / `solve.tension_only` / `solve.size_opt` / `solve.corotational` / `solve.arclength` / `analysis.modal` / `analysis.buckling` 全 method | v2 round-trip 覆蓋至少 8 個 method |
| **B4** | `solve.dyn_collapse` streaming + cancel + 二進位幀 payload | round-trip 含 streaming 重組驗 |
| **B5** | session.factor_reuse:`model.patch` 走 `ReSolveSession` + `model.set` mode=supernodal 走 `SnSession` | benchmark:同模型 100 次 patch → factor 只跑一次 |
| **B6** | named-pipe / TCP transport(可選,給雲端) | 不入主 gate |
| **B7** | Rhino 8 .gha 真實作 + Yak 發佈(needs Rhino 環境;不入引擎 gate) | 外部 CI |

每階段都各自可單獨上;**B1 不需引擎改動,可立刻開工**。

---

## ⑪ 已知 / 不做(誠實邊界)

- **schema language**:這份 spec 是手寫散文 + JSON 範例,沒用 OpenAPI/Protobuf。理由:加第三方 schema 工具會違反「無依賴」原則,且 v2 schema 規模小(~20 method)夠手寫管理。當 method 數爆炸到 50+ 時可重新考慮 codegen,但**SLA 是 schema 不變**,工具是後話。
- **二進位序列化**:不用 bincode/MessagePack/CBOR,純自定 length-prefixed JSON header + raw double payload。理由:可審計、可手寫 debug、依賴零。代價是 client/server 都要寫 frame parser,但這只是一次 ~200 lines。
- **OAuth / TLS / auth**:本協議**無 auth 概念**(in-process / 同機 stdio);TCP transport 走 TLS / cert 由部署層處理,不入 schema。
- **schema 演進的形式驗證**:沒有自動工具檢查「新版改舊欄位」;靠 review + 第 6 leg gate 用 v2.1 client 跑 v2.2 server 的對標。
- **v2 vs v1 自動橋接**:不做。client 二選一,**伺服器同時提供兩條獨立 DLL/exe**。

---

## ⑫ 對既有鐵律的影響

| 鐵律 | 影響 |
|---|---|
| FrameCore 純 C++17/Eigen,Eigen 不洩漏 | ✅ Dispatcher / frame parser 純 std + 自帶 JSON mini-parser;不引入新依賴 |
| 五腿 gate 全綠 | ✅ v2 不改既有 5 腿;v2 自成第 6 腿 |
| 索引而非裸指標 | ✅ wire schema 用 id(node/member/shell id),從不用指標 |
| commit 衛生(不碰 .gitignore / .uproject / LevelSim) | ✅ 全新檔 + 新目錄 |
| 誠實驗證、不過度宣稱 | ✅ B1 完成 → 文檔產出;B2 完成 → round-trip 驗;階段邊界明示 |

---

## ⑭ Profile — Simple vs Advanced(兩套並行,同引擎同 ABI)

**動機**:既有引擎 API 為了 **UE5 直接用** 在許多地方做了 silent 預設 / silent fallback /
silent 結果簡化,讓客戶端「丟資料進來就會跑」。對 UE5 場景這是對的;但對學術精度、
官方 Rhino 工作流程、CI 嚴格驗證、Rhino-UE5 橋接協議(本協議的另一端)是「丟失了真相」。

v2 因此在 `session.open` 加一個 `profile` 欄位,**同引擎、同 dispatcher、同 wire frame**,
用 dispatcher-level facade 切兩套行為。引擎核心一行不動(鐵律 1)。

```jsonc
// session.open body
{
  "profile": "simple" | "advanced",   // 缺欄位 = "simple"
  "diagnosticStream": true,           // 僅 advanced 可開;每階段 push event
  "mode": "default" | "supernodal" | "resolve",
  "options": { ... }
}
```

### 14.1 silent 點清單(advanced 全部要 expose 或 reject)

對應現況的 10 個 silent 點(來自 `frame_cli_core.cpp` / `SolveOptions.h` / `SnSession.h` 審讀):

| # | silent 點 | simple(現況) | advanced |
|---|---|---|---|
| 1 | `MAT`/`SMAT` 無 cap | 預設 `make(300,300,180)` MPa | error `VALIDATION_FAILED { field: "materials[i].cap" }` |
| 2 | `useSupernodalPrimary=true` 但 SPD 失敗 | 自動 fallback LDLT | error `NOT_SPD { hint: "engine refused silent fallback under advanced profile" }` |
| 3 | `SnSessionOptions.fallbackOnFail` | 預設 `true` | client 必須顯式選 `fallbackOnFail`;`false` 時失敗回 error |
| 4 | `FRAMECORE_SUPERNODAL=0` build | 旗標靜默忽略 | error `CAPABILITY_NOT_COMPILED` 在 `session.open` 階段 |
| 5 | singular system | `result.singular = true`,`u` 空、index UB-safe 回 0 | error `SINGULAR_SYSTEM { mechanismCandidates: [...] }` |
| 6 | `shellCurvatureMaxAngleDeg=0` | guard off,粗網格靜默接受 | 必須顯式給 ≥ 0;`= 0` 視為「明示關閉」記入 `advancedDiagnostics.guardsDisabled` |
| 7 | DYNC `frameStride=10`,只回 `DFRAME t maxAbsU` | 摘要 + 峰值 | 預設 `binaryFrames=true`(每幀 12N bytes raw doubles)+ 碎塊 cluster 細節 + truncationResidual + energyBefore/After + basisInheritance |
| 8 | SizeOpt `Amin=0` + 零力桿 | 截面不更新(silent) | warning event `size_opt.zero_force { members: [...] }`;結果欄位 `zeroForceMembers` |
| 9 | ReSolve Tier-3 rebaseline 自動觸發 | silent ladder 走完 | event `reanalysis.tier_change { from: 2, to: 3, reason: "pcg_nonconverged" }`;結果 `tierLadder[]` |
| 10 | `validate()` warpTol 預設 `1e-6` 嚴格 | `WARP` token 沒給就嚴格 | 必須顯式給 `warpTolerance` 與 `useWarpingCorrection` |

### 14.2 結果欄位差異

advanced 在每個 result body 加 `advancedDiagnostics`(simple = 欄位不存在):

```jsonc
{
  ...simple fields...,
  "advancedDiagnostics": {
      "factorMethod":   "LDLT" | "SupernodalPrimary",
      "factorBackend":  "SimplicialLDLT" | "SelfBuiltSupernodal" | "CHOLMODOracle",
      "factorTimeMs":   42.3,
      "solveTimeMs":    1.1,
      "pivotMarginTrace": [ ... ],      // |D|min/|D|max trace if assembleAndFactor 內部分段了
      "snIrResidualHistory": [ ... ],   // SnSession IR 每步殘差(only mode=supernodal)
      "guardsDisabled": [ "shellCurvature" ],
      "mechanismCandidates": [ ... ]    // 排序的 near-singular DOF(only if pivotMargin < 10x pivotTol)
  }
}
```

每分析另加自己的子集:

| 分析 | advanced 額外欄位 |
|---|---|
| `solve.linear` | `pivotMarginTrace`、`mechanismCandidates`、`factorMethod`/`Backend`/`TimeMs` |
| `solve.tension_only` | `iterationTrace[]`(每迭代鬆弛桿差分)、`cycleDetection`(reactivation history) |
| `solve.size_opt` | `convergenceHistory[]`、`zeroForceMembers[]`、`materialCapsApplied{matIdx → cap}` |
| `solve.dyn_collapse` | `ritzBasis{size, residual}`、`energyTrace[]`、`fragmentClusters[]`(mass/inertia/vel/angVel)、`basisInheritance.truncationResidual[]` |
| `solve.corotational` | `nrResidualHistory[]`、`tangentConditioning[]` |
| `solve.arclength` | `predictorDirection[]`、`corrector.relResHistory[]` |
| `analysis.modal` | `ritzTruncationResidual`、`subspaceIters`、`modeShape binary` 每模態(96 * n bytes) |
| `analysis.buckling` | `lambdaShiftStrategy`、`subspaceIters`、`bucklingMode binary` 每模態 |
| `analysis.reanalysis_solve` | `tierLadder[]`、`pcg.iters`、`pcg.relResidual`、`woodbury.rankUsage` |

### 14.3 diagnostic.stream(opt-in,僅 advanced)

`session.open` 時 `diagnosticStream: true` → 引擎在每階段 push `event` kind:

```jsonc
{ "kind":"event", "id":"<reqId>", "body": {
    "channel": "diagnostic",
    "stage":   "validate" | "assemble" | "factor" | "solve" | "recover" |
               "modal.basis" | "modal.subspace" | "resolve.tier" | "dyn.event",
    "elapsedMs": 12.3,
    "details": { ... }   // 階段特定
}}
```

UE5 simple 路徑**不發**這些 event(節省頻寬);advanced + `diagnosticStream` 才開,
給 Rhino 元件以及學術精度驗證的 host 用。

### 14.4 model.set strict 驗證(advanced)

advanced profile 開啟時,dispatcher 對 `model.set` body 多跑下列檢查,缺欄即 reject:

- 每個 `materials[i]` 必含 `cap.comp` / `cap.tens` / `cap.shear`
- 每個 `nodes[i]` 若 `fixed[k] = 1`,`prescribed[k]` 必須**顯式存在**(可以是 0,但欄位不能省)
- 每個 `members[i]` 必含 `refVec`(三維)、`active`、`tensionOnly`、`release`(長度 12)
- 每個 `shells[i]` 必含 `active`
- `options` 必須顯式提供 spec § ④ `session.open` 列出的所有欄位

simple 維持現況:任何欄位省略 → 用引擎預設,**結果欄位 `defaultsApplied[]` 列出哪些 default 被填**,
讓 UE5 客戶端事後可以查(不強制看)。

### 14.5 capability 字串

`hello` response 的 `capabilities` 集合,新增:

- `"profile.simple"`(永遠在;v2 的最低承諾)
- `"profile.advanced"`(advanced 完整實作後才宣告)
- `"diagnostic.stream"`(diagnosticStream:true 可用時宣告)
- `"binary.modes"`(modal/buckling mode shape binary payload 支援)
- `"dyn_collapse.full_frames"`(完整 u/v 二進位幀串流 — advanced 必備)
- `"dyn_collapse.fragment_detail"`(碎塊 cluster mass/inertia/vel/angVel 細節)

client `HasCapability("profile.advanced")` 才能 open advanced session。

### 14.6 SLA(profile-specific 承諾)

| | simple | advanced |
|---|---|---|
| Forward-compat(欄位演化) | 同 § ⑤ | 同 § ⑤,**但** advanced 額外欄位**僅承諾 minor 內穩定**(major bump 可改) |
| 預設值穩定 | 預設值改變視為 minor 變更(documented) | 不適用 — 全部 required |
| silent fallback 行為 | minor 不破 | **無 silent**(承諾) |
| 結果欄位最小集 | minor 不縮 | minor 不縮 |
| diagnostic event channel 名 | n/a | 永久保留;增 channel 不破舊 client |
| 額外開銷 | 最低 | 結果 + diagnostic event 約多 2-5×(profiler 數據,典型 frame) |

### 14.7 為什麼不做成兩個獨立 method 命名空間(`solve.linear` vs `advanced.solve.linear`)

考慮過,放棄。原因:
1. **method 名爆炸** — 每個 method 都要兩份,documentation 與 SDK 都翻倍;
2. **路由邏輯重複** — dispatcher 對每個 method 都要做兩個 entry,維護成本高;
3. **profile 是 connection-level concern,不是 method-level** — 一個 GH session 永遠固定一種 profile(GH 元件作者選擇),把它放 session 自然;
4. **C# SDK 雙入口(`OpenSimpleAsync` / `OpenAdvancedAsync`)語意清楚** — 用法層面雙 namespace 已經被 SDK 包好,使用者看不到 wire 細節。

### 14.8 與 UE5 端的關係

| 客戶端 | profile | 為何 |
|---|---|---|
| UE5 plugin(`LevelSim/Chaos`,既有 J1/J2) | n/a — 走 v1 文字協議 | v1 不動,UE5 既有元件零影響 |
| UE5 plugin v2 future(替換 J1 走 v2) | `simple` | UE5 host 不暴露學術細節 |
| Rhino .gha v2 簡單元件 | `simple` | 一般 GH 使用者 |
| Rhino .gha v2 **進階元件**(Inspector / DiagnosticView / SnIrPlot / TierTrace) | `advanced` | 學術 / pro 工作流程 |
| 雲端 Rhino-UE5 橋接協議(WS_K,future) | `advanced` | 兩端需協同精度與 fallback 行為 → 不能 silent |
| `Tools/v2_roundtrip.py`(B2 新 gate leg) | `advanced` | gate 必須測到 silent 路徑被攔下 |

### 14.9 落地路線中的位置

| B 階段 | profile 工作 |
|---|---|
| B1(設計) | 本節規格 + SDK 雙入口骨架 |
| B2(dispatcher) | `simple` 完整實作;`advanced` 只實作 `model.set strict` + `solve.linear` 的 silent 攔截;gate leg 必須驗 simple/advanced 對同 model 結果**主要欄位 bit-exact**(advanced 多 metadata,不影響核心數值) |
| B3 | 補各 method 的 advanced 子集(iterationTrace、convergenceHistory 等) |
| B4 | DYNC `binary.frames` + `fragment_detail`(advanced 必備) |
| B5 | SnSession IR residual + ReSolve tierLadder + Modal/Buckling binary modes(advanced 必備) |

---

## ⑬ 開工檢核(B1 階段)

完成 B1 需產出:

- [x] `docs/specs/S6b_rhino_bridge_v2.md`(本文)
- [ ] `Plugins/FrameSolver/Standalone/frame_capi_v2.h`(C ABI v2 header,空實作)
- [ ] `Plugins/FrameSolver/Grasshopper/v2/README.md`(v2 客戶端說明)
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Bridge/FrameSession.cs`(C# session 骨架)
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Bridge/FrameProtocol.cs`(frame parser / serializer)
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Bridge/BridgeOptions.cs`
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Model/FrameModel.cs`(強型別 model + builder)
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Model/Materials.cs` / `Sections.cs` / `Nodes.cs` / `Members.cs` / `Shells.cs` / `Loads.cs`
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Result/LinearResult.cs` / `Results.cs`(全 result POCO)
- [ ] `Plugins/FrameSolver/Grasshopper/v2/Result/RawHeader.cs`(forward-compat 未知欄位收集器)
- [ ] `Plugins/FrameSolver/Grasshopper/v2/FrameCore.Bridge.csproj`(net8.0,zero Rhino dep)

B2+ 由本 spec § ⑩ 引導實作,本輪交付 = **設計 + 骨架**。
