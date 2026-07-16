# Mangekyo for macOS (SwiftUI + Liquid Glass)

Native macOS SwiftUI front-end for Mangekyo — the Cross-API CPU & GPU Benchmark Suite.
Uses **Liquid Glass** on macOS 26 (Tahoe) with graceful fallback to Material
on macOS 14–15.

## Architecture

Same as the Windows WinUI 3 GUI: the C++ `gpu_engine` static library runs
**in-process** on a worker thread. No subprocess or shell invocation.

```
SwiftUI App
  ↕ (ObjC++ bridge: GpuBenchBridge.mm)
libgpu_engine.a  →  Metal / Vulkan / OpenGL backends
```

## Prerequisites

1. **Xcode 26** (or later) with macOS SDK
2. **CMake** — to build the engine static library
3. **Homebrew** — for `glfw`, `vulkan-loader` (optional), etc.
4. **XcodeGen** (optional) — to regenerate the `.xcodeproj` from `project.yml`

## Build & Run

### Step 1: Build the engine library

```bash
cd /path/to/Vulkan-GPU-Compute-Microbenchmark
cmake -S . -B build
cmake --build build --config Release
ls build/libgpu_engine.a   # confirm it exists
```

### Step 2: Generate Xcode project (if needed)

```bash
# Install XcodeGen if you don't have it:
brew install xcodegen

cd macos-gui
xcodegen generate
```

### Step 3: Open and run in Xcode

```bash
open GPUBenchmark.xcodeproj
# Or build from command line:
xcodebuild -project GPUBenchmark.xcodeproj -scheme GPUBenchmark -configuration Debug build
```

### Step 4: Set working directory

On first launch, go to **Settings** → **Working Directory** and select the
repository root. This is needed so the app can find `results/`, shaders, and
chart scripts.

## Features (parity with Windows WinUI 3 GUI)

| Page | Features |
|------|----------|
| **Run** | 7 presets, GPU/API/workload/precision pickers, advanced toggles (headless/VSync/host-memory), real-time log output, score display |
| **History** | Multi-column table with sorting, GPU filter, time range filter, multi-select deletion |
| **Charts** | Generate workload comparison charts via `scripts/plot_workloads.py`, display PNGs |
| **Settings** | Theme (system/light/dark), language (Auto/English/简体中文), working directory |
| **About** | Version, description, GitHub link, system info (CPU/memory/architecture) |

## Compatibility

| macOS Version | Liquid Glass | Functional |
|---------------|:------------:|:----------:|
| macOS 26+     | ✅ Full      | ✅         |
| macOS 14–15   | ❌ Material fallback | ✅ |
| < macOS 14    | ❌           | ❌ (deployment target 14.0) |

## File Structure

```
macos-gui/
├── project.yml                    # XcodeGen project spec
├── Info.plist
├── README.md
└── GPUBenchmark/
    ├── GPUBenchmarkApp.swift      # @main entry point
    ├── ContentView.swift          # NavigationSplitView shell
    ├── Views/
    │   ├── RunView.swift
    │   ├── HistoryView.swift
    │   ├── ChartsView.swift
    │   ├── SettingsView.swift
    │   └── AboutView.swift
    ├── Components/
    │   └── GlassCard.swift        # Liquid Glass card (+ fallback)
    ├── Engine/
    │   ├── BenchEngine.swift      # @Observable state management
    │   ├── GpuBenchBridge.h       # C interface
    │   ├── GpuBenchBridge.mm      # ObjC++ bridge implementation
    │   └── GPUBenchmark-Bridging-Header.h
    └── Models/
        ├── BenchResult.swift      # Codable result model
        └── Localization.swift     # i18n (EN/ZH)
```
