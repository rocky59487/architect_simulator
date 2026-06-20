@echo off
REM R2 round-3 step 2: GPU sparse Cholesky bench build script.
REM
REM Pure host code -- uses cuSOLVER + cuSPARSE C API only. No CUDA kernel code,
REM no nvcc needed; cl compiles + links against cusolver.lib / cusparse.lib /
REM cudart.lib from the conda framecore-direct env.
setlocal enabledelayedexpansion
set "HERE=%~dp0"
set "ROOT=%HERE%..\.."
set "EIGEN=%EIGEN_DIR%"
if "%EIGEN%"=="" if defined UE_ENGINE_ROOT set "EIGEN=%UE_ENGINE_ROOT%\Engine\Source\ThirdParty\Eigen"
if "%EIGEN%"=="" set "EIGEN=%HERE%..\..\..\UE_5.7\Engine\Source\ThirdParty\Eigen"
if not exist "%EIGEN%\Eigen" ( echo [build_gpu] Eigen include root not found: "%EIGEN%" & exit /b 1 )

set "CONDA_SS=%SUPERNODAL_CONDA%"
if "%CONDA_SS%"=="" set "CONDA_SS=%USERPROFILE%\anaconda3\envs\framecore-direct\Library"
REM cuSOLVER/cuSPARSE headers live under env\include (not Library\include) for conda nvidia pkgs.
set "CUDA_ROOT=%USERPROFILE%\anaconda3\envs\framecore-direct"
if not exist "%CUDA_ROOT%\include\cusolverSp.h" (
  echo [build_gpu] cuSOLVER not found under "%CUDA_ROOT%". Run:
  echo   conda install -n framecore-direct -c nvidia cuda-cudart-dev libcusolver-dev libcusparse-dev libcublas-dev
  exit /b 1
)

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -prerelease -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
  if "!VSDIR!"=="" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
)
if "!VSDIR!"=="" ( echo [build_gpu] could not locate Visual Studio via vswhere. & exit /b 1 )
for %%d in ("%VSWHERE%") do set "PATH=%%~dpd;%PATH%"
call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo [build_gpu] vcvars64 failed & exit /b 1 )

pushd "%ROOT%"
if not exist "Research\R2_realtime_150k\obj_gpu" mkdir "Research\R2_realtime_150k\obj_gpu"

cl /nologo /EHsc /std:c++17 /O2 /MD /utf-8 /DEIGEN_MPL2_ONLY /DFRAMECORE_SUPERNODAL=0 ^
   /Fe:Research\R2_realtime_150k\gpu_bench.exe ^
   /Fo:Research\R2_realtime_150k\obj_gpu\ ^
   /I"%EIGEN%" ^
   /I"%CUDA_ROOT%\include" ^
   /I"Plugins\FrameSolver\Source\FrameCore\Public" ^
   /I"Plugins\FrameSolver\Source\FrameCore\Private" ^
   Plugins\FrameSolver\Source\FrameCore\Private\Section.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\Member.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\FrameModel.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\ElementStiffness.cpp ^
   Research\R2_realtime_150k\gpu_bench.cpp ^
   /link /LIBPATH:"%CUDA_ROOT%\lib\x64" cusolver.lib cusparse.lib cudart.lib
if errorlevel 1 ( echo [build_gpu] COMPILE FAILED & popd & exit /b 1 )

echo [build_gpu] OK -^> Research\R2_realtime_150k\gpu_bench.exe
echo Run with: PATH=^"%CUDA_ROOT%\bin;%%PATH%%^" Research\R2_realtime_150k\gpu_bench.exe
popd
exit /b 0
