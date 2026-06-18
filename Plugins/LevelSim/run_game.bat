@echo off
REM One-click playable LevelSim station (PC, windowed).
REM Scene is spawned at runtime by the game mode on the engine's empty Entry map (no .umap asset).
REM NOTE: keep this file ASCII-only - cmd + cp950 mangles UTF-8 comments.
set "UE=E:\project\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
set "PROJ=E:\project\ArchSim\ArchSim.uproject"
"%UE%" "%PROJ%" /Engine/Maps/Entry?game=/Script/LevelSimPlay.LevelSimGameMode -game -windowed -ResX=1600 -ResY=900 -NoSplash
