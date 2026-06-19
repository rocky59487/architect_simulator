# Codex 任務:FrameCore × OpenSees 全方位對標基準

> **[HISTORY — 寫於 v2.1.0 cycle,留作史料]** — 此提示詞用於 v2.1.0 之後啟動 OpenSees mega benchmark
> harness 的 Codex session;benchmark 隨 v2.3(2026-06-19)整入 repo,結果在 `benchmarks/opensees_mega/results/`
> (見 [`docs/VERIFICATION.md`](VERIFICATION.md) §3.7a)。提示詞中 `當前正式版 v2.1.0` 等版本標記是寫作當下的狀態,**現況以根 [`README.md`](../README.md) 為準**。

> **任務性質**:大規模、多場景 benchmark — 對 FrameCore 引擎(我們自寫的 C++17/Eigen 結構引擎)做**全面 OpenSees 對標**,涵蓋各式建築、曲面、不同厚度、多載重場景,**記錄所有可比較的數值**,找出 FrameCore 的精度損失、邊界、與未公開的缺點。
>
> 你(Codex)看不到本專案的對話歷史、memory、或 CLAUDE.md 全文,**本提示詞自包含**你需要的一切。

---

## 1. 專案速覽

**FrameCore** 是 `E:\project\ArchSim` 倉庫的核心結構引擎,純 C++17 + Eigen 3D 線彈性 + MITC4 平板殼,涵蓋:

- 線性套件 8 段(組合/包絡/影響線/沉陷/模態/屈曲/反應譜/即時動態)
- 崩塌 C 線(LSP 驅動器 + 連續動力 + 碎塊連通)
- Karamba3D 對標主線 **S1–S10 全完成**(ReSolve / N4 dynamic / P-Delta / tension-only / FSD / CLI / BESO / 殼 QM6+DKQ / co-rotational 大位移 / N-M 互動塑鉸)
- 自建 supernodal direct solver(R 線)+ Neumaier IR(v2.1.0)

當前正式版 **v2.1.0**(tag `4e660de`,2026-06-18)。五腿 gate 全綠:F1-F64 standalone / UE 57 / OpenSees strict / audit 104 / CLI round-trip。

**已知邊界(誠實清單)** — 你的工作不是把這些當缺點,而是**驗證它們確實只在預期程度**內,以及**找出沒被列出的精度損失**:

| 已知邊界 | 預期表現 |
|---|---|
| MITC4 是 **flat-facet** 殼 | 曲面誤差隨網格細化 O(1/N²) 收斂 |
| Drilling 自由度是 Hughes-Brezzi 數值處理 | 純 in-plane 場景 OK,旋轉場景可能有 noise |
| 動力/屈曲/反應譜全線性 | 大位移場景應該偏離 |
| 倒塌驅動器是 LSP 級 sequential linear(無膜/懸鏈) | 比文獻保守 ±30% |
| 塑鉸是 event-to-event(無卸載/反轉) | 單調載重 OK,反覆載重不算 |
| 殼篩只查表面 von Mises | 不是 RC 極限 |
| **無曲面殼 K_σ 屈曲、無殼 co-rotational**(v3 在另一 branch 未 push) | 殼後屈曲場景應該明顯偏離 |
| 無雙軸 N-M 耦合塑鉸 | S10 只有單軸 |

---

## 2. 環境與位置

| 項目 | 路徑 / 命令 |
|---|---|
| Repo 根 | `E:\project\ArchSim`(已 `cd` 進去) |
| 引擎 CLI | `Plugins\FrameSolver\Standalone\frame_cli.exe` |
| Build CLI | `Plugins\FrameSolver\Standalone\build_cli.bat` |
| Build 閘門 | `Plugins\FrameSolver\Standalone\build.bat`(F1-F64,**改動後務必跑**) |
| 五腿閘門 | `powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees` |
| CLI 協議文檔 | `docs\CLI_PROTOCOL.md`(**權威**:N, mm, MPa 單位;token 格式) |
| 既有對標 harness | `Tools\opensees_compare.py`(只比參考結構,**擴充它**) |
| 工程文檔 | `ArchSim\README.md`、`docs\ARCHITECTURE.md` |
| OpenSees | `openseespy`(pip 安裝;先 `python -c "import openseespy.opensees as ops"` 驗證) |

**Shell**:PowerShell 為主,Bash(git-bash)也可用,各取自身語法。**MSVC 18 preview**(`vcvarsall` 由 build.bat 自動 call;`cl` 不在 PATH 是正常)。

**單位約定:N, mm, MPa**(整個專案;OpenSees 端必須對齊,否則差距全是 unit mismatch)。

