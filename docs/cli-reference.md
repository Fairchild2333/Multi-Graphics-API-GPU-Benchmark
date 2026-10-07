# Mangekyo CLI Reference — `gpu_benchmark`

Complete record of every Mangekyo CLI test: parameters, what it runs, and which
external tools it invokes. The product name changed, but the compatible internal
command remains `gpu_benchmark`. Source: [`src/main.cpp`](../src/main.cpp), defaults in
[`src/gpu_common.h`](../src/gpu_common.h).

Windows build: prefer [`scripts/build-windows.ps1`](../scripts/build-windows.ps1)
(CLI + WinUI). Details: [`docs/building.md`](building.md).

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
runs append summary rows unless `--cpu-no-save` is supplied. New records also
store the operating-system family, native OS architecture, and process
architecture. This distinguishes, for example, an x64 process running through
Windows-on-Arm emulation from a native ARM64 process. Older records remain
loadable and are marked as legacy/unknown where those fields did not exist.

Since 2026-10-06 every saved result also names the machine it was measured on:

- `deviceModel`: the make and model of a phone, tablet, Mac or brand-name PC.
  A self-built PC, whose firmware holds only a placeholder, is named by its
  mainboard. Empty where the system gives no name, as inside a container.
- `systemMemoryMB`: the memory of the machine in MiB. Windows reports the
  installed amount; the other systems report what the kernel can use, which is
  a few percent less.
- On Android and HarmonyOS, `osVersion` is the system release with its API
  level instead of the kernel version, and on Android `cpuName` is the
  system-on-chip name from the system properties.

Results saved before that date lack the two new fields and still load.

### Compatibility of `results.json`

Builds of different ages share one results file, so the format follows these
rules (reader and writer are in `src/benchmark_results.cpp`):

- The file is a JSON array of result objects with one member per line. A
  member is never renamed, removed or given another type or meaning; new
  information goes into a new member.
- **Older files.** A missing member takes a default. Every file written since
  the first build loads.
- **Newer files.** Members a build does not know are kept and written back
  when it rewrites the file, and `resultSchemaVersion` is written back as
  read. Builds made before 2026-10-06 (0.2.6 and earlier) ignore unknown
  members when reading but drop them when they rewrite the file; avoid
  deleting results with such a build if a newer one is also in use.
- **Saving.** The new file is written beside the old one and swapped in, so a
  reader never sees a half-written file and a crash leaves the old file whole.
- **Damaged files.** A file that cannot be read in full (cut off, damaged, or
  in a layout the build does not read) gives the results that can be read.
  Before such a file is rewritten it is copied to
  `results.json.bak-unreadable-<time>` in the same folder.
- **Text.** Braces, a backslash at the end of a string and control characters
  are written as `\u` escapes. This is valid JSON; builds up to 0.2.6 show
  the escape as written, for example `\u007b`.

`results_compat_test` checks all of this without a GPU:

