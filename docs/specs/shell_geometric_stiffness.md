# 殼幾何剛度 K_σ — v3 曲面線第 ① 項(殼線性屈曲的最低門檻)

> 一句話:opt-in 殼幾何剛度(stress stiffening),讓**含 MITC4 殼的模型獲得有意義的線性屈曲因子**
> (與殼 P-Delta)。對標 `Research/V3_RESEARCH_HANDOFF.md` 證據 B1——殼幾何非線性/屈曲是 main `8fda27d`
> 對自由曲面(Zaha 受壓薄殼)最致命的缺口;本項是補上殼穩定性能力的**最低門檻**(線性特徵值屈曲),
> 殼大位移/後屈曲(co-rotational)是曲面線第 ② 項、warped/曲面精度是第 ③ 項,皆**不在本項**。

## ① 目標 / 不做

**目標**:覆寫 `MITC4ShellElement::assembleGeometric`(原為空函式),從前一次線性解的**膜應力場**計算
橫向位移 w 的幾何剛度,接進既有 `BucklingAnalysis` / `PDeltaAnalysis`(零新分析碼,只是元素端開始貢獻
K_g)。落地前:`BucklingAnalysis.h:26` 註明「Beam-column geometric stiffness only (shell ... future
addition)」、`CorotationalAnalysis.cpp` reject 殼——含殼模型的 λ_cr **無意義**(無壓縮源 → singular)。

**不做(各有後續項或刻意排除)**:
- **in-plane (u,v) 二階項**:只取薄殼標準的橫向 w stress-stiffening 項;面內挫曲/皺褶**刻意排除**(documented)。
- **非線性後屈曲 / snap-through**:線性特徵值屈曲(小位移、線性 prebuckling);後屈曲路徑屬殼 co-rotational(第 ② 項)。
- **完整殼幾何切線的膜–彎耦合**:只用膜應力張量對 w 剛化,非一致幾何切線(那需要殼 CR formulation)。
- **warped facet 修正**:flat-facet 對曲面的逼近精度照舊(MITC4 既有特性,O(1/N²)),屬曲面線第 ③ 項。

## ② 公開 API

```cpp
// SolveOptions.h（opt-in，預設 false → 逐位元同今天 beam-column-only 屈曲/P-Delta）
bool shellGeometricStiffness = false;
```

`IElement::assembleGeometric` 簽名重構(梁只需常數軸力,殼需整個膜場 → 傳整包 prestress 讓元素各取所需):
```cpp
// 舊:assembleGeometric(std::vector<Triplet>&, const std::vector<real>& memberAxial)
// 新:
virtual void assembleGeometric(std::vector<Triplet>& trips, const SolveResult& prestress) const;
//   梁:prestress.memberForces[e_].endI.N（壓正,等同舊 memberAxial[e_],逐位元）
//   殼:prestress.u（每 Gauss point 重算膜應力,opt-in）
```

## ③ 資料流 / ④ 演算法

