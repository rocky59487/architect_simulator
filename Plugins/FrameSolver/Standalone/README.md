# FrameCore — standalone gate

Compiles the engine-agnostic FrameCore sources + `main.cpp` against UE's bundled
Eigen 3.4.0, runs the analytic fixtures, exits 0 iff all pass.

## Quick start
```bat
:: from E:\project\FrameSolver\
Standalone\build.bat
```
`build.bat` locates Visual Studio via `vswhere -prerelease` (VS18 is a preview
build; plain `-latest` returns nothing), calls `vcvars64.bat`, runs `cl`, then the exe.

## Manual cl (if you already have an x64 Native Tools prompt)
```bat
cl /nologo /EHsc /std:c++17 /O2 /MD /utf-8 /DEIGEN_MPL2_ONLY ^
   /I"E:\project\UE_5.7\Engine\Source\ThirdParty\Eigen" ^
   /I"Source\FrameCore\Public" /I"Source\FrameCore\Private" ^
   Source\FrameCore\Private\*.cpp Standalone\main.cpp ^
   /Fe:Standalone\frametest.exe /Fo:Standalone\obj\
```
(omit `FrameCoreModule.cpp` — it is UE-only; the explicit list in `build.bat` already does.)

## CMake alternative
```bat
cmake -S Standalone -B Standalone\build
cmake --build Standalone\build
```

## Expected output
```
[F1] cantilever ... [F2] simply-supported ... [F3] mechanism ... [F4] vertical column ...
ALL PASS  (failures=0)
```
