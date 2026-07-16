[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$StageDir,
    [switch]$SmokeTest,
    [switch]$RequirePortable,
    [switch]$RequireChecksums
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 3.0

$StageDir = [IO.Path]::GetFullPath($StageDir)
$errors = [Collections.Generic.List[string]]::new()
$warnings = [Collections.Generic.List[string]]::new()

function Add-ErrorMessage([string]$Message) { $errors.Add($Message) }
function Add-WarningMessage([string]$Message) { $warnings.Add($Message) }
function Require-File([string]$RelativePath) {
    $path = Join-Path $StageDir $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        Add-ErrorMessage "Missing required file: $RelativePath"
    }
    return $path
}
function Add-PortabilityGate([string]$Message) {
    if ($RequirePortable) { Add-ErrorMessage $Message }
    else { Add-WarningMessage $Message }
}
function Get-PeMachine([string]$Path) {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read,
                              [IO.FileShare]::ReadWrite)
    try {
        $reader = [IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) { return 0 }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { return 0 }
        return $reader.ReadUInt16()
    } finally {
        $stream.Dispose()
    }
}

function Get-PeImports([string]$Path, [switch]$DelayLoad) {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read,
                              [IO.FileShare]::ReadWrite)
    try {
        $reader = [IO.BinaryReader]::new($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) { return @() }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { return @() }
        $stream.Position = $peOffset + 6
        $sectionCount = $reader.ReadUInt16()
        $stream.Position = $peOffset + 20
        $optionalSize = $reader.ReadUInt16()
        $optionalOffset = $peOffset + 24
        $stream.Position = $optionalOffset
        $magic = $reader.ReadUInt16()
        if ($magic -eq 0x20B) {
            $dataDirectoryOffset = $optionalOffset + 112
            $stream.Position = $optionalOffset + 24
            $imageBase = $reader.ReadUInt64()
        } elseif ($magic -eq 0x10B) {
            $dataDirectoryOffset = $optionalOffset + 96
            $stream.Position = $optionalOffset + 28
            $imageBase = [uint64]$reader.ReadUInt32()
        } else { return @() }

        $directoryIndex = if ($DelayLoad) { 13 } else { 1 }
        $stream.Position = $dataDirectoryOffset + ($directoryIndex * 8)
        $directoryRva = $reader.ReadUInt32()
        $directorySize = $reader.ReadUInt32()
        if ($directoryRva -eq 0 -or $directorySize -eq 0) { return @() }

        $sections = @()
        $sectionOffset = $optionalOffset + $optionalSize
        for ($i = 0; $i -lt $sectionCount; ++$i) {
            $stream.Position = $sectionOffset + ($i * 40) + 8
            $virtualSize = $reader.ReadUInt32()
            $virtualAddress = $reader.ReadUInt32()
            $rawSize = $reader.ReadUInt32()
            $rawPointer = $reader.ReadUInt32()
            $sections += [pscustomobject]@{
                VirtualAddress = [uint64]$virtualAddress
                Span = [uint64]([Math]::Max($virtualSize, $rawSize))
                RawPointer = [uint64]$rawPointer
            }
        }
        function Convert-RvaToOffset([uint64]$Rva) {
            foreach ($section in $sections) {
                if ($Rva -ge $section.VirtualAddress -and
                    $Rva -lt ($section.VirtualAddress + $section.Span)) {
                    return [uint64]($section.RawPointer + ($Rva - $section.VirtualAddress))
                }
            }
            return [uint64]$Rva
        }
        function Read-AsciiZ([uint64]$Offset) {
            $stream.Position = [int64]$Offset
            $bytes = [Collections.Generic.List[byte]]::new()
            while ($stream.Position -lt $stream.Length) {
                $value = $reader.ReadByte()
                if ($value -eq 0) { break }
                if ($bytes.Count -ge 4096) { throw 'Unterminated PE import name.' }
                $bytes.Add($value)
            }
            return [Text.Encoding]::ASCII.GetString($bytes.ToArray())
        }

        $imports = [Collections.Generic.List[string]]::new()
        $descriptorOffset = Convert-RvaToOffset $directoryRva
        $descriptorSize = if ($DelayLoad) { 32 } else { 20 }
        $maxDescriptors = [Math]::Min(4096, [Math]::Ceiling($directorySize / $descriptorSize) + 1)
        for ($i = 0; $i -lt $maxDescriptors; ++$i) {
            $offset = $descriptorOffset + ($i * $descriptorSize)
            $stream.Position = [int64]$offset
            $first = $reader.ReadUInt32()
            if ($DelayLoad) {
                $nameValue = $reader.ReadUInt32()
                $stream.Position = [int64]$offset
                $allZero = $true
                for ($j = 0; $j -lt 8; ++$j) {
                    if ($reader.ReadUInt32() -ne 0) { $allZero = $false }
                }
                if ($allZero) { break }
                $nameRva = if (($first -band 1) -ne 0) {
                    [uint64]$nameValue
                } else {
                    [uint64]$nameValue - $imageBase
                }
            } else {
                $stream.Position = [int64]($offset + 12)
                $nameRva = [uint64]$reader.ReadUInt32()
                $stream.Position = [int64]$offset
                $allZero = $true
                for ($j = 0; $j -lt 5; ++$j) {
                    if ($reader.ReadUInt32() -ne 0) { $allZero = $false }
                }
                if ($allZero) { break }
            }
            if ($nameRva -ne 0) { $imports.Add((Read-AsciiZ (Convert-RvaToOffset $nameRva))) }
        }
        return $imports.ToArray()
    } finally {
        $stream.Dispose()
    }
}

