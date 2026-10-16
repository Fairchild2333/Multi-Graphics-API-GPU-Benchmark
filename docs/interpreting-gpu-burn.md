# Interpreting the GPU Burn score

GPU Burn (`gpu_burn_v3_fixed_steps_{16,64,256}`) draws two full-screen passes of
1280 x 720 per frame. Every pixel of each pass runs a fixed number of raymarch
steps, with no early exit. The score is

```
Gpix-step/s = 1280 x 720 x steps x 2 passes / GPU time of the frame
```

For example, an iPhone 13 Pro at 64 steps: 1280 x 720 x 64 x 2 / 75.84 ms =
1.555 Gpix-step/s.

## What a step costs

Each step evaluates `plasmaField` once and updates a small register state (the
loop body in `shaders/gpu_burn.frag`, ported to `gpu_burn.metal` and
`gpu_burn.hlsl`). Per step that is about:

- 2 `atan`, 9 - 11 `sin`, 2 `cos`, 1 `pow`, 1 `exp`, 2 `length` (square
  roots) and 2 divisions;
- about a hundred FP32 multiplies and adds, and an integer hash.

Three `rotate2` calls add 3 `sin` and 3 `cos` per step, but they depend only on
the frame time, so a compiler may lift them out of the loop.

Most of the time therefore goes to special functions (sine, arctangent,
exponent, power). The score measures how fast a GPU computes those inside a
fragment shader. It is not a rating of FP32 throughput in general, and not of
game performance, which depends far more on rasterisation, textures and memory
traffic.

## Apple GPUs score about half as much per TFLOPS here

Measured 2026-07 to 2026-10, GPU Burn at 256 steps:

| GPU | API | Gpix-step/s | FP32 TFLOPS | Gpix-step/s per TFLOPS |
|---|---|---|---|---|
| RTX 5090 | Vulkan | 198.8 | 104.8 (NVIDIA) | 1.9 |
| RTX 5090 | DX12 | 224.1 | 104.8 | 2.1 |
| Radeon iGPU of the Ryzen 7 9800X3D (2 CU, 2.2 GHz) | Vulkan | 1.113 | 0.56 | 2.0 |
| Radeon iGPU of the Ryzen 7 9800X3D | DX11 | 1.396 | 0.56 | 2.5 |
| M4 Pro, 20-core GPU | Metal | 9.66 | about 9 (estimate; Apple publishes none) | about 1.1 |

The mobile Apple GPUs show the same thing against an outside ranking:
- socpk's GPU index, mostly game benchmarks with Snapdragon 865 = 100, has
  A16 263, A12X 249 and A15 241, against 189 for Snapdragon 8 Gen 1 and 143 for
  Kirin 9000.
- In GPU Burn at 64 steps (Metal) the A15 scores 1.555, the A16 1.531 and the
  A12X 1.41 - 1.49. That is the level of the Kirin 9000 (1.62) and below
  Snapdragon 8 Gen 1 (1.82).

What was ruled out:

- **Timing.** On iOS the GPU time of a frame matches the frame interval within
  2 % (A15: 75.8 ms against 77.1 ms), so the GPU is busy all the time and the
  score is its real speed on this shader.
- **Translation.** Native Metal is the fastest API on Apple hardware. M4 Pro at
  256 steps: Metal 9.66, MoltenVK 7.10, OpenGL 5.92. A15 at 64 steps: Metal
  1.555, MoltenVK 1.036. Compare Apple devices by their Metal results.
- **Precise maths.** The Metal backend compiles with fast maths
  (`ConfigureFastMath` in `src/metal_backend.mm`).

Not known yet: why. The likely cause is that Apple GPUs compute these
functions slowly relative to their FMA rate. `scripts/metal-math-probe` measures
that on a Mac: the rate of FMA, `sin`, `cos`, `atan2`, `exp`, `pow`, `sqrt` and
division, and the cost of one GPU Burn step, as a compute kernel.

Mac Metal GPU Burn results from before 2026-10-14 (4.2 - 4.8 Gpix-step/s on the
M4 Pro) recorded a GPU time twice the frame interval. They were superseded by the
`serial_timing_v2` results (9.4 - 9.7) and are not comparable with anything.

## How to read it

- GPU Burn ranks GPUs by special-function throughput. A GPU that scores low here
  can still be fast in games, and the reverse.
- Compare the same step count. At 16 steps fixed per-frame costs weigh more
  (RTX 5090: 140.9 at 16 steps against 198.8 at 256).
- On Apple devices use Metal. MoltenVK on iOS also waits for every frame before
  timing it (`sync_timing`).
- For memory-bound behaviour use the particle test instead. Its cross-device
  reading is in [interpreting-stream-bandwidth.md](interpreting-stream-bandwidth.md).
