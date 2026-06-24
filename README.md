<div align="center">

# 🏛️ 建築師模擬器 · Architect Simulator

**一個畢業專題,兩顆從零寫起、各自對標獨立 oracle 的引擎。**
*A graduation project — two from-scratch engines, each anchored to an independent oracle.*

蓋一棟結構、把它算到極限、再讓它在你眼前倒下;
走進工地、整平水準儀、讀出毫米級的高程。

*Build a structure and push it to collapse — then prove the physics.
Step onto the site, level the instrument, read the millimetre.*

</div>

---

> ### 🚧 目前狀態 · Project status — 2026-06
>
> **兩顆引擎都已完成並通過驗證**(FrameCore 結構引擎 `v4.0.0` 凍結;LevelSim 水準儀核心
> 4 輪對抗式審核收斂)。**統整兩者的視覺呈現層(「表現法」)尚未產出** —— 所以這一頁
> 目前是專案門面的**佔位版**,先把入口擺好,等展示影片 / 截圖 / 互動 demo 完成後再上。
>
> *Both engines are done and verified. The unified visual showcase layer is not built
> yet, so this landing page is an intentional **placeholder** — entry points first,
> hero media later.*

<div align="center">

<!--
  TODO 表現法 / hero media:
    - 倒塌重播 (collapse replay) 動畫 GIF
    - 利用率熱圖 (utilization heat-map) 截圖
    - LevelSim 望遠鏡讀數畫面
  產出後把下面這個佔位區換成實際圖片 / 影片連結。
-->

`┌─────────────────────────────────────────────┐`
`│   🎬  展示影片 / 截圖即將登場                  │`
`│       hero showcase coming soon              │`
`└─────────────────────────────────────────────┘`

</div>

---

## 兩顆引擎 · The two engines

這個 repo 收錄兩顆**完全獨立、零共用程式碼**的引擎,各自可獨立 build / test / release。
*Two fully independent engines — no shared code, each builds, tests and ships on its own.*

### 📐 FrameCore — 結構力學引擎 / structural FEM engine

> 一顆自給自足的 C++17 + Eigen 三維有限元引擎:梁柱、MITC4 殼、完整線性分析、
> 漸進式與動態倒塌、增量再分析、二階與大變形(共旋)、張力構件、塑性鉸、尺寸與拓樸最佳化,
> 外加給 Grasshopper 用的文字 / C-API 橋接。
>
> **賣點**:它印出來的**每一個數字**都對得上一個獨立 oracle —— 解析解、已發表基準、
> 以及 OpenSees 交叉驗證。MITC4 殼對 OpenSees 自家 `ShellMITC4` 達 ~1e-10;3-D 共旋對
> `geomTransf Corotational` 達 1.22e-9。
>
> *A self-contained 3-D FEM engine where every printed number is checked against an
> independent oracle (closed-form, published benchmarks, OpenSees).*

**→ [閱讀 FrameCore README / Read the FrameCore README](Plugins/FrameSolver/README.md)**
· [完整技術文件 / full reference](docs/FrameCore_full.md)
· [驗證證據鏈 / verification](docs/VERIFICATION.md)

### 📏 LevelSim — 水準儀模擬器 / surveying-level simulator

> 一個可玩的測量教學關卡(對標測量丙級術科 04200 高程站):整平腳螺旋、瞄準制動微動、
> 對光景深、估讀到毫米、填手簿、評分。
>
> **賣點**:**所有會評分的數字都來自純 C++ 測量核心** `levelsim::`(補償器殘餘誤差、
> 視準軸 i 角都折入真值);UE 只負責渲染與輸入。像素級 oracle 從截圖反推十字絲讀數,
> 證明「玩家所見 == 核心所算」(BM 0.04mm / P1 0.24mm)。
>
> *A playable surveying-level training level — all scored numbers come from a pure C++
> core; a pixel-level oracle proves "what the player sees == what the core computes".*

**→ [閱讀 LevelSim README / Read the LevelSim README](Plugins/LevelSim/README.md)**

---

## 這個專案到底是什麼 · What this project is

「建築師模擬器」的主軸不是做一個漂亮的遊戲外殼,而是**先把底層的工程數值做對、做誠實、
做到可被獨立驗證**,再往上長互動與呈現。所以每顆引擎的核心都遵守同一條紀律:

> **凡是會被相信的數字,都必須對得上一個它自己沒參與計算的 oracle。**
> *Every number anyone might trust must match an oracle that didn't help compute it.*

*The point of this project is correctness-first, honesty-first engineering numerics that
can be independently verified — then build interactivity and presentation on top.*

| | FrameCore | LevelSim |
|---|---|---|
| 領域 / domain | 結構有限元 / structural FEM | 工程測量 / surveying |
| 核心語言 / core | 純 C++17(POD API,零 UE/Eigen 洩漏) | 純 C++17(POD API) |
| oracle | 解析解 · 已發表基準 · OpenSees | 真值物理模型 · 像素級截圖反推 |
| 狀態 / status | `v4.0.0` 引擎凍結 / engine frozen | 核心 4 輪審核收斂 + 可玩 MVP |
| 自動驗證 / gate | 5-leg gate(standalone/UE/OpenSees/audit/CLI) | `level_gate` 115 asserts + 煙霧截圖 oracle |

宿主是一個 Unreal Engine 5 專案(`ArchSim.uproject`),兩顆引擎都以 UE 外掛形式掛在底下。
*The host is a UE5 project (`ArchSim.uproject`); both engines plug in as UE modules.*

---

## 快速開始 · Quick start

```bat
:: FrameCore — 秒級結構引擎 gate(印出 ALL PASS）
Plugins\FrameSolver\Standalone\build.bat

:: LevelSim — 一鍵開玩(視窗化）
Plugins\LevelSim\run_game.bat
```

各引擎的完整建置 / 測試 / 玩法說明,請進各自的 README。
*Full build / test / play instructions live in each engine's README (linked above).*

---

## 文件地圖 · Documentation

| 文件 / doc | 內容 / what it is |
|---|---|
| [`Plugins/FrameSolver/README.md`](Plugins/FrameSolver/README.md) | FrameCore 門面 README(策展版)/ curated engine README |
| [`Plugins/LevelSim/README.md`](Plugins/LevelSim/README.md) | LevelSim 門面 README / surveying engine README |
| [`docs/FrameCore_full.md`](docs/FrameCore_full.md) | FrameCore 完整技術參考 + 發行歷史 / full reference + history |
| [`docs/VERIFICATION.md`](docs/VERIFICATION.md) | 能力 → oracle → gate fixture → 量測一致性 / the evidence chain |
| [`docs/README.md`](docs/README.md) | 完整 docs 索引 / full docs index |

---

## 授權 · License

採用 **MIT License**(見 [`LICENSE`](LICENSE))—— 畢業專題程式碼,開放重用與再散布。
第三方相依與其授權收錄於 [`third_party/NOTICE.md`](third_party/NOTICE.md)。
**OpenSees 僅用於離線驗證,不被散布或連結進引擎。**

*MIT-licensed graduation-project code. OpenSees is used for offline validation only —
never redistributed or linked into the engines.*
