# Mangekyo — Cross-API CPU & GPU Benchmark Suite

**Mangekyo** is a cross-API benchmark suite whose primary product is GPU
testing, with a native CPU benchmark as a supplementary test. It retains the
original real-time particle simulation and adds versioned compute, graphics,
stress, cinematic-liquid, per-logical-CPU and all-core workloads. The GPU engine
has five interchangeable graphics API backends: **Vulkan**, **DirectX 12**,
**DirectX 11**, **OpenGL 4.3**, and **Metal**, with GPU timestamp profiling.
It runs on **Windows**, **Linux**, and **macOS**; individual workload and CPU
affinity support is listed below and in [`HANDOFF.md`](HANDOFF.md).

`gpu_benchmark` remains the internal executable/target and CLI command for
compatibility. The stable Windows installer AppId and existing
`GpuComputeBenchmark` user-data directory also remain unchanged, so adopting the
Mangekyo display name does not orphan installations or historical results.

See [`docs/report.md`](docs/report.md) for the full analysis.
See [`HANDOFF.md`](HANDOFF.md) first for the authoritative current status, the
two active product goals, verified blockers, and the next implementation slice.
See [`docs/TODO.md`](docs/TODO.md) and [`docs/roadmap.md`](docs/roadmap.md) for
older specialised plans and historical context.
See [`docs/macos-notes.md`](docs/macos-notes.md) for macOS platform notes and limitations.

## Supported Graphics APIs

| Graphics API | API Level | Platforms | Notes |
|---------|-----------|-----------|-------|
| Vulkan  | 1.1+       | Windows, Linux, HarmonyOS; optional MoltenVK on macOS | SDK is a build-time dependency; runtime still needs a Vulkan loader/ICD |
| DirectX 12 | Feature Level 11_0+ | Windows 10+ | Tries FL 12_1→12_0→11_1→11_0; works on older GPUs too |
| DirectX 11 | Feature Level 10_0+ | Native backend: Windows 7+; packaged WinUI app: Windows 10 1809+ | FL11 uses SM5; FL10 uses SM4 and genuine optional DirectCompute 4.x when the driver exposes it |
| OpenGL  | 4.3 Core  | Windows, Linux | Cross-platform fallback; macOS stops at OpenGL 4.1 and cannot run this compute path |
| Metal   | Metal 2+  | macOS (Apple/Intel) | Native Apple GPU API (Apple/AMD) — highest priority on macOS |

The Windows binary now probes the real DX11 feature level and DirectCompute
capability instead of assuming every DXGI adapter is FL11. This is the intended
path for DirectX 10-era cards such as the GeForce GT 120: use DX11 FL10/SM4,
not a new DX9 backend. All production SM4 shader entries compile, but the GT 120
still needs physical-card acceptance; its safe N-body profile is limited to
4,096 bodies. Vulkan is delay-loaded, so a DirectX-only machine can start even
when no `vulkan-1.dll` is installed.

## Platform Porting Priority (user-locked; Win7 GUI moved before PS3 on 2026-07-19)

Planned porting order after the current Windows x64 work settles. Ports land
as their own result groups (new `workloadVersion` whenever the timing or
capture model differs), so existing Windows score comparisons are never
affected. HarmonyOS already has an isolated Vulkan particle prototype, but it
is not the current benchmark suite. Windows ARM64 packaging has landed; the
next platform slice is macOS.

1. **Windows on ARM (ARM64)** — native CLI/WinUI plus a native bilingual
   ARM64 Setup executable landed; x64 uses the same installer implementation;
   clean-machine / signing still open.
2. **macOS** — Product floor **macOS 12 Monterey** (CLI + SwiftUI). Metal code
   for the three primary workloads has landed: Particle + GPU Burn + Cinematic
   Liquid MLS-MPM/raymarch (`…_metal_preview`) + `MTLCaptureManager` (.gputrace).
   SwiftUI is aligned with WinUI (Liquid Glass on 26, Material on 12–15). Still
   open: real-Mac compile/smoke and formal acceptance.
