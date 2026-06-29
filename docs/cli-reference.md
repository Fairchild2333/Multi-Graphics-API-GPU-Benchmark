# CLI Reference — `gpu_benchmark`

Complete record of every CLI test: parameters, what it runs, and which external
tools it invokes. Source: [`src/main.cpp`](../src/main.cpp), defaults in
[`src/gpu_common.h`](../src/gpu_common.h).

## 1. Default config (`BenchmarkConfig`, gpu_common.h:103-120)

| Field | Default | Notes |
|---|---|---|
| `workload` | `Stream` (bandwidth) | one of stream/nbody/stress/synthpeak/render3d |
| `particleCount` | `1048576` (= Medium) | 1M particles |
| `maxRunTimeSec` | `15.0` | time-mode duration |
| `warmupTimeSec` | `2.0` | time-mode warmup |
| `benchFrames` | `2000` | frame-mode count (only with `--benchmark`) |
| `warmupFrames` | `100` | frame-mode warmup |
| `framesInFlight` | `2` (max 16) | flights |
| `vsync` | `false` | |
| `headless` | `false` | |
| `hostMemory` | `false` | |
| `captureAtSec` | `-1.0` | <0 = no RenderDoc capture |
| `fractalIter` / `peakIters` | `2000` / `16384` | stress / synthpeak |
| n-body bodies | `65536` | `kNBodyDefaultBodies` |

### Two run modes (app_base.cpp:391-421)

- **Time mode** (default, `benchmarkMode=false`): runs `maxRunTimeSec` seconds
  (default 15s), 2s warmup. **All interactive-menu runs use this.**
- **Frame mode** (`benchmarkMode=true`, only via `--benchmark [frames]`): runs
  `benchFrames` (default 2000) frames, ignores the time limit.

Every run appends its result to `results.json`.

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
| `--workload <stream\|nbody\|stress\|synthpeak\|render3d>` | select workload |
| `--bodies <count>` | n-body bodies (implies nbody, default 65536) |
| `--iter <count>` | stress per-pixel iters / synthpeak loop passes |
| `--precision <fp32\|fp16\|fp64\|int32>` | synthpeak data type |
| `--time <sec>` | time-mode auto-stop (default 15) |
| `--no-time-limit` | run until window closed |
| `--benchmark [frames]` | **frame mode** (default 2000), then exit |
| `--run-all` | iterate every GPU×API, then exit |
| `--capture [sec]` | RenderDoc capture at T seconds (default 5) |
| `--full-analysis` | same as menu [5] |
| `--results` / `--results-delete <id>` / `--results-clear` / `--results-export <csv>` | result management |
| `--compare [id1 id2]` / `--list-gpus` / `--help` | compare / list GPUs / help |

> `--run-all` (main.cpp:924): iterates every GPU×API, inherits command-line
> workload/precision/iter/headless/flights/hostMemory; default 15s time mode,
> frame mode with `--benchmark`, custom seconds with `--time`; prints the
> comparison table afterward; does **not** run Python.
>
> Explicit `--time` also counts as a "direct run" (no menu) — this is what lets
> the GUI's time-mode presets work in-process (main.cpp:788).

## 4. External tools summary

| Tool | When | What |
|---|---|---|
| RenderDoc In-App API | during a run when `captureAtSec ≥ 0` (menu 5/6/7/8) | captures `.rdc` via `renderdoc_app.h` |
| `renderdoccmd.exe convert` | after 5/6/7/8 if captures exist | `.rdc` → chrome JSON |
| `python scripts/rdoc_analyse.py` | 5/6 only | `docs/rdoc_comparison.md` |
| `python scripts/plot_results.py` | 5/6 only | `docs/images/*.png` |
| `python scripts/export_report.py` | 5/6 only | `docs/results-table.md` + `docs/report.html` |

## 5. GUI parity

The WinUI 3 GUI presets mirror these CLI flows (`gui/MainWindow.xaml.cpp`):

| GUI preset | CLI equivalent | Notes |
|---|---|---|
| Quick run | [0] | auto API/GPU, Stream, Medium |
| Custom run | [1] | honours every control (the only preset that uses the Workload/Precision/Advanced inputs) |
| Full analysis — one GPU | [5] | per-API for the selected GPU, `--capture 5`, then full toolchain |
| Full analysis — all GPUs | [6] | `--run-all --capture 5`, then full toolchain |
| Flights test | [7] | `--flights N --capture 5`, RenderDoc convert |
| Particle test | [8] | `--particles N --capture 5`, RenderDoc convert |
| Headless compute | [9] | `--headless`, no capture |

Parity details:
- Full-analysis/Flights/Particle/Headless presets always run the **Stream**
  workload (the Workload dropdown is ignored for them, matching the CLI; only
  Custom run honours it).
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
