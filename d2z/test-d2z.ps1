param([Parameter(Mandatory=$true)][string]$Exe, [string]$SevenZip='C:\Program Files\7-Zip\7z.exe', [string]$Dll='C:\Program Files\7-Zip\7z.dll')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression
$root=Join-Path $PSScriptRoot ('d2z-test-'+[guid]::NewGuid())
New-Item -ItemType Directory $root | Out-Null
$runner=Join-Path $root 'd2z.exe'
Copy-Item -LiteralPath (Resolve-Path $Exe).Path -Destination $runner
[IO.File]::WriteAllText((Join-Path $root 'd2z.pref'), $Dll, [Text.UTF8Encoding]::new($true))
function Assert($condition,$message) { if (!$condition) { throw $message } }
function Directory($name) { $p=Join-Path $root $name; New-Item -ItemType Directory $p | Out-Null; return $p }
function File($directory,$name,$value='sample') { [IO.File]::WriteAllText((Join-Path $directory $name),$value) }
function Entries($path) {
    $stream=[IO.File]::OpenRead($path)
    $zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Read)
    try { return @($zip.Entries | ForEach-Object { $_.FullName }) }
    finally { $zip.Dispose(); $stream.Dispose() }
}
try {
    $p=Directory '日本語 flat'
    File $p 'a.txt'; File $p 'no-extension'; File $p '.hidden'
    Push-Location $root
    try { $progress = @(& $runner '.\日本語 flat\\'); $progress | Write-Output; Assert ($progress[0] -like '[[]1/1] Compressing:*') 'Start progress missing' } finally { Pop-Location }
    Assert ($LASTEXITCODE -eq 0) 'Relative/trailing separator conversion failed'
    $names=Entries ($p+'.zip')
    Assert ($names.Count -eq 3 -and $names -contains 'a.txt' -and $names -contains 'no-extension' -and $names -contains '.hidden') 'Unexpected ZIP paths'
    Assert (!(Test-Path (Join-Path $p '.zip'))) 'ZIP created inside source'
    & $SevenZip t ($p+'.zip') > $null
    Assert ($LASTEXITCODE -eq 0) 'ZIP integrity check failed'

    $p=Directory 'wrapped'; $child=Join-Path $p 'inner'; New-Item -ItemType Directory $child | Out-Null
    File $child 'a.txt'
    & $runner $p
    Assert ($LASTEXITCODE -eq 0) 'Single child conversion failed'
    Assert ((Entries ($p+'.zip')) -contains 'a.txt') 'Single child was not unwrapped'

    $p=Directory 'nested'; New-Item -ItemType Directory (Join-Path $p 'sub') | Out-Null
    File $p 'top.txt'; File (Join-Path $p 'sub') 'deep.txt'
    & $runner $p
    Assert ($LASTEXITCODE -eq 0) 'Nested conversion failed'
    Assert ((Entries ($p+'.zip')) -contains 'sub/deep.txt') 'Nested paths were lost'

    $p=Directory 'existing'; File $p 'new.txt'; [IO.File]::WriteAllText(($p+'.zip'),'keep')
    & $runner $p
    Assert ($LASTEXITCODE -ne 0) 'Existing ZIP accepted'
    Assert ([IO.File]::ReadAllText(($p+'.zip')) -eq 'keep') 'Existing ZIP changed'

    $p=Directory 'locked'; File $p 'blocked.txt'; File $p 'readable.txt'
    $lock=[IO.File]::Open((Join-Path $p 'blocked.txt'),'Open','Read','None')
    try { & $runner $p; Assert ($LASTEXITCODE -ne 0) 'Locked input returned success' }
    finally { $lock.Dispose() }
    Assert (!(Test-Path ($p+'.zip'))) 'Partial ZIP was published'
    Assert (Test-Path (Join-Path $p 'blocked.txt')) 'Source changed'

    $p=Directory 'after'; File $p 'ok.txt'
    & $runner (Join-Path $root 'missing') $p
    Assert ($LASTEXITCODE -ne 0) 'Mixed batch returned success'
    Assert (Test-Path ($p+'.zip')) 'Batch did not continue'

    $p=Directory 'empty'
    & $runner $p
    Assert ($LASTEXITCODE -eq 0) 'Empty conversion failed'
    Assert ((Entries ($p+'.zip')).Count -eq 0) 'Empty ZIP has unexpected entries'

    foreach ($mode in '--fast','--store') {
        $p=Directory $mode.Substring(2)
        File $p 'data.txt' ('compressible text ' * 1000)
        & $runner $mode $p
        Assert ($LASTEXITCODE -eq 0) 'Compression mode failed'
        & $SevenZip t ($p+'.zip') > $null
        Assert ($LASTEXITCODE -eq 0) 'Mode ZIP integrity check failed'
        if ($mode -eq '--store') {
            $stream=[IO.File]::OpenRead(($p+'.zip'))
            $zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Read)
            try { Assert ($zip.Entries[0].Length -eq $zip.Entries[0].CompressedLength) 'Store mode compressed data' }
            finally { $zip.Dispose(); $stream.Dispose() }
        }
    }
    Assert ((Get-ChildItem $root -Directory -Filter '.d2z-*.tmp').Count -eq 0) 'Temporary directories leaked'
    [IO.File]::WriteAllText((Join-Path $root 'd2z.pref'),$SevenZip,[Text.UTF8Encoding]::new($false))
    $p=Directory 'legacy'; File $p 'a.txt'
    & $runner $p
    Assert ($LASTEXITCODE -eq 0) 'Legacy pref migration failed'

    [IO.File]::WriteAllText((Join-Path $root 'd2z.pref'),'missing.dll',[Text.UTF8Encoding]::new($false))
    $p=Directory 'explicit'; File $p 'a.txt'
    & $runner --dll $Dll $p
    Assert ($LASTEXITCODE -eq 0) 'Explicit DLL override failed'
    [IO.File]::WriteAllText((Join-Path $root 'd2z.pref'),$Dll,[Text.UTF8Encoding]::new($true))

    $p=Directory 'images.v1'; File $p 'a.txt'
    & $runner $p
    Assert ($LASTEXITCODE -eq 0 -and (Test-Path ($p+'.zip'))) 'Dotted directory output changed'

    $p=Directory 'invalid-mode'; File $p 'a.txt'
    & $runner --fast --store $p
    Assert ($LASTEXITCODE -ne 0 -and !(Test-Path ($p+'.zip'))) 'Conflicting modes accepted'

    $p=Directory '-leading'; File $p 'a.txt'
    Push-Location $root
    try { & $runner -- '-leading' } finally { Pop-Location }
    Assert ($LASTEXITCODE -eq 0 -and (Test-Path ($p+'.zip'))) 'Option terminator failed'
    Assert ((Get-ChildItem $root -Directory -Filter '.d2z-*.tmp').Count -eq 0) 'Temporary directories leaked'
    Write-Output 'All 13 cases passed.'
} finally {
    if ([IO.Path]::GetDirectoryName($root) -eq $PSScriptRoot) { Remove-Item -LiteralPath $root -Recurse -Force }
}
