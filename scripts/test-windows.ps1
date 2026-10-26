param(
    [ValidateSet('windows-debug','windows-release')][string]$Preset='windows-debug',
    [ValidateSet('full','fast','ui')][string]$Suite='full'
)
$ErrorActionPreference = 'Stop'
$originalFontDirectory=$env:QT_QPA_FONTDIR
if(!$originalFontDirectory){$env:QT_QPA_FONTDIR=Join-Path $env:SystemRoot 'Fonts'}
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    $testArguments=@("--preset",$Preset)
    if($Suite -eq "fast"){$testArguments+=@("--label-regex","core|storage|integration|package")}
    elseif($Suite -eq "ui"){$testArguments+=@("--label-regex","^ui$")}
    & ctest @testArguments
    if ($LASTEXITCODE -ne 0) { throw 'CTest failed.' }
} finally { Pop-Location; $env:QT_QPA_FONTDIR=$originalFontDirectory }
