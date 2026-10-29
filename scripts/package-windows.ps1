#requires -Version 7.0
param([string]$Version='',[switch]$LocalPreview)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
Import-Module (Join-Path $PSScriptRoot 'PackageValidation.psm1') -Force
$versionMatch=[regex]::Match([IO.File]::ReadAllText((Join-Path $projectRoot 'CMakeLists.txt')),'set\(DAYMARK_VERSION "([^"]+)"\)')
if(!$versionMatch.Success){throw 'Application version baseline missing.'}
if(!$Version){$Version=$versionMatch.Groups[1].Value}
if($Version -notmatch '^\d+\.\d+\.\d+(-rc\.\d+)?$' -or $Version -cne $versionMatch.Groups[1].Value){throw 'Package version must match the application version baseline.'}
$sourceRevision=(& git -C $projectRoot rev-parse HEAD).Trim()
if($LASTEXITCODE -ne 0 -or $sourceRevision -notmatch '^[a-f0-9]{40}$'){throw 'Source revision unavailable.'}
$changes=@(& git -C $projectRoot status --porcelain --untracked-files=normal)
if($LASTEXITCODE -ne 0 -or $changes.Count){throw 'Commit source changes before packaging; dirty source cannot be attributed to HEAD.'}
if(!(Test-Path -LiteralPath "$env:DAYMARK_QT/bin/windeployqt.exe")){throw 'Set DAYMARK_QT to the matching Qt kit.'}
# Always reconfigure/build/test this committed checkout, rather than attributing a stale binary to HEAD.
& (Join-Path $PSScriptRoot 'build-windows.ps1') -Preset windows-release
$fullTestGatePassed=$true;$knownTestFailures=@();$previewSummary=$null
if(!$LocalPreview){& (Join-Path $PSScriptRoot 'test-windows.ps1') -Preset windows-release}
else {
    $junitPath=Join-Path $projectRoot ('build/package-preview-tests-'+[guid]::NewGuid().ToString('N')+'.xml')
    Push-Location $projectRoot
    try {
        $inventory=(& ctest --preset windows-release --show-only=json-v1 | ConvertFrom-Json)
        if($LASTEXITCODE -ne 0 -or !@($inventory.tests).Count){throw 'CTest inventory unavailable.'}
        & ctest --preset windows-release --output-on-failure --output-junit $junitPath
        $testExit=$LASTEXITCODE
        $previewSummary=Read-DaymarkPreviewTests $junitPath @($inventory.tests.name)
        if(($testExit -eq 0) -ne $previewSummary.fullGatePassed){throw 'CTest exit and report disagree.'}
        $fullTestGatePassed=$previewSummary.fullGatePassed;$knownTestFailures=@($previewSummary.failed)
    } finally {Pop-Location}
}
$executable=Join-Path $projectRoot 'build/windows-release/bin/daymark.exe'
if(!(Test-Path -LiteralPath $executable)){throw 'Build and test windows-release first.'}
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$packageRoot=Join-Path $projectRoot "dist/Daymark-$Version-windows-x64-local-$stamp"
if(Test-Path -LiteralPath $packageRoot){throw 'Output already exists; no files were overwritten.'}
New-Item -ItemType Directory -Path $packageRoot | Out-Null
Copy-Item -LiteralPath $executable -Destination $packageRoot
$strip=Join-Path $env:DAYMARK_MINGW 'bin/strip.exe'
if(!(Test-Path -LiteralPath $strip)){throw 'Matching MinGW strip tool missing.'}
& $strip --strip-unneeded (Join-Path $packageRoot 'daymark.exe')
if($LASTEXITCODE -ne 0){throw 'Release symbol stripping failed.'}
$originalPath=$env:PATH
try {
    $env:PATH="$env:DAYMARK_MINGW/bin;$env:DAYMARK_QT/bin;$originalPath"
    & "$env:DAYMARK_QT/bin/windeployqt.exe" --release --qmldir "$projectRoot/src/ui" --dir $packageRoot --no-translations --skip-plugin-types 'qmltooling,generic' --exclude-plugins 'qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql' (Join-Path $packageRoot 'daymark.exe')
    if($LASTEXITCODE -ne 0){throw 'windeployqt failed; package incomplete.'}
    foreach($dll in @('libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
        $runtime=Join-Path $env:DAYMARK_MINGW "bin/$dll"
        if(!(Test-Path -LiteralPath $runtime)){throw "Compiler runtime missing: $dll"}
        Copy-Item -LiteralPath $runtime -Destination $packageRoot
    }
} finally {$env:PATH=$originalPath}
$removed=Remove-DaymarkDeploymentExtras $packageRoot
$removed | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $projectRoot ('build/package-pruned-'+$stamp+'.json')) -Encoding utf8
foreach($name in @('README.md','THIRD-PARTY-NOTICES.md')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot ('packaging/'+$name)) -Destination $packageRoot
}
if($LocalPreview){$previewSummary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $packageRoot 'TEST-RESULTS.json') -Encoding utf8}
$bomDirectory=Join-Path $packageRoot 'qt-sbom'
New-Item -ItemType Directory -Path $bomDirectory | Out-Null
foreach($module in @('qtbase','qtdeclarative','qtshadertools','qtsvg')) {
    $bom=Join-Path $env:DAYMARK_QT "sbom/$module-6.10.2.spdx.json"
    if(!(Test-Path -LiteralPath $bom)){throw 'Required Qt SBOM missing; distribution review remains incomplete.'}
    Copy-Item -LiteralPath $bom -Destination $bomDirectory
}
@'
[Paths]
Prefix=.
Plugins=.
QmlImports=qml
'@ | Set-Content -LiteralPath (Join-Path $packageRoot 'qt.conf') -Encoding ascii
$packageScope=if($LocalPreview){'Windows local preview candidate'}else{'Windows local release candidate'}
[ordered]@{version=$Version;sourceRevision=$sourceRevision;qt='6.10.2';schemaVersion=10;scope=$packageScope;signed=$false;symbolsStripped=$true;localPreview=[bool]$LocalPreview;fullTestGatePassed=$fullTestGatePassed;knownTestFailures=@($knownTestFailures)} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $packageRoot 'BUILD-INFO.json') -Encoding utf8
$manifest=Get-ChildItem -LiteralPath $packageRoot -Recurse -File | ForEach-Object {
    [ordered]@{path=[IO.Path]::GetRelativePath($packageRoot,$_.FullName).Replace('\','/');size=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
}
[ordered]@{version=$Version;platform='windows-x64';scope='local verification';qt='6.10.2';files=@($manifest)} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $packageRoot 'manifest.json') -Encoding utf8
& (Join-Path $PSScriptRoot 'Test-Package.ps1') -PackageDirectory $packageRoot -ReportParent (Join-Path $projectRoot 'build')
$finalRevision=(& git -C $projectRoot rev-parse HEAD).Trim()
$finalChanges=@(& git -C $projectRoot status --porcelain --untracked-files=normal)
if($LASTEXITCODE -ne 0 -or $finalRevision -cne $sourceRevision -or $finalChanges.Count){throw 'Checkout changed during packaging; no archive created.'}
$archive="$packageRoot.zip"
Compress-Archive -LiteralPath $packageRoot -DestinationPath $archive -CompressionLevel Optimal
$hash=(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([IO.Path]::GetFileName($archive))" | Set-Content -LiteralPath "$archive.sha256" -Encoding ascii
$null=Assert-DaymarkArchive $archive $packageRoot
Write-Output "PASS: complete package, full-lifetime DLL audit, ZIP contents and SHA256. Local package: $archive"
