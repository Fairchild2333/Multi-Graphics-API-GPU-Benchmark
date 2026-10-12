# Mangekyo for iOS / iPadOS

Native SwiftUI application targeting **iOS 16.0+**. Liquid Glass is enabled on
newer systems, with Material controls on older systems. GPU work runs through
an embedded Metal host; CPU work uses the same CPU-only execution and result
storage as the desktop CLI, without a Metal surface or desktop process spawning.

## Build

On a Mac with Xcode (Xcode 26+ for Liquid Glass) and CMake 3.22+:

```bash
./scripts/build-ios.sh
./scripts/build-ios.sh --sdk iphonesimulator
./scripts/build-ios.sh --sdk iphoneos
```

The tracked Xcode project builds without Homebrew or XcodeGen. After changing
`project.yml`, regenerate it explicitly with installed XcodeGen:

```bash
./scripts/build-ios.sh --regenerate-project
```

Outputs:

- Device: `build-ios/DerivedData-iphoneos/Build/Products/Release-iphoneos/Mangekyo.app`
- Simulator: `build-ios/DerivedData-iphonesimulator/Build/Products/Release-iphonesimulator/Mangekyo.app`

The script produces unsigned developer builds. Installing on an iPhone/iPad
requires selecting a development team and signing in Xcode. The simulator
engine archive contains arm64 and x86_64; the app uses the host architecture.

The Xcode target copies runtime Metal **source text** into the bundle, so an
ordinary Xcode build also has its shaders. It does not need the separately
installed Metal offline compiler. At launch the bridge refreshes sandbox
shader copies from the bundle. Results live under the app's Documents directory.

## Features

- Particle rendering and compute-only particle runs, with selectable counts.
- GPU Burn with selectable steps; the Metal interactive pool preview.
- Fixed 1280 × 720 rendering in landscape; tap the surface to stop.
- Selectable duration and V-Sync, cancellation on leaving the active scene,
  and prevention of screen sleep during a test.
- CPU mixed compute, cache/memory bandwidth, storage and page-mapping tests.
- Separate GPU/CPU logs, progress, scores and current-run charts.
- History preserving result schema, device model, memory and platform metadata.
- Filtered comparison charts by version, API, execution mode and particle size,
  plus CPU comparison charts.
- A native Metal cache/memory diagnostic with size curves and saved previews.

Apple does not offer strict thread-to-core pinning. Single-thread CPU runs are
scheduler-managed and are labelled accordingly. All mobile scores retain
`_ios_preview`; they are separate from desktop score groups. The particle-derived
Metal memory diagnostic uses `metal_memory_sweep_v1_preview_ios_preview`, which
is also separate from Vulkan `membw_v1` and is not a validated cache-capacity or
memory-plateau measurement. Its allocation ceiling is limited to the app's
available memory and at most 256 MiB.

## Native verification

Build the simulator app first, then run the hosted bridge regression tests on
an available simulator (replace the destination with one on your Mac):

```bash
xcodebuild test -project ios-gui/Mangekyo.xcodeproj -scheme Mangekyo \
  -configuration Release -destination 'platform=iOS Simulator,name=iPhone 16 Pro,OS=18.5' \
  -derivedDataPath build-ios/DerivedData-iphonesimulator \
  'LIBRARY_SEARCH_PATHS="$(PROJECT_DIR)/../build-ios/Release-iphonesimulator/Release-iphonesimulator"' \
  CODE_SIGNING_ALLOWED=NO ONLY_ACTIVE_ARCH=YES
```

The tests cover CPU routing without a Metal surface, CPU memory sweeps, malformed
options, saved metadata and compute-only GPU requests. The test bundle targets
iOS 18+ to match the current XCTest runtime; the product still targets iOS 16.

Verified on 2026-10-12 with Xcode 27: unsigned device and simulator builds, and
hosted bridge tests on the iOS 18.5 simulator. Physical iPhone/iPad performance,
iOS 16 runtime acceptance, thermal contracts and App Store distribution remain
unverified.
