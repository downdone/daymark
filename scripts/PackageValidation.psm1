#requires -Version 7.0
Set-StrictMode -Version Latest

function Read-DaymarkPreviewTests([string]$JunitPath,[string[]]$ExpectedTests) {
    if(!$ExpectedTests.Count){throw 'Expected CTest inventory required.'}
    $settings=[Xml.XmlReaderSettings]::new();$settings.DtdProcessing=[Xml.DtdProcessing]::Prohibit;$settings.XmlResolver=$null
    $reader=[Xml.XmlReader]::Create($JunitPath,$settings)
    try {$document=[Xml.XmlDocument]::new();$document.XmlResolver=$null;$document.Load($reader)}finally{$reader.Dispose()}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $failed=[Collections.Generic.List[string]]::new();$cases=[Collections.Generic.List[object]]::new()
    foreach($case in $document.SelectNodes('//testcase')) {
        $name=$case.GetAttribute('name');if(!$name -or !$seen.Add($name)){throw 'Missing or duplicate test result.'}
        if($case.SelectSingleNode('skipped') -or $case.GetAttribute('status') -eq 'notrun'){throw 'Skipped CTest cannot qualify a preview.'}
        $failure=$null -ne $case.SelectSingleNode('failure|error')
        if($failure){$failed.Add($name)}
        $cases.Add([ordered]@{name=$name;passed=!$failure;seconds=$case.GetAttribute('time')})
    }
    if(!$seen.SetEquals($ExpectedTests)){throw 'Incomplete CTest report; preview blocked.'}
    foreach($name in $failed){if($name -cnotin @('qml_startup','qml_watchdog_late')){throw ('Unexpected CTest failure; preview blocked: '+$name)}}
    return [ordered]@{format=1;total=$seen.Count;failed=$failed.ToArray();fullGatePassed=$failed.Count -eq 0;cases=$cases.ToArray()}
}

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

function Test-DaymarkPayloadPath([string]$Path) {
    if($Path -match '(?i)(^|/)(docs|src|tests|scripts|verification|qmltooling)(/|$)|\.(qmltypes|pdb|lib|a|h|cpp|ps1|psm1|log)$'){return $false}
    if($Path -match '^(daymark\.exe|qt\.conf|BUILD-INFO\.json|manifest\.json|README\.md|THIRD-PARTY-NOTICES\.md|TEST-RESULTS\.json)$'){return $true}
    if($Path -match '^(Qt6[A-Za-z0-9]+|libgcc_s_seh-1|libstdc\+\+-6|libwinpthread-1|opengl32sw|D3Dcompiler_47)\.dll$') {
        return $Path -notmatch '^Qt6QuickControls2(Fusion|Imagine|Material|Universal|FluentWinUI3|Windows)(StyleImpl)?\.dll$'
    }
    if($Path -match '^sqldrivers/'){return $Path -ceq 'sqldrivers/qsqlite.dll'}
    if($Path -match '^qt-sbom/'){return $Path -match '^qt-sbom/(qtbase|qtdeclarative|qtshadertools|qtsvg)-6\.10\.2\.spdx\.json$'}
    if($Path -match '^qml/') {
        return $Path -notmatch '^qml/QtQuick/Controls/(Fusion|Imagine|Material|Universal|FluentWinUI3|Windows)/' -and
            $Path -match '(\.dll|\.qml|\.png|/qmldir)$'
    }
    return $Path -match '^(platforms|imageformats|iconengines|tls|networkinformation|styles)/[A-Za-z0-9_-]+\.dll$'
}

