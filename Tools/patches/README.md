# Tools/patches — third-party plugin patches required for ArchSim

ArchSim ships some required behavior fixes against third-party UE plugins that
are vendored under `Plugins/<PluginName>/` but **not tracked in git** (size +
convention reasons). Fresh checkouts must apply these patches after installing
the upstream plugins.

If you cloned this repo and `Plugins/ALS/` is empty / missing, install
ALS-Refactored v4.17 first per `docs/ARCHITECTURE_INDEX.md` § 4 + `docs/INPUT_MAPPING.md`,
then apply the patches below.

---

## Apply mechanism

All 4 patches in this directory use `git apply --directory=<PluginDir>`:

| Patch | Apply method |
|---|---|
| `als_l400_animinstance_guard.patch` | `git apply --directory=Plugins/ALS <patch>` |
| `spud_uplugin_engineversion_57.patch` | `git apply --directory=Plugins/SPUD <patch>` |
| `suqs_uplugin_engineversion_57.patch` | `git apply --directory=Plugins/SUQS <patch>` |
| `prefabricator_uplugin_engineversion_57.patch` | `git apply --directory=Plugins/Prefabricator <patch>` |

**Automated setup (recommended):** `Scripts/setup_third_party.ps1` handles all 4 plugins
including SHA verification, patch apply, and fingerprint verification. See `docs/THIRD_PARTY.md`
for the full manifest.

**Manual apply for all 4 patches (from repo root):**

```powershell
git apply --directory=Plugins/ALS          Tools/patches/als_l400_animinstance_guard.patch
git apply --directory=Plugins/SPUD         Tools/patches/spud_uplugin_engineversion_57.patch
git apply --directory=Plugins/SUQS         Tools/patches/suqs_uplugin_engineversion_57.patch
git apply --directory=Plugins/Prefabricator Tools/patches/prefabricator_uplugin_engineversion_57.patch
```

Note: the ALS patch applies as a working-tree change in the nested ALS git repo (not a
commit). This is intentional — the ALS plugin is untracked by convention and patching
its working tree keeps the upstream git history clean.

---

## Patches in this directory

### `als_l400_animinstance_guard.patch` (v0.5.0 — S-06 U-ALS)

**Target:** `Plugins/ALS/Source/ALS/Private/AlsCharacter.cpp` L398-400 region
**Plugin version assumed:** ALS-Refactored v4.17
**Required for:** PIE startup under `AArchSimGameMode` (the default per
`Config/DefaultEngine.ini` L37)

**Why required:** ALS-Refactored's `RefreshMeshProperties()` calls
`AnimationInstance->MarkPendingUpdate()` without a null guard on the
`!bMeshIsTicking` branch (L400 originally). When `AArchSimCharacter` is spawned
directly via `DefaultPawnClass` without a Blueprint child wiring AnimBlueprint,
`AnimationInstance` is null at `PostInitializeComponents()` time → null-deref
crash during `PossessedBy()` → `RefreshMeshProperties()` chain.

**Mitigation on ArchSim side (no patch required):**
`AArchSimCharacter` overrides `PostInitProperties()` + `BeginPlay()` to
`LoadObject<>()` Settings / MovementSettings / SkeletalMesh / AnimBlueprint
at runtime-late timing (after ALS plugin content is mounted). The L400 guard
is a defensive second line of safety for headless / fixture builds where
plugin content may not be loaded.

**Apply (from repo root):**

```powershell
git apply --directory=Plugins/ALS Tools/patches/als_l400_animinstance_guard.patch
```

The patch applies as a working-tree change in the nested ALS git repo (not a commit).
`Scripts/setup_third_party.ps1` handles this automatically on fresh clone.
`run_gate.ps1` precondition check verifies the fingerprint before legs run.

*(Note: an earlier version of this patch file (pre-v0.6.1 iter2) had a corrupt blank
context line and required manual apply. The file was regenerated in AS-39-u1 iteration 2
and is now a clean `git diff` output that applies without issues.)*

**Verify after apply:** Read `Plugins/ALS/Source/ALS/Private/AlsCharacter.cpp`
in the `!bMeshIsTicking` block — should contain `if (AnimationInstance.IsValid())`
and a `// FIX(v0.5.0 U-ALS` WHY comment block above.

**Verify with build:**

```powershell
& "$env:UE_ENGINE_ROOT\Engine\Build\BatchFiles\Build.bat" `
    ArchSimEditor Win64 Development `
    -project="$PWD\ArchSim.uproject" -waitmutex
```

