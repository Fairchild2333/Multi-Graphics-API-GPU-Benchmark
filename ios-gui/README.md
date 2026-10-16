# Mangekyo for iOS / iPadOS

Native SwiftUI application targeting **iOS 16.0+**. Liquid Glass is enabled on
newer systems, with Material controls on older systems. GPU work runs through
an embedded Metal host, optional bundled MoltenVK and an EAGL ES 2.0 legacy
GPU Burn host; CPU work uses the same CPU-only execution and result
storage as the desktop CLI, without a Metal surface or desktop process spawning.

## Build

On a Mac with Xcode (Xcode 26+ for Liquid Glass), CMake 3.22+ and `glslc`:

```bash
./scripts/build-ios.sh
./scripts/build-ios.sh --sdk iphonesimulator
./scripts/build-ios.sh --sdk iphoneos
```

The tracked Xcode project builds without Homebrew or XcodeGen. After changing
`project.yml`, regenerate it explicitly with installed XcodeGen:

```bash
./scripts/build-ios.sh --regenerate-project
```

Outputs:

- Device: `build-ios/DerivedData-iphoneos/Build/Products/Release-iphoneos/Mangekyo.app`
- Simulator: `build-ios/DerivedData-iphonesimulator/Build/Products/Release-iphonesimulator/Mangekyo.app`

The build script fetches official MoltenVK 1.4.2 device/simulator archives with
a pinned SHA-256, compiles SPIR-V with `glslc`, and copies runtime sources and
the license into the app. Before a direct Xcode build, run the script for the
matching SDK/configuration. Device and simulator libraries stay separate.

The script produces unsigned developer builds. Installing on an iPhone/iPad
requires selecting a development team and signing in Xcode. The simulator
engine archive contains arm64 and x86_64; the app uses the host architecture.

The Xcode target copies runtime Metal **source text** into the bundle, so an
ordinary Xcode build also has its shaders. It does not need the separately
installed Metal offline compiler. At launch the bridge refreshes sandbox
shader copies from the bundle. Results live under the app's Documents directory.

## Features

- Particle rendering and compute-only particle runs, with selectable counts.
- GPU Burn with selectable steps; the Metal interactive pool preview.
- Fixed 1280 × 720 rendering in landscape; tap the surface to stop.
- Selectable duration and V-Sync, cancellation on leaving the active scene,
  and prevention of screen sleep during a test.
- CPU mixed compute, cache/memory bandwidth, storage and page-mapping tests.
- A **Test all items** button runs supported jobs sequentially: particle
  render/compute at four sizes, GPU Burn at three step counts, liquid, GPU memory,
  GPU cache diagnostics, CPU compute and CPU cache/memory/page-mapping/storage,
  plus legacy OpenGL ES 2.0 GPU Burn where available. CPU compute saves separate
  single-thread and all-core records.
  The suite keeps its Metal surface and screen awake across jobs, verifies newly
  saved records, and writes its manifest and raw log under Documents/suite-runs.
- **Run all GPU tests** runs only that device's supported GPU matrix;
  **Run all CPU tests** runs both CPU compute and the cache/memory,
  page-mapping/storage sweep. Formal CPU steps default to 15 seconds.
  Every suite waits for nominal thermal state before each job (up to 600 seconds,
  then reports a timeout and continues). Saved measurements with questionable
  timing or changing throughput are marked for review, with raw scores retained.
- **Allow translated APIs (MoltenVK)** is off by default. It unlocks Vulkan in
  the API selector and adds supported Vulkan jobs to the full suite. Vulkan
  records retain `_moltenvk`, `apiImplementation=MoltenVK`, `translatedTo=Metal`.
  Native entry points also reject translated requests without explicit opt-in.
- Capability checks exclude unsupported Vulkan liquid (R32 float linear
  filtering) and GPU cache controls. iOS has no ES 3.1 compute API; EAGL uses
  original ES 2.0 legacy shaders with synchronized timing and display-paced
  presentation. It retains its own score group. Software rasterizers are
  capped before ES 2.0 shader compilation so executed/reported steps agree.
- App Documents are visible in Files and Finder device file sharing, including
  GPU/CPU results.json and per-suite manifests/logs.
- Small archived result histories, complete curve exports and native text logs
  are versioned under `out/test-results`; see the
  [archive index](../out/test-results/README.md). Large diagnostic bundles and
  build products remain local. Archive files preserve their original bytes.
