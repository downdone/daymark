#requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$WorkDirectory,
    [Parameter(Mandatory)][string]$SigningDirectory,
    [Parameter(Mandatory)][string]$CandidateDirectory,
    [Parameter(Mandatory)][string]$RuntimeDirectory,
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (Test-Path -LiteralPath $WorkDirectory) { throw 'Test evidence directory exists; nothing was overwritten.' }
$work = [IO.Directory]::CreateDirectory([IO.Path]::GetFullPath($WorkDirectory)).FullName
$vault = (Get-Item -LiteralPath $SigningDirectory).FullName
if (!(Get-Acl -LiteralPath $vault).AreAccessRulesProtected) { throw 'Expected a protected signing vault.' }
$scratch = Join-Path $vault ('operation-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scratch | Out-Null
$key = Join-Path $scratch 'private.json'
$privateBytes = $null
function Invoke-Seed([string]$Directory, [string]$ExpectedError = '') {
    $text = & (Join-Path $CandidateDirectory 'native/daymark-update-cli.exe') initialize --root $Directory --json
    $code = $LASTEXITCODE
    $result = $text | ConvertFrom-Json
    if ($ExpectedError) {
        if ($code -eq 0 -or $result.error -cne $ExpectedError -or (Test-Path -LiteralPath (Join-Path $Directory 'current.json'))) {
            throw 'Untrusted or modified bootstrap was not rejected without a current pointer.'
        }
    } elseif ($code -ne 0 -or $result.version -cne '2.0.0') { throw 'Compiled production trust rejected the owned-key bootstrap.' }
    return $result
}
try {
    $privateBytes = [Security.Cryptography.ProtectedData]::Unprotect([IO.File]::ReadAllBytes((Join-Path $vault 'update-private.json.dpapi')), $null, 'CurrentUser')
    [IO.File]::WriteAllBytes($key, $privateBytes)
    $payload = Join-Path $work 'payload'
    New-Item -ItemType Directory -Path $payload | Out-Null
    foreach ($item in Get-ChildItem -LiteralPath $RuntimeDirectory) {
        if ($item.Name -in @('daymark.exe','BUILD-INFO.json','manifest.json','TEST-RESULTS.json')) { continue }
        Copy-Item -LiteralPath $item.FullName -Destination $payload -Recurse
    }
    foreach ($name in @('daymark.exe','BUILD-INFO.json')) {
        Copy-Item -LiteralPath (Join-Path $CandidateDirectory "payload/$name") -Destination $payload
    }
    $root = Join-Path $work 'installation'
    $output = Join-Path $work 'package'
    $source = (& git -C $repo rev-parse HEAD).Trim()
    $cli = Join-Path $CandidateDirectory 'native/daymark-update-cli.exe'
    & $Python (Join-Path $repo 'scripts/package-update.py') --payload $payload `
        --launcher (Join-Path $CandidateDirectory 'native/daymark-launcher.exe') `
        --updater (Join-Path $CandidateDirectory 'native/daymark-updater.exe') --cli $cli --key $key `
        --output $output --version 2.0.0 --source $source --runtime mingw131 --runtime-min bundled `
        --min-windows-build 19045 --local-fixture --release-id 123 --asset-id 456 --extract $root *> (Join-Path $work 'package.log')
    if ($LASTEXITCODE -ne 0) { throw 'Owned-key local verification package creation failed; see package.log.' }
    $bad = Join-Path $work 'bad-signature'
    New-Item -ItemType Directory -Path $bad | Out-Null
    $manifest = Join-Path $bad 'bootstrap-release.json'
    Copy-Item -LiteralPath (Join-Path $root 'bootstrap-release.json') -Destination $manifest
    Copy-Item -LiteralPath (Join-Path $root 'bootstrap-release.json.sig') -Destination "$manifest.sig"
    [IO.File]::AppendAllText($manifest, ' ')
    $null = Invoke-Seed $bad 'update.signature_invalid'
    Copy-Item -LiteralPath (Join-Path $root 'bootstrap-release.json') -Destination $manifest -Force
    & $cli sign $key $manifest Daymark.UpdateManifest.v1
    if ($LASTEXITCODE -ne 0) { throw 'Cross-domain test signing failed.' }
    $null = Invoke-Seed $bad 'update.signature_invalid'
    $wrongKey = Join-Path $scratch 'wrong-key'
    $null = & $cli keygen $wrongKey
    if ($LASTEXITCODE -ne 0) { throw 'Wrong-key fixture generation failed.' }
    & $cli sign (Join-Path $wrongKey 'private.json') $manifest Daymark.BootstrapPayload.v1
    if ($LASTEXITCODE -ne 0) { throw 'Wrong-key fixture signing failed.' }
    $null = Invoke-Seed $bad 'update.signature_invalid'
    $current = Invoke-Seed $root
    $exe = Join-Path $root "$($current.current)/daymark.exe"
    $names = @('PATH','QT_QPA_PLATFORM','QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','QT_CONF_PATH')
    $saved = @{}; foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
    $process = $null
    try {
        foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
        $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
        $env:QT_QPA_PLATFORM = 'windows'
        $process = Start-Process -FilePath $exe -ArgumentList '--update-preflight' -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput (Join-Path $work 'preflight-output.txt') -RedirectStandardError (Join-Path $work 'preflight.log')
        if (!$process.WaitForExit(30000)) { throw 'Signed Qt preflight timed out.' }
        if ($process.ExitCode -ne 0) { throw 'Signed Qt preflight failed.' }
    } finally {
        if ($null -ne $process -and !$process.HasExited) { $process.Kill(); $process.WaitForExit() }
        foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name,$saved[$name],'Process') }
    }
    # This is an exclusively owned synthetic installation, never a user root.
    Remove-Item -LiteralPath (Join-Path $root 'current.json')
    $original = [IO.File]::ReadAllBytes($exe)
    try {
        $changed = [byte[]]$original.Clone(); $changed[1024] = $changed[1024] -bxor 1
        [IO.File]::WriteAllBytes($exe, $changed)
        $null = Invoke-Seed $root 'update.hash_mismatch'
    } finally { [IO.File]::WriteAllBytes($exe,$original) }
    $null = Invoke-Seed $root
    [ordered]@{passed=$true;keySource='owned DPAPI-protected key';compiledProductionTrust=$true;
        manifestTamperRejected=$true;wrongSigningDomainRejected=$true;wrongKeyRejected=$true;payloadTamperRejected=$true;
        signedQtPreflight=$true;privateKeyDistributed=$false;formalRelease=$false;publicGithubTest=$false} |
        ConvertTo-Json | Tee-Object -FilePath (Join-Path $work 'report.json')
} finally {
    if ($null -ne $privateBytes) { [Array]::Clear($privateBytes) }
    $resolved = [IO.Path]::GetFullPath($scratch)
    if ((Split-Path $resolved -Parent) -ine $vault -or !(Split-Path $resolved -Leaf).StartsWith('operation-')) { throw 'Unsafe private operation cleanup path.' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
