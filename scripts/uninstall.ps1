$ErrorActionPreference='Stop'
$install=[IO.Path]::GetFullPath($PSScriptRoot)
$allowed=@([IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'Programs\GlideIsland')),[IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'Programs\DelightIsland')))
if ($install -notin $allowed -or !(Test-Path -LiteralPath (Join-Path $install 'installed.marker'))) { throw 'Run uninstall.ps1 from the owned installation directory.' }
$target=Join-Path $install 'GlideIsland.exe'
if (Get-Process GlideIsland -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $target }) { throw 'Quit Glide Island from its tray menu first.' }
$run='HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
foreach ($key in @('DelightIsland','GlideIsland')) {
    $entry=(Get-ItemProperty -Path $run -ErrorAction SilentlyContinue).$key
    if ($entry -eq ('"'+$target+'"')) { Remove-ItemProperty -Path $run -Name $key }
}
$link=Join-Path ([Environment]::GetFolderPath('Programs')) 'Glide Island.lnk'
if (Test-Path -LiteralPath $link) {
    $shell=New-Object -ComObject WScript.Shell
    if ($shell.CreateShortcut($link).TargetPath -eq $target) { Remove-Item -LiteralPath $link }
}
foreach ($name in @('GlideIsland.exe','installed.marker','uninstall.ps1')) {
    $path=Join-Path $install $name
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
}
Write-Host 'Glide Island removed. Settings, encrypted calendar subscription and tray references were retained. Original files were not touched.'
