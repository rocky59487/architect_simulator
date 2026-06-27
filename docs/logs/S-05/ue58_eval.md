# UE5.8 Upgrade Eval — Decision Document

**Sprint:** S-05 | **Task:** SPIKE-UE5.8-eval
**Date:** 2026-06-27 | **Author:** subagent SPIKE-UE5.8-eval
**Baseline:** v0.3.1 (commit `994be68`, UE5.7)

---

## § 1 Summary: GO / NO-GO / CONDITIONAL

### Headline: NO-GO (UE5.8 not installed on this host)

UE5.8 is not installed on the build machine. Phase A install detection probed all
conventional Windows paths and the system registry — only UE 4.0 and UE 5.7 are
present. Without a UE5.8 install, no plugin compat smoke can be run and the
upgrade cannot proceed.

**S-06 first action:** Install UE5.8 via Epic Games Launcher, then re-run this
spike's Phase B (sandbox copy + build) as the S-06 opening step.

### Pre-install risk summary (static analysis only)

Even before installing UE5.8, a static read of the 4 external `.uplugin` files and
`Build.cs` files reveals one known risk:

| Risk | Severity | Source |
|---|---|---|
| SPUD depends on `StructUtils` plugin | **MEDIUM** | `SPUD.uplugin` line 40; `SPUD.Build.cs` line 35 |
| `StructUtils` Experimental plugin deprecated at UE5.5 | **MEDIUM** | `UE_5.7/Engine/Plugins/Experimental/StructUtils/StructUtils.uplugin` line: `"DeprecatedEngineVersion": "5.5"` |
| `StructUtils` moved into `CoreUObject` in UE5.5+ | **INFO** | Headers now at `Engine/Source/Runtime/CoreUObject/Public/StructUtils/InstancedStruct.h` |
| ALS v4.17 — no UE5.8-specific known issues from static read | LOW | `ALS.uplugin` clean, no deprecated dep |
| Prefabricator v1.11.0 — no external plugin deps in `.uplugin` | LOW | `Prefabricator.uplugin` has no `Plugins` section |
| SUQS v1.0 — no plugin deps in `.uplugin` | LOW | `SUQS.uplugin` has no `Plugins` section |

**SPUD / StructUtils risk detail:** In UE5.5, Epic graduated `StructUtils` from
`Engine/Plugins/Experimental/StructUtils` into `Engine/Source/Runtime/CoreUObject/`.
The Experimental plugin stub in UE5.7 carries `"DeprecatedEngineVersion": "5.5"`.
In UE5.8, Epic may remove the Experimental plugin entirely, leaving only the
CoreUObject path. SPUD's `Build.cs` lists `"StructUtils"` as a private module dep —
if the module no longer exists as a standalone module (just headers under
CoreUObject), the build will fail with a missing-module error. This needs live
verification when UE5.8 is available. SPUD's include `"StructUtils/InstancedStruct.h"`
already resolves from CoreUObject in UE5.7, so include-path breakage is lower risk
than the module-dep breakage.

This is the **AS-06** backlog item (see `ARCHITECTURE_INDEX.md § 7`).

---

## § 2 UE5.8 Install Detection Result

### Paths probed

