param([Parameter(Mandatory = $true)][string]$Exe)
$ErrorActionPreference = 'Stop'
$Exe = (Resolve-Path -LiteralPath $Exe).Path
Add-Type -AssemblyName System.Drawing
$root = Join-Path (Split-Path -Parent $Exe) ('png2jpeg-test-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null

function Assert-True([bool]$Condition, [string]$Message)
{
    if (!$Condition) { throw $Message }
}

function New-Png([string]$Path, [int]$Alpha = 255)
{
    $bitmap = [Drawing.Bitmap]::new(32, 32)
    try
    {
        for ($y = 0; $y -lt 32; ++$y)
        {
            for ($x = 0; $x -lt 32; ++$x)
            {
                $bitmap.SetPixel($x, $y, [Drawing.Color]::FromArgb($Alpha, 200, 100, 50))
            }
        }
        $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png)
    }
    finally { $bitmap.Dispose() }
}

function Invoke-App([string[]]$AppArgs, [int]$Code = 0)
{
    & $Exe @AppArgs
    Assert-True ($LASTEXITCODE -eq $Code) "Unexpected exit code: $LASTEXITCODE, expected $Code"
}

Push-Location $root
try
{
    New-Png (Join-Path $root 'single.png')
    Invoke-App -AppArgs @('single.png')
    Assert-True ((Test-Path 'single.jpeg') -and !(Test-Path 'single.png')) 'Default deletion failed'

    New-Png (Join-Path $root 'single.png')
    $hash = (Get-FileHash 'single.jpeg').Hash
    Invoke-App -AppArgs @('single.png')
    Assert-True ((Test-Path 'single.png') -and ((Get-FileHash 'single.jpeg').Hash -eq $hash)) 'Existing output was changed'
    Invoke-App -AppArgs @('-overwrite', 'single.png', '-nodelete')
    Assert-True (Test-Path 'single.png') 'nodelete failed'
    Invoke-App -AppArgs @('single.png', '-overwrite')
    Assert-True (!(Test-Path 'single.png')) 'Overwrite did not delete input'

    $directory = Join-Path $root '日本語 空白'
    [IO.Directory]::CreateDirectory((Join-Path $directory 'sub')) | Out-Null
    New-Png (Join-Path $directory 'alpha1.PNG') 128
    New-Png (Join-Path $directory 'alpha2.png') 0
    New-Png (Join-Path $directory 'other.png')
    New-Png (Join-Path $directory 'sub/child.png')
    Invoke-App -AppArgs @((Join-Path $directory 'ALPHA?.pNg'), '-nodelete')
    Assert-True (!(Test-Path (Join-Path $directory 'other.jpeg'))) 'Wildcard matched unrelated file'
    foreach ($item in @(@('alpha1.jpeg', 100, 50, 25), @('alpha2.jpeg', 0, 0, 0)))
    {
        $image = [Drawing.Bitmap]::new((Join-Path $directory $item[0]))
        try
        {
            Assert-True ($image.Width -eq 32 -and $image.Height -eq 32) 'Wrong dimensions'
            $color = $image.GetPixel(16, 16)
            Assert-True ([Math]::Abs($color.R - $item[1]) -le 5 -and [Math]::Abs($color.G - $item[2]) -le 5 -and [Math]::Abs($color.B - $item[3]) -le 5) 'Black alpha composition failed'
        }
        finally { $image.Dispose() }
    }
    Invoke-App -AppArgs @($directory)
    Assert-True ((Test-Path (Join-Path $directory 'other.jpeg')) -and (Test-Path (Join-Path $directory 'sub/child.png')) -and !(Test-Path (Join-Path $directory 'sub/child.jpeg'))) 'Directory processing failed'

    New-Png (Join-Path $root 'current.png')
    Invoke-App -AppArgs @()
    Assert-True ((Test-Path 'current.jpeg') -and !(Test-Path 'current.png')) 'Current directory default failed'
    [IO.File]::WriteAllText((Join-Path $root 'broken.png'), 'invalid png')
    [IO.File]::WriteAllText((Join-Path $root 'broken.jpeg'), 'existing output')
    New-Png (Join-Path $root 'valid.png')
    Invoke-App -AppArgs @($root, '-overwrite') -Code 1
    Assert-True ((Test-Path 'broken.png') -and ([IO.File]::ReadAllText((Join-Path $root 'broken.jpeg')) -eq 'existing output')) 'Failed conversion damaged original data'
    Assert-True ((Test-Path 'valid.jpeg') -and !(Test-Path 'valid.png')) 'Batch did not continue'
    Invoke-App -AppArgs @('missing.png') -Code 1
    Invoke-App -AppArgs @('single.jpeg') -Code 1
    Invoke-App -AppArgs @('-unknown') -Code 2
    Invoke-App -AppArgs @('a.png', 'b.png') -Code 2
    Invoke-App -AppArgs @('no-match-*.png')
    $longDirectory = '\\?\' + $root
    foreach ($part in 1..4) { $longDirectory = $longDirectory + '\' + ('long-directory-' + ('x' * 40) + $part) }
    [IO.Directory]::CreateDirectory($longDirectory) | Out-Null
    [IO.File]::Copy((Join-Path $directory 'alpha1.PNG'), ($longDirectory + '\long.png'))
    Invoke-App -AppArgs @(($longDirectory + '\long.png'), '-nodelete')
    Assert-True ([IO.File]::Exists($longDirectory + '\long.jpeg')) 'Long path conversion failed'
    New-Png (Join-Path $root 'readonly.png')
    [IO.File]::WriteAllText((Join-Path $root 'readonly.jpeg'), 'protected output')
    [IO.File]::SetAttributes((Join-Path $root 'readonly.jpeg'), [IO.FileAttributes]::ReadOnly)
    try
    {
        Invoke-App -AppArgs @('readonly.png', '-overwrite') -Code 1
        Assert-True ((Test-Path 'readonly.png') -and ([IO.File]::ReadAllText((Join-Path $root 'readonly.jpeg')) -eq 'protected output')) 'Write failure damaged data'
    }
    finally { [IO.File]::SetAttributes((Join-Path $root 'readonly.jpeg'), [IO.FileAttributes]::Normal) }
    New-Png (Join-Path $root 'delete-failure.png')
    [IO.File]::SetAttributes((Join-Path $root 'delete-failure.png'), [IO.FileAttributes]::ReadOnly)
    try
    {
        Invoke-App -AppArgs @('delete-failure.png') -Code 1
        Assert-True ((Test-Path 'delete-failure.png') -and (Test-Path 'delete-failure.jpeg')) 'Delete failure did not preserve both files'
    }
    finally { [IO.File]::SetAttributes((Join-Path $root 'delete-failure.png'), [IO.FileAttributes]::Normal) }
    Assert-True (@(Get-ChildItem -LiteralPath $root -Recurse -Filter '.png2jpeg-*.tmp').Count -eq 0) 'Temporary files remain'
    Write-Host 'All png2jpeg tests passed.'
}
finally { Pop-Location }
