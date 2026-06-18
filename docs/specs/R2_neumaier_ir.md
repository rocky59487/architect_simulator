# R2:Neumaier 補償求和 IR(supernodal lane 殘差精度延伸)

> R 線第二階段。R1 已落地自建 supernodal Cholesky direct lane(`docs/PROGRESS_R_supernodal.md`,`F55`/`F56`)。R2 在 supernodal lane 上以 opt-in iterative refinement 突破固定精度殘差底限。

## 1. 動機與目標

### 1.1 觀察到的痛點(v3 記憶體線 recon 第一手數據)

`docs/specs/v3_memory_recon.md` 偵察結果顯示:**混合 frame+shell 建築標準拓撲在 64k DOF 即出現 res>1e-9 的固定精度殘差底限**(`Research/out/sn_sweep.txt:14`):

| 規模 | factorMs | res(K·u−F) | vsCHOLMOD | gate |
|---|---|---|---|---|
| 17k | 562 | 5.59e-10 | 8.78e-13 | PASS |
| 31k | 1542 | 8.77e-10 | 1.42e-12 | PASS |
| **64k** | **2409** | **1.40e-9** | **2.57e-12** | **\*\*\*FAIL\*\*\*** |
| 191k (CHOLMOD) | 6070 | 1.57e-9 | — | FAIL |

關鍵觀察:**vsCHOLMOD 一直在 1e-12 量級**——解向量本身非常精確。殘差超標純粹來自 K·u 的累積捨入放大,**不是 factor 精度問題**。

### 1.2 為什麼 long double residual 在 MSVC 上無效

固定精度殘差的傳統解法之一是用 `long double` 計算 K·u 後再做減法。但 MSVC 上 `sizeof(long double) == sizeof(double)`——`long double` 是 `double` 的別名。此技巧在 MSVC 為 no-op。

**Neumaier 補償求和**是 MSVC 上平台兼容的替代方案:在 `double` 精度內以 O(n) 額外運算捕捉每行累加的低位 bit,殘差精度上限從 `eps·cond(K)·‖u‖` 降至 `eps²·cond(K)·‖u‖`(Higham, *Accuracy and Stability of Numerical Algorithms*, ch.4)。

### 1.3 目標

- **正確性**:在固定精度殘差為 `eps·cond` 量級的場景,Neumaier IR 把殘差降至接近 `eps²·cond`(實測順序減少)。
- **零回退風險**:`irSteps=0`(預設)逐位元同無 IR 路徑。
- **opt-in**:現有 default(LDLT)路徑與既有測試零變化。

## 2. 設計

### 2.1 公開 API 變動(POD,無 Eigen)

`Public/FrameCore/SnSolveOptions.h` + `Public/FrameCore/SnSession.h` 各加兩個欄位:

```cpp
int    irSteps = 0;     // 0 = off (bit-identical to no-IR); 1-2 typical
double irTol   = 0.0;   // 0 = no early stop; >0 = break when ||r||_inf <= irTol * ||b||_inf
```

預設 `0/0.0` → 與 v3.0 之前完全相容。守 FRAMECORE_API / POD 邊界。

### 2.2 私有實作

**`Private/sn_chol.h`** 加兩個 inline helper(在 `sn::` namespace,header-only,免動 build.bat):

```cpp
// r = b - A*x via Neumaier compensated SpMV (full-symmetric CSC).
void neumaierResidualFullSym(int n, const int* Ap, const int* Ai, const double* Ax,
                             const double* b, const double* x, double* r);

// inf-norm helper for convergence + diagnostic.
double infNorm(int n, const double* x);
```

`neumaierResidualFullSym` 對每行累加維護一個補償 slot `c[i]`,當前項 `add = -Aij·xj` 與 `r[i]` 量級差距大時,精確保留遺失低位:

```cpp
const double s = r[i] + add;
c[i] += (|r[i]| >= |add|) ? ((r[i] - s) + add) : ((add - s) + r[i]);
r[i] = s;
```

最後 `r[i] += c[i]` 把累積補償一次性加回。

### 2.3 兩個 lane 的整合

**`Private/SnSolver.cpp`(stateless `solveLoadSupernodal`)**:K_ff 本來就在 scope 內(每次 call 重建),直接在 `sn::solveSuper` 後加 IR 迴圈,**無快取**。

**`Private/SnSession.cpp`(stateful `solveFrame`)**:K_ff 在 ctor 時計算完即丟棄。為 IR 補入 cache(opt-in,僅當 `opts.irSteps>0` 時填):

```cpp
struct SnSession::Impl {
    ...
    std::vector<int>    Ap, Ai;
    std::vector<double> Ax;
};
```

ctor 從 `Kff.outerIndexPtr()/innerIndexPtr()/valuePtr()` 複製。記憶體成本 `~nnz·(4+8)+(n+1)·4` bytes,在 100k DOF 規模約十幾 MB,相對 factor 自身可忽略;`irSteps=0` 時 cache 為空,零成本。

### 2.4 IR 迴圈

Wilkinson 標準形:

```cpp
for (k = 0; k < irSteps; ++k) {
    neumaierResidualFullSym(..., b, x, r);   // r = b - K·x (compensated)
    if (irTol > 0 && ||r||_inf <= irTol·||b||_inf) break;
    sn::solveSuper(fac, sym, r, d);          // d = K^-1 r (重用 factor)
    for (int i = 0; i < n; ++i) x[i] += d[i];
    ++irApplied;
}
```

每步成本:1 個 compensated SpMV + 1 個 forward/backward subst。Factor 不重建——這是 IR 在 supernodal lane 落地的核心優勢。

