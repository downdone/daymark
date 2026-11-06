#requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$CandidateDirectory,
    [Parameter(Mandatory)][string]$UnsignedExecutable,
    [Parameter(Mandatory)][string]$CertificateSha256
)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Import-Module (Join-Path $root 'scripts/Authenticode.psm1') -Force
function Read-RootIdentities {
    return (@(foreach ($store in @('Cert:\CurrentUser\Root','Cert:\LocalMachine\Root')) {
        foreach ($cert in Get-ChildItem $store) { $store + ':' + $cert.Thumbprint }
    }) | Sort-Object) -join "`n"
}
$rootsBefore = Read-RootIdentities
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('daymark-signing-check-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
try {
    foreach ($relative in @('payload/daymark.exe','native/daymark-launcher.exe','native/daymark-update-cli.exe','native/daymark-updater.exe')) {
        $record = @(Assert-DaymarkSelfSignedExecutable (Join-Path $CandidateDirectory $relative) $CertificateSha256)
        if ($record.Count -ne 1 -or $record[0].policy -cne 'self-signed-pinned') { throw 'Verifier must return exactly one evidence record.' }
    }
    $signed = Join-Path $CandidateDirectory 'native/daymark-update-cli.exe'
    $rejected = $false
    try { $null = Assert-DaymarkSelfSignedExecutable $signed ('0' * 64) } catch { $rejected = $true }
    if (!$rejected) { throw 'Wrong publisher pin was accepted.' }
    $unsigned = Join-Path $fixture 'unsigned.exe'
    Copy-Item -LiteralPath $UnsignedExecutable -Destination $unsigned
    $rejected = $false
    try { $null = Assert-DaymarkSelfSignedExecutable $unsigned $CertificateSha256 } catch { $rejected = $true }
    if (!$rejected) { throw 'An unsigned executable was accepted.' }
    $tampered = Join-Path $fixture 'tampered.exe'
    $bytes = [IO.File]::ReadAllBytes($signed)
    $pe = [BitConverter]::ToInt32($bytes, 0x3c)
    $firstSection = $pe + 24 + [BitConverter]::ToUInt16($bytes, $pe + 20)
    $offset = [BitConverter]::ToUInt32($bytes, $firstSection + 20) + 16
    $bytes[$offset] = $bytes[$offset] -bxor 1
    [IO.File]::WriteAllBytes($tampered, $bytes)
    $tamperStatus = [Daymark.Signing.NativeTrust]::Verify($tampered)
    if ($tamperStatus -ne [uint32]2148098064) { throw ('Expected TRUST_E_BAD_DIGEST, got 0x{0:x8}' -f $tamperStatus) }
    $rejected = $false
    try { $null = Assert-DaymarkSelfSignedExecutable $tampered $CertificateSha256 } catch { $rejected = $true }
    if (!$rejected) { throw 'A modified executable was accepted.' }
    $output = & $signed capabilities | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0 -or !$output.trust_configured) { throw 'Signed native CLI did not run with compiled public trust.' }
    if ((Read-RootIdentities) -cne $rootsBefore) { throw 'Root certificate stores changed during verification.' }
    [ordered]@{passed=$true;executableSignatures=4;wrongPinRejected=$true;unsignedRejected=$true;
        tamperingRejected=$true;tamperStatus=('0x{0:x8}' -f $tamperStatus);compiledUpdateTrust=$true;
        rootStoresUnchanged=$true;publicTrust=$false} | ConvertTo-Json
} finally {
    $resolved = [IO.Path]::GetFullPath($fixture)
    $tempPrefix = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if (!$resolved.StartsWith($tempPrefix,[StringComparison]::OrdinalIgnoreCase) -or !(Split-Path $resolved -Leaf).StartsWith('daymark-signing-check-')) { throw 'Unsafe signing test cleanup path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
