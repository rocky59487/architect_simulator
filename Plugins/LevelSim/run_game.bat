@echo off
REM One-click playable LevelSim station (PC, windowed).
REM Scene is spawned at runtime by the game mode on the engine's empty Entry map (no .umap asset).
REM NOTE: keep this file ASCII-only - cmd + cp950 mangles UTF-8 comments.
REM
REM UE_ENGINE_ROOT env var overrides the default search; the default falls back to a
REM sibling UE_5.7 directory next to the repo root (%~dp0 = directory of this script).
REM PROJ is derived from %~dp0 so a fresh clone works without editing this file.
if not defined UE_ENGINE_ROOT set "UE_ENGINE_ROOT=%~dp0..\..\..\UE_5.7"
set "UE=%UE_ENGINE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe"
set "PROJ=%~dp0..\..\ArchSim.uproject"
"%UE%" "%PROJ%" /Engine/Maps/Entry?game=/Script/LevelSimPlay.LevelSimGameMode -game -windowed -ResX=1600 -ResY=900 -NoSplash