| Path | Exists | Notes |
|---|---|---|
| `C:\Program Files\Epic Games\UE_5.8\` | No | Standard Epic Launcher default |
| `D:\Epic Games\UE_5.8\` | No | Alt drive default |
| `E:\UE_5.8\` | No | Project-adjacent |
| `E:\project\UE_5.8\` | No | Consistent with UE5.7 at `E:\project\UE_5.7\` |
| env var `UE_ENGINE_ROOT_58` (User + Machine scope) | Not set | — |

### Registry scan result

`HKLM\SOFTWARE\EpicGames\Unreal Engine` contains:

| Key | InstallDir |
|---|---|
| `4.0` | `C:\Program Files\Epic Games\4.0\` |
| `5.7` | `E:\project\UE_5.7` |

Also probed: `HKLM\SOFTWARE\Wow6432Node\EpicGames\Unreal Engine` (32-bit
WoW64 hive). Key absent — UE installers are 64-bit so the WoW64 hive is not
expected to be populated; included for completeness per Phase 3 review nit
(2026-06-27).

**UE5.8 not present in registry.**

### Binary verification

`UnrealEditor-Cmd.exe` check: **N/A** — no candidate path found.

### Result: UE5.8 install NOT detected. Phase B skipped per scope contract.

---

## § 3 Per-Plugin Status Table

Phase B (build smoke) could NOT be run (no UE5.8 install). Status below is based on
static analysis of `.uplugin` + `Build.cs` files only.

| Plugin | UE5.7 `.uplugin` EngineVersion | External plugin deps | Static risk | Verdict |
|---|---|---|---|---|
| ALS (v4.17) | `5.7.0` | ACLPlugin, AnimationModifierLibrary, ControlRig, EngineCameras, EnhancedInput, GameplayTagsEditor, Metasound, Niagara, PropertyAccessNode | All are first-party UE plugins likely present in 5.8; no known deprecated items | CONDITIONAL (needs live build verify) |
| Prefabricator (v1.11.0) | `5.7.0` | None (no `Plugins` section in `.uplugin`) | Lowest risk of the four | CONDITIONAL (needs live build verify) |
| SPUD (v1.0) | `5.7.0` | `StructUtils` (Experimental, `"DeprecatedEngineVersion":"5.5"`) | **Highest risk** — `StructUtils` module may be removed in UE5.8; `Build.cs` private dep will break if module gone | CONDITIONAL — HIGH RISK (see § 4) |
| SUQS (v1.0) | `5.7.0` | None (no `Plugins` section) | Low risk | CONDITIONAL (needs live build verify) |

All four verdicts are CONDITIONAL because no live UE5.8 build was run.

---

## § 4 Known UE5.8 Deprecation Surfaces Relevant to ArchSim

### SPUD — StructUtils dependency (AS-06)

**Status:** CONFIRMED risk, NOT confirmed as error (no UE5.8 source access).

**Evidence chain:**
1. `Plugins/SPUD/SPUD.uplugin` L40: `"Name": "StructUtils", "Enabled": true`
2. `Plugins/SPUD/Source/SPUD/SPUD.Build.cs` L35: `"StructUtils"` in `PrivateDependencyModuleNames`
3. `Plugins/SPUD/Source/SPUD/Private/SpudPropertyUtil.cpp` L8: `#include "StructUtils/InstancedStruct.h"`
4. `Plugins/SPUD/Source/SPUDTest/Private/TestSaveObject.h` L8: `#include "StructUtils/InstancedStruct.h"`
5. `E:\project\UE_5.7\Engine\Plugins\Experimental\StructUtils\StructUtils.uplugin`:
   `"DeprecatedEngineVersion": "5.5"` — **this plugin was deprecated at UE5.5**

**In UE5.7:** StructUtils Experimental plugin exists but is marked deprecated. The
headers have been moved into `CoreUObject/Public/StructUtils/` since UE5.5. The
`StructUtils` module name still resolves in UE5.7 via the Experimental plugin's
`StructUtils` module. SPUD builds OK in UE5.7 because the Experimental stub is
still present.

**Include-side mitigation in SPUD source (added 2026-06-27 per Phase 3 review):**
`SpudPropertyUtil.cpp:7` + `TestSaveObject.h:7` already guard the StructUtils
include with `#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5`-style
preprocessor blocks. Inside the `>=5` branch the include resolves from
CoreUObject's path. **Effect on UE5.8 fix scope:** the `#include
"StructUtils/InstancedStruct.h"` line continues to resolve cleanly from
CoreUObject even when the Experimental plugin module disappears — the
include-side risk is therefore **already mitigated** by SPUD's existing
version guards. The remaining risk is purely the `PrivateDependencyModuleNames`
entry in `Build.cs` (§ 4 fix path remains as documented).

**In UE5.8 (projected):** Epic may remove the Experimental StructUtils plugin
entirely. If so, `PrivateDependencyModuleNames "StructUtils"` in SPUD.Build.cs
will produce: `ERROR: Module 'StructUtils' could not be found.` This is the classic
"deprecated-in-5.5, removed-in-5.8" pattern for Experimental plugins.

**Fix path (when UE5.8 is available):**
- Remove `"StructUtils"` from `SPUD.Build.cs` PrivateDependencyModuleNames (it's
  now part of CoreUObject which is already in PublicDependencyModuleNames)
- Remove `"StructUtils"` plugin dep from `SPUD.uplugin`
- Verify `#include "StructUtils/InstancedStruct.h"` still resolves from CoreUObject
  (it should — CoreUObject provides this path since UE5.5)
- This is a sandbox-copy fix in `Research/ue58_attempt/Plugins/SPUD/`, never in
  the main `Plugins/SPUD/` (main tree must stay UE5.7-compatible until upgrade is
  confirmed)

