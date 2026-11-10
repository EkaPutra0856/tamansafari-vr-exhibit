@echo off
rem Usage: render.cmd Cinematic|FPV Preview|Final  (Cinematic Shot08 / Shot13: that animal shot only, into the Final folder)
rem   -- Movie Render Queue render of /Game/Cine/LS_Zoo_Cine_<film> in L_Zoo_Cine.
rem Editor must be CLOSED. Frames go to Saved\Cine\Render\<tag>\LS_Zoo_Cine_<film>\. -FixedSeed keeps the animal AI identical between Preview and Final.
setlocal
set UPROJ=D:\Projek UE\ZooEka2VR\Zoo.uproject
set UE=E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe
set FILM=%~1
if "%FILM%"=="" set FILM=Cinematic
set TAG=%~2
if "%TAG%"=="" set TAG=Preview
set SEQ=/Game/Cine/LS_Zoo_Cine_%FILM%.LS_Zoo_Cine_%FILM%
set PRESET=/Game/Cine/MRQ/MRQ_Cine_Preview_720p.MRQ_Cine_Preview_720p
if /I "%TAG%"=="Final" set PRESET=/Game/Cine/MRQ/MRQ_Cine_Final_1080p.MRQ_Cine_Final_1080p
if /I "%TAG%%FILM%"=="FinalFPV" set PRESET=/Game/Cine/MRQ/MRQ_Cine_Final_1080p_FPV.MRQ_Cine_Final_1080p_FPV
if /I "%TAG:~0,4%"=="Shot" set PRESET=/Game/Cine/MRQ/MRQ_Cine_Final_1080p_%TAG%.MRQ_Cine_Final_1080p_%TAG%
"%UE%" "%UPROJ%" /Game/Zoo/Maps/L_Zoo_Cine -game -LevelSequence="%SEQ%" -MoviePipelineConfig="%PRESET%" -MoviePipelineLocalExecutorClass=/Script/MovieRenderPipelineCore.MoviePipelineInProcessExecutor -windowed -ResX=1280 -ResY=720 -NoLoadingScreen -NoScreenMessages -nohmd -FixedSeed -log -stdout -FullStdOutLogOutput -ABSLOG="D:\Projek UE\ZooEka2VR\Saved\Cine\render_%FILM%_%TAG%.log" %3 %4 %5
endlocal
