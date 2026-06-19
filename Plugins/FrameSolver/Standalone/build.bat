@echo off
REM Standalone gate build for FrameCore. Locates VS (preview-aware) and runs cl.
setlocal enabledelayedexpansion
set "ROOT=%~dp0.."
set "EIGEN=%EIGEN_DIR%"
if "%EIGEN%"=="" if defined UE_ENGINE_ROOT set "EIGEN=%UE_ENGINE_ROOT%\Engine\Source\ThirdParty\Eigen"
if "%EIGEN%"=="" set "EIGEN=%~dp0..\..\..\..\UE_5.7\Engine\Source\ThirdParty\Eigen"
if not exist "%EIGEN%\Eigen" ( echo [build] Eigen include root not found: "%EIGEN%" & exit /b 1 )

REM --- conda OpenBLAS/METIS for the opt-in supernodal lane (SnSolver.cpp / FRAMECORE_SUPERNODAL=1).
REM     If absent (no-conda machine), fall back to FRAMECORE_SUPERNODAL=0: SnSolver.cpp internally
REM     routes solveLoadSupernodal to LDLT, and main.cpp's F55/F56/F62/F63 fixtures are guarded by
REM     `#if FRAMECORE_SUPERNODAL` so they compile out cleanly (marked [SKIP] in the banner).
set "CONDA_SS=%SUPERNODAL_CONDA%"
if "%CONDA_SS%"=="" set "CONDA_SS=%USERPROFILE%\anaconda3\envs\framecore-direct\Library"
set "SUPERNODAL=1"
if not exist "%CONDA_SS%\include\openblas\cblas.h" (
  echo [build] OpenBLAS not found under "%CONDA_SS%" -- building with FRAMECORE_SUPERNODAL=0 ^(F55/F56/F62/F63 will SKIP^)
  set "SUPERNODAL=0"
)

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"

set "VSDIR="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -prerelease -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
  if "!VSDIR!"=="" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
)
if "!VSDIR!"=="" (
  echo [build] could not locate Visual Studio via vswhere. Open an "x64 Native Tools" prompt and run cl directly.
  exit /b 1
)
rem Put vswhere's own folder on PATH so vcvars64's internal bare `vswhere` call resolves quietly.
for %%d in ("%VSWHERE%") do set "PATH=%%~dpd;%PATH%"
call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo [build] vcvars64 failed & exit /b 1 )

pushd "%ROOT%"
if not exist "Standalone\obj" mkdir "Standalone\obj"

set "GITSHA=unknown"
for /f "usebackq tokens=*" %%g in (`git -C "%ROOT%" rev-parse --short HEAD 2^>nul`) do set "GITSHA=%%g"

REM Split conda include/lib into separate vars so cl's /I and /LIBPATH each get their own quoted token
REM (a single batch var that pre-bakes the /I flags hits nested-quote parsing issues in cmd.exe).
if "!SUPERNODAL!"=="1" goto :build_sn_on
goto :build_sn_off

:build_sn_on
cl /nologo /EHsc /std:c++17 /O2 /MD /utf-8 /DEIGEN_MPL2_ONLY /DFRAMECORE_SUPERNODAL=1 /DFRAMECORE_BUILD_SHA=\"!GITSHA!\" ^
   /Fe:Standalone\frametest.exe ^
   /Fo:Standalone\obj\ ^
   /I"%EIGEN%" ^
   /I"%CONDA_SS%\include" ^
   /I"%CONDA_SS%\include\openblas" ^
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
   /link /LIBPATH:"%CONDA_SS%\lib" openblas.lib metis.lib
if errorlevel 1 ( echo [build] COMPILE FAILED & popd & exit /b 1 )
goto :build_done

:build_sn_off
cl /nologo /EHsc /std:c++17 /O2 /MD /utf-8 /DEIGEN_MPL2_ONLY /DFRAMECORE_SUPERNODAL=0 /DFRAMECORE_BUILD_SHA=\"!GITSHA!\" ^
   /Fe:Standalone\frametest.exe ^
   /Fo:Standalone\obj\ ^
   /I"%EIGEN%" ^
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
   Standalone\main.cpp
if errorlevel 1 ( echo [build] COMPILE FAILED & popd & exit /b 1 )

:build_done
echo [build] OK -^> Standalone\frametest.exe (FRAMECORE_SUPERNODAL=!SUPERNODAL!)
echo.
if "!SUPERNODAL!"=="1" set "PATH=%CONDA_SS%\bin;%PATH%"
"Standalone\frametest.exe"
set "RC=%errorlevel%"
popd
exit /b %RC%
