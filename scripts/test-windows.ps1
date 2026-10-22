param([ValidateSet('windows-debug', 'windows-release')][string]$Preset = 'windows-debug')
$ErrorActionPreference = 'Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    & ctest --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'CTest failed.' }
} finally { Pop-Location }
