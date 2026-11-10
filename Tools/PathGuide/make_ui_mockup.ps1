# Renders the target look of the Path Guide UI (HUD while guiding + destination list) over real L_Zoo captures.
# Uses the real fonts (Lora / Source Sans 3), the generated UI textures and the place thumbnails.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File make_ui_mockup.ps1
# Output: Docs/PathGuide/ui_mockup_hud.png, Docs/PathGuide/ui_mockup_list.png
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $here '..\..')
$ui = Join-Path $here 'out\ui'
$thumbs = Join-Path $here 'out\thumbs'
$shots = Join-Path $root 'Saved\PathGuideShots'
$docs = Join-Path $root 'Docs\PathGuide'
New-Item -ItemType Directory -Force $docs | Out-Null

$dot = [string][char]0x00B7
# palette
$cBg = [System.Drawing.Color]::FromArgb(255, 34, 56, 42); $cBgDark = [System.Drawing.Color]::FromArgb(255, 24, 40, 30)
$cCream = [System.Drawing.Color]::FromArgb(255, 243, 236, 218); $cOchre = [System.Drawing.Color]::FromArgb(255, 205, 146, 60)
$cSand = [System.Drawing.Color]::FromArgb(255, 226, 205, 160); $cMuted = [System.Drawing.Color]::FromArgb(255, 190, 200, 180)

# fonts (the actual production faces)
$pfc = New-Object System.Drawing.Text.PrivateFontCollection
Get-ChildItem (Join-Path $root 'Content\PathGuide\UI\Fonts\Source') -Filter *.ttf | ForEach-Object { $pfc.AddFontFile($_.FullName) }
function Fam([string]$like) { return ($pfc.Families | Where-Object { $_.Name -like $like } | Select-Object -First 1) }
$famSerif = Fam 'Lora'
$famSans = Fam 'Source Sans 3'
$famSansSemi = Fam 'Source Sans 3 Semibold*'
if (-not $famSansSemi) { $famSansSemi = $famSans }
function Font($fam, [float]$px, $style = [System.Drawing.FontStyle]::Regular) { return New-Object System.Drawing.Font $fam, $px, $style, ([System.Drawing.GraphicsUnit]::Pixel) }

function New-G($bmp) { $g = [System.Drawing.Graphics]::FromImage($bmp); $g.SmoothingMode = 'AntiAlias'; $g.InterpolationMode = 'HighQualityBicubic'; $g.TextRenderingHint = 'AntiAliasGridFit'; return $g }
function Round-Rect([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath; $d = $r * 2
    $p.AddArc($x, $y, $d, $d, 180, 90); $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90); $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90); $p.CloseFigure(); return $p
}
function Panel($g, [float]$x, [float]$y, [float]$w, [float]$h, [float]$r, [int]$alpha, [bool]$rule, $border = $null) {
    $path = Round-Rect $x $y $w $h $r
    $rect = New-Object System.Drawing.RectangleF $x, $y, $w, $h
    $lg = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, ([System.Drawing.Color]::FromArgb($alpha, 34, 56, 42)), ([System.Drawing.Color]::FromArgb($alpha, 24, 40, 30)), 90
    $g.FillPath($lg, $path)
    if ($rule) {
        $inner = Round-Rect ($x + 9) ($y + 9) ($w - 18) ($h - 18) ([Math]::Max(4, $r - 8))
        $pen = New-Object System.Drawing.Pen ($(if ($border) { $border } else { $cOchre })), 2
        $g.DrawPath($pen, $inner); $pen.Dispose(); $inner.Dispose()
    }
    $lg.Dispose(); $path.Dispose()
}
function Text($g, [string]$s, $font, $color, [float]$x, [float]$y, [float]$w = 2000, [string]$align = 'Near') {
    $b = New-Object System.Drawing.SolidBrush $color
    $sf = New-Object System.Drawing.StringFormat; $sf.Alignment = $align; $sf.Trimming = 'EllipsisCharacter'; $sf.FormatFlags = 'NoWrap'
    $g.DrawString($s, $font, $b, (New-Object System.Drawing.RectangleF $x, $y, $w, 200), $sf)
    $b.Dispose(); $sf.Dispose()
}
function Img($g, [string]$path, [float]$x, [float]$y, [float]$w, [float]$h) {
    $i = [System.Drawing.Image]::FromFile($path); $g.DrawImage($i, $x, $y, $w, $h); $i.Dispose()
}
function Background([string]$file, [double]$dim) {
    $bmp = New-Object System.Drawing.Bitmap 1920, 1080
    $g = New-G $bmp
    $src = [System.Drawing.Image]::FromFile($file)
    $scale = [Math]::Max(1920.0 / $src.Width, 1080.0 / $src.Height)
    $dw = $src.Width * $scale; $dh = $src.Height * $scale
    $g.DrawImage($src, (1920 - $dw) / 2, (1080 - $dh) / 2, $dw, $dh)
    $src.Dispose()
    if ($dim -gt 0) { $b = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb([int](255 * $dim), 8, 14, 10)); $g.FillRectangle($b, 0, 0, 1920, 1080); $b.Dispose() }
    return @($bmp, $g)
}

