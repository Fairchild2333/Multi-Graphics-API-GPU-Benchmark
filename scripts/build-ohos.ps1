<#
.SYNOPSIS
    Builds the HarmonyOS app (ohos/) from the command line with the tools of
    DevEco Studio.

.DESCRIPTION
    1. Compiles the GLSL shaders of the two GPU tests to SPIR-V into the app's
       rawfile resources. This comes first because the app is packaged from
       the resources that exist when the build starts.
    2. Installs the project's packages with ohpm.
    3. Builds the HAP with hvigor: the native library (the shared gpu_engine
       plus the NAPI bridge, for arm64-v8a and x86_64) and the ArkTS pages.

    The result is an unsigned HAP. Installing it on a device needs a signing
    configuration, which DevEco Studio creates after signing in with a Huawei
    account (File > Project Structure > Signing Configs).

.PARAMETER DevEcoHome
    Installation folder of DevEco Studio.

.PARAMETER BuildMode
    debug or release.

.PARAMETER Clean
    Removes the build folders first.

.EXAMPLE
    .\scripts\build-ohos.ps1
#>
param(
    [string]$DevEcoHome = "C:\Program Files\Huawei\DevEco Studio",
    [ValidateSet("debug", "release")][string]$BuildMode = "debug",
    [switch]$Clean
)

$repo = Split-Path -Parent $PSScriptRoot
$ohos = Join-Path $repo "ohos"

foreach ($needed in "sdk\default\openharmony\native", "tools\hvigor\bin\hvigorw.bat", "tools\ohpm\bin\ohpm.bat", "tools\node\node.exe") {
    if (-not (Test-Path (Join-Path $DevEcoHome $needed))) {
        Write-Host "DevEco Studio was not found at '$DevEcoHome' ($needed is missing). Pass -DevEcoHome."
        exit 2
    }
}
$env:DEVECO_SDK_HOME = Join-Path $DevEcoHome "sdk"
$env:NODE_HOME = Join-Path $DevEcoHome "tools\node"
$env:JAVA_HOME = Join-Path $DevEcoHome "jbr"
$env:PATH = "$env:NODE_HOME;$DevEcoHome\tools\ohpm\bin;$DevEcoHome\tools\hvigor\bin;$env:JAVA_HOME\bin;$env:PATH"

# ---- 1. Shaders ----
$glslc = $null
if ($env:VULKAN_SDK -and (Test-Path (Join-Path $env:VULKAN_SDK "Bin\glslc.exe"))) {
    $glslc = Join-Path $env:VULKAN_SDK "Bin\glslc.exe"
} else {
    $found = Get-Command glslc -ErrorAction SilentlyContinue
    if ($found) { $glslc = $found.Source }
}
if (-not $glslc) {
    Write-Host "glslc was not found. Install the Vulkan SDK or put glslc on PATH."
    exit 2
}
$shaderOut = Join-Path $ohos "entry\src\main\resources\rawfile\shaders"
New-Item -ItemType Directory -Force $shaderOut | Out-Null
# Particle and GPU Burn, every stage of the fluid, and the VRAM / cache bandwidth sweep.
$shaderNames = @("particle.vert", "particle.frag", "compute.comp", "fractal.vert", "gpu_burn.frag")
$shaderNames += Get-ChildItem (Join-Path $repo "shaders") -File |
    Where-Object { $_.Name -match "^(cinematic_liquid_.*\.(comp|vert|frag)|mls_mpm_.*\.comp|membw_.*\.comp)$" } |
    ForEach-Object { $_.Name }
foreach ($shader in $shaderNames) {
    & $glslc --target-env=vulkan1.0 (Join-Path $repo "shaders\$shader") -o (Join-Path $shaderOut "$shader.spv")
    if ($LASTEXITCODE -ne 0) {
        Write-Host "glslc failed for $shader"
        exit 1
    }
}
# OpenGL ES compiles its shaders on the device: the desktop OpenGL sources of
# the particle test and GPU Burn go into the package as text, and so does the
# GPU Burn of OpenGL ES 2.0.
foreach ($shader in @("compute_gl.comp", "particle_gl.vert", "particle_gl.frag", "fractal_gl.vert", "gpu_burn_gl.frag",
        "gpu_burn_es2.vert", "gpu_burn_es2.frag")) {
    Copy-Item (Join-Path $repo "shaders\$shader") (Join-Path $shaderOut $shader) -Force
}
Write-Host "Shaders compiled to $shaderOut"

# ---- 2. Packages and 3. the HAP ----
Push-Location $ohos
try {
    if ($Clean) {
        foreach ($folder in ".hvigor", "build", "entry\build", "entry\.cxx") {
            if (Test-Path $folder) { Remove-Item -Recurse -Force $folder }
        }
    }
    & ohpm.bat install --all
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ohpm install failed"
        exit 1
    }
    & hvigorw.bat assembleHap --mode module -p product=default -p buildMode=$BuildMode --no-daemon
    $buildExit = $LASTEXITCODE
} finally {
    Pop-Location
}
if ($buildExit -ne 0) {
    Write-Host "build-ohos: FAILED"
    exit 1
}
$hap = Get-ChildItem (Join-Path $ohos "entry\build\default\outputs\default") -Filter "*.hap" -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($hap) {
    Write-Host ("build-ohos: {0} ({1:N1} MB)" -f $hap.FullName, ($hap.Length / 1MB))
}
exit 0
