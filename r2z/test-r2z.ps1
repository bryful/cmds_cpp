param([Parameter(Mandatory=$true)][string]$Exe,[string]$Dll='C:\Program Files\7-Zip\7z.dll')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression
if (-not ('RarFixture' -as [type])) { Add-Type -Path (Join-Path $PSScriptRoot 'RarFixture.cs') }
$root=Join-Path $PSScriptRoot ('r2z-test-'+[guid]::NewGuid())
New-Item -ItemType Directory $root | Out-Null
$runner=Join-Path $root 'r2z.exe'
Copy-Item -LiteralPath (Resolve-Path $Exe).Path -Destination $runner
[IO.File]::WriteAllText((Join-Path $root 'r2z.pref'),$Dll,[Text.UTF8Encoding]::new($true))
function Assert($condition,$message) { if (!$condition) { throw $message } }
function Rar($name,$names,$contents,$bad=$false) { $p=Join-Path $root $name; [RarFixture]::Write($p,[string[]]$names,[string[]]$contents,$bad); return $p }
function ZipContents($path) {
    $stream=[IO.File]::OpenRead($path); $zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Read)
    $result=@{}
    try { foreach ($entry in $zip.Entries) { if (!$entry.FullName.EndsWith('/')) { $reader=[IO.StreamReader]::new($entry.Open()); try { $result[$entry.FullName]=$reader.ReadToEnd() } finally { $reader.Dispose() } } }; return $result }
    finally { $zip.Dispose(); $stream.Dispose() }
}
try {
    $p=Rar '日本語 flat.RAR' @('a.txt','b.txt') @('first','second'); $hash=(Get-FileHash -LiteralPath $p).Hash
    & $runner $p
    Assert ($LASTEXITCODE -eq 0) 'Flat RAR conversion failed'
    $data=ZipContents ([IO.Path]::ChangeExtension($p,'.zip'))
    Assert ($data.Count -eq 2 -and $data['a.txt'] -eq 'first' -and $data['b.txt'] -eq 'second') 'ZIP content differs'
    Assert ((Get-FileHash -LiteralPath $p).Hash -eq $hash) 'Original RAR changed'

    $p=Rar 'wrapped.rar' @('root/','root/sub/a.txt','root/sub/bad.scr','root/upper.SCR') @('','nested','text','text')
    & $runner $p
    Assert ($LASTEXITCODE -eq 0) 'Wrapped RAR conversion failed'
    $data=ZipContents ([IO.Path]::ChangeExtension($p,'.zip'))
    Assert ($data.Count -eq 1 -and $data['sub/a.txt'] -eq 'nested') 'Root flattening or recursive SCR exclusion failed'

    $p=Rar 'existing.rar' @('a.txt') @('new'); $zip=[IO.Path]::ChangeExtension($p,'.zip'); [IO.File]::WriteAllText($zip,'keep')
    & $runner $p
    Assert ($LASTEXITCODE -ne 0 -and [IO.File]::ReadAllText($zip) -eq 'keep') 'Existing ZIP overwritten'

    $p=Rar 'crc.rar' @('a.txt','b.txt') @('bad','good') $true
    & $runner $p
    Assert ($LASTEXITCODE -ne 0 -and !(Test-Path ([IO.Path]::ChangeExtension($p,'.zip')))) 'CRC failure published output'

    $bad=Join-Path $root 'bad.rar'; [IO.File]::WriteAllText($bad,'invalid')
    $good=Rar 'after.rar' @('ok.txt') @('ok')
    & $runner $bad $good (Join-Path $root 'missing.rar')
    Assert ($LASTEXITCODE -ne 0 -and (Test-Path ([IO.Path]::ChangeExtension($good,'.zip')))) 'Batch failure handling failed'

    $p=Rar 'only-scr.rar' @('bad.scr','upper.SCR') @('text','text')
    & $runner $p
    Assert ($LASTEXITCODE -ne 0 -and !(Test-Path ([IO.Path]::ChangeExtension($p,'.zip')))) 'Empty filter extracted everything'

    $p=Rar 'traversal.rar' @('../escape.txt') @('keep')
    & $runner $p
    Assert ($LASTEXITCODE -ne 0 -and !(Test-Path (Join-Path $root 'escape.txt'))) 'Traversal path accepted'

    $p=Rar 'duplicate.rar' @('a.txt','A.txt') @('first','second')
    & $runner $p
    Assert ($LASTEXITCODE -ne 0 -and !(Test-Path ([IO.Path]::ChangeExtension($p,'.zip')))) 'Case-insensitive duplicate accepted'

    foreach ($mode in '--fast','--store') {
        $p=Rar ($mode.Substring(2)+'.rar') @('data.txt') @(('sample '*1000))
        & $runner --dll $Dll $mode $p
        Assert ($LASTEXITCODE -eq 0 -and (ZipContents ([IO.Path]::ChangeExtension($p,'.zip')))['data.txt'] -eq ('sample '*1000)) 'Compression mode failed'
    }
    $p=Rar 'locked.rar' @('a.txt') @('locked'); $lock=[IO.File]::Open($p,'Open','Read','None')
    try { & $runner $p; Assert ($LASTEXITCODE -ne 0) 'Locked RAR accepted' } finally { $lock.Dispose() }
    Assert (!(Test-Path ([IO.Path]::ChangeExtension($p,'.zip')))) 'Locked RAR published output'
    Assert ((Get-ChildItem $root -Directory -Filter '.r2z-*.tmp').Count -eq 0) 'Staging directories leaked'
    Write-Output 'All 10 cases passed.'
} finally {
    if ([IO.Path]::GetDirectoryName($root) -eq $PSScriptRoot) { Remove-Item -LiteralPath $root -Recurse -Force }
}