**橫向位移(w）幾何剛度**,每殼 facet:
```
k_w = ∫_A  G_wᵀ S G_w  dA           （4×4,對 4 個角點 w 自由度)
S   = [[Nxx, Nxy], [Nxy, Nyy]]       膜應力張量（tension-positive,力/長度)
G_w = [∂N/∂x ; ∂N/∂y]                雙線性 w 形函數的笛卡兒梯度（2×4)
```
- **膜應力每 Gauss point 重算**:`N = t · Dm · (Bm · dm)`,`dm` = 前一線性解的膜 DOF(u,v,θz)×4(由
  `prestress.u → T_ → membraneToShellMap()` 取得)。殼膜場可在 facet 內強烈變化(比梁的常數軸力豐富),
  故不取常數、逐點積分。
- **2×2 Gauss**(`kG`/`kW`):雙線性梯度 × 線性應力,2×2 精確。
- **scatter + 旋轉**:`k_w` 散入 local Uz(`6i+2`,同 `plateToShellMap` 的 w→Uz 映射)→ `T_ᵀ k T_`
  旋轉到 global → append(濾零,與 `assemble()` 一致)。
- **符號**:與梁 `localGeometric12`(壓縮 → 軟化)一致 → 殼與梁 K_g 可直接相加(混合模型)。
- **全膜張量**:不像梁只用壓縮軸力,殼用**完整** S(含 Nxy 剪、雙軸)→ 捕捉雙軸/剪切屈曲。

**prepare 分支**(`MITC4ShellElement::prepare`):`useShellKsigma_ = opts.shellGeometricStiffness`。
**呼叫端適配**(行為保持):`BucklingAnalysis` / `PDeltaAnalysis` 移除預抽 axial vector 的迴圈,改傳整包
`lin`(`SolveResult`);`BeamColumnElement` 改讀 `prestress.memberForces[e_].endI.N`(= 舊值,梁逐位元不變)。

## ⑤ 檔案

| 檔 | 改動 |
|---|---|
| `Private/MITC4ShellElement.{h,cpp}` | `assembleGeometric` override(+60 行)、`useShellKsigma_` 旗標 |
| `Private/IElement.h` | `assembleGeometric` 簽名 `memberAxial` → `const SolveResult& prestress` |
| `Private/BeamColumnElement.{h,cpp}` | 改讀 `prestress.memberForces[e_].endI.N`(逐位元等價) |
| `Private/BucklingAnalysis.cpp` / `Private/PDeltaAnalysis.cpp` | 傳整包 `lin` 取代預抽 axial |
| `Public/FrameCore/SolveOptions.h` | `bool shellGeometricStiffness` |
| `Standalone/main.cpp` | **F57** oracle(+108 行) |
| `Private/Tests/ShellBucklingTest.cpp` | UE mirror（新檔,UE 自動編） |

**四 build 腳本免動**:standalone 三支 .bat 編 `main.cpp`(既有,F57 內嵌)、無新 `Private/*.cpp`;
UE 端 `ShellBucklingTest.cpp` 由 Adaptive Build 自動掃入(已驗證 `[1/9] Compile ShellBucklingTest.cpp`)。

## ⑥ Oracle（誠實分級）

**殼 K_σ(w-only)= `[VERIFIED]`**(標準薄殼/薄板幾何剛度橫向項之 FrameCore 實作)。

- **F57(a) 收斂 → 經典 Kirchhoff 解**:簡支方板單軸均布面內壓 → `N_cr = 4π²D/a²`(Timoshenko《彈性
  穩定理論》,square plate, lowest mode m=n=1, k=4)。實測 n=12 rel 9.2e-3 → n=16 4.9e-3 → **n=20 2.8e-3
  < 3%**,乾淨 O(1/N²),數值自上方逼近(離散偏剛)。
- **F57(b) 軸不變性**:板法向 z vs x → 同因子(rel **8.5e-13**),驗 `T_` 旋轉變換等變。
- **F57(c) sparse == dense**:同一殼 K_g,稀疏 subspace 路徑 == dense(rel **4.3e-12**)。
- **F57(d) opt-in OFF → singular**:關旗標 → 殼無 K_g → 無壓縮源 → singular。**證明是此旗標(非附帶路徑)
  在產生屈曲因子**,排除假陽性。
- **UE mirror** `FrameCore.Buckling.ShellGeometricStiffness`:n=16 收斂(rel<3%)+ opt-in OFF singular。
- **預設不破閘門**:`shellGeometricStiffness=false` 時殼 `assembleGeometric` no-op → 屈曲/P-Delta 與今天
  beam-column-only **逐位元相同**(既有 F34 Euler 屈曲 + 殼 oracle + OpenSees ~1e-10 不動)。
- **audit**:現有 104 checks 不變(殼 K_σ 暫未加 audit check;見 ⑨ 誠實標)。
- **殼 P-Delta**:`shellGeometricStiffness` 經 `PDeltaOptions::solve` 透傳 → 殼亦可貢獻 K_g 給
  `runPDelta`。共用**同一** `assembleGeometric` K_g(其正確性已由 F57 屈曲特徵問題覆蓋),但殼 P-Delta 的
  **迭代行為未加專屬 oracle**(本項聚焦殼屈曲;同 S8「殼–既有分析交互未逐一加 oracle」的誠實標)。

## ⑦ Gate

F57(a–d);UE `FrameCore.Buckling.ShellGeometricStiffness`;`run_gate.ps1 $ExpectedUeTests` **52 → 53**。
四 build 腳本源檔清單免動。**五腿全綠**(`run_gate.ps1 -RequireOpenSees`):standalone `ALL PASS`
(F1–F57)· UE **53** · OpenSees strict PASS · linear_deep_audit **104** · CLI round-trip。

## ⑧ 效能驗收

- 每殼 facet 多一個 4×4 幾何剛度(2×2 Gauss),**僅在 buckling / P-Delta 路徑且 opt-in 時**組裝期一次性。
- **不改 K_e / 不改 factorization fingerprint**:膜場從既有 `lin.u` 讀,K_g 是組裝後的特徵值問題右手邊
  (`(-Kg_ff)φ = γ K_ff φ`),非 `assembleAndFactor` 的 K_ff → factorize-once / ReSolve / 即時重解路徑
  **不受影響**。改旗標強制一次 fresh assemble 是無害的(僅 K_g,非 LDLᵀ)。

## ⑨ 誠實邊界 / novelty 定位

- **方法非新穎**:w-only stress stiffening 是**成熟文獻**(薄板/薄殼幾何剛度橫向項,見 Cook《Concepts and
  Applications of FEA》plate buckling、Bathe)。`[VERIFIED]` = 文獻方法的 oracle 化實作;`[NEW CODE]` 僅限
  **opt-in 旗標 + `IElement::assembleGeometric` 介面重構(memberAxial → prestress 傳遞)+ 與 24-DOF
  facet / drilling / T 變換 / 既有 BucklingAnalysis-PDeltaAnalysis 的接線**(工程整合,非算法原創)。
- **★ 對「曲殼屈曲」的精確宣稱**:F57 驗證的是 **facet 級平板 K_σ 對標平板屈曲解析解**。真實**曲殼**屈曲 =
  此 facet K_σ + flat-facet 網格逼近曲面(膜–彎耦合由相鄰 facet 的不同法向隱含承載)。因此本項**只宣稱**
  「facet 級 K_σ 正確 + 含殼模型屈曲因子有意義」,**不宣稱**「曲殼屈曲已第三方驗證」——後者需曲殼屈曲
  benchmark(如圓柱殼軸壓 Karman-Donnell λ、球殼外壓),列為**未來 oracle**(NOT IMPLEMENTED)。
- **能力邊界**:
  - **w-only**:in-plane (u,v) 二階項刻意排除 → 純面內挫曲/局部皺褶非本項涵蓋。
  - **線性特徵值屈曲**:小位移、線性 prebuckling、無卸載/後屈曲路徑(後屈曲 → 殼 co-rotational,第 ② 項)。
  - **flat-facet O(1/N²)**:MITC4 平面 facet,屈曲收斂 O(1/N²)(F57:粗網格 n≈8 級誤差大,須加密);
    warped quad(4 點不共面)投影誤差**無警告**(繼承 MITC4 既有殼特性 → 曲面線第 ③ 項)。
  - **膜場精度依賴前一線性解**:高 cond 模型的膜應力誤差會傳入 K_g(與 BucklingAnalysis 既有特性同)。
- **預設安全**:旗標 false → 殼 no-op,屈曲/P-Delta 逐位元同今天 → **零回歸風險**(opt-in 策略同 S8 QM6/DKQ)。

## ⑩ 風險 / fallback

- **零回歸**:預設 off,既有梁柱屈曲/P-Delta/OpenSees 對標全不動(F57(d) + UE opt-in OFF 雙重守門)。
- **曲殼 benchmark 缺口**:本項無曲殼第三方 oracle(僅平板解析 + 軸不變 + sparse/dense 互驗)。誠實標於
  spec/PROGRESS/README;補曲殼屈曲 benchmark 是收緊宣稱的下一步。
- **與後續曲面項的接續**:殼 co-rotational(第 ② 項)落地後,本線性 K_σ 是其幾何切線的退化極限——屆時
  可加「小載重 CR 殼屈曲 == 線性 K_σ λ_cr」互驗(對應梁的 audit「CR P-Delta degeneration」check)。
