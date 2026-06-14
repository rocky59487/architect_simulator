@echo off
REM build_supernodal.bat - research-only: CHOLMOD(supernodal) + METIS direct-solver eval.
REM   NOT part of the engine gate. Links against a conda-forge SuiteSparse+METIS env.
REM   Self-contained: compiles its OWN FrameCore core objs into Research\obj_sn_core (does
REM   not depend on build_research.bat, whose source list is stale). Engine sources read-only.
REM
REM Usage:  build_supernodal.bat [smoke|compare|all]    (default: all)
REM   smoke    exp_cholmod_smoke      (no FrameCore - pure CHOLMOD/METIS link test)
REM   compare  exp_supernodal_compare (compiles core then links the prototype)
REM
REM Override conda env via env var SUPERNODAL_CONDA=<...>\Library  (default framecore-direct).
setlocal enabledelayedexpansion
set "ROOT=%~dp0..\.."
set "FS=Plugins\FrameSolver"
set "PRIV=%FS%\Source\FrameCore\Private"
set "WHAT=%~1"
if "%WHAT%"=="" set "WHAT=all"

REM --- Eigen include root (same resolution order as build_research.bat) ---
set "EIGEN=%EIGEN_DIR%"
if "%EIGEN%"=="" if defined UE_ENGINE_ROOT set "EIGEN=%UE_ENGINE_ROOT%\Engine\Source\ThirdParty\Eigen"
if "%EIGEN%"=="" set "EIGEN=%ROOT%\..\UE_5.7\Engine\Source\ThirdParty\Eigen"
if not exist "%EIGEN%\Eigen" ( echo [supernodal] Eigen include root not found: "%EIGEN%" & exit /b 1 )

REM --- conda SuiteSparse/METIS env (Library dir holding include\ lib\ bin\) ---
set "CONDA_SS=%SUPERNODAL_CONDA%"
if "%CONDA_SS%"=="" set "CONDA_SS=%USERPROFILE%\anaconda3\envs\framecore-direct\Library"
if not exist "%CONDA_SS%\include\suitesparse\cholmod.h" ( echo [supernodal] cholmod.h not found under "%CONDA_SS%\include\suitesparse" & exit /b 1 )
if not exist "%CONDA_SS%\include\metis.h" ( echo [supernodal] metis.h not found under "%CONDA_SS%\include" & exit /b 1 )
set "SSLIBDIR=%CONDA_SS%\lib"

