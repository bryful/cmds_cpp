param([Parameter(Mandatory=$true)][string]$Exe)
$ErrorActionPreference='Stop'
$runner=(Resolve-Path $Exe).Path
$root=Join-Path $PSScriptRoot ('fs-test-'+[guid]::NewGuid())
New-Item -ItemType Directory $root | Out-Null
$links=[Collections.Generic.List[string]]::new()
$cases=0
function Assert($condition,$message) { if (!$condition) { throw $message } }
function Directory($name) { $p=Join-Path $root $name; New-Item -ItemType Directory $p -Force | Out-Null; return $p }
function File($path,$contents) { [IO.File]::WriteAllText($path,$contents) }
function Junction($path,$target) { New-Item -ItemType Junction -Path $path -Target $target | Out-Null; $links.Add($path) }
try {
    $p=Directory '日本語';File (Join-Path $p '[作者]作品.ZIP') 'zip';File (Join-Path $p '[作者]作品.RAR') 'rar';File (Join-Path $p '[作者]作品.txt') 'txt';File (Join-Path $p 'plain.zip') 'plain'
    & $runner $p
    Assert ($LASTEXITCODE -eq 0) 'Normal sorting failed'
    Assert ([IO.File]::ReadAllText((Join-Path $p '作者/[作者]作品.ZIP')) -eq 'zip') 'ZIP changed or missing'
    Assert ([IO.File]::ReadAllText((Join-Path $p '作者/[作者]作品.RAR')) -eq 'rar') 'RAR changed or missing'
    Assert ((Test-Path -LiteralPath (Join-Path $p '[作者]作品.txt')) -and (Test-Path (Join-Path $p 'plain.zip'))) 'Unrelated file moved'
    $cases++

    $p=Directory 'unsafe'
    foreach($group in @('..','.','bad.','bad ','CON','LPT1')) { File (Join-Path $p ('['+$group+']data.zip')) 'keep' }
    File (Join-Path $p '[safe]data.zip') 'good'
    & $runner $p
    Assert ($LASTEXITCODE -ne 0) 'Unsafe names accepted'
    foreach($group in @('..','.','bad.','bad ','CON','LPT1')) { Assert (Test-Path -LiteralPath (Join-Path $p ('['+$group+']data.zip'))) 'Unsafe name moved' }
    Assert (!(Test-Path -LiteralPath (Join-Path $root '[..]data.zip'))) 'Escaped root'
    Assert (Test-Path -LiteralPath (Join-Path $p 'safe/[safe]data.zip')) 'Failed to continue sorting'
    $cases++

    $p=Directory 'collision';$sub=Directory 'collision/sub';File (Join-Path $p 'same.txt') 'original';File (Join-Path $sub 'same.txt') 'incoming';File (Join-Path $sub 'good.txt') 'good'
    & $runner -r $p
    Assert ($LASTEXITCODE -ne 0) 'Reverse collision returned success'
    Assert ([IO.File]::ReadAllText((Join-Path $p 'same.txt')) -eq 'original') 'Existing file overwritten'
    Assert ([IO.File]::ReadAllText((Join-Path $sub 'same.txt')) -eq 'incoming') 'Conflicting source lost'
    Assert (Test-Path (Join-Path $p 'good.txt')) 'Reverse processing did not continue'
    $cases++

    $p=Directory 'forward-collision';$sub=Directory 'forward-collision/group';File (Join-Path $p '[group]a.zip') 'incoming';File (Join-Path $sub '[group]a.zip') 'original'
    & $runner $p
    Assert ($LASTEXITCODE -ne 0) 'Forward collision returned success'
    Assert ([IO.File]::ReadAllText((Join-Path $sub '[group]a.zip')) -eq 'original') 'Forward overwrite'
    Assert ([IO.File]::ReadAllText((Join-Path $p '[group]a.zip')) -eq 'incoming') 'Forward source lost'
    $cases++

    $p=Directory 'file-destination';File (Join-Path $p 'group') 'keep';File (Join-Path $p '[group]a.zip') 'incoming'
    & $runner $p
    Assert ($LASTEXITCODE -ne 0 -and (Test-Path -LiteralPath (Join-Path $p '[group]a.zip'))) 'Non-directory destination accepted'
    $cases++

    $p=Directory 'reverse-link';$outside=Directory 'outside';File (Join-Path $outside 'outside.txt') 'keep';Junction (Join-Path $p 'linked') $outside
    & $runner -r $p
    Assert ($LASTEXITCODE -eq 0) 'Link skip should not fail the batch'
    Assert ([IO.File]::ReadAllText((Join-Path $outside 'outside.txt')) -eq 'keep') 'Reverse followed junction'
    Assert (!(Test-Path (Join-Path $p 'outside.txt'))) 'External file moved'
    $cases++

    $p=Directory 'forward-link';Junction (Join-Path $p 'linked') $outside;File (Join-Path $p '[linked]a.zip') 'keep'
    & $runner $p
    Assert ($LASTEXITCODE -ne 0 -and (Test-Path -LiteralPath (Join-Path $p '[linked]a.zip'))) 'Forward destination junction accepted'
    Assert (!(Test-Path -LiteralPath (Join-Path $outside '[linked]a.zip'))) 'External destination modified'
    $cases++

    $link=Join-Path $root 'root-link';Junction $link $outside
    & $runner -r $link
    Assert ($LASTEXITCODE -ne 0 -and (Test-Path (Join-Path $outside 'outside.txt'))) 'Root junction accepted'
    $cases++

    & $runner -r (Join-Path $root 'missing')
    Assert ($LASTEXITCODE -ne 0) 'Missing target returned success'
    & $runner -r
    Assert ($LASTEXITCODE -ne 0) 'Missing option argument accepted'
    & $runner -r $outside 'extra'
    Assert ($LASTEXITCODE -ne 0) 'Extra argument accepted'
    $cases++

    $p=Directory 'reverse';$sub=Directory 'reverse/sub';File (Join-Path $sub 'any.txt') 'data'
    & $runner -r $p
    Assert ($LASTEXITCODE -eq 0 -and [IO.File]::ReadAllText((Join-Path $p 'any.txt')) -eq 'data') 'Reverse non-archive file failed'
    Assert (!(Test-Path $sub)) 'Emptied subdirectory not removed'
    $cases++

    $p=Directory 'nested';$sub=Directory 'nested/sub';$deep=Directory 'nested/sub/deep';$empty=Directory 'nested/empty';File (Join-Path $deep 'deep.txt') 'keep';File (Join-Path $sub 'top.txt') 'move'
    & $runner -r $p
    Assert ($LASTEXITCODE -eq 0 -and (Test-Path (Join-Path $p 'top.txt'))) 'Nested reverse failed'
    Assert ((Test-Path (Join-Path $deep 'deep.txt')) -and (Test-Path $empty)) 'Nested or pre-existing empty directory removed'
    $cases++

    $p=Directory 'locked';$sub=Directory 'locked/sub';File (Join-Path $sub 'locked.txt') 'keep';$lock=[IO.File]::Open((Join-Path $sub 'locked.txt'),'Open','Read','None')
    try { & $runner -r $p; Assert ($LASTEXITCODE -ne 0) 'Locked file returned success' } finally { $lock.Dispose() }
    Assert ([IO.File]::ReadAllText((Join-Path $sub 'locked.txt')) -eq 'keep') 'Locked source changed'
    $cases++

    Write-Output "All $cases cases passed."
} finally {
    # Remove junctions themselves first; never recursively clean through a link.
    foreach($link in $links) {
        if (Test-Path -LiteralPath $link) {
            Assert (([IO.File]::GetAttributes($link) -band [IO.FileAttributes]::ReparsePoint) -ne 0) 'Cleanup link changed type'
            [IO.Directory]::Delete($link)
        }
    }
    if ([IO.Path]::GetDirectoryName($root) -eq $PSScriptRoot) { Remove-Item -LiteralPath $root -Recurse -Force }
}
