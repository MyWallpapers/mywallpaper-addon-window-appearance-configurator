param([string]$BuildDirectory = '', [switch]$SkipIntegration, [switch]$SkipHostQualification)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (!$BuildDirectory) { $BuildDirectory = Join-Path $repoRoot '.build' }
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Visual Studio 2022 C++ x64 build tools are required.' }
$cmakeBin = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$cmake = Join-Path $cmakeBin 'cmake.exe'
$ctest = Join-Path $cmakeBin 'ctest.exe'
Push-Location $repoRoot
try {
    & node tools/generate.mjs --check
    if ($LASTEXITCODE) { throw 'Generated manifest/profile contract is stale.' }
    & node tests/private-capability.test.mjs
    if ($LASTEXITCODE) { throw 'Optional capability request proof failed.' }
    & $cmake -S $repoRoot -B $BuildDirectory -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
    & $cmake --build $BuildDirectory --config Release --parallel 4
    if ($LASTEXITCODE) { throw 'Windows build failed.' }
    $testArgs = @('--test-dir', $BuildDirectory, '-C', 'Release', '--output-on-failure')
    if ($SkipHostQualification) { $testArgs += @('-R', '^appearance_contract$') }
    & $ctest @testArgs
    if ($LASTEXITCODE) { throw 'Contract or Windows tests failed.' }
    if (!$SkipIntegration -and !$SkipHostQualification) {
        & python tests/integration.py (Join-Path $BuildDirectory 'Release')
        if ($LASTEXITCODE) { throw 'process-v2 / crash recovery integration failed.' }
    }
    $destination = Join-Path $PSScriptRoot 'out/windows-x86_64'
    New-Item -ItemType Directory -Force -Path (Join-Path $destination 'notices') | Out-Null
    Copy-Item -LiteralPath (Join-Path $BuildDirectory 'Release/companion.exe') -Destination (Join-Path $destination 'companion.exe')
    Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE') -Destination (Join-Path $destination 'LICENSE')
    foreach ($notice in Get-ChildItem -LiteralPath (Join-Path $repoRoot 'notices') -File) {
        Copy-Item -LiteralPath $notice.FullName -Destination (Join-Path $destination ('notices/' + $notice.Name))
    }
    Write-Output "Runtime bundle: $destination"
} finally { Pop-Location }
