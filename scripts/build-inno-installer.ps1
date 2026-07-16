[CmdletBinding()]
param(
    [string]$StageDir,
    [string]$OutputDir,
    [string]$Version,
    [string]$IsccPath,
    [string]$SignToolCommand,
    [ValidateSet('x64', 'ARM64')]
    [string]$Arch = 'x64',
    [switch]$AllowCliOnly,
    [switch]$StaticOnly,
    [switch]$SkipStageVerification,
    [switch]$RequireSigned
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$archLower = $Arch.ToLowerInvariant()
if (-not $StageDir) { $StageDir = Join-Path $projectRoot "out/stage/windows-$archLower" }
if (-not $OutputDir) { $OutputDir = Join-Path $projectRoot 'out/installer' }
$StageDir = [IO.Path]::GetFullPath($StageDir)
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$issPath = Join-Path $projectRoot 'installer/GpuComputeBenchmark.iss'

function Require-File([string]$RelativePath) {
    $path = Join-Path $StageDir $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Installer input is missing: $RelativePath"
    }
    return $path
}

if (-not (Test-Path -LiteralPath $StageDir -PathType Container)) {
    throw "Stage directory does not exist: $StageDir"
}
$manifestPath = Require-File 'release-manifest.json'
Require-File 'app/bin/gpu_benchmark.exe' | Out-Null
Require-File 'app/bin/glfw3.dll' | Out-Null
Require-File 'app/bin/vcruntime140.dll' | Out-Null
Require-File 'PACKAGE_LIMITATIONS.md' | Out-Null
Require-File 'licenses/THIRD_PARTY_NOTICES.md' | Out-Null
Require-File 'files.sha256' | Out-Null

$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding utf8 | ConvertFrom-Json
if ($manifest.product -ne 'Mangekyo') {
    throw "Inno installer requires manifest product=Mangekyo; got '$($manifest.product)'."
}
if ($manifest.architecture.ToLowerInvariant() -ne $archLower) {
    throw "Inno $Arch installer requires manifest architecture=$archLower; got '$($manifest.architecture)'."
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

$renderDocDir = Join-Path $StageDir 'tools/RenderDoc'
if (Test-Path -LiteralPath $renderDocDir -PathType Container) {
    foreach ($name in @('renderdoccmd.exe', 'qrenderdoc.exe', 'renderdoc.dll', 'renderdoc.json')) {
        if (-not (Test-Path -LiteralPath (Join-Path $renderDocDir $name) -PathType Leaf)) {
            throw "Staged RenderDoc is incomplete; missing $name"
        }
    }
    $license = @('LICENSE.rtf', 'LICENSE.md', 'LICENSE.txt') |
        Where-Object { Test-Path -LiteralPath (Join-Path $renderDocDir $_) -PathType Leaf } |
        Select-Object -First 1
    if (-not $license) {
        throw 'Staged RenderDoc is incomplete; expected LICENSE.rtf, LICENSE.md, or LICENSE.txt.'
    }
}

$iss = Get-Content -LiteralPath $issPath -Raw -Encoding utf8
$requiredInstallerTokens = @(
    '{{9DBD8675-1CE2-45DF-83BB-2E62EB71796B}',
    'AppId={#MyAppId}',
    '#define MyAppName "Mangekyo"',
    'ArchitecturesAllowed={#MyAppArchAllowed}',
    'ArchitecturesInstallIn64BitMode={#MyAppArchAllowed}',
    'DefaultDirName={localappdata}\Programs\Mangekyo',
    'OutputBaseFilename=Mangekyo-{#MyAppVersion}-windows-{#MyAppArch}-setup',
    'PrivilegesRequired=lowest',
    'SetupIconFile=',
    'Uninstallable=yes',
    'SignedUninstaller=yes',
    'tools\RenderDoc',
    'qrenderdoc.exe',
    '[Icons]',
    'Name: "desktopicon"',
    'UsePreviousAppDir=yes',
    'UsePreviousGroup=no',
    'legacy %LOCALAPPDATA%\GpuComputeBenchmark'
)
foreach ($token in $requiredInstallerTokens) {
    if (-not $iss.Contains($token)) { throw "Installer invariant is missing: $token" }
}

if (-not $SkipStageVerification) {
    $verify = Join-Path $PSScriptRoot 'verify-windows-stage.ps1'
    & powershell -NoProfile -ExecutionPolicy Bypass -File $verify -StageDir $StageDir -RequireChecksums
    if ($LASTEXITCODE -ne 0) { throw "Stage verifier failed with exit code $LASTEXITCODE" }
}

Write-Host "Static installer validation passed for $StageDir" -ForegroundColor Green
if ($StaticOnly) { exit 0 }

if ($RequireSigned -and -not $SignToolCommand) {
    throw '-RequireSigned requires -SignToolCommand.'
}
if ($SignToolCommand -and -not $SignToolCommand.Contains('$f')) {
    throw 'The Inno sign-tool command must contain the $f file-name placeholder.'
}

if (-not $IsccPath) {
    $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($command) { $IsccPath = $command.Source }
}
if (-not $IsccPath) {
    foreach ($candidate in @(
        (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
        'C:\Program Files\Inno Setup 6\ISCC.exe',
        'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
    )) {
        if (Test-Path -LiteralPath $candidate) { $IsccPath = $candidate; break }
    }
}
if (-not $IsccPath -or -not (Test-Path -LiteralPath $IsccPath -PathType Leaf)) {
    throw 'ISCC.exe was not found. Install Inno Setup 6.3+ or pass -IsccPath; use -StaticOnly for validation without compilation.'
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$arguments = @(
    '/Qp',
    "/DStageDir=$StageDir",
    "/DOutputDir=$OutputDir",
    "/DMyAppVersion=$Version",
    "/DMyAppArch=$archLower"
)
if ($AllowCliOnly) { $arguments += '/DAllowCliOnly=1' }
if ($SignToolCommand) {
    $arguments += '/DEnableSigning=1'
    $arguments += "/Srelease=$SignToolCommand"
}
$arguments += $issPath
& $IsccPath @arguments
if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE" }

$setup = Get-ChildItem -LiteralPath $OutputDir -Filter "Mangekyo-$Version-windows-$archLower-setup.exe" -File |
    Select-Object -First 1
if (-not $setup) { throw "ISCC succeeded but the expected Setup executable was not found in $OutputDir" }
$signature = Get-AuthenticodeSignature -LiteralPath $setup.FullName
if ($RequireSigned -and $signature.Status -ne [Management.Automation.SignatureStatus]::Valid) {
    throw "Installer Authenticode signature is not valid: $($signature.Status)"
}
$hash = Get-FileHash -LiteralPath $setup.FullName -Algorithm SHA256
$hashLine = $hash.Hash.ToLowerInvariant() + '  ' + $setup.Name
Set-Content -LiteralPath ($setup.FullName + '.sha256') -Value $hashLine -Encoding ascii
Write-Host "Installer: $($setup.FullName)" -ForegroundColor Green
Write-Host "SHA256:   $($hash.Hash.ToLowerInvariant())" -ForegroundColor Green
Write-Host "Signature: $($signature.Status)" -ForegroundColor $(if ($signature.Status -eq 'Valid') { 'Green' } else { 'Yellow' })
