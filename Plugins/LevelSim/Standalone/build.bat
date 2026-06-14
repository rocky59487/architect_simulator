@echo off
REM Builds and runs the LevelSim M0 standalone oracle gate (pure C++17, no UE / no Eigen).
REM Mirrors FrameCore\Standalone\build_linear_audit.bat (vswhere -> vcvars64 -> cl /std:c++17).
REM Expected output on success: "ALL PASS  (failures=0)" and exit code 0.
setlocal enabledelayedexpansion
set "ROOT=%~dp0.."
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

cl /nologo /EHsc /std:c++17 /O2 /MD /utf-8 ^
   /Fe:Standalone\level_gate.exe ^
   /Fo:Standalone\obj\ ^
   /I"Core" ^
   Core\LevelCore.cpp ^
   Standalone\level_gate.cpp
if errorlevel 1 ( echo [build] COMPILE FAILED & popd & exit /b 1 )

echo [build] OK -^> Standalone\level_gate.exe
echo.
"Standalone\level_gate.exe"
set "RC=%errorlevel%"
popd
exit /b %RC%