function Test-DelayLoadedVulkan([string]$Path, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return }
    try {
        $normal = @(Get-PeImports $Path | ForEach-Object { $_.ToLowerInvariant() })
        $delayed = @(Get-PeImports $Path -DelayLoad | ForEach-Object { $_.ToLowerInvariant() })
        if ($normal -contains 'vulkan-1.dll') {
            Add-ErrorMessage "$Label hard-imports vulkan-1.dll; DirectX-only machines cannot start it."
        }
        if ($delayed -notcontains 'vulkan-1.dll') {
            Add-ErrorMessage "$Label does not contain the required vulkan-1.dll delay import."
        }
    } catch {
        Add-ErrorMessage "Unable to inspect $Label PE imports: $($_.Exception.Message)"
    }
}

if (-not (Test-Path -LiteralPath $StageDir -PathType Container)) {
    throw "Stage directory does not exist: $StageDir"
}

$manifestPath = Require-File 'release-manifest.json'
$cliPath = Require-File 'app/bin/gpu_benchmark.exe'
$checksumPath = Join-Path $StageDir 'files.sha256'
if ($RequireChecksums) { Require-File 'files.sha256' | Out-Null }
Require-File 'app/bin/glfw3.dll' | Out-Null
Require-File 'PACKAGE_LIMITATIONS.md' | Out-Null
Require-File 'README.md' | Out-Null
Require-File 'scripts/requirements.txt' | Out-Null
Require-File 'licenses/THIRD_PARTY_NOTICES.md' | Out-Null
Require-File 'licenses/GLAD-LICENSE.txt' | Out-Null
Require-File 'licenses/GLFW-LICENSE.txt' | Out-Null

$manifest = $null
if (Test-Path -LiteralPath $manifestPath) {
    try { $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding utf8 | ConvertFrom-Json }
    catch { Add-ErrorMessage "release-manifest.json is invalid JSON: $($_.Exception.Message)" }
}

$expectedMachine = 0x8664
$expectedArch = 'x64'