- Separate GPU/CPU logs, progress, scores and current-run charts.
- History preserving result schema, device model, memory and platform metadata.
- Auto language follows the device's preferred language list, independently of
  its region. English, Simplified Chinese and Japanese are declared in the
  bundle; a saved manual language is restored before the initial status text.
- Translated Vulkan particles use synchronized compute/render completion timing
  throughout the run. Old query-timed particle records are kept in history and
  do not fulfill the current suite's translated-particle jobs.
- A12X all-core verification checks measured CPU participation of both core
  groups in every round, while retaining per-worker core-type purity as detail.
  Single-core P/E scores still require 99% verified residency.
- Large iPhones keep tab navigation when rotating to landscape, so the
  full-screen benchmark is not dismissed by a size-class navigation change.
  A simulator UI regression covers starting, retaining and cancelling the run.
- Throughput variability is retained as a review flag with the full-run mean
  and curves. A greater-than-5% early/late drop is not direct frequency
  telemetry; comparisons needing stable performance may use a separate
  control run. Do not repeat tests merely to clear warnings.
- Filtered comparison charts by version, API, execution mode and particle size,
  plus CPU comparison charts.
- Saved cache/memory curves accessible from History and Charts. Choose an older
  CPU or Metal memory run, or open a JSON result file read-only without merging
  or changing the app's history. Vulkan curves imported from desktop results
  can also be viewed; this does not enable Vulkan execution on iOS. Only saved
  sweep points are plotted, with their original units and measurement labels.
- A **Memory Bandwidth — VRAM / Cache** entry in the GPU test list, using the
  native Metal cache/memory diagnostic with size curves and saved previews.
  It runs without a rendering surface, finishes automatically and saves to
  `results.json`; duration and V-Sync settings do not apply.

Apple does not offer strict thread-to-core pinning. Single-thread CPU runs are
scheduler-managed and are labelled accordingly. All mobile scores retain
`_ios_preview`; they are separate from desktop score groups. The particle-derived
Metal memory diagnostic uses `metal_memory_sweep_v1_preview_ios_preview`, which
is also separate from Vulkan `membw_v1` and is not a validated cache-capacity or
memory-plateau measurement. Its allocation ceiling is limited to the app's
available memory and at most 256 MiB.

## Native verification

Build the simulator app first, then run the hosted bridge regression tests on
an available simulator (replace the destination with one on your Mac):

```bash
xcodebuild test -project ios-gui/Mangekyo.xcodeproj -scheme Mangekyo \
  -configuration Release -destination 'platform=iOS Simulator,name=iPhone 16 Pro,OS=18.5' \
  -derivedDataPath build-ios/DerivedData-iphonesimulator \
  'LIBRARY_SEARCH_PATHS="$(PROJECT_DIR)/../build-ios/Release-iphonesimulator/Release-iphonesimulator"' \
  CODE_SIGNING_ALLOWED=NO ONLY_ACTIVE_ARCH=YES
```

The tests cover CPU routing without a Metal surface, CPU memory sweeps, malformed
options, saved metadata and compute-only GPU requests. The test bundle targets
iOS 18+ to match the current XCTest runtime; the product still targets iOS 16.

Verified on 2026-10-12 with Xcode 27: unsigned device and simulator builds, and
hosted bridge tests on the iOS 18.5 simulator. Physical iPhone/iPad performance,
iOS 16 runtime acceptance, thermal contracts and App Store distribution remain
unverified.

2026-10-14 physical follow-up: A15 CPU P/E/all-core tests passed. The iOS E
compute stage now uses user-initiated QoS with P-load helpers and runtime-resolved
Recount `thread_selfcounts` SPI to verify the measured thread's E CPU time.
Two runs measured 89.658/89.551 MWork/s at about 2.016GHz, >=99.999% E residency.
The default CPU suite uses this method, saves it separately from background E
scores, and rejects background E results during resume. The SPI is not a public
SDK API: this documents development-signed device validation, not App Store
distribution acceptance. The memory sweep retains its own background E contract.
See [core-type methodology](../docs/apple-cpu-core-types.md).

The saved-curve reader was verified on 2026-10-13 with 139 shared parser/geometry
checks and unsigned device/simulator builds. Actual CPU, Vulkan and Metal saved
curves were inspected in the macOS GUI; the iOS UI was build-checked only for
this change. GPU L2/L3/SoC attribution and Metal's memory plateau remain unverified.

The history viewer also reads desktop `gpu_cache` diagnostics, separating
dependent-read ns/load from parallel uint32 payload GB/s. The diagnostic
engine compiles for iOS but is not exposed as a mobile benchmark entry; this
turn performed no iOS cache measurements. Shared checks now pass 144/144.

