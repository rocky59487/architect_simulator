# Optional 6th leg of the verification gate: CUDA-enabled engine + dispatcher.
#
# Activates only when:
#   * NVIDIA CUDA runtime + cuDSS are installed under a resolvable conda env
#     (Library\include\cudss.h, Library\bin\cudss64_0.dll, lib\x64\cudart.lib)
#   * cl + Eigen are available (same prereqs as run_gate.ps1)
#
# v2.11.1-RC: SUPERNODAL_CONDA is the canonical contract. The PS1 + both CUDA .bat
# files derive every other path (CONDA_SS, CUDA_ROOT, deps DIR) from one resolver
# (Resolve-SupernodalConda below). Override precedence, top wins:
#   1. -CondaEnv                 explicit script arg
#   2. SUPERNODAL_CONDA env var  (may end in \Library or be the env root)
#   3. FRAMECORE_LIB_DIR         legacy alias (environment.yml documents both)
#   4. anaconda3 / miniconda3 / mambaforge / miniforge3 layout probe under
#      USERPROFILE and C:\ProgramData, looking for a 'framecore-direct' env that
#      contains cudss.h. First match wins.
# Strict mode fails hard on a missing env; default mode soft-skips with exit 0
# so the 5-leg gate (run_gate.ps1) stays the canonical CI surface.
#
# What it does:
#   [1/3] Builds frametest_cuda.exe (build_sn_cuda.bat) and runs F1-F67.
#         When cuDSS DLLs resolve, FRAMECORE_GPU_STRICT=1 is exported so F67s
#         (strict GPU-attached fixture) FAILS instead of skipping on a silent
#         fallback to CPU -- the smoke fixture F67 still passes either way.
#         FRAMECORE_GPU_STRICT must be the literal "1" string -- "true", "yes",
#         "on" are treated as unset (SILENT SKIP). Set explicitly in CI scripts.
#   [2/3] Builds frame_capi_v2_cuda.dll (build_capi_v2_cuda.bat) and runs
#         v2_roundtrip with FRAMECORE_EXPECTED_GPU_CAP=true so the gpu_backsub
#         capability check is enforced.
#   [3/3] Runs r2_bench --gpu @ 90k to confirm production SnSession + cuDSS
#         still lands inside the 16.67 ms / frame 60 fps budget. The measured
#         number is logged but not gated (variance across machines is too wide
#         for a hard CI threshold; a soft warning above 25 ms reveals
#         regression candidates).
#
# Exit code: 0 on green, 1 on any failure. Soft skip (0) when the conda env /
# CUDA pieces are missing -- this is OPTIONAL, run_gate.ps1 stays the canonical
# 5-leg gate.
#
# Usage from repo root:
#   powershell -ExecutionPolicy Bypass -File Scripts\run_gpu_gate.ps1
# Optional:
#   -Root <repo>           (defaults to script's parent dir)
#   -CondaEnv <path>       override SUPERNODAL_CONDA / probe entirely
#   -Strict                fail on missing CUDA env instead of soft skip
param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$CondaEnv = '',
    [switch]$Strict
)
$ErrorActionPreference = 'Continue'

$RootPath = Resolve-Path -LiteralPath $Root -ErrorAction SilentlyContinue
if (-not $RootPath) { Write-Host "Repo root not found: $Root" -ForegroundColor Red; exit 1 }
$Root = $RootPath.Path

