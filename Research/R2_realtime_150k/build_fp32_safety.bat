@echo off
REM R2 round-3 step 1: FP32 SPD safety check build script.
REM
REM Compiles Research/R2_realtime_150k/fp32_safety.cpp against the FrameCore element APIs.
REM No conda OpenBLAS / METIS needed -- the safety check uses Eigen SimplicialLDLT for
REM both FP32 and FP64 paths (independent of the production supernodal lane). That's the
REM right tool for "does FP32 SPD converge at this kappa" -- if Eigen FP32 SimplicialLDLT
REM struggles, the self-built supernodal FP32 won't help.
setlocal enabledelayedexpansion
set "HERE=%~dp0"
set "ROOT=%HERE%..\.."
set "EIGEN=%EIGEN_DIR%"
if "%EIGEN%"=="" if defined UE_ENGINE_ROOT set "EIGEN=%UE_ENGINE_ROOT%\Engine\Source\ThirdParty\Eigen"
if "%EIGEN%"=="" set "EIGEN=%HERE%..\..\..\UE_5.7\Engine\Source\ThirdParty\Eigen"
if not exist "%EIGEN%\Eigen" ( echo [build_fp32] Eigen include root not found: "%EIGEN%" & exit /b 1 )

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -prerelease -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
  if "!VSDIR!"=="" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
)
if "!VSDIR!"=="" ( echo [build_fp32] could not locate Visual Studio via vswhere. & exit /b 1 )
for %%d in ("%VSWHERE%") do set "PATH=%%~dpd;%PATH%"
call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo [build_fp32] vcvars64 failed & exit /b 1 )

pushd "%ROOT%"
if not exist "Research\R2_realtime_150k\obj_fp32" mkdir "Research\R2_realtime_150k\obj_fp32"

cl /nologo /EHsc /std:c++17 /O2 /MD /utf-8 /DEIGEN_MPL2_ONLY /DFRAMECORE_SUPERNODAL=0 ^
   /Fe:Research\R2_realtime_150k\fp32_safety.exe ^
   /Fo:Research\R2_realtime_150k\obj_fp32\ ^
   /I"%EIGEN%" ^
   /I"Plugins\FrameSolver\Source\FrameCore\Public" ^
   /I"Plugins\FrameSolver\Source\FrameCore\Private" ^
   Plugins\FrameSolver\Source\FrameCore\Private\Section.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\Member.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\FrameModel.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\ElementStiffness.cpp ^
   Research\R2_realtime_150k\fp32_safety.cpp
if errorlevel 1 ( echo [build_fp32] COMPILE FAILED & popd & exit /b 1 )

echo [build_fp32] OK -^> Research\R2_realtime_150k\fp32_safety.exe
popd
exit /b 0