Then in UE Editor:
1. Confirm `Config/DefaultEngine.ini` L37 = `GlobalDefaultGameMode=/Script/ArchSim.ArchSimGameMode`
2. Press Play (PIE)
3. Observe Output Log — should see `AArchSimCharacter [ALS] (...): Settings loaded: 0x...` lines
4. Character should spawn (ALS Manny mesh) without crash and be movable

---

### `spud_uplugin_engineversion_57.patch` (v0.6.1 — AS-39-u1)

**Target:** `Plugins/SPUD/SPUD.uplugin`
**Plugin version assumed:** SPUD at SHA `a7a63863` (sinbad/SPUD)

**Why required:** Upstream SPUD at pinned SHA does not declare `"EngineVersion"` in
its `.uplugin`. UE 5.7 emits a compatibility warning at plugin load time when this
field is absent. Adding `"EngineVersion": "5.7.0"` suppresses the warning and
confirms UE 5.7 compatibility.

**Apply (from repo root):**
```powershell
git apply --directory=Plugins/SPUD Tools/patches/spud_uplugin_engineversion_57.patch
```

**Verify after apply:** `Plugins/SPUD/SPUD.uplugin` should contain `"EngineVersion" : "5.7.0"`
between `"SupportURL"` and `"EnabledByDefault"`.

---

### `suqs_uplugin_engineversion_57.patch` (v0.6.1 — AS-39-u1)

**Target:** `Plugins/SUQS/SUQS.uplugin`
**Plugin version assumed:** SUQS at SHA `284b85d3` (sinbad/SUQS)

**Why required:** Same as SPUD — upstream SUQS at pinned SHA lacks `"EngineVersion"`.
Adding `"EngineVersion": "5.7.0"` suppresses UE 5.7 load-time warning.

**Apply (from repo root):**
```powershell
git apply --directory=Plugins/SUQS Tools/patches/suqs_uplugin_engineversion_57.patch
```

**Verify after apply:** `Plugins/SUQS/SUQS.uplugin` should contain `"EngineVersion" : "5.7.0"`
between `"SupportURL"` and `"EnabledByDefault"`.

---

### `prefabricator_uplugin_engineversion_57.patch` (v0.6.1 — AS-39-u1)

**Target:** `Plugins/Prefabricator/Prefabricator.uplugin`
**Plugin version assumed:** Prefabricator at SHA `b7ef0a73` (unknownworlds/prefabricator-ue5)

**Why required:** Same family as SPUD/SUQS — upstream Prefabricator at pinned SHA
lacks `"EngineVersion"`. Adding `"EngineVersion": "5.7.0"` suppresses UE 5.7 warning.

**Apply (from repo root):**
```powershell
git apply --directory=Plugins/Prefabricator Tools/patches/prefabricator_uplugin_engineversion_57.patch
```

**Verify after apply:** `Plugins/Prefabricator/Prefabricator.uplugin` should contain
`"EngineVersion": "5.7.0"` between `"SupportURL"` and `"CanContainContent"`.

---

## Updating this directory

When a new patch is required:

1. Make the source change in the working tree under `Plugins/<Plugin>/...`
2. Generate the patch:
   ```bash
   diff -u <upstream-orig-file> <modified-file> > Tools/patches/<descriptive_name>.patch
   ```
   Or if you have the upstream version as a git ref:
   ```bash
   git diff --no-index <upstream-orig> <modified> > Tools/patches/<name>.patch
   ```
3. Add the patch with `git add Tools/patches/<name>.patch`
4. Update this README with a `### <name>` entry describing target / version / why / apply / verify
5. Document the patch as a Z-01 first-action in the next `docs/HANDOFF_v<tag>.md`

---

## Why this directory exists

The `Plugins/<X>/` directories for third-party UE plugins (ALS-Refactored,
SPUD, SUQS, Prefabricator, etc.) are large (200 MB – 1 GB each) and
**untracked by convention** in this repo (from never being `git add`-ed; they
are NOT excluded by `.gitignore` — `git check-ignore -v Plugins/ALS` exits 1).
See `docs/ARCHITECTURE_INDEX.md` § 4 and `docs/THIRD_PARTY.md` for the full
manifest. They're installed manually per project setup instructions. When ArchSim
needs to patch upstream code (e.g. to fix a plugin bug or work around a
behavior expected by ArchSim), the patch is committed here so:

- Fresh contributors can replicate ArchSim's behavior on their host
- ArchSim's modifications to vendored plugins are auditable
- Upstream plugin updates don't silently overwrite our fixes (the patch file
  serves as canonical source of truth)
- CI / release-hardening can verify the patch is applied via build-log grep
  for the WHY comment fingerprint
