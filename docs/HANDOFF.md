# FrameCore 交接指南 — v2.1 後接手 owner

> v2.1 在 2026-06-18 發布,tag `v2.1.0` = commit `4e660de`。本檔給「明天才接到這專案」的人:在哪入手、怎麼跑、踩過什麼雷、下一步是什麼。

---

## 1. 一句話脈絡

FrameCore 是畢業專題的純 C++17/Eigen 結構力學引擎(beam-column + MITC4 flat shell + 崩塌 + supernodal 直接解法 lane + 弧長 / co-rotational 大變形)。v2.0 完成主線 S1–S10,v2.1 在審核驅動下做三項架構升級(supernodal-primary / Neumaier IR / 殼挫屈 + 曲面網格守門)+ MIT 授權合規。

## 2. 快速上手(30 分鐘可開工)

### 環境
- Windows + Visual Studio 2026 preview(`cl` 透過 `vswhere -prerelease` 找)
- UE 5.7 在 `E:\project\UE_5.7`(內含 Eigen 3.4.0 @ `Engine\Source\ThirdParty\Eigen`)
- conda env `framecore-direct`(OpenBLAS + METIS,標準 standalone gate 需要)
- Python:openseespy(`pip install openseespy`,給跨工具驗證用)

### 跑五腿 gate(秒~分級)
```powershell
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
```
預期:
```
[1/5] standalone F1-F64 ALL PASS  (failures=0)
[2/5] UE automation: 57 tests run (expected >= 57)
[3/5] OpenSees compare: PASS
[4/5] linear deep audit: 104 checks PASS
[5/5] CLI round-trip: ALL PASS
GATE: PASS
```

### 改 FrameCore source 之後必跑
1. **standalone**:`Plugins\FrameSolver\Standalone\build.bat`
2. **UE rebuild**(改 public header 必要):
   ```powershell
   & "E:\project\UE_5.7\Engine\Build\BatchFiles\Build.bat" ArchSimEditor Win64 Development -project="E:\project\ArchSim\ArchSim.uproject" -waitmutex
   ```
3. **run_gate.ps1 不會自動 rebuild UE** — 改了 UE 端碼或加新 UE 測試,先 (2) 重編,否則跑舊 binary。

## 3. 動工前必讀

| 文件 | 為何讀 |
|---|---|
| [CLAUDE.md](../CLAUDE.md) | 鐵則 + commit 衛生 + durable 踩雷 |
| [README.md](../README.md) | 引擎能力 / 邊界 / scope boundaries(誠實標) |
| [docs/VERIFICATION.md](VERIFICATION.md) | capability → oracle map(F1-F64 完整對應) |
| [docs/ARCHITECTURE.md](ARCHITECTURE.md) | data model / solve pipeline / convention |
| [docs/PROGRESS_V21.md](PROGRESS_V21.md) | v2.1 完整修復清單 + 3 輪 audit 結論 |
| [docs/PROGRESS_R2.md](PROGRESS_R2.md) | R 線 supernodal + IR 落地史 |
| [docs/specs/v3_memory_recon.md](specs/v3_memory_recon.md) | v3 記憶體線痛點偵察 + 4 方向比較 |

## 4. 三大鐵則(違反必破 gate)

1. **FrameCore 純 C++17/Eigen,Eigen 只在 Private/FrameEigen.h + PreparedSystemImpl.h**(`FRAMECORE_UE` 切 dual-build)。Public API POD/std,SnSolveOptions / BucklingResult 等對外 struct 不可洩漏 Eigen。
2. **任何 source 動 = 五腿 gate 必須全綠**。standalone F1-F64 / UE 57 / OpenSees / audit 104 / CLI。
3. **絕不 `git add -A`**;commit 顯式列檔。`run_gate.ps1` 的 `$ExpectedUeTests = 57` + standalone build.bat 的源檔清單是 silent regression 守門。

## 5. v2.1 的 3 個 opt-in 旗標(全預設 off,逐位元同 v2.0)