### ALS — no confirmed deprecation surface

ALS v4.17 depends on first-party plugins (ControlRig, EnhancedInput, Niagara,
Metasound). All are expected to be present in UE5.8. No static-analysis evidence
of deprecation risk. Confirm with live build.

### Prefabricator — no external plugin deps

No `Plugins` entries in `.uplugin`. Risk is internal API usage (e.g. deprecated
UE C++ API) which requires live build to detect.

### SUQS — no external plugin deps

No `Plugins` entries in `.uplugin`. Same profile as Prefabricator.

---

## § 5 S-06 First Action

### Step 1: Install UE5.8 (prerequisite — blocks everything else)

```
# Open Epic Games Launcher
# Unreal Engine > Library > + (add version) > 5.8
# Recommended install path: E:\project\UE_5.8\
# (consistent with UE5.7 at E:\project\UE_5.7\)
```

### Step 2: Run Phase B of this spike (idempotent re-run)

Once UE5.8 is installed, this Phase B sandbox build is the S-06 opening action:

```powershell
# 1. Create sandbox probe project (already have Research/ue58_attempt/)
# 2. Create minimal uproject
$sandbox = "E:\project\ArchSim\Research\ue58_attempt"
# UE58Probe.uproject already templated below

# 3. Copy 4 external plugins into sandbox
foreach ($plugin in @("ALS","Prefabricator","SPUD","SUQS")) {
    Copy-Item "E:\project\ArchSim\Plugins\$plugin" "$sandbox\Plugins\$plugin" -Recurse -Force
}

# 4. Patch SPUD uplugin + Build.cs for UE5.8 (sandbox copy only)
# - Remove "StructUtils" from SPUD.uplugin Plugins array
# - Remove "StructUtils" from SPUD\Source\SPUD\SPUD.Build.cs PrivateDependencyModuleNames

# 5. Build probe
& "E:\project\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
    UE58ProbeEditor Win64 Development `
    -project="$sandbox\UE58Probe.uproject" -waitmutex
```

### Step 3: Triage build errors per-plugin

- If all GREEN: upgrade is GO. Update all 4 `.uplugin` `EngineVersion` to `"5.8.0"`.
  Update `ArchSim.uproject` `EngineAssociation` to `"5.8"`. Run full 5-leg gate.
- If SPUD RED (expected): apply StructUtils fix (§ 4 fix path) in sandbox, re-build,
  confirm GREEN. Then apply same fix to main `Plugins/SPUD/` as part of S-06 work.
- If ALS RED: check for UE5.8 animation API changes (LinkedAnimGraph deprecation,
  etc.). May require ALS version bump from upstream.

### Step 4: Update `.uproject` + run 5-leg gate

```powershell
# After all 4 plugins build green in sandbox:
# 1. Update ArchSim.uproject EngineAssociation "5.7" -> "5.8"
# 2. Rebuild UE editor:
& "E:\project\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
    ArchSimEditor Win64 Development `
    -project="E:\project\ArchSim\ArchSim.uproject" -waitmutex
# 3. Run 5-leg gate:
.\Scripts\run_gate.ps1 -RequireOpenSees
```

---

## § 6 Sandbox Cleanup Decision

**KEEP `Research/ue58_attempt/` for S-06 idempotent re-run.**

Rationale:
- The directory currently contains only `README.md` (Phase A early-return)
- When UE5.8 is installed, S-06 can directly proceed to Phase B by copying plugins
  into the existing sandbox structure — no re-setup needed
- The sandbox is tracked as `??` in `git status` (untracked), not modified tracked
  files — it does not pollute commits until explicitly `git add`-ed

**Do NOT `git add` sandbox files until upgrade is confirmed GO and the sandbox is
replaced by actual in-tree changes.**

---

## Appendix: Minimal UE58Probe.uproject Template

For Phase B (S-06 re-run), create this file at
`Research/ue58_attempt/UE58Probe.uproject`:

```json
{
    "FileVersion": 3,
    "EngineAssociation": "5.8",
    "Category": "",
    "Description": "",
    "Modules": [],
    "Plugins": [
        {"Name": "ALS", "Enabled": true},
        {"Name": "Prefabricator", "Enabled": true},
        {"Name": "SPUD", "Enabled": true},
        {"Name": "SUQS", "Enabled": true}
    ]
}
```

Note: `ArchSim.uproject` is NOT copied here (rule #5: never touch `.uproject` in
main worktree for this eval). This is a minimal probe-only project.
