#requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$CertificateSha256,
    [Parameter(Mandatory)][string]$Payload,
    [Parameter(Mandatory)][string]$Launcher,
    [Parameter(Mandatory)][string]$Cli,
    [Parameter(Mandatory)][string]$Updater
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Authenticode.psm1') -Force
$Files = @($Payload,$Launcher,$Cli,$Updater)
$evidence = @(foreach ($file in $Files) { Assert-DaymarkSelfSignedExecutable $file $CertificateSha256 })
$evidence | ConvertTo-Json -Depth 4
