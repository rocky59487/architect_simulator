# run_supernodal.ps1 - research-only sweep: CHOLMOD/METIS vs SimplicialLDLT on mixed buildings.
#   One process per scale (clean peak memory). Logs to Research\out\supernodal_sweep.txt.
#   Small/mid scales run all 3 solvers; large scales run CHOLMOD only (simplicial factor wall).
#   Run build_supernodal.bat compare first to produce the exe.
param([int]$reps = 31)

$ErrorActionPreference = 'Continue'
$bin = Join-Path $PSScriptRoot '..\bin\exp_supernodal_compare.exe'
if (-not (Test-Path $bin)) { Write-Error "exe not found: $bin (run build_supernodal.bat compare first)"; exit 1 }

# CHOLMOD/METIS DLLs live in the conda env bin
$condaBin = Join-Path $env:USERPROFILE 'anaconda3\envs\framecore-direct\Library\bin'
$env:PATH = "$condaBin;$env:PATH"

$outDir = Join-Path $PSScriptRoot '..\out'
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }
$log = Join-Path $outDir 'supernodal_sweep.txt'
"# supernodal sweep (reps=$reps)  $(Get-Date -Format o)" | Set-Content $log

function Run($nx, $ny, $st, [switch]$skipSimplicial) {
    $a = @('--nx', $nx, '--ny', $ny, '--stories', $st, '--reps', $reps)
    if ($skipSimplicial) { $a += '--skipSimplicial' }
    Write-Host "=== nx=$nx ny=$ny st=$st skipSimplicial=$skipSimplicial ==="
    & $bin @a | Tee-Object -FilePath $log -Append
}

# full ladder: all 3 solvers (AMD still tractable on mixed buildings up to ~30k)
Run 10 8 15      # ~9k nf
Run 12 10 20     # ~17k nf
Run 16 12 24     # ~32k nf
# big ladder: CHOLMOD only (Eigen simplicial factor too slow to wait on)
Run 20 16 30 -skipSimplicial    # ~64k nf
Run 24 20 36 -skipSimplicial    # ~113k nf
Run 28 24 44 -skipSimplicial    # ~190k nf

Write-Host "=== sweep done -> $log ==="
