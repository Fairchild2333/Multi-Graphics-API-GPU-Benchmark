# Mangekyo GUI (WinUI 3 / C++/WinRT)

A **native C++/WinRT** WinUI 3 control panel, structured like a standard
C++/WinRT app (`App.xaml` + `MainWindow.xaml`/`.idl`/`.h`/`.cpp`, `pch.h`,
hand-written `.vcxproj` with the XAML markup compiler wired into the MSVC build).

The GUI links the engine as a static library (`gpu_engine.lib`) for lightweight
queries and result access. GPU benchmarks themselves run as isolated
`gpu_benchmark.exe` child processes, one process per selected API/GPU pair, so a
driver or capture-tool fault cannot take down the WinUI shell or prevent the
remaining pairs from running. Non-headless renders still use the engine's own
window. The GUI exe also doubles as the CLI: launched with command-line args it
forwards to `cliMain` (see `App.xaml.cpp` `wWinMain`).

Fluent shell adapted from the sdr2hdr project: **Mica backdrop**, a **custom
title bar** (Tall caption with theme-matched min/max/close buttons), a left
**NavigationView** (Run / History / Charts / Settings / About) with page-in
animations, and **card-based** pages — i.e. a real WinUI 3 look, not a flat form.

Features: GPU enumeration (via the engine's `--list-gpus`), multi-select API
filtering grouped by reported support, workload / precision / frames /
extra-param selection, advanced switches
(headless, V-Sync, host-memory, WARP); a **History** tab (reads `LoadResults()`
from the engine in-process); a **Charts** tab (runs `plot_workloads.py` and shows
the PNGs); language (Auto / English / 中文 via `i18n.h`) and theme
(System / Light / Dark). Benchmark workers write results into the shared results
location, where History and the chart scripts read them.

## Files

| File | Purpose |
|------|---------|
| `App.xaml` / `.h` / `.cpp` | Application class + `wWinMain` (GUI or CLI forwarding) |
| `MainWindow.xaml` / `.idl` / `.h` / `.cpp` | Window runtime class + UI logic |
| `MainWindow.h` | Shim (`#include "MainWindow.xaml.h"`) for generated `module.g.cpp` |
| `XamlTypeInfo.cpp` | Stub pulling in the generated XAML type-info |
| `pch.h` / `pch.cpp` | Precompiled header (winrt includes) |
| `app.manifest`, `nuget.config` | DPI/OS manifest; NuGet source |
| `gpu_bench_gui.vcxproj` | MSBuild project (CppWinRT + WindowsAppSDK + SDK.BuildTools) |

## Build

1. Build the engine first with CMake (produces `build/Release/gpu_engine.lib`
   and `gpu_benchmark.exe` + shaders) — see the repo root README.
2. Build the GUI (requires Visual Studio / VS Build Tools with the **Windows App
   SDK / C++/WinRT** components — the bare .NET SDK lacks the XAML/MSIX tasks):

```bat
nuget restore gui\gpu_bench_gui.vcxproj -ConfigFile gui\nuget.config
"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe" ^
    gui\gpu_bench_gui.vcxproj /p:Configuration=Release /p:Platform=x64 ^
    /p:GpuBuildDir=<repo>\build /m
```

Output: `gui\x64\Release\gpu_bench_gui.exe` (self-contained Windows App Runtime;
`glfw3.dll` is copied next to it). The GUI locates the CMake-built
`gpu_benchmark.exe` and starts it with that directory as the worker's current
directory, keeping the engine's shader lookup intact.

## Notes

- The hand-written `.vcxproj` mirrors the proven sdr2hdr layout, including the
  `MarkupCompilePass1/Pass2` + `GpuBenchPrepareXaml` targets that drive the XAML
  compiler, and the `MainWindow.h` / `XamlTypeInfo.cpp` stubs the generated code
  expects.
