[CmdletBinding(DefaultParameterSetName = 'Archive')]
param(
    [Parameter(Mandatory = $true, ParameterSetName = 'Archive')]
    [string]$ArchivePath,
    [Parameter(Mandatory = $true, ParameterSetName = 'Download')]
    [ValidatePattern('^https://')]
    [string]$DownloadUrl,
    [ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$Sha256,
    [string]$OutputDir,
    [switch]$NoClean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$outRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'out'))
if (-not $OutputDir) { $OutputDir = Join-Path $outRoot 'dependencies/RenderDoc' }
$OutputDir = [IO.Path]::GetFullPath($OutputDir)

function Reset-SafeOutputDirectory([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path)
    $allowedPrefix = $outRoot.TrimEnd([IO.Path]::DirectorySeparatorChar) +
        [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($allowedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean a path outside the repository out directory: $full"
    }
    if (Test-Path -LiteralPath $full) { Remove-Item -LiteralPath $full -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $full | Out-Null
}

function Get-PeMachine([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $reader = [IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) { return 0 }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { return 0 }
        return $reader.ReadUInt16()
    } finally { $stream.Dispose() }
}

$cacheDir = Join-Path $outRoot 'downloads'
New-Item -ItemType Directory -Force -Path $cacheDir | Out-Null
if ($PSCmdlet.ParameterSetName -eq 'Download') {
    if (-not $Sha256) {
        throw 'Online dependency acquisition requires the published RenderDoc archive SHA-256.'
    }
    $uri = [Uri]$DownloadUrl
    $leaf = [IO.Path]::GetFileName($uri.AbsolutePath)
    if (-not $leaf.EndsWith('.zip', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'The RenderDoc online input must be an official portable .zip, not an installer.'
    }
    $ArchivePath = Join-Path $cacheDir $leaf
    Write-Host "Downloading pinned RenderDoc portable archive: $DownloadUrl" -ForegroundColor Cyan
    Invoke-WebRequest -UseBasicParsing -Uri $DownloadUrl -OutFile $ArchivePath
} else {
    $ArchivePath = [IO.Path]::GetFullPath($ArchivePath)
}

if (-not (Test-Path -LiteralPath $ArchivePath -PathType Leaf)) {
    throw "RenderDoc archive was not found: $ArchivePath"
}
if ([IO.Path]::GetExtension($ArchivePath) -ne '.zip') {
    throw 'RenderDoc release input must be the official portable ZIP.'
}
$archiveHash = (Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($Sha256 -and $archiveHash -ne $Sha256.ToLowerInvariant()) {
    Remove-Item -LiteralPath $ArchivePath -Force
    throw "RenderDoc SHA-256 mismatch. Expected $($Sha256.ToLowerInvariant()), got $archiveHash."
}

$extractDir = Join-Path $outRoot 'tmp/renderdoc-extract'
Reset-SafeOutputDirectory $extractDir
Expand-Archive -LiteralPath $ArchivePath -DestinationPath $extractDir -Force

$required = @('renderdoccmd.exe', 'qrenderdoc.exe', 'renderdoc.dll', 'renderdoc.json')
$roots = @(Get-ChildItem -LiteralPath $extractDir -Recurse -Filter 'renderdoccmd.exe' -File |
    ForEach-Object { $_.Directory.FullName } |
    Where-Object {
        $candidate = $_
        @($required | Where-Object {
            -not (Test-Path -LiteralPath (Join-Path $candidate $_) -PathType Leaf)
        }).Count -eq 0
    } |
    Sort-Object { $_.Length })
if (-not $roots) {
    throw 'The archive does not contain a complete RenderDoc portable x64 directory.'
}
$payloadRoot = $roots[0]
$license = Get-ChildItem -LiteralPath $payloadRoot -Filter 'LICENSE*' -File -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $license) { throw 'The RenderDoc portable payload has no LICENSE file.' }
foreach ($binary in @('renderdoccmd.exe', 'qrenderdoc.exe', 'renderdoc.dll')) {
    $machine = Get-PeMachine (Join-Path $payloadRoot $binary)
    if ($machine -ne 0x8664) {
        throw ("RenderDoc {0} is not x64 (machine=0x{1:X4})." -f $binary, $machine)
    }
}

if (-not $NoClean) { Reset-SafeOutputDirectory $OutputDir }
else { New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null }
Copy-Item -Path (Join-Path $payloadRoot '*') -Destination $OutputDir -Recurse -Force

$fileVersion = (Get-Item -LiteralPath (Join-Path $OutputDir 'qrenderdoc.exe')).VersionInfo.FileVersion
$source = [ordered]@{
    schemaVersion = 1
    component = 'RenderDoc portable x64'
    version = $fileVersion
    archiveName = [IO.Path]::GetFileName($ArchivePath)
    archiveSha256 = $archiveHash
    sourceUrl = if ($PSCmdlet.ParameterSetName -eq 'Download') { $DownloadUrl } else { $null }
}
$source | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $OutputDir 'BUNDLE_SOURCE.json') -Encoding utf8

Remove-Item -LiteralPath $extractDir -Recurse -Force
Write-Host "RenderDoc payload: $OutputDir" -ForegroundColor Green
Write-Host "Archive SHA256: $archiveHash" -ForegroundColor Green
