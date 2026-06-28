# Tools/patches — third-party plugin patches required for ArchSim

ArchSim ships some required behavior fixes against third-party UE plugins that
are vendored under `Plugins/<PluginName>/` but **not tracked in git** (size +
convention reasons). Fresh checkouts must apply these patches after installing
the upstream plugins.

If you cloned this repo and `Plugins/ALS/` is empty / missing, install
ALS-Refactored v4.17 first per `docs/ARCHITECTURE_INDEX.md` § 4 + `docs/INPUT_MAPPING.md`,
then apply the patches below.

---

## Apply all patches (one-shot)

From the repo root:

```bash
for p in Tools/patches/*.patch; do
    echo "Applying $p"
    git apply "$p" || { echo "FAILED on $p"; exit 1; }
done
```

Or on POSIX without git:

```bash
for p in Tools/patches/*.patch; do
    echo "Applying $p"
    patch -p1 < "$p" || { echo "FAILED on $p"; exit 1; }
done
```

PowerShell equivalent:

```powershell
Get-ChildItem Tools/patches/*.patch | ForEach-Object {
    Write-Host "Applying $_"
    & git apply $_.FullName
    if ($LASTEXITCODE -ne 0) { throw "FAILED on $_" }
}
```

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

**Apply:**

```bash
cd <ArchSim repo root>
git apply Tools/patches/als_l400_animinstance_guard.patch
```

**Verify after apply:** Read `Plugins/ALS/Source/ALS/Private/AlsCharacter.cpp` L411 — should now read `if (AnimationInstance.IsValid()) { AnimationInstance->MarkPendingUpdate(); }` wrapped in 11-line WHY comment block above.

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
SPUD, SUQS, Prefabricator, etc.) are large (200 MB – 1 GB each) and never
tracked in this repo by convention (see `.gitignore` and `docs/ARCHITECTURE_INDEX.md`
§ 4). They're installed manually per project setup instructions. When ArchSim
needs to patch upstream code (e.g. to fix a plugin bug or work around a
behavior expected by ArchSim), the patch is committed here so:

- Fresh contributors can replicate ArchSim's behavior on their host
- ArchSim's modifications to vendored plugins are auditable
- Upstream plugin updates don't silently overwrite our fixes (the patch file
  serves as canonical source of truth)
- CI / release-hardening can verify the patch is applied via build-log grep
  for the WHY comment fingerprint
