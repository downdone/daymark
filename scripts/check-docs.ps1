$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$files = @(Get-ChildItem -LiteralPath $projectRoot -Filter '*.md' -File)
$files += @(Get-ChildItem -LiteralPath (Join-Path $projectRoot 'docs') -Filter '*.md' -Recurse -File)
$broken = @()
foreach ($file in $files) {
    $content = Get-Content -LiteralPath $file.FullName -Raw
    foreach ($match in [regex]::Matches($content, '\[[^\]]*\]\(([^\s\)]+)\)')) {
        $target = $match.Groups[1].Value
        if ($target -match '^(https?://|mailto:|#)') { continue }
        $target = [uri]::UnescapeDataString(($target -split '#')[0])
        if (!(Test-Path -LiteralPath (Join-Path $file.DirectoryName $target))) {
            $broken += "$($file.Name): $target"
        }
    }
}
if ($broken.Count -gt 0) { throw ($broken -join "`n") }
Write-Output "PASS: checked local Markdown links in $($files.Count) files."
