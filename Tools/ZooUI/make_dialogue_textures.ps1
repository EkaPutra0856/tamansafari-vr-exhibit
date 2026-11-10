# Renders the dialogue UI textures (PNG with alpha) in the same style as the Path Guide UI (Tools/PathGuide/make_ui_textures.ps1).
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File make_dialogue_textures.ps1
# Output: Tools/ZooUI/out/ui/T_Zoo_DLG_*.png  (imported into /Game/Zoo/Assets/Dialogue/Textures by the import step)
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = Join-Path $here 'out\ui'
New-Item -ItemType Directory -Force $out | Out-Null

# palette (identical to make_ui_textures.ps1)
$cOchre = [System.Drawing.Color]::FromArgb(255, 205, 146, 60)
$cSand  = [System.Drawing.Color]::FromArgb(153, 226, 205, 160)

function New-Canvas([int]$w, [int]$h) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.PixelOffsetMode = 'HighQuality'
    $g.Clear([System.Drawing.Color]::Transparent)
    return @($bmp, $g)
}

function Save($bmp, $g, [string]$name) {
    $p = Join-Path $out ($name + '.png')
    $bmp.Save($p, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Output "wrote $p"
}

function Round-Rect([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

# ------------------------------------------------------------------ portrait frame (rounded square ring; the middle stays transparent)
$bmp, $g = New-Canvas 256 256
$ring = Round-Rect 3 3 250 250 28
$ringPen = New-Object System.Drawing.Pen $cOchre, 6
$g.DrawPath($ringPen, $ring)
$inner = Round-Rect 11 11 234 234 20
$innerPen = New-Object System.Drawing.Pen $cSand, 2
$g.DrawPath($innerPen, $inner)
$ringPen.Dispose(); $innerPen.Dispose(); $ring.Dispose(); $inner.Dispose()
Save $bmp $g 'T_Zoo_DLG_PortraitFrame'