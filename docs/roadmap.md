# Roadmap

> Status note: read [`HANDOFF.md`](../HANDOFF.md) first. It is the authoritative source for current findings, the two active product goals, P0 blockers, and the next implementation slice. This roadmap retains historical context and must be updated only after the handoff.

## Completed

- [x] `cinematic_liquid_v1`: a true 3D Vulkan liquid workload separate from
      the legacy 2D dye prototype. It uses 181,216 MLS-MPM particles, a
      96x56x64 fixed-point grid, ten substeps, R32F density-volume resolve and
      160-step refractive free-surface raymarch. WinUI/results integration and
      the formal 15 s + RenderDoc-at-5 s flow passed on RTX 5090 at
      `288.74 MParticle-step/s`.
- [x] `gpu_burn_v1` original Plasma Bloom visual burn: solid no-hole crystal
      scene (not particles and not a FurMark donut), safe 16-step probe with
      per-device auto-tuning, Vulkan/DX12/DX11/OpenGL + WARP, versioned
      `Gpix-step/s`, WinUI/history/charts, and 15 s + capture-at-5 s flow.
      RTX 5090 validation reached eight consecutive 99% NVML utilisation
      samples at approximately 599–600 W.
- [x] Vulkan compute + graphics pipeline with particle simulation
- [x] DirectX 12 / DirectX 11 / Metal backend ports
- [x] GPU timestamp profiling (all backends)
- [x] Benchmark mode with standardised report output
- [x] Multi-GPU selection (`--gpu` flag)
- [x] HarmonyOS (OHOS) Vulkan port via XComponent
- [x] Benchmark result history — auto-save, list, compare, delete, CSV export
- [x] Interactive main menu with quick run / custom run / comparison / delete
- [x] OpenGL 4.3 compute shader backend with GLAD2 loader
- [x] `VK_EXT_debug_utils` integration — debug labels and object names for RenderDoc
- [x] RenderDoc In-Application API — auto-detect, F12 capture, `--capture <seconds>` CLI
- [x] Python benchmark tooling — chart generation, batch runner, markdown/HTML report export
- [x] Headless compute mode (`--headless`) — pure GPU compute without window/swapchain/rendering
- [x] Configurable frames-in-flight (`--flights N`) — test swapchain depth impact
- [x] Configurable particle count — preset sizes (65K–16M) or custom values
- [x] RX 9070 XT (RDNA 4) benchmarks — cross-API, particle scaling, headless analysis
- [x] Multi-workload suite — five cross-API axes selectable via `--workload`:
      `stream` (bandwidth, GB/s), `nbody` (achievable compute, GFLOP/s),
      `stress` (fractal fill, G-iter/s), `synthpeak` (peak FLOPS by precision
      fp32/fp16/fp64/int32), `render3d` (true-3D instanced billboards with
      depth, MQuad/s). Per-axis `score`/`scoreUnit` persisted to results;
      cross-API charts via `scripts/plot_workloads.py`. See
      [`benchmark-workload-suite.md`](benchmark-workload-suite.md).
      (Vulkan/DX12/DX11/OpenGL implemented & validated on RTX 5090; Metal
      written, untested — no macOS. FP16 synthpeak works on Vulkan,
      DX12 (precompiled SM 6.2 DXIL via SDK DXC), OpenGL (GL_NV_gpu_shader5,
      NVIDIA) and Metal; impossible on DX11 (SM 5 cap). FP64 unavailable on Metal.)

### Benchmark Result History

Results are automatically saved to `~/.gpu_bench/results.json` after each run.
Full metrics are persisted: graphics API, GPU, CPU, particle count, difficulty,
timing breakdown (compute / render / total GPU), FPS, and bottleneck analysis.

| Feature | Interactive | CLI |
|---------|-----------|-----|
| List all results | Main menu → Delete results | `--results` |
| Compare (ranked table) | Main menu → Compare results | `--compare` |
| Detailed side-by-side | Compare → enter two rank #s | `--compare <id1> <id2>` |
| Delete one result | Delete → enter ID | `--results-delete <id>` |
| Clear all results | Delete → type `all` | `--results-clear` |
| Export to CSV | — | `--results-export <file.csv>` |

