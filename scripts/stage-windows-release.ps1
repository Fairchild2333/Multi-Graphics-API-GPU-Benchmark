[CmdletBinding()]
param(
    [string]$BuildDir,
    [string]$StageDir,
    [string]$PackageDir,
    [string]$ToolchainFile,
    [string]$GuiPayloadDir,
    [string]$MsBuildPath,
    [string]$RenderDocDir,
    [string]$ReportWorkerDir,
    [string]$ProjectLicenseFile,
    [ValidateSet('Release')]
    [string]$Configuration = 'Release',
    [switch]$SkipGui,
    [switch]$BuildGui,
    [switch]$SkipConfigure,
    [switch]$SkipBuild,
    [switch]$SkipPackage,
    [switch]$NoClean,
    [switch]$RequirePortable
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$outRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'out'))
if (-not $BuildDir) { $BuildDir = Join-Path $outRoot 'build/windows-x64-release' }
if (-not $StageDir) { $StageDir = Join-Path $outRoot 'stage/windows-x64' }
if (-not $PackageDir) { $PackageDir = Join-Path $outRoot 'packages' }
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$StageDir = [IO.Path]::GetFullPath($StageDir)
$PackageDir = [IO.Path]::GetFullPath($PackageDir)

if ($SkipGui -and $BuildGui) {
    throw '-SkipGui and -BuildGui are mutually exclusive.'
}
if ($BuildGui -and $SkipConfigure) {
    throw '-BuildGui requires CMake reconfiguration so the new GUI payload is installed.'
}

function Invoke-Checked {
    param([Parameter(Mandatory = $true)][string]$Program,
          [Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments)
    Write-Host ('> ' + $Program + ' ' + ($Arguments -join ' ')) -ForegroundColor DarkGray
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE"
    }
}

