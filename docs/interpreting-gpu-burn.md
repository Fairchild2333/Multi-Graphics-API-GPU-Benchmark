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

As compiled by FXC, a step is 23 special operations (sine and cosine, exponent,
logarithm, square root, reciprocal) and about 190 others. The special functions
run on slower units, at 1/4 to 1/8 of the FMA rate, but on an RTX 5090 and a
Radeon iGPU they largely overlap with the other arithmetic: a step costs the
time of about 180 - 240 FMAs there. The score measures this mixed arithmetic in
a fragment shader. It is not a rating of FP32 peak, and not of game performance,
which depends far more on rasterisation, textures and memory traffic.

## Apple GPUs score less per TFLOPS here

GPU Burn at 256 steps, measured 2026-07 to 2026-10. "Measured TFLOPS" comes from
the FMA kernel of the maths probes (`scripts/metal-math-probe`,
`scripts/gpu-math-probe-windows`); the vendor figure is given for reference.

| GPU | API | Gpix-step/s | FP32 TFLOPS, vendor | FP32 TFLOPS, measured | per measured TFLOPS |
|---|---|---|---|---|---|
| RTX 5090 | Vulkan | 198.8 | 104.8 | 73.9 (98.5 with literal constants) | 2.69 (2.02) |
| RTX 5090 | DX12 | 224.1 | 104.8 | 73.9 (98.5) | 3.03 (2.28) |
| Radeon iGPU of the Ryzen 7 9800X3D (2 CU) | Vulkan | 1.113 | 0.56 | 0.667 | 1.67 |
| Radeon iGPU of the Ryzen 7 9800X3D | DX11 | 1.396 | 0.56 | 0.667 | 2.09 |
| M4 Pro, 20-core GPU | Metal | 9.66 | none published | 6.96 | 1.39 |

Per measured TFLOPS the M4 Pro reaches 0.46 - 0.83 of the others. An earlier
version of this page used an estimated 9 TFLOPS for the M4 Pro and said "about
half"; that holds only against the RTX 5090.

The maths probes separate compute-loop cost from the complete shader path (2026-10-17):

- **Inside the compute loop the difference is small.** One GPU Burn step,
  run as a compute kernel, costs the time of 238 FMAs on the M4 Pro, 180 - 239
  on the RTX 5090 (depending on which FMA kernel is the reference) and 206 on
  the iGPU. Against FMA, `sin` and `cos` run at 0.12 on the M4 Pro, 0.19 on the
  5090 and 0.24 on the iGPU, while `exp`, square root, division, `pow` and
  `atan2` are relatively as fast or faster on the M4 Pro. Special functions
  explain only part of the gap.
- **The larger part appears with the complete Burn shader.** The real GPU
  Burn reaches 66 % of the speed of its simplified compute loop on the M4 Pro, against
  97 - 109 % on the RTX 5090 and 69 - 86 % on the iGPU.

The 16, 64 and 256-step results split a frame into a fixed cost per pixel (the
shading after the loop) and a cost per step. Two of the three results give both
numbers, and the third checks them: the M4 Pro is predicted at 9.40 for 64 steps
and measured at 9.40, the iGPU's Vulkan 1.039 against 1.042.

| GPU | API | per step in GPU Burn (G pixel-steps/s) | compute loop (probe) | ratio | fixed cost after the loop |
|---|---|---|---|---|---|
| M4 Pro | Metal | 9.75 | 14.64 | 67 % | about 2.4 steps |
| RTX 5090 | Vulkan | 204.4 | 206.0 | 99 % | about 7 steps |
| RTX 5090 | DX12 | 230.8 | 206.0 | 112 % | about 8 steps |
| Radeon iGPU | Vulkan | 1.14 | 1.62 | 70 % | about 6 steps |
| Radeon iGPU | DX11 | 1.43 | 1.62 | 88 % | about 7 steps |

The small fitted intercept does not prove that post-loop code has no effect on
how the whole shader runs. In particular, the final discard depends on values
computed by the scored loop. The score counts two submitted draws, while the
hardware can execute additional fragment work to resolve visibility.

### M4 Pro follow-up: discard and visibility execution

The newer probe rows were built and run on the Mac on 2026-10-17, followed by
fixed-step controls and Xcode GPU captures. Full evidence, timings and limits
are in the [diagnosis report](../out/test-results/metal-math-diagnosis-20261017/report.md).

| Same 256 steps, two fixed phases | Gpix-step/s |
|---|---:|
| Simplified compute loop | 14.57–14.62 |
| Simplified fragment loop | 14.27–14.34 |
| Unmodified complete shader, two draws | 9.41–9.43 |

The simplified fragment is **97.7–98.4%** of compute. The 67% deficit is therefore
not inherent to the fragment stage alone. Xcode captures of the complete shader
showed **2.756–2.785 million fragment invocations** for **1.8432 million submitted
pixel-draws**, about 50% extra execution. The complete shader allocated 134
registers against 68 for the simple fragment, but neither spilled; captured FS
occupancy was 90.18% versus 85.79%, so lower occupancy is not established as the
cause.

A diagnostic copy removing only the final `discard_fragment()` reduced a single
draw from about 35.6 ms to 18.4 ms at 256 steps. Together with the extra invocations,
this supports the discard/visibility path as the main source of the gap. Exact
internal scheduling remains an inference; see Apple's [GPU architecture](https://developer.apple.com/videos/play/wwdc2020/10602/)
and [counter guidance](https://developer.apple.com/videos/play/wwdc2020/10603/).
It does not establish the same cause on every Apple device or graphics API.

**Deleting discard is not a valid benchmark fix.** With no discard, the opaque
second draw hides the first, and the capture shows roughly one screen of fragment
work. Dividing two submitted draws by that time produces an invalid replacement
score of 25–26. It also changes the image. Any production change must preserve the
observable work of both draws and the image; this diagnosis changed neither the
engine/shader nor the stored results.

Literal FMA was initially 7.291 versus 6.783 TFLOPS. A warmed forward/reverse
pairing gave 7.030 versus 6.817, a 3.13% difference, far smaller than the Windows
probe's literal gain. Neither FMA row establishes the M4 Pro's hardware peak.
The earlier probe and Windows comparison remain historical evidence:
[M4 initial output](../out/test-results/metal-math-probe-20261016-233740/output.txt),
[Windows comparison](../out/test-results/gpu-math-probe-windows-20261017-031454/report.md).

The mobile Apple GPUs rank lower in GPU Burn than in an outside ranking:
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

Mac Metal GPU Burn results from before 2026-10-14 (4.2 - 4.8 Gpix-step/s on the
M4 Pro) recorded a GPU time twice the frame interval. They were superseded by the
`serial_timing_v2` results (9.4 - 9.7) and are not comparable with anything.

## How to read it

- GPU Burn ranks GPUs by how fast they run this mixed arithmetic in a fragment
  shader. A GPU that scores low here can still be fast in games, and the reverse.
- Compare the same step count. At 16 steps fixed per-frame costs weigh more
  (RTX 5090: 140.9 at 16 steps against 198.8 at 256).
- On Apple devices use Metal. MoltenVK on iOS also waits for every frame before
  timing it (`sync_timing`).
- For memory-bound behaviour use the particle test instead. Its cross-device
  reading is in [interpreting-stream-bandwidth.md](interpreting-stream-bandwidth.md).
