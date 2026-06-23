# GPU Compute & Rendering Pipeline — Multi-Graphics API

A real-time particle simulation using GPU compute shaders with five
interchangeable graphics API backends: **Vulkan**, **DirectX 12**,
**DirectX 11**, **OpenGL 4.3**, and **Metal**. Each backend implements the
same particle physics (Euler integration in a compute shader) and point-cloud
rendering, with GPU timestamp profiling. Runs on **Windows**, **Linux**, and
**macOS**.

See [`docs/report.md`](docs/report.md) for the full analysis.

## Supported Graphics APIs

| Graphics API | API Level | Platforms | Notes |
|---------|-----------|-----------|-------|
| Vulkan  | 1.1+       | Windows, Linux, HarmonyOS | Requires Vulkan SDK + ICD driver |
| DirectX 12 | Feature Level 11_0+ | Windows 10+ | Tries FL 12_1→12_0→11_1→11_0; works on older GPUs too |
| DirectX 11 | Feature Level 11_0 | Windows 7+  | Simplest, broadest Windows support |
| OpenGL  | 4.3 Core  | Windows, Linux, macOS (legacy) | Cross-platform fallback; requires `GL_ARB_compute_shader` |
| Metal   | Metal 2+  | macOS (Apple/Intel) | Native Apple GPU API (Apple/AMD) — highest priority on macOS |

## Benchmark Workloads

The benchmark runs **five interchangeable workloads**, each isolating one axis of
GPU performance and reporting a deterministic, **cross-API-comparable** metric
(not just FPS). Every backend runs the *same* algorithm, so results compare
directly across Vulkan / DX12 / DX11 / OpenGL / Metal.

| Axis | `--workload` | Stresses | Metric | Knobs |
|------|--------------|----------|--------|-------|
| **Bandwidth** | `stream` (default) | Memory subsystem | GB/s | `--particles` |
| **Compute (achievable)** | `nbody` | FP32 ALU + SFU + shared memory | GFLOP/s | `--bodies` |
| **Fill / fragment** | `stress` | Rasteriser + fragment ALU + ROP | G-iter/s | `--iter` |
| **Compute (peak)** | `synthpeak` | Raw ALU throughput per precision | GFLOPS / GIOPS | `--precision`, `--iter` |
| **3D render** | `render3d` | Vertex transform + raster + fill + depth | MQuad/s | `--particles` |

```bash
./build/gpu_benchmark --benchmark --headless                          # Stream — bandwidth (GB/s)
./build/gpu_benchmark --benchmark --workload nbody --bodies 65536      # N-body — achievable GFLOP/s
./build/gpu_benchmark --benchmark --workload stress --iter 8000        # Fractal — fill rate (G-iter/s)
./build/gpu_benchmark --benchmark --workload synthpeak --precision fp32  # Peak FLOPS (fp32|fp16|fp64|int32)
./build/gpu_benchmark --benchmark --workload render3d                 # True-3D billboards (MQuad/s)
```

- `stream` is bandwidth-bound (~0.15 FLOP/byte); `nbody` is a shared-memory-tiled
  all-pairs simulation that is genuinely ALU/SFU-bound; `stress` is a fixed-iteration
  fullscreen fractal (FurMark-style sustained load); `synthpeak` is a vkpeak-style
  register-resident FMA loop measuring near-theoretical peak per data type;
  `render3d` is a real 3D pipeline — perspective + orbiting camera + depth test,
  particles drawn as instanced camera-facing billboard quads (vertex transform +
  rasterisation + fill + ROP).
- **Precision support** (`synthpeak`): FP32/FP64/INT32 work on Vulkan, DX12, DX11, OpenGL.
  **FP16** works on **Vulkan** (`VK_KHR_shader_float16_int8`), **DX12** (precompiled
  SM 6.2 DXIL via the Windows SDK DXC), **OpenGL** (`GL_NV_gpu_shader5`, NVIDIA), and
  **Metal** (`half`); it is **not possible on DX11** (Direct3D 11 caps at SM 5, no true
  FP16). **FP64 is unavailable on Metal** (Apple GPUs have no doubles). Unsupported
  combinations report a clear message instead of a misleading number.
- **DX11 + `synthpeak`**: the kernel runs, but DX11 doesn't resolve GPU timestamps in
  the required headless mode (a known driver limitation — see
  [`docs/woa-dx11-timestamp-issue.md`](docs/woa-dx11-timestamp-issue.md)), so it
  reports no score. DX11 timing works in windowed mode (used for its `nbody`/`stress`).

Each run records its axis metric (`score` + `scoreUnit`) to the results file.
Generate cross-API comparison charts with:

```bash
python scripts/plot_workloads.py        # writes docs/images/workload_*.png
```

See [`docs/benchmark-workload-suite.md`](docs/benchmark-workload-suite.md) for the
full design (algorithms, scoring formulas, scaling, risks).

## Project Structure

