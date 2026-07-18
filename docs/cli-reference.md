# Mangekyo CLI Reference — `gpu_benchmark`

Complete record of every Mangekyo CLI test: parameters, what it runs, and which
external tools it invokes. The product name changed, but the compatible internal
command remains `gpu_benchmark`. Source: [`src/main.cpp`](../src/main.cpp), defaults in
[`src/gpu_common.h`](../src/gpu_common.h).

## 1. Default config (`BenchmarkConfig`, gpu_common.h:103-120)

| Field | Default | Notes |
|---|---|---|
| `workload` | `Stream` (bandwidth) | one of the 12 public selections listed under `--workload` below |
| `particleCount` | `1048576` (= Medium) | 1M particles |
| `maxRunTimeSec` | `15.0` | time-mode duration |
| `warmupTimeSec` | `2.0` | time-mode warmup; runs shorter than 8s reduce this to at most 25% of total duration |
| `benchFrames` | `2000` | frame-mode count (only with `--benchmark`) |
| `warmupFrames` | `100` | frame-mode warmup |
| `framesInFlight` | `2` (max 16) | flights |
| `vsync` | `false` | |
| `headless` | `false` | |
| `hostMemory` | `false` | |
| `captureAtSec` | `-1.0` | <0 = no RenderDoc capture |
| `renderDocEnabled` | `true` | master switch; `--no-renderdoc` skips DLL/API initialization and manual F12 |
| `fractalIter` / `peakIters` | `2000` / `16384` | stress / synthpeak |
| n-body bodies | `65536` | `kNBodyDefaultBodies` |

### Two run modes (app_base.cpp:391-421)

- **Time mode** (default, `benchmarkMode=false`): runs `maxRunTimeSec` seconds
  (default 15s), including a 2s warmup. Runs shorter than 8s cap warmup at 25%
  of total duration so even a 1s preview has measured frames and GPU timestamp
  samples. The formal 15s contract remains unchanged. **All interactive-menu
  runs use this.**
- **Frame mode** (`benchmarkMode=true`, only via `--benchmark [frames]`): runs
  `benchFrames` (default 2000) frames, ignores the time limit.

Every successful GPU run appends its result to `results.json`. Successful CPU
runs append summary rows unless `--cpu-no-save` is supplied.

## 2. Interactive menu [0]–[10] (main.cpp:1026-1041)

> All menu runs are **15s time mode + Stream workload + Medium (1M) particles +
> V-Sync off**, unless overridden on the command line.

### [0] Quick run
- Params: `backend = recommended API`, `gpu = recommended GPU`, Medium 1M.
- Runs: one GPU×API, 15s, Stream. Tools: none.

### [1] Custom run
- Params: `backend = auto` (prompts for API), prompts for difficulty
  (Light/Medium/Heavy/Extreme). Runs: one GPU×API, 15s. Tools: none.

### [2] Run again
- Reuses the previous `benchCfg` (only if a prior run exists).

### [3] Compare results
- No GPU run. Calls `PrintComparisonTable`; optionally `PrintDetailedComparison`
  for two chosen ranks.

### [4] Delete (submenu)
- [1] `ClearResults()`; [2] delete Python charts/reports; [3] delete all.

### [5] / [6] Full Analysis — one GPU / all GPUs
- Config: Medium 1M, vsync off, **`captureAtSec = 5.0`**, 15s, Stream workload
  (faCfg does not set a workload).
- Runs: each available API (Vulkan/DX12/DX11/OpenGL) of the selected GPU (single)
  or of all GPUs, 15s each, RenderDoc capture at the 5s mark.
- Post-processing tools (in order):
  1. `renderdoccmd.exe convert` — `.rdc` → chrome JSON (main.cpp:1396)
  2. `python scripts/rdoc_analyse.py --captures rdoc_captures --results <results.json> --output docs/rdoc_comparison.md`
  3. `python scripts/plot_results.py --save docs/images`
  4. `python scripts/export_report.py --md docs/results-table.md`
  5. `python scripts/export_report.py --html docs/report.html`

### [7] Flights test — one GPU, all APIs
- Prompts frames-in-flight (default 2, 1–16); `captureAtSec = 5.0`, 15s.
- Tools: `renderdoccmd.exe convert` only (no Python).

### [8] Particle test — one GPU, all APIs
- Particle presets: Light 65536 / Medium 1048576 / Heavy 4194304 / Extreme
  16777216, or a custom count (rounded to 256); `captureAtSec = 5.0`, 15s.
- Tools: `renderdoccmd.exe convert` only.

### [9] Headless compute — one GPU, all APIs
- `headless = true`, `captureAtSec = -1` (no capture), 15s, no window/render/present.
- Tools: none.

### [10] Exit

## 3. Command-line flags (non-interactive, main.cpp:657-682)

