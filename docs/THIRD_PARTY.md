# 第三方套件清單 — Architect Simulator

> **用途：** 記錄所有 ArchSim 所依賴但不直接追蹤於 git 的外部 plugin 與資產，
> 包含每個套件的 pinned commit SHA、授權、所需 patch、安裝路徑。
>
> **新 session 協定：** 首次 clone 或懷疑 plugin 狀態不對時，執行
> `Scripts/setup_third_party.ps1` 自動驗證並補齊缺失項目。
> Patch 詳細說明見 `Tools/patches/README.md`。

---

## 外部 Plugin 清單

| Plugin | 安裝路徑 | 上游 URL | Pinned SHA | 授權 | 對應 Patch |
|---|---|---|---|---|---|
| ALS-Refactored v4.17 | `Plugins/ALS/` | https://github.com/Sixze/ALS-Refactored.git | `ba232486` | MIT | `als_l400_animinstance_guard.patch` |
| SPUD | `Plugins/SPUD/` | https://github.com/sinbad/SPUD.git | `a7a63863` | MIT | `spud_uplugin_engineversion_57.patch` |
| SUQS | `Plugins/SUQS/` | https://github.com/sinbad/SUQS.git | `284b85d3` | MIT | `suqs_uplugin_engineversion_57.patch` |
| Prefabricator UE5 | `Plugins/Prefabricator/` | https://github.com/unknownworlds/prefabricator-ue5.git | `b7ef0a73` | MIT | `prefabricator_uplugin_engineversion_57.patch` |

> **注意：** 上表 SHA 為完整 40 位的前 8 碼縮寫；`setup_third_party.ps1` 以完整 SHA 驗證。

---

## Patch 說明

### ALS — `als_l400_animinstance_guard.patch`

**目標檔案：** `Plugins/ALS/Source/ALS/Private/AlsCharacter.cpp` (L400 區域)

**為何需要：** `AAlsCharacter::RefreshMeshProperties()` 在 `!bMeshIsTicking` 分支直接呼叫
`AnimationInstance->MarkPendingUpdate()` 未加 null-guard，在 `AArchSimCharacter` 直接
從 `DefaultPawnClass` 產生（無 AnimBlueprint wiring 的 Blueprint child class）時會 null-deref
並 EXCEPTION_ACCESS_VIOLATION。

**Apply 方式（從 repo root）：**
```powershell
git apply --directory=Plugins/ALS Tools/patches/als_l400_animinstance_guard.patch
```
`setup_third_party.ps1` 會自動執行此步驟（clone + checkout + apply）。

*（注：v0.6.1 iter2 前的舊版 patch 檔有格式問題（blank context line 缺前置空格），需手動套用。
該檔已於 AS-39-u1 iteration 2 重新生成，現為乾淨 `git diff` 格式，可自動 apply。）*

### SPUD — `spud_uplugin_engineversion_57.patch`

**目標檔案：** `Plugins/SPUD/SPUD.uplugin`

**為何需要：** 上游 SPUD SHA `a7a63863` 的 `SPUD.uplugin` 未宣告 `"EngineVersion": "5.7.0"`，
UE 5.7 引擎在載入時警告版本不相容。加入此欄位後警告消除，plugin 正常載入。

**Apply 方式（從 repo root）：**
```powershell
git apply --directory=Plugins/SPUD Tools/patches/spud_uplugin_engineversion_57.patch
```

### SUQS — `suqs_uplugin_engineversion_57.patch`

**目標檔案：** `Plugins/SUQS/SUQS.uplugin`

**為何需要：** 同 SPUD — 上游 SUQS SHA `284b85d3` 的 `SUQS.uplugin` 未宣告 `"EngineVersion"`,
加入 `"5.7.0"` 消除 UE 版本警告。

**Apply 方式（從 repo root）：**
```powershell
git apply --directory=Plugins/SUQS Tools/patches/suqs_uplugin_engineversion_57.patch
```

### Prefabricator — `prefabricator_uplugin_engineversion_57.patch`

**目標檔案：** `Plugins/Prefabricator/Prefabricator.uplugin`

**為何需要：** 同 SPUD/SUQS — 上游 Prefabricator SHA `b7ef0a73` 的 `Prefabricator.uplugin`
未宣告 `"EngineVersion"`, 加入 `"5.7.0"` 消除 UE 版本警告。

**Apply 方式（從 repo root）：**
```powershell
git apply --directory=Plugins/Prefabricator Tools/patches/prefabricator_uplugin_engineversion_57.patch
```

---

## Content 資產

| 資產路徑 | 說明 |
|---|---|
| `Content/BP_ArchSimScenarioWidget.uasset` | `UArchSimScenarioWidget` 的 Blueprint child class（`UCLASS(Abstract)` 的具體實作）。大小 ~18 KB。此檔案自 AS-39-u1 (v0.6.1) 起改為 tracked（列入 `git add`）。Editor Utility Widget tab 在 PIE 前必須由此 Blueprint 產生。 |

---

## 安裝指引

1. **自動驗證/安裝：**
   ```powershell
   powershell -ExecutionPolicy Bypass -File Scripts\setup_third_party.ps1
   ```
   - 若 plugin 目錄不存在：clone + checkout pinned SHA + apply patches
   - 若 plugin 目錄已存在：驗證 HEAD SHA + patch fingerprint

2. **手動安裝（若 clone 受限）：**
   - 分別 clone 各 upstream URL，checkout 到上表 pinned SHA
   - 從 repo root 執行各 patch 的 apply 命令

3. **驗證：**
   ```powershell
   powershell -ExecutionPolicy Bypass -File Scripts\setup_third_party.ps1 -DryRun
   ```
   `-DryRun` 只印狀態、不做任何修改，exit 0 代表全部就緒。

---

## Patch 原則

- 所有 patch 儲存在 `Tools/patches/`，說明見 `Tools/patches/README.md`
- **4 個 patch 全部使用 `git apply --directory=Plugins/<X>`** 從 repo root apply（AS-39-u1 iter2 統一）
- ALS patch 套用為 nested ALS git repo 的 working-tree change（未 commit），其他 3 個套用在 untracked plugin 目錄
- **不追蹤 plugin 目錄本身**（untracked by convention；詳見下方說明）

---

## 為何不追蹤 Plugin 目錄

4 個外部 plugin 目錄（`Plugins/ALS/`、`Plugins/SPUD/`、`Plugins/SUQS/`、
`Plugins/Prefabricator/`）各自有獨立的 `.git` 目錄（nested git clone），
加上體積龐大（200 MB – 1 GB），因此不直接追蹤於 ArchSim repo。

這些目錄目前在 ArchSim repo 中屬於 **untracked（從未 git add）**，而非
被 `.gitignore` 排除（`git check-ignore -v Plugins/ALS Plugins/SPUD` 均 exit 1；
**注意：若未來 .gitignore 變動需重新確認 check-ignore 結果**）。
Tracked 的是 patch 文件（`Tools/patches/*.patch`）與本 manifest（`docs/THIRD_PARTY.md`），
確保：

- 精確的版本可重現性（pinned SHA）
- ArchSim 對上游的修改可稽核（patch 文件）
- 不因 upstream plugin 更新而靜默覆蓋本地 fix（patch 作為正典）