```bash
ctest --test-dir build -C Release
```

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
| `--gpu <index>` / `--warp` | select GPU / WARP software renderer (DX only); a DX12 linked adapter is listed once per physical node, so selecting its `#2` row binds an ordinary DX12 run to node 1 rather than repeating node 0 |
| `--vsync` / `--host-memory` | enable vsync / keep particle buffer in host RAM |
| `--flights <N>` | frames-in-flight (1–16) |
| `--multi-gpu <off\|afr\|sfr>` / `--afr` / `--sfr` | experimental two-GPU Plasma modes; AFR requires at least 4 flights and supports DX12/Vulkan/DX11 with the documented backend limits; SFR is DX12-only and splits one frame 50/50 before a Tier-1 cross-node copy/composition; both disable RenderDoc and use separate score contracts |
| `--headless` | pure compute, no window |
| `--particles <count>` | particle count (rounded to 256, skips difficulty menu) |
| `--workload <id>` | recommended product ids: `stream`, `gpu_burn`, `cinematic_liquid`; maintained advanced/compatibility ids: `nbody`, `gpu_burn_v1`, `gpu_stress`, `stress`, `synthpeak`, `render3d`, `volumetric`; `gpu_burn` is Plasma×Kaleidoscope and `gpu_burn_v1` preserves Plasma Bloom v1 |
| `--bodies <count>` | n-body bodies (implies nbody, default 65536) |
| `--iter <count>` | fixed iteration/step request shared by `stress`, `gpu_stress`, `gpu_burn` and `synthpeak`; GPU Burn accepts 16–2048 with no auto-tuning (software renderers retain a safety cap) |
| `--precision <fp32\|fp16\|fp64\|int32>` | synthpeak data type |
| `--steps <count>` | `volumetric` per-pixel ray samples (default 96; minimum 1) |
| `--liquid-solver <mpm\|sph>` | solver for `cinematic_liquid`; default `mpm`; `sph` is the independent preview-only slice |
| `--time <sec>` | time-mode auto-stop (default 15) |
| `--no-time-limit` | run until window closed |
| `--benchmark [frames]` | **frame mode** (default 2000), then exit |
| `--run-all` | iterate every GPU×API, then exit |
| `--complete-suite` | run the formal 15-second matrix on every GPU and compiled API: Particle 65K/1M/4M/16M both windowed and headless, GPU Burn 64 steps, and Cinematic Liquid; windowed jobs request a 5-second capture and unsupported combinations are attempted and recorded |
| `--fill-missing` | inspect both `results.json` and the real capture directory, then offer to run only missing scores/captures; known unsupported and previously failed combinations are reported but not retried |
| `--yes` | confirm `--fill-missing` non-interactively (used by the GUI after its confirmation dialog) |
| `--capture [sec]` | RenderDoc capture at T seconds (default 5) |
| `--full-analysis` | same as menu [5] |
| `--results` / `--results-delete <id>` / `--results-clear` / `--results-export <csv>` | result management |
| `--compare [id1 id2]` / `--list-gpus` / `--help` | compare / list GPUs / help; comparison groups keep windowed and headless execution separate |
| `--cpu-benchmark [per-core\|multi\|all]` | run the native CPU-only path and exit before GLFW/GPU probing; default mode is `all` |
| `--cpu-mode <per-core\|multi\|all>` | compatibility alias for the preferred compact `--cpu-benchmark <mode>` form; it also selects the CPU-only path |
| `--cpu-time <seconds>` | total measurement budget per CPU test, split across three rounds; default `1`, range `0.03..3600` |
| `--cpu-warmup <seconds>` | warm-up before each logical-processor test and before multi-core; default `0.15`, range `0..60` |
| `--cpu-no-save` | run the CPU test without appending successful summaries to `results.json` |
| `--cpu-membw` | run the CPU cache / memory bandwidth sweep and exit before GLFW/GPU probing (desktop builds only) |
| `--cpu-membw-max-mib <MiB>` | largest single-thread data set of that sweep; default `1024`, range `1..65536`; a non-default value makes the result a preview |
| `--cpu-membw-vector <auto\|128\|256\|512>` | vector width of its kernels; default `auto` picks the widest the CPU supports; a forced width makes the result a preview |
| `--cpu-membw-no-save` | run that sweep without appending its result to `results.json` |
| `--cpu-membw-curve-levels` | read the cache levels off the curve even where the OS reports cache sizes; makes the result a preview |
| `--cpu-membw-no-storage` | skip the storage test, so that no file is written |
| `--cpu-membw-storage-dir <path>` | directory for the storage test file; default is the temp directory on Windows and the results directory elsewhere |
| `--cpu-membw-storage-mib <MiB>` | size of the storage test file; default `1024`, range `32..65536` |
| `--cpu-membw-reread <results.json>` | reads the cache levels of every saved `cpu_membw` result in the file again, off the curves the result keeps, with the rules of this build; measures nothing and does not change the file |
| `--cpu-membw-debug-classes <N>` / `--cpu-membw-debug-hide-llc` | diagnostics: treat the cores as `N` classes, and ignore the last cache the OS reports; both make the result a preview |
| `--workload membw` | run the standalone memory-bandwidth sweep (Vulkan only, compute-only, no window); `--gpu` selects the device and `--backend` must be `auto` or `vulkan` |
| `--membw-max-mib <MiB>` | largest data set of the `membw` sweep; default `2048`, range `1..65536`; a non-default value makes the result a preview |
| `--membw-no-save` | run `membw` without appending its result to `results.json` |

