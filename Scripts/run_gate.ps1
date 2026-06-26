# One-click verification gate for the FrameSolver engine (FrameCore-only, post-cleanup).
#   [1/5] standalone FrameCore fixtures (analytic golden oracles)
#   [2/5] UE headless automation (FrameCore.*)
#   [3/5] OpenSees offline cross-validation (skipped if openseespy absent)
#   [4/5] linear-analysis deep audit (post F17-F25 strengthening; prints its own
#         independent-check count -- the audit reports "checks=N", not a hardcoded number)
#   [5/5] CLI round-trip (S6: frame_cli J1 text bridge end-to-end; VERSION/TONLY/SIZEOPT/DYNC)
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
param(
    [switch]$RequireOpenSees,       # CI: fail (not skip) when openseespy is absent
    [int]$ExpectedUeTests = 141,     # S-03 AS-17 audit: +1 FrameCore.UE.EmptyModelStartSession (AS-17 C-02 audit oracle; 8 sub-checks: fully-empty StartSession returns false / OutError non-empty / IsSessionActive false; double EndSession idempotent; partial-empty also gracefully fails; valid model after failures recovers) (total 141; non-cuDSS fallback 139). v0.2.0 game body: +1 ArchSim.Gameplay.CharacterInput (AArchSimCharacter + AArchSimGameMode headless smoke; 7 sub-checks: class hierarchy AAlsCharacter/ACharacter/APawn; AArchSimGameMode inherits AGameModeBase; DefaultPawnClass wire; bUseControllerRotation* false from AS-03a; UAlsCameraComponent default subobject from AS-03c; Enhanced Input UPROPERTY slots null in CDO; LogArchSim resolves; reflection names. Full input + locomotion deferred to AS-13 PIE fixture) (total 140; non-cuDSS fallback 138). v0.1.5 game body: +1 ArchSim.Integration.TickDriver (UArchSimGameInstance Tick telemetry + IsTickable filter smoke; 7 sub-checks; headless cannot exercise the full registry-delta driver-loop branch — deferred to PIE-world fixture) (total 139; non-cuDSS fallback 137). v0.1.4 game body: +1 ArchSim.Persistence.RebaselineCeiling (pins strict `>` semantic of MaxRankBeforeRebaseline=96 in RequestSolve cpp:281; in headless NewObject fixture the trip path is unreachable (GI-null early-return at cpp:275), so test pins accumulator math and const-getter purity; 7 sub-checks: constexpr==96 / initial-state / boundary-96-no-trip / at-97-accum-grows-no-flag / getter-purity / multi-rank-single-patch / empty-patch-no-op) (total 138; non-cuDSS fallback 136). v0.1.3 game body: +1 ArchSim.Persistence.MaxRankCeiling (97-register stress; pins true production semantic that RegisterMember has NO register-count ceiling and MaxRankBeforeRebaseline=96 bounds PendingRankAccumulation in RequestSolve, not registry size) (total 137; non-cuDSS fallback 135). v0.1.1 game body: +1 ArchSim.Persistence.SaveLoadRoundTrip (total 136; non-cuDSS fallback 134). v4.0.0 stable seal: counts unchanged from v3.6.0 (engine FROZEN; UE consumer surface frozen for this seal). On a box without cuDSS (FRAMECORE_CUDA=0 build), both GPU tests (F67/F67s) compile out -- pass `-ExpectedUeTests 139` so the count guard matches. v3.6 Phase 1-8: +15 final-release tests (Hermite 3 / LoadPatch 2 / InternalForceField 4 / UtilField 3 / Redundancy 2 / InfluencePolarity 1). v3.5 Phase 1-8: +22 visual surface tests (Phase 7 +3 InteractiveSubsystem: Lifetime / PatchSemantics / PerfBaseline). v3.4 Phase 1-5: see git log for individual test additions.
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$Engine = $env:UE_ENGINE_ROOT,
    [string]$UProject = ''
)
$ErrorActionPreference = 'Continue'

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

Write-Host '======================================================'
Write-Host ' FrameSolver verification gate'
Write-Host '======================================================'

# ---- [1/5] standalone gate ----
Write-Host ''
Write-Host '[1/5] standalone FrameCore gate (build.bat)...'
& (Join-Path $Root 'Plugins\FrameSolver\Standalone\build.bat') | Tee-Object -Variable StandaloneOut | Out-Null
$StandaloneRC = $LASTEXITCODE
$StandaloneLine = ($StandaloneOut | Select-String -Pattern 'ALL PASS|FAILURES' | Select-Object -Last 1)
Write-Host ("       standalone: {0} (exit {1})" -f $StandaloneLine, $StandaloneRC)

# ---- [2/5] UE headless automation ----
Write-Host ''
Write-Host '[2/5] UE headless automation...'
$ExecCmds = 'Automation RunTests FrameCore+ArchSim; Quit'
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

# ---- [3/5] OpenSees offline cross-validation (#14; skipped if openseespy absent) ----
Write-Host ''
Write-Host '[3/5] OpenSees offline cross-validation...'
$OsRC = 0; $OsState = 'skipped'
& python (Join-Path $Root 'Tools\opensees_compare.py') | Tee-Object -Variable OsOut | Out-Null
$OsRC = $LASTEXITCODE
if ($OsRC -eq 0)     { $OsState = 'PASS' }
elseif ($OsRC -eq 2) { $OsState = 'skipped (openseespy not installed)' }
else                 { $OsState = 'FAIL' }
Write-Host ("       OpenSees compare: {0} (exit {1})" -f $OsState, $OsRC)

# ---- [4/5] linear-analysis deep audit (post F17-F25 strengthening) ----
Write-Host ''
Write-Host '[4/5] linear-analysis deep audit...'
& (Join-Path $Root 'Plugins\FrameSolver\Standalone\build_linear_audit.bat') | Tee-Object -Variable AuditOut | Out-Null
$AuditRC = $LASTEXITCODE
$AuditLine = ($AuditOut | Select-String -Pattern 'PASS failures=|FAIL failures=' | Select-Object -Last 1)
Write-Host ("       linear deep audit: {0} (exit {1})" -f $AuditLine, $AuditRC)

# ---- [5/5] CLI round-trip (S6 frame_cli J1 text bridge end-to-end) ----
Write-Host ''
Write-Host '[5/5] CLI round-trip (frame_cli J1 bridge)...'
& python (Join-Path $Root 'Tools\cli_roundtrip.py') | Tee-Object -Variable CliOut | Out-Null
$CliRC = $LASTEXITCODE
$CliLine = ($CliOut | Select-String -Pattern 'ALL PASS|FAILURES' | Select-Object -Last 1)
Write-Host ("       CLI round-trip: {0} (exit {1})" -f $CliLine, $CliRC)

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
$GateOk = ($StandaloneRC -eq 0) -and ($UeExit -eq 0) -and $UeCountOk -and $OsOk -and ($AuditRC -eq 0) -and ($CliRC -eq 0)
if ($GateOk) {
    Write-Host (" GATE: PASS  (standalone OK, UE {0} tests green, OpenSees {1}, deep audit OK, CLI round-trip OK)" -f $Total, $OsState) -ForegroundColor Green
    exit 0
} else {
    Write-Host (" GATE: FAIL  (standalone exit {0}, UE exit {1}, {2}/{3} UE tests, OpenSees {4}, audit exit {5}, CLI exit {6})" -f $StandaloneRC, $UeExit, $Total, $ExpectedUeTests, $OsState, $AuditRC, $CliRC) -ForegroundColor Red
    exit 1
}
