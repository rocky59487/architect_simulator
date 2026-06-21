@echo off
REM Optional CUDA-enabled standalone gate build. Mirrors build.bat exactly, but adds
REM   /DFRAMECORE_CUDA=1   so SnSession's GPU lane (cuDSS) compiles in
REM   include + lib paths for conda cuDSS / CUDA runtime
REM Output goes to Standalone\frametest_cuda.exe so the default build.bat result
REM (Standalone\frametest.exe) stays untouched. F67 (GPU bit-equivalence) only
REM activates under this build.
setlocal enabledelayedexpansion
set "ROOT=%~dp0.."
set "EIGEN=%EIGEN_DIR%"
if "%EIGEN%"=="" if defined UE_ENGINE_ROOT set "EIGEN=%UE_ENGINE_ROOT%\Engine\Source\ThirdParty\Eigen"
if "%EIGEN%"=="" set "EIGEN=%~dp0..\..\..\..\UE_5.7\Engine\Source\ThirdParty\Eigen"
if not exist "%EIGEN%\Eigen" ( echo [build_sn_cuda] Eigen include root not found: "%EIGEN%" & exit /b 1 )

REM v2.11.1-RC: SUPERNODAL_CONDA is the canonical contract; CUDA_ROOT, when not
REM explicitly set, is derived from it (env-root = SUPERNODAL_CONDA minus the
REM trailing "\Library" suffix). Precedence (top wins):
REM   1. CUDA_ROOT   explicit override
REM   2. SUPERNODAL_CONDA -> strip \Library -> env root
REM   3. CUDA_PATH   standard NVIDIA CUDA Toolkit install
REM   4. anaconda3\envs\framecore-direct  legacy default
REM
REM Why call :derive_cuda_root rather than an inline if-block: cmd's multi-line
REM `if () (...)` blocks parse once and the inner %VAR% expansion captures the
REM block-entry value, not the current value -- so setting CUDA_ROOT inside a
REM nested if and then checking %CUDA_ROOT% inside the same outer if-block
REM produces a stale empty string. A :subroutine called via `call :label` parses
REM each line independently and the issue evaporates.
REM C-12 audit: PS1 Resolve-SupernodalConda accepts SUPERNODAL_CONDA either as the
REM env root OR with \Library suffix; bat must normalize the same way so direct
REM bat invocation with SUPERNODAL_CONDA=<env-root> doesn't fail at the OpenBLAS
REM check with a confusing message. Append \Library if it isn't there.
set "CONDA_SS=%SUPERNODAL_CONDA%"
if "%CONDA_SS%"=="" set "CONDA_SS=%USERPROFILE%\anaconda3\envs\framecore-direct\Library"
call :normalize_conda_ss
if not exist "%CONDA_SS%\include\openblas\cblas.h" ( echo [build_sn_cuda] OpenBLAS not found under "%CONDA_SS%" -- set SUPERNODAL_CONDA to the conda env Library dir & exit /b 1 )

call :derive_cuda_root
if exist "%CUDA_ROOT%\include\cuda_runtime.h" goto :cuda_runtime_ok
echo [build_sn_cuda] CUDA runtime not found under "%CUDA_ROOT%". Set one of:
echo   CUDA_ROOT          override directly
echo   SUPERNODAL_CONDA   point at ^<env^>\Library (CUDA_ROOT derived from it)
echo   CUDA_PATH          standard CUDA Toolkit install
echo Then re-run, or:
echo   conda install -n framecore-direct -c nvidia cuda-cudart-dev libcusolver-dev libcusparse-dev libcublas-dev libcudss-dev cuda-cccl cuda-crt
exit /b 1
:cuda_runtime_ok
if exist "%CONDA_SS%\include\cudss.h" goto :cudss_ok
echo [build_sn_cuda] cuDSS not found under "%CONDA_SS%". Run:
echo   conda install -n framecore-direct -c nvidia libcudss-dev
exit /b 1
:cudss_ok

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -prerelease -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
  if "!VSDIR!"=="" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
)
if "!VSDIR!"=="" ( echo [build_sn_cuda] could not locate Visual Studio via vswhere & exit /b 1 )
for %%d in ("%VSWHERE%") do set "PATH=%%~dpd;%PATH%"
call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo [build_sn_cuda] vcvars64 failed & exit /b 1 )

pushd "%ROOT%"
if not exist "Standalone\obj_cuda" mkdir "Standalone\obj_cuda"

set "GITSHA=unknown"
for /f "usebackq tokens=*" %%g in (`git -C "%ROOT%" rev-parse --short HEAD 2^>nul`) do set "GITSHA=%%g"

