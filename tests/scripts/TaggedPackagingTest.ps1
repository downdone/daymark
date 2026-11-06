$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Import-Module (Join-Path $root 'scripts/MsvcRuntime.psm1') -Force
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $root 'scripts/package-tagged-windows.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Packager parse failed'}
$function=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Get-DaymarkReleaseExecutable'},$true)
if(!$function){throw 'Release executable resolver missing'}
# Load only the real pure resolver, without deploying or running any executable.
. ([scriptblock]::Create($function.Extent.Text))
$checksum=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Write-DaymarkArchiveChecksum'},$true)
if(!$checksum){throw 'Archive checksum writer missing'}
. ([scriptblock]::Create($checksum.Extent.Text))
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('daymark-tagged-layout-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
try {
    $redist = Join-Path $fixture 'VS/VC/Redist/MSVC'
    foreach ($version in @('14.9.1','14.39.33519','14.44.35211','14.52.37025')) {
        $family = if ($version -eq '14.52.37025') {'Microsoft.VC145.CRT'} else {'Microsoft.VC143.CRT'}
        New-Item -ItemType Directory -Path (Join-Path $redist "$version/x64/$family") -Force | Out-Null
    }
    $expected = Join-Path $redist '14.44.35211/x64/Microsoft.VC143.CRT'
    if ((Get-DaymarkMsvcRedistDirectory $redist) -cne $expected) { throw 'Wrong CRT family/version selected.' }
    foreach ($invalid in @($fixture, (Join-Path $redist '14.44.35211/x64'), (Join-Path $fixture 'empty/VC/Redist/MSVC'))) {
        New-Item -ItemType Directory -Path $invalid -Force | Out-Null
        $rejected = $false
        try { $null = Get-DaymarkMsvcRedistDirectory $invalid } catch { $rejected = $true }
        if (!$rejected) { throw 'Invalid or empty redistributable source was admitted.' }
    }
    'PASS: app-local CRT source selection rejects non-redist sources and unsupported compiler families'
    foreach($layout in @('bin/Release/daymark.exe','bin/daymark.exe')) {
        $build=Join-Path $fixture ([guid]::NewGuid().ToString('N'))
        $exe=Join-Path $build $layout
        New-Item -ItemType Directory -Path (Split-Path $exe) -Force | Out-Null
        [IO.File]::WriteAllText($exe,'synthetic fixture; never executed')
        if((Get-DaymarkReleaseExecutable $build) -cne $exe){throw 'Wrong Release output selected'}
    }
    foreach($layouts in @(@(),@('bin/Debug/daymark.exe'),@('bin/Release/daymark.exe','bin/daymark.exe'))) {
        $build=Join-Path $fixture ([guid]::NewGuid().ToString('N'))
        foreach($layout in $layouts) {
            $exe=Join-Path $build $layout
            New-Item -ItemType Directory -Path (Split-Path $exe) -Force | Out-Null
            [IO.File]::WriteAllText($exe,'synthetic fixture; never executed')
        }
        $rejected=$false
        try {$null=Get-DaymarkReleaseExecutable $build} catch {$rejected=$true}
        if(!$rejected){throw 'Missing, Debug-only or ambiguous output was admitted'}
    }
    $archive=Join-Path $fixture 'synthetic.zip'
    [IO.File]::WriteAllBytes($archive,[byte[]](0,1,2,3))
    Write-DaymarkArchiveChecksum $archive
    $text=[IO.File]::ReadAllText("$archive.sha256")
    if($text.Contains("`r") -or !$text.EndsWith("  synthetic.zip`n") -or $text.Substring(0,64) -cne (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()){throw 'Checksum must preserve the archive hash and portable LF filename'}
    'PASS: MSVC/Ninja Release layouts; reject missing, Debug-only and ambiguous output'
    'PASS: checksum basename, SHA256 and LF on Windows'
} finally {
    $resolved=[IO.Path]::GetFullPath($fixture)
    $tempRoot=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
    if(!$resolved.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase) -or !(Split-Path $resolved -Leaf).StartsWith('daymark-tagged-layout-')){throw 'Unsafe fixture cleanup path'}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
