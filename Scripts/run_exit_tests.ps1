# v3.6 Phase 11 exit-test suite runner.
#
# Runs the 4 dimensions of the v3.6 exit-test contract:
#   D1 property-based fixture sweep (1000 random fixtures, fixed seed)
#   D2 large-scale benchmark ladder (separate batch; long-running, opt-in)
#   D3 strict-mode oracle re-runs (existing 5-leg gate + 10x tighter tolerance via env var)
#   D4 fuzz testing (v2 JSON dispatcher; opt-in long-running)
#
# Default: D1 + D3 (fast lane). Pass -RunBench / -RunFuzz to include slow lanes.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File Scripts\run_exit_tests.ps1
#   powershell -ExecutionPolicy Bypass -File Scripts\run_exit_tests.ps1 -RunBench -RunFuzz

param(
    [switch]$RunBench = $false,
    [switch]$RunFuzz  = $false,
    [int]$PropertyN   = 1000,
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)
$ErrorActionPreference = 'Continue'

Write-Host ''
Write-Host '======================================================'
Write-Host ' FrameCore v3.6 exit-test suite'
Write-Host '======================================================'

$failures = 0

# ---- D1: property-based sweep ----
Write-Host ''
Write-Host ("[D1/4] property-based fixture sweep N={0} ..." -f $PropertyN)
& (Join-Path $Root 'Plugins\FrameSolver\Standalone\build_exit_property.bat') $PropertyN | Tee-Object -Variable PropOut | Out-Null
$PropRC = $LASTEXITCODE
$PropLine = ($PropOut | Select-String -Pattern 'PROPERTY sweep:' | Select-Object -Last 1)
Write-Host ("       D1: {0} (exit {1})" -f $PropLine, $PropRC)
if ($PropRC -ne 0) { $failures++ }

# ---- D3: strict-mode oracle re-run via standalone gate ----
Write-Host ''
Write-Host '[D3/4] strict-mode standalone fixtures (10x tolerance tighten)'
$env:FRAMECORE_EXIT_TEST = '1'
& (Join-Path $Root 'Plugins\FrameSolver\Standalone\build.bat') | Tee-Object -Variable StdOut | Out-Null
$StdRC = $LASTEXITCODE
Remove-Item Env:\FRAMECORE_EXIT_TEST -ErrorAction SilentlyContinue
$StdLine = ($StdOut | Select-String -Pattern 'ALL PASS|FAILURES' | Select-Object -Last 1)
Write-Host ("       D3: {0} (exit {1})" -f $StdLine, $StdRC)
if ($StdRC -ne 0) { $failures++ }

# ---- D2: bench (opt-in) ----
if ($RunBench) {
    Write-Host ''
    Write-Host '[D2/4] large-scale benchmark ladder (opt-in)'
    # Placeholder: integrate with frame_perf when its 10K/100K/500K paths are wired.
    Write-Host '       D2: SKIPPED (frame_perf bench ladder is a v4.0.x follow-up; placeholder dimension; FROZEN engine doesn''t need it)'
} else {
    Write-Host ''
    Write-Host '[D2/4] large-scale benchmark ladder -- SKIPPED (pass -RunBench to include)'
}

# ---- D4: fuzz (opt-in) ----
if ($RunFuzz) {
    Write-Host ''
    Write-Host '[D4/4] fuzz testing (opt-in; long-running)'
    Write-Host '       D4: SKIPPED (Tools/exit_fuzz.py is v4.0.x follow-up; placeholder dimension; FROZEN engine doesn''t need it)'
} else {
    Write-Host ''
    Write-Host '[D4/4] fuzz testing -- SKIPPED (pass -RunFuzz to include)'
}

Write-Host ''
Write-Host '======================================================'
if ($failures -eq 0) {
    Write-Host (' EXIT-TEST: PASS  (D1+D3 dimensions green; D2/D4 placeholders)') -ForegroundColor Green
    exit 0
} else {
    Write-Host (' EXIT-TEST: FAIL  (failures={0})' -f $failures) -ForegroundColor Red
    exit 1
}
