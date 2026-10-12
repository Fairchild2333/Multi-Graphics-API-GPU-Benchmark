<#
.SYNOPSIS
    Checks the iOS and macOS code on a machine without Xcode, using Docker.

.DESCRIPTION
    Two checks, each in a throwaway container with the repository mounted
    read-only:

    1. Swift (image swift:6.0-jammy, about 4.3 GB on disk): a Swift 6
       type-check of the engine wrapper, models, charts and the CPU, GPU and
       Charts pages of both apps against stand-in modules, and tests of the
       code that has no UI. See scripts/apple-check/README.md.
    2. Engine (image python:3.12-slim plus zig from PyPI): compiles the engine
       sources that need no GPU (CPU tests, machine details, results file) to
       arm64 and x64 macOS objects.

    Neither is a build with the Apple SDK. Passing here means the code has no
    Swift language or concurrency errors in the checked files; only Xcode can
    tell whether it builds and runs.

.PARAMETER SkipEngine
    Runs the Swift check only.

.PARAMETER SkipSwift
    Runs the engine check only.

.EXAMPLE
    .\scripts\check-apple.ps1
#>
param(
    [switch]$SkipEngine,
    [switch]$SkipSwift
)

$repo = Split-Path -Parent $PSScriptRoot
$failed = $false

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Write-Host "docker was not found on PATH."
    exit 2
}

if (-not $SkipSwift) {
    Write-Host "### Swift: type-check and tests (swift:6.0-jammy)"
    docker run --rm -v "${repo}:/repo:ro" swift:6.0-jammy bash /repo/scripts/apple-check/check.sh
    if ($LASTEXITCODE -ne 0) { $failed = $true }
}

if (-not $SkipEngine) {
    Write-Host "### Engine: macOS objects of the sources that need no GPU (zig)"
    docker run --rm -v "${repo}:/repo:ro" python:3.12-slim bash /repo/scripts/apple-check/zig_check.sh
    if ($LASTEXITCODE -ne 0) { $failed = $true }
}

if ($failed) {
    Write-Host "check-apple: FAILED"
    exit 1
}
Write-Host "check-apple: passed (not a build with Xcode)"
exit 0
