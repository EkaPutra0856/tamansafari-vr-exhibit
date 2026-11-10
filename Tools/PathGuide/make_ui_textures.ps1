# Renders the Path Guide UI textures (PNG with alpha) in the zoo signage style (same palette / grain as Tools/ZooSigns).
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File make_ui_textures.ps1
# Output: Tools/PathGuide/out/ui/T_PG_*.png  (imported into /Game/PathGuide/UI/Textures by the import step)
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = Join-Path $here 'out\ui'
New-Item -ItemType Directory -Force $out | Out-Null

# palette (identical to make_signs.ps1)
$cBg     = [System.Drawing.Color]::FromArgb(255, 34, 56, 42)
$cBgDark = [System.Drawing.Color]::FromArgb(255, 24, 40, 30)
$cCream  = [System.Drawing.Color]::FromArgb(255, 243, 236, 218)
$cOchre  = [System.Drawing.Color]::FromArgb(255, 205, 146, 60)
$cSand   = [System.Drawing.Color]::FromArgb(255, 226, 205, 160)
$cMuted  = [System.Drawing.Color]::FromArgb(255, 190, 200, 180)

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

function Add-Grain($g, [int]$w, [int]$h, [System.Drawing.Drawing2D.GraphicsPath]$clip, [int]$seed) {
    $g.SetClip($clip)
    $rnd = New-Object System.Random $seed
    for ($i = 0; $i -lt ($w * $h / 700); $i++) {
        $b = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb($rnd.Next(5, 16), 255, 255, 255))
        $g.FillRectangle($b, $rnd.Next(0, $w), $rnd.Next(0, $h), 2, 2)
        $b.Dispose()
    }
    $g.ResetClip()
}

# ------------------------------------------------------------------ panels
function Panel([string]$name, [int]$size, [float]$radius, [int]$alpha, [bool]$border, $topColor, $bottomColor, $borderColor) {
    $bmp, $g = New-Canvas $size $size
    $path = Round-Rect 1 1 ($size - 2) ($size - 2) $radius
    $rect = New-Object System.Drawing.Rectangle 0, 0, $size, $size
    $top = [System.Drawing.Color]::FromArgb($alpha, $topColor.R, $topColor.G, $topColor.B)
    $bottom = [System.Drawing.Color]::FromArgb($alpha, $bottomColor.R, $bottomColor.G, $bottomColor.B)
    $lg = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, $top, $bottom, 90
    $g.FillPath($lg, $path)
    Add-Grain $g $size $size $path 11
    if ($border) {
        $inset = [float]($size / 34)
        $inner = Round-Rect ($inset + 1) ($inset + 1) ($size - 2 * $inset - 2) ($size - 2 * $inset - 2) ([Math]::Max(4, $radius - $inset))
        $pen = New-Object System.Drawing.Pen $borderColor, ([Math]::Max(2.5, $size / 170))
        $g.DrawPath($pen, $inner)
        $pen.Dispose(); $inner.Dispose()
    }
    $lg.Dispose(); $path.Dispose()
    Save $bmp $g $name
}

# main panel: forest green sign board with the ochre inner rule (use as a 9-slice box brush, margin ~0.12)
Panel 'T_PG_Panel'        512 34 250 $true  $cBg     $cBgDark $cOchre
# flat / card backgrounds
Panel 'T_PG_PanelFlat'    256 20 235 $false $cBgDark $cBgDark $cOchre
Panel 'T_PG_Card'         256 18 215 $true  $cBg     $cBgDark ([System.Drawing.Color]::FromArgb(110, 190, 200, 180))
Panel 'T_PG_CardActive'   256 18 245 $true  ([System.Drawing.Color]::FromArgb(255, 52, 66, 44)) $cBgDark $cOchre

# ------------------------------------------------------------------ badge (numbered circle)
$bmp, $g = New-Canvas 128 128
$rim = New-Object System.Drawing.SolidBrush $cBgDark
$g.FillEllipse($rim, 4, 4, 120, 120)
$fill = New-Object System.Drawing.SolidBrush $cOchre
$g.FillEllipse($fill, 10, 10, 108, 108)
$pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(140, 255, 240, 200)), 3
$g.DrawEllipse($pen, 16, 16, 96, 96)
$rim.Dispose(); $fill.Dispose(); $pen.Dispose()
Save $bmp $g 'T_PG_Badge'