> `--run-all` (main.cpp:924): iterates every GPU×API, inherits command-line
> workload/precision/iter/headless/flights/hostMemory; default 15s time mode,
> frame mode with `--benchmark`, custom seconds with `--time`; prints the
> comparison table afterward; does **not** run Python.
>
> Explicit `--time` also counts as a "direct run" (no menu) — this is what lets
> the GUI's isolated benchmark workers use time-mode presets (main.cpp:788).

`gpu_burn` selects the current Plasma×Kaleidoscope scene
(cut gems, layered diamond shards, Fresnel reflection and RGB dispersion), while
`gpu_burn_v1` selects the preserved Plasma Bloom implementation. Both persist
under the `gpu_burn` family id and are separated by their versioned result
contract. Current runs use `gpu_burn_v3_fixed_steps_<N>_kaleidoscope`:
the GUI offers Light (16), Medium (64), Heavy (256), and Custom (16–2048),
and always sends a fixed `--iter` value without per-device auto-tuning. Software
renderers clamp this workload to 64 steps, so the formal 64-step score remains
directly comparable instead of silently dropping to 32.
For `--liquid-solver sph`, every duration—including 15 seconds—is currently
saved as `cinematic_liquid_sph_slice_v1_preview`; changing only the duration
does not make the four open correctness contracts formal.

### Memory-bandwidth sweep (`membw_v1`)

```powershell
# Full sweep on GPU 0: data sets from 1 MiB to 2048 MiB, the L1 sweep and the
# system-memory pass; about 23 s on an RTX 5090 with a PCIe 5.0 x4 link
.\out\build\windows-x64-release\Release\gpu_benchmark.exe --workload membw --gpu 0
```

`membw` measures real VRAM and cache bandwidth, which the particle test
(`stream`) cannot do. It is a separate Vulkan code path in
`src/vulkan_membw.cpp`: it creates its own device, and it opens no window,
swapchain or RenderDoc capture.

The WinUI GPU page lists it as "Memory Bandwidth — VRAM / Cache / PCIe" for
Custom and Full Analysis runs. Selecting it limits the API list to Vulkan and
disables Duration, Headless, V-Sync, System memory, RenderDoc and Capture,
which do not apply. The GUI starts one worker per GPU with `--workload membw
--backend vulkan --no-renderdoc [--gpu N]` and builds its four summary lines
from the `MEMBW_DEVICE` / `MEMBW_SUMMARY` / `MEMBW_L1` / `MEMBW_HOST` lines.

Below the summary, the GUI draws the `MEMBW_POINT` data as up to three
log-log line charts: VRAM (or system memory) and cache, system memory over
PCIe, and L1. They are separate charts because the sweeps differ by orders of
magnitude and the L1 sweep has a different x quantity. Hovering a line shows
the exact value of that point; the full table stays in the raw CLI output
panel. When several GPUs run in one job list, the charts show the last one.

- **Kernels:** `read` (16 B loads), `write` (16 B stores) and `copy` (16 B load
  + 16 B store into a second buffer). Byte counts are the bytes the kernel
  requests, not an estimate.
- **Data sets:** every workgroup streams one 512 KiB chunk. The number of
  distinct chunks sets the data-set size, which is swept from 1 MiB to
  `--membw-max-mib` in ×1.5 / ×2 steps. The number of dispatched workgroups is
  calibrated separately so that each dispatch lasts about 4 ms at any size.
- **Data check:** before anything is timed, the buffer is filled by the write
  kernel and a fifth kernel (`membw_check.comp`, never timed) loads every
  element and compares it with the value that was stored. If any differs, the
  buffer is replaced by one of half the size and checked again, and the sweep
  ends at that size (`ceiling lowered by the device`). The outcome is printed
  as a `[membw] Data check:` line. Reason: a driver may advertise a
  storage-buffer range it cannot address. An Adreno 660 (driver 512.530)
  reports 512 MiB, loads zeros from a buffer of that size without touching
  memory and reported 116 GB/s for it; at 256 MiB it is correct (32.7 GB/s).
  The system-memory buffer of the PCIe pass is checked the same way.
