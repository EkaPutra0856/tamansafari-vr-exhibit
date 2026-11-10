# Run the asset commandlets (wombat habitat, media card), then record and encode. No compile.
$ErrorActionPreference = "Continue"
$Copy = "D:\Projek UE\ZooEka2VR"
$Logs = "$Copy\Saved\VRDemo"
$Cmd = "E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
foreach ($script in @("wombat_adjust.py")) {
	& $Cmd "$Copy\Zoo.uproject" -run=pythonscript -script="$Copy\Tools\VRDemo\$script" -unattended -nullrhi -stdout -FullStdOutLogOutput -NoShaderCompile -EnablePlugins=PythonScriptPlugin *> "$Logs\$script.log"
	Select-String -Path "$Logs\$script.log" -Pattern "\[MediaCard\]|\[WombatHabitat\]|LogPython: Error|Error: " | Select-Object -First 20 | ForEach-Object { $_.Line -replace '^.*LogPython: ','' }
}
& "$Copy\Tools\VRDemo\run_demo.cmd" *> $null
Write-Output "run exit $LASTEXITCODE"
Select-String -Path "$Copy\Saved\DemoVideo\demo_run.log" -Pattern "LogZooVRDemo: Warning|arrived|Demo end|ViewportClosed|disabled: its animal" | ForEach-Object { $_.Line -replace '^\[[0-9.:-]+\]\[ *\d+\]','' }
python -I "$Copy\Tools\VRDemo\make_mp4.py" 2>&1 | Select-String -Pattern "frames =|Video:" | ForEach-Object { $_.Line }
