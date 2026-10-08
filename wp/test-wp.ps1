param([Parameter(Mandatory=$true)][string]$Exe)
$ErrorActionPreference = 'Stop'
$Exe = (Resolve-Path $Exe).Path
$root = Join-Path $PSScriptRoot ('wp-test-' + [guid]::NewGuid())
New-Item -ItemType Directory $root | Out-Null
$sample = Join-Path $PSScriptRoot '..\libwebp-1.6.0-windows-x64\test.webp'
function Assert($condition, $message) { if (!$condition) { throw $message } }
function Case($name) { $p = Join-Path $root $name; New-Item -ItemType Directory $p | Out-Null; return $p }
try {
    $p = Case 'success'
    $name = 'e' + [char]0x301 + '_999999999999999999999999.webp'
    $source = Join-Path $p $name
    Copy-Item $sample $source
    & $Exe $p
    Assert ($LASTEXITCODE -eq 0) 'Unicode/long number conversion failed'
    Assert (!(Test-Path -LiteralPath $source)) 'Successful source not removed'
    $jpeg = [IO.Path]::ChangeExtension($source, '.jpeg')
    $bytes = [IO.File]::ReadAllBytes($jpeg)
    Assert ($bytes[0] -eq 255 -and $bytes[1] -eq 216 -and $bytes[-2] -eq 255 -and $bytes[-1] -eq 217) 'Invalid JPEG markers'

    $decodeInput = Join-Path $p 'decode.jpeg'
    Copy-Item -LiteralPath $jpeg -Destination $decodeInput
    & (Join-Path $PSScriptRoot '..\libjpeg-turbo64\bin\djpeg.exe') -bmp -outfile (Join-Path $p 'decoded.bmp') $decodeInput
    Assert ($LASTEXITCODE -eq 0) 'JPEG decoding failed'

    $p = Case 'existing'
    Copy-Item $sample (Join-Path $p 'x.webp')
    [IO.File]::WriteAllText((Join-Path $p 'x.jpeg'), 'keep')
    & $Exe $p
    Assert ($LASTEXITCODE -ne 0) 'Existing destination accepted'
    Assert ((Test-Path (Join-Path $p 'x.webp')) -and [IO.File]::ReadAllText((Join-Path $p 'x.jpeg')) -eq 'keep') 'Existing files changed'

    $p = Case 'collision'
    Copy-Item $sample (Join-Path $p 'img001.webp')
    Copy-Item $sample (Join-Path $p 'imgi_1_1.webp')
    & $Exe $p
    Assert ($LASTEXITCODE -ne 0) 'Duplicate destination accepted'
    Assert ((Get-ChildItem $p -Filter '*.webp').Count -eq 2) 'Collision deleted sources'
    Assert (!(Test-Path (Join-Path $p 'img001.jpeg'))) 'Collision produced output'

    $p = Case 'mixed'
    [IO.File]::WriteAllText((Join-Path $p 'bad.webp'), 'invalid')
    Copy-Item $sample (Join-Path $p 'imgi_999999999999_999999999999.WEBP')
    & $Exe $p
    Assert ($LASTEXITCODE -ne 0) 'Corrupt input returned success'
    Assert (Test-Path (Join-Path $p 'bad.webp')) 'Corrupt source deleted'
    Assert (Test-Path (Join-Path $p 'img999999999999.jpeg')) 'Batch did not continue'

    $p = Case 'locked'
    Copy-Item $sample (Join-Path $p 'x.webp')
    $lock = [IO.File]::Open((Join-Path $p 'x.webp'), 'Open', 'Read', 'None')
    try { & $Exe $p; Assert ($LASTEXITCODE -ne 0) 'Locked input returned success' }
    finally { $lock.Dispose() }
    Assert (Test-Path (Join-Path $p 'x.webp')) 'Locked input deleted'
    Assert ((Get-ChildItem $root -Recurse -Filter '*.tmp').Count -eq 0) 'Temporary files leaked'
    Write-Output 'All 5 cases passed.'
} finally {
    if ([IO.Path]::GetDirectoryName($root) -eq $PSScriptRoot) {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}
