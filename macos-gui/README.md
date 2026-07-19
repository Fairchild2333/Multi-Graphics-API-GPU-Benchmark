# Mangekyo for macOS (SwiftUI + Liquid Glass)

Native macOS SwiftUI front-end for Mangekyo — the Cross-API CPU & GPU Benchmark Suite.
**Minimum OS: macOS 12 Monterey.** Uses **Liquid Glass** on macOS 26 (Tahoe)
with Material fallback on 12–15. Build with a current Xcode (26 recommended);
run on Monterey+.

**iOS (future, not in this target):** floor **iOS 16**; **iOS 26/27 = Liquid
Glass**, 16–25 Material. `#available(iOS 26.0, *)` already on `GlassCard`.
Full build/surface/acceptance spec: root `HANDOFF.md` §3.0.3. No iOS target yet.

## Architecture

Aligned with the Windows WinUI 3 GUI. Prefers launching the built
`gpu_benchmark` CLI as a subprocess (Cancel = terminate). Falls back to the
in-process `gpu_engine` bridge when the binary is missing.

```
SwiftUI App
  ↕ Process (preferred) or ObjC++ bridge
gpu_benchmark / libgpu_engine.a  →  Metal / Vulkan / OpenGL backends
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
| **GPU / Run** | Custom + Full Analysis; multi-select API (supported / unsupported); Duration seconds/minutes/hours/frames/Until Cancel; particle & Burn presets; capture-at-N; legacy toggle; progress; Cancel; open results/captures |
| **CPU** | per-core / multi / all; Quick 1s & Formal 15s; warm-up; summary + log |
| **History** | Table + sort; GPU/API/workload/time filters; `workloadVersion` column; clear all; open folders |
| **Charts** | `scripts/plot_workloads.py` PNGs; open charts folder |
| **Settings** | Theme; language Auto/EN/简体中文/日本語; working directory |
| **About** | Version, description, GitHub link, system info |

## Compatibility

| macOS Version | Liquid Glass | Shell | Functional |
|---------------|:------------:|:-----:|:----------:|
| macOS 26+     | ✅ Full      | NavigationSplitView | ✅ |
| macOS 13–15   | ❌ Material  | NavigationSplitView | ✅ |
| macOS 12      | ❌ Material  | NavigationView columns | ✅ |
| < macOS 12    | —            | — | ❌ (deployment target 12.0) |

CLI/`gpu_engine` CMake deployment target is also **12.0**.

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
