#!/usr/bin/env pwsh
<#
.SYNOPSIS
    AS-35-u2 / AS-42-u1: Leg 6 PIE auto-smoke gate — runs the whole ArchSim.PIE category
    WITHOUT -nullrhi.

.DESCRIPTION
    Invokes UnrealEditor-Cmd to run all ArchSim.PIE.* tests in a real render context
    (no -nullrhi), then parses automation results to verify EACH expected test by name.

    WHY a separate script (not folded into run_gate.ps1 leg 2):
      PIE tests use EditorContext + ClientContext and require FStartPIECommand, which starts
      a real Play-In-Editor session. Under -nullrhi the RHI thread is absent, causing
      FStartPIECommand to fail or produce a crash (observed 2026-06-28). Leg 2 runs all
      other ArchSim/FrameCore tests under -nullrhi; this leg runs only the PIE tests with
      the full render stack.

    AS-42-u1 hardening (#6):
      (a) Per-test verdict: each name in $ExpectedPieTests must have a success-verdict entry
          in the log. Missing or failed tests are reported by name → FAIL.
      (b) Screenshot freshness: screenshot LastWriteTime must be >= $RunStartTime (captures
          only this run's artifact; rejects stale files from prior runs).
      (c) Expected-test-names array at top: adding a new ArchSim.PIE.* test requires
          updating this array. Found-but-unexpected tests emit a warning; expected-but-
          missing tests emit FAIL. This keeps the gate intentionally loud as the category grows.

    This script is invoked by run_gate.ps1 as leg [6/6]. It can also be run standalone
    for isolation testing.

.PARAMETER Root
    Repo root path. Default: parent of this script's directory.

.PARAMETER Engine
    UE 5.7 engine root. Default: sibling UE_5.7/ directory or $env:UE_ENGINE_ROOT.

.PARAMETER UProject
    Absolute path to ArchSim.uproject. Default: $Root\ArchSim.uproject.

.EXAMPLE
    & "<repo-root>\Scripts\run_pie_gate.ps1" -Root "<repo-root>" -Engine "$env:UE_ENGINE_ROOT" -UProject "<repo-root>\ArchSim.uproject"

.NOTES
    Exit 0 = all expected PIE tests PASS + screenshot artifact fresh + size >= 1024 bytes.
    Exit 1 = any expected test missing/failed, stale/missing screenshot, or log parse error.
    Called by run_gate.ps1 leg [6/6]; do NOT pass -nullrhi to the UE commandlet here.
#>

param(
    [string]$Root    = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$Engine  = $env:UE_ENGINE_ROOT,
    [string]$UProject = ''
)

$ErrorActionPreference = 'Continue'

# ============================================================================
# AS-42-u1 (#6c): Expected PIE test names array.
#
# MAINTENANCE: When adding a new ArchSim.PIE.* test, add its full dotted name
# here. The gate verifies that EVERY name in this array ran AND passed.
# Found-but-unexpected tests emit a warning (won't FAIL gate; may indicate a
# rename). Expected-but-missing tests FAIL the gate with a clear message.
#
# Current expected tests (as of v0.6.1: 2 — PortalFrameSmoke + SaveLoadSmoke):
# ============================================================================
$ExpectedPieTests = @(
    'ArchSim.PIE.PortalFrameSmoke',
    'ArchSim.PIE.SaveLoadSmoke'
)

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

# AS-42-u1 (#6b): Record wall-clock time BEFORE launch so screenshot freshness can
# be verified. Screenshot files written BEFORE this time are stale artifacts from
# prior runs and must NOT satisfy the gate. Using UTC to be timezone-agnostic.
$RunStartTime = [DateTime]::UtcNow

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
# SECONDARY signal: per-test "Test Completed. Result={X}" lines — verified by name.
#
# LOCALE NOTE — why we do NOT use -match with Chinese characters in .ps1 source:
#   Windows PowerShell 5.1 reads .ps1 files as ANSI by default. Embedding Chinese
#   characters (e.g. "成功") directly in a regex literal stored in the .ps1 source
#   causes the characters to be mis-encoded as "??" at parse time, producing an
#   ArgumentException: "Unrecognized grouping construct."
#   Workaround: capture the raw result string with an ASCII-only regex, then use
#   string -eq comparisons for known English values, and fall back to the EXIT CODE
#   line (which is ASCII-only and unambiguous) for other locales.
$PieExitCode        = 1   # pessimistic default
$PieResultLine      = ''  # last result line seen (diagnostic)

# AS-42-u1 (#6a): per-test verdict tracking.
# Key = full test name (e.g. 'ArchSim.PIE.PortalFrameSmoke')
# Value = $true (success verdict seen) / $false (failed or not seen)
$TestVerdicts = @{}
foreach ($Name in $ExpectedPieTests) { $TestVerdicts[$Name] = $false }

# Tracks all test names seen in the log (for found-but-unexpected warning).
$SeenTestNames = [System.Collections.Generic.HashSet[string]]::new()

if (Test-Path $Log) {
    # 1) EXIT CODE line — authoritative PASS/FAIL signal (ASCII-only, locale-agnostic)
    #    "TEST COMPLETE. EXIT CODE: N" where N = number of failed tests (0 = all green)
    $ExitMatch = Select-String -Path $Log `
        -Pattern 'TEST COMPLETE\. EXIT CODE: (\d+)' |
        Select-Object -Last 1
    if ($ExitMatch) {
        $PieExitCode = [int]$ExitMatch.Matches[0].Groups[1].Value
    }

    # 2) Per-test "Test Completed" lines — verify each expected test by name.
    #
    # UE 5.7 automation log format (observed on CJK locale):
    #   "Test Completed. Result={成功} Name={PortalFrameSmoke} Path={ArchSim.PIE.PortalFrameSmoke}"
    # Older UE format (English locale, pre-UE5.7):
    #   "ArchSim.PIE.PortalFrameSmoke Test Completed. Result={Passed}"
    # CJK locale may produce a different Result value (e.g. Chinese "成功" / "失败").
    # Strategy: match the Path={} tag for the test name (locale-agnostic), and
    # capture the Result value with an ASCII-only regex. If the line contains
    # a known-failure token we count it as FAIL; otherwise PASS.
    # EXIT CODE remains the authoritative overall verdict.
    #
    # WHY match on "Test Completed" not "Test Started": a test that starts but is
    # killed mid-run will have a "Started" line but no "Completed" → correctly stays $false.
    #
    # LOCALE-DEFENSIVE: ASCII-only regex captures both Result and Path values;
    # comparison uses -eq / -contains on known ASCII strings; CJK result values
    # (e.g. "成功", "失败") fall through to the EXIT CODE authority which is always ASCII.
    # KNOWN FAIL values: CJK locale uses "失败" but we can't embed CJK in this .ps1
    # (ANSI parse corruption — see LOCALE NOTE above). So we use the EXIT CODE as
    # the authoritative fail signal; KnownFailValues only catches the English case.
    $KnownFailValues = @('Failed', 'Failure')

    # Match both formats by looking for "Test Completed" on the line, then extracting
    # Result and Path via separate named-group captures.
    $AllResultLines = Select-String -Path $Log `
        -Pattern 'Test Completed\. Result=\{([^}]+)\}' -ErrorAction SilentlyContinue
    foreach ($Match in $AllResultLines) {
        $Line        = $Match.Line.Trim()
        $ResultValue = $Match.Matches[0].Groups[1].Value

        # Try new UE5.7 format first: "... Path={ArchSim.PIE.X}"
        $TestName = $null
        if ($Line -match 'Path=\{(ArchSim\.PIE\.[^}]+)\}') {
            $TestName = $Matches[1]
        }
        # Fall back to old format: "ArchSim.PIE.X Test Completed."
        elseif ($Line -match 'ArchSim\.PIE\.(\S+)\s+Test Completed') {
            $TestName = 'ArchSim.PIE.' + $Matches[1]
        }

        if ($TestName) {
            [void]$SeenTestNames.Add($TestName)
            $PieResultLine = $Line  # remember last line for diagnostics

            $bFailed = ($KnownFailValues -contains $ResultValue)
            if ($TestVerdicts.ContainsKey($TestName)) {
                # Mark success only if result is not a known failure.
                # CJK "成功" is not in KnownFailValues so bFailed=$false → TestVerdicts=$true.
                # CJK "失败" is also not in KnownFailValues, BUT: UE will have returned
                # EXIT CODE != 0, and the belt-and-suspenders block below will override $PerTestPass.
                $TestVerdicts[$TestName] = (-not $bFailed)
            }
        }
    }
} else {
    Write-Host "PIE gate: log file not found: $Log" -ForegroundColor Red
}

# --- AS-42-u1 (#6a): per-test verdict report ---
$PerTestPass = $true
foreach ($Name in $ExpectedPieTests) {
    if ($TestVerdicts[$Name]) {
        Write-Host ("PIE gate: [PASS] $Name") -ForegroundColor Green
    } else {
        Write-Host ("PIE gate: [FAIL] $Name — no success verdict in log. " +
                    "Check that the test ran and completed without error.") -ForegroundColor Red
        $PerTestPass = $false
    }
}

# Warn about unexpected tests found in the log (not in $ExpectedPieTests).
# This is informational only — a new test added to the category but not yet
# in the array will produce a warning, not a gate failure.
# UPDATE $ExpectedPieTests above to silence the warning and make it a gate check.
foreach ($SeenName in $SeenTestNames) {
    if (-not ($ExpectedPieTests -contains $SeenName)) {
        Write-Host ("PIE gate: [WARN] Unexpected PIE test found in log: $SeenName. " +
                    "Add to `$ExpectedPieTests in run_pie_gate.ps1 to gate it.") `
            -ForegroundColor Yellow
    }
}

# Belt-and-suspenders: if EXIT CODE is non-zero, override any per-test verdicts
# that were marked success (shouldn't happen in practice, but guards log-parse bugs).
if ($PieExitCode -ne 0) {
    Write-Host ("PIE gate: EXIT CODE $PieExitCode indicates overall failure. " +
                "Overriding per-test verdicts to FAIL.") -ForegroundColor Red
    $PerTestPass = $false
}

# ---- screenshot verification ----
# Screenshot path written by the test: Saved/Screenshots/WindowsEditor/v0_5_x_pie_smoke*.png
# Accept any file >= 1024 bytes (a zero-byte or tiny file means screenshot call failed).
#
# MAINTENANCE NOTE (AS-08-u2 review #3): the screenshot is produced ONLY by
# ArchSim.PIE.PortalFrameSmoke (FSafeEditorScreenshotCommand). SaveLoadSmoke does
# not screenshot. If PortalFrameSmoke is ever removed or its screenshot path
# changes, this check must move/adapt or leg 6 will false-FAIL.
#
# AS-42-u1 (#6b): Screenshot FRESHNESS check.
# We require the screenshot LastWriteTimeUtc >= $RunStartTime (recorded before launch).
# This prevents a stale artifact from a prior run satisfying the gate:
#   Adversarial scenario: previous run wrote v0_5_x_pie_smoke_2026...png; current run
#   crashes before the screenshot stage. Old file still exists, passes size check → false PASS.
#   With freshness: old file LastWriteTimeUtc < $RunStartTime → ScreenshotFresh = $false → FAIL.
$ScreenshotPattern = Join-Path $Root 'Saved\Screenshots\WindowsEditor\v0_5_x_pie_smoke*.png'
$LatestScreenshot  = Get-ChildItem $ScreenshotPattern -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTimeUtc -Descending |
    Select-Object -First 1

$ScreenshotSizeOk  = $LatestScreenshot -and ($LatestScreenshot.Length -ge 1024)
$ScreenshotFresh   = $LatestScreenshot -and
                     ($LatestScreenshot.LastWriteTimeUtc -ge $RunStartTime)
$ScreenshotOk      = $ScreenshotSizeOk -and $ScreenshotFresh

if ($LatestScreenshot -and $ScreenshotSizeOk -and (-not $ScreenshotFresh)) {
    Write-Host ("PIE gate: STALE SCREENSHOT detected — file LastWriteTimeUtc={0} is before " +
                "RunStartTime={1}. File: {2}. This artifact is from a prior run. " +
                "PortalFrameSmoke must have crashed before the screenshot stage. " +
                "Treating as FAIL.") -f `
        $LatestScreenshot.LastWriteTimeUtc.ToString('o'), `
        $RunStartTime.ToString('o'), `
        $LatestScreenshot.FullName `
        -ForegroundColor Red
}

# ---- verdict ----
# PRIMARY pass conditions (ALL must be true):
#   (a) EXIT CODE = 0 (authoritative, locale-agnostic overall verdict)
#   (b) Every expected test in $ExpectedPieTests has a success verdict in the log
#       ($PerTestPass; AS-42-u1 #6a)
#   (c) Screenshot artifact is fresh (LastWriteTimeUtc >= $RunStartTime) AND >= 1024 bytes
#       ($ScreenshotOk; AS-42-u1 #6b + pre-existing size check)
#
# NOTE: $PerTestPass already incorporates EXIT CODE override (see above).
$GatePassed = ($PieExitCode -eq 0) -and $PerTestPass -and $ScreenshotOk

if ($GatePassed) {
    $SsInfo = if ($LatestScreenshot) { ("{0} bytes, written {1}" -f $LatestScreenshot.Length, $LatestScreenshot.LastWriteTimeUtc.ToString('o')) } else { "N/A" }
    Write-Host ("PIE smoke: PASS (exit {0}; all {1} expected tests passed; screenshot={2})" -f `
        $PieExitCode, $ExpectedPieTests.Count, $SsInfo) -ForegroundColor Green
    if ($PieResultLine) {
        Write-Host ("       Last result line: $PieResultLine")
    }
    exit 0
} else {
    $ScreenshotInfo = if ($LatestScreenshot) {
        ("{0} bytes; fresh={1}; path={2}" -f `
            $LatestScreenshot.Length, $ScreenshotFresh, $LatestScreenshot.FullName)
    } else {
        "MISSING (pattern: $ScreenshotPattern)"
    }
    Write-Host ("PIE smoke: FAIL (exit_code={0}, per_test_pass={1}, screenshot_ok={2}; ss={3})" -f `
        $PieExitCode, $PerTestPass, $ScreenshotOk, $ScreenshotInfo) -ForegroundColor Red
    if ($PieResultLine) {
        Write-Host ("       Last result line: $PieResultLine")
    }
    exit 1
}
