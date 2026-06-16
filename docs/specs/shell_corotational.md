# 殼 co-rotational 大變形 — v3 曲面線第②項・階段 1（EICR）

> 一句話:opt-in **EICR(Element-Independent Co-Rotational)殼大變形** —— 把線性 MITC4 `kl_` 當黑箱,
> 外層套每 facet 的 co-rotational 框架追蹤大剛體轉動,讓含殼模型能做大位移幾何非線性(原 `runCorotational`
> 遇殼即 reject)。對標 `Research/V3_RESEARCH_HANDOFF.md` 證據 B1——殼大變形/後屈曲是 Zaha 薄殼最致命缺口。
> 本階段做**大轉動小應變 + NR 載重控制**;arc-length 殼後屈曲(snap-through)是階段 2。

## ① 目標 / 不做

**目標**:解鎖 `CorotationalAnalysis.cpp` 的殼 reject,以 EICR 接入 MITC4 殼。每個 facet 維護輕量
`CrShell24D`(類比梁的 `CrBeam3D`),核心 `crComputeShell24D` 在當前構形算內力 + 切線,接進既有 NR driver
(殼節點與梁節點共用 `model.nodes` 同一 6-DOF 池 + SO(3) spatial 更新,零 driver 結構改動)。

**不做(各有後續或刻意排除)**:
- **arc-length 殼後屈曲**:本階段 NR 載重控制;driver 已有 arc-length 路徑,殼接入留階段 2。
- **完整解析切線**:本階段材料切線 + driver FD 一致切線;解析 spin/moment 幾何項留後(只影響收斂速度)。
- **CR 殼力 recover**:本階段填 shellForces id-tag + 零(線性 recover 會把大轉動誤判為應變→錯應力);
  CR-consistent 殼應力 recover 留後。位移場 + 穩定性是 CR 主要輸出。
- **殼壓力 follower load**:殼壓力等效節點力用初始構形(非隨法向轉動);follower 留後。
- **大應變**:EICR 假設小應變(kl0 保持有效線性元素)。
- **warped facet 精度**:flat-facet O(1/N²) 曲面逼近不變(CR 只去剛體轉動,不修 faceting);warped 精度
  是曲面線第③項(Sabir-Lock,獨立 commit)。

## ② 公開 API

```cpp
// CorotationalAnalysis.h（opt-in,預設 false → beam-column only 逐位元同今天;不入 modelFingerprint）
bool shellCorotational = false;
```

## ③ 資料流 / ④ 演算法（EICR-simplified, flat-facet）

每個殼 facet,每次 NR 迭代(`crComputeShell24D`):
1. **當前 frame** `R_cr`(rows=局部軸):從當前 4 節點座標,**同 `MITC4ShellElement::prepare()` 的構造**
   (`n=(P2−P0)×(P3−P1)` 正規化、`e1` 沿 P1−P0 投影到平面、`e2=n×e1`),差別只在用當前座標。
2. **natural deformation**(扣除剛體運動 —— 旋轉不變性成敗關鍵):
   - 局部變形平移:`d_k = R_cr·(x_k − x_c) − R0·(X0_k − X0_c)`(x_c/X0_c=當前/初始 centroid;
     centroid 扣剛體平移、R_cr/R0 扣剛體轉動)。
   - 局部變形轉動:`θ_k = logSO3(R_cr · Rnode[k] · R0ᵀ)`(同梁 `crCompute3D` 的 `logSO3(Eᵀ·qI)` 模式)。
   - `u_local(24) = [d_k(3); θ_k(3)] × 4`
3. **局部力 + 轉回**:`f_local = kl0 · u_local`;`fe = blkdiag(R_cr)ᵀ · f_local`。
4. **材料切線**:`Ke = blkdiag(R_cr)ᵀ · kl0 · blkdiag(R_cr)`;spin/幾何項由 driver FD 一致切線
   (`opts.consistentTangent`)補。

**為何旋轉不變(代數證 + 數值證)**:純剛體運動下 `R_cr = R0·R_rigᵀ` → `d_k = 0`、`θ_k = 0`
→ `u_local = 0` → `fe = 0`(CR 定義性質)。F58b 實測旋轉不變到 **2.6e-14**(w/L≈0.69 大變形下)。

**driver 接入**:`runCorotational` 解鎖殼 reject(opt-in)→ 元素建立段借 `MITC4ShellElement::prepare()` 取
`localKForAudit()`(kl0)+ 新 `localFrameForAudit()`(R0)→ `assemble` lambda 梁迴圈後加殼迴圈(主路徑 +
FD 路徑都加)→ `recoverState` 填殼位移(`logSO3(Rnode)` 已涵蓋)。`applyInc`/`Rnode` SO(3) 更新零改動。

## ⑤ 檔案（全在既有檔,無新 .cpp → 免動三支 build.bat）

| 檔 | 改動 |
|---|---|
| `Private/CorotationalAnalysis.cpp` | `CrShell24D` struct + `crComputeShell24D`(匿名 ns)+ opt-in guard + 殼元素建立 + assemble/FD 殼迴圈 + recoverState 殼填充;include `MITC4ShellElement.h` |
| `Public/FrameCore/CorotationalAnalysis.h` | `CorotationalOptions::shellCorotational` |
| `Private/MITC4ShellElement.h` | `localFrameForAudit()` getter(回傳 R_) |
| `Standalone/main.cpp` | **F58/F59** oracle |
| `Private/Tests/CorotationalTest.cpp` | UE mirror `FrameCore.Corotational.ShellRotationInvariance` |