### Interactive Main Menu

On startup (when no CLI flags are given), the application presents:

```
========== GPU Benchmark ==========
  [0] Quick run (Vulkan 1.2 / RTX 5090 / Medium)  <- default
  [1] Custom run (choose API / GPU / difficulty)
  [2] Run again (same settings)
  [3] Compare results (N saved)
  [4] Delete results
  [5] Full analysis — one GPU (all APIs + RenderDoc + charts)
  [6] Full analysis — all GPUs x APIs (+ RenderDoc + charts)
  [7] Flights test — one GPU (all APIs + RenderDoc, custom flights)
  [8] Particle count test — one GPU (all APIs + RenderDoc, custom particles)
  [9] Headless compute — one GPU (all APIs, pure compute, no rendering)
  [10] Exit
====================================
```

- **Quick run** auto-selects the best GPU (discrete > integrated > software)
  and the best API that GPU supports (Vulkan > DX12 > DX11 > Metal).
- **Run again** appears after the first run and reuses the previous settings.
- **Full analysis** [5]/[6] — one-click workflow that:
  1. **[5]** selects one GPU (default: best dGPU); **[6]** runs every GPU.
  2. Benchmarks every supported API on the selected GPU(s) (1M particles, 15s).
  3. Triggers a RenderDoc frame capture at the 5-second mark on each run (if
     RenderDoc is attached or the in-app API detects the DLL).
  4. After all runs, automatically calls Python scripts to generate:
     - FPS comparison charts → `docs/images/`
     - Markdown results table → `docs/results-table.md`
     - Standalone HTML report → `docs/report.html`
  5. Prints a final comparison table to the console.

  Requires `pip install -r scripts/requirements.txt` for the Python step.
- **Flights test** [7] — benchmarks with a custom number of frames-in-flight
  (swapchain images). Tests how swapchain depth affects throughput. RenderDoc
  capture filenames include the flights count (e.g. `_flights3`).
- **Particle count test** [8] — benchmarks with a custom particle count
  (preset sizes from 65K to 16M, or enter any number). Tests GPU scaling
  behaviour. RenderDoc capture filenames include the particle count.
- **Headless compute** [9] — pure GPU compute benchmark with no window,
  swapchain, rendering, or presentation. Eliminates swapchain throttling and
  semaphore wait pollution, revealing true compute throughput. On the
  RX 9070 XT, headless mode achieves **21,000+ FPS** across all APIs
  (vs ~1,750 FPS windowed) — a 12× speedup.
- After each run the menu reappears — no need to restart the application.

---

## In Progress / Planned

### Near-term product order (updated 2026-07-15)

Current `cinematic_liquid_v2` acceptance status:

- [x] Current Vulkan physical-scene slice: 128x64x96 MLS-MPM, ten substeps and
      320,920 particles (142x14x98 base + 48x37x71 dam), with stiffness 45,000,
      viscosity 0.035 and max speed 8. The result identity is
      `cinematic_liquid_v2_physical_scene_v7`, shader version 9 and scene
      version 4. The user's adjusted mother duck and three ducklings are
      preserved together with the seven-body index ABI: boat=2, sink sphere=3,
      ducklings=4-6.
- [x] Surface/optics slice: fixed-u32 Spiky-squared particle splat into an
      independent 128x64x96 R32F volume, then a 5x5x5 binomial blend with
      `mix = 0.90`. The current renderer follows at most four medium interfaces,
      applying Fresnel/Snell, per-segment Beer-Lambert attenuation and opaque
      scene depth sorting. It uses `extinction=(30,10,8)`, linear exposure and
      zero density at volume boundaries. The reconstruction now adaptively
      preserves low-support spray/drop cells instead of erasing them.
