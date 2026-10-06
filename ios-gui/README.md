# Mangekyo iOS GUI (SwiftUI)

Native iOS front-end for **Mangekyo**, targeting **iOS 16.0+** (Liquid Glass on 26/27).
Intended path: in-process `gpu_engine` via Objective-C++ bridge + embedded Metal host
(`CAMetalLayer` → `MetalBackend::Run`).

> Status (2026-07-24, fact layers):
> - **Code written**: `ios-gui/` + `src/ios_engine_host.*` + bridge (`gpb_*`) + Metal NO_GLFW iPhone path.
> - **Not compiled here**: this Windows workstation cannot build iOS; no Xcode/`cmake` iOS artifact on this machine.
> - **Not device/simulator verified**: no smoke run logged.
> - **Score identity (engine, when built for iPhone)**: `CollectResult` appends `_ios_preview`
>   (e.g. `stream_v1_ios_preview`,
>   `gpu_burn_v3_fixed_steps_16_kaleidoscope_ios_preview`).
>   Default host duration is 3s preview — **not** desktop 15s; do not mix with desktop groups.
> - **Not done**: formal mobile duration/thermal contract, liquid via embedded host,
>   App Store packaging, Mac CI.
>
> Added 2026-10-06 (still not built with Xcode, still not run on a device):
> - **CPU page**: a fourth mode, cache and memory bandwidth (`--cpu-membw`, result
>   `cpu_membw_v1_ios_preview`), with level tables, log-log curves and storage bars; the
>   compute test shows one bar per logical processor. iOS cannot bind a thread to a core,
>   so the one-thread sweep runs on the core the system picks.
> - **GPU page**: a chart of the GPU time per frame after a run. The bridge collects what
>   the embedded run prints (`gpb_poll_output`).
> - **Charts page**: one bar chart per test from the saved results, drawn with SwiftUI
>   `Canvas` (the Swift Charts dependency is gone).
> - **Engine wrapper**: `gpb_run` no longer runs on the main actor, where it froze the UI
>   until the run ended.
> - **Checked on Linux only**: the parser and the chart geometry were compiled and run with
>   Swift 6.0.3 on real engine output. `CpuView`, `ChartsView`, `RunView` and `BenchEngine`
>   pass a Swift 6 type-check against hand-written stand-ins for SwiftUI and UIKit, which
>   is not a build with the Apple SDK. The other views and the Objective-C++ bridge were
>   not checked.

## Checking without a Mac

`scripts/check-apple.ps1` type-checks the engine wrapper, the models, the charts
and the CPU, GPU and Charts pages with Swift 6 in a Linux container, and runs
tests of the code that has no UI. It is not a build with the Apple SDK; see
`scripts/apple-check/README.md` for what it covers and what it cannot.

## Prerequisite

1. macOS 14+ with **Xcode 16+** (Xcode 26+ for Liquid Glass APIs).
2. [XcodeGen](https://github.com/yonaskolb/XcodeGen):
   ```bash
   brew install xcodegen
   ```

## How to Build

### 1. Compile `gpu_engine` for iOS

```bash
# Repository root:
cmake -S . -B build-ios -GXcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0

cmake --build build-ios --config Release --target gpu_engine -- -sdk iphoneos
```

Simulator:

```bash
cmake --build build-ios --config Release --target gpu_engine -- -sdk iphonesimulator
```

### 2. Generate the Xcode project

```bash
cd ios-gui
xcodegen
```

### 3. Run

Open `Mangekyo.xcodeproj`, pick a device/simulator, Run.

On the GPU tab: wait for the Metal preview surface, choose **stream** or **gpu_burn**,
set duration (default 3s), tap Run. Stop remains available under load.

## Architecture

| Piece | Role |
|---|---|
| `MetalBenchView` | `UIView` + `CAMetalLayer` |
| `gpb_set_metal_layer` / `gpb_start_workload` | Bridge → `ios_engine_host` |
| `MetalBackend` + `SetEmbeddedWindow` | NO_GLFW iOS path |
| `RequestStop` | Cooperative cancel from Swift / scenePhase |
| `gpb_poll_output` | Lines the embedded run printed, for the run chart |
| `Models/EngineOutput.swift` | Parses engine output into tables and chart data |
| `Models/ChartGeometry.swift` | Scales, ticks and positions of the charts |
| `Components/BenchCharts.swift`, `ResultCharts.swift` | Charts and result views (shared with macOS) |

Bundled resources: `shaders/particle.metal`, `shaders/gpu_burn.metal` (copied into Documents at launch).
Data root: app Documents via `GPU_BENCH_DATA_DIR`.
