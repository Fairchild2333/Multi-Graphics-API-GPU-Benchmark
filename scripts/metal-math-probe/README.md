# Metal maths probe

Why GPU Burn scores about half as much per TFLOPS on Apple GPUs as on NVIDIA and
AMD is open (see [docs/interpreting-gpu-burn.md](../../docs/interpreting-gpu-burn.md)).
A GPU Burn step calls `atan2`, `sin`, `cos`, `exp` and `pow` some twenty times.
This probe measures how fast the GPU computes each of them, next to its FMA
rate. It also times one GPU Burn step: the loop body copied from
`shaders/gpu_burn.metal`.

Run it on a Mac:

```bash
bash scripts/metal-math-probe/run.sh
```

It builds with `xcrun clang++`, runs for well under a minute, prints a table per
maths mode, and saves it to `out/test-results/metal-math-probe-<time>/output.txt`.
Pass `fast` or `precise` to run one mode only. `fast` is how the engine compiles
GPU Burn.

## Reading the output

| column | meaning |
|---|---|
| `G results/s` | results of that function per second over the whole GPU (16 per thread and loop iteration) |
| `vs fma` | that rate divided by the FMA rate: 0.25 means a quarter of the FMA rate |
| `GPU Burn step` | pixel-steps per second of the copied loop on a 1280 x 720 grid |

For `GPU Burn step`, a real GPU Burn frame also shades each pixel after the
loop and runs as a fragment shader, so it scores somewhat less than this figure.
The M4 Pro's GPU Burn result is 9.4 - 9.7 Gpix-step/s.

The `MATHPROBE` lines carry the same figures in tab-separated form.

What to look for: if `sin`, `atan2`, `exp` and `pow` come out at a small
fraction of the FMA rate, Apple's special-function throughput explains the
GPU Burn gap. To compare with another vendor, the same kernels would have to run
on that GPU as a Vulkan or D3D compute shader; that version does not exist yet.

Not compiled or run yet: it was written on Windows on 2026-10-16. The
`mathFloatingPointFunctions` setting needs the macOS 15 SDK or newer.