```cpp
// 1. 把 supernodal Cholesky 變主 factor(skip LDLT)
SolveOptions opt;
opt.useSupernodalPrimary = true;
PreparedSystem ps = assembleAndFactor(model, opt);
SolveResult R = solveLoad(ps, model);              // 走 sn::solveSuper

// 2. Neumaier 補償求和 IR(supernodal lane 上的精度延伸)
SnSessionOptions sopt;
sopt.irSteps = 1;                                  // 1-2 通常足夠
SnSession sess(ps, sopt);
SolveResult Rf = sess.solveFrame(model);

// 3. 殼挫屈 design knockdown
BucklingOptions bopt;
bopt.shellBucklingKnockdown = 0.65;                // NASA SP-8007 軸壓圓筒
BucklingResult b = solveBuckling(ps, model, bopt);
// b.reportedCriticalFactor = raw eigenvalue
// b.criticalFactor         = alpha * raw  (= design value)
// b.knockdownFactor        = 0.65

// 4. 曲面殼粗網格守門
SolveOptions opt;
opt.shellCurvatureMaxAngleDeg = 22.5;              // 16-facet 每 90 deg
PreparedSystem ps = assembleAndFactor(model, opt); // 超粗網格 -> singular + 清楚 diagnostic
```

**限制**:`useSupernodalPrimary` 的 `PreparedSystem` 不能傳給需要 LDLT 的分析(`solveBuckling` / `solveModal` 拒絕並回 diagnostic);`PDelta` / `Reanalysis` / `DynamicCollapse` 各自呼叫 `assembleAndFactor` 內部 force-disable 此旗標。生產建議:跑 default LDLT 給分析,跑 SnPrimary 給 raw `solveLoad`。

## 6. 候選下一階段(依現有 spec 排序;每階段動工前先補滿 spec、完成即停)

| 階段 | 起手 | 為何 |
|---|---|---|
| **v3 記憶體線實作** | `docs/specs/v3_memory_recon.md` §7 推薦的 Neumaier IR 生產整合(已部分到位)+ benchmark 補齊 64k 混合建築 res 跨點 | recon 已篩出 Neumaier 為唯一值得做方向(BLR/HSS/nested-dissection/out-of-core 皆 not-worth);F62 已在 standalone 驗證,生產規模實證需要 `exp_sn_chol --mixed --ir=1` 跑全尺寸 |
| **殼 CR 階段 2** | `docs/specs/shell_corotational.md` 末段 | v3 曲面②落地 NR load-control,arc-length 殼後挫屈 + CR 殼力 recover + 解析切線是後續 |
| **可視化資料線 C6-C8** | `README.md` Roadmap | 沿桿 BMD/SFD、利用率場、贅餘度報告;這條對使用者體驗最直接 |
| **UE5 視覺層** | `README.md` Roadmap | 吃 `CollapseStep.u` 回放、`FragmentCluster` → Chaos、D/C 熱圖 |
| **S11 MITC9i 高階殼** | `docs/AGENT_PROMPT_S5_S11.md` | 殿後,9 處引擎修改先決;殼曲面最終解 |
| **Tools/cli_roundtrip.py ERR-prefix 守門** | audit final 留 backlog 的 API-FINAL-2 | 5 分鐘工作量,但不影響引擎正確性 |

## 7. 踩雷(durable,反覆咬人)