function Remove-DaymarkDeploymentExtras([string]$PackageDirectory) {
    $root=(Get-Item -LiteralPath $PackageDirectory -Force).FullName
    if([IO.Path]::GetFileName($root) -notmatch '^Daymark-.+-windows-x64-local-\d{8}-\d{6}$' -or !(Test-Path -LiteralPath (Join-Path $root 'daymark.exe'))){throw 'Expected a new Daymark deployment directory.'}
    $ancestor=Get-Item -LiteralPath $root -Force
    while($null -ne $ancestor){if($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse point in deployment path.'};$ancestor=$ancestor.Parent}
    if(@(Get-ChildItem -LiteralPath $root -Recurse -Force | Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}).Count){throw 'Reparse point in deployment contents.'}
    $prefix=$root.TrimEnd([IO.Path]::DirectorySeparatorChar)+[IO.Path]::DirectorySeparatorChar
    $removed=[Collections.Generic.List[string]]::new()
    foreach($file in Get-ChildItem -LiteralPath $root -Recurse -File) {
        if(!$file.FullName.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or $file.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Unsafe deployment cleanup target.'}
        $relative=[IO.Path]::GetRelativePath($root,$file.FullName).Replace('\','/')
        if(!(Test-DaymarkPayloadPath $relative)) {
            if($relative -notmatch '(?i)^qmltooling/|^generic/qtuiotouchplugin\.dll$|\.qmltypes$|^qml/QtQuick/Controls/(Fusion|Imagine|Material|Universal|FluentWinUI3|Windows)/|^Qt6QuickControls2(Fusion|Imagine|Material|Universal|FluentWinUI3|Windows)(StyleImpl)?\.dll$|^sqldrivers/qsql(ibase|mimer|oci|odbc|psql)\.dll$'){throw ('Unreviewed deployment file: '+$relative)}
            Remove-Item -LiteralPath $file.FullName;$removed.Add($relative)
        } elseif($file.Name -ceq 'qmldir') {
            $text=[IO.File]::ReadAllText($file.FullName)
            [IO.File]::WriteAllText($file.FullName,([regex]::Replace($text,'(?m)^typeinfo[^\r\n]*\r?\n','')))
        }
    }
    # Nonrecursive, deepest-first empty-directory cleanup inside the new deployment.
    foreach($directory in Get-ChildItem -LiteralPath $root -Directory -Recurse | Sort-Object { $_.FullName.Length } -Descending) {
        if(!$directory.FullName.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or $directory.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Unsafe deployment directory.'}
        if(!(Get-ChildItem -LiteralPath $directory.FullName -Force)){Remove-Item -LiteralPath $directory.FullName}
    }
    return $removed.ToArray()
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
            if($item.PSIsContainer){$pending.Enqueue($item.FullName)}else{if(!(Test-DaymarkPayloadPath $relative)){throw ('Non-runtime payload rejected: '+$relative)};$files.Add($relative,$item)}
        }
    }
    if(!$files.ContainsKey('manifest.json') -or !$files.ContainsKey('BUILD-INFO.json')){throw 'Package metadata missing.'}
    $manifest=Read-PackageJson (Join-Path $root 'manifest.json')
    $info=Read-PackageJson (Join-Path $root 'BUILD-INFO.json')
    if($manifest.version -notmatch '^\d+\.\d+\.\d+(-rc\.\d+)?$' -or $manifest.version -cne $info.version -or
       $manifest.qt -cne '6.10.2' -or $info.qt -cne '6.10.2' -or $manifest.platform -cne 'windows-x64' -or
       $info.schemaVersion -ne 9 -or $info.sourceRevision -notmatch '^[a-f0-9]{40}$' -or
       $info.signed -isnot [bool] -or $info.signed -or $manifest.scope -cne 'local verification' -or
       $info.scope -cnotin @('Windows local release candidate','Windows local preview candidate')){throw 'Unsupported or inconsistent package metadata.'}
    if($info.scope -ceq 'Windows local preview candidate') {
        if(!$info.ContainsKey('localPreview') -or $info.localPreview -isnot [bool] -or !$info.localPreview -or
           !$info.ContainsKey('fullTestGatePassed') -or $info.fullTestGatePassed -isnot [bool] -or
           !$info.ContainsKey('knownTestFailures')){throw 'Preview gate evidence missing.'}
        foreach($name in $info.knownTestFailures){if($name -cnotin @('qml_startup','qml_watchdog_late')){throw 'Unexpected preview failure metadata.'}}
        if($info.fullTestGatePassed -ne (@($info.knownTestFailures).Count -eq 0)){throw 'Inconsistent preview gate metadata.'}
        $summaryPath=Join-Path $root 'TEST-RESULTS.json'
        if(!(Test-Path -LiteralPath $summaryPath)){throw 'Preview full test summary missing.'}
        $summary=Read-PackageJson $summaryPath
        if($summary.fullGatePassed -ne $info.fullTestGatePassed -or
           (@($summary.failed) -join ',') -cne (@($info.knownTestFailures) -join ',') -or
           $summary.total -ne @($summary.cases).Count){throw 'Inconsistent preview full test summary.'}
    }
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
                            'README.md','THIRD-PARTY-NOTICES.md')) {
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

Export-ModuleMember -Function Assert-DaymarkPackage,Assert-DaymarkArchive,Get-DaymarkRuntimeOrigin,Read-DaymarkPreviewTests,Test-DaymarkPayloadPath,Remove-DaymarkDeploymentExtras
