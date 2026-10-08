param(
    [Parameter(Mandatory=$true)][string]$Exe,
    [string]$SevenZip = 'C:\Program Files\7-Zip\7z.exe'
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
$root = Join-Path $PSScriptRoot ('a2d-test-' + [guid]::NewGuid())
New-Item -ItemType Directory $root | Out-Null
$runner = Join-Path $root 'a2d.exe'
Copy-Item -LiteralPath (Resolve-Path $Exe).Path -Destination $runner
[IO.File]::WriteAllText((Join-Path $root 'a2d.pref'), $SevenZip, [Text.UTF8Encoding]::new($true))
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
    Write-Output 'All 8 cases passed.'
} finally {
    if ([IO.Path]::GetDirectoryName($root) -eq $PSScriptRoot) { Remove-Item -LiteralPath $root -Recurse -Force }
}
