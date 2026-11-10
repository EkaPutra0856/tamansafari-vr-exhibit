# Overlays all planned placements on the terrain preview (north up) for review.
param([string]$OutPng = "$PSScriptRoot\..\..\Saved\ZooBuild\plan_preview.png", [double]$X0 = -70, [double]$X1 = 60, [double]$Y0 = -75, [double]$Y1 = 75, [int]$PxPerM = 7)
Add-Type -AssemblyName System.Drawing
$terrain = [System.Drawing.Image]::FromFile((Resolve-Path "$PSScriptRoot\..\ZooTerrain\out\zoo_preview.png"))
$W = [int](($Y1 - $Y0) * $PxPerM); $H = [int](($X1 - $X0) * $PxPerM)
$bmp = New-Object System.Drawing.Bitmap $W, $H
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = 'NearestNeighbor'
# source rect in the 505px preview: col = (y+126)/0.5, row = 504 - (x+126)/0.5
$sx = ($Y0 + 126) / 0.5; $sy = 504 - ($X1 + 126) / 0.5
$g.DrawImage($terrain, (New-Object System.Drawing.Rectangle 0, 0, $W, $H), [float]$sx, [float]$sy, [float](($Y1 - $Y0) / 0.5), [float](($X1 - $X0) / 0.5), [System.Drawing.GraphicsUnit]::Pixel)
$g.SmoothingMode = 'AntiAlias'
function P([double]$xcm, [double]$ycm) { $px = (($ycm / 100.0) - $Y0) * $PxPerM; $py = ($X1 - ($xcm / 100.0)) * $PxPerM; return @($px, $py) }
$palette = @{ 'GIR' = [System.Drawing.Color]::Orange; 'WOM' = [System.Drawing.Color]::Magenta; 'POND' = [System.Drawing.Color]::Cyan; 'PATH' = [System.Drawing.Color]::White; 'ENT' = [System.Drawing.Color]::Yellow; 'REST' = [System.Drawing.Color]::Yellow; 'VEG' = [System.Drawing.Color]::Lime; 'BND' = [System.Drawing.Color]::Red }
foreach ($f in Get-ChildItem "$PSScriptRoot\..\..\Saved\ZooBuild\plan_*.json") {
    $plan = Get-Content $f.FullName -Raw | ConvertFrom-Json
    foreach ($grp in $plan.groups) {
        $key = $grp.label.Split('_')[0]
        $col = $palette[$key]; if (-not $col) { $col = [System.Drawing.Color]::Gray }
        $isTree = $grp.label -match 'Forest$|Groves|Specimens|Trees'
        foreach ($m in $grp.meshes) {
            foreach ($i in $m.inst) {
                $p = P $i[0] $i[1]
                if ($p[0] -lt 0 -or $p[1] -lt 0 -or $p[0] -gt $W -or $p[1] -gt $H) { continue }
                if ($isTree) {
                    $r = 3.2 * $i[6] * $PxPerM
                    $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(170, $col)), 1.5
                    $g.DrawEllipse($pen, [float]($p[0] - $r), [float]($p[1] - $r), [float](2 * $r), [float](2 * $r)); $pen.Dispose()
                }
                $b = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(230, $col))
                $g.FillEllipse($b, [float]($p[0] - 2), [float]($p[1] - 2), 4, 4); $b.Dispose()
            }
        }
    }
    foreach ($h in $plan.hero) {
        $p = P $h.loc[0] $h.loc[1]
        $b = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 30, 30, 255))
        $g.FillRectangle($b, [float]($p[0] - 4), [float]($p[1] - 4), 8, 8); $b.Dispose()
    }
}
$bmp.Save($OutPng, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose(); $terrain.Dispose()
"saved $OutPng"