# ================================================================= 1. HUD while guiding
$bmp, $g = Background (Join-Path $shots 'v3_moat.png') 0
# waypoint pin (destination on screen)
$px = 1130; $py = 330
Img $g (Join-Path $ui 'T_PG_Pin.png') ($px - 40) ($py - 100) 80 100
Text $g 'Giraffe Feeding Deck' (Font $famSerif 30 ([System.Drawing.FontStyle]::Bold)) ([System.Drawing.Color]::FromArgb(70, 0, 0, 0)) ($px - 209) ($py - 196) 420 'Center'
Text $g 'Giraffe Feeding Deck' (Font $famSerif 30 ([System.Drawing.FontStyle]::Bold)) $cCream ($px - 211) ($py - 198) 420 'Center'
Text $g '75 m' (Font $famSansSemi 24) $cOchre ($px - 211) ($py - 160) 420 'Center'
# Navigate button (guiding state)
$bx = 1920 - 48 - 500; $by = 1080 - 48 - 104
Panel $g $bx $by 500 104 20 238 $true
Img $g (Join-Path $ui 'T_PG_IconNavigate.png') ($bx + 26) ($by + 30) 44 44
Text $g 'To: Giraffe Feeding Deck' (Font $famSansSemi 27) $cCream ($bx + 84) ($by + 16) 330
Text $g '75 m' (Font $famSans 22) $cOchre ($bx + 84) ($by + 54) 200
Text $g '[N]' (Font $famSansSemi 20) $cMuted ($bx + 322) ($by + 58) 60
$stop = Round-Rect ($bx + 404) ($by + 26) 52 52 12
$sb = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(150, 24, 40, 30)); $g.FillPath($sb, $stop)
Img $g (Join-Path $ui 'T_PG_IconStop.png') ($bx + 404) ($by + 26) 52 52
# caption
Text $g 'HUD while guiding - Navigate button (bottom-right) + destination pin' (Font $famSans 20) ([System.Drawing.Color]::FromArgb(200, 255, 255, 255)) 24 1040 900
$bmp.Save((Join-Path $docs 'ui_mockup_hud.png'), [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()

# ================================================================= 2. destination list
$bmp, $g = Background (Join-Path $thumbs 'T_PG_Thumb_Entrance.png') 0.55
$pw = 760; $ph = 1010; $pxL = (1920 - $pw) / 2; $pyL = (1080 - $ph) / 2
Panel $g $pxL $pyL $pw $ph 34 246 $true
Img $g (Join-Path $ui 'T_PG_Leaf.png') ($pxL + 48) ($pyL + 44) 44 44
Text $g 'Where to?' (Font $famSerif 46 ([System.Drawing.FontStyle]::Bold)) $cCream ($pxL + 104) ($pyL + 36) 400
Img $g (Join-Path $ui 'T_PG_IconClose.png') ($pxL + $pw - 104) ($pyL + 38) 52 52
Img $g (Join-Path $ui 'T_PG_Divider.png') ($pxL + 40) ($pyL + 108) ($pw - 80) 8
$cards = @(
    @('1', 'Entrance', 'Main Entrance', 'The plaza, the trail map and the start of the circular trail.', ('8 m ' + $dot + ' < 1 min'), $false),
    @('2', 'Giraffe_Paddock', 'Giraffe Paddock', 'Look across the moat at the tallest animal on Earth.', ('45 m ' + $dot + ' < 1 min'), $false),
    @('3', 'Giraffe_Deck', 'Giraffe Feeding Deck', 'Eye to eye with the tallest animal on Earth.', ('75 m ' + $dot + ' < 1 min'), $true),
    @('4', 'PondBridge', 'Pond & Bridge', 'Cross the wooden bridge over the woodland pond.', ('95 m ' + $dot + ' 2 min'), $false),
    @('5', 'Wombat_Burrow', 'Wombat Burrow', 'The overlook above the wombat burrows.', ('105 m ' + $dot + ' 2 min'), $false)
)
$cy = $pyL + 136
foreach ($c in $cards) {
    $cx = $pxL + 36; $cw = $pw - 72; $chh = 156
    $active = $c[5]
    Panel $g $cx $cy $cw $chh 18 ($(if ($active) { 250 } else { 215 })) $true ($(if ($active) { $cOchre } else { [System.Drawing.Color]::FromArgb(110, 190, 200, 180) }))
    # thumbnail with rounded corners
    $tp = Round-Rect ($cx + 18) ($cy + 18) 176 99 10
    $g.SetClip($tp); Img $g (Join-Path $thumbs ('T_PG_Thumb_' + $c[1] + '.png')) ($cx + 18) ($cy + 18) 176 99; $g.ResetClip()
    # badge + number
    Img $g (Join-Path $ui 'T_PG_Badge.png') ($cx + 212) ($cy + 16) 42 42
    Text $g $c[0] (Font $famSerif 24 ([System.Drawing.FontStyle]::Bold)) $cBgDark ($cx + 212) ($cy + 21) 42 'Center'
    Text $g $c[2] (Font $famSerif 28 ([System.Drawing.FontStyle]::Bold)) $cCream ($cx + 266) ($cy + 14) 360
    $db = New-Object System.Drawing.SolidBrush $cMuted
    $g.DrawString($c[3], (Font $famSans 19), $db, (New-Object System.Drawing.RectangleF ($cx + 212), ($cy + 62), 440, 50))
    $db.Dispose()
    Text $g $c[4] (Font $famSansSemi 20) $cOchre ($cx + 212) ($cy + 116) 300
    if ($active) { Text $g 'CURRENT' (Font $famSansSemi 17) $cOchre ($cx + $cw - 150) ($cy + 20) 130 'Far' }
    $cy += $chh + 10
}
Text $g 'N  /  Esc  -  close' (Font $famSans 20) $cMuted $pxL ($pyL + $ph - 44) $pw 'Center'
Text $g 'Destination list - opened with N (UI-only input, cursor visible until closed)' (Font $famSans 20) ([System.Drawing.Color]::FromArgb(200, 255, 255, 255)) 24 1040 1100
$bmp.Save((Join-Path $docs 'ui_mockup_list.png'), [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
Write-Output "wrote $docs\ui_mockup_hud.png and ui_mockup_list.png"
