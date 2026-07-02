# Setup and verify third-party UE plugin dependencies for ArchSim.
#
# For each of the 4 external plugins (ALS / SPUD / SUQS / Prefabricator):
#   - Directory absent  -> git clone + checkout pinned SHA + apply patches
#   - Directory present -> verify HEAD SHA + patch fingerprint; report status
#
# ALS patch: als_l400_animinstance_guard.patch now uses standard git-apply
# via --directory=Plugins/ALS (regenerated in v0.6.1 AS-39-u1 iteration 2;
# previous format had a corrupt blank context line). The patch is applied as
# a working-tree change in the nested ALS git repo (not committed).
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File Scripts\setup_third_party.ps1
#   powershell -ExecutionPolicy Bypass -File Scripts\setup_third_party.ps1 -DryRun
#   powershell -ExecutionPolicy Bypass -File Scripts\setup_third_party.ps1 -Root <repo-root>
#   powershell -ExecutionPolicy Bypass -File Scripts\setup_third_party.ps1 -SkipShaCheck
#     (use only when plugin was zip-extracted and SHA cannot be verified;
#      manual version confirmation required before using -SkipShaCheck)
#
# Exit codes:
#   0  all plugins present, correct SHA, patches applied
#   1  one or more plugins missing / SHA mismatch / patch not applied
#
# Output: ASCII-only (locale-defensive per v0.5.1 lesson).

param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [switch]$DryRun,
    [switch]$SkipShaCheck
    # WHY -SkipShaCheck: zip-extracted installs have no .git and SHA cannot be
    # verified. This switch documents the escape hatch explicitly rather than
    # silently continuing. Default is strict (fail on unverifiable SHA).
)
$ErrorActionPreference = 'Continue'

$RootPath = Resolve-Path -LiteralPath $Root -ErrorAction SilentlyContinue
if (-not $RootPath) {
    Write-Host "ERROR: repo root not found: $Root"
    exit 1
}
$Root = $RootPath.Path

if ($DryRun) {
    Write-Host "[DryRun] No changes will be made."
}

# ---------------------------------------------------------------------------
# Plugin definitions
# ---------------------------------------------------------------------------
# Each entry: Name / Directory (relative to Root) / UpstreamUrl / PinnedSha /
#   PatchFile (relative to Root, may be $null) /
#   PatchDirectory (--directory arg for git apply, $null = use ALS special path)
#   FingerprintFile (relative to plugin dir) / FingerprintPattern
$Plugins = @(
    @{
        Name             = 'ALS-Refactored'
        PluginDir        = 'Plugins\ALS'
        UpstreamUrl      = 'https://github.com/Sixze/ALS-Refactored.git'
        PinnedSha        = 'ba2324866bcb8c099bba0e1278d65b1848f053a1'
        PatchFile        = 'Tools\patches\als_l400_animinstance_guard.patch'
        # WHY directory: ALS patch is applied as a working-tree change in the
        # nested ALS git repo. git apply --directory=Plugins/ALS maps the
        # patch's a/Source/... paths correctly relative to the ArchSim repo root.
        # (patch regenerated in AS-39-u1 iter2 v0.6.1; old format was corrupt)
        PatchApplyMethod = 'directory'   # git apply --directory=Plugins/ALS
        FingerprintFile  = 'Source\ALS\Private\AlsCharacter.cpp'
        # WHY pattern: unique comment keyword from our patch, not present in upstream
        FingerprintPattern = 'FIX\(v0\.5\.0 U-ALS'
    },
    @{
        Name             = 'SPUD'
        PluginDir        = 'Plugins\SPUD'
        UpstreamUrl      = 'https://github.com/sinbad/SPUD.git'
        PinnedSha        = 'a7a63863fa80d6445d30f021fc17a2f418d8c973'
        PatchFile        = 'Tools\patches\spud_uplugin_engineversion_57.patch'
        PatchApplyMethod = 'directory'   # git apply --directory=Plugins/SPUD
        FingerprintFile  = 'SPUD.uplugin'
        # WHY version number in pattern: prevents false-positive if upstream adds
        # "EngineVersion" for a different UE version in a future update (Fix B)
        FingerprintPattern = '"EngineVersion".*5\.7'
    },
    @{
        Name             = 'SUQS'
        PluginDir        = 'Plugins\SUQS'
        UpstreamUrl      = 'https://github.com/sinbad/SUQS.git'
        PinnedSha        = '284b85d3843e6d60a7bf42918618f2b27770713c'
        PatchFile        = 'Tools\patches\suqs_uplugin_engineversion_57.patch'
        PatchApplyMethod = 'directory'   # git apply --directory=Plugins/SUQS
        FingerprintFile  = 'SUQS.uplugin'
        # WHY version number in pattern: same reasoning as SPUD above (Fix B)
        FingerprintPattern = '"EngineVersion".*5\.7'
    },
    @{
        Name             = 'Prefabricator'
        PluginDir        = 'Plugins\Prefabricator'
        UpstreamUrl      = 'https://github.com/unknownworlds/prefabricator-ue5.git'
        PinnedSha        = 'b7ef0a73f19c7579591f5a17d937da442264482f'
        PatchFile        = 'Tools\patches\prefabricator_uplugin_engineversion_57.patch'
        PatchApplyMethod = 'directory'   # git apply --directory=Plugins/Prefabricator
        FingerprintFile  = 'Prefabricator.uplugin'
        # WHY version number in pattern: same reasoning as SPUD above (Fix B)
        FingerprintPattern = '"EngineVersion".*5\.7'
    }
)

