# GPU maths probe for Windows (D3D11)

The [Metal maths probe](../metal-math-probe/README.md) as a D3D11 compute shader,
so the same kernels run on NVIDIA and AMD GPUs and can be set against the M4 Pro.
It measures the rate of FMA, `sin`, `cos`, `atan2`, `exp`, `pow`, `sqrt` and
division, and of one GPU Burn step (the loop body of `shaders/gpu_burn.hlsl`).

Run it on Windows:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\gpu-math-probe-windows\run.ps1
```

It builds `gpu_math_probe.cpp` with the VS 18 BuildTools MSVC into
`%TEMP%\gpu-math-probe-windows`, runs once on every hardware GPU and saves to
`out/test-results/gpu-math-probe-windows-<time>/`:

| file | content |
|---|---|
| `output-adapter<N>.txt` | the table for DXGI adapter N |
| `adapters.txt` | every DXGI adapter with its LUID and driver version |
| `dxbc/*.dxbc.txt` | the compiled DXBC of each kernel |

`-Adapters 0,1` picks adapters by their DXGI index. Without it the script runs
the first adapter of each hardware GPU name: a virtual display driver lists the
GPU it renders on again under its own LUID (on the test PC the RTX 5090 is
adapters 0, 2 and 3), and those copies are skipped. By hand:
`gpu_math_probe.exe --list`, `gpu_math_probe.exe --adapter 1`.

## What is the same as the Metal probe

- The kernels: four independent `float4` chains per thread (16 results per loop
  iteration) with the same expressions. HLSL has `fma` for doubles only, so the
  FMA kernel uses `mad`, which the drivers turn into a fused multiply-add.
- 1280 x 720 = 921 600 threads in groups of 256; the burn-step kernel treats each
  thread as one pixel and its loop count as the step count.
- Timing: the loop count is doubled until one dispatch takes 20 ms, then scaled
  to about 50 ms, and the shortest GPU time of five dispatches counts. GPU time
  comes from D3D11 timestamp queries inside a disjoint query.
- The output: `function`, `iterations`, `GPU ms`, `G results/s`, `vs fma`, FP32
  TFLOPS on the FMA row, G pixel-steps/s on the burn row, and `MATHPROBE` lines.

## What is different

- Compiler: `D3DCompile`, `cs_5_0`, flags 0, as `src/dx11_backend.cpp` and
  `src/dx12_backend.cpp` compile GPU Burn. That is default optimisation without
  IEEE strictness. D3D has no precise mode for these functions, so there is one
  table, to be read against the Metal `fast` one. FXC turns `atan2` into a
  polynomial with one reciprocal and `pow` into `log`, `mul`, `exp`; see `dxbc/`.
- One second of FMA dispatches before the table, so the clocks have left idle
  before the rate every other row is divided by.
- Two `Check` lines after the table: the FMA kernel once more (if it moved, the
  clocks moved during the run) and the FMA kernel with `a` and `b` written as
  literals instead of constant-buffer values. On the RTX 5090 the literal version
  is 1.33 times as fast (98.5 against 73.9 TFLOPS), so the `fma` row is not that
  GPU's FMA peak. Read `vs fma` as "against this FMA kernel"; the literal row
  gives the other denominator. The Metal probe has no literal row yet.

## First results (2026-10-17)

| GPU | FMA kernel TFLOPS | literal FMA TFLOPS | sin / FMA | atan2 / FMA | GPU Burn step |
|---|---:|---:|---:|---:|---:|
| RTX 5090 | 73.93 | 98.51 | 0.187 | 0.057 | 205.968 Gpix-step/s |
| Radeon iGPU of the 9800X3D | 0.667 | 0.663 | 0.238 | 0.040 | 1.622 Gpix-step/s |
| M4 Pro (Metal probe, fast) | 6.96 | not measured | 0.122 | 0.067 | 14.644 Gpix-step/s |

The comparison and what it does and does not show are in the
[report](../../out/test-results/gpu-math-probe-windows-20261017-031454/report.md).
