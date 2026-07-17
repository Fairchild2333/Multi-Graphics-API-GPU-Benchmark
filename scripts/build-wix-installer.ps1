[CmdletBinding()]
param(
    [string]$StageDir,
    [string]$BuildDir,
    [string]$OutputDir,
    [string]$Version,
    [string]$WixBinDir,
    [ValidateSet('x64', 'ARM64')]
    [string]$Arch = 'x64',
    [switch]$AllowCliOnly,
    [switch]$SkipStageVerification,
    [switch]$StaticOnly,
    [string]$SignToolCommand,
    [switch]$RequireSigned
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$archLower = $Arch.ToLowerInvariant()
$outRoot = Join-Path $projectRoot 'out'
if (-not $StageDir) { $StageDir = Join-Path $outRoot "stage/windows-$archLower" }
if (-not $BuildDir) { $BuildDir = Join-Path $outRoot "build/windows-$archLower-release" }
if (-not $OutputDir) { $OutputDir = Join-Path $outRoot 'installer' }
$StageDir = [IO.Path]::GetFullPath($StageDir)
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)

function Require-File([string]$RelativePath) {
    $path = Join-Path $StageDir $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Installer input is missing: $RelativePath"
    }
    return $path
}

function Ensure-WixOnPath {
    $wixCli = Get-Command wix.exe -ErrorAction SilentlyContinue
    $candle = Get-Command candle.exe -ErrorAction SilentlyContinue
    if ($wixCli -or $candle) { return }

    $candidates = @(
        $WixBinDir,
        (Join-Path $outRoot 'dependencies/wix314'),
        "${env:ProgramFiles(x86)}\WiX Toolset v3.14\bin",
        "${env:ProgramFiles}\WiX Toolset v3.14\bin"
    ) | Where-Object { $_ }
    foreach ($dir in $candidates) {
        $candlePath = Join-Path $dir 'candle.exe'
        $wixPath = Join-Path $dir 'wix.exe'
        if ((Test-Path -LiteralPath $candlePath) -or (Test-Path -LiteralPath $wixPath)) {
            $env:Path = $dir + ';' + $env:Path
            Write-Host "Using WiX tools from $dir" -ForegroundColor DarkGray
            return
        }
    }
    throw @'
WiX tools were not found (wix.exe or candle.exe).
Install one of:
  - `dotnet tool install --global wix --version 5.0.2`
  - WiX Toolset v3.14 binaries (candle/light)
  - pass -WixBinDir <dir>
'@
}

if (-not (Test-Path -LiteralPath $StageDir -PathType Container)) {
    throw "Stage directory does not exist: $StageDir"
}
if (-not (Test-Path -LiteralPath (Join-Path $BuildDir 'CPackConfig.cmake') -PathType Leaf)) {
    throw "CPackConfig.cmake was not found in $BuildDir. Configure with packaging enabled first."
}