### 2.5 diagnostic

`SolveResult::diagnostic` 在 IR 觸發時附帶資訊:

```
[SnSession] supernodal solve (reused factor) + IR2/2 (resInf=1.234e-12)
[SnSolver]  self-built supernodal Cholesky + IR2/2
```

## 3. 驗證(F62)

`Standalone/main.cpp` 新增 **F62:slender cantilever(L/d=200, ~cond 1e6)**,四個分項:

| 分項 | 檢驗內容 | 結果 |
|---|---|---|
| F62a | `SnSessionOptions{}`(default) 與 `irSteps=0` 顯式 → `u` 逐位元相同 | uDiff=0.0 ✓ |
| F62b | `SnSession` `irSteps=2` 殘差 < `irSteps=0` 殘差 | 6.123e-8 → 5.029e-8(ratio 0.82) ✓ |
| F62c | IR 修正後解與非 IR 解相對距離 < 1e-6(IR 是精度 polish 非別解) | rel=1.148e-9 ✓ |
| F62d | stateless `solveLoadSupernodal` IR 路徑同 SnSession 改善殘差 | s0=6.123e-8 → s2=5.029e-8 ✓ |

### 3.1 為什麼 standalone 改善只有 18%

standalone 受限於小規模 fixture(~50 elements / cond ~1e6),殘差已接近機器精度。**IR 真正的價值在 100k+ 規模、cond ~1e8 以上的混合建築**——但這種規模不能放進 5 腿 gate(秒級執行原則)。

F62 的角色是 **regression guard + 機制驗證**,不是 production performance demonstration:
- F62a 守 backward-compat(零回退)
- F62b/d 證明 IR 確實降殘差(任意改善幅度 > 0)
- F62c 證明 IR 不是 redirect 而是 polish

生產規模的真實改善需未來在 `Research/` 量測補上,或直接接 `sn_sweep.txt` 的 64k 混合建築 case 跑 `irSteps=1`。

### 3.2 為什麼選 slender cantilever 而非 alternating stiff/weak

最初設計用交替剛性/弱段(side 100mm vs 1mm)製造 cond ~1e15。實測 IR 改善僅 0.8 倍且解被「修正」0.2%——這不是 IR 真的修正,是 fixture 太接近數值奇異,**weak 段在數值上形同 hinge,IR 落在 K 的 near-null-space 而被放大**。

換成 uniform slender cantilever 後,系統 cond 高但不在數值奇異附近,IR 收斂正常。

## 4. 五腿閘門

```
[1/5] standalone F1-F62 ALL PASS  (failures=0)
[2/5] UE automation: 55 tests green
[3/5] OpenSees compare: PASS
[4/5] linear deep audit: 104 checks PASS
[5/5] CLI round-trip: ALL PASS
GATE: PASS
```

UE 端未新增 IR 專屬測試(`FrameCoreSnSessionTest` 已涵蓋 factor-once+solve-many,`irSteps=0` 預設下逐位元等價);新增 UE IR 測試屬未來工作。

## 5. 邊界 / 留未來

- **F62 standalone 改善幅度溫和**:18% 殘差降低。生產規模實證需獨立 benchmark(`exp_sn_chol --mixed --ir=1`)。
- **沒有 UE 端 IR 測試**:UE automation 只跑 `irSteps=0`(預設),涵蓋零回退但不驗 IR 路徑。
- **IR overhead 隨規模成長**:64k 混合建築 backsub 33.6ms → 加 IR 估算 ~94ms(`v3_memory_recon.md:244`),逼近 100ms 互動上界。**在規模 >~80k 時 IR 與互動目標可能衝突**。
- **`irTol` 早停未在 F62 驗證**:目前測試固定走 irSteps 步;`irTol > 0` 路徑的單元測試留未來。
- **Neumaier 對解誤差有理論上限**:當 `cond > 1/eps`(~1e16)系統實質奇異,IR 不收斂(本輪初版 alternating stiff/weak fixture 實證)。
- **CSC cache 假設 K_ff 對稱完整存儲**:由 `reduceFF` 保證(`PreparedSystemImpl.h:24-37`);若未來引入只儲存下三角的路徑,需修改 compensated SpMV 內核。

## 6. 觸發判據(opt-in)

使用者呼叫端:

```cpp
SnSessionOptions sopt;
sopt.enabled = true;
sopt.irSteps = 1;        // 一步 IR 通常足夠
sopt.irTol   = 1e-12;    // 早停閾值(可選)
SnSession sess(ps, sopt);
SolveResult R = sess.solveFrame(model);   // diagnostic 顯示 "+ IR1/1 (resInf=...)"
```

stateless lane 用法相同 `SnSolveOptions::irSteps = 1`。

## 7. 關鍵檔案 / 行號

- `Public/FrameCore/SnSolveOptions.h`:`irSteps` / `irTol` 欄位
- `Public/FrameCore/SnSession.h`:`SnSessionOptions::irSteps` / `irTol` 欄位
- `Private/sn_chol.h`:`sn::neumaierResidualFullSym` / `sn::infNorm`
- `Private/SnSolver.cpp`:stateless IR 迴圈
- `Private/SnSession.cpp`:stateful IR 迴圈 + CSC cache
- `Standalone/main.cpp`:F62(slender cantilever IR)
- `docs/specs/v3_memory_recon.md`:推薦動機與證據基礎
- `docs/PROGRESS_R_supernodal.md`:R1 supernodal direct lane 落地紀錄(本階段建在 R1 之上)
