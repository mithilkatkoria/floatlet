param([string]$Source=$PSScriptRoot,[switch]$NoStartup,[switch]$AcceptDefaults,[switch]$Launch)
$ErrorActionPreference='Stop'
$sourceExe=Join-Path $Source 'Floatlet.exe'
if (!(Test-Path -LiteralPath $sourceExe)) { throw 'Supply a package directory containing Floatlet.exe.' }
$legacy=Join-Path $env:LOCALAPPDATA 'Programs\DelightIsland'
$previous=Join-Path $env:LOCALAPPDATA 'Programs\GlideIsland'
$install=if (Test-Path -LiteralPath (Join-Path $legacy 'installed.marker')) { $legacy } elseif (Test-Path -LiteralPath (Join-Path $previous 'installed.marker')) { $previous } else { Join-Path $env:LOCALAPPDATA 'Programs\Floatlet' }
$existing=Test-Path -LiteralPath (Join-Path $install 'installed.marker')
$target=Join-Path $install 'Floatlet.exe'
$oldTargets=@((Join-Path $install 'DelightIsland.exe'),(Join-Path $install 'GlideIsland.exe'))
foreach ($process in @(Get-Process Floatlet,GlideIsland,DelightIsland -ErrorAction SilentlyContinue)) {
    if (!$existing -or $process.Path -notin (@($target)+$oldTargets)) { throw 'Close the other island instance before installing.' }
    Start-Process -FilePath $process.Path -ArgumentList '--quit' -WindowStyle Hidden -Wait
    Wait-Process -Id $process.Id -Timeout 15 -ErrorAction SilentlyContinue
    if (Get-Process -Id $process.Id -ErrorAction SilentlyContinue) { throw 'The running island did not close. Quit from its tray menu and retry.' }
}
if (!$existing -and !$NoStartup -and !$AcceptDefaults) {
    $choice=Read-Host 'Start Floatlet with Windows? [Y/n]'
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
Set-Content -LiteralPath (Join-Path $install 'installed.marker') -Value 'Floatlet 0.5.0'
$shell=New-Object -ComObject WScript.Shell
$shortcut=$shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Programs')) 'Floatlet.lnk'))
$shortcut.TargetPath=$target
$shortcut.WorkingDirectory=$install
$shortcut.IconLocation=$target+',0'
$shortcut.Description='Native Windows island for music, a file tray, timers and calendar'
$shortcut.Save()
foreach ($oldName in @('Delight Island.lnk','Delay Island.lnk','Luma Island.lnk','Glide Island.lnk')) {
    $oldLink=Join-Path ([Environment]::GetFolderPath('Programs')) $oldName
    if ((Test-Path -LiteralPath $oldLink) -and $shell.CreateShortcut($oldLink).TargetPath -in (@($target)+$oldTargets)) { Remove-Item -LiteralPath $oldLink }
}
$run='HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
if ($existing) {
    # Preserve the registry value name and Windows StartupApproved preferences.
    foreach ($key in @('DelightIsland','GlideIsland','Floatlet')) {
        $value=(Get-ItemProperty -Path $run -ErrorAction SilentlyContinue).$key
        if ($value -in @($oldTargets | ForEach-Object { '"'+$_+'"' })) { Set-ItemProperty -Path $run -Name $key -Value ('"'+$target+'"') }
    }
} elseif (!$NoStartup) {
    New-Item -Path $run -Force | Out-Null
    New-ItemProperty -Path $run -Name Floatlet -Value ('"'+$target+'"') -PropertyType String -Force | Out-Null
}
foreach ($oldTarget in $oldTargets) { if ($existing -and (Test-Path -LiteralPath $oldTarget)) { Remove-Item -LiteralPath $oldTarget } }
Write-Host "Installed Floatlet at $target. Existing settings and tray references were preserved."
if ($Launch) { Start-Process -FilePath $target -WorkingDirectory $install -WindowStyle Hidden }
