@echo off
rem Usage: run_demo.cmd [script.json] [extra UE args]
rem Runs the VR autopilot demo in a 1080p window at a fixed 30 fps game step, captures every frame to Saved\DemoVideo\Frames.
rem Editor must be CLOSED (it shares the module DLL only when not compiling; a running editor is fine for a plain run).
set UPROJ=D:\Projek UE\ZooEka2VR\Zoo.uproject
set UE=E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe
set SCRIPT=%~1
if "%SCRIPT%"=="" set SCRIPT=D:\Projek UE\ZooEka2VR\Content\ZooVR\Demo\ZooVRDemoScript.json
shift
"%UE%" "%UPROJ%" /Game/Zoo/Maps/L_Zoo -game -nohmd -ZooVRDemo -ZooVRDemoCapture -ZooVRDemoScript="%SCRIPT%" -UseFixedTimeStep -FPS=30 -NoVSync -NoScreenMessages -Windowed -ResX=1920 -ResY=1080 -log -ABSLOG=D:\Projek UE\ZooEka2VR\Saved\DemoVideo\demo_run.log %1 %2 %3 %4 %5
