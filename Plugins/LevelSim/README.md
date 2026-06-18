# LevelSim — 水準儀模擬器(測量核心 + 可玩 PC 單站垂直切片)

「建築師模擬器」畢專底下、**與 FrameSolver/FrameCore 完全獨立**的測量教學關卡
(對標測量丙級術科 04200 高程站)。設計全文:`E:\project\水準儀模擬器_設計文件.md`。

```
Plugins/LevelSim/
├─ Core/                  純 C++17 測量核心(零 UE、零 Eigen;4 輪對抗式審核收斂)
│   ├─ LevelCore.h        公開 API(POD/std 型別)
│   └─ LevelCore.cpp      真值與誤差物理 + 驗證 + 多站閉合/平差
├─ Standalone/
│   ├─ level_gate.cpp     oracle gate(L1..L16,115 asserts)
│   └─ build.bat          一鍵編譯+執行 → "ALL PASS (failures=0)"
├─ Source/LevelSimPlay/   UE 呈現/輸入層(可玩 MVP;unity-include 核心,零複製)
│   └─ Private/
│       ├─ LevelSimGameMode.{h,cpp}   運行時生成場景(零內容資產)+ 煙霧測試序列
│       ├─ LevelSimPawn.{h,cpp}       玩法 FSM + 輸入 + 三相機 + 核心 glue
│       ├─ LevelSimHUD.{h,cpp}        十字絲/視距絲/氣泡特寫/記簿/評分(Canvas)
│       ├─ LevelStaffActor.{h,cpp}    E 字分劃標尺(幾何分劃,mm 判讀不吃 mip 模糊)
│       └─ LevelCoreUnit.cpp          核心 unity-include(單一 TU,免 export 宏)
├─ run_game.bat           ▶ 一鍵開玩(視窗化 1600x900)
└─ run_smoke.bat          headless 煙霧測試(自動截圖 4 張 + log 後退出)
```

## 怎麼玩(MVP = PC 單站垂直切片)

```
Plugins\LevelSim\run_game.bat
```

流程(對標視線高法 H.I. 作業):

1. **整平 [L]**:`[1][2][3]` 選腳螺旋、滾輪粗調、`Shift+滾輪` 微調;圓水準器特寫,
   氣泡**浮向高側**;進 0.1 格 = FINE LEVEL OK。
2. **望遠鏡 [T]**:滑鼠粗瞄 → `[C]` 制動 → 滾輪微動(`Shift` 更細)→ `[F]+滾輪` 對光
   (真實景深模糊,焦距不對就糊)。補償器超範圍(±15′)→ 影像不可用,回去重新整平。
3. **讀數 [Enter]**:鍵入中絲讀數(m,估讀到 mm)→ 先 BM 後視(BS)、再 P1 前視(FS)。
4. **記簿**:手動填 H.I. = BM + BS、P1 高程 = H.I. − FS。
5. **評分**:BS 30 + FS 30(±1mm 滿分、±3mm 之外 0,線性)+ 算術各 20;≥60 及格。
   `[R]` 重新設站(亂數擾動腳螺旋)。

## 真值紀律(誠實邊界)

- **所有會評分的數字都來自 `levelsim::` 核心**(`measure`/`bubbleFromTilt`/`sightTilt`/
  `scoreReading`);UE 層只做單位換算(1 uu = 1 cm)、渲染、輸入。
- **望遠鏡相機 pitch 鎖定 = 核心 `sightTilt().losTiltRad`**(補償器決定視線,玩家不能俯仰)
  → 玩家在畫面上讀到的就是 `measure()` 預測的值(煙霧截圖驗證十字絲落在核心預測的 dm 帶)。
- 折入真值的誤差源(設計 §12):補償器殘餘安平誤差(±0.4″,對腳螺旋組態決定性)+
  視準軸 i 角(+10″ → 10 m 處 0.48 mm)。**視差 = 讀數抖動 stub**(十字絲晃動 + 真 DOF 模糊),
  不折入真值。標尺永遠鉛直。
- 已知簡化(MVP,誠實標註):HUD 為英文(引擎字型無 CJK 字模,中文字型屬後續呈現層工作);
  望遠鏡遮罩為方窗非圓窗;粗平(腳架腿)未做,只做腳螺旋精平;只中絲(上下絲為 HUD 視距絲線,
  不評分);單站不含轉點/閉合(核心 `closeLoop` 已備,屬 M5/M6)。

## 驗證

1. **核心 gate(秒級)**:`Standalone\build.bat` → `ALL PASS (failures=0)`(L1..L16/115 asserts;
   4 輪對抗式審核 R1=27→R2=14→R3=5→R4=0 收斂)。
2. **煙霧測試**:`run_smoke.bat` → 自動 全景→氣泡(已知擾動)→望遠鏡 BM→望遠鏡 P1 截圖
   (`Saved\Screenshots\`)+ `[LevelSimSmoke]` log(氣泡格數、真值讀數、評分路徑、
   dH→高程 vs 場景真值 100.3700)後自動退出。
3. UE 編譯隨 `ArchSimEditor` target;FrameCore 四腿 gate 不受影響(零耦合,實測 standalone/UE/OpenSees/audit=63 全綠)。
4. **像素級 oracle**:`Tools\verify_smoke_shots.py` 從截圖反推十字絲落點讀數,與核心 `measure()` 真值比對
   (BM 0.04mm、P1 0.24mm,容差 ±2mm)→ 證明「玩家所見 == 核心所算」,不靠目視。

## glue 層審核(2026-06-10)

UE 呈現/輸入層經一輪對抗式審核(核心已是 4 輪收斂、不在此輪範圍)。確認並修正:
- **(major)** 望遠鏡相 Enter+Escape 同幀 → `bTyping=true` 卻轉到 Overview,殘留 typing 狀態無相位可服務 → **永久卡死**。
  修:Enter/Escape 改 else-if + `EnterPhase()` 進非 Booking 相位一律清 `bTyping`/`TypeBuf`(雙保險,擋未來新增轉換)。
- **(minor)** 記簿相 ESC 被靜默吞掉(玩家在望遠鏡學到「ESC 可取消」,記簿時 ESC 無反應也無提示)。
  修:記簿 ESC 改顯示「Field book must be completed」提示。
- 自查零發現項(實證):相機 pitch 符號(像素 0.04mm 吻合,反置會差 ~0.97mm)、`FStaffTarget::Actor` GC
  (世界持有參照、spawn 後不解參考)、pawn 原點生成(全景幾何正確)、README 宣稱對得上碼。

> 範圍與基調由 2026-06-08 採訪定案(PC 先、偏寫實、教學/考試遞進);核心審核紀錄與
> oracle 清單見 `Standalone/level_gate.cpp` 頂註與設計文件 §7/§12。
