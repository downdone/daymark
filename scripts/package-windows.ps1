param([string]$Version='0.1.0')
$ErrorActionPreference='Stop'
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw 'Version must be major.minor.patch.' }
$projectRoot=Split-Path $PSScriptRoot -Parent
if (!(Test-Path "$env:DAYMARK_QT/bin/windeployqt.exe")) { throw 'Set DAYMARK_QT to the matching Qt kit.' }
$executable=Join-Path $projectRoot 'build/windows-release/bin/daymark.exe'
if (!(Test-Path $executable)) { throw 'Build and test windows-release first.' }
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$packageRoot=Join-Path $projectRoot "dist/Daymark-$Version-windows-x64-local-$stamp"
if(Test-Path -LiteralPath $packageRoot) {throw 'Output directory already exists; no files were overwritten.'}
New-Item -ItemType Directory -Path $packageRoot | Out-Null
Copy-Item -LiteralPath $executable -Destination $packageRoot
$originalPath=$env:PATH
$originalBackend=$env:QT_QPA_PLATFORM
$qtSearchVariables=@('QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH')
$savedSearch=@{}
foreach($name in $qtSearchVariables){$savedSearch[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
    $env:PATH="$env:DAYMARK_MINGW/bin;$env:DAYMARK_QT/bin;$originalPath"
    & "$env:DAYMARK_QT/bin/windeployqt.exe" --release --qmldir "$projectRoot/src/ui" --dir $packageRoot --no-translations (Join-Path $packageRoot 'daymark.exe')
    if($LASTEXITCODE -ne 0) {throw 'windeployqt failed; package is incomplete.'}
    foreach($dll in @('libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll')) {
        $runtime=Join-Path $env:DAYMARK_MINGW "bin/$dll"
        if(!(Test-Path -LiteralPath $runtime)){throw "Missing compiler runtime: $dll"}
        Copy-Item -LiteralPath $runtime -Destination $packageRoot -Force
    }
    Copy-Item -LiteralPath (Join-Path $projectRoot 'docs') -Destination $packageRoot -Recurse
    @'
# Daymark v0.1.0 本地验证包

解压后双击 daymark.exe。默认数据保存在系统应用数据目录，包内没有个人任务数据库。

- [使用指南](docs/user-guide.md)
- [发行说明和未验证项](docs/release-notes-v0.1.md)
- [第三方组件/再分发边界](docs/third-party-notices.md)

本包只交付本地验证，未进行GitHub发布、macOS验收或签名。
'@ | Set-Content -LiteralPath (Join-Path $packageRoot 'README.md') -Encoding utf8
    $bomDirectory=Join-Path $packageRoot 'qt-sbom'
    New-Item -ItemType Directory -Path $bomDirectory | Out-Null
    foreach($module in @('qtbase','qtdeclarative','qtshadertools','qtsvg')) {
        $bom=Join-Path $env:DAYMARK_QT "sbom/$module-6.10.2.spdx.json"
        if(Test-Path -LiteralPath $bom){Copy-Item -LiteralPath $bom -Destination $bomDirectory}
    }
    # The deployed process receives only system paths; no developer Qt/MinGW PATH.
    @'
[Paths]
Prefix=.
Plugins=.
QmlImports=qml
'@ | Set-Content -LiteralPath (Join-Path $packageRoot 'qt.conf') -Encoding ascii
    $env:PATH="$env:SystemRoot/System32;$env:SystemRoot"
    foreach($name in $qtSearchVariables){[Environment]::SetEnvironmentVariable($name,$null,'Process')}
    $env:QT_QPA_PLATFORM='windows'
    $log=Join-Path $projectRoot 'build/package-smoke.log'
    $process=Start-Process -FilePath (Join-Path $packageRoot 'daymark.exe') -ArgumentList '--smoke-test' -WorkingDirectory $packageRoot -WindowStyle Hidden -PassThru -RedirectStandardError $log
    Start-Sleep -Milliseconds 500
    $loaded=@((Get-Process -Id $process.Id).Modules | Where-Object {$_.ModuleName -match '^(Qt6|libgcc_s|libstdc\+\+|libwinpthread)'})
    if($loaded.Count -eq 0){throw 'Could not verify deployed runtime modules.'}
    foreach($module in $loaded){if(!$module.FileName.StartsWith($packageRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw "Runtime loaded outside package: $($module.ModuleName)"}}
    if(!$process.WaitForExit(20000)){$process.Kill();throw 'Deployed smoke timed out.'}
    if($process.ExitCode -ne 0) {throw "Deployed smoke failed. Inspect $log"}
} finally {
    $env:PATH=$originalPath
    $env:QT_QPA_PLATFORM=$originalBackend
    foreach($name in $qtSearchVariables){[Environment]::SetEnvironmentVariable($name,$savedSearch[$name],'Process')}
}
$manifest=Get-ChildItem -LiteralPath $packageRoot -Recurse -File | ForEach-Object {
    [ordered]@{path=[System.IO.Path]::GetRelativePath($packageRoot,$_.FullName).Replace('\','/');size=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
}
[ordered]@{version=$Version;platform='windows-x64';scope='local verification';qt='6.10.2';files=@($manifest)} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $packageRoot 'manifest.json') -Encoding utf8
$archive="$packageRoot.zip"
Compress-Archive -LiteralPath $packageRoot -DestinationPath $archive -CompressionLevel Optimal
$hash=(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([System.IO.Path]::GetFileName($archive))" | Set-Content -LiteralPath "$archive.sha256" -Encoding ascii
Write-Output "PASS: deployed native smoke without SDK PATH. Local package: $archive"
