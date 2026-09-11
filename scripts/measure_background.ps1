param([int]$Seconds=30)
$ErrorActionPreference='Stop'
$expected=@((Join-Path $env:LOCALAPPDATA 'Programs\DelightIsland\Floatlet.exe'),(Join-Path $env:LOCALAPPDATA 'Programs\Floatlet\Floatlet.exe'))
$matches=@(Get-Process Floatlet | Where-Object { $_.Path -in $expected })
if ($matches.Count -ne 1) { throw 'Expected exactly one installed island process.' }
$islandId=$matches[0].Id
$start=Get-Process -Id $islandId
$startCpu=$start.TotalProcessorTime.TotalSeconds
$clock=[Diagnostics.Stopwatch]::StartNew()
$samples=@()
for ($i=0; $i -lt $Seconds; $i++) {
    Start-Sleep -Seconds 1
    $current=Get-Process -Id $islandId
    $samples+=[pscustomobject]@{seconds=[math]::Round($clock.Elapsed.TotalSeconds,2); privateMiB=[math]::Round($current.PrivateMemorySize64/1MB,2);workingSetMiB=[math]::Round($current.WorkingSet64/1MB,2);handles=$current.HandleCount}
}
$elapsed=$clock.Elapsed.TotalSeconds
$result=[pscustomobject]@{
    measuredAt=(Get-Date).ToString('o');pid=$islandId;version=$current.FileVersion;hostArchitecture=$env:PROCESSOR_ARCHITECTURE;seconds=[math]::Round($elapsed,2)
    totalMachineCpuPercent=[math]::Round(($current.TotalProcessorTime.TotalSeconds-$startCpu)/$elapsed/[Environment]::ProcessorCount*100,4)
    privateMiBStart=[math]::Round($start.PrivateMemorySize64/1MB,2);privateMiBEnd=$samples[-1].privateMiB
    privateMiBPeak=($samples | Measure-Object privateMiB -Maximum).Maximum
    workingSetMiBEnd=$samples[-1].workingSetMiB;handlesEnd=$samples[-1].handles
    conditions='Installed background process. Three monitors connected. Playback and pointer activity uncontrolled. Battery and GPU not measured.'
    samples=$samples
}
$out=Join-Path $PSScriptRoot '..\docs\evidence\glide-resource-sample.json'
New-Item -ItemType Directory -Path (Split-Path $out) -Force | Out-Null
$result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $out -Encoding utf8
$result | Select-Object measuredAt,pid,version,seconds,totalMachineCpuPercent,privateMiBStart,privateMiBEnd,privateMiBPeak,workingSetMiBEnd,handlesEnd | Format-List
