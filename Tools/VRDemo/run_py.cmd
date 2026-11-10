@echo off
rem Usage: run_py.cmd <script.py> [extra UE args]   -- runs a Python script in the editor commandlet (nullrhi). Editor must be CLOSED.
set UPROJ=D:\Projek UE\ZooEka2VR\Zoo.uproject
set UE=E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe
set SCRIPT=%~1
shift
"%UE%" "%UPROJ%" -run=pythonscript -script="%SCRIPT%" -unattended -nullrhi -stdout -FullStdOutLogOutput -NoShaderCompile -EnablePlugins=PythonScriptPlugin %1 %2 %3 %4 %5
