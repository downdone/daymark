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
    [string]$SourceDirectory=(Split-Path $PSScriptRoot -Parent)
)
$ErrorActionPreference = 'Stop'

$binary = Join-Path $BuildDirectory 'bin/daymark.exe'
if (!(Test-Path -LiteralPath $binary)) { throw "Release binary missing: $binary" }
if (!(Test-Path -LiteralPath "$env:DAYMARK_QT/bin/windeployqt.exe")) { throw 'windeployqt missing; set DAYMARK_QT to the Qt kit.' }

$stage = Join-Path $OutputDirectory "Daymark-$Version-windows-x64"
if (Test-Path -LiteralPath $stage) { throw 'Output already exists; nothing was overwritten.' }
New-Item -ItemType Directory -Path $stage -Force | Out-Null

Copy-Item -LiteralPath $binary -Destination $stage

$originalPath = $env:PATH
try {
    $env:PATH = "$env:DAYMARK_QT/bin;$originalPath"
    & "$env:DAYMARK_QT/bin/windeployqt.exe" --release --qmldir (Join-Path $SourceDirectory 'src/ui') `
        --dir $stage --no-translations --skip-plugin-types 'qmltooling,generic' `
        --exclude-plugins 'qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql' (Join-Path $stage 'daymark.exe')
    if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed; the archive would be incomplete.' }
} finally { $env:PATH = $originalPath }

# MSVC runtime ships with the vc_redist; do not assume it is present on the
# target machine, so state it rather than copying DLLs we cannot verify.
Copy-Item -LiteralPath (Join-Path $SourceDirectory 'LICENSE') -Destination $stage
Copy-Item -LiteralPath (Join-Path $SourceDirectory 'packaging/THIRD-PARTY-NOTICES.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $SourceDirectory 'LICENSE') -Destination (Join-Path $stage 'LGPL-3.0.txt')
Copy-Item -LiteralPath (Join-Path $SourceDirectory 'packaging/licenses/GPL-3.0.txt') -Destination $stage
$bomDirectory = Join-Path $stage 'qt-sbom'
& python (Join-Path $PSScriptRoot 'copy-qt-sbom.py') --qt-root $env:DAYMARK_QT --destination $bomDirectory --version 6.10.2
if ($LASTEXITCODE -ne 0) { throw 'Qt module SBOM validation failed; archive was not created.' }
@'
Daymark - tagged build artifact

Requires the Microsoft Visual C++ Redistributable 2015-2022 (x64).
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
    signed         = $false
    cleanMachine   = $false
    sourceRevision = (& git -C $SourceDirectory rev-parse HEAD).Trim()
    fileCount      = $files.Count
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stage 'BUILD-INFO.json') -Encoding utf8

$archive = "$stage.zip"
Compress-Archive -LiteralPath $stage -DestinationPath $archive -CompressionLevel Optimal
$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([IO.File]::GetFileName($archive))" | Set-Content -LiteralPath "$archive.sha256" -Encoding ascii
Write-Output "PASS: $archive ($($files.Count) files)"