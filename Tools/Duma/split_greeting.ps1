# Splits the Duma greeting voice into one wave per dialogue line.
# Each transcript timestamp is moved to the quietest 10 ms hop within +-0.5 s (so a cut lands in a pause, not mid-word).
# ponytail: silence = quietest hop near the timestamp; swap for a real VAD if a cut ever lands mid-word.
param(
	[string]$Source = 'C:\Users\handi\Downloads\Duma\Duma\Duma_Greeting.wav',
	[string]$OutDir = (Join-Path $PSScriptRoot 'out\voices'),
	[double[]]$Timestamps = @(0, 1, 5, 7, 9, 14, 16, 18, 20),
	[double]$Window = 0.5,
	[string]$Prefix = 'SW_Duma_Greeting'
)

$ErrorActionPreference = 'Stop'
$bytes = [IO.File]::ReadAllBytes($Source)

# ---------------------------------------------------------------------------------------------- Read the WAV (walk the RIFF chunks)
if ([Text.Encoding]::ASCII.GetString($bytes, 0, 4) -ne 'RIFF' -or [Text.Encoding]::ASCII.GetString($bytes, 8, 4) -ne 'WAVE') { throw 'Not a RIFF/WAVE file' }
$pos = 12
$fmtOk = $false
$dataOffset = -1
$dataLength = 0
while ($pos + 8 -le $bytes.Length)
{
	$id = [Text.Encoding]::ASCII.GetString($bytes, $pos, 4)
	$size = [BitConverter]::ToUInt32($bytes, $pos + 4)
	if ($id -eq 'fmt ')
	{
		$fmt = [BitConverter]::ToUInt16($bytes, $pos + 8)
		$ch = [BitConverter]::ToUInt16($bytes, $pos + 10)
		$rate = [BitConverter]::ToUInt32($bytes, $pos + 12)
		$bits = [BitConverter]::ToUInt16($bytes, $pos + 22)
		if ($fmt -ne 1 -or $ch -ne 1 -or $bits -ne 16 -or $rate -ne 48000) { throw "Expected PCM mono 16-bit 48000 Hz, got fmt=$fmt ch=$ch bits=$bits rate=$rate" }
		$fmtOk = $true
	}
	elseif ($id -eq 'data')
	{
		$dataOffset = $pos + 8
		$dataLength = [Math]::Min([int]$size, $bytes.Length - $dataOffset)
		break
	}
	$pos += 8 + $size + ($size % 2)
}
if (-not $fmtOk -or $dataOffset -lt 0) { throw 'fmt or data chunk missing' }

$rate = 48000
$count = [int]($dataLength / 2)
$samples = New-Object 'int16[]' $count
[Buffer]::BlockCopy($bytes, $dataOffset, $samples, 0, $count * 2)

# ---------------------------------------------------------------------------------------------- Energy: RMS over 20 ms windows, hop 10 ms
$win = [int]($rate * 0.020)
$hop = [int]($rate * 0.010)
$hops = [int][Math]::Floor(($count - $win) / $hop) + 1
$rms = New-Object 'double[]' $hops
for ($h = 0; $h -lt $hops; $h++)
{
	$sum = 0.0
	$start = $h * $hop
	for ($i = 0; $i -lt $win; $i++) { $v = [double]$samples[$start + $i]; $sum += $v * $v }
	$rms[$h] = [Math]::Sqrt($sum / $win)
}

# ---------------------------------------------------------------------------------------------- Cut points (sample indices); the cut sits in the middle of the quietest window
$cuts = New-Object 'int[]' ($Timestamps.Count + 1)
$cuts[0] = 0
for ($n = 1; $n -lt $Timestamps.Count; $n++)
{
	$t = $Timestamps[$n]
	$lo = [Math]::Max(0, [int][Math]::Round(($t - $Window) / 0.010))
	$hi = [Math]::Min($hops - 1, [int][Math]::Round(($t + $Window) / 0.010))
	$best = $lo
	$target = [int][Math]::Round($t / 0.010)
	for ($h = $lo; $h -le $hi; $h++)
	{
		if ($rms[$h] -lt $rms[$best] -or ($rms[$h] -eq $rms[$best] -and [Math]::Abs($h - $target) -lt [Math]::Abs($best - $target))) { $best = $h }
	}
	$cuts[$n] = $best * $hop + [int]($win / 2)
}
$cuts[$Timestamps.Count] = $count

# ---------------------------------------------------------------------------------------------- Write the parts (5 ms fades) + cuts.json
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$fade = [int]($rate * 0.005)
$report = @()
for ($n = 0; $n -lt $Timestamps.Count; $n++)
{
	$from = $cuts[$n]
	$len = $cuts[$n + 1] - $from
	$part = New-Object 'int16[]' $len
	for ($i = 0; $i -lt $len; $i++)
	{
		$gain = 1.0
		if ($i -lt $fade) { $gain = $i / $fade }
		if (($len - 1 - $i) -lt $fade) { $gain = [Math]::Min($gain, ($len - 1 - $i) / $fade) }
		$part[$i] = [int16][Math]::Round($samples[$from + $i] * $gain)
	}

	$file = '{0}_{1:00}.wav' -f $Prefix, ($n + 1)
	$ms = New-Object IO.MemoryStream
	$bw = New-Object IO.BinaryWriter($ms)
	$bw.Write([Text.Encoding]::ASCII.GetBytes('RIFF')); $bw.Write([int](36 + $len * 2)); $bw.Write([Text.Encoding]::ASCII.GetBytes('WAVE'))
	$bw.Write([Text.Encoding]::ASCII.GetBytes('fmt ')); $bw.Write([int]16); $bw.Write([int16]1); $bw.Write([int16]1); $bw.Write([int]$rate); $bw.Write([int]($rate * 2)); $bw.Write([int16]2); $bw.Write([int16]16)
	$bw.Write([Text.Encoding]::ASCII.GetBytes('data')); $bw.Write([int]($len * 2))
	$raw = New-Object 'byte[]' ($len * 2)
	[Buffer]::BlockCopy($part, 0, $raw, 0, $len * 2)
	$bw.Write($raw)
	$bw.Flush()
	[IO.File]::WriteAllBytes((Join-Path $OutDir $file), $ms.ToArray())

	$report += [pscustomobject]@{ line = $n + 1; start = [Math]::Round($from / $rate, 3); duration = [Math]::Round($len / $rate, 3); file = $file; asked = $Timestamps[$n] }
}

$report | ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $OutDir 'cuts.json')
$report | Format-Table -AutoSize
'Total duration: {0:N3} s' -f ($count / $rate)
