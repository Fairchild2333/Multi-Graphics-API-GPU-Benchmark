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

Bundled resources: `shaders/particle.metal`, `shaders/gpu_burn.metal` (copied into Documents at launch).
Data root: app Documents via `GPU_BENCH_DATA_DIR`.
