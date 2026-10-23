#requires -Version 7.0
param(
    [string]$PackageDirectory=(Split-Path $PSScriptRoot -Parent),
    [string]$ReportParent=[IO.Path]::GetTempPath()
)
$ErrorActionPreference='Stop'
if(!$IsWindows){throw 'This verifier requires Windows and PowerShell 7.'}
Import-Module (Join-Path $PSScriptRoot 'PackageValidation.psm1') -Force
$package=Assert-DaymarkPackage $PackageDirectory
$prefix=$package.root.TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
$reportParentPath=(Get-Item -LiteralPath $ReportParent).FullName
if($reportParentPath -ieq $package.root -or $reportParentPath.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Reports must be outside the package.'}
$reportAncestor=Get-Item -LiteralPath $reportParentPath -Force
while($null -ne $reportAncestor) {
    if($reportAncestor.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Report path cannot traverse a reparse point.'}
    $reportAncestor=$reportAncestor.Parent
}
$output=Join-Path $reportParentPath ('daymark-package-check-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
$environmentNames=@('PATH','QT_QPA_PLATFORM','QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','QT_CONF_PATH')
$saved=@{};foreach($name in $environmentNames){$saved[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
$process=$null;$passed=$false;$failure=$null
$observed=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$systemModules=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$deployedNames=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($path in $package.files.Keys){if($path.EndsWith('.dll',[StringComparison]::OrdinalIgnoreCase)){[void]$deployedNames.Add([IO.Path]::GetFileName($path))}}
try {
    foreach($path in $package.files.Keys) {
        if([IO.Path]::GetFileName($path) -notmatch '^Qt6.*\.dll$'){continue}
        $versionInfo=[Diagnostics.FileVersionInfo]::GetVersionInfo($package.files[$path].FullName)
        if($versionInfo.FileMajorPart -ne 6 -or $versionInfo.FileMinorPart -ne 10 -or $versionInfo.FileBuildPart -ne 2){throw 'Deployed Qt DLL version differs from 6.10.2.'}
    }
    foreach($name in $environmentNames){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:PATH="$env:SystemRoot/System32;$env:SystemRoot";$env:QT_QPA_PLATFORM='windows'
    $exe=Join-Path $package.root 'daymark.exe'
    $process=Start-Process -FilePath $exe -ArgumentList '--version' -WorkingDirectory $package.root -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $output 'version.txt') -RedirectStandardError (Join-Path $output 'version-error.txt')
    if(!$process.WaitForExit(10000)){throw 'Version process timed out.'}
    if($process.ExitCode -ne 0 -or ([IO.File]::ReadAllText((Join-Path $output 'version.txt'))).Trim() -cne ('Daymark '+$package.info.version)){throw 'Executable version differs from package metadata.'}
    $process=Start-Process -FilePath $exe -ArgumentList '--smoke-test' -WorkingDirectory $package.root -WindowStyle Hidden -PassThru -RedirectStandardError (Join-Path $output 'native.log')
    $elapsed=[Diagnostics.Stopwatch]::StartNew()
    while($true) {
        $process.Refresh();if($process.HasExited){break}
        if($elapsed.Elapsed.TotalSeconds -gt 60){throw 'Native smoke timed out.'}
        try {$modules=@((Get-Process -Id $process.Id -ErrorAction Stop).Modules)}
        catch {$process.Refresh();if($process.HasExited){break};throw}
        foreach($module in $modules) {
            if(!$deployedNames.Contains($module.ModuleName)){continue}
            try {$origin=Get-DaymarkRuntimeOrigin $module.ModuleName $module.FileName $package.root $env:SystemRoot}
            catch {
                [IO.File]::WriteAllText((Join-Path $output 'external-module.txt'),$module.ModuleName+': '+$module.FileName)
                throw
            }
            if($origin -eq 'windows-system'){[void]$systemModules.Add($module.ModuleName)}else{[void]$observed.Add($module.ModuleName)}
        }
        Start-Sleep -Milliseconds 100
    }
    $process.WaitForExit()
    if($process.ExitCode -ne 0){throw 'Native smoke failed.'}
    foreach($required in @('Qt6Core.dll','qwindows.dll','qsqlite.dll')){if(!$observed.Contains($required)){throw 'Required runtime module was not observed.'}}
    $nativeLog=[IO.File]::ReadAllText((Join-Path $output 'native.log'))
    if($nativeLog -notmatch 'QML startup: visible windows=\s*2 warnings=\s*false'){throw 'Native smoke completion evidence missing.'}
    if($nativeLog -match 'items in the process of being created at engine destruction'){throw 'Native smoke exited with unfinished QML creation.'}
    $null=Assert-DaymarkPackage $package.root
    $passed=$true
} catch {$failure=$_.Exception.Message}
finally {
    if($null -ne $process -and !$process.HasExited){$process.Kill();$process.WaitForExit()}
    foreach($name in $environmentNames){[Environment]::SetEnvironmentVariable($name,$saved[$name],'Process')}
    [ordered]@{format=1;passed=$passed;failure=$failure;version=$package.info.version;sourceRevision=$package.info.sourceRevision;qt=$package.info.qt;
        executableSha256=$package.manifest.files.Where({$_.path -ieq 'daymark.exe'})[0].sha256;
        observedModules=@($observed | Sort-Object);windowsSystemModules=@($systemModules | Sort-Object);scope='Local SDK-isolated native smoke; not an independent clean machine';
        cleanMachineVerified=$false;redistributionLicenseVerified=$false;userDataAccessed=$false} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'report.json') -Encoding utf8
}
if(!$passed){throw "Package validation failed: $failure Report: $output"}
Write-Output "PASS: manifest, executable version and SDK-isolated native smoke. Report: $output"