1. **Edit tool 顯示成功 ≠ 真落地** — 動工/commit 前 `git diff` + grep 核對。
2. **UE 測試常數勿命名 `IN`/`OUT`**(Windows SAL 巨集)→ 用 `kIn/kLb`。
3. **UE 測試裡 std::vector 用 `.size()` 不是 `.Num()`**;FrameCore 容器是純 std。
4. **`bUseUnity=false`** 因 Adaptive Build 排除新 .cpp 出 unity;加 analysis .cpp(含匿名 namespace helper)會在 unity 合併 TU 衝突。
5. **localAxes 慣例 `z=x×ref, y=z×x`**:X 軸 + refVec(0,0,1) → local y=+Z / local z=−Y → global-Y 端載彎曲用 `Iy`。
6. **`Section::Rectangular(b,d)`**:`b`=寬(local z)、`d`=深(local y);`Zz=b·d²/4`、`Zy=d·b²/4`、`A=b·d`。
7. **塑鉸**:`PlasticHinge.dof∈{4,5,10,11}`;成鉸端 recover 出 `M=0`(非 Mp,residual 走節點側力矩通道)。
8. **non-supernodal standalone build(`build_cli`/`build_capi`/`build_perf`/`build_linear_audit`)必須帶 `/DFRAMECORE_SUPERNODAL=0`**,否則 PreparedSystemImpl.h 拉入 sn_chol.h → 需要 metis.h(這些 build 沒裝 conda 路徑)。v2.1 改 → 已修。
9. **`PreparedSystem::Impl` opaque**:standalone 測試不可 `ps.impl->...`;改用 v2.1 新公開 accessor `isSingular() / diagnostic() / pivotMargin() / usingSupernodalPrimary()`。
10. **UE build 用 PowerShell `&` call operator**(git-bash 下 `cmd //c '"…Build.bat" -project="…"'` 巢狀引號失敗)。
11. **CLI/OpenSees 兩腿自呼 `build_cli.bat` 重編 frame_cli**(故 frame_cli.exe 舊時間戳無妨)。
12. **接續未提交工作**:**先 `git diff` 全讀核對**(Edit 假落地;R2 與 v3 曲面線都踩過)。
13. **OpenSees `CorotShellMITC4` 不可用**(openseespy unknown element);`ShellNLDKGQ` 可用但不同 formulation → 殼大變形用 Mattiasson 解析 oracle 更強。
14. **`Subagent 模型`**:`CLAUDE_CODE_SUBAGENT_MODEL` 環境變數曾誤設為 `kimi-k2.5` 導致 Agent 失敗,已刪。若 Agent 又報模型錯先查該 User-scope 環境變數。

## 8. 既知 backlog(post-release,不阻擋使用)

| ID | 維度 | 描述 | 工作量 |
|---|---|---|---|
| API-FINAL-2 | tooling | `Tools/cli_roundtrip.py` ERR-prefix 不分辨 exception vs protocol mismatch | 5-10 min |
| (待 audit 開新輪) | docs | `docs/AGENT_PROMPT_S5_S11.md` / `docs/IMPLEMENTATION_PLAN.md` 殘留 `F1-F54` / `UE 50` 字樣(歷史文件) | 標 [HISTORICAL] 即可 |

## 9. 核心檔案速查

| 目的 | 路徑 |
|---|---|
| 公開 API | `Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/*.h`(全 POD,Eigen 0 洩漏) |
| 核心解算 | `Private/FrameSolver.cpp`(assembleAndFactor + solveLoad,supernodal-primary 分流在此) |
| 自建 supernodal | `Private/sn_chol.h`(header-only,METIS+BLAS3) |
| 殼元素 | `Private/MITC4ShellElement.cpp`(QM6/DKQ/CR/warped 都在此) |
| 動態崩塌 | `Private/DynamicCollapse.cpp`(Ritz / Newmark / events 串流) |
| Standalone gate | `Standalone/main.cpp`(F1-F64) |
| UE automation | `Private/Tests/*.cpp`(57 tests) |
| 五腿入口 | `Scripts/run_gate.ps1` |
| OpenSees 驗證 | `Tools/opensees_compare.py` 等 |

## 10. 哪裡的審核遺產

- **v2.0 commercial audit**(76 findings):`<repo-root>/../v2-audit/_audit/REPORT.md / AUDIT_FINDINGS.md / findings_summary.txt / audit_probe.cpp / perf_sn.cpp`
  - **退役建議**:`git worktree remove <repo-root>/../v2-audit`(留 .git 檔案在 ArchSim/.git/worktrees 內,日後想重 checkout 可 `git worktree add ... v2.0.0` 重建)
- **v2.1 audit team 全 transcript**:`~/.claude/projects/<project>/sessions/<uuid>/` 對應 workflow IDs(initial / final re-check;真正的 session UUID 與 workflow ID 保留在原作業者 local memory,公開 repo 不寫入身份識別資訊)
- **v3 memory recon**:`docs/specs/v3_memory_recon.md`(331 行)+ 一條 workflow ID(`Research/V3_RESEARCH_HANDOFF.md` 是預 R2 落地版本)

## 11. 一句話總結

> v2.0 是「正確性 A+ / 穩健性 A / 文檔誠實業界少見」的研究級引擎,v2.1 在審核驅動下擦掉了商業合規與 supernodal 性能宣稱兩個阻擋,並把所有 audit Major 真修(不只警示)。下一輪自由接下:**v3 記憶體線生產實證 → C6-C8 可視化 → UE5 視覺層 → S11**。
