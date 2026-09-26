Add-Type -AssemblyName System.Drawing
$image = [Drawing.Bitmap]::new(1200,630)
$g = [Drawing.Graphics]::FromImage($image)
$g.SmoothingMode = 'AntiAlias'
$g.TextRenderingHint = 'AntiAliasGridFit'
$g.Clear([Drawing.ColorTranslator]::FromHtml('#f7f6f2'))
function Box($x,$y,$w,$h,$radius,$color) {
 $path=[Drawing.Drawing2D.GraphicsPath]::new();$d=$radius*2
 $path.AddArc($x,$y,$d,$d,180,90);$path.AddArc($x+$w-$d,$y,$d,$d,270,90)
 $path.AddArc($x+$w-$d,$y+$h-$d,$d,$d,0,90);$path.AddArc($x,$y+$h-$d,$d,$d,90,90);$path.CloseFigure()
 $brush=[Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($color));$g.FillPath($brush,$path);$brush.Dispose();$path.Dispose()
}
function Label($x,$y,$text,$size,$color) {
 $font=[Drawing.Font]::new('Segoe UI',$size,[Drawing.FontStyle]::Regular,[Drawing.GraphicsUnit]::Pixel)
 $brush=[Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($color));$g.DrawString($text,$font,$brush,$x,$y);$font.Dispose();$brush.Dispose()
}
Box 760 50 400 530 40 '#e0e5d4'
Label 65 60 'FLOATLET' 23 '#30392c'
Label 65 151 'A little island.' 65 '#242820'
Label 65 237 'A calmer desktop.' 60 '#6d775a'
Label 68 369 'Music. Files. Clocks. Your day.' 23 '#62675c'
Label 68 413 'Free & open source for Windows 10 and 11' 20 '#62675c'
Box 790 237 340 117 56 '#090b0d'
Box 815 263 65 65 17 '#81936d'
Label 828 264 '♫' 37 '#f3f0df'
Label 894 266 'Room to breathe' 16 '#f7f6f2'
Label 894 292 'Floatlet Sessions' 12 '#a1a7a1'
$heights=@(8,16,24,14,20,10)
for($i=0;$i -lt 6;$i++){Box (990+$i*6) (321-$heights[$i]/2) 3 $heights[$i] 1.5 '#aca0d1'}
Label 68 535 'floatlet.vercel.app' 18 '#62675c'
$image.Save((Join-Path $PSScriptRoot '../public/social-card.png'),[Drawing.Imaging.ImageFormat]::Png)
$g.Dispose();$image.Dispose()
