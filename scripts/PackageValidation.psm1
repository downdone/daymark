#requires -Version 7.0
Set-StrictMode -Version Latest

function Read-PackageJson([string]$Path) {
    $document=[System.Text.Json.JsonDocument]::Parse([IO.File]::ReadAllText($Path))
    try {
        function Test-UniqueKeys($Element) {
            if($Element.ValueKind -eq [System.Text.Json.JsonValueKind]::Object) {
                $keys=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
                foreach($property in $Element.EnumerateObject()) {
                    if(!$keys.Add($property.Name)){throw 'Duplicate JSON key.'}
                    Test-UniqueKeys $property.Value
                }
            } elseif($Element.ValueKind -eq [System.Text.Json.JsonValueKind]::Array) {
                foreach($elementItem in $Element.EnumerateArray()){Test-UniqueKeys $elementItem}
            }
        }
        Test-UniqueKeys $document.RootElement
        return [IO.File]::ReadAllText($Path) | ConvertFrom-Json -AsHashtable
    } finally {$document.Dispose()}
}

function Assert-DaymarkPackage([string]$PackageDirectory) {
    $root=(Get-Item -LiteralPath $PackageDirectory -Force).FullName
    if(![IO.Directory]::Exists($root)){throw 'Package directory required.'}
    # Walk without following reparse points, including ancestors of the package.
    $ancestor=Get-Item -LiteralPath $root -Force
    while($null -ne $ancestor) {
        if($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse point in package path.'}
        $ancestor=$ancestor.Parent
    }
    $files=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
    $pending=[Collections.Generic.Queue[string]]::new();$pending.Enqueue($root)
    while($pending.Count) {
        foreach($item in Get-ChildItem -LiteralPath $pending.Dequeue() -Force) {
            if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse point in package.'}
            $relative=[IO.Path]::GetRelativePath($root,$item.FullName).Replace('\','/')
            if($relative -match '(?i)(^|/)(\.git|build|dist)(/|$)|\.(sqlite[^/]*|db|db-wal|db-shm|log|pfx|pem|key|p12)$'){throw 'Private or build artifact in package.'}
            if($item.PSIsContainer){$pending.Enqueue($item.FullName)}else{$files.Add($relative,$item)}
        }
    }
    if(!$files.ContainsKey('manifest.json') -or !$files.ContainsKey('BUILD-INFO.json')){throw 'Package metadata missing.'}
    $manifest=Read-PackageJson (Join-Path $root 'manifest.json')
    $info=Read-PackageJson (Join-Path $root 'BUILD-INFO.json')
    if($manifest.version -notmatch '^\d+\.\d+\.\d+(-rc\.\d+)?$' -or $manifest.version -cne $info.version -or
       $manifest.qt -cne '6.10.2' -or $info.qt -cne '6.10.2' -or $manifest.platform -cne 'windows-x64' -or
       $info.schemaVersion -ne 4 -or $info.sourceRevision -notmatch '^[a-f0-9]{40}$' -or
       $info.signed -isnot [bool] -or $info.signed -or $manifest.scope -cne 'local verification' -or
       $info.scope -cne 'Windows local release candidate'){throw 'Unsupported or inconsistent package metadata.'}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($entry in $manifest.files) {
        if($entry.path -isnot [string] -or !$entry.path -or $entry.path -match '(^/|\\|:|(^|/)\.{1,2}(/|$)|//|[ .](/|$))' -or
           $entry.path -ieq 'manifest.json' -or !$seen.Add($entry.path)){throw 'Unsafe or duplicate manifest path.'}
        if(!$files.ContainsKey($entry.path)){throw 'Manifest file missing.'}
        if($entry.size -isnot [long] -and $entry.size -isnot [int]){throw 'Invalid file size.'}
        if($entry.size -lt 0 -or $files[$entry.path].Length -ne $entry.size -or $entry.sha256 -notmatch '^[a-fA-F0-9]{64}$' -or
           (Get-FileHash -LiteralPath $files[$entry.path].FullName -Algorithm SHA256).Hash -ine $entry.sha256){throw 'Package file integrity failed.'}
    }
    if($seen.Count -ne $files.Count-1){throw 'Unlisted file in package.'}
    foreach($required in @('daymark.exe','BUILD-INFO.json','qt.conf','Qt6Core.dll','platforms/qwindows.dll','sqldrivers/qsqlite.dll',
                            'docs/user-guide.md','docs/third-party-notices.md','verification/Test-Package.ps1','verification/PackageValidation.psm1')) {
        if(!$seen.Contains($required)){throw 'Required deployment file missing.'}
    }
    return @{root=$root;manifest=$manifest;info=$info;files=$files}
}

function Assert-DaymarkArchive([string]$ArchivePath,[string]$PackageDirectory) {
    $archive=(Get-Item -LiteralPath $ArchivePath).FullName
    $expected=([IO.File]::ReadAllText($archive+'.sha256')).Trim()
    if($expected -notmatch '^([a-fA-F0-9]{64})  (.+)$' -or $Matches[2] -cne [IO.Path]::GetFileName($archive)){throw 'Invalid archive sidecar.'}
    $expectedHash=$Matches[1]
    if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ine $expectedHash){throw 'Archive checksum failed.'}
    $package=Assert-DaymarkPackage $PackageDirectory
    $prefix=[IO.Path]::GetFileName($package.root)+'/'
    $zip=[IO.Compression.ZipFile]::OpenRead($archive)
    try {
        $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach($entry in $zip.Entries) {
            if(!$entry.FullName.StartsWith($prefix,[StringComparison]::Ordinal)){throw 'Unexpected archive root.'}
            $relative=$entry.FullName.Substring($prefix.Length)
            if($relative -match '(^/|\\|:|(^|/)\.{1,2}(/|$)|//|[ .](/|$))'){throw 'Unsafe archive path.'}
            if($entry.FullName.EndsWith('/')){continue}
            if(!$seen.Add($relative) -or !$package.files.ContainsKey($relative)){throw 'Duplicate or unexpected archive file.'}
            $file=$package.files[$relative]
            if($entry.Length -ne $file.Length){throw 'Archive content length differs from verified package.'}
            $stream=$entry.Open();$algorithm=[Security.Cryptography.SHA256]::Create()
            try {$actual=[BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-','')}finally {$stream.Dispose();$algorithm.Dispose()}
            if($actual -ine (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash){throw 'Archive content differs from verified package.'}
        }
        if($seen.Count -ne $package.files.Count){throw 'Archive file missing.'}
    } finally {$zip.Dispose()}
    return $expectedHash.ToLowerInvariant()
}

function Get-DaymarkRuntimeOrigin([string]$ModuleName,[string]$ModulePath,[string]$PackageRoot,[string]$WindowsRoot) {
    $path=[IO.Path]::GetFullPath($ModulePath)
    if([IO.Path]::GetFileName($path) -ine $ModuleName){throw 'Module name and path disagree.'}
    $prefix=[IO.Path]::GetFullPath($PackageRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
    if($path.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){return 'package'}
    # Windows may resolve this graphics dependency from System32 despite a deployed fallback.
    $systemCompiler=[IO.Path]::GetFullPath((Join-Path $WindowsRoot 'System32/D3DCompiler_47.dll'))
    if($ModuleName -ieq 'D3DCompiler_47.dll' -and $path -ieq $systemCompiler){return 'windows-system'}
    throw ('Runtime DLL outside permitted locations: '+$ModuleName)
}

Export-ModuleMember -Function Assert-DaymarkPackage,Assert-DaymarkArchive,Get-DaymarkRuntimeOrigin
