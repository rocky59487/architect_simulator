# One-click verification gate for the FrameSolver engine (FrameCore-only, post-cleanup).
#   [1/6] standalone FrameCore fixtures (analytic golden oracles)
#   [2/6] UE headless automation (FrameCore.* + ArchSim.Persistence/Integration/Gameplay)
#   [3/6] OpenSees offline cross-validation (skipped if openseespy absent)
#   [4/6] linear-analysis deep audit (post F17-F25 strengthening; prints its own
#         independent-check count -- the audit reports "checks=N", not a hardcoded number)
#   [5/6] CLI round-trip (S6: frame_cli J1 text bridge end-to-end; VERSION/TONLY/SIZEOPT/DYNC)
#   [6/6] PIE auto-smoke (AS-35: render-thread test, no -nullrhi; separate from leg 2)
# Prints a combined PASS/FAIL summary and sets the exit code (0 = all green).
#
# Usage from repo root:
#   powershell -ExecutionPolicy Bypass -File Scripts\run_gate.ps1 -RequireOpenSees
# Optional:
#   -Root <repo> -Engine <UE root>   (or set UE_ENGINE_ROOT)
#
# PRECONDITIONS (R-AUDIT B-03 / B-04 — silent until now, documented here):
#   * This script does **NOT rebuild the UE module**. Touch any FrameCore .cpp / add a
#     UE automation test? Rebuild ArchSimEditor first with Engine\Build\BatchFiles\Build.bat,
#     or leg 2 runs the stale binary and tests added in this session are silently absent
#     (the $ExpectedUeTests guard catches the silent-absent case as a gate failure).
#   * Standalone (leg 1) + deep audit (leg 4) link the opt-in supernodal lane (OpenBLAS +
#     METIS via the conda `framecore-direct` env). The Standalone build .bat files exit 1
#     when the env is not on PATH; this script does NOT activate conda for you. Activate
#     `conda activate framecore-direct` before running, or expect leg 1 / leg 4 to fail at
#     the compile step with a clear "could not locate openblas / metis" diagnostic.
#   * OpenSeesPy (leg 3) lives in its own Python env (Tools/opensees_compare.py). With
#     `-RequireOpenSees`, missing OpenSeesPy is a gate failure rather than a soft skip.
#   * Third-party plugins (ALS / SPUD / SUQS / Prefabricator) must be installed and patched
#     before legs run. This script checks plugin dirs + patch fingerprints and fails fast
#     with "run Scripts\setup_third_party.ps1" if any are missing. (AS-39-u1, v0.6.1)
param(
    [switch]$RequireOpenSees,       # CI: fail (not skip) when openseespy is absent
    [int]$ExpectedUeTests = 165,
    #   cuDSS build: 165 | non-cuDSS (FRAMECORE_CUDA=0, F67/F67s compile out): pass -ExpectedUeTests 163
    #
    #   Count history — major anchors only (intermediate counts omitted):
    #   v0.1.1  +1  ArchSim.Persistence.SaveLoadRoundTrip
    #   v0.1.3  +1  ArchSim.Persistence.MaxRankCeiling (97-register stress; pins no ceiling on RegisterMember)
    #   v0.1.4  +1  ArchSim.Persistence.RebaselineCeiling (strict > semantic of MaxRankBeforeRebaseline=96)
    #   v0.1.5  +1  ArchSim.Integration.TickDriver (UArchSimGameInstance Tick telemetry smoke)
    #   v0.2.0  +1  ArchSim.Gameplay.CharacterInput (AArchSimCharacter CDO/reflection smoke)
    #   v0.3.0  +1  FrameCore.UE.InteractiveSubsystem.EmptyModelStartSession (AS-17 graceful-fail oracle)
    #   v0.3.0  +1  ArchSim.Integration.PieHarnessSmoke (AS-13-u1; GEngine GetWorldContexts harness self-verify)
    #   v0.3.0  +3  ArchSim.Integration.PieRebaseline / PieDriverLoop / ArchSim.Gameplay.PieInputRuntime
    #               (AS-13-u2; harness-based deferred-branch coverage for rebaseline / driver-loop / character)
    #   v3.4 Phase 1-5 / v3.5 Phase 1-8 / v3.6 Phase 1-8 / v4.0.0 seal: FrameCore.UE.* tests (see git log)
    #   S-05    +1  ArchSim.Gameplay.ScenarioWidget (SPIKE-Scenario-u1; UArchSimScenarioWidget CDO/reflection smoke)
    #   S-05    +1  ArchSim.Gameplay.ScenarioSolveWire (SPIKE-Scenario-u2; RequestSolveAndVisualize reflection + graceful-fail)
    #   S-05    +1  ArchSim.Gameplay.ScenarioTutorial (SPIKE-Scenario-u3; K2/K4 UFunction + Tutorial state machine + Reset headless smoke)
    #   S-06    +1  ArchSim.Gameplay.ScenarioFixture (AS-30; RegisterFixedSupport + SpawnDefaultPortalFrame reflection + headless dedup oracle)
    #   S-07    +0  ArchSim.PIE.PortalFrameSmoke (AS-35-u1) MOVED TO LEG 6 (render thread required,
    #               no -nullrhi); leg 2 count unchanged (149 cuDSS / 147 non-cuDSS).
    #               Leg 2 filter is now category-enumerated to explicitly exclude ArchSim.PIE.*
    #               (see WHY comment at the ExecCmds line below).
    #   S-08    +4  ArchSim.Persistence.SpudSidecarClearSemantics / SpudSidecarRoundtrip /
    #               SpudRfTransientAudit / SpudEmptyModelSave
    #               (AS-08-u1; RF_Transient audit + Registry::Reset + sidecar UPROPERTY scan contracts)
    #   S-09    +4  ArchSim.Persistence.ResetClearsComponentFlags / RegisterMemberNonFinite /
    #               ReplayOrphanGuard / SaveLoadGuards
    #               (AS-40-u1; persistence edge-case contracts)
    #   S-09    +8  ArchSim.Persistence.V2LibraryStructRoundtrip / V2RestoreLibraries /
    #               V2InjectLoads / V2FixityApi / V2FormatVersion / V2DeactivatedSaveGuard /
    #               V2V1CompatDefaults / N00TensionReleaseWire
    #               (AS-41-u1; sidecar format v2 full model-state persistence; SC17 renamed
    #               ReplayOrphanGuard -> ReplayOrphanDataInvariant, same test count)
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$Engine = $env:UE_ENGINE_ROOT,
    [string]$UProject = ''
)
$ErrorActionPreference = 'Continue'

