param([ValidateSet('arm64','x64')][string]$Architecture='arm64')
$ErrorActionPreference='Stop'
$exe=Join-Path (Split-Path $PSScriptRoot -Parent) "out\$Architecture\Release\Floatlet.exe"
if (!(Test-Path $exe)) { throw "Build $Architecture first using scripts/build.ps1." }
if ($Architecture -eq 'x64' -and $env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { Write-Warning 'This x64 development build runs under emulation. It is not the native Snapdragon build.' }
Start-Process -FilePath $exe -WindowStyle Hidden
