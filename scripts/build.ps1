param([ValidateSet('arm64','x64')][string]$Architecture='arm64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $vswhere)) { throw 'Install Visual Studio 2022 C++ Build Tools and Windows SDK 10.0.26100. No tools will be downloaded automatically.' }
$vs=& $vswhere -latest -products '*' -property installationPath
$cmake=Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (!(Test-Path $cmake)) { throw 'The Visual Studio CMake component is missing.' }
$compiler=Get-ChildItem "$vs\VC\Tools\MSVC\*\bin\Host*\$Architecture\cl.exe" -ErrorAction SilentlyContinue
if (!$compiler) { throw "MSVC $Architecture compiler is missing. Add the matching C++ build tools component in Visual Studio Installer." }
# Normalize duplicate Path/PATH entries inherited by some developer hosts.
$buildPath=$env:PATH
Remove-Item Env:PATH
$env:Path=$buildPath
Push-Location $root
try {
    & $cmake --preset $Architecture
    if ($LASTEXITCODE) { throw 'CMake configure failed' }
    & $cmake --build --preset $Architecture --parallel 2
    if ($LASTEXITCODE) { throw 'Build failed' }
    & (Join-Path (Split-Path $cmake) 'ctest.exe') --preset $Architecture
    if ($LASTEXITCODE) { throw 'Tests failed' }
} finally { Pop-Location }
