#requires -Version 7.0
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot '../../scripts/PackageValidation.psm1') -Force
$testRoot=Join-Path ([IO.Path]::GetTempPath()) ('daymark-manifest-test-'+[guid]::NewGuid().ToString('N'))
$root=Join-Path $testRoot 'package'
New-Item -ItemType Directory -Path $root | Out-Null
$required=@('daymark.exe','Qt6Core.dll','platforms/qwindows.dll','sqldrivers/qsqlite.dll','qt.conf','docs/user-guide.md','docs/third-party-notices.md','verification/Test-Package.ps1','verification/PackageValidation.psm1')
foreach($relative in $required) {
    $path=Join-Path $root $relative
    New-Item -ItemType Directory -Path (Split-Path $path -Parent) -Force | Out-Null
    [IO.File]::WriteAllText($path,'synthetic fixture; not a runnable Qt deployment')
}
$info=[ordered]@{version='1.0.0-rc.2';sourceRevision=('a'*40);qt='6.10.2';schemaVersion=3;scope='Windows local release candidate';signed=$false}
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
    Write-Manifest;$null=Assert-DaymarkPackage $root;Write-Output 'PASS: valid synthetic manifest (no native runtime claim)'
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
    $link=Join-Path $root 'linked';New-Item -ItemType Junction -Path $link -Target (Join-Path $root 'docs') | Out-Null
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
