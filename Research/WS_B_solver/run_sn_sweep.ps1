# run_sn_sweep.ps1 - M2: self-built supernodal Cholesky vs CHOLMOD on mixed buildings at scale.
#   One process per scale. Needs exp_sn_chol.exe (build_supernodal.bat sn).
param()

$ErrorActionPreference = 'Continue'
$bin = Join-Path $PSScriptRoot '..\bin\exp_sn_chol.exe'
if (-not (Test-Path $bin)) { Write-Error "exe not found: $bin (run build_supernodal.bat sn first)"; exit 1 }
$env:PATH = "$(Join-Path $env:USERPROFILE 'anaconda3\envs\framecore-direct\Library\bin');$env:PATH"

$outDir = Join-Path $PSScriptRoot '..\out'
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }
$log = Join-Path $outDir 'sn_sweep.txt'
"# sn supernodal vs CHOLMOD (mixed building)  $(Get-Date -Format o)" | Set-Content $log

function Run($nx, $ny, $st) {
    Write-Host "=== mixed $nx,$ny,$st ==="
    & $bin --bigSweep --nx $nx --ny $ny --stories $st | Tee-Object -FilePath $log -Append
}

Run 12 10 20     # ~17k nf
Run 16 12 24     # ~32k nf
Run 20 16 30     # ~64k nf

Write-Host "=== sn sweep done -> $log ==="
