#requires -Version 7.0
# Local signed candidate only. No publication, trust-store import or user data access.
param(
 [Parameter(Mandatory)][string]$BuildDirectory,
 [Parameter(Mandatory)][string]$QtRoot,
 [Parameter(Mandatory)][string]$MingwRoot,
 [Parameter(Mandatory)][string]$CTestReport,
 [Parameter(Mandatory)][string]$Makensis,
 [Parameter(Mandatory)][string]$SignTool,
 [Parameter(Mandatory)][string]$OutputDirectory,
 [string]$VaultDirectory='config/signing.local'
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$build=(Resolve-Path $BuildDirectory).Path
$qt=(Resolve-Path $QtRoot).Path
$source=(& git -C $repo rev-parse HEAD).Trim()
if($LASTEXITCODE -ne 0 -or $source -cnotmatch '^[a-f0-9]{40}$'){throw 'Source revision unavailable.'}
& git -C $repo diff --quiet HEAD -- src cmake CMakeLists.txt assets/icons assets/brand/current config/update-public-keys.json config/authenticode-policy.json packaging scripts third_party
if($LASTEXITCODE -ne 0){throw 'Commit implementation and packaging inputs before building a candidate.'}
$version=(Select-String -Path (Join-Path $repo 'CMakeLists.txt') -Pattern 'set\(DAYMARK_VERSION "([^"]+)"\)').Matches[0].Groups[1].Value
$schemaMatch=[regex]::Match([IO.File]::ReadAllText((Join-Path $repo 'src/update/sdk/Update.h')),'inline constexpr unsigned Schema = ([0-9]+);')
if(!$schemaMatch.Success){throw 'Current update data schema unavailable.'}
$schemaVersion=[int]$schemaMatch.Groups[1].Value
Import-Module (Join-Path $PSScriptRoot 'PackageValidation.psm1') -Force
$inventory=(& ctest --test-dir $build --show-only=json-v1) | ConvertFrom-Json
if($LASTEXITCODE -ne 0){throw 'CTest inventory unavailable.'}
$summary=Read-DaymarkPreviewTests $CTestReport @($inventory.tests.name)
if(!$summary.fullGatePassed){throw 'Complete passing local CTest report required.'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $out){throw 'Choose a new output directory; nothing will be overwritten.'}
$candidate=Join-Path $out 'candidate';$payload=Join-Path $candidate ("Daymark-$version-windows-x64-local-"+(Get-Date -Format 'yyyyMMdd-HHmmss'));$native=Join-Path $candidate 'native'
New-Item -ItemType Directory -Path $payload,$native | Out-Null
Copy-Item -LiteralPath (Join-Path $build 'bin/daymark.exe') -Destination $payload
& (Join-Path $MingwRoot 'bin/strip.exe') --strip-unneeded (Join-Path $payload 'daymark.exe')
if($LASTEXITCODE -ne 0){throw 'Strip failed.'}
$originalPath=$env:PATH
try{
 $env:PATH="$qt/bin;$MingwRoot/bin;$originalPath"
 & (Join-Path $qt 'bin/windeployqt.exe') --release --compiler-runtime --qmldir (Join-Path $repo 'src/ui') --dir $payload --no-translations --skip-plugin-types 'qmltooling,generic' --exclude-plugins 'qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql' (Join-Path $payload 'daymark.exe')
 if($LASTEXITCODE -ne 0){throw 'Qt deployment failed.'}
}finally{$env:PATH=$originalPath}
$removed=@(Remove-DaymarkDeploymentExtras $payload)
if(![IO.Path]::GetFullPath($payload).StartsWith($out+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Deployment staging path escaped output directory.'}
Rename-Item -LiteralPath $payload -NewName 'payload'
$payload=Join-Path $candidate 'payload'
foreach($name in @('daymark-launcher.exe','daymark-update-cli.exe','daymark-updater.exe','daymark-setup-helper.exe')){Copy-Item -LiteralPath (Join-Path $build "bin/$name") -Destination $native}
foreach($name in @('LICENSE','packaging/THIRD-PARTY-NOTICES.md','packaging/licenses/GPL-3.0.txt')){Copy-Item -LiteralPath (Join-Path $repo $name) -Destination $payload}
Copy-Item -LiteralPath (Join-Path $repo 'LICENSE') -Destination (Join-Path $payload 'LGPL-3.0.txt')
New-Item -ItemType Directory -Path (Join-Path $payload 'licenses') | Out-Null
Copy-Item -LiteralPath (Join-Path $repo 'packaging/licenses/NSIS-COPYING.txt') -Destination (Join-Path $payload 'licenses/NSIS-COPYING.txt')
& python (Join-Path $PSScriptRoot 'copy-qt-sbom.py') --qt-root $qt --destination (Join-Path $payload 'qt-sbom') --version 6.10.2
if($LASTEXITCODE -ne 0){throw 'Qt SBOM failed.'}
[IO.File]::WriteAllText((Join-Path $payload 'qt.conf'),"[Paths]`nPrefix=.`nPlugins=.`nQmlImports=qml`n",[Text.Encoding]::ASCII)
$info=[ordered]@{version=$version;sourceRevision=$source;qt='6.10.2';runtime='mingw131';schemaVersion=$schemaVersion;scope='local installer candidate';signed=$false;localPreview=$true;cleanMachine=$false;fullTestGatePassed=$true;ctestCount=$summary.total;ctestReportSha256=(Get-FileHash $CTestReport).Hash.ToLowerInvariant()}
$info | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $payload 'BUILD-INFO.json') -Encoding utf8
$metadata=Get-Content -LiteralPath (Join-Path $VaultDirectory 'metadata.json') -Raw | ConvertFrom-Json
& (Join-Path $PSScriptRoot 'Sign-WindowsCandidate.ps1') -Directory $candidate -CertificateThumbprint $metadata.certificateThumbprint -CertificateSha256 $metadata.certificateSha256 -SignTool $SignTool
$privateDirectory=Join-Path $out ('private-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $privateDirectory | Out-Null
$acl=[Security.AccessControl.DirectorySecurity]::new();$acl.SetAccessRuleProtection($true,$false)
foreach($sid in @([Security.Principal.WindowsIdentity]::GetCurrent().User,[Security.Principal.SecurityIdentifier]::new('S-1-5-18'))){$acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($sid,'FullControl','ContainerInherit,ObjectInherit','None','Allow'))}
Set-Acl -LiteralPath $privateDirectory -AclObject $acl
$key=Join-Path $privateDirectory 'private.json';$bytes=$null
try{
 $bytes=[Security.Cryptography.ProtectedData]::Unprotect([IO.File]::ReadAllBytes((Join-Path $VaultDirectory 'update-private.json.dpapi')),$null,'CurrentUser')
 [IO.File]::WriteAllBytes($key,$bytes)
 & python (Join-Path $PSScriptRoot 'package-update.py') --payload $payload --launcher (Join-Path $native 'daymark-launcher.exe') --cli (Join-Path $native 'daymark-update-cli.exe') --updater (Join-Path $native 'daymark-updater.exe') --setup-helper (Join-Path $native 'daymark-setup-helper.exe') --key $key --output (Join-Path $out 'managed') --extract (Join-Path $out 'bundle') --version $version --source $source --runtime mingw131 --runtime-min bundled --min-windows-build 19045 --local-candidate --local-test-report $CTestReport --self-signed-cert-sha256 $metadata.certificateSha256
 if($LASTEXITCODE -ne 0){throw 'Signed managed candidate packaging failed.'}
}finally{
 if($null -ne $bytes){[Array]::Clear($bytes)}
 if(Test-Path -LiteralPath $key){Remove-Item -LiteralPath $key}
 Remove-Item -LiteralPath $privateDirectory
}
$installer=Join-Path $out "Daymark-$version-windows-x64-setup-local.exe"
& python (Join-Path $PSScriptRoot 'package-setup.py') --makensis $Makensis --bundle (Join-Path $out 'bundle') --output $installer --signtool $SignTool --certificate-thumbprint $metadata.certificateThumbprint --certificate-sha256 $metadata.certificateSha256
if($LASTEXITCODE -ne 0){throw 'Installer compilation/signing failed.'}
Write-Output "PASS: local signed installer candidate: $installer"