- **Timing:** each dispatch is submitted on its own and bracketed by GPU
  timestamps. Each point runs 3 warm-up and 10 measured dispatches; the median
  is the score and the fastest run is also printed. If the timestamps do not
  scale with the workload, the run falls back to wall-clock timing and says so.
- **Summary:** the memory plateau is the median of the three largest data
  sets. The cache plateau is the longest flat stretch of the curve that is
  clearly above it.
- **L1 sweep:** a fourth kernel makes every workgroup re-read a private block,
  whose size is swept from 16 KiB to 1 MiB. It runs twice:
  - `l1`: 4096 workgroups, so every SM/CU is busy. The plateau around the
    fastest block size is the L1 read rate of the whole GPU.
  - `l1_single`: one workgroup, which runs on one SM/CU. The block size at
    which this curve drops brackets the L1 capacity of one SM/CU.

  The L1 kernel declares a shared variable and calls `barrier()` once. Shared
  memory is local to one SM/CU, so this keeps the whole workgroup there;
  without it the NVIDIA driver spreads the warps of a single workgroup over
  several SMs and the single-group curve changes from run to run.
- **System-memory pass (discrete GPUs only):** the `read` and `write` kernels
  run again on a buffer of up to 2 GiB that lives in host memory
  (`host_read`, `host_write`), then `vkCmdCopyBuffer` copies that buffer to
  VRAM and back. Every byte crosses the PCIe link, so the largest data sets
  give the usable link rate in each direction:
  - `host_read` and the copy to VRAM move data from system memory to the GPU;
  - `host_write` and the copy to system memory move data the other way.

  The pass is skipped on integrated and software devices, where all memory is
  system memory. A sweep stops early once one dispatch takes more than 0.5 s.
- **Output:** a table per kernel, a summary, and tab-separated `MEMBW_POINT` /
  `MEMBW_SUMMARY` / `MEMBW_L1` / `MEMBW_HOST` lines for tools.
- **Result:** workload `membw`, score = read memory plateau in GB/s. The
  version is `membw_v1` for the default sweep when the memory plateau is
  reached, otherwise `membw_v1_preview`. `workloadConfig` stores the write and
  copy plateaus, the L1 figures, the system-memory figures and the full curves.

Compare the system-memory figures with the PCIe link, which the tool cannot
read through Vulkan. On Windows, `nvidia-smi
--query-gpu=pcie.link.gen.current,pcie.link.width.current --format=csv` reports
it for NVIDIA cards. One lane carries 0.985 GB/s (gen 3), 1.969 GB/s (gen 4) or
3.938 GB/s (gen 5) per direction before protocol overhead, so a gen 5 x16 link
is 63.0 GB/s, x8 is 31.5 GB/s and x4 is 15.75 GB/s.

Five limits apply to the numbers:

- **Small system-memory data sets:** `host_read` is faster than the link below
  about 32 MiB on an RTX 5090, because the GPU caches data it has already
  fetched. Only the plateau at the largest data sets is the link rate.

- **L1 rate:** the L1 figure is limited by the instruction rate as well as by
  the cache, because each 16 B load is followed by four float adds. It is the
  rate this kernel reaches, not a hardware maximum.
- **L1 levels:** the summary reports only the first drop of the single-group
  curve. A GPU with several small cache levels shows further steps in the
  table; an AMD RDNA2 iGPU shows three.

- **Smallest data sets:** below a few MiB, several workgroups on one SM/CU read
  the same chunk and share it through L1, so those points are faster than L2.
  The summary flags this.
- **Cache capacity:** the end of the cache plateau is not an exact capacity. A
  large GPU runs more workgroups at once than a mid-sized data set has chunks,
  so concurrent groups share chunks through the cache and stretch the
  transition to memory.

