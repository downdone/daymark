#requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$Directory,
    [Parameter(Mandatory)][string]$CertificateThumbprint,
    [Parameter(Mandatory)][string]$CertificateSha256,
    [Parameter(Mandatory)][string]$SignTool
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Authenticode.psm1') -Force
if ($CertificateThumbprint -cnotmatch '^[0-9A-Fa-f]{40}$') { throw 'A current-user signing certificate thumbprint is required.' }
$certificate = Get-Item -LiteralPath "Cert:\CurrentUser\My\$CertificateThumbprint"
$pin = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($certificate.RawData)).ToLowerInvariant()
if ($pin -cne $CertificateSha256 -or !$certificate.HasPrivateKey) { throw 'The signing certificate does not match the independent pin or lacks a private key.' }
$files = @('payload/daymark.exe','native/daymark-launcher.exe','native/daymark-update-cli.exe','native/daymark-updater.exe')
foreach ($relative in $files) {
    $file = Join-Path $Directory $relative
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Candidate executable missing: $relative" }
    if ((Get-AuthenticodeSignature -LiteralPath $file).Status -ne 'NotSigned') { throw 'Refusing to replace an existing executable signature.' }
}
$evidence = @()
foreach ($relative in $files) {
    $file = Join-Path $Directory $relative
    & $SignTool sign /fd SHA256 /s My /sha1 $CertificateThumbprint /d 'Daymark' $file
    if ($LASTEXITCODE -ne 0) { throw 'Authenticode signing failed.' }
    $evidence += Assert-DaymarkSelfSignedExecutable $file $CertificateSha256
}
$infoPath = Join-Path $Directory 'payload/BUILD-INFO.json'
$info = [IO.File]::ReadAllText($infoPath) | ConvertFrom-Json -AsHashtable
# Keep the legacy public-signing flag false; it must never imply public trust.
$info.signed = $false
$info.authenticode = [ordered]@{policy='self-signed-pinned';certificateSha256=$CertificateSha256;publicTrust=$false;files=$evidence}
$info | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $infoPath -Encoding utf8
Write-Output 'PASS: four Daymark executables signed and verified with a pinned self-signed certificate; Windows public trust remains false.'
