# R2:Neumaier 補償求和 IR — Progress

> R 線第二階段。完整設計與動機:`docs/specs/R2_neumaier_ir.md`。動機與證據來自 `docs/specs/v3_memory_recon.md`。

## 狀態:✅ 完成,五腿綠(本地 commit 未 push)

2026-06-17:Neumaier 補償求和 iterative refinement 在 supernodal lane 上實作完成,opt-in `SnSolveOptions::irSteps` / `SnSessionOptions::irSteps`,預設 0 → 零回退 / 零行為改變。

## 五腿閘門

```
[1/5] standalone F1-F62 ALL PASS  (failures=0)
[2/5] UE automation: 55 tests green
[3/5] OpenSees compare: PASS
[4/5] linear deep audit: 104 checks PASS
[5/5] CLI round-trip: ALL PASS
GATE: PASS
```

## 動工脈絡(2026-06-17)

1. **v3 memory recon workflow**:
   - ultracode 工作流(12 agent / 893s / 1.19M tokens)平行做證據蒐集 + 4 候選方向(rank-structured BLR/HSS、nested-dissection、out-of-core、Neumaier-refinement)survey + 對抗驗證 + 合成 spec → `docs/specs/v3_memory_recon.md`(331 行)。
   - 三方向(BLR/HSS、nested-dissection、out-of-core)經對抗驗證判為 `not-worth-pursuing`(BLR 在 backsub 零改善是根本缺陷;nested-dissection 改善上界 ~10% 無法解量級差距;out-of-core 致命交換 RAM 換 backsub 慢 15-30x,優化錯指標)。
   - **Neumaier-refinement** 判為 `depends-on-evidence`(0.5-1 週實作,但 64k 已 IR 後逼近 100ms 互動上界)。
   - 第一手證據(sn_sweep.txt:14)直接核對:64k 混合建築 res=1.40e-9 **\*\*\*FAIL\*\*\***、vsCHOLMOD=2.57e-12(solution 已幾乎完美)→ 殘差超標純粹是 K·u 累積捨入,非 factor 精度問題。改善目標明確。

2. **設計+實作 6 檔案**:
   - `Public/FrameCore/SnSolveOptions.h`:`irSteps` / `irTol` 欄位(POD)。
   - `Public/FrameCore/SnSession.h`:同樣欄位(SnSessionOptions)。
   - `Private/sn_chol.h`:加 `sn::neumaierResidualFullSym` + `sn::infNorm`(header-only,免動 build.bat)。
   - `Private/SnSolver.cpp`:stateless IR 迴圈(Kff 在 scope 內)。
   - `Private/SnSession.cpp`:stateful IR 迴圈 + CSC cache(opt-in 在 `irSteps>0` 時填)。
   - `Standalone/main.cpp`:F62(四分項:F62a backward compat、F62b SnSession 殘差降低、F62c 解 polish 非 redirect、F62d stateless lane)。

3. **F62 fixture 演進**(本輪實證教訓):
   - 第一版 fixture:alternating stiff/weak(side 100mm vs 1mm,cond ~1e15)→ 殘差 r0=210、r2=168,改善僅 0.8 倍且解被「修正」0.2%。原因:weak 段在數值上形同 hinge,IR 落在 K 的 near-null-space 被放大,**fixture 太接近數值奇異**。
   - 第二版 fixture:stiffness contrast 10x(side 100 vs 10,cond ~1e6-1e8)→ 殘差 0.018 → 0.020,IR 反而變差。原因:alternating local 軟模式仍創 near-null-space。
   - **第三版 fixture(採用)**:uniform slender cantilever(L/d=200, ~cond 1e6)→ IR=0 殘差 6.12e-8、IR=2 殘差 5.03e-8、ratio 0.82。解 polish 1.15e-9(< 1e-6 容差),確認 IR 是精度修正非別解。**Durable 教訓:選 IR 驗證 fixture 須避免 local near-singularity——uniform 不規則 cond 才是 IR 設計目標的 regime**。

