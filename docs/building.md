# Building Mangekyo from Source

Mangekyo's internal CMake target and command remain `gpu_benchmark`; the display
brand does not require downstream build scripts to rename the executable.

## Prerequisites

| Dependency | Install |
|---|---|
| **CMake 3.25+** | https://cmake.org/download/ or system package manager |
| **C++17 compiler** | MSVC (Visual Studio 2019+), GCC 8+, Clang, or Apple Clang |
| **GLFW** | `vcpkg install glfw3` / `brew install glfw` / `sudo apt install libglfw3-dev` |
| **Vulkan SDK** (optional) | [LunarG](https://vulkan.lunarg.com/sdk/home) or `sudo apt install libvulkan-dev` |
| **Windows SDK** (for DX) | Included with Visual Studio |
| **Xcode CLT** (for Metal) | `xcode-select --install` (macOS) |

---

## Linux

> **Tested on Ubuntu.** Fedora and Arch commands are provided for
> convenience but have not been verified by the author.

**Ubuntu / Debian** (`apt`):

```bash
sudo apt install build-essential cmake libglfw3-dev libgl-dev
sudo apt install libvulkan-dev vulkan-tools glslc   # optional, for Vulkan backend
```

**Fedora / RHEL** (`dnf`):

```bash
sudo dnf install gcc-c++ cmake glfw-devel mesa-libGL-devel
sudo dnf install vulkan-loader-devel vulkan-tools glslc   # optional, for Vulkan backend
```

**Arch / Manjaro** (`pacman`):

```bash
sudo pacman -S base-devel cmake glfw-x11 mesa
sudo pacman -S vulkan-icd-loader vulkan-tools shaderc   # optional, for Vulkan backend
```

At least one of the OpenGL (`libgl-dev` / `mesa-libGL-devel` / `mesa`) or
Vulkan development packages must be installed — otherwise no backend will be
available. DirectX and Metal backends are automatically disabled on Linux.

| Backend | Available on Linux | Driver Requirement |
|---------|-------------------|--------------------|
| Vulkan  | Yes (with `libvulkan-dev`) | Mesa or NVIDIA proprietary driver |
| OpenGL 4.3 | Yes (with `libgl-dev`) | Mesa or NVIDIA proprietary driver |
| DirectX 11/12 | No | Windows only |
| Metal | No | macOS only |

**GPU selection for OpenGL:** On Linux, the application uses the `DRI_PRIME`
environment variable to route OpenGL to the user's chosen GPU. This is set
automatically when a GPU is selected via the interactive menu or `--gpu`.
You can also set it manually:

```bash
DRI_PRIME=1 ./build/gpu_benchmark --backend opengl   # use secondary GPU
```

For NVIDIA proprietary drivers, use:

```bash
__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia ./build/gpu_benchmark --backend opengl
```

---

## Windows (x64 / ARM64)

### 1. Install Visual Studio C++ Build Tools

Install [**Visual Studio 2026**](https://visualstudio.microsoft.com/)
(Community edition is free) with the following workloads selected in the
Visual Studio Installer:

- **Desktop development with C++** — provides MSVC compiler (`cl`), Windows
  SDK, CMake, and the linker.
- **C++ CMake tools for Windows** — bundled CMake integration.

If you only need command-line builds (no IDE), install
[Build Tools for Visual Studio](https://visualstudio.microsoft.com/visual-cpp-build-tools/)
instead — select the same workloads above.

> **Note:** Visual Studio does **not** add `cmake` or `cl` to the system
> PATH by default. They are only available inside the **Developer PowerShell
> for VS** (or **Native Tools Command Prompt**). If you run `cmake` or `cl`
> in a regular PowerShell window, you will get:
>
> ```
> cmake : The term 'cmake' is not recognized as the name of a cmdlet, function, script file, or operable program. Check
> the spelling of the name, or if a path was included, verify that the path is correct and try again.
> At line:1 char:1
> + cmake --version
> + ~~~~~
>     + CategoryInfo          : ObjectNotFound: (cmake:String) [], CommandNotFoundException
>     + FullyQualifiedErrorId : CommandNotFoundException
> ```
>
> To make them available globally, add their directories to your User PATH
> (adjust the VS year/edition and MSVC version to match your installation):

```powershell
# cmake
$cmakeDir = "C:\Program Files\Microsoft Visual Studio\<year>\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"

# cl (x64 — for ARM64, replace Hostx64\x64 with Hostarm64\arm64)
$clDir = "C:\Program Files\Microsoft Visual Studio\<year>\Community\VC\Tools\MSVC\<version>\bin\Hostx64\x64"

$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
foreach ($dir in @($cmakeDir, $clDir)) {
    if ($currentPath -notlike "*$dir*") {
        $currentPath = "$currentPath;$dir"
    }
}
[Environment]::SetEnvironmentVariable("Path", $currentPath, "User")
```

To find the exact MSVC version installed on your system:

```powershell
ls "C:\Program Files\Microsoft Visual Studio\<year>\Community\VC\Tools\MSVC"
# Example output: 14.50.35717
```

Reopen your terminal, then verify:

```powershell
cmake --version   # Should be 3.25+
cl                # Should print MSVC version information
```

### 2. Install vcpkg (manifest mode)

The repository now contains `vcpkg.json` with a pinned builtin baseline and a
declared GLFW dependency. Do not manually mutate a shared classic-mode package
set for release builds. Clone and bootstrap vcpkg, then expose its root to
CMake:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
$env:VCPKG_ROOT = 'C:\vcpkg'
& "$env:VCPKG_ROOT\vcpkg.exe" version
```

The target computer never needs vcpkg; this is only a build-machine input.

### 3. Install Vulkan SDK (optional — required for Vulkan API)

Download and install the [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
for Windows. The installer will set the `VULKAN_SDK` environment variable and
add the SDK `Bin` directory (containing `glslc`, `vulkaninfo`, etc.) to PATH.

After installation, verify that your GPU supports Vulkan:

```powershell
vulkaninfo --summary
```

Expected output (example):

```
==========
VULKANINFO
==========
Vulkan Instance Version: 1.x.xxx

Devices:
========
GPU0:
    apiVersion         = 1.3.xxx
    driverVersion      = xxx.xx
    vendorID           = 0x10de
    deviceID           = 0x2684
    deviceType         = PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
    deviceName         = NVIDIA GeForce RTX 5090
    driverName         = NVIDIA
    driverInfo         = xxx.xx
```

If `vulkaninfo` reports no physical devices, your GPU driver may not support
Vulkan — the application will still work with DirectX or OpenGL backends.

> **Cross-compiling?** The Vulkan SDK installer is architecture-specific. The
> `VULKAN_SDK` environment variable must point to the SDK matching your
> **target** architecture, not your host. For example, on an ARM64 machine
> cross-compiling for x64, you need to install both the ARM64 and x64
> versions of the Vulkan SDK in separate directories, and set `VULKAN_SDK`
> accordingly before running CMake:
>
> ```powershell
> # Targeting ARM64 (native on ARM64 host)
> $env:VULKAN_SDK = "C:\VulkanSDK\1.4.x.x"
>
> # Targeting x64 (cross-compiling from ARM64 host)
> $env:VULKAN_SDK = "C:\VulkanSDK-x64\1.4.x.x"
> ```
>
> If the `VULKAN_SDK` architecture does not match the build target, the
> linker will fail with `LNK4272: library machine type conflicts with target
> machine type`.

### 4. Python 3 (optional report tooling only)

OpenGL no longer downloads or generates GLAD during configure: a reproducible
GLAD 2.0.8 OpenGL 4.3 loader is checked into `third_party/glad`. Python is only
needed on the development machine for the legacy chart/report scripts under
`scripts/`; it is not needed to compile or run the CLI/GUI.

### 5. Restore GLFW via vcpkg

No separate `vcpkg install glfw3` command is required. The CMake toolchain
restores the architecture-specific dependency from `vcpkg.json`. Select it with
`VCPKG_TARGET_TRIPLET=x64-windows` or `arm64-windows` while configuring.

The DX12 and DX11 backends only need the Windows SDK (bundled with Visual
Studio). No additional driver installation is needed — D3D12/D3D11 work
through the built-in Windows graphics stack.

---

## macOS (Apple Silicon / Intel)

```bash
brew install glfw cmake
```

The Metal backend uses the system Metal framework — no additional SDK or
driver installation is needed.

---

## Verify Environment

### Linux

```bash
cmake --version    # Should be 3.20+
g++ --version      # GCC 8+ or clang++ 7+
pkg-config --modversion glfw3   # Should print 3.x
glslc --version    # Optional — only required for the Vulkan backend
```

If `glslc` is not found and you need the Vulkan backend, install the LunarG
Vulkan SDK or `sudo apt install glslc`.

### Windows

Before building on Windows, ensure that `cmake`, `cl` (MSVC compiler), and
`glslc` (Vulkan shader compiler — optional) are available in your PATH.
If they are not found, see [Step 1](#1-install-visual-studio-c-build-tools)
for how to add them. Verify in PowerShell:

```powershell
cmake --version   # Should be 3.20+
cl                # Should print MSVC version information
glslc --version   # Optional — only required for the Vulkan backend
```

Typical default paths (Visual Studio 2026 Community on ARM64 as an example):

| Tool | Default Path |
|------|-------------|
| cmake | `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin` |
| cl | `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\<version>\bin\Hostarm64\arm64` |
| glslc | `C:\VulkanSDK\<version>\Bin` |

Add the relevant directories to **User environment variables → Path**, then
reopen your terminal for the changes to take effect.

### macOS

```bash
cmake --version   # Should be 3.20+
clang --version   # Apple Clang (comes with Xcode Command Line Tools)
```

If `cmake` is not found, install it via Homebrew: `brew install cmake`.

---

## Build Steps

### Linux

```bash
# Configure (backends auto-detected based on installed packages)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run
./build/gpu_benchmark
```

CMake will print which backends are enabled during configuration:

```
-- Vulkan backend: ENABLED
-- DX12 backend:   DISABLED (not Windows)
-- DX11 backend:   DISABLED (not Windows)
-- Metal backend:  DISABLED (not macOS)
-- OpenGL backend: ENABLED
```

### Windows

```powershell
# Configure (vcpkg toolchain, all backends auto-detected)
# By default, CMake targets the host architecture (x64 on x64 machines, ARM64 on ARM64 machines).
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

# Build
cmake --build build --config Release
```

To explicitly target a specific architecture (e.g., cross-compiling), use `-A`
and `-DVCPKG_TARGET_TRIPLET`:

```powershell
# Target x64 (needed when cross-compiling from ARM64)
$env:VULKAN_SDK = "C:\VulkanSDK-x64\1.4.x.x"   # must point to x64 SDK
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows

# Target ARM64 (needed when cross-compiling from x64)
$env:VULKAN_SDK = "C:\VulkanSDK\1.4.x.x"        # must point to ARM64 SDK
cmake -S . -B build -A ARM64 -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=arm64-windows
```

> **Cross-compiling checklist:**
>
> 1. **Vulkan SDK** — Install the SDK for **both** host and target
>    architectures. Set `$env:VULKAN_SDK` to the target architecture's SDK
>    path before running CMake. If the architectures don't match, the linker
>    will fail with `LNK4272: library machine type conflicts with target
>    machine type`.
>
> 2. **vcpkg packages** — Install GLFW (and any other vcpkg dependencies) for
>    the **target** triplet:
>    ```powershell
>    vcpkg install glfw3:x64-windows       # when targeting x64
>    vcpkg install glfw3:arm64-windows     # when targeting ARM64
>    ```
>
> 3. **Clean build directory** — When switching target architectures, always
>    delete the old `build` directory first (`rm -r build`) to avoid stale
>    CMake cache entries.

### macOS

```bash
# Configure (Metal backend auto-detected on macOS)
cmake -S . -B build

# Build
cmake --build build --config Release
```

### Backend Toggles

```bash
cmake -S . -B build -DENABLE_VULKAN=OFF -DENABLE_DX12=ON -DENABLE_DX11=ON -DENABLE_METAL=OFF ...
```

## Native CPU benchmark build and smoke

The supplementary CPU benchmark is compiled into the same `gpu_benchmark`
target; it has no additional runtime dependency, graphics window, shader or
RenderDoc requirement. On Windows the WinUI project also contains the dedicated
CPU page and starts the adjacent CLI as a no-window child process.

After a Release build, run a non-persistent smoke first:

```powershell
.\build\Release\gpu_benchmark.exe `
  --cpu-benchmark all `
  --cpu-time 0.09 `
  --cpu-warmup 0 `
  --cpu-no-save
```

Expected Windows acceptance signals are exit code 0, `CPU_META` reporting
`strict_group_affinity`, one valid `CPU_RESULT kind=core` for every available
logical processor, and a valid multi result with `pinned_threads=thread_count`.
Exit code 3 means the strict affinity/completion contract failed; such results
must not be packaged as validation data or persisted.

The formal score command is intentionally long, especially on high-thread-count
systems: 15 seconds is applied separately to every logical processor and then to
the multi-core stage, with a 0.2-second warm-up per test and three median rounds.

```powershell
.\build\Release\gpu_benchmark.exe `
  --cpu-benchmark all `
  --cpu-time 15 `
  --cpu-warmup 0.2
```

Only Windows x64 is currently validated. Linux/Android enumerate the process
allowed CPU set and require pthread affinity to read back as the requested sole
CPU; successful results use `strict_sched_affinity`, while failure is invalid and
returns 3. Native Linux/container/cpuset and Android device tests are still
required. macOS is scheduler-managed and its logical-to-physical mapping is
estimated. iOS and Web/WASM CPU modes are not release targets yet.

## Windows x64 release staging

Release staging is intentionally separate from a developer build. It installs
only runtime files into a deterministic tree, verifies the tree, creates a ZIP,
and emits a SHA-256 file. User results, captures, reports, logs, PDBs, SDKs,
vcpkg, Python, and shader compilers are not copied.

For a complete GitHub Release candidate, use the umbrella command. It rebuilds
the CMake CLI/engine and self-contained WinUI GUI, validates and bundles the
official RenderDoc portable tree, creates the ZIP and WiX MSI installer, then
writes a unified asset manifest and `SHA256SUMS.txt`:

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\build-windows-github-release.ps1 `
  -RenderDocArchive C:\release-inputs\RenderDoc_<version>_64.zip `
  -RenderDocSha256 <sha256>
```

Use `-RenderDocDownloadUrl <pinned-https-zip> -RenderDocSha256 <sha256>`
instead for an online build. A mutable unverified "latest" download is never
accepted. Add `-SignToolCommand '<signtool command containing $f>' -RequireSigned`
in signed release CI. Final assets are placed in `out/release/windows-x64`
(or `windows-arm64`). The wrapper requires a clean Git worktree; use
`-AllowDirtySource` only for a non-publishable local engineering candidate.

`-BuildGui` creates a fresh self-contained WinUI payload and then reconfigures
the install rules around it. `-GuiPayloadDir` selects an externally prepared
self-contained output when a separate CI job owns the GUI build.

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\stage-windows-release.ps1 `
  -BuildGui `
  -RenderDocDir out\dependencies\RenderDoc
```

Outputs are written below `out/`:

```text
out/stage/windows-x64/                 verified install tree
out/packages/Mangekyo-*.zip
out/packages/Mangekyo-*.zip.sha256
```

For an explicit CLI-only smoke artifact, pass `-SkipGui`. This is not the
intended final GUI product. To add prebuilt portable tools:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\stage-windows-release.ps1 `
  -RenderDocDir C:\release-inputs\RenderDoc `
  -ReportWorkerDir C:\release-inputs\report_worker
```

The RenderDoc input must be the complete portable distribution with its
license, not a copied `renderdoc.dll`. The report-worker input must contain a
frozen `report_worker.exe`; packaged `.py` files alone are not a portable
report feature.

The equivalent CMake commands inspect the raw install/CPack rules:

```powershell
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
cmake --install out/build/windows-x64-release --config Release `
  --prefix out/stage/windows-x64
cpack --preset windows-x64-zip
powershell -NoProfile -ExecutionPolicy Bypass `
  -File scripts\verify-windows-stage.ps1 `
  -StageDir out\stage\windows-x64 -SmokeTest
```

Raw CMake/CPack output does not contain the post-install `files.sha256`
inventory or GitHub `release-assets.json`; use the PowerShell staging/release
flow for anything intended for distribution.

The preset does not guess a GUI output; pass
`-DGPU_BENCH_GUI_PAYLOAD_DIR=<absolute-directory>` during configuration when
using the lower-level commands.

### Current release gates

Normal verification checks that the core staged artifact is internally
complete and prints unresolved portability warnings. `-RequirePortable` turns
those warnings into CI failures. It currently fails by design until all of the
following are true:

- delay-loaded Vulkan fallback starts successfully on a clean DirectX-only
  machine with no `vulkan-1.dll`;
- a frozen report worker is present and integrated into the GUI, and a complete
  portable RenderDoc bundle is present;
- the staged GUI and fifth-second capture flow pass a clean-machine test;
- the project MIT license is staged (`licenses/LICENSE`; dependency notices are
  also under `licenses/`);
- VC runtime redistribution and installer signing have been reviewed.

The target computer does not need Visual Studio, vcpkg, Python, a Vulkan SDK,
or a shader compiler. Windows builds delay-load and probe `vulkan-1.dll`, so its
absence is designed to disable Vulkan while leaving DirectX/WARP available;
that exact no-loader path remains a clean-machine release test.

The CPU engine adds no new redistributable, so the normal install rules include
it automatically through `gpu_benchmark.exe`. However, any ZIP/MSI built
before the CPU page/engine changes predates this feature. Rebuild the CLI and
self-contained GUI, regenerate the stage and installer, then add an installed-
location CPU smoke to release acceptance: the GUI must find
`app/bin/gpu_benchmark.exe`, complete Quick, cancel a longer run cleanly, and
append its isolated preview/formal summary under the user-data results path.

CPack defaults to `ZIP;WIX`. See `packaging/README.md` and the generated
`PACKAGE_LIMITATIONS.md` for the complete gates.
