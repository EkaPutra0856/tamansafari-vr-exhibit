# Wombat redo: sync the hologram ring fix to the original + compile it, then record the full demo again (frames only;
# splice_wombat.py swaps the wombat part into the previous video). The copy must already be compiled and wombat_place.py applied.
$ErrorActionPreference = "Continue"
$Copy = "D:\Projek UE\ZooEka2VR"
$Orig = "C:\Users\USER\Perforce\EkaZoo2\Zoo"
$Logs = "$Copy\Saved\VRDemo"
foreach ($f in @("Source\Zoo\Hologram\ZooHologramComponent.cpp")) { if (Test-Path "$Orig\$f") { attrib -R "$Orig\$f" }; Copy-Item "$Copy\$f" "$Orig\$f" -Force }
& "E:\UE_5.8\Engine\Build\BatchFiles\Build.bat" ZooEditor Win64 Development "-Project=$Orig\Zoo.uproject" -WaitMutex -NoHotReload *> "$Logs\build_original_wombat.log"
Write-Output "original build exit $LASTEXITCODE"
& "$Copy\Tools\VRDemo\run_demo.cmd" *> $null
Write-Output "run exit $LASTEXITCODE"
Select-String -Path "$Copy\Saved\DemoVideo\demo_run.log" -Pattern "LogZooVRDemo: Warning|marker \d|Demo end|ViewportClosed|disabled: its animal" | ForEach-Object { $_.Line -replace '^\[[0-9.:-]+\]\[ *\d+\]','' }
