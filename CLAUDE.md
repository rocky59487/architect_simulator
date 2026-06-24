# CLAUDE.md — FrameCore / 建築師模擬器 貢獻者鐵則

> 這份文件是 repo 內**最高權威的契約規則**:鐵則 + commit 衛生 + durable 踩雷。
> 在這個 repo 動任何手(尤其是 AI agent / Claude Code)之前先讀完。
> 全專案的 README、release notes、handoff、spec 都把這裡當「鐵則 #N」的出處引用。
>
> *This is the repo's canonical contract: iron rules + commit hygiene + durable gotchas.
> Read it before touching anything. The whole repo cites "鐵則 #N" back to this file.*

---

## 🧊 FROZEN marker(鐵則 #1 正式凍結標記)

> **`Plugins/FrameSolver/Source/FrameCore/` 的引擎原始碼自 `v4.0.0`(2026-06）起 FROZEN。**
> 演算法不可變,API / wire-ABI 不再有 breaking change,**不會有 v3.7**。
> 對該目錄的逐版 source delta 目標永遠是 **0 行**。
>
> 還能演進的是 **UE 消費端**(`Plugins/FrameSolver/Source/FrameCoreUE/`),
> 走 `v4.0.x` patch / `v4.1.x` minor。LevelSim(`Plugins/LevelSim/`)獨立演進。
>
> 例外只有一條:**使用者明確點頭**才可動 frozen 引擎,且必須走完整版本程序
> (見下方「版本面同步」+ 五腿 gate + 新增測試)。

---

## 五大鐵則(違反必破 gate)

**鐵則 #1 — 引擎凍結 + 純度**
- `FrameCore/` 引擎已 FROZEN(見上)。預設**不動**;要動先拿到使用者同意。
- FrameCore 是純 **C++17 + Eigen**;**Eigen 只能出現在 `Private/FrameEigen.h`
  與 `Private/PreparedSystemImpl.h`**(以 `FRAMECORE_UE` 切 dual-build)。
- **Public API 全 POD/std**(`Public/FrameCore/*.h`):`SolveResult` / `SnSolveOptions`
  / `BucklingResult` 等對外 struct 不可洩漏 Eigen 或 UE 型別。Eigen 物件藏在
  `PreparedSystem` 的 PIMPL 之內。

**鐵則 #2 — 動 source → 五腿 gate 必須全綠**
- 任何 source 改動,提交前五腿 gate 全綠才算數:standalone `F1..F71` ·
  UE automation · OpenSees strict · linear deep audit(104)· CLI round-trip。

**鐵則 #3 — 誠實高於好看**
- 在某主機上**沒跑到的腿就標 `NOT RUN`**,不可把「文件寫綠」當成「驗過綠」。
- GPU(cuDSS)與 UE automation 腿若在無對應硬體/UE 的主機上,如實標註手動執行,
  不得在 CI summary 假裝通過。帶一個誠實的「未驗證」比帶一個假綠燈嚴重得多。

**鐵則 #4 — 加 UE 測試就 bump `$ExpectedUeTests`**
- 新增 `FrameCore.*` / `FrameCore.UE.*` 測試時,同步調高 `Scripts/run_gate.ps1`
  的 `$ExpectedUeTests`(目前 **135**;無 cuDSS 用 `-ExpectedUeTests 133`)。
  這個 guard 是 silent-regression 守門:沒 bump 等於默許測試被靜默漏掉。

**鐵則 #5 — 絕不 `git add -A`**
- commit **顯式列出檔案**。一次 release = 一個 commit、明確 `git add <files>`。
- `run_gate.ps1` 的 `$ExpectedUeTests` + `build.bat` 的源檔清單是雙重 silent-regression 守門。

---

## 版本面同步(release 必改、必須一起改)

改版時下面**四個**值必須同步,任何一個漏掉都會被 `v2_roundtrip` 的 version-pin 抓到
(歷史上 `kEngineVer` 曾被遺忘卡在舊版):

| 值 | 位置 | 目前 |
|---|---|---|
| `kEngineVer` | `Plugins/FrameSolver/Standalone/v2/Dispatcher.h` | `4.0.0` |
| uplugin `VersionName` | `Plugins/FrameSolver/FrameSolver.uplugin` | `4.0.0` |
| `FRAMECORE_EXPECTED_ENGINE_VER` | `Scripts/run_gpu_gate.ps1` | `4.0.0` |
| `FRAMECORE_EXPECTED_ENGINE_VER` | `.github/workflows/release-gate.yml` | `4.0.0` |

---

## 五腿 gate 怎麼跑

```powershell
# 一鍵五腿(秒~分級):standalone + UE automation + OpenSees + audit + CLI
powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees

# 只跑秒級 standalone(改引擎/standalone 後最快的回饋)
Plugins\FrameSolver\Standalone\build.bat        # 預期 ALL PASS (failures=0)
```

GitHub CI(`.github/workflows/release-gate.yml`)只跑 **CPU 腿**(standalone /
audit 104 / CLI / v2 dispatcher round-trip)並上傳 gate log 作證據。
**GPU(cuDSS)腿** 在裝有 cuDSS 的整合者主機上跑 `Scripts\run_gpu_gate.ps1 -Strict`;
**UE automation 腿** 需要 UE 5.7 + 先 rebuild(`run_gate.ps1` 不會自動重編 UE module)。

改 UE 端碼或加 UE 測試後,先重編再跑 gate,否則跑的是舊 binary:
```powershell
& "E:\project\UE_5.7\Engine\Build\BatchFiles\Build.bat" ArchSimEditor Win64 Development -project="<repo>\ArchSim.uproject" -waitmutex
```

---

## 哪裡能動 / 哪裡不能動

| 路徑 | 狀態 |
|---|---|
| `Plugins/FrameSolver/Source/FrameCore/` | 🧊 **FROZEN**(預設不動;需使用者點頭) |
| `Plugins/FrameSolver/Source/FrameCoreUE/` | ✅ 可演進(UE 消費端;v4.0.x / v4.1.x) |
| `Plugins/FrameSolver/Standalone/`, `Tools/`, `Scripts/` | ✅ 可動(gate / 工具;改完跑 gate) |
| `Plugins/LevelSim/` | ✅ 獨立演進(對 FrameCore 零耦合) |
| `docs/`, `README.md` | ✅ 可動(保持誠實;`docs/FrameCore_full.md` 是完整封存參考) |

---

## durable 踩雷(反覆咬人,優先看)

1. **Edit 工具顯示成功 ≠ 真落地** — 動工/commit 前 `git diff` + grep 核對。接手未提交工作先全讀 diff。
2. **UE 測試常數勿命名 `IN` / `OUT`**(撞 Windows SAL 巨集)→ 用 `kIn` / `kLb`。
3. **UE 測試裡 FrameCore 容器是純 std** → 用 `.size()` 不是 `.Num()`。
4. **`bUseUnity=false`**:加含匿名 namespace helper 的新 `.cpp` 會在 unity 合併 TU 衝突。
5. **localAxes 慣例 `z=x×ref, y=z×x`**:X 軸 + refVec(0,0,1) → local y=+Z / local z=−Y。
6. **`Section::Rectangular(b,d)`**:`b`=寬(local z)、`d`=深(local y);`A=b·d`、`Zz=b·d²/4`、`Zy=d·b²/4`。
7. **塑鉸 `PlasticHinge.dof∈{4,5,10,11}`**;成鉸端 recover 出 `M=0`(非 Mp)。
8. **non-supernodal standalone build**(`build_cli`/`build_capi`/`build_perf`/`build_linear_audit`)必帶 `/DFRAMECORE_SUPERNODAL=0`,否則拉入 `sn_chol.h` → 需要 `metis.h`。
9. **`PreparedSystem::Impl` 是 opaque**:standalone 測試不可碰 `ps.impl->...`;用公開 accessor `isSingular()` / `diagnostic()` / `pivotMargin()` / `usingSupernodalPrimary()`。
10. **OpenSees `CorotShellMITC4` 不可用**(openseespy unknown element);殼大變形用 Mattiasson 解析 oracle。

---

## 關鍵檔案速查

| 目的 | 路徑 |
|---|---|
| 公開 API | `Plugins/FrameSolver/Source/FrameCore/Public/FrameCore/*.h`(全 POD) |
| 核心解算 | `Private/FrameSolver.cpp`(assembleAndFactor + solveLoad) |
| 唯一 Eigen include 點 | `Private/FrameEigen.h`(+ `PreparedSystemImpl.h` 持 Eigen 型別) |
| 自建 supernodal | `Private/sn_chol.h`(header-only,METIS + BLAS3) |
| 殼元素 | `Private/MITC4ShellElement.cpp`(QM6/DKQ/CR/warped) |
| Standalone gate | `Plugins/FrameSolver/Standalone/`(`build.bat` → F1..F71) |
| 五腿入口 | `Scripts/run_gate.ps1` · GPU:`Scripts/run_gpu_gate.ps1` |
| 驗證證據鏈 | `docs/VERIFICATION.md` |

## 延伸閱讀

- [`README.md`](README.md) — 專案門面
- [`Plugins/FrameSolver/README.md`](Plugins/FrameSolver/README.md) — FrameCore 策展 README
- [`docs/FrameCore_full.md`](docs/FrameCore_full.md) — 完整技術參考 + 範圍邊界 + 發行史
- [`docs/VERIFICATION.md`](docs/VERIFICATION.md) — capability → oracle → fixture
- [`docs/V3_SERIES_RETROSPECTIVE.md`](docs/V3_SERIES_RETROSPECTIVE.md) — v3 系列回顧
