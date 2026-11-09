#requires -Version 7.0
<#
.SYNOPSIS
Build a distributable Windows archive from an already built MSVC Release tree.

.DESCRIPTION
Used by the tagged-release workflow, where Qt is installed by
install-qt-action (open-source LGPLv3 archives) and the compiler is MSVC.
scripts/package-windows.ps1 targets the MinGW preset and performs the strict
local-preview payload audit; this script only produces the tagged artifact and
records what it contains.

The archive is explicitly unsigned and not clean-machine validated. It exists
so a tag produces runnable binaries alongside the source; see release-policy.md.
#>
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][string]$Version,
    [string]$SourceDirectory=(Split-Path $PSScriptRoot -Parent),
    [switch]$PayloadOnly
)
$ErrorActionPreference = 'Stop'

function Get-DaymarkReleaseExecutable([string]$BuildRoot) {
    # MSVC/Visual Studio is multi-config; Ninja/MinGW is single-config.
    $candidates = @('bin/Release/daymark.exe','bin/daymark.exe') |
        ForEach-Object { Join-Path $BuildRoot $_ } |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf }
    if (@($candidates).Count -ne 1) { throw 'Exactly one Release executable is required; missing or ambiguous build output.' }
    return $candidates
}
function Write-DaymarkArchiveChecksum([string]$Archive) {
    $hash = (Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash.ToLowerInvariant()
    $name = [IO.Path]::GetFileName($Archive)
    [IO.File]::WriteAllText("$Archive.sha256", "$hash  $name`n", [Text.Encoding]::ASCII)
}
$binary = Get-DaymarkReleaseExecutable $BuildDirectory
if (!(Test-Path -LiteralPath "$env:DAYMARK_QT/bin/windeployqt.exe")) { throw 'windeployqt missing; set DAYMARK_QT to the Qt kit.' }

$stage = Join-Path $OutputDirectory "Daymark-$Version-windows-x64"
if (Test-Path -LiteralPath $stage) { throw 'Output already exists; nothing was overwritten.' }
New-Item -ItemType Directory -Path $stage -Force | Out-Null

Copy-Item -LiteralPath $binary -Destination $stage

$originalPath = $env:PATH
try {
    $env:PATH = "$env:DAYMARK_QT/bin;$originalPath"
    & "$env:DAYMARK_QT/bin/windeployqt.exe" --release --no-compiler-runtime --qmldir (Join-Path $SourceDirectory 'src/ui') `
        --dir $stage --no-translations --skip-plugin-types 'qmltooling,generic' `
        --exclude-plugins 'qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql' (Join-Path $stage 'daymark.exe')
    if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed; the archive would be incomplete.' }
} finally { $env:PATH = $originalPath }

# Self-contained updates cannot depend on installing vc_redist on the target.
# Use the licensed VS redistributable tree, never developer bin/System32 DLLs.
Import-Module (Join-Path $PSScriptRoot 'MsvcRuntime.psm1') -Force
$runtime = @(Copy-DaymarkMsvcRuntime $stage)
Copy-Item -LiteralPath (Join-Path $SourceDirectory 'LICENSE') -Destination $stage
Copy-Item -LiteralPath (Join-Path $SourceDirectory 'packaging/THIRD-PARTY-NOTICES.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $SourceDirectory 'LICENSE') -Destination (Join-Path $stage 'LGPL-3.0.txt')
Copy-Item -LiteralPath (Join-Path $SourceDirectory 'packaging/licenses/GPL-3.0.txt') -Destination $stage
New-Item -ItemType Directory -Path (Join-Path $stage 'licenses') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $SourceDirectory 'packaging/licenses/NSIS-COPYING.txt') -Destination (Join-Path $stage 'licenses/NSIS-COPYING.txt')
$bomDirectory = Join-Path $stage 'qt-sbom'
& python (Join-Path $PSScriptRoot 'copy-qt-sbom.py') --qt-root $env:DAYMARK_QT --destination $bomDirectory --version 6.10.2
if ($LASTEXITCODE -ne 0) { throw 'Qt module SBOM validation failed; archive was not created.' }
@'
Daymark - tagged build artifact

Includes the Microsoft Visual C++ x64 release runtime for app-local use.
Microsoft runtime files remain subject to the Visual Studio distribution terms.
Qt is licensed under LGPLv3 and linked dynamically; you may replace the Qt
DLLs with a compatible build. See THIRD-PARTY-NOTICES.md.

This archive is not code-signed and has not been validated on a clean machine.
'@ | Set-Content -LiteralPath (Join-Path $stage 'README.txt') -Encoding utf8

$files = Get-ChildItem -LiteralPath $stage -Recurse -File
[ordered]@{
    version        = $Version
    platform       = 'windows-x64'
    qt             = '6.10.2'
    compiler       = 'MSVC 2022'
    runtime        = 'msvc143'
    runtimeMinimum = 'bundled'
    runtimeFiles   = $runtime
    signed         = $false
    cleanMachine   = $false
    sourceRevision = (& git -C $SourceDirectory rev-parse HEAD).Trim()
    fileCount      = $files.Count
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'BUILD-INFO.json') -Encoding utf8

if ($PayloadOnly) {
    Write-Output "PASS: $stage ($($files.Count) files)"
    return
}
$archive = "$stage.zip"
Compress-Archive -LiteralPath $stage -DestinationPath $archive -CompressionLevel Optimal
Write-DaymarkArchiveChecksum $archive
Write-Output "PASS: $archive ($($files.Count) files)"
