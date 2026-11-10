# Renders the zoo signage textures (PNG) used by L_Zoo.
# Usage: powershell -ExecutionPolicy Bypass -File make_signs.ps1
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = Join-Path $here 'out'
New-Item -ItemType Directory -Force $out | Out-Null

# palette
$cBg      = [System.Drawing.Color]::FromArgb(255, 34, 56, 42)     # forest green
$cBgDark  = [System.Drawing.Color]::FromArgb(255, 24, 40, 30)
$cCream   = [System.Drawing.Color]::FromArgb(255, 243, 236, 218)
$cOchre   = [System.Drawing.Color]::FromArgb(255, 205, 146, 60)
$cSand    = [System.Drawing.Color]::FromArgb(255, 226, 205, 160)
$cMuted   = [System.Drawing.Color]::FromArgb(255, 190, 200, 180)

function New-Canvas([int]$w, [int]$h) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.TextRenderingHint = 'AntiAliasGridFit'
    $g.InterpolationMode = 'HighQualityBicubic'
    return @($bmp, $g)
}

function Fill-Panel($g, [int]$w, [int]$h) {
    $rect = New-Object System.Drawing.Rectangle 0, 0, $w, $h
    $lg = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, $cBg, $cBgDark, 90
    $g.FillRectangle($lg, $rect)
    # subtle print grain
    $rnd = New-Object System.Random 7
    for ($i = 0; $i -lt ($w * $h / 900); $i++) {
        $a = $rnd.Next(6, 18)
        $b = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb($a, 255, 255, 255))
        $g.FillRectangle($b, $rnd.Next(0, $w), $rnd.Next(0, $h), 2, 2)
        $b.Dispose()
    }
    # inner border
    $pen = New-Object System.Drawing.Pen $cOchre, ([math]::Max(4, $w / 400))
    $m = [int]($w / 60)
    $g.DrawRectangle($pen, $m, $m, $w - 2 * $m, $h - 2 * $m)
    $pen.Dispose()
}

function Draw-Text($g, [string]$text, [string]$font, [float]$size, $style, $color, [float]$x, [float]$y, [float]$w, [float]$h, [string]$align = 'Near') {
    $f = New-Object System.Drawing.Font $font, $size, $style, ([System.Drawing.GraphicsUnit]::Pixel)
    $b = New-Object System.Drawing.SolidBrush $color
    $sf = New-Object System.Drawing.StringFormat
    $sf.Alignment = $align
    $sf.LineAlignment = 'Near'
    $g.DrawString($text, $f, $b, (New-Object System.Drawing.RectangleF $x, $y, $w, $h), $sf)
    $f.Dispose(); $b.Dispose(); $sf.Dispose()
}

function Draw-Leaf($g, [float]$cx, [float]$cy, [float]$s, $color) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddBezier($cx, $cy + $s, $cx - $s * 0.9, $cy + $s * 0.2, $cx - $s * 0.6, $cy - $s * 0.8, $cx, $cy - $s)
    $path.AddBezier($cx, $cy - $s, $cx + $s * 0.6, $cy - $s * 0.8, $cx + $s * 0.9, $cy + $s * 0.2, $cx, $cy + $s)
    $b = New-Object System.Drawing.SolidBrush $color
    $g.FillPath($b, $path)
    $pen = New-Object System.Drawing.Pen $cBgDark, ($s / 10)
    $g.DrawLine($pen, $cx, $cy + $s * 0.95, $cx, $cy - $s * 0.7)
    $pen.Dispose(); $b.Dispose(); $path.Dispose()
}

