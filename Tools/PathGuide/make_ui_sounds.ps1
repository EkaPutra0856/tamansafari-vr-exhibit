# Synthesises the Path Guide UI cues as 16-bit mono 44.1 kHz WAV files. PLACEHOLDERS until the client supplies final audio.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File make_ui_sounds.ps1
# Output: Tools/PathGuide/out/sounds/S_PG_*.wav  (imported into /Game/PathGuide/UI/Sounds by the import step)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = Join-Path $here 'out\sounds'
New-Item -ItemType Directory -Force $out | Out-Null
$rate = 44100

# One soft bell partial: sine + a quiet octave, exponential decay, short attack.
function Add-Note([double[]]$buf, [double]$start, [double]$freq, [double]$dur, [double]$gain, [double]$decay, [double]$endFreq = 0) {
    $n0 = [int]($start * $rate)
    $n = [int]($dur * $rate)
    $phase = 0.0
    for ($i = 0; $i -lt $n; $i++) {
        $idx = $n0 + $i
        if ($idx -ge $buf.Length) { break }
        $t = $i / $rate
        $f = if ($endFreq -gt 0) { $freq + ($endFreq - $freq) * ($i / $n) } else { $freq }
        $phase += 2 * [Math]::PI * $f / $rate
        $attack = [Math]::Min(1.0, $t / 0.006)
        $env = $attack * [Math]::Exp(-$decay * $t)
        $buf[$idx] += $gain * $env * ([Math]::Sin($phase) + 0.25 * [Math]::Sin(2 * $phase))
    }
}

function Write-Wav([string]$name, [double[]]$buf) {
    $peak = 0.0
    foreach ($s in $buf) { if ([Math]::Abs($s) -gt $peak) { $peak = [Math]::Abs($s) } }
    $scale = if ($peak -gt 0) { 0.30 / $peak } else { 1.0 }   # about -10 dBFS peak: soft UI level
    $fade = [int](0.02 * $rate)
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter $ms
    $dataLen = $buf.Length * 2
    $bw.Write([Text.Encoding]::ASCII.GetBytes('RIFF')); $bw.Write([int](36 + $dataLen))
    $bw.Write([Text.Encoding]::ASCII.GetBytes('WAVE')); $bw.Write([Text.Encoding]::ASCII.GetBytes('fmt '))
    $bw.Write([int]16); $bw.Write([int16]1); $bw.Write([int16]1); $bw.Write([int]$rate)
    $bw.Write([int]($rate * 2)); $bw.Write([int16]2); $bw.Write([int16]16)
    $bw.Write([Text.Encoding]::ASCII.GetBytes('data')); $bw.Write([int]$dataLen)
    for ($i = 0; $i -lt $buf.Length; $i++) {
        $tail = [Math]::Min(1.0, ($buf.Length - $i) / $fade)
        $v = [Math]::Max(-1.0, [Math]::Min(1.0, $buf[$i] * $scale * $tail))
        $bw.Write([int16]($v * 32767))
    }
    $bw.Flush()
    $path = Join-Path $out ($name + '.wav')
    [IO.File]::WriteAllBytes($path, $ms.ToArray())
    $bw.Dispose(); $ms.Dispose()
    Write-Output "wrote $path ($([Math]::Round($buf.Length / $rate, 2)) s)"
}

function New-Buf([double]$seconds) { return New-Object 'double[]' ([int]($seconds * $rate)) }

# open: two rising notes
$b = New-Buf 0.55
Add-Note $b 0.00 659.25 0.30 1.0 9
Add-Note $b 0.11 880.00 0.40 0.9 8
Write-Wav 'S_PG_Open' $b

# close: two falling notes
$b = New-Buf 0.45
Add-Note $b 0.00 880.00 0.25 0.9 11
Add-Note $b 0.09 659.25 0.32 0.9 10
Write-Wav 'S_PG_Close' $b

# select: light pluck
$b = New-Buf 0.28
Add-Note $b 0.00 1174.66 0.22 1.0 22
Add-Note $b 0.00 1760.00 0.10 0.35 40
Write-Wav 'S_PG_Select' $b

# arrive: gentle three-note chime (C5 E5 G5) with a soft tail
$b = New-Buf 1.6
Add-Note $b 0.00 523.25 1.0 0.9 4.2
Add-Note $b 0.16 659.25 1.0 0.85 4.2
Add-Note $b 0.32 783.99 1.2 0.85 3.8
Add-Note $b 0.32 1567.98 0.6 0.15 6
Write-Wav 'S_PG_Arrive' $b

# stop: low, short glide down
$b = New-Buf 0.4
Add-Note $b 0.00 330.00 0.32 1.0 9 220
Write-Wav 'S_PG_Stop' $b