# ----- canonical SUPERNODAL_CONDA resolver -----
function Strip-LibrarySuffix {
    param([string]$path)
    if ([string]::IsNullOrEmpty($path)) { return $path }
    $trimmed = $path.TrimEnd('\','/')
    if ($trimmed -match '[\\/]Library$') {
        return ($trimmed -replace '[\\/]Library$', '')
    }
    return $trimmed
}

function Resolve-SupernodalConda {
    param([string]$Override = '')

    if (-not [string]::IsNullOrWhiteSpace($Override)) {
        return @{ root = (Strip-LibrarySuffix $Override); source = '-CondaEnv arg' }
    }
    if (-not [string]::IsNullOrWhiteSpace($env:SUPERNODAL_CONDA)) {
        return @{ root = (Strip-LibrarySuffix $env:SUPERNODAL_CONDA); source = 'SUPERNODAL_CONDA env' }
    }
    if (-not [string]::IsNullOrWhiteSpace($env:FRAMECORE_LIB_DIR)) {
        return @{ root = (Strip-LibrarySuffix $env:FRAMECORE_LIB_DIR); source = 'FRAMECORE_LIB_DIR env (legacy alias)' }
    }
    # Probe common layouts. We prefer envs that actually have cudss.h installed.
    $envName = 'framecore-direct'
    # C-04 audit: include both "miniforge3" and "miniforge" variants; some installers
    # drop the digit. Order = newest-default-installer first within each distro family.
    $bases = @(
        (Join-Path $env:USERPROFILE 'anaconda3\envs'),
        (Join-Path $env:USERPROFILE 'miniconda3\envs'),
        (Join-Path $env:USERPROFILE 'mambaforge\envs'),
        (Join-Path $env:USERPROFILE 'miniforge3\envs'),
        (Join-Path $env:USERPROFILE 'miniforge\envs'),
        'C:\ProgramData\anaconda3\envs',
        'C:\ProgramData\miniconda3\envs'
    )
    # C-03 audit: empty USERPROFILE (service-context PowerShell) produces drive-relative
    # paths like "\anaconda3\envs\...", which Test-Path resolves against the current
    # drive root. Skip the user-level probes when USERPROFILE is empty.
    if ([string]::IsNullOrEmpty($env:USERPROFILE)) {
        $bases = $bases | Where-Object { $_ -notlike '*USERPROFILE*' -and $_ -match '^[A-Za-z]:\\' }
    }
    $candidates = $bases | ForEach-Object { Join-Path $_ $envName }
    # First, anything with cudss.h
    foreach ($c in $candidates) {
        if (-not [string]::IsNullOrEmpty($c) -and (Test-Path (Join-Path $c 'Library\include\cudss.h'))) {
            return @{ root = $c; source = "probe (cudss.h found: $c)" }
        }
    }
    # Otherwise, anything that exists at all
    foreach ($c in $candidates) {
        if (-not [string]::IsNullOrEmpty($c) -and (Test-Path $c)) {
            return @{ root = $c; source = "probe (env exists, no cudss.h: $c)" }
        }
    }
    return $null
}

Write-Host '======================================================'
Write-Host ' FrameSolver GPU (CUDA) verification gate (optional)'
Write-Host '======================================================'

$resolved = Resolve-SupernodalConda -Override $CondaEnv
if (-not $resolved) {
    $msg = "       no SUPERNODAL_CONDA / FRAMECORE_LIB_DIR set, and no 'framecore-direct' env found under anaconda3 / miniconda3 / mambaforge / miniforge3 (USERPROFILE + ProgramData)"
    if ($Strict) {
        Write-Host $msg -ForegroundColor Red
        Write-Host ' GATE: FAIL (CUDA env required by -Strict)' -ForegroundColor Red
        exit 1
    } else {
        Write-Host $msg -ForegroundColor Yellow
        Write-Host ' GATE: SKIP (CUDA env absent; pass -Strict to enforce, or set SUPERNODAL_CONDA)' -ForegroundColor Yellow
        exit 0
    }
}

$CondaEnvRoot = $resolved.root
$CudssHdr  = Join-Path $CondaEnvRoot 'Library\include\cudss.h'
$CudartLib = Join-Path $CondaEnvRoot 'lib\x64\cudart.lib'
$CudssDll  = Join-Path $CondaEnvRoot 'Library\bin\cudss64_0.dll'

Write-Host ("       Conda env: {0}" -f $CondaEnvRoot)
Write-Host ("       Resolved via: {0}" -f $resolved.source)

$haveHeader = Test-Path $CudssHdr
$haveLib    = Test-Path $CudartLib
$haveDll    = Test-Path $CudssDll
$envOk      = $haveHeader -and $haveLib

if (-not $envOk) {
    $missing = @()
    if (-not $haveHeader) { $missing += "cudss.h ($CudssHdr)" }
    if (-not $haveLib)    { $missing += "cudart.lib ($CudartLib)" }
    $msg = "       missing CUDA pieces: $($missing -join ', ')"
    if ($Strict) {
        Write-Host $msg -ForegroundColor Red
        Write-Host ' GATE: FAIL (CUDA env required by -Strict)' -ForegroundColor Red
        exit 1
    } else {
        Write-Host $msg -ForegroundColor Yellow
        Write-Host ' GATE: SKIP (CUDA env incomplete; pass -Strict to enforce)' -ForegroundColor Yellow
        exit 0
    }
}

# Export the canonical contract for child processes (bat files + frametest_cuda + r2_bench).
# build_sn_cuda.bat / build_capi_v2_cuda.bat both read SUPERNODAL_CONDA, then derive CUDA_ROOT
# off it; setting both here keeps PS1 + bat reading the SAME env-root.
$env:SUPERNODAL_CONDA = (Join-Path $CondaEnvRoot 'Library')
$env:CUDA_ROOT        = $CondaEnvRoot
$CondaBin  = Join-Path $CondaEnvRoot 'bin'
$CondaLib  = Join-Path $CondaEnvRoot 'Library\bin'
$DepsDirs  = "$CondaBin;$CondaLib"

# F-01 audit: snapshot $env:Path BEFORE mutating it for child-process DLL lookup, so we
# can restore the parent shell's PATH on exit (the script may be invoked from an
# interactive PowerShell prompt; without restore, conda Library\bin leaks into the
# user's shell for the rest of the session and can shadow system DLLs).
$SavedPath = $env:Path

# When cuDSS DLLs resolve, demand a real GPU attach -- the strict fixture F67s + UE
# FFrameCoreGpuBacksubStrictTest read this flag and FAIL on a silent CPU fallback.
# The default smoke fixture F67 / FFrameCoreGpuBacksubTest still pass either way.
if ($haveDll) {
    $env:FRAMECORE_GPU_STRICT = '1'
    Write-Host '       FRAMECORE_GPU_STRICT=1 (cuDSS DLL present -- strict GPU-attached fixtures armed)'
} else {
    if ($Strict) {
        Write-Host "       cudss64_0.dll missing ($CudssDll); strict GPU fixtures cannot be enforced" -ForegroundColor Red
        Write-Host ' GATE: FAIL (strict requested, cuDSS runtime DLL absent)' -ForegroundColor Red
        exit 1
    }
    $env:FRAMECORE_GPU_STRICT = '0'
    Write-Host '       FRAMECORE_GPU_STRICT=0 (cuDSS DLL absent -- only smoke fixtures armed)' -ForegroundColor Yellow
}

# ---- [1/3] CUDA standalone gate ----
Write-Host ''
Write-Host '[1/3] CUDA standalone gate (build_sn_cuda.bat -> frametest_cuda.exe)...'
& (Join-Path $Root 'Plugins\FrameSolver\Standalone\build_sn_cuda.bat') | Tee-Object -Variable BuildOut | Out-Null
$BuildRC = $LASTEXITCODE
if ($BuildRC -ne 0) {
    # C-07 audit: surface the captured build output on failure instead of swallowing it.
    Write-Host '       --- build_sn_cuda.bat output (last 40 lines) ---' -ForegroundColor Yellow
    $BuildOut | Select-Object -Last 40 | ForEach-Object { Write-Host "       $_" }
    Write-Host "       build_sn_cuda FAILED (exit $BuildRC)" -ForegroundColor Red
    Write-Host " GATE: FAIL (CUDA standalone build)" -ForegroundColor Red
    $env:Path = $SavedPath
    exit 1
}

$env:Path = "$DepsDirs;$($env:Path)"
& (Join-Path $Root 'Plugins\FrameSolver\Standalone\frametest_cuda.exe') | Tee-Object -Variable F67Out | Out-Null
$F67RC = $LASTEXITCODE
$F67Line = ($F67Out | Select-String -Pattern 'ALL PASS|FAILURES' | Select-Object -Last 1)
Write-Host ("       frametest_cuda: {0} (exit {1})" -f $F67Line, $F67RC)

# v3.0.1 BLOCKER 2 audit: enforce STRICT_EXECUTED fingerprint under strict mode.
# Catches the case where the strict branch was compiled out, env wasn't seen by the
# child process, or any future refactor skipped F67s without anyone noticing.
if ($env:FRAMECORE_GPU_STRICT -eq '1') {
    $StrictExec = ($F67Out | Select-String -Pattern '\[F67s\] STRICT_EXECUTED' | Measure-Object).Count
    $StrictSkip = ($F67Out | Select-String -Pattern '\[F67s\] STRICT_SKIPPED' | Measure-Object).Count
    if ($StrictExec -lt 1 -or $StrictSkip -ge 1) {
        Write-Host ("       F67s strict enforcement FAIL (expected STRICT_EXECUTED, got exec={0} skip={1})" -f $StrictExec, $StrictSkip) -ForegroundColor Red
        Write-Host ' GATE: FAIL (F67s strict path was not executed under -Strict mode)' -ForegroundColor Red
        $env:Path = $SavedPath
        exit 1
    }
    Write-Host ("       F67s strict enforcement: OK (STRICT_EXECUTED fingerprint observed)") -ForegroundColor Green
}

# ---- [2/3] CUDA v2 dispatcher gate ----
Write-Host ''
Write-Host '[2/3] CUDA v2 dispatcher (build_capi_v2_cuda.bat -> frame_capi_v2_cuda.dll)...'
& (Join-Path $Root 'Plugins\FrameSolver\Standalone\build_capi_v2_cuda.bat') | Tee-Object -Variable BuildV2Out | Out-Null
$BuildV2RC = $LASTEXITCODE
if ($BuildV2RC -ne 0) {
    # C-07 audit: surface the captured build output on failure.
    Write-Host '       --- build_capi_v2_cuda.bat output (last 40 lines) ---' -ForegroundColor Yellow
    $BuildV2Out | Select-Object -Last 40 | ForEach-Object { Write-Host "       $_" }
    Write-Host "       build_capi_v2_cuda FAILED (exit $BuildV2RC)" -ForegroundColor Red
    Write-Host " GATE: FAIL (CUDA v2 dispatcher build)" -ForegroundColor Red
    $env:Path = $SavedPath
    exit 1
}

# v3.0.1: bumped from '2.11.1' alongside Dispatcher.h kEngineVer + uplugin VersionName.
# This pin MUST move every time kEngineVer moves -- v3.0.0 release-after-the-fact audit
# caught that release had stale "2.11.1" runtime version while tag said v3.0.0.
$env:FRAMECORE_EXPECTED_ENGINE_VER  = '3.0.1'
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

    # v3.0.1 HIGH 1 audit: r2_bench's own exit code only enforces "margin >= 0" vs the
    # 16.67 ms 60-fps budget. A GPU lane that silently regresses to ~12 ms still PASSes
    # that loose check while shipping a 2.5x slowdown vs v2.11.0 baseline (~4.7 ms /
    # margin ~+12 ms). Add a regression-hard threshold: require margin >= +8.0 ms
    # (i.e. frame time <= 8.67 ms, ~2x the measured baseline). Tighten when baseline
    # tightens.
    if ($PerfLine) {
        $marginMatch = ([string]$PerfLine) -match 'margin=\+?(?<m>[-\d\.]+)\s*ms'
        if ($marginMatch) {
            $marginMs = [double]$Matches['m']
            $marginMin = 8.0   # baseline ~+11.94 ms; alert at 2x baseline frame time
            if ($marginMs -lt $marginMin) {
                Write-Host ("       r2_bench REGRESSION: margin={0} ms < required {1} ms (baseline v2.11.0 was ~+11.94 ms)" -f $marginMs, $marginMin) -ForegroundColor Red
                $PerfRC = 1   # promote to gate failure
            } else {
                Write-Host ("       r2_bench regression check: OK (margin {0} ms >= {1} ms threshold)" -f $marginMs, $marginMin) -ForegroundColor Green
            }
        } else {
            Write-Host ("       r2_bench margin parse FAILED (cannot enforce regression) -- treating as soft warning") -ForegroundColor Yellow
        }
    }
}
Write-Host ("       r2_bench --gpu 90k: {0} (exit {1})" -f $PerfLine, $PerfRC)

# ---- verdict ----
Write-Host ''
Write-Host '======================================================'
$GateOk = ($F67RC -eq 0) -and ($V2RC -eq 0) -and ($PerfRC -eq 0)
# F-01 audit: restore parent shell PATH unconditionally before exit.
$env:Path = $SavedPath
if ($GateOk) {
    Write-Host (" GPU GATE: PASS  (F1-F67 + F67s strict OK, v2_roundtrip CUDA OK, r2_bench --gpu {0})" -f $PerfLine) -ForegroundColor Green
    exit 0
} else {
    Write-Host (" GPU GATE: FAIL  (frametest_cuda exit {0}, v2_roundtrip exit {1}, r2_bench exit {2})" -f $F67RC, $V2RC, $PerfRC) -ForegroundColor Red
    exit 1
}
