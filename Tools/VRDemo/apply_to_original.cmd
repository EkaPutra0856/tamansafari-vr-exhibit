@echo off
rem Copies the shared Zoo changes (Duma clear of the HUD + centred, quest tracker fold button) from the VR copy into the
rem original Perforce workspace, through p4 edit / p4 add. Run AFTER "p4 login" (and set P4CLIENT to the EkaZoo2 workspace).
rem Usage: apply_to_original.cmd [-build]
setlocal
set SRC=D:\Projek UE\ZooEka2VR
set DST=C:\Users\USER\Perforce\EkaZoo2\Zoo
p4 login -s >nul 2>&1
if errorlevel 1 (
	echo Perforce session expired: run "p4 login" first.
	exit /b 1
)
set EDIT=Source\Zoo\UI\ZooHUD.h Source\Zoo\UI\ZooHUD.cpp Source\Zoo\Duma\ZooDumaController.h Source\Zoo\Duma\ZooDumaController.cpp
set ADD=Source\Zoo\UI\ZooViewClutter.h Source\Zoo\UI\ZooViewClutter.cpp Source\Zoo\UI\ZooQuestTrackerPeek.h Source\Zoo\UI\ZooQuestTrackerPeek.cpp
for %%f in (%EDIT%) do (
	p4 edit "%DST%\%%f" || exit /b 1
	copy /y "%SRC%\%%f" "%DST%\%%f" >nul || exit /b 1
)
for %%f in (%ADD%) do (
	copy /y "%SRC%\%%f" "%DST%\%%f" >nul || exit /b 1
	p4 add "%DST%\%%f" || exit /b 1
)
echo Files opened in Perforce (p4 opened to review). Not submitted.
if "%~1"=="-build" (
	echo Building ZooEditor of the original (close its editor first)...
	call "E:\UE_5.8\Engine\Build\BatchFiles\Build.bat" ZooEditor Win64 Development "-Project=%DST%\Zoo.uproject" -WaitMutex -NoHotReload
)
endlocal
