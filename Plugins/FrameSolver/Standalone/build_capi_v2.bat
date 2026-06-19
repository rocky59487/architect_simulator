@echo off
REM Builds frame_capi_v2.dll (S6b B2 stub). COMPLETELY INDEPENDENT of build_capi.bat / build.bat.
REM
REM SCOPE (B2 stub level)
REM   Compiles only the v2 transport layer (Dispatcher + MiniJson + FrameWire + frame_capi_v2.cpp).
REM   NO FrameCore source files are linked yet -- the dispatcher's method handlers return
REM   NOT_IMPLEMENTED for everything except connection-mgmt (hello / session.* / cancel) and the
REM   shape-correct stubs for model.set / solve.linear. B3 expands this batch to mirror the
REM   build.bat full TU list when handlers wire to the engine.
REM
REM   This isolation lets the v2 DLL build and the v2_roundtrip gate test connection-level
REM   semantics WITHOUT touching the existing 5-leg gate.

setlocal enabledelayedexpansion
set "ROOT=%~dp0.."
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
for /f "usebackq tokens=*" %%g in (`git -C "%ROOT%" rev-parse --short HEAD 2^>nul`) do set "GITSHA=%%g"

cl /nologo /LD /EHsc /std:c++17 /O2 /MD /utf-8 /DFRAMECORE_BUILD_SHA=\"!GITSHA!\" ^
   /Fe:Standalone\frame_capi_v2.dll ^
   /Fo:Standalone\obj_capi_v2\ ^
   /I"Standalone" ^
   Standalone\v2\Dispatcher.cpp ^
   Standalone\frame_capi_v2.cpp
if errorlevel 1 ( echo [build_capi_v2] COMPILE FAILED & popd & exit /b 1 )

echo [build_capi_v2] OK -^> Standalone\frame_capi_v2.dll
popd
exit /b 0
