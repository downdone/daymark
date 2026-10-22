param([ValidateSet('windows-debug', 'windows-release')][string]$Preset = 'windows-debug')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (!(Test-Path "$env:DAYMARK_QT/bin/Qt6Core.dll")) { throw 'Set DAYMARK_QT to the Qt 6.10.2 MinGW kit directory.' }
if (!(Test-Path "$env:DAYMARK_MINGW/bin/g++.exe")) { throw 'Set DAYMARK_MINGW to the matching MinGW directory.' }
$testExecutable = Join-Path $projectRoot "build/$Preset/bin/test_widget_window.exe"
$appExecutable = Join-Path $projectRoot "build/$Preset/bin/daymark.exe"
if (!(Test-Path $testExecutable) -or !(Test-Path $appExecutable)) { throw 'Build this preset first.' }
$reportPath = Join-Path $projectRoot "build/$Preset/native-widget.xml"
$originalPath = $env:PATH
$originalBackend = $env:QT_QPA_PLATFORM
try {
    $env:PATH = "$env:DAYMARK_MINGW/bin;$env:DAYMARK_QT/bin;$originalPath"
    $env:QT_QPA_PLATFORM = 'windows'
    $testProcess = Start-Process -FilePath $testExecutable -ArgumentList @('-o', "`"$reportPath,junitxml`"") -WorkingDirectory $projectRoot -WindowStyle Hidden -Wait -PassThru
    if ($testProcess.ExitCode -ne 0) { throw "Native window tests failed; inspect $reportPath" }
    $startupProcess = Start-Process -FilePath $appExecutable -ArgumentList '--smoke-test' -WorkingDirectory $projectRoot -WindowStyle Hidden -Wait -PassThru
    if ($startupProcess.ExitCode -ne 0) { throw 'Native QML startup failed.' }
    Write-Output 'PASS: native Qt window state and QML startup. Desktop stacking, focus and physical drag remain UNVERIFIED.'
} finally {
    $env:PATH = $originalPath
    $env:QT_QPA_PLATFORM = $originalBackend
}