3. **Android** — reuses the Vulkan backend; GLFW does not support Android, so
   a NativeActivity/ANativeWindow surface layer and a new frontend are
   required. RenderDoc supports Android remote capture. Thermal throttling
   may require a separate mobile duration contract.
4. **iOS** — Not started (after Android). Floor **iOS 16**. Metal only; no
   RenderDoc; `MTLCaptureManager`; shared SwiftUI with macOS. **iOS 26 / 27
   must use Liquid Glass** (Material on 16–25). See `HANDOFF.md` §3.0.3.
5. **Debian Linux** — technically the lowest-friction port: Vulkan/OpenGL
   backends, GLFW, RenderDoc and the XDG data path already exist; mostly
   build fixes, `.deb` packaging, CI and real-machine validation.
6. **WebGPU** — per the established plan: unified capability registry first,
   then a native Dawn-pinned backend porting Stream → GPU Burn → Cinematic
   Liquid, then a `/web` browser frontend. Results use separate version ids
   (e.g. `stream_webgpu_v1`) and record the underlying implementation; no
   formal score without reliable GPU timestamps, and browser runs have no
   RenderDoc capture.
7. **HarmonyOS PC / 鸿蒙** — promote the existing standalone `ohos/` Vulkan
   particle demo into a real product port: align the workload registry, CLI/GUI,
   result contracts and platform-appropriate capture path with the main suite.
   The current prototype has none of that full-suite integration, so this item
   remains unstarted.
8. **Windows 7 dedicated GUI (before PS3)** — separate Win32/DWM frontend and
   installer sharing `gpu_engine` / CLI / result schema; Aero capability probe
   with non-Aero fallback; must not ship WinUI / Windows App SDK.
9. **PS3 (exploratory, out of the score system)** — homebrew only
   (PSL1GHT/RSXGL); the RSX GPU has no compute/SSBO/atomics/GPU timestamps
   and PSGL is OpenGL ES 1.0 + Cg, not desktop GL 4.3, so none of the main
   workloads can be ported directly. At most a fixed-function novelty demo in
   a separate repository; it must never enter the formal score contracts or
   the GUI main page.
