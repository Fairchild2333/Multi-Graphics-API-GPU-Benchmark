# Builds mangekyo_cpumem, the stand-alone CPU cache / memory bandwidth sweep,
# for Android with the NDK. The binary runs from adb without installing the
# app, which is how a phone is tested from a PC:
#
#   adb push out\android-cpumem\arm64-v8a\mangekyo_cpumem /data/local/tmp/
#   adb shell chmod 755 /data/local/tmp/mangekyo_cpumem
#   adb shell "cd /data/local/tmp && ./mangekyo_cpumem"
#
# /data/local/tmp is on the phone's internal storage, so the storage test
# measures that storage.
#
# "./mangekyo_cpumem --compute" runs the CPU compute test instead (all cores,
# then every core, formal settings). Nothing is saved; keep the output.
#
# -Static links everything in, for a device older than the NDK supports and
# older than the app installs on. Android 4.4:
#   .\scripts\build-android-cpumem.ps1 -Abi armeabi-v7a -ApiLevel 21 -Static
param(
    [string[]]$Abi = @("arm64-v8a"),
    [string]$Ndk = $env:ANDROID_NDK_HOME,
    [int]$ApiLevel = 24,
    [switch]$Static
)
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot

if (-not $Ndk) {
    $ndkRoot = Join-Path $env:LOCALAPPDATA "Android\Sdk\ndk"
    if (Test-Path $ndkRoot) {
        $Ndk = (Get-ChildItem $ndkRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
    }
}
if (-not $Ndk -or -not (Test-Path $Ndk)) {
    throw "Android NDK not found. Install it with Android Studio or pass -Ndk <path>."
}
$bin = Join-Path $Ndk "toolchains\llvm\prebuilt\windows-x86_64\bin"
$triples = @{
    "arm64-v8a"   = "aarch64-linux-android"
    "armeabi-v7a" = "armv7a-linux-androideabi"
    "x86_64"      = "x86_64-linux-android"
    "x86"         = "i686-linux-android"
}

foreach ($a in $Abi) {
    if (-not $triples.ContainsKey($a)) { throw "Unknown ABI '$a'. Use one of: $($triples.Keys -join ', ')." }
    $compiler = Join-Path $bin "clang++.exe"
    $outDir = Join-Path $repo "out\android-cpumem\$a$(if ($Static) { '-static' })"
    New-Item -ItemType Directory -Force $outDir | Out-Null
    $out = Join-Path $outDir "mangekyo_cpumem"
    $arguments = @(
        "--target=$($triples[$a])$ApiLevel", "-std=c++17", "-O2", "-Wall", "-Wextra",
        "-static-libstdc++", "-Wl,-z,max-page-size=16384",
        "-I", (Join-Path $repo "src"),
        (Join-Path $repo "src\cpu_membw.cpp"),
        (Join-Path $repo "src\cpu_benchmark.cpp"),
        (Join-Path $repo "src\system_info.cpp"),
        (Join-Path $repo "src\cpumem_cli.cpp"),
        "-o", $out
    )
    if ($Static) { $arguments = @("-static") + $arguments }
    & $compiler @arguments
    if ($LASTEXITCODE -ne 0) { throw "Build failed for $a." }
    "{0,-12} {1} ({2:N0} bytes)" -f $a, $out, (Get-Item $out).Length

    # mangekyo_es2burn: the OpenGL ES 2.0 GPU Burn, drawn off-screen. It needs
    # the system's EGL and OpenGL ES libraries, so it is never static, and it
    # is linked without the C++ library so that it starts on Android 4.4.
    # Push it with shaders\gpu_burn_es2.vert and .frag into one directory:
    #   adb shell "cd /data/local/tmp && ./mangekyo_es2burn --steps 16"
    $burn = Join-Path $outDir "mangekyo_es2burn"
    $burnArguments = @(
        "--target=$($triples[$a])$ApiLevel", "-std=c++17", "-O2", "-Wall", "-Wextra",
        "-fno-exceptions", "-fno-rtti", "-nostdlib++", "-Wl,-z,max-page-size=16384",
        (Join-Path $repo "src\es2burn_cli.cpp"),
        "-lEGL", "-lGLESv2", "-lm",
        "-o", $burn
    )
    & $compiler @burnArguments
    if ($LASTEXITCODE -ne 0) { throw "Build of mangekyo_es2burn failed for $a." }
    "{0,-12} {1} ({2:N0} bytes)" -f $a, $burn, (Get-Item $burn).Length
}
