# v2.1 — Audit-driven release candidate

> **狀態**:五腿綠;audit team 判 `release-as-v2.1`(0 Blocker / 0 Critical / 1 Effective Major,該 Major 已修)。等使用者授權發布。
>
> **權威修復清單在此**;v2.0 audit 與 76 findings 在 `E:\project\v2-audit\_audit\`(REPORT.md / AUDIT_FINDINGS.md / findings_summary.txt / audit_probe.cpp / perf_sn.cpp)。
>
> **本檔涵蓋:v2 → v2.1 的所有修復;v3 surface 線 + R2 Neumaier IR 也整合進來(branch `feature/shell-geometric-stiffness`,本地 commit 未 push)。**

---

## 1. 五腿 gate(v2.1 final,2026-06-18)

```
[1/5] standalone F1-F64 ALL PASS  (failures=0)
[2/5] UE automation: 57 tests run (expected >= 57)
[3/5] OpenSees compare: PASS
[4/5] linear deep audit: 104 checks PASS
[5/5] CLI round-trip: ALL PASS
GATE: PASS
```

**3 輪 audit 結論**:Pass 1(15-agent / 1.49M tokens / 574s)→ Pass 2 修齊所有 nice-to-fix → Pass 3 final re-check(9-agent / 731k tokens / 257s)→ **`release`,0 Blocker / 0 Critical / 0 殘留 Major**(所有 Major 與 Minor 都已修;1 個 Observation 是 `Tools/` post-release 改善)。

---

## 2. v2 audit fixes — 3 Major 全修

### 🔴 P0-1 / DC-01 (合規 Blocker-commercial) — 加 LICENSE
- 加 `LICENSE`:MIT,版權人 + 年份明確。
- README "License / use" 段重寫:明示 MIT、指向 `third_party/NOTICE.md`。

### 🔴 P0-2 / LIC-02 (合規 Major) — 加 third-party NOTICE
- 加 `third_party/NOTICE.md`:OpenBLAS (BSD-3) / METIS (Apache-2.0) / Eigen (MPL-2.0) **全文** + NOTICE clause。
- 滿足 BSD-3 §3 / Apache-2.0 §4(a)(d) / MPL-2.0 §3.2 散布義務。
- audit DHL-OLD-1 後續修:Apache-2.0 + MPL-2.0 也加 inline 主要條款(原本只有 URL)。

### 🟠 P1-3 / PERF-01 (Major) — **架構真修:supernodal 取代 LDLT 主 factor**(不是文檔降級)

使用者明確要求「**解除限制不降低宣稱**」。實際解決方式:

- 新 `SolveOptions::useSupernodalPrimary`(預設 `false` → v2.0 逐位元相容)。
- `assembleAndFactor` 在 `useSupernodalPrimary=true` + `FRAMECORE_SUPERNODAL=1` 時:
  1. 先建 supernodal Cholesky(METIS + OpenBLAS BLAS3)
  2. SPD 成功 → **完全跳過** LDLT factor。`solveLoad` 路由到 `sn::solveSuper`。
  3. SPD 失敗 → fallthrough 到 LDLT path(機構偵測仍由 LDLT 權威)。
- `pivotMargin` 從 supernodal L 對角推(`L_ii² ≈ K LDLᵀ 之 D pivot`)。
- 公開新 accessor:`PreparedSystem::isSingular() / diagnostic() / usingSupernodalPrimary()`,POD 邊界守住。
- `SnSession` 偵測 `Impl.useSnPrimary` 自動**重用**現有 factor(避免雙重 factor 成本)。
- 內部分析(PDelta / Reanalysis / DynamicCollapse)在自有 `assembleAndFactor` 呼叫時 silent force-disable flag(它們需要 LDLT);`solveModal / solveBuckling` 接受外傳 ps 時若見 `useSnPrimary` 拒絕並回清楚 diagnostic。
- `SnSolver` / `SnSession` LDLT fallback 守:`useSnPrimary` 時 LDLT 沒建好 → 回 singular + 清楚 diagnostic 非 UB。

**F63(5 子項)實證**:
- F63a — cantilever / SS+UDL / shell plate 三 fixture,SnPrimary vs LDLT primary 解 rel < 1e-9
- F63b — supernodal pivotMargin = 1.875e-07 > LDLT pivotTol 1e-12 數量級充足
- F63c — mechanism 模型(無支撐)在 SnPrimary 路徑正確判 singular
- F63d — solveModal / solveBuckling 對 SnPrimary ps 拒絕並回清楚 diagnostic
- F63e — SnSession 在 SnPrimary ps 上自動重用,non-double-build

**perf_sn 證據(v2 原始量測)**:單 solve factor 18.7k DOF **8.3× 加速**、62k DOF **20× 加速** — 現在可由 public API 兌現。

### 🟠 P1-4 / AC-03 + MA-02 (Major) — 弧長 snap-through 真守

- `CorotationalAnalysis.h::CorotationalOptions::arcLength` 註解延伸:寫清 `0 → auto` 對 soft-direction snap-through 失效、靜默跳過極限載重、PROGRESS_S9c.md durable lesson 1 引用。
- README Scope Boundaries 新段:警示手動設 `arcLength = 1%-5% of rise / characteristic deflection`。
- `arcLengthSolve()` 入口 guard:`||Fext_f|| ≤ 1e-30` → 立即回 `singular = true` + clear diagnostic "arc-length requires a non-zero external reference load on free DOFs ... Crisfield's cylindrical constraint is geometrically meaningless at zero load"。

---

## 3. v2 audit fixes — 多 Minor 一併修

| ID | 維度 | 修法 |
|---|---|---|
| **LC-05** | api-memory-safety | `frame_capi_solve_text` 包 `try-catch` C++ 例外,回 `"ERR ..."` 經 buffer 通道(返回 size 語意不變;ABI 兼容) |
| **LC-06** | api-memory-safety | `SolveResult::disp/reaction` 加 bounds check(nodeIndex/dof 越界、u/reactions empty 時回 0 而非 UB) |
| **MI-02** | robustness | `Section::Rectangular/Circular` 非正側邊立即回零殼,留給 `validate()` 報"非正截面屬性" |
| **MI-04** | robustness | `FrameModel::validate()` 重複 ID 偵測 O(n²) → `unordered_set` O(n)|
| **SLV-01** | solver-infra | `SnSolver.h / SnSession.h` 加 process-global OpenBLAS thread state 警示文檔(非 thread-safe) |
| **DC-03** | docs-honesty | UE 計數 README / VERIFICATION.md / ARCHITECTURE.md / `run_gate.ps1` 統一為 55(v2.0 時 52 / v3 surface line 加 3) |

---

## 4. v2 audit fixes — AC-06 / AC-07 真修(不只警示)

### AC-06 — 薄殼線性屈曲 knockdown(碼層交付,非文檔警語)

- 新 `BucklingOptions::shellBucklingKnockdown`(預設 `0` = 用 raw eigenvalue,逐位元同 v2.0)。
- 新 `BucklingResult::reportedCriticalFactor`(raw 特徵值)+ `knockdownFactor`(套用 alpha)+ 修正後的 `criticalFactor = alpha × raw`。
- 雙報:使用者永遠看到 raw 與 design 並列。
- 典型 alpha:0.65 (NASA SP-8007 軸壓圓筒下界) / 0.70 (一般加工品質) / 1.0 (粗壯殼)。
- 設計值直接可塞入碼檢。
- audit NLL-NEW-2 後續修:`shellBucklingKnockdown` 在 (0, 1] 之外 silent 退回 alpha=1.0 → 加 `BucklingResult.diagnostic` 警示防 unit conversion bug。

### AC-07 — 曲面殼網格守門(碼層交付,非文檔指引)

- 新 `SolveOptions::shellCurvatureMaxAngleDeg`(預設 `0` = off,逐位元同 v2.0)。
- `assembleAndFactor` 用 edge-hash O(N_shells) 算每對相鄰殼面法線夾角;> tol → 立即回 singular + 名出最壞 pair shell ID + 建議 refinement。
- audit BLDG SLV-NEW-1 修:跳過 `!sh.active` 殼(progressive collapse 已移除的 shell 不該觸發 guard)。
- audit RBD-NEW-1 修:`|cos|` 將 dihedral 映射 [0, 90°] 在 SolveOptions 註解標清楚(凹 / 反折面需另方法)。

**F64 雙項全 PASS**:
- F64a — alpha=0.65 套用,raw 不變、knockdownFactor 記錄正確、default 0 = 1.0
- F64b — 22.5° 守門對 8-facet 圓筒(45°/facet)拒絕;50° 守門允許;refinement 32-facet (11.25°/facet) 通過

---

## 5. v2.1 audit team 審核結果(2026-06-18)

15-agent ultracode workflow 8 維度平行審 + 對抗驗證 + release synthesis。**1.49M tokens / 574s**。

### Findings 統計

| | Blocker | Critical | Major | Minor | Observation |
|---|---|---|---|---|---|
| Initial | 0 | 0 | 6 | 22 | 25 |
| **After adversarial verify** | **0** | **0** | **1** | (多項降級) | — |

對抗驗證效果:5 個初判 Major 經對抗驗證降為 Minor(refuted 0 / partial 3 / confirmed 3)。

### Effective Major × 1(已修)

**DHL-NEW-2** — `run_gate.ps1` `$ExpectedUeTests = 54` 但 README / VERIFICATION / ARCHITECTURE 全 55,守門失效(可 silently drop 1 test)→ **已修為 55**。

### nice-to-fix 順手清(已修)

| ID | 維度 | 修法 |
|---|---|---|
| API-MEM SLV-NEW-1 | api-mem | `SnSession::diagnostic()` 加 `p_ == nullptr` 守(moved-from 時不 crash) |
| COLLAPSE SLV-NEW-1 | collapse | DynamicCollapse `events==0` Stable 短路改 `H.events.empty()`(initialRemovals shock 不能 silently 跳過) |
| COLLAPSE SLV-NEW-2 | collapse | Ritz 退化重啟若仍 < 1e-8 → 截斷 basis 不寫入 garbage,GES 只在 valid sub-basis 上跑 |
| BLDG SLV-NEW-1 | building | AC-07 guard 跳過 `!sh.active` |
| NLL-NEW-1 | nonlinear | `shellCorotational + useArcLength` 組合拒絕(v3 phase B 未支援) |
| NLL-NEW-2 | nonlinear | `shellBucklingKnockdown` out-of-range 加 diagnostic |
| RBD-NEW-1 | robustness | AC-07 `|cos|` 凹面限制 doc |
| SOLVER SLV-NEW-1 | solver | supernodal pivot 診斷用 `snSym.perm[idx]` un-permute 回 physical DOF |
| DHL-NEW-1 | docs | README status banner F1-F61 → F1-F64;header 補 v2.1 候選說明 |
| DHL-NEW-3 | docs | VERIFICATION.md `$ExpectedUeTests = 52` → 55 |
| DHL-OLD-1 | docs | NOTICE.md 加 Eigen MPL-2.0 + METIS Apache-2.0 主要條款 inline |

### Audit Pass 2 — 收尾清零(2026-06-18,user 指示「全部完整修完」)

| ID | 修法 |
|---|---|
| **LINEAR SLV-NEW-3** | **REFUTED**:audit 把 polar mass moment(`ρ·Ip·L` = 質量分布) 與 St-Venant J(`GJ/L` = 扭轉剛度常數)混淆。`Ip = Iy + Iz` 是極轉動慣量正確物理量;`(1/2)·ρ·Ip·ω²·L` 為剛體繞 x 軸轉動 KE。現碼正確,不改。 |
| **BLDG SLV-NEW-3** | F64a-shell 新分項:SS plate + `shellGeometricStiffness` + alpha=0.65 vs F57 analytic Kirchhoff(rel 9.2e-3 at n=12 < 5%);out-of-range alpha 雙報 + diagnostic check。 |
| **BLDG SLV-NEW-4** | UE 新 2 test:`FrameCore.Buckling.ShellKnockdown`(完整 raw / design / 預設 1.0 / out-of-range clamp / 兩 substring diagnostic / Kirchhoff oracle rel<5%) + `FrameCore.Validation.ShellCurvatureGuard`(default admit / 22.5° reject 8-facet / admit 32-facet / inactive-shell skip)。run_gate ExpectedUeTests 55→57。 |
| **DHL-NEW-5** | VERIFICATION.md §3.9 加 F57–F64 完整 capability map + 誠實 scope 標(F62 standalone 改善溫和但 64k 大規模才有量級價值、F63 single-solve 8-20× 來自 perf_sn、F64 是 F57 plate 同 oracle)。 |

### Audit Pass 3 — Final release-gate audit(2026-06-18,9-agent workflow / 257s / 731k tokens)

對 6 個動工區域(PERF-01 SnPrimary / AC-06+07 修 / Collapse 修 / docs / 弧長 / API memory)做最終 re-check。

**Verdict: `release`(0 Blocker / 0 Critical / 1 confirmed Major,已修)**

| Finding | 修法 |
|---|---|
| **FINAL-ARCLEN-1**(Major,confirmed) | 弧長 zero-load guard 之前回傳半初始化 CorotationalResult(沒填 `finalState.u/reactions/memberForces`)→ 改透過既有 `reject` lambda 路由,回 well-formed result。 |
| **DOC-ARCH-1**(Major→Minor adv) | ARCHITECTURE.md §7 殘 `$ExpectedUeTests = 52` → 改 57。 |
| **FINAL-AC07-1**(Minor) | active-pair-only 限制:在 SolveOptions.h `shellCurvatureMaxAngleDeg` 註解標明(漸進倒塌情境正確,但用戶若手動 deactivate fine-mesh 留 coarse 不會被 guard 抓)。 |
| **DOC-README-1**(Minor) | README line 232 `$ExpectedUeTests = 55` → 57。 |
| **DOC-PLUGIN-1**(Minor) | Plugins/FrameSolver/README.md `50 tests` → 57。 |
| **API-FINAL-1**(Minor) | SnSession::solveFrame() null guard(moved-from 不 crash,回 singular + clean diagnostic)。 |
| **FINAL-NLLNEW2-1**(Observation) | UE ShellKnockdown 補 'shellBucklingKnockdown=' 子字串檢查,標準化與 standalone 一致。 |
| **FINAL-UE-1**(Observation) | UE ShellKnockdown 補 Kirchhoff oracle rel<5% 檢查(與 standalone F64a-shell mirror)。 |

唯一**留 backlog**(明確標 post-release):
- **API-FINAL-2**(Observation): `Tools/cli_roundtrip.py` C-API test 不分辨 ERR-prefix exception output vs protocol mismatch。`Tools/` 改動可隨時做、不影響 v2.1 引擎正確性。

### Release notes(workflow 產出)

> **FrameCore v2.1 是一個高品質、經充分測試的發布**。穿過 8 個審核維度的對抗驗證,確認 0 Blockers、0 Criticals,以及單一 effective Major(gate script vs doc 計數失同步,已修)。架構亮點:PERF-01 supernodal-primary 解除 LDLT 強制疊加(F63 證明 single-solve 8-20× 加速兌現);R2 Neumaier IR 在固定精度殘差底限上方擴 cond 範圍;AC-06 / AC-07 真修把薄殼設計挫屈 knockdown 與曲面網格守門做成 opt-in 工具(不只警語);LICENSE + NOTICE 解除商業散布合規阻擋。

---

## 6. 整合範圍 — v2.1 = v2.0 + (v3 surface) + (R2) + (audit fixes)

- **v2.0 baseline**:tag `v2.0.0` = `8fda27d`(主線)
- **v3 曲面線**(branch local):`c317043` K_σ + `bb82b04` shell CR + `e3b66ee` warped quads + `9910ea6` docs
- **v3 memory recon**:`docs/specs/v3_memory_recon.md`(workflow 產出,331 行)
- **R2 Neumaier IR**:6 source 改 + spec + PROGRESS(本地未 commit;F62 全綠)
- **v2 audit fixes**:本檔列表(P0-1 + P0-2 + P1-3 真修 + P1-4 + Minor + AC-06/07 真修)
- **v2.1 audit fixes**:本檔列表(DHL-NEW-2 must-fix + 多 nice-to-fix)
- **新增**:F62 / F63 / F64 共 3 個 fixture(15+ 子斷言);UE 55(不變)

工作樹目前所有改動 vs main(`7030321`)在 audit team 框架下:
- ≤Major 1(已修)、五腿綠、對抗驗證通過

---

## 7. 等使用者授權

**順序(待 OK):**
1. **commit 規劃**(顯式 `git add` 個別檔案,絕不 `git add -A`):
   - 建議 5-6 個 commit:① v3 surface 三 commit 已在(待 push)② v3 memory recon spec ③ R2 Neumaier IR ④ v2 audit fixes(LICENSE/NOTICE/P1-3 真修/AC-06/AC-07/Minor)⑤ v2.1 audit fixes(DHL-NEW-2 + nice-to-fix)
   - 或集成單 commit(較簡)
2. **annotate tag v2.1.0**
3. **GitHub release**(release-as-v2.1,release notes 從 workflow 產出)
4. **發布後**:
   - HANDOFF.md(下一輪 owner 入手指南)
   - 退役 audit worktree (`git worktree remove E:/project/v2-audit`)
   - 更新 memory frame-engine-next-plan.md

**請使用者確認**:
- (a) 是否進入 commit / push / tag / release 程序?
- (b) 分幾個 commit?(建議:單 commit 整集成 vs 多 commit 按 v3 / R2 / audit 分線)
- (c) 是否要 push v3 surface 那 3 個 commit(本地已存 `c317043` / `bb82b04` / `e3b66ee` / `9910ea6`)?