# ---------------------------------------------------------------------------
# Precondition check: third-party plugins (AS-39-u1, v0.6.1)
# Verifies 4 plugin dirs exist + patch fingerprints are present before legs run.
# WHY: fresh clones lack these dirs; stale setups may lack patches. Failing fast
# here avoids mysterious compile errors in leg 1/2 that are hard to diagnose.
# Output: ASCII-only (locale-defensive per v0.5.1 lesson).
# ---------------------------------------------------------------------------
function Test-ThirdPartyPreconditions {
    param([string]$RepoRoot)

    $ok = $true

    # Helper: check fingerprint pattern in a file (safe if file absent -> returns false)
    # WHY: Select-String throws on missing path; Test-Path guards against that.
    $CheckFp = {
        param($FilePath, $Pat)
        if (-not (Test-Path -LiteralPath $FilePath)) { return $false }
        return [bool](Select-String -Path $FilePath -Pattern $Pat -Quiet -ErrorAction SilentlyContinue)
    }

    # ---- Plugin 1: ALS ----
    $alsDir  = Join-Path $RepoRoot 'Plugins\ALS'
    $alsCpp  = Join-Path $alsDir   'Source\ALS\Private\AlsCharacter.cpp'
    if (-not (Test-Path -LiteralPath $alsDir -PathType Container)) {
        Write-Host "PRECONDITION FAIL: Plugins\ALS not found."
        Write-Host "  Run: powershell -ExecutionPolicy Bypass -File Scripts\setup_third_party.ps1"
        $ok = $false
    } elseif (-not (& $CheckFp $alsCpp 'FIX\(v0\.5\.0 U-ALS')) {
        Write-Host "PRECONDITION FAIL: ALS patch not applied (AnimationInstance guard missing)."
        Write-Host "  Run: powershell -ExecutionPolicy Bypass -File Scripts\setup_third_party.ps1"
        Write-Host "  Note: ALS patch requires manual apply -- see docs\THIRD_PARTY.md."
        $ok = $false
    }

    # ---- Plugin 2: SPUD ----
    $spudDir    = Join-Path $RepoRoot 'Plugins\SPUD'
    $spudPlugin = Join-Path $spudDir  'SPUD.uplugin'
    if (-not (Test-Path -LiteralPath $spudDir -PathType Container)) {
        Write-Host "PRECONDITION FAIL: Plugins\SPUD not found."
        Write-Host "  Run: powershell -ExecutionPolicy Bypass -File Scripts\setup_third_party.ps1"
        $ok = $false
    } elseif (-not (& $CheckFp $spudPlugin '"EngineVersion".*5\.7')) {
        # WHY include version: prevents false-positive if upstream adds "EngineVersion"
        # for a different UE version in a future pinned-SHA update (Fix B, AS-39-u1 iter2)
        Write-Host "PRECONDITION FAIL: SPUD uplugin patch not applied (EngineVersion 5.7 missing)."
        Write-Host "  Run: powershell -ExecutionPolicy Bypass -File Scripts\setup_third_party.ps1"
        $ok = $false
    }

    # ---- Plugin 3: SUQS ----
    $suqsDir    = Join-Path $RepoRoot 'Plugins\SUQS'
    $suqsPlugin = Join-Path $suqsDir  'SUQS.uplugin'
    if (-not (Test-Path -LiteralPath $suqsDir -PathType Container)) {
        Write-Host "PRECONDITION FAIL: Plugins\SUQS not found."
        Write-Host "  Run: powershell -ExecutionPolicy Bypass -File Scripts\setup_third_party.ps1"
        $ok = $false
    } elseif (-not (& $CheckFp $suqsPlugin '"EngineVersion".*5\.7')) {
        # WHY include version: same reasoning as SPUD above (Fix B, AS-39-u1 iter2)
        Write-Host "PRECONDITION FAIL: SUQS uplugin patch not applied (EngineVersion 5.7 missing)."
        Write-Host "  Run: powershell -ExecutionPolicy Bypass -File Scripts\setup_third_party.ps1"
        $ok = $false
    }

    # ---- Plugin 4: Prefabricator ----
    $prefDir    = Join-Path $RepoRoot 'Plugins\Prefabricator'
    $prefPlugin = Join-Path $prefDir  'Prefabricator.uplugin'
    if (-not (Test-Path -LiteralPath $prefDir -PathType Container)) {
        Write-Host "PRECONDITION FAIL: Plugins\Prefabricator not found."
        Write-Host "  Run: powershell -ExecutionPolicy Bypass -File Scripts\setup_third_party.ps1"
        $ok = $false
    } elseif (-not (& $CheckFp $prefPlugin '"EngineVersion".*5\.7')) {
        # WHY include version: same reasoning as SPUD above (Fix B, AS-39-u1 iter2)
        Write-Host "PRECONDITION FAIL: Prefabricator uplugin patch not applied (EngineVersion 5.7 missing)."
        Write-Host "  Run: powershell -ExecutionPolicy Bypass -File Scripts\setup_third_party.ps1"
        $ok = $false
    }

    return $ok
}