---

## 3. 任務目標

對下列**矩陣**中每一個 case:

1. 用 J1 token(`docs\CLI_PROTOCOL.md` 是權威)建 FrameCore 模型,跑 `frame_cli.exe` 收集**所有可輸出的數值**(`DISP` / `RF` / `MF` / `SF` / `FREQ`)
2. 在 `openseespy` 端建**等效模型**(相同節點、相同截面、相同材料、相同邊界、相同載重),跑出**對應數值**
3. **逐項對比**,計算誤差,寫入結構化 CSV
4. 識別超出容差的項目,分級為:
   - `CRITICAL`(正確性錯,差距 > 50% 或符號錯)
   - `MAJOR`(精度差 5%-50%,或單調背離)
   - `MINOR`(< 5%,不影響工程使用)
   - `KNOWN`(已落入第 1 節已知邊界,符合預期)
5. 對每一類 MAJOR/CRITICAL,**追蹤根因**:建模差異(可修)vs 引擎演算法缺陷(留作後續路線)

**最重要**:**不要為了好看而隱瞞或修飾差距**。誠實匯報比過 gate 重要。

---

## 4. Benchmark 矩陣

### A. 一般建築(梁柱 + 部分殼)

| ID | 名稱 | 規模(估) | 重點 |
|---|---|---|---|
| A1 | 單層單跨剛性框架(門式) | 4 節點、3 桿 | 基準 sanity |
| A2 | 多層多跨剛性框架 | 10F × 4 跨 × 3 開間,~200 桿 | 高層平面框架對標 |
| A3 | 高層框架-剪力牆 | 20F + 中央核心殼,~1000 DOF | 殼+梁柱混合對標 |
| A4 | 高層筒中筒 | 30F 外殼+內殼,~3000 DOF | 純殼為主 |
| A5 | 大跨度 Pratt 桁架 | 30m 跨,單榀 | 桁架軸力對標 |
| A6 | 雙向桁架屋頂 | 30 × 40m 網架 | 3D 桁架剛度組合 |
| A7 | 平板無樑樓蓋 | 8×8m 殼 + 4 柱 | 殼-梁界面對標 |
| A8 | 板柱+柱托+邊樑 | 含柱頭加厚 | 厚度突變 |

### B. 橋梁

| ID | 名稱 | 重點 |
|---|---|---|
| B1 | 簡支梁橋(20m) | 梁元基準 |
| B2 | 連續梁橋(3×20m) | 連續性效應 |
| B3 | 桁架橋 | 重複 A5 但更長 |
| B4 | 雙曲拱橋(30m 跨,矢高 5m) | 梁元曲率近似(分段直梁) |

### C. 曲面殼結構 ★ 此區是找缺點重點

| ID | 名稱 | 厚度比 | 重點 |
|---|---|---|---|
| C1 | 球殼穹頂(R=30m,1/4 對稱) | t/R = 1/100 / 1/200 / 1/500 | flat-facet 對曲率的收斂 |
| C2 | 雙曲拋物面(馬鞍) | t/L = 1/100 / 1/50 | warped quad(非平面 4 點) |
| C3 | 圓筒殼(R=10m,L=20m,軸向開口) | t/R = 1/100 / 1/50 | pinched cylinder 典型 benchmark |
| C4 | Scordelis-Lo 屋頂 | t/L = 1/100 | 經典殼 benchmark |
| C5 | 自由曲面網格殼(隨意 NURBS 採樣) | t/L 變化 | warped 嚴重、混合曲率 |

每個 C 案至少做 **3 種網格細度**(粗 / 中 / 細;例如 8×8 / 16×16 / 32×32),畫收斂曲線。

### D. 特殊載重 / 邊界

| ID | 名稱 | 重點 |
|---|---|---|
| D1 | 預應力梁(高 N + 彎矩耦合) | 軸力影響剛度,測試 P-Delta 開關 |
| D2 | 含釋放的桁架/框架 | `MEMBER … release` 與 OpenSees `releaseY/Z` 對齊 |
| D3 | 沉陷支座(prescribed displacement) | `NODE` 的 `pUx..pRz` 對 OpenSees `sp` |
| D4 | 自重 + 預應力 + 不對稱活載 | 載重組合 |

### 載重場景(每個結構至少跑 L1+L2+L3)

