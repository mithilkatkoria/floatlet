param([string]$Source=$PSScriptRoot,[switch]$NoStartup,[switch]$AcceptDefaults,[switch]$Launch)
$ErrorActionPreference='Stop'
$sourceExe=Join-Path $Source 'GlideIsland.exe'
if (!(Test-Path -LiteralPath $sourceExe)) { throw 'Supply a package directory containing GlideIsland.exe.' }
$legacy=Join-Path $env:LOCALAPPDATA 'Programs\DelightIsland'
$install=if (Test-Path -LiteralPath (Join-Path $legacy 'installed.marker')) { $legacy } else { Join-Path $env:LOCALAPPDATA 'Programs\GlideIsland' }
$existing=Test-Path -LiteralPath (Join-Path $install 'installed.marker')
$target=Join-Path $install 'GlideIsland.exe'
$oldTarget=Join-Path $install 'DelightIsland.exe'
foreach ($process in @(Get-Process GlideIsland,DelightIsland -ErrorAction SilentlyContinue)) {
    if (!$existing -or $process.Path -notin @($target,$oldTarget)) { throw 'Close the other island instance before installing.' }
    Start-Process -FilePath $process.Path -ArgumentList '--quit' -WindowStyle Hidden -Wait
    Wait-Process -Id $process.Id -Timeout 15 -ErrorAction SilentlyContinue
    if (Get-Process -Id $process.Id -ErrorAction SilentlyContinue) { throw 'The running island did not close. Quit from its tray menu and retry.' }
}
if (!$existing -and !$NoStartup -and !$AcceptDefaults) {
    $choice=Read-Host 'Start Glide Island with Windows? [Y/n]'
    if ($choice -match '^[Nn]') { $NoStartup=$true }
}
New-Item -ItemType Directory -Path $install -Force | Out-Null
# Windows may briefly retain the image mapping after the process exits.
for ($attempt=0; $attempt -lt 12; $attempt++) {
    try { Copy-Item -LiteralPath $sourceExe -Destination $target -Force; break }
    catch [System.IO.IOException] {
        if ($attempt -eq 11) { throw }
        Start-Sleep -Milliseconds 250
    }
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'uninstall.ps1') -Destination $install -Force
Set-Content -LiteralPath (Join-Path $install 'installed.marker') -Value 'GlideIsland 0.3.0'
$shell=New-Object -ComObject WScript.Shell
$shortcut=$shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Programs')) 'Glide Island.lnk'))
$shortcut.TargetPath=$target
$shortcut.WorkingDirectory=$install
$shortcut.IconLocation=$target+',0'
$shortcut.Description='Native Windows island for music, a file tray, timers and calendar'
$shortcut.Save()
foreach ($oldName in @('Delight Island.lnk','Delay Island.lnk','Luma Island.lnk')) {
    $oldLink=Join-Path ([Environment]::GetFolderPath('Programs')) $oldName
    if ((Test-Path -LiteralPath $oldLink) -and $shell.CreateShortcut($oldLink).TargetPath -in @($oldTarget,$target)) { Remove-Item -LiteralPath $oldLink }
}
$run='HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
if ($existing) {
    # Preserve the registry value name and Windows StartupApproved preferences.
    foreach ($key in @('DelightIsland','GlideIsland')) {
        $value=(Get-ItemProperty -Path $run -ErrorAction SilentlyContinue).$key
        if ($value -eq ('"'+$oldTarget+'"')) { Set-ItemProperty -Path $run -Name $key -Value ('"'+$target+'"') }
    }
} elseif (!$NoStartup) {
    New-Item -Path $run -Force | Out-Null
    New-ItemProperty -Path $run -Name GlideIsland -Value ('"'+$target+'"') -PropertyType String -Force | Out-Null
}
if ($existing -and (Test-Path -LiteralPath $oldTarget)) { Remove-Item -LiteralPath $oldTarget }
Write-Host "Installed Glide Island at $target. Existing settings and tray references were preserved."
if ($Launch) { Start-Process -FilePath $target -WorkingDirectory $install -WindowStyle Hidden }
