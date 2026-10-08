param([Parameter(Mandatory=$true)][string]$Exe)
$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot ('rh-test-'+[guid]::NewGuid())
New-Item -ItemType Directory $root | Out-Null
$runner=Join-Path $root 'rh.exe'
Copy-Item -LiteralPath (Resolve-Path $Exe).Path -Destination $runner
$rules=Join-Path $root 'rh.lst'
function Assert($condition,$message) { if (!$condition) { throw $message } }
function Case($name) { $p=Join-Path $root $name; New-Item -ItemType Directory $p | Out-Null; return $p }
function File($p,$name,$content='keep') { [IO.File]::WriteAllText((Join-Path $p $name),$content) }
function Rules($text,$bom=$false) { [IO.File]::WriteAllText($rules,$text,[Text.UTF8Encoding]::new($bom)) }
try {
    Rules '(tag)' $true
    $p=Case 'basic'
    File $p '(tag)Ａ  Ｂ.ZIP' 'zip'
    File $p '(tag)Ｃ.txt' 'unsupported'
    New-Item -ItemType Directory (Join-Path $p '(tag)Ｄ') | Out-Null
    & $runner $p
    Assert ($LASTEXITCODE -eq 0) 'Basic rename failed'
    Assert ([IO.File]::ReadAllText((Join-Path $p 'A B.ZIP')) -eq 'zip') 'BOM/width/space rules failed'
    Assert (Test-Path (Join-Path $p 'D') -PathType Container) 'Directory rename failed'
    Assert (Test-Path (Join-Path $p '(tag)Ｃ.txt')) 'Unsupported extension changed'
    $output=& $runner $p
    Assert ($LASTEXITCODE -eq 0 -and ($output -join '') -match 'Renamed: 0') 'Unchanged names not recognized'

    $p=Case 'unicode'; $name='e'+[char]0x301+'.mp4'; File $p $name
    & $runner $p
    Assert ($LASTEXITCODE -eq 0 -and (Test-Path (Join-Path $p ([string][char]0xe9+'.mp4')))) 'NFC normalization failed'

    $p=Case 'collision'
    File $p 'A.zip' 'original'; File $p 'Ａ.zip' 'wide'; File $p 'A_1.zip' 'existing suffix'
    & $runner $p
    Assert ($LASTEXITCODE -eq 0) 'Collision rename failed'
    Assert ([IO.File]::ReadAllText((Join-Path $p 'A.zip')) -eq 'original') 'Original destination overwritten'
    Assert ([IO.File]::ReadAllText((Join-Path $p 'A_1.zip')) -eq 'existing suffix') 'Existing suffix overwritten'
    Assert ([IO.File]::ReadAllText((Join-Path $p 'A_2.zip')) -eq 'wide') 'Suffix allocation failed'

    $p=Case 'locked'; File $p 'Ａ.zip' 'locked'; File $p 'Ｂ.zip' 'continue'
    $lock=[IO.File]::Open((Join-Path $p 'Ａ.zip'),'Open','Read','None')
    try { & $runner $p; Assert ($LASTEXITCODE -ne 0) 'Locked source returned success' }
    finally { $lock.Dispose() }
    Assert (Test-Path (Join-Path $p 'Ａ.zip')) 'Locked source changed'
    Assert (Test-Path (Join-Path $p 'B.zip')) 'Rename did not continue after failure'

    $p=Case 'empty'; File $p '(tag).zip'
    & $runner $p
    Assert ($LASTEXITCODE -ne 0 -and (Test-Path (Join-Path $p '(tag).zip'))) 'Empty transformed name accepted'

    $p=Case 'invalid-rule'; File $p 'Ａ.zip'
    [IO.File]::WriteAllBytes($rules,[byte[]](255,254,255))
    & $runner $p
    Assert ($LASTEXITCODE -ne 0 -and (Test-Path (Join-Path $p 'Ａ.zip'))) 'Invalid UTF-8 accepted'
    Assert ([IO.File]::ReadAllBytes($rules)[0] -eq 255) 'Invalid rules overwritten'

    $p=Case 'path-launch'; File $p '(tag)Ａ.zip'; Rules '(tag)' $true
    $cwd=Case 'cwd'; $oldPath=$env:PATH
    Push-Location $cwd
    try {
        $env:PATH=$root+';'+$env:PATH
        & cmd.exe /d /c ('rh.exe "'+$p+'"')
        Assert ($LASTEXITCODE -eq 0) 'PATH launch failed'
    } finally { $env:PATH=$oldPath; Pop-Location }
    Assert (Test-Path (Join-Path $p 'A.zip')) 'PATH launch ignored adjacent rules'
    Assert (!(Test-Path (Join-Path $cwd 'rh.lst'))) 'Rules created in current directory'

    & $runner (Join-Path $p 'A.zip')
    Assert ($LASTEXITCODE -ne 0) 'File input accepted as directory'

    $p=Case 'default-rules'; Remove-Item -LiteralPath $rules; File $p '(成年コミック)Ａ.zip'
    & $runner $p
    Assert ($LASTEXITCODE -eq 0 -and (Test-Path (Join-Path $p 'A.zip')) -and (Test-Path $rules)) 'Default rules failed'
    Write-Output 'All 9 cases passed.'
} finally {
    if ([IO.Path]::GetDirectoryName($root) -eq $PSScriptRoot) { Remove-Item -LiteralPath $root -Recurse -Force }
}
