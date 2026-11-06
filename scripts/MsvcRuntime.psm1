#requires -Version 7.0
Set-StrictMode -Version Latest

function Get-DaymarkMsvcRedistDirectory([string]$RedistRoot) {
    # Only the VS 2022 redistributable tree is an admissible source. In
    # particular, never copy runtime DLLs out of System32 or VC/Tools/bin.
    $root = (Get-Item -LiteralPath $RedistRoot -Force -ErrorAction Stop).FullName
    if ($root.Replace('\','/').TrimEnd('/') -notmatch '/VC/Redist/MSVC$') {
        throw 'Expected the Visual Studio VC/Redist/MSVC directory.'
    }
    $ancestor = Get-Item -LiteralPath $root -Force
    while ($null -ne $ancestor) {
        if ($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Reparse point in redistributable source.' }
        $ancestor = $ancestor.Parent
    }
    $choices = @(Get-ChildItem -LiteralPath $root -Directory | Where-Object {
        $_.Name -match '^14\.(3[0-9]|4[0-9])\.\d+(\.\d+)?$' -and
        (Test-Path -LiteralPath (Join-Path $_.FullName 'x64/Microsoft.VC143.CRT') -PathType Container)
    } | Sort-Object { [version]$_.Name } -Descending)
    if (!$choices.Count) { throw 'No VS 2022 x64 Microsoft.VC143.CRT redistributable found.' }
    $selected = Join-Path $choices[0].FullName 'x64/Microsoft.VC143.CRT'
    foreach ($part in @($choices[0].FullName, (Split-Path $selected -Parent), $selected)) {
        if ((Get-Item -LiteralPath $part -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw 'Reparse point in redistributable source.'
        }
    }
    return $selected
}

function Copy-DaymarkMsvcRuntime([string]$Destination) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere -PathType Leaf)) { throw 'vswhere is required to locate licensed VS 2022 redistributables.' }
    $installation = @(& $vswhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
    if ($LASTEXITCODE -ne 0 -or $installation.Count -ne 1 -or !$installation[0]) { throw 'Exactly one VS 2022 C++ installation is required.' }
    $source = Get-DaymarkMsvcRedistDirectory (Join-Path $installation[0] 'VC/Redist/MSVC')
    $target = (Get-Item -LiteralPath $Destination -Force).FullName
    $allowed = @('concrt140.dll','msvcp140.dll','msvcp140_1.dll','msvcp140_2.dll',
        'msvcp140_atomic_wait.dll','msvcp140_codecvt_ids.dll','vccorlib140.dll',
        'vcruntime140.dll','vcruntime140_1.dll','vcruntime140_threads.dll')
    $files = @(Get-ChildItem -LiteralPath $source -Force)
    foreach ($required in @('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll')) {
        if ($required -notin $files.Name) { throw "Required release runtime missing: $required" }
    }
    $evidence = @()
    # Validate the complete set before copying any file.
    foreach ($file in $files) {
        if ($file.PSIsContainer -or $file.Attributes -band [IO.FileAttributes]::ReparsePoint -or $file.Name -cnotin $allowed) {
            throw "Unreviewed file in CRT redistributable directory: $($file.Name)"
        }
        if (Test-Path -LiteralPath (Join-Path $target $file.Name)) { throw 'Runtime destination already exists; no overwrite is allowed.' }
        $signature = Get-AuthenticodeSignature -LiteralPath $file.FullName
        if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch '(^|,\s*)O=Microsoft Corporation(,|$)') {
            throw "Microsoft runtime signature validation failed: $($file.Name)"
        }
        $version = [Diagnostics.FileVersionInfo]::GetVersionInfo($file.FullName)
        if ($version.FileMajorPart -ne 14 -or $version.FileMinorPart -lt 30 -or $version.FileMinorPart -ge 50) {
            throw 'Runtime is outside the reviewed VS 2022 family.'
        }
        $stream = [IO.File]::OpenRead($file.FullName)
        $reader = [IO.BinaryReader]::new($stream)
        try {
            if ($reader.ReadUInt16() -ne 0x5a4d) { throw 'Invalid CRT PE image.' }
            $stream.Position = 0x3c
            $offset = $reader.ReadUInt32()
            if ($offset -gt $stream.Length - 6) { throw 'Invalid CRT PE offset.' }
            $stream.Position = $offset
            if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) { throw 'CRT must be a Windows x64 PE image.' }
        } finally { $reader.Dispose() }
        $evidence += [ordered]@{name=$file.Name;version=$version.FileVersion;
            sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
    }
    foreach ($file in $files) {
        Copy-Item -LiteralPath $file.FullName -Destination $target
        $expected = ($evidence | Where-Object { $_.name -ceq $file.Name }).sha256
        if ((Get-FileHash -LiteralPath (Join-Path $target $file.Name) -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expected) {
            throw 'Copied runtime differs from the validated source.'
        }
    }
    return $evidence
}

Export-ModuleMember -Function Get-DaymarkMsvcRedistDirectory,Copy-DaymarkMsvcRuntime
