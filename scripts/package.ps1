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
$dest=Join-Path $root "packages\Floatlet-0.4.0-$Architecture"
New-Item -ItemType Directory -Path $dest -Force | Out-Null
$inputs=@($exe,(Join-Path $root 'README.md'),(Join-Path $root 'LICENSE'),(Join-Path $root 'CHANGELOG.md'),(Join-Path $PSScriptRoot 'install.ps1'),(Join-Path $PSScriptRoot 'uninstall.ps1'))
$archive=@()
foreach ($inputFile in $inputs) {
    $outputFile=Join-Path $dest ([IO.Path]::GetFileName($inputFile))
    Copy-Item -LiteralPath $inputFile -Destination $outputFile -Force
    $archive+=$outputFile
}
# Never include local diagnostics, screenshots, settings or credentials.
Compress-Archive -LiteralPath $archive -DestinationPath "$dest.zip" -Force
Get-FileHash -LiteralPath "$dest.zip" -Algorithm SHA256
