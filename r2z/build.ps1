param(
    [ValidateSet('Debug','Release')][string]$Configuration='Release',
    [ValidateSet('x64','Win32')][string]$Platform='x64',
    [string]$OutputDirectory,
    [switch]$DisablePdb,
    [switch]$Clean
)
$ErrorActionPreference='Stop'
$cmake=(Get-Command cmake -ErrorAction SilentlyContinue).Source
if (!$cmake) {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $cmake=(& $vswhere -latest -products '*' -find 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' | Select-Object -First 1)
}
if (!$cmake) { throw 'CMake 3.24+ is required (Visual Studio C++ CMake tools or PATH).' }
$build=Join-Path $PSScriptRoot ('out\build\'+$Platform)
$arguments=@('-S',$PSScriptRoot,'-B',$build,'-A',$Platform)
if ($DisablePdb) { $arguments+='-DR2Z_DISABLE_PDB=ON' }
& $cmake @arguments
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
if ($Clean) { & $cmake --build $build --config $Configuration --target clean }
else { & $cmake --build $build --config $Configuration --target r2z --parallel }
if ($LASTEXITCODE -ne 0) { throw 'CMake build failed' }
if (!$Clean -and $OutputDirectory) {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $build ($Configuration+'\r2z.exe')) -Destination (Join-Path $OutputDirectory 'r2z.exe') -Force
}
