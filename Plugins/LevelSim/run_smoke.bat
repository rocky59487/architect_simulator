@echo off
REM Headless smoke test: auto-walks overview -> bubble -> telescope(BM) -> telescope(P1),
REM takes a screenshot at each stop, then quits. Screenshots: ArchSim\Saved\Screenshots\.
REM Log lines: search "[LevelSimSmoke]" in ArchSim\Saved\Logs\ArchSim.log.
REM NOTE: keep this file ASCII-only - cmd + cp950 mangles UTF-8 comments.
REM
REM UE_ENGINE_ROOT env var overrides the default search; the default falls back to a
REM sibling UE_5.7 directory next to the repo root (%~dp0 = directory of this script).
REM PROJ is derived from %~dp0 so a fresh clone works without editing this file.
if not defined UE_ENGINE_ROOT set "UE_ENGINE_ROOT=%~dp0..\..\..\UE_5.7"
set "UE=%UE_ENGINE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set "PROJ=%~dp0..\..\ArchSim.uproject"
"%UE%" "%PROJ%" /Engine/Maps/Entry?game=/Script/LevelSimPlay.LevelSimGameMode -game -RenderOffscreen -ResX=1280 -ResY=720 -levelsim.smoke -NoSplash -Unattended -log
