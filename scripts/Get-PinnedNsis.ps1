#requires -Version 7.0
param([Parameter(Mandatory)][string]$Directory)
$ErrorActionPreference='Stop'
$destination=[IO.Path]::GetFullPath($Directory)
if((Test-Path -LiteralPath $destination) -and @(Get-ChildItem -LiteralPath $destination -Force).Count){throw 'Use a new empty compiler staging directory; cached stubs/macros are not trusted.'}
New-Item -ItemType Directory -Path $destination -Force | Out-Null
$compiler=Join-Path $destination 'nsis-3.13/makensis.exe'
$archive=Join-Path $destination 'nsis-3.13.zip'
Invoke-WebRequest -Uri 'https://prdownloads.sourceforge.net/nsis/nsis-3.13.zip?download' -OutFile $archive -TimeoutSec 90
if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ine 'ba63dffc4410ee89193e1cb5a41989991bd77c61068da17e3156d136b7b0b3d8'){throw 'NSIS archive pin mismatch.'}
Expand-Archive -LiteralPath $archive -DestinationPath $destination
if((Get-FileHash -LiteralPath $compiler -Algorithm SHA256).Hash -ine '4ec390d0f96f9728507fbc2f4e91311dd90bfc6c8ddfe2159357b3651ae151c0'){throw 'NSIS compiler pin mismatch.'}
Write-Output $compiler