10. **Dual-GPU collaboration (experimental feature; first target: dual FirePro
   D700 under Boot Camp Windows).** The first Plasma/GPU Burn AFR slice is now
   implemented behind `--multi-gpu afr` (or `--afr`). DX12 uses the two nodes of
   the linked adapter, with node-local queues/backbuffers/PSOs/timestamps;
   Vulkan uses a two-device group and explicit acquire/submit/present masks.
   DX11 can request only driver-managed implicit CrossFire and reports that the
   physical participation is unverified. A final AMD AGS 6.3.1 probe enumerated
   both D700s, but both explicit and driver-managed AFR device creation reported
   `crossfireAPI=0` and only one active GPU, confirming that this FireGL driver
   does not expose DX11 CrossFire to the application. The corrected July 21 D700 acceptance
   run found that AFR requires **at least four frames in flight** (two reusable
   slots per GPU) and must run without RenderDoc injection. With those conditions,
   DX12 scales from about 113 to 222–230 FPS at 16 steps and from 19 to 39 FPS at
   128 steps. Vulkan remains serial on AMD `20.45.40.15` because its graphics
   queue family exposes only one `VkQueue` (about 102 vs 104 FPS at 16 steps and
   17 vs 17 FPS at 128); DX11 remains unchanged at about 120/120 and 20/20 FPS.
   DX12 SFR is also available through `--multi-gpu sfr` / `--sfr`: node 0 shades
   the left half, node 1 shades the right half into a local target, and a Tier-1
   cross-node buffer carries that half back for one composition and one present.
   On the D700 it is workload-dependent: at 16 steps the fixed copy cost reduces
   performance from about 112 to 69 FPS, while at 128 steps SFR improves 19 to
   28 FPS (about 1.47x), still behind AFR's 39 FPS. SFR scores use a separate
   `..._sfr2` contract and automatically disable RenderDoc. AFR likewise remains
   experimental and uses `..._afr2`; only DX12 currently has measured dual-D700
   speed-ups. The Windows GUI exposes **Multi-GPU: Off / AFR / SFR** only for a
   Custom run with exactly DX12 + Plasma selected; its adjacent info icon explains
   the linked-adapter requirement and RenderDoc shutdown. The implementation uses
   standard DX12 node masks rather than AMD-specific calls, so an NVIDIA SLI pair
   could work only if its driver exposes one linked adapter with at least two nodes;
   that path is not yet validated. An explicit OpenGL SFR probe found that FireGL exposes
   `WGL_AMD_gpu_association` and two GPU IDs, but refuses to create an associated
   context for GPU 2; the prototype was therefore reverted without a fallback.
   Original Particle
   fixed-total-work splitting and the current Cinematic Liquid simulation/render
   pipeline remain future measured work. Independent benchmark scores are never
   added together. They are also never synthesized from DX12/DX11 adapter aliases:
   on the current `27.20.14540.15002` driver DXGI exposes one D700 linked-adapter
   LUID while Vulkan exposes two physical devices. The GPU probe expands the
   linked adapter's two D3D12 nodes into D700 #1/#2 rows, and an ordinary DX12
   run binds its queue, swap-chain buffers, pipelines, descriptors, timestamps,
   and resources to the selected node. DX11 remains available only on the
   logical/primary row because D3D11 has no equivalent node selector. AFR/SFR
   continue to use both DX12 nodes cooperatively. The archived `25.20.14020.10001` driver exposed
   two DXGI LUIDs, but Task Manager showed both D3D handles executing on the
   primary physical GPU. Use short bursts on the Mac Pro 2013, not a
   dual-GPU long soak.

   History has a separate **Mode** column: `Single`, `Headless`, `AFR ×2`, or
   `SFR ×2`. Historical DX11/Vulkan AFR probes are labelled unverified or
   experimental rather than presented as measured scaling. Headless and windowed
   scores are also separate comparison contracts even when their workload version
   strings are otherwise identical.

## Benchmark Workloads

The product-facing workload set is centred on Original Particle (`stream`),
Plasma/GPU Burn (`gpu_burn`) and the current Cinematic Liquid
(`cinematic_liquid`). `gpu_stress` remains an advanced GraphicsBurn component
score, and the original `stress` path is retained as `Legacy Stress v1`.
Compatibility-only selectors are not part of the current product or multi-GPU
roadmap.
See [`HANDOFF.md`](HANDOFF.md) before treating code presence as validated support.

| Axis | `--workload` | Stresses | Metric | Knobs |
|------|--------------|----------|--------|-------|
| **Bandwidth** | `stream` (default) | Particle working-set memory traffic | GB/s | `--particles` |
| **Compute (achievable)** | `nbody` | FP32 ALU + SFU + shared memory | GFLOP/s | `--bodies` |
| **Primary visual GPU burn** | `gpu_burn` | Perspective 3D cut-glass crown and layered diamond shards; fixed-step SDF/FP32/SFU/INT + overdraw | Gpix-step/s | fixed `--iter 16..2048`; software renderers cap at 64 |
| **Legacy visual GPU burn** | `gpu_burn_v1` | Preserved Plasma Bloom v1 contract and shader | Gpix-step/s | fixed `--iter 16..2048`; software renderers cap at 64 |
| **Advanced GraphicsBurn component** | `gpu_stress` | Fragment FP32/SFU/INT ALU + four-pass overdraw | Gpix-iter/s | auto-tuned, or `--iter` |
| **Legacy fill test** | `stress` | Fractal fragment ALU + fill | G-iter/s | `--iter` |
| **Compute (peak)** | `synthpeak` | Raw ALU throughput per precision | GFLOPS / GIOPS | `--precision`, `--iter` |
| **3D render** | `render3d` | Vertex transform + raster + fill + depth | MQuad/s | `--particles` |
| **Volume raymarch** | `volumetric` | Fragment ALU/SFU + register pressure | GSample/s | `--steps` |
| **3D cinematic liquid** | `cinematic_liquid` | 320,920-particle MLS-MPM + particle-splatted 3D density volume + iterative free-surface optics; optional SPH preview | MParticle-step/s | Vulkan v8 formal score still open; Metal = compute + raymarch present (`…_metal_preview`); `--liquid-solver mpm\|sph` |

