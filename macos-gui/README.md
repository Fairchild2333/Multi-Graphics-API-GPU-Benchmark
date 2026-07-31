# Mangekyo for macOS

Native SwiftUI front-end for Mangekyo, backed by the same C++ benchmark engine
as the CLI. The supported product floor is **macOS 12 Monterey**. macOS 26+
uses Liquid Glass; macOS 12–15 use the Material/standard-control fallback.

## Build the CLI and app

From the repository root:

```bash
./scripts/build-macos.sh
```

The command builds a native-architecture, Metal-only Release app and verifies
the result. Its final output is:

```text
out/macos/<arm64|x86_64>/Release/GPUBenchmark.app
```

Launch it with Finder, or during development:

```bash
open out/macos/$(uname -m)/Release/GPUBenchmark.app
```

The staged app contains:

```text
GPUBenchmark.app/
└── Contents/
    ├── MacOS/GPUBenchmark
    ├── Helpers/
    │   ├── gpu_benchmark
    │   └── *.metal -> ../Resources/Shaders/*.metal
    └── Resources/
        ├── Shaders/
        └── Licenses/
```

The GUI looks for the bundled worker first. It therefore does not need a
repository checkout or a manually selected repository working directory after
staging. Scores, captures, reports, and logs use:

```text
~/Library/Application Support/GpuComputeBenchmark/
```

Set `GPU_BENCH_DATA_DIR` only when an isolated data root is useful for testing.

### Requirements

- macOS with Xcode 26 and its macOS SDK
- CMake 3.22 or newer
- Network access on the first configure, unless a GLFW 3.4 source tree is
  supplied through `GPU_BENCH_GLFW_SOURCE_DIR`

Homebrew, a Homebrew library prefix, Python, and XcodeGen are **not** runtime or
normal build requirements. CMake downloads a checksum-pinned GLFW 3.4 source
archive and builds it statically with `CMAKE_OSX_DEPLOYMENT_TARGET=12.0`. This
avoids accidentally shipping a Homebrew bottle built for a newer macOS.

Useful options:

```bash
./scripts/build-macos.sh --configuration Debug
./scripts/build-macos.sh --regenerate-project
./scripts/build-macos.sh --help
```

`--regenerate-project` is the only path that requires XcodeGen. The generated
and tracked `.xcodeproj` remains buildable without it.

### What the script verifies

The script fails instead of staging a misleading app when any of these checks
fail:

- CMake CLI and `libgpu_engine.a` were not produced;
- the pinned, static GLFW archive was not produced;
- Xcode could not compile/link the SwiftUI app;
- either Mach-O executable has a non-system/Homebrew dynamic dependency;
- either executable's minimum OS is not macOS 12.0;
- the staged code signature is invalid; or
- the bundled CLI cannot complete `--help` and Metal-device enumeration smokes
  from a temporary working directory outside the bundle/repository assets.

The default signature is ad-hoc and suitable for local testing. Set
`MACOS_SIGN_IDENTITY` to a local Developer ID identity when appropriate. The
script does **not** create a universal binary, installer, notarization ticket,
or App Store archive.

## Backend contract

| Backend | Default `.app` | macOS status |
|---|:---:|---|
| Metal | Yes | Native and the supported macOS path |
| Vulkan | No | Optional developer CLI path through a separately supplied Monterey-compatible MoltenVK loader/ICD |
| OpenGL | No | The benchmark needs OpenGL 4.3 compute/SSBO; macOS stops at 4.1 |
| DX11 / DX12 | No | Windows-only |

The default bundle is intentionally Metal-only. It does not copy
`vulkan-loader`, MoltenVK, or other Homebrew dylibs. An optional Vulkan build
must be treated as a separate developer configuration and must prove that both
the loader and ICD support the selected deployment target.

## GUI scope

The macOS app currently provides:

- GPU custom runs, full analysis, Complete Suite, and Fill Missing presets;
- duration in seconds/minutes/hours/frames/until-cancel;
- Metal-native capture requests, progress, cancellation, and structured run
  issue reporting;
- CPU quick/formal runs;
- normalized/filterable History;
- native comparison charts (Swift Charts on macOS 13+, SwiftUI bar fallback on
  Monterey), with no Python dependency;
- Settings, About, English, Simplified Chinese, and Japanese UI.

Platform-specific Windows features are not presented as macOS parity:

- DX11, DX12, WARP, and DX12 AFR/SFR are not available;
- RenderDoc is replaced by `MTLCaptureManager` and `.gputrace`;
- macOS CPU scheduling is scheduler-managed rather than strict Windows thread
  affinity.

## Workload and acceptance status

| Workload | Metal implementation | Release status |
|---|---|---|
| `stream` | Native compute + render, unified-memory metadata, offscreen formal timing | 15-second M4 Pro run + native capture passed; Monterey runtime remains open |
| `gpu_burn` | Native Plasma × Kaleidoscope path | 15-second packaged-helper acceptance run passed on M4 Pro |
| `cinematic_liquid` | MLS-MPM + Metal raymarch presentation | 6-second packaged-helper smoke passed; remains a `_metal_preview` score group and is not comparable with formal Vulkan v8 |
| `gpu_stress` | No Metal implementation | Explicitly unsupported |

Developer smoke commands for a completed bundle:

```bash
APP="out/macos/$(uname -m)/Release/GPUBenchmark.app"
HELPER="$APP/Contents/Helpers/gpu_benchmark"

"$HELPER" --backend metal --workload stream --particles 1048576 --time 15
"$HELPER" --backend metal --workload gpu_burn --time 15 --iter 16
"$HELPER" --backend metal --workload cinematic_liquid --time 6
```

Capture validation must additionally confirm that a requested capture produces
a real `.gputrace` under the captures directory. A score with
`captureUnavailable` is not capture acceptance. The app declares
`MetalCaptureEnabled=true`; its external worker also enables
`MTL_CAPTURE_ENABLED` so `MTLCaptureManager` can write a trace when the OS and
device support the requested destination.

Building on a current macOS proves SDK compatibility and the encoded deployment
target; it does not prove runtime compatibility with Monterey. A release still
needs an actual macOS 12 launch/UI/worker smoke on supported Intel or Apple
hardware.

### Verified development smoke (2026-07-31)

On an Apple M4 Pro with Xcode 26.6 / macOS SDK 26.5:

- the canonical Release script completed and produced the app path above;
- both GUI and worker encode `minos 12.0`, and `otool -L` found no non-system
  or Homebrew dependency;
- Charts is a weak system-framework load;
- ad-hoc deep signature verification passed;
- the staged `.app` opened independently of the repository working directory
  and quit cleanly;
- the bundled helper completed a 1-second, 65,536-particle Metal `stream`
  capture and wrote a real 8.4 MB `.gputrace` in an isolated data directory.
- the packaged helper also completed the canonical 15-second, 1,048,576-particle
  `stream` run with a real native capture, a 15-second `gpu_burn` acceptance
  run, and a 6-second `cinematic_liquid` preview smoke.

This is current-host build/runtime evidence, not the still-required macOS 12
acceptance matrix.

## Xcode development

`project.yml` is the source for the tracked Xcode project. It contains no
`/opt/homebrew`, `/usr/local`, or repository-specific absolute library path.
The normal workflow is to run `scripts/build-macos.sh` once, then open:

```bash
open macos-gui/GPUBenchmark.xcodeproj
```

The project defaults its native library paths to the script's CMake output for
the active architecture/configuration. Custom build directories can be passed
to the script; the script supplies their resolved values to `xcodebuild`.

The separate iPhone/iPad application now lives under `ios-gui/`; it is not a
future target inside this macOS project.
