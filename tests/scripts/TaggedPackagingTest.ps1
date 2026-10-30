$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $root 'scripts/package-tagged-windows.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Packager parse failed'}
$function=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Get-DaymarkReleaseExecutable'},$true)
if(!$function){throw 'Release executable resolver missing'}
# Load only the real pure resolver, without deploying or running any executable.
. ([scriptblock]::Create($function.Extent.Text))
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('daymark-tagged-layout-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
try {
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
    'PASS: MSVC/Ninja Release layouts; reject missing, Debug-only and ambiguous output'
} finally {
    $resolved=[IO.Path]::GetFullPath($fixture)
    $tempRoot=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
    if(!$resolved.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase) -or !(Split-Path $resolved -Leaf).StartsWith('daymark-tagged-layout-')){throw 'Unsafe fixture cleanup path'}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