function Save($bmp, $g, [string]$name) {
    $p = Join-Path $out $name
    $bmp.Save($p, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Write-Output "wrote $p"
}

$dot = '  ' + [string][char]0x00B7 + '  '
$Bold = [System.Drawing.FontStyle]::Bold
$Reg = [System.Drawing.FontStyle]::Regular
$Ital = [System.Drawing.FontStyle]::Italic

# ------------------------------------------------------------------ species panels (2048 x 1024)
function Species-Panel([string]$file, [string]$title, [string]$latin, [string[]]$facts, [string]$didYouKnow, [string]$status, $statusColor) {
    $w = 2048; $h = 1024
    $bmp, $g = New-Canvas $w $h
    Fill-Panel $g $w $h
    # header band
    $hb = New-Object System.Drawing.SolidBrush $cOchre
    $g.FillRectangle($hb, 34, 34, $w - 68, 16)
    $hb.Dispose()
    Draw-Leaf $g 140 170 70 $cSand
    $ts = 150
    while ($ts -gt 60) {
        $tf = New-Object System.Drawing.Font 'Georgia', $ts, $Bold, ([System.Drawing.GraphicsUnit]::Pixel)
        $tw = $g.MeasureString($title, $tf).Width; $tf.Dispose()
        if ($tw -le 1260) { break }
        $ts -= 6
    }
    Draw-Text $g $title 'Georgia' $ts $Bold $cCream 230 (70 + (150 - $ts) * 0.6) 1500 200
    Draw-Text $g $latin 'Georgia' 62 $Ital $cSand 238 238 1400 90
    # status pill
    $pill = New-Object System.Drawing.SolidBrush $statusColor
    $g.FillRectangle($pill, 1530, 110, 440, 110)
    $pill.Dispose()
    Draw-Text $g 'IUCN STATUS' 'Segoe UI Semibold' 30 $Reg $cBgDark 1530 118 440 40 'Center'
    Draw-Text $g $status 'Segoe UI' 50 $Bold $cBgDark 1530 152 440 70 'Center'
    # divider
    $pen = New-Object System.Drawing.Pen $cMuted, 3
    $g.DrawLine($pen, 90, 350, $w - 90, 350)
    $pen.Dispose()
    # facts (two columns)
    $y = 390
    $col = 0
    foreach ($f in $facts) {
        $parts = $f.Split('|')
        $x = 110 + $col * 960
        Draw-Text $g $parts[0].ToUpper() 'Segoe UI Semibold' 34 $Reg $cOchre $x $y 880 50
        Draw-Text $g $parts[1] 'Segoe UI' 46 $Reg $cCream $x ($y + 46) 880 130
        $col++
        if ($col -gt 1) { $col = 0; $y += 175 }
    }
    # did you know box
    $box = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(60, 255, 255, 255))
    $g.FillRectangle($box, 90, 760, $w - 180, 200)
    $box.Dispose()
    Draw-Text $g 'DID YOU KNOW?' 'Segoe UI Semibold' 36 $Reg $cOchre 130 780 800 50
    Draw-Text $g $didYouKnow 'Georgia' 48 $Reg $cCream 130 830 ($w - 260) 130
    Save $bmp $g $file
}

Species-Panel 'T_Sign_Giraffe.png' 'Giraffe' 'Giraffa camelopardalis' @(
    'Height|Up to 5.5 m - the tallest animal on Earth',
    'Weight|800 - 1,200 kg',
    'Diet|Leaves and shoots, up to 34 kg a day',
    'Habitat|Savannas and open woodland of Africa'
) 'A giraffe''s tongue is about 50 cm long, and its neck has just seven vertebrae - the same number as yours.' 'VULNERABLE' ([System.Drawing.Color]::FromArgb(255, 232, 178, 60))

Species-Panel 'T_Sign_Wombat.png' 'Common Wombat' 'Vombatus ursinus' @(
    'Size|Up to 1.2 m long, 20 - 35 kg',
    'Diet|Grasses, sedges, roots and herbs',
    'Habitat|Forests and heath of south-east Australia',
    'Activity|Mostly at dusk and night; digs long burrows'
) 'A wombat''s pouch opens backwards so it stays clean while digging - and wombats leave cube-shaped droppings!' 'LEAST CONCERN' ([System.Drawing.Color]::FromArgb(255, 150, 200, 120))

# ------------------------------------------------------------------ feeding deck panel (2048 x 1024)
$w = 2048; $h = 1024
$bmp, $g = New-Canvas $w $h
Fill-Panel $g $w $h
Draw-Leaf $g 150 190 80 $cSand
Draw-Text $g 'Giraffe Feeding Deck' 'Georgia' 132 $Bold $cCream 250 90 1700 180
Draw-Text $g 'Eye to eye with the tallest animal on Earth' 'Georgia' 64 $Ital $cSand 258 260 1700 100
$pen = New-Object System.Drawing.Pen $cMuted, 3
$g.DrawLine($pen, 90, 400, $w - 90, 400); $pen.Dispose()
Draw-Text $g ('Please feed only the browse handed out by our keepers.' + [Environment]::NewLine + 'Hold branches flat and let the giraffe come to you.' + [Environment]::NewLine + 'Keep hands and bags inside the railing.') 'Segoe UI' 58 $Reg $cCream 120 450 1820 330
Draw-Text $g 'KEEPER FEEDING TALKS  11:30  &  15:00' 'Segoe UI Semibold' 60 $Reg $cOchre 120 820 1820 100
Save $bmp $g 'T_Sign_FeedingDeck.png'

# ------------------------------------------------------------------ entrance sign (2048 x 512)
$w = 2048; $h = 512
$bmp, $g = New-Canvas $w $h
Fill-Panel $g $w $h
Draw-Leaf $g 190 256 95 $cSand
Draw-Leaf $g ($w - 190) 256 95 $cSand
Draw-Text $g 'TAMAN SAFARI' 'Georgia' 152 $Bold $cCream 0 66 $w 200 'Center'
Draw-Text $g ('P  R  I  G  E  N') 'Georgia' 84 $Bold $cOchre 0 250 $w 110 'Center'
Draw-Text $g ('GIRAFFE' + $dot + 'WOMBAT' + $dot + 'WOODLAND POND') 'Segoe UI Semibold' 44 $Reg $cSand 0 372 $w 70 'Center'
Save $bmp $g 'T_Sign_Entrance.png'