```bash
./build/gpu_benchmark --benchmark --headless                          # Stream — bandwidth (GB/s)
./build/gpu_benchmark --benchmark --workload nbody --bodies 65536      # N-body — achievable GFLOP/s
./build/gpu_benchmark --time 15 --workload gpu_burn --capture 5        # Primary 15 s Mangekyo Kaleidoscope visual burn
./build/gpu_benchmark --time 15 --workload gpu_burn_v1 --capture 5     # Preserved Plasma Bloom v1 visual burn
./build/gpu_benchmark --time 15 --workload gpu_stress --capture 5      # Advanced GraphicsBurn component
./build/gpu_benchmark --benchmark --workload stress --iter 8000        # Legacy fractal fill test
./build/gpu_benchmark --benchmark --workload synthpeak --precision fp32  # Peak FLOPS (fp32|fp16|fp64|int32)
./build/gpu_benchmark --benchmark --workload render3d                 # True-3D billboards (MQuad/s)
./build/gpu_benchmark --time 15 --workload volumetric --steps 96      # Experimental raymarch
./build/gpu_benchmark --backend vulkan --time 15 --workload cinematic_liquid --capture 5
./build/gpu_benchmark --backend vulkan --time 15 --workload cinematic_liquid --liquid-solver sph
```

- `stream` is bandwidth-dominated (~0.15 FLOP/byte), so its GB/s score is mainly
  a working-set memory-throughput result rather than a complete GPU-performance
  score. `nbody` is a shared-memory-tiled all-pairs simulation that is genuinely
  ALU/SFU-bound. `gpu_burn` is a versioned, auto-tuned visual GraphicsBurn: two
  fixed-step fullscreen passes raymarch a perspective 3D Mangekyo crown built
  from truncated cut gems and layered diamond shards. Planar SDF normals,
  Fresnel reflection, chromatic refraction, absorption and camera parallax make
  the geometry visibly faceted while consuming FP32/SFU/INT work.
  `gpu_burn_v1` preserves a historical Plasma Bloom selector/result identity.
  Public `gpu_burn` runs on Vulkan, DX12, DX11, OpenGL (including DX WARP), and
  **Metal** (`gpu_burn.metal`, pending real-Mac verification). `gpu_stress`
  remains Metal-unsupported. Neither burn path is currently a hardware-error
  detector. `stress` is the unchanged legacy fractal test;
  `synthpeak` is a vkpeak-style
  register-resident FMA loop measuring near-theoretical peak per data type;
  `render3d` is a real 3D pipeline — perspective + orbiting camera + depth test,
  particles drawn as instanced camera-facing billboard quads (vertex transform +
  rasterisation + fill + ROP).
