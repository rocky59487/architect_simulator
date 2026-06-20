@echo off
REM Builds frame_capi_v2.dll (S6b B2 dispatcher + S6b/B3 method handlers wired to FrameCore).
REM
REM SCOPE (B3 wire level)
REM   B2 stub level shipped without engine linkage; B3 links the full FrameCore translation-unit
REM   set (mirrors build_capi.bat for v1) so dispatcher handlers can call frame::solve / SnSession /
REM   etc. The build STAYS independent of build.bat / build_capi.bat -- only the SOURCE list overlaps.
REM
REM   SUPERNODAL lane is conditional (mirrors build.bat): when conda OpenBLAS/METIS env is present,
REM   FRAMECORE_SUPERNODAL=1 and the v2 session.open `mode: "supernodal"` path (B5) can build. Without
REM   conda, FRAMECORE_SUPERNODAL=0 and SnSolver/SnSession fall back to LDLT (still produces a v2 DLL
REM   that links + dispatches; just no supernodal speedup).

setlocal enabledelayedexpansion
set "ROOT=%~dp0.."
set "EIGEN=%EIGEN_DIR%"
if "%EIGEN%"=="" if defined UE_ENGINE_ROOT set "EIGEN=%UE_ENGINE_ROOT%\Engine\Source\ThirdParty\Eigen"
if "%EIGEN%"=="" set "EIGEN=%~dp0..\..\..\..\UE_5.7\Engine\Source\ThirdParty\Eigen"
if not exist "%EIGEN%\Eigen" ( echo [build_capi_v2] Eigen include root not found: "%EIGEN%" & exit /b 1 )

REM --- conda OpenBLAS/METIS for the opt-in supernodal lane (conditional, same policy as build.bat).
set "CONDA_SS=%SUPERNODAL_CONDA%"
if "%CONDA_SS%"=="" set "CONDA_SS=%USERPROFILE%\anaconda3\envs\framecore-direct\Library"
set "SUPERNODAL=1"
if not exist "%CONDA_SS%\include\openblas\cblas.h" (
  echo [build_capi_v2] OpenBLAS not found under "%CONDA_SS%" -- building with FRAMECORE_SUPERNODAL=0
  set "SUPERNODAL=0"
)

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"

set "VSDIR="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -prerelease -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
  if "!VSDIR!"=="" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
)
if "!VSDIR!"=="" ( echo [build_capi_v2] could not locate Visual Studio via vswhere. & exit /b 1 )
for %%d in ("%VSWHERE%") do set "PATH=%%~dpd;%PATH%"
call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo [build_capi_v2] vcvars64 failed & exit /b 1 )

pushd "%ROOT%"
if not exist "Standalone\obj_capi_v2" mkdir "Standalone\obj_capi_v2"

set "GITSHA=unknown"
set "GITROOT=%~dp0..\..\.."
for /f "usebackq tokens=*" %%g in (`git -C "%GITROOT%" rev-parse --short HEAD 2^>nul`) do set "GITSHA=%%g"
set "GITDIRTY="
git -C "%GITROOT%" diff --quiet --ignore-submodules -- 2>nul || set "GITDIRTY=1"
git -C "%GITROOT%" diff --cached --quiet --ignore-submodules -- 2>nul || set "GITDIRTY=1"
if "!GITDIRTY!"=="1" set "GITSHA=!GITSHA!-dirty"

if "!SUPERNODAL!"=="1" goto :build_sn_on
goto :build_sn_off

:build_sn_on
cl /nologo /LD /EHsc /std:c++17 /O2 /MD /utf-8 /DEIGEN_MPL2_ONLY /DFRAMECORE_SUPERNODAL=1 /DFRAMECORE_BUILD_SHA=\"!GITSHA!\" ^
   /Fe:Standalone\frame_capi_v2.dll ^
   /Fo:Standalone\obj_capi_v2\ ^
   /I"%EIGEN%" ^
   /I"%CONDA_SS%\include" ^
   /I"%CONDA_SS%\include\openblas" ^
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
   /link /LIBPATH:"%CONDA_SS%\lib" openblas.lib metis.lib
if errorlevel 1 ( echo [build_capi_v2] COMPILE FAILED & popd & exit /b 1 )
goto :build_done

:build_sn_off
cl /nologo /LD /EHsc /std:c++17 /O2 /MD /utf-8 /DEIGEN_MPL2_ONLY /DFRAMECORE_SUPERNODAL=0 /DFRAMECORE_BUILD_SHA=\"!GITSHA!\" ^
   /Fe:Standalone\frame_capi_v2.dll ^
   /Fo:Standalone\obj_capi_v2\ ^
   /I"%EIGEN%" ^
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
   Standalone\frame_capi_v2.cpp
if errorlevel 1 ( echo [build_capi_v2] COMPILE FAILED & popd & exit /b 1 )

:build_done
echo [build_capi_v2] OK -^> Standalone\frame_capi_v2.dll (FRAMECORE_SUPERNODAL=!SUPERNODAL!)
popd
exit /b 0