# ------------------------------------------------------------------ direction sign (2048 x 512)
$w = 2048; $h = 512
$bmp, $g = New-Canvas $w $h
Fill-Panel $g $w $h
Draw-Text $g ([string][char]0x2191 + '  Giraffe Paddock & Feeding Deck') 'Segoe UI Semibold' 78 $Reg $cCream 110 70 1900 110
Draw-Text $g ([string][char]0x2196 + ('  Wombat Burrow' + $dot + 'Pond' + $dot + 'Rest Area')) 'Segoe UI Semibold' 78 $Reg $cCream 110 200 1900 110
Draw-Text $g ('Circular trail' + $dot + 'approx. 10 minutes') 'Segoe UI' 56 $Ital $cSand 110 340 1900 90
Save $bmp $g 'T_Sign_Directions.png'

# ------------------------------------------------------------------ visitor map board (2048 x 1536)
$mapSrc = Join-Path $here '..\ZooTerrain\out\zoo_map_clean.png'
if (Test-Path $mapSrc) {
    $w = 2048; $h = 1536
    $bmp, $g = New-Canvas $w $h
    Fill-Panel $g $w $h
    Draw-Text $g 'Zoo Trail Map' 'Georgia' 120 $Bold $cCream 110 60 1400 160
    Draw-Text $g 'Follow the circular trail - it starts and ends at this entrance.' 'Segoe UI' 50 $Ital $cSand 118 210 1800 80
    $map = [System.Drawing.Image]::FromFile((Resolve-Path $mapSrc))
    # map covers X -62..50 (rows, north up) and Y -64..64 (cols); 6 px per metre in source
    $mx = 110; $my = 320; $mw = 1300; $mh = [int](1300 * $map.Height / $map.Width)
    $ia = New-Object System.Drawing.Imaging.ImageAttributes
    $cm = New-Object System.Drawing.Imaging.ColorMatrix
    $cm.Matrix00 = 0.85; $cm.Matrix11 = 0.85; $cm.Matrix22 = 0.8; $cm.Matrix40 = 0.06; $cm.Matrix41 = 0.05; $cm.Matrix42 = 0.02
    $ia.SetColorMatrix($cm)
    $g.DrawImage($map, (New-Object System.Drawing.Rectangle $mx, $my, $mw, $mh), 0, 0, $map.Width, $map.Height, [System.Drawing.GraphicsUnit]::Pixel, $ia)
    $pen = New-Object System.Drawing.Pen $cCream, 6
    $g.DrawRectangle($pen, $mx, $my, $mw, $mh); $pen.Dispose()
    $sx = $mw / 128.0; $sy = $mh / 112.0
    function MapPt([double]$X, [double]$Y) { return @(($mx + ($Y + 64) * $sx), ($my + (50 - $X) * $sy)) }
    function Label([string]$t, [double]$X, [double]$Y, [int]$n) {
        $p = MapPt $X $Y
        $b = New-Object System.Drawing.SolidBrush $cOchre
        $g.FillEllipse($b, $p[0] - 30, $p[1] - 30, 60, 60); $b.Dispose()
        Draw-Text $g ([string]$n) 'Segoe UI' 40 $Bold $cBgDark ($p[0] - 30) ($p[1] - 26) 60 60 'Center'
    }
    Label 'Entrance' -38 0 1
    Label 'Giraffe Paddock' -12 30 2
    Label 'Feeding Deck' 9.5 13.5 3
    Label 'Pond & Bridge' 23 -12 4
    Label 'Wombat Burrow' 29 -44 5
    Label 'Rest Area' -3 -31 6
    # you are here
    $p = MapPt -44 0
    $rb = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 210, 60, 50))
    $g.FillEllipse($rb, $p[0] - 22, $p[1] - 22, 44, 44); $rb.Dispose()
    Draw-Text $g 'YOU ARE HERE' 'Segoe UI' 34 $Bold $cCream ($p[0] + 30) ($p[1] - 22) 400 50
    # legend
    $lx = 1480; $ly = 360
    $items = @('Entrance', 'Giraffe Paddock', 'Feeding Deck', 'Pond & Bridge', 'Wombat Burrow', 'Shaded Rest Area')
    for ($i = 0; $i -lt $items.Count; $i++) {
        $b = New-Object System.Drawing.SolidBrush $cOchre
        $g.FillEllipse($b, $lx, $ly + $i * 130, 70, 70); $b.Dispose()
        Draw-Text $g ([string]($i + 1)) 'Segoe UI' 46 $Bold $cBgDark $lx ($ly + $i * 130 + 2) 70 70 'Center'
        Draw-Text $g $items[$i] 'Segoe UI Semibold' 48 $Reg $cCream ($lx + 95) ($ly + $i * 130 + 4) 480 70
    }
    Draw-Text $g ('Please stay on the paths.' + [Environment]::NewLine + 'Do not climb barriers.') 'Segoe UI' 42 $Ital $cSand $lx 1200 520 140
    $map.Dispose()
    Save $bmp $g 'T_Sign_Map.png'
}
