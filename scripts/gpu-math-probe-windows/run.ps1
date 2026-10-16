# Builds the D3D11 maths probe with MSVC and runs it once on each hardware
# DXGI adapter (or on the ones given). The output is printed and also saved
# under out/test-results/gpu-math-probe-windows-<time>/, one file per adapter,
# with the DXBC listing of every kernel.
# Usage: powershell -ExecutionPolicy Bypass -File scripts\gpu-math-probe-windows\run.ps1 [-Adapters 0,1]
param([int[]]$Adapters)
$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
$repo = (Resolve-Path (Join-Path $here '..\..')).Path
$vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path $vcvars)) { throw "MSVC not found: $vcvars" }

$buildDir = Join-Path $env:TEMP 'gpu-math-probe-windows'
New-Item -ItemType Directory -Force $buildDir | Out-Null
$exe = Join-Path $buildDir 'gpu_math_probe.exe'
$buildCmd = Join-Path $buildDir 'build.cmd'
@"
@echo off
call "$vcvars" x64 >nul || exit /b 1
cl /nologo /std:c++17 /O2 /EHsc /W4 /utf-8 "$here\gpu_math_probe.cpp" /Fo"$buildDir\\" /Fe"$exe" /link d3d11.lib dxgi.lib d3dcompiler.lib
"@ | Set-Content -Encoding ascii $buildCmd
& cmd /c "`"$buildCmd`""
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }

$list = & $exe --list
if (-not $Adapters) {
    # One run per hardware GPU: virtual display drivers list the GPU they render
    # on again under the same name, so keep the first index of each name.
    $seen = @{}
    $Adapters = @(foreach ($row in $list) {
        $f = $row -split "`t"
        if ($f[0] -eq 'ADAPTER' -and $f[3] -eq 'software=0' -and -not $seen.ContainsKey($f[2])) {
            $seen[$f[2]] = $true
            [int]$f[1]
        }
    })
}

$outDir = Join-Path $repo ("out\test-results\gpu-math-probe-windows-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$dxbcDir = Join-Path $outDir 'dxbc'
New-Item -ItemType Directory -Force $dxbcDir | Out-Null
$os = Get-CimInstance Win32_OperatingSystem
$cpu = (Get-CimInstance Win32_Processor | Select-Object -First 1).Name.Trim()
$hostLine = "Host: $env:COMPUTERNAME / $cpu / $($os.Caption) $($os.Version)"
[System.IO.File]::WriteAllLines((Join-Path $outDir 'adapters.txt'), [string[]](@($hostLine) + $list))

$failed = 0
foreach ($index in $Adapters) {
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add($hostLine)
    Write-Host $hostLine
    $probeArgs = @('--adapter', "$index")
    if ($index -eq $Adapters[0]) { $probeArgs += @('--dump-dxbc', $dxbcDir) }
    & $exe @probeArgs | ForEach-Object { Write-Host $_; $lines.Add($_) }
    if ($LASTEXITCODE -ne 0) { $failed++; Write-Host "adapter ${index}: exit code $LASTEXITCODE" }
    $file = Join-Path $outDir "output-adapter$index.txt"
    [System.IO.File]::WriteAllLines($file, $lines)
    Write-Host "Saved: $file`n"
}
if ($failed) { exit 1 }
