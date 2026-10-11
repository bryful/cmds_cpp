param([Parameter(Mandatory=$true)][string]$Exe)
$ErrorActionPreference='Stop'
$runner=(Resolve-Path -LiteralPath $Exe).Path
$root=Join-Path $PSScriptRoot ('out\rb-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root -Force | Out-Null
function Assert($condition,$message) { if (!$condition) { throw $message } }
function CheckLog($folder,$expectedLines) {
    $logs=@(Get-ChildItem -LiteralPath $folder -Filter 'log*.txt')
    Assert ($logs.Count -gt 0) 'Missing log'
    $lines=0
    foreach($log in $logs) {
        $bytes=[IO.File]::ReadAllBytes($log.FullName)
        Assert (!($bytes -contains [byte]0)) 'Log contains NUL bytes'
        # CP932 and UTF-8 both represent the surrounding ASCII delimiters identically.
        $text=[Text.Encoding]::ASCII.GetString($bytes)
        foreach($line in ($text -split "`r?`n" | Where-Object {$_})) {
            Assert ($line -match '^".+" >> ".+"$') 'Truncated log record'
            $lines++
        }
    }
    Assert ($lines -eq $expectedLines) 'Unexpected log record count'
}
function Run($folder) {
    & $runner $folder | Out-Null
    Assert ($LASTEXITCODE -eq 0) 'rb execution failed'
}

$basic=Join-Path $root 'basic'
New-Item -ItemType Directory -Path $basic | Out-Null
$japanese=[string][char]0x4f5c+[char]0x54c1
$old='[author(alias)] '+$japanese+'.zip'
$new='[alias] '+$japanese+'.zip'
[IO.File]::WriteAllText((Join-Path $basic $old),'payload')
Run $basic
Assert ([IO.File]::ReadAllText((Join-Path $basic $new)) -eq 'payload') 'Basic rename/content mismatch'
CheckLog $basic 1
Run $basic
CheckLog $basic 1

$collision=Join-Path $root 'collision'
New-Item -ItemType Directory -Path $collision | Out-Null
[IO.File]::WriteAllText((Join-Path $collision '[alias].zip'),'existing')
[IO.File]::WriteAllText((Join-Path $collision '[author(alias)].zip'),'new')
Run $collision
Assert ([IO.File]::ReadAllText((Join-Path $collision '[alias].zip')) -eq 'existing') 'Existing file changed'
Assert ([IO.File]::ReadAllText((Join-Path $collision '[alias]_1.zip')) -eq 'new') 'Collision suffix failed'
CheckLog $collision 1

$normalization=Join-Path $root 'normalization'
New-Item -ItemType Directory -Path $normalization | Out-Null
$nfd=[string][char]0x304b+[char]0x3099
$nfc=[string][char]0x304c
[IO.File]::WriteAllText((Join-Path $normalization ('[author(alias)] '+$nfd+'.zip')),'composed')
New-Item -ItemType Directory -Path (Join-Path $normalization ('[author(alias)] '+$nfd)) | Out-Null
Run $normalization
Assert ([IO.File]::ReadAllText((Join-Path $normalization ('[alias] '+$nfc+'.zip'))) -eq 'composed') 'NFC filename failed'
Assert (Test-Path -LiteralPath (Join-Path $normalization ('[alias] '+$nfc)) -PathType Container) 'NFC directory failed'
CheckLog $normalization 2
Write-Output "PASS: Japanese names, unchanged second run, collision suffix, NFC file/directory, log integrity. Fixtures: $root"
