[CmdletBinding()]
param(
    [ValidateSet('x64', 'ARM64')]
    [string]$Arch = 'x64',

    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',

    [string]$MsBuildPath,
    [string]$ToolchainFile,

    # Opt out of WinUI; default is a complete CLI + GUI developer build.
    [switch]$SkipGui
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$archLower = $Arch.ToLowerInvariant()
$preset = "windows-$archLower-release"
$buildDir = Join-Path $projectRoot "out/build/$preset"
$guiProject = Join-Path $projectRoot 'gui/gpu_bench_gui.vcxproj'
$guiOutDir = Join-Path $projectRoot "gui/$Arch/$Configuration"

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Program,
        [Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments
    )
    Write-Host ('> ' + $Program + ' ' + ($Arguments -join ' ')) -ForegroundColor DarkGray
    & $Program @Arguments
    if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE"
    }
}

function Find-MsBuild {
    if ($MsBuildPath) {
        $candidate = [IO.Path]::GetFullPath($MsBuildPath)
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            throw "MSBuild was not found: $candidate"
        }
        return $candidate
    }

    $command = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $candidate = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild `
            -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return $candidate
        }
    }

    throw 'MSBuild.exe was not found. Install Visual Studio with Desktop development with C++ / Windows App SDK, or pass -MsBuildPath.'
}

function Resolve-VcpkgToolchain {
    if ($ToolchainFile) {
        $path = [IO.Path]::GetFullPath($ToolchainFile)
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "vcpkg toolchain was not found: $path"
        }
        return $path
    }
    if ($env:VCPKG_ROOT) {
        $path = Join-Path $env:VCPKG_ROOT 'scripts/buildsystems/vcpkg.cmake'
        if (Test-Path -LiteralPath $path -PathType Leaf) { return [IO.Path]::GetFullPath($path) }
    }
    if (Test-Path -LiteralPath 'C:\vcpkg\scripts\buildsystems\vcpkg.cmake') {
        if (-not $env:VCPKG_ROOT) { $env:VCPKG_ROOT = 'C:\vcpkg' }
        return 'C:\vcpkg\scripts\buildsystems\vcpkg.cmake'
    }
    throw 'A vcpkg toolchain is required. Set VCPKG_ROOT (e.g. C:\vcpkg) or pass -ToolchainFile.'
}

Resolve-VcpkgToolchain | Out-Null

Write-Host "Building Mangekyo Windows $Arch $Configuration (CLI$(if ($SkipGui) { '' } else { ' + GUI' }))..." -ForegroundColor Cyan

if ($Configuration -eq 'Release') {
    Invoke-Checked cmake '--preset' $preset
    Invoke-Checked cmake '--build' '--preset' $preset '--parallel'
} else {
    # Debug uses the same binaryDir layout without a dedicated preset.
    $triplet = if ($Arch -eq 'ARM64') { 'arm64-windows' } else { 'x64-windows' }
    $toolchain = Resolve-VcpkgToolchain
    Invoke-Checked cmake '-S' $projectRoot '-B' $buildDir '-A' $Arch `
        "-DCMAKE_TOOLCHAIN_FILE=$toolchain" `
        "-DVCPKG_TARGET_TRIPLET=$triplet" `
        '-G' 'Visual Studio 18 2026'
    Invoke-Checked cmake '--build' $buildDir '--config' $Configuration '--parallel'
}

$cliExe = Join-Path $buildDir "$Configuration/gpu_benchmark.exe"
if (-not (Test-Path -LiteralPath $cliExe -PathType Leaf)) {
    throw "CLI build succeeded but gpu_benchmark.exe was not found: $cliExe"
}
Write-Host "CLI: $cliExe" -ForegroundColor Green

if ($SkipGui) {
    Write-Host 'Skipped GUI (-SkipGui).' -ForegroundColor Yellow
    return
}

$triplet = if ($Arch -eq 'ARM64') { 'arm64-windows' } else { 'x64-windows' }
$vcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { 'C:\vcpkg' }
$vcpkgLib = Join-Path $vcpkgRoot "installed/$triplet/lib"
$vcpkgBin = Join-Path $vcpkgRoot "installed/$triplet/bin"
if (-not (Test-Path -LiteralPath (Join-Path $vcpkgBin 'glfw3.dll') -PathType Leaf)) {
    $vcpkgLib = Join-Path $buildDir "vcpkg_installed/$triplet/lib"
    $vcpkgBin = Join-Path $buildDir "vcpkg_installed/$triplet/bin"
}
if (-not (Test-Path -LiteralPath (Join-Path $vcpkgBin 'glfw3.dll') -PathType Leaf)) {
    throw "glfw3.dll was not found under $vcpkgBin. Configure/build the CLI first so vcpkg restores GLFW."
}

$msbuild = Find-MsBuild
Invoke-Checked $msbuild $guiProject '/restore' '/m' `
    "/p:Configuration=$Configuration" "/p:Platform=$Arch" `
    "/p:GpuBuildDir=$buildDir" "/p:GpuSourceDir=$projectRoot" `
    "/p:VcpkgLib=$vcpkgLib" "/p:VcpkgBin=$vcpkgBin"

$guiExe = Join-Path $guiOutDir 'gpu_bench_gui.exe'
$workerBesideGui = Join-Path $guiOutDir 'gpu_benchmark.exe'
$shaderProbe = Join-Path $guiOutDir 'compute.hlsl'
if (-not (Test-Path -LiteralPath $guiExe -PathType Leaf)) {
    throw "GUI build completed but gpu_bench_gui.exe was not found: $guiExe"
}
if (-not (Test-Path -LiteralPath $workerBesideGui -PathType Leaf)) {
    throw "GUI output is missing adjacent gpu_benchmark.exe: $workerBesideGui"
}
if (-not (Test-Path -LiteralPath $shaderProbe -PathType Leaf)) {
    throw "GUI output is missing worker shader assets (expected $shaderProbe). CopyGpuBenchmarkWorker did not stage HLSL/SPIR-V/OpenGL files."
}

Write-Host "GUI: $guiExe" -ForegroundColor Green
Write-Host 'Developer build complete (CLI + GUI with adjacent worker/shaders).' -ForegroundColor Green
