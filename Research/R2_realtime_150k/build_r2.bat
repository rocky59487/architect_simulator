@echo off
REM R2 realtime-150k micro-bench build script.
REM
REM Compiles Research/R2_realtime_150k/r2_bench.cpp against the same FrameCore TU set as
REM Standalone/build.bat (sn-on branch), linking the same conda OpenBLAS/METIS. This is
REM intentionally a standalone build that does NOT participate in run_gate.ps1 -- the
REM bench is a measurement tool, not a fixture. It produces ./r2_bench.exe next to the
REM source.
REM
REM Requirements (same as standalone gate):
REM   * conda env framecore-direct activated (OpenBLAS + METIS on PATH for runtime DLL load)
REM   * Visual Studio with cl in PATH via vswhere
REM   * Eigen at %UE_ENGINE_ROOT%\Engine\Source\ThirdParty\Eigen (or %EIGEN_DIR%)
REM
REM Run from anywhere (paths anchored on script directory):
REM   Research\R2_realtime_150k\build_r2.bat
setlocal enabledelayedexpansion
set "HERE=%~dp0"
set "ROOT=%HERE%..\.."
set "EIGEN=%EIGEN_DIR%"
if "%EIGEN%"=="" if defined UE_ENGINE_ROOT set "EIGEN=%UE_ENGINE_ROOT%\Engine\Source\ThirdParty\Eigen"
if "%EIGEN%"=="" set "EIGEN=%HERE%..\..\..\UE_5.7\Engine\Source\ThirdParty\Eigen"
if not exist "%EIGEN%\Eigen" ( echo [build_r2] Eigen include root not found: "%EIGEN%" & exit /b 1 )

set "CONDA_SS=%SUPERNODAL_CONDA%"
if "%CONDA_SS%"=="" set "CONDA_SS=%USERPROFILE%\anaconda3\envs\framecore-direct\Library"
if not exist "%CONDA_SS%\include\openblas\cblas.h" (
  echo [build_r2] OpenBLAS not found under "%CONDA_SS%" -- this bench requires the supernodal lane.
  echo            Activate `framecore-direct` conda env first.
  exit /b 1
)

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -prerelease -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
  if "!VSDIR!"=="" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
)
if "!VSDIR!"=="" ( echo [build_r2] could not locate Visual Studio via vswhere. & exit /b 1 )
for %%d in ("%VSWHERE%") do set "PATH=%%~dpd;%PATH%"
call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo [build_r2] vcvars64 failed & exit /b 1 )

pushd "%ROOT%"
if not exist "Research\R2_realtime_150k\obj" mkdir "Research\R2_realtime_150k\obj"

cl /nologo /EHsc /std:c++17 /O2 /MD /utf-8 /DEIGEN_MPL2_ONLY /DFRAMECORE_SUPERNODAL=1 /DSN_SESSION_TIMING=1 ^
   /Fe:Research\R2_realtime_150k\r2_bench.exe ^
   /Fo:Research\R2_realtime_150k\obj\ ^
   /I"%EIGEN%" ^
   /I"%CONDA_SS%\include" ^
   /I"%CONDA_SS%\include\openblas" ^
   /I"Plugins\FrameSolver\Source\FrameCore\Public" ^
   /I"Plugins\FrameSolver\Source\FrameCore\Private" ^
   Plugins\FrameSolver\Source\FrameCore\Private\Section.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\Member.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\FrameModel.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\ElementStiffness.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\BeamColumnElement.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\MITC4ShellElement.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\FrameSolver.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\ElasticAllowable.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\Grillage.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\SelfWeight.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\Combination.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\InfluenceLine.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\ModalAnalysis.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\BucklingAnalysis.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\ResponseSpectrum.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\ModalDynamics.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\Connectivity.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\Collapse.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\DynamicCollapse.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\Reanalysis.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\PDeltaAnalysis.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\CorotationalAnalysis.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\TensionOnly.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\SizeOpt.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\Topology.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\SnSolver.cpp ^
   Plugins\FrameSolver\Source\FrameCore\Private\SnSession.cpp ^
   Research\R2_realtime_150k\r2_bench.cpp ^
   /link /LIBPATH:"%CONDA_SS%\lib" openblas.lib metis.lib
if errorlevel 1 ( echo [build_r2] COMPILE FAILED & popd & exit /b 1 )

echo [build_r2] OK -^> Research\R2_realtime_150k\r2_bench.exe
popd
exit /b 0
