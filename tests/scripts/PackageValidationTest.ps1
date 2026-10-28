#requires -Version 7.0
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot '../../scripts/PackageValidation.psm1') -Force
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('daymark-manifest-test-'+[guid]::NewGuid().ToString('N'))
$root=Join-Path $testRoot 'package'
New-Item -ItemType Directory -Path $root | Out-Null
$required=@('daymark.exe','Qt6Core.dll','platforms/qwindows.dll','sqldrivers/qsqlite.dll','qt.conf','README.md','THIRD-PARTY-NOTICES.md')
foreach($relative in $required) {
    $path=Join-Path $root $relative
    New-Item -ItemType Directory -Path (Split-Path $path -Parent) -Force | Out-Null
    [IO.File]::WriteAllText($path,'synthetic fixture; not a runnable Qt deployment')
}
$info=[ordered]@{version='1.0.0-rc.2';sourceRevision=('a'*40);qt='6.10.2';schemaVersion=9;scope='Windows local release candidate';signed=$false}
$info | ConvertTo-Json | Set-Content (Join-Path $root 'BUILD-INFO.json')
$entries=@(Get-ChildItem $root -File -Recurse | ForEach-Object {@{path=[IO.Path]::GetRelativePath($root,$_.FullName).Replace('\','/');size=$_.Length;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
$manifest=@{version='1.0.0-rc.2';qt='6.10.2';platform='windows-x64';scope='local verification';files=$entries}
function Write-Manifest {$manifest | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $root 'manifest.json')}
function Expect-Rejection([string]$Case) {
    $rejected=$false
    try {$null=Assert-DaymarkPackage $root}catch {$rejected=$true}
    if(!$rejected){throw "Expected rejection: $Case"}
    Write-Output "PASS: rejects $Case"
}
try {
    $junit=Join-Path $testRoot 'preview-tests.xml'
    $inventory=@('task_service','qml_startup','qml_watchdog_late')
    $known='<testsuite tests="3"><testcase name="task_service" status="run" time="0.1"/><testcase name="qml_startup" status="run"><failure message="known timeout"/></testcase><testcase name="qml_watchdog_late" status="run"><failure message="known timeout"/></testcase></testsuite>'
    [IO.File]::WriteAllText($junit,$known)
    $preview=Read-DaymarkPreviewTests $junit $inventory
    if($preview.fullGatePassed -or $preview.total -ne 3 -or ($preview.failed -join ',') -cne 'qml_startup,qml_watchdog_late'){throw 'Known failure preview scope lost.'}
    foreach($invalid in @(
        $known.Replace('<testcase name="task_service" status="run" time="0.1"/>',''),
        $known.Replace('<testcase name="task_service" status="run" time="0.1"/>','<testcase name="task_service"><skipped/></testcase>'),
        $known.Replace('<testcase name="task_service" status="run" time="0.1"/>','<testcase name="task_service"><failure/></testcase>'),
        $known.Replace('name="qml_watchdog_late"','name="qml_startup"'),
        '<!DOCTYPE testsuite [<!ENTITY external SYSTEM "file:///forbidden">]><testsuite>&external;</testsuite>'
    )) {
        [IO.File]::WriteAllText($junit,$invalid)
        $rejected=$false;try {$null=Read-DaymarkPreviewTests $junit $inventory}catch {$rejected=$true}
        if(!$rejected){throw 'Preview accepted incomplete, skipped, duplicate, unexpected failure or DTD.'}
    }
    [IO.File]::WriteAllText($junit,($known -replace '<failure[^>]*/>',''))
    if(!(Read-DaymarkPreviewTests $junit $inventory).fullGatePassed){throw 'Passing report classified as failed.'}
    Write-Output 'PASS: preview requires complete full-test inventory; rejects new failures, skipped/duplicate cases and DTD'
    Write-Manifest;$null=Assert-DaymarkPackage $root;Write-Output 'PASS: valid synthetic manifest (no native runtime claim)'
    foreach($relative in @('docs/PROGRESS.md','docs/design/draft.md','verification/Test-Package.ps1','qmltooling/qmldbg_debugger.dll','qml/QtQuick/Controls/Material/qmldir','qml/QtQuick/plugins.qmltypes','sqldrivers/qsqlodbc.dll','Qt6QuickControls2Material.dll','extra.txt')) {
        if(Test-DaymarkPayloadPath $relative){throw ('Non-runtime path accepted: '+$relative)}
    }
    foreach($relative in @('qml/QtQuick/Controls/Basic/qmldir','qml/QtQuick/Dialogs/quickimpl/qml/FileDialog.qml','opengl32sw.dll','D3Dcompiler_47.dll','qt-sbom/qtbase-6.10.2.spdx.json')) {
        if(!(Test-DaymarkPayloadPath $relative)){throw ('Required runtime path rejected: '+$relative)}
    }
    # A self-consistent manifest cannot authorize developer documents.
    $extraDirectory=Join-Path $root 'docs';New-Item -ItemType Directory -Path $extraDirectory | Out-Null
    $extraPath=Join-Path $extraDirectory 'PROGRESS.md';[IO.File]::WriteAllText($extraPath,'synthetic developer file')
    $manifest.files=@($entries)+@(@{path='docs/PROGRESS.md';size=(Get-Item $extraPath).Length;sha256=(Get-FileHash $extraPath -Algorithm SHA256).Hash.ToLowerInvariant()})
    Write-Manifest;Expect-Rejection 'developer document even with valid manifest entry'
    Remove-Item -LiteralPath $extraPath;Remove-Item -LiteralPath $extraDirectory;$manifest.files=$entries;Write-Manifest
    $pruneRoot=Join-Path $testRoot 'Daymark-test-windows-x64-local-20261028-120000'
    New-Item -ItemType Directory -Path $pruneRoot | Out-Null
    foreach($relative in @('daymark.exe','qml/QtQuick/qmldir','qml/QtQuick/plugins.qmltypes','qml/QtQuick/Controls/Material/qmldir','Qt6QuickControls2Material.dll','sqldrivers/qsqlodbc.dll','sqldrivers/qsqlite.dll')) {
        $path=Join-Path $pruneRoot $relative;New-Item -ItemType Directory -Path (Split-Path $path -Parent) -Force | Out-Null
        [IO.File]::WriteAllText($path,"module Test`ntypeinfo plugins.qmltypes`n")
    }
    $removed=@(Remove-DaymarkDeploymentExtras $pruneRoot)
    if($removed.Count -ne 4 -or !(Test-Path -LiteralPath (Join-Path $pruneRoot 'sqldrivers/qsqlite.dll')) -or [IO.File]::ReadAllText((Join-Path $pruneRoot 'qml/QtQuick/qmldir')) -match 'typeinfo'){throw 'Runtime pruning did not preserve imports/SQLite or remove tool metadata.'}
    Write-Output 'PASS: minimal payload admission and deployment pruning preserve Basic/runtime resources'
    $previewInfo=[ordered]@{};foreach($key in $info.Keys){$previewInfo[$key]=$info[$key]}
    $previewInfo.scope='Windows local preview candidate';$previewInfo.localPreview=$true
    $previewInfo.fullTestGatePassed=$false;$previewInfo.knownTestFailures=@($preview.failed)
    $summaryPath=Join-Path $root 'TEST-RESULTS.json'
    $preview | ConvertTo-Json -Depth 6 | Set-Content $summaryPath
    $previewInfo | ConvertTo-Json | Set-Content (Join-Path $root 'BUILD-INFO.json')
    $savedEntries=$manifest.files
    $manifest.files=@(Get-ChildItem $root -File -Recurse | Where-Object Name -ne 'manifest.json' | ForEach-Object {@{path=[IO.Path]::GetRelativePath($root,$_.FullName).Replace('\','/');size=$_.Length;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}})
    Write-Manifest;$null=Assert-DaymarkPackage $root
    $previewInfo.fullTestGatePassed=$true
    $previewInfo | ConvertTo-Json | Set-Content (Join-Path $root 'BUILD-INFO.json')
    Expect-Rejection 'preview metadata claiming passed gate with recorded failures'
    $previewInfo.fullTestGatePassed=$false
    $previewInfo | ConvertTo-Json | Set-Content (Join-Path $root 'BUILD-INFO.json')
    Remove-Item -LiteralPath $summaryPath
    Expect-Rejection 'preview without full-test evidence'
    $info | ConvertTo-Json | Set-Content (Join-Path $root 'BUILD-INFO.json')
    $manifest.files=$savedEntries;Write-Manifest
    $info.schemaVersion=4
    $info | ConvertTo-Json | Set-Content (Join-Path $root 'BUILD-INFO.json')
    $metadataEntry=$entries | Where-Object { $_.path -eq 'BUILD-INFO.json' }
    $metadataEntry.size=(Get-Item (Join-Path $root 'BUILD-INFO.json')).Length
    $metadataEntry.sha256=(Get-FileHash (Join-Path $root 'BUILD-INFO.json') -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Manifest;Expect-Rejection 'obsolete schema metadata'
    $info.schemaVersion=9
    $info | ConvertTo-Json | Set-Content (Join-Path $root 'BUILD-INFO.json')
    $metadataEntry.size=(Get-Item (Join-Path $root 'BUILD-INFO.json')).Length
    $metadataEntry.sha256=(Get-FileHash (Join-Path $root 'BUILD-INFO.json') -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Manifest;$null=Assert-DaymarkPackage $root
    $taskExe=Join-Path $root 'daymark.exe';$original=[IO.File]::ReadAllBytes($taskExe)
    [IO.File]::WriteAllText($taskExe,'tampered');Expect-Rejection 'modified file';[IO.File]::WriteAllBytes($taskExe,$original)
    Move-Item -LiteralPath $taskExe -Destination ($taskExe+'.held');Expect-Rejection 'missing file';Move-Item -LiteralPath ($taskExe+'.held') -Destination $taskExe
    $extra=Join-Path $root 'extra.txt';[IO.File]::WriteAllText($extra,'extra');Expect-Rejection 'unlisted file';Remove-Item -LiteralPath $extra
    $manifest.files=@($entries)+@($entries[0]);Write-Manifest;Expect-Rejection 'duplicate path'
    $manifest.files=@($entries)+@(@{path='../outside.txt';size=0;sha256=('0'*64)});Write-Manifest;Expect-Rejection 'path traversal'
    $manifest.files=@($entries);$manifest.version='9.0.0';Write-Manifest;Expect-Rejection 'metadata version conflict';$manifest.version='1.0.0-rc.2'
    Write-Manifest;$text=Get-Content (Join-Path $root 'manifest.json') -Raw
    [IO.File]::WriteAllText((Join-Path $root 'manifest.json'),$text.Replace('"qt": "6.10.2"','"qt": "6.10.2", "qt": "6.10.2"'));Expect-Rejection 'duplicate JSON key';Write-Manifest
    $sensitive=Join-Path $root 'personal.sqlite';[IO.File]::WriteAllText($sensitive,'synthetic');Expect-Rejection 'database artifact';Remove-Item -LiteralPath $sensitive
    $link=Join-Path $root 'linked';New-Item -ItemType Junction -Path $link -Target (Join-Path $testRoot 'package') | Out-Null
    Expect-Rejection 'reparse point';[IO.Directory]::Delete($link)
    $archive=Join-Path $testRoot 'package.zip';Compress-Archive -LiteralPath $root -DestinationPath $archive
    $sha=(Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant();"$sha  package.zip" | Set-Content ($archive+'.sha256')
    $null=Assert-DaymarkArchive $archive $root;[IO.File]::AppendAllText($archive,'tampered')
    $rejected=$false;try {$null=Assert-DaymarkArchive $archive $root}catch {$rejected=$true};if(!$rejected){throw 'Tampered archive accepted'}
    Write-Output 'PASS: archive checksum round-trip and tamper rejection'
    # Updating the sidecar cannot conceal an extra file inside the ZIP.
    $zip=[IO.Compression.ZipFile]::Open($archive,[IO.Compression.ZipArchiveMode]::Update)
    try {$null=$zip.CreateEntry('package/extra.txt')}finally {$zip.Dispose()}
    $sha=(Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant();"$sha  package.zip" | Set-Content ($archive+'.sha256')
    $rejected=$false;try {$null=Assert-DaymarkArchive $archive $root}catch {$rejected=$true};if(!$rejected){throw 'Unexpected ZIP content accepted'}
    Write-Output 'PASS: rejects ZIP content mismatch even with updated sidecar'
    $fakeWindows=Join-Path $testRoot 'windows'
    if((Get-DaymarkRuntimeOrigin 'Qt6Core.dll' (Join-Path $root 'Qt6Core.dll') $root $fakeWindows) -ne 'package'){throw 'Package origin not accepted'}
    if((Get-DaymarkRuntimeOrigin 'D3DCompiler_47.dll' (Join-Path $fakeWindows 'System32/D3DCompiler_47.dll') $root $fakeWindows) -ne 'windows-system'){throw 'System graphics origin not accepted'}
    foreach($path in @((Join-Path $testRoot 'package-other/Qt6Core.dll'),(Join-Path $root '../sdk/Qt6Core.dll'),(Join-Path $fakeWindows 'System32/Qt6Core.dll'))) {
        $rejected=$false;try {$null=Get-DaymarkRuntimeOrigin 'Qt6Core.dll' $path $root $fakeWindows}catch {$rejected=$true};if(!$rejected){throw 'External Qt runtime accepted'}
    }
    $rejected=$false;try {$null=Get-DaymarkRuntimeOrigin 'D3DCompiler_47.dll' (Join-Path $testRoot 'sdk/D3DCompiler_47.dll') $root $fakeWindows}catch {$rejected=$true};if(!$rejected){throw 'External graphics compiler accepted'}
    Write-Output 'PASS: runtime path algorithm rejects sibling prefix, traversal, system Qt and SDK graphics; no OS verification claim'
    $verifier=Join-Path $PSScriptRoot '../../scripts/Test-Package.ps1'
    $rejected=$false;try {& $verifier -PackageDirectory $root -ReportParent $root}catch {if($_.Exception.Message -notmatch 'outside the package'){throw};$rejected=$true};if(!$rejected){throw 'In-package report path accepted'}
    $reportLink=Join-Path $testRoot 'report-link';New-Item -ItemType Junction -Path $reportLink -Target $root | Out-Null
    try {
        $rejected=$false;try {& $verifier -PackageDirectory $root -ReportParent $reportLink}catch {if($_.Exception.Message -notmatch 'reparse point'){throw};$rejected=$true};if(!$rejected){throw 'Reparse report path accepted'}
    } finally {[IO.Directory]::Delete($reportLink)}
    $null=Assert-DaymarkPackage $root
    Write-Output 'PASS: report paths inside package or through junction rejected before executing synthetic fixture'
} finally {
    # This exact, freshly created GUID directory is the only recursive cleanup target.
    $resolved=[IO.Path]::GetFullPath($testRoot)
    $tempPrefix=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
    if(!$resolved.StartsWith($tempPrefix,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetFileName($resolved) -notmatch '^daymark-manifest-test-[a-f0-9]{32}$'){throw 'Unsafe test cleanup target'}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