The run does not compare the result with a theoretical bandwidth, because
Vulkan does not expose memory clock or bus width.

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
available logical processor; `all` runs all-core first and the long single-core
sweep second, preventing the all-core score from being depressed by heat and
power state left by the sweep. Each
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

### CPU cache and memory bandwidth sweep (`cpu_membw_v1`)

```powershell
# Full sweep: 4 KiB to 1 GiB on one thread, then 4 KiB to 256 MiB per thread on
# every physical core; about 25 s on a Ryzen 7 9800X3D
gpu_benchmark.exe --cpu-membw

# The same with 256-bit vectors, for comparison with a CPU that lacks AVX-512
gpu_benchmark.exe --cpu-membw --cpu-membw-vector 256 --cpu-membw-no-save
```

`--cpu-membw` measures how fast the CPU reads, writes and copies data at each
level of its memory hierarchy, and how fast the drive behind virtual memory
is. It is a separate code path in `src/cpu_membw.cpp` that needs no GPU and
is built on every platform.

- **Kernels:** `read` (vector loads summed into four accumulators), `write`
  (vector stores) and `copy` (load from the first half of the block, store to
  the second half). Copy counts the bytes read plus the bytes written. The
  kernels use the widest vectors the CPU supports: AVX-512, AVX2 or SSE2 on
  x86-64, NEON on ARM64, and 64-bit words elsewhere.
- **Data sets:** the block of one thread is swept from 4 KiB upwards in ×1.5 /
  ×2 steps. Each thread allocates and fills its own block.
  - `single`: one thread, up to `--cpu-membw-max-mib` (1 GiB).
  - `all`: one thread on every physical core, each with a private block of up
    to a quarter of that (256 MiB), and at most 8 GiB in total.
  - Both ceilings are lowered so that the sweep never touches more than half
    of the free memory.
- **Timing:** every point runs 2 warm-up and 7 measured samples of 10 ms. All
  threads start together and each times its own interval; the rates are
  summed. The median is the score. A round whose median is more than 15% below
  its best sample was disturbed, and is measured again up to twice.
- **Threads:** every thread is pinned to its core, except on macOS and iOS,
  which have no such control. A single-thread sweep runs on the second core
  of its core class.
- **Core classes:** cores are grouped by the performance class the OS
  reports (Windows efficiency class; `cpu_capacity` or maximum frequency on
  Linux and Android). Every class gets its own single-thread sweep, named
  `single`, `single_1`, `single_2` from fastest to slowest, with the caches
  of its own reference core. On such a CPU the all-core sweep reports memory
  only, because the classes leave their caches at different data sets.
- **Levels:** cache sizes come from the OS (`GetLogicalProcessorInformationEx`
  on Windows, sysfs on Linux, `sysctl` on macOS). A cache level is the median
  of the data sets between twice the next smaller cache and half of this one
  (for L1, between a quarter and half of it).
  Memory is the median of the data sets from four times the last cache
  upwards. In the all-core sweep a shared cache counts as its size divided by
  the number of threads sharing it. A level with no data set in its range is
  reported as not separable.
- **Levels without cache sizes:** where the OS reports none, the levels are
  the plateaus of the read curve. A plateau is at least three consecutive
  data sets within 15% of each other, and a new level is at least 15% slower
  than the one before it; the flat end of the curve is memory. Such a level
  is marked `*` and shows the two data sets its capacity lies between. The
  same search runs above the last reported cache when the memory range is not
  flat, which finds a cache the OS did not report, such as a system-level
  cache of an SoC.
- **Other ways to write memory:** for the memory-sized data sets on x86-64 the
  sweep also measures streaming (non-temporal) stores, and on AMD CPUs a zero
  fill with `CLZERO`.
- **Virtual memory:** one thread writes one byte to every page of a fresh
  256 MiB region, nine times; the median gives the rate at which the OS maps
  new pages, in GB/s and in nanoseconds per page.
