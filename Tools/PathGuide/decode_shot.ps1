# Decodes a base64 viewport capture written by the editor (Saved/PathGuideShots/<name>.txt) into a resized PNG.
# Usage: powershell -ExecutionPolicy Bypass -File decode_shot.ps1 -Name aerial_route [-Width 1400]
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [int]$Width = 1400
)
Add-Type -AssemblyName System.Drawing
$dir = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) '..\..\Saved\PathGuideShots'
$txt = Join-Path $dir ($Name + '.txt')
$bytes = [Convert]::FromBase64String((Get-Content $txt -Raw).Trim())
$ms = New-Object System.IO.MemoryStream (, $bytes)
$src = [System.Drawing.Image]::FromStream($ms)
$h = [int]($src.Height * $Width / $src.Width)
$dst = New-Object System.Drawing.Bitmap $Width, $h
$g = [System.Drawing.Graphics]::FromImage($dst)
$g.InterpolationMode = 'HighQualityBicubic'
$g.DrawImage($src, 0, 0, $Width, $h)
$out = Join-Path $dir ($Name + '.png')
$dst.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $dst.Dispose(); $src.Dispose(); $ms.Dispose()
Write-Output $out
