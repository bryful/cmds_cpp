param(
    [Parameter(Mandatory=$true)][string]$Exe,
    [string]$SevenZip = 'C:\Program Files\7-Zip\7z.exe',
    [string]$Dll = 'C:\Program Files\7-Zip\7z.dll'
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
$root = Join-Path $PSScriptRoot ('a2d-test-' + [guid]::NewGuid())
New-Item -ItemType Directory $root | Out-Null
$runner = Join-Path $root 'a2d.exe'
Copy-Item -LiteralPath (Resolve-Path $Exe).Path -Destination $runner
[IO.File]::WriteAllText((Join-Path $root 'a2d.pref'), $Dll, [Text.UTF8Encoding]::new($true))
function Assert($condition, $message) { if (!$condition) { throw $message } }
function Zip($name, $entries) {
    $path = Join-Path $root $name
    $stream = [IO.File]::Create($path)
    $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in $entries.Keys) {
            $entry = $zip.CreateEntry($name, [IO.Compression.CompressionLevel]::NoCompression)
            if (!$name.EndsWith('/')) {
                $writer = [IO.StreamWriter]::new($entry.Open())
                try { $writer.Write($entries[$name]) } finally { $writer.Dispose() }
            }
        }
    } finally { $zip.Dispose(); $stream.Dispose() }
    return $path
}
try {
    $archive = Zip '日本語 flat.ZIP' @{'a.txt'='a';'b.txt'='b'}
    & $runner $archive
    Assert ($LASTEXITCODE -eq 0) 'Flat extraction failed'
    Assert ([IO.File]::ReadAllText((Join-Path $root '日本語 flat\a.txt')) -eq 'a') 'Flat output missing'
    Assert (!(Test-Path (Join-Path $root 'a.txt'))) 'Flat extraction polluted parent'

    $archive = Zip 'wrapped.zip' @{'different-root/sub/a.txt'='a';'different-root/b.txt'='b'}
    & $runner $archive
    Assert ($LASTEXITCODE -eq 0) 'Implicit single root extraction failed'
    Assert (Test-Path (Join-Path $root 'wrapped\sub\a.txt')) 'Single root not unwrapped'
    Assert (!(Test-Path (Join-Path $root 'wrapped\different-root'))) 'Double directory created'

    $archive = Zip 'filter.zip' @{'root/'='';'root/a.txt'='a';'root/sub/lower.scr'='text';'root/sub/upper.SCR'='text';'top.SCR'='text'}
    & $runner $archive
    Assert ($LASTEXITCODE -eq 0) 'Filtered extraction failed'
    Assert (Test-Path (Join-Path $root 'filter\a.txt')) 'Normal file excluded'
    Assert ((Get-ChildItem (Join-Path $root 'filter') -Recurse -File | Where-Object Extension -eq '.scr').Count -eq 0) 'SCR exclusion failed'

    $archive = Zip 'collision.zip' @{'a.txt'='new'}
    New-Item -ItemType Directory (Join-Path $root 'collision') | Out-Null
    [IO.File]::WriteAllText((Join-Path $root 'collision\a.txt'), 'keep')
    & $runner $archive
    Assert ($LASTEXITCODE -ne 0) 'Existing destination accepted'
    Assert ([IO.File]::ReadAllText((Join-Path $root 'collision\a.txt')) -eq 'keep') 'Existing output changed'

    $bad = Join-Path $root 'bad.zip'
    [IO.File]::WriteAllText($bad, 'invalid archive')
    $good = Zip 'after.zip' @{'ok.txt'='ok'}
    & $runner $bad $good (Join-Path $root 'missing.zip')
    Assert ($LASTEXITCODE -ne 0) 'Mixed batch returned success'
    Assert (!(Test-Path (Join-Path $root 'bad'))) 'Failed extraction published output'
    Assert (Test-Path (Join-Path $root 'after\ok.txt')) 'Batch did not continue'

    $archive = Zip 'crc.zip' @{'one.txt'='payload';'two.txt'='second'}
    $bytes = [IO.File]::ReadAllBytes($archive)
    $offset = 30 + [BitConverter]::ToUInt16($bytes, 26) + [BitConverter]::ToUInt16($bytes, 28)
    $bytes[$offset] = $bytes[$offset] -bxor 1
    [IO.File]::WriteAllBytes($archive, $bytes)
    & $runner $archive
    Assert ($LASTEXITCODE -ne 0) 'CRC failure returned success'
    Assert (!(Test-Path (Join-Path $root 'crc'))) 'Partial extraction was published'

    $archive = Zip 'only-scr.zip' @{'bad.scr'='text'}
    & $runner $archive
    Assert ($LASTEXITCODE -ne 0) 'Empty filtered result returned success'
    Assert (!(Test-Path (Join-Path $root 'only-scr'))) 'Empty filtered result published output'

    $archive = Zip 'empty-folder.zip' @{'root/'=''}
    & $runner $archive
    Assert ($LASTEXITCODE -eq 0) 'Empty folder extraction failed'
    Assert (Test-Path (Join-Path $root 'empty-folder') -PathType Container) 'Empty folder missing'
    Assert ((Get-ChildItem $root -Directory -Filter '.a2d-*.tmp').Count -eq 0) 'Staging directories leaked'
    Assert ((Get-ChildItem $root -Filter '*.zip').Count -eq 9) 'Source archives changed'
    $archive = Zip 'traversal.zip' @{'../escape.txt'='text'}
    & $runner $archive
    Assert ($LASTEXITCODE -ne 0 -and !(Test-Path (Join-Path $root 'escape.txt')) -and !(Test-Path (Join-Path $root 'traversal'))) 'Traversal path accepted'

    $entries = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
    $entries.Add('a.txt','first'); $entries.Add('A.txt','second')
    $archive = Zip 'duplicates.zip' $entries
    & $runner $archive
    Assert ($LASTEXITCODE -ne 0 -and !(Test-Path (Join-Path $root 'duplicates'))) 'Unsafe duplicate accepted'

    if (-not ('RarFixture' -as [type])) { Add-Type -Path (Join-Path $PSScriptRoot 'RarFixture.cs') }
    $archive = Join-Path $root 'rar4.RAR'
    [RarFixture]::Write($archive,@('root/a.txt','root/sub/upper.SCR'),@('rar contents','excluded'),$false)
    $hash = (Get-FileHash -LiteralPath $archive).Hash
    & $runner --dll $Dll $archive
    Assert ($LASTEXITCODE -eq 0 -and [IO.File]::ReadAllText((Join-Path $root 'rar4/a.txt')) -eq 'rar contents') 'RAR4 failed'
    Assert (!(Test-Path (Join-Path $root 'rar4/sub/upper.SCR'))) 'RAR SCR exclusion failed'
    Assert ((Get-FileHash -LiteralPath $archive).Hash -eq $hash) 'Source RAR changed'

    $archive = Join-Path $root 'rar-crc.rar'
    [RarFixture]::Write($archive,@('a.txt'),@('bad'),$true)
    & $runner $archive
    Assert ($LASTEXITCODE -ne 0 -and !(Test-Path (Join-Path $root 'rar-crc'))) 'Corrupt RAR published'

    # Legacy .pref paths are translated to the DLL in the same directory.
    [IO.File]::WriteAllText((Join-Path $root 'a2d.pref'),$SevenZip,[Text.UTF8Encoding]::new($false))
    $archive = Zip 'legacy.zip' @{'ok.txt'='legacy'}
    & $runner $archive
    Assert ($LASTEXITCODE -eq 0 -and [IO.File]::ReadAllText((Join-Path $root 'legacy/ok.txt')) -eq 'legacy') 'Legacy pref migration failed'
    Assert ((Get-ChildItem $root -Directory -Filter '.a2d-*.tmp').Count -eq 0) 'Staging directories leaked after safety tests'
    $archive = Zip 'unix-link.zip' @{'link'='../escape.txt'}
    $stream = [IO.File]::Open($archive,'Open','ReadWrite')
    $zip = [IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Update)
    $zip.Entries[0].ExternalAttributes = [int]0xA1FF0000
    $zip.Dispose(); $stream.Dispose()
    $bytes = [IO.File]::ReadAllBytes($archive)
    for ($i=0; $i -lt $bytes.Length-46; $i++) {
        if ($bytes[$i] -eq 80 -and $bytes[$i+1] -eq 75 -and $bytes[$i+2] -eq 1 -and $bytes[$i+3] -eq 2) { $bytes[$i+5] = 3 }
    }
    [IO.File]::WriteAllBytes($archive,$bytes)
    & $runner $archive
    Assert ($LASTEXITCODE -ne 0 -and !(Test-Path (Join-Path $root 'unix-link'))) 'Unix symlink accepted'
    Write-Output 'All 14 cases passed.'
} finally {
    if ([IO.Path]::GetDirectoryName($root) -eq $PSScriptRoot) { Remove-Item -LiteralPath $root -Recurse -Force }
}
