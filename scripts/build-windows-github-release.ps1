[CmdletBinding()]
param(
    [string]$BuildDir,
    [string]$StageDir,
    [string]$PackageDir,
    [string]$InstallerDir,
    [string]$ReleaseDir,
    [string]$ToolchainFile,
    [string]$GuiPayloadDir,
    [string]$MsBuildPath,
    [string]$RenderDocDir,
    [string]$RenderDocArchive,
    [string]$RenderDocDownloadUrl,
    [ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$RenderDocSha256,
    [string]$ReportWorkerDir,
    [string]$ProjectLicenseFile,
    [string]$IsccPath,
    [string]$SignToolCommand,
    [string]$Version,
    [switch]$SkipRenderDoc,
    [switch]$SkipCompilation,
    [switch]$SkipInstaller,
    [switch]$RequireSigned,
    [switch]$RequirePortable,
    [switch]$AllowDirtySource,
    [switch]$NoClean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$outRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'out'))
if (-not $BuildDir) { $BuildDir = Join-Path $outRoot 'build/windows-x64-release' }
if (-not $StageDir) { $StageDir = Join-Path $outRoot 'stage/windows-x64' }
if (-not $PackageDir) { $PackageDir = Join-Path $outRoot 'packages' }
if (-not $InstallerDir) { $InstallerDir = Join-Path $outRoot 'installer' }
if (-not $ReleaseDir) { $ReleaseDir = Join-Path $outRoot 'release/windows-x64' }
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$StageDir = [IO.Path]::GetFullPath($StageDir)
$PackageDir = [IO.Path]::GetFullPath($PackageDir)
$InstallerDir = [IO.Path]::GetFullPath($InstallerDir)
$ReleaseDir = [IO.Path]::GetFullPath($ReleaseDir)

$savedErrorPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    $gitStatus = @(& git -C $projectRoot status --porcelain=v1 --untracked-files=normal 2>$null)
    $gitStatusExit = $LASTEXITCODE
} finally { $ErrorActionPreference = $savedErrorPreference }
if ($gitStatusExit -ne 0) {
    throw 'Unable to inspect Git source state; a GitHub Release candidate requires an identifiable source revision.'
}
$sourceTreeDirty = $gitStatus.Count -gt 0
if ($sourceTreeDirty -and -not $AllowDirtySource) {
    throw 'The source tree has tracked/untracked changes. Commit the release source or use -AllowDirtySource only for an engineering candidate.'
}

function Invoke-CheckedScript([string]$Script, [string[]]$Arguments) {
    Write-Host ('> powershell -File ' + $Script + ' ' + ($Arguments -join ' ')) -ForegroundColor DarkGray
    & powershell -NoProfile -ExecutionPolicy Bypass -File $Script @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Script failed with exit code $LASTEXITCODE" }
}