REM --- locate Visual Studio / vcvars64 (same as build_research.bat) ---
set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -prerelease -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
  if "!VSDIR!"=="" for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath 2^>nul`) do set "VSDIR=%%i"
)
if "!VSDIR!"=="" ( echo [supernodal] could not locate Visual Studio via vswhere. & exit /b 1 )
for %%d in ("%VSWHERE%") do set "PATH=%%~dpd;!PATH!"
call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 ( echo [supernodal] vcvars64 failed & exit /b 1 )

pushd "%ROOT%"
if not exist "Research\obj_exp"     mkdir "Research\obj_exp"
if not exist "Research\bin"         mkdir "Research\bin"

set "CFLAGS=/nologo /EHsc /std:c++17 /O2 /MD /utf-8 /DEIGEN_MPL2_ONLY /wd4005 /I"%EIGEN%" /I"%CONDA_SS%\include" /I"%CONDA_SS%\include\suitesparse""
set "INCCORE=/I"%FS%\Source\FrameCore\Public" /I"%FS%\Source\FrameCore\Private" /I"Research\common""
set "SSLIBS=cholmod.lib suitesparseconfig.lib metis.lib amd.lib camd.lib ccolamd.lib colamd.lib"
set "OBINC=/I"%CONDA_SS%\include\openblas""
set "OBLIB=openblas.lib"

set "RC=0"

if /I "%WHAT%"=="smoke"   goto smoke
if /I "%WHAT%"=="all"     goto smoke
if /I "%WHAT%"=="compare" goto compare
if /I "%WHAT%"=="openblas" goto openblas_smoke
if /I "%WHAT%"=="sn" goto sn
echo [supernodal] unknown target "%WHAT%" & set "RC=1" & goto end

:openblas_smoke
echo [supernodal] building exp_openblas_smoke (OpenBLAS BLAS3 link test)...
cl %CFLAGS% %OBINC% /Fe:Research\bin\exp_openblas_smoke.exe /Fo:Research\obj_exp\ ^
   Research\WS_B_solver\exp_openblas_smoke.cpp ^
   /link /LIBPATH:"%SSLIBDIR%" %OBLIB%
if errorlevel 1 ( echo [supernodal] openblas_smoke COMPILE/LINK FAILED & set "RC=1" & goto end )
echo [supernodal] running exp_openblas_smoke (PATH += conda bin for DLLs)...
set "PATH=%CONDA_SS%\bin;%PATH%"
"Research\bin\exp_openblas_smoke.exe"
if errorlevel 1 ( echo [supernodal] openblas_smoke RUN reported failure & set "RC=1" & goto end )
goto end

:smoke
echo [supernodal] building exp_cholmod_smoke (CHOLMOD/METIS link test)...
cl %CFLAGS% /Fe:Research\bin\exp_cholmod_smoke.exe /Fo:Research\obj_exp\ ^
   Research\WS_B_solver\exp_cholmod_smoke.cpp ^
   /link /LIBPATH:"%SSLIBDIR%" %SSLIBS%
if errorlevel 1 ( echo [supernodal] smoke COMPILE/LINK FAILED & set "RC=1" & goto end )
echo [supernodal] running exp_cholmod_smoke (PATH += conda bin for DLLs)...
set "PATH=%CONDA_SS%\bin;%PATH%"
"Research\bin\exp_cholmod_smoke.exe"
if errorlevel 1 ( echo [supernodal] smoke RUN reported failure & set "RC=1" & goto end )
if /I "%WHAT%"=="smoke" goto end

:compare
REM --- compile FrameCore core objs once (self-contained; correct source list per build_hp_bench.bat) ---
if not exist "Research\obj_sn_core" mkdir "Research\obj_sn_core"
if not exist "Research\obj_sn_core\FrameSolver.obj" (
  echo [supernodal] compiling FrameCore core objs into Research\obj_sn_core ...
  cl %CFLAGS% %INCCORE% /MP /c /Fo:Research\obj_sn_core\ ^
     "%PRIV%\Section.cpp" "%PRIV%\Member.cpp" "%PRIV%\FrameModel.cpp" ^
     "%PRIV%\ElementStiffness.cpp" "%PRIV%\BeamColumnElement.cpp" "%PRIV%\MITC4ShellElement.cpp" ^
     "%PRIV%\FrameSolver.cpp" "%PRIV%\ElasticAllowable.cpp" "%PRIV%\Grillage.cpp" ^
     "%PRIV%\SelfWeight.cpp" "%PRIV%\Combination.cpp" "%PRIV%\InfluenceLine.cpp" ^
     "%PRIV%\ModalAnalysis.cpp" "%PRIV%\BucklingAnalysis.cpp" "%PRIV%\ResponseSpectrum.cpp" ^
     "%PRIV%\ModalDynamics.cpp" "%PRIV%\Connectivity.cpp" "%PRIV%\Collapse.cpp" ^
     "%PRIV%\DynamicCollapse.cpp" "%PRIV%\Reanalysis.cpp" "%PRIV%\PDeltaAnalysis.cpp" ^
     "%PRIV%\CorotationalAnalysis.cpp" "%PRIV%\TensionOnly.cpp" "%PRIV%\SizeOpt.cpp" "%PRIV%\Topology.cpp"
  if errorlevel 1 ( echo [supernodal] CORE COMPILE FAILED & set "RC=1" & goto end )
)
echo [supernodal] building exp_supernodal_compare (links Research\obj_sn_core)...
cl %CFLAGS% %INCCORE% /Fe:Research\bin\exp_supernodal_compare.exe /Fo:Research\obj_exp\ ^
   Research\WS_B_solver\exp_supernodal_compare.cpp Research\obj_sn_core\*.obj ^
   /link /LIBPATH:"%SSLIBDIR%" %SSLIBS%
if errorlevel 1 ( echo [supernodal] compare COMPILE/LINK FAILED & set "RC=1" & goto end )
echo [supernodal] OK -^> Research\bin\exp_supernodal_compare.exe
echo [supernodal] (DLLs: add "%CONDA_SS%\bin" to PATH before running)
goto end

:sn
if not exist "Research\obj_sn_core\FrameSolver.obj" ( echo [supernodal] need core - run "build_supernodal.bat compare" first & set "RC=1" & goto end )
echo [supernodal] building exp_sn_chol (self-built supernodal symbolic vs Eigen)...
cl %CFLAGS% %OBINC% %INCCORE% /Fe:Research\bin\exp_sn_chol.exe /Fo:Research\obj_exp\ ^
   Research\WS_B_solver\exp_sn_chol.cpp Research\obj_sn_core\*.obj ^
   /link /LIBPATH:"%SSLIBDIR%" %SSLIBS% %OBLIB%
if errorlevel 1 ( echo [supernodal] sn COMPILE/LINK FAILED & set "RC=1" & goto end )
echo [supernodal] running exp_sn_chol...
set "PATH=%CONDA_SS%\bin;%PATH%"
"Research\bin\exp_sn_chol.exe"
if errorlevel 1 ( echo [supernodal] sn RUN reported failure & set "RC=1" & goto end )

:end
popd
exit /b %RC%