$RootPath = Resolve-Path -LiteralPath $Root -ErrorAction SilentlyContinue
if (-not $RootPath) { Write-Host "Repo root not found: $Root" -ForegroundColor Red; exit 1 }
$Root = $RootPath.Path

if ([string]::IsNullOrWhiteSpace($Engine)) {
    $Candidate = Join-Path (Split-Path $Root -Parent) 'UE_5.7'
    if (Test-Path -LiteralPath $Candidate) { $Engine = $Candidate }
}
$EnginePath = Resolve-Path -LiteralPath $Engine -ErrorAction SilentlyContinue
if (-not $EnginePath) {
    Write-Host "UE engine root not found. Pass -Engine <UE root> or set UE_ENGINE_ROOT." -ForegroundColor Red
    exit 1
}
$Engine = $EnginePath.Path

$UProj  = if ([string]::IsNullOrWhiteSpace($UProject)) { Join-Path $Root 'ArchSim.uproject' } else { $UProject }
$UeCmd  = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$Log    = Join-Path $Root 'Saved\Logs\ArchSim.log'

# ---- Third-party precondition check (AS-39-u1) ----
$PreconOk = Test-ThirdPartyPreconditions -RepoRoot $Root
if (-not $PreconOk) {
    Write-Host ""
    Write-Host "GATE ABORTED: third-party preconditions not met (see above)."
    Write-Host "Run: powershell -ExecutionPolicy Bypass -File Scripts\setup_third_party.ps1"
    exit 1
}

