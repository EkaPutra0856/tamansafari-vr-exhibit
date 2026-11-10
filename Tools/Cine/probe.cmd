@echo off
rem Usage: probe.cmd <in.json> <out.json>  -- runs zoo.cine.Probe in L_Zoo_Cine (-game, nullrhi). Editor must be CLOSED.
set UPROJ=D:\Projek UE\ZooEka2VR\Zoo.uproject
set UE=E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe
"%UE%" "%UPROJ%" "/Game/Zoo/Maps/L_Zoo_Cine?game=/Script/Zoo.ZooCinematicGameMode" -game -nullrhi -nosound -unattended -FixedSeed -ExecCmds="zoo.cine.Probe %~1 %~2, quit" -log -stdout -FullStdOutLogOutput