## Physical iPhone verification — 2026-10-14

Installed a signed Release build on iPhone 13 Pro (iPhone14,2), iOS 27.0.1.
All 15 supported jobs completed and 16 selected records were checked using the
app's result model. Saved CPU curves (174 points), GPU memory curves (52 points),
page mapping and a 1 GiB temporary storage test were restored by the real parser.
The phone retained 17 raw history records because the final CPU memory job was
repeated after the first run received cancellation at its end. The cancelled
attempt, its report and output were preserved; no desktop history was changed.
The completed manifest links the prior attempt through `resumedFrom`.

For development automation, launch the app with `--ios-complete-suite` to use the
same full-screen flow as the button. `--ios-complete-suite-resume` verifies and
reuses passed records from Documents/suite-runs/latest.json and reruns incomplete
jobs. The GUI button starts a fresh suite. Backgrounding or stopping cancels the
remaining work; a late cancellation after a native job has completed does not
invalidate its complete saved result. Cancellation reasons are recorded.

Continuous GPU load reached the system's serious thermal state before CPU
compute; these are sustained-load observations, not cold peak scores. The CPU
memory repeat ran in nominal thermal state. Thread affinity remains scheduler
managed. GPU cache levels and SoC/SLC were not independently attributed by the
fixed-grid diagnostic. Native bridge regression tests passed 5/5 on the iOS 18.5
simulator, and shared parser/geometry checks passed 151/151.

Local artifacts: `out/test-results/iphone13pro-20261014/` (raw phone Documents,
suite-results.json, report.md, PNG/PDF curves, CSV points and verification output).

共享成绩解析与曲线可回看 macOS 已验证的大核／小核记录及各自缓存容量。iOS 本轮保持自动调度，未开放未经实机归属验证的核心类型选择。

## API completion and expanded physical suite — 2026-10-14

The expanded iPhone 13 Pro suite passed **31/31 jobs**, selecting **32 records**:
13 Metal workload variants, 12 translated Vulkan variants, one GPU cache-state
probe, CPU compute, CPU memory/VM/storage and three ES 2.0 legacy Burn tiers.
The GPU lacks Vulkan R32_SFLOAT linear filtering required by the liquid score
contract, so that variant is excluded. With translated APIs off this device has
19 jobs; with the switch on it has 31. A device with Vulkan liquid support can
have 32. This differs from Android's API matrix; unavailable APIs are not queued.

Initial native scores and passed Vulkan scores were preserved through verified
resume. An unbounded 128 MiB cache warmup later triggered the iOS watchdog; the
failed attempt was archived. iOS v2 now traverses the same complete cycle in
8192-load command-buffer chunks, verifies every sink and restores the original
start before timing. The timed workload remains unchanged; the v2 contract
keeps the changed warming control separate from v1 and desktop measurements.
All four cache-state curves (124 points) subsequently passed on the real device.
GPU cache-level attribution still remains unverified.

Saved curves restored using the production Swift parser: CPU 174 points,
Metal memory 52, Vulkan memory 77, cache states 124. All 17 pre-existing phone
records were preserved field-for-field. Raw phone history has 56 records from
all attempts; suite-results.json selects only the 32 records in the final
manifest. Mac canonical history was unchanged. The completed suite spans
resumed runs and is not a temperature-controlled API performance comparison.

Launch automation can add `--ios-allow-translated-api` to explicitly permit
Vulkan with `--ios-complete-suite` or `--ios-complete-suite-resume`. Resume only
reuses a verified passed prefix; inserting a new job reruns later work. The GUI
button always begins a fresh suite. Vulkan probing requests only extensions
actually exposed by the bundled runtime. Invalid/empty timestamp intervals
fall back to synchronized timing and remain separately marked.

Final SDK builds cover arm64 device and arm64/x86_64 simulator engine archives.
The iOS 18.5 simulator bridge checks passed 10/11 with the unsupported unified
memory cache test skipped; cache v2 and all three ES 2.0 tiers passed on iPhone.
Desktop compatibility, suite, serializer, CPU residency and process tests passed
5/5. Artifacts: `out/test-results/ios-api-completion-20261014/` (report, selected
results, raw phone data, saved curves, verification, test logs and failed attempts).

## Second physical device — iPad Pro 12.9-inch, third generation

