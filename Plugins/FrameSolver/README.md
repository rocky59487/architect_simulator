<div align="center">

# 📐 FrameCore

**一顆研究級的三維結構有限元引擎 —— 它印出來的每個數字,都對得上一個獨立 oracle。**
*A research-grade 3-D structural FEM engine where every number it prints is checked
against an independent oracle.*

`C++17 + Eigen` · POD 公開 API · 同一份原始碼編成 standalone gate **和** UE5 外掛
· `v4.0.0` 引擎凍結 / engine frozen

[完整技術參考 / full reference](../../docs/FrameCore_full.md) ·
[驗證證據鏈 / verification](../../docs/VERIFICATION.md) ·
[CLI 協定 / wire protocol](../../docs/CLI_PROTOCOL.md) ·
[← 專案門面 / project landing](../../README.md)

</div>

---

<div align="center">

<!--
  TODO 表現法 / hero media:
    變形圖、利用率熱圖、倒塌重播 —— 產出後換掉這個佔位區。
-->
`┌──────────────────────────────────────────────┐`
`│  🎬  變形 / 熱圖 / 倒塌重播 截圖即將登場         │`
`│      deformation · heat-map · collapse replay │`
`└──────────────────────────────────────────────┘`

</div>

## 30 秒導覽 · 30-second tour

你給它一個結構(節點、梁柱、殼、載重),它**解**;你要它走到極限,它**讓結構倒給你看**
—— 一個構件一個構件地失效、清掉碎塊、重算、直到變成機構。然後最關鍵的一步:它算出來的
每一個位移、內力、屈曲因子、倒塌順序,都**回頭去對一個它自己沒參與計算的答案**:解析解、
已發表基準、或 OpenSees。對不上,就不是綠燈。

*Hand it a structure and it solves. Ask for the limit and it collapses the thing in front
of you — element by element, cleaning up debris, re-solving, until it's a mechanism. Then
the part that matters: every displacement, force, buckling factor and collapse step is
checked back against an answer it didn't help produce — closed-form, a published benchmark,
or OpenSees. No match, no green light.*

公開 API 只用普通 C++ / POD 型別(沒有 UE、沒有 Eigen 洩漏),所以**同一份原始碼**既能編成
秒級的 console 驗證 gate,也能當成 Unreal Engine module 直接掛進遊戲。
*The public API is plain C++/POD — the same source compiles as a standalone gate and as a
UE module.*

## 能力一覽 · Capabilities at a glance

每一層都躲在同樣兩道接縫之後(`IElement` 換元素型別、`PreparedSystem` 重用分解),
而且每一層都有自己的 oracle 把關。詳細表格與每項的量測一致性見
[完整技術參考](../../docs/FrameCore_full.md)。

*Each layer sits behind the same two seams and is gated by its own oracles. Full per-row
detail in the [reference doc](../../docs/FrameCore_full.md).*

| 層 / layer | 內容 / what | 把關的 oracle |
|---|---|---|
| **線性核心** Linear core | 3-D 直接勁度、Timoshenko 剪力、端點釋放、**從分解偵測機構/失穩**、MITC4 殼(膜+板彎+鑽轉)、彈性 D/C 篩 | 解析解 · OpenSees `ShellMITC4` ~1e-10 |
| **線性分析** Analysis suite | 載重組合/包絡、自重、**一次分解多次求解**(互動再解)、支承沉陷、影響線、模態、線性屈曲、反應譜、實時暫態 | OpenSees `eigen` ~1e-11 · Müller-Breslau |
| **倒塌線** Collapse | 構件移除、安全裕度(`pivotMargin`)、碎塊連通(→ UE Chaos 交棒)、GSA 式 LSP 倒塌驅動、殼 von Mises 失效、事件式塑性鉸 + 選配 N–M 互制、**連續動態倒塌**(模態空間 Newmark) | `w*=16Mp/L²` ±2% · 動量守恆 |
| **非線性/再分析** Nonlinear | 三階再分析階梯(Woodbury / 過期分解 PCG / rebaseline)、P-Delta 二階、張力構件、**共旋大變形** 2-D/3-D + 弧長越過極限點 | fresh-factor ~1e-12 · OpenSees corot 1.22e-9 · arc-length 6.4e-3 |
| **最佳化** Optimization | FSD 全應力尺寸設計、BESO 拓樸最佳化 + 選配抗倒塌約束 | 10-bar truss 文獻最優 · 應變能 ~4e-14 |
| **生態橋接** Ecosystem | `frame_cli` 文字橋、daemon 模式、C API DLL、Grasshopper C# client | bit-identical(CLI==DLL==daemon) |

## 為什麼可以信它 · Why you can trust it

