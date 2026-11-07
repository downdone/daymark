#requires -Version 7.0
param([Parameter(Mandatory)][string]$Directory)
$ErrorActionPreference='Stop'
$destination=[IO.Path]::GetFullPath($Directory)
if((Test-Path -LiteralPath $destination) -and @(Get-ChildItem -LiteralPath $destination -Force).Count){throw 'Use a new empty compiler staging directory; cached stubs/macros are not trusted.'}
New-Item -ItemType Directory -Path $destination -Force | Out-Null
$compiler=Join-Path $destination 'nsis-3.13/makensis.exe'
$archive=Join-Path $destination 'nsis-3.13.zip'
& curl.exe --fail --silent --show-error --location --proto '=https' --proto-redir '=https' --connect-timeout 15 --max-time 90 --output $archive 'https://downloads.sourceforge.net/project/nsis/NSIS%203/3.13/nsis-3.13.zip'
if($LASTEXITCODE -ne 0){throw 'NSIS HTTPS download failed.'}
$header=[IO.File]::ReadAllBytes($archive)
if($header.Length -lt 4 -or $header[0] -ne 0x50 -or $header[1] -ne 0x4b -or $header[2] -ne 3 -or $header[3] -ne 4){throw 'NSIS download is not a ZIP archive (a download page or proxy response may have been returned).'}
if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ine 'ba63dffc4410ee89193e1cb5a41989991bd77c61068da17e3156d136b7b0b3d8'){throw 'NSIS archive pin mismatch.'}
Expand-Archive -LiteralPath $archive -DestinationPath $destination
if((Get-FileHash -LiteralPath $compiler -Algorithm SHA256).Hash -ine '4ec390d0f96f9728507fbc2f4e91311dd90bfc6c8ddfe2159357b3651ae151c0'){throw 'NSIS compiler pin mismatch.'}
Write-Output $compiler
