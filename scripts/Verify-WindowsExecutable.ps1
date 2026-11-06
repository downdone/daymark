#requires -Version 7.0
param(
 [Parameter(Mandatory)][string]$Executable,
 [Parameter(Mandatory)][string]$CertificateSha256
)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'Authenticode.psm1') -Force
Assert-DaymarkSelfSignedExecutable $Executable $CertificateSha256 | ConvertTo-Json -Depth 4
