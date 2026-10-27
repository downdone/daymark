# Rebuild Daymark's original vector and platform icon assets using Windows drawing APIs.
# No downloaded assets or additional package dependencies.
#requires -Version 7.0
$ErrorActionPreference='Stop'
if(!$IsWindows){throw 'Icon asset generation requires Windows; generated assets build on either platform.'}
Add-Type -AssemblyName System.Drawing.Common
$destination=Join-Path (Split-Path $PSScriptRoot -Parent) 'assets/icons'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$svg=@'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">
  <title>Daymark</title>
  <rect x="8" y="8" width="240" height="240" rx="56" fill="#0063cf"/>
  <rect x="52" y="61" width="152" height="151" rx="24" fill="#fff"/>
  <path d="M52 102H204" stroke="#0063cf" stroke-width="10"/>
  <path d="M88 48V74M168 48V74" fill="none" stroke="#fff" stroke-width="16" stroke-linecap="round"/>
  <path d="M88 153L115 180L171 127" fill="none" stroke="#0063cf" stroke-width="17" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
'@
[IO.File]::WriteAllText((Join-Path $destination 'daymark.svg'),$svg+"`n")
function New-RoundedPath([single]$x,[single]$y,[single]$w,[single]$h,[single]$r) {
    $path=[Drawing.Drawing2D.GraphicsPath]::new();$diameter=2*$r
    $path.AddArc($x,$y,$diameter,$diameter,180,90)
    $path.AddArc($x+$w-$diameter,$y,$diameter,$diameter,270,90)
    $path.AddArc($x+$w-$diameter,$y+$h-$diameter,$diameter,$diameter,0,90)
    $path.AddArc($x,$y+$h-$diameter,$diameter,$diameter,90,90);$path.CloseFigure()
    return ,$path
}
$master=[Drawing.Bitmap]::new(1024,1024)
$graphics=[Drawing.Graphics]::FromImage($master)
$blue=[Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#0063cf'))
$white=[Drawing.SolidBrush]::new([Drawing.Color]::White)
try {
    $graphics.SmoothingMode=[Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.ScaleTransform(4,4)
    $background=New-RoundedPath 8 8 240 240 56
    $paper=New-RoundedPath 52 61 152 151 24
    try {$graphics.FillPath($blue,$background);$graphics.FillPath($white,$paper)}finally{$background.Dispose();$paper.Dispose()}
    $header=[Drawing.Pen]::new($blue,10)
    try {$graphics.DrawLine($header,52,102,204,102)}finally{$header.Dispose()}
    $rings=[Drawing.Pen]::new($white,16);$rings.StartCap='Round';$rings.EndCap='Round'
    try {$graphics.DrawLine($rings,88,48,88,74);$graphics.DrawLine($rings,168,48,168,74)}finally{$rings.Dispose()}
    $tick=[Drawing.Pen]::new($blue,17);$tick.StartCap='Round';$tick.EndCap='Round';$tick.LineJoin='Round'
    try {$graphics.DrawLines($tick,[Drawing.PointF[]]@([Drawing.PointF]::new(88,153),[Drawing.PointF]::new(115,180),[Drawing.PointF]::new(171,127)))}finally{$tick.Dispose()}
    $frames=@{}
    foreach($size in @(16,20,24,32,48,64,128,256,512,1024)) {
        $bitmap=[Drawing.Bitmap]::new($size,$size)
        $scaled=[Drawing.Graphics]::FromImage($bitmap)
        $stream=[IO.MemoryStream]::new()
        try {
            $scaled.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $scaled.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $scaled.DrawImage($master,0,0,$size,$size)
            $bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png)
            $frames[$size]=$stream.ToArray()
            if($size -in @(16,32,48,256,1024)){[IO.File]::WriteAllBytes((Join-Path $destination "daymark-$size.png"),$frames[$size])}
        } finally {$stream.Dispose();$scaled.Dispose();$bitmap.Dispose()}
    }
    $ico=[IO.MemoryStream]::new();$writer=[IO.BinaryWriter]::new($ico)
    try {
        $sizes=@(16,20,24,32,48,64,128,256)
        $writer.Write([uint16]0);$writer.Write([uint16]1);$writer.Write([uint16]$sizes.Count)
        $offset=6+16*$sizes.Count
        foreach($size in $sizes) {
            $edge=if($size -eq 256){0}else{$size}
            $writer.Write([byte]$edge);$writer.Write([byte]$edge);$writer.Write([uint16]0)
            $writer.Write([uint16]1);$writer.Write([uint16]32)
            $writer.Write([uint32]$frames[$size].Length);$writer.Write([uint32]$offset)
            $offset+=$frames[$size].Length
        }
        foreach($size in $sizes){$writer.Write([byte[]]$frames[$size])}
        [IO.File]::WriteAllBytes((Join-Path $destination 'daymark.ico'),$ico.ToArray())
    }finally{$writer.Dispose();$ico.Dispose()}
    # ICNS chunks use big endian lengths and PNG payloads.
    $icns=[IO.MemoryStream]::new();$writer=[IO.BinaryWriter]::new($icns)
    function Write-BigEndian([IO.BinaryWriter]$output,[int]$number) {
        $bytes=[BitConverter]::GetBytes($number);if([BitConverter]::IsLittleEndian){[Array]::Reverse($bytes)};$output.Write($bytes)
    }
    try {
        $chunks=@(@('icp4',16),@('icp5',32),@('icp6',64),@('ic07',128),@('ic08',256),@('ic09',512),@('ic10',1024))
        $total=8;foreach($chunk in $chunks){$total+=8+$frames[$chunk[1]].Length}
        $writer.Write([Text.Encoding]::ASCII.GetBytes('icns'));Write-BigEndian $writer $total
        foreach($chunk in $chunks){$writer.Write([Text.Encoding]::ASCII.GetBytes($chunk[0]));Write-BigEndian $writer (8+$frames[$chunk[1]].Length);$writer.Write([byte[]]$frames[$chunk[1]])}
        [IO.File]::WriteAllBytes((Join-Path $destination 'daymark.icns'),$icns.ToArray())
    }finally{$writer.Dispose();$icns.Dispose()}
}finally{$graphics.Dispose();$master.Dispose();$blue.Dispose();$white.Dispose()}
Write-Output 'Generated original Daymark SVG, PNG, ICO and ICNS assets.'