- **Storage behind virtual memory:** a temporary file of up to 1 GiB is
  written once, then measured one request at a time with unbuffered I/O:
  - sequential 1 MiB read and write, fastest whole pass over the file;
  - random 4 KiB read and write for 2 s each, in MB/s and IOPS.

  These correspond to the `SEQ1M Q1T1` and `RND4K Q1T1` rows of
  CrystalDiskMark. Paging issues random 4 KiB requests, so the random line is
  the closer estimate of how fast memory comes back from the drive. On
  Windows the file goes to the temp directory and the output says whether
  that is the drive holding the page file. On Linux and Android the output
  names the swap target; `zram` means memory is compressed in RAM and no
  drive is involved. One run writes up to about 4.5 GiB.
- **Output:** a table per thread count, a level summary, and tab-separated
  `CPUMEM_META` / `CPUMEM_CLASS` / `CPUMEM_CACHE` / `CPUMEM_POINT` /
  `CPUMEM_PROGRESS` / `CPUMEM_LEVEL` / `CPUMEM_PAGEFAULT` / `CPUMEM_STORAGE` /
  `CPUMEM_DONE` lines for tools.
- **Result:** workload `cpu_membw`, score = all-core memory read in GB/s (the
  single-thread figure on a one-core machine). The version is `cpu_membw_v1`
  for the default sweep with a settled memory plateau, otherwise
  `cpu_membw_v1_preview`. `workloadConfig` stores every level, the vector
  width, the cache sizes and the full curves.

The WinUI CPU page lists it as "Cache & memory bandwidth" in the test-mode
box. Selecting it disables the duration controls, which do not apply. The
result card shows one row per level with the single-thread and all-core
figures side by side, and a card below it draws both sweeps as log-log charts
with a dashed line at each cache size. The CPU tab of History lists the runs
as "CPU — Cache & memory bandwidth".

On a phone, run the stand-alone build of the same sweep over adb. It saves
nothing and takes `--max-mib`, `--curve-levels`, `--no-storage`,
`--storage-dir` and `--storage-mib`:

```powershell
.\scripts\build-android-cpumem.ps1                 # arm64-v8a by default
adb push out\android-cpumem\arm64-v8a\mangekyo_cpumem /data/local/tmp/
adb shell chmod 755 /data/local/tmp/mangekyo_cpumem
adb shell "cd /data/local/tmp && ./mangekyo_cpumem"
```

The phone default ends the sweep at 256 MiB. `/data/local/tmp` is on the
internal storage, so the storage test measures that storage.

Compare the memory figures with the theoretical bandwidth by hand, because
the OS does not report the number of memory channels reliably: channels ×
transfer rate × 8 bytes, so dual-channel DDR5-6400 is 102.4 GB/s.

Twelve limits apply to the numbers:

- **Cache levels depend on the vector width:** inside a cache the rate is set
  by how many loads or stores the core issues per cycle. A Ryzen 7 9800X3D
  issues about 1.8 vector loads per cycle at any width, so its L1 read rate is
  588 GB/s with AVX-512, 297 GB/s with AVX2 and 147 GB/s with SSE2. Compare
  cache levels between CPUs only at the same vector width.
- **Ordinary stores:** outside the caches every stored line is first read from
  memory. The memory traffic is therefore 2× the write figure and 1.5× the
  copy figure.
- **Streaming stores:** they can be limited by the CPU and not by memory. On
  the 9800X3D they reach 30 GB/s on one thread and on eight, which is also
  what the C runtime's `memset` and `rep stosb` reach, while `CLZERO` reaches
  75 GB/s into the same memory. The cause was not investigated.
- **Cache levels are not always flat:** the 9800X3D reads 230–245 GB/s for
  64–384 KiB blocks and 300–320 GB/s for 512–768 KiB blocks, all inside L2.
  The summary reports the median and notes the range.
- **Mixed core types:** the levels follow the caches of the reference core.
  In the all-core sweep the smaller cores leave their caches at other sizes.
- **Shared caches:** the all-core figure of a shared cache also depends on how
  much of it the threads use, so it falls as the data set grows.
- **Page mapping:** the figure depends on the state of the OS and ranged from
  9 to 16 GB/s between runs on Windows 11. With Linux transparent huge pages
  set to `always`, the kernel maps 2 MiB at a time; the output says so.