| Flag | Effect |
|---|---|
| `--backend <vulkan\|dx12\|dx11\|metal\|opengl>` | select backend (default auto) |
| `--gpu <index>` / `--warp` | select GPU / WARP software renderer (DX only) |
| `--vsync` / `--host-memory` | enable vsync / keep particle buffer in host RAM |
| `--flights <N>` | frames-in-flight (1–16) |
| `--headless` | pure compute, no window |
| `--particles <count>` | particle count (rounded to 256, skips difficulty menu) |
| `--workload <id>` | select one of 12 public ids: `stream`, `nbody`, `gpu_burn`, `gpu_burn_v1`, `gpu_stress`, `stress`, `synthpeak`, `render3d`, `volumetric`, `cinematic_liquid`, `cinematic_liquid_v1`, `fluid`; `gpu_burn` is Mangekyo Kaleidoscope v2 and `gpu_burn_v1` preserves Plasma Bloom v1 |
| `--bodies <count>` | n-body bodies (implies nbody, default 65536) |
| `--iter <count>` | fixed iteration/step request shared by `stress`, `gpu_stress`, `gpu_burn` and `synthpeak`; GPU Burn accepts 16–2048 with no auto-tuning (software renderers retain a safety cap) |
| `--precision <fp32\|fp16\|fp64\|int32>` | synthpeak data type |
| `--steps <count>` | `volumetric` per-pixel ray samples (default 96; minimum 1) |
| `--grid <count>` | legacy `fluid` square-grid side; rounded up to a multiple of 16 (minimum 16) |
| `--jacobi <count>` | legacy `fluid` pressure iterations (default 30) |
| `--liquid-solver <mpm\|sph>` | solver for `cinematic_liquid`; default `mpm`; `sph` is the independent preview-only slice |
| `--time <sec>` | time-mode auto-stop (default 15) |
| `--no-time-limit` | run until window closed |
| `--benchmark [frames]` | **frame mode** (default 2000), then exit |
| `--run-all` | iterate every GPU×API, then exit |
| `--capture [sec]` | RenderDoc capture at T seconds (default 5) |
| `--full-analysis` | same as menu [5] |
| `--results` / `--results-delete <id>` / `--results-clear` / `--results-export <csv>` | result management |
| `--compare [id1 id2]` / `--list-gpus` / `--help` | compare / list GPUs / help |
| `--cpu-benchmark [per-core\|multi\|all]` | run the native CPU-only path and exit before GLFW/GPU probing; default mode is `all` |
| `--cpu-mode <per-core\|multi\|all>` | compatibility alias for the preferred compact `--cpu-benchmark <mode>` form; it also selects the CPU-only path |
| `--cpu-time <seconds>` | total measurement budget per CPU test, split across three rounds; default `1`, range `0.03..3600` |
| `--cpu-warmup <seconds>` | warm-up before each logical-processor test and before multi-core; default `0.15`, range `0..60` |
| `--cpu-no-save` | run the CPU test without appending successful summaries to `results.json` |

> `--run-all` (main.cpp:924): iterates every GPU×API, inherits command-line
> workload/precision/iter/headless/flights/hostMemory; default 15s time mode,
> frame mode with `--benchmark`, custom seconds with `--time`; prints the
> comparison table afterward; does **not** run Python.
>
> Explicit `--time` also counts as a "direct run" (no menu) — this is what lets
> the GUI's isolated benchmark workers use time-mode presets (main.cpp:788).

`cinematic_liquid_v1` is a public selector for the preserved original liquid
implementation even though persisted rows retain the historical workload id
`cinematic_liquid` and are separated by `workloadVersion=cinematic_liquid_v1`.
Likewise, `gpu_burn` selects the perspective 3D Mangekyo faceted-glass v2 scene
(cut gems, layered diamond shards, Fresnel reflection and RGB dispersion), while
`gpu_burn_v1` selects the preserved Plasma Bloom implementation. Both persist
under the `gpu_burn` family id and are separated by their versioned result
contract. Current runs use `gpu_burn_v3_fixed_steps_<N>_kaleidoscope`:
the GUI offers Light (16), Medium (64), Heavy (256), and Custom (16–2048),
and always sends a fixed `--iter` value without per-device auto-tuning.
For `--liquid-solver sph`, every duration—including 15 seconds—is currently
saved as `cinematic_liquid_sph_slice_v1_preview`; changing only the duration
does not make the four open correctness contracts formal.

### Native CPU-only path

```powershell
# Quick preview; writes isolated preview summaries
gpu_benchmark.exe --cpu-benchmark all --cpu-time 1 --cpu-warmup 0.15

# Formal contract: 15.0 s total measurement per test, 0.2 s warm-up, 3 rounds
gpu_benchmark.exe --cpu-benchmark all --cpu-time 15 --cpu-warmup 0.2

# Diagnostic smoke without result persistence
gpu_benchmark.exe --cpu-benchmark per-core --cpu-time 0.09 --cpu-warmup 0 --cpu-no-save
```