function Reset-SafeOutputDirectory {
    param([Parameter(Mandatory = $true)][string]$Path)
    $full = [IO.Path]::GetFullPath($Path)
    $allowedPrefix = $outRoot.TrimEnd([IO.Path]::DirectorySeparatorChar) +
        [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($allowedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean a path outside the repository out directory: $full"
    }
    if (Test-Path -LiteralPath $full) {
        Remove-Item -LiteralPath $full -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $full | Out-Null
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
    throw 'MSBuild.exe was not found. Install Visual Studio Build Tools with Desktop development with C++, or pass -MsBuildPath.'
}

function Invoke-CMakeConfigure([string]$GuiDirectory) {
    $configureArgs = @(
        '-S', $projectRoot,
        '-B', $BuildDir,
        '-A', 'x64',
        "-DCMAKE_TOOLCHAIN_FILE=$ToolchainFile",
        '-DVCPKG_TARGET_TRIPLET=x64-windows',
        '-DGPU_BENCH_ENABLE_PACKAGING=ON',
        '-DGPU_BENCH_STRICT_RELEASE_ASSETS=ON',
        '-DGPU_BENCH_CPACK_GENERATORS=ZIP'
    )
    if ($GuiDirectory) { $configureArgs += "-DGPU_BENCH_GUI_PAYLOAD_DIR=$GuiDirectory" }
    else { $configureArgs += '-DGPU_BENCH_GUI_PAYLOAD_DIR=' }
    if ($RenderDocDir) { $configureArgs += "-DGPU_BENCH_RENDERDOC_DIR=$([IO.Path]::GetFullPath($RenderDocDir))" }
    else { $configureArgs += '-DGPU_BENCH_RENDERDOC_DIR=' }
    if ($ReportWorkerDir) { $configureArgs += "-DGPU_BENCH_REPORT_WORKER_DIR=$([IO.Path]::GetFullPath($ReportWorkerDir))" }
    else { $configureArgs += '-DGPU_BENCH_REPORT_WORKER_DIR=' }
    if ($ProjectLicenseFile) { $configureArgs += "-DGPU_BENCH_PACKAGE_LICENSE_FILE=$([IO.Path]::GetFullPath($ProjectLicenseFile))" }
    else { $configureArgs += '-DGPU_BENCH_PACKAGE_LICENSE_FILE=' }
    Invoke-Checked cmake @configureArgs
}

function Write-StageChecksums {
    $checksumPath = Join-Path $StageDir 'files.sha256'
    $lines = [Collections.Generic.List[string]]::new()
    Get-ChildItem -LiteralPath $StageDir -Recurse -File |
        Where-Object { $_.FullName -ne $checksumPath } |
        ForEach-Object {
            $relative = $_.FullName.Substring($StageDir.TrimEnd('\').Length + 1).Replace('\', '/')
            [pscustomobject]@{ Path = $relative; FullName = $_.FullName }
        } |
        Sort-Object Path |
        ForEach-Object {
            $digest = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            $lines.Add("$digest  $($_.Path)")
        }
    [IO.File]::WriteAllLines($checksumPath, $lines, [Text.Encoding]::ASCII)
    Write-Host "Stage checksums: $checksumPath ($($lines.Count) files)" -ForegroundColor Green
}

if (-not $ToolchainFile) {
    if ($env:VCPKG_ROOT) {
        $ToolchainFile = Join-Path $env:VCPKG_ROOT 'scripts/buildsystems/vcpkg.cmake'
    } elseif (Test-Path -LiteralPath 'C:\vcpkg\scripts\buildsystems\vcpkg.cmake') {
        $ToolchainFile = 'C:\vcpkg\scripts\buildsystems\vcpkg.cmake'
    }
}
if (-not $SkipConfigure) {
    if (-not $ToolchainFile -or -not (Test-Path -LiteralPath $ToolchainFile)) {
        throw 'A build-machine vcpkg toolchain is required. Set VCPKG_ROOT or pass -ToolchainFile.'
    }
}

if ($BuildGui -and -not $GuiPayloadDir) {
    $GuiPayloadDir = Join-Path $outRoot 'gui/windows-x64-release'
}
if (-not $SkipGui -and -not $BuildGui -and -not $GuiPayloadDir) {
    $candidate = Join-Path $projectRoot 'gui/x64/Release'
    if (Test-Path -LiteralPath (Join-Path $candidate 'gpu_bench_gui.exe')) {
        $GuiPayloadDir = $candidate
    }
}
if ($GuiPayloadDir) { $GuiPayloadDir = [IO.Path]::GetFullPath($GuiPayloadDir) }
if ($SkipGui) {
    $GuiPayloadDir = ''
    Write-Warning 'Creating a CLI-only smoke package. This is not the intended GUI-first release.'
} elseif (-not $GuiPayloadDir) {
    Write-Warning 'No prebuilt WinUI self-contained payload was found; falling back to a CLI-only smoke package.'
} elseif (-not $BuildGui -or (Test-Path -LiteralPath (Join-Path $GuiPayloadDir 'gpu_bench_gui.exe'))) {
    if (-not (Test-Path -LiteralPath (Join-Path $GuiPayloadDir 'gpu_bench_gui.exe'))) {
        throw "GUI payload does not contain gpu_bench_gui.exe: $GuiPayloadDir"
    }
}

if (-not $SkipConfigure) {
    # Packaging.cmake validates a GUI payload at configure time. During a full
    # release build the GUI does not exist yet, so configure/build the engine
    # first and reconfigure with the freshly built self-contained GUI below.
    $initialGuiPayload = if ($BuildGui -and -not (Test-Path -LiteralPath (Join-Path $GuiPayloadDir 'gpu_bench_gui.exe'))) { '' } else { $GuiPayloadDir }
    Invoke-CMakeConfigure $initialGuiPayload
}

if (-not $SkipBuild) {
    Invoke-Checked cmake '--build' $BuildDir '--config' $Configuration '--parallel'
}

if ($BuildGui) {
    if (-not $ToolchainFile) {
        throw 'The GUI build requires a vcpkg toolchain path.'
    }
    $vcpkgRoot = if ($env:VCPKG_ROOT) {
        [IO.Path]::GetFullPath($env:VCPKG_ROOT)
    } else {
        [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent (Split-Path -Parent $ToolchainFile)) '..'))
    }
    $vcpkgLib = Join-Path $vcpkgRoot 'installed/x64-windows/lib'
    $vcpkgBin = Join-Path $vcpkgRoot 'installed/x64-windows/bin'
    if (-not (Test-Path -LiteralPath (Join-Path $vcpkgBin 'glfw3.dll') -PathType Leaf)) {
        throw "The x64 vcpkg GLFW runtime was not found under $vcpkgBin"
    }

    if (-not $NoClean) { Reset-SafeOutputDirectory $GuiPayloadDir }
    else { New-Item -ItemType Directory -Force -Path $GuiPayloadDir | Out-Null }
    $guiIntermediate = Join-Path $outRoot 'build/gui-windows-x64-release'
    if (-not $NoClean) { Reset-SafeOutputDirectory $guiIntermediate }
    else { New-Item -ItemType Directory -Force -Path $guiIntermediate | Out-Null }

    $msbuild = Find-MsBuild
    $guiProject = Join-Path $projectRoot 'gui/gpu_bench_gui.vcxproj'
    # A trailing backslash immediately before the closing command-line quote
    # escapes that quote in MSBuild's Windows argument parser. Forward slashes
    # are accepted by MSBuild and keep OutDir/IntDir explicitly directory-like.
    $outWithSlash = $GuiPayloadDir.Replace('\', '/').TrimEnd('/') + '/'
    $intWithSlash = $guiIntermediate.Replace('\', '/').TrimEnd('/') + '/'
    Invoke-Checked $msbuild $guiProject '/restore' '/m:1' `
        "/p:Configuration=$Configuration" '/p:Platform=x64' `
        "/p:GpuBuildDir=$BuildDir" "/p:GpuSourceDir=$projectRoot" `
        "/p:VcpkgLib=$vcpkgLib" "/p:VcpkgBin=$vcpkgBin" `
        "/p:OutDir=$outWithSlash" "/p:IntDir=$intWithSlash"
    if (-not (Test-Path -LiteralPath (Join-Path $GuiPayloadDir 'gpu_bench_gui.exe') -PathType Leaf)) {
        throw "MSBuild completed but gpu_bench_gui.exe was not found in $GuiPayloadDir"
    }
    Invoke-CMakeConfigure $GuiPayloadDir
}

if (-not $NoClean) {
    Reset-SafeOutputDirectory $StageDir
} else {
    New-Item -ItemType Directory -Force -Path $StageDir | Out-Null
}
Invoke-Checked cmake '--install' $BuildDir '--config' $Configuration '--prefix' $StageDir

Write-StageChecksums

$verifyScript = Join-Path $PSScriptRoot 'verify-windows-stage.ps1'
$verifyArgs = @('-StageDir', $StageDir, '-SmokeTest', '-RequireChecksums')
if ($RequirePortable) { $verifyArgs += '-RequirePortable' }
Invoke-Checked powershell '-NoProfile' '-ExecutionPolicy' 'Bypass' '-File' $verifyScript @verifyArgs

if (-not $SkipPackage) {
    New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null
    Invoke-Checked cpack '--config' (Join-Path $BuildDir 'CPackConfig.cmake') `
        '-C' $Configuration '-G' 'ZIP' '-B' $PackageDir
    $manifest = Get-Content -LiteralPath (Join-Path $StageDir 'release-manifest.json') -Raw -Encoding utf8 | ConvertFrom-Json
    $expectedName = "GpuComputeBenchmark-$($manifest.version)-windows-x64.zip"
    $package = Get-Item -LiteralPath (Join-Path $PackageDir $expectedName) -ErrorAction SilentlyContinue
    if (-not $package) { throw "CPack completed but the expected ZIP was not found: $expectedName" }
    # files.sha256 is generated after cmake --install, so it is not part of
    # CPack's configure-time install rules. Add it to the portable ZIP under
    # CPack's single top-level directory before hashing the final asset.
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::Open(
        $package.FullName, [System.IO.Compression.ZipArchiveMode]::Update)
    try {
        $firstEntry = $archive.Entries | Where-Object { $_.FullName.Contains('/') } | Select-Object -First 1
        if (-not $firstEntry) { throw 'CPack ZIP has no top-level release directory.' }
        $topLevel = $firstEntry.FullName.Split('/')[0]
        $entryName = "$topLevel/files.sha256"
        $existing = $archive.GetEntry($entryName)
        if ($existing) { $existing.Delete() }
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $archive, (Join-Path $StageDir 'files.sha256'), $entryName,
            [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
    } finally {
        $archive.Dispose()
    }
    $hash = Get-FileHash -LiteralPath $package.FullName -Algorithm SHA256
    $hashLine = $hash.Hash.ToLowerInvariant() + '  ' + $package.Name
    Set-Content -LiteralPath ($package.FullName + '.sha256') -Value $hashLine -Encoding ascii
    Write-Host "Package: $($package.FullName)" -ForegroundColor Green
    Write-Host "SHA256:  $($hash.Hash.ToLowerInvariant())" -ForegroundColor Green
}

Write-Host "Stage:   $StageDir" -ForegroundColor Green
Write-Warning 'Core staging succeeded; review PACKAGE_LIMITATIONS.md and verifier warnings before redistribution.'