Write-Host '======================================================'
Write-Host ' FrameSolver verification gate'
Write-Host '======================================================'

# ---- [1/6] standalone gate ----
Write-Host ''
Write-Host '[1/6] standalone FrameCore gate (build.bat)...'
& (Join-Path $Root 'Plugins\FrameSolver\Standalone\build.bat') | Tee-Object -Variable StandaloneOut | Out-Null
$StandaloneRC = $LASTEXITCODE
$StandaloneLine = ($StandaloneOut | Select-String -Pattern 'ALL PASS|FAILURES' | Select-Object -Last 1)
Write-Host ("       standalone: {0} (exit {1})" -f $StandaloneLine, $StandaloneRC)

# ---- [2/6] UE headless automation ----
Write-Host ''
Write-Host '[2/6] UE headless automation...'
# WHY category-enumerated filter (Option A, AS-35-u2):
#   The test ArchSim.PIE.PortalFrameSmoke (AS-35-u1) requires EditorContext | ClientContext
#   and uses FStartPIECommand, which needs a real render context. Under -nullrhi the test
#   crashes (EXCEPTION_ACCESS_VIOLATION observed 2026-06-28 09:53:41). The previous wildcard
#   filter 'FrameCore+ArchSim' would pick up ArchSim.PIE.* here and fail under -nullrhi.
#   Solution: enumerate the categories that are safe under -nullrhi explicitly.
#   MAINTENANCE NOTE: if a new ArchSim.<Category> is added (e.g. ArchSim.Network),
#   and it is safe under -nullrhi, add +ArchSim.<Category> to this filter AND bump
#   $ExpectedUeTests accordingly. If it requires render context, add a new leg instead.
#
#   Categories enumerated (all safe under -nullrhi as of S-07):
#     FrameCore        — all F1..F71 standalone-mirrored UE tests
#     ArchSim.Persistence — SaveLoadRoundTrip / MaxRankCeiling / RebaselineCeiling
#     ArchSim.Integration — TickDriver / PieHarnessSmoke / PieDriverLoop / PieRebaseline
#     ArchSim.Gameplay    — CharacterInput / PieInputRuntime / ScenarioWidget /
#                           ScenarioSolveWire / ScenarioTutorial / ScenarioFixture
#   EXCLUDED from this filter (intentionally):
#     ArchSim.PIE.*    — requires render thread (no -nullrhi); handled by leg [6/6]
$ExecCmds = 'Automation RunTests FrameCore+ArchSim.Persistence+ArchSim.Integration+ArchSim.Gameplay; Quit'
& $UeCmd $UProj "-ExecCmds=$ExecCmds" -unattended -nullrhi -nopause -nosplash -log | Out-Null
$UeRC = $LASTEXITCODE

