# Metal maths probe

GPU Burn scores less per TFLOPS on Apple GPUs than on NVIDIA and AMD (see
[docs/interpreting-gpu-burn.md](../../docs/interpreting-gpu-burn.md)). A GPU Burn
step calls `atan2`, `sin`, `cos`, `exp` and `pow` some twenty times. This probe
measures how fast the GPU computes each of them, next to its FMA rate. It also
times one GPU Burn step (the loop body copied from `shaders/gpu_burn.metal`),
both as a compute kernel and as a full-screen fragment shader.

Run it on a Mac:

```bash
bash scripts/metal-math-probe/run.sh
```

It builds with `xcrun clang++`, runs for well under a minute, prints a table per
maths mode, and saves it to `out/test-results/metal-math-probe-<time>/output.txt`.
Pass `fast` or `precise` to run one mode only. `fast` is how the engine compiles
GPU Burn.

## Reading the output

| row or column | meaning |
|---|---|
| `G results/s` | results of that function per second over the whole GPU (16 per thread and loop iteration) |
| `vs fma` | that rate divided by the FMA rate: 0.25 means a quarter of the FMA rate |
| `fma (literal)` | the same FMA kernel with its constants written as literals instead of read from the parameter buffer |
| `GPU Burn step` | pixel-steps per second of the copied loop, as a compute kernel on a 1280 x 720 grid |
| `Burn fragment` | the same loop drawn as a full-screen fragment shader into a 1280 x 720 BGRA8 target, and its rate as a fraction of the compute kernel |

The `MATHPROBE` lines carry the same figures in tab-separated form.

## Results so far

First compiled and run unchanged on the M4 Pro on 2026-10-16. Fast mode measured:
- 6.964 TFLOPS for the FMA loop;
- sin/FMA 0.122 and atan2/FMA 0.067;
- GPU Burn step 14.644 Gpix-step/s.

See the [raw output](../../out/test-results/metal-math-probe-20261016-233740/output.txt)
and [run report](../../out/test-results/macos-targeted-retest-20261016/report.md).

The D3D11 version (`scripts/gpu-math-probe-windows`, RTX 5090 and a Radeon iGPU,
2026-10-17) found that special functions explain only part of the GPU Burn gap.
It raised two questions that the two newer rows answer:

- **Is 6.96 TFLOPS the M4 Pro's FMA peak?** On the RTX 5090 the FMA kernel that
  reads its constants from the parameter buffer ran 25 % below the literal
  version. If `fma (literal)` is clearly faster on the M4 Pro too, its TFLOPS and
  every `vs fma` ratio have to be read against the literal row.
- **Where does GPU Burn lose a third?** From its 16, 64 and 256-step results, GPU
  Burn on the M4 Pro runs each step at 9.75 G pixel-steps/s, 67 % of the compute
  step (14.64). The fixed cost after the loop is only about 2.4 steps. The RTX
  5090 runs each step at 99 % of its compute step. If `Burn fragment` also comes
  out near 0.67 of the compute step, the fragment stage alone accounts for the
  loss. The next look is then a GPU capture in Xcode of both versions:
  occupancy and register use.

The two rows were compiled and run on the M4 Pro on 2026-10-17 (Xcode 27.1).
Fixed 256-step fragment/compute was 97.7–98.4%, so the pure fragment stage does
not account for the real shader's 67%. Xcode found about 50% extra fragment
invocations in the complete shader. Removing the final discard only in a
diagnostic copy almost halved single-draw time, but made the first of two opaque
draws disappear and changed the image. The control is not a valid replacement
benchmark. Warmed paired literal FMA was 7.030 versus 6.817 TFLOPS, gain 3.13%.
See the [report and captures](../../out/test-results/metal-math-diagnosis-20261017/report.md).

## Fixed-step and visibility controls

`burn_stage_probe.mm` reuses the simplified loop from the original probe and
reads `shaders/gpu_burn.metal` directly for the unmodified full shader. It stores
no App scores. From the repository root:

```bash
mkdir -p "${TMPDIR:-/tmp}/metal-math-probe"
xcrun clang++ -std=c++17 -O2 -fobjc-arc -framework Foundation -framework Metal \
  scripts/metal-math-probe/burn_stage_probe.mm \
  -o "${TMPDIR:-/tmp}/metal-math-probe/burn_stage_probe"
"${TMPDIR:-/tmp}/metal-math-probe/burn_stage_probe" shaders/gpu_burn.metal
```

An optional second argument selects `fma` (warmed forward/reverse FMA), `discard`
(single/two-draw visibility controls), or a new `.gputrace` path. Capture requires
`MTL_CAPTURE_ENABLED=1` in that process's environment. Replays and profiling can
run at a different performance state: use uncaptured timings for throughput,
and captured counters to investigate execution, never interchange their times.

`MATHSTAGE` rates count **submitted** pixel-steps. In the no-discard two-draw
control the first draw can be culled, so its printed rate does not represent two
executed loops. Its image differs too. This control must not populate formal
`results.json` or replace any existing score. All raw 7-sample timings, including
startup outliers, are retained; the fixed-step summary is their median.
The `mathFloatingPointFunctions` setting needs the macOS 15 SDK or newer.
