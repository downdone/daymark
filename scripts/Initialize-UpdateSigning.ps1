#requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$Directory,
    [Parameter(Mandatory)][string]$UpdateCli
)
$ErrorActionPreference = 'Stop'
if (!$IsWindows) { throw 'This signing vault uses Windows CurrentUser DPAPI.' }
if (Test-Path -LiteralPath $Directory) { throw 'Signing vault already exists; nothing was overwritten.' }
$vault = [IO.Directory]::CreateDirectory([IO.Path]::GetFullPath($Directory)).FullName
$identity = [Security.Principal.WindowsIdentity]::GetCurrent().User
$acl = [Security.AccessControl.DirectorySecurity]::new()
$acl.SetOwner($identity)
$acl.SetAccessRuleProtection($true, $false)
foreach ($sid in @($identity, [Security.Principal.SecurityIdentifier]::new('S-1-5-18'))) {
    $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($sid, 'FullControl', 'ContainerInherit,ObjectInherit', 'None', 'Allow'))
}
Set-Acl -LiteralPath $vault -AclObject $acl
$certificate = $null
$sealed = $false
try {
    & $UpdateCli keygen $vault
    if ($LASTEXITCODE -ne 0) { throw 'Update signing key generation failed.' }
    $privateFile = Join-Path $vault 'private.json'
    $privateBytes = [IO.File]::ReadAllBytes($privateFile)
    try {
        $encrypted = [Security.Cryptography.ProtectedData]::Protect($privateBytes, $null, 'CurrentUser')
        [IO.File]::WriteAllBytes((Join-Path $vault 'update-private.json.dpapi'), $encrypted)
    } finally { [Array]::Clear($privateBytes) }
    $certificate = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=Daymark Self-Signed Publisher' `
        -FriendlyName 'Daymark self-signed release signing' -CertStoreLocation 'Cert:\CurrentUser\My' `
        -KeyAlgorithm RSA -KeyLength 3072 -HashAlgorithm SHA256 -KeyExportPolicy Exportable `
        -NotAfter (Get-Date).AddYears(2)
    $password = [Convert]::ToBase64String([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    $pfx = $certificate.Export([Security.Cryptography.X509Certificates.X509ContentType]::Pfx, $password)
    $passwordBytes = [Text.Encoding]::UTF8.GetBytes($password)
    try {
        [IO.File]::WriteAllBytes((Join-Path $vault 'authenticode.pfx.dpapi'), [Security.Cryptography.ProtectedData]::Protect($pfx, $null, 'CurrentUser'))
        [IO.File]::WriteAllBytes((Join-Path $vault 'authenticode-password.dpapi'), [Security.Cryptography.ProtectedData]::Protect($passwordBytes, $null, 'CurrentUser'))
    } finally { [Array]::Clear($pfx); [Array]::Clear($passwordBytes); $password = $null }
    $public = [IO.File]::ReadAllText((Join-Path $vault 'public.json')) | ConvertFrom-Json
    $metadata = [ordered]@{format=1;updateKeyId=$public.key_id;authenticodePolicy='self-signed-pinned';
        certificateSha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($certificate.RawData)).ToLowerInvariant();
        certificateThumbprint=$certificate.Thumbprint;expiresUtc=$certificate.NotAfter.ToUniversalTime().ToString('o');
        publicTrust=$false;rootStoreModified=$false;keyProtection='CurrentUser DPAPI plus owner/SYSTEM-only ACL'}
    $metadata | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $vault 'metadata.json') -Encoding utf8
    $sealed = $true
    $metadata | ConvertTo-Json -Compress
} finally {
    # The signer needs a transient JSON, but the persisted local vault must not.
    $plain = Join-Path $vault 'private.json'
    if (Test-Path -LiteralPath $plain) { Remove-Item -LiteralPath $plain -Force }
    if (!$sealed -and $null -ne $certificate) { Remove-Item -LiteralPath "Cert:\CurrentUser\My\$($certificate.Thumbprint)" }
}