| ID | 載重 |
|---|---|
| L1 | 自重(質量 × g,等效 NLOAD 或 UDL) |
| L2 | 均布活載(UDL / SPRESS) |
| L3 | 集中點載(NLOAD) |
| L4 | 不對稱載 |
| L5 | 水平風載(節點力) |
| L6 | 模態分析(`EIGEN n`),前 10 階 |
| L7 | 線性屈曲(opt-in,前 5 階)— **僅對能跑的**,殼 K_σ 是 v3 未 push 部分,先不測 |

**規模上限指引**:每 case 控制在 100 ms 內 frame_cli 解完(< 5000 DOF 通常 OK)。大規模建築用 supernodal lane(若你想試)是 opt-in;default LDLT 走得動就走 default,避免引入 lane 切換干擾。

---

## 5. 工作流程(每個 case)

```
for case_id in matrix:
    1. Python 生 J1 token 字串(model = build_case(case_id))
    2. 寫 inputs/<case_id>.tok(留檔重現)
    3. echo inputs/<case_id>.tok | frame_cli.exe > outputs/fc_<case_id>.txt
    4. 解析 fc_<case_id>.txt → fc_disp, fc_rf, fc_mf, fc_sf, fc_freq
    5. ops 端用 openseespy 重建等效模型 → os_disp, os_rf, os_mf, os_sf, os_freq
    6. 逐節點/逐桿/逐殼/逐模態算誤差:
         abs_err = |fc - os|
         rel_err = |fc - os| / max(|os|, eps_scale)
    7. 寫入 results/<run_id>/matrix.csv 一行(每 case × 每量值類別 × max/mean/p99)
    8. 標等級(CRITICAL / MAJOR / MINOR / KNOWN)
```

### 等效建模對照表(關鍵)

| FrameCore token | OpenSees 對應 |
|---|---|
| `MAT E G rho` | `nDMaterial ElasticIsotropic` 或 `uniaxialMaterial Elastic` |
| `SEC A Iy Iz J cy cz Asy Asz` | `section Elastic` |
| `MEMBER i j matIdx secIdx refx refy refz` | `geomTransf Linear $tag $vecxz` + `element elasticBeamColumn` |
| `SHELL n0 n1 n2 n3 matIdx t` | `element ShellMITC4` 或 `ShellDKGQ`(視 opt 旗標) |
| `NODE … fUx..fRz` 固定旗標 | `fix $node 1 1 1 1 1 1` |
| `NODE … pUx..pRz` 沉陷 | `sp $node $dof $val` |
| `NLOAD` | `load $node $Fx $Fy $Fz $Mx $My $Mz` |
| `UDL wx wy wz`(local) | `eleLoad -ele $tag -type -beamUniform $Wy $Wz $Wx` |
| `EIGEN n` | `eigen $n` 後讀 `eigenvalue` |
| `PDELTA path` | `geomTransf PDelta` |

⚠️ **本地軸與 vecxz**:FrameCore `refVec` 慣例 `z=x×ref, y=z×x`(`docs\CLI_PROTOCOL.md` 與既有 `opensees_compare.py` 第一部分有範例)。OpenSees `geomTransf` 的 `vecxz` 是不同定義 — 必須仔細對齊,**否則 Iy/Iz 對換、彎矩符號錯,精度差會被誤判為引擎缺點**。對齊方法:用 **方形截面 Iy==Iz** 做 sanity case,確認 zero rotation 後再用矩形(這也是既有 harness 的做法)。

⚠️ **MITC4 元素旗標**:`OPT useIncompatibleMembrane useDKQPlate` 兩個 S8 旗標可選 — 預設 off 走標準 MITC4。OpenSees 對應 `ShellMITC4`。若你打開了 QM6/DKQ,OpenSees 端要換對應元素或記為「無對應(只能對自己標)」。

⚠️ **OpenSees 自身限制**:OpenSees `eigen` 在小 / singular 系統會吐負特徵值或 nan,要 try-except。它的 `ShellMITC4` 大型場景可能慢得很 — 規模上限要試。

### 容差建議(初稿,實測後調整)

| 量值 | 線彈性梁柱 | 殼結構 | 大跨曲面 |
|---|---|---|---|
| 位移 rel | 1e-9 | 1e-6 | 1e-3(細網格)|
| 軸力/彎矩 rel | 1e-9 | 1e-4 | 1e-2 |
| 反力 rel | 1e-9 | 1e-6 | 1e-3 |
| 模態頻率 rel | 1e-6 | 1e-3 | 1e-2 |

超出容差 → 等級判定走 §3 規則。**容差不要為了過而調寬;為差距找原因,不為過 gate 調容差**。

---

## 6. 交付物

新分支 `feature/benchmark-opensees-mega-<YYYYMMDD>`,**不 push 不開 PR**,等使用者授權。