# ---------------------------------------------------------------------------
# Helper: check fingerprint
# ---------------------------------------------------------------------------
function Test-PatchFingerprint {
    param([string]$FullPath, [string]$Pattern)
    if (-not (Test-Path -LiteralPath $FullPath)) { return $false }
    $match = Select-String -Path $FullPath -Pattern $Pattern -Quiet 2>$null
    return [bool]$match
}

# ---------------------------------------------------------------------------
# Helper: apply uplugin patch via --directory
# ---------------------------------------------------------------------------
function Apply-DirectoryPatch {
    param([string]$PatchFull, [string]$Directory, [string]$Name)
    # --directory is relative to repo root (git is invoked from $Root)
    $result = & git -C $Root apply --directory=$Directory $PatchFull 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ERROR: git apply --directory=$Directory failed for $Name"
        Write-Host "  $result"
        return $false
    }
    return $true
}

# ---------------------------------------------------------------------------
# Helper: get current HEAD SHA of a nested git repo
# ---------------------------------------------------------------------------
function Get-PluginSha {
    param([string]$PluginDirFull)
    $sha = & git -C $PluginDirFull rev-parse HEAD 2>&1
    if ($LASTEXITCODE -ne 0) { return $null }
    return $sha.Trim()
}

# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------
$OverallOk = $true

Write-Host "============================================================"
Write-Host " ArchSim third-party plugin setup/verify"
Write-Host " Root: $Root"
if ($DryRun) { Write-Host " Mode: DRY-RUN (no changes)" }
Write-Host "============================================================"

