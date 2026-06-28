#!/usr/bin/env pwsh
<#
.SYNOPSIS
    AS-35-u2: Leg 6 PIE auto-smoke gate — runs ArchSim.PIE.PortalFrameSmoke WITHOUT -nullrhi.

.DESCRIPTION
    Invokes UnrealEditor-Cmd to run the PIE smoke test (ArchSim.PIE.PortalFrameSmoke) in a
    real render context (no -nullrhi), then parses the automation result from the UE log.

    WHY a separate script (not folded into run_gate.ps1 leg 2):
      The PIE smoke test uses EditorContext + ClientContext and requires FStartPIECommand,
      which starts a real Play-In-Editor session. Under -nullrhi the RHI thread is absent,
      causing FStartPIECommand to fail or produce a crash (observed in Saved/Logs/ArchSim.log
      at 2026-06-28 09:53:41 EXCEPTION_ACCESS_VIOLATION under headless nullrhi run).
      Leg 2 runs all other ArchSim/FrameCore tests under -nullrhi; this leg runs only the
      PIE test with the full render stack.

    This script is invoked by run_gate.ps1 as leg [6/6]. It can also be run standalone
    for isolation testing.

.PARAMETER Root
    Repo root path. Default: parent of this script's directory.

.PARAMETER Engine
    UE 5.7 engine root. Default: sibling UE_5.7/ directory or $env:UE_ENGINE_ROOT.

.PARAMETER UProject
    Absolute path to ArchSim.uproject. Default: $Root\ArchSim.uproject.

.EXAMPLE
    & "E:\project\ArchSim\Scripts\run_pie_gate.ps1" -Root "E:\project\ArchSim" -Engine "E:\project\UE_5.7" -UProject "E:\project\ArchSim\ArchSim.uproject"

.NOTES
    Exit 0 = PIE test PASS + screenshot artifact >= 1024 bytes.
    Exit 1 = PIE test FAIL, log parse error, or missing/tiny screenshot.
    Called by run_gate.ps1 leg [6/6]; do NOT pass -nullrhi to the UE commandlet here.
#>

param(
    [string]$Root    = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$Engine  = $env:UE_ENGINE_ROOT,
    [string]$UProject = ''
)

$ErrorActionPreference = 'Continue'

# ---- resolve paths ----
$RootPath = Resolve-Path -LiteralPath $Root -ErrorAction SilentlyContinue
if (-not $RootPath) { Write-Host "PIE gate: repo root not found: $Root" -ForegroundColor Red; exit 1 }
$Root = $RootPath.Path

if ([string]::IsNullOrWhiteSpace($Engine)) {
    $Candidate = Join-Path (Split-Path $Root -Parent) 'UE_5.7'
    if (Test-Path -LiteralPath $Candidate) { $Engine = $Candidate }
}
$EnginePath = Resolve-Path -LiteralPath $Engine -ErrorAction SilentlyContinue
if (-not $EnginePath) {
    Write-Host "PIE gate: UE engine root not found. Pass -Engine <UE root> or set UE_ENGINE_ROOT." -ForegroundColor Red
    exit 1
}
$Engine = $EnginePath.Path

$UProj = if ([string]::IsNullOrWhiteSpace($UProject)) { Join-Path $Root 'ArchSim.uproject' } else { $UProject }
$UeCmd = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$Log   = Join-Path $Root 'Saved\Logs\ArchSim.log'

if (-not (Test-Path $UeCmd)) {
    Write-Host "PIE gate: UnrealEditor-Cmd.exe not found at: $UeCmd" -ForegroundColor Red
    exit 1
}
if (-not (Test-Path $UProj)) {
    Write-Host "PIE gate: ArchSim.uproject not found at: $UProj" -ForegroundColor Red
    exit 1
}

# ---- run PIE test (NO -nullrhi — render thread required for FStartPIECommand) ----
# WHY no -nullrhi: EditorContext | ClientContext test uses FStartPIECommand which
# requires a real PIE world with render context. Under -nullrhi the RHI thread is
# absent and the test crashes (observed 2026-06-28). Leg 2 of run_gate.ps1 is
# explicitly filtered to exclude ArchSim.PIE.* for this reason.
Write-Host "PIE gate: launching UnrealEditor-Cmd for ArchSim.PIE.PortalFrameSmoke..."
& $UeCmd $UProj `
    '-ExecCmds=Automation RunTests ArchSim.PIE.PortalFrameSmoke; Quit' `
    -unattended -nopause -nosplash -log | Out-Null