```
benchmarks/opensees_mega/
├── README.md                    # 怎麼重跑、矩陣概覽、找報告位置
├── harness/
│   ├── builders/                # 每個 case 一個 build_<id>.py
│   │   ├── a1_portal.py
│   │   ├── ...
│   │   └── c5_freeform.py
│   ├── compare.py               # 核心 harness(擴充 Tools/opensees_compare.py 的模式)
│   ├── opensees_wrap.py         # ops 等效建模工具
│   └── tolerances.py            # 容差表(可調)
├── inputs/                      # 每 case 一份 frame_cli token .tok 留檔
├── outputs/
│   ├── frame_core/              # frame_cli stdout 原始
│   └── opensees/                # ops 數值原始(json/csv)
├── results/
│   └── <run_id>/                # 例如 20260618-001
│       ├── matrix.csv           # 每 case × 每量值類別 × 統計(max/mean/p99 abs/rel)
│       ├── findings.json        # 等級分類後的 finding list
│       ├── plots/               # 殼收斂曲線、誤差 heatmap 等
│       └── report.md            # **核心報告:本輪所有發現**
└── rerun.ps1                    # 一鍵重跑全套
```

### `report.md` 必含小節

1. **執行摘要**(< 200 字):總 N 個 case、發現 X 個 CRITICAL / Y 個 MAJOR / Z 個 MINOR / W 個 KNOWN
2. **CRITICAL 清單**:每項一段 — 現象、最小重現 case、根因假設、建議修補方向
3. **MAJOR 清單**:同上
4. **MINOR + KNOWN**:表格摘要即可
5. **每結構類型小結**(A/B/C/D):平均誤差、最差 case、是否符合已知邊界
6. **殼收斂曲線**(C 區):每個曲面 case 的 h-refinement 收斂率 vs O(1/N²) 理論
7. **與已知邊界的對照**:第 1 節清單 vs 實測,哪些更差/更好/符合
8. **後續路線建議**(誠實,不為自誇):這輪結果指向哪些引擎升級值得做(例如「C3 pinched cylinder 細網格仍誤差 5% → 強化 v3 殼 CR 收割」)

### `matrix.csv` schema

```
case_id, struct_type, mesh, thickness, load_id,
fc_disp_max, os_disp_max, disp_abs_max, disp_rel_max, disp_rel_p99,
fc_axial_max, os_axial_max, axial_rel_max,
fc_moment_max, os_moment_max, moment_rel_max,
fc_freq1, os_freq1, freq1_rel, ... freq10_rel,
fc_runtime_ms, os_runtime_ms,
verdict     # KNOWN | MINOR | MAJOR | CRITICAL
notes
```

---

## 7. 鐵則(不可違反)

1. **不改 default**:任何引擎 opt-in flag 你都不要動 default;benchmark 用 opt-in 的場景明確標記
2. **不破五腿 gate**:這輪工作期間隨時跑 `Plugins\FrameSolver\Standalone\build.bat`(F1-F64)確認沒被弄壞;**改了引擎源碼必跑**(雖然原則上 benchmark 不該改引擎)
3. **不 push 不開 PR 不動 main**:全部留在 `feature/benchmark-opensees-mega-<YYYYMMDD>` 本地分支
4. **顯式 `git add` 個別檔案**,絕不 `git add -A`;絕不碰 `.gitignore`、`ArchSim.uproject`、`Plugins/LevelSim/`
5. **單位 N, mm, MPa**:兩端必須一致;每個 builder 開頭寫 docstring 標單位
6. **OpenSees 模型要附參考**:每個 case 的 builder docstring 寫出「對應教科書/文獻/手算 oracle」(像 C4 Scordelis-Lo 就寫文獻名)
7. **誠實分級**:不要把 MAJOR 寫成 MINOR、不要把建模差異說成引擎缺點;區分清楚
8. **失敗 case 也要記**:builder 寫不出來、OpenSees 跑不動、誤差爆表 — 都寫進 `findings.json`,**不要靜悄悄跳過**
9. **不引新第三方依賴**(numpy / scipy / openseespy 已有);不需要 matplotlib 就不用,要用就只寫到 plots/

---

## 8. 階段建議(P1 → P4)

### P1:harness 骨幹(必要)
- 寫 `harness/compare.py`(支援 batch 多 case + 統一 CSV 輸出)
- 寫 `harness/opensees_wrap.py`(MAT/SEC/MEMBER/SHELL/NLOAD/UDL/EIGEN 對應的 ops 包裝)
- 跑 A1(門式框架)端到端,確認 `disp_rel_max < 1e-9`(這是 sanity,不通過就 harness 有 bug)
- **產出**:`results/<run_id>/` 含一筆 A1 數據

