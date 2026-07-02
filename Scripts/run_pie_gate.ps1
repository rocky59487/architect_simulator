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

# NIT 3: record log timestamp + length BEFORE launch so we can detect a stale log later.
# WHY: if UnrealEditor-Cmd exits before writing the log (e.g. DLL load failure),
# the parser reads the PREVIOUS session's log and may return a false PASS.
# MinValue / -1 = sentinels for "log file did not exist before this run".
# WHY length too (v0.5.3): timestamp-only comparison produced a same-timestamp
# false FAIL on a real run (NTFS write-time granularity/caching; predicted by the
# NITS-u1 review, observed same day during AS-37-u2). A genuinely stale log has
# BOTH unchanged time AND unchanged length; a real UE run always changes length.
$PreRunStampUtc = [DateTime]::MinValue
$PreRunLength   = -1

# AS-08-u2: Pre-run log cleanup to guarantee UE writes to ArchSim.log (not ArchSim_2.log etc).
# WHY: UE 5.7 appends _2, _3 suffix when it finds an existing ArchSim.log at startup.
# run_pie_gate.ps1 hardcodes reading Saved\Logs\ArchSim.log. If a previous UE session left
# ArchSim.log and ArchSim_2.log on disk, the new run may write to ArchSim_2.log while we
# parse ArchSim.log (wrong file) → false FAIL. Fix: remove the live logs before launch so UE
# always starts fresh with ArchSim.log. Backup logs (ArchSim-backup-*.log) are not affected.
$LogDir = Split-Path $Log -Parent
Get-ChildItem -Path $LogDir -Filter "ArchSim*.log" -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -notmatch '-backup-' } |
    ForEach-Object {
        Write-Host ("PIE gate: removing pre-existing log: {0}" -f $_.Name)
        Remove-Item -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue
    }

if (Test-Path -LiteralPath $Log) {
    $PreRunItem     = Get-Item -LiteralPath $Log
    $PreRunStampUtc = $PreRunItem.LastWriteTimeUtc
    $PreRunLength   = $PreRunItem.Length
}
# NOTE (AS-08-u2 review #2): after the cleanup above the live log usually does NOT
# exist, so $PreRunStampUtc stays MinValue / $PreRunLength stays -1 — that is the
# expected "fresh" sentinel, and the post-run Test-Path guard below covers the
# "UE died before writing anything" case. Not a stale-guard hole.

Write-Host "PIE gate: launching UnrealEditor-Cmd for ArchSim.PIE (all PIE tests)..."
# AS-08-u2: changed from single test to whole ArchSim.PIE category.
# RunTests accepts '+'-delimited names OR a single category prefix that matches all
# tests in the category. "ArchSim.PIE" matches ArchSim.PIE.PortalFrameSmoke AND
# ArchSim.PIE.SaveLoadSmoke (and any future ArchSim.PIE.* additions).
# WHY not '+'-join: category prefix is cleaner for an open-ended test namespace.
# The authoritative PASS signal is still EXIT CODE: 0 (all tests passed).
& $UeCmd $UProj `
    '-ExecCmds=Automation RunTests ArchSim.PIE; Quit' `
    -unattended -nopause -nosplash -log | Out-Null
# WHY | Out-Null (NativeCommandError discipline — same pattern as run_gate.ps1 leg 2):
#   PowerShell 5.1 pipelines native exe stdout/stderr through the PS object pipeline.
#   Any stderr output gets wrapped as NativeCommandError objects, which (a) pollute
#   $LASTEXITCODE / $?, and (b) can trigger Tee-Object / *>&1 redirect issues that
#   set $? = $false even when the exe returned exit code 0 (observed with UE log
#   warning lines going to stderr). Silencing stdout here is safe because ALL
#   authoritative test signals are in Saved\Logs\ArchSim.log, not stdout.
#   See also: LOCALE NOTE below (parse section) for the parallel CJK regex
#   constraint that prevents inline stdout parsing anyway.

# NIT 3 (AS-08-u2 extended): verify log was actually written by THIS run.
# When multiple UE sessions run in parallel or back-to-back, UE may write to
# ArchSim_2.log, ArchSim_3.log, etc. instead of ArchSim.log (suffix appended
# when the base name already exists on disk). Scan ALL non-backup ArchSim*.log
# files written AFTER the pre-run cleanup, pick the one containing THIS run's
# ExecCmds line (or fall back to the newest file by mtime).
$LogDir = Split-Path $Log -Parent
$CandidateLogs = Get-ChildItem -Path $LogDir -Filter "ArchSim*.log" -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -notmatch '-backup-' } |
    Sort-Object LastWriteTimeUtc -Descending

# Find the best candidate: prefer a file that contains the test result or the ExecCmds marker.
$ActiveLog = $null
foreach ($Candidate in $CandidateLogs) {
    $HasResult = (Select-String -Path $Candidate.FullName `
        -Pattern 'TEST COMPLETE\. EXIT CODE:' -ErrorAction SilentlyContinue |
        Select-Object -First 1)
    if ($HasResult) {
        $ActiveLog = $Candidate.FullName
        Write-Host ("PIE gate: selected result log: {0}" -f $Candidate.Name)
        break
    }
}
if (-not $ActiveLog) {
    # Fallback: newest log file (most recently written by UE).
    if ($CandidateLogs) {
        $ActiveLog = $CandidateLogs[0].FullName
        Write-Host ("PIE gate: no result-bearing log found; using newest: {0}" -f $CandidateLogs[0].Name)
    }
}

# Override $Log for all subsequent parse/stale-guard steps.
if ($ActiveLog) { $Log = $ActiveLog }

if (Test-Path -LiteralPath $Log) {
    $PostRunItem     = Get-Item -LiteralPath $Log
    $PostRunStampUtc = $PostRunItem.LastWriteTimeUtc
    $PostRunLength   = $PostRunItem.Length
    if (($PostRunStampUtc -le $PreRunStampUtc) -and ($PostRunLength -eq $PreRunLength)) {
        Write-Host ("PIE gate: STALE LOG detected -- log was not updated by this run. " +
                    "pre=$($PreRunStampUtc.ToString('o'))/len=$PreRunLength " +
                    "post=$($PostRunStampUtc.ToString('o'))/len=$PostRunLength. " +
                    "UnrealEditor-Cmd may have crashed before writing. Treating as FAIL.") `
            -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "PIE gate: log file missing after run: $Log" -ForegroundColor Red
    exit 1
}

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
# MAINTENANCE NOTE (AS-08-u2 review #3): the screenshot is produced ONLY by
# ArchSim.PIE.PortalFrameSmoke (FSafeEditorScreenshotCommand). SaveLoadSmoke does
# not screenshot. If PortalFrameSmoke is ever removed or its screenshot path
# changes, this check must move/adapt or leg 6 will false-FAIL.
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