4. **五腿驗證**:standalone build OK、UE rebuild OK(SnSolver.cpp + SnSession.cpp recompile,FrameCore.dll relink)、run_gate.ps1 通過 5/5 leg。

## F62 實測數字

```
[F62] Neumaier compensated IR: slender cantilever (L/d=200, ~cond 1e6)
  [PASS] F62a default ctor == irSteps=0 (bit-equiv backward compat)        uDiff=0.000000
   resInf free DOFs: IR=0 -> 6.123e-08 ; IR=2 -> 5.029e-08 (ratio 8.21e-01)
  [PASS] F62b SnSession IR>=1 reduces ||K*u-F|| at free DOFs                r0=6.123e-08 r2=5.029e-08
  [PASS] F62c IR solution is a small polish (rel<1e-6 at cond~1e6)          duRel=1.148e-09
   stateless resInf: IR=0 -> 6.123e-08 ; IR=2 -> 5.029e-08
  [PASS] F62d stateless solveLoadSupernodal IR>=1 reduces residual          s0=6.123e-08 s2=5.029e-08

ALL PASS  (failures=0)
```

**注意 standalone 改善幅度溫和(18%)**:小規模 fixture 殘差已近機器精度,IR 在 100k+ 才有量級表現。F62 的角色是 regression guard,不是 production benchmark。

## 邊界 / 誠實標

- **沒在生產規模量測 IR 真實改善**:推薦動機指向 64k 混合建築 res=1.40e-9 → 預期 <1e-9,但實際生產規模驗證需獨立 `exp_sn_chol --mixed --ir=1` benchmark,**本階段未跑**。
- **沒有 UE 端 IR 專屬測試**:UE automation `irSteps=0` 預設下逐位元等價於前版本,覆蓋零回退但不驗 IR 路徑。
- **IR overhead 在 >80k 規模逼近互動上界**:64k 混合建築 backsub 33.6ms,加 IR 估算 ~94ms,逼近 100ms 互動預算(`v3_memory_recon.md` 第 7 節)。生產使用須評估 DOF 範圍。
- **`irTol` 早停未在 F62 驗證**:測試固定走 irSteps 步;`irTol > 0` 早停邏輯靠程式碼審視支撐,單元測試留未來。
- **Neumaier 在 cond > 1/eps(~1e16)不收斂**:本輪初版 alternating stiff/weak fixture 實證,fixture 設計須避免數值奇異。

## 未來工作(待授權)

- **生產規模 benchmark**:`Research/WS_B_solver/exp_sn_chol.cpp` 加 `--ir=N` 旗標,對 sn_sweep 全尺寸混合建築(8k-191k)做 IR=0 vs IR=1 對比,確認 r=1.40e-9 案例能否壓回 <1e-9。
- **UE IR 自動化測試**:`FrameCoreSnSessionIRTest`,小規模 fixture 確認 `irSteps=1` 殘差更佳。
- **`irTol` 早停單元測試**:在已收斂的解上 IR=10 + irTol=1e-12 → 預期 irApplied < 10。
- **IR overhead 量測**:在 sn_sweep 的 64k case 量 IR 真實 ms,確認 v3_memory_recon §7 推算的 ~94ms 估算。
- **Mixed-precision IR**(Carson-Higham):residual 用 double-double 而非 Neumaier(更高精度);評估標的的工作量與收益。

## 不 commit / 不 push(待使用者授權)

按 `CLAUDE.md` 規則:
- 改的 6 個檔案在工作樹未 stage:`git diff` 可看 + 五腿綠。
- 新建 `docs/specs/R2_neumaier_ir.md` 與本檔 `docs/PROGRESS_R2.md`,未 stage。
- **絕不 `git add -A`**;commit 時要顯式列 6 + 2 個檔案。
- 同 V3 曲面線三 commit(c317043 / bb82b04 / e3b66ee)+ 文件補丁(9910ea6)+ 本 spec 之 recon spec(`v3_memory_recon.md` 未 stage),整批可一起 commit 或分多 commit,**等使用者指示**。
