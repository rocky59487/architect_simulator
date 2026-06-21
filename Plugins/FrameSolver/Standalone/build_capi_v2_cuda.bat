@echo off
REM Builds frame_capi_v2_cuda.dll -- CUDA-enabled v2 dispatcher. Mirrors build_capi_v2.bat
REM (sn-on branch) exactly but adds /DFRAMECORE_CUDA=1 and links cudart.lib + cudss.lib so
REM the Dispatcher::Capabilities() advertises "solve.linear.gpu_backsub" and
REM session.open's gpuBacksub=true forwards into SnSessionOptions::useGpuBacksub.
REM Output goes to Standalone\frame_capi_v2_cuda.dll so the default frame_capi_v2.dll
REM (CPU-only) stays unchanged.

setlocal enabledelayedexpansion
set "ROOT=%~dp0.."
set "EIGEN=%EIGEN_DIR%"
if "%EIGEN%"=="" if defined UE_ENGINE_ROOT set "EIGEN=%UE_ENGINE_ROOT%\Engine\Source\ThirdParty\Eigen"
if "%EIGEN%"=="" set "EIGEN=%~dp0..\..\..\..\UE_5.7\Engine\Source\ThirdParty\Eigen"
if not exist "%EIGEN%\Eigen" ( echo [build_capi_v2_cuda] Eigen include root not found: "%EIGEN%" & exit /b 1 )

set "CONDA_SS=%SUPERNODAL_CONDA%"
if "%CONDA_SS%"=="" set "CONDA_SS=%USERPROFILE%\anaconda3\envs\framecore-direct\Library"
if not exist "%CONDA_SS%\include\openblas\cblas.h" ( echo [build_capi_v2_cuda] OpenBLAS not found & exit /b 1 )

set "CUDA_ROOT=%USERPROFILE%\anaconda3\envs\framecore-direct"
if not exist "%CUDA_ROOT%\include\cuda_runtime.h" (
  echo [build_capi_v2_cuda] CUDA runtime not found under "%CUDA_ROOT%". Run:
  echo   conda install -n framecore-direct -c nvidia cuda-cudart-dev libcudss-dev libcusolver-dev libcusparse-dev libcublas-dev cuda-cccl cuda-crt cuda-nvcc libnvjitlink
  exit /b 1
)
if not exist "%CONDA_SS%\include\cudss.h" ( echo [build_capi_v2_cuda] cuDSS not found & exit /b 1 )

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -prerelease -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
  if "!VSDIR!"=="" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
)
if "!VSDIR!"=="" ( echo [build_capi_v2_cuda] could not locate Visual Studio via vswhere. & exit /b 1 )
for %%d in ("%VSWHERE%") do set "PATH=%%~dpd;%PATH%"
call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo [build_capi_v2_cuda] vcvars64 failed & exit /b 1 )

pushd "%ROOT%"
if not exist "Standalone\obj_capi_v2_cuda" mkdir "Standalone\obj_capi_v2_cuda"

set "GITSHA=unknown"
set "GITROOT=%~dp0..\..\.."
for /f "usebackq tokens=*" %%g in (`git -C "%GITROOT%" rev-parse --short HEAD 2^>nul`) do set "GITSHA=%%g"
set "GITDIRTY="
git -C "%GITROOT%" diff --quiet --ignore-submodules -- 2>nul || set "GITDIRTY=1"
git -C "%GITROOT%" diff --cached --quiet --ignore-submodules -- 2>nul || set "GITDIRTY=1"
if "!GITDIRTY!"=="1" set "GITSHA=!GITSHA!-dirty"

cl /nologo /LD /EHsc /std:c++17 /O2 /MD /utf-8 /DEIGEN_MPL2_ONLY /DFRAMECORE_SUPERNODAL=1 /DFRAMECORE_CUDA=1 /DFRAMECORE_BUILD_SHA=\"!GITSHA!\" ^
   /Fe:Standalone\frame_capi_v2_cuda.dll ^
   /Fo:Standalone\obj_capi_v2_cuda\ ^
   /I"%EIGEN%" ^
   /I"%CONDA_SS%\include" ^
   /I"%CONDA_SS%\include\openblas" ^
   /I"%CUDA_ROOT%\include" ^
   /I"Source\FrameCore\Public" ^
   /I"Source\FrameCore\Private" ^
   /I"Standalone" ^
   Source\FrameCore\Private\Section.cpp ^
   Source\FrameCore\Private\Member.cpp ^
   Source\FrameCore\Private\FrameModel.cpp ^
   Source\FrameCore\Private\ElementStiffness.cpp ^
   Source\FrameCore\Private\BeamColumnElement.cpp ^
   Source\FrameCore\Private\MITC4ShellElement.cpp ^
   Source\FrameCore\Private\FrameSolver.cpp ^
   Source\FrameCore\Private\ElasticAllowable.cpp ^
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
   Standalone\v2\Dispatcher.cpp ^
   Standalone\frame_capi_v2.cpp ^
   /link /LIBPATH:"%CONDA_SS%\lib" /LIBPATH:"%CUDA_ROOT%\lib\x64" openblas.lib metis.lib cudart.lib cudss.lib
if errorlevel 1 ( echo [build_capi_v2_cuda] COMPILE FAILED & popd & exit /b 1 )

echo [build_capi_v2_cuda] OK -^> Standalone\frame_capi_v2_cuda.dll (FRAMECORE_SUPERNODAL=1, FRAMECORE_CUDA=1)
popd
exit /b 0
