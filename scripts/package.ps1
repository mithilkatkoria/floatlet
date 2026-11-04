param([ValidateSet('arm64','x64')][string]$Architecture='arm64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $root "out\$Architecture\Release\Floatlet.exe"
if (!(Test-Path -LiteralPath $exe)) { throw 'Build first.' }
$bytes=[IO.File]::ReadAllBytes($exe)
$offset=[BitConverter]::ToInt32($bytes,0x3c)
$machine=[BitConverter]::ToUInt16($bytes,$offset+4)
$expected=if ($Architecture -eq 'arm64') {0xAA64} else {0x8664}
if ($machine -ne $expected) { throw 'Binary architecture does not match package label.' }
# Reject personal compiler paths before publishing a binary.
$ascii=[Text.Encoding]::ASCII.GetString($bytes)
$wide=[Text.Encoding]::Unicode.GetString($bytes)
foreach ($content in @($ascii,$wide)) {
    if ($content -match '(?i)[A-Z]:[\\/]Users[\\/]' -or $content -match '(?i)OneDrive[ -]') { throw 'Release privacy check failed: personal build path in executable. Use scripts/release.ps1.' }
}
$dest=Join-Path $root "packages\Floatlet-0.8.2-$Architecture"
New-Item -ItemType Directory -Path $dest -Force | Out-Null
$inputs=@($exe,(Join-Path $root 'README.md'),(Join-Path $root 'LICENSE'),(Join-Path $root 'CHANGELOG.md'),(Join-Path $PSScriptRoot 'install.ps1'),(Join-Path $PSScriptRoot 'uninstall.ps1'),(Join-Path $root 'assets/fonts/Figtree-OFL.txt'),(Join-Path $root 'assets/fonts/EBGaramond-OFL.txt'))
$archive=@()
foreach ($inputFile in $inputs) {
    $outputFile=Join-Path $dest ([IO.Path]::GetFileName($inputFile))
    Copy-Item -LiteralPath $inputFile -Destination $outputFile -Force
    $archive+=$outputFile
}
# Never include local diagnostics, screenshots, settings or credentials.
Compress-Archive -LiteralPath $archive -DestinationPath "$dest.zip" -Force
Get-FileHash -LiteralPath "$dest.zip" -Algorithm SHA256
