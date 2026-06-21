@echo off
REM R2 round-4 step 3: cuDSS PHASE_REFACTORIZATION timing bench.
setlocal enabledelayedexpansion
set "HERE=%~dp0"
set "ROOT=%HERE%..\.."
set "EIGEN=%EIGEN_DIR%"
if "%EIGEN%"=="" if defined UE_ENGINE_ROOT set "EIGEN=%UE_ENGINE_ROOT%\Engine\Source\ThirdParty\Eigen"
if "%EIGEN%"=="" set "EIGEN=%HERE%..\..\..\UE_5.7\Engine\Source\ThirdParty\Eigen"
if not exist "%EIGEN%\Eigen" ( echo [build_gpu3] Eigen include root not found: "%EIGEN%" & exit /b 1 )

set "CUDA_ROOT=%USERPROFILE%\anaconda3\envs\framecore-direct"
set "CUDSS_ROOT=%USERPROFILE%\anaconda3\envs\framecore-direct\Library"

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -prerelease -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
  if "!VSDIR!"=="" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
)
for %%d in ("%VSWHERE%") do set "PATH=%%~dpd;%PATH%"
call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul

pushd "%ROOT%"
if not exist "Research\R2_realtime_150k\obj_gpu3" mkdir "Research\R2_realtime_150k\obj_gpu3"

cl /nologo /EHsc /std:c++17 /O2 /MD /utf-8 /DEIGEN_MPL2_ONLY /DFRAMECORE_SUPERNODAL=0 ^
   /Fe:Research\R2_realtime_150k\gpu_bench3_refactor.exe ^
   /Fo:Research\R2_realtime_150k\obj_gpu3\ ^
   /I"%EIGEN%" ^
   /I"%CUDA_ROOT%\include" ^
   /I"%CUDSS_ROOT%\include" ^
   /I"Plugins\FrameSolver\Source\FrameCore\Public" ^
   /I"Plugins\FrameSolver\Source\FrameCore\Private" ^
   Plugins\FrameSolver\Source\FrameCore\Private\Section.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\Member.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\FrameModel.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\ElementStiffness.cpp ^
   Research\R2_realtime_150k\gpu_bench3_refactor.cpp ^
   /link /LIBPATH:"%CUDA_ROOT%\lib\x64" /LIBPATH:"%CUDSS_ROOT%\lib" cudart.lib cudss.lib
if errorlevel 1 ( echo [build_gpu3] COMPILE FAILED & popd & exit /b 1 )

echo [build_gpu3] OK -^> Research\R2_realtime_150k\gpu_bench3_refactor.exe
popd
exit /b 0
