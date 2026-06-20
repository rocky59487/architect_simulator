@echo off
REM R2 round-3 step 2b: GPU sparse Cholesky via cuDSS (NVIDIA GPU-native direct solver).
setlocal enabledelayedexpansion
set "HERE=%~dp0"
set "ROOT=%HERE%..\.."
set "EIGEN=%EIGEN_DIR%"
if "%EIGEN%"=="" if defined UE_ENGINE_ROOT set "EIGEN=%UE_ENGINE_ROOT%\Engine\Source\ThirdParty\Eigen"
if "%EIGEN%"=="" set "EIGEN=%HERE%..\..\..\UE_5.7\Engine\Source\ThirdParty\Eigen"
if not exist "%EIGEN%\Eigen" ( echo [build_gpu2] Eigen include root not found: "%EIGEN%" & exit /b 1 )

set "CUDA_ROOT=%USERPROFILE%\anaconda3\envs\framecore-direct"
set "CUDSS_ROOT=%USERPROFILE%\anaconda3\envs\framecore-direct\Library"
if not exist "%CUDA_ROOT%\include\cuda_runtime.h" (
  echo [build_gpu2] CUDA runtime not found under "%CUDA_ROOT%" & exit /b 1
)
if not exist "%CUDSS_ROOT%\include\cudss.h" (
  echo [build_gpu2] cuDSS not found under "%CUDSS_ROOT%". Run:
  echo   conda install -n framecore-direct -c nvidia libcudss-dev
  exit /b 1
)

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -prerelease -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
  if "!VSDIR!"=="" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
)
if "!VSDIR!"=="" ( echo [build_gpu2] could not locate Visual Studio via vswhere. & exit /b 1 )
for %%d in ("%VSWHERE%") do set "PATH=%%~dpd;%PATH%"
call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo [build_gpu2] vcvars64 failed & exit /b 1 )

pushd "%ROOT%"
if not exist "Research\R2_realtime_150k\obj_gpu2" mkdir "Research\R2_realtime_150k\obj_gpu2"

cl /nologo /EHsc /std:c++17 /O2 /MD /utf-8 /DEIGEN_MPL2_ONLY /DFRAMECORE_SUPERNODAL=0 ^
   /Fe:Research\R2_realtime_150k\gpu_bench2.exe ^
   /Fo:Research\R2_realtime_150k\obj_gpu2\ ^
   /I"%EIGEN%" ^
   /I"%CUDA_ROOT%\include" ^
   /I"%CUDSS_ROOT%\include" ^
   /I"Plugins\FrameSolver\Source\FrameCore\Public" ^
   /I"Plugins\FrameSolver\Source\FrameCore\Private" ^
   Plugins\FrameSolver\Source\FrameCore\Private\Section.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\Member.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\FrameModel.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\ElementStiffness.cpp ^
   Research\R2_realtime_150k\gpu_bench2.cpp ^
   /link /LIBPATH:"%CUDA_ROOT%\lib\x64" /LIBPATH:"%CUDSS_ROOT%\lib" cudart.lib cudss.lib
if errorlevel 1 ( echo [build_gpu2] COMPILE FAILED & popd & exit /b 1 )

echo [build_gpu2] OK -^> Research\R2_realtime_150k\gpu_bench2.exe
popd
exit /b 0
