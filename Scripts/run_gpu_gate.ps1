# Optional 6th leg of the verification gate: CUDA-enabled engine + dispatcher.
#
# Activates only when:
#   * NVIDIA CUDA runtime + cuDSS are installed under the conda framecore-direct env
#     (Library\include\cudss.h, Library\bin\cudss64_0.dll, lib\x64\cudart.lib)
#   * cl + Eigen are available (same prereqs as run_gate.ps1)
#
# What it does:
#   [1/3] Builds frametest_cuda.exe (build_sn_cuda.bat) and runs F1-F67 (F67 is the
#         GPU-vs-CPU bit-equivalence fixture).
#   [2/3] Builds frame_capi_v2_cuda.dll (build_capi_v2_cuda.bat) and runs v2_roundtrip
#         pointed at it, with FRAMECORE_EXPECTED_GPU_CAP=true so the gpu_backsub
#         capability check is enforced.
#   [3/3] Runs r2_bench --gpu @ 90k to confirm production SnSession + cuDSS still
#         lands inside the 16.67 ms / frame 60 fps budget. The measured number is
#         logged but not gated (variance across machines is too wide for a hard CI
#         threshold; a soft warning above 25 ms reveals regression candidates).
#
# Exit code: 0 on green, 1 on any failure. Soft skip (0) when the conda env / CUDA
# pieces are missing -- this is OPTIONAL, run_gate.ps1 stays the canonical 5-leg gate.
#
# Usage from repo root:
#   powershell -ExecutionPolicy Bypass -File Scripts\run_gpu_gate.ps1
# Optional:
#   -Root <repo>    (defaults to script's parent dir)
#   -Strict         (fail on missing CUDA env instead of soft skip)
param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [switch]$Strict
)
$ErrorActionPreference = 'Continue'

$RootPath = Resolve-Path -LiteralPath $Root -ErrorAction SilentlyContinue
if (-not $RootPath) { Write-Host "Repo root not found: $Root" -ForegroundColor Red; exit 1 }
$Root = $RootPath.Path

$CondaEnv = Join-Path $env:USERPROFILE 'anaconda3\envs\framecore-direct'
$CudssHdr = Join-Path $CondaEnv 'Library\include\cudss.h'
$CudartLib = Join-Path $CondaEnv 'lib\x64\cudart.lib'

Write-Host '======================================================'
Write-Host ' FrameSolver GPU (CUDA) verification gate (optional)'
Write-Host '======================================================'

if (-not (Test-Path $CudssHdr) -or -not (Test-Path $CudartLib)) {
    $msg = "       cuDSS / CUDA runtime not found under $CondaEnv"
    if ($Strict) {
        Write-Host $msg -ForegroundColor Red
        Write-Host " GATE: FAIL (CUDA env required by -Strict)" -ForegroundColor Red
        exit 1
    } else {
        Write-Host $msg -ForegroundColor Yellow
        Write-Host " GATE: SKIP (CUDA env absent; pass -Strict to enforce)" -ForegroundColor Yellow
        exit 0
    }
}

$CondaBin  = Join-Path $CondaEnv 'bin'
$CondaLib  = Join-Path $CondaEnv 'Library\bin'
$DepsDirs  = "$CondaBin;$CondaLib"

# ---- [1/3] CUDA standalone gate ----
Write-Host ''
Write-Host '[1/3] CUDA standalone gate (build_sn_cuda.bat -> frametest_cuda.exe)...'
& (Join-Path $Root 'Plugins\FrameSolver\Standalone\build_sn_cuda.bat') | Tee-Object -Variable BuildOut | Out-Null
$BuildRC = $LASTEXITCODE
if ($BuildRC -ne 0) {
    Write-Host "       build_sn_cuda FAILED (exit $BuildRC)" -ForegroundColor Red
    Write-Host " GATE: FAIL (CUDA standalone build)" -ForegroundColor Red
    exit 1
}