```text
.
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp                # Entry point — interactive menu, GPU selection, CLI
│   ├── app_base.h/cpp          # Shared base class (window, particles, timing)
│   ├── benchmark_results.h/cpp # Result persistence, comparison tables, CSV export
│   ├── gpu_common.h            # Shared types (BenchmarkConfig, BackToMenuException)
│   ├── vulkan_backend.h/cpp    # Vulkan
│   ├── dx12_backend.h/cpp      # DirectX 12
│   ├── dx11_backend.h/cpp      # DirectX 11
│   ├── opengl_backend.h/cpp    # OpenGL 4.3
│   └── metal_backend.h/mm     # Metal (Objective-C++)
├── shaders/
│   ├── compute.comp          # Vulkan GLSL compute shader
│   ├── particle.vert         # Vulkan GLSL vertex shader
│   ├── particle.frag         # Vulkan GLSL fragment shader
│   ├── compute.hlsl          # DX12/DX11 compute shader
│   ├── particle_vs.hlsl      # DX12/DX11 vertex shader
│   ├── particle_ps.hlsl      # DX12/DX11 pixel shader
│   ├── compute_gl.comp       # OpenGL 4.3 compute shader
│   ├── particle_gl.vert      # OpenGL 4.3 vertex shader
│   ├── particle_gl.frag      # OpenGL 4.3 fragment shader
│   └── particle.metal        # Metal compute + vertex + fragment
└── build/
```

## Quick Start

See [`docs/building.md`](docs/building.md) for detailed prerequisites and
platform-specific setup (Windows/Linux/macOS).

**Linux (Debian/Ubuntu):**
```bash
sudo apt install build-essential cmake libglfw3-dev libgl-dev  # + libvulkan-dev for Vulkan
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/gpu_benchmark
```

**Windows:**
```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
.\build\Release\gpu_benchmark.exe
```

**macOS:**
```bash
brew install glfw cmake
cmake -S . -B build && cmake --build build --config Release
./build/gpu_benchmark
```

Toggle individual backends with `-DENABLE_VULKAN=OFF`, `-DENABLE_DX12=ON`, etc.

## Run

```bash
./build/gpu_benchmark                          # interactive menu
./build/gpu_benchmark --backend vulkan         # force specific API
./build/gpu_benchmark --backend dx12 --gpu 1   # specific API + GPU
./build/gpu_benchmark --benchmark              # benchmark mode (2000 frames, V-Sync off)
./build/gpu_benchmark --benchmark --headless   # pure GPU compute, no rendering
./build/gpu_benchmark --help                   # all options
```

### Backend Auto-Selection

When no `--backend` is specified, the application probes in order:

- **macOS:** Metal → Vulkan → OpenGL
- **Linux:** Vulkan → OpenGL
- **Windows:** Vulkan → DX12 → DX11 → OpenGL

### Result Management

```bash
./build/gpu_benchmark --results                # list saved results
./build/gpu_benchmark --compare                # compare all results
./build/gpu_benchmark --compare <id1> <id2>    # detailed side-by-side
./build/gpu_benchmark --results-export out.csv # export to CSV
```

## GPU Profiling

All backends collect per-frame GPU timestamps:

| Backend | Mechanism |
|---------|-----------|
| Vulkan  | `vkCmdWriteTimestamp` query pool |
| DX12    | `ID3D12GraphicsCommandList::EndQuery` timestamp heap |
| DX11    | `ID3D11Query` with `D3D11_QUERY_TIMESTAMP` |
| OpenGL  | `glQueryCounter` with `GL_TIMESTAMP` |
| Metal   | `MTLCommandBuffer.GPUStartTime` / `GPUEndTime` |

Built-in [RenderDoc](https://renderdoc.org/) integration via In-Application
API (`--capture <seconds>` or **F12**). See
[`docs/renderdoc-capture-guide.md`](docs/renderdoc-capture-guide.md).

## Architecture

```
               ┌──────────┐
               │ AppBase  │  window, particles, timing
               └────┬─────┘
      ┌──────┬──┴──┬──────┬────────┐
      │      │     │      │        │
┌─────┴──┐ ┌┴───┐ ┌┴────┐ ┌┴──────┐ ┌┴─────┐
│ Vulkan │ │DX12│ │DX11 │ │OpenGL │ │Metal │
│Backend │ │Back│ │Back │ │Back.  │ │Back. │
└────────┘ └────┘ └─────┘ └───────┘ └──────┘
```

Each backend overrides:
- `InitBackend()` — create device, pipelines, buffers
- `DrawFrame(dt)` — dispatch compute, render, present
- `CleanupBackend()` — release GPU resources
- `GetBackendName()` / `GetDeviceName()` — for display

## HarmonyOS PC

A standalone HarmonyOS application is provided in the `ohos/` directory.
It uses `VK_OHOS_surface` + XComponent instead of GLFW. See
[ohos/README.md](ohos/README.md) for build and run instructions.

## Further Reading

| Document | Description |
|----------|-------------|
| [`docs/report.md`](docs/report.md) | Full cross-platform & cross-GPU performance analysis |
| [`docs/benchmark-workload-suite.md`](docs/benchmark-workload-suite.md) | Workload suite design — bandwidth / compute / fill / peak axes, scoring |
| [`docs/nbody-workload-plan.md`](docs/nbody-workload-plan.md) | N-body compute workload — algorithm, scaling, integration |
| [`docs/winui3-render3d-plan.md`](docs/winui3-render3d-plan.md) | WinUI3 integration, true-3D rendering, and RenderDoc plan |
| [`docs/building.md`](docs/building.md) | Detailed build prerequisites and platform setup |
| [`docs/roadmap.md`](docs/roadmap.md) | Completed features, in-progress work, and planned enhancements |
| [`docs/renderdoc-capture-guide.md`](docs/renderdoc-capture-guide.md) | Step-by-step RenderDoc capture instructions |
| [`docs/renderdoc-analysis.md`](docs/renderdoc-analysis.md) | RenderDoc analysis template |
| [`ohos/README.md`](ohos/README.md) | HarmonyOS build and run guide |

---

## Acknowledgements