$manifestPath = Require-File 'release-manifest.json'
Require-File 'app/bin/gpu_benchmark.exe' | Out-Null
Require-File 'app/bin/glfw3.dll' | Out-Null
Require-File 'app/bin/vcruntime140.dll' | Out-Null
Require-File 'PACKAGE_LIMITATIONS.md' | Out-Null
Require-File 'licenses/THIRD_PARTY_NOTICES.md' | Out-Null
Require-File 'files.sha256' | Out-Null
$projectLicense = Join-Path $StageDir 'licenses/LICENSE'
if (-not (Test-Path -LiteralPath $projectLicense -PathType Leaf)) {
    # CMake installs the configured license under licenses/; accept LICENSE.txt too.
    $alt = @(
        (Join-Path $StageDir 'licenses/LICENSE.txt'),
        (Join-Path $StageDir 'licenses/LICENSE.md')
    ) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if (-not $alt) {
        throw 'Staged project distribution license is missing under licenses/.'
    }
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding utf8 | ConvertFrom-Json
if ($manifest.product -ne 'Mangekyo') {
    throw "WiX installer requires manifest product=Mangekyo; got '$($manifest.product)'."
}
if ($manifest.architecture.ToLowerInvariant() -ne $archLower) {
    throw "WiX $Arch installer requires manifest architecture=$archLower; got '$($manifest.architecture)'."
}
if (-not [bool]$manifest.bundled.projectDistributionLicense) {
    throw 'Stage manifest reports projectDistributionLicense=false; reconfigure with the repo LICENSE.'
}
$manifestVersion = [string]$manifest.version
if ([string]::IsNullOrWhiteSpace($manifestVersion)) {
    throw 'The staged release manifest has no version.'
}
if (-not $Version) {
    $Version = $manifestVersion
} elseif ($Version -ne $manifestVersion) {
    throw "Requested installer version '$Version' does not match staged payload version '$manifestVersion'."
}
if ($Version -notmatch '^\d+\.\d+\.\d+(?:\.\d+)?$') {
    throw "Installer version '$Version' must contain three or four numeric components."
}

$guiPath = Join-Path $StageDir 'app/bin/gpu_bench_gui.exe'
if (-not $AllowCliOnly -and -not (Test-Path -LiteralPath $guiPath -PathType Leaf)) {
    throw 'GUI-first installer requires app/bin/gpu_bench_gui.exe. Use -AllowCliOnly only for engineering smoke output.'
}

if (-not $SkipStageVerification) {
    $verifyScript = Join-Path $PSScriptRoot 'verify-windows-stage.ps1'
    & powershell -NoProfile -ExecutionPolicy Bypass -File $verifyScript `
        -StageDir $StageDir -SmokeTest -RequireChecksums
    if ($LASTEXITCODE -ne 0) { throw "Stage verification failed with exit code $LASTEXITCODE" }
}

$msiName = "Mangekyo-$Version-windows-$archLower.msi"
$msiPath = Join-Path $OutputDir $msiName
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

if ($StaticOnly) {
    Write-Host "Static WiX validation passed for $StageDir" -ForegroundColor Green
    Write-Host "Would produce: $msiPath"
    return
}

Ensure-WixOnPath

if ($SignToolCommand -and $SignToolCommand -notmatch '\$f') {
    throw 'The WiX/MSI sign-tool command must contain the $f file-name placeholder.'
}

Write-Host "> cpack -G WIX ($Arch)" -ForegroundColor DarkGray
& cpack --config (Join-Path $BuildDir 'CPackConfig.cmake') -C Release -G WIX -B $OutputDir
if ($LASTEXITCODE -ne 0) { throw "cpack -G WIX failed with exit code $LASTEXITCODE" }

# CPack names the MSI from CPACK_PACKAGE_FILE_NAME; normalize to the release name.
$produced = Get-ChildItem -LiteralPath $OutputDir -Filter '*.msi' |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $produced) { throw "cpack succeeded but no MSI was found in $OutputDir" }

if ($produced.FullName -ne $msiPath) {
    if (Test-Path -LiteralPath $msiPath) { Remove-Item -LiteralPath $msiPath -Force }
    Move-Item -LiteralPath $produced.FullName -Destination $msiPath -Force
}

if ($SignToolCommand) {
    $cmd = $SignToolCommand.Replace('$f', $msiPath)
    Write-Host "> $cmd" -ForegroundColor DarkGray
    cmd /c $cmd
    if ($LASTEXITCODE -ne 0) { throw "SignTool failed with exit code $LASTEXITCODE" }
}

$signature = (Get-AuthenticodeSignature -LiteralPath $msiPath).Status.ToString()
if ($RequireSigned -and $signature -ne 'Valid') {
    throw "RequireSigned was set but MSI Authenticode status is $signature"
}

$hash = (Get-FileHash -LiteralPath $msiPath -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath ($msiPath + '.sha256') -Value "$hash  $msiName" -Encoding ascii
Write-Host "WiX MSI:  $msiPath" -ForegroundColor Green
Write-Host "SHA256:   $hash"
Write-Host "Signature: $signature"