cl /nologo /EHsc /std:c++17 /O2 /MD /utf-8 /openmp /DEIGEN_MPL2_ONLY /DFRAMECORE_SUPERNODAL=1 /DFRAMECORE_CUDA=1 /DFRAMECORE_BUILD_SHA=\"!GITSHA!\" ^
   /Fe:Standalone\frametest_cuda.exe ^
   /Fo:Standalone\obj_cuda\ ^
   /I"%EIGEN%" ^
   /I"%CONDA_SS%\include" ^
   /I"%CONDA_SS%\include\openblas" ^
   /I"%CUDA_ROOT%\include" ^
   /I"Source\FrameCore\Public" ^
   /I"Source\FrameCore\Private" ^
   Source\FrameCore\Private\Section.cpp ^
   Source\FrameCore\Private\Member.cpp ^
   Source\FrameCore\Private\FrameModel.cpp ^
   Source\FrameCore\Private\ElementStiffness.cpp ^
   Source\FrameCore\Private\BeamColumnElement.cpp ^
   Source\FrameCore\Private\MITC4ShellElement.cpp ^
   Source\FrameCore\Private\FrameSolver.cpp ^
   Source\FrameCore\Private\ElasticAllowable.cpp ^
   Source\FrameCore\Private\StressField.cpp ^
   Source\FrameCore\Private\Grillage.cpp ^
   Source\FrameCore\Private\SelfWeight.cpp ^
   Source\FrameCore\Private\Combination.cpp ^
   Source\FrameCore\Private\InfluenceLine.cpp ^
   Source\FrameCore\Private\ModalAnalysis.cpp ^
   Source\FrameCore\Private\BucklingAnalysis.cpp ^
   Source\FrameCore\Private\ResponseSpectrum.cpp ^
   Source\FrameCore\Private\ModalDynamics.cpp ^
   Source\FrameCore\Private\Connectivity.cpp ^
   Source\FrameCore\Private\Collapse.cpp ^
   Source\FrameCore\Private\DynamicCollapse.cpp ^
   Source\FrameCore\Private\Reanalysis.cpp ^
   Source\FrameCore\Private\PDeltaAnalysis.cpp ^
   Source\FrameCore\Private\CorotationalAnalysis.cpp ^
   Source\FrameCore\Private\TensionOnly.cpp ^
   Source\FrameCore\Private\SizeOpt.cpp ^
   Source\FrameCore\Private\Topology.cpp ^
   Source\FrameCore\Private\SnSolver.cpp ^
   Source\FrameCore\Private\SnSession.cpp ^
   Standalone\main.cpp ^
   /link /LIBPATH:"%CONDA_SS%\lib" /LIBPATH:"%CUDA_ROOT%\lib\x64" openblas.lib metis.lib cudart.lib cudss.lib cusparse.lib
if errorlevel 1 ( echo [build_sn_cuda] COMPILE FAILED & popd & exit /b 1 )

echo [build_sn_cuda] OK -^> Standalone\frametest_cuda.exe
popd
exit /b 0

REM ---- subroutines ----
:normalize_conda_ss
REM Ensure CONDA_SS ends in \Library (the bat's lookup convention).
REM If user passed the env root directly, append. If already \Library-suffixed, leave alone.
if /I "%CONDA_SS:~-8%"=="\Library" exit /b 0
if /I "%CONDA_SS:~-8%"=="/Library" exit /b 0
set "CONDA_SS=%CONDA_SS%\Library"
exit /b 0

:derive_cuda_root
REM Resolve CUDA_ROOT per precedence: explicit override > SUPERNODAL_CONDA env
REM (strip trailing \Library or /Library) > CUDA_PATH > anaconda3 legacy default.
REM Lives in a subroutine so each line parses independently of any caller-side
REM if-block context.
if not "%CUDA_ROOT%"=="" exit /b 0
if "%SUPERNODAL_CONDA%"=="" goto :_drc_try_cuda_path
set "CONDA_LIB=%SUPERNODAL_CONDA%"
set "CUDA_ROOT=%CONDA_LIB%"
if /I "%CONDA_LIB:~-8%"=="\Library" set "CUDA_ROOT=%CONDA_LIB:~0,-8%"
if /I "%CONDA_LIB:~-8%"=="/Library" set "CUDA_ROOT=%CONDA_LIB:~0,-8%"
exit /b 0
:_drc_try_cuda_path
if "%CUDA_PATH%"=="" goto :_drc_default
set "CUDA_ROOT=%CUDA_PATH%"
exit /b 0
:_drc_default
set "CUDA_ROOT=%USERPROFILE%\anaconda3\envs\framecore-direct"
exit /b 0
