#requires -Version 7.0
param([string]$Version='')
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
& (Join-Path $PSScriptRoot 'test-windows.ps1') -Preset windows-release
$executable=Join-Path $projectRoot 'build/windows-release/bin/daymark.exe'
if(!(Test-Path -LiteralPath $executable)){throw 'Build and test windows-release first.'}
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$packageRoot=Join-Path $projectRoot "dist/Daymark-$Version-windows-x64-local-$stamp"
if(Test-Path -LiteralPath $packageRoot){throw 'Output already exists; no files were overwritten.'}
New-Item -ItemType Directory -Path $packageRoot | Out-Null
Copy-Item -LiteralPath $executable -Destination $packageRoot
$originalPath=$env:PATH
try {
    $env:PATH="$env:DAYMARK_MINGW/bin;$env:DAYMARK_QT/bin;$originalPath"
    & "$env:DAYMARK_QT/bin/windeployqt.exe" --release --qmldir "$projectRoot/src/ui" --dir $packageRoot --no-translations (Join-Path $packageRoot 'daymark.exe')
    if($LASTEXITCODE -ne 0){throw 'windeployqt failed; package incomplete.'}
    foreach($dll in @('libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
        $runtime=Join-Path $env:DAYMARK_MINGW "bin/$dll"
        if(!(Test-Path -LiteralPath $runtime)){throw "Compiler runtime missing: $dll"}
        Copy-Item -LiteralPath $runtime -Destination $packageRoot
    }
} finally {$env:PATH=$originalPath}
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs') -Destination $packageRoot -Recurse
$verification=Join-Path $packageRoot 'verification'
New-Item -ItemType Directory -Path $verification | Out-Null
foreach($script in @('Test-Package.ps1','PackageValidation.psm1')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $script) -Destination $verification}
@"
# Daymark $Version Windows本地候选包

解压后双击daymark.exe。默认数据位于系统应用数据目录，包内没有个人任务数据库。

- [使用指南](docs/user-guide.md)
- [发行说明/未验证项](docs/release-notes-v1-rc2.md)
- [离线校验及升级](docs/delivery-guide.md)
- [第三方组件/再分发边界](docs/third-party-notices.md)

可用PowerShell 7执行 verification/Test-Package.ps1 校验完整性及无SDK运行；报告位于包外临时目录。
BUILD-INFO.json记录构建元数据，manifest.json提供逐文件校验。校验和不是可信签名。
本包只用于本地验证，未进行GitHub发布、macOS/独立干净机验收或签名，公开再分发许可未核实。
"@ | Set-Content -LiteralPath (Join-Path $packageRoot 'README.md') -Encoding utf8
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
[ordered]@{version=$Version;sourceRevision=$sourceRevision;qt='6.10.2';schemaVersion=3;scope='Windows local release candidate';signed=$false} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $packageRoot 'BUILD-INFO.json') -Encoding utf8
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