- The current Vulkan v2 MPM implementation uses a fixed 128x64x96 grid,
  320,920 particles (142x14x98 base plus a 48x37x71 dam), ten substeps,
  stiffness 45,000, viscosity 0.035 and an 8-unit/s speed cap. The current
  result contract is `cinematic_liquid_v2_physical_scene_v8`, shader version 9
  and scene version 5. V8 was required because the shared scene changed after
  v7: the pool-wall inset is now 0.45, the wall-top fraction is 0.42 and water
  extinction is `(12,3.6,2.5)`. No v8 formal score has been run. It preserves
  the user's duck-family geometry and the
  seven-body index ABI: the boat remains body 2, the sinking sphere remains
  body 3, and the three ducklings remain appended as bodies 4-6. The boat is no
  longer hard-anchored: it is a finite 34 kg rigid body with a soft mooring, so
  fluid forces and propeller reaction can drive and rock it; its exact motion
  has not yet received visual acceptance. The body-3 sealed sphere has density
  ratio 1.06, releases at 4.28 s, uses -9.81 gravity and 0.015 air damping, and
  gates material water drag by immersion fraction using displaced mass.
- The pool now has finite-height embedded walls inset by 0.45, with wall top at
  0.42 of the simulation-domain height, and a limited
  outer simulation catch band, allowing particles to cross the rim and fall to
  the ground without pretending the fluid domain is infinite. A separate thin
  foreground soft-PVC film uses IOR 1.50, Fresnel reflection, weak absorption
  and procedural wrinkles; it is a dedicated material approximation, not a
  complete multi-medium PVC ray path. The rendered pool floor, PVC bottom,
  liner, grass and physical particle floor now share ground `y=0`; ring spacing
  is derived from twice the tube radius. The surrounding scene adds an infinite
  procedural grass field and atmospheric sky/clouds without cubemap assets.
  Surface reconstruction still uses a 5x5x5 binomial kernel, but adaptively
  preserves low-support spray/drop cells. The sinking sphere's entry crown and
  whitewater are driven from the real GPU rigid-body state and local fluid
  response rather than a fragment-only fake splash; secondary spray particles
  are not implemented yet.
