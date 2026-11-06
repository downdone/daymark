#requires -Version 7.0
Set-StrictMode -Version Latest

function Assert-DaymarkSelfSignedExecutable([string]$Path, [string]$CertificateSha256) {
    if (!$IsWindows -or $CertificateSha256 -cnotmatch '^[0-9a-f]{64}$') { throw 'Windows and an independent SHA256 certificate pin are required.' }
    if (!('Daymark.Signing.NativeTrust' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace Daymark.Signing {
    public static class NativeTrust {
        [StructLayout(LayoutKind.Sequential)] struct FileInfo {
            public uint size; public IntPtr path, file, subject;
        }
        [StructLayout(LayoutKind.Sequential)] struct TrustData {
            public uint size; public IntPtr callback, sip;
            public uint ui, revocation, choice; public IntPtr info;
            public uint action; public IntPtr state, url;
            public uint flags, context; public IntPtr signature;
        }
        [DllImport("wintrust.dll", ExactSpelling=true)]
        static extern int WinVerifyTrust(IntPtr window, ref Guid action, ref TrustData data);
        public static uint Verify(string path) {
            var name = Marshal.StringToCoTaskMemUni(path);
            var info = Marshal.AllocHGlobal(Marshal.SizeOf<FileInfo>());
            var data = new TrustData { size=(uint)Marshal.SizeOf<TrustData>(), ui=2,
                choice=1, info=info, action=1, flags=0x1010 };
            var action = new Guid("00AAC56B-CD44-11D0-8CC2-00C04FC295EE");
            try {
                Marshal.StructureToPtr(new FileInfo { size=(uint)Marshal.SizeOf<FileInfo>(), path=name }, info, false);
                return unchecked((uint)WinVerifyTrust(new IntPtr(-1), ref action, ref data));
            } finally {
                data.action=2; WinVerifyTrust(new IntPtr(-1), ref action, ref data);
                Marshal.FreeHGlobal(info); Marshal.FreeCoTaskMem(name);
            }
        }
    }
}
'@
    }
    $file = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if ($file.PSIsContainer -or $file.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'A regular executable is required.' }
    # Only this precise chain error is admissible. A bad digest, missing
    # signature, expiry, wrong usage, or provider failure is never ignored.
    $status = [Daymark.Signing.NativeTrust]::Verify($file.FullName)
    if ($status -notin @([uint32]0, [uint32]2148204809)) { throw ('Authenticode integrity verification failed: 0x{0:x8}' -f $status) }
    $signature = Get-AuthenticodeSignature -LiteralPath $file.FullName
    $certificate = $signature.SignerCertificate
    if ($null -eq $certificate -or $signature.SignatureType -ne 'Authenticode') { throw 'Embedded Authenticode signature is required.' }
    $pin = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($certificate.RawData)).ToLowerInvariant()
    if ($pin -cne $CertificateSha256 -or $certificate.Subject -cne $certificate.Issuer) { throw 'Pinned self-signed publisher certificate mismatch.' }
    $chain = [Security.Cryptography.X509Certificates.X509Chain]::new()
    try {
        $chain.ChainPolicy.TrustMode = 'CustomRootTrust'
        [void]$chain.ChainPolicy.CustomTrustStore.Add($certificate)
        $chain.ChainPolicy.RevocationMode = 'NoCheck'
        $chain.ChainPolicy.DisableCertificateDownloads = $true
        [void]$chain.ChainPolicy.ApplicationPolicy.Add([Security.Cryptography.Oid]::new('1.3.6.1.5.5.7.3.3'))
        if (!$chain.Build($certificate) -or $chain.ChainElements.Count -ne 1) { throw 'Pinned certificate validity or code-signing usage check failed.' }
    } finally { $chain.Dispose() }
    return [ordered]@{file=$file.Name;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant();
        policy='self-signed-pinned';certificateSha256=$pin;publicTrust=$false;
        winVerifyTrust=('0x{0:x8}' -f $status);rootStoreModified=$false}
}

Export-ModuleMember -Function Assert-DaymarkSelfSignedExecutable
