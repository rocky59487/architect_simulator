# warped quad 處理 — v3 曲面線第③項(務實完成:可用性 + 誠實精度邊界)

> 一句話:讓 **warped(非共面)四邊形殼網格能用於自由曲面建築**(Gaudí/Zaha 分網必然產生 warped quad),
> 並對其精度誠實標界。核心交付 = `SolveOptions::warpTolerance` 放寬 `validate()` 對 warped quad 的硬拒絕;
> 輔以 `useWarpingCorrection`(best-fit 平面投影 + 記錄角點翹曲 `warp_`)。**精度靠網格加密**(MITC4 仍是
> flat facet,warp 加一個隨 warp→0 消失的有界誤差),完整 MacNeal/Sabir 旋轉耦合修正是未來。

## ★ 動工中的關鍵發現(誠實,改變了原計畫)

原 plan 假設「best-fit 投影改善 warped 精度」。**實作 + 實測(F61)推翻此假設**:
- 對常見 warped quad(平行四邊形 + 單點抬升),**Newell 平均法線恰好等於對角叉積法線**(代數可證),
  故 best-fit 只是把投影原點從 P0 移到 centroid —— 純平移,**剛度逐位元不變,精度零改善**
  (F61 實測:warp=3% 時 P0 與 best-fit 的 Nxx 誤差都是 9.006e-4,改善 1.00x)。
- 真正改善 warped 精度需 **MacNeal/Sabir rigid-offset 修正**(把節點偏離平面 `warp_k` 引起的「旋轉↔面內
  位移」耦合加入剛度)。但這對**純膜場(旋轉固定)無效**,需旋轉自由的彎曲場景,且可能與 drilling/MITC4
  交互破壞既有 gate —— 研究級難題,改善幅度不確定 → **列為未來**。
- 因此本項**務實定位**:交付「warped 網格可用 + 誠實精度邊界」,不過度宣稱 best-fit 改善精度。

## ① 目標 / 不做

**目標**:warped 自由曲面殼網格能 solve(放寬 validate)、預設行為不變、精度誠實標界(靠加密收斂)。

**不做**:完整 MacNeal/Sabir 旋轉耦合修正項(未來,需旋轉自由場景驗證 + 防破 drilling gate);MITC4 仍是
flat facet(O(1/N²) faceting 不變);best-fit 投影**不宣稱**改善規則 warped quad 精度。

## ② 公開 API

```cpp
// SolveOptions.h（皆 opt-in,預設保持今天行為 BIT-FOR-BIT）
bool useWarpingCorrection = false;   // best-fit 平面投影（Newell 法線 + centroid origin）+ 記錄 warp_
real warpTolerance = 1.0e-6;         // 放寬 validate 對 warped quad 的硬拒絕（1e-6=strict 預設）
```

`FrameModel::validate(std::string& why, real warpTol = 1.0e-6)` 加預設參數(呼叫處不變;FrameSolver /
CorotationalAnalysis 透傳 `opts.warpTolerance` / `opts.solve.warpTolerance`)。

## ③ 資料流 / ④ 演算法

`MITC4ShellElement::prepare()` 的局部 frame / 角點投影(`xl_/yl_`)opt-in 分支:
- **OFF(預設)**:原點 P0、法線 = 對角叉積 `(P2−P0)×(P3−P1)`、`xl_/yl_ = dot(Pg−P0, e1/e2)`、`warp_=0`
  (今天行為,逐位元)。
- **ON**:原點 centroid、法線 = **Newell 平均法線**(4 邊環積)、`xl_/yl_ = dot(Pg−centroid, e1/e2)`、
  `warp_[k] = dot(Pg[k]−centroid, n)`(角點到 best-fit 平面的有號距離,供未來 MacNeal 修正 / 診斷)。

`validate()`:`maxWarp > warpTol · max(1, maxEdge)` 才拒(warpTol 預設 1e-6 = 今天;放寬讓 warped 可用)。

## ⑤ 檔案(全在既有檔,無新 .cpp)

