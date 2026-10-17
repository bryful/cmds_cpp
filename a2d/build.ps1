param(
    [ValidateSet('Debug','Release')][string]$Configuration='Release',
    [ValidateSet('x64','Win32')][string]$Platform='x64',
    [string]$OutputDirectory,
    [string]$IntermediateDirectory,
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
$configurationPath=if ($Platform -eq 'Win32') { $Configuration } else { Join-Path $Platform $Configuration }
if (!$IntermediateDirectory) {
    $IntermediateDirectory=Join-Path $PSScriptRoot $configurationPath
}
if (!$OutputDirectory) {
    $OutputDirectory=Join-Path (Split-Path -Parent $PSScriptRoot) $configurationPath
}
$build=Join-Path $IntermediateDirectory 'cmake'
$arguments=@('-S',$PSScriptRoot,'-B',$build,'-A',$Platform,'-T','host=x64')
if ($DisablePdb) { $arguments+='-DA2D_DISABLE_PDB=ON' }
& $cmake @arguments
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
if ($Clean) { & $cmake --build $build --config $Configuration --target clean -- /p:PreferredToolArchitecture=x64 }
else { & $cmake --build $build --config $Configuration --target a2d --parallel -- /p:PreferredToolArchitecture=x64 }
if ($LASTEXITCODE -ne 0) { throw 'CMake build failed' }
if (!$Clean -and $OutputDirectory) {
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $build ($Configuration+'\a2d.exe')) -Destination (Join-Path $OutputDirectory 'a2d.exe') -Force
}