foreach ($p in $Plugins) {
    $pluginDirFull  = Join-Path $Root $p.PluginDir
    $patchFileFull  = if ($p.PatchFile) { Join-Path $Root $p.PatchFile } else { $null }
    $fingerprintFull = Join-Path $pluginDirFull $p.FingerprintFile

    Write-Host ""
    Write-Host "--- $($p.Name) ---"

    # ---- Case 1: directory does not exist -> clone + checkout + patch ----
    if (-not (Test-Path -LiteralPath $pluginDirFull -PathType Container)) {
        Write-Host "  Plugin directory not found: $($p.PluginDir)"

        if ($DryRun) {
            Write-Host "  [DryRun] Would clone $($p.UpstreamUrl) and checkout $($p.PinnedSha.Substring(0,8))"
            Write-Host "  STATUS: MISSING (would install)"
            $OverallOk = $false
            continue
        }

        Write-Host "  Cloning $($p.UpstreamUrl) ..."
        & git clone $p.UpstreamUrl $pluginDirFull 2>&1 | ForEach-Object { Write-Host "    $_" }
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  ERROR: git clone failed for $($p.Name)"
            $OverallOk = $false
            continue
        }

        Write-Host "  Checking out pinned SHA $($p.PinnedSha.Substring(0,8)) ..."
        & git -C $pluginDirFull checkout $p.PinnedSha 2>&1 | ForEach-Object { Write-Host "    $_" }
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  ERROR: git checkout $($p.PinnedSha) failed for $($p.Name)"
            $OverallOk = $false
            continue
        }

        # Apply patch (all 4 plugins now use 'directory' method)
        if ($p.PatchApplyMethod -eq 'directory') {
            Write-Host "  Applying patch $($p.PatchFile) ..."
            $ok = Apply-DirectoryPatch -PatchFull $patchFileFull -Directory $p.PluginDir -Name $p.Name
            if (-not $ok) { $OverallOk = $false; continue }
            if (Test-PatchFingerprint -FullPath $fingerprintFull -Pattern $p.FingerprintPattern) {
                Write-Host "  STATUS: INSTALLED + PATCH APPLIED [OK]"
            } else {
                Write-Host "  WARNING: patch applied but fingerprint not found in $($p.FingerprintFile)"
                $OverallOk = $false
            }
        }
        continue
    }

    # ---- Case 2: directory exists -> verify SHA ----
    $currentSha = Get-PluginSha -PluginDirFull $pluginDirFull
    if ($null -eq $currentSha) {
        # WHY FAIL not WARNING: zip-extracted installs silently pass fingerprint
        # checks even if the version is wrong. An unverifiable SHA is a hard
        # gate failure by default. Use -SkipShaCheck as a documented escape hatch
        # only after manually confirming the plugin version. (Fix C, AS-39-u1 iter2)
        if ($SkipShaCheck) {
            Write-Host "  WARNING: $($p.PluginDir) exists but is not a git repo (no .git)."
            Write-Host "  -SkipShaCheck supplied: SHA verification bypassed. Checking fingerprint only."
            Write-Host "  NOTE: You must manually confirm this is the correct plugin version ($($p.PinnedSha.Substring(0,8)))."
        } else {
            Write-Host "  ERROR: $($p.PluginDir) exists but is not a git repo (no .git directory)."
            Write-Host "  SHA cannot be verified for a zip-extracted install."
            Write-Host "  Options:"
            Write-Host "    1. Replace with a git clone: remove $($p.PluginDir) and re-run this script."
            Write-Host "    2. If you have confirmed the correct version manually, pass -SkipShaCheck to bypass."
            Write-Host "  FAIL: unverifiable SHA is a gate error (strict by default)."
            $OverallOk = $false
            continue
        }
    } else {
        $shaShort = $currentSha.Substring(0, [Math]::Min(8, $currentSha.Length))
        $expectedShort = $p.PinnedSha.Substring(0, 8)

        if ($currentSha -ne $p.PinnedSha) {
            Write-Host "  SHA MISMATCH: expected $expectedShort got $shaShort"
            Write-Host "  ESCALATE: plugin HEAD does not match pinned SHA."
            Write-Host "  Do NOT run setup automatically -- manual investigation required."
            Write-Host "  See docs/THIRD_PARTY.md for the expected SHA."
            $OverallOk = $false
            continue
        }
        Write-Host "  SHA OK: $shaShort"
    }

    # ---- Verify fingerprint ----
    $fpOk = Test-PatchFingerprint -FullPath $fingerprintFull -Pattern $p.FingerprintPattern
    if ($fpOk) {
        Write-Host "  Patch fingerprint OK in $($p.FingerprintFile)"
        Write-Host "  STATUS: present + SHA match + patch applied [OK]"
    } else {
        Write-Host "  Patch fingerprint MISSING in $($p.FingerprintFile)"
        Write-Host "  Expected pattern: $($p.FingerprintPattern)"

        # All 4 plugins use 'directory' method (Fix A, AS-39-u1 iter2)
        if ($p.PatchApplyMethod -eq 'directory') {
            if ($DryRun) {
                Write-Host "  [DryRun] Would apply patch $($p.PatchFile)"
                Write-Host "  STATUS: present + SHA match + patch MISSING"
                $OverallOk = $false
            } else {
                Write-Host "  Applying patch $($p.PatchFile) ..."
                $ok = Apply-DirectoryPatch -PatchFull $patchFileFull -Directory $p.PluginDir -Name $p.Name
                if ($ok -and (Test-PatchFingerprint -FullPath $fingerprintFull -Pattern $p.FingerprintPattern)) {
                    Write-Host "  STATUS: present + SHA match + patch APPLIED [OK]"
                } else {
                    Write-Host "  ERROR: patch apply failed or fingerprint still missing"
                    $OverallOk = $false
                }
            }
        }
    }
}

Write-Host ""
Write-Host "============================================================"
if ($OverallOk) {
    Write-Host " RESULT: OK -- all plugins present, SHA match, patches applied"
    exit 0
} else {
    Write-Host " RESULT: FAIL -- one or more plugins need attention (see above)"
    Write-Host " Run 'Scripts\setup_third_party.ps1' (without -DryRun) to install missing items."
    exit 1
}
