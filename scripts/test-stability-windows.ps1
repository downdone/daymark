param(
    [ValidateSet('windows-debug','windows-release')][string]$Preset='windows-release',
    [ValidateRange(10,43200)][int]$DurationSeconds=28800,
    [ValidateRange(1,60)][int]$SampleSeconds=5,
    [string]$Executable=''
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if(!$Executable){$Executable=Join-Path $projectRoot "build/$Preset/bin/daymark.exe"}
$Executable=(Resolve-Path -LiteralPath $Executable).Path
$binaryHash=(Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash.ToLowerInvariant()
$cpuModel=(Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name).Trim()
$runDirectory=Join-Path $projectRoot ('build/stability-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,6))
New-Item -ItemType Directory -Path $runDirectory | Out-Null
$reportPath=Join-Path $runDirectory 'runtime.json'
$samplePath=Join-Path $runDirectory 'resources.csv'
$errorLog=Join-Path $runDirectory 'native.log'
$summaryPath=Join-Path $runDirectory 'summary.json'
$originalPath=$env:PATH
$originalBackend=$env:QT_QPA_PLATFORM
$process=$null
$samples=[Collections.Generic.List[object]]::new()
$elapsed=[Diagnostics.Stopwatch]::StartNew()
$previousSeconds=0.0
$previousCpu=0.0
$failure=$null
$success=$false
try {
    if(!(Test-Path -LiteralPath (Join-Path (Split-Path $Executable -Parent) 'Qt6Core.dll')) -and $env:DAYMARK_QT -and $env:DAYMARK_MINGW){$env:PATH="$env:DAYMARK_MINGW/bin;$env:DAYMARK_QT/bin;$originalPath"}
    $env:QT_QPA_PLATFORM='windows'
    $arguments=@('--soak-seconds',$DurationSeconds.ToString(),'--soak-report',('"'+$reportPath+'"'))
    $process=Start-Process -FilePath $Executable -ArgumentList $arguments -WorkingDirectory (Split-Path $Executable -Parent) -WindowStyle Hidden -PassThru -RedirectStandardError $errorLog
    while($true) {
        $process.Refresh()
        if($process.HasExited){break}
        $seconds=$elapsed.Elapsed.TotalSeconds
        if($seconds -gt $DurationSeconds+150){throw 'Stability process timed out.'}
        $cpu=$process.TotalProcessorTime.TotalSeconds
        if($seconds -gt $previousSeconds) {
            # One fully used logical core is 100%; normalized percent is also retained.
            $corePercent=[math]::Max(0,100*($cpu-$previousCpu)/($seconds-$previousSeconds))
            $sample=[ordered]@{elapsedSeconds=[math]::Round($seconds,3);cpuCorePercent=[math]::Round($corePercent,3);cpuMachinePercent=[math]::Round($corePercent/[Environment]::ProcessorCount,3);workingSetBytes=$process.WorkingSet64;privateBytes=$process.PrivateMemorySize64}
            $samples.Add([pscustomobject]$sample)
            [pscustomobject]$sample | Export-Csv -LiteralPath $samplePath -NoTypeInformation -Append -Encoding utf8
            $previousSeconds=$seconds;$previousCpu=$cpu
        }
        Start-Sleep -Seconds $SampleSeconds
    }
    $process.WaitForExit()
    if($process.ExitCode -ne 0){throw "Stability process exited with code $($process.ExitCode)."}
    if(!(Test-Path -LiteralPath $reportPath)){throw 'Runtime report missing.'}
    $runtime=Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    if(!$runtime.passed -or $runtime.completedCycles -lt 1 -or $runtime.elapsedSeconds -lt $DurationSeconds -or $runtime.integrityChecks -lt 1){throw 'Runtime report did not satisfy the requested run.'}
    $success=$true
} catch {
    $failure=$_.Exception.Message
} finally {
    if($null -ne $process -and !$process.HasExited){$process.Kill();$process.WaitForExit()}
    $env:PATH=$originalPath;$env:QT_QPA_PLATFORM=$originalBackend
    $elapsed.Stop()
    $peakPrivate=0L;$peakWorking=0L
    if($samples.Count){$peakPrivate=($samples | Measure-Object privateBytes -Maximum).Maximum;$peakWorking=($samples | Measure-Object workingSetBytes -Maximum).Maximum}
    [ordered]@{format=1;passed=$success;failure=$failure;binarySha256=$binaryHash;cpuModel=$cpuModel;requestedSeconds=$DurationSeconds;totalProcessWallSeconds=[math]::Round($elapsed.Elapsed.TotalSeconds,3);samples=$samples.Count;peakPrivateBytes=$peakPrivate;peakWorkingSetBytes=$peakWorking;logicalProcessors=[Environment]::ProcessorCount;scope='Synthetic workload in one isolated process; includes smoke bootstrap. CPU includes active operations, not an idle-only benchmark.';eightHourRunCompleted=($success -and $DurationSeconds -ge 28800);manualSleepMultiScreenVerified=$false;userDataAccessed=$false} | ConvertTo-Json | Set-Content -LiteralPath $summaryPath -Encoding utf8
}
if(!$success){throw "Stability validation failed: $failure Evidence: $runDirectory"}
Write-Output "PASS: $DurationSeconds-second isolated continuous run. Evidence: $runDirectory"
