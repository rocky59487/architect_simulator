@echo off
REM Headless smoke test: auto-walks overview -> bubble -> telescope(BM) -> telescope(P1),
REM takes a screenshot at each stop, then quits. Screenshots: ArchSim\Saved\Screenshots\.
REM Log lines: search "[LevelSimSmoke]" in ArchSim\Saved\Logs\ArchSim.log.
REM NOTE: keep this file ASCII-only - cmd + cp950 mangles UTF-8 comments.
set "UE=E:\project\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set "PROJ=E:\project\ArchSim\ArchSim.uproject"
"%UE%" "%PROJ%" /Engine/Maps/Entry?game=/Script/LevelSimPlay.LevelSimGameMode -game -RenderOffscreen -ResX=1280 -ResY=720 -levelsim.smoke -NoSplash -Unattended -log
