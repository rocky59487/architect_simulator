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
    [int]$ExpectedUeTests = 70,      # v3.2.0 Phase 6 closeout: +1 FFrameCoreUEAxialColumnTest (F4 vertical column refVec degeneracy fallback; |N| constancy along member + Vy/Vz/Mz~0 invariants under pure axial load). v3.2.0 Phase 6f: +1 FFrameCoreUEThetaRangeTest (40 shell sample points; principal stress ThetaRad in (-π/2, π/2] invariant per v3.1.0 A-09 audit); +1 FFrameCoreUEZeroLoadTest (P=0 zero-load fixture; all sample sigmas exactly 0, no NaN, sentinels honour engine convention). v3.2.0 Phase 6e: +1 FFrameCoreUEEditorTabSpawnerTest (closes U-04: asserts nomad tab spawner registered in StartupModule -- catches WorkspaceMenuStructure API drift); +1 FFrameCoreUERobustnessTest (SamplesPerSpan<2 clamp / 20-member marshal scaling / 100x repeat bit-exact). v3.2.0 Phase 6a: +3 FrameCoreUE marshal coverage (MarshalSSBeamTest = SS beam UDL incl. internal forces; MarshalShellPlateTest = clamped plate shellsTop/shellsBot + 4-corner + bIsTopLayer; MarshalMultiMemberTest = 3-member frame, user IDs 100/200/300 preserved). v3.2.0 (Phase 3): +1 FFrameCoreUEEditorSmokeTest (Slate panel construct + OnComputeClicked under #if WITH_EDITOR). v3.2.0 (Phase 2): +1 FFrameCoreUEBlueprintSmokeTest (BP exposure smoke; F68 cantilever POD oracle rel<1e-5 float lossy). v3.1.0 (S11): +1 FFrameCoreStressFieldTest (UE F68 mirror + D/C interlock). v2.11.1-RC: +1 FFrameCoreGpuBacksubStrictTest (UE F67s mirror, fails on silent CPU fallback). v2.11 Phase 7: +1 FFrameCoreGpuBacksubTest (UE F67 mirror, smoke). Previously 57: S10 +1; supernodal opt-in +1; SnSession +1; shell K_sigma buckling +1; shell CR rotation invariance +1; warped MITC4 admit +1; shell knockdown +1; shell curvature guard +1. On a box without cuDSS (FRAMECORE_CUDA=0 build), both GPU tests compile out -- pass `-ExpectedUeTests 68` so the count guard matches.
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

# ---- [1/3] standalone gate ----
Write-Host ''
Write-Host '[1/5] standalone FrameCore gate (build.bat)...'
& (Join-Path $Root 'Plugins\FrameSolver\Standalone\build.bat') | Tee-Object -Variable StandaloneOut | Out-Null
$StandaloneRC = $LASTEXITCODE
$StandaloneLine = ($StandaloneOut | Select-String -Pattern 'ALL PASS|FAILURES' | Select-Object -Last 1)
Write-Host ("       standalone: {0} (exit {1})" -f $StandaloneLine, $StandaloneRC)

# ---- [2/3] UE headless automation ----
Write-Host ''
Write-Host '[2/5] UE headless automation...'
$ExecCmds = 'Automation RunTests FrameCore; Quit'
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

# ---- [3/3] OpenSees offline cross-validation (#14; skipped if openseespy absent) ----
Write-Host ''
Write-Host '[3/5] OpenSees offline cross-validation...'
$OsRC = 0; $OsState = 'skipped'
& python (Join-Path $Root 'Tools\opensees_compare.py') | Tee-Object -Variable OsOut | Out-Null
$OsRC = $LASTEXITCODE
if ($OsRC -eq 0)     { $OsState = 'PASS' }
elseif ($OsRC -eq 2) { $OsState = 'skipped (openseespy not installed)' }
else                 { $OsState = 'FAIL' }
Write-Host ("       OpenSees compare: {0} (exit {1})" -f $OsState, $OsRC)

# ---- [4/4] linear-analysis deep audit (post F17-F25 strengthening) ----
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