- **Paging to disk:** page-file or swap speed is not measured. It is the speed
  of the drive, and reaching it needs the machine to run out of memory.
- **Idle machine:** other load lowers the figures. A build running at the same
  time halved the cache-level results in one run.
- **Levels read off the curve:** two caches whose speeds differ by less than
  15% merge into one level, a cache less than about three data sets wide is
  missed, and the capacity is a range, not a value. A sweep that ends before
  memory is reached labels its last plateau as memory.
- **Storage:** sequential write on an SSD with a fast write cache varies
  between runs (4.9-9.7 GB/s, once 1.5 GB/s, on a PCIe 5.0 drive). One
  request at a time is the slowest way to use a drive; real paging often has
  several requests outstanding. The figures were not compared with another
  disk benchmark.
- **Phones:** the mixed-core and curve-level paths were exercised on a desktop
  CPU with diagnostic switches, and the Android binary was compiled, but
  nothing has been run on a phone or on a CPU with mixed core types. The
  Android app does not offer the test yet, and iOS and macOS, which cannot pin
  threads, sweep one class only.

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

The product-facing GUI flows are centred on `stream`, `gpu_burn` and
`cinematic_liquid`. Advanced and compatibility selectors remain isolated from
the three primary workload contracts.

| GUI preset | CLI equivalent | Notes |
|---|---|---|
| Quick run | [0] | auto API/GPU, Stream, Medium |
| Custom run | [1] | honours every visible control, including optional Headless |
| Full analysis — one GPU | [5] | selected workload/APIs for one GPU; optional Headless, RenderDoc, and full toolchain |
| Full analysis — all GPUs | [6] | selected workload/APIs for all GPUs; optional Headless, RenderDoc, and full toolchain |
| Flights test | [7] | `--flights N --capture 5`, RenderDoc convert |
| Particle test | [8] | `--particles N --capture 5`, RenderDoc convert |
| Headless compute | [9] | `--headless`, no capture |
| Complete test | `--complete-suite` | one-click formal matrix across all GPUs/APIs, including headless Particle tests and windowed captures |
| Fill missing | `--fill-missing --yes` | GUI confirms first; the worker rechecks legacy/current results and the capture directory before launching only genuinely missing work |

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
  `export_report.py --md/--html`), **plus** `plot_workloads.py`. RenderDoc
  steps are no-ops unless the GUI is launched under RenderDoc (so `.rdc`
  captures exist).
- Every test has charts in the GUI, drawn by the GUI itself without Python:
  - **GPU page, after a run:** the score of every job side by side when the
    run had several (one bar per GPU and API), and for every job the GPU time
    per frame over the run, as compute and render lines. The warm-up is left
    out. The memory-bandwidth sweep shows its bandwidth curves instead, and
    the Complete test and Fill missing presets show no run charts.
  - **CPU page, after a run:** one bar per logical processor for the compute
    test, and the bandwidth curves for the cache and memory sweep.
  - **Charts tab:** one bar chart per test that has saved results, with the
    best score of every device and API under the newest version of that
    test. It reads the same results file as History.

  The time axis of the per-frame charts comes from the `T:` field that the
  periodic `[GPU Timing]` line of the worker now ends with, for example
  `| FPS: 2729 | T: 6.0 s`; lines printed during the warm-up end in
  `(warm-up)`.
- Software/WARP runs by selecting the `… (CPU / WARP)` entry in the GPU dropdown
  (there is no separate WARP checkbox).
- Particle count is a free-form number field (a superset of the CLI's
  Light/Medium/Heavy/Extreme presets).
- Duration defaults to **15 seconds** (time mode); switch the unit to Frames for
  a fixed frame-count run.
- Fill-missing uses `complete-suite-status.tsv` beside the result database to
  remember unsupported and failed combinations. A score in `results.json` does
  not satisfy a required capture by itself: the matching `.rdc`/`.gputrace`
  must still exist in the capture directory. Successful retries append through
  the normal result writer, which rewrites a valid current `results.json` while
  preserving compatible legacy records.
