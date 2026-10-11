param([string]$Exe = "$PSScriptRoot\..\x64\Release\findName.exe")
$ErrorActionPreference = 'Stop'
$OutputEncoding = [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
$root = Join-Path $PSScriptRoot ('out\test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path "$root\nested", "$root\sample-parent" -Force | Out-Null
foreach ($name in @('Sample.ZIP','nested\BACKUP.zip','nested\SampleBackup.zip','nested\日本語 Sample.zip','nested\ÄPFEL.zip','sample.txt','nested\BACKUP.TXT','nested\SampleBackup.TXT','sample-parent\unrelated.zip')) {
    [IO.File]::WriteAllText((Join-Path $root $name), '')
}
$count = 0
function Check($Arguments, $Expected, $Code = 0) {
    $actual = @(& $Exe @Arguments 2>$null)
    if ($LASTEXITCODE -ne $Code) { throw "Unexpected exit code: $LASTEXITCODE for $Arguments" }
    $wanted = @($Expected | ForEach-Object { Join-Path $root $_ })
    if (@(Compare-Object @($actual | Sort-Object) @($wanted | Sort-Object)).Count) {
        throw "Unexpected paths for $Arguments : $actual"
    }
    $script:count++
}
Check @($root,'sample','backup') @('nested\SampleBackup.zip')
Check @($root,'sample','backup','-e','txt') @('nested\SampleBackup.TXT')
Check @($root,'backup','-e','.TXT') @('nested\BACKUP.TXT','nested\SampleBackup.TXT')
Check @($root,'日本語') @('nested\日本語 Sample.zip')
Check @($root,'äpfel') @('nested\ÄPFEL.zip')
# Empty expected sets are checked separately (Compare-Object requires non-null arrays).
foreach ($arguments in @(@($root,'absent'), @($root,'sample-parent'), @($root,'sample','absent'))) {
    $actual = @(& $Exe @arguments)
    if ($LASTEXITCODE -ne 0 -or $actual.Count -ne 0) { throw 'Expected no matches' }
    $count++
}
foreach ($arguments in @(@($root,'sample','-e'), @($root,'sample','-e','.'), @($root,'-e','txt'), @($root,'sample','--bad'), @($root,'sample','-e','zip','-e','txt'), @($root,''))) {
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $Exe @arguments 2>$null | Out-Null
    $code = $LASTEXITCODE
    $ErrorActionPreference = $oldPreference
    if ($code -ne 2) { throw "Expected argument error: $arguments" }
    $count++
}
& $Exe --help | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Help failed' }
$count++
# cmd.exe redirects the native handles directly, without PowerShell transcoding.
$exePath = (Resolve-Path $Exe).Path
$redirected = Join-Path $root 'redirected.txt'
$command = '""' + $exePath + '" "' + $root + '" sample > "' + $redirected + '""'
& $env:ComSpec /d /s /c $command
if ($LASTEXITCODE -ne 0) { throw 'Native file redirection failed' }
$utf8 = [Text.UTF8Encoding]::new($false, $true)
$bytes = [IO.File]::ReadAllBytes($redirected)
if ($bytes.Length -eq 0) { throw 'Redirected output was empty' }
$lines = @($utf8.GetString($bytes) -split '\r?\n' | Where-Object { $_ })
$expected = @('Sample.ZIP','nested\SampleBackup.zip','nested\日本語 Sample.zip' | ForEach-Object { Join-Path $root $_ })
if (@(Compare-Object ($lines | Sort-Object) ($expected | Sort-Object)).Count) { throw 'Redirected UTF-8 paths differ' }
$count++
$command = '""' + $exePath + '" "' + $root + '" sample -e . > "' + $redirected + '" 2> "' + (Join-Path $root 'errors.txt') + '""'
& $env:ComSpec /d /s /c $command
if ($LASTEXITCODE -ne 2 -or (Get-Item (Join-Path $root 'errors.txt')).Length -eq 0) { throw 'Stderr redirection failed' }
$count++
& $exePath $root sample > $redirected
if ($LASTEXITCODE -ne 0 -or (Get-Item $redirected).Length -eq 0) { throw 'PowerShell file redirection failed' }
$lines = @(Get-Content $redirected)
if (@(Compare-Object ($lines | Sort-Object) ($expected | Sort-Object)).Count) { throw 'PowerShell redirected paths differ' }
$count++
# Validate raw BOM bytes, including no matches and combination with -e/AND.
foreach ($case in @(
    @{ Args = 'sample -bom'; Paths = $expected },
    @{ Args = '-bom sample backup -e txt'; Paths = @((Join-Path $root 'nested\SampleBackup.TXT')) },
    @{ Args = 'absent -bom'; Paths = @() }
)) {
    $command = '""' + $exePath + '" "' + $root + '" ' + $case.Args + ' > "' + $redirected + '""'
    & $env:ComSpec /d /s /c $command
    if ($LASTEXITCODE -ne 0) { throw 'BOM search failed' }
    $bytes = [IO.File]::ReadAllBytes($redirected)
    if ($bytes.Length -lt 3 -or $bytes[0] -ne 0xEF -or $bytes[1] -ne 0xBB -or $bytes[2] -ne 0xBF) {
        throw 'Missing UTF-8 BOM'
    }
    $body = $utf8.GetString($bytes, 3, $bytes.Length - 3)
    if ($body.Contains([char]0xFEFF)) { throw 'Duplicate BOM' }
    $lines = @($body -split '\r?\n' | Where-Object { $_ })
    if ($lines.Count -ne $case.Paths.Count) { throw 'Unexpected BOM output line count' }
    if ($lines.Count -and @(Compare-Object ($lines | Sort-Object) ($case.Paths | Sort-Object)).Count) {
        throw 'Unexpected BOM output paths'
    }
    $count++
}
Write-Host "$count checks passed. Fixtures: $root"