這才是整個專案的重點。每個能力都錨定一個**獨立** oracle,不是自己跑兩次比自己:
解析解、(用前先獨立重推的)已發表基準、OpenSees 交叉驗證、gate 內一顆獨立的稠密 solver、
每個選配功能的**位元等價 no-op 證明**、以及旋轉等變性檢查。完整證據鏈在
**[`docs/VERIFICATION.md`](../../docs/VERIFICATION.md)**。幾個亮點:

*Every capability is anchored to an independent oracle — not a self-consistent re-run.
Highlights:*

- 🎯 MITC4 殼 vs OpenSees **自家 `ShellMITC4`**:~1e-10(平/斜面)、~1e-7–1e-8(歪斜+翹曲)。
- 🎯 3-D 共旋 vs OpenSees `geomTransf Corotational`:**1.22e-9**;弧長極限載重 vs `integrator ArcLength`:6.4e-3。
- 🎯 每個增量法(ReSolve / P-Delta 凍結路徑 / 張力構件)都對 fresh 重分解參考 ~1e-12 或更好。
- 🎯 量測一致性與 gate 容差**分開報告**(gate 容差刻意放寬,給浮點/函式庫留餘裕)。

> **誠實定位**:OpenSees 是*參考*,不是競爭對手。FrameCore 的利基是**可嵌入、引擎無關、
> POD API、互動式分解重用、把碎塊交給物理引擎**。S1–S10 線是對著
> [Karamba3D benchmarking roadmap](../../docs/KARAMBA3D_ROADMAP.md) 開發的:有些對齊它
> (二階分析、尺寸/拓樸最佳化、參數化 CAD 橋接),有些走在它文件功能之外(倒塌動力學、
> 互動再分析),而它的某些強項(EC3 設計檢核、成熟的 Grasshopper 生態)我們**明確不宣稱**。

## 誠實的範圍邊界 · Scope boundaries (it is honest about what it is *not*)

口試最加分的一段:這顆引擎很清楚自己**不是**什麼。摘要如下,完整逐項(更細)見
[完整技術參考](../../docs/FrameCore_full.md)與每份 `docs/PROGRESS_S*.md` 結尾。

*The most defensible part of the project — it knows its own limits. Summary; full per-stage
lists in the reference doc.*

- **D/C 是彈性/容許應力篩**,不是 RC 極限強度或法規設計檢核。
- **MITC4 殼是平面四節點 facet**:曲面靠網格細化收斂(基準有報數字:N=16/90° ≈ 2% 容差);
  選配 QM6/DKQ 分別救膜/薄板。
- **動力、屈曲、反應譜都是線性**;P-Delta 是 Theory-II 線性化(大變形屬共旋驅動)。
- **共旋驅動是梁 + 選配 EICR 殼,小應變/大轉動**;殼弧長後屈曲與解析切線是後續階段。
- **倒塌驅動是 LSP 級逐次線性分析**(事件間線性,文獻定 LSP 約 ±30%,結果偏保守);
  鉸是事件式、無卸載、零鉸長;**刻意不做纖維斷面 / pushover**。
- **ReSolve Tier-1 公式精確但非位元等價**(~1e-12),Tier-2 是容差級,只有 Tier-3 構造正確。
- **FSD 僅對靜定結構是最優**(否則為啟發式不動點);**BESO 是啟發式**,不宣稱全域最優。

## 快速開始 · Quick start

**秒級 standalone gate** —— 編譯 FrameCore + oracle fixtures 並跑:
```bat
Standalone\build.bat
```
預期看到一串 `[PASS] Fn …`,然後 `ALL PASS (failures=0)`、exit 0。
(需要 Visual Studio C++ 工具鏈 + conda `framecore-direct` 環境的 OpenBLAS + METIS;
conda 不在 `%USERPROFILE%\anaconda3` 時設 `SUPERNODAL_CONDA=<conda-root>\envs\framecore-direct\Library`。)

**一鍵五腿 gate**(standalone + UE 自動化 + OpenSees + 深度稽核 + CLI):
```powershell
powershell -ExecutionPolicy Bypass -File ..\..\Scripts\run_gate.ps1 -RequireOpenSees
```

**不寫 C++ 也能試** —— 文字橋從 stdin 解一個模型(2 m 懸臂 + 1 kN 端載):
```bat
Standalone\build_cli.bat
(
  echo MAT 210000 80769 7850
  echo SEC 10000 8.333e6 8.333e6 1.406e7 50 50 8333 8333
  echo NODE 0 0 0 0  1 1 1 1 1 1
  echo NODE 1 2000 0 0  0 0 0 0 0 0
  echo MEMBER 0 0 1 0 0  0 0 1
  echo NLOAD 1 0 0 -1000 0 0 0
  echo END
) | Standalone\frame_cli.exe
```
節點 1 的 `DISP` 列會回報 `-PL³/3EI` 的端點撓度。完整協定見
[`docs/CLI_PROTOCOL.md`](../../docs/CLI_PROTOCOL.md)。