if ($manifest) {
    if ([int]$manifest.schemaVersion -lt 2) {
        Add-ErrorMessage "release-manifest.json schemaVersion must be at least 2."
    }
    if ($manifest.product -ne 'Mangekyo') {
        Add-ErrorMessage "Unexpected manifest product: '$($manifest.product)'"
    }
    $expectedArch = $manifest.architecture
    if ($expectedArch -notin @('x64', 'arm64', 'ARM64')) {
        Add-ErrorMessage "Unexpected manifest architecture: '$expectedArch'"
    }
    $expectedMachine = if ($expectedArch -eq 'x64') { 0x8664 } else { 0xAA64 }
    $shaderNames = [Collections.Generic.List[string]]::new()
    if ($manifest.compiledBackends.directX11 -or $manifest.compiledBackends.directX12) {
        @('compute.hlsl','nbody.hlsl','fractal.hlsl','gpu_stress.hlsl','gpu_burn.hlsl','volumetric.hlsl',
          'synthpeak_fp32.hlsl','synthpeak_fp64.hlsl','synthpeak_int32.hlsl',
          'render3d.hlsl','particle_vs.hlsl','particle_ps.hlsl') |
            ForEach-Object { $shaderNames.Add($_) }
    }
    if ($manifest.compiledBackends.openGL) {
        @('compute_gl.comp','nbody_gl.comp','particle_gl.vert','particle_gl.frag',
          'fractal_gl.vert','fractal_gl.frag','gpu_stress_gl.frag','gpu_burn_gl.frag','volumetric_gl.vert',
          'volumetric_gl.frag','synthpeak_fp32_gl.comp','synthpeak_fp64_gl.comp',
          'synthpeak_int32_gl.comp','synthpeak_fp16_gl.comp','render3d_gl.vert',
          'render3d_gl.frag') | ForEach-Object { $shaderNames.Add($_) }
    }
    if ($manifest.compiledBackends.vulkan) {
        @('particle.vert.spv','particle.frag.spv','compute.comp.spv','nbody.comp.spv',
          'fractal.vert.spv','fractal.frag.spv','gpu_stress.frag.spv','gpu_burn.frag.spv','volumetric.vert.spv',
          'volumetric.frag.spv','fluid_advect.comp.spv','fluid_divergence.comp.spv',
          'fluid_jacobi.comp.spv','fluid_subtract.comp.spv','fluid_render.vert.spv',
          'fluid_render.frag.spv',
          'mls_mpm_clear_grid.comp.spv','mls_mpm_p2g_mass_momentum.comp.spv',
          'mls_mpm_p2g_density_stress.comp.spv','mls_mpm_grid_update.comp.spv',
          'mls_mpm_g2p.comp.spv','cinematic_liquid_resolve.comp.spv',
          'cinematic_liquid_render.vert.spv','cinematic_liquid_render.frag.spv',
          'mls_mpm_clear_grid_v2.comp.spv','mls_mpm_p2g_mass_momentum_v2.comp.spv',
          'mls_mpm_p2g_density_stress_v2.comp.spv','mls_mpm_grid_update_v2.comp.spv',
          'mls_mpm_g2p_v2.comp.spv','cinematic_liquid_rigid_integrate_v2.comp.spv',
          'cinematic_liquid_resolve_v2.comp.spv',
          'cinematic_liquid_surface_clear_v2.comp.spv',
          'cinematic_liquid_surface_splat_v2.comp.spv',
          'cinematic_liquid_surface_resolve_v2.comp.spv',
          'cinematic_liquid_render_v2.vert.spv',
          'cinematic_liquid_render_v2.frag.spv',
          'cinematic_liquid_sph_external.comp.spv',
          'cinematic_liquid_sph_hash_count.comp.spv',
          'cinematic_liquid_sph_scan_block.comp.spv',
          'cinematic_liquid_sph_scan_add.comp.spv',
          'cinematic_liquid_sph_scatter.comp.spv',
          'cinematic_liquid_sph_density.comp.spv',
          'cinematic_liquid_sph_pressure.comp.spv',
          'cinematic_liquid_sph_viscosity.comp.spv',
          'cinematic_liquid_sph_integrate.comp.spv',
          'synthpeak_fp32.comp.spv','synthpeak_fp64.comp.spv',
          'synthpeak_int32.comp.spv','synthpeak_fp16.comp.spv','render3d.vert.spv',
          'render3d.frag.spv') | ForEach-Object { $shaderNames.Add($_) }
        Test-DelayLoadedVulkan $cliPath 'CLI'
        Add-PortabilityGate 'The delay-loaded Vulkan fallback still requires a clean machine with no vulkan-1.dll to confirm DirectX/WARP startup.'
    }
    foreach ($shader in $shaderNames | Select-Object -Unique) {
        Require-File (Join-Path 'app/bin' $shader) | Out-Null
        Require-File (Join-Path 'assets/shaders' $shader) | Out-Null
    }
    if ($manifest.bundled.dx12Fp16Shader) {
        Require-File 'app/bin/synthpeak_fp16.cso' | Out-Null
        Require-File 'assets/shaders/synthpeak_fp16.cso' | Out-Null
    }

    if ($manifest.bundled.guiSelfContainedPayload) {
        $guiPath = Require-File 'app/bin/gpu_bench_gui.exe'
        Require-File 'app/bin/Microsoft.WindowsAppRuntime.dll' | Out-Null
        Require-File 'app/bin/Microsoft.ui.xaml.dll' | Out-Null
        Require-File 'app/bin/resources.pri' | Out-Null
        if (Test-Path -LiteralPath $guiPath) {
            $guiMachine = Get-PeMachine $guiPath
            if ($guiMachine -ne $expectedMachine) {
                Add-ErrorMessage ("GUI is not a {0} PE image (machine=0x{1:X4})" -f $expectedArch, $guiMachine)
            }
            if ($manifest.compiledBackends.vulkan) {
                Test-DelayLoadedVulkan $guiPath 'GUI'
            }
        }
        Add-PortabilityGate 'The staged GUI still requires a clean-machine launch and CLI orchestration test.'
    } else {
        Add-PortabilityGate 'No GUI payload is bundled; this is only a CLI smoke artifact.'
    }

    if (-not $manifest.bundled.msvcRuntime) {
        Add-PortabilityGate 'CMake did not confirm bundled MSVC runtime DLLs.'
    } else {
        Require-File 'app/bin/msvcp140.dll' | Out-Null
        Require-File 'app/bin/vcruntime140.dll' | Out-Null
        Require-File 'app/bin/vcruntime140_1.dll' | Out-Null
    }
    if ($manifest.bundled.renderDocPortable) {
        Require-File 'tools/RenderDoc/renderdoccmd.exe' | Out-Null
        $qrenderdocPath = Require-File 'tools/RenderDoc/qrenderdoc.exe'
        Require-File 'tools/RenderDoc/renderdoc.dll' | Out-Null
        Require-File 'tools/RenderDoc/renderdoc.json' | Out-Null
        $renderDocLicense = Get-ChildItem -LiteralPath (Join-Path $StageDir 'tools/RenderDoc') `
            -Filter 'LICENSE*' -File -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $renderDocLicense) {
            Add-ErrorMessage 'Bundled RenderDoc payload has no LICENSE file.'
        }
        if ((Test-Path -LiteralPath $qrenderdocPath -PathType Leaf) -and
            (Get-PeMachine $qrenderdocPath) -ne 0x8664) {
            Add-ErrorMessage 'Bundled qrenderdoc.exe is not x64.'
        }
        $renderDocSource = Join-Path $StageDir 'tools/RenderDoc/BUNDLE_SOURCE.json'
        if (-not (Test-Path -LiteralPath $renderDocSource -PathType Leaf)) {
            Add-PortabilityGate 'Bundled RenderDoc has no BUNDLE_SOURCE.json archive provenance; prepare it with prepare-renderdoc-portable.ps1.'
        }
        Add-PortabilityGate 'Bundled RenderDoc lookup and process-local Vulkan layer setup exist but still need clean-machine 5-second capture validation.'
    } else {
        Add-PortabilityGate 'RenderDoc portable is not bundled; the fixed 5-second capture workflow is incomplete.'
    }
    if ($manifest.bundled.frozenReportWorker) {
        Require-File 'tools/report_worker/report_worker.exe' | Out-Null
        Add-PortabilityGate 'A frozen report worker is staged, but the current GUI still invokes external python.exe and does not consume it.'
    } else {
        Add-PortabilityGate 'No frozen report worker is bundled, and the current GUI report path requires external Python.'
    }
    if (-not $manifest.bundled.projectDistributionLicense) {
        Add-PortabilityGate 'No approved project distribution license is bundled; public redistribution is blocked.'
    }

    # Audit machine architecture of all DLL/EXE files under app/bin
    $binDir = Join-Path $StageDir 'app/bin'
    if (Test-Path -LiteralPath $binDir -PathType Container) {
        Get-ChildItem -LiteralPath $binDir -File |
            Where-Object { $_.Extension -in @('.exe', '.dll') } |
            ForEach-Object {
                $binMachine = Get-PeMachine $_.FullName
                if ($binMachine -ne 0 -and $binMachine -ne $expectedMachine) {
                    if ($expectedArch -eq 'x64' -and $_.Name.EndsWith('_ec.dll', [StringComparison]::OrdinalIgnoreCase)) {
                        return
                    }
                    if ($expectedArch -in @('arm64', 'ARM64') -and $_.Name.Equals('vcruntime140_1.dll', [StringComparison]::OrdinalIgnoreCase) -and $binMachine -eq 0x8664) {
                        return
                    }
                    Add-ErrorMessage ("$($_.Name) is not a $expectedArch PE image (machine=0x{0:X4})" -f $binMachine)
                }
            }
    }
}

if (Test-Path -LiteralPath $checksumPath -PathType Leaf) {
    $listed = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($line in Get-Content -LiteralPath $checksumPath -Encoding ascii) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        if ($line -notmatch '^([0-9a-fA-F]{64})  (.+)$') {
            Add-ErrorMessage "Malformed files.sha256 line: $line"
            continue
        }
        $expected = $Matches[1].ToLowerInvariant()
        $relative = $Matches[2].Replace('/', [IO.Path]::DirectorySeparatorChar)
        if ([IO.Path]::IsPathRooted($relative) -or $relative.Split([IO.Path]::DirectorySeparatorChar) -contains '..') {
            Add-ErrorMessage "Unsafe path in files.sha256: $relative"
            continue
        }
        if (-not $listed.Add($relative)) {
            Add-ErrorMessage "Duplicate path in files.sha256: $relative"
            continue
        }
        $path = Join-Path $StageDir $relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            Add-ErrorMessage "files.sha256 references a missing file: $relative"
            continue
        }
        $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $expected) {
            Add-ErrorMessage "SHA-256 mismatch: $relative"
        }
    }
    Get-ChildItem -LiteralPath $StageDir -Recurse -File |
        Where-Object { $_.FullName -ne $checksumPath } |
        ForEach-Object {
            $relative = $_.FullName.Substring($StageDir.TrimEnd('\').Length + 1)
            if (-not $listed.Contains($relative)) {
                Add-ErrorMessage "File is not covered by files.sha256: $relative"
            }
        }
}

$forbidden = Get-ChildItem -LiteralPath $StageDir -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object {
        $_.Extension -in @('.pdb','.lib','.exp','.ilk') -or
        ($_.Extension -eq '.pyc' -and
         -not $_.FullName.StartsWith((Join-Path $StageDir 'tools\RenderDoc\'), [StringComparison]::OrdinalIgnoreCase))
    }
foreach ($file in $forbidden) {
    Add-ErrorMessage "Developer artifact leaked into stage: $($file.FullName.Substring($StageDir.Length + 1))"
}
foreach ($userDataDir in @('results','captures','reports','logs','rdoc_captures')) {
    if (Test-Path -LiteralPath (Join-Path $StageDir $userDataDir)) {
        Add-ErrorMessage "User-data directory must not be packaged: $userDataDir"
    }
}

if ($SmokeTest -and (Test-Path -LiteralPath $cliPath)) {
    $canRun = $false
    $hostArch = $env:PROCESSOR_ARCHITECTURE
    if ($expectedArch -eq 'x64') {
        if ($hostArch -in @('AMD64', 'ARM64')) { $canRun = $true }
    } elseif ($expectedArch -in @('arm64', 'ARM64')) {
        if ($hostArch -eq 'ARM64') { $canRun = $true }
    }
    
    if ($canRun) {
        Push-Location (Split-Path -Parent $cliPath)
        try {
            $smokeOutput = & $cliPath '--help' 2>&1
            if ($LASTEXITCODE -ne 0) {
                Add-ErrorMessage "CLI --help smoke test failed with exit code ${LASTEXITCODE}: $smokeOutput"
            }
        } catch {
            Add-ErrorMessage "CLI --help smoke test could not start: $($_.Exception.Message)"
        } finally {
            Pop-Location
        }
    } else {
        Add-WarningMessage "Skipping CLI smoke test because host architecture ($hostArch) cannot run target architecture ($expectedArch)."
    }
}

foreach ($warning in $warnings) { Write-Warning $warning }
foreach ($errorMessage in $errors) { Write-Host "ERROR: $errorMessage" -ForegroundColor Red }

Write-Host "Stage verification: $($errors.Count) error(s), $($warnings.Count) warning(s)" `
    -ForegroundColor $(if ($errors.Count) { 'Red' } else { 'Green' })
if ($errors.Count) { exit 1 }