function Reset-SafeReleaseDirectory([string]$Path) {
    $allowedPrefix = $outRoot.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if (-not $Path.StartsWith($allowedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean a path outside the repository out directory: $Path"
    }
    if (Test-Path -LiteralPath $Path) { Remove-Item -LiteralPath $Path -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $Path | Out-Null
}

if ($SkipRenderDoc -and ($RenderDocDir -or $RenderDocArchive -or $RenderDocDownloadUrl)) {
    throw '-SkipRenderDoc cannot be combined with a RenderDoc input.'
}
if ($RenderDocArchive -and $RenderDocDownloadUrl) {
    throw 'Choose either -RenderDocArchive (offline) or -RenderDocDownloadUrl (online), not both.'
}
if (-not $SkipRenderDoc -and -not $RenderDocDir) {
    $preparedRenderDoc = Join-Path $outRoot 'dependencies/RenderDoc'
    $prepare = Join-Path $PSScriptRoot 'prepare-renderdoc-portable.ps1'
    if ($RenderDocArchive) {
        $prepareArgs = @('-ArchivePath', $RenderDocArchive, '-OutputDir', $preparedRenderDoc)
        if ($RenderDocSha256) { $prepareArgs += @('-Sha256', $RenderDocSha256) }
        Invoke-CheckedScript $prepare $prepareArgs
        $RenderDocDir = $preparedRenderDoc
    } elseif ($RenderDocDownloadUrl) {
        if (-not $RenderDocSha256) {
            throw '-RenderDocDownloadUrl requires -RenderDocSha256; unpinned downloads are not release inputs.'
        }
        Invoke-CheckedScript $prepare @(
            '-DownloadUrl', $RenderDocDownloadUrl,
            '-Sha256', $RenderDocSha256,
            '-OutputDir', $preparedRenderDoc)
        $RenderDocDir = $preparedRenderDoc
    } else {
        throw @'
A full GitHub release requires RenderDoc. Supply one reproducible input:
  -RenderDocArchive <official RenderDoc_*_64.zip> [-RenderDocSha256 <hash>]
or:
  -RenderDocDownloadUrl <pinned HTTPS portable ZIP> -RenderDocSha256 <hash>
Use -SkipRenderDoc only for an explicitly incomplete engineering artifact.
'@
    }
}

$stageScript = Join-Path $PSScriptRoot 'stage-windows-release.ps1'
$stageArgs = @(
    '-BuildDir', $BuildDir,
    '-StageDir', $StageDir,
    '-PackageDir', $PackageDir)
if ($ToolchainFile) { $stageArgs += @('-ToolchainFile', $ToolchainFile) }
if ($MsBuildPath) { $stageArgs += @('-MsBuildPath', $MsBuildPath) }
if ($RenderDocDir) { $stageArgs += @('-RenderDocDir', $RenderDocDir) }
if ($ReportWorkerDir) { $stageArgs += @('-ReportWorkerDir', $ReportWorkerDir) }
if ($ProjectLicenseFile) { $stageArgs += @('-ProjectLicenseFile', $ProjectLicenseFile) }
if ($RequirePortable) { $stageArgs += '-RequirePortable' }
if ($NoClean) { $stageArgs += '-NoClean' }
if ($SkipCompilation) {
    # Reconfigure even when reusing compiled binaries so GUI/RenderDoc/license
    # install inputs cannot silently come from an older CMake cache.
    $stageArgs += '-SkipBuild'
    if (-not $GuiPayloadDir) { $GuiPayloadDir = Join-Path $outRoot 'gui/windows-x64-release' }
    $stageArgs += @('-GuiPayloadDir', $GuiPayloadDir)
} elseif ($GuiPayloadDir) {
    $stageArgs += @('-GuiPayloadDir', $GuiPayloadDir)
} else {
    $stageArgs += '-BuildGui'
}
Invoke-CheckedScript $stageScript $stageArgs

$manifest = Get-Content -LiteralPath (Join-Path $StageDir 'release-manifest.json') -Raw -Encoding utf8 | ConvertFrom-Json
$manifestVersion = [string]$manifest.version
if ($Version -and $Version -ne $manifestVersion) {
    throw "Requested version '$Version' does not match the CMake project/stage version '$manifestVersion'."
}
$Version = $manifestVersion

if (-not $SkipInstaller) {
    $installerScript = Join-Path $PSScriptRoot 'build-inno-installer.ps1'
    $installerArgs = @('-StageDir', $StageDir, '-OutputDir', $InstallerDir, '-Version', $Version)
    if ($IsccPath) { $installerArgs += @('-IsccPath', $IsccPath) }
    if ($SignToolCommand) { $installerArgs += @('-SignToolCommand', $SignToolCommand) }
    if ($RequireSigned) { $installerArgs += '-RequireSigned' }
    Invoke-CheckedScript $installerScript $installerArgs
}

if (-not $NoClean) { Reset-SafeReleaseDirectory $ReleaseDir }
else { New-Item -ItemType Directory -Force -Path $ReleaseDir | Out-Null }

$zipName = "Mangekyo-$Version-windows-x64.zip"
$assets = [Collections.Generic.List[IO.FileInfo]]::new()
$zip = Get-Item -LiteralPath (Join-Path $PackageDir $zipName) -ErrorAction Stop
Copy-Item -LiteralPath $zip.FullName -Destination (Join-Path $ReleaseDir $zip.Name) -Force
$assets.Add((Get-Item -LiteralPath (Join-Path $ReleaseDir $zip.Name)))
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipAudit = [System.IO.Compression.ZipFile]::OpenRead((Join-Path $ReleaseDir $zip.Name))
try {
    $fileEntries = @($zipAudit.Entries | Where-Object { -not $_.FullName.EndsWith('/') })
    if ($fileEntries.Count -eq 0) { throw 'Portable ZIP is empty.' }
    $firstParts = $fileEntries[0].FullName.Split('/')
    if ($firstParts.Count -lt 2) { throw 'Portable ZIP has no top-level release directory.' }
    $topLevel = $firstParts[0] + '/'
    $entryMap = [Collections.Generic.Dictionary[string,System.IO.Compression.ZipArchiveEntry]]::new(
        [StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in $fileEntries) {
        if (-not $entry.FullName.StartsWith($topLevel, [StringComparison]::Ordinal)) {
            throw "Portable ZIP entry is outside its top-level directory: $($entry.FullName)"
        }
        $relative = $entry.FullName.Substring($topLevel.Length)
        if ($entryMap.ContainsKey($relative)) {
            throw "Portable ZIP contains a duplicate path: $relative"
        }
        $entryMap.Add($relative, $entry)
    }

    $inventoryEntry = $null
    if (-not $entryMap.TryGetValue('files.sha256', [ref]$inventoryEntry) -or
        $inventoryEntry.Length -eq 0) {
        throw 'Portable ZIP does not contain the staged files.sha256 inventory.'
    }
    $expectedPaths = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $inventoryStream = $inventoryEntry.Open()
        try {
            $zippedInventoryDigest = ([BitConverter]::ToString(
                $sha.ComputeHash($inventoryStream))).Replace('-', '').ToLowerInvariant()
        } finally { $inventoryStream.Dispose() }
        $stageInventoryDigest = (Get-FileHash -LiteralPath (Join-Path $StageDir 'files.sha256') `
            -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($zippedInventoryDigest -ne $stageInventoryDigest) {
            throw 'Portable ZIP files.sha256 differs from the verified stage inventory.'
        }
        foreach ($line in Get-Content -LiteralPath (Join-Path $StageDir 'files.sha256') -Encoding ascii) {
            if ($line -notmatch '^([0-9a-f]{64})  (.+)$') {
                throw "Malformed staged files.sha256 line: $line"
            }
            $expectedDigest = $Matches[1]
            $relative = $Matches[2]
            $entry = $null
            if (-not $entryMap.TryGetValue($relative, [ref]$entry)) {
                throw "Portable ZIP is missing staged file: $relative"
            }
            $stream = $entry.Open()
            try {
                $actualDigest = ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '').ToLowerInvariant()
            } finally { $stream.Dispose() }
            if ($actualDigest -ne $expectedDigest) {
                throw "Portable ZIP content hash differs from stage: $relative"
            }
            $expectedPaths.Add($relative) | Out-Null
        }
    } finally { $sha.Dispose() }
    foreach ($relative in $entryMap.Keys) {
        if ($relative -ne 'files.sha256' -and -not $expectedPaths.Contains($relative)) {
            throw "Portable ZIP contains an un-inventoried file: $relative"
        }
    }
} finally { $zipAudit.Dispose() }
if (-not $SkipInstaller) {
    $setupName = "Mangekyo-$Version-windows-x64-setup.exe"
    $setup = Get-Item -LiteralPath (Join-Path $InstallerDir $setupName) -ErrorAction Stop
    Copy-Item -LiteralPath $setup.FullName -Destination (Join-Path $ReleaseDir $setup.Name) -Force
    $assets.Add((Get-Item -LiteralPath (Join-Path $ReleaseDir $setup.Name)))
}

$assetRows = @($assets | Sort-Object Name | ForEach-Object {
    $signature = if ($_.Extension -eq '.exe') { (Get-AuthenticodeSignature -LiteralPath $_.FullName).Status.ToString() } else { 'NotApplicable' }
    [ordered]@{
        name = $_.Name
        sizeBytes = $_.Length
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        authenticode = $signature
    }
})

$sourceRevision = $null
$savedErrorPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    $revisionLines = @(& git -C $projectRoot rev-parse HEAD 2>$null)
    $revisionExit = $LASTEXITCODE
} finally { $ErrorActionPreference = $savedErrorPreference }
if ($revisionExit -eq 0 -and $revisionLines.Count -gt 0) {
    $sourceRevision = $revisionLines[0].Trim()
}
$renderDocSource = $null
$renderDocSourcePath = Join-Path $StageDir 'tools/RenderDoc/BUNDLE_SOURCE.json'
if (Test-Path -LiteralPath $renderDocSourcePath -PathType Leaf) {
    $renderDocSource = Get-Content -LiteralPath $renderDocSourcePath -Raw -Encoding utf8 | ConvertFrom-Json
}
$releaseManifest = [ordered]@{
    schemaVersion = 1
    product = [string]$manifest.product
    version = $Version
    architecture = 'x64'
    sourceRevision = $sourceRevision
    sourceTreeDirty = $sourceTreeDirty
    stageManifest = 'release-manifest.json'
    stageInventory = 'files.sha256'
    components = [ordered]@{ renderDoc = $renderDocSource }
    assets = $assetRows
}
$releaseAssetsPath = Join-Path $ReleaseDir 'release-assets.json'
$releaseManifest | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath $releaseAssetsPath -Encoding utf8
$shaLines = [Collections.Generic.List[string]]::new()
$assetRows | ForEach-Object { $shaLines.Add("$($_.sha256)  $($_.name)") }
$releaseAssetsHash = (Get-FileHash -LiteralPath $releaseAssetsPath -Algorithm SHA256).Hash.ToLowerInvariant()
$shaLines.Add("$releaseAssetsHash  release-assets.json")
[IO.File]::WriteAllLines((Join-Path $ReleaseDir 'SHA256SUMS.txt'), $shaLines, [Text.Encoding]::ASCII)

Write-Host "GitHub Release assets: $ReleaseDir" -ForegroundColor Green
Get-ChildItem -LiteralPath $ReleaseDir -File | Sort-Object Name |
    ForEach-Object { Write-Host ("  {0} ({1:N1} MiB)" -f $_.Name, ($_.Length / 1MB)) }
$quotedAssets = Get-ChildItem -LiteralPath $ReleaseDir -File | Sort-Object Name |
    ForEach-Object { '"' + $_.FullName + '"' }
Write-Host ("Upload command (after review): gh release create v$Version " +
    ($quotedAssets -join ' ') + ' --verify-tag --generate-notes') -ForegroundColor Cyan