## ⑥ Oracle（誠實分級）

**EICR = `[VERIFIED]`**(Felippa-Haugen 文獻方法之 FrameCore 實作;`[NEW CODE]` 僅 opt-in 接線 + CrShell24D)。

- **F58a 小位移 == 線性**(`[VERIFIED]`):微小載重殼 CR == 線性 `solve()` → rel **3.8e-11**
  (CR 內力/切線在小變形精確退化到線性)。
- **F58b 任意軸旋轉不變**(機器精度,`[VERIFIED]`,仿梁 F51a):懸臂殼板整個模型+載重繞任意軸 SO(3) 轉,
  tip |u| 不變 → rel **2.6e-14**,且 w/L≈0.69(真大變形)。**驗 natural-deformation 剛體扣除正確**。
- **F59 大變形對 Mattiasson elastica**(外部解析 oracle,`[NEW CODE]`):細長 **nu=0 殼條**(板條彎曲剛度
  `D·W = E·W·t³/12 = 梁 EI`)的大變形 tip 路徑(dv/L, dh/L)收斂到 Bisshopp-Drucker / Mattiasson 射擊表
  (梁 F50/F51 同一獨立半解析 oracle)。實測 alpha=1/5/10 → dv rel **1.3e-4 ~ 7.1e-4**、dh rel
  **7e-4 ~ 2e-3**(alpha=10 時 dv/L=0.81 極大變形)。
- **UE mirror** `FrameCore.Corotational.ShellRotationInvariance`:F58 小位移==線性 + 旋轉不變。
- **預設不破**:`shellCorotational=false` → 殼 reject 同今天,梁 CR 全 oracle(F50-F52)逐位元不動。

**OpenSees 對標**:實測 openseespy **無 `CorotShellMITC4`**(unknown element);`ShellNLDKGQ` 可用但是
**不同 formulation**(DKGQ Kirchhoff vs 本實作 MITC4 Mindlin + 不同 CR),跨工具對標容差須很寬、驗證力弱
→ 本階段**不納入**(Mattiasson elastica 解析 oracle 更強);ShellNLDKGQ 跨 formulation 量級對照留作未來。

## ⑦ Gate

F58/F59;UE `FrameCore.Corotational.ShellRotationInvariance`;`run_gate.ps1 $ExpectedUeTests` **53 → 54**。
三支 build.bat 源檔清單免動。**五腿全綠**(`run_gate.ps1 -RequireOpenSees`):standalone `ALL PASS`
(F1–F59)· UE **54** · OpenSees strict PASS · linear_deep_audit **104** · CLI round-trip。

## ⑧ 效能驗收

- 每殼 facet 每 NR 迭代:一個 24×24 frame 變換 + `kl0·ul`(矩陣-向量)+ FD 切線時 24 次重算(小模型 OK)。
- **僅在 `runCorotational` + opt-in 時**;`solve()`/factorize-once/ReSolve 完全不受影響。
- 材料切線收斂但較慢;大變形 oracle 用 FD 一致切線(`consistentTangent`)達二次收斂。

## ⑨ 誠實邊界 / novelty 定位

- **方法非新穎**:EICR = Felippa & Haugen(2005)成熟文獻;`[VERIFIED]` = oracle 化實作,`[NEW CODE]` 僅
  opt-in 旗標 + `CrShell24D`/`crComputeShell24D` 接線 + 借 MITC4 kl0/R0 的整合(工程,非算法原創)。
- **能力邊界**:
  - **小應變大轉動**:kl0 是線性 MITC4 黑箱(小應變有效);非 geometrically-exact 殼。
  - **flat-facet O(1/N²) 仍在**:CR frame 只去剛體轉動,**不修** facet 對曲面的逼近(曲殼仍靠網格密度)。
  - **NR 載重控制**:arc-length 殼後屈曲(snap-through)未做(階段 2)。
  - **FD 切線**:解析 spin/moment 幾何項未加(只影響收斂速度)。
  - **CR 殼力 recover 未做**:shellForces id-tag + 零(誠實,不給大轉動誤判的錯應力);位移場是主輸出。
  - **殼壓力初始構形等效**(非 follower)。
- **F59 是梁退化 oracle**:nu=0 殼條 → 梁 elastica,驗「沿長度大變形彎曲」嚴格;真 2D 殼(雙向彎/曲殼)
  大變形無第三方 benchmark(OpenSees 無 CorotShellMITC4),F58b 旋轉不變(真 2D 殼任意軸)補強自洽性。
- **預設安全**:旗標 false → 殼 reject 同今天 → 零回歸(梁 CR F50-F52 不動)。

## ⑩ 風險 / fallback

- **已通過的最高風險**:natural-deformation convention(R_cr/R0/Rnode + logSO3 符號)→ F58b 旋轉不變 2.6e-14
  + F58a 小位移==線性 3.8e-11 雙重證明 formulation 正確。
- **大變形收斂**:材料切線在大變形收斂慢 → 用 FD 一致切線(F59 alpha=10 收斂)。極端變形需更多 loadSteps。
- **零回歸**:opt-in 預設 off;殼 reject 與梁 CR 全 oracle 守住。
- **接續階段 2**:arc-length 殼後屈曲(driver 已有路徑)+ CR 殼力 recover + 解析切線。