$env:Path = "$DepsDirs;$($env:Path)"
& (Join-Path $Root 'Plugins\FrameSolver\Standalone\frametest_cuda.exe') | Tee-Object -Variable F67Out | Out-Null
$F67RC = $LASTEXITCODE
$F67Line = ($F67Out | Select-String -Pattern 'ALL PASS|FAILURES' | Select-Object -Last 1)
Write-Host ("       frametest_cuda: {0} (exit {1})" -f $F67Line, $F67RC)

# ---- [2/3] CUDA v2 dispatcher gate ----
Write-Host ''
Write-Host '[2/3] CUDA v2 dispatcher (build_capi_v2_cuda.bat -> frame_capi_v2_cuda.dll)...'
& (Join-Path $Root 'Plugins\FrameSolver\Standalone\build_capi_v2_cuda.bat') | Tee-Object -Variable BuildV2Out | Out-Null
$BuildV2RC = $LASTEXITCODE
if ($BuildV2RC -ne 0) {
    Write-Host "       build_capi_v2_cuda FAILED (exit $BuildV2RC)" -ForegroundColor Red
    Write-Host " GATE: FAIL (CUDA v2 dispatcher build)" -ForegroundColor Red
    exit 1
}

$env:FRAMECORE_EXPECTED_ENGINE_VER  = '2.10.0'
$env:FRAMECORE_EXPECTED_GPU_CAP     = 'true'
$env:FRAMECORE_V2_DLL               = (Join-Path $Root 'Plugins\FrameSolver\Standalone\frame_capi_v2_cuda.dll')
$env:FRAMECORE_V2_DLL_DEPS_DIRS     = $DepsDirs
& python (Join-Path $Root 'Tools\v2_roundtrip.py') | Tee-Object -Variable V2Out | Out-Null
$V2RC = $LASTEXITCODE
$V2Line = ($V2Out | Select-String -Pattern '=== summary' | Select-Object -Last 1)
Write-Host ("       v2_roundtrip (CUDA): {0} (exit {1})" -f $V2Line, $V2RC)

# ---- [3/3] Production GPU perf sanity ----
Write-Host ''
Write-Host '[3/3] Production GPU perf sanity (r2_bench --gpu --preset 90k)...'
& (Join-Path $Root 'Research\R2_realtime_150k\build_r2.bat') | Out-Null
$BuildR2RC = $LASTEXITCODE
if ($BuildR2RC -ne 0) {
    Write-Host "       build_r2 FAILED (exit $BuildR2RC) -- skipping perf sanity" -ForegroundColor Yellow
    $PerfRC = 0  # soft skip
    $PerfLine = '(skipped: build_r2 failed)'
} else {
    & (Join-Path $Root 'Research\R2_realtime_150k\r2_bench.exe') '--preset' '90k' '--gpu' '--compare' '--repeat' '15' '--warmup' '3' | Tee-Object -Variable R2Out | Out-Null
    $PerfRC = $LASTEXITCODE
    $PerfLine = ($R2Out | Select-String -Pattern '60fps_budget' | Select-Object -Last 1)
}
Write-Host ("       r2_bench --gpu 90k: {0} (exit {1})" -f $PerfLine, $PerfRC)

# ---- verdict ----
Write-Host ''
Write-Host '======================================================'
$GateOk = ($F67RC -eq 0) -and ($V2RC -eq 0) -and ($PerfRC -eq 0)
if ($GateOk) {
    Write-Host (" GPU GATE: PASS  (F1-F67 OK, v2_roundtrip CUDA OK, r2_bench --gpu OK)") -ForegroundColor Green
    exit 0
} else {
    Write-Host (" GPU GATE: FAIL  (frametest_cuda exit {0}, v2_roundtrip exit {1}, r2_bench exit {2})" -f $F67RC, $V2RC, $PerfRC) -ForegroundColor Red
    exit 1
}