On 2026-10-14, iPad8,7 (Apple A12X GPU, iOS 26.7.1, system RAM 3752 MiB)
completed **31/31 supported jobs**, producing **32 saved records** in a new app
history. The runtime excludes Vulkan liquid for the missing R32 float linear
filter feature. Metal/Vulkan/ES2, CPU compute, CPU cache/memory, page mapping and
storage were measured. Production-parser audit restored CPU 174, Metal 52,
Vulkan 77 and cache-state 124 points; all four cache curves reached128 MiB.

The original cache v2 profile twice encountered GPU recovery at the 48 MiB
parallel stage. The failure logs were preserved. Devices before Apple GPU
family 6 now use 4096 chains with 512 loads per parallel dispatch; serial timing
and the complete address-span grid remain unchanged. Its version is
`metal_dependent_cache_v3_parallel_512_preview_ios_preview`, separate from
v2/8192-load data. The shorter dispatch passed on A12X. No L2/L3/SLC hardware
attribution is implied by either profile.

Resume now retains all verified passed jobs for an unchanged plan and also
when replacing only a failed cache profile. Other plan/contract changes reuse
only a verified prefix. Each reused record still passes workload/API/version,
positive-score, unique-ID and CPU single/multi-role validation. This reran only
the failed diagnostic and preserved all 31 first-run records field-for-field.
The final manifest selects 32 records; the first run and both failed diagnostic
attempts remain archived. The completed ledger spans resumed runs.

Output: `out/test-results/ipadpro3-20261014/` with the report, raw app Documents,
selected scores, all saved curves, verification and 30 compatible result pairs
against the preceding iPhone 13 Pro. Cache v3 and the different 65K Vulkan render
timing contract are excluded from that comparison. Original iPhone/Mac results
were unchanged. CPU single-thread mean109.415 MWork/s, all8 cores 491.864 MWork/s,
all-core memory read51.204 GB/s. SDK builds and shared resume regression checks
passed; simulator native tests 10 passed/1 unsupported cache test skipped;
desktop tests 5/5 passed.

## 2026-10-14 measurement audit correction

31/31 completed jobs and 32 positive saved records do not establish valid timing.
Exclude the A12X Vulkan headless particle scores for 1M, 4M and 16M; GPU query
intervals disagree with sustained throughput by 21–60 times. Some Metal render
intervals imply greater than 100% utilization and liquid intervals overlap;
these metrics remain provisional. Raw results are preserved. See
`out/test-results/apple-score-audit-20261014/comparison-audit.md` and
`score-audit.json` before comparing or ranking scores. No engine timing fix or
physical retest was performed by this read-only score audit.

## 2026-10-14 targeted timing retest

Physical retests corrected the flagged iPad11 /iPhone7 jobs, with four iPhone
thermal repeats; original histories are preserved. iOS rendered Metal now
serializes compute/render command buffers and frames; new records use
`serial_timing_v2`. Embedded Vulkan validates headless timestamps too and
falls back to synchronized timing when consecutive-submission queries omit
work. MoltenVK GPU Burn uses one frame with render-query wall-clock validation.
Default GUI suites still contain the full supported matrix. Internal device
automation may pass `--ios-suite-jobs` with supported job IDs to target a subset;
these targeted runs wait for reported nominal thermal state, up to600s per job.
No user-facing option was added. See
`out/test-results/apple-targeted-retest-20261014/report.md` for selected18
retests, all22 new records, thermal provenance and updated complete32-row sets.

## 2026-10-14 A15 foreground CPU and GPU cache follow-up

A15 normal-priority E compute passed two physical runs (89.658/89.551 MWork/s).
Foreground E cache/memory then passed with 186 verified P/E points; all-core
read35.178GB/s, E L1/L2/memory read46.4/31.0/21.9GB/s. P placement helpers are
excluded from scores. See `docs/apple-cpu-core-types.md` for the private counter
interface and its runtime validation limits. Old background results remain
separate and cannot satisfy the new foreground resume contract.

GPU cache-state follow-up completed three full scans, including doubled serial
loads, and saved372 points. A15 exposes only timestamp/GPUTimestamp through
public Metal counters in this session; external tracing collected no cache
counter samples. GPU L2/L3/SLC attribution remains unverified. Full inventory
and timing-only evidence are retained in new saved configurations.

The complete-test button is at the top. Actual simulator button acceptance
found two Vulkan16M allocation failures; capability probing now reports the
minimum storage-buffer/allocation limit and excludes larger particle presets
from its Vulkan plan, with a visible explanation. Individual Run also rejects
above-limit particle counts. The failed attempt is preserved and full revised
button acceptance is in progress. Failed jobs are shown with their count and
individual errors instead of appearing only in the finished count.
