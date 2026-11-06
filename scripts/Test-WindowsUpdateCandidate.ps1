#requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$Directory,
    [Parameter(Mandatory)][string]$SourceRevision,
    [Parameter(Mandatory)][string]$Version
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Import-Module (Join-Path $PSScriptRoot 'Authenticode.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'PackageValidation.psm1') -Force
$root = (Get-Item -LiteralPath $Directory).FullName
$payload = Join-Path $root 'payload'
$infoPath = Join-Path $payload 'BUILD-INFO.json'
$info = Get-Content -LiteralPath $infoPath -Raw | ConvertFrom-Json -AsHashtable
if ($SourceRevision -cnotmatch '^[0-9a-f]{40}$' -or $info.sourceRevision -cne $SourceRevision -or $info.version -cne $Version -or $info.fullTestGatePassed -isnot [bool] -or !$info.fullTestGatePassed) {
    throw 'Candidate lacks exact source/version and successful full tests.'
}
$policy = Get-Content -LiteralPath (Join-Path $repo 'config/authenticode-policy.json') -Raw | ConvertFrom-Json
$files = @('payload/daymark.exe','native/daymark-launcher.exe','native/daymark-update-cli.exe','native/daymark-updater.exe')
foreach ($file in $files) { $null = Assert-DaymarkSelfSignedExecutable (Join-Path $root $file) $policy.certificate_sha256 }
$native = @($files | Select-Object -Skip 1 | ForEach-Object { Join-Path $root $_ })
& python (Join-Path $PSScriptRoot 'audit-native-imports.py') @native
if ($LASTEXITCODE -ne 0) { throw 'Native updater imports are not OS-only.' }
$capabilities = & $native[1] capabilities | ConvertFrom-Json
if ($LASTEXITCODE -ne 0 -or !$capabilities.trust_configured -or $capabilities.runtime -ne 'msvc143' -or $capabilities.qt_required) { throw 'Invalid compiled native capabilities.' }
foreach ($file in Get-ChildItem -LiteralPath $payload -Filter 'Qt6*.dll') {
    $versionInfo = [Diagnostics.FileVersionInfo]::GetVersionInfo($file.FullName)
    if ($versionInfo.FileMajorPart -ne 6 -or $versionInfo.FileMinorPart -ne 10 -or $versionInfo.FileBuildPart -ne 2) { throw 'Qt version is not 6.10.2.' }
}
if ($info.runtimeMinimum -ne 'bundled' -or $info.runtime -ne 'msvc143' -or !$info.runtimeFiles.Count) { throw 'Bundled MSVC runtime evidence missing.' }
foreach ($file in $info.runtimeFiles) {
    if ($file.name -notmatch '^[A-Za-z0-9_]+\.dll$' -or (Get-FileHash -LiteralPath (Join-Path $payload $file.name) -Algorithm SHA256).Hash -ine $file.sha256) { throw 'Bundled runtime identity mismatch.' }
    $signature = Get-AuthenticodeSignature -LiteralPath (Join-Path $payload $file.name)
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch '(^|,\s*)O=Microsoft Corporation(,|$)') { throw 'Bundled Microsoft runtime signature failed.' }
}
$reportDirectory = Join-Path (Split-Path $root -Parent) 'deployment-evidence'
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
$names = @('PATH','QT_QPA_PLATFORM','QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','QT_CONF_PATH')
$saved = @{}; foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
$observed = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$deployed = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($file in Get-ChildItem -LiteralPath $payload -Recurse -File -Filter '*.dll') { [void]$deployed.Add($file.Name) }
$process = $null
try {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
    $env:QT_QPA_PLATFORM = 'windows'
    $exe = Join-Path $payload 'daymark.exe'
    $process = Start-Process -FilePath $exe -ArgumentList '--version' -WorkingDirectory $payload -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $reportDirectory 'version.txt') -RedirectStandardError (Join-Path $reportDirectory 'version-error.txt')
    if (!$process.WaitForExit(10000) -or $process.ExitCode -ne 0 -or ([IO.File]::ReadAllText((Join-Path $reportDirectory 'version.txt'))).Trim() -cne "Daymark $Version") { throw 'Deployed application version probe failed.' }
    $process = Start-Process -FilePath $exe -ArgumentList '--smoke-test' -WorkingDirectory $payload -WindowStyle Hidden -PassThru -RedirectStandardError (Join-Path $reportDirectory 'native.log')
    $timer = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        $process.Refresh(); if ($process.HasExited) { break }
        if ($timer.Elapsed.TotalSeconds -gt 120) { throw 'Independent deployed native smoke timed out.' }
        try { $modules = @((Get-Process -Id $process.Id -ErrorAction Stop).Modules) }
        catch { $process.Refresh(); if ($process.HasExited) { break }; throw }
        foreach ($module in $modules) {
            if (!$deployed.Contains($module.ModuleName) -and $module.ModuleName -notmatch '^(Qt6.*|qwindows|qsqlite|msvcp140.*|vcruntime140.*|concrt140|vccorlib140)\.dll$') { continue }
            $origin = Get-DaymarkRuntimeOrigin $module.ModuleName $module.FileName $payload $env:SystemRoot
            if ($origin -eq 'package') { [void]$observed.Add($module.ModuleName) }
        }
        Start-Sleep -Milliseconds 50
    }
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw 'Independent deployed native smoke failed.' }
    foreach ($required in @('Qt6Core.dll','qwindows.dll','qsqlite.dll','msvcp140.dll','vcruntime140.dll')) {
        if (!$observed.Contains($required)) { throw "Required app-local module was not observed: $required" }
    }
    if ([IO.File]::ReadAllText((Join-Path $reportDirectory 'native.log')) -notmatch 'QML startup: visible windows=\s*2 warnings=\s*false') { throw 'Native window/QML completion evidence missing.' }
} finally {
    if ($null -ne $process -and !$process.HasExited) { $process.Kill(); $process.WaitForExit() }
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
}
$info.cleanMachine = $true
$info.deploymentValidation = [ordered]@{kind='independent-windows-hosted-runner';runtimeOriginsAudited=$true;minimumWindows10Verified=$false}
$info | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath $infoPath -Encoding utf8
$inventory = @(foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File | Sort-Object FullName) {
    if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Reparse point in validated candidate.' }
    [ordered]@{path=[IO.Path]::GetRelativePath($root,$file.FullName).Replace('\','/');sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
})
[ordered]@{format=1;passed=$true;sourceRevision=$SourceRevision;version=$Version;kind='independent-windows-hosted-runner';
    runtimeOriginsAudited=$true;minimumWindows10Verified=$false;osBuild=[Environment]::OSVersion.Version.Build;
    observedModules=@($observed | Sort-Object);files=$inventory} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $root 'deployment-report.json') -Encoding utf8
Write-Output 'PASS: independent Windows runner; cleared SDK environment; actual native windows and app-local Qt/CRT origins; candidate hashes bound to deployment report.'