# ASCII-only parse (Windows PowerShell 5.1 reads .ps1 as ANSI, which would corrupt
# localized result strings): the automation runner prints "TEST COMPLETE. EXIT CODE: N"
# (N = failed-test count, 0 = all green) and one "Test Completed." per test.
$UeExit = 1; $Total = 0
if (Test-Path $Log) {
    $ec = Select-String -Path $Log -Pattern 'TEST COMPLETE\. EXIT CODE: (\d+)' | Select-Object -Last 1
    if ($ec) { $UeExit = [int]$ec.Matches[0].Groups[1].Value }
    $tot = (Select-String -Path $Log -Pattern 'Test Completed\.' -AllMatches |
            ForEach-Object { $_.Matches.Count } | Measure-Object -Sum).Sum
    if ($tot) { $Total = [int]$tot }
}
Write-Host ("       UE automation: {0} tests run, exit code {1} (process exit {2}; expected >= {3})" -f $Total, $UeExit, $UeRC, $ExpectedUeTests)

# v3.0.1 BLOCKER 2 audit: enforce that FFrameCoreGpuBacksubStrictTest actually
# executed (not silent SKIP) when FRAMECORE_GPU_STRICT=1 is set in the env. The
# test SKIPs cleanly (PASS) when env unset -- intentional for dev-box compile
# tests -- but under strict mode we demand the EXECUTED fingerprint in the log.
if ($env:FRAMECORE_GPU_STRICT -eq '1' -and (Test-Path $Log)) {
    $UeStrictExec = (Select-String -Path $Log -Pattern '\[F67s_UE\] STRICT_EXECUTED' -AllMatches | Measure-Object).Count
    $UeStrictSkip = (Select-String -Path $Log -Pattern '\[F67s_UE\] STRICT_SKIPPED'  -AllMatches | Measure-Object).Count
    if ($UeStrictExec -lt 1 -or $UeStrictSkip -ge 1) {
        Write-Host ("       UE strict enforcement FAIL: expected STRICT_EXECUTED in log; got exec={0} skip={1}" -f $UeStrictExec, $UeStrictSkip) -ForegroundColor Red
        $UeStrictRC = 1
    } else {
        Write-Host ("       UE strict enforcement: OK (STRICT_EXECUTED fingerprint observed)") -ForegroundColor Green
        $UeStrictRC = 0
    }
} else {
    $UeStrictRC = 0   # not in strict mode
}

# ---- [3/6] OpenSees offline cross-validation (#14; skipped if openseespy absent) ----
Write-Host ''
Write-Host '[3/6] OpenSees offline cross-validation...'
$OsRC = 0; $OsState = 'skipped'
& python (Join-Path $Root 'Tools\opensees_compare.py') | Tee-Object -Variable OsOut | Out-Null
$OsRC = $LASTEXITCODE
if ($OsRC -eq 0)     { $OsState = 'PASS' }
elseif ($OsRC -eq 2) { $OsState = 'skipped (openseespy not installed)' }
else                 { $OsState = 'FAIL' }
Write-Host ("       OpenSees compare: {0} (exit {1})" -f $OsState, $OsRC)

# ---- [4/6] linear-analysis deep audit (post F17-F25 strengthening) ----
Write-Host ''
Write-Host '[4/6] linear-analysis deep audit...'
& (Join-Path $Root 'Plugins\FrameSolver\Standalone\build_linear_audit.bat') | Tee-Object -Variable AuditOut | Out-Null
$AuditRC = $LASTEXITCODE
$AuditLine = ($AuditOut | Select-String -Pattern 'PASS failures=|FAIL failures=' | Select-Object -Last 1)
Write-Host ("       linear deep audit: {0} (exit {1})" -f $AuditLine, $AuditRC)