## 最小 C++ 用法 · Minimal C++ usage

```cpp
#include "FrameCore/FrameSolver.h"
#include "FrameCore/ElasticAllowable.h"
using namespace frame;

Material mat(210000.0, 80769.0, 7850.0);            // E, G (MPa), rho
mat.cap = Capacity::make(300.0, 300.0, 180.0);      // allowable comp/tens/shear (MPa)
Section sec = Section::Rectangular(100.0, 100.0);   // b, d (mm)

FrameModel m;
m.materials = { mat };  m.sections = { sec };       // material index 0, section index 0
Node n0(0, 0,0,0);  n0.fixAll();                    // encastre base
Node n1(1, 2000,0,0);                               // 2 m cantilever
m.nodes = { n0, n1 };
m.members = { Member(0, 0, 1, 0, 0) };              // matIdx = 0, secIdx = 0
NodalLoad p;  p.node = 1;  p.comp[Uz] = -1000.0;    // 1 kN tip load
m.nodalLoads = { p };

SolveResult r = solve(m);                           // SolveOptions optional
if (!r.singular) {
    double tip = r.disp(1, Uz);                     // = -PL^3/3EI
    DemandResult d = ElasticAllowable{}.checkSection(r.memberForces[0].endI, sec, mat.cap);
    // d.risk (D/C), d.mode (governing failure mode)
}
```

> 構件以**索引**參照材料/斷面(`matIdx`/`secIdx`),`validate()` 會做範圍檢查 ——
> 新增節點/構件永遠不會讓它們變成 dangling 參照。

## 目錄結構 · Layout

```
Source/FrameCore/          引擎本體(純 C++17 + Eigen,UE-agnostic)
  Public/FrameCore/*.h      POD-only 公開 API(model / solver / analyses /
                            collapse / reanalysis / corotational / optimization)
  Private/*.cpp             實作(+ Private/FrameEigen.h:唯一的 Eigen include 點,雙編譯防護)
  Private/Tests/*.cpp       FrameCore.* UE 自動化測試(oracle 鏡像)
Source/FrameCoreUE/         v3.2.0+ 消費端 BP/USTRUCT reflection module
  Private/Tests/*.cpp       FrameCore.UE.* UE 自動化測試
                            UE gate 合計 135(含 cuDSS)/ 133(不含),v4.0.0 凍結
Standalone/                 console gate + CLI / C-API 驅動(見 Standalone/README.md)
Grasshopper/                文字橋的 C# 參考 client
```

公開邊界完全乾淨:`Public/` 只有 POD/std 型別,Eigen 全部關在 `Private/FrameEigen.h`
後面。所有 Eigen 型別藏在 `PreparedSystem` 的 PIMPL 之內,呼叫端永遠不會被傳染到 Eigen 標頭。

## 驗證 · Verification

```bat
Standalone\build.bat                    :: 秒級;ALL PASS (failures=0)
```
```powershell
powershell -ExecutionPolicy Bypass -File ..\..\Scripts\run_gate.ps1 -RequireOpenSees
```
CI 用 `-RequireOpenSees`,讓缺 OpenSeesPy 不會靜默跳過外部比對。能力 → oracle → gate-fixture
的證據對照表在 [`docs/VERIFICATION.md`](../../docs/VERIFICATION.md)。

> GitHub CI(`release-gate.yml`)目前跑 **CPU 腿**(standalone / 104-check 深度稽核 /
> CLI round-trip / v2 dispatcher),並上傳 gate log 作為可稽核證據;**GPU(cuDSS)與
> UE 自動化腿在整合者主機上手動執行**(需 GPU + UE 5.7)。

## 授權 · License

**MIT License**(見 [`LICENSE`](../../LICENSE))。預設 solver 只相依 **Eigen**
(MPL-2.0,header-only,以 `EIGEN_MPL2_ONLY` 排除 LGPL 模組)。選配 supernodal lane
(`FRAMECORE_SUPERNODAL=1`)另外連結 **OpenBLAS**(BSD-3)與 **METIS**(Apache-2.0),
完整第三方授權收錄於 [`third_party/NOTICE.md`](../../third_party/NOTICE.md)。
**OpenSees 僅用於離線驗證,不被散布或連結進引擎。**

---

<div align="center">

姊妹引擎 / sister engine:[📏 LevelSim 水準儀模擬器](../LevelSim/README.md)
· 回到 [🏛️ 專案門面 / project landing](../../README.md)

</div>
