param([ValidateSet('x64','arm64')][string]$Architecture='x64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
# A neutral build root prevents C++ source-location diagnostics exposing user paths.
$stage=Join-Path $env:ProgramData ('FloatletRelease\0.8.2-'+$Architecture)
if (Test-Path -LiteralPath $stage) { throw "Release staging directory already exists: $stage. Review it before reusing." }
New-Item -ItemType Directory -Path $stage -Force | Out-Null
foreach ($name in @('src','tests','assets','scripts','CMakeLists.txt','CMakePresets.json','README.md','LICENSE','CHANGELOG.md')) {
    Copy-Item -LiteralPath (Join-Path $root $name) -Destination $stage -Recurse
}
& (Join-Path $stage 'scripts\build.ps1') -Architecture $Architecture
& (Join-Path $stage 'scripts\package.ps1') -Architecture $Architecture
New-Item -ItemType Directory -Path (Join-Path $root 'packages') -Force | Out-Null
Copy-Item -Path (Join-Path $stage 'packages\*') -Destination (Join-Path $root 'packages') -Recurse -Force
Write-Host 'Release built, tested and checked for private build paths.'