`per-core` means a sequential test of every available **logical processor**, not
one representative thread per physical core. `multi` starts one worker for every
available logical processor; `all` runs per-core first and multi second. Each
test splits its measurement budget into three rounds and selects the median.
The per-core summary is the arithmetic mean of those logical-processor medians.

The machine protocol consists of tab-separated `CPU_META`, `CPU_TOPOLOGY`,
`CPU_PROGRESS`, `CPU_RESULT` and `CPU_ERROR` records. It includes affinity,
formal/preview status, validity, round/median metadata and topology/classification
sources. The checksum is a dead-code-elimination sink, not a correctness oracle.
Successful persistence stores per-core average and multi-core summary rows;
detailed logical-processor rows remain in stdout/the current GUI session.

Windows formal results require strict group affinity. Linux/Android require
pthread affinity to succeed and read back as the requested sole CPU under the
separate `strict_sched_affinity` identity. A failed required bind marks the
affected result invalid, returns exit code 3 and prevents persistence. macOS
reports `scheduler_managed`; affinity capability, timing and `multi` sequence are
part of the stored result identity, so unlike contracts do not share a comparison
group. Core-class names prefixed with `Inferred` are OS-metadata ranks, not
authoritative P/E/Mid/LPE microarchitecture identification.

## 4. External tools summary

| Tool | When | What |
|---|---|---|
| RenderDoc In-App API | during a run when `captureAtSec > 0` (menu 5/6/7/8) | captures `.rdc` via `renderdoc_app.h`; timed capture is clamped to `duration - 1s`, and disabled when duration is at most 1s |
| `renderdoccmd.exe convert` | after 5/6/7/8 if captures exist | `.rdc` → chrome JSON |
| `python scripts/rdoc_analyse.py` | 5/6 only | `docs/rdoc_comparison.md` |
| `python scripts/plot_results.py` | 5/6 only | `docs/images/*.png` |
| `python scripts/export_report.py` | 5/6 only | `docs/results-table.md` + `docs/report.html` |

`--renderdoc` enables RenderDoc injection and manual F12 capture independently
of the automatic timer. `--no-renderdoc` is a true master-off switch: the worker
does not initialize the RenderDoc DLL/API and ignores automatic/manual capture.

## 5. GUI parity

The WinUI 3 GUI presets mirror these CLI flows (`gui/MainWindow.xaml.cpp`):

The Workload dropdown exposes the same **12 selections**: `stream`, `gpu_burn`,
`gpu_burn_v1`, `cinematic_liquid`, `gpu_stress`, `nbody`, `synthpeak`, `stress`, `render3d`,
`volumetric`, `fluid`, and `cinematic_liquid_v1` (display order differs from the
CLI list but the ids are identical).

| GUI preset | CLI equivalent | Notes |
|---|---|---|
| Quick run | [0] | auto API/GPU, Stream, Medium |
| Custom run | [1] | honours every visible control, including optional Headless |
| Full analysis — one GPU | [5] | selected workload/APIs for one GPU; optional Headless, RenderDoc, and full toolchain |
| Full analysis — all GPUs | [6] | selected workload/APIs for all GPUs; optional Headless, RenderDoc, and full toolchain |
| Flights test | [7] | `--flights N --capture 5`, RenderDoc convert |
| Particle test | [8] | `--particles N --capture 5`, RenderDoc convert |
| Headless compute | [9] | `--headless`, no capture |

Parity details:
- Custom and Full Analysis honour the selected workload. The specialised
  Flights, Particle, and Headless Compute presets retain their Stream-only
  behaviour.
- Headless is available in Custom and Full Analysis when the selected workload
  supports compute-only execution. Enabling it turns off RenderDoc/Capture;
  Full Analysis still runs its selected GPU/API matrix and report/chart steps.
- APIs are filtered to those the selected GPU supports (from `--list-gpus`);
  OpenGL only runs on the first GPU on Windows.
- After a full-analysis run the GUI runs the same toolchain as the CLI
  (`renderdoccmd convert` → `rdoc_analyse.py` → `plot_results.py` →
  `export_report.py --md/--html`), **plus** `plot_workloads.py` for the GUI's
  Charts tab. RenderDoc steps are no-ops unless the GUI is launched under
  RenderDoc (so `.rdc` captures exist).
- Software/WARP runs by selecting the `… (CPU / WARP)` entry in the GPU dropdown
  (there is no separate WARP checkbox).
- Particle count is a free-form number field (a superset of the CLI's
  Light/Medium/Heavy/Extreme presets).
- Duration defaults to **15 seconds** (time mode); switch the unit to Frames for
  a fixed frame-count run.
