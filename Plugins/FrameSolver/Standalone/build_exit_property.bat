@echo off
REM v3.6 Phase 11 — exit-test property sweep builder (1000-fixture randomised oracle).
setlocal enabledelayedexpansion
set "ROOT=%~dp0.."
set "EIGEN=%EIGEN_DIR%"
if "%EIGEN%"=="" if defined UE_ENGINE_ROOT set "EIGEN=%UE_ENGINE_ROOT%\Engine\Source\ThirdParty\Eigen"
if "%EIGEN%"=="" set "EIGEN=%~dp0..\..\..\..\UE_5.7\Engine\Source\ThirdParty\Eigen"
if not exist "%EIGEN%\Eigen" ( echo [build] Eigen include root not found: "%EIGEN%" & exit /b 1 )
set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"

set "VSDIR="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -prerelease -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
  if "!VSDIR!"=="" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
)
if "!VSDIR!"=="" ( echo [build] could not locate Visual Studio. & exit /b 1 )
for %%d in ("%VSWHERE%") do set "PATH=%%~dpd;%PATH%"
call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo [build] vcvars64 failed & exit /b 1 )

pushd "%ROOT%"
if not exist "Standalone\obj_exit_property" mkdir "Standalone\obj_exit_property"

cl /nologo /EHsc /std:c++17 /O2 /MD /utf-8 /DEIGEN_MPL2_ONLY /DFRAMECORE_SUPERNODAL=0 ^
   /Fe:Standalone\exit_property.exe ^
   /Fo:Standalone\obj_exit_property\ ^
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
   Source\FrameCore\Private\StressField.cpp ^
   Source\FrameCore\Private\Grillage.cpp ^
   Source\FrameCore\Private\SelfWeight.cpp ^
   Source\FrameCore\Private\Connectivity.cpp ^
   Source\FrameCore\Private\Collapse.cpp ^
   Standalone\exit_property.cpp
if errorlevel 1 ( echo [build] COMPILE FAILED & popd & exit /b 1 )

echo [build] OK -^> Standalone\exit_property.exe
"Standalone\exit_property.exe" %*
set "RC=%errorlevel%"
popd
exit /b %RC%