| 檔 | 改動 |
|---|---|
| `Public/FrameCore/SolveOptions.h` | `useWarpingCorrection` + `warpTolerance` |
| `Public/FrameCore/FrameModel.h` / `Private/FrameModel.cpp` | `validate` 加 `warpTol` 預設參數,閾值用之 |
| `Private/MITC4ShellElement.{h,cpp}` | `warp_[4]` 成員 + prepare opt-in best-fit 投影分支 |
| `Private/FrameSolver.cpp` / `Private/CorotationalAnalysis.cpp` | validate 呼叫透傳 warpTolerance |
| `Standalone/main.cpp` | **F61** oracle |
| `Private/Tests/ShellWarpedTest.cpp` | UE mirror `FrameCore.Shell.WarpedPatch`（新檔,自動編） |

## ⑥ Oracle(誠實分級)

- **F61a `[VERIFIED]`**:warped 膜 patch(中心節點抬升)在 `warpTolerance` 放寬下**能 solve、非奇異、誤差有界**
  (warp=3% Nxx rel<10%)。**這是核心交付(warped 網格可用)的驗證**。並記錄 best-fit vs P0(實測 1.00x,
  誠實揭露 best-fit 對規則 warped 無改善)。
- **F61b `[VERIFIED]`**:flat patch 兩路徑(P0 / best-fit)都回復**精確常 Nxx**(rel<1e-10)→ 預設不破 +
  best-fit 在 flat 上正確。
- **F61c `[VERIFIED]`**:warp 誤差隨 warp 減小**乾淨二次收斂**(warp 4%→1.6e-3、2%→4.0e-4、1%→1.0e-4)
  → **warped 網格靠加密達精度**的數據支撐(每元素 warp 隨網格加密減小)。
- **UE mirror** `FrameCore.Shell.WarpedPatch`:warped admitted + flat 不破 + warp 誤差隨 warp 收斂。
- **預設不破**:兩旗標 false → flat / warped-rejected 行為逐位元同今天(既有殼 gate F13-F16 + OpenSees ~1e-10 不動)。

## ⑦ Gate

F61(a/b/c);UE `FrameCore.Shell.WarpedPatch`;`run_gate.ps1 $ExpectedUeTests` **54 → 55**。
三支 build.bat 免動。**五腿全綠**(standalone F1–F61 / UE 55 / OpenSees / audit 104 / CLI)。

## ⑧ 效能驗收

prepare 期一次性的 best-fit 投影(Newell 法線 + centroid)= O(1) 小常數;不改 solve / factorize-once /
ReSolve(只改元素局部幾何快取)。opt-in,預設零成本。

## ⑨ 誠實邊界 / novelty 定位

- **核心價值 = 可用性**:`warpTolerance` 讓 warped 自由曲面網格能 solve(不被硬拒)—— 這是 Gaudí/Zaha
  分網的實際需求,務實且有效。
- **best-fit 投影非精度魔法**:對規則(仿射)warped quad 零改善(Newell==對角叉積,只 origin 平移,F61 實測
  1.00x);對一般非仿射 warped 可能微小改善;主要作用是建立 best-fit 投影基礎 + 記錄 `warp_`(未來修正用)。
- **精度靠加密**:MITC4 flat facet,warp 加一個 **O(warp²)** 有界誤差(F61c),隨網格加密(每元素 warp 減小)
  收斂。warped 網格達精度的方法是加密,非單元素魔法修正。
- **完整修正是未來**:MacNeal rigid-offset(旋轉↔面內耦合,對純膜無效需旋轉場景)+ Sabir 膜-彎耦合項;
  研究級,改善幅度不確定,且須防破 drilling/flat-patch gate。
- **預設安全**:旗標 false → 逐位元今天 → 零回歸。

## ⑩ 風險 / fallback

- **零回歸**:opt-in 預設 off;flat / 既有殼 gate 守住(F61b + F13-F16)。
- **warpTolerance 濫用**:放太大 → 嚴重 warped quad 精度差但不拒 → 誠實標(診斷字串提示加密 + 配
  useWarpingCorrection);使用者責任。
- **接續(未來)**:MacNeal/Sabir 完整修正在旋轉自由 warped 殼板彎曲場景驗證,對標 twisted-beam / warped
  shell 文獻;與殼 CR(已完成)結合 → 自由曲面 warped 網格 + 大變形。