# ---- [5/6] CLI round-trip (S6 frame_cli J1 text bridge end-to-end) ----
Write-Host ''
Write-Host '[5/6] CLI round-trip (frame_cli J1 bridge)...'
& python (Join-Path $Root 'Tools\cli_roundtrip.py') | Tee-Object -Variable CliOut | Out-Null
$CliRC = $LASTEXITCODE
$CliLine = ($CliOut | Select-String -Pattern 'ALL PASS|FAILURES' | Select-Object -Last 1)
Write-Host ("       CLI round-trip: {0} (exit {1})" -f $CliLine, $CliRC)

# ---- [6/6] PIE auto-smoke (AS-35 — render thread required, no -nullrhi) ----
# DESIGN NOTE: run_pie_gate.ps1 uses Write-Host (Information stream, not pipeline).
# Capturing Write-Host across PowerShell script boundaries is unreliable — *>&1 causes
# NativeCommandError wrapping (from UE's stderr) to set $? = false and corrupt $LASTEXITCODE.
# Strategy: invoke the script with a dot-source-style call so its Write-Host goes directly
# to the parent console, and trust only $LASTEXITCODE (0 = PASS, 1 = FAIL) for gate logic.
# The PASS/FAIL summary is printed by run_pie_gate.ps1 itself; the gate verdict picks it up.
Write-Host ''
# AS-08-u2: run_pie_gate.ps1 now selects the whole ArchSim.PIE category
# (PortalFrameSmoke + SaveLoadSmoke). Display header updated accordingly.
Write-Host '[6/6] PIE auto-smoke (ArchSim.PIE.* — PortalFrameSmoke + SaveLoadSmoke)...'
& (Join-Path $Root 'Scripts\run_pie_gate.ps1') -Root $Root -Engine $Engine -UProject $UProj
$PieRC = $LASTEXITCODE
Write-Host ("       PIE smoke overall: {0} (exit {1})" -f $(if ($PieRC -eq 0) { 'PASS' } else { 'FAIL' }), $PieRC)

# ---- verdict ----
Write-Host ''
Write-Host '======================================================'
# OpenSees leg: a real diff (1) always fails. Absence (2) is a soft skip locally, but a hard
# fail under -RequireOpenSees (CI), so the cross-validation can never silently vanish.
if ($OsRC -eq 2) {
    if ($RequireOpenSees) { Write-Host '       OpenSees REQUIRED but openseespy absent -> FAIL' -ForegroundColor Red }
    else { Write-Host '       (OpenSees skipped: openseespy absent; pass -RequireOpenSees to enforce in CI)' -ForegroundColor Yellow }
}
$OsOk = if ($RequireOpenSees) { $OsRC -eq 0 } else { $OsRC -ne 1 }
$UeCountOk = ($Total -ge $ExpectedUeTests) -and ($UeStrictRC -eq 0)
$GateOk = ($StandaloneRC -eq 0) -and ($UeExit -eq 0) -and $UeCountOk -and $OsOk -and ($AuditRC -eq 0) -and ($CliRC -eq 0) -and ($PieRC -eq 0)
if ($GateOk) {
    Write-Host (" GATE: PASS  (standalone OK, UE {0} tests green, OpenSees {1}, deep audit OK, CLI round-trip OK, PIE smoke OK)" -f $Total, $OsState) -ForegroundColor Green
    exit 0
} else {
    Write-Host (" GATE: FAIL  (standalone exit {0}, UE exit {1}, {2}/{3} UE tests, OpenSees {4}, audit exit {5}, CLI exit {6}, PIE exit {7})" -f $StandaloneRC, $UeExit, $Total, $ExpectedUeTests, $OsState, $AuditRC, $CliRC, $PieRC) -ForegroundColor Red
    exit 1
}
