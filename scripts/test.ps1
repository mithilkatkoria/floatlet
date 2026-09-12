param([ValidateSet('arm64','x64')][string]$Architecture='arm64')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
& "$root\out\$Architecture\Release\model_tests.exe"
if ($LASTEXITCODE) { throw 'Model tests failed' }
& "$root\out\$Architecture\Release\storage_tests.exe"
if ($LASTEXITCODE) { throw 'Storage tests failed' }
& "$root\out\$Architecture\Release\integration_tests.exe"
if ($LASTEXITCODE) { throw 'Integration tests failed' }
& "$root\out\$Architecture\Release\calendar_tests.exe"
if ($LASTEXITCODE) { throw 'Calendar tests failed' }
& "$root\out\$Architecture\Release\time_tools_tests.exe"
if ($LASTEXITCODE) { throw 'Clock tests failed' }
