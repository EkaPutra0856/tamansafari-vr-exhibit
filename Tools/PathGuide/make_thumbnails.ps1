# Turns the raw in-level captures (Saved/PathGuideShots/thumb_<Name>.png, 1024x576) into the card thumbnails:
# a light warm grade (matches the signage palette), resized to 512x288, plus a contact sheet for review.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File make_thumbnails.ps1
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$raw = Join-Path $here '..\..\Saved\PathGuideShots'
$out = Join-Path $here 'out\thumbs'
New-Item -ItemType Directory -Force $out | Out-Null

$names = 'Entrance', 'Giraffe_Paddock', 'Giraffe_Deck', 'PondBridge', 'Wombat_Burrow', 'RestArea'
$w = 512; $h = 288

$ia = New-Object System.Drawing.Imaging.ImageAttributes
$cm = New-Object System.Drawing.Imaging.ColorMatrix
$cm.Matrix00 = 1.04; $cm.Matrix11 = 1.00; $cm.Matrix22 = 0.94   # slight warmth
$cm.Matrix40 = 0.008; $cm.Matrix41 = 0.004; $cm.Matrix42 = 0.0
$ia.SetColorMatrix($cm)

$sheet = New-Object System.Drawing.Bitmap ($w * 3 + 40), ($h * 2 + 30)
$sg = [System.Drawing.Graphics]::FromImage($sheet)
$sg.Clear([System.Drawing.Color]::FromArgb(255, 24, 40, 30))
$i = 0
foreach ($n in $names) {
    $srcPath = Join-Path $raw ("thumb_$n.png")
    $src = [System.Drawing.Image]::FromFile((Resolve-Path $srcPath))
    $dst = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($dst)
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.PixelOffsetMode = 'HighQuality'
    $g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $w, $h), 0, 0, $src.Width, $src.Height, [System.Drawing.GraphicsUnit]::Pixel, $ia)
    $g.Dispose()
    $path = Join-Path $out ("T_PG_Thumb_$n.png")
    $dst.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $col = $i % 3; $row = [int][Math]::Floor($i / 3)
    $sg.DrawImage($dst, 10 + $col * ($w + 10), 10 + $row * ($h + 10))
    $dst.Dispose(); $src.Dispose()
    Write-Output "wrote $path"
    $i++
}
$sg.Dispose()
$sheetPath = Join-Path $out 'contact_sheet.png'
$sheet.Save($sheetPath, [System.Drawing.Imaging.ImageFormat]::Png)
$sheet.Dispose()
Write-Output "contact sheet: $sheetPath"