# ---- parse log for test result ----
# PRIMARY signal: "TEST COMPLETE. EXIT CODE: N" (N=0 = all tests passed).
# SECONDARY signal: "Test Completed. Result={X}" — captures the result string.
#
# LOCALE NOTE — why we do NOT use -match with Chinese characters in .ps1 source:
#   Windows PowerShell 5.1 reads .ps1 files as ANSI by default. Embedding Chinese
#   characters (e.g. "成功") directly in a regex literal stored in the .ps1 source
#   causes the characters to be mis-encoded as "??" at parse time, producing an
#   ArgumentException: "Unrecognized grouping construct."
#   Workaround: capture the raw result string with an ASCII-only regex, then use
#   string -eq comparisons for known English values, and fall back to the EXIT CODE
#   line (which is ASCII-only and unambiguous) for other locales.
$PieTestResultFound = $false
$PieExitCode        = 1   # pessimistic default
$PieResultLine      = ''

if (Test-Path $Log) {
    # 1) EXIT CODE line — authoritative PASS/FAIL signal (ASCII-only, locale-agnostic)
    #    "TEST COMPLETE. EXIT CODE: N" where N = number of failed tests (0 = all green)
    $ExitMatch = Select-String -Path $Log `
        -Pattern 'TEST COMPLETE\. EXIT CODE: (\d+)' |
        Select-Object -Last 1
    if ($ExitMatch) {
        $PieExitCode = [int]$ExitMatch.Matches[0].Groups[1].Value
    }

    # 2) Test Completed line — secondary; captured for diagnostic output only
    #    Regex captures the result value with an ASCII-only pattern; the captured
    #    $ResultValue may be a CJK string (e.g. Chinese "成功") on this host.
    #    We do NOT embed CJK in a regex literal (see LOCALE NOTE above).
    $ResultMatch = Select-String -Path $Log `
        -Pattern 'Test Completed\. Result=\{([^}]+)\}' |
        Select-Object -Last 1
    if ($ResultMatch) {
        $PieTestResultFound = $true
        $PieResultLine      = $ResultMatch.Line.Trim()
        # For English-locale hosts: detect explicit failure strings via -eq (not regex)
        $ResultValue = $ResultMatch.Matches[0].Groups[1].Value
        $KnownFailValues = @('Failed', 'Failure')
        if ($KnownFailValues -contains $ResultValue) {
            # Override EXIT CODE if result is explicitly "Failed" — belt-and-suspenders
            $PieExitCode = [Math]::Max($PieExitCode, 1)
        }
        # For Chinese locale ("成功" / "失敗"): defer to EXIT CODE (already set above)
    }
} else {
    Write-Host "PIE gate: log file not found: $Log" -ForegroundColor Red
}

# ---- screenshot verification ----
# Screenshot path written by the test: Saved/Screenshots/WindowsEditor/v0_5_x_pie_smoke*.png
# Accept any file >= 1024 bytes (a zero-byte or tiny file means screenshot call failed).
$ScreenshotPattern = Join-Path $Root 'Saved\Screenshots\WindowsEditor\v0_5_x_pie_smoke*.png'
$LatestScreenshot  = Get-ChildItem $ScreenshotPattern -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
$ScreenshotOk = $LatestScreenshot -and ($LatestScreenshot.Length -ge 1024)

# ---- verdict ----
# PRIMARY pass condition: EXIT CODE = 0 (authoritative, locale-agnostic)
# SECONDARY pass condition: screenshot artifact >= 1024 bytes (visual render evidence)
# "Test Completed. Result={X}" line must be present (confirms test actually ran, not skipped)
$GatePassed = ($PieExitCode -eq 0) -and $PieTestResultFound -and $ScreenshotOk

if ($GatePassed) {
    Write-Host ("PIE smoke: PASS (exit {0}; screenshot={1} bytes)" -f `
        $PieExitCode, `
        $LatestScreenshot.Length) -ForegroundColor Green
    if ($PieResultLine) {
        Write-Host ("       Result line: $PieResultLine")
    }
    exit 0
} else {
    $ScreenshotInfo = if ($LatestScreenshot) {
        ("{0} bytes at {1}" -f $LatestScreenshot.Length, $LatestScreenshot.FullName)
    } else {
        "MISSING (pattern: $ScreenshotPattern)"
    }
    Write-Host ("PIE smoke: FAIL (exit_code={0}, result_found={1}, screenshot_ok={2}; ss={3})" -f `
        $PieExitCode, $PieTestResultFound, $ScreenshotOk, $ScreenshotInfo) -ForegroundColor Red
    if ($PieResultLine) {
        Write-Host ("       Last result line: $PieResultLine")
    }
    exit 1
}
