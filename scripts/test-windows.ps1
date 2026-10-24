param([ValidateSet('windows-debug', 'windows-release')][string]$Preset = 'windows-debug')
$ErrorActionPreference = 'Stop'
$originalFontDirectory=$env:QT_QPA_FONTDIR
if(!$originalFontDirectory){$env:QT_QPA_FONTDIR=Join-Path $env:SystemRoot 'Fonts'}
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    & ctest --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'CTest failed.' }
} finally { Pop-Location; $env:QT_QPA_FONTDIR=$originalFontDirectory }
