param([ValidateSet('windows-debug', 'windows-release')][string]$Preset = 'windows-debug')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$executable = Join-Path $projectRoot "build/$Preset/bin/daymark.exe"
if (!(Test-Path $executable)) { throw 'Build Daymark first.' }
if (!(Test-Path "$env:DAYMARK_QT/bin/Qt6Core.dll")) { throw 'Set DAYMARK_QT to the Qt kit directory.' }
$originalPath = $env:PATH
try {
    $env:PATH = "$env:DAYMARK_MINGW/bin;$env:DAYMARK_QT/bin;$originalPath"
    Start-Process -FilePath $executable -WorkingDirectory $projectRoot -WindowStyle Hidden
} finally { $env:PATH = $originalPath }