- The renderer retains the v6 four-interface Fresnel/Snell path, per-segment
  Beer-Lambert attenuation and opaque-scene depth sorting; current v8 uses
  `extinction=(12,3.6,2.5)`,
  linear exposure and zero density at volume boundaries. Historical v1,
  `cinematic_liquid_v2_surface_splat_optics_v4` (shader version 6),
  `cinematic_liquid_v2_duck_family_v5` (shader version 7), and
  `cinematic_liquid_v2_iterative_optics_v6` (shader version 8) results/previews
  plus physical-scene v7 results remain historical and isolated from v8.
  The optics_v4 RTX 5090 Vulkan formal 15-second run plus 5.1-second RenderDoc
  capture passed at 263.98 MParticle-step/s (result `20260715-170629-492`). The
  historical v6 has only a six-second preview: result `20260715-221447-024`, 257.01
  MParticle-step/s, image
  `rdoc_captures/cinematic-liquid-v2-5s-iterative-optics-v6-final-preview.png`.
  Historical v7 passed shader compilation, all six final SPIR-V validations,
  CLI Release and WinUI Release x64 builds. Its independently versioned SPH
  successor now runs 318,464 particles and has completed a 15-second
  RTX 5090/Vulkan visual run: the pool, water, duck family, balls, boat and grass
  remained stable, the process stopped normally, and the user accepted the
  current appearance. This closes visual iteration only; it is not a formal
  fifth-second capture/score acceptance.
  The transient console value `241.13` is neither formal nor persisted. A smoke
  window closing after a few seconds is the test script's `--time 8` lifecycle,
  not evidence of a crash. Interactive WinUI run/history acceptance,
  fixed-timestep cross-GPU trajectory and DX12/DX11/OpenGL liquid ports remain
  open. Metal has an MLS-MPM + raymarch preview host
  (`cinematic_liquid_v2_physical_scene_v8_metal_preview`); real-Mac verification
  is still open. V8 remains MLS-MPM; use `--liquid-solver sph` for the experimental SPH
  slice inspired by [jeantimex/fluid](https://github.com/jeantimex/fluid).
  Every SPH duration, including 15 seconds, is forcibly recorded as
  `cinematic_liquid_sph_slice_v1_preview`. Before it can produce a formal score,
  its simulation must be decoupled from render rate, rigid-body impulses must be
  cleared between substeps, the in-place viscosity SSBO race must be removed,
  and atomic scatter must have a deterministic cell order. Grass soak/recycle
  uses a staggered 0.50–1.80 simulation-second countdown. No upstream assets
  are vendored.
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

## Supplementary CPU Benchmark

The CPU benchmark is deliberately supplementary: it is native C++, opens no 3D
window and never invokes RenderDoc. The CLI and dedicated WinUI page provide
`per-core`, `multi` and `all`. `per-core` sequentially tests every available
**logical processor**; `multi` starts one worker for every available logical
processor; `all` runs those two stages in that order. The kernel mixes dependent
integer, branch, FP32 and FP64 work, reports `MWork/s`, and selects the median of
three rounds. Its checksum prevents dead-code elimination; it is not a known-answer
correctness test.

The formal `cpu_mixed_v1` contract is exactly 15.0 seconds of total measurement
per test, split across three rounds, plus a 0.2-second warm-up. All other timings
are labelled preview. Stored versions include affinity capability, formal/preview,
timings, round count and whether multi-core ran standalone or after the per-core
sweep, preventing those unlike conditions from sharing a comparison group.

Windows x64 is the only verified slice. On a Ryzen 7 9800X3D the Release CLI
enumerated 16 logical processors / 8 physical cores through Windows CPU Sets,
strictly pinned every per-core and multi-core worker, and completed with exit 0.
Windows falls back to processor-core relationships when CPU Sets are unavailable;
strict-affinity failure makes the result invalid, returns exit code 3 and is not
persisted. Performance/Efficiency/Middle/LPE names are explicitly **inferred
rank labels** derived from OS metadata, not authoritative microarchitecture
identification.

The scored per-core rounds now use the same seed and dependency trace on every
logical processor. The measured worker performs no stdout formatting or flushing,
and the all-core stage emits no output inside a measurement window. The WinUI
page also prevents CPU/GPU/chart jobs from overlapping, sets the complete
`15.0 + 0.2` formal preset, batches live output and rejects an incomplete or
incompatible child protocol even when the child exits with code 0. An isolated
0.1-second GUI preview completed all 16 rows, the per-core summary and the
16-thread result at 100%; it is an orchestration smoke, not a formal score.

Successful CLI/GUI runs append only the per-core average and multi-core summary
to `results.json`; detailed logical-processor rows remain in the live page and
machine-readable stdout protocol. Use:

```text
gpu_benchmark.exe --cpu-benchmark per-core|multi|all
                  [--cpu-time <seconds>] [--cpu-warmup <seconds>]
                  [--cpu-no-save]
```

Linux/Android enumerate the process's allowed CPU set and require pthread
affinity to succeed and read back as the requested single CPU; success uses the
separate `strict_sched_affinity` identity, while failure is invalid and exits 3.
Those paths still need native/container/device builds before release. macOS is
`scheduler_managed`: its logical-to-physical mapping is only an estimate and it
has no strict public per-core binding equivalent. iOS and Web/WASM CPU modes have
not been built or accepted; a browser can expose only logical concurrency, not
reliable host-core selection or P/E classification. Affinity capability is part
of the result identity, so these paths cannot silently mix with verified Windows
strict-affinity scores.

## Project Structure

```text
.
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp                # Entry point — interactive menu, GPU selection, CLI
│   ├── app_base.h/cpp          # Shared base class (window, particles, timing)
│   ├── benchmark_results.h/cpp # Result persistence, comparison tables, CSV export
│   ├── cpu_benchmark.h/cpp     # Native supplementary CPU kernel/topology runner
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
│   ├── particle.metal        # Metal stream / nbody / synth / fractal / volumetric
│   ├── gpu_burn.metal        # Metal GPU Burn (Plasma×Kaleidoscope)
│   └── cinematic_liquid_v2.metal  # Metal liquid MLS-MPM + raymarch present
├── scripts/
│   └── build-windows.ps1       # Windows default: CLI + WinUI + adjacent shaders
├── gui/x64/Release/            # Developer WinUI output (after build-windows.ps1)
└── out/build/windows-*-release/  # CMake preset CLI/engine output
```

## Quick Start

See [`docs/building.md`](docs/building.md) for detailed prerequisites and
platform-specific setup (Windows/Linux/macOS).

For a Windows GUI-first GitHub Release candidate, use
[`scripts/build-windows-github-release.ps1`](scripts/build-windows-github-release.ps1).
It rebuilds CLI/WinUI, verifies a pinned RenderDoc portable input, audits PE
imports, inventories and rechecks the ZIP, then emits the portable ZIP and a
native bilingual Setup EXE,
`SHA256SUMS.txt` and `release-assets.json` under
`out/release/windows-x64` (or `windows-arm64`). The project is MIT-licensed
(`LICENSE`); public release still needs Authenticode signing, frozen report
worker and clean-machine acceptance — see
[`packaging/PACKAGE_LIMITATIONS.md`](packaging/PACKAGE_LIMITATIONS.md).
See [`docs/windows-installer-packaging.md`](docs/windows-installer-packaging.md)
for the authoritative x64/ARM64 installer build, signing and validation commands.

**Linux (Debian/Ubuntu):**
```bash
sudo apt install build-essential cmake libglfw3-dev libgl-dev  # + libvulkan-dev for Vulkan
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/gpu_benchmark
```

**Windows (default: CLI + WinUI GUI):**
```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build-windows.ps1
.\gui\x64\Release\gpu_bench_gui.exe
# CLI worker also at:
#   out\build\windows-x64-release\Release\gpu_benchmark.exe
#   gui\x64\Release\gpu_benchmark.exe  (copied beside the GUI with shaders)
```

CLI-only: `scripts\build-windows.ps1 -SkipGui`. Or the lower-level CMake path:
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

## Run Mangekyo

### Windows GUI: separate GPU and CPU navigation

The WinUI application now separates the main **GPU** benchmark page from the
supplementary **CPU** benchmark page. Use GPU for graphics API/device/workload
runs, including the fixed 15-second + fifth-second capture flow. Use CPU for the
sequential per-logical-processor sweep, the all-core run, or both; it opens no
3D window and never invokes RenderDoc. History and Charts remain separate
navigation destinations, and CPU/GPU/chart jobs are mutually exclusive.

### GPU benchmark CLI

```bash
./build/gpu_benchmark                          # interactive menu
./build/gpu_benchmark --backend vulkan         # force specific API
./build/gpu_benchmark --backend dx12 --gpu 1   # specific API + GPU
./build/gpu_benchmark --benchmark              # benchmark mode (2000 frames, V-Sync off)
./build/gpu_benchmark --benchmark --headless   # pure GPU compute, no rendering
./build/gpu_benchmark --help                   # all options
```

### Supplementary CPU benchmark CLI

```bash
# Quick preview: per-logical-CPU sweep followed by all-core
./build/gpu_benchmark --cpu-benchmark all --cpu-time 1 --cpu-warmup 0.15

# Formal contract: 15.0 s measurement per item + 0.2 s warm-up, three rounds
./build/gpu_benchmark --cpu-benchmark all --cpu-time 15 --cpu-warmup 0.2

# Run only one stage
./build/gpu_benchmark --cpu-benchmark per-core
./build/gpu_benchmark --cpu-benchmark multi
```

`--cpu-mode` remains a compatibility alias; new scripts should prefer the
compact `--cpu-benchmark <per-core|multi|all>` form.

### GPU backend auto-selection

When no `--backend` is specified, the application probes in order:

- **macOS:** Metal → Vulkan → OpenGL
- **Linux:** Vulkan → OpenGL
- **Windows:** Vulkan → DX12 → DX11 → OpenGL

### Result management

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
