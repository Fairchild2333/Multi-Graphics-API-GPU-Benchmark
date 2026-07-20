# Mangekyo iOS GUI (SwiftUI)

This directory contains the native iOS front-end for **Mangekyo** (Cross-API CPU & GPU Benchmark Suite), targeting **iOS 16.0+**. 
It runs the C++ GPU engine (`gpu_engine`) in-process via a lightweight Objective-C++ bridge.

## Prerequisite

1. A Mac running macOS 14+ with **Xcode 16+** installed (required for iOS 26+ Liquid Glass APIs).
2. [XcodeGen](https://github.com/yonaskolb/XcodeGen) installed on your Mac:
   ```bash
   brew install xcodegen
   ```

## How to Build

Follow these steps on your Mac:

### 1. Compile the C++ Engine for iOS

Run CMake using the iOS Toolchain:

```bash
# In the repository root:
cmake -S . -B build-ios -GXcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0

cmake --build build-ios --config Release -- -sdk iphoneos
```

This compiles `libgpu_engine.a` for physical iOS devices (ARM64). If you want to compile for the simulator, compile using `-sdk iphonesimulator`.

### 2. Generate the Xcode Project

Generate `Mangekyo.xcodeproj` using XcodeGen:

```bash
# Inside the ios-gui directory:
codegen
```

This will automatically create `Mangekyo.xcodeproj` according to `project.yml`.

### 3. Build & Run in Xcode

1. Open `Mangekyo.xcodeproj` in Xcode.
2. Select your physical iOS device or Simulator.
3. Click the **Run** button (or press `Cmd + R`).
