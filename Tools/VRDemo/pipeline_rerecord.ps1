# Compile the copy, scale the wombat in L_Zoo, sync shared C++ to the original + compile it, record, encode.
$ErrorActionPreference = "Continue"
$Copy = "D:\Projek UE\ZooEka2VR"
$Orig = "C:\Users\USER\Perforce\EkaZoo2\Zoo"
$Logs = "$Copy\Saved\VRDemo"
& "E:\UE_5.8\Engine\Build\BatchFiles\Build.bat" ZooEditor Win64 Development "-Project=$Copy\Zoo.uproject" -WaitMutex -NoHotReload *> "$Logs\build_copy_final.log"
Write-Output "copy build exit $LASTEXITCODE"
if ($LASTEXITCODE -ne 0) { Select-String -Path "$Logs\build_copy_final.log" -Pattern "error" | Select-Object -First 10 | ForEach-Object { $_.Line }; exit 1 }
& "E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Copy\Zoo.uproject" -run=pythonscript -script="$Copy\Tools\VRDemo\scale_wombat.py" -unattended -nullrhi -stdout -FullStdOutLogOutput -NoShaderCompile -EnablePlugins=PythonScriptPlugin *> "$Logs\scale_wombat.log"
Select-String -Path "$Logs\scale_wombat.log" -Pattern "\[Wombat\]|LogPython: Error" | ForEach-Object { $_.Line -replace '^.*LogPython: ','' }
$Shared = @("Source\Zoo\Duma\ZooDuma.h", "Source\Zoo\Duma\ZooDuma.cpp", "Source\Zoo\Duma\ZooDumaController.h", "Source\Zoo\Duma\ZooDumaController.cpp", "Source\Zoo\Hologram\ZooHologramComponent.h", "Source\Zoo\Hologram\ZooHologramComponent.cpp", "Source\Zoo\Hologram\ZooHologramProfile.h")
foreach ($f in $Shared) { if (Test-Path "$Orig\$f") { attrib -R "$Orig\$f" }; Copy-Item "$Copy\$f" "$Orig\$f" -Force; Write-Output "synced $f" }
& "E:\UE_5.8\Engine\Build\BatchFiles\Build.bat" ZooEditor Win64 Development "-Project=$Orig\Zoo.uproject" -WaitMutex -NoHotReload *> "$Logs\build_original_final.log"
Write-Output "original build exit $LASTEXITCODE"
& "$Copy\Tools\VRDemo\run_demo.cmd" *> $null
Write-Output "run exit $LASTEXITCODE"
Select-String -Path "$Copy\Saved\DemoVideo\demo_run.log" -Pattern "LogZooVRDemo: Warning|arrived|Demo end|LogExit: Preparing" | ForEach-Object { $_.Line -replace '^\[[0-9.:-]+\]\[ *\d+\]','' }
python -I "$Copy\Tools\VRDemo\make_mp4.py" 2>&1 | Select-String -Pattern "frames =|Video:|^\|" | ForEach-Object { $_.Line }