# ------------------------------------------------------------------ waypoint pin (teardrop, anchor at bottom centre)
$bmp, $g = New-Canvas 128 160
$pin = New-Object System.Drawing.Drawing2D.GraphicsPath
$pin.AddArc(14, 10, 100, 100, 150, 240)
$pin.AddLine(107, 82, 64, 152)
$pin.AddLine(64, 152, 21, 82)
$pin.CloseFigure()
$shadow = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(90, 0, 0, 0))
$m = New-Object System.Drawing.Drawing2D.Matrix
$m.Translate(3, 4)
$sp = $pin.Clone(); $sp.Transform($m)
$g.FillPath($shadow, $sp)
$fill = New-Object System.Drawing.SolidBrush $cOchre
$g.FillPath($fill, $pin)
$pen = New-Object System.Drawing.Pen $cCream, 6
$pen.LineJoin = 'Round'
$g.DrawPath($pen, $pin)
$dark = New-Object System.Drawing.SolidBrush $cBgDark
$g.FillEllipse($dark, 44, 40, 40, 40)
$shadow.Dispose(); $fill.Dispose(); $pen.Dispose(); $dark.Dispose(); $pin.Dispose(); $sp.Dispose()
Save $bmp $g 'T_PG_Pin'

# ------------------------------------------------------------------ off-screen edge arrow (points RIGHT = 0 degrees)
$bmp, $g = New-Canvas 128 128
$arrow = New-Object System.Drawing.Drawing2D.GraphicsPath
$pts = @(
    (New-Object System.Drawing.PointF 112, 64), (New-Object System.Drawing.PointF 40, 16),
    (New-Object System.Drawing.PointF 62, 64),  (New-Object System.Drawing.PointF 40, 112))
$arrow.AddPolygon($pts)
$fill = New-Object System.Drawing.SolidBrush $cOchre
$g.FillPath($fill, $arrow)
$pen = New-Object System.Drawing.Pen $cCream, 6
$pen.LineJoin = 'Round'
$g.DrawPath($pen, $arrow)
$fill.Dispose(); $pen.Dispose(); $arrow.Dispose()
Save $bmp $g 'T_PG_EdgeArrow'

# ------------------------------------------------------------------ icons (cream)
# navigate: location arrow
$bmp, $g = New-Canvas 128 128
$nav = New-Object System.Drawing.Drawing2D.GraphicsPath
$nav.AddPolygon(@(
    (New-Object System.Drawing.PointF 110, 18), (New-Object System.Drawing.PointF 26, 54),
    (New-Object System.Drawing.PointF 62, 66),  (New-Object System.Drawing.PointF 74, 102)))
$fill = New-Object System.Drawing.SolidBrush $cCream
$g.FillPath($fill, $nav)
$fill.Dispose(); $nav.Dispose()
Save $bmp $g 'T_PG_IconNavigate'

# close: X
$bmp, $g = New-Canvas 128 128
$pen = New-Object System.Drawing.Pen $cCream, 12
$pen.StartCap = 'Round'; $pen.EndCap = 'Round'
$g.DrawLine($pen, 34, 34, 94, 94)
$g.DrawLine($pen, 94, 34, 34, 94)
$pen.Dispose()
Save $bmp $g 'T_PG_IconClose'

# stop: rounded square
$bmp, $g = New-Canvas 128 128
$sq = Round-Rect 32 32 64 64 12
$fill = New-Object System.Drawing.SolidBrush $cCream
$g.FillPath($fill, $sq)
$fill.Dispose(); $sq.Dispose()
Save $bmp $g 'T_PG_IconStop'

# ------------------------------------------------------------------ divider (ochre rule fading at both ends)
$bmp, $g = New-Canvas 512 8
$rect = New-Object System.Drawing.Rectangle 0, 0, 512, 8
$lg = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, $cOchre, $cOchre, 0
$blend = New-Object System.Drawing.Drawing2D.ColorBlend 5
$blend.Colors = @(
    [System.Drawing.Color]::FromArgb(0, 205, 146, 60), $cOchre, $cOchre, $cOchre, [System.Drawing.Color]::FromArgb(0, 205, 146, 60))
$blend.Positions = @(0.0, 0.18, 0.5, 0.82, 1.0)
$lg.InterpolationColors = $blend
$g.FillRectangle($lg, 0, 2, 512, 4)
$lg.Dispose()
Save $bmp $g 'T_PG_Divider'

# ------------------------------------------------------------------ leaf ornament (same leaf as the signs)
$bmp, $g = New-Canvas 128 128
$cx = 64; $cy = 64; $s = 52
$leaf = New-Object System.Drawing.Drawing2D.GraphicsPath
$leaf.AddBezier($cx, $cy + $s, $cx - $s * 0.9, $cy + $s * 0.2, $cx - $s * 0.6, $cy - $s * 0.8, $cx, $cy - $s)
$leaf.AddBezier($cx, $cy - $s, $cx + $s * 0.6, $cy - $s * 0.8, $cx + $s * 0.9, $cy + $s * 0.2, $cx, $cy + $s)
$fill = New-Object System.Drawing.SolidBrush $cSand
$g.FillPath($fill, $leaf)
$vein = New-Object System.Drawing.Pen $cBgDark, ($s / 10)
$g.DrawLine($vein, $cx, $cy + $s * 0.95, $cx, $cy - $s * 0.7)
$fill.Dispose(); $vein.Dispose(); $leaf.Dispose()
Save $bmp $g 'T_PG_Leaf'