- [x] Physical scene response: body-2 is now a finite 34 kg boat with a soft
      mooring; propeller reaction and fluid forces can drive and rock it, but
      its trajectory has not yet received visual acceptance. Body-3 remains a
      sealed sphere with density ratio 1.06, releases at 4.28 s, uses -9.81
      gravity and 0.015 air damping, and gates material water drag by immersed
      displaced mass. Its entry crown and whitewater come from the GPU body
      state and local fluid rather than a fragment-only fake; no secondary
      spray-particle pass exists yet.
- [x] Finite-height pool walls are embedded with inset 0.22. A limited outer
      simulation catch band lets particles cross the rim and fall to the
      ground, but it is not an infinite fluid domain. The foreground soft-PVC
      film is a separate IOR-1.50 Fresnel/weak-absorption/wrinkle approximation,
      not a full multi-medium PVC ray path. Rendered pool/PVC/liner/grass and
      the physical particle floor share ground y=0, with ring separation derived
      from twice the tube radius. The environment adds infinite
      procedural grass and an atmospheric sky/cloud treatment without cubemap
      assets.
- [x] Historical identities remain isolated: v1;
      `cinematic_liquid_v2_surface_splat_optics_v4` / shader version 6 formal;
      `cinematic_liquid_v2_duck_family_v5` / shader version 7 previews; and
      `cinematic_liquid_v2_iterative_optics_v6` / shader version 8 previews.
      None may be scored as current physical-scene v7. The
      implementation uses only architecture and parameter proportions from MIT-licensed
      [jeantimex/fluid](https://github.com/jeantimex/fluid); no upstream code or
      assets are vendored.
- [x] Historical optics_v4 RTX 5090 Vulkan formal 15-second run plus 5.1-second RenderDoc capture
      passed: result `20260715-170629-492`, Compute 10.572 ms, Render 1.553 ms,
      Total 12.125 ms, 263.98 MParticle-step/s and 966 measured frames. The
      0.103-second capture was excluded; one attempt saved one capture. The
      checked frame is
      `rdoc_captures/cinematic-liquid-v2-5s-formal-optics-v4.png`.
- [x] Historical v6 short preview passed on RTX 5090/Vulkan: result
      `20260715-221447-024`, Compute 9.450 ms, Render 3.003 ms, Total 12.454 ms,
      257.01 MParticle-step/s and 284 measured frames. Its excluded 0.117-second
      capture is `rdoc_captures/cinematic-liquid-v2-5s-iterative-optics-v6-final-preview.png`.
      This is a six-second `_preview`; v6 has no formal 15-second score.
- [x] Current v7 shader compilation, all six final SPIR-V validations, CLI
      Release and WinUI Release x64 builds passed. WinUI produced zero errors
      and only existing MSB8027/C4996/LNK4042-class warnings (two duplicate
      WinAppSDK warnings in the latest incremental build; four when affected
      sources were rebuilt). Only automatically
      terminating six/eight-second smoke runs were used. There is no formal
      15-second v7 score, reliably saved new RenderDoc capture, or complete
      visual acceptance; transient console value `241.13` is not a formal or
      persisted result. A window closing after a few seconds is the smoke
      script's `--time 8` lifecycle, not an established crash.
- [ ] Run the v7 formal 15-second + five-second capture flow only after the
      fixed-timestep trajectory contract is frozen; do not reuse any v4/v6 score.
- [ ] Exercise WinUI selection/run/history on the final build and record exact
      boat, sink-sphere and overflow-particle trajectories. Build success and
      short smoke runs are not GUI or visual acceptance.
- [ ] Freeze a cross-GPU trajectory contract that removes fixed-dt/render-rate
      feedback; audit Vulkan timestamp boundaries; and close abnormal-exit
      cleanup for every new surface buffer, image, descriptor and pipeline.
- [ ] Implement and validate DX12, DX11, OpenGL and Metal versions after the
      Vulkan scene/pass/quality contract is frozen; they are not supported now.

The independently versioned SPH vertical slice is now implemented at 318,464
particles. A complete 15-second RTX 5090/Vulkan visual run kept the pool, water,
duck family, balls, boat and environment stable; it stopped normally and the
user accepted the current appearance. Visual iteration is therefore closed for
this slice. Formal scoring is still blocked by render-frame-driven simulation,
missing per-substep rigid-body impulse clearing and the in-place viscosity SSBO
race; secondary spray/foam and SPH propeller wake remain later enhancements.

1. Keep the accepted SPH appearance fixed while closing its correctness contract:
   decouple simulation time from render rate, clear body impulses every substep,
   remove the viscosity race, then run the formal 15-second + fifth-second
   RenderDoc/timestamp acceptance. Preserve all MLS-MPM versions unchanged and
   isolated.
2. Immediately add three non-scored modes: Liquid Lab with a free camera and
   adjustable parameters, GPU Burn Unlimited Soak, and VRAM Integrity Soak.
   Explore/Soak sessions must never enter the benchmark leaderboard.
3. Use the generated v0.1.0 ZIP/Setup as the clean-machine candidate. Runtime
   Vulkan delay-load, official RenderDoc 1.45 staging, real Inno Setup 6.7.3,
   stage/ZIP inventories and Release asset hashes are complete. Remaining gates
   are the frozen report worker, project license, Authenticode signing, clean
   Windows VM install/upgrade/uninstall/capture, and physical GT 120 validation.
4. Continue WebGPU/backend registry and the wider cross-GPU matrix afterward.
5. Treat Vulkan KHR ray-query hybrid water RT, later DXR/Metal RT,
   frozen-snapshot path tracing, and DLSS/FSR/XeSS/MetalFX comparisons as later,
   separately versioned rendering suites rather than changes to the v2 score
   contract. Hardware RT and the current software density raymarch must remain
   separate capability paths and score groups.

Execution gate: item 5 is documentation-only for now. Do not start it until
the existing Stream/Particle and GPU Burn tests, scored Cinematic Liquid v2,
GUI integration, fixed 15-second/capture flow, and result contracts have all
passed their acceptance checks and the user explicitly raises its priority.

### Supplementary native CPU benchmark (Windows vertical slice implemented)

- [x] Native `cpu_mixed_v1` kernel with integer/branch/FP32/FP64 work, fixed
      three-round median aggregation, per-logical-processor sweep and all-logical-
      processor throughput. It opens no graphics window and invokes no RenderDoc.
- [x] CLI flags: `--cpu-benchmark [per-core|multi|all]`, `--cpu-time`,
      `--cpu-warmup`, and `--cpu-no-save`; the CPU path exits before GLFW/GPU
      probing. The WinUI CPU page provides Quick/Formal, live progress, detailed
      logical-processor rows, summary, raw output and Run/Cancel.
- [x] Successful summaries persist to `results.json`. Formal is exactly 15.0 s
      measurement + 0.2 s warm-up + three rounds; previews, affinity capability,
      timing and multi standalone/after-percore sequence receive separate result
      identities.
- [x] Windows Release smoke on a Ryzen 7 9800X3D: 16 logical / 8 physical / SMT2,
      Windows CPU Set topology, 16/16 per-core and 16/16 multi workers strict-
      pinned, exit 0. The short smoke is not a formal score.
- [ ] Rebuild and clean-machine-test the ZIP/Inno installer with this CPU slice;
      verify installed GUI-to-CLI discovery, Run/Cancel and History persistence.
- [ ] Validate >64-logical processor groups and a real hybrid CPU. Core-class
      names remain inferred OS-metadata ranks, not authoritative P/E/Mid/LPE
      identities.
- [ ] Native-test Linux as best-effort affinity and macOS as scheduler-managed;
      implement/validate Android, iOS and Web/WASM separately. Their results must
      never mix with Windows strict-affinity groups.

### DirectX 10-era compatibility (implemented, physical validation pending)

- [x] Replace optimistic DX11 detection with a real FL10_0/10_1/11.x device
      probe and optional DirectCompute 4.x feature query; expose the compute bit
      to WinUI and workload-aware API scheduling.
- [x] Compile FL10 workloads as SM4, skip compute/UAV resources for fragment-only
      tests, reject FP64 and oversized dispatches, and cap SM4 N-body at 4,096.
      All 16 production SM4 HLSL entries compile with Windows SDK FXC.
- [x] Delay-load `vulkan-1.dll` so a DirectX-only machine can enter DX11/WARP;
      guard both automatic probing and explicit Vulkan selection.
- [ ] Validate on the physical GeForce GT 120 under Windows 10 1809+ with its
      legacy driver. Do not add DX9: it cannot preserve the compute workloads
      or the RenderDoc capture contract. A Windows 7 target would require a
      separate legacy CLI/OS package because the current WinUI installer is
      Windows 10 1809+.

### RenderDoc Capture & Cross-GPU Analysis (completed historical work)

End-to-end RenderDoc profiling on multiple GPUs: **RX 9070 XT** (RDNA 4, 64 CU),
**RX 6900 XT** (RDNA 2, 80 CU), and the AMD iGPU (2 CU) as baseline.
Full step-by-step guide: [`renderdoc-capture-guide.md`](renderdoc-capture-guide.md).

- [x] Run baseline benchmarks (9070 XT + 6900 XT + iGPU, Vulkan + DX11) without RenderDoc.
- [x] Capture one Vulkan frame on each GPU via `--capture 5` (at 5s mark).
- [x] Take 7 annotated screenshots (event list, compute pipeline, SSBO data,
      graphics pipeline, barrier, render output, per-event timing).
- [x] Cross-validate app timestamp queries against RenderDoc GPU timing (< 5 %
      deviation target).
- [x] Write cross-GPU comparison (CU scaling, memory bandwidth, barrier cost).
- [x] Compare RDNA 4 vs RDNA 2 per-CU efficiency via RenderDoc per-event timing.
- [x] ~~Propose optimisations~~ — deferred; current analysis is sufficient.
- [x] Fill in [`renderdoc-analysis.md`](renderdoc-analysis.md) and
      update [`report.md`](report.md).

Code integration already complete:

- `VK_EXT_debug_utils` labels and object names in the Vulkan backend.
- RenderDoc In-Application API: auto-detect on launch, **F12** manual capture,
  `--capture <seconds>` for time-based unattended capture.

### Python Benchmark Tooling (P0 — done)

A `scripts/` directory with Python utilities for automated benchmark data
analysis, satisfying the JD's *"Scripting languages — Python, Perl, shell"*
requirement.

| Script | Purpose |
|--------|---------|
| [`scripts/plot_results.py`](../scripts/plot_results.py) | Read `~/.gpu_bench/results.json` and generate 4 charts: FPS by GPU × API, GPU time breakdown (compute/render), CPU overhead, particle-count scaling |
| [`scripts/batch_benchmark.py`](../scripts/batch_benchmark.py) | Automate batch runs across all GPU × API × particle-count combinations, with `--dry-run` preview |
| [`scripts/export_report.py`](../scripts/export_report.py) | Export results as markdown tables (for docs) or a standalone sortable HTML report |
| [`scripts/compare_3dmark.py`](../scripts/compare_3dmark.py) | Cross-validate project FPS against 3DMark Time Spy / Fire Strike scores — normalised bar chart, R² correlation scatter plot, deviation table |
| [`scripts/rdoc_export_timing.py`](../scripts/rdoc_export_timing.py) | Export per-event GPU timing from a RenderDoc `.rdc` capture to JSON — works in RenderDoc GUI Python Shell or standalone via `renderdoc` module |
| [`scripts/compare_rdoc_timing.py`](../scripts/compare_rdoc_timing.py) | Cross-validate app timestamp queries vs RenderDoc GPU timing — side-by-side comparison, deviation analysis, per-event breakdown |
| [`scripts/3dmark_scores.json`](../scripts/3dmark_scores.json) | 3DMark reference scores (edit with your own or public data) |
| [`scripts/requirements.txt`](../scripts/requirements.txt) | Python dependencies (`matplotlib`, `numpy`) |

```bash
# Install dependencies
pip install -r scripts/requirements.txt

# Generate charts as PNGs
python scripts/plot_results.py --save docs/images

# Batch-run all GPU × API combos (dry-run first)
python scripts/batch_benchmark.py --gpus 0 1 --dry-run
python scripts/batch_benchmark.py --gpus 0 1 --frames 500

# Export markdown table
python scripts/export_report.py --md docs/results-table.md

# Export standalone HTML report
python scripts/export_report.py --html docs/report.html

# Cross-validate against 3DMark
# Option A: auto-import from .3dmark-result files (saved by 3DMark GUI)
python scripts/compare_3dmark.py --import-3dmark "C:/Users/*/Documents/3DMark/*.3dmark-result"

# Option B: auto-import from exported XML (3DMark Professional --export)
python scripts/compare_3dmark.py --import-xml path/to/timespy.xml path/to/firestrike.xml

# Option C: manually edit scripts/3dmark_scores.json, then:
python scripts/compare_3dmark.py --save docs/images   # generate charts
python scripts/compare_3dmark.py --md                  # markdown table to stdout

# RenderDoc timing export & cross-validation
# Step 1: Export timing from .rdc capture (inside RenderDoc Python Shell)
#   exec(open('scripts/rdoc_export_timing.py').read())
# Step 1 (alt): Standalone (requires renderdoc on PYTHONPATH)
python scripts/rdoc_export_timing.py capture_6900xt.rdc -o rdoc_6900xt.json
python scripts/rdoc_export_timing.py capture_igpu.rdc   -o rdoc_igpu.json

# Step 2: Compare app timestamps vs RenderDoc timing
python scripts/compare_rdoc_timing.py rdoc_6900xt.json rdoc_igpu.json
```

### Web Backend — WebGL / WebGPU

Browser-based port of the particle benchmark, inspired by projects such as
[Volume Shader BM](https://volumeshader-bm.com/). Goals:

- **WebGPU** compute shader path (WGSL) for browsers with WebGPU support.
- **WebGL 2.0** fallback using transform feedback for particle updates.
- Hosted as a static site so anyone can run the benchmark without installing
  drivers or SDKs.
- Cross-platform, cross-system league table comparing results from native
  backends (Vulkan / DX / Metal) against web backends on the same hardware.

### Cross-Platform & Cross-GPU Performance Comparison

Written analysis document comparing frame rates and GPU timings across a
range of AMD and NVIDIA hardware spanning 16 years (2009–2025):

| GPU | Architecture | CUs / SPs | VRAM | Platform | Notes |
|-----|-------------|-----------|------|----------|-------|
| **RTX 5090** | Blackwell (NVIDIA) | 170 SMs | 32 GB GDDR7 | Windows | Current flagship — reference for cross-vendor comparison |
| **RX 9070 XT** | RDNA 4 (AMD) | 64 CUs | 16 GB GDDR6 | Windows | Latest AMD architecture — fastest per-CU compute tested |
| GTX 970 | Maxwell (NVIDIA) | 13 SMs | 4 GB GDDR5 | Windows | Older NVIDIA — reveals API ranking reversal vs modern GPUs |
| RX 6900 XT | RDNA 2 | 80 CUs | 16 GB | Windows | Flagship RDNA 2 |
| RX 6600 XT | RDNA 2 | 32 CUs | 8 GB | Windows | Mid-range RDNA 2 — half the CU count of 9070 XT, enables per-CU comparison |
| Vega Frontier Edition | Vega (GCN 5) | 64 CUs | 16 GB HBM2 | Windows | Prosumer / compute |
| RX 580 | Polaris (GCN 4) | 36 CUs | 8 GB | Windows | Mid-range GCN — baseline for normalised comparisons |
| FirePro D700 | Tahiti (GCN 1.0) | 2048 SPs | 6 GB | Windows | Mac Pro 2013 dual-GPU — each card benchmarked independently |
| HD 5770 | Evergreen (TeraScale 2, before GCN) | 800 SPs | 1 GB | Windows (DX11) | Legacy DX11-era GPU |
| Ryzen 7 9800X3D iGPU | RDNA 2 | 2 CUs | Shared | Windows | Integrated graphics |
| Ryzen 7 9800X3D (WARP) | Software | — | System RAM | Windows | Microsoft WARP software rasteriser on AMD CPU |

> **HD 5770 note:** Evergreen does not support Vulkan. Testing will use the
> DX11 backend only (Feature Level 11_0).
>
> **FirePro D700 note:** The Mac Pro (Late 2013) has two identical D700
> GPUs (GCN 1.0, Tahiti XT). macOS does **not** support CrossFire; each GPU
> is an independent `MTLDevice`. One GPU handles display output while the
> other is dedicated to compute. Both cards will be benchmarked individually
> via Metal, and optionally via MoltenVK (Vulkan→Metal) or Boot Camp DX11.
> This provides the only GCN 1.0 data point in the comparison.
> Although the D700 can create a DX12 device (Feature Level 11_0), its
> compute shader performance under DX12 is identical to WARP software
> rendering (~29 FPS vs ~28 FPS), indicating the driver does not
> accelerate DX12 compute on GCN 1.0. Vulkan 1.1 and DX11 run on the GPU
> normally (~570–600 FPS).
>
> **WARP note:** The Windows Advanced Rasterization Platform (WARP) is a
> high-performance software renderer included in DirectX. Running the DX11 /
> DX12 backends on WARP with an AMD CPU provides a pure-software baseline,
> isolating CPU compute throughput from GPU hardware.

The document covers:

- Per-backend (Vulkan / DX11 / DX12 / Metal / OpenGL / WARP) frame-rate comparison.
- Scaling behaviour when increasing particle count (65 K → 1 M → 16 M).
- Compute vs render timing breakdown per GPU (and CPU via WARP).
- Hardware vs software rendering comparison (discrete GPU vs WARP baseline).
- RX 9070 XT (RDNA 4) vs RX 6600 XT (RDNA 2): **4.1× per-CU compute improvement**
  with 2× the CU count (64 vs 32), demonstrating generational architectural gains.
- Headless compute mode: removing swapchain/render/present reveals true compute
  throughput — RX 9070 XT achieves 21,000+ FPS in headless vs 1,750 windowed.
- Swapchain semaphore wait pollution analysis — why fast GPUs (9070 XT, RTX 5090)
  report inflated render timestamps in windowed mode.
- Cross-validation against 3DMark Time Spy and Fire Strike across all GPUs.
- OpenGL compute dispatch overhead on AMD GPUs (2.6–48 ms, vs 0.03 ms on Vulkan).
- Generational progression from TeraScale 2 → GCN 1.0 → GCN 4 → GCN 5 → RDNA 2 → RDNA 4.
- Mac Pro 2013 dual-GPU analysis: display GPU vs headless GPU performance
  isolation, and macOS Metal vs Boot Camp DX11 cross-platform comparison.

### Workgroup Size Tuning Experiments

Sweep `local_size_x` across powers of two (32, 64, 128, 256, 512, 1024) and
measure the impact on compute dispatch time for each GPU above. Publish
findings as an analysis document covering:

- Optimal workgroup size per architecture (GCN vs RDNA 2 vs RDNA 4 vs Maxwell vs Blackwell).
- Occupancy and wavefront utilisation implications.
- Correlation with CU count and cache hierarchy.

### Memory Allocation Strategy Comparison

Benchmark and document the performance difference between:

| Strategy | Vulkan Flags | Use Case |
|----------|-------------|----------|
| Host-visible / host-coherent | `HOST_VISIBLE \| HOST_COHERENT` | Current approach — simple, CPU-mappable |
| Device-local + staging buffer | `DEVICE_LOCAL` + staging copy | Optimal for discrete GPUs |
| Persistent mapping | `HOST_VISIBLE \| HOST_COHERENT` + persistent `vkMapMemory` | Avoids repeated map/unmap |
| Device-local host-visible (ReBAR) | `DEVICE_LOCAL \| HOST_VISIBLE` | AMD SAM / ReBAR on supported GPUs |

Measure particle-buffer throughput and compute dispatch latency for each
strategy across integrated and discrete GPUs.

### Multi-Draw-Call Stress Test

The current benchmark issues a single compute dispatch and a single draw call
per frame — a workload profile that favours DX11's highly optimised implicit
driver path over DX12/Vulkan's explicit model (see
[`report.md`](report.md) for measured data).

To demonstrate the scalability advantage of explicit APIs, add an optional
**multi-draw-call mode**:

- Render particles in batches (e.g. 1 draw call per 1 024 particles), producing
  1 000+ draw calls per frame at default particle counts.
- Record draw commands across **4–8 threads** on DX12 (secondary command lists)
  and Vulkan (secondary command buffers), then submit in a single primary.
- Compare single-threaded vs multi-threaded CPU submission time per API.
- Expected outcome: DX12/Vulkan overtake DX11 when draw-call count is high
  enough for the driver's single-threaded path to become the bottleneck.

This will complete the cross-API analysis by showing both sides of the
implicit-vs-explicit trade-off.

### Advanced Particle Interactions

Extend the compute shader to support richer physics:

- **Gravitational attraction** — N-body style pairwise forces (shared-memory
  tiling for O(N log N) or O(N²) with optimisation notes).
- **Simple collision response** — spatial hashing or grid-based broad phase.
- **Attractors / repulsors** — mouse-driven interactive forces.

This demonstrates more complex compute shader design, including shared-memory
optimisation and synchronisation within workgroups.

### HIP / ROCm Headless Compute Backend

Add a headless (no rendering) compute benchmark using AMD's
[HIP](https://github.com/ROCm/HIP) runtime:

- Port the particle-update kernel from GLSL/HLSL to a HIP kernel.
- Time kernel dispatch with `hipEvent` and output the same standardised
  benchmark report as the graphics backends.
- Compare HIP kernel throughput against Vulkan/DX compute shader dispatch on
  identical AMD hardware.
- HIP compiles for both AMD (ROCm) and NVIDIA (CUDA back-end) GPUs, so the
  same source covers both vendors.

### CUDA Headless Compute Backend

Equivalent headless compute benchmark targeting NVIDIA GPUs natively:

- Port the particle-update kernel to a CUDA kernel (`.cu`).
- Time with `cudaEvent` and produce the same report format.
- Compare CUDA kernel throughput against Vulkan compute and the HIP path on
  NVIDIA hardware.

### Explicit Multi-GPU — Split Compute Across Dual GPUs

Implement explicit multi-GPU support, splitting the particle compute workload
across two physical GPUs and merging results for rendering. Target hardware:
**Mac Pro 2013 dual FirePro D700** (GCN 1.0, 6 GB each).

| API | Mechanism | Status |
|-----|-----------|--------|
| **Metal** (primary) | `MTLCopyAllDevices()` → two `MTLDevice` / `MTLCommandQueue`, split particle buffer, `MTLSharedEvent` cross-GPU sync | Planned — most feasible path; macOS natively exposes both D700s |
| **DX12** | `IDXGIFactory6::EnumAdapters` → Linked or Unlinked Explicit Multi-Adapter, `ID3D12Fence` cross-GPU sync | Long-term — requires Boot Camp + working DX12 driver for D700 |
| **Vulkan** | `VK_KHR_device_group` / `VK_KHR_device_group_creation`, sub-allocate per-device memory, semaphore sync | Long-term — needs dual Vulkan ICDs on the same machine |

Tasks:

- [ ] Metal: enumerate both D700s, create per-device command queues and
      particle buffers (each device owns half the particles).
- [ ] Metal: dispatch compute on both devices in parallel, synchronise with
      `MTLSharedEvent`, blit results to the display-GPU buffer.
- [ ] Metal: render merged particle buffer on the display GPU.
- [ ] Benchmark single-GPU vs dual-GPU throughput (ideal ≈ 2× compute, less
      for render due to data transfer overhead).
- [ ] Write analysis document: scaling efficiency, PCIe transfer cost,
      synchronisation overhead, comparison with implicit CrossFire AFR.
- [ ] (Optional) DX12 Explicit Multi-Adapter implementation on Boot Camp
      Windows, if D700 drivers support DX12.
- [ ] (Optional) Vulkan `VK_KHR_device_group` implementation on a system with
      two discrete Vulkan-capable GPUs.