### P2:純線彈性 A+B(梁柱基準)
- A1-A8、B1-B4 全跑
- 預期:全部 MINOR 以下(線彈性梁柱兩端應該 ~1e-9)
- 任何 MAJOR 都是 harness 建模對齊問題,要先修

### P3:殼結構 C 區(找缺點重點)
- C1-C5 × 3 種網格細度
- 收斂曲線:細網格往 O(1/N²) 收斂 = 符合 flat-facet 預期
- 不收斂 / 慢於 O(1/N²) / 大誤差不消 = MAJOR finding
- 厚度敏感度:t/L 變化是否導致誤差暴增?(剪鎖測試)

### P4:特殊 D + 動力 L6/L7
- D1-D4
- 全結構跑 EIGEN 前 10 階,模態頻率對比
- 線性屈曲(opt-in 的部分)— 注意殼 K_σ 是 v3 未 push 部分,先不測

每階段結束**寫一份小 report.md**,確認方向再繼續。

---

## 9. 已知踩雷(durable)

1. **Edit 顯示成功 ≠ 真落地**:改檔案後務必 `git status` + `grep` 核對真的改了。動工/commit 前都確認。
2. **單位混淆**:OpenSees 教學常用 m+kPa,FrameCore 強制 N+mm+MPa。每個 ops builder 開頭轉換清楚。
3. **`refVec` ↔ `vecxz` 不等價**:見 §5 對照表底下的 ⚠️ 段。先用方形截面 sanity case。
4. **OpenSees `eigen` 偶有負/nan**:try-except + 標 `freq_rel = NaN`,不要靜默回 0。
5. **frame_cli stdin 結尾必須 `END`**(daemon 模式還可以加 EOR,你不需要 daemon)。
6. **stderr 有 provenance(`# frame_cli | build <sha> | …`)**:解析只讀 stdout,別讀 stderr 進 numeric。
7. **規模上限**:OpenSees ShellMITC4 在 10k+ 殼 case 可能極慢,先小網格驗證 harness,再加密。
8. **build.bat 是顯式源碼清單**:你**不該**碰 build.bat;**也不該**改引擎源碼(本任務是 benchmark,不是改引擎)。

---

## 10. 開工檢查清單

在動矩陣之前先跑這幾步,確認環境 OK:

```powershell
# 1. 確認 frame_cli 可跑
.\Plugins\FrameSolver\Standalone\frame_cli.exe < .\Plugins\FrameSolver\Standalone\tests\smoke.tok
# 若無 smoke.tok 就手寫一個極簡 cantilever 餵進去

# 2. 確認 openseespy
python -c "import openseespy.opensees as ops; print(ops.getTime())"

# 3. 確認既有 harness 跑得起來
python Tools\opensees_compare.py

# 4. 確認 standalone gate 過(這是基線,動之前要綠)
Plugins\FrameSolver\Standalone\build.bat
```

四個都過再開新分支動工:

```bash
git checkout -b feature/benchmark-opensees-mega-20260618
mkdir benchmarks/opensees_mega
# 開始 §6 的目錄結構
```

---

## 11. 完成定義(Done = 全部以下成立)

- [ ] 矩陣 A1-A8、B1-B4、C1-C5(× 3 網格)、D1-D4 全跑
- [ ] 每 case L1+L2+L3 + 至少 1 個 L4-L7 場景
- [ ] `results/<run_id>/matrix.csv` 完整(每行一個 case × load,fc/os/rel 完整)
- [ ] `results/<run_id>/findings.json` 含所有 CRITICAL/MAJOR,每項有最小重現 case
- [ ] `results/<run_id>/report.md` 完整含 §6 八小節
- [ ] 殼收斂曲線 plot 存在(C 區每個 case)
- [ ] `rerun.ps1` 從零重跑一次可達相同結果(deterministic)
- [ ] standalone gate 仍綠(`build.bat ALL PASS`)
- [ ] 分支 commit 整齊,顯式 `git add`,**未 push**
- [ ] 在 `benchmarks/opensees_mega/README.md` 寫清楚怎麼讀報告、怎麼重跑、矩陣概覽

完工後在 chat 把 `report.md` 的執行摘要(<200 字)貼出來,等使用者授權後續(push / 修引擎 / 加 case)。

**禁止**:報告寫「全綠通過,沒發現問題」— 這個矩陣是設計來找缺點的,**完全無發現本身就是可疑的**,請重審容差與建模對齊。
