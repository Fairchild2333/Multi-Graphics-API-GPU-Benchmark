# Mangekyo Project Handoff — 当前事实、两条主线与下一步

> **2026-10-17：先读 [APPLE-NEXT-TESTS.md](APPLE-NEXT-TESTS.md)。** iOS Vulkan固定256 MiB限制已修复，iPad8,7新预算版本确认56.657 GB/s平台，历史74条。A15/另一台iPad仍需接回补这一新版本；旧记录保留。

## 2026-10-17 iOS Vulkan platform fixed and physically verified

- Root cause:ios_host hard-coded256MiB, so V3's three tail sets could never
  all reach streamingMin256MiB. Shared Vulkan allocator lacked iOS app headroom.
- Default target512MiB; each source/destination buffer <=1/4 of current
  os_proc_available_memory(), plus existing Vulkan/heap/allocation bounds.
  API unknown -> old256MiB fallback; OOM still shrinks. No GUI options, kernel
  math, timing, data checks or20% platform threshold changes.
- Version ios_preview_app_budget_v1 and budget metadata saved. Mobile jobs
  reject old fixed-cap profile/missing policy, preventing stale score reuse.
- On wired/unlocked iPad8,7:read platform56.657,write53.5,copy50.1GB/s,
  83 points/38 checks,timestamp anomalies0,thermal0->0. App allowance2.98GB,
  buffers512MiB each, actual tail256/384/512 all streamed/flat; status passed.
  ios_preview suffix remains the mobile comparison-group scope, not review.
- Native Release/CTest7/7, device/simulator Release and signing pass. Swift
  complete-plan regression passes with selected16 true legacy records; first
  two fixture-input assertion failures retained, not suppressed. Current
  production Swift accepts real new record and restores all83 points.
- Simulator allowance0 -> actual256MiB,77points/34checks,plateau0/review;
  confirms conservative fallback, not a physical memory-pressure test.
- iPad73->74 old rows intact; other4 histories/live Mac unchanged. RawDocs,
  commands,patch/package hashes,CSV,report and tests archived under
  out/test-results/ios-vulkan-memory-platform-fix-20261017/.
- Physical App PID3380 stopped, simulator stopped/shut down. A15 and11-inch
  iPad8,1 need this new memory profile once connected; other stable projects
  need no rerun. A16 pending memory will automatically use new default.

## 2026-10-17 wired iPad8,7 Vulkan V3 follow-up completed

- Identified actual wired12.9-inch A12X iPad8,7, OS26.7.1, developer mode on
  and unlocked. Other paired11-inch iPad8,1 was not connected or tested.
- Ran only pending vulkan/memory_sweep with current signed Release App.
  Same validated A15 package, production sources unchanged since5ec3730,
  provisioning profile includes this iPad; signatures and SPIR-V hashes checked.
- Read reference53.932/write53.0/copy51.4 GB/s;55.1s,77 saved curve points,
  34 data checks passed, timestamp anomalies0, nominal0->0. V3/ordered full
  traversal and synchronized MoltenVK wall-clock metadata saved correctly.
- Existing iOS256MiB software preview cap still means mixed128/192/256 tail
  cannot establish V3 plateau. Keep plateau0/review, do not call this formal
  DRAM bandwidth or replace the old V2 row. No threshold/code changes.
- History72->73, all72 old rows unchanged; other4 current histories/live Mac
  hashes unchanged. Current production Swift validates contract and restores
  all77 points; native logs, Documents, CSV, report and exact commands archived
  under out/test-results/ipad87-v3-memory-20261017/.
- App PID3132 stopped after export; can unplug. Stable GPU/CPU projects not
  repeated; old12.9-inch Metal16M rendering stability review remains preserved.
  11-inch iPad8,1 Metal headless16M and V3 follow-ups still await that device.

## 2026-10-17 M4 Pro independent CPU baseline and controlled research

- User authorized one Geekbench CPU run and automatic public upload. Official
  signed6.7.1 ran native AArch64:single3675/multi22129, exit0, nominal before
  and after, AC/low-power off. Public result19350647. Single remains about6%
  below selected public M4Pro samples; cause/peak performance not established.
- Fixed isolated matrix:default vs apple-m4 target, original vs alternating
  branch, three2s rounds/.2s warmup each, P residency and worker busy verified.
  Original225.621 vs224.871(-.33%), alternating423.492(+87.70%) default.
  Original/alternating cycles per unit19.014/10.355. All4 actual functions keep
  bit0 branch and FP32/64FMA, no csel/fcsel; fixed checksums match CPU targets.
- Strong evidence this serial mixed kernel is branch/dependency-sensitive,
  not a comprehensive single-core score. Geekbench additionally covers SIMD,
  matrix/SME and diverse application workloads. No measured hardware branch-
  miss rate, no claim that branch/SME explains every performance difference.
- Diagnostic values are not App replacements. Production216.737 retained;
  no benchmark code/build option changes, no score scaling, no new CPU suite.
  All5 histories and live Mac hash untouched(Mac171,A15132). A15 Geekbench
  was not run; any A15 Geekbench comparison is an identified public sample.
- Evidence in out/test-results/m4-single-core-research-20261017/. Small logs,
  code and metadata versioned; official binary/ZIP stay local. Claim token
  redacted in versioned log, unredacted original stays in ignored out/research.

## 2026-10-17 A15 physical V3 memory and standard CPU confirmation

- Latest signed Release at5ec3730 installed on connected/unlocked iPhone14,2.
  Ran existing targeted jobs vulkan/memory_sweep and cpu_compute exactly once;
  no production source/threshold changes and no whole-suite button claim.
- Vulkan V3:28.842 GB/s reference,77 persisted points,34 data checks passed,
  timestamp anomalies0, synchronized MoltenVK wall clock, thermal0->0.
  Existing iOS software cap256MiB means mixed128/192/256 tail cannot establish
  the new plateau; correctly review/plateau0. This is not a hardware buffer
  limit. A formal iOS Vulkan memory plateau is still unestablished.
- CPU P147.000/E89.287/all634.868 MWork/s; changes-0.08%/-0.29%/-4.09% from
  retained147.120/89.551/661.962. P/E residency bounds >=99.8728%/99.9989%,
  worker busy >=99.9875%/99.5396%; E uses existing two-P-helper foreground
  strategy. A15 all-core remains6 scheduler-managed, unclassified workers;
  do not claim the A12X-specific typed participation contract was exercised.
- Current production models validate both jobs, four unique records and full
  saved Vulkan curve. Old128 rows unchanged, new132-row primary snapshot;
  Mac and three other iOS primary histories untouched. Native logs and exact
  commands in out/test-results/a15-v3-cpu-confirm-20261017/.
- App PID46012 stopped after final Documents copy. Phone can be unplugged.
  Two A12X V3 follow-ups and A16 missing projects remain separate pending work.

## 2026-10-17 M4 Pro single-core low-score confirmation

- User questioned207.532 as low. Ran exactly one unchanged packaged standard
  per-core confirmation,15s/.2s warmup/3 rounds; default verified P/E both.
  New P216.737, background E76.322 MWork/s. Full-core was not repeated.
- P selected round effective4.223561GHz vs prior4.034418GHz; score+4.44%,
  frequency+4.69%, cycles/work19.487 vs19.440. MinP residency99.9694%, selected
  worker busy99.8677%, thermal0->0. No wrong-core/time-unit/work-count error
  found; neither run establishes maximum single-core frequency/performance.
- Current iOS arm64 object inspected:3 timed mixed loops preserve actual bit0
  branch and FP32/FP64 FMA, zero csel/fcsel in those loops.2 integer-only warmup
  loops use selects and do not contribute measured work. This is source/binary
  inspection, not a new A15 physical run or proof of every historical binary.
- CPU MWork/s characterizes this serial mixed kernel; do not market as generic
  CPU ranking. Empty Mac inline asm emits no extra arithmetic. Keep old207.532
  and312/3582 old-version rows; do not rescale them or rerun to pick a maximum.
- Evidence/report in out/test-results/m4-single-core-check-20261017/. No
  production code change.2 formal records appended:Mac169->171, all169 old rows
  and four iOS histories unchanged; main archive index points to new snapshot.

## 2026-10-17 latest iOS simulator one-button flow passed

- User asked whether simulator validation was needed. Ran the existing real-UI
  acceptance against Release at35ea4eb on a fresh, dedicated iPhone18ProMax /
  iOS27.0 simulator, with Chinese test language. No production source change.
- XCTest tapped actual complete-suite-all; only the existing translation gate
  was enabled, no automatic suite launch, targeted job or resume.27/27 jobs
  finished,28 unique records,18 passed/9 review/0 failed/cancelled; XCTest1/1
  passed and xcodebuild exit0. Before/after screenshots exported and inspected.
- Current production models match every requested contract and result group.
  Persisted CPU174/Metal memory52/Vulkan memory77 points restored; page mapping
  and storage details present. Both CPU progress-page screenshots are portrait
  and normal, with their native job still running at capture time.
- V3 Vulkan memory correctly remains review at the simulator's256MiB ceiling:
  dataVerified1,34 checks, ordered_full_dataset_passes marker, plateau0. This is
  an expected boundary result; other8 reviews are stability/throughput notices.
  All Vulkan particle/Burn rows retain synchronized_wall_clock. No rerun to
  choose high simulated scores; no physical-device performance claim.
- Evidence/report in out/test-results/ios-simulator-v3-flow-20261017/. All five
  hardware histories and live Mac results hashes unchanged (Mac169 rows).
  Simulator stopped and preserved for reuse; full xcresult stays local, readable
  logs/JSON/screenshots are versioned. A16/A12X/A15 physical follow-ups unchanged.

## 2026-10-17 Mac measurement fixes completed and verified

- User explicitly requested continuing repairs until completion. This supersedes
  the earlier no-implementation-changes limit for the identified Mac issues.
  Base 1af3202; no benchmark score constants or historical observations changed.
- V2 write/copy looped a private 512 KiB chunk, permitting cache reuse despite
  large allocation size. V3 orders complete-buffer passes at >=256 MiB with
  compute execution/memory barriers and direct read chunk addressing. All
  platforms use new membw_v3_streaming identities; smaller cache/L1 probes stay
  diagnostics. No plateau remains preview/review, not a falsely completed score.
- Three default physical sweeps: read245.897/245.908/250.419 GB/s, each95 points,46 checks,
  dataVerified1, read platform reached, synchronized MoltenVK wall clock,
  timestampAnomalies0. Latest saved write244.7/copy238.9 GB/s. V2 preview300.885
  and its high repeated-write throughput are not V3 or DRAM peak measurements.
- Final eligibility guard requires all3 tail sets >=256MiB:128/192/256 cannot
  establish a plateau. All platforms rebuilt and CTest passed after this guard.
  Final default run passed; explicit256MiB negative control retained77 points,
  34 data checks but memoryPlateauReached0/preview/review as intended. Its
  diagnostic write550.5 must not be reported as formal DRAM bandwidth.
- Apple defaults to the existing wrapped coverage shader; CPU/particle update
  kernels and the40 B convention unchanged. Query A/B controls retain flat
  ~3.1 ms rendering versus old3.2-><1 ms. Candidate query drifts .14%/-.91%.
  Non-Apple particle defaults unchanged. Fixed-coverage captures get a distinct
  file tag; suite reuse rejects old coverage and unkept-branch Mac CPU rows.
- Mac MoltenVK particles now use completion timing as iOS already did; Android/
  Harmony native Vulkan query policy unchanged. Native Metal headless keeps its
  command-buffer GPU span, now accurately named command_buffer_gpu_time and
  separately versioned. These intervals are not interchangeable API peak scores.
- Final real6 jobs all passed, no execution failure/review: memory245.908;
  Vulkan rendered1M A126.338/B127.526 GB/s, drop-1.73%/-.19%; headless1M111.462,
  headless16M146.072; Metal rendered1M194.269. Candidate stage6 also retained:
  current policy accepts3 and flags3 Vulkan query controls. Latest valid per
  configuration selection has6 target configurations; never highest-score choice.
- All runs thermal0->0,30 s rests, ACmode2. Final tests began only after builds
  ended plus cooldown. No GPU frequency telemetry; do not invent thermal cause.
  Independent cache-level attribution remains out of scope per user agreement.
- Mac Release/package checks PASS; iOS device arm64 and universal simulator
  libraries/apps PASS; Android4 ABI APK and Kotlin5/5 tests PASS; OHOS2 ABI HAP
  PASS. Native CTest7/7 (memory assertions active in Release), shared154/154,
  iOS version/resume regression PASS. Five packages' new read SPIR-V bytes match
  their fresh outputs/staging. C++ and Swift production readers accept final6;
  Swift restores95 Vulkan points per sweep. RunOutput.points is CPU-only;
  use vulkanMemoryPoints for Vulkan, gpuMemoryPoints for Metal. An initial audit
  script used the wrong collection; fixed the script, not the production parser.
- Evidence out/test-results/macos-measurement-fixes-20261016/ (session started
  Oct16, finished17), report/verification/production audits/CSV/PNG/PDF/logs,
  candidate and final raw6 each plus boundary2, shader-package-verification,
  source patches and per-phase worker hashes.14 records total:10 accepted,
  3 legacy-query reviews,1 expected preview. Latest selection remains6 configs.
  Mac history155->169, all old objects unchanged; four iOS hashes
  unchanged. archive-index now points to newest Mac snapshot. All test workers
  exited. No physical iOS or Windows measurements/builds claimed this turn.
- docs/macos-measurement-fixes-20261017.md and APPLE-NEXT-TESTS are current.
  A16 nine unfinished jobs and iPad8,1 Metal16M remain device-dependent. A15/two
  iPads have V1/V2 memory histories; next connection needs a V3 memory run only,
  not wholesale rerunning valid CPU/Metal scores. Preserve xcuserdata.

## 2026-10-16 Mac targeted follow-ups completed; iOS Release rebuilt

- User requested this Mac's pending measurements and rebuilding iOS. Based on
  b637637, rebuilt the packaged Release arm64 app and used its production CLI
  helper, without changing kernels, shaders, score constants, timing, defaults
  or quality thresholds. No GUI-button acceptance or new iOS performance run.
- M1 Apple clang assembly: all five kernel loops retain bit-0 `tbz`, with no
  in-loop `fcsel`. The old fixed 40-line scan included two post-loop clamps;
  updated the documented check to stop at loop back edges. Full assembly and
  per-loop evidence archived. New `_kept_branch` formal CPU P207.532,
  background E69.902, all14 threads2436.328 MWork/s. Every P/E residency round
  passed, min99.9392%/99.9970%. P selected round4.034GHz, about19.44cycles/unit.
  All42 worker contributions match the three round totals and their median.
  Old Mac CPU rows remain unchanged in their old version group.
- M2 original Metal maths probe compiled and ran unchanged in fast/precise
  modes. Fast FMA6.964TFLOPS, sin/FMA.122, atan2/FMA.067, exp/FMA.288,
  pow/FMA.140, copied Burn step14.644Gpix-step/s; precise FMA7.054TFLOPS.
  Do not treat the earlier8–10 estimate as an acceptance threshold, or claim
  cross-vendor causality without a same-implementation vendor control.
- M4 Metal headless1M162.761GB/s, stable162.807, CV.34%, change-.01%;
  GPUcompute=total=.258ms, frame interval=.258ms. Only compute in the command
  buffer; no excess timing span observed. Old Vulkan219.681 uses an inner
  compute interval(.191ms) versus total GPU.267/frame.305ms, unlike the Metal
  compute-buffer span. API difference not independently attributed; timing
  implementation unchanged. The specified source audit plus short run is done.
- M3 default Vulkan v2 memory1–2048MiB completed95 points,46 write/copy checks,
  dataVerified1, no timestamp anomalies, uniform synchronized wall clock.
  Read300.885/write554.828/copy238.802GB/s are diagnostic logical throughput;
  read alternates roughly247/301 at large data sets, memoryPlateauReached0,
  version membw_v2_verified_preview_moltenvk. MUST retain preview, not claim a
  formal DRAM peak or completed formal-memory acceptance. Cache attribution
  remains unverified. Do not modify defaults or rerun to select a high score.
- M3 Vulkan rendered1M177.361GB/s, stable183.293, drop4.66%, GPU1.715ms versus
  frame2.114ms, startup timestamp check passed. Below5% in THIS run; old
  12–23% drift cause still open, tail still changes, no hardware GPU frequency.
  Five executions all thermal0->0,30s cooldowns, ACpower mode2, low power off.
- After all measurements, bash scripts/build-ios.sh rebuilt device arm64 and
  simulator arm64+x86_64 native libraries and Release apps. Source shaders
  match,56 SPIR-V resources each, Mac-only kept-branch identity absent from
  both iOS libraries. Script uses CODE_SIGNING_ALLOWED=NO; actual device
  installation still needs development signing. No simulator UI or physical
  iOS tests this turn. Native CTest7/7 passed, all benchmark processes exited.
- Evidence out/test-results/macos-targeted-retest-20261016/ contains six raw
  results, logs, assembly, audits, CSV and PNG/PDF curves, build verification.
  Standalone maths output in metal-math-probe-20261016-233740/. Main Mac
  history149->155, all149 old rows object-identical, live file and results-after
  byte-identical. archive-index.json now points to this latest Mac snapshot.
  All four iOS primary snapshot hashes unchanged. Preserve xcuserdata.
- Summary docs/macos-targeted-retest-20261016.md; APPLE-NEXT-TESTS M1/M2/M4
  marked executed, M3 formal-platform and historical-cause caveats retained.
  A16 nine jobs, iPad8,1 Metal16M and A15 optional jobs still require devices.

## 2026-10-16 Test results now belong in Git

User explicitly requested that small raw scores, full curves and logs no longer
be ignored. out/test-results data/logs/reports/exports and the root CLI history
are now versioned; the old blanket out/results/log rules no longer hide them.
See docs/test-result-archives.md and out/test-results/README.md. Preserve bytes
and original statuses; do not treat simulator/control/cancelled rows as accepted
physical scores. Large trace/xcresult bundles, dependencies and build products
remain local. Earlier notes saying all raw out data is untracked are historical.
A16 remains unplugged and CPU untested; no device test is authorized by archiving.

## 2026-10-13 Core-type selector removed; default covers all cores

User explicitly requested no additional selection option: automatically run
all core types / all cores, and asked whether cache, memory and virtual memory
were tested. Removed the Mac core-type picker/state and the newly introduced
--cpu-core-type flag from both CLI parsers, help and current method docs.
CpuBenchmarkConfig and CpuMemBwConfig now share DefaultCpuCoreType(): supported
native Apple Silicon macOS defaults to Both, other hosts keep Automatic.
GUI and suite jobs require no extra arguments. A multi-only compute run uses
Automatic internally so all14 threads run and short multi previews are not
rejected by the single-thread residency minimum. Existing test mode/duration
controls remain, and the CPU page defaults to single-thread + all-core.

No-flag compute all and short multi-only smokes passed. A cache-only smoke
(max1MiB, no-save, no storage) exposed background preemption: actual worker CPU
time within50ms could be only4-16ms, too little to meet the conservative99%
bound despite almost entirely E execution. Memory retries now automatically
double ONLY rejected samples (50,100,200,400,800ms with defaults), keeping the
99% threshold. Protocol and stored residency point encoding now include the
actual/max requested sample duration. No new user option. The repeated cache
smoke completed123 points with both classes and all-core and exit0. This is
an intentionally unsettled cached preview, NOT a DRAM score; no result saved.
The initial diagnostic harness wrongly expected exit1 for an entirely cached
sweep, but the existing summarizer can label an unsettled last point memory;
corrected the harness's expectation without changing that older summarizer.

Final macOS arm64/Intel and iOS device/simulator builds pass, CTest4/4. GUI was
restarted and inspected: no core-type dropdown, default single-thread+all-core,
footer says both types/all cores automatically. App left idle on CPU page.
Main137 records SHA256 b6284e581490616bb5892ff58ba3f19bdc4dd507086ff181ac2424b0083c96cf
unchanged. Evidence out/test-results/cpu-default-all-20261013/; no new formal
benchmarks, commits or pushes.

VM coverage clarified to user and report: the formal full sweep already tested
P/E L1/L2 R/W/C, P/E and all-core memory R/W/C, first-touch page mapping
739ns per16KiB page (22.16GB/s logical mapped-region rate), and paging-drive
sequential/random4KiB file I/O. It did NOT measure actual OS swap-out/in,
memory compression, TLB reach or VM remapping. Do not say all virtual-memory
operations were measured. Existing report updated to automatic/no-selector
wording and this precise VM coverage. Phone comparison remains deferred.

## 2026-10-13 Verified Mac P/E compute and cache/memory integration complete

User authorized continuing the core-type workaround. Added public-SDK QoS /
process-accounting / thread-CPU-clock verification in src/cpu_scheduling.h,
shared by the existing CPU compute and memory kernels. Mach process time is
converted to ns before comparison; independent conversions tolerate only a
2 ns overshoot. Attribute all possible other-thread CPU against the requested
class, plus 50 us accounting margin. Require a >=99% worker-type lower bound
for EVERY measured round/sample; retry compute twice, memory four times, and
never save an affected invalid result. Keep affinity=scheduler_managed and
never report a numbered-core binding. Capability is native arm64 macOS with
verified two named P/E levels and V6 accounting; iOS/Intel retain auto mode.
No private SPI, root, process-wide policy changes or security changes.

New --cpu-core-type auto|performance|efficiency|both selects single-thread
stages. Mac CPU UI defaults to both; complete-suite CPU jobs use both when
supported. Compute all = original all-core stage, then one P and one E stage.
Each type saves a distinct cpu_single_core version/configuration; no combined
P/E average in GUI/status. Full-suite job count is unchanged (CPU 2 jobs), but
CPU compute produces 3 records. Memory both uses P/E-specific sysctl capacities,
>=50 ms samples, both single-thread curves plus all-core/page/storage. Typed
memory version suffix qos_bound_v1_both; saved config keeps per-class capacities,
per-point lower bound/retries and last-sample effective GHz/P share. Full
per-sample observations are in the raw CLI log. P/E modes in History and
comparison plots are distinct. Shared iOS parser/charts display imported
verified data, but iOS exposes no unverified core-type run selection.

Formal GUI compute 15 s / .2 warmup / r3 passed at 21:35:45 Sydney:
P 312.254 MWork/s, lower bound >=99.8796%, observed 4.127-4.170 GHz;
E background 80.762 MWork/s, >=99.9743%, observed 1.574-1.616 GHz;
14-thread all 3581.602 MWork/s. E policy values are NOT maximum-clock E capability.
New IDs 20261013-213545-108-cpu-single-performance,
20261013-213545-114-cpu-single-efficiency, 20261013-213545-118-cpu-multi.
Full memory both passed in 182.84 s, ID 20261013-214534-490-cpu-membw:
303 curve points, 1764 accepted classified sample records, 2 rejected samples
retried. P/E curve bounds >=99.2594% /99.5465%. System cache capacities P L1
128 KiB/L2 16 MiB shared5; E L1 64 KiB/L2 4 MiB shared4. P L1 read132.7/L2
112.1/memory80.4 GB/s; E 35.0/21.1/12.7; all memory R/W/C240.6/218.1/222.4,
settled. No new independent GPU L2/L3/SLC attribution.

Found/fixed ApplyLegacyDefaults changing explicit framesInFlight=0 to2 during
append. Default2 now applies ONLY when the field is absent. Repaired the one
pre-existing row changed by these appends from the exact before snapshot and
normalized the 4 newly generated non-frame CPU rows to their original intended
zero. All original 133 records now compare equal field by field; main history
137 =133+4. Isolated append compatibility test seeded all137 and verified each
field survives another append. Negative live P-under-taskpolicy-b control exits
3 and appends zero; isolated positive saves one P/oneE/onemulti. The first
isolated-data test imported220 legacy rows before saving3; those are duplicate
IDs of the repo history, not new device records. control-data starts with [];
append-compat-data is a duplicate137-row snapshot plus one short test. Never
count reference/backup copies twice.

Final builds macOS arm64/Intel, iOS device/simulator pass; CTest4/4, parser151/151.
Mac GUI formal scores, History P/E modes, and restored303-point memory curves
with distinct cache sizes and >=99.26% verification were inspected. Native
screenshot layout is correct. App left idle in saved curves sheet on latest
CPU memory run; no benchmark process active. Relevant method docs:
docs/apple-cpu-core-types.md; evidence out/test-results/cpu-core-types-20261013/
(new-results, before/after snapshots, logs, negative-control ledger, validation,
CSV, PNG/PDF). User report/artifacts copied to shared reports/
cpu-core-types-2026-10-13/report.md. All older phone comparison remains deferred.
No commit/push. Older all-results-2026-10-13 exports remain an earlier snapshot.

## 2026-10-13 Online research: verified core-type scheduling workaround

User asked to search for a workaround for Mac core binding. Apple QoS/taskpolicy
sources and XNU accounting reveal a usable way to request P/E scheduling and
verify actual CPU-type residency, while numbered-core pinning remains unavailable.
A disposable single-threaded CLI probe uses public SDK proc_pid_rusage / V6
P-time counters around three 0.5 s rounds per policy, with no root. High QoS
P shares: 98.412%, 99.997%, 100%; background QoS and taskpolicy -b both yield
100% E time in all three rounds. The process-background probe still reports
user_interactive QoS, so requested/reported QoS alone cannot establish placement.
For a future integration, measure every round's actual P/E shares, retry or flag
mixed runs, keep scheduler_managed rather than claim pinned CPU IDs, and use a
new workload/configuration identity. Process accounting needs an isolated worker;
GUI and observer activity cannot be attributed to that worker. Background-mode
scores must record policy/frequency context and cannot be advertised as E-core
maximum frequency. No formal CPU benchmark rerun, engine change, or iOS runtime
verification in this research turn. Result-store SHA unchanged. Detailed sources,
code and raw observations: out/test-results/mac-core-affinity-20261013/research.md
and residency-verification.json. Debug/development binding sysctls are absent on
this release-kernel host; root alone cannot enable an uncompiled interface.

## 2026-10-13 Phone comparison deferred; current-host affinity API checked

User requested phone/Mac comparison and asked whether Mac can pin cores, then
explicitly deferred phone lookup when asked where the phone archives reside.
Do not continue the phone comparison under that request. This Mac lacks the
out/adb and out/hdc JSON archives referred to in earlier handoff records.

A disposable current-thread API probe reports 10P+4E and returns 46 / service
not supported for THREAD_AFFINITY_POLICY. user_interactive/background QoS calls
succeed, but are scheduling hints, not CPU-ID binding or verified P/E residency.
The affinity tag API is itself a shared-L2 placement hint, not a core number.
Evidence: out/test-results/mac-core-affinity-20261013/. No new performance run
or stored-score change. Differentiating P/E by QoS requires residency validation;
the existing fourteen single-thread samples still have no P/E identities.

## 2026-10-13 Final inventory and CPU core-identity clarification

User questioned why big/small-core scores were similar and requested all
results summarized. Source and logs confirm no Apple per-core bind: all 14
single-thread samples have scheduler_managed / Unclassified / performance_level
-1. They are sampling ordinals, not P/E-core identities; do not claim that P/E
cores were separately measured or have equal performance. No benchmark was
started for this read-only inventory.

Five independent result files contain 367 unique IDs: current Mac main 133,
repository legacy 220, and three isolated validation files with 7+4+3 records.
The two main histories total 353; the 14 isolated short tests are listed
separately. Nine backup/selected/before/after snapshots have no new IDs and are
excluded from duplicate counts. All five source SHA256 hashes are unchanged.
Exports (Markdown summary with all rows, complete CSV, 367-row JSON, 353-row
main-history JSON, manifest and optional offline filterable HTML) are under
~/Library/Application Support/GpuComputeBenchmark/reports/all-results-2026-10-13/.
The local HTML browser preview was blocked by its URL-protocol policy; source,
JSON/CSV counts and Markdown were validated without bypassing that policy.

## 2026-10-13 Complete CPU tests and native GPU cache-state diagnostic

User requested further GPU cache measurement and all CPU tests, then explicitly
authorized continuing the new diagnostic after asking why it was necessary.
Existing CPU/Vulkan/particle-derived Metal kernels and score contracts were not
changed. CPU formal `all` (15 s, 0.2 s warmup), full cache/memory/page/storage
sweep, and the existing curve-level diagnostic all passed. CPU mixed single-
thread sampling averaged 322.3 MWork/s, 14-thread multi 3726.7 MWork/s. macOS
is scheduler-managed: these are 14 single-thread samples, not verified pins of
each physical P/E core. CPU memory read/write/copy is 247.4/223.5/231.1 GB/s;
the curve-level diagnostic did not independently find a SoC tier.

A separate native Metal `--gpu-cache-probe` now measures dependent random uint32
cycles at 128-byte spacing, with GPU-warmed / after-eviction / CPU-warmed
conditions and 4096 concurrent chains. Every timed/warm dispatch checks the
exact final index and index sum; the eviction read checks its output too.
Three full 4 KiB–128 MiB sweeps passed: default, doubled loads, default repeat;
each has 124 points / 620 measured samples. It saves a new `gpu_cache` preview
with separate latency/payload-throughput curves, visible in Apple history.
Method and limits: docs/gpu-cache-probe.md. This is a diagnostic CLI path,
not a new formal complete-suite job or an iOS GPU API execution path.

Large-span GPU-warmed latency is repeatable near 319 ns at 16–32 MiB and 341 ns
at 128 MiB. Small spans show startup/iteration-count effects; CPU warming did
not consistently reproduce a distinct shared-cache benefit. Public Metal
counter-set enumeration on this host exposes only timestamp, not per-level
cache counters. Eviction is not a validated hardware flush, warm commands
do not pin a GPU core, and TLB/clock/coherence effects remain. **Do not claim
independent GPU L2/L3/SLC peak speeds or capacities from this diagnostic.**

The original Vulkan memory/L1 sweep was also repeated successfully: memory
read/write/copy 252.4/244.6/243.4 GB/s, all plateau flags true; cache-hit read
1668.9 GB/s and local probe 1439.3/125.1 GB/s aggregate/single-workgroup. This
is a different method from native dependent latency/payload throughput.

Seven runs saved eight new records (CPU all saves two), shared count 125→133.
Every pre-existing field of the original 125 records remains unchanged; the
repository's 220-row legacy JSON is untouched. Evidence and PNG/PDF curves:
out/test-results/cache-cpu-20261013/, with report also under the shared reports
directory as cache-cpu-complete-2026-10-13.md. Parser checks 144/144 and CTest
3/3 passed; macOS arm64/Intel and iOS device/simulator builds passed. No iOS
performance test was run. All requested measurements are finished.

## 2026-10-13 Saved cache and memory curves in Apple GUI

User asked to complete the curve display and inspect previous scores. Both
Apple GUIs now offer **Cache & memory curves** from History and Charts. The
viewer reconstructs saved CPU read/write/copy curves, native Metal allocation
curves and Vulkan memory/local-cache curves from workloadConfig. It also shows
Vulkan cache/memory summaries and the local/L1-style probe in current-run and
history views. This resolves the display gap described in the audit below;
independent GPU L2/L3/SLC measurement and attribution are still unverified.
Metal retains its particle-derived preview label and unverified plateau.

The viewer can open a JSON result array or results envelope read-only; this
changes only its local selection and never imports into the shared score store.
CPU points are KiB per thread, GPU memory points MiB, GPU local-cache points
KiB per workgroup, and Metal allocation sizes already bytes. Unknown P/E
classification, cache capacity or plateau status must remain unknown.

The current shared file has 125 records, including three memory sweeps with
198 CPU, 95 Vulkan and 56 Metal curve points. The repository's older
results/results.json has 220 particle records (62 DX11, 59 DX12, 46 Vulkan,
45 OpenGL, 8 Metal) and no memory sweeps; cache curves cannot be reconstructed
from those individual particle scores. Both files retain their original bytes:
shared SHA256 a9478417d06615402f95c34d293ce9d9d5c29731b4a3477b973536db919cb6bb;
repository SHA256 5f01464406c9dfd9927061beeccdbc2b4c2f03cc4613c0f41e1e1fd8cc1b3c35.
No new benchmark was requested or run for this display change.

Shared parser/geometry checks pass 139/139. macOS arm64/Intel and iOS device/
simulator builds pass. The native macOS GUI was inspected with actual saved
CPU, Vulkan and Metal results; the curves and summary rates agree with the JSON.
The old 220-record JSON opened in the GUI with a read-only/no-sweep message,
and returning to the current history restored its saved memory data. The macOS
file panel attaches to the current window and restores focus after dismissal.
Local evidence belongs under out/test-results/history-curves/.

## 2026-10-13 Cache coverage and unified-memory audit

User asked why GPU L1/L2/L3 and SoC cache speeds were absent, and whether the
memory speed is credible. This turn audited saved results/source and Apple
primary documentation; no new benchmark or GUI implementation was performed.
Apple's MacBook Pro M4 Pro specification lists 273 GB/s. This host is Mac16,8,
14 CPU cores (10P+4E), 20 GPU cores and 48 GiB RAM. Saved large-set read rates
are Vulkan 238.796, Metal preview 244.776 and CPU 245.834 GB/s (87.5-90.0% of
the specification). Vulkan's 1/1.5/2 GiB tail is flat, with zero timestamp
anomalies; CPU's large all-thread tail is also flat. The native Metal value
retains its unverified-plateau preview status.

Cache results do exist in workloadConfig: GPU local/L1-style probe 1393.8 GB/s
aggregate and 123.3 GB/s single workgroup; GPU cache-hit range 1521.5 GB/s read;
CPU one-thread L1/L2 read 129.9/111.3 GB/s, all-thread 1444.0/1125.9 GB/s.
These are kernel effective rates. Do not claim hardware L2/L3/SLC attribution
or a validated M4 GPU L1 capacity from those numbers. GPU distinct L2/L3 and
SoC/SLC speed isolation remain unimplemented/unverified. The GUI currently
plots MEMBW_POINT but omits MEMBW_L1/MEMBW_SUMMARY cache summary tables;
History shows only the primary score. This is a display gap in addition to
the missing independent cache-level methods. The 31-job pass does not certify
all cache levels. CPU saved-curve reread preserves OS-selected levels and does
not reveal a new independently measured SoC tier.

Audit evidence: `out/test-results/macos-complete-20261013/cache-audit.{json,md}`
and `cache-audit-reread.txt`; user report in
`~/Library/Application Support/GpuComputeBenchmark/reports/m4-pro-cache-memory-audit-2026-10-13.md`.
Sources: https://support.apple.com/en-au/121553 and
https://developer.apple.com/videos/play/wwdc2020/10603/.

## 2026-10-13 Full macOS run with Vulkan / Metal comparison

The user explicitly requested the full series including Vulkan today, revoking
the earlier stopped-run restriction for this test. The GUI opt-in was enabled
and its one-click 31-job series completed **31/31 success**, no failed/skipped
jobs and all 8 required native Metal captures present. CPU all-core/per-core
execution saves two records, so the initial series produced 32 new records.

Runtime verification found a real MoltenVK Retina mismatch: GLFW's 1280x720
logical window produced a **2560x1440 Vulkan surface**, while native Metal
rendered 1280x720. A surface-capabilities probe reproduced this and confirmed
the corrected surface as 1280x720. The macOS framebuffer-scaling hint now
applies to Vulkan too; MoltenVK records/logs the actual render extent. Corrected
rendered scores add `_viewport1280x720_v1`, preserving the old result group.
Headless/native score contracts and stored historical records remain intact.

After rebuilding arm64 and Intel and passing CTest 3/3, Vulkan rendered jobs
passed 8/8 again. Metal rendered jobs also passed 8/8 without capture, giving
the comparison matching 1280x720, 15-second, no-capture settings. All 48 new
records are retained in the shared results.json; the report selects 32 records
for the 31 projects. Initial rendered scores are kept as evidence and excluded
from the final comparison. No old scores were changed or deleted.

On this M4 Pro single run: Vulkan GPU Burn is 33-57% above Metal; 16M
headless particle is Vulkan 133.438 vs Metal 143.277 GB/s. Memory results are
Vulkan 238.796 GB/s (2 GiB, median, plateau reached) and native Metal preview
244.776 GB/s (512 MiB, best, plateau unverified), so do not claim the difference
is translation overhead. Particle rates are derived kernel metrics and liquid
retains its distinct native preview contract. Ordering/thermal repeats remain
outside this run's evidence.

Evidence: `out/test-results/macos-complete-20261013/` contains the before/after
data, plan, 31-job ledger, viewport probe, per-run logs/IDs, selected-results.json,
comparison.csv and summary.json. The user report is also saved to
`~/Library/Application Support/GpuComputeBenchmark/reports/metal-vulkan-complete-2026-10-13.md`.
The test is finished, translated APIs remain enabled for the user's requested
comparison, and no new full series should start without a new request.

## 2026-10-12 GPU memory entry in Apple GUI

The GPU memory test was missing from the main workload selector: only a
standalone probe button below the results was present. macOS now lists
**Memory Bandwidth — VRAM / Cache** as a normal workload; Auto selects native
Metal first, explicit Metal saves its preview through `--memory-sweep-save`,
and explicit Vulkan uses `--workload membw` only with the translated API
switch enabled. OpenGL is unavailable for this workload. Full analysis can
queue each supported memory implementation/device; memory runs do not request
duration, presentation or capture settings. The complete series already
contained memory and retains its 18 native / 31 opt-in jobs.

iOS lists the same entry and runs its native `memory_sweep` without a rendering
surface or fullscreen cover. Both GUIs retain automatic result persistence.
Native Metal preview curves and Vulkan median read/write/copy/local-cache
curves are parsed separately; an unsettled Vulkan memory plateau is explicit
in the score label. Shared parser/geometry checks pass 104/104. macOS arm64
and Intel plus iOS device/simulator builds passed. The native macOS workload
menu and disabled irrelevant settings were inspected in the running GUI.
No complete suite or translated benchmark was started; the app remains idle
with translated APIs off.

## 2026-10-12 Translated API opt-in (latest user preference)

The user's final instruction supersedes outright removal: retain Vulkan behind
an **Allow translated APIs (MoltenVK)** switch, default OFF, and expose it in
API selection only after opt-in. The stored preference gates capability probes,
custom GPU jobs, complete/fill-missing jobs and suite children. With the current
arm64 runtime: OFF = 18 jobs/no Vulkan; ON = 31 jobs/13 Vulkan additions.
Without the flag an explicit CLI Vulkan request exits 2 before initialization.

The host has an arm64 MoltenVK dylib with minos 12.0; its Vulkan SDK loader has
minos 27.0. The build therefore optionally copies/links/signs compatible
MoltenVK directly, with a relative bundled runtime path, its license and SPIR-V
resources. No new SDK loader is shipped. Direct MoltenVK has no instance
portability-enumeration extension (unlike the SDK loader); device portability
subset is enabled. Intel falls back to Metal/OpenGL if no matching runtime is
available. MoltenVK scores carry `_moltenvk` and explicit translation metadata.

Only builds, signatures, capability enumeration and policy-manifest checks were
performed. Do not claim translated workload/performance acceptance or restart
the complete series: the user has not turned on the new switch or requested a
new benchmark run in this task.


## 2026-10-12 Complete-series correction and native OpenGL 4.1

The user meant the adb 29-job series, not the old 10-job desktop GPU matrix.
Their latest instruction supersedes the interim 29-slot/skip proposal: APIs
that are unavailable do not enter the queue. Native macOS OpenGL 4.1 is now
built and probed; its 16/64/256 GPU Burn tiers run with no compute shader or
SSBO, GLSL 4.10 and explicit uniform-block binding. Particle compute still
requires GL 4.3 and is excluded from GL4.1. Current M4 Pro plan: Metal 13,
OpenGL 3, CPU 2 = 18. `--suite-list` is a read-only manifest inspection.

The old macOS GPU bridge parsed the CLI TSV columns incorrectly; corrected
metadata and external-helper probing avoid Cocoa calls from a GUI background
thread. The complete UI displays its detected count and includes CPU result
charts. OpenGL requests no macOS capture. Native capture names now contain the
workload and configuration, matching the suite's real-file checks; previously
native traces had only device/time names and could be falsely reported missing.
Suite cancellation forwards to the complete worker process group (tested with
non-GPU controller/worker/grandchild processes).

Three short, isolated OpenGL runs passed and saved positive scores; driver
`4.1 Metal - 91.7` has no timestamp queries so `_sync_timing` is used. Matrix,
capture-naming and process-cancellation tests pass alongside result compatibility.
Apple Silicon/Intel macOS and iOS device/simulator builds pass. Shared Swift
parser/geometry tests now pass 96/96; CTest passes 3/3. A short native capture
created a trace with the exact workload/size filename. Cancelled suite progress
retains its completed count instead of showing 100%.

The user's complete test remains stopped; do not start the full series again
without a new request. Android's adb/hdc lists remain unchanged.


## 2026-10-12 Apple integration verified with Xcode

- Merged origin/main `111db53` while preserving the three local Apple commits.
- Extracted desktop CPU execution/storage into `src/cpu_cli.*`; iOS CPU and
  CPU-memory requests now reach those functions without a CAMetalLayer or
  unavailable desktop process spawning. iOS CPU results retain `_ios_preview`.
- GPU/CPU charts now belong to their own channel; comparison filters include
  workload version and compatible particle sizes. Native process output is
  buffered by complete lines and drained before completion.
- iOS preserves headless particle requests, chosen counts/steps and V-Sync;
  rendered tests use 1280 x 720. Both bridges use canonical result serialization
  so schema/platform/device metadata is retained.
- Both Apple GUIs expose the particle-derived Metal memory diagnostic, with
  live curves and separate preview results. Allocation budgets, initial data,
  command failures and cooperative cancellation are handled; it is not Vulkan
  `membw_v1` or a validated memory-plateau contract.
- Runtime shader sources are bundled by the iOS Xcode project itself. Build
  scripts use tracked projects by default and no longer install XcodeGen.
  macOS staging resolves the actual cached GLFW source license.
- Verified with Xcode 27: arm64 + x86_64 macOS Release apps, iOS device arm64
  and simulator engine arm64/x86_64 + native simulator app builds. Product floors
  remain macOS 12.0 and iOS 16.0. macOS slices pass signature/dependency checks;
  Intel is a cross-build, not physical Intel runtime evidence.
- Native arm64 helper smokes: CPU compute, CPU memory, Metal headless particle,
  GPU Burn, interactive pool and GPU memory diagnostic all exit 0 from /tmp;
  saved result schema/model metadata was inspected in isolated test storage.
- iOS 18.5 hosted XCTest: 5/5 pass (CPU with no surface, headless GPU with no
  surface, malformed CPU input, CPU memory sweep, stored metadata through bridge).
  Shared Swift parser/geometry/translation: 92/92 checks; results compatibility
  CTest: 1/1 pass. macOS GPU/CPU/Charts UI navigation inspected.
- Artifacts: `out/macos/{arm64,x86_64}/Release/Mangekyo.app`,
  `build-ios/DerivedData-{iphoneos,iphonesimulator}/Build/Products/Release-<sdk>/Mangekyo.app`,
  `out/test-results/apple-ios18-complete.xcresult`, `out/test-results/native-smoke-summary.json`.
- Still unverified: physical iPhone/iPad, macOS 12/iOS 16 runtime acceptance,
  thermal contracts and distribution signing/notarization. iOS device app is
  unsigned; installing it requires a development team/device.


> 最后更新：2026-10-08（Australia/Sydney）
> 分支 / 提交：`main`（本地含 macOS 构建/Metal/SwiftUI/DMG 与 iOS 移植，以及 Android / HarmonyOS 设备实测改动；用户自己的未跟踪 `results/` 不得删除、移动或纳入提交；勿把 `out/` 构建产物当仓库事实）
> 本文件是项目的**首要进度与交接入口**。后续 AI 或开发者开始工作前先完整阅读，结束工作前优先更新本文件，再更新专题 TODO、roadmap 和 README。

## 1. 交接规则

1. 事实优先级：**可复现测试结果 > 当前源码 > 本文件 > 专题 TODO > roadmap > README**。
2. “代码已写”“可编译”“已实机运行”“跨 API 可比”“GUI 已接入”“可安装发布”是六种不同状态，不得混写成“已完成”。
3. 新发现应带文件与行号；完成项应带命令、机器/API、结果或产物路径。
4. 每次会话结束前至少更新：`当前结论`、`正在进行`、`下一步`、`验证记录`。
5. 不删除原始粒子测试，不改变其历史 workload id（`stream`）、确定性 seed、默认参数或旧结果含义，除非同时做结果 schema/version 迁移。
6. GUI 是今后的主入口。CLI、WinUI、macOS GUI、图表和结果模型必须从同一份测试 metadata/registry 获取能力，避免再次出现 7/5/1 套状态漂移。
7. **不要随便创建新分支**：未经用户明确同意，不得 `git switch -c` / `git checkout -b` 新建分支。在默认/主干分支上的改动按用户指示直接提交或暂存；如确需分支，先说明用途并征得同意。
8. **成绩文件的兼容规则（2026-10-06 用户要求）**：`results.json` 由不同版本的程序共用。改 `BenchmarkResult` 时只能新增字段，不能改名、删除、改类型或改含义；新字段要加进 `src/benchmark_results.cpp` 的 `kFields` 表；改完运行 `ctest --test-dir build -C Release`，并重新编译 WinUI 界面。细则和已知限制见 §3.9。

## 2. 用户锁定的目标（A/B 两条产品主线 + C 平台移植优先级）

### 2026-07-16 品牌与兼容性决定

- 用户可见产品名统一为 **Mangekyo**，说明语为 **Cross-API CPU & GPU Benchmark Suite**。产品定位仍是 **GPU 主测试 + CPU 补充测试**，不能因品牌调整把 CPU 写成与 GPU 后端/图形场景同等成熟的主线。
- 这次是显示品牌与信息架构调整，不是结果合同或内部 ABI 迁移。内部 CMake target、可执行文件和 CLI 命令暂时继续使用 `gpu_benchmark`；workload id、`workloadVersion` 与历史成绩名称保持不变。
- Windows 安装器继续复用既有 stable AppId；用户数据继续写入 `%LOCALAPPDATA%/GpuComputeBenchmark`（或 `GPU_BENCH_DATA_DIR` 覆盖路径）。不要为了改名生成第二套 AppId、数据目录或静默丢失旧 History；若以后迁移安装包文件名，必须做显式升级/兼容验证。
- WinUI 主导航已拆为独立 **GPU** 与 **CPU** 页面，History/Charts 继续共享同一结果存储。文档中的历史窗口标题、旧 ZIP/Setup 文件名和验证记录属于证据，不做盲目字符串替换。
- 2026-07-16 品牌验收：Release CLI 与 WinUI x64 均重新构建通过；`gpu_benchmark.exe --help` 首行显示 Mangekyo，GUI PE 的 `ProductName/FileDescription/FileVersion` 为 `Mangekyo / Mangekyo - Cross-API CPU & GPU Benchmark Suite / 0.1.0`。stage verifier 为 0 errors，并实际生成 `Mangekyo-0.1.0-windows-x64.zip` 与 `Mangekyo-0.1.0-windows-x64-setup.exe`，Setup VersionInfo 为 `Mangekyo / Mangekyo Setup / 0.1.0`。本次品牌产物是 CLI-only smoke，用于验证命名和安装器；它不替代带 WinUI、RenderDoc、report worker 与许可证的完整 GitHub Release 候选，完整发布仍须在提交后重跑正式 release 脚本。

### 目标 A — 测试产品线

- 保留原始粒子测试，继续支持当前电脑的全部可用 GPU、软件设备/WARP 和图形 API。
- **2026-07-21 用户锁定当前关注范围**：Original Particle（`stream`）、Plasma/GPU Burn（`gpu_burn`）和当前 Cinematic Liquid（`cinematic_liquid_v2_physical_scene_v8` 或其后继版本）。本文后续的“液体”默认只指当前版本；N-body 不进入双卡路线，也不是当前需要深究的产品项。
- 固定流程维持 **15 秒运行 + 第 5 秒 RenderDoc 抓取完整一帧**；多 GPU AFR/SFR 是例外：RenderDoc 注入会破坏 linked-adapter/device-group 的排队与计时，因此自动禁用抓帧并明确记录 unavailable/disabled，不能伪造第 5 秒捕获。
- 新增能达到 FurMark 类负载水平、但不照搬甜甜圈造型的短时极限压力测试。
- 新增真正的 3DMark 类综合图形测试；流体可以成为综合场景的重要 pass，而不是单独替代核心压力测试。
- 当前效果不够好的新原型保留到 `Other / Advanced / Legacy`，不能删除，也不能冒充正式主分数。
- 所有正式能力必须同步进入 GUI。

### 目标 B — 一装即用

- 发布的是预编译、依赖与资源均已配置好的应用，而不是要求用户复制作者开发环境。
- 换一台干净电脑后，用户不需要 Visual Studio、vcpkg、Vulkan SDK、shader compiler、Python 或手工补丁即可运行。
- RenderDoc 抓取与报告链也由安装包提供或受控安装，行为等价于当前开发机配置完成后的效果。

### 目标 C — 平台与兼容前端优先级（2026-07-16 用户最新锁定）

用户最新锁定的顺序（**2026-07-19 调整**：Windows 7 GUI 挪到 PS3 之前）：**1. Windows on ARM (ARM64) → 2. macOS → 3. Android → 4. iOS → 5. Debian Linux → 6. WebGPU → 7. HarmonyOS PC / 鸿蒙 → 8. Windows 7 专用 GUI（尽量使用 Aero）→ 9. PS3（探索性）→ 10. Lumia 1520 legacy Windows Phone（探索性）→ 11. iPhone 4 legacy iOS（探索性）→ 12. Dual-GPU Collaboration（双卡协作模式，功能项）→ 13. HTC HD2（探索性，2026-10-08 排入）**。ARM64 已完工；macOS vertical slice 已在 M4 Pro / macOS 26 上完成自包含 App、三主项与原生抓帧验收，实际 Monterey 运行与公开签名/notarization 仍开放。用户指令曾为先完善 macOS 0.2.6 并分别产出 Apple Silicon / Intel DMG，再完善 iOS/iPadOS；随后 Android 与 HarmonyOS 推进了多设备实测与移植。液体正确性、自由模式和 soak 任务仍然开放，但不阻塞平台顺序，除非用户再次改序。

除仓库已有的隔离 HarmonyOS Vulkan 粒子原型外，均未开始完整产品移植；该原型不等于主 workload suite 已移植。Windows 7 GUI 是共享现有引擎与 worker 协议的兼容前端，不得为了支持旧系统复制出另一套含义不同的成绩合同。

通用规则：
- 逐平台落地时沿用现有合同规则：能力不齐明确 unsupported、不静默 fallback；**计时/抓帧模型不同的实现必须使用新 `workloadVersion` 独立成组**，现有 Windows 成绩组（`stream`、`gpu_burn_v2_mangekyo_faceted_glass_v1`、`gpu_burn_v1`、cinematic liquid 各版本）的 A/B 对比不受任何影响。
- 每个发布包必须记录主程序、GUI、安装器 bootstrap 与所有原生 DLL/EXE 的真实 PE 架构。ARM64 包优先使用原生 ARM64 依赖；任何 x64/ARM64EC/仿真组件必须显式列入 manifest/SBOM 和 UI 能力说明，不能静默混入并宣称“全原生 ARM64”。
- 本节顺序与第 9 节保持一致；后续 AI 不得再按旧的液体优先级自行调回顺序，除非用户再次明确调整。

逐平台要点：
1. **Windows ARM64（已于 2026-07-16 完工；2026-07-31 与 x64 统一原生双语安装器）**：已实现本体、GUI、依赖与打包闭环。新增了 VS/CMake ARM64 配置，原生编译 `gpu_engine`、CLI 和 WinUI，使用 vcpkg `arm64-windows` 在 manifest 模式下引入原生 GLFW。完成了动态 ARM64 Vulkan 导入库自动生成，解决了 x64 SDK 链接冲突；豁免了 VC 运行时中特有的 x64 `vcruntime140_1.dll` 架构审计。**CPU 与 GPU 测项本体也是按架构分别原生编译**（x64 包 = AMD64 PE，ARM64 包 = ARM64 PE；同一套源码、两套二进制），不是 x64 测项装到 ARM 上。正式安装器为 x64/ARM64 各一个对应原生 PE 的单文件中英双语 Setup EXE；WiX MSI 是内嵌事务层，Inno Setup 降为 legacy。
   - **依赖规则**：GLFW、WinAppSDK、VC runtime 等均使用原生 ARM64；豁免了特殊的 x64 `vcruntime140_1.dll`（Redist 目录自带）。RenderDoc 在 ARM64 发布语义上可为 Skip/N/A；Python 报告链仍未冻结。
   - **完成门槛**：ARM64 本体、GUI、ZIP/原生双语 Setup 与依赖本地 staging 校验及 PE 架构审计已通过；clean-machine 与签名仍开放。
2. **macOS（2026-07-31 已完成当前 Mac vertical slice；用户 2026-07-19 指定四步）**：
   - **产品 OS 底线（用户 2026-07-19 锁定）：macOS 12 Monterey**。CLI/`gpu_engine` 与 SwiftUI GUI 均 `deploymentTarget=12.0`（`CMakeLists.txt` `CMAKE_OSX_DEPLOYMENT_TARGET`、`macos-gui/project.yml` / `Info.plist` `LSMinimumSystemVersion`）。macOS 11 及更旧不支持。
   - **版本差异（功能合同相同，UI/壳层不同）**：12 = `NavigationView` + Material；13–15 = `NavigationSplitView` + Material；26+ = Liquid Glass（`.glassEffect`）。建议在较新 Xcode（26）上编译、部署到 Monterey+ 运行。
   1. **CLI Metal Particle 对齐 Windows `stream_v1` 合同（M4 Pro 实机通过）**：正式 time-mode 离屏绕过 ProMotion；结果 `memory=Unified-memory`；`--capture` / F12 / `--capture-frame` 走 `MTLCaptureManager` → `.gputrace`（失败则诚实 `captureUnavailable`）。2026-07-31 包内 helper 已完成 1M 粒子 15 秒 + 第 5 秒真实 `.gputrace`。
   2. **Metal GPU Burn（M4 Pro 实机通过）**：`shaders/gpu_burn.metal` + `metal_backend` 双 pass 全屏；成绩组仍走现有 `gpu_burn_v3_fixed_steps_*_kaleidoscope`。2026-07-31 包内 helper 15 秒 / 16 steps 通过，stable score 4.16 Gpix-step/s、CV 0.14%。
   3. **SwiftUI ↔ WinUI 工作流对齐（已编译、链接、独立启动）**：CPU；Duration；Capture；预设；API 能力；Progress/Cancel；Complete Suite / Fill Missing；结构化错误摘要；History mode/旧异常分数处理；原生 Charts（13+ Swift Charts、12 SwiftUI fallback）；英/中/日。最终 App 可脱离仓库运行，Monterey `NavigationView` 壳层仍须在实际 macOS 12 验证。
   4. **液体 Metal（M4 Pro preview 实机通过）**：`scripts/port_liquid_v2_metal.py` + `port_liquid_render_v2_metal.py` → `shaders/cinematic_liquid_v2.metal`（10 compute + `liquidFragment` raymarch）；宿主 `src/metal_cinematic_liquid.mm`。2026-07-31 包内 helper 6 秒通过，正确报告 320,920 粒子；成绩继续强制 `cinematic_liquid_v2_physical_scene_v8_metal_preview`，不得混入 Vulkan v8 榜，正式跨 API 合同仍开放。

3. **Android**：复用 Vulkan 后端；GLFW 不支持 Android，需 NativeActivity/ANativeWindow 表面层与新前端；RenderDoc 支持 Android 远程抓帧；须评估温控降频对 15 秒 Burst 语义的影响（可能需要独立移动端 duration 合同）。
   - **主路径基线**：Vulkan **1.0+**（主力，后端理论可复用；基线放宽到 1.0 以覆盖 Tegra K1 这类只有 Vulkan 1.0 驱动的设备）+ GL ES 3.1/3.2（由现有 GL 4.3 后端降级适配：GLSL ES 方言重写、EGL 上下文、`EXT_disjoint_timer_query` 能力探测，无可靠 GPU timestamp 不出正式 score）。计时/抓帧模型不同的实现独立 `workloadVersion` 成组。
   - **设备分界线**：**Tegra K1 (2014, Kepler, ES 3.1 + Vulkan 1.0, 系统停在 Android 7.0) 及以后 = 主路径**；X1 (Maxwell, ES 3.2 + Vulkan 1.1) 完全无障碍。**Tegra 3/4 = 下方 ES 2.0 legacy tier**。K1 跑液体是性能问题而非能力问题。
   - **SDK 版本合同（用户 2026-07-20 锁定）**：主包 **minSdk 21 (Android 5.0) / targetSdk 最新**。要点：NDK r27+ 编译并开启 **16 KB 页对齐**（target Android 15+ 对原生 `.so` 强制）；结果/历史写应用专属目录（`getExternalFilesDir`/内部存储，对应 Windows 侧 `%LOCALAPPDATA%` 语义），不碰共享存储；新 API 一律 `Build.VERSION.SDK_INT` 守卫；Vulkan 运行时门控（API ≥24 且 `dlopen` 成功才启用，否则 ES 3.1，同 Windows delay-load 思路）。legacy APK 单独 `minSdk 21 / targetSdk 可较低`（仅侧载，不为老壳子适配新行为）。targetSdk 不影响 15 秒 Burst 语义；温控评估照旧。
   - **ABI 合同（用户 2026-07-20 锁定）**：主包计划编译 **armeabi-v7a、arm64-v8a、x86、x86_64 四个 ABI**，全部按架构原生编译（同 Windows x64/ARM64 两套原生二进制的规则，不做仿真混入）；x86/x86_64 主要覆盖模拟器、老 Atom 设备与 Chromebook。结果 metadata 必须记录真实 ABI，不同 ABI 不混排宣称。legacy APK 只需 **armeabi-v7a**（Tegra 3/4 均为 32-bit ARM）。
   - **UI 基调（用户 2026-07-20 锁定）**：主包**单套 Jetpack Compose + Material 3**，能力递减而非双套 UI——动态取色 (Material You) 仅 Android 12+，以下回退 Mangekyo 品牌静态 Material 3 配色；预测式返回 (13/14+)、单色图标 (13+)、SplashScreen compat 均自动降级。3D 场景走原生 SurfaceView/ANativeWindow，Material 3 只管壳层；信息架构对齐 WinUI/SwiftUI（GPU/CPU/History/Charts 四页，同一份 workload registry/metadata，遵守交接规则 6）。**androidx 下限抬升注意**：若开工时最新 Compose 已要求 minSdk 23，主包直接提到 23（主包最老目标设备 K1 = API 24，零损失）；否则锁 21 兼容版本。**legacy APK 不用 Compose/Material 3**：2012 年 1GB RAM 设备用极简原生 View（列表 + Run + 结果文本）。满载时 UI 必须保持可 Stop，跑分工作不占 UI 线程（对齐 GPU Burn Soak 的 GUI 规则）。
   - **前端 + 引擎垂直切片（2026-07-24 事实）**：`android/` Compose Material 3 壳 + `libmangekyo_jni` 链入根 `gpu_engine`（`GPU_BENCH_NO_GLFW` / ANativeWindow `vkCreateAndroidSurfaceKHR`）。GPU 页可对 `stream` / `gpu_burn`（light steps=16）做默认 3s preview Run/Stop；SPIR-V 经 glslc 进 assets。**本机 Windows：`assembleDebug` 已通过；未真机运行。** `CollectResult` 在 `__ANDROID__` 下真实追加 `_android_preview`（例：`stream_v1_android_preview`、`gpu_burn_v3_fixed_steps_16_kaleidoscope_android_preview`）——不是桌面 15s 合同，禁止混排。GLES 降级 / 液体 / CPU 宿主 / 温控 15s 仍 TODO。详见 `android/README.md`。
   - **用户锁定底线（2026-07-20）：Tegra 3/4（GL ES 2.0）必须支持**，形态为独立 **ES 2.0 legacy tier**，不允许用 unsupported 打发：
     - **GPU Burn**：简化 legacy 变体（纯 fragment 全屏重载，避开 highp 依赖——Tegra 3 fragment 仅 FP20；降低 SDF/raymarch 复杂度），新组如 `gpu_burn_es2_legacy_v1`。三主项中最先做。
     - **Stream/Particle**：ES 2.0 无 transform feedback、Tegra VTF 不可靠，粒子物理无法留在 GPU。改为 CPU(NEON) 物理 + GPU 渲染，或纯 fragment 填充率/带宽测试；含义已变，必须新 workloadVersion，绝不与现有 `stream` 组混排。
     - **Cinematic Liquid**：无原子/SSBO/3D 纹理/精度，无降级空间，明确 **unsupported**。
     - **计时**：Tegra 3/4 无可靠 GPU timer query，legacy tier 只用墙钟 FPS，成绩自成体系；**用户已确认不要求 RenderDoc/抓帧**，抓帧标注 unavailable 即可。
     - **打包**：这些设备停在 Android 4.1–5.1，现代 NDK 最低 minSdk 21；Tegra 4 的 4.4 设备可能需旧 NDK。做**独立 legacy APK**，主 Android 包（Vulkan/ES 3.1+）不背此包袱。
4. **iOS**（**2026-07-24 事实：代码已写，本机未编译、未模拟器/真机验收**——`ios-gui/` + `ios_engine_host` + `gpb_*` 桥；`CollectResult` 在 iPhone 目标下追加 `_ios_preview`）。用户锁定细节见 **§3.0.3**（最低 **iOS 16**；**iOS 26/27 = Liquid Glass**）。摘要：Metal-only + SwiftUI；无 RenderDoc；`MTLCaptureManager`；App Store/沙盒；温控/正式 duration 合同仍开放。
5. **Debian Linux**：技术摩擦最低——Vulkan/OpenGL 后端、GLFW、RenderDoc、XDG 数据路径全部已有；主要是构建修正、`.deb` 打包、CI 与实机验证。**2026-09-30**：Ubuntu 20.04 / JetPack 5 的构建阻塞已修复（提交 `a894c8a`，仅做过容器验证，未在真机上运行），见第 10 节。
6. **WebGPU**：按第 11 节既定路线执行——先统一 capability registry（P0），再固定 Dawn 版本的原生后端，依次移植 Stream、GPU Burn、Cinematic Liquid，最后 `/web` 浏览器前端。结果使用独立临时版本 id（如 `stream_webgpu_v1`），记录 `apiImplementation/underlyingBackend/timingMode`；timestamp-query 是可选能力，没有可靠 GPU timestamp 不产生正式 score；浏览器无 RenderDoc，抓帧标注不可用。
7. **HarmonyOS PC / 鸿蒙**：**2026-10-06 起 `ohos/` 已改为接入共享 `gpu_engine` 的 App**（粒子测试、GPU Burn、CPU 算力测试、缓存 / 内存带宽扫描、历史和图表），能用 DevEco Studio 26.0 的工具链编出未签名的 HAP，但**没有在任何设备或模拟器上运行过**。成绩使用独立的结果组（`_ohos_preview` 后缀）。流体测试、抓帧编排、正式的 15 秒结果合同、签名和发布包都还没有做，不能写成已完成移植。见 `ohos/README.zh-CN.md` 和第 10 节验证记录。
8. **Windows 7 专用 GUI（排在 HarmonyOS 之后、PS3 之前）**：当前 WinUI 3 / Windows App SDK 与现有安装器最低版本是 Windows 10 1809，不能通过改 manifest 假装支持 Windows 7。另建共享 `gpu_engine`/CLI worker/结果 schema 的原生 Win32 兼容前端与独立安装包；优先使用 DWM/Aero 能力（例如玻璃区域、非客户区整合、主题化控件、Direct2D/DirectWrite），但必须运行时探测 DWM composition，并在 Aero Basic、经典主题、远程桌面或 DWM 关闭时正常回退，不能把透明/模糊当硬依赖。
   - Windows 7 包不得携带 WinUI/WinAppSDK payload；需要单独审计 toolset、Windows SDK、VC runtime、GLFW、DX11 FL10/SM4 与抓帧工具的 Win7 兼容版本。若没有可安全再分发且实测可用的 RenderDoc 组合，第 5 秒抓帧必须明确显示 unavailable，而不是降级到含义不同的捕获或伪造成功。GUI 外观不同不应改变 GPU/CPU 计时与成绩组；任何实际 worker、timer 或 capture 合同差异才触发新 `workloadVersion`。
9. **PS3（探索性，永不进成绩体系）**：官方开发授权已停止，只能 homebrew（PSL1GHT/RSXGL）；RSX 无 compute/SSBO/原子/GPU timestamp，PSGL≈OpenGL ES 1.0+Cg 而非桌面 GL 4.3，三个主测试均不可直移。至多做隔离的固定管线情怀 demo，建议独立仓库，不进入正式成绩合同与 GUI 主界面。
10. **Lumia 1520 legacy Windows Phone（探索性，用户 2026-07-20 排入 PS3 之后、iPhone 4 之前；永不进成绩体系）**：骁龙 800 / Adreno 330——硬件本是 ES 3.0 级，但 WP8.1 / Windows 10 Mobile 驱动仅暴露 **D3D11 FL 9_3**（无 compute/UAV，shader 仅 `*_4_0_level_9_3` 档），所以形态同 ES 2.0 legacy tier：简化 fragment Burn、粒子 CPU(NEON) 物理、液体 unsupported。前端必须 WinRT/UWP + CoreWindow（无 Win32/GLFW/GL/Vulkan）；工具链 VS2015（W10M）/ VS2013（WP8.1），可在现代 Windows 上运行；部署靠设备开发者模式侧载（免费），商店已死、无分发渠道。FL 9_3 GPU timestamp 不保证可用 → 墙钟计时，成绩自成体系。shader 复用优先走微软 archived **ANGLE**（GL ES 2.0 → D3D11 9_3），直接复用 Tegra 3/4 的 GLSL ES 2.0 legacy shader；备选原生 HLSL 9_3 重写（多一份维护）。主引擎/结果合同不带过去，独立小工程。
11. **iPhone 4 legacy iOS（探索性，用户 2026-07-20 排入 Lumia 1520 之后；永不进成绩体系）**：iPhone 4 = A4 + PowerVR SGX535，仅 GL ES 2.0，无 Metal，系统止于 iOS 7.1；32-bit 应用无法上 App Store，只能免费账号自签侧载（7 天过期）。工具链：**OS X 10.11 + Xcode 7.3.1**（首个支持 free provisioning 且仍可构建 armv7 / 部署 iOS 7 设备的版本；只能跑在老 Intel Mac 或 10.11 虚拟机）。**不精简主工程**：主 C++ 引擎（现代 C++ 标准）、Metal/Vulkan、结果合同均不带过去；独立仓库或 `legacy-ios/` 小工程，仅复用 Android ES 2.0 legacy tier 的 GLSL ES shader 与 workload 定义，宿主为 EAGL + 极简 UIKit + ObjC，粒子走 CPU 物理；SGX535 比 Tegra 3 更弱，需单独更低预设。墙钟计时、无任何抓帧、成绩不进主 History。主 iOS 计划（最低 iOS 16 + Metal + SwiftUI，§3.0.3）完全不受影响。
12. **Dual-GPU Collaboration（实验性功能；首个目标为 Boot Camp Windows 双 D700）**：2026-07-21 已完成 Plasma/GPU Burn 的第一条 AFR vertical slice，CLI 为 `--multi-gpu afr` / `--afr`，仅允许窗口化 `gpu_burn`。DX12 已确认同一 LDA 暴露 2 nodes，并使用交替 NodeMask、node-local queue/backbuffer/command list/root signature/PSO/RTV/timestamp/fence；Vulkan 已确认两张 D700 属于同一 physical-device group，并把 device mask 贯穿 acquire、command buffer、submit 与 LOCAL present；DX11 只能提交普通单逻辑设备帧流，让 UMD 决定是否启用隐式 CrossFire，程序明确记录 `implicit_driver_unverified`。**纠正后的 D700 验收结论**：AFR 至少需要 4 frames-in-flight（每卡两个可复用 slot），并必须完全关闭 RenderDoc 注入；RenderDoc 加载时会出现约 2000 FPS、无 timestamp、退出阶段长时间 drain 的假结果。自动强制上述条件后，DX12 在 1280×720、2 draws 下由单卡约 113 FPS 提升到 222–230 FPS（16 steps），由 19 提升到 39 FPS（128 steps），约 1.96–2.05×，双节点平均占用约 91–99%。Vulkan `20.45.40.15` 的 graphics queue family 只有 1 个 queue，device-mask AFR 仍串行：约 102 vs 104 FPS（16 steps）、17 vs 17 FPS（128 steps）。DX11 窗口模式约 120 vs 120、20 vs 20 FPS；最终又用 AMD 官方 AGS 6.3.1 探针分别请求 `AGS_CROSSFIRE_MODE_EXPLICIT_AFR` 与 `AGS_CROSSFIRE_MODE_DRIVER_AFR`，两次都能枚举 2 张 D700、成功创建设备，却都返回 `crossfireAPI=0`、`crossfireGPUCount=1`，确认 FireGL 驱动未向该应用开放 CrossFire，隐式或显式 AFR 均未激活。AFR 结果隔离到 `..._afr2` 合同，禁止与单卡混榜；目前只有 DX12 可以描述为实测双 D700 加速。AMD Vulkan `20.45.40.15` 在 device-group LOCAL swapchain 的所有子资源销毁后仍会于 `vkDestroyDevice` 内部崩溃，代码只对该精确版本延迟最后 device/instance 释放到 worker 进程退出；其他驱动正常销毁。下一步若继续双卡，只做 Original Particle 固定总量分片，以及当前 Cinematic Liquid 的模拟卡→渲染卡流水线实测；不做两份完整 workload、成绩相加、N-body 深挖或长时双卡烤机。

   **AFR / SFR 边界（0.2.2）**：Original Particle 虽可在 DX12 API 层交替节点，但其 `N+1` 帧依赖 `N` 帧粒子状态；每帧跨节点复制会抵消收益，两份独立状态又改变测试语义，因此不实现粒子 AFR。DX11 只有不可控、不可验证的驱动隐式 AFR，本机实测也无提升。粒子双卡只接受固定总粒子数的 SFR/data-parallel：两卡各模拟并绘制同一帧的一半，最终只合成/呈现一次，不是同时跑两个测试。**DX12 Plasma SFR 已实现**，CLI 为 `--multi-gpu sfr` / `--sfr`：node 0 直接画左半屏，node 1 在本地全尺寸 target 上只画右半屏，再把 640×720 区域复制到 node-0-owned 的 1,843,200-byte cross-adapter buffer；跨节点 fence 后 node 0 复制进 backbuffer 并唯一 Present。D700 返回 `D3D12_CROSS_NODE_SHARING_TIER_1`；旧驱动拒绝 cross-adapter committed resource（`E_INVALIDARG`），改用微软示例的 explicit shared heap + placed resource 后稳定运行并正常退出。独立短跑对照：16 steps 单卡 112 FPS / 8.17 ms，SFR 69 FPS / 14.33 ms（0.62×，固定复制成本主导）；128 steps 单卡 19 FPS / 49.75 ms，SFR 28 FPS / 35.11 ms（1.47×，但仍低于 AFR 约 39 FPS）。2/4 flights 结果相同，排除帧环过浅。SFR 结果隔离为 `..._sfr2`，不能写成普遍提速。Vulkan 因只有一个 graphics `VkQueue` 且 group present 仅 LOCAL，SFR 暂不进入实现。

   **GUI 双卡入口（0.2.2）**：Windows Advanced 卡片现有 `Multi-GPU: Off / AFR / SFR`，选择框后带 `i` 提示。仅当 Preset=Custom、Workload=Plasma、且 API 只勾选 DX12 时启用；其他组合自动退回 Off。AFR/SFR 会向 worker 传 `--multi-gpu afr|sfr`，自动关闭并锁定 RenderDoc，切回 Off 时恢复用户原先的抓帧选择。该控件只暴露当前已验证的 DX12 linked-adapter 路径，不展示 Vulkan/DX11 的无加速实验。代码使用标准 DX12 NodeMask/GetNodeCount，并无 AMD 专用调用；理论上可覆盖驱动暴露为单个 ≥2-node linked adapter 的 NVIDIA SLI，但尚未在 NVIDIA 双卡上验证，绝不能标成已支持传统 SLI。

   **History 运行模式（0.2.2）**：`results.json` 原本已保存 `headless`，并在 `workloadConfig` / `workloadVersion` 中保存 `multiGpu=afr|sfr`、`_afr2|_sfr2` 与控制方式，但旧 History 表只显示 workload + steps。现新增独立 `Mode` 列：`Single`、`Headless`、`AFR ×2`、`SFR ×2`；旧 DX11 隐式记录显示 `AFR（未验证）`，Vulkan device-group 记录显示 `AFR（实验）`，无 control 元数据的早期记录显示 `AFR（旧记录）`。CLI 通用排行榜分组键也新增 headless/windowed 维度，两项比较只有 execution mode 相同才输出 score delta，防止 `stream_v1` 的无窗口和窗口成绩混排。

   **抓帧边界（0.2.2）**：RenderDoc 并非全局关闭。单 GPU 仍执行 15 秒 + 第 5 秒抓帧；AFR 与当前 SFR 都自动禁用注入和抓帧，因为 D700 多节点运行加载 RenderDoc 后会产生约 2000 FPS、timestamp 缺失和退出长时间 drain 的假数据。SFR 尚未单独证明 capture-safe。多 GPU 调度用原生 timestamp + GPUView/PIX 诊断；需要检查 shader 时可退回单节点复现并抓帧。

   **0.2.2 Windows 构建记录**：2026-07-21 用恢复后的 VS 18 / MSVC 14.50.35717 完成 x64 Release CLI + WinUI 构建，0 error；SFR 收口后又通过 `scripts/build-windows.ps1` 完整重建，GUI 邻接 worker 与 CMake Release worker 的 SHA-256 均为 `8AA80CBF799092A374394DFE37436278A63631E8B9F34F5255E9C2677E1E41AF`。GUI 保留既有 4 个 VCLibs/重复 WinAppSDK initializer warning。此前 CMake 消失、MSVC props 指向新 toolset 但文件未齐，与 Visual Studio 自动更新过程中的组件中间态吻合，不是本次双卡代码导致；更新完成后 CMake 4.2.3-msvc3 与完整编译均恢复。所有 SFR/AFR 短探针都使用独立 `GPU_BENCH_DATA_DIR`，未写入正式 History。

   **Vulkan AFR 深挖（2026-07-21）**：D700 的 queue family 0 是唯一 graphics+compute family（`queueCount=1`），但 family 1（2× compute queue）和 family 2（2× transfer queue）也都支持 Win32 present。0.2.2 现于 AFR 优先把 present 移到 family 2，实测正常运行但性能仍不变：16 steps 约 103 FPS、128 steps 单卡/AFR 均为 17 FPS，聚合 GPU-equivalent 约 98–100%，排除了 present 占用 graphics queue 和轻负载掩盖。尝试在同一 LOCAL swapchain 上先 acquire GPU0 图、再挂起 acquire GPU1 图，以便两次 submit 都排在 present 前；AMD `20.45.40.15` 会在第二 acquire 内无限等待，该实验已完全撤回，不得恢复。device-group peer-memory 探针显示两个 multi-instance device-local heap 的 0→1 / 1→0 都只有 `VK_PEER_MEMORY_FEATURE_COPY_DST_BIT`，没有 copy-src/generic 访问：副卡可以主动写主卡目标，但主卡不能直接读取副卡本地输出。最后又完整实作并实测了 Vulkan 专用 compute-to-swapchain Plasma AFR：surface/format 的 storage 能力通过，family 1 的 queue 0/1 按 device mask 0x1/0x2 直接写各 GPU 的 LOCAL swapchain image，禁用 RenderDoc 的 3 秒探针正常初始化、运行和退出；但 16 steps 只有约 119 FPS / 8.18 ms，GPU-equivalent 约 98%（平均每卡约 49%），仍没有跨帧重叠，只是 compute shader 与原 fragment 合同的微小管线差异，不能算双卡加速。该实验代码、shader 变体和独立成绩合同已完整撤回，不发布。至此单 `VkDevice` 下的低复杂度 Vulkan AFR 绕行已穷尽；若未来再开，只能单独立项评估两套独立 `VkDevice` + external memory/semaphore，不能继续在 0.2.2 上试探。

   **OpenGL SFR 探针（2026-07-21）**：FireGL 20.45.40.15 暴露 `WGL_AMD_gpu_association`，`wglGetGPUIDsAMD` 返回两个 GPU ID，当前可见 context 对应 id 1；但 id 2 的 renderer 查询为空，而且 `wglCreateAssociatedContextAttribsAMD`（OpenGL 4.3 core）与旧式 `wglCreateAssociatedContextAMD` 都返回 `NULL`。先解绑可见 context 后重试仍失败，`GetLastError` 也保持 0。也就是说 ICD 能枚举副卡，却不能为它创建可提交 GL 命令的 associated context；没有第二 context 就无法执行“左右半屏分别渲染 + `wglBlitContextFramebufferAMD` 合成”的显式 SFR。原型及 CLI 放行已完整撤回，0.2.2 继续只允许 DX12 SFR。

13. **HTC HD2（探索性，用户 2026-10-08 排入，排在已有各项之后；永不进成绩体系）**：初代骁龙 QSD8250（单核 Scorpion 1 GHz，ARMv7 带 NEON）/ Adreno 200，原装 Windows Mobile 6.5。轮到它时先看这台实际装的是什么系统，分两条路：
    - **刷了 Android（这台机器有名的用法）**：不用开发。Android 4.x 上 App 装不了（最低 6.0），直接用 2026-10-08 为小米 3 做的两个命令行工具：`mangekyo_cpumem`（静态链接，CPU 算力和缓存 / 内存）和 `mangekyo_es2burn`（OpenGL ES 2.0 GPU Burn，离屏），`scripts/build-android-cpumem.ps1 -Abi armeabi-v7a -ApiLevel 21 -Static`，推到 `/data/local/tmp` 里跑。能不能出 GPU 的数取决于那个移植版的 EGL / OpenGL ES 驱动能不能用，插上才知道；CPU 两项只要内核是 Linux 就能跑。成绩只有日志。
    - **仍是原装 Windows Mobile 6.5**：要另起一个小工程（Visual Studio 2008 + Windows Mobile 6 SDK，原生 C++，OpenGL ES 2.0 走 Adreno 200 的早期驱动），工具链和驱动都已停止维护多年。默认不做，除非用户明确要。
    - 数据意义：它比今天测过的最慢的设备还慢一两个数量级，只作为下限参照，不和别的设备排在一张图里。

## 3. 当前结论（先读这一节）

### 3.1 产品判断

建议最终只把三条路线放在主界面：

| 主测试 | 定位 | 当前状态 |
|---|---|---|
| **Particle (Original / Baseline)** | 原始粒子计算+绘制；历史基线，实际偏显存/内存带宽 | 已保留稳定 id `stream`；GUI 明确标为 Memory Throughput。**Windows 全 API 正式可用**。**Metal 已在 M4 Pro 完成 1M / 15 秒与第 5 秒 `.gputrace` 实机验收**（离屏 / `Unified-memory` / `MTLCaptureManager`） |
| **GPU Burn (15s Burst)** | 全屏 Plasma×Kaleidoscope 视觉 Burn（当前公开 `gpu_burn` / `gpu_burn_v3_fixed_steps_*` / shaderVersion=3）；历史 faceted-glass / Plasma Bloom legacy 身份按 `workloadVersion` 隔离 | **Windows：Vulkan/DX12/DX11/OpenGL 正式可用**。**Metal：M4 Pro / 16 steps / 15 秒实机通过**，stable 4.16 Gpix-step/s、CV 0.14%。`gpu_stress` 仍 Metal unsupported |
| **Cinematic Liquid** | 固定镜头的真实 3D 粒子液体 + 粒子重建密度体积自由表面 | **Vulkan**：v1 与历史 optics_v4 正式成绩保留；当前 MPM `physical_scene_v8` **尚无正式 15s+第5秒抓帧成绩**；SPH 强制 `_preview`（四项 blocker）。**Metal：320,920 粒子 MLS-MPM + raymarch 已在 M4 Pro 6 秒实机通过**；成绩仍强制 `…_metal_preview`，正式跨 API 合同未完成。DX12/DX11/GL 液体未实现 |

### 3.0.1 三主项缺口清单（2026-07-19）

| 主项 | Windows 还差 | macOS / Metal 还差 |
|---|---|---|
| **Particle** | 无合同级 blocker（基线已稳） | 当前 M4 Pro 已验收；仍需实际 **macOS 12** 启动/短跑回归与公开发布签名/notarization |
| **GPU Burn** | 无 Metal 相关 blocker；可选后续 CoreBurn/错误校验 | 当前 M4 Pro 15 秒已验收；仍需实际 **macOS 12** 短跑回归 |
| **Cinematic Liquid** | v8 正式 15s+第5秒 RenderDoc 成绩；SPH 四项正确性；跨后端 | Metal preview 当前 M4 Pro 已验收；正式成绩不得混 Vulkan v8；SPH/v1 未移植；macOS 12 回归仍开放 |

### 3.0.2 macOS OS 底线（2026-07-19 用户锁定）

- **最低运行系统：macOS 12.0 Monterey**（用户明确要求「至少最低能在 macOS 12 上运行」）。
- **已改代码侧**（非仅文档）：
  - `CMakeLists.txt`：`CMAKE_OSX_DEPLOYMENT_TARGET=12.0`
  - `macos-gui/project.yml`、`GPUBenchmark.xcodeproj`、`Info.plist`：`MACOSX_DEPLOYMENT_TARGET` / `LSMinimumSystemVersion=12.0`
  - `ContentView.swift`：`#available(macOS 13, *)` → `NavigationSplitView`，否则 `NavigationView` + `.columns`
  - `GPUBenchmarkApp.swift`：`defaultSize` 仅 13+；12 仅保留 `minWidth/minHeight`
  - `GlassCard`：26+ Liquid Glass，12–15 Material（原有回退）
- **2026-07-31 构建事实**：`scripts/build-macos.sh` 在 M4 Pro / Xcode 26.6 成功生成自包含、ad-hoc 签名的 `out/macos/arm64/Release/GPUBenchmark.app`；GUI 与 helper 均为 arm64 / `minos 12.0`，只依赖系统 framework/lib，Charts 为 weak link，未链接 Homebrew/Vulkan dylib。该事实证明交叉部署标记与当前系统运行，不替代实际 Monterey 运行。
- **验收门槛**：在 Monterey 上至少冒烟 GUI 启动 + `gpu_benchmark --backend metal --workload stream --time 3`；15s 正式流程仍按三主项清单。
- **构建约定**：在较新 Xcode（建议 26）交叉编译部署到 12+；不要求在 Monterey 本机用旧 Xcode 从源码构建（`.glassEffect` 依赖新 SDK 符号，已用 `#available` 包裹）。

### 3.0.3 iOS 实现规格（2026-07-19 用户锁定 — 给后续 AI 开工用）

> **状态（2026-07-31 checkpoint）**：仓库已有 `ios-gui/` SwiftUI + `CAMetalLayer` → `ios_engine_host` → `MetalBackend::Run` 垂直切片；stream/gpu_burn host 存在，Cinematic Liquid 的 Metal v2 backend 存在但 iOS host parser 与 bundle resource 尚未接通。已在本 Mac 用 Xcode 26.6 成功生成 iPhoneSimulator arm64 / iOS 16 / Metal-only / NO_GLFW 工程；第一次 `gpu_engine` build 因 mobile archive 错带桌面 `src/main.cpp`、其中 5 处 `std::system` 在 iOS SDK unavailable 而失败，`CMakeLists.txt` 已改为 iOS 排除 `main.cpp`，**但暂停点尚未复建验证**。Bridge/UI 仍引用不适合沙盒的 `cliMain`/`gpb_run`，不能声称三主项、CPU、抓帧或 App 已可用。`BenchResult.swift` 与新增 `MobileBenchmark.swift` 已开始补 schema/三主项能力模型，但尚未编译。无签名身份、无连接的 iPhone/iPad；模拟器和真机验收均未完成。当前执行顺序：macOS 0.2.6 双 DMG → **iOS/iPadOS**。

#### A. 用户锁定

| 项 | 决定 |
|---|---|
| **最低系统** | **iOS 16.0**（`IPHONEOS_DEPLOYMENT_TARGET=16.0` / `LSMinimumSystemVersion=16.0`） |
| **Liquid Glass** | **iOS 26 与 iOS 27 必须使用新版 Liquid Glass**（`.glassEffect` 等，与 macOS 26 Tahoe 同设计语言） |
| **更低系统 UI** | **iOS 16 … 25**：Material / 标准 SwiftUI 回退（与 macOS 12–15 策略对称） |
| **后端** | **仅 Metal**（首版不暴露 Vulkan/MoltenVK/OpenGL/DX）；能力不足写 unsupported，禁止静默 fallback |
| **抓帧** | **无 RenderDoc**；`MTLCaptureManager` → `.gputrace`；失败 JSON/UI 诚实 `captureUnavailable` |
| **成绩** | 与桌面共用 schema；若 duration/温控/后台模型不同 → **新 `workloadVersion`**，不得混入 Windows/macOS 桌面 15s 正式榜 |

#### B. 仓库布局（代码已存在；Mac 构建/设备验收未做）

```
ios-gui/                          # 已存在：XcodeGen project.yml + SwiftUI
  project.yml
  Mangekyo/                       # iOS app target, deployment 16.0
    Views/ Engine/ Models/ …      # Run/CPU/History/…；嵌入 Metal 预览面
src/ios_engine_host.h/.cpp        # CAMetalLayer → MetalBackend::Run
src/metal_backend.mm              # TARGET_OS_IPHONE 嵌入路径
# AppleShared/ 仍未抽离；部分 UI 从 macos-gui 复制/改写，非独立共享 target
```

参考 macOS 前端：`macos-gui/GPUBenchmark/`。

#### C. 引擎与表面层（相对 macOS 的必改点）

1. **禁止依赖 GLFW / 桌面窗口** 作为 iOS 正式路径。用 `UIView` + `CAMetalLayer` 或 `MTKView`；可先「全屏 Metal 视图 + 覆盖 SwiftUI 控制条」，再考虑嵌入。
2. **复用** `gpu_engine` / `MetalBackend` / `shaders/*.metal`；CMake 增加 `ios` / `iphoneos`/`iphonesimulator` 产物或 Xcode 直接编译 ObjC++ 源（与 macOS 链接 `libgpu_engine.a` 对称）。
3. **PathService**：用 iOS Application Support / Documents（沙盒）；结果与 captures 不得假设 `~/Library/...` macOS 路径。
4. **后台/打断**：进后台应 Cancel 或暂停并标记 run incomplete；勿在挂起时继续计分。
5. **温控**：15s Burst 可先沿用；若实测降频严重，另定移动 duration 合同并升 `workloadVersion`（例如后缀 `_ios_thermal_v1`），桌面组不动。
6. **CPU 补充测试**：iOS 无桌面级 affinity；沿用 `scheduler_managed` / 估计拓扑语义，与 macOS CPU 合同对齐并独立标注平台。

#### D. SwiftUI / 导航 / 玻璃（按系统版本）

| iOS 版本 | 壳层建议 | 视觉 |
|---|---|---|
| **16 – 25** | `NavigationStack` + 列表/ Tab（手机）；iPad 可用 `NavigationSplitView`（iOS 16+） | **Material**（`.ultraThinMaterial` 等），**禁止**假装 Glass |
| **26 / 27+** | 同上导航；控件跟系统 Liquid Glass | **必须** `.glassEffect`（`GlassCard` / 选中态等）；与 macOS 26 共享组件 |

- 部署底线 16 → 可直接用 iOS 16 API（`NavigationStack`、`Table` 在 iPad 等），不必为 iOS 15 写 `NavigationView` 回退。
- `GlassCard` 已预留：`#available(macOS 26.0, iOS 26.0, *)`；开工时确认 iOS 26/27 SDK 符号，缺则用 `#available` + 编译期 SDK 探测，**不得**在 16–25 上硬链不存在的 API。
- 本地化：复用 `Localization.swift`（EN / 简中 / 日语），与 WinUI/macOS 文案对齐。

#### E. 功能对齐（相对 WinUI / macOS GUI）

首版至少：Run（Custom + 有限 Full Analysis）、三主项（`stream` / `gpu_burn` / `cinematic_liquid` Metal）、Duration、Capture-at-N、Progress + Cancel、History（含 `workloadVersion`）、Settings（主题/语言/数据目录说明）、About。Charts：若无 Python，首版可「打开文件/分享结果」或标 N/A，禁止假成功。  
API 选择器：iOS 只列 **Metal**；其余 API 标 unsupported。

#### F. 构建与分发

- **工具链**：Xcode 26+（为 Liquid Glass / iOS 26–27 SDK）；`IPHONEOS_DEPLOYMENT_TARGET=16.0`。
- **真机 + 模拟器**：Metal 路径以真机为准；模拟器可烟测 UI。
- **App Store**：Sandbox、隐私清单、无私下拉 RenderDoc；导出 `.gputrace` 用 Files / Share（能力不够则 UI 标明不可用）。
- **签名/TestFlight**：实现阶段再接；不阻塞本地 Debug 跑通。

#### G. 验收门槛（完工定义）

1. iOS 16 真机或模拟器：App 启动，Run 页可发起 Metal `stream` 短时冒烟（例如 `--time 3` 等价）。
2. iOS 26 或 27 真机/模拟器：**目视确认 Liquid Glass**（卡片/导航非纯 Material 回退）。
3. 一次带 capture 的短跑：成功则有 `.gputrace`（或明确 Share 路径）；失败则 `captureUnavailable`，不伪装 RenderDoc。
4. History 能读到本次结果且 `workloadVersion` / `graphicsApi=Metal` / `osVersion` 正确。
5. 进后台再回前台不崩溃、不计脏分。

#### H. 给其他 AI 的硬约束

- 先读本文件与 `macos-gui/`，再写代码；结束时更新本 § 与 §9。
- 不新建 git 分支除非用户同意（见 §1.7）。
- 不把 preview / stub 写成正式桌面可比分；Metal 液体桌面仍是 `…_metal_preview` 合同，iOS 若未对齐 timing 须再加平台后缀。
- 不实现 Android/Web 冒充 iOS；不引入 GLFW 作为 iOS 主路径。

移动端若因温控缩短正式 duration，必须新 `workloadVersion`，不得混入桌面 15s 榜。`15s` 对外称 **Burst / 短时峰值压力**，不宣称热稳定认证。

### 3.1.1 2026-07-15 历史暂停点与恢复状态

用户曾明确要求**暂时停止新的实机测试、预览运行和 RenderDoc 抓帧**；该暂停在当时已遵守。此要求是历史上下文，不再是当前阻塞：用户随后先授权 duck family 视觉改造，之后又明确要求继续流体工作，本轮据此恢复了水体光学实现，并只做了必要的短 `_preview` 验证。

2026-07-15 晚间第一阶段：用户明确提出“照参考图优化鸭子造型，加大鸭子/小鸭子/子母鸭”，据此完成 duck family 场景改造（见 3.1.1a）及最短预览。该 v5 鸭群改动当时原样保留到 v7，并继续保留到当前场景；上一个四刚体基线 `cinematic_liquid_v2_surface_splat_optics_v4` / `shaderVersion=6` 及其 15 秒正式结果永久保留为独立成绩组。

### 3.1.1a 2026-07-15 Duck family 场景改造（v5）

- 鸭子 SDF 重做：旧的 7 块硬拼椭球（肚+胸+尾+双翅，接缝生硬）替换为 smooth-min 融合的经典大黄鸭（丰满蛋形身体 + 上翘尾 + 大圆头 + 扁宽橙嘴）。三处实现保持同一份形状数学：`shaders/mls_mpm_grid_update_v2.comp`、`shaders/mls_mpm_g2p_v2.comp`（碰撞）与 `shaders/cinematic_liquid_render_v2.frag`（渲染改为对同一 SDF 做 72 步 sphere-trace，黑眼珠/白高光/橙嘴均为贴花上色，不再是独立几何）。
- 新增 3 只约 0.45 倍的小鸭子（刚体 4-6，`kCinematicLiquidV2BodyCount` 4→7）；sink sphere 的释放编排硬编码在 lane 3，因此新刚体只允许追加在 index 3 之后。小鸭质量 ~1.82、扶正力矩更强（shape0.w=26）、线/角阻尼更高，初始间距均大于两两 bounding 半径之和，避免出生瞬间被 pair solver 弹开。
- grid-update / g2p 的刚体循环加入保守 bounding-sphere 剔除，七刚体循环对绝大多数节点/粒子只付一次距离平方判断。
- 结果身份升级：`workloadVersion=cinematic_liquid_v2_duck_family_v5`（预览为 `..._preview`）、`shaderVersion=7`、`sceneVersion=3`；不得与 optics_v4 混分。

### 3.1.1b 2026-07-15 Iterative optics v6（历史预览合同）

- 用户调整后的鸭子及 duck family v5 的 7 刚体/SDF/碰撞布局全部保留；本轮只在其上升级水体重建与光学，没有回退或覆盖鸭子造型。
- 当时结果身份为 `workloadVersion=cinematic_liquid_v2_iterative_optics_v6`（短运行写为 `..._preview`）、`shaderVersion=8`、`sceneVersion=3`。它与 v1、历史 optics_v4 正式组、duck-family-v5、v7 及当前 v8/SPH 均不得混分。
- 表面重建升级为 5x5x5 binomial filter（`mix=0.90`）；ray path 最多处理 4 次界面，逐段使用 Fresnel/Snell 与 Beer–Lambert，并在每一段重新做刚体/池体/地面深度排序。当时消光系数为 `extinction=(30,10,8)`，输出采用 linear exposure + sRGB，不再使用 ACES；密度体积边界强制归零，避免 clamp 边缘形成实体水盒。
- 最终短预览结果 `20260715-221447-024`（RTX 5090 / Vulkan / 6 秒、5.1 秒抓帧）：Compute 9.450 ms、Render 3.003 ms、Total 12.454 ms、`257.01 MParticle-step/s`、284 measured frames；0.117 秒抓帧开销已排除，1 attempt/1 saved。核验图为 `rdoc_captures/cinematic-liquid-v2-5s-iterative-optics-v6-final-preview.png`。
- **v6 从未运行正式 15 秒流程，没有正式成绩。** 当时 CLI Release 与 WinUI Release x64 已构建通过；该历史结果不能转记为 v7。

### 3.1.1c 2026-07-15 Physical scene v7（历史合同）

- 历史结果身份为 `workloadVersion=cinematic_liquid_v2_physical_scene_v7`、`shaderVersion=9`、`sceneVersion=4`；固定 128x64x96 网格、10 substeps 与 **320,920 粒子**，分解为 142x14x98 base（194,824）+ 48x37x71 dam（126,096）。v1/v4/v5/v6 的正式结果或预览全部原样隔离。
- 用户鸭子和 7 刚体 index ABI 原样保留：母鸭 index 0、彩球 index 1、船 index 2、沉球 index 3、小鸭 index 4-6。船由硬锚定改为 34 kg 有限质量刚体和软系泊，推进器反冲与流体冲量可驱动/摇摆；这只说明实现路径已接通，**尚未视觉验收船的实际轨迹**。
- index 3 沉球仍为 1.06 水密度比，4.28 秒释放，使用 -9.81 重力与 0.015 空气阻尼；材料水阻按 displaced mass 和浸没率门控，以避免未入水时就产生水下式阻尼。沉球入水 crown/whitewater 来自真实 GPU body state 与局部流体响应，不是 fragment shader 画出的假水花；当前仍没有 secondary-spray 粒子 pass。
- v7 当时的池体合同使用有限高度的内嵌碰撞壁（inset 0.22）；外层 simulation catch band 允许粒子真实越过池沿并落到地面，但带宽有限，不能描述成无限流体域。5x5x5 binomial surface resolve 自适应保留低支持喷滴。
- 前景侧壁为独立透明软 PVC 薄膜近似：IOR 1.50、Fresnel、弱吸收和程序化 wrinkle；这**不是完整的 PVC 多介质 ray path**。池底内衬使用独立 hit 分类，使水下焦散落到实际可见内衬而非被遮住的草地平面。场景加入无限程序化草地与大气天空/云，没有引入 cubemap 资产。
- v7 只完成 shader 编译、最终 6 个 SPIR-V 校验、CLI Release 与 WinUI Release x64 构建；WinUI 为 0 error，仅有既有 MSB8027/C4996/LNK4042 类 warning（最新增量构建显示 2 个重复 WinAppSDK warning，源文件重编时曾显示 4 个）。只运行了若干 `--time 6` / `--time 8` 自动停止 smoke：**没有正式 15 秒、没有可靠保存的新 RenderDoc、没有完整视觉验收**。控制台 transient `241.13` 不是正式或持久结果。用户看到窗口约 2-3 秒后关闭，是测试脚本 `--time 8` 的自动生命周期，不是目前证实的崩溃。

### 3.1.1d 2026-07-16 当前 shared scene v8 与 SPH 收口

- 后续共享池体/光学参数已经变为 `poolWallInset=0.45`、`poolWallTopFraction=0.42`、`extinction=(12,3.6,2.5)`，因此当前 MLS-MPM 身份升为 `cinematic_liquid_v2_physical_scene_v8`、`sceneVersion=5`；任何历史 v7 smoke 都不得转记为 v8，v8 尚无正式成绩。
- SPH vertical slice 为 318,464 粒子。RTX 5090/Vulkan 已完整运行 15 秒且正常结束；用户接受水池、水面、鸭群、球、船、草地/天空的当前外观，**视觉迭代已经收口**。
- 视觉通过不等于成绩通过。SPH 在以下四项全部关闭前，任何时长（包括 15 秒）都强制写入 `cinematic_liquid_sph_slice_v1_preview`：渲染帧驱动 2×1/120 时间推进、每 substep 未清 `bodyImpulses`、viscosity 原位 SSBO race、atomic scatter cell-order 非确定性。草地吸收/回收倒计时的当前实现是 0.50–1.80 sim 秒。

“是否基本完成”必须按层次回答：

- **Cinematic Liquid v2 的 Vulkan benchmark vertical slice：视觉上基本完成，正式计分仍未完成**。SPH 的 15 秒外观已获用户接受；只有 optics_v4 有正式历史成绩，v7 只有历史 smoke，当前 MPM v8 没有正式成绩，SPH 则被代码强制为 preview。
- **用户要求的当前水体视觉：已在 SPH 切片上通过验收**。这取代旧 optics_v4 “水体太假”的当前视觉结论，但不抹除该历史反馈，也不关闭上述四个正确性 blocker。
- **完整产品：未基本完成**。最终 WinUI 交互/history、跨 GPU 固定时间推进、其余四后端、异常资源清理、安装器 clean-machine 验收以及无限/自由模式仍开放。

恢复前的只读上游源码对照已从 v6 保留到 v7：MIT [jeantimex/fluid raymarch](https://github.com/jeantimex/fluid/blob/9daf3ae2add7dedc3eadcc10da5e4a44ef1b771f/src/sph/3d/webgpu_raymarch/shaders/raymarch.wgsl) 使用最多 4 次界面迭代、Fresnel、Beer–Lambert 与环境光；其 [raymarch 配置](https://github.com/jeantimex/fluid/blob/9daf3ae2add7dedc3eadcc10da5e4a44ef1b771f/src/sph/3d/webgpu_raymarch/main.ts) 为 `densityTextureRes=150`、`stepSize=0.02`、`maxSteps=512`、`IOR=1.33`、`numRefractions=4`、`extinction=(12,4,4)`、`renderScale=0.5`。本项目没有 vendor 上游代码。该历史对照后来促成独立 SPH vertical slice；当前 SPH 外观已经验收，**回到流体路线时**（平台刀由用户另定；Windows 7 GUI 已后移至 PS3 前），应先关闭四项正式计分 blocker；secondary spray 只能作为真实版本化模拟增强，不能用 fragment 装饰冒充。

### 3.2 源码真实状态

下表记录当前产品与仍需维护的高级 workload；兼容性选择不再属于产品重点或未来双卡计划：

| Workload id | 真实负载 | 后端代码状态 | 建议归类 |
|---|---|---|---|
| `stream` | 原始粒子 Euler 更新 + 点绘制；低算术强度，偏带宽 | Vulkan/DX12/DX11/OpenGL/**Metal（M4 Pro 15 秒 + `.gputrace` 已验收）** | **主界面：Particle (Original)** |
| `gpu_burn` | 当前公开合同为 Plasma×Kaleidoscope / `gpu_burn_v3_fixed_steps_*`（shaderVersion=3）：2 次全屏 opaque draw、固定 step FP32/SFU/INT；历史 faceted-glass 成绩组按 `workloadVersion` 隔离 | Vulkan/DX12/DX11/OpenGL；DX WARP；**Metal（M4 Pro 15 秒已验收）** | **主界面：GPU Burn**；`gpu_burn_v1` selector 保留历史 Plasma Bloom 身份 |
| `gpu_stress` | 独立 shader 的 4 次全屏 opaque overdraw；FP32/SFU/INT 循环，warmup 首秒自动标定到约 8 ms/draw-group | Vulkan/DX12/DX11/OpenGL；DX WARP；**Metal 仍明确 unsupported** | Other / Advanced：GraphicsBurn component |
| `nbody` | tiled all-pairs 粒子计算 | 五后端 | Other / Advanced Compute |
| `stress` | 全屏固定次数 fractal + `sin()`，fragment ALU/SFU | 五后端 | **Legacy Stress v1** |
| `synthpeak` | 寄存器内合成峰值循环 | 五后端，精度能力不同 | Other / Advanced Synthetic |
| `render3d` | 6 顶点实例化 billboard + 深度 | 五后端 | **Legacy 3D Prototype** |
| `volumetric` | 程序化 FBM 体积 raymarch | 五后端代码已接入，但仓库结果无验证记录 | Other / Experimental；未来综合场景 pass |
| `cinematic_liquid` | 3D MLS-MPM 或 `--liquid-solver sph` + 独立粒子 splat/binomial R32F 密度体积 + 最多 4 界面自由表面 ray path；GUI 文案为「流体 —— 互动水池」 | **正式合同仍以 Vulkan 为准**。Metal：320,920 粒子 MLS-MPM compute + raymarch present 已在 M4 Pro 6 秒通过，成绩强制 `…_metal_preview`；SPH 正式合同未完成。DX12 曾接线后撤回；DX11/OpenGL 未实现 | **主界面：Cinematic Liquid / 互动水池**；当前 v8、Metal preview 与 SPH preview 必须按 `workloadVersion` 分组 |

仓库内历史 `results/results.json` 仍为 232 条：`stream=224`、`nbody=5`、`stress=1`、`synthpeak=1`、`render3d=1`、`volumetric=0`。本轮 smoke 数据刻意写到 `out/*/results`，没有污染历史库。新结果 schema 为 v2，记录 workloadVersion、最终标定参数与 capture 状态；旧结果读取时仍按 schema v1 兼容。

公开 `gpu_burn` 当前成绩身份为可选手动步数的 `gpu_burn_v3_fixed_steps_<N>_kaleidoscope`（shaderVersion=3，Plasma×Kaleidoscope 视觉）；历史 faceted-glass / 自动标定结果按各自 `workloadVersion` 永久隔离。`gpu_burn_v1` selector 保留更早 Plasma Bloom 合同。Windows 上 Vulkan/DX12/DX11/OpenGL（含 WARP）可跑；**Metal 已在 M4 Pro 完成 15 秒验收**。RTX 5090 / Vulkan 历史标定数据（如 2048 steps → 13.199 ms render / 286 Gpix-step/s）仍有效，但不得改写身份。`gpu_stress_v1` 仍为 Advanced GraphicsBurn，**Metal unsupported**。液体：当前 MPM `physical_scene_v8` 尚无正式成绩；SPH 强制 `_preview`；Metal 仅为 `…_metal_preview`（raymarch 已在 M4 Pro 冒烟通过）。跨后端正式合同仍开放。

### 3.3 为什么现有新测试“效果不好”的判断成立

- 旧 `stress` 的固定循环适合可重复的 fragment ALU/SFU 微测试，但没有 compute+graphics 混合与错误校验，所以继续作为 Legacy Stress v1；新的 `gpu_stress` 没有复用或修改旧 fractal shader。
- `render3d` 只有相机朝向 billboard，没有 mesh、材质、纹理、灯光、阴影、环境光照或后处理，不能称为 3DMark 类场景（`shaders/render3d.vert:23-32`、`shaders/render3d.frag:8-12`）。
- `volumetric` 是可用的独立 raymarch 技术积木，但文档记录 RTX 5090 默认仅约 68% 利用率（`docs/TODO-volumetric-fluid.md:20`），不足以承担“绝对压力”主项。

### 3.4 CPU 补充测试（2026-07-16，Windows vertical slice 已跑通）

定位已锁定：CPU 只是 GPU 跑分项目的补充，不创建 3D 窗口，也不调用
RenderDoc。当前已同时提供原生 C++ CLI 与独立 WinUI CPU 页面：
`per-core` 按可用逻辑 CPU 逐个顺序测试并实时更新当前核心/总进度，
`multi` 同时使用全部可用逻辑 CPU，`all` 顺序执行两者；测量时长与预热
时长可调。逐核结果必须显示逻辑 CPU、物理核、SMT sibling 与可识别的
性能层级，另给逐核平均与多核结果，不能把用户给出的旧 Python EXE 分数
或算法当成本项目成绩合同。

**已完成且有证据的边界：**

- `cpu_mixed_v1` 使用原生 mixed integer/branch/FP32/FP64 依赖链、单调时钟、
  固定三轮中位数和 DCE checksum。checksum 不是 known-answer 正确性验证。
  正式合同固定为每项总测量 15.0 秒 + 预热 0.2 秒 + 三轮；其他时长均为
  `preview`。per-core 汇总是各逻辑处理器中位数的算术平均，multi 是全部
  逻辑处理器线程总吞吐的三轮中位数。
- 正式计分复核已关闭三类污染：所有 per-core 现在使用相同 seed/依赖轨迹；
  stdout 格式化/flush 完全移出被测线程；multi 测量窗口内完全不输出，只在
  轮次边界更新。multi worker 的热计数为线程局部并按 128-byte cacheline
  隔离，结束计时后才发布，避免 false sharing。
- CLI 已接 `--cpu-benchmark [per-core|multi|all]`、`--cpu-time`、
  `--cpu-warmup`、`--cpu-no-save`，并在 GLFW/GPU/API probe 前直接返回。
  `CPU_META`/`CPU_TOPOLOGY`/`CPU_PROGRESS`/`CPU_RESULT` 协议包含版本、
  formal/preview、round/median、affinity、valid、classification source；
  checksum 明标 `dce_sink`。
- Windows 优先 `GetSystemCpuSetInformation`，回退
  `GetLogicalProcessorInformationEx`；按 processor group 用
  `SetThreadGroupAffinity` 严格绑核并恢复。严格绑核失败会 `valid=0`、从汇总
  排除、CLI exit 3 且不保存。2026-07-16 的 Ryzen 7 9800X3D Release smoke
  正确枚举 16 logical / 8 physical / SMT2，16/16 逐核与 16/16 multi 均
  `affinity=strict`、exit 0。该 0.09 秒运行只证明路径闭环，不是正式成绩；
  >64 logical processor-group 与真实混合核机器仍未验收。
- WinUI CPU 页已有模式、Quick/Formal、时长/预热、逐核/总进度、逐逻辑核
  结果、汇总、raw output、Run/Cancel；它通过无窗口子进程读取上述协议。
  CPU/GPU/Charts 入口现在全局互斥；Formal 会同时恢复 15.0/0.2，任一值偏离
  即取消 Formal 标识；raw/progress 批量节流，并审计 meta/version、topology、
  逐核 summary 与 multi 完整性，不能把“exit 0 但协议截断”显示为完成。
  CLI 与 WinUI Release x64 均已构建；隔离临时数据目录的 0.1 秒 GUI E2E 已
  显示 16 条逐核、平均、多核、100%/Done 且恢复 Run。该运行只是 preview
  orchestration smoke。成功运行会把 per-core average 与 multi summary 分别
  追加到现有 `results.json`/History；详细逐逻辑核行只在本次页面/协议中。
- 持久化 `workloadVersion` 包含 kernel、affinity capability、formal/preview、
  round/time/warmup 以及 multi 是 standalone 还是 after-percore，避免 Quick、
  Formal、不同绑核能力和不同热状态顺序静默混排。

**仍开放的发布/跨平台边界：**

- 当前已有本机 CLI 与 WinUI build/smoke，不等于干净机安装验收已完成；公开安装包前须在干净 Windows 机器验证 Setup 安装后 GUI 能找到
  相邻 `gpu_benchmark.exe`、Run/Cancel、结果写入与卸载保留用户数据。
- 当前主发布产物是 **ZIP + 原生中英双语 Setup EXE**；x64/ARM64 各一个安装包且测项本体均为对应原生 ISA。WiX MSI 只作内嵌中间文件，Inno 仅保留为 legacy。
- Windows 的 `EfficiencyClass`、Linux/Android 的 `cpu_capacity`/最大频率只
  用来生成 `InferredPerformance/Efficiency/Middle/LPE` 排名标签，**不是**
  CPUID/SoC 官方核心身份，不能宣称已精确识别所有 P/E/Mid/LPE 变体。
- Linux/Android 会枚举 allowed cpuset，并要求 `pthread_setaffinity_np` 成功后
  回读为唯一目标 CPU；成功身份为独立的 `strict_sched_affinity`，失败会
  `valid=0`/exit 3。原生 Linux、容器/cpuset 与 Android 设备构建仍未实测。
  macOS 只能 `scheduler_managed`，logical→physical/SMT 是估计；各 affinity
  capability 已进入版本，不能与 Windows strict 成绩混排。
- Android/iOS 与 Web/WASM CPU 模式均未构建验收。浏览器只能看到暴露的
  logical concurrency，不能选择、硬绑定或可靠分类宿主核心；移动端还需独立
  热状态/前后台合同，禁止把当前源代码存在写成已完成端口。

### 3.5 WinUI 秒数输入样式（2026-07-17）

- 用户要求把当前横向内联上下箭头改成参考图中的竖向浮层效果。`DurationValueBox`、`CaptureValueBox`、`CpuTimeBox` 与 `CpuWarmupBox` 现统一使用 WinUI `NumberBox` 的 `SpinButtonPlacementMode="Compact"`；默认值、范围、步长与成绩合同未改变。
- Compact 交互补层现包括：Enter 提交后把焦点移到当前 GPU/CPU 导航项；GPU/CPU 滚动内容的外部点击会移走 NumberBox 焦点；× 或编辑产生 `NaN` 时立即恢复 `OldValue`（无合法旧值才取 Minimum），避免原生上下按钮进入全灰状态；`UpDownPopup` 追加轻微水平 `PopupThemeTransition`，不复制整份 WinUI 控件模板。
- 源码、独立输出目录以及正式 `gui/x64/Release` 目录的 Release x64 构建均已通过。按用户明确要求，本轮最终版本只改代码和重新编译，没有启动 GUI；逐控件视觉与手动交互仍由用户侧验收。

### 3.6 Original Particle（`stream_v1`）GB/s 的真实含义（2026-09-30 实测）

- **用户决定：暂不修改** `stream_v1` 的 shader、`bytesPerParticle=40` 公式与默认参数，以保持与历史成绩兼容。完整方法和数据见 [`docs/benchmark-workload-suite.md`](docs/benchmark-workload-suite.md) §6。
- **流量被低估：** 实际硬件流量约为每粒子 64 B，而不是 40 B。在 RTX 5090 上，原 kernel 与"整粒子读 32 + 写 32"对照 kernel 在 16M/32M/64M 下耗时相差不到 1.5%，v1 分数低估了 37.5%。按 64 B 计，5090 为 1347–1354 GB/s（理论值的 75%）；只读对照为 1503 GB/s（84%）。
- **跨厂商不公平：** AMD 核显（9800X3D iGPU）上，原 kernel 的耗时比整粒子读写多 35–43%，所以不能把旧成绩统一乘以 1.6 换算，v1 的 GB/s 对不同厂商并不公平。
- **默认 1M 测的是缓存：** 1M（32 MiB）装得进大容量 L2/Infinity Cache，5090 上折合约 3.2 TB/s，不是显存速度；≥ 16M 的结果才接近显存行为。
- **同卡跨驱动漂移：** RTX 5090 16M 的最快帧从 0.553 ms（驱动 591.86）变为 0.664 ms（驱动 616.92），原因未查。
- **对外表述：** 称为"粒子更新吞吐（v1，按 40 B 估算）"，不要称为显存带宽。GUI 的"Particle — Memory Throughput"和 CLI 的"VRAM rate"文案尚未修改，属于已知文案偏差。
- **后续：** 独立的 `membw_v1`（缓存层级带宽扫描）已在 2026-10-05 完成第一步（Vulkan 命令行版，见 §3.7）；`stream_v2`（整粒子读写，每粒子 64 B 确定值）仍暂缓，没有开工。

### 3.7 显存 / 缓存带宽测试 `membw_v1`（2026-10-05，Vulkan 命令行版）

- **状态：** 代码已写，Windows 与 Linux 可编译，已在 RTX 5090、AMD 核显和 lavapipe 上实际运行。只支持 Vulkan。命令行完整可用；WinUI 的 GPU 运行页、History 和结果区曲线图已接入（2026-10-05）；其他四个 API 和 macOS / 移动端前端没有接入。
- **实现：** `src/vulkan_membw.{h,cpp}` 加四个 shader（`shaders/membw_{read,write,copy,l1}.comp`）。它是独立路径，自己创建 Vulkan 设备，只做计算；`Workload` 枚举、五个后端和粒子测试都没有改动。入口是 `--workload membw`，选项为 `--membw-max-mib` 和 `--membw-no-save`。Android/iOS 构建不编译这个模块（由 `HAVE_VULKAN_MEMBW` 控制）。
- **方法：** 每个工作组读写一块 512 KiB 数据，用数据块个数控制数据量（1 MiB 到 2 GiB）；每次派发单独提交并用 GPU 时间戳计时；每个点预热 3 次、测 10 次取中位数。完整说明见 [`docs/cli-reference.md`](docs/cli-reference.md)。
- **成绩合同：** workload 为 `membw`，分数是只读显存平台（GB/s）。默认扫描且到达显存平台时版本为 `membw_v1`，否则为 `membw_v1_preview`。与 `stream` 不同组。
- **RTX 5090 实测**（驱动 616.92，GPU 空闲）：显存读 1536 GB/s，为理论值的 86%；写约 1355 GB/s；复制约 1345 GB/s；L2 平台约 6.7 TB/s（3–96 MiB）。
- **L1 测试（同日追加）：** 每个工作组反复读自己私有的一块数据（16 KiB 到 1 MiB），分别在全部 SM/CU 满载和只有一个工作组两种情况下测。RTX 5090 满载时约 55 TB/s，单个 SM 约 287 GB/s，L1 容量在 128–192 KiB 之间。AMD 核显单个 CU 的第一级缓存在 16–24 KiB 之间，之后在 64–96 KiB 和 256–384 KiB 还有两次下降。kernel 用一个共享变量加 `barrier()` 把工作组固定在一个 SM/CU 上，否则 NVIDIA 驱动会把它的 warp 分散到多个 SM。
- **系统内存测试（同日追加，仅独显）：** 只读和只写 kernel 在系统内存缓冲区上再跑一遍，并用 `vkCmdCopyBuffer` 双向复制；每个字节都经过 PCIe。RTX 5090 测得读 13.1 GB/s、写 13.0 GB/s、驱动复制 12.0 GB/s。**这张卡当前的链路是 PCIe 5.0 x4**（`nvidia-smi` 与 Windows 设备属性一致，最大 x16），单方向理论值 15.75 GB/s，实测为 83%。x4 的原因没有查。
- **已知限制：**
  - 不做理论值校验，因为 Vulkan 不提供显存频率和位宽；
  - 缓存平台的终点不是精确容量，并发工作组共享数据块会把过渡段拉宽；
  - 几 MiB 以下的数据量受 L1 影响，不算 L2 成绩；
  - L1 读速率同时受指令执行速率限制，是这个 kernel 能达到的速率，不是硬件上限；汇总只报告单工作组曲线的第一次下降；
  - 测试时 GPU 必须空闲。2026-10-05 第一遍测试时 Edge 占用了约 65% 的 GPU，那一遍数据作废。
- **GUI 接入：** 下拉框新增 "Memory Bandwidth — VRAM / Cache / PCIe"，选中后只列 Vulkan 并禁用不适用的选项；结果区显示显存、缓存平台、L1、PCIe 四行汇总；History 显示正式名称和 "Compute" 模式。GUI 打开时测得的显存读带宽比纯命令行低约 5%。
- **GUI 曲线图：** 结果区在汇总下方画出最多三张双对数折线图（显存与缓存、经 PCIe 的系统内存、L1），用 XAML Canvas 直接绘制，支持明暗主题和三种语言，按宽度自动分列。只显示最近一次运行，不能从 History 重画。
- **剩余步骤：** 见 `docs/TODO.md`（延迟测试、其他 API、从 History 重画曲线、外部对照）。

### 3.8 CPU 缓存 / 内存带宽测试 `cpu_membw_v1`（2026-10-06）

- **状态：** 代码已写；Windows（MSVC）和 Linux（GCC 9）可编译，aarch64 交叉编译通过；已在 Ryzen 7 9800X3D 上实际运行（Windows 11 和 WSL2 里的 Ubuntu 20.04 容器）。命令行完整可用；WinUI 的 CPU 页、History 和曲线图已接入。macOS、Windows ARM64、真实 ARM 设备没有运行过。
- **实现：** `src/cpu_membw.{h,cpp}`，入口 `--cpu-membw`，选项为 `--cpu-membw-max-mib`、`--cpu-membw-vector` 和 `--cpu-membw-no-save`。它在 GPU 探测之前返回，不需要 GPU。Android/iOS 构建不编译这个模块（由 `HAVE_CPU_MEMBW` 控制）。原有的 `cpu_benchmark.cpp` 没有改动，只复用了它的 `ProbeCpuTopology()`。
- **方法：** 读、写、复制三种内核，数据量从 4 KiB 扫到 1 GiB；先单线程，再每个物理核心一个线程。每个点测 7 个 10 ms 的样本取中位数。缓存容量从操作系统读取，用来把曲线上的点归到 L1–L4 和内存。完整说明见 [`docs/cli-reference.md`](docs/cli-reference.md)。
- **成绩合同：** workload 为 `cpu_membw`，分数是全核内存读带宽（GB/s）。默认扫描且内存平台稳定时版本为 `cpu_membw_v1`，否则为 `cpu_membw_v1_preview`。
- **9800X3D 实测**（2×48 GB DDR5-6400，AVX-512）：
  - 单线程读：L1 588 GB/s，L2 240 GB/s，L3 136 GB/s，内存 55–61 GB/s；
  - 全核（8 线程）内存：读 54–55 GB/s，写 27 GB/s，复制 40 GB/s；
  - 内存读为理论值 102.4 GB/s 的 54%；这台机器没有 L4。
- **虚拟内存：** 测的是操作系统映射新页面的速度，Windows 11 上约 15 GB/s（每个 4 KiB 页约 270 ns）。页面文件的读写速度没有测。
- **两个没有查清的现象：**
  - 流式写入只有 30 GB/s，和普通写入差不多，而 `CLZERO` 清零能到 75 GB/s；独立小程序确认 `memset` 和 `rep stosb` 也是 30 GB/s，所以不是测试内核的问题。
  - L2 曲线分两段：64–384 KiB 为 230–245 GB/s，512–768 KiB 为 300–320 GB/s。改变分片大小后结果不变，所以是硬件行为。
- **已知限制：**
  - 缓存级的速度取决于向量宽度和主频，不同 CPU 只能在同一向量宽度下比较；
  - 大小核 CPU 的分层只按参考核心的缓存来算；
  - 测试时机器必须空闲。2026-10-06 有一遍 GUI 测试和容器编译同时进行，缓存级成绩低了四到五成，那一遍作废。
- **大小核、SoC 缓存、存储（2026-10-06 追加）：**
  - 核心按性能等级分类，每类各做一遍单线程扫描（模式名 `single`、`single_1`…）；
  - 系统不报告缓存容量时从曲线平台识别各级缓存，也用来找系统没报告的缓存；
  - 新增存储读写测试（顺序 1 MiB、随机 4 KiB，不经过系统缓存）；
  - 模块改为所有平台都编译；`src/cpumem_cli.cpp` 加 `scripts/build-android-cpumem.ps1` 可编出通过 adb 运行的 Android 独立程序。
  - **没有在任何手机或大小核 CPU 上实际运行过。**
- **Android / iOS / macOS App 接入（2026-10-06 追加）：** 三个前端的 CPU 页都有了这项测试的入口、表格和曲线图；Android 编译通过，iOS 和 macOS 只做过类型检查，三端都没有在设备上运行过。Apple 平台不能绑定线程，只测系统挑选的核心。见第 10 节验证记录。
- **剩余步骤：** 见 `docs/TODO.md`。

### 3.9 成绩文件 `results.json` 的前后兼容（2026-10-06）

- **状态：** 代码已写。Windows 和 Linux 容器上实际运行并通过测试；用 0.1.0、0.2.5、0.2.6 三个旧版本的程序实测过和新版本轮流读写同一个文件。Android 和 HarmonyOS 只编译通过，macOS 只编出目标文件，iOS 没有编译。
- **文件格式没有变：** 仍是一个 JSON 数组，每条成绩一个对象，每个字段占一行；`resultSchemaVersion` 仍是 4。
- **规则（改成绩字段时必须遵守）：**
  - 字段只增不改：不改名、不删除、不改类型、不改含义；新信息用新字段，默认值表示"没有记录"；
  - 新字段加进 `kFields` 表（`src/benchmark_results.cpp`），读、写和字段在文件里的顺序都来自这张表；
  - 只加字段时不提高 `resultSchemaVersion`，只有已有字段的含义变了才提高；
  - 文件的整体布局不换（例如不在外面再包一层对象）；真要换格式就换一个文件名；
  - 引擎是唯一写这个文件的程序。前端只读；Android 和 HarmonyOS 的"全部清除"是例外，它们写入一个空数组。
- **读旧版本写的文件：** 缺的字段取默认值，和原来的读法逐字段一致。
- **读更新版本写的文件：** 不认识的字段原样保留在 `BenchmarkResult::unknownFields` 里，重写文件时原样写回；`resultSchemaVersion` 按读到的值写回。
- **旧版本读这个版本写的文件：** 0.2.6 及更早的版本靠数花括号切分成绩、按带引号的字段名查找，并且要求每个字段占一行。所以写文件时保持原来的布局，并把三种文字写成 `\u` 转义：字符串里的花括号、字符串结尾的反斜杠、内容恰好等于某个字段名的值。
- **保护数据：**
  - 保存时先写临时文件（`results.json.tmp`），再整体替换。读的一方不会看到写了一半的文件，中途崩溃也不会损坏原文件。
  - 文件读不全（被截断、损坏，或是这个版本不认识的布局）时，能读的成绩照常读出；重写之前先把原文件另存为 `results.json.bak-unreadable-<时间>`，另存失败就不保存。
  - 字段的类型不对（数字写成了字符串、`null`、`nan`）时取最接近的值，不会报错中断。非有限的数写成 0，控制字符写成转义。
- **已知限制：**
  - 0.2.6 及更早的版本，以及 2026-10-06 之前编出的开发版，重写文件时会丢掉它们不认识的字段，包括机型和内存。旧程序改不了，只能避免用旧版本删改成绩。
  - 这些旧版本显示带 `\u` 转义的文字时会把转义符原样显示出来（例如 `\u007b`），数值不受影响；文件被它们重写之后，这段文字会一直带着转义符。
  - 更新的版本如果在未知字段里写了带花括号字符串的嵌套对象，旧版本仍然会读错。以后的版本要继续遵守上面的写法。
  - 两个程序同时保存时，后保存的会覆盖先保存的那一条；没有加跨进程的锁。
- **测试：** `tests/results_compat_test.cpp`，CMake 目标 `results_compat_test`，153 条检查，不需要 GPU。里面有一份 0.2.6 读法的原样拷贝，用来证明旧版本读得了新文件；不要去"修"那份拷贝。

## 4. P0 正确性阻塞项

GPU Burn `gpu_burn_v2_mangekyo_faceted_glass_v1` 已作为带版本的新主项接入；`gpu_burn_v1` 只作为公开 legacy 保留，两者均不与 `gpu_stress_v1` 或旧 `stress` 混分。当前液体 P0 只以 `physical_scene_v8` / SPH preview 的正确性清单为准。

### 4.1 “全部 GPU × API + 第 5 秒抓帧”目前不成立

- ~~`--run-all --capture 5` 丢失 `captureAtSec`。~~ **已修复**：run-all 复制 capture、GPU Burn/GraphicsBurn 参数与 autotune 配置；任一矩阵项失败时现在返回非零，GUI 不再把部分成功冒充 Full Analysis 完成。
- ~~Windows OpenGL 会给每张硬件 GPU 建重复/错标项。~~ **已修复**：probe 只把 OpenGL 能力赋给 renderer 名称匹配的 adapter；2026-07-15 `gpu_burn --run-all` 实测只产生一条 RTX 5090 OpenGL 记录。
- 软件设备自动 backend 与显式 DX11/DX12 现在都正确进入 WARP，并清空硬件 LUID/VRAM 元数据；统一 capability registry 仍待实现。

### 4.2 15 秒的当前语义

- 默认是墙钟 15 秒，前 2 秒 warmup（`src/gpu_common.h:215-216`）。
- 第 5 秒从整个 run 开始计时（`src/app_base.cpp:318-349`）。
- 正式 FPS/score 统计从第 2 秒后开始，所以约统计 13 秒（`src/app_base.cpp:407-420`）。
- ~~RenderDoc 抓取帧进入统计。~~ **已修复**：抓帧尝试的墙钟开销、当前帧及随后至少 16 个异步 timestamp samples（取 `max(16, framesInFlight + 1)`）均从正式统计排除；这覆盖当前最大的 DX11 8-slot 与 OpenGL 4-slot query ring。结果分别记录 `captureAttempts`、成功的 `captureCount` 和“这次尝试是否已排除计分”的 `captureExcluded`；即使 `EndFrameCapture` 失败也不会污染成绩，但不会误报生成了 `.rdc`。

Windows 打包版 Vulkan 另有一个已修复的关键点：单独复制
`renderdoc.dll` 能取得 API，却不能自动成为 Vulkan capture layer。CLI 现在在 GPU
probe 前把随包 `tools/RenderDoc` 设为进程级 `VK_IMPLICIT_LAYER_PATH` 并启用
`renderdoc.json`，不写系统注册表；最终 staged 目录已实测四个 Windows API 均在
第 5 秒生成 `.rdc`。提权运行时 Vulkan Loader 会忽略这类环境路径，因此产品仍应
保持 `asInvoker`。

为了保留旧粒子成绩语义，`stream` 暂时保持“总墙钟 15 秒 / 第 5 秒抓帧 / 2 秒后开始统计”。若将来改成“2 秒预热 + 15 秒正式窗口”，必须增加 suite/result schema version，不能与旧分数混排。

### 4.3 Cinematic Liquid：当前 v8/SPH 尚未正式计分

已完成并验证的 Vulkan 切片：

- [x] 固定 128x64x96 MLS-MPM 网格、10 substeps 与 320,920 粒子；粒子分解为 142x14x98 的浅水底（194,824）和 48x37x71 的溃坝体（126,096）。当前固定参数为 stiffness 45,000、viscosity 0.035、maxSpeed 8。
- [x] 用户鸭子和七刚体 ABI 保留：船 index 2、沉球 index 3、小鸭 index 4-6。船由硬锚定改为 34 kg 有限质量/软系泊，推进器反冲可驱动和摇摆，但轨迹尚未视觉验收。沉球保持 1.06 水密度比、4.28 秒释放、-9.81 重力、0.015 空气阻尼；材料水阻按 displaced mass 与浸没率门控。
- [x] 当前池壁为有限高度、inset 0.45、wall-top fraction 0.42 的内嵌碰撞体；有限宽 outer catch band 允许真实越沿和落地，但不是无限流体域。render push 的 `pool.z` 已对齐真实 ground/pool-floor `y=0`，PVC bottom、liner、无限草地与物理粒子底面共面；pool ring separation 由 `2*tube` 推导，避免视觉池沿与物理地面漂移。
- [x] 当前表面不直接读取 MLS-MPM 网格质量：粒子以 Spiky² 核和 fixed-u32 原子独立 splat 到 128x64x96 R32F 密度体积，5x5x5 binomial resolve 会自适应保留低支持喷滴。沉球入水 crown/whitewater 来自真实 GPU 刚体状态与局部流体，不是 fragment 假水花；没有 secondary-spray 粒子 pass。
- [x] 当前 renderer 保留最多 4 次水/空气界面的 Fresnel/Snell、Beer–Lambert 与 opaque depth sorting，并使用 `extinction=(12,3.6,2.5)`、linear exposure 和 density 边界归零。新增独立透明软 PVC 前景薄膜（IOR 1.50、Fresnel、弱吸收、wrinkle），但不是完整 PVC 多介质 ray path；环境为无限程序化草地与大气天空/云，没有 cubemap 资产。历史 v6 的 `(30,10,8)` metadata 不回写。
- [x] 结果身份严格隔离：历史正式 `cinematic_liquid_v2_surface_splat_optics_v4` / `shaderVersion=6`；鸭群预览 `cinematic_liquid_v2_duck_family_v5` / `shaderVersion=7`；iterative optics v6 预览 / `shaderVersion=8`；历史 physical-scene v7 / `sceneVersion=4`；当前 `cinematic_liquid_v2_physical_scene_v8` / `shaderVersion=9` / `sceneVersion=5`；SPH 始终 `cinematic_liquid_sph_slice_v1_preview`。
- [x] RTX 5090 / Vulkan / 正式墙钟 15 秒 + 5.1 秒 RenderDoc 已通过：结果 id `20260715-170629-492`，Compute 10.572 ms、Render 1.553 ms、Total 12.125 ms、`263.98 MParticle-step/s`、966 measured frames；capture 0.103 秒已排除，`captureAttempts=1`、成功保存 1 个。最终核验图为 `rdoc_captures/cinematic-liquid-v2-5s-formal-optics-v4.png`。
- [x] 上一条只验证 optics_v4。历史 v6 的 6 秒短预览为 `20260715-221447-024`：Compute 9.450 ms、Render 3.003 ms、Total 12.454 ms、`257.01 MParticle-step/s`；它属于 v6 `_preview`，不是正式成绩，更不能归到 v7。
- [x] 历史 v7 的 shader、最终 6 个 SPIR-V、CLI Release 与 WinUI Release x64 build 均通过；只运行若干 `--time 6/8` 自动停止 smoke，无正式 v7 成绩。当前 SPH 已实际完整显示 15 秒并获用户视觉接受，但该运行仍是 preview，不是正式成绩。

视觉结论必须诚实：当前 SPH 外观已获用户接受，视觉迭代收口；这不代表正式 benchmark 完成。secondary spray/foam 与 SPH propeller wake 仍是后续增强，光追也只改善反射/阴影/焦散。当前计分 blocker 是渲染帧驱动时间、每 substep 冲量清零、viscosity 原位 race、atomic scatter 顺序四项，而不是继续调水色。

仍开放、因此**不得把整个 v2 标为完成或跨 API 完成**的 P0：

- [ ] 为 `cinematic_liquid_v2_physical_scene_v8` 运行正式 15 秒 + 第 5 秒 RenderDoc 并建立独立正式结果；在完成前不得引用 optics_v4 的 `263.98 MParticle-step/s` 或 v6/v7 的值作为 v8 成绩。
- [ ] SPH 先关闭四项正确性 blocker；在此之前任何时长都必须 `_preview`。关闭后再运行正式 15 秒 + 第 5 秒 RenderDoc/timestamp/确定性验收。
- [ ] 用当前 v8/SPH build 实机验收 WinUI 的 workload/solver 选择、Vulkan-only 能力限制、启动/结束与 History/Charts 的 `workloadVersion` 分组。
- [ ] 读取/记录船、沉球和越沿粒子的精确 GPU 状态轨迹，并用可靠新 capture 验收船运动、1G 下落/入水减速、局部 crown/whitewater、池壁/PVC 和室外环境；代码路径与自动 smoke 不能代替数值/视觉验收。
- [ ] 当前固定 dt/每个渲染帧推进固定 substeps 会把 GPU 帧率反馈进 5 秒场景状态。必须冻结跨 GPU 的时间推进/补步/丢步策略与第 5 秒 trajectory contract，否则同一 workload 在快慢 GPU 上可能捕获不同模拟时刻。
- [ ] 审计 Vulkan timestamp 起止点，确保 Compute/Render/Total 只覆盖合同规定的 P2G/G2P、surface clear/splat/blur/resolve 和 raymarch 边界，不漏计新增 pass，也不把 present/CPU 等非 GPU 工作混进分项。
- [ ] 补齐初始化失败、surface/swapchain 异常、device-lost 和提前退出路径的 surface atomic buffer、R32F image、descriptor、pipeline/layout 等资源释放；正常退出不泄漏不能替代异常路径审计。
- [ ] DX12、DX11、OpenGL 与 Metal 尚未实现 cinematic liquid v2。必须在 Vulkan scene/pass/quality/trajectory contract 冻结后逐后端移植并实机验证；当前不得显示为 supported，也不得静默 fallback。

## 5. GUI 与文档同步状态

### Windows WinUI 3

- **本地开发默认构建（2026-07-21）**：`scripts/build-windows.ps1` 一次编齐 CMake CLI + WinUI GUI（`-SkipGui` 才只要 CLI）。GUI 的 `CopyGpuBenchmarkWorker` 会把 `gpu_benchmark.exe` **以及** HLSL / SPIR-V / OpenGL 着色器资产拷到 `gui/<Arch>/<Config>/`，避免同目录 worker 因缺 `compute.hlsl` 等文件在 DX/OpenGL 初始化失败。正式发布仍走 `build-windows-github-release.ps1` / `stage-windows-release.ps1`，与开发树分离。
- 下拉框当前仍包含兼容性 workload；参数映射、score parser 与 History label/filter 覆盖对应 id。当前产品验收与后续双卡工作只围绕 Original Particle、Plasma/GPU Burn、当前 Cinematic Liquid。
- GPU Burn 主项为 `Mangekyo Kaleidoscope — GPU Burn`，Plasma Bloom v1 收入 legacy；`gpu_stress` 归入 Advanced，旧 `stress`、`render3d` 明确标 Legacy，volumetric 标 Experimental。
- Custom 与 Quick 的非 headless 运行现在默认追加 `--capture 5`；默认 duration 仍为 15 秒。Full analysis / Flights / Particle 预设也继续抓帧。
- installed/staged 布局优先使用同目录 CLI/资产；开发树 GUI 同样优先同目录 `gpu_benchmark.exe`（现须连同 shader 一起由构建同步）。若不存在外部 CLI，静态 engine 以 GUI module 目录运行，不再强制切到 repo CWD。
- Full Analysis 优先调用随包 `tools/RenderDoc/renderdoccmd.exe`；报告输出改到用户数据目录。若缺 Python/冻结 report worker 或任一后处理命令失败，GUI 现在显示“benchmark 已完成、报告不可用/失败”，不再误报图表与报告已成功生成。
- Charts 页（2026-10-06 起）由 GUI 读取保存的成绩后自己绘制，不再调用 Python：每项测试一张条形图，只比较同一 workloadVersion（取最新的），按设备 × API 分组。GPU 页和 CPU 页在每次运行结束后也会画出该次运行的图表。Full Analysis 的报告链仍调用 Python 脚本。
- Release x64 已用最终 v2 `gpu_engine.lib` 重编并成功（0 error，2 个既有 duplicate WinAppSDK warning）。
- 当前 v8/SPH 的 WinUI 仍须实际完成 workload/solver 选择、Vulkan-only 限制、完整 run 和 History/Charts 当前/历史版本分组验收；不得把“可编译”或 SPH 独立 CLI 视觉运行写成 GUI 全流程通过。

### macOS / iOS / HarmonyOS

- **OS 底线 macOS 12 Monterey**（见 §3.0.2）：CMake + SwiftUI deployment 12.0；壳层/玻璃按系统版本回退；功能与成绩合同不按 OS 拆分。
- macOS SwiftUI 已完成平台等价工作流（Run/CPU/History/Duration/Capture/API 能力、Complete Suite、Fill Missing、原生 Charts、结构化错误摘要），并在 M4 Pro 编译、链接、独立启动；仍待 **实际 Monterey 冒烟**。细节见 `macos-gui/README.md`、`docs/macos-notes.md`。
- **iOS：代码已写 / 本机未编译 / 未设备验收**（见 §3.0.3）。`ios-gui/` + `ios_engine_host` 存在；`CollectResult` 追加 `_ios_preview`。用户已锁定 **最低 iOS 16** + **iOS 26/27 = Liquid Glass**（16–25 Material）。共享 `GlassCard` 已预留 `iOS 26.0`。
- **2026-10-06：** macOS 和 iOS 的 CPU 页接入缓存 / 内存带宽测试，GPU 页、CPU 页和 Charts 页的图表改为 SwiftUI 自己绘制；同时修掉了 `BenchEngine.swift` 里一处无法编译的写法和会让界面卡死的阻塞调用。只在 Linux 容器里做过类型检查，**仍未用 Xcode 编译**。见第 10 节验证记录。
- **HarmonyOS（2026-10-06）：** 原来的独立粒子演示已换成接入共享引擎的 App，四个页面与安卓版对齐；用真实工具链编译通过，**没有运行过**。没有流体测试、15 秒结果合同和抓帧编排。见 `ohos/README.zh-CN.md`。

### 已知文档漂移

| 文件 | 主要漂移 |
|---|---|
| `README.md` / `docs/building.md` / `docs/cli-reference.md` / `gui/README.md` / `packaging/README.md` / `packaging/PACKAGE_LIMITATIONS.md` / `installer/README.md` / `docs/renderdoc-capture-guide.md` / `docs/report.md` / `docs/TODO.md` / `macos-gui/README.md` | **2026-07-21 已同步**：Windows 开发默认 `scripts/build-windows.ps1`（CLI+GUI+同目录 shader）；发布链仍为 github-release/stage |
| `docs/cli-reference.md` | 已同步 12 个 CLI/GUI 选择及两代 GPU Burn selector/结果分组 |
| `docs/roadmap.md` | 历史内容仍有漂移；结果真实默认路径现已改为平台用户数据目录 |
| `docs/benchmark-workload-suite.md` | “canonical enum” 仍只有最初 4 项 |
| `docs/TODO.md` | 多项报告/RenderDoc 数据任务与仓库现有大报告、抓帧 JSON 状态不一致 |
| `docs/TODO-volumetric-fluid.md` | 最接近 HEAD，但遗漏 binding 2/3、静默非 Vulkan fallback 和 fragment barrier 问题 |

## 6. 目标 A 的推荐实现路线

### A0 — 先恢复可信度

- [ ] 增加共享 `TestDescriptor/WorkloadMetadata`：稳定 id、分类、状态、支持后端、参数 schema、score 单位、headless/capture 能力、默认 `duration=15`、`captureAt=5`。
- [x] Windows GUI 已把 fluid 标成 developer-only；非 Vulkan fluid 与 Metal GraphicsBurn（`gpu_stress`）明确 error，不再 fallback。**Metal GPU Burn 已改为可跑路径（待 Mac 验收）**，不再写死 unsupported。
- [ ] 修复 fluid binding、真实 ping-pong、同步和 per-frame descriptor；加 validation-layer 单帧 smoke test。
- [x] run-all capture、capture 文件名 workload 与 Windows OpenGL 重复/错标已修；低强度 9 项矩阵通过。
- [ ] 捕获帧排除评分、result schema/version/config/captureCount 已做；app build hash 与统一 quality metadata 仍待补。
- [x] CLI 比较表按 workloadVersion/scoreUnit/precision 分组，优先 stableScore/score，禁止不同轴按 FPS 混排；图表也只取同版本。

### A1 — GUI 信息架构

- [ ] 主界面只展示 `Particle (Original)`、`GPU Burn`、`Cinematic Fluid`。
- [ ] `Other / Advanced / Legacy` 展示 NBody、SynthPeak、Legacy Stress v1、Legacy 3D、Volumetric 和 Developer Fluid。
- [ ] Flights、particle count、host memory、headless 是运行变体/诊断参数，不再伪装成 workload。
- [ ] CLI、WinUI、macOS、图表和报告从同一个 registry 生成显示与参数。

### A2 — GPU Burn (15s Burst)

- [ ] `CoreBurn`：寄存器/共享内存驻留的 FP32 FMA 热循环，少量全局访存，checksum 防止优化并做确定性错误检测。
- [x] `VisualBurn v2 / Mangekyo faceted glass`：独立跨 API fragment shader、透视 3D 截顶宝石/八面体碎钻、前后层视差、SDF 平面法线、Fresnel 反射、RGB 折射色散、2 次 opaque draw、固定 step FP32/SFU/INT、warmup 自动标定、版本化 `Gpix-step/s`；Plasma Bloom v1 以公开 legacy selector 与独立结果身份保留。
- [x] `GraphicsBurn v1`：原 `gpu_stress_v1` 保留为 Advanced component；4 次 opaque overdraw、FP32/SFU/INT、版本化 `Gpix-iter/s`。
- [ ] `MixedBurn`：同帧 compute + graphics，覆盖 core、SFU、texture/fill/ROP；按设备自动校准到短而可取消的 dispatch/draw，单块工作不要逼近 Windows TDR。
- [ ] 三个分项分别报告，不宣传某一个 shader 对所有 GPU 都是“绝对最热”。

### A3 — Cinematic Liquid

- [x] 当前 `physical_scene_v8` 使用固定 128x64x96 MLS-MPM 网格、320,920 粒子、10 substeps、密度体积重建、迭代自由表面光学与版本化结果合同。
- [ ] 关闭当前 v8 正式 15 秒/抓帧验收、SPH 正确性、刚体轨迹和资源清理 blocker。
- [ ] 建立跨后端 scene/pass registry；能力不足必须明确 unsupported，不能静默 fallback 或跨 API 混分。
- [ ] 双卡只评估同一固定液体 workload 的模拟/渲染流水线；不做重复模拟、简单 AFR 或域分解。

## 7. 目标 B 的当前阻塞与发布路线

### 7.1 已完成的第一阶段与剩余阻塞

本轮已建立并实际执行 Windows x64/ARM64 GitHub Release 候选链：CMake
`install()`/CPack、固定 `vcpkg.json` baseline、CMakePresets、staging verifier、
WinUI self-contained payload、MSVC runtime、GLFW、全部预编译 shader、GLAD
2.0.8 in-tree、完整官方 RenderDoc 1.45 portable（x64 常规捆绑）、**x64/ARM64 原生双语 Setup**、
逐文件 SHA-256 与 ZIP 解包复核。目标机运行核心 benchmark/GUI/抓帧不需要 VS、
vcpkg、Vulkan SDK、shader compiler、单独的 Windows App SDK 或预装 RenderDoc。
Inno Setup 仍保留为 legacy 工程路径，**不再是** `build-windows-github-release.ps1`
的默认安装器。

PathService 已把 results/captures/reports/logs 改到
`%LOCALAPPDATA%/GpuComputeBenchmark`（可用 `GPU_BENCH_DATA_DIR` 覆盖），并一次性
迁移旧相对 `results/results.json`。GUI/CLI 与 RenderDoc 能从 staged 布局运行。

已生成可供换机验收的 ZIP 与原生 Setup EXE，但仍不能宣称“已经公开发布/完全验收”，原因是：

- CLI/GUI 已把 `vulkan-1.dll` 改为 delay-import，probe 与显式 Vulkan backend 创建前都有 loader guard；本机构建和 PE 审计通过。仍须在真正没有 `vulkan-1.dll` 的干净机确认 DX11/DX12/WARP 启动。
- 报告链仍只有 `.py` 源码，没有冻结的 `report_worker.exe`；核心 benchmark/GUI 不需要 Python，但自动报告仍需要开发环境。打包规则虽预留 `tools/report_worker`，GUI 尚未实现调用该 worker 的 argv 协议。
- **项目分发 LICENSE 已订为 MIT**（仓库根 `LICENSE`，staged `licenses/LICENSE`，`projectDistributionLicense=true`）。CPack WiX 需要 `.txt`/`.rtf`，构建时会复制为 `cpack-LICENSE.txt`。
- 当前 Setup EXE、内嵌 MSI、GUI 与 CLI 均未做 Authenticode 签名；Release asset manifest 会记录 `NotSigned`，公开下载有 SmartScreen 风险。
- 尚未在真正干净 Windows VM 上验证 GUI 启动、无 RenderDoc/VC Redist/SDK 环境下的运行与升级/卸载。
- GUI 与 CLI 均已有 **x64 与 ARM64 原生**产物；勿再写成“GUI 只有 x64”。

### 7.2 推荐发布架构（当前 Windows x64 基线）

1. **路径产品化（第一阶段已做）**：统一 PathService；可写数据放 `%LOCALAPPDATA%/GpuComputeBenchmark/{results,captures,reports,logs}`；staged GUI/CLI 不依赖 repo CWD。
2. **进程隔离**：GUI 是唯一 orchestrator，每个 API×GPU 启动一个 worker；device loss、driver crash 和 RenderDoc 注入不拖垮 GUI。
3. **锁定构建（部分已做）**：已有 `vcpkg.json` baseline、CMakePresets、strict shader asset gate 与 in-tree GLAD；NuGet lock、完整 shader manifest/CI 仍待补。
4. **预编译资产（已做）**：发布 SPIR-V、HLSL/GLSL 与 DX12 FP16 DXIL；用户机器不装 SDK/compiler。
5. **报告 worker**：把 Python 报告链冻结为随包的 `report_worker`（嵌入 Python或独立 onedir），主程序只用绝对路径/argv 调用，不查 PATH、不拼 shell。
6. **安装器（当前主路径）**：`scripts/build-wix-installer.ps1` 先生成原生架构 MSI 中间文件，`scripts/build-native-bootstrapper.ps1` 再生成对应架构的单文件中英双语 Setup EXE，并把界面选择的路径传给 MSI `INSTALL_ROOT`。正式产物名为 `Mangekyo-<ver>-windows-{x64,arm64}-setup.exe`；`build-windows-github-release.ps1` 已串联整套流程且不再发布 raw MSI。Inno（`installer/GpuComputeBenchmark.iss`）保留作 legacy。完整操作见 `docs/windows-installer-packaging.md`。
7. **RenderDoc 可选组件（构建机 staged 已验证）**：随包放完整官方 1.45 bundle 到 `{app}/tools/RenderDoc`，archive SHA-256 `bd665c348a8245d10a1f513e35b83603edc1a78006277583d09ec0769286eea4`；in-app API 在第 5 秒包住一帧，Vulkan 使用进程级 implicit-layer path。仍需 clean-machine 抓帧与 credits/签名审计。
8. **Vulkan loader（代码与 PE 已收口）**：SDK 只用于构建；Windows 运行时 delay-load 并探测 loader/ICD。缺 Vulkan 时隐藏该能力；若显式请求 Vulkan则返回可读错误，DX11/DX12/WARP 仍可启动。不随软件安装 GPU 驱动或复制系统 loader。

### 7.3 Fresh-machine 验收门槛

- [ ] Windows 10/11 干净 VM：无 VS、vcpkg、Python、Vulkan SDK、RenderDoc、VC Redist；离线安装后 GUI 能启动。
- [ ] Windows ARM64 干净实机/VM：安装前无开发环境或独立 RenderDoc；本体、GUI、全部原生依赖和可选抓帧组件通过 PE 架构清单核对，安装后完成 GPU/CPU smoke、结果保存、升级/卸载与数据保留。任何 x64/ARM64EC 组件均在 manifest 和 UI 中显式标明。
- [ ] Windows 7 SP1 干净实机/VM：独立 Win32 GUI 与安装包启动；分别验收 Aero 开/关、Basic/经典主题、DWM composition 关闭与远程桌面 fallback，不依赖 WinUI/WinAppSDK；GT 120 另做 DX11 FL10/SM4 实卡流程。
- [ ] 无 Vulkan loader 时仍可跑 DX11 WARP 的 15 秒流程并保存结果。
- [ ] AMD/NVIDIA/Intel 实机：每个受支持 API/GPU 精确 15 秒，第 5 秒产生 `.rdc`，转换与报告成功。
- [ ] 安装路径、用户名含空格与中文；普通用户运行，不向 `{app}` 写结果。
- [ ] 安装/升级/卸载通过；默认保留用户结果，另提供显式“删除数据”。
- [ ] 依赖扫描无漏 DLL；发布带 hashes、SBOM、LICENSE 与 THIRD_PARTY_NOTICES。

### 7.4 DirectX 10 时代 GPU / GT 120

- **决定：不实现 DX9 后端。** GT 120 已能由 D3D11 runtime 创建 FL10_0 设备；DX9 没有 compute/UAV，无法忠实承载 Stream/N-body/SynthPeak，且 RenderDoc 不支持 D3D9。正确路线是 DX11 downlevel，而不是把同一个测试改成含义不同的 pixel-shader 仿真。
- `ProbeGpus()` 现在实际创建探测设备，记录 DX11 Feature Level 与 DirectCompute；不再把每个 DXGI adapter 硬编码为 DX11 supported。WinUI 也读取 `dx11Compute`，Full Analysis 会跳过不支持计算的 DX11 组合，Custom Run 会给出明确错误。
- **双 D700 能力合并与 DX12 单节点选择纠正（2026-07-21）**：必须按驱动区分拓扑。归档的 2026-03 D700 运行使用 `25.20.14020.10001`，当时 DXGI 探针确实得到 2 个不同 LUID，DX11/DX12 都能通过两个 handle 创建设备；但同期 Task Manager 已确认两条 D3D 路径的物理负载都被驱动路由到主卡，所以旧 #2 是有效 API 运行、却不是副卡独立成绩。重装后的 `27.20.14540.15002` 下，Vulkan 枚举 2 个 `VkPhysicalDevice`，DXGI 当前只枚举 1 个 D700 LUID，其 D3D12 device 内部有 2 nodes。最初为第二个 Vulkan 设备补行时复制了第一行的 DXGI index/LUID 和 D3D flags，导致 Full-All 的 DX12 默认 `NodeMask=0` 两次都落在 node 0；82.610/82.723 GB/s 即属于该问题。现探针按 `ID3D12Device::GetNodeCount()` 把 linked adapter 展开成两个可选行，普通 DX12 运行把 command queue、swap-chain buffers、command lists、root signatures/PSOs、descriptor/query heaps、粒子/流体资源全部绑定到所选 node。当前 `--list-gpus` 为 D700 #1=`Vulkan/DX12/DX11/OpenGL`、#2=`Vulkan/DX12`；DX11 仍只能使用逻辑主行。隔离目录下 4M Particle 短跑已验证 node 0 使用 mask `0x1`（122.8 FPS / 78.66 GB/s），node 1 使用 mask `0x2`（120.1 FPS / 80.14 GB/s），且 `workloadConfig` 写入 `dx12Node`、`dx12NodeMask`、`dx12LinkedNodes`，History/JSON 不再把两次运行当成无法区分的同一设备。AFR/SFR 双节点路径保持不变；若旧驱动再次真实暴露两个 DXGI LUID，探针仍会分别保留它们。另：本机 FireGL/UMD + RenderDoc 在 **linked-adapter 次节点（node≥1）** 上会在 `CreateCommandQueue` 触发 AV。先前仅关 `renderDocEnabled` 不够：GUI Full Analysis 对 DX12 worker 仍会在 probe 前设置 `VK_IMPLICIT_LAYER_PATH`，Vulkan 探测会把 RenderDoc 注入进程并挂钩 D3D12。现对显式 DX12/DX11/OpenGL worker **不再配置/预加载** Vulkan RenderDoc 层；次节点仍自动关闭抓帧（与 AFR/SFR 同类例外），node 0 仍可经 `InitRenderDoc` 抓帧。
- DX11 backend 在 FL10 使用 `cs_4_0/vs_4_0/ps_4_0`，FL11 使用 SM5；对 FL10 查询 `D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS::ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x`。fragment-only workload 完全跳过 compute shader、UAV 与粒子 buffer。
- 生产 shader 的 16 个 SM4 entry/profile 已由 Windows SDK FXC 全部编译通过；N-body `SV_GroupIndex` 已修正，FP64 在 SM4 明确拒绝，DX11 超过 65,535 dispatch groups 会拒绝，SM4 N-body 安全上限暂定 4,096 以避免 TDR。
- **尚未在 GT 120 实卡运行。** 正式支持结论必须在目标驱动/系统上完成 Stream Light/Medium、fragment workload、4,096-body N-body、15 秒、可用时的第 5 秒抓帧、timestamp 与 TDR 验收。当前 WinUI/安装器不支持 Windows 7；用户已把独立 Windows 7 GUI + 安装包排在 **PS3 之前**（HarmonyOS 之后），计划共享现有 DX11 FL10/SM4 worker，并以 Aero/DWM 原生前端取代此前仅做 legacy CLI package 的设想。Windows 10 与 Windows 7 的驱动、抓帧工具和 TDR 结论必须分别记录，不得由一端外推另一端。

## 8. 开源与官方参考（只借架构/算法，逐项审许可证）

- [gpu-burn](https://github.com/wilicc/gpu-burn)（BSD-2-Clause）：高算术强度 + 重复结果校验；可借 CoreBurn/checksum 思路，不复制 CUDA 路径。
- [memtest_vulkan](https://github.com/GpuZelenograd/memtest_vulkan)（zlib）：Vulkan 显存带宽/错误检测；数分钟才适合稳定性判断。
- [vkmark](https://github.com/vkmark/vkmark)（LGPL-2.1）：scene registry、可配置 option、有序 suite 与逐场景 duration；优先独立实现结构，避免无意引入 LGPL 代码义务。
- [Sascha Willems Vulkan](https://github.com/SaschaWillems/Vulkan)（MIT）与 [Khronos Vulkan Samples](https://github.com/KhronosGroup/Vulkan-Samples)（Apache-2.0）：PBR、shadow、deferred、N-body、timestamp 等技术积木；素材许可证需另审。
- [jeantimex/fluid](https://github.com/jeantimex/fluid)（MIT）：用户截图对应项目；其路线是 3D SPH/PIC-FLIP、粒子空间网格、独立密度体积 splat 与 free-surface raymarch。当前 Cinematic Liquid 只参考该架构和参数比例，shader 与宿主实现均为本项目独立代码，未 vendor 上游代码/资产。
- [matsuoka-601/Splash](https://github.com/matsuoka-601/Splash)（MIT）：WebGPU MLS-MPM + screen-space fluid；参考其粒子/网格管线和密度体积阴影思路。
- [luihabl/VkFluidSim](https://github.com/luihabl/VkFluidSim)（CC0）与 [Wumpf/blub](https://github.com/Wumpf/blub)（MIT）：原生 Vulkan SPH 与 APIC/MLS-MPM 结构参考。核心算法依据 [MLS-MPM/APIC paper](https://yuanming.taichi.graphics/publication/2018-mlsmpm/) 独立实现。
- [LegitEngine](https://github.com/Raikiri/LegitEngine)（代码 MIT）：Vulkan rendergraph、3D FFT/流体与 point-sprite 可视化参考；仓库素材另有权利人。
- [RenderDoc](https://github.com/baldurk/renderdoc)（MIT）：可再分发，但必须保留 [LICENSE](https://github.com/baldurk/renderdoc/blob/v1.x/LICENSE.md) 与 [第三方声明](https://github.com/baldurk/renderdoc/blob/v1.x/docs/credits_acknowledgements.rst)。程序化抓帧见 [in-application API](https://github.com/baldurk/renderdoc/blob/v1.x/docs/in_application_api.rst)。
- [Windows App SDK self-contained deployment](https://learn.microsoft.com/windows/apps/package-and-deploy/self-contained-deploy/deploy-self-contained-apps)、[VC Runtime redistribution](https://learn.microsoft.com/cpp/windows/redistributing-visual-cpp-files)、[vcpkg manifest mode](https://learn.microsoft.com/vcpkg/concepts/manifest-mode)。

## 9. 正在进行 / 下一步

### 当前正在进行

- [x] **2026-09-30 Linux / JetPack 5 构建修复与 Mesa 软件渲染标记**（提交 `a894c8a`）：
  - 构建修复：`app_base.cpp` 补上 `<cstring>`；Linux 下链接 `Threads::Threads` 与 `${CMAKE_DL_LIBS}`（glibc < 2.34）。
  - 软件渲染标记：新增 `isSoftwareRendererName()`，llvmpipe/lavapipe 的结果不再被记为硬件 GPU。
  - 文档：`docs/building.md` 新增 Jetson Xavier NX 章节。
  - 验证范围：只在容器中验证过，未在 Jetson 实机上运行。
- [x] **2026-09-30 `stream_v1` 字节数审计**：只更新了文档，代码未改（用户决定），见 §3.6 与 `docs/benchmark-workload-suite.md` §6。
- [x] **2026-10-05 `membw_v1` 第一步：Vulkan 命令行版**，见 §3.7 与第 10 节验证记录。
- [x] **2026-10-05 `membw_v1` 接入 WinUI 运行页和 History**，见 §3.7 与第 10 节验证记录。
- [x] **2026-10-05 `membw_v1` GUI 曲线图**，见 §3.7 与第 10 节验证记录。
- [x] **2026-10-06 GUI 里所有测试的图表**：GPU 页的成绩对比条形图和每帧耗时折线图、CPU 页的逐处理器柱状图、不依赖 Python 的 Charts 页，见第 10 节验证记录。
- [x] **2026-10-06 CPU 缓存 / 内存带宽测试 `cpu_membw_v1`**：命令行版、WinUI CPU 页和曲线图，见 §3.8 与第 10 节验证记录。
- [x] **2026-10-06 Android / iOS / macOS 前端补齐**：三端的 CPU 页接入算力测试和缓存 / 内存带宽测试，所有测试都有图表。Android `assembleDebug` 通过；iOS 和 macOS 只做过类型检查；三端都没有在设备上运行过。见第 10 节验证记录。
- [x] **2026-10-06 HarmonyOS 版接入共享引擎**：`ohos/` 从独立演示改为和安卓版对齐的 App，`scripts\build-ohos.ps1` 可以从命令行编出未签名的 HAP；没有设备、模拟器镜像和签名，一次都没有运行过。见第 10 节验证记录。
- [ ] **HarmonyOS 上机验证**：需要用户在 DevEco Studio 里登录华为账号生成签名，并连接设备或下载模拟器镜像；然后确认界面显示、Vulkan、绑核、缓存容量读取和 hdc 启动参数。
- [x] **2026-10-06 Android 的 adb 入口**：引擎输出写入 logcat，debug 包可用启动参数直接开始一项测试；只编译通过，没有在设备上运行过。见 `android/README.md` 和第 10 节验证记录。
- [x] **2026-10-06 Apple 检查脚本入库**：`scripts/check-apple.ps1`，改 iOS / macOS 的 Swift 代码后先跑它；它不是 Xcode 编译，范围见 `scripts/apple-check/README.md`。
- [x] **2026-10-06 Android 和 HarmonyOS 的大屏、折叠屏和新界面（只写了代码）**：两端都按窗口宽度切换布局（侧边导航、两栏、网格），预览画面固定 16:9；Android 用上 Material 3 Expressive 的导航组件，HarmonyOS 6.1 以上用悬浮页签。两端都能编译；按用户要求没有在模拟器或设备上运行，界面没有看过。见第 10 节验证记录。
- [x] **2026-10-07 Android 在模拟器上跑通**：Android 17（API 37，x86_64）和 Android 7.0（API 24，x86）两个模拟器。修了三个只有运行才能发现的问题（GPU 测试一启动就崩溃、预览画面被盖住、Android 7 上第一条成绩的编号用了 UTC）。布局、折叠时不重建、adb 入口、成绩里的机器信息都看过了。见第 10 节验证记录。模拟器里的数字不是成绩。
- [x] **2026-10-07 各平台对齐**：流体和显存带宽测试接入 Android 和 HarmonyOS；iOS 运行页改成和它们相同的测试列表加全屏运行；"帧率被刷新率限制"的判定扩到 DX12、DX11、OpenGL，Windows 界面显示提示。Android 在模拟器上跑过，HarmonyOS 只编译，iOS 只做过类型检查。详见第 10 节。
- [x] **2026-10-07 HarmonyOS 在模拟器上跑通**：手机、三折叠、TV、二合一四个模拟器；修了启动崩溃、TV 上的选中颜色、显存带宽不能停止等问题，见第 10 节。车机模拟器要用户从 DevEco 启动。
- [ ] **HarmonyOS 还没看的**：车机模拟器（`Huawei_Car`，7.0 Beta1）要用户从登录了账号的 DevEco 设备管理器里启动，命令行启动缺授权文件；手机类型设备上的全屏横屏画面（两个 5.1.1 的手机模拟器没有 Vulkan 驱动）；6.1 以上手机上的悬浮页签；TV 遥控器的焦点操作；真机。
- [ ] **Android 真机按标准流程跑一遍**：`scripts\adb-suite.ps1 -Install`（13 项，见第 10 节）。第一台是 Xiaomi 2308CPXD0C（骁龙 8 Gen 2），已连上，等用户在手机上打开"USB 安装"。
- [ ] **Android 真机**：模拟器验证不了的有芯片名（模拟器报的是 `AOSP ranchu`）、大小核、温控，以及带宽类测试的数字。
- [x] **2026-10-06 成绩文件的前后兼容**：重写了 `results.json` 的读写。读更新版本的文件时保留不认识的字段；保存改为先写临时文件再替换；读不全的文件在重写前留副本。用三个旧版本的程序和这台机器上的 55 个真实成绩文件验证过。规则见 §1 第 8 条和 §3.9，记录见第 10 节。
- [x] **2026-10-06 成绩里记录机器信息**：每条成绩新增机型（`deviceModel`）和内存（`systemMemoryMB`）；Android 和 HarmonyOS 的系统版本改记系统发行版本，Android 的芯片名取系统属性；移动端和 Apple 端的历史页显示这一行。Windows 和 Linux 容器实际运行过；Android、HarmonyOS、iOS、macOS 只编译或类型检查过，读到的文字没有在设备上核对。见第 10 节验证记录。
- [ ] **`cpu_membw_v1` 剩余步骤**：查清流式写入偏慢和 L2 曲线分段的原因；在 macOS、Windows ARM64、Jetson 和大小核 CPU 上实测；见 `docs/TODO.md`。
- [ ] **macOS / iOS 按性能核、能效核分组测量（未做；2026-10-06 记录）**：
  - **现状：** macOS 和 iOS 不能把线程绑定到指定核心，所以 CPU 算力测试的"单核"模式和缓存 / 内存带宽测试的单线程扫描都跑在系统挑选的核心上，能效核测不到。引擎把这类结果标记为 `scheduler_managed`。
  - **可行的办法（只是按对 Apple 芯片的了解，没有实现，也没有在 Mac 上试过）：** 指定某一个核心做不到，但可以分成"性能核一组、能效核一组"来测。做法是设置线程的 QoS 等级：后台等级（`QOS_CLASS_BACKGROUND`）的线程只在能效核上运行，高等级（如 `QOS_CLASS_USER_INTERACTIVE`）的线程优先在性能核上运行。两类核心的数量和缓存容量系统分别报告（`hw.nperflevels`、`hw.perflevel0.*`、`hw.perflevel1.*`）。
  - **已知的坑：** 能效核在后台等级下可能被降频，测出来的数字会低于能效核的实际能力，结果里必须写明这一点。
  - **不涉及：** Intel Mac 没有大小核。
  - **动手前要确认的：** 上面的接口名和行为都没有在设备上核对过；iOS 上是否同样有效也不清楚。
  - **要改的地方：** `src/cpu_benchmark.cpp` 的 `ProbeAppleTopology()`（现在不区分核心类别）；`src/cpu_membw.cpp` 的 `PinCurrentThread()` 和 `PlatformCanPin()`；两个 Apple 前端里按 `schedulerManaged` 显示的说明文字。
- [ ] **`membw_v1` 剩余步骤**：访存延迟测试（定位缓存容量）、移植到 DX12/DX11/OpenGL/Metal、从 History 重画曲线、RX 6900 XT 和 vkpeak 外部对照，见 `docs/TODO.md`。`stream_v2` 仍暂缓，粒子测试 v1 保持不变。
- [ ] **实机远程测试（用户 2026-09-30 排入）**：
  - NVIDIA Jetson Xavier NX：通过 SSH，跑三个 GPU 主测试和 CPU 测试；
  - Android 手机：通过 adb；
  - iPhone：需要一台 Mac，通过 SSH + `xcodebuild` / `devicectl`。

  前提条件、分工（需要输密码或在设备上确认的步骤由用户完成）和验收项见 `docs/TODO.md` 平台移植第 3、4、5 条。
- [ ] **OpenGL + llvmpipe 的 GPU Burn 计分虚高约 40 倍**：GL timer 只统计到约 5.9 ms，实际帧时间约 330 ms。两种处理方案（软件渲染器改用墙钟帧时间，或把该成绩判为无效）尚待用户决定；目前只在 `docs/building.md` 中提醒不要使用该成绩。
- [~] **2026-07-31 会话暂停点 — macOS 0.2.6 双架构 DMG**：
  - 版本源已经对齐到 `0.2.6`（CMake、`macos-gui/Info.plist`、XcodeGen 工程配置）；原生 arm64 自包含 App 及三主项实跑记录见 §10。
  - `scripts/build-macos.sh` 已扩展为按 `--arch arm64|x86_64` 构建，并校验 GUI/helper 的单一目标架构、`minos 12.0`、bundle 版本、只含系统动态依赖和 deep codesign；离线复用 GLFW 3.4 source 的 license 路径也已修正。
  - **x86_64 交叉构建已通过**：`out/macos/x86_64/Release/GPUBenchmark.app` 的 SwiftUI GUI 与 helper 均为纯 x86_64、`minos 12.0`、ad-hoc 签名通过且无非系统动态依赖。该 slice 在 arm64 主机只完成静态验收，**没有在 Intel Mac 实跑**。
  - 新的 `scripts/package-macos-dmg.sh` 与 `packaging/macos/README.txt.in` 已写入工作树；脚本会分别重建两 slice、放入 `Mangekyo.app` + `/Applications` 软链 + README/license/notices，生成 UDZO/HFS+ DMG，readonly 挂载后再次审计 bundle，再生成 SHA-256 清单。两个 shell 脚本 `bash -n` 通过。
  - **暂停点尚未完成**：新脚本还没有从头重建 arm64，也没有实际运行 DMG 打包；因此 `Mangekyo-0.2.6-macos-arm64.dmg`、`Mangekyo-0.2.6-macos-x86_64.dmg` 和 checksum 清单此刻均不得写成已产出。
- [~] **2026-07-31 会话暂停点 — iOS/iPadOS 引擎与构建链**：
  - Xcode 26.6 下以 `CMAKE_SYSTEM_NAME=iOS`、arm64、deployment 16.0、`CODE_SIGNING_ALLOWED=NO` fresh configure **通过**；日志确认 Metal ON、Vulkan/DX/OpenGL OFF、NO_GLFW、`ios_engine_host` ON、`arm64-apple-ios16.0-simulator`。
  - 第一次 `gpu_engine` build 的首要错误是 desktop `src/main.cpp` 内 5 个 `std::system` 在 iOS SDK unavailable；`CMakeLists.txt` 已将 mobile archive 与 desktop CLI 源图分开，**修复后复建尚未执行**。
  - stream 与 gpu_burn 的 embedded host 已存在；Cinematic Liquid Metal v2 backend 已存在，但 host parser、第三个 `.metal` resource 与 runtime 尚未接通。Bridge 仍需从 `cliMain`/`gpb_run` 改为 `ProbeMetalDevices`、直接 workload API 与原生 CPU API。
  - 不能把当前状态表述为“三个测试通过”：此刻只有 macOS 三主项有实跑证据；iOS 三主项均无 simulator/真机运行证据。
- [~] **2026-07-31 会话暂停点 — iOS/iPadOS SwiftUI**：
  - 已完成现有 6 页、macOS GUI 与 WinUI 的差距审计。现状问题包括：Run/CPU 仍会走 `gpb_run`/`cliMain`，embedded progress 固定不动，Capture 只有文案，History/Charts 尚无完整 schema normalization/export，About 仍有硬编码版本。
  - 已改 `ios-gui/Mangekyo/Models/BenchResult.swift`：补 schema/platform/arch/stability/throttle 字段、workloadConfig 解析、execution mode、旧异常 bandwidth 分数隐藏与合同显示；已新增 `MobileBenchmark.swift` 定义三主 workload、移动 preview contract、请求/阶段/cancel/capture capability/结构化 issue。两者均**尚未 Swift 编译**。
  - GUI 完成度只能记为架构/审计完成、第一批 model 在写；Run engine、History/Charts 导出、Settings/About、iPhone/iPad adaptive UI、iOS 16–25 Material 与 iOS 26 Glass 尚未完成，不能使用百分比替代验收。
  - 当前机器可用目标包括 iPhone 16 Pro iOS 18.5、iPad Pro 11-inch (M4) iOS 18.5、iPhone 17 Pro iOS 26.5 模拟器；但暂停点尚未生成、安装或启动 App。`security find-identity -v -p codesigning` 为 0 个有效 identity，`xctrace` 无连接的 iPhone/iPad。
- [x] **2026-07-31：macOS vertical slice 在 M4 Pro 实机收口**。默认 Apple CMake 改为 checksum-pinned GLFW 3.4 源码静态构建、Vulkan 默认 OFF、非 Windows CPack 默认 ZIP；严格 macOS 12 availability 构建及 ZIP package 通过。`scripts/build-macos.sh` 生成自包含、ad-hoc 签名的 arm64 App，GUI/helper `minos 12.0` 且无 Homebrew 动态依赖。SwiftUI 新增 Complete Suite / Fill Missing、原生 Charts、History mode/旧异常分数归一、结构化错误摘要与 bundle helper 发现。最终包内 helper 完成 1M Particle 15 秒 + `.gputrace`、GPU Burn 16 steps / 15 秒、Cinematic Liquid 320,920 粒子 / 6 秒 preview；App 脱离仓库启动/退出通过。**仍开放：实际 Monterey 回归、Developer ID/notarization/universal binary；Metal liquid 仍是独立 preview 合同。**
- [x] 2026-07-21：用户要求开发默认编完整 GUI+CLI。新增 `scripts/build-windows.ps1`；`CopyGpuBenchmarkWorker` 扩展为同步 worker **与** HLSL/SPIR-V/OpenGL 资产。本机已用 preset 编过 CLI、MSBuild 编过 WinUI，并在 `gui/x64/Release` 验证 `compute.hlsl` 等可由该 target 自动补齐。双 FirePro D700 上仍见独立问题：Vulkan `vkCreateDevice failed`（与缺 shader 无关）；DX/OpenGL 缺文件问题在拷贝后应消失。正式发布脚本未改。
- [x] 2026-07-18：按用户对 Full Analysis / All GPUs 实图反馈修正 WinUI 结果编排与软件设备显示。WARP/Basic Render 在 GPU 下拉框和本次 Summary 中保留真实软件渲染器名称，并追加当前 CPU 型号（例如 `Microsoft WARP (AMD Ryzen …)`），不再退化显示为 `GPU 2`。Full Analysis 会在启动前跳过 probe 已知不支持的 GPU×API（含非 fragment workload 的 DX11 compute 不可用）并在 Summary hint/raw output 列表说明；这些组合计为 unsupported/skipped、使用绿色完成状态，只有实际启动后非零退出才计为 failed/红色。另为“所选核显但 Windows/WGL 实际分配到另一 GL_RENDERER”增加专用 Summary 说明和状态文案；分类严格要求 OpenGL worker + CLI 精确 `cannot select GPU index`/`active GL_RENDERER` 标记，shader/context/driver/timeout 等其他 OpenGL 错误不会套用此说明。Release WinUI x64 已编译通过；尚未运行新的 Full Analysis/核显 OpenGL 矩阵或做 GUI 视觉点击验收。
- [x] 2026-07-17：WinUI 四个秒数/时长 `NumberBox`（GPU duration、RenderDoc capture、CPU per-test、CPU warm-up）由 `Inline` 统一改为参考图对应的 `Compact` 竖向浮层按钮；补齐 Enter/页面外点击失焦、空值恢复与 `PopupThemeTransition`，避免 × 清空后 `NaN` 导致按钮全灰。数值合同未变，Release engine 与 WinUI x64 build 通过；按用户要求未启动最终 GUI，视觉手测仍待用户侧验收。
- [x] 2026-07-16/17：用户否定首个二维 plasma chamber 原型后，将公开 `gpu_burn` 重做为 **Mangekyo faceted glass v2**：Vulkan/HLSL/OpenGL 三份一致 shader 现使用透视 3D SDF 截顶宝石/八面体碎钻、前后深度层、平面法线、Fresnel、RGB 色散和吸收；两次全屏固定循环使用独立 `gpu_burn_v2_mangekyo_faceted_glass_v1` / shaderVersion=3 身份，被否定原型不混分。Plasma Bloom 通过 `gpu_burn_v1` selector、原 shader 与历史合同完整保留。Release core 编译通过；RTX 5090 Vulkan 自动标定 16→2048 后 render 13.199 ms / 286.00 Gpix-step/s / 90.4% timestamp utilisation，stableScore 288.21 / CV 1.50%；DX12/DX11/OpenGL v2 低步数 runtime smoke 均通过。视觉核验图为 `out/kaleidoscope-prototype/mangekyo_faceted_final.png`。
- [x] 2026-07-15：完成独立 `gpu_burn_v1` vertical slice。用户否定甜甜圈后改为原创 **Plasma Bloom / 等离子晶核**：实心七瓣晶体、无环形孔洞、非粒子；保留 `gpu_stress_v1` 成绩契约，默认 15 秒 / 第 5 秒抓帧，并同步 Vulkan/DX12/DX11/OpenGL、WARP、GUI、结果/图表与打包资产。
- [x] 2026-07-15：RTX 5090 Vulkan 自动标定 16 → 1604 steps/draw；正式 render 14.899 ms、`198.44 Gpix-step/s`，稳定段连续 8 次 NVML utilization 均为 99%，功耗约 599–600W（600W limit）。应用内 93% 是 GPU timestamp / 总帧墙钟比，包含 CPU/present 间隙，不等同于 NVML busy。
- [x] 2026-07-15：完成 README/TODO/roadmap/真实源码/结果文件/GUI/构建链审计。
- [x] 2026-07-15：建立本 handoff，并把其设为 AI 的优先交接入口。
- [x] 2026-07-15：交付 `gpu_stress_v1` GraphicsBurn vertical slice，旧 `stream`/`stress` 不删除、不改旧 shader；Vulkan/DX12/DX11/OpenGL 与 WARP 可运行，Metal 显式 unsupported。
- [x] 2026-07-15/16：WinUI 最终同步 11 项 workload 选择、Plasma Bloom 与 Cinematic Liquid 主测试标签、默认 15 秒与非 headless 第 5 秒抓帧；Full Analysis 会传递所选 workload，并拒绝 Vulkan-only liquid 的跨 API 伪运行。
- [x] 2026-07-15：PathService、结果 schema v2、抓帧排除计分、run-all capture 传递与 bundled RenderDoc Vulkan layer 修复。
- [x] 2026-07-15：生成并验证 Windows x64 GUI-first engineering ZIP；本机 staged 四 API 均实际产生 `.rdc`。
- [x] 2026-07-15：最终可靠性收口：抓帧后固定排空 16 个 query samples；GUI 后处理改为随包 RenderDoc 优先、缺报告运行时时诚实降级；Charts 接入 GPU Burn/GraphicsBurn。
- [x] 2026-07-15：GPU Burn 未探测的固定步数收紧为 16–32；默认自动标定仍可在 16-step 安全探针后升至 2048。Windows 直接/GUI Custom OpenGL 若请求的 GPU 与真实 GL_RENDERER 不符会明确失败，不能错标成绩。
- [x] 2026-07-15：Cinematic Liquid 已同步 CLI、WinUI、历史/图表和结果 schema；固定 15 秒、第 5.1 秒 RenderDoc 抓帧，首个 3.55 秒演示循环使抓帧落在明显撞击/飞溅阶段。
- [x] 2026-07-15：完成 `cinematic_liquid_v2_surface_splat_optics_v4` Vulkan 切片：320,068 粒子、128x64x96、10 substeps、四刚体/双向耦合、Spiky² surface + 3x3x3 Gaussian（mix 0.75）、352-step 两界面折射；`shaderVersion=6`。
- [x] 2026-07-15：最终低侧第 5 秒镜头和开放侧单层细池沿已进入固定场景；正式图 `rdoc_captures/cinematic-liquid-v2-5s-formal-optics-v4.png` 显示真实宏观溃坝，画面/光学显著改善，但仍未达到 jeantimex SPH 参考的自然微观细节。
- [x] 2026-07-15：RTX 5090 Vulkan 正式 15 秒 + 5.1 秒 RenderDoc 通过，结果 id `20260715-170629-492`，Compute 10.572 ms、Render 1.553 ms、Total 12.125 ms、`263.98 MParticle-step/s`、966 measured frames；capture 0.103 秒排除，1 attempt/1 saved。CLI Release 与 WinUI Release x64 build 通过。
- [x] 2026-07-15：stage verifier 已覆盖 3 个 surface SPIR-V；v1/v2 capture 命名已隔离；非 15 秒预览结果进入独立 `_preview` 组。
- [ ] `cinematic_liquid_v2` 的 WinUI 交互/run/history、船/沉球/越沿粒子精确数值与视觉轨迹、跨 GPU 时间推进合同、Vulkan timestamp 边界和异常路径资源清理仍待验收；DX12/DX11/OpenGL 液体仍未实现（**Metal 已有 MLS-MPM + raymarch preview，见目标 C / §3.0.1**）。
- [x] **2026-07-19 / 2026-07-31：macOS 12 底线与当前 Mac vertical slice**：CMake/`macos-gui` deployment 12.0；`NavigationView`/`defaultSize` 可用性回退；M4 Pro 已完成自包含 App、三主项、原生 `.gputrace` 与独立启动验收。**实际 Monterey 运行仍开放**。
- [~] **2026-07-19：iOS 规格写入**；**2026-07-24：源码垂直切片写入仓库**；**2026-07-31：首次 Mac/Xcode configure 通过，首次 build 揭示 desktop `main.cpp` blocker并已改 source graph，复建待执行**。最低 **iOS 16**；**iOS 26/27 = Liquid Glass**。当前仍无成功 App build、模拟器运行或真机验收，详见本节顶部 checkpoint 与完整计划。
- [x] 2026-07-15：安装/Release 链完成并实跑：安装 Inno Setup 6.7.3，以官方 RenderDoc 1.45 固定 archive+SHA 为输入，从头构建 CLI/WinUI、511-file stage、PE delay-import 审计、CPack ZIP、ZIP 内容复核与 Inno Setup；最终 v0.1.0 ZIP/Setup、`SHA256SUMS.txt`、`release-assets.json` 已生成。
- [x] 2026-07-15：应用户要求完成 duck family v5 场景改造（经典造型大黄鸭 + 3 只小鸭、7 刚体、SDF/渲染/碰撞三处一致、bounding 剔除；`cinematic_liquid_v2_duck_family_v5`、`shaderVersion=7`、`sceneVersion=3`）。CLI Release 重建通过；验证细节见第 10 节。15 秒正式成绩尚未在新版本下重跑。
- [x] 2026-07-15：用户恢复流体工作后完成 iterative optics v6：保留用户的 duck family，surface 改为 5x5x5 binomial（mix 0.90），最多 4 界面 Fresnel/Snell + 分段 Beer–Lambert/opaque depth sorting，`extinction=(30,10,8)`、linear exposure、density 边界归零；`cinematic_liquid_v2_iterative_optics_v6`、`shaderVersion=8`、`sceneVersion=3`。CLI/WinUI Release 构建通过，只有 6 秒短预览，尚无正式 15 秒成绩。
- [x] 2026-07-15：完成历史 physical-scene v7 代码切片：320,920 粒子（142x14x98 + 48x37x71）、`cinematic_liquid_v2_physical_scene_v7` / `shaderVersion=9` / `sceneVersion=4`；后续共享 scene 参数变化已升为 v8，不能继续把 v7 写成当前合同。
- [x] 2026-07-15：v7 shader 编译、最终 6 个 SPIR-V、CLI Release、WinUI Release x64 build 通过（0 error，仅有既有 warning；最新增量构建 2 个，源文件重编时曾为 4 个）；仅做 6/8 秒自动停止 smoke，无正式 15 秒、无可靠保存的新 RenderDoc、无完整视觉验收。`241.13` transient console 值不得记录为结果；`--time 8` 窗口自动关闭不是已确认的崩溃。
- [x] 2026-07-16：当前 scene 合同升为 `cinematic_liquid_v2_physical_scene_v8` / `sceneVersion=5`（inset 0.45、wall-top fraction 0.42、extinction 12/3.6/2.5）；SPH 318,464 粒子完整运行 15 秒并获用户视觉验收。SPH 仍无正式 score，所有时长强制 `_preview`。
- [x] 2026-07-16：完成 Windows ARM64 原生本体、WinUI GUI、ZIP、Inno Setup 安装包、依赖与全链路构建审计。通过自动构建 ARM64 `vulkan-1.lib` 并处理 `vcruntime140_1.dll`，全量测试及 CPack 打包全部通过，产物生成于 `out/release/windows-arm64/`。
- [x] 2026-07-17：用户订 **MIT** 根 `LICENSE`；CMake/CPack 默认 `ZIP;WIX`；发布主路径改为 WiX MSI（x64+ARM64 原生 Template）；Inno 降为 legacy。本机已产出 `Mangekyo-0.1.3-windows-{x64,arm64}.msi`（stage `projectDistributionLicense=true`）。
- [x] 2026-07-17：互动水池（`cinematic_liquid`）跨 API 需求曾短暂接 DX12，后按用户要求 **撤回**，恢复 Vulkan-only；勿把未接线的 HLSL 草稿写成已支持。
- [x] **macOS vertical slice（2026-07-31）**：Particle/Burn/SwiftUI/液体 raymarch/MTLCapture + **OS 底线 12** 已落；M4 Pro 当前系统编译、三主项、抓帧、签名与 App 启动通过。实际 Monterey 与公开发布签名/notarization 仍是 release gate，不再把“未在 Mac 编译”列为事实。
- [x] 实际 MSI/ZIP 与动态 Vulkan loader 已完成构建/静态审计；显式 Vulkan 缺 loader 的异常路径也已加 guard。
- [ ] 冻结 report worker、Authenticode signing、GT120 实卡与 clean-machine 安装/升级/卸载/抓帧验收仍开放（**项目 LICENSE 阻塞已解除**）。
- [x] **日语 GUI 本地化（2026-07-18 / 0.2.0）**：`gui/i18n.h` 扩展 `Lang::Ja`、`tr(en,zh,ja)`、`trDyn`、`usesYmdDate`、`detectOsLangLabel`；OS 自动探测 `LANG_JAPANESE`；设置页语言下拉增加「日本語」；`MainWindow.xaml.cpp` 全表 UI 文案含第三参日语。安装器 WiX 向导仍为英文（可选后续加日语 MSI UI）；Inno legacy 仍为 EN + 简体中文 `.isl`。成绩合同与内部 id 不变。

### 完整移植与发布计划（2026-07-31 checkpoint）

以下计划以当前未提交工作树为起点，执行顺序是 **先封装 macOS 0.2.6 双 DMG，再完成 iOS/iPadOS**。每一阶段都必须分别记录“源码存在 / 编译通过 / simulator 运行 / 真机运行 / 可公开发布”，不得用后一阶段尚未取得的结论描述前一阶段。

#### Phase 0 — 恢复现场与变更边界

1. 开工先读本节、§3.0.1、§3.0.3、§10，再执行 `git status --short` / `git diff --check`。当前工作树本来就有大量 macOS vertical-slice 改动；不要 reset、checkout 或覆盖这些文件。
2. `results/` 是用户未跟踪数据，始终排除在删除、清理、暂存与提交之外。`out/` 是可再生验证产物，不作为源码完成证据。
3. 继续前先复核暂停点新增/修改：`scripts/build-macos.sh`、`scripts/package-macos-dmg.sh`、`packaging/macos/README.txt.in`、`CMakeLists.txt`、`ios-gui/Mangekyo/Models/BenchResult.swift`、`MobileBenchmark.swift`。后两个 Swift model 尚未 build，不能假定 API/初始化器自动兼容。
4. 不新建分支，不提交/推送，除非用户另行明确要求。每完成一个阶段立刻更新本节与 §10，避免再次只有聊天记录、没有仓库交接事实。

#### Phase 1 — macOS 0.2.6 Apple Silicon / Intel 工程 DMG

1. **版本单一来源**：确认 CMake/CPack、`macos-gui/project.yml`、`Info.plist`、生成的 pbxproj、App bundle 与 DMG 文件名都为 `0.2.6`；GUI About 应从 bundle 读取版本，不再硬编码。Build number 可独立递增，但必须在两架构一致。
2. **干净构建两个单架构 slice**：用 `scripts/build-macos.sh --configuration Release --arch arm64` 与 `--arch x86_64` 分别生成自包含 App。继续 source-build pinned GLFW 3.4、Metal-only、deployment 12.0、无 Homebrew/Vulkan runtime 泄漏。
3. **每个 slice 的强制静态门**：对 GUI/helper 执行 `lipo -archs`/`file`、`vtool -show-build`、`otool -L`、`plutil` 与 `codesign --verify --deep --strict`；检查三个 Metal shader、项目 license、third-party notices、GLFW license 都在 bundle。任何单项失败都不得生成“已验证”DMG。
4. **运行门**：
   - arm64：重新运行 App 启动/退出、CLI help/Metal probe，并至少重跑三主项 short smoke；既有 15 秒 Particle+capture、15 秒 Burn、6 秒 Liquid 记录保留为正式证据。
   - x86_64：在本 M4 Pro 可尝试 Rosetta smoke（若系统具备 Rosetta），但这仍不能替代 Intel 硬件；至少保留交叉构建静态验收。公开写法必须是“Intel slice 已交叉编译/静态验证，Intel Mac runtime 待验证”。
   - macOS 12：部署标记不是运行证据；实际 Monterey 启动/短跑继续作为兼容性 release gate。
5. **生成两个独立 DMG**：运行 `scripts/package-macos-dmg.sh --arch all`。预期产物：
   - `out/macos/packages/Mangekyo-0.2.6-macos-arm64.dmg`
   - `out/macos/packages/Mangekyo-0.2.6-macos-x86_64.dmg`
   - `out/macos/packages/Mangekyo-0.2.6-macos-SHA256SUMS.txt`
6. **DMG 内容/介质复验**：`hdiutil verify`，readonly mount，检查 `Mangekyo.app`、指向 `/Applications` 的软链、README、LICENSE、THIRD_PARTY_NOTICES；对挂载后的 App 再执行架构/minOS/依赖/签名/bundle 版本检查，detach 后验证 SHA-256。
7. **工程包与公开包分界**：ad-hoc 签名双 DMG 可作为本地工程交付；公开下载仍需要 Developer ID Application 重签、notary submit/wait、staple、Gatekeeper (`spctl`) 和干净用户账户安装验证。不要把 ad-hoc DMG 写成“已公证发布”。
8. 两个 DMG 均通过上述门后，更新根 README、`macos-gui/README.md`、`docs/macos-notes.md` 和本文件，列明真实路径/hash、机器/系统、arm64 runtime 与 x86_64 验证层级。

#### Phase 2 — iOS/iPadOS 原生引擎边界

1. **彻底拆开 desktop CLI 与 mobile archive**：iOS `gpu_engine` 不包含 `src/main.cpp`、终端菜单、`std::system`、process spawning、GLFW 或桌面路径假设；macOS/Windows CLI target 保持原行为。先复建 simulator static library，随后复建 macOS，防止 source-list 调整造成桌面回归。
2. **Bridge 不再伪装 CLI**：移除 iOS App 对 `cliMain`/`gpb_run` 的依赖。设备探测直接调用 `ProbeMetalDevices`；GPU 运行通过 typed request 调 `ios_engine_host`；CPU 通过可取消的原生 `RunCpuBenchmark` adapter。若某能力尚未接好，Bridge 返回 typed unavailable/error，UI 禁用，不静默失败。
3. **稳定的 C ABI / Swift wrapper** 至少包含：
   - engine/app version、Metal device/capability probe；
   - `start(workload, duration, capture options, CAMetalLayer)`；
   - `requestStop(reason)`、`isRunning`、elapsed/phase/progress；
   - structured error code/message/recovery；
   - 最终 result JSON 与 capture outcome/path；
   - CPU start/cancel/progress/result（如果本切片实现；否则明确 unavailable）。
   生命周期须保证同一时刻只运行一个任务，Swift 对象销毁/进后台时不会留下 native thread 或悬空 layer。
4. **路径与 sandbox**：结果写 Application Support，用户导出写临时/Documents 后走 Files/Share；不要使用 macOS `~/Library/...` 假设。所有 native 文件写入错误回传 UI，不能只写 stderr。
5. **Capture**：iOS 仅使用 `MTLCaptureManager`，禁止 RenderDoc。请求成功必须返回真实存在的 `.gputrace`；SDK/device 不允许时返回 `captureUnavailable`，并说明原因。模拟器 capture 与真机 capture 分开记录。
6. **构建产物**：先令 CMake Xcode target 在 `iphonesimulator arm64` 与 generic `iphoneos arm64 CODE_SIGNING_ALLOWED=NO` 编译通过；再决定 App 直接链接 per-SDK static lib，或产出不混淆 simulator/device slice 的 xcframework。不得将 simulator binary 塞进 device App。

#### Phase 3 — iOS 三主 workload 合同

1. **Particle / `stream`**：接通现有 embedded host，确认 particle count、duration、Metal device、统一内存、score unit 与 JSON schema；mobile timing/thermal 模型继续使用独立 `_ios_preview`（或经真机数据评审后的新版本），不得混入 desktop 15 秒榜。
2. **GPU Burn / `gpu_burn`**：接通 16-step 安全 profile 与 `gpu_burn.metal`；确认真实 shader/workloadVersion 来自 engine result，而不是 Swift UI 硬编码。记录 frame/GPU timing、stable variance 与 throttle；模拟器成绩必须标 `simulator` 且不入正式排行。
3. **Cinematic Liquid**：把 `cinematic_liquid` parser 接到现有 Metal v2 host，将 `cinematic_liquid_v2.metal` 纳入 iOS bundle，核对 320,920 particles / 128×64×96 / 10 substeps 与内存上限。移动端先保持明确的 Metal+iOS preview 合同；若为防内存/温控调整粒子数、substeps 或 duration，必须再升独立 workloadVersion。
4. 三项共同规则：只暴露 Metal；预热、计分窗口、抓帧排除、取消、后台中止、错误 JSON 与保存路径语义一致；不支持的组合显式 unsupported。Swift `MobileBenchmark.expectedContract` 最终不得保留 `…` placeholder，也不得成为合同真相源，应显示 native result 的真实 version。
5. “三个测试没问题”的最低证据分三层写：
   - build pass：只能说明代码编译；
   - simulator smoke：说明 UI/生命周期/模拟 Metal 路径可运行，不代表真机性能；
   - physical device pass：每项实际完成、有可解析结果、无 crash/资源泄漏，才允许写 iOS 三主项通过。

#### Phase 4 — SwiftUI 产品功能对齐

1. 重写 `BenchEngine` 为 direct embedded async state machine：preparing/running/cancelling/completed/failed、基于 elapsed 的真实进度、重复启动保护、用户 cancel、scenePhase 进后台 cancel、结构化 issue 和最终 result decode。
2. **Run**：三主项、duration、capture capability、Metal-only device info、progress/cancel/result summary；只有三个 host capability 全部通过才启用 Complete Suite / Fill Missing，部分支持时逐项说明。
3. **CPU**：接 native CPU API 后再启用；若本切片没有 native adapter，页面保留但明确 unavailable，绝不调用 `gpb_run`。
4. **History / Charts**：读取 schema v2/v4 及 legacy 数据；按 workloadVersion/device/platform 过滤；隐藏但保留异常 legacy score；使用原生 Charts（可用系统）和 SwiftUI fallback；JSON/CSV 导出到 Files 并提供 Share，失败显示 structured issue。
5. **Settings / About**：语言 EN/简中/日语、主题/外观、存储说明/清理的安全范围、能力列表；版本从 `Bundle` 读取并显示 0.2.6，engine version 来自 Bridge。任何清理动作不得触碰用户未授权文件。
6. **adaptive shell**：iPhone 使用 Tab/NavigationStack；iPad 使用 NavigationSplitView/合适宽度布局与旋转；Dynamic Type、VoiceOver label、safe area、键盘/弹窗与深浅色至少做 smoke。
7. **视觉版本**：iOS 16–25 使用 Material/标准 SwiftUI；iOS 26+ 使用实际 `.glassEffect`/Liquid Glass 且有 availability guard。不能用 Material 截图冒充 Glass，也不能让 iOS 26 符号破坏 deployment 16 构建。

#### Phase 5 — 版本、工程与 build matrix

1. 将 `ios-gui/project.yml`、`Info.plist`、生成 pbxproj、App/About、Bridge engine version 全部对齐 `0.2.6`；确保 iPhone+iPad device families、deployment 16.0、Metal capture/Files sharing 等键只按真实能力配置。
2. 资源 target membership 至少包含三个 `.metal` shader、Assets、本地化与需要的 notices；App 不依赖仓库相对路径或外部 CLI。
3. fresh generate 工程后依次执行：
   - iPhoneSimulator arm64 library/App Release build；
   - iPhone 16 Pro / iOS 18.5 simulator build、install、launch；
   - iPad Pro 11-inch (M4) / iOS 18.5 build、install、launch；
   - iPhone 17 Pro / iOS 26.5 build、install、launch并检查 Glass；
   - generic iOS device arm64 archive/build with signing disabled（编译/链接门）。
4. 生成工程后再检查 pbxproj diff；不要同时手改 `project.yml` 和生成文件而让它们漂移。Swift/C++ warning 中涉及 concurrency、lifetime、availability、nullability 与 result decode 的问题必须关闭，不能只看 exit code。
5. 每个 build 使用 fresh DerivedData/构建目录至少验证一次；增量成功不能替代 clean build。

#### Phase 6 — 运行与回归验证梯度

1. **模拟器基础**：启动、六页导航、phone/iPad adaptive、语言/主题、后台/前台、cancel、错误路径、History save/read、JSON/CSV export/share sheet；iOS 26.5 目视确认真正 Glass。
2. **模拟器 workload**：若 simulator Metal 支持，则分别短跑 stream/burn/liquid 并解析 result；结果强制标 simulator/unranked。若 shader/capture/timestamp 能力不同，记录真实 unavailable，不通过 CPU fallback 伪造。
3. **真机（正式必需）**：至少一台受支持 iPhone 与一台目标 iPad，逐项跑三主测试；记录设备/OS/SoC、duration、温控状态、内存峰值、score/stability、workloadVersion。验证 cancel、锁屏/来电或后台、内存压力、连续 Complete Suite、History/Charts/export。
4. **真机 capture**：对至少一个 workload 请求 capture；成功时验证 `.gputrace` 可在 Mac Xcode 打开，失败时验证 JSON/UI 为 `captureUnavailable`。抓帧窗口不得进入正式计分。
5. **iOS 16 底线**：当前 simulator inventory 没有 iOS 16 runtime；deployment 16 clean build 只是静态证据。发布前需补 iOS 16/17 真机或对应 runtime 的启动/短跑，或把“实际 iOS 16 未测”列为已知限制。
6. **桌面回归**：iOS source graph/Bridge 修改后重建 macOS arm64 app，并至少跑 Metal probe + 三主项 short smoke；确保移动端条件编译没有破坏已通过的 macOS 0.2.6。

#### Phase 7 — TestFlight / App Store 交付

1. 当前机器没有有效 codesigning identity，也没有连接真机；需要用户提供 Apple Developer Team/证书/provisioning 或在 Xcode 登录后，才能做真机安装、Archive、TestFlight。
2. 补齐 App icon/launch experience、PrivacyInfo.xcprivacy、数据收集/文件共享说明、后台能力最小化、export compliance 与第三方 notices；禁止运行时下载可执行代码或 RenderDoc。
3. Product Archive 后做 Organizer Validate、签名/entitlements 审计、安装到干净设备、TestFlight internal smoke。App Store 文案必须把移动成绩标成独立合同，不宣称和 Windows/macOS 正式分数可直接比较。

#### Phase 8 — 完成定义与剩余阻塞

| 里程碑 | 必须满足 | 当前状态 |
|---|---|---|
| macOS 0.2.6 工程 DMG | 两 slice clean build；DMG readonly 挂载复验；SHA-256；arm64 runtime；Intel 层级诚实标注 | **进行中**：x86_64 build/static pass；DMG 脚本已写但未运行 |
| macOS 0.2.6 公开发布 | Developer ID、notarization/staple、Gatekeeper、实际 Monterey；最好实际 Intel runtime | **阻塞于证书/外部机器** |
| iOS source/build complete | mobile 无 CLI/process；三 host+shader；SwiftUI clean build；sim+generic-device build | **未完成**：configure pass；首次 build blocker 已修源码但未复建 |
| iOS simulator vertical slice | iPhone+iPad+iOS26 启动；三项能跑或诚实 capability；History/export/background | **未开始运行验收** |
| iOS/iPadOS 产品完成 | iPhone+iPad 真机三项、capture、温控/内存/后台、签名 archive/TestFlight | **阻塞于签名与物理设备，并且代码尚未完成** |

**恢复后的最短正确执行链**：先运行并修到 `scripts/package-macos-dmg.sh --arch all` 全通过、登记两 DMG/hash；然后复建已排除 `main.cpp` 的 iOS `gpu_engine`；接着去除 Bridge/Swift 的 `cliMain`/`gpb_run`，接通三 workload+第三 shader；再完成 SwiftUI state/history/export；最后跑三类 simulator 与 generic device build。没有物理设备前可以完成“编译 + simulator vertical slice”，但不得把 iOS 三主项写成真机已通过或可发布。

### 推荐下一个实现切片

用户在 **2026-07-19** 将 **Windows 7 GUI** 从 ARM64 之后挪到 **PS3 之前**（HarmonyOS 之后）；用户在 **2026-07-31** 又明确要求当前先完成 macOS 0.2.6 双 DMG、随后继续 iOS/iPadOS。因此最近的执行队列按以下理解：

1. **Windows ARM64 vertical slice（已完成）**：含原生测项、GUI、ZIP 与原生 ARM64 中英双语 Setup。
2. **macOS vertical slice（功能完成，0.2.6 双 DMG 正在收口）**：先实际运行新打包脚本，取得 arm64/x86_64 两份 DMG 与 checksum；公开 release 仍补实际 Monterey、Developer ID/notarization 和 Intel runtime。
3. **iOS/iPadOS**：按上面的 Phase 2–7 完成 mobile engine、三主项、SwiftUI、simulator/device build 与真实验证。
4. **回到未关闭的正确性与自由模式**（可与平台刀并行由用户指定）：依次关闭 Cinematic Liquid SPH 的 frame-driven timestep、per-substep impulse clear、viscosity race、atomic scatter ordering；之后再做 Liquid Lab / Explore、GPU Burn Unlimited Soak 和 VRAM Integrity Soak。四项关闭前 SPH 始终 `_preview`，旧结果合同不变。
5. **Windows 完整公开发布收口**仍开放：冻结 `report_worker`，签名证书，在干净 Windows 10/11 VM 验收 bundled RenderDoc 和完整 GUI-first Setup 安装/自定义路径/升级/卸载。
6. 之后按第 2 节顺序继续 Android → iOS → Debian → WebGPU → HarmonyOS → **Windows 7 Aero GUI** → PS3（探索）→ Lumia 1520 legacy WP（探索）→ iPhone 4 legacy iOS（探索）→ Dual-GPU Collaboration → HTC HD2（探索）；后期 RT/路径追踪/厂商超分不得抢占用户当前指定的平台刀。

## 10. 验证记录

### 2026-10-17 树莓派 4：第一台 ARM Linux 实机；修掉 16M 粒子派发超限

- **设备：** 树莓派 4B Rev 1.5（4 GB），Raspberry Pi OS 64 位（Debian 13 trixie），Mesa 26.2.2（V3DV，Vulkan 1.3，64 位时间戳），走 Wi-Fi，没接显示器。在板子上用 GCC 14 编译（约 15 分钟），`ctest` 6/6。散热是 Pimoroni Fan SHIM（GPIO18，一开始没插到底），用户加了 `gpio-fan` 50 °C 起转；全程最高 60.8 °C，`get_throttled` 一直 0x0，负载下 CPU 1.8 GHz、GPU 500 MHz。
- **测法：** 桌面版完整测试里 V3D（`--gpu 0`）那一组任务加两项 CPU，用和 suite 一样的参数逐个跑（llvmpipe 那一组没跑）；没有显示器。副本 `out\linux\RaspberryPi4-series-20261017-161555\`，带 `DEVICE-STATE.txt`、`REMOVED-ENTRIES.txt`、每 2 秒的温度频率记录。构建步骤写进 `docs/building.md`。

  | 项目 | 成绩 |
  |---|---|
  | 粒子 headless 65K / 1M / 4M / 16M | 2.01 / 1.98 / 2.02 / 1.98 GB/s |
  | GPU Burn 16 / 64 / 256 步 | 0.027 / 0.035 / 0.039 Gpix-step/s |
  | GPU 显存读取（V3，473.5 MiB，从 118 MiB 起完整遍历） | 2.95 GB/s，到平台 |
  | CPU 单核（4 核一样）/ 全核 | 74.3 / 296.3 MWork/s（每单元约 24 个周期） |
  | CPU 内存读取 单线程 / 全核 | 4.0 / 3.8 GB/s |

- **不是成绩、从副本里删掉的：** 显示画面的粒子 65K/1M/4M 读到 70.5/5.5/4.9 GB/s，计算时间戳在同一步里从 0.01 ms 跳到 93 ms，没有框住计算；headless 是 2.0。显示画面的 16M 报交换链过期，没跑起来。后来查清：这些显示画面的项目并没有跑在 Xvfb 上，而是连到了树莓派自己那个没接屏幕仍在运行的桌面（labwc，Wayland，GLFW 3.4 默认连 `wayland-0`）；强制走 Xvfb 时 V3D 根本不能输出画面（`Requested GPU index unsuitable`）。安卓和鸿蒙上用的那个“计算时间戳对墙钟”的启动检查在这里打开试过，检查本身通过（324 ms 对 324 ms），正式跑仍然错，所以没有保留这个改动。显示画面的项目要接显示器或 HDMI 假负载插头再测。GPU Burn 也是在这个没屏幕的桌面上跑的，16 和 256 步的 GPU 时间和帧时间对得上，成绩先保留，接上屏幕后也要核对一次。流体被拒（没有可线性过滤的 R32F 3D 纹理，和 Adreno 一样）。
- **引擎 bug（已修）：** 粒子测试每步派发“粒子数 / 256”个工作组，16M 就是 65,536 个，比 Vulkan 规范保证的下限 65,535 多一个。V3D 正好只允许 65,535，于是这次派发什么都没做（0.044 ms 一步，15,086 GB/s）。现在 `CreateComputeDescriptorResources` 会在超限时把一步拆成几次派发，每次绑定缓冲区的一段（着色器从绑定起点开始编号，所以计算完全一样）；上限够的设备命令和以前一模一样（5090 的上限远大于 65,536，仍是一次派发；Windows 上只编译并跑了 `ctest` 6/6，没在 5090 上跑测试）。树莓派上 16M 改后 1.98 GB/s，和其他档一致。OpenGL ES 本来就会检查这个上限并拒绝（几台设备没有 OpenGL ES 16M，可能就是这个原因，没核对），移动端的能力检测也照旧算这个上限，都没改。手机和电视的这个上限没有记下来过；它们的 16M Vulkan 成绩数值合理，说明一次派发在那里是生效的（上限更大，或驱动容忍越界）。拆分只在超过上限时才启用，计算量不变，旧成绩照样可比。

### 2026-10-17 显存测试 V3：完整遍历的起点随设备上限变化，上限低的手机也能到平台

- **问题：** V3 从 256 MiB 起才按完整数据集遍历，而"到平台"要求最大的三个点都是完整遍历，所以上限低于 512 MiB 的设备永远到不了平台。已测的 15 台安卓和鸿蒙设备里有 9 台是这样：128 MiB 的 MIX Fold 3、Quest 3、黑鲨 5 Pro、Quest 2；256 MiB 的 MIX 4、黑鲨 3 Pro、Mate 40 Pro、TCL 电视；342 MiB 的小米电视。
- **修法：** `MemBwStreamingMinBytes`（`src/vulkan_membw.h`）。
  - 上限 ≥ 512 MiB：仍从 256 MiB 起，电脑、Mac、iOS 默认设置的测法完全不变。
  - 上限更小：从上限的四分之一起（取整 MiB），至少 32 MiB。128 MiB 从 32 MiB 起，平台看 64/96/128；256 MiB 从 64 MiB 起，看 128/192/256；342 MiB 从 85 MiB 起，看 192/256/342。
  - 起点按数据校验之后的实际上限算（校验失败会把缓冲减半，例如 Adreno 660 从 512 退到 256）。分派方式、`dispatches_per_sample`、平台判定和保存的 `streamingMinMiB` 都用同一个值；系统内存那一段也一样。
  - 版本号不变（`membw_v3_streaming`）：上限 ≥ 512 MiB 的结果和以前完全一样，上限更小的设备以前 V3 只会得到"未到平台"。安卓、鸿蒙、苹果前端都不读 `streamingMinMiB`，不用改。
- **验证：** Windows 编译和 `ctest` 6/6（`vulkan_membw_results_test` 加了各档上限的起点和尾部用例），安卓 APK、鸿蒙 HAP 和 `check-mobile-suite.cjs` 通过。
- **真机（同日，每台只跑显存一项、一次）：** 九台的读取都到了平台，写入和复制除黑鲨 5 Pro 的写入外也都到了。TCL 电视没有 GPU 时间戳，用墙钟计时，所以也验证了墙钟这条路。

  | 设备 | 上限 | 起点 | 读取（最大三点） | 旧版 V1 |
  |---|---|---|---|---|
  | 黑鲨 5 Pro（Adreno 730） | 128 MiB | 32 MiB | 40.2/40.3/40.3 → 40.3 | 41.9、44.2（未到平台） |
  | MIX 4（Adreno 660） | 256 MiB（512 校验失败减半） | 64 MiB | 29.6/29.0/29.1 → 29.1 | 32.7 |
  | 小米电视（Mali-G57） | 261.5 MiB（按可用内存，上次 342） | 65 MiB | 5.02/5.03/5.03 → 5.04 | 5.23、5.24 |
  | MateBook Pro（Maleoon 916） | 1024 MiB | 256 MiB（不变） | 53.7/55.0/55.1 → 55.0 | 52.6、60.2 |
  | TCL 电视（Mali-G52，墙钟） | 256 MiB | 64 MiB | 2.18/2.20/2.20 → 2.20 | 2.24、2.26 |
  | MIX Fold 3（Adreno 740） | 128 MiB | 32 MiB | 56.7/55.9/54.9 → 55.9 | 51.8、53.0、56.8（未到平台） |
  | Switch（Tegra X1，内存 800 MHz） | 439 MiB（按可用内存，上次 580.5） | 109 MiB | 10.4/10.2/10.5 → 10.46 | 11.2（早于数据校验） |
  | Quest 2（Adreno 650） | 128 MiB | 32 MiB | 14.8/13.8/15.2 → 14.86 | 14.16（未到平台） |
  | Quest 3（Adreno 740） | 128 MiB | 32 MiB | 30.9/33.1/32.3 → 32.3 | 34.2、32.6（未到平台） |

  - 黑鲨 5 Pro 的写入差一点：128 MiB 那个点这一轮波动大（中位 28.5，最好 37.6），三点差距超过 20%。分数只看读取，不受影响。
  - MIX 4 比 V1 低 11%：两版测法相同的 6–48 MiB 这天也低约 6%，大半是手机当天的状态；V1 在 96 MiB 以上反而比它自己 6–64 MiB 那段高，V3 曲线没有这个现象。
  - 成绩副本在 `out\adb\blackshark-SHARK-KTUS-A0-20261017-035416`、`out\adb\Xiaomi-2106118C-20261017-040126`、`out\adb\xiaomi-MiTV-MFFU1-20261017-041143`、`out\adb\TCL-Smart-TV-Pro-20261017-145138`、`out\hdc\HUAWEI-HAD-W24-20261017-035734`，已按各台以前的 `REMOVED-ENTRIES.txt` 删掉已知错误的记录（每台都是上次最终副本加今天这一条）。小米电视的编号 `20261017-011228-396-membw` 用的是电视自己的时钟，比电脑慢 3 小时。
  - MIX Fold 3 是折着测的（装了散热背甲，展开不了）。折叠时 `mi_thermald` 把 GPU 压低两档（615/719 MHz）、大核压到 2342/3360 MHz；`cmd device_state state 3` 假装展开没用，小米按真实铰链限频。用户用 su 运行了我写的脚本（`/data/local/tmp/uncap.sh` 只放开上限；`uncap2.sh` 停掉 `mi_thermald` 并清掉 CPU/GPU 降频档位），GPU 回到 719 MHz，CPU 上限没放开（另一套机制），显存测试不看 CPU。10-11 有一次 root 调频对照（`out\adb\mixfold3-root-tuning-20261011\`）：V1 显存读取默认频率 52.66、最高档 52.68，频率不影响这项。状态记在副本旁的 `DEVICE-STATE.txt`。恢复：`su -c start mi_thermald` 或重启。
  - MIX Fold 3 的 PC 副本 `out\adb\Xiaomi-2308CPXD0C-20261017-152220`：按以前的清单删 48 条；另外删了 10-11 那次调频对照里最高档的 15 条和一条记录里没有、在恢复设置途中跑的全核测试（`20261011-133738-134-cpu-multi`），共 16 条，理由写在 `REMOVED-ENTRIES.txt`；那次默认频率的 20 条保留（和之前的成绩对得上）。
  - Switch：用户那次开机内存频率是 400 MHz，经用户同意 `adb reboot` 后回到 800 MHz（和 10-08 全套测试一样），测试中每 10 秒读一次都是 800。比 10-08 的 V1（11.2，800 MHz）低 7%。状态记在副本旁的 `DEVICE-STATE.txt`；副本按 10-08 的清单删了 30 条。用户之后会给 Switch 重刷安卓，到时整套重测。
  - Quest 2 的读取从 1 MiB 到 128 MiB 一直平在 14–15（写入 21.4 反而更高），和 10-08 的曲线一样；应用在它上面只拿到受限的频率。副本按以前的清单删了 10 条。
  - Quest 3 测时电池 40 °C（温控状态 0），副本按以前的清单删了 24 条。
  - 至此所有有 Vulkan 驱动的安卓和鸿蒙设备都有算数的显存成绩：以前没到平台的 4 台（MIX Fold 3、Quest 3、Quest 2、黑鲨 5 Pro）和早于数据校验的 Switch 都已用新版补测并到平台。电脑上的完整回归等用户安排。

### 2026-10-17 Windows 上显存测试随机作废（已修）；V1 与 V3 可比，非苹果设备不用重测；两个数学小测试的结论

- **显存测试随机作废：** 拉取 Mac 那边的 8 个提交（到 `2832f08`）后，在 RTX 5090 上跑 `--workload membw`，连续三次都中途作废，位置都不同：12 MiB 读取、256 MiB 读取、192 MiB 复制。报错是 "Vulkan timestamps became unreliable during the sweep"。
  - 原因：10-14 的 `13aeafd` 对每个样本都检查"GPU 时间不能短于墙钟的一半且相差超过 2 ms"。Windows 上单次提交的墙钟时间会带上几毫秒的线程唤醒延迟，于是被误判。
  - 修法（`src/vulkan_membw.cpp`）：每个样本只排除不可能出现的值（没有区间，或 GPU 时间比墙钟还长）；"少算了"改为每个数据点检查一次，用该点墙钟最短的那次提交来判断（`BeginCoverage` / `CheckCoverage`）。测出来的数不变。
  - 修后在 5090 上跑了 4 次，全部完整完成（119 个点）。用户在看视频时那两次，读取中位数只有 1360–1366；停了视频后两次是 1530.3 和 1538.1。
- **V1 与 V3 可比：**

  | 设备 | V1 | V2 | V3 |
  |---|---|---|---|
  | RTX 5090 读取平台 | 1528–1542 | — | 1530–1538 |
  | M4 Pro | 252.4 | 300.9 | 245.9–250.4 |
  | A12X（iPad8,7） | 56.76 | 57.90 | 56.66 |

  出问题的是 V2（10-14 引入，只在苹果设备上跑过）。新增 `LegacyV1VulkanMemoryComparable`（`src/result_quality.h`）：V1 已到平台、`timestampAnomalies=0`、编号不早于 `20261008-042956`，就当作和 V3 可比。
  - 门槛最初定在数据校验的提交时间 `20261008-045054`（`266a197`）。但带校验的版本在提交前就已经在四台手机上跑过（MIX 4、黑鲨 3 Pro、MIX 2S、小米平板 5，04:29–04:48，日志里都有 "Data check"），所以门槛改成最早有校验的那次运行。
  - 按 PC 上的成绩副本核对：10 台被接受。MIX Fold 3、Quest 3、黑鲨 5 Pro、Quest 2 没到平台，Switch 早于校验，这 5 台想让显存成绩算数要补测这一项。桌面版补缺（`src/main.cpp`）、安卓和鸿蒙的一键测试（两个 `ResultsStore`）都接受这样的 V1，所以 19 台设备和电脑的显存成绩不用重测。转译的 MoltenVK V1 和 V2 不接受；苹果前端没有改。
  - 测试：`tests/vulkan_membw_results_test.cpp` 和 `SuitePlanTest.kt` 各加了 V1 的接受/拒绝用例；`scripts/check-mobile-suite.cjs` 原来还停在 V2 规则（按现有代码会失败），已改为 V3 加 V1。
- **两个数学小测试：** `scripts/gpu-math-probe-windows`（D3D11，另一个会话写的）在 5090 和核显上跑了，见 `out/test-results/gpu-math-probe-windows-20261017-031454/report.md`。
  - 按实测算力，M4 Pro 每 TFLOPS 的 Burn 分数是其他 GPU 的 0.46–0.83 倍，不是"一半"。
  - 只看计算循环，每步相当于 180–240 次 FMA 的时间，各 GPU 相近。
  - 真实 Burn 跑到自己纯计算循环速度的比例，M4 Pro 是 66%，5090 是 97–109%，核显是 69–86%。
  - 用 Burn 16/64/256 三档成绩把一帧拆成"循环后的固定开销"和"每步时间"（两档解出来，第三档验证，误差在 0.3% 以内）：
    - M4 Pro 在 Burn 里每步只有计算内核速度的 67%，收尾只相当于 2.4 步；
    - 5090（Vulkan）是 99%；核显 Vulkan 70%、DX11 88%。
    - 所以损失在片元着色器里跑循环这一步，不在收尾，也不在每帧两遍。
  - 为什么苹果的片元着色器更慢，只能在 Mac 上查。`scripts/metal-math-probe` 加了 `fma (literal)` 和 `Burn fragment` 两行（在 Windows 上写的，没有编译过），交给 Mac 跑，记为 `APPLE-NEXT-TESTS.md` 的 M5。
  - 已更新 `docs/interpreting-gpu-burn.md`。
- 验证：Windows 编译和 `ctest` 6/6，安卓单元测试 5/5 和 APK，鸿蒙 HAP 和 `check-mobile-suite.cjs` 均通过。

### 2026-10-16 苹果设备在 GPU Burn 和粒子测试里偏弱的原因（只读分析，写进文档；Mac 小测试代码已写，未编译）

- **A15 大核 147 MWork/s 核对过，是对的。** 测小核时，陪跑的两个大核线程记下的实测频率是 2.94 GHz（两核同时满载时的最高频率）。按单核 3.23 GHz 算，每个工作单元 22.0 个周期。A12X 大核的频率也是实测的（2.47–2.49 GHz），每单元 22.3 个周期，两者一致。7 次运行都落在 146.7–147.7。
- **GPU Burn：** 每一步光线行进约要算 2 次 `atan`、9–11 次 `sin`、2 次 `cos`、1 次 `pow`、1 次 `exp`，所以这项量的主要是特殊函数的吞吐。苹果 GPU 每 TFLOPS 只跑出约 1.1，RTX 5090 和 9800X3D 核显是 1.9–2.5。
  - 已排除的原因：
    - 计时：A15 每帧 GPU 时间 75.8 ms，帧间隔 77.1 ms。
    - 转译：苹果设备上 Metal 原生最快。M4 Pro 的 Burn 256，Metal 9.66、MoltenVK 7.10、OpenGL 5.92；A15 的 Burn 64，Metal 1.555、MoltenVK 1.036。
    - 编译选项：Metal 版本来就开了快速数学。
  - 确切原因还没确认。写在 `docs/interpreting-gpu-burn.md`；Mac 上 10-14 之前的 Metal Burn（4.2–4.8）是旧的计时错误，同样写在里面。
- **粒子：** 16M 粒子的分数跟着各设备自己实测的内存带宽走。苹果把带宽转成粒子吞吐的比例是 0.49–0.67，安卓是 0.43–0.64，同一档。A15 分数一般，是因为它用的是 64 位 LPDDR4X，实测 30.9 GB/s，是这些旗舰里最慢的内存；A12X 的 128 位总线让两台 iPad 排在最前面。写在 `docs/interpreting-stream-bandwidth.md`。麒麟 9000S（0.27）和 X90（0.30）明显偏离，原因没查。
- **流体：** A15（Metal 移植版）12.93，和 15S Pro 的 12.81 并列移动端第一，不弱。
  - A15 比 A12X（6.1–6.9）快一倍，差距全在模拟（154 对 350–418 ms/帧）；渲染只差 1.2 倍（94 对 112–114）。
  - 原因：模拟每个子步要对约 12.6 MB 的网格（128×64×96 格，每格 16 字节）做大量分散的原子加法。GPU 缓存诊断的曲线显示，A15 到 32 MB 都还在缓存那一段，A12X 从约 6 MB 起就到了内存级的延迟。两边的测法不同（每条访问链 512 次读取对 8192 次），只能比曲线在哪里拐到顶。
  - 写在 `docs/interpreting-cinematic-liquid.md`。
- **`scripts/metal-math-probe/`：** 在 Mac 上用计算着色器量 FMA、`sin`、`cos`、`atan2`、`exp`、`pow`、`sqrt`、除法的吞吐（以 FMA 为基准的比值），外加一份原样复制的 GPU Burn 单步。用 `bash scripts/metal-math-probe/run.sh` 运行，结果存到 `out/test-results/metal-math-probe-<时间>/`。
  - 在 Windows 上写的，没有编译过。`mathFloatingPointFunctions` 需要 macOS 15 SDK。
  - 要和其他厂商的 GPU 对比，还得写同样内核的 Vulkan 或 D3D 版本，现在还没有。
- 尚未提交。

### 2026-10-16 CPU 算力测试：M4 Pro 单核偏高是因为 macOS 编译掉了内核里的分支（已修，测试本身不变）

- **起因：** 把苹果设备和安卓、电脑放在一起排名时，M4 Pro 大核 312 MWork/s。按实测 4.13–4.17 GHz 算，每个工作单元约 13.3 个周期；而 A15、A12X 约 22，Arm 大核（A77–X925）约 16–17，9800X3D（Zen 5）约 25。
- **原因：** 内核里的分支 `(a ^ b) & 1` 取的是伪随机数的最低位，谁都猜不中，一半的工作单元要付猜错的代价。clang 编译 arm64 macOS 时默认目标 CPU 是 `apple-m1`，会把这个分支改写成 `csel`/`fcsel`，于是 M4 不付这个代价。用本机 NDK 的 clang 19（不是 Apple clang）编同一段内核核对过：
  - 默认目标下，macOS 有 4 条条件选择；iOS、安卓、鸿蒙保留分支。
  - MSVC（x64 和 ARM64，`cl /FA`）也保留分支。
  - 在真实的 `cpu_benchmark.cpp` 里，安卓和鸿蒙 arm64 **预热**那份内核也被改写了（结果丢弃、不计时），**正式计时**那份一直保留着 `tbz`，所以以前的安卓、鸿蒙成绩没有问题。
- **改法：** `GPU_BENCH_KEEP_BRANCH`，在 if 和 else 两边各放一条空的 `asm volatile`，**只在 arm64 macOS 上生效**。两条的内容必须不同，否则会被合并挪到判断之前，等于没加。
  - 第一版对所有 clang/GCC 平台都生效，结果 32 位 x86 安卓（小米平板 2）正式计时循环的寄存器分配变了，arm64 的预热代码也变了，所以收窄到只对 arm64 macOS。
  - 验证：
    - 安卓 4 个 ABI、鸿蒙 2 个 ABI：整份 `cpu_benchmark.cpp` 改动前后编出的汇编逐行相同（只去掉了 `.loc`/`.file` 行号记录）。
    - MSVC x64 和 ARM64：去掉按行号生成的标号和名字哈希后逐行相同。
    - 独立那份内核：macOS 默认目标和 `-mcpu=apple-m1` 都恢复成了分支。
    - Windows 编译和 `ctest` 通过。
- **成绩分组：** arm64 Mac 的 CPU 算力成绩版本名里多一段 `_kept_branch`。改动前的 arm64 Mac CPU 成绩留在旧组里，不和其他平台比。Intel Mac 不受影响（`x86_64-apple-macos` 本来就保留分支）。
- **用户的决定：** 测试流程和内容不变，不新增测试，除 Mac 外不重测。9800X3D 和骁龙 8 Gen 2 打平是这项测试本身的特点（串行链，加上一半猜错的分支），已写进 `docs/cli-reference.md` 的 "What the score measures"。
  - 当天在 9800X3D 上用临时代码做过对照：1 条链、分支一半猜错，208–216 MWork/s；同样的分支改成 4 条链，218；4 条链、分支 16 次里 1 次走另一边，556；4 条链、分支总能猜中，669。这些只作参考，没有采用。
- **待办：**
  - 在 Mac 上用 Xcode 自带的编译器反汇编确认分支确实保留了，然后重跑 M4 Pro 的 CPU 算力测试（大核、小核、全核）。
  - Linux 上用 GCC 编译的版本（Jetson）没有核对过。
- 尚未提交。

### 2026-10-10 第十九台设备：小米 15S Pro（玄戒 O1，Immortalis-G925）——第一台玄戒；前台应用只能用 10 个核心里的 8 个；29 项里 27 项有成绩

- **设备：** `25042PN24C`（代号 dijun，序列号 `0B02500081A00540`），Android 17（API 37），16 GB 内存，交换 16 GiB。玄戒 O1 是 10 核：`cpu0-1` Cortex-A520（1795 MHz），`cpu2-3` Cortex-A725（1891 MHz），`cpu4-7` Cortex-A725（3398 MHz），`cpu8-9` Cortex-X925（3898 MHz）；开跑前四组都在各自的频率上限。GPU 是 Mali-G925 Immortalis（驱动 `49.1.0`）。电池 31.6 → 34.5 °C，系统在每项开始前都没有报温控。
- **App 只能用 8 个核心。** `/dev/cpuset/top-app/cpus` 是 `2-9`，两个 A520 不给前台应用用（`background` 是 `0-3`）。这是这台手机对所有应用的做法，没有改动。所以“全核”是 8 线程，两个小核没有成绩。CPU 日志里 `logical_count=8` 对不上 10 核，就是这样查出来的。
- **新加的 Vulkan 时间戳核对在这颗 Mali 上通过**（GPU 时钟 25.7 ms，一帧接一帧 20–24 ms，墙钟 25.9 ms），照常用时间戳。Adreno 上仍然没验过。
- **窗口粒子的“计算”时间在这台上明显偏长，原因多半是时间戳的归属，不是算得慢：**
  - 同样一步，窗口对 Headless：100 万 3.70 对 1.41 ms，400 万 9.8 对 5.9 ms，1600 万 27.9 对 24.1 ms。
  - 窗口模式开跑的前半秒，一步是 1.05 ms（和 Headless 一样），之后稳定在 3.6–3.8 ms；而整帧耗时几乎没变（6.3 → 7.3 ms），是“渲染”那一段变短了、“计算”那一段变长了。帧一帧接一帧之后，两段之间的那个时间戳往后挪了。
  - 这和 Maleoon 的方向正好相反（那边是计算读成 0）。开跑前的核对只查“盖得太少”，查不出“盖得太多”，所以通过了。**这台看 Headless 的数。** 引擎没有改。
- **正式成绩**（完整副本 `out\adb\Xiaomi-25042PN24C-20261010-213704\results.json`，58 条，没有要删的；测试间等待 5 秒）：

  | 测试 | 成绩 |
  |---|---|
  | Vulkan 粒子，Headless，65K / 1M / 4M / 16M | 31.0 / 29.7 / 28.3 / 27.8 GB/s |
  | Vulkan 粒子，窗口，65K / 1M / 4M / 16M | 39.8 / 11.3 / 17.1 / 24.0 GB/s（见上） |
  | Vulkan GPU Burn 16 / 64 / 256 步 | 2.56 / 3.42 / 3.65 Gpix-step/s |
  | 流体 | 12.81 MParticle-step/s（快速检查 3 秒时是 17.60） |
  | 显存带宽 读 / 写 / 拷贝 | 44.6 / 33.7 / 41.8 GB/s（最大数据集 1713 MiB，数据校验通过；时间戳 86.7 ms 对墙钟 86.9 ms） |
  | OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 22.4 / 29.3 / 18.2 GB/s（400 万那项时间戳读不出，退回了墙钟） |
  | OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 20.3 / 12.2 / 20.2 GB/s |
  | OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 2.34 / 3.34 / 3.46（墙钟） |
  | OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 11.2 / 18.2 / 29.7 |
  | CPU 全核（8 线程） | 1179.19 MWork/s |
  | CPU 逐核 | A725 低频 118.9 / 119.0，A725 214.1–214.3，X925 227.1 / 228.9 |

  - 缓存 / 内存（X925 单线程读 / 写 / 拷贝，GB/s）：第一段 91.4 / 111.9 / 199.0，第二段 69.7 / 82.0 / 114.6，内存 37.8 / 52.4 / 46.0；全核（8 线程）内存 56.9 / 38.2 / 49.7。两类 A725 的内存“写”只有 9.3 和 11.9 GB/s（读是 33.0 和 22.7），偏低，没有查。
  - 虚拟内存：映射新页 4.01 GB/s（1022 ns/页）；换页目标是 16383 MiB 的交换；存储顺序读 2373、写 2655 MB/s，随机 4K 读 52.9、写 117.0 MB/s。
- **看这些数要知道的：**
  - 全核 1179 是 8 个线程之和（1551）的 76%：八个核一起跑时降了频。快速检查 1 秒时是 1285。
  - 引擎标了四项“越跑越慢”：Vulkan 粒子 65K 窗口 13.5%、Headless 8.9%，OpenGL ES 粒子 100 万窗口 19.0%，OpenGL ES GPU Burn 256 步 12.6%。GPU 频率没有 root 读不到。
  - 和此前最快的手机 MIX Fold 3 比：粒子 Headless 29.7 对 27.2，GPU Burn 256 步 3.65 对 2.95，显存读 44.6 对 53.0，CPU 全核 1179（8 线程）对 937（8 线程）。
  - OpenGL ES 的 1600 万粒子 2 项跑不了，和别的设备一样。
- **当晚应用户要求改了一处：核心数同时报整机的。** 这台 10 核芯片在 App 里显示成“8 个逻辑处理器 · 8 个物理核心”，用户问为什么是 8 核。引擎数的是进程被允许使用的处理器，本身没错，但读起来像一颗 8 核芯片。
  - 改法：`ProbeCpuTopology` 在 Linux / Android / HarmonyOS 上另外读 `/sys/devices/system/cpu/present`，按同样的“共用核心的处理器列表”数出整机的处理器数和核心数；**只有整机比进程可用的多时**才报出来（`CpuBenchmarkReport::systemLogicalCount` / `systemPhysicalCount`，否则是 0）。
  - 出现在四处：`CPU_META` 和 `CPUMEM_META` 多两个字段 `system_logical_count` / `system_physical_count`；文字输出多一行 `Processors: 8 of the machine's 10 are open to this process …`；保存的成绩的 `workloadConfig` 里多 `systemLogicalProcessors` / `systemPhysicalCores`（只是字符串里多两个键，没有新增成绩字段，旧版本照常能读）；Android 和 HarmonyOS 的 CPU 页在核心数后面加一句“整机是 10 核 10 线程，其余的系统不给应用使用”。
  - Windows、macOS、iOS 的前端没有动：这几个平台上引擎不报这两个数。成绩和分组都不受影响。
  - 验证：这台手机上 `logical_count=8 physical_count=8 … system_logical_count=10 system_physical_count=10`，CPU 页截图确认了那句话；Windows 上输出和以前一样（16 / 8，没有多出字段），`ctest` 通过；HarmonyOS 包编译通过，但没有在鸿蒙设备上看过（那两台都能用全部核心，不会出现这句话）。
  - 这台此前存下的正式成绩里没有这两个键（是改之前跑的），没有为此重跑。Quest 2 同理，下次接上时会带上。
- **还没有提交。**

### 2026-10-09 更正：各设备有成绩的项数（按清理后的成绩文件逐项数出来的）

- 下面几节里有的标题写着“29 项跑齐”“29 项全过”，指的是 29 项都跑过了，**不是 29 项都有成绩**。做全设备汇总时按 `out\adb`、`out\hdc` 里清理后的 `results.json` 重新数了一遍：

  | 设备 | 有成绩 | 没有成绩的项 |
  |---|---|---|
  | Switch | 29 | 无 |
  | Quest 3、Quest 2、MateBook Pro、Mate 60 Pro、小米 15S Pro | 27 | OpenGL ES 1600 万粒子 2 项 |
  | MIX Fold 3、黑鲨 5 Pro、MIX 4、黑鲨 3 Pro、小米平板 5、MIX 2S | 26 | 流体；OpenGL ES 1600 万粒子 2 项 |
  | TCL 电视 | 26 | Vulkan 1600 万窗口；OpenGL ES 1600 万 2 项 |
  | 小米电视 | 25 | Vulkan 1600 万 2 项；OpenGL ES 1600 万 2 项（下面那节写的 24 是数错了） |
  | 小米 5C、小米平板 3、小米平板 1、小米平板 2 | 14 | 全部 Vulkan 项（没有 Vulkan）；OpenGL ES 1600 万 2 项 |
  | 小米 3 | 命令行 3 项 | 装不了 App |

- **流体在六台高通手机上都被引擎拒绝**：`Cinematic Liquid v2 fixed score requires linear-filterable R32_SFLOAT 3D images`，它们的驱动不支持对这种 3D 纹理做线性过滤。同是 Adreno 的 Quest 3 和 Quest 2 能跑。

### 2026-10-08 第十八台设备：华为 Mate 60 Pro（麒麟 9000S，Maleoon 910，HarmonyOS 7）——第一台鸿蒙手机；全核 CPU 成绩不再受绑核影响；29 项里 27 项有成绩

- **设备：** `ALN-AL00`，HarmonyOS 7.0.0.109（API 26），内核 HongMeng 1.13.0。麒麟 9000S：8 个核心、12 个逻辑处理器——`cpu0-3` 4 个小核（单线程），`cpu4-9` 3 个中核（各 2 线程），`cpu10-11` 1 个大核（2 线程）。GPU 是 Maleoon 910（Vulkan 驱动 `292.366.2691`）。内存 11.3 GiB，交换 12 GiB。hdc 走 USB（目标名 `FMR0223901075040`），所以一直在充电。和 MateBook 一样，温度、性能模式、频率都读不到。
  - **换一台鸿蒙设备要重新签一次名：** 调试签名里登记的是设备的 UDID，新设备装包报 `code:9568423 … the device is unauthorized, make sure the UDID of your device is configured in the signing profile`。让用户连着新设备，在 DevEco Studio 的 Signing Configs 里把 Automatically generate signature 取消再勾上，然后重新打包。
  - 手机的 toybox 没有 `getconf`，`hdc-suite.ps1` 改成先试 `nproc`。壳里看到的处理器数（8）比 App 看到的（12）少，脚本里注明了"壳看到的"。
- **刚才在 MateBook 上修的三处在这台上都直接生效：** 拓扑一次认对（8 核 12 线程）；CPU 成绩是 `scheduler_managed`（全核 12 个线程里系统让绑住 7 个）；Vulkan 窗口粒子的时间戳核对触发（每帧等完 86–108 ms，一帧接一帧 0.003 ms，墙钟 88–96 ms），改用墙钟。快速检查和正式系列里没有一条错的成绩，电脑副本不用删。
- **引擎改动（用户定的）：全核 CPU 成绩不再因为绑核而作废。**
  - 起因：MateBook 第一次跑时全核 1489.80 被标成无效没有保存，用户指出"全核总是有效的吧"。N 个线程占满 N 个处理器，绑不绑结果一样；线程在哪个处理器上只对逐核测试有意义。
  - 改法：
    - `src/cpu_benchmark.cpp`：全核成绩只要量到了就有效（`result.valid = result.workUnits > 0`），不再看绑住了几个；`pinned_threads` 和 `affinity=strict|partial|failed|scheduler_managed` 照常记录，没全绑住时多打一行说明。删掉了 `MultiAffinityIsValid` 和 `CPU_ERROR required_multicore_affinity_failed`。
    - `src/main.cpp`：两部分各自保存。逐核有无效的处理器时，逐核汇总不存、退出码仍是 3，但全核成绩照样存（以前是两个都不存，小米平板 3 上因此要另外单跑一次全核）。
    - 逐核的规则没有变：没绑住的处理器照旧无效。
  - Windows 上确认：编译、`ctest`（1 项，通过）、`--cpu-benchmark all` 冒烟——16 / 16 线程绑住，`strict_group_affinity`，退出码 0，和以前一样。
  - 文档改了三处：`README.md`、`docs/cli-reference.md`、`docs/building.md`。
- **正式成绩**（完整副本 `out\hdc\HUAWEI-ALN-AL00-20261008-191619\results.json`，57 条；测试间等待 5 秒）：

  | 测试 | 成绩 |
  |---|---|
  | Vulkan 粒子，窗口，65K / 1M / 4M / 16M | 1.47 / 5.79 / 7.24 / 7.53 GB/s（墙钟计时） |
  | Vulkan 粒子，Headless，65K / 1M / 4M / 16M | 8.92 / 8.01 / 7.97 / 7.86 GB/s |
  | Vulkan GPU Burn 16 / 64 / 256 步 | 0.39 / 0.52 / 0.55 Gpix-step/s |
  | 流体 | 2.64 MParticle-step/s |
  | 显存带宽 读 / 写 / 拷贝 | 29.3 / 35.5 / 28.9 GB/s（最大数据集 1024 MiB，数据校验通过；时间戳 146.1 ms 对墙钟 146.9 ms） |
  | OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 3.78 / 8.46 / 9.69 GB/s（墙钟计时） |
  | OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 3.44 / 9.20 / 10.74 GB/s |
  | OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.40 / 0.57 / 0.63 |
  | OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 7.36 / 11.07 / 15.28 |
  | CPU 全核（12 线程，由系统调度） | 697.10 MWork/s |
  | CPU 逐处理器 | 小核 40.2–41.6，中核 92.3–92.5，大核 111.7–112.0 |

  - 缓存 / 内存（大核单线程读 / 写 / 拷贝，GB/s）：第一段 78.5 / 75.8 / 123.7，第二段 24.3 / 32.0 / 42.0，第三段 14.6 / 32.1 / 30.5，内存 24.9 / 42.2 / 33.3；中核 46.0 / 58.7 / 91.2，内存 18.9 / 40.9 / 28.4；全核（8 线程）内存 23.8 / 37.7 / 27.5。
  - 虚拟内存：映射新页 4.07 GB/s（1005 ns/页）；换页目标是 12288 MiB 的交换；存储顺序读 1442、写 1384 MB/s，随机 4K 读 59.2、写 110.8 MB/s。
  - 和 MateBook Pro（麒麟 X90 / Maleoon 916）比：粒子 Headless 约 1/2.2，GPU Burn 约 1/1.9，流体 1/2.5，显存带宽约 1/2，CPU 全核 1/2.7。
- **看这些数要知道的：**
  - Vulkan 窗口粒子 65K 和 100 万偏低（每步 1.8 ms 对 Headless 的 0.3 ms，是墙钟计时的提交和等待），看 Headless 的数；400 万和 1600 万与 Headless 相差 10% 以内。
  - CPU 全核 15 秒是 697，开跑前 1 秒的快速检查是 800：手机撑不住最初那一秒的功率。逐处理器里第 8 号（算中核）读 112，是系统把线程放到了大核上。
  - 引擎标了两项"越跑越慢"：OpenGL ES 窗口粒子 65K（16.6%）和 OpenGL ES 2.0 GPU Burn 16 步（6.0%），都是负载很轻的项目；其余 GPU 项目稳定。
  - **小核从内存读只有 0.8 GB/s（写 16 GB/s，第一段缓存内读 12.6 GB/s），当晚查清了，是真的：**
    - 这台的 4 个小核是 ARM 原版的 Cortex-A510（MIDR `0x41 / 0xd46`，r1p1），中核（`0xd42`）和大核（`0xd02`）才是海思自己的。MateBook 的小核就是 `0xd42`，所以它没有这个现象。
    - 临时在引擎里加了一段独立的读循环（已删）：小核读 96 MiB，16 字节向量读 1.1–1.3、8 字节标量读 1.1–1.2、每条缓存行只读一次 1.1–1.4 GB/s。三种读法一样快，折合每条缓存行约 50 ns——既没有提前取，也没有两条同时取。加软件预取只到 2–3 GB/s。同一段循环在中核上是 18–21，大核 26–27。
    - 同样是 A510 小核的 MIX Fold 3 和黑鲨 5 Pro 从内存读 10–12 GB/s。这台的小核为什么没有预取（硬件配置还是固件设置），从 App 里看不到；查了公开的 A510 勘误列表，没有找到会关掉预取的那一条。
    - 不是测量问题，成绩保留。含义：这台手机上被调度到小核的任务，读大块内存会很慢；缓存内的计算不受影响。
- **当晚鸿蒙 App 改了三处界面（用户在深色模式下截图发现的，都在这台上截图确认过）：**
  - **深色模式下整页不变深，只有底部悬浮页签有深色阴影。** 鸿蒙的规则：应用没有 `resources/dark` 目录、也没有调用 `setColorMode` 时，系统颜色在深色模式下仍取浅色值；悬浮页签用的系统材质不受这条管。`EntryAbility.onCreate` 里加了 `setColorMode(COLOR_MODE_NOT_SET)`，应用跟随系统。
  - **卡片在深色下和页面一样黑。** `Card` 的底色从 `ohos_id_color_sub_background`（深色下是纯黑）换成 `comp_background_secondary`。浅色模式下的样子没有重新截图看（切换系统外观是用户的事），按色值应与原来几乎相同。
  - **缓存测试的卡片标题显示成“核心类别 0 · · 0 个核心”。** `ForEach` 的键只有“级别 + 模式”，开跑前那张占位卡片和真正的第 0 类撞了键，卡片没有重建，标题用的还是占位对象。键里加上了类别名和核心数。
  - OpenGL ES 的 1600 万粒子 2 项跑不了，和别的设备一样。
- **构建：** HarmonyOS（已签名）、Android（四个 ABI）、Windows（含测试）都用最终代码编过。
- **提交时 `ohos\build-profile.json5` 和那两个 Gradle 文件一样要排除**（里面是本机的签名配置）。

### 2026-10-08 第十七台设备：华为 MateBook Pro（麒麟 X90，Maleoon 916，HarmonyOS 7）——第一台鸿蒙真机；有了 hdc 版系列脚本；修了物理核心数、绑核口径和 Vulkan 窗口粒子的计时；29 项里 27 项有成绩

- **设备：** `HAD-W24`，HarmonyOS PC（设备类型 `2in1`），HarmonyOS 7.0.0.107（API 26），**内核是鸿蒙自己的（HongMeng 1.13.0），不是 Linux**。麒麟 X90：10 个核心、每个 2 线程，共 20 个逻辑处理器——`cpu0-3` 是 2 个小核（part `0xd42`，上限 2050 MHz），`cpu4-11` 是 4 个中核（`0xd43`，2009 MHz），`cpu12-19` 是 4 个大核（`0xd03`，2316 MHz）。GPU 是 Maleoon 916（Vulkan 驱动 `598.836.102`，OpenGL ES 3.2 `B312`）。内存 23.3 GiB，交换 50 GiB。hdc 走 USB，目标名 `5XQ0225430000594`。
  - **零售版鸿蒙的调试壳几乎什么都读不了：** `/proc/cpuinfo`、`/sys/devices/system/cpu`、电池、温控一律拒绝。电源、性能模式、温度、实际频率这次都不知道（让用户确认过插电）。App 自己能读 sysfs，所以拓扑和频率上限是从 App 的日志里拿到的。
  - App 在桌面上以窗口方式运行（没有最大化），GPU 测试照旧按 1280 × 720 绘制。
- **怎么装上去的（以后照做）：**
  - 真机只收签过名的包：`hdc install` 未签名的 HAP 报 `code:9568320 error: no signature file`（模拟器不查）。签名要用户在 DevEco Studio 里做一次：打开工程 → File → Project Structure → Project → Signing Configs → 勾选 Automatically generate signature（设备要连着，账号要登录）。之后 `scripts\build-ohos.ps1` 直接产出 `entry-default-signed.hap`。
  - **DevEco Studio 不肯打开带空格的路径**（仓库在 `VSCode Project` 下）。给仓库建了一个目录联接 `C:\Users\lzz28\mangekyo`，在 DevEco 里打开 `C:\Users\lzz28\mangekyo\ohos`。命令行构建不受空格影响，照旧在原路径下跑。
  - 自动签名把证书路径和加密过的口令写进了 `ohos\build-profile.json5`。**这个改动是本机专用的，不要提交**（和那两个 Gradle 文件一样留在工作区）。
- **新脚本 `scripts\hdc-suite.ps1`：** 和 `adb-suite.ps1` 同样的 29 项、同样的时长和顺序，换成 hdc 的做法（`aa force-stop` 后等 2 秒再 `aa start … --ps run '<test>' --pi seconds N`；日志用 `hilog -T Mangekyo,MangekyoEngine` 边跑边存；成绩用 `hdc file recv -b <包名> ./data/storage/el2/base/haps/entry/files/results/results.json` 取回）。结果放在 `out\hdc\<型号>-<时间>\`。温度、解锁状态读不到，所以没有 `-StartBelowC`，测试之间只能固定等待。`-Install` 优先装签过名的包。
- **问题一（已修）：10 个物理核心被认成 20 个。** 这个内核的 `topology/core_id` 是按逻辑处理器编号的（`cpu0` 是 0、`cpu1` 是 1，尽管它俩是同一个核心的两个线程），引擎按"包 + core_id"分物理核心，就分成了 20 个。真正的配对在 `topology/thread_siblings_list`（`0-1`、`2-3`……）。改法（`ProbeLinuxTopology`，`src/cpu_benchmark.cpp`）：物理核心按"共用这个核心的处理器列表"来认（先读 `core_cpus_list`，再读 `thread_siblings_list`），两个都没有才退回"包 + core_id"。普通 Linux / Android 上两种认法结果相同。改后是 10 核 20 线程，缓存测试的"全核"也从 20 线程变回每核一个线程。
- **问题二（已修）：CPU 成绩全部被判无效（20 个里 19 个"没能固定在自己的线程上"）。** 两层原因：
  - 这个内核的 `sched_getcpu()` 返回的是物理核心号（逻辑号的一半），引擎拿它和逻辑号比，除了 0 号全对不上。
  - 系统不让线程单独绑在某一个大核处理器上：请求掩码 `{12}`，`sched_setaffinity` 返回成功，读回来是 `4-19`。小核和中核可以单独绑。
  - 改法：鸿蒙上开跑前探一次（`ProbedOhosAffinity`）——每个处理器请求单独绑一下，看掩码是不是原样回来、`sched_getcpu()` 说的是不是这个处理器。**只要有处理器绑不住，这台设备的 CPU 成绩就按 `scheduler_managed`（由系统调度）的口径记，和 macOS 一样**，`workloadVersion` 是 `cpu_mixed_v1_scheduler_managed`；线程仍然会请求自己的处理器，系统听多少算多少。`sched_getcpu()` 不可信时不拿它判"中途被挪走"。模拟器是 Linux 内核，全部绑得住，仍然是严格口径。
  - 这台上的含义：全核 20 个线程里 12 个是绑住的；逐核里 8 个"大核"成绩其实是"系统把它放在某个大核上"。
- **问题三（已修）：Vulkan 窗口粒子 400 万读成 643 GB/s。**
  - 现象：计算读成 0.002 ms，它的耗时全算进了渲染，一帧的总时间是对的（27 ms）。100 万那档读 21.1 GB/s 也偏高。同样大小只算不画（Headless）是 15–18 GB/s，一直是对的。
  - 原因：这个驱动在"一帧接一帧提交"时把下一帧的计算提前执行，"计算开始 / 结束"两个时间戳打在了一起。单独一帧（提交后等它做完）时时间戳是准的，所以第一版核对（每帧都等）没查出来。
  - 改法（`VulkanBackend::CheckTimestampsAgainstWallClock`，只在 Android / HarmonyOS 上编译，只管窗口粒子测试）：开跑前把计算部分用三种办法各量一次——墙钟（计算单独提交、等完）、时间戳（每帧等完）、时间戳（一帧接一帧，取最差的一帧）。为了让提交和等待的开销不碍事，核对用的帧里计算做 1600 万粒子步。两种时间戳读数里只要有一个不到墙钟的一半，就改用今天下午加的同步墙钟计时。日志里是 `[Profiling] Timestamp check:` 一行。
  - 这台上的读数：每帧等完 38–41 ms，一帧接一帧 0.002 ms，墙钟 38–44 ms（1600 万那档连每帧等完也读成 0），四档大小全部触发。
  - **同步墙钟计时里补了一处：** 计时开始前先 `vkQueueWaitIdle`。这个驱动在 1600 万粒子的帧实际画完之前就把那一帧的 fence 置位了，剩下的活被算进了下一帧的计算（一步 42 ms 读成 5.5 秒，0.12 GB/s）。加了之后是 15.87 GB/s，和 Headless 一致。
  - **在第二家 GPU 上确认过不误判：** 小米电视（Mali-G57，时间戳本来就对）读到 175 / 175 / 175 ms，不触发，照旧用时间戳。那次电视自己处在另一个状态（缩放芯片的中断很忙，连没改过的 Headless 和 OpenGL ES 都比下午低 16–28%），所以那几条快速检查不算成绩，目录里放了 `NOTE.txt`。**Adreno 上还没跑过这个核对**：下一台高通设备先跑一项窗口粒子，看这一行。
  - PC 不编译这段，对照跑了一次仍是时间戳（100 万 1842 GB/s）。
- **正式成绩**（清理过的完整副本 `out\hdc\HUAWEI-HAD-W24-20261008-184209\results.json`，56 条；测试间等待 5 秒）：

  | 测试 | 成绩 |
  |---|---|
  | Vulkan 粒子，窗口，65K / 1M / 4M / 16M | 4.04 / 11.58 / 16.78 / 17.57 GB/s（墙钟计时） |
  | Vulkan 粒子，Headless，65K / 1M / 4M / 16M | 17.88 / 17.94 / 17.84 / 17.79 GB/s |
  | Vulkan GPU Burn 16 / 64 / 256 步 | 0.72 / 0.97 / 1.04 Gpix-step/s |
  | 流体 | 6.60 MParticle-step/s |
  | 显存带宽 读 / 写 / 拷贝 | 60.2 / 65.3 / 61.2 GB/s（最大数据集 1024 MiB，数据校验通过；时间戳 72.4 ms 对墙钟 73.3 ms） |
  | OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 9.12 / 20.76 / 24.23 GB/s（墙钟计时） |
  | OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 8.82 / 21.70 / 24.30 GB/s |
  | OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.78 / 1.09 / 1.21 |
  | OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 18.25 / 27.24 / 30.74 |
  | CPU 全核（20 线程，由系统调度） | 1901.70 MWork/s |
  | CPU 逐处理器 | 小核 70.3 / 72.0 / 82.1 / 81.9，中核 106.4–107.4，大核 125.0–125.1 |

  - 缓存 / 内存（单线程读 / 写 / 拷贝，GB/s）：大核 第一段 88.1 / 113.1 / 189.6，第二段 47.4 / 69.2 / 66.1，内存 31.1 / 56.6 / 45.6；中核 77.0 / 97.9 / 160.2，45.1 / 70.2 / 61.9，内存 31.8 / 56.1 / 44.4；小核 46.8 / 51.6 / 85.6，内存 17.5 / 42.0 / 27.4；全核（10 线程）内存 58.7 / 66.8 / 57.6。缓存大小 App 读不到，各段是从曲线上认的，第一段一直延伸到 768 KiB–4 MiB，所以"L1 / L2"的名字在这台上不可靠。
  - 虚拟内存：映射新页 2.47 GB/s（1658 ns/页）；换页目标是 51200 MiB 的交换（App 看不到是谁）；存储顺序读 2282、写 2359 MB/s，随机 4K 读 69.3、写 143.0 MB/s。
- **看这些数要知道的：**
  - Vulkan 窗口粒子在 65K 和 100 万时偏低：墙钟每步带着约 0.5 ms 的提交和等待（65K 是 0.65 ms 对 Headless 的 0.15 ms）。这两档看 Headless 的数。400 万和 1600 万两种计时一致。
  - OpenGL ES 的粒子比 Vulkan 快（400 万一步 6.9 ms 对 9.4 ms）。两边 Headless 的帧率（144 对 105）和各自的计时对得上，所以是这个驱动上两条路径真的不一样快。
  - 全核 1901.70 是逐处理器之和（2162）的 88%，是"10 个核心各跑一个线程"之和（约 1081）的 1.76 倍：这项测试从超线程里拿到的好处很大。
  - OpenGL ES 的 1600 万粒子 2 项跑不了（差一个工作组），和别的设备一样。
- **电脑副本删了 12 条：** 时间戳不可信时存下的 8 条 Vulkan 窗口粒子（含 643 和 784 GB/s 两条）、加"等队列空"之前的 3 条 1600 万（0.12 GB/s）、按 20 核跑的 1 条缓存测试。说明在各目录的 `REMOVED-ENTRIES.txt`，设备状态在 `DEVICE-STATE.txt`。
- **构建：** HarmonyOS（已签名）、Android（四个 ABI）、Windows 引擎都用最终代码编过。

### 2026-10-08 第十六台设备：TCL 电视（MT5889，Mali-G52，32 位系统）——Vulkan 驱动没有时间戳，给 Vulkan 后端补了墙钟计时；修了存储顺序读；29 项里 26 项有成绩

- **设备：** `Smart TV Pro`（代号 G05），联发科 MT5889，4 个 Cortex-A73、1300 MHz（就是上限，跑 CPU 测试时读到的也是它），Mali-G52（驱动 r25p1），Android 11，**系统只有 32 位**（`armeabi-v7a`），2.7 GiB 内存（可用 1.1–1.5 GiB），交换 0.9 GiB，界面 1920 × 1080。网络调试 `192.168.20.15:5555`，做法和上一台电视一样。
  - 脚本温度一栏的 42.4 °C 是电视报的固定值，它没有电池。
  - 接上时（17:15）电视正在自己更新 10 个应用并编译，占着 3 个多核，到 17:22 结束。第一次快速检查是顶着它跑的；正式系列在它空下来之后才开始（空闲时 400% 里约 40% 在忙，是电视自己的服务）。**以后接电视先看一眼 `top`。**
- **问题一（已修）：这台的 Vulkan 驱动没有时间戳，12 项 Vulkan 测试能跑但全都没有成绩。**
  - 现象：`[Profiling] Timestamps not supported -- disabled.`，测试正常跑完、有帧率，成绩是 `none`，存进历史的是 0。`cmd gpu vkjson` 里 `timestampValidBits` 是 0，`timestampPeriod` 也是 0。显存带宽不受影响（它本来就会退回墙钟）。
  - 原因：DX11、OpenGL、OpenGL ES 三个后端都有"时间戳不可用就改用同步墙钟计时"的办法（`EnableSynchronizedTimingFallback`，成绩的 `workloadVersion` 带 `_sync_timing`，单独成组），**唯独 Vulkan 后端没有**，此前的设备都有时间戳所以没暴露。
  - 改法（`src/vulkan_backend.cpp` / `.h`）：
    - `CreateTimestampQueryPool` 里，驱动没有时间戳或建不出查询池时调用新的 `EnableSynchronizedTimingFallback`；`GetTimingMode()` 照另外三个后端返回 `synchronized_wall_clock`。多 GPU 交替渲染时不启用。
    - 一帧拆成两次提交：计算部分录进单独的命令缓冲（`BeginComputePart` / `EndComputePart`），提交后等它的 fence，量出计算时间；其余部分照常提交，再等这一帧的 fence，量出渲染时间。Headless 只有计算，提交后等 fence 即可。
    - 粒子、GPU Burn 走通用路径；四个流体录制函数（`RecordFluidFrame`、`RecordCinematicLiquidV2Frame`、`…SphFrame`、`…Frame`）各在开头和"计算 / 渲染"分界处加一行。**有时间戳时这两个函数原样返回帧的命令缓冲，录进去的命令和原来逐条相同。**
  - 验证一（PC，RTX 5090，同一个测试两种计时；临时加了个环境变量强制走墙钟，对完已删掉）：

    | 测试 | 时间戳 | 墙钟 |
    |---|---|---|
    | 粒子 1600 万，窗口 | 0.818 ms | 0.953 ms |
    | 粒子 1600 万，Headless | 0.757 ms | 0.897 ms |
    | GPU Burn 16 步 | 0.225 ms | 0.424 ms |
    | 流体（计算 + 渲染） | 8.80 + 4.36 ms | 9.78 + 4.56 ms |
    | 旧版流体 `fluid` | 0.050 ms | 0.232 ms |

    墙钟每次"提交再等"多出 0.15–0.2 ms，是这种计时固有的，帧越短占比越大，所以这类成绩才单独成组。PC 上不强制时仍然是时间戳，`workloadVersion` 和成绩都没变（粒子 100 万 1880 GB/s，GPU Burn 131.5，流体 244.3）。
  - 验证二（这台电视，Vulkan 对 OpenGL ES，两边都是墙钟）：粒子 Headless 2.09 / 2.39 / 2.45 对 2.07 / 2.44 / 2.40 GB/s；GPU Burn 一帧 1423 / 3737 / 12078 对 1333 / 3717 / 12008 ms。
  - 顺带看到的一个现象（没有改）：同步计时下，"预热结束"是在一帧画完之后才标记的，所以预热后的第一帧算进了"测量帧数"却没有计时样本，结果里帧数总比样本数多 1。只有一帧比预热还长的时候才看得出来：快速检查（3 秒）里流体和 256 步的 GPU Burn 量到 1 帧、0 个样本，存成 0。正式的 15 秒够用。
- **问题二（已修）：缓存 / 内存测试里的"存储顺序读"读成了 2222 MB/s。**
  - 现象：顺序读 1857 / 2222 MB/s，而同一块盘顺序写只有 14 MB/s。eMMC 不可能读到这个数。
  - 原因：这台的文件系统接受 `O_DIRECT` 打开，实际照样走文件缓存。引擎看到打开成功就认为是直接读写，不再在每遍读之前清缓存；八遍里第一遍之后都是从内存读的，而成绩取最快的一遍。随机读没有受影响（它排在写之后，写会让缓存失效）。
  - 改法（`src/cpu_membw.cpp`）：每遍顺序读和随机读之前都清一次这个文件的缓存（`posix_fadvise(DONTNEED)`），不再看是不是直接读写。直接读写真正生效的设备上没有东西可清，不受影响。
  - 改后：顺序读 237、写 21 MB/s。
  - 查了此前所有设备存下的这一项：只有这台和模拟器（21 GB/s，本来就不算成绩）不合理，其余都是正常的闪存速度（UFS 620–1800，eMMC 61–280 MB/s），不用重跑。
- **CPU 成绩核对过，是对的，只是 32 位构建在这项测试上慢得多。** 单核 18.0–18.8，全核 69.95；频率确实是 1300 MHz（一级缓存读 13.0 GB/s，上一台电视同样的核心 1800 MHz 时是 18.6，比例和频率接近）。同样的 Cortex-A73 在上一台 64 位系统里每 MHz 的成绩是这台的 2.6 倍：这项测试大量用 64 位整数运算。小米平板 2（x86，32 位）的 CPU 成绩偏低多半也是这个原因（那台没有 64 位的对照）。所以 `_32bit` 这一组不能和 64 位的比。
- **正式成绩**（清理过的完整副本 `out\adb\TCL-Smart-TV-Pro-20261008-175614\results.json`，47 条；29 项系列的日志在 `…-173408\`；测试间等待 5 秒；GPU 成绩全部是墙钟计时）：

  | 测试 | 成绩 |
  |---|---|
  | Vulkan 粒子，窗口，65K / 1M / 4M | 1.91 / 2.20 / 2.20 GB/s |
  | Vulkan 粒子，Headless，65K / 1M / 4M / 16M | 2.09 / 2.39 / 2.45 / 2.33 GB/s |
  | Vulkan GPU Burn 16 / 64 / 256 步 | 0.021 / 0.032 / 0.039 Gpix-step/s（256 步一帧 12 秒，只量到 1 帧） |
  | 流体 | 0.25 MParticle-step/s（一帧 12.9 秒，只量到 1 帧） |
  | 显存带宽 读 / 写 / 拷贝 | 2.3 / 2.0 / 3.0 GB/s（最大数据集 256 MiB，数据校验通过） |
  | OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 1.96 / 2.34 / 1.68 GB/s（工作组 128） |
  | OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 2.07 / 2.44 / 2.40 GB/s |
  | OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.022 / 0.032 / 0.039 |
  | OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 0.30 / 0.32 / 0.31 |
  | CPU 全核（4 线程，32 位） | 69.95 MWork/s |
  | CPU 单核 | 18.0 / 18.6 / 18.8 / 18.6（1300 MHz，四个相加 74.0，全核是它的 95%） |

  - 缓存 / 内存（单线程读 / 写 / 拷贝，GB/s）：L1 13.0 / 10.1 / 15.4，内存 5.9 / 9.1 / 9.8（没有分出二级缓存的台阶）；4 线程内存 10.1 / 4.6 / 9.0。
  - 虚拟内存：映射新页 1.33 GB/s（3076 ns/页）；换页目标是 895 MiB 的交换（应用看不到是谁，按 Android 的惯例是 zram）；存储顺序读 237、写 21 MB/s，随机 4K 读 28.8、写 3.4 MB/s。
  - 和上一台电视（MT9655 / Mali-G57）比：粒子约 1/2，GPU Burn 1/2.4 到 1/3.3，流体约 1/4.5，显存带宽约 1/2.3。
- **跑不了的 3 项：**
  - Vulkan 1600 万粒子，窗口：提交第一帧画面时丢 GPU 设备（`vkQueuePresentKHR` 返回 `VK_ERROR_DEVICE_LOST`）。缓冲建好了，计算那一步也做完了，同样的缓冲只算不画（Headless）能跑，所以出错的是“一帧画 1677 万个点”。最可能是分块渲染的 GPU 为这么多图元要的内存拿不到（可用内存 1.1–1.5 GiB）；驱动的内核日志没有读，这一条是推断。只丢了这个程序的上下文，电视和下一项测试都正常。
  - OpenGL ES 3.1 的 1600 万粒子 2 项：这台的工作组是 128，要 131072 组，驱动一次只允许 65535 组。
  - 算在 26 项里但要注意的一项：Vulkan Headless 1600 万粒子跑出了 2.33 GB/s，每步 288 ms，正好是 400 万的 4.2 倍；但驱动报的存储缓冲上限是 256 MiB，而这块缓冲是 512 MiB，跑完之后缓冲里的内容没有校验过。
- **电脑副本删了 16 条：** 没有计时办法的旧包存下的 11 条 Vulkan 0 分、快速检查里没量到帧的 3 条 0 分、存储顺序读不对的 2 条缓存测试。说明在各目录的 `REMOVED-ENTRIES.txt`，设备状态在 `DEVICE-STATE.txt`。
- **构建：** Android（四个 ABI）、Windows 引擎、HarmonyOS 包都用最终代码重新编过。

### 2026-10-08 第十五台设备：小米电视（MT9655，Mali-G57）——第一台走网络调试的设备，第一台有 Vulkan 的 Mali；29 项里 24 项有成绩

- **设备：** `MiTV-MFFU1`（代号 beekeeper），联发科 MT9655（平台名 mt5877），4 核 1800 MHz，Mali-G57（驱动 r44p1），Android 14，2.8 GiB 内存（可用 1.4–1.5 GiB），zram 1.5 GiB，界面 1920 × 1080。
  - **走网络调试：** `adb connect 192.168.20.17:5555`，电视上要用遥控器点"允许"。老式网络调试不会被 adb 自动发现，IP 要用户给。脚本的 `-Serial` 直接填 `192.168.20.17:5555`，全程没有掉线。
  - 没有电池，脚本的温度一栏是 0。电视的时钟比电脑慢约 3 小时，成绩 id 以 `20261008-13` 开头。
  - 电视平时后台就占着约一个核心（系统服务、语音唤醒、音频服务，空闲时 400% 里有 121% 在忙），CPU 成绩是带着这个量的。
- **问题（已修）：显存带宽在内存小的设备上卡死。** 第一次快速检查里这一项五分钟没有任何输出。原因：共用系统内存的 GPU 把全部内存报成自己的堆，引擎按"堆的四分之一"给源和目标各申请约 700 MiB，而这台可用内存只有 1.5 GiB，一直在换页。改法（`Runner::CreateBuffers`，`src/vulkan_membw.cpp`）：共用内存的设备上，每块缓冲再限制在"可用内存的四分之一"以内（读 `/proc/meminfo` 的 `MemAvailable`，只在 Linux 系生效，Windows 核显不变）。改后这台用 342 MiB，40 秒跑完。内存大的手机不受影响（它们本来就被别的上限压在 128–512 MiB）。
- **今天的两项核对在 Mali 上都正常：** OpenGL ES 时间戳可用（41.3 ms 对墙钟 41.8 ms），粒子成绩不带 `_sync_timing`；"等待核对"不触发（16 步等了 171 ms，剩 0.38 ms）。显存带宽的数据校验通过。粒子的工作组是 256，没有走 128 的回退。
- **正式成绩**（`out\adb\xiaomi-MiTV-MFFU1-20261008-163938\`，里面的 `results.json` 是清理过的完整副本，51 条；测试间等待 5 秒）：

  | 测试 | 成绩 |
  |---|---|
  | Vulkan 粒子，窗口，65K / 1M / 4M | 4.42 / 4.44 / 4.51 GB/s |
  | Vulkan 粒子，Headless，65K / 1M / 4M | 4.72 / 4.49 / 4.56 GB/s |
  | Vulkan GPU Burn 16 / 64 / 256 步 | 0.069 / 0.088 / 0.095 Gpix-step/s（256 步一帧 5 秒） |
  | 流体 | 1.14 MParticle-step/s |
  | 显存带宽 读 / 写 / 拷贝 | 5.2 / 7.7 / 7.7 GB/s |
  | OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 3.28 / 3.97 / 3.97 GB/s |
  | OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 3.93 / 3.96 / 4.00 GB/s |
  | OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.068 / 0.087 / 0.094 |
  | OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 0.93 / 0.77 / 0.79 |
  | CPU 全核（4 线程） | 224.23 MWork/s |
  | CPU 单核 | 62.3 / 67.1 / 66.7 / 66.9（1800 MHz，四个相加 263，全核是它的 85%） |

  - 缓存 / 内存（单线程读 / 写 / 拷贝，GB/s）：L1 18.6 / 14.1 / 21.3，L2 9.6 / 14.1 / 18.8，内存 6.6 / 9.2 / 9.7；4 线程内存 9.3 / 9.3 / 10.8。
  - 虚拟内存：映射新页 1.27 GB/s（3217 ns/页）；换页目标 zram（1535 MiB）；存储顺序读 277、写 143 MB/s，随机 4K 读 27.7、写 37.7 MB/s。
- **跑不了的 5 项：**
  - Vulkan 1600 万粒子（窗口和 Headless）：要一块 512 MiB 的缓冲，填它还要同样大的一块，这台内存不够。快速检查里窗口那项丢了 GPU 设备、Headless 那项跑通过一次（3.98 GB/s）；正式系列里两项都是 180 秒超时。不稳定，不算有成绩。
  - OpenGL ES 3.1 的 1600 万粒子 2 项（差一个工作组），和别的设备一样。
  - 另有一项算在 24 项里但要注意：两种 API 的 GPU Burn 256 步在 13 秒里只量到两三帧。
- **电脑副本删了 2 条：** 快速检查里存成 0 的两条 GPU Burn 256 步。

### 2026-10-08 第十四台设备：Meta Quest 2（骁龙 XR2，Adreno 650）——29 项里 27 项有成绩；改正了"等待核对"的误判，修了大粒子数下 GPU 设备丢失

- **设备：** `Quest 2`（序列号 `1WMHHA6G3P2281`，代号 hollywood），骁龙 XR2（SM8250），Adreno 650，驱动 `512.819.0`，Android 14，6 GB 内存，zram 3.6 GB。App 以 2D 面板在头显里运行；测试时头显要戴着或遮住感应器，接着电源。用户要求这台不做快速检查，直接跑正式系列；中途又要求把测试之间的等待从 30 秒缩到 5 秒（`-CoolDownSeconds 5`，脚本默认值没有改）。电池 30 → 40 °C。
- **"运行时加载 Vulkan"在真机上确认了。** 这是改完之后第一台有 Vulkan 的真机，Vulkan 的粒子、GPU Burn、流体、显存带宽都跑了。
- **问题一（已修，是我早上引入的）：新加的"等待核对"在高通上误判。**
  - 现象：OpenGL ES 粒子测试的日志里出现"Waiting for the GPU does not cover compute work"，计时被换成了"读回"。
  - 查法：临时在核对里加了一步，等完之后先调 `glFinish` 再读回。结果 `glFinish` 0.005 ms 就返回（活确实已经做完），而读回要 1.3–5.2 ms，且和工作量无关：**那是读回这个动作自己在这个驱动上的开销，不是没做完的活。**
  - 改法（`GlesBackend::CheckWaitCoversCompute`）：剩余量 = `glFinish` 的耗时 + 读回比"空读回"多出来的部分；工作量从约 400 万粒子步加到约 1600 万，让读回的开销相形之下很小；判据改成"剩余量超过等待时间的 30% 且超过 0.5 ms"。各驱动上"剩余 ÷ 等待"的实测：PowerVR 0.5 以上（最高 140），Adreno 650 改前 0.06–0.24、改后 0.03–0.05，Tegra K1 和 Intel 不到 0.03。
  - 影响范围：今天上午到下午之间，带旧判据的包只装到过平板 3（PowerVR，触发是对的）、平板 1（没触发）、平板 2（没触发）和这台。这台第一批 6 条 OpenGL ES 粒子成绩是按读回计时的（6.5 万粒子 4.33 / 4.10，改正后 5.24 / 5.24；100 万和 400 万差不到 3%），已从副本里剔除并重跑。
  - **平板 3 要用新判据再确认一次**：按上面的比值它应该照样触发，但新判据还没在 PowerVR 上跑过。
  - **当天下午在平板 3 上确认，结果又改了一版（现在代码里的是这一版）。** 上面那版在 PowerVR 上对 100 万和 400 万粒子触发，但对 6.5 万粒子不触发：连做 256 步再等一次，只有最后一小段没等到（230 ms 里的 18 ms），被摊薄了；而一帧里那一步恰恰就是"最后一段"，于是 6.5 万粒子又量成了 5.61 / 9.21 GB/s。
    - 现在的做法：照一帧的用法做 16 次"一步、等一次"，每次等完调 `glFinish` 看还剩多少（减去一次空的 `glFinish` 的耗时）；剩余总量超过等待总量的 30% 且超过 0.8 ms 就改用读回来等。读回不再参与判断，只是触发之后的等待办法。
    - 平板 3 上六项全部触发：6.5 万粒子等了 7.6 ms、剩 16.6 ms；100 万剩 201 ms；400 万剩 697 ms。成绩回到上午的水平（6.5 万 2.23 / 2.58，100 万 3.47 / 3.65，400 万 3.97 / 3.95），所以平板 3 上午的正式成绩有效，没有重跑。
    - **这一版在高通上也确认了（当天 16:15，MIX Fold 3，Adreno 740，快速检查，`out\adb\Xiaomi-2308CPXD0C-20261008-161518\`）：六项都不触发。** 16 步的等待合计 3.7–154 ms，`glFinish` 之后剩下的只有 0.001–0.07 ms。这台正是之前 `glFinish` 会按 4 ms 取整的那台：没有待办的活时它立刻返回，所以拿它当裁判没有问题。
    - 同一轮顺带确认了今天另外两处改动在手机上不影响成绩：运行时加载 Vulkan 正常；分批绘制后 Vulkan 窗口粒子 100 万 / 400 万 / 1600 万是 19.61 / 26.19 / 25.81 GB/s，和它早先的正式成绩（20.97 / 25.99 / 25.60）一致；显存带宽数据校验在 128 MiB 通过，56.8 GB/s。这些是快速检查，没有替换它已有的正式成绩。
- **问题二（已修）：Vulkan 窗口粒子测试在 100 万粒子及以上，一提交就丢 GPU 设备。**
  - 现象：`vkQueueSubmit failed (VkResult -4) -- GPU device lost`，100 万、400 万都是 4 秒内失败，重跑照旧；6.5 万正常；Headless（只算不画）到 1600 万都正常；OpenGL ES 画同样多的点也正常。同是 Adreno 650 的黑鲨 3 Pro 没有这个问题。
  - 原因范围：是"一次绘制调用画 100 万个以上的点"这件事。把同样的点按原顺序拆成每次 65,536 个来画，就全部通过了。更底层的原因（多半是头显的合成器等不到 GPU）没有查，日志里驱动没有留下说明。
  - 改法：`src/vulkan_backend.cpp` 的粒子绘制在 Android 和 HarmonyOS 上按 65,536 个点一次来画；桌面保持一次画完，不动。计算部分、着色器、成绩公式都没改。
- **命令行版 OpenGL ES 2.0 GPU Burn 和 App 对了数**（早上为小米 3 写的 `mangekyo_es2burn`，这是第一次在能装 App 的设备上比）：

  | 步数 | 命令行（离屏） | App | 差 |
  |---|---|---|---|
  | 16 | 13.47 | 11.86 | +14% |
  | 64 | 12.79 | 11.82 | +8% |
  | 256 | 15.25 | 15.83 | −4% |

  帧很轻（2 ms 一帧）时离屏的略快，帧重了就一致。小米 3 的帧是 69–1040 ms，属于"重"的一头，所以它那三个数可以认为和 App 的口径一致。
- **设备状态（写在 `DEVICE-STATE.txt` 里）：**
  - App 只能用 8 个核心里的 4 个：`top-app` 是 `cpu4-7`，即 3 个 A77 加 1 个大核，4 个小核留给系统。所以"全核"是 4 线程。
  - 系统还把这 4 个核心的频率压着：A77 是 1478 / 2419 MHz，大核是 2458 / 2842 MHz。这是头显对所有应用的做法，没有改动。
  - GPU 频率档位 305–587 MHz，系统上限 525 MHz，测试时读到 490 MHz。
  - **对照：Quest 3（XR2 Gen 2）当天下午接上读了一次。** 6 个核心应用全都能用（`top-app` 是 `cpu0-5`）。App 在前台时，`cpu2-5` 能到频率表顶端 2361.6 MHz，`cpu0-1` 被压在 1920 / 2054.4 MHz；没有应用在前台时 `cpu2-5` 也被压到 2208 MHz。GPU 上限 640 MHz（表到 690）。所以 Quest 3 的限制很轻（6–7%），Quest 2 则是砍掉一半核心再降频。写进了 `out\adb\Oculus-Quest-3-20261008-024339\DEVICE-STATE.txt`；这些数不是在它跑系列时读的。
- **正式成绩**（清理过的完整副本 `out\adb\Oculus-Quest-2-20261008-143256\results.json`，31 条；29 项系列的日志在 `…-141347\`）：

  | 测试 | 成绩 |
  |---|---|
  | Vulkan 粒子，窗口，65K / 1M / 4M / 16M | 11.99 / 8.15 / 7.57 / 8.01 GB/s |
  | Vulkan 粒子，Headless，65K / 1M / 4M / 16M | 8.16 / 7.61 / 7.75 / 7.72 GB/s |
  | Vulkan GPU Burn 16 / 64 / 256 步 | 0.42 / 0.57 / 0.63 Gpix-step/s |
  | 流体 | 1.69 MParticle-step/s |
  | 显存带宽 读 / 写 / 拷贝 | 14.2 / 21.5 / 16.8 GB/s（最大数据集 128 MiB，数据校验通过） |
  | OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 5.24 / 6.32 / 7.23 GB/s |
  | OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 5.24 / 6.48 / 6.71 GB/s |
  | OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.46 / 0.66 / 0.66 |
  | OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 11.86 / 11.82 / 15.83 |
  | CPU 全核（4 线程） | 359.23 MWork/s |
  | CPU 单核 | A77 89.3–89.8（1478 MHz），大核 136.2（2458 MHz） |

  - 缓存 / 内存（单线程读 / 写 / 拷贝，GB/s）：大核 L1 76.9 / 39.1 / 79.3、L2 35.9 / 29.9 / 60.9、L3 19.4 / 21.2 / 22.4、内存 13.1 / 17.8 / 17.7；A77 的 L1 44.6 / 22.6 / 45.6、L2 26.1 / 22.8 / 44.7、内存 14.1 / 19.2 / 18.2；4 线程内存 18.0 / 21.3 / 22.1。
  - 虚拟内存：映射新页 2.73 GB/s（1498 ns/页）；换页目标 zram（3583 MiB）；存储顺序读 1022、写 688 MB/s，随机 4K 读 24.9、写 40.4 MB/s。
  - 和同是 Adreno 650 的黑鲨 3 Pro 比，GPU 成绩低一截（Vulkan GPU Burn 0.42 / 0.57 / 0.63 对 0.41 / 0.64 / 0.77，粒子约 8 对约 13.5 GB/s）：头显把 GPU 限在 490–525 MHz。
- **跑不了的 2 项：** OpenGL ES 3.1 的 1600 万粒子（差一个工作组）。
- **电脑副本删了 10 条：** 按读回计时的 6 条正式成绩和 4 条快速检查成绩。

### 2026-10-08 第十三台设备：小米 3（Tegra 4，Android 4.4.4）——App 装不了，用命令行工具跑了 CPU 两项和 OpenGL ES 2.0 GPU Burn

- **设备：** `MI 3`（序列号 `0a8213b8`，代号 pisces，Tegra 4 版），4 个 Cortex-A15（上限 1810 MHz，32 位），NVIDIA Tegra GPU 只有 OpenGL ES 2.0，Android 4.4.4（API 19），MIUI 9 开发版 7.11.16，2 GB 内存，zram 768 MiB，内核 3.4.35。电池 26 °C。
- **App 装不了。** `adb install` 被 MIUI 拒了（`INSTALL_FAILED_USER_RESTRICTED`）；就算放行也装不上，App 最低要 Android 6.0（API 23），这是 Compose 的要求，降不下去。所以 29 项的系列在这台上一项也跑不了。
- **GPU：新写了一个命令行版的 OpenGL ES 2.0 GPU Burn（`mangekyo_es2burn`，`src/es2burn_cli.cpp`）。** 这块 GPU 只有 OpenGL ES 2.0，能跑的只有这一项。
  - 做法：不要窗口，用 EGL 的离屏缓冲（pbuffer，1280 × 720，和 App 的渲染尺寸一样），照 `GlesBackend::DrawEs2BurnFrame` 画同样的帧（同样的两个着色器文件、每帧两次绘制相加、同样的相位），用同样的办法计时（墙钟，以 EGL 栅栏结束），用同样的公式算成绩。和 App 的差别只有一处：不把画面交给屏幕。
  - 先用一个小探测程序确认了前提：这台允许 adb 下的程序创建 OpenGL ES 2.0 上下文和离屏缓冲，画完能读回正确的颜色。
  - **在 Android 4.4 上让程序能启动费了三次**：`atof` 在 4.4 的 C 库里不存在；NDK 的 C++ 流要 `strtof`，也不存在；最后发现只要用到一个 `std::string`，C++ 库的那个目标文件就会带进 `strtof`、`strtold`、`wcstof` 等一串 4.4 没有的函数。现在这个程序只用 C 库，链接时不带 C++ 库（`-nostdlib++`）。缓存 / 内存那个工具用静态链接绕开了同一个问题，但这个要用系统的 EGL，不能静态。
  - 每次运行结束时读回最后一帧正中的像素并打印，证明确实画了东西（这台是 `195 173 219`）。
  - **和 App 对数（当天下午在 Quest 2 上做了，见上一节）：** 帧重时两者相差 4% 以内，帧很轻时离屏的高 8–14%。这台的帧属于重的一头。
  - 成绩（15 秒、预热 2 秒，离屏）：

    | 步数 | 成绩 | 每帧 | 帧数 |
    |---|---|---|---|
    | 16 | 0.43 Gpix-step/s | 68.9 ms | 188 |
    | 64 | 0.46 | 255.9 ms | 50 |
    | 256 | 0.45 | 1039.5 ms | 12 |

    Tegra K1（平板 1）同一项是 3.50 / 4.47 / 4.97，Tegra 4 约为它的八分之一到十分之一。日志是 `25-…`、`26-…`、`27-gles2-gpu-burn-…-cli.log`。
- **改用命令行工具通过 adb 直接跑**（`mangekyo_cpumem`，`src/cpumem_cli.cpp`，原来就有，只有缓存 / 内存测试）：
  - 加了 `--compute`：按正式设置跑 CPU 算力测试（先全核，再逐核；15 秒、预热 0.2 秒、3 轮）。
  - `scripts/build-android-cpumem.ps1` 加了 `-Static`：NDK 最低只支持到 Android 5.0，动态链接的程序在 4.4 上跑不了，全部静态链接就可以。这台用的是 `-Abi armeabi-v7a -ApiLevel 21 -Static`。
  - 用法：`adb push out\android-cpumem\armeabi-v7a-static\mangekyo_cpumem /data/local/tmp/`，`chmod 755`，`cd /data/local/tmp && ./mangekyo_cpumem`（缓存 / 内存）或 `./mangekyo_cpumem --compute`。
  - **这两个工具只打印，不保存成绩。** 这台的结果只有日志，在 `out\adb\Xiaomi-MI-3-20261008-132013\`（`28-cpu-compute-cli.log`、`29-cpu-cache-memory-cli.log` 和三个 GPU Burn 的日志），没有 `results.json`。程序和两个着色器文件留在了手机的 `/data/local/tmp/`。`scripts/build-android-cpumem.ps1` 现在两个工具一起构建。
- **成绩（都是 32 位代码）：**
  - CPU 全核（4 线程）94.86 MWork/s。单核 22.79（`cpu0`，有效）；另外三个 22.6–22.8，被标成无效：这台的热插拔调速器（`cpuquiet`）平时只留 `cpu0`，逐核测量中途把核心关掉、线程被挪走，和平板 3 是同一种情况。四个核心一样，所以单核就是约 22.8，每 GHz 约 12.6（Tegra K1 的 A15 约 14）。
  - 今天加的"先把核心叫起来"在这台上起了作用：空闲时只有 1 个核心在线，两项测试都认出了 4 个。
  - **之后用户 root 强开了全部核心，逐核 4 个全部有效**（`28-cpu-compute-cli-all-cores-forced.log`）。命令由用户执行：`su -c 'echo 0 > /sys/devices/system/cpu/cpuquiet/tegra_cpuquiet/enable'`，再把 `cpu1`–`cpu3` 的 `online` 写成 1。读回来：开关是 0，空闲时 `online` 是 `0-3`。
    - 单核 23.51 / 23.65 / 23.66 / 23.66，全核 94.79（没强开时 94.86）。四个相加 94.5，和全核一致。
    - **这时的频率是 1428 MHz。** 压着它的有两层：4 个核心都在线时系统把上限压到 1606.5 MHz（`scaling_max_freq`）；MIUI 另外还设了一个 1428 MHz 的上限（`/sys/module/cpu_tegra/parameters/cpu_user_cap`），负载下读到的就是 1428 MHz。我一开始只看到第一层，说成了"约 1.6 GHz"，是错的。所以 23.6 是 1428 MHz 的 A15 的成绩，每 GHz 约 16.5。
  - **用户要求再测最高档：单核 1810.5 MHz 是 29.17**（`28-cpu-compute-cli-one-core-top-step.log`）。命令由用户执行：把 `cpu1`–`cpu3` 下线，`cpu_user_cap` 写 0，调速器改成 `performance`。跑的时候每 4 秒读一次：8 次都是 1 个核心在线、1810.5 MHz，芯片 40–48 °C。每 GHz 约 16.1，和 1428 MHz 时一致。
    - 这是"这颗芯片的一个核心在频率表顶端"的成绩，手机自己平时到不了这个状态；它平时的单核是上一条的 23.6（1428 MHz）。
    - **顺带的疑问（当天已查清）：** 按这个每 GHz 16 左右的水平，Tegra K1 的 A15 在 2218 MHz 应该有 36 左右，而平板 1 量到的是 31.2。把平板 1 重新接上，跑 CPU 算力测试并每 3 秒读一次频率（`out\adb\Xiaomi-MI-PAD-20261008-134627\cpu-clock-every-3s.txt`）：全核时是 2218.5 MHz（CPU 15 秒内从 51 °C 升到 86 °C）；**单核时调速器（smartmax）大部分时间只给 1836 MHz，偶尔 2116.5 MHz，上限本身也在 1836 和 2218.5 之间来回变。** 这一次单核 29.1–29.4、全核 131.49。29.2 ÷ 1.836 GHz 约 15.9，和 Tegra 4 一致。所以平板 1 的单核成绩是"约 1.84–2.1 GHz 的 A15"的成绩，不是 2218 MHz 的；它在 2218 MHz 下的单核按比例约 35–36，没有量。
      - **又试了把调速器改成 `performance`（用户执行）：单核 31.0–32.0，全核 130.92，但频率仍然稳不在上限。** 这个系统把 `scaling_governor` 设成了只读（0444），内核对没有写权限位的 sysfs 文件连 root 也拒绝，要先 `chmod 644` 再写。改成之后每 3 秒读一次（`out\adb\Xiaomi-MI-PAD-20261008-135130\cpu-clock-every-3s-performance-governor.txt`）：单核阶段约六成读数是 2218.5 MHz，四成是 1734 MHz——**是上限本身在 2231 和 1734 MHz 之间被系统来回改**，调速器只是跟着上限走。平均约 2.0 GHz，31.6 ÷ 2.0 ≈ 15.8，仍和 Tegra 4 一致。是谁在改上限没有查。这两条成绩（`20261008-135249-962-cpu-single`、`…-992-cpu-multi`）留在了副本里，`DEVICE-STATE.txt` 里标明了是 `performance` 调速器下量的；现在最新的清理过的副本是 `…-135130\results.json`（38 条）。
    - 手机上的这些改动到重启为止都在。成绩文件夹里的 `DEVICE-STATE.txt` 说明了三种状态。
  - **教训：** 读 `scaling_max_freq` 不够。还要读厂商自己的上限节点（这台是 `cpu_user_cap`），并在有负载时读一次实际频率。
  - 缓存 / 内存（NEON，单线程读 / 写 / 拷贝，GB/s）：L1 13.3 / 11.2 / 17.7，L2 7.8 / 9.3 / 9.9，内存 2.2 / 6.2 / 3.4；4 线程内存 2.6 / 4.9 / 3.1。
  - 虚拟内存：映射新页 0.76 GB/s（5366 ns/页）；换页目标 zram；存储顺序读 61、写 30 MB/s，随机 4K 读 6.0、写 3.1 MB/s。

### 2026-10-08 第十二台设备：小米平板 2（Atom x5，Android 6.0.1，x86）——App 在 Android 6.0 上原来完全不能用（已修）；29 项里 14 项有成绩

- **设备：** `Mipad 2`（序列号 `A3P42C30B441`，代号 latte），Intel Atom x5-Z8500（4 核 Airmont，睿频 2240 MHz），Intel HD Graphics（Cherry Trail），OpenGL ES 3.1 驱动 `4.61.52868-R`，没有 Vulkan，LineageOS 13 / Android 6.0.1（API 23），x86 32 位用户空间，系统报 7.7 GiB 内存，zram 512 MiB，1536 × 2048 屏。第一台 x86 安卓设备，第一块 Intel 核显，第一台 Android 6。
  - 这个系统没有 `battery` 服务，脚本读不到电池温度（表格里是空的）；`/sys/class/power_supply/battery/temp` 读到 29–38 °C。
- **问题一（已修，影响所有 Android 6.0 设备）：原生库加载不了，29 项全部被拒绝，CPU 测试也一样。**
  - 原因：`libmangekyo_jni.so` 在"需要的库"里写着 `libvulkan.so`，Android 6.0 没有这个库（Vulkan 是 7.0 加的），系统拒绝加载整个库。另外原生代码是按 `android-24` 编的，还引用了一个 6.0 没有的 C 库符号（`__pwrite_chk`）。App 的 `minSdk` 是 23，所以它能装上，但装上之后什么都跑不了；加载失败的异常还被 `NativeBridge.kt` 吞掉了，日志里只有"engine missing"。
  - 改法：不再链接 `libvulkan.so`。引擎调用的 91 个 Vulkan 函数都变成同名的函数指针（`android/app/src/main/cpp/vulkan_loader.h`，由构建强制放在引擎和桥接层每个源文件的最前面，引擎源码没有改动），库加载时用 `dlopen` / `dlsym` 从系统的 `libvulkan.so` 里填上（`vulkan_loader.cpp`）。系统没有这个库时，`vkCreateInstance` 返回 `VK_ERROR_INCOMPATIBLE_DRIVER`，Vulkan 测试照旧被拒绝，其他测试正常。函数名单在 `vulkan_functions.inc`：引擎新用到一个函数而没加进名单，链接时就报"未定义符号"，不会拖到设备上才出错。
  - 原生代码改成按 `android-23` 编（`app/build.gradle.kts`），和 `minSdk` 一致。
  - `NativeBridge.kt` 现在把加载失败的原因打到日志里。
  - **验证：**
    - 用 NDK 的工具核对了四种架构的库：需要的库里没有 `libvulkan.so`，强引用的符号在 API 23 的系统库里都有，没有导出任何以 `vk` 开头的符号。
    - 平板 2（Android 6.0.1，没有 Vulkan）：库加载成功，OpenGL ES 和 CPU 测试都能跑。
    - 本机模拟器（`Pixel_10a`，Android 17，Vulkan 走主机的 RTX 5090）：Vulkan 的粒子测试（带窗口和交换链）、显存带宽（数据校验通过）、GPU Burn 都跑完了。模拟器上的数字不算成绩，这里只证明运行时加载这条路是通的。
    - **还没有在真机的 Vulkan 设备上跑过。** 下次接上任意一台有 Vulkan 的手机时先跑一项 Vulkan 测试。
- **问题二（已修）：32 位 x86 的缓存 / 内存测试也没用向量内核**（和 32 位 ARM 一样的毛病）。开关放宽到"32 位 x86 且带 SSE2"（`CPUMEM_X86`），改后是 `vector=SSE2`。
- **这台是第一台 OpenGL ES 时间戳能用的设备**：`Timestamp check: 37.8 ms by the GPU's clock, 38.3 ms by wall clock`，所以粒子成绩的名字没有 `_sync_timing`，6.5 万粒子也不偏低。"等待核对"在 Intel 驱动上没有误触发（读回只多 0.15 ms）。
- **正式成绩**（`out\adb\Xiaomi-Mipad-2-20261008-114421\`，里面的 `results.json` 是清理过的完整副本，29 条）：

  | 测试 | 成绩 |
  |---|---|
  | OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 4.13 / 5.37 / 5.43 GB/s |
  | OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 5.15 / 5.32 / 5.31 GB/s |
  | OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.125 / 0.161 / 0.173 Gpix-step/s（256 步一帧 2.7 秒，量到 5 帧） |
  | OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 2.40 / 2.74 / 2.85 |
  | CPU 全核（4 线程，名字带 `_32bit`） | 105.58 MWork/s |
  | CPU 单核 | 29.40 / 30.31 / 30.16 / 30.14（四个相加 120.0，全核是它的 88%） |

  - CPU 是 32 位代码的成绩（见下一节关于 `_32bit` 的说明）：每 GHz 约 13，和 32 位的 A15（约 14）差不多。全核只有单核之和的 88%：四个核心都满载时这颗 Atom 到不了单核时的睿频。
  - 缓存 / 内存（SSE2，单线程读 / 写 / 拷贝，GB/s）：L1 12.1 / 24.7 / 34.0，L2 9.1 / 16.1 / 14.1，内存 4.1 / 3.4 / 3.9；4 线程内存 8.2 / 5.1 / 6.1。
  - 虚拟内存：映射新页 1.49 GB/s（2755 ns/页）；换页目标 zram；存储顺序读 128、写 78 MB/s，随机 4K 读 12.2、写 6.2 MB/s。
- **跑不了的 15 项：** Vulkan 13 项（没有 Vulkan）；OpenGL ES 3.1 的 1600 万粒子 2 项（同样差一个工作组）。
- **电脑副本删了 2 条：** 快速检查里用标量内核跑的缓存测试，和存成 0 的那次 GPU Burn 256 步。
- **脚本：** 这一轮用上了"被拒绝的测试不等冷却"，29 项正式系列 14 分钟跑完。

### 2026-10-08 第十一台设备：初代小米平板（Tegra K1）——第一台 32 位设备；32 位构建没用上 NEON（已修），29 项里 14 项有成绩

- **设备：** `MI PAD`（序列号 `331460AC`，代号 mocha），NVIDIA Tegra K1（4 个 Cortex-A15，只有 32 位），LineageOS 14.1 / Android 7.1.2（API 25），OpenGL ES 3.1 驱动 `334.00`，没有 Vulkan 驱动，2 GB 内存，没有交换分区，1536 × 2048 屏。App 以 32 位（`armeabi-v7a`）运行。电池 30–35 °C。
- **问题（已修）：32 位 ARM 构建的缓存 / 内存测试没用 NEON。** 快速检查的日志里是 `vector=scalar`，单线程 L1 读只有 3.6 GB/s。内核的 NEON 版本只在 64 位 ARM 下编译；它用到的指令 ARMv7 的 NEON 都有，所以把开关放宽到"32 位 ARM 且带 NEON"（`CPUMEM_ARM_NEON`，`src/cpu_membw.cpp`）。改后 `vector=NEON`，L1 读 15.6 GB/s。
- **不是问题、但要知道的：32 位下 CPU 算力成绩低。** 单核 30.9–31.4 MWork/s（A15 @ 2218 MHz），每 GHz 约 14，不到 64 位 A53（约 29）的一半。CPU 测试里有大量 64 位整数运算，32 位代码要用好几条指令才能做一次。这是这颗芯片以 32 位代码跑这个负载的真实水平，但不能当成"A15 比 A53 慢一半"来读。成绩里没有单独标出 32 位，要不要加标识等用户决定。
- **新加的"等待核对"在这块 NVIDIA GPU 上没有误触发**：按平时的办法等完之后，读回只多花 0.05–0.6 ms（等了 27–108 ms）。这是 PowerVR 之外第一台验证过的设备；高通和 Mali 仍然没有验证。
- **设备状态（写在 `DEVICE-STATE.txt` 里）：GPU 的频率由系统调速器按负载定，不是每项测试都在最高频。** 事后跑同样的测试、每秒读一次时钟：

  | 测试 | GPU 频率 | 内存时钟 |
  |---|---|---|
  | 粒子 400 万，Headless | 924 MHz | 924 MHz（最高） |
  | 粒子 6.5 万，Headless | 180–396 MHz | 300 MHz |
  | GPU Burn（ES 3.1 和 ES 2.0） | 648–756 MHz | 528 MHz |

  - **6.5 万粒子的成绩（约 1 GB/s）是 GPU 被降频后的成绩**：负载太轻，调速器把 GPU 压到 180–396 MHz，一步要 2.4 ms；连续做 64 步时每步只要 0.5 ms。这是设备的行为，引擎没有错。我曾怀疑是"等待"里的睡眠粒度太粗，加了一段自适应的代码，实测没有效果，已经去掉。
  - **GPU Burn 的成绩是 GPU 在约 750 MHz 时的成绩**，不是芯片标称的 852 MHz，也不是这个内核频率表的上限 1008 MHz。我当时读到 GPU 49 °C 就写了"不是过热"，那是测试结束后读的；后来在测试中每 2 秒读一次，GPU 几秒内就到 63–71 °C，750 MHz 是系统的功耗 / 温度上限（见下面"强制到最高频"那条）。
  - CPU 上限 2218 MHz 是这个系统设的（芯片标称 2.2 GHz）；内核表里还有到 2499 MHz 的档，没有启用。
- **32 位的 CPU 算力成绩加了标识（用户同意，已改）。** 32 位构建下 `workload_version` 末尾多一段 `_32bit`（`ProbeCpuTopology`，`src/cpu_benchmark.cpp`，按指针宽度判断，所以 32 位 x86 也算）。保存的名字变成 `cpu_mixed_v1_strict_sched_affinity_32bit_formal_...`；各前端的图表按名字完全相同来分组，Windows 只检查 `cpu_mixed_v1_` 前缀，所以不用改前端。这台重跑了一次：全核 129.77，单核 30.57 / 30.84 / 31.05 / 31.51（`20261008-111616-592-cpu-single`、`…-616-cpu-multi`）。之前没带标识的 4 条已从电脑副本里剔除。缓存 / 内存测试没加标识：它用的是 128 位 NEON，和 64 位设备做的是同样的事。
- **用户想把 GPU 强制到最高频再跑 GPU Burn：没成，平板在第二项时掉线。** 命令都由用户执行（Magisk 的 `su`）。
  - `echo 1008000000 > …/devfreq/gk20a.0/min_freq` 不起作用：写完读回来仍是 72 MHz，这个调速器不认它。
  - `echo 0 > /sys/devices/platform/host1x/gk20a.0/enable_3d_scaling`（关掉按负载调频）读回来是 0。之后跑系列里的 GPU Burn，每 2 秒读一次时钟（`out\adb\Xiaomi-MI-PAD-20261008-111729\clocks-every-2s.txt`）：
    - ES 3.1 的 16 步跑完了：0.41 Gpix-step/s（之前 0.39）。但 GPU 仍在 708–756 MHz，没有上去；GPU 温度从 40 °C 几秒内升到 63–71 °C。**所以 GPU Burn 时的 750 MHz 不是调速器按负载估出来的，是系统另有的功耗 / 温度上限压着的**；我之前在 `DEVICE-STATE.txt` 里写的"调速器按负载定"对这一项不准确。
    - ES 3.1 的 64 步开始 2 秒后（还在预热，帧时间 199–217 ms），adb 丢了设备。最后两次读数是 924 MHz / 44 °C 和 756 MHz / 67.5 °C。之后三分多钟没有回来，是死机还是重启要看平板。
  - 结论：这台不适合强制 GPU 频率。它平时的成绩（约 750 MHz 下的 GPU Burn）就是这台设备在这个系统上能稳定跑出来的成绩。
  - **平板 11:24 重启回来，一切正常。** 调频开关恢复成默认的 1，App 和成绩文件完好。重启后在正常状态下快速检查了两项 64 步的 GPU Burn（`out\adb\Xiaomi-MI-PAD-20261008-112609\`，不算成绩）：ES 3.1 是 0.53，ES 2.0 是 4.86，和系列里一致。每 1.5 秒读一次时钟看到了原因：ES 3.1 那项开始时 GPU 到过 924 MHz，GPU 温度 2 秒内从 42 °C 升到 76 °C，随后系统把频率压下来；ES 2.0 那项是 756 MHz / 66.5 °C。所以长时间负载下稳在 750 MHz 左右是散热决定的。
- **正式成绩**（`out\adb\Xiaomi-MI-PAD-20261008-055415\` 是 29 项的日志；清理过的完整副本是 `out\adb\Xiaomi-MI-PAD-20261008-134627\results.json`，36 条，含后来为读 CPU 频率又跑的一次 CPU 测试）：

  | 测试 | 成绩 |
  |---|---|
  | OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 0.90 / 5.62 / 6.14 GB/s |
  | OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 1.07 / 6.08 / 6.92 GB/s |
  | OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.39 / 0.53 / 0.57 Gpix-step/s |
  | OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 3.50 / 4.47 / 4.97 |
  | CPU 全核（4 线程，名字带 `_32bit`） | 129.77 MWork/s（没带标识的那次 125.79，已从副本剔除） |
  | CPU 单核 | 30.57 / 30.84 / 31.05 / 31.51（四个相加 124.0） |

  - 缓存 / 内存（NEON，单线程读 / 写 / 拷贝，GB/s）：L1 15.6 / 11.9 / 23.2，L2 8.6 / 10.3 / 13.3，内存 3.4 / 10.7 / 5.9；4 线程内存 3.9 / 12.6 / 6.3。内存的写比读快三倍：A15 对整行写入不先读回，读则受它自己的预取限制；4 线程的读也只有 3.9。
  - 虚拟内存：映射新页 1.44 GB/s（2851 ns/页）；没有交换分区（属实）；存储顺序读 137、写 24 MB/s，随机 4K 读 19.1、写 7.8 MB/s。
- **跑不了的 15 项：** Vulkan 13 项（没有驱动）；OpenGL ES 3.1 的 1600 万粒子 2 项（和平板 3 一样差一个工作组）。
- **电脑副本删了 10 条：** 快速检查里用标量内核跑的那次缓存测试；没带 `_32bit` 标识的 CPU 成绩 4 条；为了读时钟而临时跑的 5 条（时长不是系列里的时长）。强制 GPU 时跑完的那条 16 步 GPU Burn 只在平板上，没有进副本。
- **脚本：** 被设备两三秒内直接拒绝的测试（没有 Vulkan 驱动的那 13 项）后面不再等 30 秒冷却，没有 Vulkan 的设备一轮能省 6–7 分钟（`scripts/adb-suite.ps1`；这台和平板 3 的正式系列是改之前跑的）。

### 2026-10-08 第十台设备：小米平板 3（MT8176，PowerVR GX6250）——第一台 PowerVR、第一台联发科；修了两处引擎问题，29 项里 14 项有成绩

- **设备：** `MI PAD 3`（序列号 `SGPZ9DC6PRHEVK95`，代号 cappu），联发科 MT8176（平台名 mt8173，2 个 A72 @ 2106 MHz + 4 个 A53 @ 1703 MHz），PowerVR Rogue GX6250，OpenGL ES 3.2 build `1.7@4167538`，Android 7.0（API 24），MIUI 10 开发版 9.8.29，4 GB 内存，没有交换分区，1536 × 2048 屏。电池从 27 °C 升到 33 °C。
  - 屏幕 60 秒无操作就休眠，锁屏没有密码。脚本等解锁时用 `input keyevent KEYCODE_WAKEUP` 和 `wm dismiss-keyguard` 点亮、划开即可。
  - `adb install` 这台五次都直接成功，没有等人点确认。
- **问题一（已修）：OpenGL ES 粒子测试的计时在 PowerVR 上不对。** 快速检查里 100 万粒子 127 GB/s、400 万粒子 640 GB/s，每一步都读成 0.3 ms，不随粒子数变。
  - 原因：这台没有时间戳扩展，走墙钟计时，靠 EGL 栅栏"等 GPU 做完"。PowerVR 的栅栏不等计算着色器：0.3 ms 就通过了，真正的活在后面别的调用里才做完（Headless 400 万粒子一帧实际 41 ms，量到 0.3 ms；窗口模式下这些时间全算进了渲染）。渲染不受影响：GPU Burn 的帧时间和量到的 GPU 时间一致。
  - 改法（`GlesBackend::CheckWaitCoversCompute` / `WaitForComputeByReadback`）：开跑前先做约 400 万粒子步的计算，按平时的办法等，再把结果读回来。如果等是真的，读回应该立刻返回；如果读回还要等很久（超过总量的一成且超过 0.5 ms），以后每一步就用"读回"来等。读回的做法是把一个粒子拷到一块专用的小缓冲里再映射读取，粒子缓冲本身不映射。日志里每次都打印一行 `[Profiling] Wait check: ...`。
  - 判据改过三次才稳：最初用"一步"来判，6.5 万粒子时等待和剩余是同一量级（0.4 ms 对 0.9 ms），正式运行里没触发，成绩 5.44 GB/s 偏高一倍；改成重复到约 400 万粒子步、取三次里中间的一次、按占总量的比例判之后，四项测试每次都触发。
  - **这个核对只在这台平板上跑过。** 它在所有走墙钟计时的设备上都会运行（高通全系、Mali-T860）。正常的驱动上读回应该几乎不花时间，不会触发；但还没有在任何一台高通或 Mali 设备上确认过。下次接上其中一台时跑一项粒子测试，看 `Wait check` 那行。
- **问题二（已修）：CPU 测试开跑时只有部分核心在线。** 联发科的热插拔调速器平时把空闲核心整个关掉（闲置时 `online` 只有 `0` 或 `0-1`），被关的核心连 cpufreq 文件都没有。快速检查里全核只用了 3 个线程（140 MWork/s），缓存测试只认出 5 个核心、大核只算了 1 个。加负载后 1 秒内 6 个核心全部回来。
  - 改法：读拓扑之前先让所有核心忙起来，等 `present` 里的核心都上线（`parked_cores::AllOnline`，`src/cpu_parked_cores.h`，在 `ProbeLinuxTopology` 里用）。等到全部上线、或 1.5 秒内再没有新核心上线、或总共 4 秒为止，所以像小米 5C 那样被系统长期关着的核心不会让它卡住。
  - 改后：6 个核心、两类（2 大 4 小）都认对了。
- **没有修的：逐核测试在这台上大半无效。** 只有一个线程在跑时，调速器只留 `cpu0` 和 `cpu4`，其余核心会在测量中途被关掉，线程被挪到别的核心上。引擎的"是否还在原核心上"检查如实把这几个标成无效：正式运行里 6 个核心只有 `cpu0`（A53，50.1）和 `cpu4`（A72，76.2）有效，另外 4 个的数是在 A72 上跑出来的（75.8–76.2）。
  - 两类核心各有一个有效值，信息是全的。6 个核心相加（4 × 50.1 + 2 × 76.2 = 353）和全核 352 吻合。
  - 要让每个核心都有效，只能在测某个核心时让其他核心也忙着，那样量的就是"别的核心在空转时的单核"，和其他设备的条件不一样。没有做，等用户决定。
  - **连带的问题：`cpu_all`（逐核 + 全核）里只要逐核无效，全核的成绩也不保存。** 所以系列里的 CPU 一项在这台上没有存下成绩。我用 `--es run cpu_multi --ei seconds 15` 单独跑了一次全核，存下了。
  - **之后用户 root 强开了全部核心，逐核 6 个全部有效。** 命令由用户执行：`su -c 'echo 0 > /proc/hps/enabled'`（关掉热插拔），再把 `cpu1`–`cpu5` 的 `online` 写成 1。开跑前读回来：`/proc/hps/enabled` 是 0，空闲时 `online` 是 `0-5`，六个核心的频率上限都是硬件上限；跑完再读仍然如此。
    - 逐核（`20261008-054155-288-cpu-single`）：A53 四个 49.86 / 51.78 / 51.77 / 51.77，A72 两个 76.26 / 76.27。全核（`…-054155-326-cpu-multi`）350.86。六个单核相加 357.7，全核是它的 98%。
    - 和没强开时有效的那两个核心（50.1、76.2）以及单独跑的全核（352.11）一致，所以强开没有改变成绩，只是让逐核能测完。
    - 平板上的这个改动到重启为止都在。
- **正式成绩**（`out\adb\Xiaomi-MI-PAD-3-20261008-050810\` 是 29 项的日志；清理过的完整副本是 `out\adb\Xiaomi-MI-PAD-3-20261008-054005\results.json`，32 条）：

  | 测试 | 成绩 |
  |---|---|
  | OpenGL ES 3.1 粒子，窗口，65K / 1M / 4M | 2.10 / 3.45 / 3.92 GB/s |
  | OpenGL ES 3.1 粒子，Headless，65K / 1M / 4M | 2.49 / 3.82 / 4.00 GB/s |
  | OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 0.054 / 0.070 / 0.076 Gpix-step/s（256 步一帧 6.2 秒，只量到 3 帧） |
  | OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 0.88 / 0.96 / 1.02 |
  | CPU 全核（6 线程） | 352.11 MWork/s（单独跑的那次；强开核心后那次 350.86；系列里那次是 348.22，没存） |
  | CPU 单核（强开核心后，6 个都有效） | A53 49.9–51.8，A72 76.3 |

  - 粒子每步的工作组是 256 个调用（这个驱动允许，没有走 128 的回退）。6.5 万粒子比大粒子数慢，是因为每步"读回"有约 0.4 ms 的固定开销。
  - A72 每 GHz 约 36，和 Switch 的 A57（全核 ÷ 4 折算约 33）是一个水平；负载下读到大核 2106 MHz、小核 1703 MHz，都是硬件上限。
  - 缓存 / 内存（单线程读 / 写，GB/s）：A72 的 L1 21.8 / 16.7、L2 18.0 / 16.4、内存 7.2 / 7.4；A53 的 L1 7.7 / 14.3、L2 6.6 / 13.8、内存 2.3 / 5.9。6 线程内存读 9.2、写 9.1。
  - 虚拟内存：映射新页 1.70 GB/s（2414 ns/页）；没有交换分区（属实）；存储顺序读 148、写 72 MB/s，随机 4K 读 16.1、写 19.4 MB/s。
- **跑不了的 15 项：** Vulkan 13 项（没有 Vulkan 驱动）；OpenGL ES 3.1 的 1600 万粒子 2 项（要 65536 个工作组，驱动一次最多 65535 个；按之前的决定保持不支持）。
- **电脑副本删了 12 条：** 第一次快速检查的 6 条粒子成绩（计时错）、1 条存成 0 的 GPU Burn 256 步、3 核的 CPU 两条、5 核的缓存测试；以及两条 6.5 万粒子窗口的成绩（核对没触发，5.70 和 5.44）。
- **构建：** Android 和 HarmonyOS 包都用最终代码构建通过。这两处改动只在 Linux 系（Android / HarmonyOS / Linux）编译，Windows 不受影响。

### 2026-10-08 显存带宽加了数据校验（已改引擎）；MIX 4 重跑，读 32.7 GB/s

- **改了什么：** 显存带宽在计时之前先核对缓冲里的数据。写内核填完缓冲后，新的不计时内核 `shaders/membw_check.comp` 把每个元素按读内核的寻址方式读出来，和写内核该写的哈希值比；有不一致就把源缓冲和目标缓冲都换成一半大小的，重新填、重新核对，直到通过（`Runner::FillChecked` / `CheckData`，`src/vulkan_membw.cpp`）。
  - 结果怎么传回 CPU：校验内核把 sink 当成整数数组用，第 0 组的 256 个线程各写一个标记（证明内核跑了），读到错值的线程在后 256 个位置写一个非零数；引擎把 sink 拷进一块 CPU 可见的小缓冲读出来。标记不齐算"没法校验"，照原样继续并打印一行，不会因此缩小缓冲。
  - 输出多一行 `[membw] Data check: ...`（和已有的 `Timing check` 一样每次都打印）。缩小过缓冲时，`Sweep:` 行带 `(ceiling lowered by the device)`，保存的配置里 `maxDatasetMiB` 和 `datasetLimitedByDevice=1` 照旧反映出来，没有新增字段。
  - 独显的"系统内存"那一趟（PCIe）用的主机缓冲也做同样的校验，减到 64 MiB 以下还不行就跳过那一趟并写明原因。
  - **没动的：** 读 / 写 / 拷贝 / L1 四个计时内核、计分方式、`membw_v1` 的名字。能通过校验的设备成绩不变。
  - 新着色器进了 `CMakeLists.txt` 和 `scripts/verify-windows-stage.ps1` 的清单；Android 和 HarmonyOS 按 `membw_*.comp` 通配，自动带上。Android 的通配加了 `CONFIGURE_DEPENDS`，以后新增着色器不用清构建目录。
- **验证：**
  - **MIX 4（正式运行，`20261008-042956-459-membw`）：** 512 MiB 没通过（256 个线程位置 256 个读到错值），256 MiB 通过。读 **32.7 GB/s**，写 34.0，拷贝 35.3（读写合计）。读曲线：1 MiB 72.1，2 MiB 49.2，4 MiB 33.9，8 MiB 到 256 MiB 28.5–32.7。L1 全部单元满载 46.0（之前的 116.6 也是读 0 读出来的）。两个时钟一致（133.3 / 134.3 ms）。
  - **RTX 5090：** 2048 MiB 的显存缓冲和 2048 MiB 的系统内存缓冲都通过；显存读 1544.6 GB/s、写 1341.7、L2 平台 6522.9，系统内存读 26.3，和改之前一致。
  - **AMD 核显：** 2048 MiB 通过，读 61.2 GB/s。
  - HarmonyOS 包（`scripts\build-ohos.ps1`）和 Windows GUI 都重新构建通过，五个 `membw_*.spv` 都在包里；HarmonyOS 没有在设备上跑。
- **电脑副本：** MIX 4 现在以 `out\adb\Xiaomi-2106118C-20261008-042942\results.json` 为准（52 条）：之前 5 条 116 GB/s 的、7 条实验的显存带宽成绩和 1 条"无交换分区"的缓存测试都已剔除，只留这次的一条。
- **黑鲨 3 Pro 重跑（Adreno 650，驱动 `512.502.0`，正式运行，`20261008-044302-872-membw`）：驱动的毛病一样，成绩几乎没变。**
  - 512 MiB 同样没通过校验（256 个线程位置 256 个读到错值），256 MiB 通过。所以这个问题不是 MIX 4 一台的，是这一代 Adreno 6xx 驱动的。
  - 读 **23.2 GB/s**（之前在坏缓冲上测的是 23.5），写 32.5，拷贝 25.1。读曲线在数据核对无误的情况下仍然是平的：1 MiB 23.5，256 MiB 23.1。也就是说这块 GPU 上"读"确实是被 GPU 自己发读请求的速度卡住的，到不了内存上限（写能到 32.5）；第五台设备那节当初的这个解释是对的，我上一节说它"站不住"说过头了。平的曲线本身不能当作"读到全是 0"的证据，要靠校验。
  - 之前两条在 512 MiB 缓冲上测的显存带宽成绩（23.619 快速检查、23.454 正式）已从三份电脑副本里剔除；现在以 `out\adb\blackshark-SHARK-MBU-A0-20261008-044243\results.json` 为准（53 条）。
- **MIX 2S 重跑（Adreno 630，驱动 `512.502.0`，正式运行，`20261008-044710-910-membw`）：没有这个毛病，原来的成绩有效。**
  - 512 MiB 的缓冲通过了校验，没有缩小。读 15.4 GB/s（之前两次 15.2、15.3），写 24.9，拷贝 19.3。读曲线 1 MiB 20.2，8 MiB 16.6，512 MiB 15.4。
  - 驱动版本号和黑鲨 3 Pro 相同，但 630 能用 512 MiB、650 不能，所以毛病跟 GPU 这一代有关，不只看驱动版本号；这也是不靠设备名单、每次都实测校验的理由。
  - 电脑副本什么都不用删，三条显存带宽成绩都有效；最新一份是 `out\adb\Xiaomi-MIX-2S-Emerald-Edition-20261008-044645\results.json`（55 条）。
- **小米平板 5 重跑（Adreno 640，驱动 `512.502.0`，正式运行，`20261008-044859-616-membw`）：也没有这个毛病，原来的成绩有效。**
  - 512 MiB 的缓冲通过了校验。读 24.6 GB/s（之前两次都是 24.5），写 26.3，拷贝 25.7。读曲线 1 MiB 27.0，8 MiB 25.0，512 MiB 24.6。
  - 电脑副本什么都不用删；最新一份是 `out\adb\Xiaomi-21051182C-20261008-044836\results.json`（55 条）。
- **四台 Adreno 6xx 的结论：** 660（MIX 4）和 650（黑鲨 3 Pro）在 512 MiB 处通不过校验，降到 256 MiB；640（平板 5）和 630（MIX 2S）512 MiB 没问题。只有 MIX 4 的成绩因此变了（116 → 32.7），黑鲨 3 Pro 是 23.5 → 23.2。
- **还没做的：**
  - 1600 万粒子的 Vulkan 粒子测试同样用 512 MiB 的缓冲，还没有类似的校验（见下一节）。

### 2026-10-08 MIX 4 显存带宽"读"116 GB/s 的原因查清了：驱动报的缓冲上限它自己用不了（引擎已在上一节改了）

- **原因：** MIX 4（Adreno 660，驱动 `512.530.0`）报 `maxStorageBufferRange = 536870912`（512 MiB）。引擎按这个上限建了 512 MiB 的源缓冲并整块绑定，而这版驱动对超过 256 MiB 的存储缓冲寻址是错的：
  - **512 MiB 的缓冲：读内核每一次加载拿到的都是 0。** 读回的结果是每个线程 128 次加载 128 次不对，第一个值、最后一个值、总和全是 0。GPU 根本没有去读内存，116 GB/s 是它"把 0 加起来"的速度，所以任何数据量都一样，也正好等于"L1、全部单元满载"的速率。
  - **写内核写到 256 MiB 以上的数据落到了低 256 MiB 里。** 元素 0 里存的是元素 16,777,216（= 256 MiB 处）该有的值，缓冲末尾是 0（没被写过）。
  - **511.5 MiB 的缓冲：** 加载拿到的是内存里真实的内容了，但写仍然在 256 MiB 处折回，所以内容还是不对。
  - **256 MiB 的缓冲：全部正确。** 写进去的和该写的一致（首、尾、跨条带、跨块都核对了），读内核 128 次加载 0 次不对。
- **MIX 4 真实的读速大约是 30 GB/s。** 用发布版的读内核、缓冲限制在 256 MiB 时的曲线：1 MiB 69.4，2 MiB 43.1，4 MiB 起 28–32 GB/s 一直到 256 MiB；写 32.6。LPDDR5 6400 的理论峰值 51.2 GB/s，这组数在能力之内，曲线也是正常的"缓存到内存"的下降。这次是带临时限制的快速检查，不算成绩。
- **怎么查出来的（三步，都是临时代码，已全部还原）：**
  1. 读内核自检：把读到的每个 `vec4` 和写内核该写的哈希值比，分三种模式轮流（不罚时 / 全对才罚时 / 有错才罚时，罚时是每线程 20000 次 `sqrt`）。MIX 4 上"全对才罚时"不慢、"有错才罚时"掉到 4–5 GB/s，说明每个线程都读到了错的数据。
  2. 对照：同一个着色器在 RTX 5090 上反过来（"全对才罚时"掉到约 650 GB/s，"有错才罚时"和不罚时一样快），证明自检逻辑本身没问题。对照用的是 `build\Release` 的副本，没动正式构建，没存成绩。
  3. 直接读回：引擎里临时加了一段，用 `vkCmdCopyBuffer` 把源缓冲和 sink 的几个元素拷到一块 CPU 可见的小缓冲里打印出来，再把缓冲上限依次改成 256 MiB、511.5 MiB 重跑。
- **其他设备：**
  - Adreno 730 / 740（黑鲨 5 Pro、MIX Fold 3、Quest 3）最大数据集只有 128 MiB（驱动报的上限更小），读曲线是正常的下降，不受影响。Switch（Tegra X1，580.5 MiB）也正常。
  - **黑鲨 3 Pro（Adreno 650）、小米平板 5（640）、MIX 2S（630）很可能是同一个问题，没有验证。** 它们的缓冲也是 512 MiB，读曲线从 1 MiB 到 512 MiB 是平的（23.5 / 24.5 / 15–20 GB/s），而且 1 MiB 时读只有写的一半（别的设备上 1 MiB 的读都不比写慢）。数字本身没超过内存能力，所以当时没被拦下；下面第五台设备那节里"和 MIX 4 不是一回事"的判断现在看站不住。三台都不在手边，电脑副本里它们的显存带宽成绩还留着，属于"存疑"。
- **1600 万粒子的 Vulkan 粒子测试用的也是 512 MiB 的缓冲（每粒子 32 B），没有核对过。** 这四台 Adreno 6xx 上它的成绩看不出异常（和 400 万粒子差不到 3%：MIX 4 20.85 对 20.22，黑鲨 3 Pro 13.46 对 13.46，平板 5 10.17 对 10.21，MIX 2S 9.07 对 9.10），但粒子测试在这些 GPU 上本来就不随数据量变，所以成绩正常不能证明数据是对的。下面 MIX 4 那节里"粒子测试不受影响"只是按数字合不合理推的，不算验证过。400 万及以下的缓冲不超过 128 MiB，不涉及。
- **当时的建议（同一天已按它改了，见上一节）：** 显存带宽在填完源缓冲之后做一次校验（读内核读到的是不是写进去的），不对就把缓冲减半再试，并在输出里说明；不靠设备名单，也不改 `membw_v1` 的计时内核。改完后 MIX 4 和上面三台各重跑一次显存带宽（每台约 1 分钟）。
- **收尾状态：** `git diff -- src shaders` 为空；重新构建后的包里读内核是 3376 字节的发布版，库里没有实验代码，已装回 MIX 4。7 次实验的文件夹（`out\adb\Xiaomi-2106118C-20261008-0404xx` 到 `-0417xx`）各放了 `EXPERIMENT.txt`，电脑副本里实验产生的 7 条显存带宽成绩已剔除。
- **一个坑：** 用 `Copy-Item` 把备份的着色器拷回来时文件时间还是旧的，Gradle 认为编译产物是新的，不会重新编译，包里仍是实验版（有一次运行就是这样作废的）。拷回之后要把文件时间改成当前时间，并核对包里 `.spv` 的大小。

### 2026-10-08 第九台设备：任天堂 Switch（Tegra X1，Android 11）——29 项全过，但内存时钟卡在 800 MHz，成绩要带着这个标注看

- **设备：** `Switch`（序列号 `494E2FBE1E390FF40B`，Nintendo Switch (2017)，v1 / Erista / T210），Switchroot 的 Android 11，4 个 A57，3.4 GB 内存，1280 × 720 屏，NVIDIA Tegra X1（nvgpu）驱动 `495.0.0.0`，Vulkan 1.1。第一台 NVIDIA GPU，也是第一台 29 项全部通过的设备：流体能跑（驱动支持可线性过滤的 `R32_SFLOAT` 三维纹理），OpenGL ES 的 1600 万粒子也能跑（驱动一次调度的工作组上限够大）。
- **这台机器的时钟怎么调**（查的 Switchroot 文档，用户要求）：
  - 开机配置在 SD 卡的 `bootloader/ini/android.ini`：`oc=`（CPU 超频档，v1 默认 0 是 1785 MHz，最高 3 是 2091 MHz）、`ram_oc=`（内存，kHz，v1 可到 2131200）；GPU 在 v1 上固定是底座 921 MHz、掌机 768 MHz。改了要重启。
  - 系统"设置"里的 Switch Configuration 有 Perf Mode 开关，打开才把超频档用上（或者插底座）。
  - 这台实际是 `ro.boot.oc=3`，`pll_mb` 在 2131.2 MHz（对应 `ram_oc=2131200`），都已经是 v1 的最高档。
  - 时钟都能用普通权限读：`/sys/kernel/debug/clk/emc/clk_rate`（内存）、`/sys/class/devfreq/57000000.gpu/cur_freq`（GPU）、`cpufreq/policy0/scaling_cur_freq`（CPU）。
- **内存时钟不跟负载走，停在开机时的那一档。**
  - 第一次开机它一直是 400 MHz（开关 Perf Mode 都一样），粒子测试只有 1 GB/s 左右；用户重启后变成 800 MHz，同样的测试是 5 GB/s 左右（400 万粒子无画面 1.02 → 4.93）。
  - 正式运行期间每 5 秒读一次，322 次全是 800 MHz。原厂是 1600 MHz，这台配的是 2131 MHz。
  - Switch 安装指南里写着 Android 11 在 v1 机型上有时钟方面的 bug，会降低性能，官方建议换到 Android 15；文档没有说是哪个时钟，现象和这里读到的吻合。
  - **所以下面凡是吃内存的数字（粒子、显存带宽、CPU 内存）都是"内存 800 MHz 的 Switch"的数字，不是这颗芯片的能力。** 用户决定就这样测，成绩带标注。
- **GPU 没到最高档：** 正式运行期间最高 768 MHz，从未到 921.6 MHz（按文档那是底座模式才有的）。
- **CPU：** 正式运行时用户已关掉 Perf Mode，但重启之后 CPU 仍跑到了 2091 MHz（322 次读数里 105 次）。重启后 `scaling_max_freq` 普通权限读不了，上限是多少没有读到。
- **正式成绩**（15 秒；`out\adb\NINTENDO-Switch-20261008-033220\`，里面有 `DEVICE-STATE.txt` 写着上面的状态、`clocks-every-5s.txt` 是时钟记录、`results.json` 是清理过的副本，电池 36–38 °C）：

  | 测试 | Vulkan | OpenGL ES 3.1 |
  |---|---|---|
  | 粒子 65,536，有画面 / 无画面 | 4.74 / 5.12 GB/s | 5.25 / 5.27 |
  | 粒子 1,048,576，有画面 / 无画面 | 4.64 / 4.82 | 4.77 / 5.16 |
  | 粒子 4,194,304，有画面 / 无画面 | 4.83 / 4.93 | 4.96 / 5.19 |
  | 粒子 16,777,216，有画面 / 无画面 | 4.85 / 5.13 | 5.17 / 5.29 |
  | GPU Burn 16 / 64 / 256 步 | 0.54 / 0.69 / 0.74 Gpix-step/s | 0.53 / 0.69 / 0.74 |
  | 流体 | 2.65 MParticle-step/s（每帧计算 683 ms + 渲染 527 ms） | — |
  | 显存带宽 读 / 写 / 拷贝 | 11.2 / 10.6 / 10.0 GB/s | — |

  - OpenGL ES 2.0 的 GPU Burn：7.92 / 9.05 / 9.45 Gpix-step/s。
  - 800 MHz 的 LPDDR4 理论峰值是 12.8 GB/s，显存带宽读到 11.2（88%），粒子各档都在 5 GB/s 左右：全部卡在内存上，和时钟读数对得上。
  - GPU Burn 不吃内存，可以和别的设备比：256 步 0.74，介于 Adreno 640（0.66）和 650（0.81）之间，是在 GPU 只有 768 MHz 的情况下。
  - CPU 算力：全核（4 线程）273.97；四个核心 67.8–69.7。缓存 / 内存：L1 读 21.2、L2 14.5、内存读 3.4 / 写 2.6 GB/s；4 线程内存读 5.1、写 3.5。
  - 虚拟内存：映射新页 1.29 GB/s（3179 ns/页）；换页目标 zram；存储（SD 卡）顺序读 83、写 68 MB/s，随机 4K 读 8.0、写 5.0 MB/s。
- **副本里删了 30 条：** 重启之前、内存在 400 MHz 时跑的两轮快速检查。
- 这台没有做正式运行前的快速检查（用户要求省时间），重启后只跑过一项 3 秒的。

### 2026-10-08 黑鲨 5 Pro：补了缓存测试（"换页目标"字段）

- 只跑了这一项（`out\adb\blackshark-SHARK-KTUS-A0-20261008-030226\`，这台设备最新的清理过的完整副本，56 条）。换页目标是 6143 MiB 交换空间。
- 数字和 10-07 那次 8 核的一致：X2 的内存读 32.9 / 写 45.8 GB/s（上次 33.2 / 46.5），8 线程读 34.4 / 写 40.3（上次 34.5 / 38.9）；A710 内存读 23.8，A510 读 9.8；映射新页 3.30 GB/s（1240 ns/页）。
- 存储又是另一个数：顺序读 1478、写 876 MB/s，随机 4K 读 39.3、写 38.7 MB/s。这台手机四次的顺序读是 1358 / 911 / 1067 / 1478，写是 796 / 811 / 553 / 876，波动一直很大，原因没有查。
- **待机时大核有频率上限，App 到前台就解除：** 接上时 `cpu7` 是 2170 / 2995 MHz、`cpu4` 是 1997 / 2496 MHz；打开 App 后都回到硬件上限，跑完又被压回去。所以这台手机要在 App 到前台之后再看上限。
- 副本里用新的缓存测试换掉了两条写着"no swap"的；连同之前的 6 条，一共删了 8 条。
- 至此八台设备存下的缓存测试里，"换页目标"都是对的。

### 2026-10-08 Quest 3：OpenGL ES 用现在的计时方法重跑，补了缓存测试

- **为什么重跑：** 它 10-07 的 OpenGL ES 成绩是用 `glFinish` 加每帧交换测的，之后计时改成了 EGL 栅栏加最多 33 ms 交换一次，其他设备都是新方法。用户对重跑 14 项花的时间有意见（"没测试完的测试了不就行了"）：真正没测过的只有缓存测试；以后要重测已有成绩，先说明哪些、为什么、多久，等用户同意。
- **结果**（`out\adb\Oculus-Quest-3-20261008-024339\`，这台设备最新的清理过的完整副本，48 条；头显电池从 30 °C 升到 44 °C，引擎没有报降频）：

  | 测试 | 新方法 | 10-07 旧方法 |
  |---|---|---|
  | 粒子 65,536，有画面 / 无画面 | 8.07 / 9.18 GB/s | 5.33 / 6.43 |
  | 粒子 1,048,576，有画面 / 无画面 | 12.78 / 12.63 | 12.23 / 12.70 |
  | 粒子 4,194,304，有画面 / 无画面 | 13.41 / 13.08 | 13.37 / 13.22 |
  | 粒子 16,777,216 | 不支持 | 不支持 |
  | OpenGL ES 3.1 GPU Burn 16 / 64 / 256 步 | 1.66 / 2.37 / 2.64 Gpix-step/s | 1.64 / 2.35 / 2.63 |
  | OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 24.42 / 31.09 / 35.59 | 21.46 / 30.61 / 35.28 |

  - 变的只有每帧很短的那几项（65,536 粒子高了五成，ES 2.0 的 16 步高了 14%）：旧方法每帧多出的 `glFinish` 那 0.4 ms 在短帧里占比大。每帧几毫秒以上的基本不变，说明这台设备上旧成绩的大数据量部分本来就是对的。
  - 缓存测试：换页目标是 4607 MiB 交换空间；大核 L1 读 71.6、L2 47.2、L3 23.5、内存读 16.1 / 写 21.8 GB/s；6 线程内存读 21.3、写 23.4；映射新页 2.32 GB/s（1765 ns/页）；存储顺序读 1600、写 1506 MB/s，随机 4K 读 34.3、写 106.1 MB/s。和 10-07 的一致（6 线程读 21.3 对 21.3）。
  - 副本里删了 24 条：10-07 的 OpenGL ES 成绩 22 条（含时间戳坏掉的 3 条）和写着"no swap"的缓存测试 2 条。
- **Vulkan 探测的修正在有 Vulkan 的设备上确认了：** Quest 3 装上新包后日志是 `vulkan probe: a device is present`。
- 测试时 CPU 的频率上限是小核 1920 / 2054 MHz、大核 2208 / 2362 MHz，这是头显平时的状态，10-07 也是在这个状态下测的（没有单独验证过）。

### 2026-10-08 粒子测试的工作组从 256 改成 128 会差多少：在 MIX Fold 3 上做了对照（方案 B 的验证）

- **背景：** 粒子测试（`stream_v1`）的计算着色器每组 256 个调用，OpenGL ES 3.1 只保证 128，小米 5C 的 Mali-T860 因此跑不了。用户问改成 128 之后还能不能横向比，定了先验证：只在驱动不支持 256 的设备上用 128，成绩单独成组（方案 B）。
- **做了一个开关，默认不变：** 请求里加 `workgroup=128`（只对 `api=gles` 的粒子测试有效，别的测试或别的 API 带它会被拒绝）。OpenGL ES 后端把着色器里的 `local_size_x = 256` 换成 128，组数翻倍，日志里说明；成绩的组名多一段 `_wg128`（如 `stream_v1_wg128_android_preview_sync_timing`），`workloadConfig` 里记 `computeWorkGroupSize=128`。用户看过对照结果后定了：加上自动切换，但必须有标识和提示（见本节末尾）。
- **对照**（MIX Fold 3，OpenGL ES 3.1，每次 15 秒，256 → 128 → 256 → 128 各两遍，间隔 20 秒，电池 28–30 °C；记录在 `out\adb\wg128-check-Xiaomi-2308CPXD0C-20261008-015311\`，`runs.csv` 是逐次结果）：

  | GB/s | 256 分组（两次） | 128 分组（两次） | 128 相对 256 |
  |---|---|---|---|
  | 有画面 65,536 | 9.73 / 9.68 | 9.63 / 9.73 | −0.3% |
  | 有画面 1,048,576 | 16.43 / 16.60 | 16.97 / 17.00 | +2.8% |
  | 有画面 4,194,304 | 17.93 / 18.50 | 18.60 / 18.64 | +2.2% |
  | 无画面 65,536 | 9.64 /（6.46） | 9.63 / 9.58 | −0.4% |
  | 无画面 1,048,576 | 17.88 / 18.04 | 18.28 / 18.33 | +1.9% |
  | 无画面 4,194,304 | 19.10 / 19.09 | 19.27 / 19.74 | +2.1% |

  - 65,536：没有差别。括号里那一次整轮都慢（每步 0.40 ms，别的是 0.27 ms），这一档受 CPU 提交速度影响，是调度的偶然，没有算进去。
  - 100 万和 400 万：128 分组快 2–3%。同一种分组两次之间的差也有 0–3%，和这个差别同一量级；但 8 组相邻的对比里 128 分组 8 次都不低于 256 分组，所以是个小而稳定的差别，不是噪声。
  - 这只是一台设备（Adreno 740）一版驱动的结果。在 Mali 上两者差多少量不出来，因为它跑不了 256。
  - **结论：两种分组不是完全等价，大数据量上差 2–3%。** 128 分组的成绩要单独成组（现在就是这样），和 256 分组的放在一起看时要记着这 2–3%。
- **那一晚这台手机在限频，绝对数字不能用。** 对照时注意到数字比几小时前的正式成绩低：OpenGL ES 的 100 万是 16.5（正式 21.4），Vulkan 的 400 万无画面是 22.27（正式 26.40），CPU 单线程内存读 30.4（下午 35.1）。
  - 查到 X3 的频率上限是 2342 MHz（硬件 3360 MHz），中核是 2592 / 2803 MHz；手机当时是折叠状态，用的是外屏（`wm size` 是 1080 × 2520，下午正式测试时是内屏 1916 × 2160）。是不是折叠造成的没有验证。
  - 对照是交替着跑的，两种分组在同样的状态下，所以上面的百分比有效。
  - 这一晚的 26 条（对照 24 条、Vulkan 参照 1 条、缓存测试 1 条）已从电脑副本里删掉（`out\adb\Xiaomi-2308CPXD0C-20261008-020913\REMOVED-ENTRIES.txt`）。
  - **限频就是折叠造成的：** 用户把手机展开后，`cpu7` 的上限立刻回到 3360 MHz，中核回到 2803 MHz，`wm size` 回到 1916 × 2160；Vulkan 的 400 万无画面重跑是 26.56（正式 26.40），OpenGL ES 的 100 万有画面是 22.90（正式 21.41）。
  - **展开后补跑了缓存测试**（`out\adb\Xiaomi-2308CPXD0C-20261008-021535\`，这台手机最新的清理过的完整副本，56 条）：换页目标是 16383 MiB 交换空间；X3 的 L1 读 92.8、L2 77.7、L3 62.6、内存读 32.4 / 写 55.6 GB/s；8 线程内存读 40.9、写 55.1；映射新页 4.29 GB/s；存储顺序读 1513、写 1862 MB/s。缓存内的数字和下午一样，内存读比下午低 8%（32.4 对 35.1，40.9 对 44.2），是这台手机内存带宽逐次之间的波动。副本里用它换掉了下午那两条写着"no swap"的。
- **以后测这台折叠屏：** 展开了再测，开跑前看一眼 `cpu7` 的 `scaling_max_freq` 是不是 3360000。
- **自动切换已经做了（用户要求：一定要有标识或者提示）。**
  - 触发条件是驱动报的上限，不看 GPU 的牌子：`GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS` 和 `GL_MAX_COMPUTE_WORK_GROUP_SIZE` 不到 256 但至少 128 时，粒子测试自动用 128 分组；到 256 的设备照旧，一个字节都不变（装了新包的 MIX Fold 3 上确认过：日志里没有任何工作组的字样，组名不变）；连 128 都不到就报不支持。Mali-T860 报的是 128。更新的 Mali 报多少没有查到可靠的数字，接上设备一跑就知道。
  - 标识和提示有四处：
    1. 成绩的组名带 `_wg128`，`workloadConfig` 里有 `computeWorkGroupSize=128`，历史和图表里因此自成一组。
    2. 引擎开跑时的日志两行：驱动只允许多少，改用多少，成绩进哪个组。
    3. 引擎摘要里一行 `>> Work groups of 128 invocations, not 256: …`，写明单独成组和 2–3% 的差别。
    4. Android 和 HarmonyOS 的"最近一次运行"卡片上一段红色的中文说明（两个 App 认上面那一行摘要）。
  - **测试项目卡片上预先显示的组名也跟着变了（用户要求）。** 引擎多了一个查询 `EsParticleWorkGroupSize()`：在一块离屏的小表面上建一个 OpenGL ES 3 上下文，读和开跑时相同的两个上限，返回 256、128 或 0（跑不了）；有测试在跑时两个宿主直接返回 0。Android 经 JNI、HarmonyOS 经 NAPI（`esParticleWorkGroupSize`）在启动时问一次，选中 OpenGL ES 3.1 时粒子测试的卡片写成 `stream_v1_wg128_…`。小米 5C 上确认过卡片显示 `stream_v1_wg128_android_preview`；HarmonyOS 只编译过。
  - 卡片上的名字仍然不含 `_sync_timing`：用不用墙钟计时要到开跑前核对完时间戳才知道，所以 OpenGL ES 下实际保存的名字可能比卡片上多这一段（这一点从 OpenGL ES 3.1 接入起就是这样）。
  - 桌面版确认没有受共用代码改动的影响：Windows 引擎编译通过，RTX 5090 上显存带宽的两个时钟是 1.812 ms 对 1.878 ms，仍用 GPU 时间戳，读 1393、写 1238、拷贝 1202 GB/s。
  - Android 和 HarmonyOS 都编译通过。
  - **在小米 5C（Mali-T860，上限 128）上跑通了**（用户点了安装确认）：日志里两行说明都在，摘要里有 `>> Work groups of 128…`，成绩存成 `stream_v1_wg128_android_preview_sync_timing`，"最近一次运行"卡片上的红字说明显示正常（截图 `out\ui\mi5c-last-run.png`）。HarmonyOS 那边没有真机，只编译过。
  - 小米 5C 的粒子正式成绩（128 分组，墙钟计时，`out\adb\Xiaomi-MI-5C-20261008-022349\`，这台设备最新的清理过的完整副本）：

    | GB/s | 65,536 | 1,048,576 | 4,194,304 | 16,777,216 |
    |---|---|---|---|---|
    | 有画面 | 1.19 | 2.17 | 1.96 | 不支持 |
    | 无画面 | 1.27 | 2.47 | 2.93 | 不支持 |

    - 1600 万需要 131,072 个工作组，驱动一次调度最多 65,535 个（256 分组时是 65,536 个，同样超出）。
    - 六项里三项引擎没有判为稳定（有画面的 65,536 和 400 万、无画面的 65,536）；100 万两项和无画面 400 万是稳定的（波动 0.4–1.1%）。
    - 当时手机是 8 个核心在线、大核上限 1843 MHz（用户上一轮 root 改过，大核的上限后来又被系统改回去了）。
- **Android 的"本机支持"把没有驱动的 Vulkan 也列上了（已改；小米 5C 上验证过，有 Vulkan 的设备上还没验证）。** 改后小米 5C 上探测的结论是 `vulkan probe: no device`，页面写"本机支持：OpenGL ES 3.1、OpenGL ES 2.0"，Vulkan 按钮变灰，默认选中 OpenGL ES 3.1（截图 `out\ui\mi5c-gpu-page.png`）。 小米 5C 的页面写着"本机支持：Vulkan、OpenGL ES 3.1、OpenGL ES 2.0"，Vulkan 按钮也能选，选了就是 13 项全失败。原来的探测只看 `libvulkan.so` 里有没有 `vkGetInstanceProcAddr`；改成真的建一个实例、数一下物理设备，有设备才算支持（仍然全部走 `dlsym`）。编译通过；要验证得在小米 5C 上再装一次（要点确认），还要在一台有 Vulkan 的设备上确认没有误伤。

### 2026-10-08 第八台设备：小米 5C（澎湃 S1，Mali-T860）——第一台非高通设备，29 项里只有 8 项能跑

- **设备：** `MI 5C`（序列号 `968ceb2b`，代号 meri），澎湃 S1（平台名 abies，8 个 A53）/ Mali-T860，OpenGL ES 驱动 `v1.r18p0`，Android 7.1.2（API 25），MIUI 11，3 GB 内存，1080 × 1920 屏。全程电池 25–27 °C。
- **安装要人工确认：** MIUI 每次 `adb install` 都弹"USB 安装提示"，10 秒倒计时结束自动拒绝（`INSTALL_FAILED_USER_RESTRICTED`）。"USB 安装"开着也一样，必须有人在 10 秒内点"继续安装"。前六次都没有被点到，第七次用户点了才装上。所以这台之后没有再重装过，下面的两处引擎改动没有在它上面跑过。
- **跑不了的 21 项：**
  - Vulkan 13 项：`No Vulkan physical device found`。系统只声明 OpenGL ES 3.2，没有 Vulkan 驱动。
  - OpenGL ES 3.1 的粒子 8 项：计算着色器编译失败，`'local_size_x' qualifier value (256) exceeds the maximum supported size (128)`。粒子测试的工作组是 256 个调用，OpenGL ES 3.1 标准只保证 128（桌面 OpenGL 保证 1024，高通给 1024）。
  - 引擎现在在编译前先查 `GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS` 和 `GL_MAX_COMPUTE_WORK_GROUP_SIZE`，不够就报一句"the particle test uses compute work groups of 256 invocations; this OpenGL ES driver allows 128"。
  - **要让 Mali 这类设备能跑粒子测试，得把工作组改成 128、组数翻倍。总工作量不变，但这是改 `stream_v1` 的着色器，没有做，等用户决定。**
- **正式成绩**（能跑的 8 项，`out\adb\Xiaomi-MI-5C-20261008-011847\`；快速检查在 `…-011206\`）：

  | 测试 | 成绩 | 每帧 |
  |---|---|---|
  | OpenGL ES 3.1 GPU Burn 16 步 | 0.029 Gpix-step/s | 1013 ms（14 帧） |
  | OpenGL ES 3.1 GPU Burn 64 步 | 0.055 | 2133 ms（8 帧） |
  | OpenGL ES 3.1 GPU Burn 256 步 | 0.079 | 5985 ms（只有 3 帧） |
  | OpenGL ES 2.0 GPU Burn 16 / 64 / 256 步 | 0.54 / 0.60 / 0.49 | 55 / 198 / 959 ms |

  - 都是墙钟计时。时间戳核对在这台上三次都没有可用读数，日志里打印成"0 ms by the GPU's clock, 0 ms by wall clock"，容易误读；改成这种情况直接说"no usable reading in three attempts"。
  - 和 Adreno 630（MIX 2S）比：ES 3.1 GPU Burn 256 步是它的六分之一（0.079 对 0.48），ES 2.0 是它的三十分之一（0.49 对 15.28）。
  - ES 2.0 的 256 步比 64 步低（每步 3.75 ms 对 3.09 ms），和两台旧 Adreno 上 Vulkan 的情况一样：每帧接近 1 秒时效率下降。原因没有查。
- **CPU 只有 6 个核心在工作。** cpu6、cpu7 被系统关着（`/sys/devices/system/cpu/online` 是 `0-5`），测试的负载也没让它们上线；频率上限也被压低（大核 1843 / 2158 MHz，小核 1248 / 1404 MHz）。不是省电模式，电池也不热；是这台手机平时的状态还是别的原因，没有查。
  - 全核（6 线程）251.43 MWork/s；四个小核 36.2–37.3、两个大核 54.9–55.2。六个单核相加是 257，全核是它的 98%。
  - 缓存 / 内存（单线程读 / 写，GB/s）：大核 L1 8.9 / 17.3、L2 7.2 / 15.5、缓存之外 2.1 / 4.4 降到 1.4 / 3.2；小核 L1 5.6 / 10.7、内存 1.3 / 3.2。6 线程内存读 3.3、写 3.1。A53 没有 L3，曲线上标的"L3"只是 1–24 MiB 这一段的平台。
  - 虚拟内存：映射新页 1.10 GB/s（3739 ns/页，只有 1.1 GiB 空闲，区域缩到 146 MiB）；换页目标 zram；存储顺序读 100、写 53 MB/s，随机 4K 读 4.2、写 4.6 MB/s。
- **用户用 root 把核心全部打开后重跑了 CPU 两项**（`out\adb\Xiaomi-MI-5C-20261008-013314\`，里面的 `results.json` 是这台设备清理过的完整副本）。命令由用户执行：`echo 1 > …/cpu6/online`、`…/cpu7/online`，再把 `policy4` 和 `policy0` 的 `scaling_max_freq` 写成 2158000 和 1404000。
  - 开跑前读回来：8 个核心在线，两组上限都是硬件上限。跑完再读：8 个核心仍在线，小核上限仍是 1404 MHz，**大核上限又变回了 1843 MHz**。是谁写回去的没有查。
  - 成绩印证了这一点：

    | | 6 核、限频（平时的状态） | 8 核，root 之后 |
    |---|---|---|
    | 全核 | 251.43（6 线程） | 378.82（8 线程） |
    | 小核 × 4 | 36.2–37.3（1248 MHz） | 41.3–42.0（1404 MHz，高 12.5%，正好是频率比） |
    | 大核 | 54.9、55.2（两个） | 55.0–55.1（四个，和限频时一样） |

  - 所以 378.82 是"8 核、小核全速、大核仍在最低一档"的成绩，不是这颗芯片的上限。大核如果真到 2158 MHz，按频率比单核约 64.5，全核约 420，这是推算，没有量到。
  - 缓存 / 内存（8 核）：大核单线程内存读 1.3、写 3.2 GB/s，和之前一样；8 线程读 4.6、写 3.3。存储顺序读 85、写 60 MB/s。
  - **第三次：用户把大核的上限节点写成 2158000 后 `chmod 444` 锁住，再跑一遍**（`out\adb\Xiaomi-MI-5C-20261008-014134\`，这台设备最新的清理过的完整副本）。跑完读回来上限仍是 2158 MHz，8 个核心在线，电池 24 °C。
    - 全核（8 线程）**416.18**；小核 40.4–42.0；大核 **64.4–64.6**，正好是 55.1 × 2158 / 1843。八个单核相加是 424，全核是它的 98%。这才是这颗芯片 8 个核心全速的成绩。
    - 缓存 / 内存：大核 L1 读 9.7、写 18.3 GB/s（限频时 8.9 / 17.3）；内存读 1.4、写 3.2，和限频时一样；8 线程内存读 3.4、写 3.2。
    - 锁节点管用，说明大核的限制是有东西通过这个文件写回去的，不是内核温控直接加的。
    - 手机上的这些改动到重启为止都在：两个核心被强制在线，大核上限节点是只读。
  - 三组成绩都留在副本里：6 核限频（平时的状态，251.43）、8 核大核限频（378.82）、8 核全速（416.18）。
- **电脑副本删了 1 条：** `20261008-011536-773`，快速检查里的 ES 3.1 GPU Burn 256 步，3 秒里一帧都没量到，成绩存成了 0。
- **脚本：** Android 9 及更早没有 `thermalservice`，每项测试前都刷一屏"Can't find service"；已把这条查询的错误输出关掉。

### 2026-10-08 第七台设备：小米 MIX 2S（骁龙 845），29 项跑齐；两台旧 GPU 上 Vulkan 的 GPU Burn 256 步都比 OpenGL ES 慢

- **设备：** `MIX 2S Emerald Edition`（序列号 `9124c53f`，代号 polaris），sdm845 平台 / Adreno 630，Vulkan 驱动 `512.502.0`，Android 13，8 GB 内存，1080 × 2160 屏。包直接装上了。全程电池 28–30 °C，没有限频，没有掉线。
- **正式成绩**（`out\adb\Xiaomi-MIX-2S-Emerald-Edition-20261008-003553\`，里面的 `results.json` 没有需要删的条目）：

  | 测试 | Vulkan | OpenGL ES 3.1（墙钟计时） |
  |---|---|---|
  | 粒子 65,536，有画面 | 10.96 GB/s | 15.27 GB/s |
  | 粒子 1,048,576，有画面 | 9.20 | 8.17 |
  | 粒子 4,194,304，有画面 | 9.09 | 8.33 |
  | 粒子 16,777,216，有画面 | 9.11 | 不支持 |
  | 粒子 65,536，无画面 | 9.13 | 7.63 |
  | 粒子 1,048,576，无画面 | 9.12 | 8.04 |
  | 粒子 4,194,304，无画面 | 9.10 | 8.38 |
  | 粒子 16,777,216，无画面 | 9.07 | 不支持 |
  | GPU Burn 16 / 64 / 256 步 | 0.16 / 0.30 / **0.37（不稳）** Gpix-step/s | 0.12 / 0.28 / 0.48 |
  | 流体 | 不支持（缺可线性过滤的 `R32_SFLOAT` 三维纹理） | — |
  | 显存带宽 读 / 写 / 拷贝 | 15.3 / 25.6 / 19.2 GB/s | — |

  - OpenGL ES 2.0 的 GPU Burn：10.86 / 14.04 / 15.28 Gpix-step/s。
  - 显存带宽的读（15.3）等于这块 GPU 的"L1、全部单元满载"速率（15.2），读被 GPU 发请求的速度卡住；LPDDR4X 1866 的峰值是 29.9 GB/s。
  - CPU 算力：全核（8 线程）548.44；四个大核 105.7–106.4、四个小核 49.7–50.4。八个单核相加是 625，全核是它的 88%。
  - 缓存 / 内存（单线程读 / 写，GB/s；这台设备自己报了缓存容量）：大核 L1 39.3 / 42.8、L2 15.7 / 43.7、L3 14.8 / 32.2、内存 10.6 / 21.0；小核 L1 8.5 / 17.8、内存 4.2 / 13.3。8 线程内存读 17.8、写 22.1。
  - 虚拟内存：映射新页 3.32 GB/s（1233 ns/页）；换页目标 zram（这台设备允许应用读 `/proc/swaps`）；存储顺序读 624、写 192 MB/s，随机 4K 读 19.1、写 35.9 MB/s。
- **两台旧 GPU（Adreno 630、640）上，Vulkan 的 GPU Burn 256 步都比 OpenGL ES 慢，而且不稳；16 步反过来是 Vulkan 快。**

  | 每帧 ms | 16 步 | 64 步 | 256 步 |
  |---|---|---|---|
  | Adreno 630，Vulkan | 189 | 393 | 1263（多数帧 1195，两帧 1538） |
  | Adreno 630，OpenGL ES | 251 | 426 | 990（波动 0.11%） |
  | Adreno 640，Vulkan | 52 | 208 | 870（807–920 之间跳） |
  | Adreno 640，OpenGL ES | 55 | 206 | 715（波动 0.26%） |
  | Adreno 650，Vulkan / ES | 71 / 67 | 185 / 178 | 612 / 581 |
  | Adreno 740，Vulkan / ES | 13.6 / 13.2 | 42.9 / 41.2 | 159.7 / 153.3 |

  - 630 上按每步算：Vulkan 4.5 ms、OpenGL ES 3.1 ms；与步数无关的部分 Vulkan 118 ms、OpenGL ES 202 ms。两种 API 各有一份内容对应的着色器，走的是驱动里不同的编译路径（SPIR-V 和 GLSL ES），旧驱动编出来的效率不同，这是和数据最吻合的解释，但没有验证。
  - 所以前一条里说平板 5 的 256 步"受了干扰"说得不准：稳态本身就慢，另外才是偶尔有长帧。长帧的来源没有查。
  - 这两台的 Vulkan 256 步成绩留在副本里；跨设备比 256 步时，这两台用 OpenGL ES 的数字更稳。

### 2026-10-08 第六台设备：小米平板 5（骁龙 860），29 项跑齐；Vulkan 的 GPU Burn 256 步不稳

- **设备：** `21051182C`（序列号 `f2b08420`，代号 nabu），SM8150 平台 / Adreno 640，Vulkan 驱动 `512.502.0`，Android 15，系统报 11.2 GiB 内存，6 GiB 交换空间，1600 × 2560 屏。包直接装上了。全程电池 27–31 °C，没有限频，没有掉线。
- **正式成绩**（`out\adb\Xiaomi-21051182C-20261008-000134\`，里面的 `results.json` 没有需要删的条目）：

  | 测试 | Vulkan | OpenGL ES 3.1（墙钟计时） |
  |---|---|---|
  | 粒子 65,536，有画面 | 20.02 GB/s | 19.72 GB/s |
  | 粒子 1,048,576，有画面 | 7.70 | 9.35 |
  | 粒子 4,194,304，有画面 | 9.77 | 8.85 |
  | 粒子 16,777,216，有画面 | 10.23 | 不支持 |
  | 粒子 65,536，无画面 | 10.67 | 9.47 |
  | 粒子 1,048,576，无画面 | 10.27 | 10.01 |
  | 粒子 4,194,304，无画面 | 10.21 | 10.08 |
  | 粒子 16,777,216，无画面 | 10.17 | 不支持 |
  | GPU Burn 16 / 64 / 256 步 | 0.56 / 0.57 / **0.54（不稳）** Gpix-step/s | 0.53 / 0.57 / 0.66 |
  | 流体 | 不支持（缺可线性过滤的 `R32_SFLOAT` 三维纹理） | — |
  | 显存带宽 读 / 写 / 拷贝 | 24.5 / 26.1 / 26.2 GB/s | — |

  - OpenGL ES 2.0 的 GPU Burn：15.46 / 18.22 / 19.39 Gpix-step/s。
  - **Vulkan 的 GPU Burn 256 步：** 每帧在 807–920 ms 之间跳，引擎判为未达到稳定；OpenGL ES 同一档是 711–722 ms（波动 0.26%）。16 步和 64 步两种 API 相差不到 6%。所以这台设备上 Vulkan 三档不随步数上升（0.56 / 0.57 / 0.54），不是 GPU Burn 的规律变了，是 256 步这一档在 Vulkan 下受了干扰；干扰来自哪里没有查。快速检查里也是这样（835–891 ms）。
  - 有画面的 100 万粒子比无画面低四分之一（7.70 对 10.27），是稳定的（波动 1.06%）。
  - 显存带宽的读（24.5）等于这块 GPU 的"L1、全部单元满载"速率（24.7），读被 GPU 发请求的速度卡住；LPDDR4X 2133 的峰值是 34.1 GB/s，读写都在它以内。
  - GPU Burn 五代对比（OpenGL ES 3.1，256 步）：Adreno 640 0.66、650 0.81、660 1.09、730 1.90、740 3.08。
  - CPU 算力：全核（8 线程）607.79；A55 × 4 50.0–51.0、A76 × 3 105.7、A76 大核 129.4。八个单核相加是 649，全核是它的 94%。
  - 缓存 / 内存（单线程读 / 写，GB/s）：大核 L1 69.5 / 47.0、L2 53.9 / 44.6、L3 22.5 / 29.1、内存 16.0 / 27.4；A76 的 L1 59.6 / 38.4、L2 43.8 / 37.2、L3 26.9 / 17.8、内存 15.8 / 25.7；A55 的 L1 8.6 / 18.2、内存 6.0 / 13.7。8 线程内存读 20.7、写 25.4。
  - 虚拟内存：映射新页 3.19 GB/s（1283 ns/页）；换页目标 6143 MiB 交换空间；存储顺序读 888、写 454 MB/s，随机 4K 读 15.5、写 29.6 MB/s。

### 2026-10-07 第五台设备：黑鲨 3 Pro（骁龙 865），29 项跑齐

- **设备：** `SHARK MBU-A0`（序列号 `767b9490`，代号 mobius），kona 平台 / Adreno 650，Vulkan 驱动 `512.502.0`，OpenGL ES 驱动 `V\0502.0`（2021-04），Android 11，12 GB 内存，1440 × 3120 屏。包直接装上了。全程电池 26–27 °C，没有限频，没有掉线，每项波动都在 3% 以内。
- **正式成绩**（`out\adb\blackshark-SHARK-MBU-A0-20261007-232550\`，里面的 `results.json` 没有需要删的条目）：

  | 测试 | Vulkan | OpenGL ES 3.1 |
  |---|---|---|
  | 粒子 65,536，有画面 | 16.43 GB/s | 20.19 GB/s |
  | 粒子 1,048,576，有画面 | 12.48 | 13.29 |
  | 粒子 4,194,304，有画面 | 12.40 | 13.26 |
  | 粒子 16,777,216，有画面 | 12.68 | 不支持 |
  | 粒子 65,536，无画面 | 14.78 | 12.96 |
  | 粒子 1,048,576，无画面 | 13.82 | 13.22 |
  | 粒子 4,194,304，无画面 | 13.46 | 13.07 |
  | 粒子 16,777,216，无画面 | 13.46 | 不支持 |
  | GPU Burn 16 / 64 / 256 步 | 0.42 / 0.64 / 0.77 Gpix-step/s | 0.44 / 0.66 / 0.81 |
  | 流体 | 不支持（缺可线性过滤的 `R32_SFLOAT` 三维纹理） | — |
  | 显存带宽 读 / 写 / 拷贝 | 23.5 / 32.8 / 26.5 GB/s | — |

  - OpenGL ES 2.0 的 GPU Burn：17.02 / 20.13 / 21.11 Gpix-step/s。
  - 显存带宽的读在每个数据量下都是 23.5 GB/s，等于这块 GPU 的"L1、全部单元满载"速率（23.4）：读被 GPU 发出读请求的速度卡住，没到内存的上限，所以写（32.8）反而比读高。这和 MIX 4 的 116 GB/s 不是一回事，23.5 在内存能力之内。（2026-10-08 补充：这台的驱动确实有 MIX 4 那个毛病，512 MiB 的缓冲通不过数据校验，所以这里的 23.5 是在坏缓冲上测的，已作废；但换成 256 MiB、数据核对无误之后重跑是 23.2，曲线照样是平的，"被读请求的速度卡住"这个解释成立。见本章最上面一节。）
  - GPU Burn 四代对比（Vulkan，256 步）：Adreno 650 0.77、660 1.10、730 1.95、740 2.95。
  - **没解释的一处：** ES 2.0 的 GPU Burn 在 64 步和 256 步上，Adreno 650（20.13 / 21.11）和 660（20.36 / 21.77）几乎一样，而现行的 GPU Burn 上 660 比 650 快 43%。两台都是稳定的读数。
  - CPU 算力：全核（8 线程）773.17；A55 × 4 50.8–51.4、A77 × 3 152.6、A77 大核 179.4。八个单核相加是 842，全核是它的 92%。
  - 缓存 / 内存（单线程读 / 写，GB/s）：大核 L1 89.5 / 45.3、L2 53.9 / 41.8、L3 29.2 / 27.8、内存 22.2 / 34.7；A77 的 L1 76.2 / 38.5、L2 46.0 / 37.7、L3 27.9 / 15.1、内存 20.9 / 29.2；A55 的 L1 8.6 / 18.0、内存 4.7 / 8.6。8 线程内存读 16.6、写 17.5，低于单个大核：每个线程做同样多的工作，四个 A55 拖住了总时间。
  - 虚拟内存：映射新页 2.86 GB/s（1434 ns/页）；换页目标 4047 MiB 交换空间；存储顺序读 1198、写 638 MB/s，随机 4K 读 29.0、写 53.6 MB/s。
- **黑鲨 5 Pro 的顺序读写为什么比官方低（用户问）：** 官方说 SSD + UFS 3.1 的 RAID 0 顺序读可到 3000 MB/s，快科技用 AndroBench 测到 2951；这边三次是读 911 / 1067 / 1358、写 811 / 553 / 796 MB/s。
  - 这一项测的是"虚拟内存背后的存储"：一次一个 1 MiB 请求、单线程、不走缓存、每个方向 2 秒。RAID 0 把 1 MiB 拆给两块盘，各自拿到的请求又小又是一个接一个，到不了峰值。MIX Fold 3（UFS 4.0，标称读 4000 MB/s 级）这边也只测到 1787，比例相近。
  - 没有验证的：那台机器上阵列是否在工作（问的时候它没接着）；三次结果差这么多的原因。
  - 要一个能和 AndroBench 对比的峰值顺序读写，得另加一项（大块、多并发）；现有这项的定义不动。用户还没有决定。

### 2026-10-07 第四台设备：小米 MIX 4（骁龙 888+），29 项跑齐；显存带宽的"读"在它上面不可信；交换空间报错了（已修）

- **设备：** `2106118C`（序列号 `18b513e3`），SM8350 / Adreno 660，Vulkan 驱动 `512.530.0`，OpenGL ES 驱动 `V\0530.49`（2023-01），Android 15，12 GB 内存，120 Hz 屏。包直接装上了。整个过程电池 23–26 °C，没有限频。
- **正式成绩**（`out\adb\Xiaomi-2106118C-20261007-224556\`；清理过的完整副本是 `…-231407\results.json`）：

  | 测试 | Vulkan | OpenGL ES 3.1 |
  |---|---|---|
  | 粒子 65,536，有画面 | 22.21 GB/s | 19.79 GB/s |
  | 粒子 1,048,576，有画面 | 19.32 | 19.97 |
  | 粒子 4,194,304，有画面 | 19.96 | 20.60 |
  | 粒子 16,777,216，有画面 | 12.41（每步 54.1 ms） | 不支持 |
  | 粒子 65,536，无画面 | 22.46 | 20.83 |
  | 粒子 1,048,576，无画面 | 21.24 | 20.89 |
  | 粒子 4,194,304，无画面 | 20.22 | 20.86 |
  | 粒子 16,777,216，无画面 | 20.85（每步 32.2 ms） | 不支持 |
  | GPU Burn 16 / 64 / 256 步 | 0.68 / 0.98 / 1.10 Gpix-step/s | 0.67 / 0.97 / 1.09 |
  | 流体 | 不支持（缺可线性过滤的 `R32_SFLOAT` 三维纹理） | — |

  - OpenGL ES 2.0 的 GPU Burn：22.57 / 20.36 / 21.77 Gpix-step/s。
  - 有画面的 1600 万粒子只有无画面的六成：这台手机画 1600 万个点每帧要 200 ms，计算夹在中间变慢；数字是稳定的（波动 1.7%）。
  - GPU Burn 三代对比（Vulkan，256 步）：Adreno 660 1.10、730 1.95、740 2.95。
  - CPU 算力：全核（8 线程）758.80；A55 × 4 50.4–50.7、A78 × 3 154.8–155.0、X1 185.8。八个单核相加是 852，全核是它的 89%。
  - 缓存 / 内存（单线程读 / 写，GB/s）：X1 的 L1 80.7 / 94.6、L2 67.8 / 94.3、内存 27.9 / 39.7；A78 的 L1 56.4 / 76.4、L2 45.8 / 74.8、内存 23.6 / 34.0；A55 的 L1 8.6 / 18.5、内存 4.4 / 10.7。8 线程内存读 21.7、写 34.8。
  - 虚拟内存：映射新页 4.59 GB/s（893 ns/页）；存储顺序读 1526、写 638 MB/s，随机 4K 读 27.9、写 28.4 MB/s。
- **显存带宽的"读"是 116.4 GB/s，不可信，这一项的成绩已从电脑副本里删掉。**
  - 从 1 MiB 到 512 MiB 每个数据量都是 116 GB/s，每次耗时精确到小数点后三位都一样。LPDDR5 6400 的理论峰值是 51.2 GB/s，512 MiB 的数据不可能以两倍于峰值的速度从内存读出来。
  - 它正好等于这块 GPU 的"L1、全部单元满载"速率（116.6），也就是说读内核在任何数据量下都像在读缓存。
  - 写 32.1 GB/s 随数据量下降，正常。拷贝 63.1（读写合计）偏高：拷贝 256 MiB 用 8.2 ms，和只写 256 MiB 的 8.1 ms 一样，读的那一半没花时间。
  - 其他三台的读曲线都是正常的缓存到内存的下降：Adreno 740 从 275 降到 53，Adreno 730 从 78 降到 44。
  - **排除了两个原因：** 不是计时（新加的核对：4096 MiB 的读，GPU 时钟 41.35 ms，墙钟 41.77 ms）；不是读被编译器删掉（临时把校验和改成无条件写出，读速不变，实验用的着色器已还原）。
  - **没有查清的（2026-10-08 已查清，见本章最上面一节）：** 读为什么落不到内存上。可能是这版驱动对只读缓冲的寻址有问题（读到的地址范围比该读的小），要查得让读内核把读到的数据校验出来。粒子测试不受影响：它的读写合计约 33 GB/s，在内存能力之内。
  - 引擎现在在显存带宽开头打印一行两个时钟的读数；GPU 时钟不到墙钟一半且差 2 ms 以上时退回墙钟。这条在这台手机上没有触发（两个时钟一致）。
- **所有安卓设备的"换页目标"都报成了"无交换分区"（已修）。**
  - 应用读不了 `/proc/swaps`（Permission denied），引擎读不到就当成没有。MIX 4 实际有 8 GB（`SwapTotal: 8388604 kB`）。
  - 改成读不到时看 `/proc/meminfo` 的 `SwapTotal`（应用可读）。MIX 4 的正式成绩里现在是"swap of 8191 MiB…（Android 上通常是 zram）"。
  - **MIX Fold 3、Quest 3、黑鲨的缓存测试成绩里存的还是"no swap"**，数字本身没有问题，这一个字段是错的；下次接上时各重跑一次缓存测试（一两分钟）可以换掉。
  - 各设备的虚拟内存成绩（映射新页，单线程）：MIX Fold 3 4.63 GB/s（885 ns/页）、Quest 3 2.49（1646）、黑鲨 5 Pro 3.33（1232）、MIX 4 4.59（893）。
- **一次误报的超时：** OpenGL ES 有画面 65,536 粒子那项，测试在手机上正常跑完并保存（19.79 GB/s），但 App 结束时那行 `RUN finished` 日志没有出现，脚本等满 120 秒报了 TIMEOUT。只发生了这一次，原因没有查。
- **快速检查时手机锁屏过一次：** 两次运行之间隔了五分钟以上（自动熄屏时间），脚本等解锁等到超时。

### 2026-10-07 第三台设备：黑鲨 5 Pro（骁龙 8 Gen 1），29 项跑齐

- **设备：** `SHARK KTUS-A0`（序列号 `9893218b`），SM8450 / Adreno 730，驱动 `V\0615.0`（2022-03），Android 12，12 GB 内存，144 Hz 屏。包直接装上了，没有要求打开"USB 安装"。
- **第一次正式运行作废：这台手机一热就限频。**
  - 3 秒快速检查把电池从 32.4 °C 烤到 36.8 °C，紧接着的正式运行从 38.1 °C 起步。400 万粒子头 0.4 秒每步 9.3 ms，之后一直是 14.0 ms（11.97 GB/s）；机身凉的时候是 8.7 ms（19.3 GB/s）。100 万粒子在一次运行里从 1.9 ms 掉到 3.4 ms 又回到 2.2 ms。停下后系统报 `Thermal Status: 1`，外壳传感器 42.5 °C。
  - 那三条（`20261007-211350-698`、`20261007-211439-872`、`20261007-211529-357`）已从电脑上的副本里删掉。
- **用户随后调了手机的温控，第二次正式运行有效**（`out\adb\blackshark-SHARK-KTUS-A0-20261007-211911\`，电池从 38 °C 一路降到 31 °C，每项的波动都在 1.3% 以内，没有限频）：

  | 测试 | Vulkan | OpenGL ES 3.1（墙钟计时） |
  |---|---|---|
  | 粒子 65,536，有画面 | 37.39 GB/s（0.070 ms） | 25.39 GB/s（0.103 ms） |
  | 粒子 1,048,576，有画面 | 19.76 GB/s | 18.33 GB/s |
  | 粒子 4,194,304，有画面 | 19.32 GB/s | 18.94 GB/s |
  | 粒子 16,777,216，有画面 | 19.15 GB/s | 不支持 |
  | 粒子 65,536，无画面 | 20.65 GB/s（0.127 ms，CPU 和 GPU 各占一半） | 30.80 GB/s（0.085 ms） |
  | 粒子 1,048,576，无画面 | 19.69 GB/s | 18.44 GB/s |
  | 粒子 4,194,304，无画面 | 19.31 GB/s | 19.01 GB/s |
  | 粒子 16,777,216，无画面 | 19.24 GB/s | 不支持 |
  | GPU Burn 16 / 64 / 256 步 | 1.42 / 1.82 / 1.95 Gpix-step/s | 1.36 / 1.76 / 1.90 |
  | 流体 | 不支持（缺可线性过滤的 `R32_SFLOAT` 三维纹理） | — |
  | 显存带宽 | 44.17 GB/s | — |

  - OpenGL ES 2.0 的 GPU Burn：42.60 / 35.70 / 38.72 Gpix-step/s（每帧 0.692 / 3.305 / 12.187 ms）。
  - 100 万粒子以上四档都在 19.2–19.8 GB/s，是内存带宽的平台。LPDDR5 6400 的峰值是 51.2 GB/s，GPU 一侧的显存带宽测试读到 44.2（86%）。
  - 65,536 粒子的数据只有 2.6 MB，落在缓存里，所以能高过内存带宽；这一档每步不到 0.13 ms，受 CPU 提交速度影响大，四个数字之间差一倍，不适合拿来比 GPU。
  - ES 3.1 的 GPU Burn：时间戳通过了开跑前的核对，但运行中出现了不单调的读数，引擎中途退回墙钟计时。比 Vulkan 低 3–4%。
  - 对 MIX Fold 3（骁龙 8 Gen 2）：GPU Burn 是它的 66%，大数据量粒子是它的 75%，方向和两代芯片的差距一致。
- **CPU 两项：第一次只测到 4 个核心，用户把设置改回去后补跑了 8 核的**（`out\adb\blackshark-SHARK-KTUS-A0-20261007-215220\` 和 `…-215730\`，电池 28–29 °C；最后这个文件夹里的 `results.json` 是清理过的完整副本）。
  - 调过温控的那个状态下，系统把前台应用限定在 cpu4–7（`/dev/cpuset/top-app/cpus` 是 `4-7`），四个 A510 不给用，它们的最高频率也被压到 1075 MHz（原本 1786 MHz）。GPU 那 27 项就是在这个状态下跑的，不受影响。
  - 改回去之后 `top-app` 是 `0-7`。CPU 算力跑了两遍：

    | | 第一遍 | 第二遍 | 只有大核的那个状态 |
    |---|---|---|---|
    | 全核 | 835.74（8 线程） | 844.99（8 线程） | 648.67（4 线程） |
    | A510 × 4 | 50.6–52.4 | 51.4–52.2 | 没测到 |
    | A710（cpu4、cpu5） | 162.8、162.4 | 162.9、158.6 | 163.1、163.2 |
    | A710（cpu6） | **137.1** | **134.6** | 163.3 |
    | X2（cpu7） | 185.9 | 184.1 | 192.0 |

  - **cpu6 两遍都比另外两个 A710 低 16–17%**，在只有大核的那个状态下却和它们一样。三个 A710 共用一个频率，所以不是频率的差别，是普通状态下有别的东西固定占着 cpu6；是什么没有查。
  - 缓存 / 内存（8 核，三类核心，单线程读 / 写，GB/s）：

    | | 第一级 | 第二级 | 第三级 | 内存 |
    |---|---|---|---|---|
    | X2 | 89.0 / 90.3（到 512 KiB） | 59.5 / 75.8（到 4 MiB） | — | 33.2 / 46.5 |
    | A710 | 61.4 / 74.3（到 32 KiB） | 47.7 / 73.9（到 512 KiB） | 33.8 / 35.1（到 6 MiB） | 24.5 / 41.1 |
    | A510 | 17.7 / 19.5（到 128 KiB） | 13.0 / 14.3（到 6 MiB） | — | 10.2 / 13.4 |

    8 线程内存读 34.5、写 38.9 GB/s（只有大核的那个状态下 4 线程是 38.3 / 45.1，四个小核拖了后腿）。存储顺序读 1067、写 553 MB/s，随机读 34.1、写 40.4 MB/s。
  - X2 的 L1（64 KiB）和 L2（1 MiB）读速只差一成（89.5 对 80.6 GB/s），曲线把它们并成了一级，所以 X2 的"第一级"其实是 L1 + L2，"第二级"其实是 L3（6 MiB）。A510 的第一级到 128 KiB，是不是同样并了两级没有核对。
  - 那三条 4 核的 CPU 记录（`20261007-214221-272-cpu-single`、`…-281-cpu-multi`、`20261007-214342-173-cpu-membw`）已从电脑上的副本里删掉，手机上还在。
- **USB 断过两次，换线后好了。** 21:22:55 手机从 USB 上整个消失约半分钟，21:50 又连不上；两次 Windows 的设备日志都记到"设备描述符读取失败"的未知设备（`VID_0000&PID_0002`）。第一次它自己连回来，脚本接着跑，没有丢测试；第二次用户换了一条数据线，之后的三次运行没有再断。MIX Fold 3 当晚的 5 次 adb 掉线现象不同（设备还在，只是 adb 变成 offline），用的是不是同一条线没有确认。
- **脚本加了 `-StartBelowC <温度>`：** 每项开始前等到电池不高于这个温度、而且系统不报热限频（`dumpsys thermalservice` 的 `Thermal Status` 为 0），最多等十分钟；开始时仍在限频的会在备注里写明。这次没有用上（用户改了温控），**还没有在真机上跑过**。

### 2026-10-07 OpenGL ES 墙钟计时在手机上的两处错误（已修）；MIX Fold 3 补跑

在 MIX Fold 3（骁龙 8 Gen 2，120 Hz 屏，驱动 `V@0676.76.1`）上第一次跑 OpenGL ES 时，墙钟计时的成绩都偏低，原因有两个，互不相干。Quest 3 上两个都没有出现，所以之前没发现。

- **`glFinish` 按系统时钟节拍睡眠。**
  - 现象：每次 `glFinish` 的耗时都是 4 ms 的整数倍（内核 `CONFIG_HZ=250`）。无画面 100 万粒子一步读成 4.36 ms（9.61 GB/s），实际是 1.8 ms。Quest 3 上同一个调用只花 0.4 ms。
  - 修法（`GlesBackend::WaitForGpu`）：用 `EGL_KHR_fence_sync` 建一个栅栏，先不睡眠地问 1 ms，之后每 50 µs 问一次，从不在驱动里睡。没有这个扩展的设备才退回 `glFinish`，并在日志里说明。所有结束墙钟计时的等待都换成它。
- **交换缓冲之后，第一批真正的 GPU 命令要等屏幕刷新。**
  - 现象：有画面的测试无论工作量多少，每帧总耗时都是 8.1 ms，正好 120 FPS，尽管交换间隔是 0。分段计时（临时加的，已删）：65,536 粒子的计算等了 5.4 ms、渲染 2.1 ms；100 万粒子是 2.8 + 4.7 ms。渲染时间是真的，多出来的等待全落在交换后的第一步（计算）上。65,536 粒子因此读成 0.45 GB/s。
  - 交换后立刻再等一次没有用：这时队列里没有命令，等待马上返回。交换后先清一次屏再等，等待才被接住。
  - 但只接住等待还不够：GPU 每 8.3 ms 里大部分时间空着，系统就把它降频。OpenGL ES 2.0 的 GPU Burn 16 步，预热头 0.3 秒每帧 0.8 ms，稳定后变成 3.2 ms。
  - 修法（`GlesBackend::Present`）：**每帧照常完整绘制并计时，但最多每 33 ms 交换一次**，所以屏幕上的画面一秒更新约 30 次。这样总有一块屏幕早已放开的缓冲，没有命令被压住，GPU 一直满负荷。交换之后的"清屏再等"保留，接住万一还有的等待。
  - 修完后有画面的 65,536 粒子约 1,300 FPS，每步 0.147 ms；ES 2.0 GPU Burn 16 步头一秒是 0.60 ms。
- **这个修法第一版自己带了一个错（当晚发现并改掉）：** 不交换的帧在 GPU 时间戳计时下只补了 `glFlush`，没有等 GPU。交换原本也起着"别跑到 GPU 前面去"的作用，少了它，程序一口气往队列里塞帧。
  - 后果：ES 3.1 GPU Burn 64 步的 13 秒只量到 6.3 秒（153 帧）；256 步（每帧 153 ms）两次一帧都没量到，成绩存成 0。
  - 改成不交换的帧用 `WaitForGpu` 等完再发下一帧。之后 16 / 64 / 256 步都是 13.0 秒整，948 / 314 / 85 帧。
  - 墙钟计时的测试不受影响，它们每帧本来就等 GPU。
- **GPU 的时钟频率读不到：** `/sys/class/kgsl/kgsl-3d0/` 下的文件对 adb 都是 Permission denied，"降频"是从每帧耗时的变化推出来的，没有直接看到。
- **MIX Fold 3 正式成绩**（15 秒，电池 35–36 °C；粒子和 Vulkan 的 GPU Burn 在 `out\adb\Xiaomi-2308CPXD0C-20261007-204121\`，ES 3.1 的 GPU Burn 在 `…-205741\`，ES 2.0 在 `…-210145\`，最后这个文件夹里的 `results.json` 是清理过的完整副本）。这台手机的 29 项至此跑齐：

  | 测试 | OpenGL ES 3.1 | Vulkan |
  |---|---|---|
  | 粒子 65,536，有画面 | 12.53 GB/s（0.209 ms） | 11.09 |
  | 粒子 1,048,576，有画面 | 21.41 GB/s（1.959 ms） | 20.97 |
  | 粒子 4,194,304，有画面 | 24.16 GB/s（6.944 ms） | 25.99 |
  | 粒子 16,777,216 | 不支持（驱动一次调度最多 65,535 个工作组） | 25.60 |
  | 粒子 65,536，无画面 | 10.45 GB/s（0.251 ms） | 15.62 |
  | 粒子 1,048,576，无画面 | 23.86 GB/s（1.758 ms） | 27.19 |
  | 粒子 4,194,304，无画面 | 24.66 GB/s（6.803 ms） | 26.40 |
  | GPU Burn 16 步 | 2.24 Gpix-step/s（13.174 ms） | **2.16**（13.624 ms，本次） |
  | GPU Burn 64 步 | 2.86 Gpix-step/s（41.181 ms） | 2.75（42.9 ms，15:37 那次） |
  | GPU Burn 256 步 | 3.08 Gpix-step/s（153.290 ms） | **2.95**（159.736 ms，本次） |

  | OpenGL ES 2.0 的 GPU Burn（自己一组） | 成绩 | 每帧 | 是否稳定 |
  |---|---|---|---|
  | 16 步 | 40.17 Gpix-step/s | 0.734 ms | 稳定（CV 0.53%） |
  | 64 步 | 40.07 Gpix-step/s | 2.944 ms | **不稳定**，每帧在 2.6–3.3 ms 之间来回 |
  | 256 步 | 47.98 Gpix-step/s | 9.835 ms | 稳定（CV 0.19%） |

  - 粒子测试用墙钟计时，ES 3.1 的 GPU Burn 用 GPU 时间戳（这台手机的时间戳通过了开跑前的核对）。
  - 两种 API 的 GPU Burn 都随步数递增（Vulkan 2.16 / 2.75 / 2.95，ES 3.1 2.24 / 2.86 / 3.08），和 Quest 3（1.82 / 2.49 / 2.75）同一个规律；ES 3.1 比 Vulkan 高 4%。
  - 无画面 65,536 粒子比 Vulkan 低三分之一：一步只有 0.15 ms 左右，墙钟计时每帧多出的提交和等待占了大头，数据量越大差得越少。
  - **ES 2.0 的 16 步和 64 步被手机的 GPU 调频压低了约两成：** 16 步头 1.2 秒每帧 0.602 ms，之后跳到 0.73 ms 并停在那里；64 步头 3 秒 2.6 ms，之后来回跳。帧太轻，两帧之间 CPU 来回的那一小段让 GPU 不算满负荷，系统就降一档频率。256 步每帧近 10 ms，GPU 是满的，48.0 才是这块 GPU 的水平。频率本身读不到，这是从每帧耗时的台阶推出来的。
- **电脑上的副本清掉了 20 条**（清单在各文件夹的 `REMOVED-ENTRIES.txt`）：当晚用上面几种错误办法测出的 OpenGL ES 成绩 14 条、队列塞帧的 5 条、早上未优化包的缓存测试 1 条。留下的 3 秒快速检查（`durationSec` 是 2.3）是用最终办法测的，数字没有错，但不是正式成绩。
- **adb 当晚 5 次丢失手机，原因没有查清。** 状态先变 offline，`adb reconnect offline` 之后经过几十秒的 unauthorized 又回到 device，不需要在手机上点任何东西。
  - 其中 3 次是队列塞帧那几次。另外 2 次的测试本身完全正常（Vulkan GPU Burn 16 步、修好后的 ES 3.1 GPU Burn 256 步），都断在开跑后约 10 秒，测试在手机上照常跑完并保存。
  - 手机的 `system_server`、`surfaceflinger`、`adbd` 都没有重启过；Windows 的设备日志里没有这台手机重新枚举的记录。
  - `adb-suite.ps1` 原来只等 30 秒让它自己好，而 offline 不会自己好。改成先发 `adb reconnect offline` 再等 75 秒，测试结果随后从手机的日志缓冲里取。**这条改动还没有遇到真实的掉线来验证。**
- **Quest 3 的 OpenGL ES 成绩要重跑：** 它们是用 `glFinish` 且每帧交换时测的，和现在的方法不一致。

### 2026-10-07 缓存 / 内存成绩和公开资料的对照

- **MIX Fold 3（骁龙 8 Gen 2）：对得上。**
  - 公开规格：LPDDR5X 4200 MHz，4 × 16 位，峰值 67.2 GB/s；X3 的 L1 数据缓存 64 KiB、L2 1 MiB，全部核心共用 8 MiB 的 L3；内核表里分给 CPU 的系统缓存 5 MiB。
  - 实测全核内存读 44.2（峰值的 66%）、写 54.7（81%），GPU 一侧读 53.0（79%），都在峰值以内。
  - X3 的曲线台阶在 64–96 KiB、1–1.5 MiB、12–16 MiB，分别对应 64 KiB、1 MiB、8 + 5 MiB。
  - 旁证：论坛上 Galaxy S23 Ultra（同一颗芯片）用 A1 SD Bench 测的内存复制约 12 GB/s；这边 X3 单线程复制 42.6 是读写两个方向合计，折成被复制的数据是 21.3 GB/s，同一量级。
- **这台 PC（9800X3D，2 × 48 GB DDR5-6400）：对得上，但要对齐口径。**
  - TechPowerUp 的内存测试：9800X3D 只有一个核心小芯片，它和内存控制器之间的链路把读带宽限制在约 64 GB/s，从 DDR5-4800 到 DDR5-8000 的 AIDA64 读都是 63.3–63.9 GB/s。同架构的 7800X3D：读 63、写 86、复制 63。
  - 实测全核读 55.4，是这个上限的 87%（当时有后台）。
  - 写 26.6、复制 40.7 不能直接和 AIDA64 的 86 / 63 比：AIDA64 用流式写入；这边用普通写入，每写一行要先读回，总线上是两倍流量（写 26.6 → 53）。复制算的是读写合计，40.7 即复制了 20.4 GB/s，加上读回是 61，正好贴着上限。
  - **对不上的一项：流式写入 30.6 GB/s，AIDA64 同类是 86。** 没有查原因。
- **Quest 3：没法用公开资料验证，而且公开资料和设备自己报的不一致。**
  - 维基百科写的是 2 个 A715 + 4 个 A510、8 GB LPDDR5 4200 MT/s（68 GB/s）。设备上 `/proc/cpuinfo` 六个核心的 CPU part 都是 `0xd4b`，即 Cortex-A78C；四个 2.36 GHz 核心的算力（130–138）也远高于手机上的 A510（58）。核心型号以设备为准。
  - 内存：实测全核读 21.3、GPU 读 32.6，只有手机的一半左右，也只有维基那个 68 GB/s 的三分之一到一半。CPU 和 GPU 两边一致，所以不是测错了某一项；是头显把内存跑在较低的频率，还是维基那行是照手机芯片抄的，没有查清。
- **没有公开数据可比的：** 各级缓存的读写速度。只能用每周期的读写次数检查：X3 的 L1 是每周期 1.67 次 16 字节读、1.75 次写；A78C 是 1.86 / 1.87；A715 是 1.84 / 1.67；9800X3D 是每周期 1.78 次 64 字节读。都在这些核心每周期 2–3 次的能力以内。A510 只有 0.59 次，偏低，没有资料可对。

### 2026-10-07 更正：GPU Burn 要跑 16 / 64 / 256 三档；SoC 缓存在手机上的表现

- **GPU Burn 的档位（用户指出）：** 设备测试流程里 GPU Burn 原来只跑 64 步，这是从引擎的 `--complete-suite` 抄来的，和用户以前的成绩不符。重新数了 Windows 成绩文件和三份备份（509 条不重复的成绩，67 条 GPU Burn）：现行的 `gpu_burn_v3` 在 RTX 5090 和 Radeon 核显上，每个 API 都是 **16 步 1 次、256 步 2 次**，64 步只有核显的 Vulkan 跑过 1 次。
  - `scripts\adb-suite.ps1` 改成每个 API 都跑 16 / 64 / 256 三档（Vulkan、OpenGL ES 3.1、OpenGL ES 2.0），整套 29 项。
  - **Quest 3 已补齐（2026-10-07 20:07，`out\adb\Oculus-Quest-3-20261007-200652\`，电池 31–38 °C，六项全部热稳定）。** GPU Burn 三档（Gpix-step/s，括号里是每帧毫秒）：

    | API | 16 步 | 64 步 | 256 步 |
    |---|---|---|---|
    | Vulkan（GPU 时间戳） | 1.82（16.2） | 2.49（47.3） | 2.75（171.5） |
    | OpenGL ES 3.1（墙钟） | 1.64（17.9） | 2.35（50.2） | 2.63（179.5） |
    | OpenGL ES 2.0（墙钟，自己一组） | 21.46（1.37） | 30.61（3.85） | 35.28（13.37） |

    - 每帧时间随步数是直线：Vulkan 每步约 0.647 ms，另有约 5.8 ms 和步数无关（法线、背景、着色）。所以步数越多成绩越高，往 2.85 靠拢。Windows 上的旧成绩也是这个形状（RTX 5090 的 256 步是 16 步的 1.41 倍，核显 1.35 倍，这台 1.51 倍）。
    - OpenGL ES 3.1 比 Vulkan 低 10% / 6% / 4%，差的是墙钟计时每帧多出的那一截，步数越多占比越小。
    - **MIX Fold 3 还缺 16 项**（Vulkan 的 16 和 256 步、OpenGL ES 3.1 的 11 项、OpenGL ES 2.0 的 3 项），手机没有连着。
    - 头显的成绩文件里那三条错的还在，每次拉回电脑的副本都要再去掉一次；这次的副本已去掉。
  - 以前成绩里的参考值（Gpix-step/s）：RTX 5090 Vulkan 16 步 140.9、256 步 198.1 / 198.8；Radeon 核显 Vulkan 16 步 0.82、64 步 1.04、256 步 1.11。
- **SoC 缓存（用户问"没有 soc 缓存吗"）：** CPU 缓存测试没有一行叫"SoC 缓存"。系统不报告缓存容量时，各级是从读曲线的平台上认出来的，按顺序叫 L1、L2、L3……系统级缓存如果被认出来，会是 L3 后面多出来的一级。
  - **硬件是有的：** Quest 3 的内核里有高通的系统缓存驱动（`qcom-llcc`，设备节点 `19e00000.cache-controller`）。`/sys/devices/system/cpu/cpu3/cache/index0–3` 存在（L1 数据 / 指令、L2、L3，L3 由 0–5 号核心共享），但 `size` 是空的，所以引擎说"系统没有报告容量"。系统缓存分给 CPU 多少，手机上读不到。
  - **MIX Fold 3 的 X3：** 工具只标出三级，没有第四级。但曲线上 L3 之后确实还有一级台阶：读速度 3–6 MiB 是 54–59 GB/s，**8 和 12 MiB 是 44.8、44.6**，16 MiB 掉到 36.9，24 MiB 以上是 35（内存）。中核的曲线也是在 12–16 MiB 之间才落到内存。这和"8 MiB 的 L3 后面还有几 MiB 的系统缓存"对得上，但只凭曲线不能断定它是系统缓存而不是共享 L3 的尾巴。
  - **为什么工具没把它单列：** 6 到 16 MiB 之间只有 8 和 12 MiB 两个数据点，不够认定成一个平台，被并进了 L3 的容量范围（"6–16 MiB"）。
  - **Quest 3：** 2.05 GHz 那组核心被标出了第四级（"L4，3–6 MiB"，读 20.4 GB/s）；2.36 GHz 那组的同一段（3–6 MiB）被标成 L3。两组是同一块共享缓存，名字差一级是因为小频率那组在 768 KiB–1 MiB 多认出了一个台阶。
  - **查到的公开资料（2026-10-07，Linux 内核 `drivers/soc/qcom/llcc-qcom.c`）：** 高通把这块缓存叫系统缓存，由 LLCC（Last Level Cache Controller）管理，切成若干片分给不同用途（CPU、GPU、显示、相机、基带……），每片的容量写在内核的表里。分给 CPU 的那一片（`LLCC_CPUSS`）：SDM845 是 2816 KB，SM8450（8 Gen 1）3072 KB，**SM8550（8 Gen 2）5120 KB，开机即启用**，SM8650 和 SM8750 也是 5120 KB，X1E80100（X Elite）6144 KB。SM8550 分给 GPU 的一片是 3096 KB，不是开机即启用。主线内核里没有 SXR2230P（Quest 3）的表。
  - **和实测对得上：** 8 Gen 2 的 L3 是 8 MiB，加上 CPU 的 5 MiB 系统缓存是 13 MiB。X3 的曲线在 8 和 12 MiB 还停在中间那级台阶上（读 44.8、44.6 GB/s），16 MiB 才落到内存；中核也是 12–16 MiB 之间落下去。所以那级台阶就是系统缓存：**X3 单线程读约 44.7 GB/s，写 66.7–68.9，复制 78.0–87.8**（8 和 12 MiB 两个点）。
  - **改进（2026-10-07，用户要求）：从曲线认级别时能认出系统缓存，旧成绩不用重跑。**
    - **规则**（`src/cpu_membw.cpp`）：紧挨着内存平台之前、只有两个数据点的一段，如果两点相差不到 5%，并且比前一级慢 15% 以上、比内存快 15% 以上，就算一级（`kShortPlateauSpread`）。测试的数据点表没有改，所以新旧成绩的曲线是同一种东西。
    - **叫法：** ARM 系统上，从曲线认出来的第四级叫 `SoC`；系统报告了缓存容量、但曲线上还多出一级的，也叫 `SoC`（Apple 的 SLC 会落在这里）。x86 上仍叫 `L4`。各前端都是把级别名原样显示，不用改。
    - **旧成绩重新判读：** 每条 CPU 缓存成绩都存着完整曲线（`workloadConfig` 里的 `curve_<模式>_<读写类型>`）。新加的 `gpu_benchmark --cpu-membw-reread <results.json>` 用当前规则把文件里每条成绩的级别重读一遍并打印，不测量、不改文件。
    - **重读的结果：**
      - MIX Fold 3 的 X3（15:49 那次）：L1、L2、L3（容量 6–8 MiB，读 57.4）、**SoC（数据量 8–12 MiB，读 44.7 / 写 67.8 / 复制 82.9）**、内存（35.1）。中核和小核不变。
      - 同一台手机 14:53 那次没有认出这一级：那次的曲线在这一段不是平的。所以这一级能不能单列，取决于那一次的曲线形状。
      - Quest 3：2.36 GHz 核心重读后是 L1、L2、L3（768 KiB–1.5 MiB，读 30.6）、SoC（2–4 MiB，读 25.4），和当时运行中判出的三级不一样；2.05 GHz 核心重读后是三级。原因是保存的曲线只留一位小数，这台头显的曲线又是缓慢下降，刚好卡在 15% 的门槛上，级别怎么分会翻。
    - **所以要这样看：** L3 和 SoC 缓存都是所有核心共用的最后几级缓存，曲线上能可靠读出的是"某个数据量下有多快"，哪一段叫 L3、哪一段叫 SoC 只是近似。用户也是这个看法（"主要是叫法不同"）。
    - Windows 上用系统报告的容量时结果不变（L1 / L2 / L3 / 内存）；Android 和 HarmonyOS 的包编译通过，没有在设备上重新跑。
  - **GPU 也用得到这块缓存（用户指出）：** 显存带宽测试的原始数据点能看出来，只是引擎没有把它报成一级。
    - MIX Fold 3：读 1 MiB 时 275.5 GB/s，2 MiB 129.8，4 MiB 87.0，8 MiB 68.2，16 MiB 60.2，24 MiB 以上约 51（内存）。
    - Quest 3：读 1 MiB 时 243.9 GB/s，2 MiB 112.0，4 MiB 73.6，8 MiB 51.5，16 MiB 36.7，24 MiB 以上约 33。
    - 引擎报"没有缓存级别"（`cache_gbps=0`）的原因：这项扫描最小的数据量就是 1 MiB，曲线从第一个点起就在下降，没有平台可认。内核表里 8 Gen 2 分给 GPU 的系统缓存是 3096 KB，和"几 MiB 以内明显更快"对得上，但曲线给不出确切容量。
    - **没有做：** 没有把扫描的起点降到 1 MiB 以下（那会改这项测试的数据点表）。

### 2026-10-07 OpenGL ES 的第一次真机运行（Quest 3）：Adreno 的时间戳不能用

- **发现：** Adreno 740 的 OpenGL ES 驱动（`V\0837.0.9`）接受 `GL_EXT_disjoint_timer_query` 的时间戳查询，位数 53，但时间戳是在指令交给驱动时打的，不是在它之前的工作做完时打的。
  - 一次计算调度前后的两个时间戳只差 0.001 ms，真正的工作出现在这一帧后面的位置。有画面的 65,536 粒子因此报出 **2501 GB/s**，GPU Burn 报出 **10 万 Gpix-step/s**，都是错的。整帧的总时间（0.441 ms）反而和 Vulkan 的"计算 + 渲染"（0.424 ms）对得上，说明时钟本身是好的，错的是分段。
  - 原来的合理性检查（超过 10 TB/s 才算坏）只拦住了 100 万粒子以上的情况，65K 的 2.5 TB/s 漏过去了。GPU Burn 没有任何检查。
  - **模拟器看不出这个问题**：模拟器根本不提供这个扩展，一直走的是墙钟。
- **修法**（`GlesBackend::CheckTimestampsAgainstWallClock`）：开跑之前，把这项测试的工作用两种办法同时计一次时（时间戳放在正式运行时的位置，随后 `glFinish` 加 CPU 时钟），取三次里最好的。GPU 时钟的数字不到墙钟的一半，就不用时间戳，改用墙钟。
  - 粒子测试的校验把调度重复若干次（凑够约 400 万粒子步），免得等待 GPU 的开销盖过工作本身；步长为零，粒子不动。
  - GPU Burn 的校验要先清屏再打第一个时间戳，和正式运行一样在同一个渲染通道里：按瓦片渲染的 GPU 把整个通道推迟执行，通道里的两个时间戳是同一个值。第一次校验没照这个顺序，没查出来。
  - Quest 3 上的校验结果：GPU 时钟 0.002 ms，墙钟 9–66 ms，全部改用墙钟。
- **正式成绩**（`scripts\adb-suite.ps1 -Serial 2G0YC5ZF7Z022H -Only "^gles"`，结果在 `out\adb\Oculus-Quest-3-20261007-171305\`，电池 40–44 °C，全部热稳定）：

  | 测试 | OpenGL ES 3.1（墙钟） | Vulkan（GPU 时间戳，同日） |
  |---|---|---|
  | 粒子 65,536，窗口 / Headless | 5.33 / 6.44 GB/s | 27.38 / 25.51 |
  | 粒子 1,048,576，窗口 / Headless | 12.23 / 12.70 GB/s | 14.32 / 14.41 |
  | 粒子 4,194,304，窗口 / Headless | 13.37 / 13.22 GB/s | 14.48 / 14.34 |
  | 粒子 16,777,216 | **跑不了** | 14.54 / 14.79 |
  | GPU Burn 64 步 | 2.35 Gpix-step/s | 2.49 |
  | ES 2.0 的 GPU Burn 64 步 | 30.61 Gpix-step/s（自己一组，每帧 3.85 ms） | — |

- **怎么读：**
  - **两列的计时方法不同，不能直接当成两个 API 的快慢。** 墙钟每量一次要等 GPU 做完，多出约 0.4–1 ms：1M 每步 3.43 ms 对 2.93 ms，4M 是 12.55 对 11.59，GPU Burn 是 50.2 对 47.3。数据量越大，这部分占比越小（4M 低 8%，GPU Burn 低 6%）。
  - **65K 的 5.33 GB/s 基本是在量等待的开销**（每步 0.49 ms，其中工作本身约 0.1 ms），不代表 GPU。
  - 引擎把墙钟计时的成绩放在带 `_sync_timing` 的分组里，不会和时间戳计时的混排。
  - ES 2.0 的 GPU Burn 每步的工作量比 `gpu_burn_v3` 轻得多（两个三角函数对十几个），30.6 和 2.35 不是同一种东西。
- **1600 万粒子跑不了：** 驱动一次调度最多 65535 个工作组（ES 3.1 的最低保证），这一档要 65536 个。引擎开跑前报错，没有保存成绩。要让它能跑得改调度方式（分两次或者二维调度），也就是改着色器，没有动。**用户的决定（2026-10-07）：保持不支持。**
- **头显的成绩文件里有三条错的成绩**，是修好之前的快速检查留下的（3 秒的预览）：`20261007-170839-586`（2501.679 GB/s）、`20261007-170859-794`（100317.787）、`20261007-171145-587`（100824.615）。它们在不带 `_sync_timing` 的分组里。**用户的决定（2026-10-07）：只要保证这台电脑上保存的成绩是对的。** 所以从 `out\adb\` 下各个文件夹的 `results.json` 副本里去掉了这三条，同时去掉了手机那边未优化包跑出的一条 CPU 缓存成绩（`20261007-054338-465-cpu-membw`，14.435 GB/s）；每个改过的文件夹里有一个 `REMOVED-ENTRIES.txt` 写明去掉了什么。设备上的文件没有动。两个摘要里带错数的快速检查文件夹（`…-170831`、`…-171108`）在 `summary.txt` 开头标了出来。
- **没有做：** `GL_TIME_ELAPSED_EXT`（另一种查询）在 Adreno 上是否可信没有试；别的厂商的驱动没有测过。

### 2026-10-07 OpenGL ES 2.0：只有 GPU Burn，独立分组 `gpu_burn_es2_legacy_v1`

- **范围（用户定）：** ES 2.0 只做 GPU Burn。粒子测试在 ES 2.0 上做不了（没有计算着色器、存储缓冲区、变换反馈），流体更不行。这是 2026-07-20 定的"ES 2.0 legacy tier"里最先做的那一项。
- **为什么是另一个着色器：** 现有 GPU Burn 用了无符号整数、位运算、uniform 块、`gl_VertexID` 和由参数决定次数的循环，GLSL ES 1.00 都没有；ES 2.0 的片元着色器也不保证高精度，Tegra 3 只有中等精度。所以它不是 `gpu_burn_v3` 的移植，而是新写的，成绩自成一组，不和其他 GPU Burn 放在一起。
- **着色器**（`shaders/gpu_burn_es2.frag`、`gpu_burn_es2.vert`）：
  - 画面是七个方向（相隔七分之一圈）行波的叠加，有万花镜式的对称。每一步加上下一个方向的波，并让这个波把平面略微弯一下，后面的步骤都依赖前面的，不能跳过。
  - 全部按 `mediump` 写：坐标在几个单位以内，三角函数的参数在几十以内；方向向量每步旋转后重新归一，不用循环计数去算（中等精度数不远）。
  - 步数由引擎在着色器前面加一行 `#define BURN_STEPS N` 编进去：GLSL ES 1.00 要求循环上限是常量，成绩合同要求每像素恰好这么多步，没有 `break`、没有 `discard`。
  - 每帧画两次，两次相加（引擎用 `ONE, ONE` 混合）。这样带隐藏面消除的 GPU 不能把第一次省掉。
  - 用 `glslangValidator` 按 GLSL ES 1.00 验过（16 / 64 / 2048 步）。
- **引擎：** 没有新增测试类型，仍是 `Workload::GpuBurnV1` 加一个开关 `BenchmarkConfig::gpuBurnEs2Legacy`，所以计分公式不变（像素 × 步数 × 2 次绘制 ÷ 渲染时间，单位 Gpix-step/s）。`GlesBackend` 多了一个 ES 2.0 模式：只用 ES 2.0 的调用（顶点缓冲、普通 uniform，没有 VAO、UBO）。
  - **计时一律用墙钟**（`glFinish` 加 CPU 时钟），新老设备同一种办法，这是当初为 legacy tier 定的。引擎因此给名字加上 `_sync_timing`。
  - 保存的名字：`gpu_burn_es2_legacy_v1_fixed_steps_<N>_<平台>_preview_sync_timing`，`graphicsApi` 是 `OpenGL ES 2.0`。
  - 向系统要的是 ES 2 的上下文，新设备可能给一个更高版本的；把测试限制在 ES 2.0 的是着色器和所用的调用。
- **请求：** `gpu_burn?steps=64&api=gles2`。`api=gles2` 配别的测试会被拒绝（`test_not_under_this_api`）。
- **界面：** Android 和 HarmonyOS 的 GPU 页都是三个选项：Vulkan / OpenGL ES 3.1 / OpenGL ES 2.0。选了某个 API 时，它没有的测试会标出需要什么（"仅 Vulkan"或"需要 Vulkan 或 OpenGL ES 3.1"），运行按钮不可用。GPU Burn 下面那行名字会跟着 API 变。
- **测试脚本：** 加了 `gles2-gpu-burn-64`，整套 23 项（之后 GPU Burn 改成每个 API 跑 16 / 64 / 256 三档，共 29 项，见上面的更正）。
- **验证：** Android 模拟器和 HarmonyOS 5.1.1 模拟器上跑通并保存，画面截图看过（第一版着色器是混沌的折叠迭代，64 步平均下来整屏一个颜色，换成了现在这版）。Windows 引擎重编，`ctest` 通过。模拟器的数字不是成绩。
- **没有做：**
  - **真机没有跑过**，包括真正只有 ES 2.0 的设备。老驱动可能把常量循环整个展开，256 步以上可能编译不过；遇到了引擎会报出编译错误。
  - **独立的 legacy APK 没有做。** 现在的 App 最低 Android 6.0、用 Compose，Tegra 3/4 那批停在 Android 4.1–5.1 的设备装不上。ES 2.0 的测试目前只是现有 App 里的一个 API 选项，能在新设备上跑出这一组的成绩；要上老设备还得做当初定的那个极简原生界面的独立包。

### 2026-10-07 OpenGL ES 3.1 后端：Android 和 HarmonyOS 的粒子测试、GPU Burn

- **范围（用户定）：** 移动端加 OpenGL ES，只做粒子测试和 GPU Burn，不做流体。显存带宽测试也仍然只有 Vulkan。iOS 没有做：iOS 的 OpenGL ES 最高 3.0，没有计算着色器，而且已被系统弃用。
- **引擎：** 新后端 `src/gles_backend.{h,cpp}`（`HAVE_GLES`，只在 Android 和 HarmonyOS 上编进去），EGL 建上下文，`GetBackendName()` 是 `OpenGL ES`。桌面的 OpenGL 4.3 后端没有动。
  - **着色器就是桌面 OpenGL 那一份**（`compute_gl.comp`、`particle_gl.*`、`fractal_gl.vert`、`gpu_burn_gl.frag`），运行时只把第一行 `#version 430` 换成 `#version 310 es` 加精度声明。所以同一项测试在 OpenGL ES 下每帧做的事和桌面 OpenGL 下相同。五个文件按 ES 3.10 用 `glslangValidator` 验过。它们以文本形式打进包里（Android 的 `assets/shaders`、HarmonyOS 的 `rawfile/shaders`）。
  - **画面** 固定 1280 × 720，和 Vulkan 一样由系统缩放；交换间隔设为 0，不跟显示器同步，驱动不给就标为受显示限制。
  - **Headless** 不需要窗口，用一个 16 × 16 的离屏表面建上下文。
  - **计时** 用 `GL_EXT_disjoint_timer_query` 的时间戳，和桌面 OpenGL 的做法相同；驱动没有这个扩展、时间戳位数为 0、结果不单调或不合理时，退回到 `glFinish` 加 CPU 时钟（`timingMode = synchronized_wall_clock`）。驱动报告"时钟被打断"的样本作废，连续 64 个作废也退回。
  - **开跑前检查两个上限：** 一次调度的工作组数（`GL_MAX_COMPUTE_WORK_GROUP_COUNT`）和单个存储块的大小（`GL_MAX_SHADER_STORAGE_BLOCK_SIZE`）。ES 3.1 只保证 65535 组和 128 MiB，1600 万粒子要 65536 组、512 MiB，驱动不够就明确报错，不改着色器去凑。
  - **出错也释放窗口：** 后端的析构函数会销毁 EGL 表面和上下文，否则一次失败之后同一个窗口再也接不上。不调用 `eglTerminate`，App 自己的界面可能在用同一个显示。
- **请求的写法**（`src/embedded_request.h`）：任何 GPU 测试后面加 `api=gles`，例如 `stream?particles=1048576&api=gles`、`gpu_burn?steps=64&api=gles`、`stream_headless?particles=1048576&api=gles`。不写就是 Vulkan。
  - Headless 在 Vulkan 下仍走命令行入口；在 OpenGL ES 下走引擎宿主（宿主负责建上下文），原生桥对 Headless 不再要求有窗口。
- **成绩：** 保存的名字不变（`stream_v1_<平台>_preview` 等），`graphicsApi` 是 `OpenGL ES`。历史页和图表页本来就按"设备 + API"分组，所以不会和 Vulkan 的混在一起。结果文件的格式没有变。
- **界面：**
  - Android 的 GPU 页原来就有"Vulkan / GL ES 3.1"两个选项，只是没接上；现在接上了，设备不到 3.1 时 OpenGL ES 选不了。
  - HarmonyOS 的 GPU 页新加了"图形 API"一栏。
  - 两边选了 OpenGL ES 时，流体和显存带宽标"仅 Vulkan"，运行按钮不可用。
- **顺带修的两处：**
  - **Android 首次构建的包里没有着色器：** 资源在原生构建之前就打包了。`app/build.gradle.kts` 让 `merge*Assets` 依赖对应的 `buildCMake*`。
  - **更新 App 后着色器不刷新：** 解包时"文件已存在就跳过"，旧版本的着色器会一直用下去。现在按安装时间戳，每次安装或更新后重新解包。
- **测试脚本：** `scripts@adb-suite.ps1` 加了 9 项 OpenGL ES 测试（名字以 `gles-` 开头：粒子四档窗口和 Headless、GPU Burn 64 步），整套从 13 项变成 22 项（之后加上 ES 2.0 的一项是 23 项）。`-Only "^gles-"` 只跑这 9 项。备注栏会写出"timed by wall clock"。
- **验证：**
  - Android 模拟器（Android 17，宿主 RTX 5090）：9 项 OpenGL ES 测试全部跑完并保存；同一轮里 Vulkan 的窗口、Headless、GPU Burn 也正常。界面截图核对过。
  - HarmonyOS 模拟器：7.0 的二合一和 **5.1.1 的手机**上，窗口粒子、GPU Burn、Headless 三种都跑通。5.1.1 的镜像没有 Vulkan 驱动，以前 GPU 测试一项都跑不了，现在这两项能跑。7.0 上 Vulkan 的窗口和 Headless 仍然正常；`cinematic_liquid?api=gles` 被拒绝（`test_needs_vulkan`）。
  - Windows 引擎重编，`ctest` 通过。
  - **模拟器里的数字都不是成绩：** 两种模拟器都不提供 GPU 时间戳扩展，走的是 CPU 时钟，而且模拟器的 `glFinish` 并不真的等。
- **没有验证：** Mali、PowerVR 等其他厂商的驱动；HarmonyOS 真机。（Quest 3 上的真机运行见上面"OpenGL ES 的第一次真机运行"那条，它查出了时间戳的问题。）

### 2026-10-07 第二台设备：Meta Quest 3（骁龙 XR2 Gen 2）

- **设备：** Oculus Quest 3（`eureka`），`Qualcomm SXR2230P`，6 个 Cortex-A78C（cpu0–1 最高 2.05 GHz，cpu2–5 最高 2.36 GHz），Adreno 740（驱动 512.837.9），8 GB 内存，Android 14，72 Hz。序列号 `2G0YC5ZF7Z022H`。App 是普通的 Android 包，在头显里以 2D 面板运行。
- **13 项全部有成绩**（`scripts\adb-suite.ps1 -Serial 2G0YC5ZF7Z022H`，结果在 `out\adb\Oculus-Quest-3-20261007-160655\`；电池 40 → 46 °C，开跑前已经被快速检查热过）：

  | 测试 | Quest 3 | MIX Fold 3（同是 Adreno 740） |
  |---|---|---|
  | 粒子 65,536，窗口 / Headless | 27.38 / 25.51 GB/s | 11.09 / 15.62（受 CPU 限制并降频） |
  | 粒子 1,048,576，窗口 / Headless | 14.32 / 14.41 GB/s | 20.97（被降频）/ 27.19 |
  | 粒子 4,194,304，窗口 / Headless | 14.48 / 14.34 GB/s | 25.99 / 26.40 |
  | 粒子 16,777,216，窗口 / Headless | 14.54 / 14.79 GB/s | 25.60 / 26.14 |
  | GPU Burn 64 步 | 2.49 Gpix-step/s | 2.75 |
  | 流体 | **5.10 MParticle-step/s**（每帧计算 432 ms + 渲染 196 ms） | 跑不了 |
  | 显存带宽 读 / 写 / 复制 | 32.6 / 18.3 / 26.4 GB/s，L1 满载 197 | 53.0 / 50.3 / 47.0，L1 235 |
  | CPU 算力，全核 | 608.5 MWork/s（6 线程） | 936.75（8 线程） |
  | CPU 算力，逐核 | cpu0–1：96.9 / 101.4；cpu2–5：105.7 / 138.0 / 130.3 / 136.2 | A510 58–60，A715 171，A710 184，X3 200 |
  | 内存读，单线程 / 全核 | 16.3 / 21.3 GB/s | 35.1 / 44.2 |
  | 存储 顺序读 / 写 | 1563 / 1513 MB/s | 1787 / 1666 |
  | 存储 随机读 / 写 | 33.1 / 97.3 MB/s | 19.6 / 81.0 |

- **怎么读这些数：**
  - **吃内存的测试是手机的一半左右，吃运算的是九成。** 1M 以上的粒子（数据集 32 MiB 起，受内存限制）是手机的 54%；CPU 内存读是 46–48%，显存带宽读是 61%。GPU Burn（不碰内存）是 91%。同一颗 GPU，差别在头显的内存慢，CPU 和 GPU 两边的测试互相印证。
  - **65K 粒子反而比手机高：** 数据集小，头显上每步 0.096 ms，没有手机那种"GPU 没吃满就自己降频"的现象。
  - **引擎没有标任何降频**，各项的 CV 都在 1.4% 以内。
  - **CPU：** A78C 在 2.36 GHz 下 130–138，折合每个工作单元约 17 个周期，和手机上的 A715（16.4）、A710（15.2）是一个量级，合理。cpu0–2 偏低（97–106），全核也只有逐核之和的 86%：头显自己的定位和合成一直在用 CPU，这些数字包含了它们。
  - **头显上的成绩是"一个 App 在头显里能拿到多少"，不是芯片的极限：** VR 合成器始终在用 GPU。1M 粒子每步平均 2.93 ms，最快 2.07 ms，就是被合成器插队的结果。
- **流体能跑：** 这台的驱动（512.837.9）给 R32F 3D 纹理做线性过滤，手机上的 512.676.1 不给。同是 Adreno 740，是驱动版本的差别。保存的名字是 `cinematic_liquid_v2_physical_scene_v8_android_preview`。
- **遇到的问题：**
  - **装好后头两次启动失败了一次，没有重现：** 快速检查的第一项 `vkQueuePresentKHR` 返回"表面丢失"（-1000000000），第二项 120 秒没有结束；从第三项起、以及之后重跑这两项，都正常。当时的系统日志已经被冲掉，原因没有查到。
  - **头显的系统日志一两分钟就被冲掉**，测试结束后再读，长测试的开头已经没了（超时的那一项连日志文件都没留下）。脚本改成每项测试开始时就让 `adb logcat` 往文件里写，结束时停掉；adb 掉线时退回到读缓冲区。
  - **必须戴着头显**（或挡住距离传感器）：摘下来它就休眠。脚本的"是否锁屏"检查看不出头显在休眠，开跑前要自己确认 `dumpsys power` 里是 `mWakefulness=Awake`。
- **没有做：** 没有在头显休眠 / 摘下的情况下试；没有试 VR 模式（App 不是 VR 应用）。

### 2026-10-07 被系统停放的核心（已修），优化后的包在 MIX Fold 3 上重跑 13 项

- **问题：** 高通的 `core_ctl` 会把没活干的核心停放（这台手机：中核 4 个里至少留 3 个，大核可以全停）。核心被停放时：
  - `sched_getaffinity()` 不把它算在内，引擎以为这台手机只有 7 个核心，全核测试少开一个线程；
  - 单独绑到它上面的 `sched_setaffinity()` 返回 `EINVAL`，逐核测试轮到它时绑不上，结果无效，整项退出码 3。
- **修法**（`src/cpu_parked_cores.h`，新文件；`src/cpu_benchmark.cpp`、`src/cpu_membw.cpp`）：
  - **数核心：** 除了 `sched_getaffinity()`，再读 `/proc/thread-self/status` 里的 `Cpus_allowed_list`（配置的掩码，内核不会按"当前醒着的"去裁剪），和 `/sys/devices/system/cpu/online` 取交集。被停放的核心照样列入，拓扑记录里 `parked=1`。
  - **绑核：** 被 `EINVAL` 拒绝时，在所有核心上起忙线程，每 2 ms 重试一次，最多等 3 秒；绑上就撤掉。这一步都在计时开始之前。旁观线程不做这件事，它是在被测线程已经开跑之后才绑核的。
  - **事后核对：** 被测线程测完再看一次自己还在不在那个核心上（`sched_getcpu()`），不在就判无效，免得把别的核心的速度记到这个核心名下。
  - **恢复掩码：** 以前用 `sched_getaffinity()` 的结果恢复，有核心被停放时会把线程永久关在那个核心之外，现在用配置的掩码。
  - 只动了 Linux 这一族（Linux、Android、HarmonyOS）的分支，Windows 和 Apple 的代码路径没有变。
- **验证：**
  - 手机上边跑边每 1.5 秒读一次 `core_ctl/global_state`：探测拓扑时 cpu5 正被停放，引擎仍然数出 8 个核心；逐核轮到 cpu5 之前它是停放的，开测时被唤醒，换成 cpu6 被停放；轮到 cpu6 时同样被唤醒。8 个核心全部有效，全核 8 线程全部绑定，退出码 0。连跑三次（一次 1 秒的快速检查，两次正式时长）都是这样。
  - Windows 引擎重编，`ctest` 通过。Android、HarmonyOS 的包都编过。另外用 zig 把这两个源文件给 aarch64 / x86-64 glibc Linux、aarch64 musl Linux、两种 macOS 各编了一遍目标文件，都通过（只编不跑）。
  - **没有验证：** HarmonyOS 和桌面 Linux（包括 Jetson）上没有运行；那里没遇到过被停放的核心，新逻辑只在 `EINVAL` 时才动。
- **优化后的包重跑 13 项**（`scripts\adb-suite.ps1 -Serial 56d15443`，结果在 `out\adb\Xiaomi-2308CPXD0C-20261007-153703\`，电池 32–35 °C）：

  | 测试 | 成绩 | 引擎的判定 / 说明 |
  |---|---|---|
  | 粒子 65,536，窗口 | 11.09 GB/s | 受 CPU 限制，GPU 空闲 62%；GPU 没吃满就自己降频（预热时每步 0.14 ms，计分时 0.236 ms）。不是 GPU 的能力 |
  | 粒子 1,048,576，窗口 | 20.97 GB/s | **中途被降频：** 前 8 秒每步 1.57 ms（约 26.7 GB/s），第 10 秒起变成 3.0 ms。三次里两次这样（26.85 / 20.62 / 20.97） |
  | 粒子 4,194,304，窗口 | 25.99 GB/s | GPU 瓶颈，稳定（CV 0.88%） |
  | 粒子 16,777,216，窗口 | 25.60 GB/s | GPU 瓶颈，热稳定 |
  | 粒子 Headless 65K / 1M / 4M / 16M | 15.62 / 27.19 / 26.40 / 26.14 GB/s | 65K 受 CPU 限制；其余稳定 |
  | GPU Burn 64 步 | 2.75 Gpix-step/s | GPU 瓶颈，CV 0.11%，和上次一样 |
  | 流体 | 跑不了 | 仍是 R32F 3D 纹理不支持线性过滤 |
  | 显存带宽 | 读 53.0 / 写 50.3 / 复制 47.0 GB/s | 上限 128 MiB，没到内存平台；L1 满载 235 GB/s |
  | CPU 算力 | 全核（8 线程）936.75 MWork/s | A510 57.9–59.7，A715 171.2 / 171.6，A710 183.9 / 183.7，X3 200.1；全部有效，退出码 0 |
  | CPU 缓存 / 内存 | 大核内存读 35.1 / 写 57.1 / 复制 42.6 GB/s | 大核 L1 读 89.8 / 写 94.2 / 复制 181.8；中核内存读 33.3；A510 内存读 13.7；全核（8 线程）内存读 44.2 / 写 54.7 / 复制 42.6 |
  | 存储 | 顺序读 1787 / 写 1666 MB/s | 随机 4 KiB 读 19.6 MB/s、写 81.0 MB/s；新页映射 4.63 GB/s |

  - **和未优化的包比：** GPU 成绩基本没变（时间戳在 GPU 上量），差别都落在"这一次有没有被降频"上。全核算力从 7 线程的 842.9 变成 8 线程的 936.75（另两次是 953.9 和 869.4，后者每步只有 1 秒）。
  - **随机读这次只有 19.6 MB/s，上次是 35 MB/s**，没有查原因。
  - 手机上的历史里还留着早先未优化时的 CPU 成绩，没有删。

### 2026-10-07 第一台安卓真机：Xiaomi MIX Fold 3（骁龙 8 Gen 2）

- **设备：** Xiaomi 2308CPXD0C，`QTI SM8550`，Adreno 740（驱动 512.676.1），Android 16，内屏 1916 × 2160。用 `scripts\adb-suite.ps1` 跑标准流程。
- **最重要的发现：移动端的包一直在用没开优化的引擎。**
  - Android 的 debug 包（adb 只能启动 debug 包）和 HarmonyOS 的包，CMake 都是 Debug 构建类型，引擎按 `-O0` 编译。
  - 怎么发现的：手机上 CPU 缓存测试"读 3.2 GB/s、写 18.6 GB/s"，读比写慢五倍，而且从 4 KiB 到 256 MiB 是一条平线，说明瓶颈在代码本身。查 `compile_commands.json`，确实没有 `-O`。
  - 旁证：同一颗 9800X3D，Windows 上单线程 175–203 MWork/s，Android / HarmonyOS 模拟器里只有 92–101。**之前记录里把模拟器上"各级缓存一样快"归因于虚拟机时钟，是错的，原因是这个。**
  - 修复：两个平台的 CMake 包装（`android/app/src/main/cpp/CMakeLists.txt`、`ohos/entry/src/main/cpp/CMakeLists.txt`）在 Debug 构建类型下追加 `-O2 -DNDEBUG`。改后确认编译参数里有 `-O2`；HAP 从 7.2 MB 变成 5.0 MB。
  - **影响范围：** 这之前在模拟器和手机上跑出的所有 CPU 成绩都无效。GPU 成绩用 GPU 时间戳，不受影响（GPU Burn 三个平台对得上就是因为这个）。手机上的成绩文件里还留着未优化时跑的两条 CPU 成绩，没有删，要用户在历史页清掉。
- **GPU 成绩（15 秒，Vulkan，显示方式是 `mailbox`，不受刷新率限制）：**

  | 测试 | 成绩 | 说明 |
  |---|---|---|
  | 粒子 65,536，窗口 | 15.23 GB/s | GPU 空闲 56%，引擎判为受限并标了降频 41.7%，这个数偏低 |
  | 粒子 1,048,576，窗口 | 26.85 GB/s | 每步 1.562 ms，212 FPS，稳定 |
  | 粒子 4,194,304，窗口 | 25.86 GB/s | 每步 6.488 ms |
  | 粒子 16,777,216，窗口 | 25.69 GB/s | 每步 26.119 ms，9 FPS |
  | 粒子 Headless 65K / 1M / 4M / 16M | 16.79 / 27.32 / 26.43 / 26.30 GB/s | |
  | GPU Burn 64 步 | 2.75 Gpix-step/s | 每帧 42.9 ms，约 22 FPS，GPU 占用 96.6%，稳定 |
  | 显存带宽 | 读 51.8 / 写 50.8 / 复制 47.5 GB/s | 9 秒；设备把扫描上限降到 128 MiB，没到内存平台；L1 满载 236 GB/s |
  | 流体 | **跑不了** | 见下 |

  - 数字自洽：1M 粒子 40 B × 1,048,576 ÷ 1.562 ms ≈ 26.85 GB/s。GPU 跑满时三档粒子都在 25.7–27.3 GB/s。
  - 和 RTX 5090 比：GPU Burn 64 步 5090 是 173 Gpix-step/s，约 63 倍。
  - **同一项重跑会差很多：** 换成优化后的包重跑 1M 粒子是 20.62 GB/s，引擎标了"降频 48.3%"；最快的一步仍是 1.52 ms，和第一次一样。也就是 GPU 的峰值速度没变，这一次中途被调频压下去了。手机上的成绩要看引擎的稳定性判定，不能只看一个数。
- **流体跑不了：** `Cinematic Liquid v2 fixed score requires linear-filterable R32_SFLOAT 3D images`。Adreno 740 的驱动不给 R32F 做线性过滤（Vulkan 里 32 位浮点格式的线性过滤是可选的）。要在这类手机上跑流体，得换纹理格式或者自己插值，那就是另一个成绩合同，没有动。
- **CPU 成绩（优化后的包）：**
  - **算力：** 3 个 A510 是 57–59，4 个中核 171–184，X3 大核 203 MWork/s；全核（7 线程）842.9。
  - **这项测试在手机上没有通过自己的合同**（退出码 3）：这次系统只给了 7 个核心，其中一个核心的线程没能固定住，引擎把它标为无效。第一次跑是 8 个核心里 1 个无效。高通的调度会把核心拿走或把线程挪开，严格按核心测量在这台手机上做不到每次都成立。脚本现在把这种情况单独标成 INVALID。（**已修**，见上一条记录：核心是被 `core_ctl` 停放了。）
  - **X3 和这台 9800X3D 的单线程一样快（203 对 175–203），查清了：数是真的，是这项测试的面太窄。** 把工作单元原样抄成一个小程序，再加两个变体，在两台机器上各跑一遍（MWork/s）：

    | | 9800X3D（MSVC） | X3（cpu7） | A715（cpu3） |
    |---|---|---|---|
    | 原样（随机分支） | 204.6 | 191.1 | 167.6 |
    | 同样的工作，分支改成可预测 | 431.1 | 340.3 | 310.7 |
    | 只留整数乘法链，没有分支 | 569.6 | 490.0 | 420.3 |

    - 第一行和引擎的成绩对得上（约 200 / 203 / 171），说明小程序量的是同一个东西。
    - 工作单元里的 `if ((a ^ b) & 1)` 取的是随机数的最低位，一半一半，预测不了。三份编译结果（MSVC x64、Clang x64、Clang ARM64）的计分循环里都是真的条件跳转。ARM64 里那段没有分支的 `csel` 循环是预热循环，结果用不上，浮点部分被编译器删掉了，不计分。
    - 9800X3D 一半以上的时间花在预测失败之后的恢复上。按最高频率（5.2 GHz、3.36 GHz）折算，每次失败 9800X3D 约 27 个周期，X3 约 15 个周期，折成时间都是 5 ns 左右。
    - 剩下的是一条串行的 64 位乘法依赖链，一次只能走一步，核心再宽也用不上：没有分支时 9800X3D 也只领先 16%。
    - **结论：** 这项测试量的是"预测失败的恢复时间 + 乘法延迟"，同一架构内比较成立（A510 / A715 / X3 的顺序是对的），跨架构不能比，也不代表日常的单核性能。两边的成绩本来就在不同的分组里。测试没有改：改了就是另一个成绩合同。
    - 手机上固定到 cpu7 要先让别的核心忙起来，否则它被 `core_ctl` 停放着，`taskset 80` 直接报 Invalid argument。
    - **PC 的后台程序不是原因（实测）：** 跑小程序时那个核心的实际频率一直是 5217 MHz；普通优先级 205.2，高优先级 205.7，前后四次都在 204.6–205.7。后台（当时占整颗 CPU 的 13–26%，含测量脚本自己）影响的是另外两个数：逐核成绩里唯一偏低的 174.1 是 1 号逻辑处理器，系统的后台活都落在它上面；全核成绩也会被压低，要干净的全核数得关掉后台重跑。X3 的 3.36 GHz 是标称最高频率，没有在运行中读到。
  - **同一天这台 PC 的 CPU 成绩**（9800X3D，存在临时目录，没有进历史）：算力每个逻辑处理器 174.1–202.3 MWork/s（平均 198.3），全核 16 线程 2527.8，合同通过。缓存测试（AVX-512）单线程 L1 读 593.6 / 写 327.9 / 复制 662.1 GB/s，L2 246.4 / 260.4 / 245.3，L3 149.1 / 146.2 / 133.2，内存 56.7 / 25.1 / 37.4，流式写 29.1；全核内存 55.4 / 26.6 / 40.7，流式写 30.6。存储顺序读 5687 MB/s、顺序写 4103 MB/s、随机读 63.5 MB/s。
  - **缓存 / 内存 / 存储（NEON，16 字节）：** X3 大核 L1 读 93.7 / 写 94.2 / 复制 181.7 GB/s，内存读 33.0 / 写 54.2 / 复制 43.5；中核内存读 27.6；A510 内存读 11.9；全核内存读 40.3 / 写 53.2 / 复制 43.8 GB/s。存储顺序读 1534 MB/s、顺序写 1638 MB/s、随机读 35 MB/s。
  - Android 不公开缓存容量，各级是从曲线上推断的（`cache_source=curve`），级别的名字只是近似。
- **流程本身遇到的问题：**
  - **小米拒绝 adb 安装**，要用户在开发者选项里打开"USB 安装"。
  - **GPU Burn 运行期间 adb 掉线了约 7 分钟**，手机没有重启、App 没有崩，测试其实跑完了。脚本当时在等一条不会返回的 adb 命令，看起来像卡死。现在每次轮询前先问 adb 服务端设备还在不在，掉线超过半分钟就标 DEVICE LOST 并停下。
  - 脚本的等待循环最初没有指定设备，手机和模拟器同时连着时空等了三分钟；现在检查了 `-Serial`。
- **还没做：** 流体在手机 GPU 上的出路。（整套 13 项已用优化后的包重跑，见上一条记录。）

### 2026-10-07 设备测试的标准流程（adb 一键跑）

- **用户的要求：** 看以前的成绩都跑了哪些测试，加上新的缓存测试，以后 adb 测设备都按这一套来。
- **以前的成绩里有什么**（`%LOCALAPPDATA%\GpuComputeBenchmark\results\results.json` 和它的三个备份，只读了副本；最大的备份 501 条，36 个设备名）：
  - **主体是粒子测试。** 测得最全的显卡（GT 730、GTX 680、MTT S80）每块 20 条：每个图形 API（Vulkan、DX12、DX11、OpenGL）各跑窗口模式的 65,536 / 1,048,576 / 4,194,304 / 16,777,216 四档，再加一条 1,048,576 的 Headless。较早测的显卡少一两档，最早的一批只有每个 API 一条 1M。
  - **开发机上的显卡**（RTX 5090、核显、WARP）另有 GPU Burn（16 / 32 / 64 / 256 步，以及更早的版本）、流体（各个开发版本，正式的 `cinematic_liquid_v2_physical_scene_v8` 有 5 条）和 CPU 算力测试。
  - 引擎自己的"完整测试"（`--complete-suite`，界面里的 Complete test）定义的是：每个 API 跑粒子四档窗口、粒子四档 Headless、GPU Burn 64 步，Vulkan / Metal 再加流体，都是 15 秒。
- **定下来的一套**（Android 只有 Vulkan 一个 API）：

  | 序号 | 测试 | 设置 |
  |---|---|---|
  | 1–4 | 粒子，窗口 | 65,536 / 1,048,576 / 4,194,304 / 16,777,216，各 15 秒 |
  | 5–8 | 粒子，Headless | 同样四档，各 15 秒 |
  | 9 | GPU Burn | 64 步，15 秒 |
  | 10 | 流体 | 15 秒 |
  | 11 | 显存 / GPU 缓存带宽（新增） | 固定的扫描 |
  | 12 | CPU 算力 | 每个核心单独跑，再全核，每步 15 秒 |
  | 13 | CPU 缓存、内存和存储（新增） | 固定的扫描 |

- **脚本：** `scripts\adb-suite.ps1`。
  - `.\scripts\adb-suite.ps1 -Install` 安装 debug 包并跑完整的一套；`-Serial` 指定设备；`-Only "burn|fluid"` 只跑名字匹配的；`-Quick` 是 3 秒 / 1 秒的快速档，只用来确认每项能启动，不算成绩。
  - 每项之间默认停 30 秒（`-CoolDownSeconds`），免得上一项的发热影响下一项；每项开始前记电池温度。
  - 手机锁着时等用户解锁（最多 10 分钟）；不改手机的任何设置。
  - 结果放在 `out\adb\<机型>-<时间>\`：`device.txt`（机型、芯片、系统）、每项一个日志、`results.json`（从手机拉回来的成绩文件）、`summary.txt`（每项一行，含引擎的"被刷新率限制"判定和显示方式）。
- **为了这一套新加的：**
  - **移动端的 Headless 粒子测试。** Android 和 HarmonyOS 的 GPU 页在粒子测试的设置里多了 Headless（和 Windows 的 Headless 对应），adb / hdc 的名字是 `stream_headless?particles=N`。它不显示画面，走引擎的命令行入口（`--backend vulkan --workload stream --particles N --headless --time S`）。iOS 没有。
  - **长时间的测试期间屏幕保持常亮。** CPU 测试和不显示画面的 GPU 测试要跑几分钟，手机中途熄屏会让 App 降速。Android 用 `KeepScreenOn`，HarmonyOS 用 `setWindowKeepScreenOn`。
  - 两个桥接层在进入命令行入口前清掉上一次留下的停止请求，否则按过"停止"之后的下一次命令行测试会立刻结束。
- **验证（Android 17 模拟器，`-Quick`）：** 粒子 1M 窗口 1587.33 GB/s、粒子 1M Headless 1674.89 GB/s、GPU Burn 64 步 172.03 Gpix-step/s、CPU 缓存测试，四项都正常结束并写进 `summary.txt`。完整的 13 项没有在模拟器上全跑一遍。
- **HarmonyOS：** Headless 和屏幕常亮只编译通过，没有运行。
- **真机（第一台）：** Xiaomi 2308CPXD0C（骁龙 8 Gen 2，`QTI SM8550`，Android 16，1916 × 2160）已经连上并授权。**安装被手机拒绝**：`INSTALL_FAILED_USER_RESTRICTED: Install canceled by user`。小米手机要在开发者选项里打开"USB 安装"，并在安装时的提示上点继续；这要用户自己操作。**真机上一项测试都还没有跑。**

### 2026-10-07 移动端 GPU 测试的设置项和 Windows 对齐

- **起因：** 用户指出安卓和鸿蒙的测试列表不能细调，要和 Windows 一样。原来的做法是把"粒子 26 万 / 100 万""GPU Burn 16 / 64 / 256 步"列成七个固定项，时长固定 3 秒。
- **现在（Android、HarmonyOS、iOS 相同）：**
  - 测试列表是四项，名字和 Windows 一致：粒子 —— 内存吞吐、等离子晶核 × 万花镜 —— GPU Burn、流体 —— 互动水池、显存带宽 —— 显存 / 缓存（iOS 没有最后一项）。
  - 选中的测试下面是"测试设置"，预设、名字和范围都照 Windows：
    - **粒子数**：轻量 65K、中等 1M、重载 4M、极限 16M、自定义（256 的倍数，上限 67,108,864）。默认中等 1M。
    - **GPU Burn 步数**：轻量 16、中等 64、重载 256、自定义（16–2048）。默认轻量 16。
    - **运行时长**：3 秒、15 秒、60 秒、自定义（1–3600 秒）。默认 15 秒，和 Windows 一样。显存带宽没有时长。
  - 每项测试下面那行保存名随设置变化，例如 128 步是 `gpu_burn_v3_fixed_steps_128_kaleidoscope_android_preview`；流体跑满 15 秒是 `cinematic_liquid_v2_physical_scene_v8_android_preview`，其他时长带 `_preview`。
  - **没有搬过来的 Windows 选项：** Headless、系统内存、多 GPU、RenderDoc、抓帧，移动端宿主没有对应的实现；垂直同步也没有加（移动端的显示方式由系统决定，见下面刷新率那条记录）。
- **引擎：**
  - 三个移动宿主共用 `src/embedded_request.h`。测试名可以带设置：`stream?particles=4194304`、`gpu_burn?steps=64`。旧名字 `stream_heavy`、`gpu_burn_medium`、`gpu_burn_heavy` 仍然可用。
  - 设置按桌面命令行的做法落到配置里：粒子数向上取整到 256，标签是 Light / Medium / Heavy / Extreme / Custom；GPU Burn 的步数夹在 16–2048，粒子缓冲取最小值，标签是 `GPU Burn v3 / Selectable fixed steps`；流体的通用粒子缓冲取最小值，`framesInFlight = 1`，标签是 `Cinematic Liquid v2`。
  - **预热改为引擎自己的规则**（2 秒，短于 8 秒的运行取时长的四分之一），和桌面一样。原来移动端固定最多 0.5 秒。所以手机上 15 秒的运行现在是 2 秒预热加 13 秒测量。
  - **不带设置的 `stream` 现在是 1,048,576 个粒子**（原来是 262,144），和 Windows 的默认值一致。之前的移动端成绩都来自模拟器，没有需要保留可比性的真机成绩。
  - adb / hdc 的启动参数也可以带设置：`--es run "gpu_burn?steps=128"`。不带 `seconds` 时仍然跑 3 秒。页面会显示这次请求的测试和设置。
- **HarmonyOS 的其他改动（用户指出的）：**
  - **CPU 页签的图标**原来是内存条（系统符号里没有处理器，用了最接近的 `memory_module`），现在是自己画的芯片（`ic_chip.svg`）。
  - **悬浮页签离窗口底边太近。** 手机上系统在底部留有导航条的区域，页签自然离开边缘；二合一设备的自由窗口没有这块区域，页签贴在边上。现在窗口底部没有系统保留区域时加 16 vp 的间距（`barBottomMargin`）。
  - **二合一设备上的说明文字**改为"在窗口内运行"：测试画面显示在 App 的窗口里，和 Windows、macOS 一样不强制全屏。行为本身没有变。
  - 自定义数值不在范围内时，"运行"按钮不可用，旁边的文字说明原因。
- **验证（Android 17 模拟器，实际运行）：**
  - 界面上选"重载 4M + 3 秒"运行：引擎报告 4,194,304 个粒子（Heavy），工作集 128 MiB，预热 0.75 秒；成绩里的粒子数和标签正确。
  - adb 带设置的 GPU Burn 128 步：181.50 Gpix-step/s，保存名带 `128`；页面切到 GPU Burn、自定义 128、3 秒。
  - 流体 15 秒：预热 2.0 秒、测量 13.0 秒，保存为 `cinematic_liquid_v2_physical_scene_v8_android_preview`，通用粒子缓冲 256。
- **验证（HarmonyOS 7.0 二合一模拟器，实际运行）：**
  - 界面上选 GPU Burn、中等 64、3 秒运行：173.55 Gpix-step/s。
  - hdc 带设置的 128 步：185.31；再在界面上按"运行"（用页面上显示的自定义 128）：185.63。Android 模拟器同一块显卡是 181.50。
  - hdc 带设置的粒子 65,536：引擎报告 Light。
  - 芯片图标、"在窗口内运行"的文字、悬浮页签的底边距都在截图里看过。
  - **没有验证：在自定义输入框里打字。** 模拟器的系统输入法（小艺键盘）第一次使用时弹出隐私协议，同意与否应由用户决定，所以选了取消并退出。自定义数值是通过 hdc 请求带进页面的。
  - 输入法窗口关闭的那一刻正在启动的一次运行失败了（`vkAcquireNextImageKHR failed`），界面显示了错误，没有崩溃；之后的运行正常。没有再追。
- **验证（Windows）：** 引擎重新编译，`ctest` 通过。这轮没有改桌面的行为。
- **只做过类型检查：** iOS 运行页的同样改动（`scripts/check-apple.ps1` 通过）。iOS 宿主的 C++ 改动和另外两个宿主是同一份代码，但没有为 iOS 编译过。
- **回答用户的两个问题：**
  - **粒子和流体在鸿蒙上能不能跑：** 粒子在 6.1.1 和 7.0 的模拟器上能跑。流体在模拟器上不能：虚拟 GPU 不报告 `R32_SFLOAT` 既可采样又可作存储图像，而这是 Vulkan 规范要求每个实现都支持的，所以符合规范的真机驱动应该没有这个问题，但没有真机，没有验证。Android 模拟器上流体能跑。
  - **鸿蒙 5 上：** 5.1.1 的模拟器镜像没有 Vulkan 驱动，任何 GPU 测试都跑不了，只能跑 CPU 测试。真机是否有驱动要上真机才知道。App 的最低版本是 HarmonyOS 5.0.0(12)，代码上没有排除它。
  - **手机形态为什么没有悬浮页签：** 这个页签是 HarmonyOS 6.1（API 23）才有的系统组件（SDK 里 `HdsTabsFloatingStyle` 标注 `@since 6.1.0(23)`）。装的两个手机类模拟器都是 5.1.1，所以走普通页签。6.1 以上的手机会显示，但没有这样的模拟器，没有看过。

### 2026-10-07 HarmonyOS 在模拟器上实际运行

- **环境：** DevEco Studio 26.0 的模拟器，宿主是这台 Windows 工作站（RTX 5090，Ryzen 7 9800X3D）。这是鸿蒙版第一次真正运行。未签名的 HAP 可以直接装进模拟器，不需要签名。
- **怎么从命令行启动：** `Emulator.exe -hvd <名字> -path %LOCALAPPDATA%\Huawei\Emulator\deployed -imageRoot %LOCALAPPDATA%\Huawei\Sdk`（`Emulator.exe` 在 DevEco 的 `tools\emulator` 下），停止用 `Emulator.exe -stop <名字>`。一次只能开一个，都占 `127.0.0.1:5555`。`aa force-stop` 之后要等两秒再 `aa start`，否则启动请求被系统接受后丢掉，没有窗口也没有日志。
- **四个模拟器的结果：**

  | 模拟器 | 系统 | 界面 | CPU 测试 | GPU 测试 |
  |---|---|---|---|---|
  | `Huawei_Phone` | 5.1.1（API 19） | 四个页面正常 | 算力、缓存 / 内存 / 存储都跑通 | **跑不了**：系统有 Vulkan 加载器但没有驱动（`VK_ERROR_INCOMPATIBLE_DRIVER`） |
  | `Huawei_TripleFold` | 5.1.1（API 19） | 折叠是手机布局，展开是侧边导航加两栏 | 跑通 | 同上，没有驱动 |
  | `Huawei_TV` | 6.1.1（API 24） | 4K 全屏，侧边导航加两栏 | 没有单独测 | GPU Burn 跑通，画面铺满整个屏幕 |
  | `Huawei_2in1_Foldable` | 7.0（API 26） | 窗口化，可以拖动改变大小 | 跑通 | 粒子、GPU Burn 三档跑通；流体和显存带宽见下 |

- **`Huawei_Car`（7.0 Beta1）没有启动：** 日志是 `can not read uuid file` / `can not read sn`。它的镜像标着 `os.isPublic=false`，要由登录了账号的 DevEco 启动；命令行启动缺这份授权文件。需要用户在 DevEco 的设备管理器里启动它。
- **运行后发现并修掉的问题：**
  - **一启动就崩溃（所有系统）。** `ReferenceError: TabBarSymbol is not defined`。`TabBarSymbol` 在 SDK 里声明成类，但系统里没有这个构造函数，`new TabBarSymbol()` 编译能过、运行就崩。改成对象字面量（`common/Shell.ets`）。**鸿蒙版在这之前从来没有能启动过。**
  - **TV 上选中的页签文字看不见、选中的测试项几乎没有高亮。** 系统默认的"选中"颜色在 TV 上是很浅的灰色。页签的图标和文字、选中的测试项改用明确的品牌色（`icon_emphasize`、`font_emphasize`、`comp_background_emphasize`、`font_on_primary`）。
  - **显存带宽测试不能中途停止。** 在 7.0 模拟器上每个数据点要 5–37 秒，一共 119 个点，而页面上的"停止"对它不起作用。引擎现在在每个数据点之前检查 `StopRequested()`，被停止的扫描不保存成绩；Android 和 HarmonyOS 的"停止"按钮都接上了。入口会先清掉上一次测试留下的停止请求。桌面版不受影响（Windows 界面是结束子进程）。
  - **两栏布局下"最近一次运行"在测试列表下面，要滚动才看得到。** `AdaptivePanes` 加了第三个位置 `result`：两栏时放在"运行"按钮下面，单栏时放在最后。
  - **`vkCreateInstance failed` 没有说原因。** 现在带上原因和 `VkResult`，并打印加载器提供的实例扩展。5.1.1 模拟器上就是靠这个确认"有加载器、没有驱动"。
- **顺带知道的：** 我第一次改两栏布局时，把条件判断写在了页面传给 `AdaptivePanes` 的构建函数里，结果两边都不渲染。条件要由接收的组件自己判断。
- **数字对得上的地方（同一块 RTX 5090）：**

  | GPU Burn | Windows | Android 模拟器 | HarmonyOS 7.0 模拟器 | HarmonyOS 6.1.1 TV 模拟器（4K 屏） |
  |---|---|---|---|---|
  | 16 步，Gpix-step/s | 131.12 | 128.27 | 128.80 | 128.82 |
  | 每帧渲染，ms | 0.225 | 0.230 | 0.229 | 0.229 |
  | 256 步，Gpix-step/s | 没有测 | 187.5 | 190.59 | 没有测 |

  - 这说明鸿蒙上固定 1280 × 720 渲染生效了（`SET_BUFFER_GEOMETRY`），分数不随屏幕分辨率变化。
  - 7.0 模拟器上 16 步连跑 8 次，128.56–128.85，相差 0.2%。
- **模拟器里不可信的数字：**
  - 粒子测试：计算 8.6 ms，1.22 GB/s（Android 模拟器上同一块卡是 0.007 ms）。
  - 显存带宽：一次读 64 GiB 的调度报 0.009 ms，折合 770 万 GB/s，物理上不可能。虚拟 GPU 的计算时间戳两个方向都会错。
  - 流体：引擎拒绝运行，`Cinematic Liquid v2 requires sampled+storage R32_SFLOAT 3D images`，虚拟 GPU 不报告这项能力。界面显示了原因，没有崩溃。
  - CPU 带宽各级缓存几乎一样快（7–14 GB/s），和 Android 模拟器上的现象相同。（**更正：** 原因是移动端的包没开优化，不是模拟器，见 2026-10-07 第一台安卓真机那条记录。）
- **看过并且正常的：**
  - 四个页面、中文和英文（模拟器两次启动系统语言不一样，两种都看到了）、立方体图标。
  - hdc 入口：`cpu_all`、`cpu_multi`、`cpu_membw`、`stream`、`gpu_burn`、`gpu_burn_heavy`、`cinematic_liquid`、`membw` 都能自动开始，hilog 里有 `AUTORUN accepted / started` 和 `RUN finished`。
  - 绑核被系统允许（`strict_sched_affinity`），缓存容量读得到（L1 48 KiB、L2 1 MiB、L3 96 MiB，是宿主 CPU 的），存储测试用直接读写跑通。
  - 成绩文件：`platform = HarmonyOS`、`osVersion = HarmonyOS 5.1.0 (API 19)` / `HarmonyOS 7.0.0 (API 26)`、`deviceModel = HUAWEI emulator`、`cpuName` 来自 `/proc/cpuinfo`、`systemMemoryMB = 3933`（设备的 `MemTotal` 是 4027396 kB）。失败和被停止的运行没有留下成绩。
  - **GPU 测试的画面：** TV 上铺满 4K 屏幕；二合一设备不能强制全屏和横屏，画面显示在 App 自己的窗口里，16:9，不变形。5.1.1 手机上 Vulkan 不可用，全屏只看到了进入和退出，没有看到画面。
  - **新外壳（悬浮页签）：** 7.0 的二合一模拟器上加载的是 `IndexHds`（日志 `shell=floating api=26 device=2in1 hds=true`）。窗口宽的时候是侧边导航；把窗口拖窄到 600 vp 以下，页签变成悬浮在内容上方的胶囊，内容从它下面滚过。拖动过程中 App 没有重启。TV 按设备类型走普通外壳（`shell=plain api=24 device=tv`）。
  - **三折叠：** 用户在模拟器上手动切换了折叠状态（屏幕宽度 3184 → 2048 → 1008 → 3184），App 进程没有变，测试结果保留，布局跟着切换。用命令切换不行（`hidumper -s DisplayManagerService -a -p / -z / -y` 在模拟器上不改变屏幕）。
- **没有验证的：** 车机；真机（芯片名、大小核、温控、真实的 GPU 数字）；手机类型设备上的全屏横屏画面（两个手机模拟器都没有 Vulkan 驱动）；HarmonyOS 6.1 以上手机上的悬浮页签（没有这样的模拟器，只在二合一上看过）；TV 上用遥控器焦点操作。

### 2026-10-07 各平台对齐：移动端流体和显存带宽、iOS 运行页、刷新率判定

- **移动端现在能跑的测试：**

  | 测试 | Android | HarmonyOS | iOS |
  |---|---|---|---|
  | 粒子（26 万 / 100 万） | 模拟器上跑过 | 只编译 | 只做过类型检查 |
  | GPU Burn（16 / 64 / 256 步） | 模拟器上跑过 | 只编译 | 只做过类型检查 |
  | 流体（互动水池） | 模拟器上跑过 | 只编译 | 只做过类型检查 |
  | 显存 / GPU 缓存带宽 | 模拟器上跑过 | 只编译 | 没有（这项测试只有 Vulkan 版） |
  | CPU 缓存、内存带宽和存储读写 | 模拟器上跑过 | 只编译 | 只做过类型检查 |
  | CPU 算力 | 模拟器上跑过 | 只编译 | 只做过类型检查 |

- **流体（`cinematic_liquid`）：**
  - Android 和 HarmonyOS 的宿主（`src/android_engine_host.cpp`、`src/ohos_engine_host.cpp`）认这个名字，全屏运行，和粒子、GPU Burn 走同一条路。
  - 流体和显存带宽的着色器打进了两个 App：Android 的 `android/app/src/main/cpp/CMakeLists.txt` 和 `scripts/build-ohos.ps1` 按文件名匹配 `cinematic_liquid_*`、`mls_mpm_*`、`membw_*`，现在一共 38 个 `.spv`。
  - 保存的测试名是引擎写的，3 秒运行时是 `cinematic_liquid_v2_physical_scene_v8_preview_android_preview`（HarmonyOS 结尾是 `_ohos_preview`）；界面上显示的名字已经改成和它一致。
- **显存 / GPU 缓存带宽（`membw`）：**
  - `src/vulkan_membw.cpp` 编进了 Android 和 HarmonyOS 的引擎（`HAVE_VULKAN_MEMBW`）。这项测试只做计算，没有画面，所以不走全屏，而是和 CPU 测试一样走引擎的命令行入口：`--workload membw --api vulkan --gpu 0`。
  - 命令行入口原来在可执行文件旁边找着色器，App 里没有这个位置。新增环境变量 `GPU_BENCH_SHADER_DIR`，由 JNI 和 NAPI 设置，**只在 Android 和 HarmonyOS 的构建里读取**，桌面版不受影响。
  - **手机上的成绩单独分组：** 保存的测试名是 `membw_v1_android_preview` / `membw_v1_ohos_preview`（`src/main.cpp`），和 CPU 带宽测试的做法一样。**改之前在模拟器上存成了 `membw_v1`，和桌面正式成绩同组**，是这次核对结果文件时发现的。
  - GPU 页显示读、写、复制三条曲线和各自的缓存 / 显存带宽。
- **iOS 运行页重写（`ios-gui/Mangekyo/Views/RunView.swift`）：**
  - **原来的问题：** 页面是从 macOS 版搬过来的，GPU 选择、Headless、垂直同步、系统内存、抓帧、粒子数和步数这些选项，iOS 的宿主一个都不读；默认选中的粒子测试因为参数里没有 `stream` 这个词，走不到内嵌宿主；流体在列表里但宿主不认。
  - **现在：** 和 Android、HarmonyOS 相同的六项测试（粒子两档、GPU Burn 三档、流体），时长 3 / 10 / 15 秒，点"运行"后横屏全屏（`fullScreenCover`），跑完自动回到页面，显示成绩和图表。
  - iOS 宿主（`src/ios_engine_host.cpp`）认 `stream_heavy`、`gpu_burn_medium`、`gpu_burn_heavy`、`cinematic_liquid`；`cinematic_liquid_v2.metal` 加进了 App 的资源（`ios-gui/project.yml`），启动时和另外两个着色器一起拷到 Documents（改为每次启动都重新拷，原来拷过就不再更新）。
  - `MetalBenchView` 不再按视图大小改 `drawableSize`。原来的写法会在布局或转屏时把渲染尺寸改掉，而分数按 1280 × 720 计算，和 Android 上修过的是同一类问题。
  - 横屏靠新加的 `Components/ScreenOrientation.swift`（应用代理返回当前允许的方向，加 `requestGeometryUpdate`）。
  - `MangekyoApp.swift` 里 `onChange(of:)` 用了两个参数的闭包，那是 iOS 17 的接口，而 App 支持到 iOS 16，改成了一个参数的写法。
  - **验证程度：** `RunView.swift` 和 `BenchEngine.swift` 通过了 Swift 6 类型检查（`scripts/check-apple.ps1`，0 错误）。`MetalBenchView.swift`、`ScreenOrientation.swift`、`MangekyoApp.swift`、`ios_engine_host.cpp` 和 Metal 后端**没有任何工具检查过**。iOS App 至今没有用 Xcode 编译过。
- **"帧率被刷新率限制"的判定覆盖所有 API：**
  - 原来只有 Vulkan 和 Metal 后端会设置 `presentFollowsDisplay_`。现在 `AppBase` 构造时按"开了垂直同步且有窗口"设初值，Vulkan 和 Metal 再按设备的实际情况覆盖。
  - **Windows 界面**把引擎的 `>> Display-bound` 显示成一条运行提示（`GpuRunIssueKind::DisplayBound`），文字和其他平台一致，多一句"请关闭垂直同步"。
  - **实测（Windows，RTX 5090，240 Hz，16384 个粒子，开垂直同步）：** DX12 240 FPS、GPU 忙碌 0.2%，判为 Display-bound；OpenGL 239 FPS、0.3%，判为 Display-bound。
  - **DX11 判定不出来：** 同样条件下 239 FPS，但渲染时间量出来是 4.144 ms（不开垂直同步时是 0.062 ms），正好是一帧的时长，GPU 忙碌算成 99.8%，被判为 GPU-bound。也就是 DX11 开垂直同步时，驱动给的 GPU 时间戳把等屏幕的时间算进去了。这是原有的计时特性，没有改。粒子测试的分数只用计算时间（0.003 ms），不受影响。
  - **Vulkan 在 NVIDIA 上开垂直同步并不限帧：** 引擎在垂直同步打开时优先选 `mailbox`，同样条件下跑到 3331 FPS，成绩里 `vsync` 仍记为 true。这也是原有行为，没有改，记在这里供以后决定。
- **验证（Android 17 模拟器，实际运行）：**
  - 流体：全屏运行，320,920 个粒子，215.94 MParticle-step/s；保存名和界面一致。
  - 显存带宽：读 1439 GB/s，保存为 `membw_v1_android_preview`；曲线显示正常。
  - 模拟器里的数字不是成绩：GPU 是宿主机的 RTX 5090，时间戳来自虚拟 GPU。
- **验证（Windows）：** 引擎、界面重新编译，0 个新警告；`ctest` 通过。Windows 界面里的新提示**没有打开界面看过**，只确认了它匹配的那一行引擎确实会打印。
- **验证（HarmonyOS）：** `scripts\build-ohos.ps1` 编译通过（HAP 7.1 MB）。没有运行。
- **没有做：** HarmonyOS 上机；iOS 用 Xcode 编译和上机；Android 真机；iOS 和 macOS 上的显存带宽测试（需要先有 Metal 版）。

### 2026-10-07 移动端 GPU 测试：固定渲染尺寸、全屏运行、图标

- **起因：** 用户问预览画面为什么这么小、会不会影响性能。查下来**会，而且成绩算错了**：
  - 安卓和鸿蒙的交换链取的是预览控件的像素尺寸（手机上是 932 × 524），渲染的像素数跟着预览区大小变；
  - 但分数和成绩里的分辨率一直按桌面版的常量 1280 × 720 计算。
  - 结果是 GPU Burn 的分数被高估（手机预览下约 1.9 倍），并且随布局变化：同一个模拟器上出现过 224.8 和 297.7 两个数。
  - iOS 和 macOS 不受影响，Metal 后端一直把渲染尺寸固定在 1280 × 720。
- **引擎的改动：**
  - 安卓和鸿蒙（`GPU_BENCH_EMBEDDED_VULKAN`）的交换链固定为 `kWindowWidth × kWindowHeight`，由系统缩放到显示区域；鸿蒙通过 `SET_BUFFER_GEOMETRY` 设置。
  - `AppBase` 新增 `renderWidth_ / renderHeight_`，分数和成绩里的分辨率按实际渲染尺寸计算。桌面版这两个值始终是常量，行为不变。
  - 安卓和鸿蒙的 `preTransform` 改为不旋转，横屏时由系统旋转画面。
- **界面的改动（安卓和鸿蒙相同）：**
  - GPU 测试开始时横屏占满屏幕，隐藏系统栏，屏幕常亮，只显示画面、测试名称和"停止"按钮；跑完回到页面。返回键等于停止。
  - 页面里原来的小预览去掉了，换成一段说明。
  - 流程是先切到全屏、等新的渲染表面建好再启动引擎，所以运行中不会销毁表面。
- **图标和文字：**
  - GPU 页的图标在所有平台改成立方体，各用自己的风格：Windows 用 Segoe Fluent Icons 的 `F158`，iOS / macOS 用 SF Symbols 的 `cube.fill`；Material 图标集和鸿蒙的系统符号里没有立方体，安卓和鸿蒙用同一个三面实心立方体（安卓画成矢量图，鸿蒙是 `ic_cube.svg`）。
  - 安卓原来没有启动图标（清单里没写，显示的是系统默认图标），现在用 Windows 版的图标（`gui/app.ico`）生成了自适应图标。
  - 安卓 GPU 页的"图形后端 / Backend"改为"图形 API / API"；其他平台本来就叫 API。
- **验证（Android 17 模拟器，实际运行）：**
  - GPU Burn 全屏运行：截图是横屏 2424 × 1080，画面 16:9 居中；引擎报告 1280 × 720，跑完自动回到竖屏页面。
  - **修正后的数字和桌面版对得上：** 模拟器里 128.27 Gpix-step/s（每帧渲染 0.230 ms），同一块 RTX 5090 在 Windows 上同样 16 步是 131.12 Gpix-step/s（0.225 ms）。修正前模拟器报的是 224.8。
  - 启动图标、立方体图标、"API"字样都在模拟器上看过。
- **验证（Windows，实际运行）：** 引擎、界面重新编译，`ctest` 通过；GPU Burn 仍报告 1280 × 720，数字和以前一样。
- **只编译、没有运行：** HarmonyOS 的全屏运行和固定渲染尺寸（`SET_BUFFER_GEOMETRY` 是否生效、横屏和隐藏系统栏的实际表现都没有看过；编译器提示转屏和全屏布局在二合一、TV、车机上不可用，代码里按设备类型跳过了）。iOS / macOS 只改了一个图标名，没有编译。Windows 的新图标没有打开界面看。
- **帧率和屏幕刷新率（用户追问后补充）：**
  - **能不能超过刷新率，取决于系统给不给不同步的显示方式。** 引擎在垂直同步关闭时依次要 `immediate`、`mailbox`，都没有才用 `fifo`（跟随刷新率）。现在每次运行都会打印一行 `[Vulkan] Present mode: ...`。
  - **Android 模拟器给的是 `mailbox`**，不受刷新率限制：60 Hz 的虚拟屏幕上粒子测试 1212 FPS、GPU Burn 重度 284 FPS。真机上是不是也这样，没有设备，没有验证。
  - **iOS 一定跟随刷新率**（系统没有关闭的开关），所以 iOS 上轻量测试必然顶着 60 或 120 FPS。鸿蒙的情况不知道。
  - **顶着刷新率时的处理（用户决定）：** 不去绕过，而是提示成绩可能不准、请换更重的测试。引擎里新增判定：显示方式跟随刷新率，并且 GPU 忙碌比例低于 80%，就在摘要里打印 `>> Display-bound: ...`，成绩的 `bottleneck` 记为 `Display-bound`，`vsync` 记为 true。Android、HarmonyOS、iOS、macOS 的"最近一次运行"里看到这一行就显示警告。Android 和 HarmonyOS 另外加了一条：平均帧率和屏幕刷新率相差不到 4% 也提示。
  - **实测到这个判定（Windows）：** 9800X3D 的核显开垂直同步跑 16384 个粒子，显示方式是 `fifo`，帧率正好 240（显示器刷新率），GPU 忙碌 3.4%，引擎给出 `Display-bound`。同一块核显跑 100 万粒子时 GPU 忙碌 83%，判为 GPU-bound，没有误报。
  - **移动端补了更重的档位**，否则提示里的"换更重的测试"没有可选项：粒子 100 万（`stream_heavy`）、GPU Burn 中等 64 步和重度 256 步（`gpu_burn_medium`、`gpu_burn_heavy`）。adb / hdc 的启动参数也认这几个名字。模拟器上 256 步是 187.5 Gpix-step/s（每帧 2.5 ms）。
- **没有做：** 真机上的显示方式和帧率；鸿蒙上机；iOS 的警告只做过类型检查。

### 2026-10-07 Android 在模拟器上实际运行

- **环境：** Android 模拟器 37.2.12，宿主是这台 Windows 工作站（RTX 5090，Ryzen 7 9800X3D）。两个虚拟设备：`Pixel_10a`（Android 17，API 37，x86_64，16 KB 页）和 `Medium_Phone`（Android 7.0，API 24，x86）。这是 App 第一次真正运行。
- **模拟器起不来的原因（和 App 无关）：** 报错是 "It seems too many emulator instances are running on this machine"。实际原因是这台机器的 TCP 动态端口范围被改成从 1024 开始，Hyper-V 把 5533–6634 等端口段整段保留了，模拟器要用的 5554–5682 一个都绑不上。用户用管理员权限把范围改回默认值（49152 起），并手工保留了 5554–5683，之后正常。
- **运行后发现并修掉的三个问题：**
  - **GPU 测试一启动就崩溃（Android 17）。** 崩在系统的 `libvulkan.so` 里，地址 0。原因：安卓和鸿蒙的 Vulkan 实例是按 1.0 创建的，但 `vulkan_backend.cpp` 在设备支持 1.2 时会去调 1.1 的 `vkGetPhysicalDeviceProperties2`（为了取驱动名称）。安卓的加载器会给出这个函数的地址，再跳进一个空的驱动入口。现在嵌入式构建跳过这一段。鸿蒙走同一段代码，一起修了，没有在鸿蒙上验证。
  - **预览画面看不到渲染。** `BenchmarkSurface` 给 `SurfaceView` 设了背景色，这个背景画在窗口里，盖在渲染画面前面。去掉背景后预览正常；没跑过测试时改用一层占位色盖住。
  - **Android 7.0 上，每次启动后第一条成绩的编号是 UTC 时间**，和它自己的时间戳差了时区的小时数。原因是旧版系统库的 `localtime_r` 不会自己读时区。现在转换前先调 `tzset()`。
- **顺带改的：** 运行结束后状态文字改为"已完成"（原来一直停在"正在运行"）；历史页改为最新的在最前；失败且一帧都没画出来时恢复占位色。
- **看过并且正常的（Android 17）：**
  - 启动、四个页面、浅色和深色、中文和英文；顶部只有一份状态栏高度，底部导航正常。
  - adb 入口：`cpu_multi`、`cpu_all`、`cpu_membw`、`stream`、`gpu_burn` 都能自动开始，logcat 里有 `AUTORUN accepted / started` 和 `RUN finished`，引擎输出在 `MangekyoEngine` 标签下。
  - 成绩文件：`deviceModel = Google sdk_gphone16k_x86_64`、`cpuName = AOSP ranchu`、`osVersion = Android 17 (API 37)`、`systemMemoryMB = 3912`（设备的 `MemTotal` 是 4006172 kB），历史页那一行显示正确。
  - 宽屏布局：用 `wm size` 和 `wm density` 把屏幕改成 1000 × 1200 dp 和 1280 × 800 dp。侧边导航、两栏、1200 dp 以上导航展开显示文字，都和设计一致。
  - **运行中改屏幕大小：** GPU Burn 运行到一半时把屏幕从平板大小改回手机大小，进程号不变，引擎没有重启，测试正常跑完（13.5 秒，16200 帧），预览画面一直在。
- **看过并且正常的（Android 7.0）：** 启动和布局正常；没有 Vulkan 驱动，GPU 测试给出"No Vulkan physical device found"，不崩溃；CPU 测试跑完并保存；`deviceModel = Google Android SDK built for x86`，`osVersion = Android 7.0 (API 24)`，内存 2022 MB。
- **模拟器里的数字不能当成绩，其中两个明显不对：**
  - 粒子测试报 1490 GB/s（计算耗时 0.007 ms）。26 万个粒子不可能这么快，是虚拟 GPU 的时间戳不可信。
  - 缓存 / 内存带宽测试里 L1、L2、L3、内存的单线程读速度全是 7–8 GB/s，没有台阶。这台机器原生的 L1 是 588 GB/s。原因没有查，可能是虚拟机里读时钟很慢。
- **改完之后的回归：** Windows 的引擎、命令行程序、界面重新编译，`ctest` 通过，新存的成绩编号和时间戳一致；HarmonyOS 重新编译通过。
- **没有做：** HarmonyOS 的模拟器还没有启动过；Android 真机没有；用手指点界面的操作只试了导航和滚动，按钮主要是靠 adb 参数触发的。
- **用 Git Bash 跑 adb 的注意事项：** 设备上的路径（`/sdcard/...`）会被 Git Bash 改写成 Windows 路径，命令前要加 `MSYS_NO_PATHCONV=1`，或者在 PowerShell 里运行。

### 2026-10-06 Android 和 HarmonyOS：大屏、折叠屏和新界面（只写了代码）

- **状态：** 代码已写，`assembleDebug` 和 `scripts\build-ohos.ps1` 都通过。**用户要求只写代码、不测试，所以没有在任何模拟器或设备上运行，界面一眼都没看过。** 下面的"会怎样显示"都是按代码推断的。
- **原来的情况（读代码得出）：** 两端都只有手机布局：底部导航加一列拉满宽度的卡片，预览画面是"拉满宽度 × 200"。Android 在折叠屏展开或分屏拖动时会重建页面；HarmonyOS 装不到 TV 和车机上。
- **两端共同的做法：**
  - 按**窗口**宽度分档，不按设备类型：600 起导航从底部移到侧边，840 起一页分成左右两栏，各自滚动；
  - 单栏时内容最宽 720，不再无限拉宽；
  - 历史页改成网格（1 到 3 列），图表页在宽窗口上并排两张；
  - 预览画面固定 16:9；
  - 放预览画面的那一栏在两种布局里处于同一位置，所以运行中折叠、旋转、改窗口大小时预览画面不会被销毁重建。
- **Android：**
  - 新文件 `ui/AdaptiveLayout.kt`（宽度分档 `WindowWidth`、两栏容器 `AdaptivePanes`）；`ui/AppRoot.kt` 重写。
  - 导航：窄窗口用 `ShortNavigationBar`，600 dp 起用 `WideNavigationRail`，1200 dp 起侧栏展开显示文字。
  - `AndroidManifest.xml` 的 `configChanges` 增加 `smallestScreenSize`、`density`、`keyboard`、`navigation`，折叠屏展开、分屏拖动、接上键盘时不再重建 Activity。
  - 去掉了外层的 Scaffold。原来外层的边距没有标记为已处理，每页的标题栏很可能会再加一次状态栏高度（这个问题本身也没有在设备上确认过）。
  - **Material 3 Expressive 能用的只有一部分。** 工程用的 material3 是 1.4.0，其中公开的 Expressive 接口只有上面两个导航组件；它的主题、动效方案、强调字重和放大的圆角在 1.4.0 里都是 `internal`。所以另外只做了两件事：主题里的圆角改成 16 / 20 / 32 dp，运行按钮改成带文字的。其余 Expressive 组件在 1.5 的预览版里，没有引入。
  - 没有新增依赖。折叠屏的铰链位置和半开姿态（需要 `androidx.window`）没有处理。
  - **不用预览版（用户 2026-10-06 决定）。** 当天查到的情况：material3 最新是 1.5.0-alpha29，最新的 Compose BOM（2026.09.00）仍然对应 1.4.0，所以 Expressive 的其余组件要等 1.5 出正式版。在那之前只用稳定接口补了一处：GPU 页里被选中的测试卡片，圆角和颜色用弹簧动画变化。
- **HarmonyOS：**
  - 新文件 `components/AdaptivePanes.ets`、`common/Shell.ets`、`pages/IndexHds.ets`；`AppState` 增加窗口宽度和由它推出的几个属性。
  - `module.json5`：设备类型增加 `tv` 和 `car`，最小窗口 360 × 480。
  - 页签加了系统图标；600 vp 起页签竖排在侧边。历史页和图表页用 `GridRow`。
  - **新外壳只在 HarmonyOS 6.1（API 23）以上的手机、平板、二合一上加载。** `EntryAbility` 按系统版本、设备类型和系统能力选择加载 `pages/IndexHds`（UI Design Kit 的 `HdsTabs`，悬浮页签加系统材质）还是原来的 `pages/Index`。SDK 标明这些接口在 TV 和车机上不可用，所以那两类设备始终用原来的外壳。
  - 编译器对 `IndexHds.ets` 有 15 条警告，内容是"接口从 6.0 / 6.1 起支持，而兼容版本是 5.0.0(12)"以及"TV、车机不可用"，都在预期之内；其他文件没有警告。
  - **系统材质（鸿蒙对应苹果"液态玻璃"的东西，SDK 里叫 immersive material）用在哪里：**
    - 6.1 以上的新外壳里，悬浮页签用了它；
    - `module.json5` 增加了 `ohos.arkui.UIMaterial.state = enable`。HarmonyOS 7（API 26）上，对话框、菜单、开关、滑块、半模态面板这些系统控件默认带材质；更早的系统忽略这一项。App 现在几乎没用到这些控件，所以眼下看不出区别。
    - **内容卡片不能用。** 华为文档（`@ohos.arkui.uiMaterial`，2026-09-24 版）写明：普通组件上的沉浸式材质只在 Navigation 的标题栏和底部页签栏里生效，放在其他区域不生效。所以没有给卡片加。
- **未验证（全部要在模拟器或设备上看）：**
  - 两端在手机、展开的折叠屏、平板上的实际显示，包括侧边导航、两栏的比例和滚动；
  - Android：顶部和底部的边距是否正确；折叠、分屏、旋转时页面是否真的不重建，预览画面改变大小后引擎是否正常；`WideNavigationRail` 展开和收起；
  - HarmonyOS：`HdsTabs` 悬浮页签的样子、页面底部留的 96 vp 是否合适、它是否真的浮在内容上；页签图标在 5.1 的系统上是否存在；老系统上不加载 `IndexHds` 这个判断是否如预期；TV 上用遥控器能否操作；车机上的显示；
  - 两端运行中改变窗口大小时测试是否继续。

### 2026-10-06 成绩文件的前后兼容

- **起因：** 用户要求把前后兼容做好。读了成绩文件的读写代码（`src/benchmark_results.cpp`）和各端的读法，原来的实现有这些问题。它们都是读代码和写测试时发现的，这台机器上的真实成绩文件里一个都没有发生过：
  - 不认识的字段在重写文件时被丢掉，所以旧版本会删掉新版本写的内容；
  - 数字位置上不是数字时抛异常，WinUI 历史页有一处没有捕获，会直接崩溃；
  - 文件读不出来（被截断，或是别的布局）时，下一次保存会用"读到的那一点"覆盖整个文件；
  - 保存是直接覆盖写，写到一半崩溃文件就坏了；
  - 字符串里有花括号、或者以反斜杠结尾时，读回来会错位；
  - `nan` 和控制字符写进文件后不是合法的 JSON，HarmonyOS 端会整份历史都读不出来。
- **改动：**
  - **读：** 换成一个容错的 JSON 读取器。不认识的字段保留在 `BenchmarkResult::unknownFields`；类型不对的值取最接近的值；读不全的文件读出能读的部分。
  - **写：** 布局和原来逐字节相同。新增：未知字段原样写回、`\u` 转义（控制字符，以及旧版本读不了的三种文字）、非有限的数写成 0。
  - **保存：** 先写临时文件再整体替换；读不全的文件在重写前另存为 `results.json.bak-unreadable-<时间>`。
  - **字段表：** 读和写共用一张 `kFields` 表，以后加字段只加一行。
  - **新接口：** `ParseResultsJson()` 和 `SerializeResultsJson()`（不碰磁盘，测试用）。
  - **测试：** 新增 `tests/results_compat_test.cpp` 和 CMake 目标 `results_compat_test`（选项 `GPU_BENCH_BUILD_TESTS`，默认开；嵌入式构建不编）。
  - **前端：** Android 的 `ResultsStore.appendRawObject` 删掉了（没有调用方；解析失败时会用空数组覆盖文件）。HarmonyOS 的读取加了"不是数组"和"空条目"的判断。
  - **顺带：** CSV 导出里带引号的字段按 CSV 规则写成两个引号。
- **验证（实际运行，Windows 11，Ryzen 7 9800X3D）：**
  - **和原来的实现对拍：** 这台机器上的 55 个真实成绩文件，共 11,892 条成绩（最早的格式没有版本号，另有版本 1、2、4）。旧读法和新读法逐字段相同，旧写法和新写法逐字节相同，重写两遍结果不变。最大的文件 710 KiB，读一遍约 10 ms，和原来一样。
  - **测试：** `ctest` 通过，153 条检查全过。
  - **旧版本的程序：** 用 `out/` 里留着的 0.1.0、0.2.5、0.2.6 三个 `gpu_benchmark.exe`。文件是用户真实成绩的副本（236 条）加一条新版本写的成绩、一条"更新版本"的成绩（带未知字段）和一条带花括号等文字的成绩，共 239 条。
    - 三个旧版本都能列出全部 239 条，删掉一条后其余 238 条顺序和数值不变；
    - 旧版本重写后丢掉了 `deviceModel`、`systemMemoryMB` 和未知字段，带转义的文字显示成 `Odd \u007bGPU\u007d name`，都在预期之内；
    - 新版本能读旧版本重写过的文件并继续追加，没有触发留副本；
    - 新版本自己删一条时，"更新版本"那条成绩的未知字段和版本号 9 都还在。
  - **界面：** 重新编译的 WinUI 用临时数据目录打开这个混合文件，历史页正常显示旧成绩；只看不改时文件没有变化。
  - 所有测试都在临时目录和副本上做，没有写过 `%LOCALAPPDATA%` 下的真实成绩文件。
- **验证（Ubuntu 20.04 容器，GCC 9，实际运行）：** 引擎和测试编译零警告；`ctest` 通过，153 条全过；引擎在最早格式的文件上追加后旧成绩不变；在带未知字段的文件上追加后未知字段还在；文件被截断时留了副本并保存了新成绩。
- **验证（只编译）：** Android `assembleDebug`；HarmonyOS `scripts/build-ohos.ps1`；macOS 目标文件（检查脚本新增了 `benchmark_results.cpp` 和 `path_service.cpp`，arm64 和 x64 都通过）。
- **未验证：**
  - Android、HarmonyOS、iOS、macOS 上没有运行过，包括"先写临时文件再替换"在应用沙箱里的行为；
  - Swift 这一轮没有改，也没有重新做类型检查；
  - WinUI 只确认了能打开和显示，没有在界面里做删除操作。

### 2026-10-06 成绩里记录机型、系统版本、芯片和内存

- **起因：** 用户问移动端的成绩是否记录了机型、系统版本和 CPU。读代码后发现三处缺口（没有在手机上看过实际内容）：
  - 成绩里没有机型，也没有内存容量；
  - Android 和 HarmonyOS 的 `osVersion` 记的是内核版本，不是系统版本；
  - GPU 成绩的 `cpuName` 在 Linux 系的系统上只读 `/proc/cpuinfo` 的 `model name` 一行，ARM 内核没有这一行。
- **改动：**
  - 新增 `src/system_info.{h,cpp}`，各平台的读取方法都在这一个文件里；
  - `BenchmarkResult` 增加 `deviceModel` 和 `systemMemoryMB`，由 `AppendResult()` 在保存时统一填写，所以所有测试、所有平台的成绩都带这两个字段；
  - JSON 和 CSV 都写出这两个字段，CSV 的两列加在最后；
  - `resultSchemaVersion` 仍是 4。两个字段是可选的，旧文件照常读取，旧成绩里它们是空和 0。
- **各平台取值：**

  | 平台 | 机型 `deviceModel` | 芯片 `cpuName` | 系统 `osVersion` | 内存 `systemMemoryMB` |
  |---|---|---|---|---|
  | Android | `ro.product.manufacturer` + `ro.product.model` | `ro.soc.manufacturer` + `ro.soc.model`（Android 12 起）；没有时依次退回 `ro.hardware.chipname`、`ro.board.platform` | `Android <版本> (API <级别>)` | `/proc/meminfo` 的 `MemTotal` |
  | HarmonyOS | 品牌 + 市场名称 +（型号代码），来自 deviceinfo 接口 | 系统没有芯片名的接口，仍读 `/proc/cpuinfo` | `<系统名> <版本> (API <级别>)` | 同上 |
  | iOS | 机型标识符（`hw.machine`） | 系统不给芯片名时记为 `Apple Silicon (<机型标识符>)` | `iOS <版本>`（原有） | `hw.memsize` |
  | macOS | `hw.model` | 原有 | 原有 | `hw.memsize` |
  | Windows | 固件里的系统厂商 + 系统名称；组装机的系统名称只是占位文字，改取主板厂商 + 主板型号 | 原有（GPU 成绩里去掉了末尾的空格） | 原有 | 固件报告的已安装容量 |
  | Linux | 设备树的 `model`（ARM 开发板）；没有时取 DMI 的系统名称，再退回主板 | `model name`，没有时取 `Hardware` | 原有 | `MemTotal` |

- **界面：**
  - Android、HarmonyOS、iOS 的历史页在每条成绩下面加一行：机型 · 芯片 · 系统 · 内存；
  - macOS 的历史表已经有 10 列，SwiftUI 一个表最多 10 列，所以改为鼠标停在"设备"一格上时显示；
  - Windows 界面没有加显示。它重新编译了，否则它带的旧测试程序写不出新字段，在历史页删除一条成绩时还会把其余成绩的新字段丢掉。
- **验证（实际运行）：**
  - **Windows：** 重新编译引擎、命令行程序和 WinUI 界面。连续保存两条 CPU 成绩，两条都是 `deviceModel = ASUSTeK COMPUTER INC. ROG CROSSHAIR X670E HERO`、`systemMemoryMB = 98304`（这台机器是 2 × 48 GB）。把一个没有新字段的旧结果文件（23 条）复制出来再追加一条，变成 24 条，旧成绩内容不变。
  - **Linux（Ubuntu 20.04 容器，GCC 9）：** 编译通过，`system_info.cpp` 没有警告；`systemMemoryMB = 46990`，和 `/proc/meminfo` 的 48118064 kB 一致；容器里没有 DMI 信息，`deviceModel` 为空，符合预期。
- **验证（只编译）：**
  - **Android：** `assembleDebug` 通过；独立 CPU 程序（`scripts\build-android-cpumem.ps1`）重新编出。
  - **HarmonyOS：** `scripts\build-ohos.ps1` 通过，`BUILD SUCCESSFUL`。
  - **macOS：** `system_info.cpp` 用 zig 编成 arm64 和 x64 的目标文件。`scripts\check-apple.ps1` 整套重新跑了一遍：两端 Swift 类型检查 0 错误，92 条断言通过，两端共用的六个文件内容一致。这不是用 Xcode 编译。
- **未验证和已知限制：**
  - Android、HarmonyOS、iOS、macOS 上实际读到的文字一次都没有看过。文档和代码注释里的 `Xiaomi 23049PCD8G`、`QTI SM8550`、`iPhone16,1` 之类只是格式示例。
  - 引擎里 iOS 专用的分支（`TARGET_OS_IPHONE`）没有任何工具编译过，两个 Apple 桥接文件（`GpuBenchBridge.mm`）和两个 `HistoryView.swift` 也不在检查范围内。
  - Linux 实机的 DMI 路径和 Jetson 的设备树路径没有试过。
  - HarmonyOS 的芯片名可能记为 `Unknown CPU`，这时要靠型号代码去查。
  - iOS 记的是机型标识符，不是 "A17 Pro" 这样的芯片名；要显示芯片名需要一张机型对照表，没有做。
  - 除 Windows 外，内存记的是内核能用的总量，比标称容量少几个百分点。
  - 应用沙箱是否允许读这些属性和 `/proc/meminfo`，要上机确认。

### 2026-10-06 HarmonyOS（鸿蒙）版接入共享引擎

- **结论：** 代码已写，能用真实工具链编出未签名的 HAP；**没有在任何鸿蒙设备或模拟器上运行过**。
- **原来的状态：** `ohos/` 是一个独立的 Vulkan 粒子演示，自带一份渲染器（`vulkan_renderer.cpp`），和主引擎无关；工程缺少 hvigor 配置、包描述文件和图标等，实际上编不出来。
- **现在的状态：**
  - **引擎：** 新增 `src/ohos_engine_host.{h,cpp}`（对应安卓的 `android_engine_host`），根目录 `CMakeLists.txt` 增加 OHOS 分支；`vulkan_backend.cpp` 增加 `vkCreateSurfaceOHOS` 等鸿蒙分支，和安卓共用"单 GPU、只用 Vulkan 1.0 入口"的路径（宏 `GPU_BENCH_EMBEDDED_VULKAN`）。旧的独立渲染器已删除。
  - **绑核：** 鸿蒙的 C 库没有 `pthread_setaffinity_np`，和安卓一样改用 `sched_setaffinity`（`cpu_benchmark.cpp`、`cpu_membw.cpp`）。这是用鸿蒙工具链实际编译时才发现的。
  - **NAPI 桥接：** `ohos/entry/src/main/cpp/napi_init.cpp`，生成 `libmangekyo.so`：接收 XComponent 的窗口、启动 / 停止 GPU 测试、在原生线程上运行引擎的命令行入口（CPU 测试）、收集引擎输出（同时写入 hilog，标签 `MangekyoEngine`）。
  - **界面（ArkTS）：** GPU、CPU、历史、图表四个页面，功能与安卓版对齐：粒子测试和 GPU Burn（3 秒预览）、CPU 算力测试、缓存 / 内存带宽扫描、各级缓存表格、Canvas 绘制的曲线图 / 柱状图 / 条形图、中英文界面（跟随系统语言）。
  - **远程测试入口：** debug 包支持 `hdc shell aa start -a EntryAbility -b com.mangekyo.benchmark --ps run <测试>`，用法见 `ohos/README.zh-CN.md`。
  - **成绩身份：** `stream_v1_ohos_preview`、`gpu_burn_v3_fixed_steps_16_kaleidoscope_ohos_preview`、`cpu_membw_v1_ohos_preview`，平台字段为 `HarmonyOS`。
- **构建（实际执行过）：**
  - 环境：DevEco Studio 26.0（SDK API 26、hvigor 6.26.8），Windows 11；`scripts\build-ohos.ps1` 先编译着色器，再执行 `ohpm install` 和 `hvigorw assembleHap`。
  - 结果：`BUILD SUCCESSFUL`，`entry-default-unsigned.hap` 约 6.2 MB，包内有 `arm64-v8a` 和 `x86_64` 两个架构的 `libmangekyo.so`、五个 SPIR-V 着色器和 ArkTS 字节码。ArkTS 编译器没有警告。
  - 着色器必须在打包之前生成：第一次构建时资源打包早于原生库编译，包里缺着色器；所以脚本把编译着色器放在最前面。
- **回归（因为改了引擎源码和根 CMake）：**
  - Windows：重新配置并编译 `gpu_benchmark`、`gpu_engine`，通过；
  - Android：`assembleDebug` 通过；
  - Linux（Ubuntu 20.04 容器，GCC 9）：配置和编译通过，CPU 算力测试和缓存 / 内存扫描各跑了一遍短的，退出码都是 0；
  - WinUI 界面这一轮没有重新编译（界面源码没有改动）。
- **没有运行的原因：** 没有连接鸿蒙设备；机器上没有模拟器系统镜像；没有签名配置（需要用户在 DevEco Studio 里登录华为账号后自动生成）。
- **未验证（都要在设备上确认）：**
  - 界面的实际显示、深色模式、图表是否画对；ArkTS 的状态刷新是否按预期工作；
  - 设备是否提供 `VK_OHOS_surface`，粒子测试和 GPU Burn 能否跑完并写出成绩；
  - 应用沙箱是否允许 `sched_setaffinity`（不允许的话算力测试会被判为无效）、能否读到 `/sys/devices/system/cpu` 下的缓存容量；
  - `hdc` 启动参数和 hilog 输出。
- **没有做：** 流体测试、抓帧、正式的移动端时长合同、签名和发布包、CPU 测试的取消。

### 2026-10-06 Android 的 adb 入口和 Apple 检查脚本入库

- **Android，方便用 adb 远程测试（只编译通过，没有设备，一次都没有运行过）：**
  - 引擎打印的每一行同时写入 logcat，标签 `MangekyoEngine`（`mangekyo_jni.cpp`）。原来这些输出只显示在 App 界面里，`adb logcat` 看不到进度和成绩。
  - debug 包支持启动参数：`adb shell am start -S -n com.mangekyo.benchmark/.MainActivity --es run <测试>`，App 打开后自动切到对应页面并开始。测试名为 `cpu_membw`、`cpu_all`、`cpu_per_core`、`cpu_multi`、`stream`、`gpu_burn`；`--ei seconds N` 可选。
  - 开始、拒绝和结束都会在标签 `Mangekyo` 下打印一行（如 `RUN finished kind=cpu exit=0`），脚本可以等这一行。
  - 启动参数只在 debug 包里生效，正式包会拒绝，避免手机上别的 App 用它触发测试。
  - GPU 测试要等预览画面创建（最多 10 秒），所以屏幕必须亮着并解锁。
  - 实现在 `core/AutoRun.kt`、`MainActivity.kt`（`singleTop` 加 `onNewIntent`）、`AppRoot.kt`、`CpuScreen.kt`、`GpuScreen.kt`；用法写在 `android/README.md`。
  - **构建：** `assembleDebug` 通过。机器上装了 adb（37.0.1），但没有连接手机，也没有模拟器镜像。
- **Apple 检查脚本入库：** `scripts/check-apple.ps1` 加 `scripts/apple-check/`（容器里跑的两个脚本、手写的替身模块、测试和引擎输出样例）。
  - 测试 `tests.swift` 原来只是把结果打印出来看，现在改成 92 条断言，失败时脚本返回非零。
  - 引擎输出样例取自这台机器的真实输出，已去掉本机路径。
  - 从仓库里的位置实际跑了一遍：两端类型检查 0 错误，92 条断言全部通过，两端共用的六个文件内容一致，`aarch64-macos` 和 `x86_64-macos` 的目标文件都能编出来。
  - 它能查什么、不能查什么写在 `scripts/apple-check/README.md`，结论不变：**这不是用 Xcode 编译**。
- **Docker 镜像：** `swift:6.0-jammy`（4.31 GB）在跑完上面这一遍之后已经删除；下次运行 `check-apple.ps1` 时会重新下载。

### 2026-10-06 中文界面补全（Windows / Android / iOS / macOS）

- **Windows（已在界面上逐页核对）：**
  - **语言和主题现在会保存。** 原来每次启动都回到"自动"，在英文系统上就是英文，每次都要重新选。现在选择写入数据目录下的 `gui_settings.txt`，下次启动时恢复；设置了环境变量 `MANGEKYO_LANG` 时仍以环境变量为准。
  - 补上漏翻的文字：进度卡片左侧的空闲文字"Ready"、"双 GPU"下拉框里的"Off"（译文早就有，但切换语言后下拉框不刷新）、CPU 页标题下的"16 logical / 8 physical"、成绩行里的"ms compute / ms render"、存储一行里引擎给出的英文说明（如 `page file on C:` 显示为"C: 盘上的页面文件"，以及跳过存储测试的各种原因）。
  - **核对方法：** `GPU_BENCH_DATA_DIR` 指向临时目录，切到简体中文后关闭再启动，不碰设置页直接进入中文；用 UI Automation 导出六个页面上全部文字（包括滚动区域以外的），并跑了一遍缓存 / 内存带宽测试和一遍默认的粒子测试。剩下带英文字母的只有专有名词、API 名称、单位、测试版本号，以及有意保留的"Headless"、"RenderDoc"、"FPS"。
  - 标题栏按钮和信息图标的无障碍名称（Minimize、Close 等）来自系统，跟随 Windows 的显示语言，不受 App 的语言设置影响。
- **Android：** 原来全部是写死的英文。新增 `core/I18n.kt`（`tr(en, zh)`，跟随系统语言），四个页面、能力检测提示、测试项目名称、CPU 测试的状态文字都有了中文；成绩行和引擎的英文说明用和 Windows 相同的译法。只有中文和英文两种语言，其他语言显示英文。`assembleDebug` 通过；没有在设备上看过实际显示。
- **iOS 和 macOS：** 补上粒子数和 Burn 步数预设、"垂直同步"、输入框占位文字、历史页的列名和行内文字、关于页的版本和许可证、引擎包装层的"就绪 / 启动失败"；成绩行和引擎的英文说明通过 `Localization.scoreLine` / `Localization.engineText` 翻译，日文也一并补上。`Localization.swift` 在容器里用 Swift 6 真实编译并运行过，三种语言的输出都核对了；其余改动只做了类型检查，没有用 Xcode 编译。
- **本轮的成绩数字只用来核对文字，不是正式成绩：** 缓存 / 内存带宽测试的内存读带宽这几次是 50.6–50.7 GB/s（单线程），比当天早些时候的 55–61 GB/s 低，测试前后刚做过编译，原因没有查。

### 2026-10-06 Android / iOS / macOS 前端：CPU 测试和图表

- **结论：** 三个前端的代码都已写好。Android 编译通过；iOS 和 macOS 没有用 Xcode 编译过，只在 Linux 容器里做了类型检查。三个前端都没有在真实设备上运行过。
- **Android（`android/`）：**
  - CPU 页原来是空壳，现在接入了 CPU 算力测试和缓存 / 内存带宽测试；两项都在 App 进程内的原生线程上调用引擎的命令行入口（JNI `nativeStartCli`）。
  - 引擎打印的内容通过替换 `std::cout` / `std::cerr` 的缓冲区收集，界面用 `nativePollOutput` 轮询。
  - 图表用 Compose 绘制：逐处理器柱状图；每类核心和全核各一张带缓存容量标记的双对数曲线图；存储读写条形图；GPU 页"Last run"卡片里的每帧耗时折线图；Charts 页的每项测试条形图。
  - 缓存 / 内存带宽测试在 Android 上写出的版本是 `cpu_membw_v1_android_preview`，iOS 上是 `cpu_membw_v1_ios_preview`（`src/main.cpp`）。
  - **构建：** `.\gradlew.bat :app:assembleDebug --no-configuration-cache`，`BUILD SUCCESSFUL`，四个 ABI，debug APK 约 33 MB。构建时工作区里的版本是 Gradle 9.6.0、AGP 9.4.1（这两个版本号不是本轮修改的）。
  - **没有做：** 没有连接手机或模拟器，一次都没有运行；CPU 测试开始后不能取消。界面文字当时只有英文，同一天稍后补上了中文，见上一条记录。
- **iOS 和 macOS（`ios-gui/`、`macos-gui/`）：**
  - 新增四个两端内容相同的文件：`Models/EngineOutput.swift`（解析引擎输出）、`Models/ChartGeometry.swift`（坐标轴和刻度计算）、`Components/BenchCharts.swift`（用 SwiftUI `Canvas` 绘图）、`Components/ResultCharts.swift`（结果表格和图表）。macOS 的 `project.pbxproj` 已登记这四个文件；iOS 用 XcodeGen 按目录收集，不需要登记。
  - CPU 页增加第四种模式"缓存与内存带宽"（`--cpu-membw`），显示各级缓存表格、曲线图、存储条形图；算力测试增加逐处理器柱状图。进度条和状态文字跟随引擎的 `CPU_PROGRESS` / `CPUMEM_PROGRESS`。
  - GPU 页在摘要下方增加"本次运行的图表"：多个任务的成绩条形图和每个任务的每帧耗时折线图。iOS 的嵌入式预览原来不向界面输出任何内容，桥接层新增 `gpb_poll_output()` 收集它的输出。
  - Charts 页改为读取保存的成绩直接绘制，macOS 这一页不再需要 Python，iOS 不再使用 Swift Charts。macOS 的 Full Analysis 预设仍会调用 `plot_workloads.py` 生成 PNG。
  - **引擎（`src/cpu_membw.cpp`）：** Apple 平台原来不查询空闲内存，"最多使用一半空闲内存"的限制不生效；现在 macOS 用 `host_statistics64`，iOS 用 `os_proc_available_memory`。
  - **Apple 平台不能把线程绑定到核心**，所以单线程扫描跑在系统挑选的核心上，能效核不会单独测量；界面说明里已写明。
  - **算力测试的"单核"模式在 Apple 平台上不是真正的逐核：** 它对每个逻辑处理器各跑一次单线程测试，但每次跑在哪个核心由系统决定（引擎标记为 `scheduler_managed`）。两端的柱状图在这种情况下标题改为"单线程各次运行的成绩"并写明这一点，行标签不再显示处理器编号。按性能核 / 能效核分组测量（用线程 QoS 等级）还没有做，做法和已知的坑见第 9 节。
- **类型检查时发现并修掉的原有问题**（这些代码是 2026-07 在 Windows 上写的，从未编译过）：
  - macOS `BenchEngine.swift`：`needCharts && !(await …)` 在 `&&` 的右侧使用 `await`，无法编译。
  - 两端 `BenchEngine.swift`：`runViaProcess` / `runViaBridge` 是主线程上的方法，却一直阻塞到测试结束，界面会卡死，输出要到结束后才出现；现在改为 `nonisolated`。
  - 两端 `BenchEngine.swift`：把 `UnsafeMutableRawPointer` 从主线程闭包里返回，Swift 6 模式下是错误。
  - macOS：子进程输出按读取块切分，一行可能被切成两段；现在在主线程上拼成整行，并在子进程退出后最多等 2 秒读完剩余输出。
  - macOS `RunView.swift`：`.filter(allow)` 在 Swift 6.0 下报"call can throw"，改成闭包。
  - iOS 桥接文件：本轮第一次打补丁时转义符被写成了真正的换行，类型检查发现后已修正。
- **验证方法和范围：**
  - **解析和坐标计算（真实编译并运行）：** 在 `swift:6.0-jammy` 容器里用 Swift 6.0.3 编译 `EngineOutput.swift` 和 `ChartGeometry.swift`，输入本机引擎的真实输出（缓存 / 内存扫描、算力测试、两个 GPU 任务）。解析出的数值与引擎输出一致：L1 / L2 / L3 为 48 KiB / 1 MiB / 96 MiB，单线程读 587 / 244 / 153 GB/s（这次扫描上限设为 64 MiB，内存一行标为未稳定），16 个逻辑处理器的成绩，两个 GPU 任务各 3 个非预热采样。
  - **界面代码（只做类型检查）：** 同一个容器里以 Swift 6 模式检查两端的 `Models/`、新增的两个 `Components/` 文件、`CpuView`、`ChartsView`、`RunView`、`BenchEngine`，结果 0 个错误、0 个警告。SwiftUI、AppKit、UIKit、QuartzCore 用的是手写的替身模块，Foundation 是 Linux 上的真实实现，C 桥接头文件是真实文件。**这不是用 Apple SDK 编译**，SDK 接口与替身不一致的地方查不出来。
  - **引擎的 Apple 分支（交叉编译到目标文件）：** 用 zig 0.16 自带的 clang 和 macOS 头文件，把 `cpu_membw.cpp`、`cpu_benchmark.cpp`、`cpumem_cli.cpp` 编译到 `aarch64-macos` 和 `x86_64-macos`，都通过，只有 `cpu_benchmark.cpp` 一个原有的未使用成员警告。x86_64 需要在命令行加上 `evex512` 特性，AVX-512 内核才能通过；Android NDK 的 clang 不需要，Apple 的 clang 是否需要没有验证。
  - **Windows 回归：** 重新编译 `gpu_benchmark` 和 `gpu_engine`，短扫描（上限 16 MiB）正常结束。
- **未验证：**
  - 没有用 Xcode 编译过任何一端；没有在 Mac、iPhone、Android 手机或模拟器上运行过。
  - iOS 的 `os_proc_available_memory` 分支和 Objective-C++ 桥接文件没有任何编译器检查过（zig 不带 iOS 头文件）。
  - 两端的 `ContentView`、`HistoryView`、`SettingsView`、`AboutView`、`GlassCard`、`MetalBenchView` 和 App 入口本轮没有改动，也没有做类型检查，里面可能还有同类问题。
  - 图表的实际显示效果（文字是否重叠、深色模式、窄屏）在三个前端上都没有看过。

### 2026-10-06 GUI 里所有测试的图表

- **实现：** `gui/MainWindow.xaml.cpp` 里的 `drawMemBwPlot` 增加线性坐标模式，新增 `drawBarPlot`（横向和纵向条形图），`layoutMemBwCharts` 支持独占一行的图；新增 `renderGpuRunCharts`、`renderCpuCoreChart`、`renderChartsPage`。`src/app_base.cpp` 在 `[GPU Timing]` 行末尾加了 `T:` 字段。
- **验证方法：** `GPU_BENCH_DATA_DIR` 指向临时目录，用 UI Automation 操作并截图；RTX 5090，测试时 GPU 另有约 16–20% 的占用，所以下面的成绩只用来确认图表与文字一致，不作为正式成绩。
- **粒子测试（默认预设，4 个 API）：** 对比条形图四条（Vulkan 1,734、DirectX 12 2,299、DirectX 11 2,199、OpenGL 1,832 GB/s），与汇总文字一致；四张每帧耗时折线图，计算和渲染各一条线。
- **GPU Burn（4 个 API）：** 运行完成并出图，汇总为 130.31 / 142.35 / 102.26 / 117.86 Gpix-step/s。
- **流体测试（仅 Vulkan）：** 只有一个任务，不画对比图，只画一张折线图：计算约 12 ms、渲染约 4.5 ms，第 5 秒抓帧处有一个下凹。
- **CPU 算力测试（单核 + 全核）：** 16 个逻辑处理器各一根柱子，纵轴 0–200 MWork/s。
- **Charts 页：**
  - 浅色英文：粒子测试（4 个 API）、CPU 单核、CPU 全核三张图；
  - 深色中文（换用之前的测试数据）：显存带宽（核显 61.2 对 RTX 5090 1,537 GB/s）、CPU 全核、CPU 缓存与内存带宽三张图。
- **回归：** 绘图函数重构后重跑了一次显存带宽测试，三张对数坐标曲线图显示正常；CPU 缓存/内存带宽的曲线图用的是同一条代码路径，本轮没有重新截图。
- **顺带发现：** 这次 RTX 5090 的链路是 PCIe 5.0 x8（`nvidia-smi`），经 PCIe 读系统内存 24.1 GB/s、写 23.2 GB/s，是 x8 理论值 31.5 GB/s 的 77%；2026-10-05 链路为 x4 时是 13.1 GB/s。
- **未验证：** 鼠标悬停提示、日文界面、多 GPU 同时运行的对比图、其余旧版测试项目（N-Body、SynthPeak 等）的图表、发布包。

### 2026-10-06 `cpu_membw_v1`：大小核、曲线识别缓存、存储读写、Android 独立程序

- **后台负载：** 用户确认后已停掉 `marketing-simplified` 的全部容器并关闭自动重启；本节测试时没有其他容器在运行。
- **从曲线识别各级缓存（`--cpu-membw-curve-levels`，忽略系统报告的容量）：**
  - 9800X3D 单线程：L1 容量在 48–64 KiB 之间（实际 48 KiB），L2 在 384 KiB–1.5 MiB 之间（实际 1 MiB），L3 在 64–256 MiB 之间（实际 96 MiB）；
  - 各级读带宽 594 / 245 / 138 GB/s，内存 61.5 GB/s，与按系统容量分层的结果一致；
  - 全核曲线也识别出三级：每线程 L1 48–64 KiB、L2 384 KiB–2 MiB、L3 4–32 MiB（实际每线程份额 12 MiB）。
- **大小核路径（`--cpu-membw-debug-classes 2`）加隐藏最后一级缓存（`--cpu-membw-debug-hide-llc`）：**
  - 8 个核心被当成两类，各自做了单线程扫描（参考核心为逻辑处理器 2 和 10），全核只报告内存；
  - 两类核心都从曲线找回了被隐藏的 L3：容量在 48/64–256 MiB 之间，读 138 GB/s，内存 59–62 GB/s；
  - 第一版尾部判定过严，第二类核心没有识别出内存平台，放宽为"末尾连续三个以上、相差 25% 以内"后两类都正常。
- **存储测试（Windows，不经过系统缓存，一次一个请求）：**

  | 硬盘 | 顺序读 / 写（MB/s） | 随机 4 KiB 读 / 写（MB/s） |
  |---|---|---|
  | C:，ZHITAI TiPro9000（PCIe 5.0，页面文件所在盘） | 5323–5889 / 4850–9707，另有两次约 1450–1670 | 70.6–75.6 / 210–222 |
  | D:，ZHITAI TiPlus7100 | 4236–4286 / 4715–4838 | 65.8–66.8 / 205–207 |
  | F:，WD SN640 | 552–553 / 757–760 | 34.8–35.5 / 114–132 |

  - **测法改过两次：** 最初写完立刻读，顺序读在 321–2694 MB/s 之间乱跳；改为先写好文件、停 1 秒、再按"整遍取最快"计时后稳定在 5.3–5.9 GB/s。
  - **顺序写仍然不稳定**，原因没有查清。
  - **没有外部对照：** `winsat disk` 需要管理员权限，机器上没有 CrystalDiskMark。
- **Linux（Ubuntu 20.04 容器，GCC 9）：**
  - 整个工程和独立程序编译通过，`cpu_membw.cpp` 和 `cpumem_cli.cpp` 0 个警告；独立程序的 aarch64 交叉编译也是 0 个警告；
  - 存储测试用 `O_DIRECT` 跑通：顺序 3071 / 3664 MB/s，随机 4 KiB 27.4 / 36.2 MB/s（WSL2 虚拟磁盘），换页目标识别为 `swap on /dev/sdb`；
  - 目录在 tmpfs（`/dev/shm`）时按设计跳过并说明原因；测试结束后没有残留文件；
  - 独立程序的 `--curve-levels` 结果与 Windows 一致（L1 48–64 KiB，L2 768 KiB–3 MiB）。
- **Android（NDK 28.2，只编译）：** `scripts\build-android-cpumem.ps1 -Abi arm64-v8a,armeabi-v7a,x86_64,x86` 四个 ABI 都编译通过，没有警告。没有连接手机，**没有运行**。
- **GUI：**
  - 正常运行：表格下方新增存储一行（`page file on C:`，顺序读 5,833 MB/s、写 4,850 MB/s，随机读 74.7 MB/s、写 221.4 MB/s）；
  - 用环境变量 `GPU_BENCH_CPUMEM_EXTRA_ARGS` 传入诊断开关后：表格按核心类别分成两组，从曲线识别的 L3 显示为 `L3  64 MiB–256 MiB *` 并带脚注；曲线图变为每类核心一张加全核一张。
- **未验证：** 真实的手机和大小核 CPU；iOS / macOS 编译；Android App 和 iOS App 的界面入口；中文界面下新增的几行文字。

### 2026-10-06 CPU 缓存 / 内存带宽测试 `cpu_membw_v1`

- **机器：** Ryzen 7 9800X3D（8 核 16 线程，L1d 48 KiB、L2 1 MiB、L3 96 MiB），2×48 GB G.Skill DDR5-6400（`Win32_PhysicalMemory` 的 `ConfiguredClockSpeed` 为 6400），Windows 11。测试期间后台 CPU 占用约 6–10%。
- **构建：** `cmake --build build --config Release --target gpu_benchmark gpu_engine`，新文件没有警告；GUI 用第 10 节上一条记录里的 MSBuild 命令，0 error。
- **命令行（`--cpu-membw --cpu-membw-no-save`，约 24 s，共 4 次；`CLZERO` 一行来自后 3 次）：**

  | 层级 | 单线程 读 / 写 / 复制（GB/s） | 8 线程 读 / 写 / 复制（GB/s） |
  |---|---|---|
  | L1（48 KiB） | 584–591 / 325–330 / 637–651 | 4243–4525 / 2290–2482 / 4447–4798 |
  | L2（1 MiB） | 239–241 / 244–258 / 239–241 | 1689–1846 / 1720–1891 / 1746–1832 |
  | L3（96 MiB） | 135–137 / 134–136 / 129–134 | 756–856 / 829–903 / 797–855 |
  | 内存 | 54.6–58.6 / 24.6–25.8 / 38.8–42.2 | 54.0–55.3 / 26.5–27.0 / 40.0–41.3 |
  | 内存，流式写入 | – / 28.5–29.6 / 51.6–53.2 | – / 30.2–30.5 / 54.3–54.5 |
  | 内存，`CLZERO` 清零 | – / 75.8–78.6 / – | – / 74.1–74.8 / – |

- **与理论值对比：**
  - 内存：双通道 DDR5-6400 为 2 × 6400 MT/s × 8 B = 102.4 GB/s；全核读 54–55 GB/s 为 54%，`CLZERO` 写 74–75 GB/s 为 73%。
  - L1 写：按每周期 1 条 64 字节存储指令算，329 GB/s 对应 5.15 GHz，与这颗 CPU 的最高频率相符。
  - L1 读：按 5.15 GHz、每周期 2 条 64 字节加载指令算，上限约 659 GB/s，实测 588 GB/s 为 89%。独立小程序里把循环展开加倍为 585 GB/s，去掉加法为 603 GB/s，说明已接近这颗 CPU 能达到的速度。
- **三种向量宽度（`--cpu-membw-vector`）：** L1 读分别为 588（AVX-512）、297（AVX2）、147（SSE2）GB/s，都是每周期约 1.8 条加载指令；内存级成绩三者相近（全核读 54.7、57.7、58.2 GB/s）。
- **独立小程序交叉验证（临时代码，不在仓库里）：**
  - 内存写入：三种宽度的流式写入、C 运行库 `memset`、`rep stosb` 在 1 到 16 线程下都是 23–32 GB/s；`CLZERO` 为 70–80 GB/s。
  - 一半线程读、一半线程流式写入时，写保持 28 GB/s，读降到 5–13 GB/s。
  - L2 读：分片大小取 64 KiB、256 KiB、1 MiB 或整块时，64–384 KiB 都是 229–244 GB/s，512–896 KiB 都是 287–308 GB/s。
- **与核显对比：** 核显（`membw`，2026-10-05 测）读系统内存为 58–62 GB/s，CPU 全核读为 54–55 GB/s；两者访问同一组内存条。
- **虚拟内存：** 映射新页面 14.4–15.9 GB/s（每个 4 KiB 页 257–285 ns），另有两次为 9.4 和 10.1 GB/s。
- **Linux（Docker 里的 Ubuntu 20.04，GCC 9，运行在 WSL2 上）：**
  - 本机编译：`cpu_membw.cpp` 0 个警告；`main.cpp` 有 1 个警告（`NameLooksIntegrated` 未使用），是原有代码，不是这次引入的；
  - aarch64 交叉编译：`cpu_membw.cpp` 和 `main.cpp` 通过，目标文件里有 NEON 加载指令，没有运行；
  - 运行两次（上限 256 MiB）：缓存容量从 sysfs 读到，线程绑定为 strict；
    - 单线程：L1 读 585–589 GB/s、L2 236–241 GB/s、L3 135 GB/s，与 Windows 一致；
    - 全核内存：读 54.7 GB/s、流式写入 30.9 GB/s、`CLZERO` 75.3 GB/s，与 Windows 一致；
    - 全核缓存级比 Windows 低约一成（L1 读 3845 GB/s），180 个点里有 16 个被重测，推测与虚拟机和同时运行的其他容器有关；
    - 单线程内存为 67.8 GB/s，但上限 256 MiB 不到 L3 的 4 倍，程序已标为"未稳定"；
    - 映射新页面 24.2 GB/s，透明大页为 `always`，程序已注明；
    - 第一次运行时同一台机器上还在跑 GUI 测试，那次的全核缓存级成绩（L1 读 2837 GB/s）不采用。
  - 128 位路径可运行；`--cpu-membw-vector 1024` 被拒绝并返回 2；成绩保存为 `cpu_membw_v1_preview`。
- **后台负载：** 测试期间机器上一直有十几个其他项目的 Docker 容器在运行，后台 CPU 占用约 6–10%，上面的成绩都是在这种状态下测的。
- **GUI（UI Automation，`GPU_BENCH_DATA_DIR` 指向临时目录）：**
  - 选中"缓存与内存带宽"后，时长预设、秒数、预热三个控件禁用，说明文字和结果标题切换；
  - 有效运行 2 次，都到 `Done` / 100%，表格为 L1、L2、L3、内存、流式写入、`CLZERO` 清零六行加一行虚拟内存，数字与命令行一致（例如 L1 592.0 / 331.0 / 655.5，内存读 61.3 和 55.2）；
  - 曲线图：单线程和全核两张，虚线标出 L1、L2、L3（全核图为 L3 ÷ 8）；截图检查了深色英文和浅色中文；
  - History 的 CPU 分类：`CPU — Cache & memory bandwidth  55.2 GB/s`；
  - 回归：切回"全核"模式后原有 CPU 测试正常（2368.6 MWork/s，16 线程）；GPU 的 `membw` 曲线图在重构布局代码后显示正常。
- **顺带改动的图表规则（GPU 和 CPU 的曲线图共用）：**
  - 线尾标注由逐条判断改为全标或全不标，避免一张图里只有一条线带名字；
  - 横轴刻度在扫描范围很长时由 4 倍一格改为 16 倍一格，避免标签重叠（GPU 的三张图范围较短，刻度不变）；
  - 排版代码抽成公共函数 `layoutMemBwCharts`，并支持在图上画虚线参考标记。
- **作废的一遍：** 第一遍 GUI 测试与 Linux 容器编译同时进行，L1 读只有 361 GB/s、L2 只有 121 GB/s。容器结束后重测恢复正常。
- **未验证：** 鼠标悬停提示、日文界面、macOS、Windows ARM64、真实 ARM 设备、带 L4 或大小核的 CPU、发布包。

### 2026-10-05 `membw_v1` 接入 WinUI

- **构建：**
  - 引擎：`cmake --build build --config Release --target gpu_benchmark gpu_engine`。
  - GUI：`MSBuild gui\gpu_bench_gui.vcxproj /p:Configuration=Release /p:Platform=x64 /p:GpuBuildDir=<repo>\build /p:VcpkgLib=C:\vcpkg\installed\x64-windows\lib /p:VcpkgBin=C:\vcpkg\installed\x64-windows\bin`。`build` 目录用的是经典模式的 vcpkg，不带 `VcpkgLib` 时链接会报找不到 `glfw3dll.lib`。
  - 构建后 `gui\x64\Release` 里的 `gpu_benchmark.exe` 和四个 `membw_*.spv` 已更新。`out\build\windows-x64-release` 预设目录和发布包没有重新构建。
- **验证方法：** 把 `GPU_BENCH_DATA_DIR` 指向临时目录后启动 GUI，用 Windows UI Automation 选中项目、点击运行、读取控件文本并截图。没有写入用户 History。
- **控件状态：** 选中新项目后，DurationUnitBox、DurationValueBox、HeadlessBox、VsyncBox、HostMemBox、RenderDocBox、CaptureBox 均为禁用，API 选择器显示 `Graphics API: Vulkan`。
- **RTX 5090（GPU 选自动）：** 运行到 `Done` / 100%，结果区为：
  - `VRAM bandwidth: read 1467 GB/s · write 1295 GB/s · copy 1251 GB/s`
  - `Cache plateau: read 6319 GB/s (3 MiB – 128 MiB)`
  - `L1 cache: 55156 GB/s all SMs/CUs busy · 280.5 GB/s per SM/CU · capacity 128 KiB – 192 KiB`
  - `PCIe to system memory: read 13.0 GB/s · write 12.9 GB/s · driver copy 11.0 / 11.0 GB/s`
- **AMD 核显（在 GPU 下拉框里选中）：** 结果区为 `System memory bandwidth: read 61.2 GB/s · write 58.3 GB/s · copy 42.2 GB/s` 和 `L1 cache: 230.7 GB/s all SMs/CUs busy · 169.6 GB/s per SM/CU · capacity 16 KiB – 24 KiB`，没有 PCIe 行。
- **History：** 行内容为 `Memory Bandwidth — VRAM / Cache / PCIe | Compute | - | 1466.6 GB/s | -`，未勾选"显示 headless"时也可见。
- **修正过的问题：** GPU 选自动时结果标题原先只显示 `GPU`，现在从 `MEMBW_DEVICE` 行取设备名；History 的帧率列原先显示 `0`，现在显示 `-`。
- **GUI 与命令行的差异：** GUI 打开时两次运行的显存读带宽为 1462 和 1467 GB/s，同一天纯命令行为 1528–1542 GB/s，低约 5%。
- **未验证：** 日文界面的实际显示、"全部 GPU"与 Full Analysis 预设、Charts 页、发布包。

### 2026-10-05 `membw_v1` GUI 曲线图

- **实现：** `gui/MainWindow.xaml` 的结果卡片里新增 `MemBwChartHost`；`gui/MainWindow.xaml.cpp` 新增 `parseMemBwPoints`、`drawMemBwPlot` 和 `MainWindow::renderMemBwCharts`，数据来自 worker 输出的 `MEMBW_POINT` 行。
- **配色校验：** 用 dataviz 技能的 `validate_palette.js` 校验蓝、橙、青三色。浅色（`#2a78d6`、`#eb6834`、`#1baf7a`）和深色（`#3987e5`、`#d95926`、`#199e70`）都通过；浅色下青色对背景的对比度为 2.74:1，低于 3:1，按规则由图例和线尾标注补足。最初给 L1 图选的蓝配紫在深色下校验失败，已改用蓝配橙。
- **截图检查（RTX 5090）：**
  - 浅色英文：三张图按两列排布，标题、图例、坐标刻度（1 MiB 到 1 GiB，16 KiB 到 1 MiB）和线尾圆点正常；
  - 深色中文：切换主题和语言后图表自动重画，标题、图例、提示文字均为中文，汇总四行也正确本地化。
- **修正过的问题：** PCIe 图中读和写的线尾重合，写的圆点盖住读的圆点，而旁边标着"Read"。现在线尾相距不到 16 px 的曲线都不在线尾标名字，只用图例区分。
- **未验证：** 鼠标悬停提示、日文界面、窄窗口下的单列布局、"全部 GPU"预设下只画最后一个 GPU 的行为。

### 2026-10-05 `membw_v1` Vulkan 命令行版

- **构建：**
  - Windows：`cmake --build build --config Release --target gpu_benchmark gpu_engine`，MSVC `/W4` 下新代码没有警告。
  - Linux：`ubuntu:20.04` 容器，GCC 9.4 加 `-Wall -Wextra -Wpedantic`，新模块没有警告；`aarch64-linux-gnu-g++` 9.4 能编译 `vulkan_membw.cpp` 和 `main.cpp`。
- **命令：** `gpu_benchmark --workload membw --gpu <n>`。测试时把 `GPU_BENCH_DATA_DIR` 指向临时目录，没有写入用户 History。
- **RTX 5090**（驱动 616.92，开跑前 GPU 占用 6%）：
  - 显存读：连续运行为 1536 / 1542 / 1536 / 1528 / 1537 GB/s，是理论值 1792 的 86%；
  - 显存写 1352–1364 GB/s，复制 1342–1354 GB/s（复制按读写两个方向计字节）；
  - 只读 L2 平台 6608–6724 GB/s，范围 3 MiB 到 96 或 128 MiB；192 MiB 为 2763 GB/s，512 MiB 及以上稳定在约 1540 GB/s；
  - 1 MiB 为约 19 TB/s，属于 L1 参与的结果，程序输出了对应提示；
  - 一次完整扫描约 4.9 秒；加上 L1 测试后约 6.6–6.9 秒；再加上系统内存测试后约 23 秒（x4 链路）。
- **RTX 5090 的 L1 测试：**
  - 单工作组（`l1_single`，三次运行）：16–96 KiB 为 279–289 GB/s，128 KiB 为 244–249 GB/s，192 KiB 为 88–93 GB/s，256 KiB 以上为 78–84 GB/s；三次都判定 L1 容量在 128–192 KiB 之间；
  - 满载（`l1`，三次运行）：16 KiB 为 55.1–55.4 TB/s，24 KiB 为 54.3–54.4 TB/s，32 KiB 为 26.7–28.6 TB/s，48 KiB 为 7.5–10.5 TB/s，64–96 KiB 为 6.1–6.5 TB/s，384 KiB 以上为约 1.52–1.59 TB/s；
  - 满载折合每个 SM 约 326 GB/s（按 170 个 SM 计），即每周期约 128 字节；
  - L1 内核改过两次。第一版用嵌套循环，小数据块时每一遍的循环开销偏大，曲线先升后降，程序误报容量为 12–16 KiB；改成单层循环并围绕最高点找平台后曲线变平。
  - 第二个问题是单工作组结果不稳定：未做处理时，三次运行的速率在 288、410、540 GB/s 之间跳，容量拐点落在 512 KiB 到 1 MiB，说明驱动把一个工作组的 warp 分散到了多个 SM。加入共享变量和 `barrier()` 后，才得到上面三次一致的结果。
- **RTX 5090 的系统内存测试**（链路为 PCIe 5.0 x4，单方向理论值 15.75 GB/s）：
  - `host_read`：1 MiB 为 450 GB/s，2 MiB 为 137 GB/s，4 MiB 为 174 GB/s，8 MiB 为 62 GB/s，16 MiB 为 27 GB/s，32 MiB 为 14.9 GB/s，64 MiB 到 2048 MiB 为 12.3–13.1 GB/s；
  - `host_write`：1 MiB 到 2048 MiB 为 12.0–13.1 GB/s；
  - `vkCmdCopyBuffer`（每次 2048 MiB）：系统内存到显存 12.0 GB/s，显存到系统内存 12.0 GB/s；
  - 链路查询：`nvidia-smi --query-gpu=pcie.link.gen.current,pcie.link.width.current` 返回 `5, 4`，测试前后一致；`DEVPKEY_PciDevice_CurrentLinkWidth` 为 4、`MaxLinkWidth` 为 16。主板是 ROG CROSSHAIR X670E HERO。
- **作废的一遍：** 当天第一遍测试时，Edge 占用约 41–68% 的 GPU（210–240 W），显存读只有约 1200–1280 GB/s，中位数与最快值相差 20% 以上。这一遍只用来发现并修正校准和平台识别的问题，数据不采用。
- **AMD Radeon(TM) Graphics**（9800X3D 核显）：
  - 数据量扫描：读 58.0–61.5 GB/s，写 54.3–58.4 GB/s，4 MiB 以上基本是平线，没有明显的缓存平台；1 MiB 为 168–181 GB/s。复制在 16 MiB 以内约 64–69 GB/s，24 MiB 以上约 40 GB/s，原因未查。
  - L1 单工作组：16 KiB 为 168 GB/s，24–64 KiB 为 127 GB/s，96–256 KiB 为 88–89 GB/s，384 KiB 以上为 37.7 GB/s。程序报告 L1 容量在 16–24 KiB 之间；后两次下降对应哪一级缓存没有确认。
  - L1 满载：16–32 KiB 为 216–276 GB/s，48 KiB 以上回到约 60 GB/s。加入共享变量后核显的两条 L1 曲线没有变化。
  - 系统内存测试：按设计跳过（集成显卡的全部内存都是系统内存）。
- **lavapipe**（容器内，`--membw-max-mib 128`）：读约 8–9 GB/s，退出码 0，结果记为 `membw_v1_preview`、`isSoftware=true`、`Unified-memory`。加入 L1 测试后重新构建并运行通过；软件渲染器上 L1 曲线是平的（满载约 10 GB/s，单工作组约 1 GB/s），没有缓存层级可言。加入系统内存测试后再次构建并运行通过（GCC 9.4 无新警告，aarch64 交叉编译通过），该测试在软件设备上按设计跳过。
- **保存与错误路径：**
  - 默认扫描保存为 `membw_v1`，`--membw-max-mib 64` 保存为 `membw_v1_preview`；
  - `--backend dx12` 退出码为 2；指定没有 Vulkan 的 GPU 时退出码为 1。
- **未验证：** GUI 对 `membw` 结果的显示、Intel / 摩尔线程 / Adreno / Jetson 设备、墙钟计时回退路径（没有设备触发过）、与 RX 6900 XT 和 vkpeak 的外部对照。

### 2026-09-30 `stream_v1` 字节数审计（Vulkan，RTX 5090 + AMD 核显）

- **方法：**
  - 使用 `build/Release` 的隔离副本，并把 `GPU_BENCH_DATA_DIR` 指向临时目录，没有写入用户 History。
  - 把 `compute.comp.spv` 依次替换为 `full`（读 32 + 写 32）、`read`（读 32）、`write`（写 32）三个对照 kernel；这些 kernel 未入库。
  - 运行 `gpu_benchmark --time 8 --backend vulkan --gpu <n> --particles <N> --no-renderdoc --headless`，取 Avg compute。
  - 第一轮测量时用户在用 GPU 做视频补帧，该轮数据作废，以下为复测结果。
- **RTX 5090**（驱动 616.92）：
  - 原 kernel / `full`：16M 为 0.793 / 0.796 ms，32M 为 1.590 / 1.614 ms，64M 为 3.189 / 3.186 ms；
  - `read`：16M 为 0.357 ms，64M 为 1.429 ms；
  - 窗口模式 16M：原 kernel / `full` 为 0.798 / 0.788 ms；
  - 1M：0.021 ms，数据驻留在 L2 中。
- **AMD Radeon(TM) Graphics（16M）：**
  - 原 kernel 21.626 ms，`full` 15.148 ms，`read` 8.801 ms，`write` 8.506 ms；
  - 用 `glslc -O` 重编原 kernel 后为 21.994 ms，差距不是 SPIR-V 优化级别造成的。
- **结论与边界：** 见 §3.6。其他厂商（Intel、摩尔线程、Adreno）没有测过，不下结论。

### 2026-09-30 Linux / Jetson 构建与 Mesa 软件渲染（Docker 容器）

- **环境：** `ubuntu:20.04`，与 JetPack 5 的用户态一致：GCC 9.4、glibc 2.31、`libvulkan-dev` 1.2.131、GLFW 3.3.2。系统自带的 CMake 3.16 低于项目要求的 3.22，改用 pip 安装的 CMake 3.31.10。本机 Docker 没有 arm64 模拟，因此另用 `aarch64-linux-gnu-g++` 9.4 做交叉编译。
- **修复前：** `app_base.cpp` 缺少 `<cstring>`，导致编译失败；链接时报 `dlsym ... DSO missing from command line`。
- **修复后：**
  - x86_64 完成构建和链接，`--help` 正常；
  - 9 个引擎源文件均能用 aarch64 编译器编成目标文件；
  - `--cpu-benchmark all` 的 `strict_sched_affinity` 结果为 `valid=1`（容器内 16 个逻辑核）。
- **Mesa 21.2.6 + Xvfb 软件渲染：**
  - Particle 1M：lavapipe 为 46.49 GB/s，llvmpipe 为 10.44 GB/s；
  - GPU Burn 64 步：lavapipe 为 0.48 Gpix-step/s，**llvmpipe 为 19.89（GL timer 虚高，不可用）**；
  - Cinematic Liquid（lavapipe）：1.47 MParticle-step/s，约 2.2 s/帧；
  - 以上运行全部 exit 0。修复后结果记为 `isSoftware=true`、`bottleneck=Software`，摘要显示为 `CPU Renderer:`。
- **未宣称：** Jetson 实机运行、`.deb` 打包、Ubuntu 20.04 上的 `glslc`（该版本没有 glslc 软件包，SPIR-V 需从 Windows 构建中复制）。
### 2026-07-31 暂停点：macOS x86_64 交叉构建、DMG 脚本与 iOS 首次 Xcode build

- macOS x86_64：`scripts/build-macos.sh` 的实际交叉构建完整通过，产物为 `out/macos/x86_64/Release/GPUBenchmark.app`；GUI/helper 均只含 x86_64、`minos 12.0`、ad-hoc deep codesign 通过，`otool -L` 无非系统依赖。主机是 arm64，所以没有把它记为 Intel runtime pass。
- macOS DMG：`scripts/package-macos-dmg.sh` 与 `packaging/macos/README.txt.in` 已存在，`bash -n scripts/build-macos.sh` 和 `bash -n scripts/package-macos-dmg.sh` 通过。**未执行实际 `hdiutil create/attach` 全链，因此无 DMG/hash 可登记。**
- iOS configure 命令：
  `cmake -S . -B /private/tmp/mangekyo-ios-audit.VHIRua -GXcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO`
  结果为 **PASS**；Metal ON、其它 GPU API OFF、NO_GLFW、`ios_engine_host` ON、target 为 `arm64-apple-ios16.0-simulator`。
- iOS 首次 build：
  `cmake --build /private/tmp/mangekyo-ios-audit.VHIRua --config Release --target gpu_engine -- -sdk iphonesimulator CODE_SIGNING_ALLOWED=NO`
  结果为 **FAIL**；首要错误是 `src/main.cpp` 5 处 `std::system` 在 iOS SDK unavailable。`CMakeLists.txt` 已改为 iOS archive 排除 desktop `main.cpp`，但本会话为保存 handoff 而暂停，**修改后的 build 尚未重跑**。
- iOS Swift：`BenchResult.swift` 和新增 `MobileBenchmark.swift` 只有源码 diff，尚无 `xcodebuild` 证据；不得因 model 存在就写 GUI 已支持三主项。
- 环境限制：Xcode 26.6；有 iOS 18.5 的 iPhone/iPad 与 iOS 26.5 iPhone simulator；0 个有效 codesigning identity，无连接的 iPhone/iPad。generic-device 无签名编译仍可做，但真机安装/性能/capture/TestFlight 不能在当前权限与硬件状态下完成。

### 2026-07-31 macOS M4 Pro 自包含 App、三主项与原生抓帧

- 环境：Apple M4 Pro，macOS 26.5.2，Xcode 26.6；源码目标仍为 **macOS 12.0**。
- `./scripts/build-macos.sh` 成功生成 `out/macos/arm64/Release/GPUBenchmark.app`。GUI/helper 均为 arm64、`LC_BUILD_VERSION minos 12.0`；`otool -L` 只有系统 framework/lib，Charts 为 weak framework，无 `/opt/homebrew` / `/usr/local` / Vulkan dylib；`codesign --verify --deep --strict` 通过。App 脱离仓库 `open -n` 启动并可正常退出。
- 默认 CMake fresh configure/build/package 通过；非 Windows CPack 只启用 ZIP，产出 `Mangekyo-0.2.6-Darwin-arm64.zip` + SHA256。另以 `-Werror=unguarded-availability-new` 完成严格 macOS 12 build。GLFW 3.4 从 pinned URL/SHA 源码静态构建；没有使用本机 minos 26 的 Homebrew bottle。
- 包内 helper：`stream --particles 1048576 --time 15 --capture 5` 成功，结果 `memory=Unified-memory`、63.987 GB/s，并写出真实 `.gputrace`（captureCount=1；抓帧窗口排除计分）。
- 包内 helper：`gpu_burn --iter 16 --time 15 --no-renderdoc` 成功，render 7.092 ms、4.16 Gpix-step/s、stable CV 0.14%、early→late drop 0.1%。
- 包内 helper：`cinematic_liquid --time 6 --no-renderdoc` 成功，正确报告 320,920 particles、128x64x96、10 substeps；compute 7.994 ms、render 14.263 ms。该结果仍属于 `_metal_preview`，不与 Vulkan v8 正式榜混排。
- CPU `--cpu-benchmark all --cpu-time 0.1 --cpu-warmup 0.05 --cpu-no-save`、Metal headless stream/N-body/SynthPeak、windowed Render3D/Volumetric 也完成 smoke。`gpu_stress`、legacy fluid/v1 liquid 在 Metal 上继续显式 unsupported，不做静默 fallback。
- 未完成/不得夸大：没有在实际 Monterey 系统运行；没有 Developer ID/notarization/universal binary；Metal Liquid 尚非正式跨 API 合同。

### 2026-07-21 Windows 开发默认 CLI+GUI 构建与同目录 shader

- 根因：开发树 GUI 的 `findEngineExe()` 优先启动 `gui/x64/Release/gpu_benchmark.exe`，但旧 `CopyGpuBenchmarkWorker` 只拷 EXE、不拷着色器；worker 按 exe 目录解析 `compute.hlsl` / `*.spv` / `*_gl.*`，DX11/DX12/OpenGL 报 `Failed to open file: ...\gui\x64\Release\compute.hlsl`。
- 修复：`gui/gpu_bench_gui.vcxproj` 的 `CopyGpuBenchmarkWorker` 现拷贝 worker + `*.hlsl` / `*.spv` / `*.cso` / `*_gl.*`；新增 `scripts/build-windows.ps1` 默认 `cmake --preset windows-*-release` 再 MSBuild GUI。
- 验证：对本机已有 `out/build/windows-x64-release/Release`，单独跑 `/t:CopyGpuBenchmarkWorker` 后删除的 `compute.hlsl` 被恢复；`compute.comp.spv` / `compute_gl.comp` 存在。文档已更新：`README.md`、`docs/building.md`、`gui/README.md`、`packaging/README.md`、`packaging/PACKAGE_LIMITATIONS.md`、`installer/README.md`、`docs/renderdoc-capture-guide.md`、`docs/report.md`、`docs/TODO.md`、`macos-gui/README.md`、本文件。
- 未宣称：双 D700 Vulkan `vkCreateDevice failed`、干净机发布包、或正式 github-release 重跑。

### 2026-07-19 macOS 最低系统 12 Monterey（代码侧，无真机）

- 用户要求产品至少能在 **macOS 12** 运行；已将 CMake + SwiftUI deployment 从 14.0 降至 **12.0**，并补 `NavigationView` / `defaultSize` 回退（详见 §3.0.2）。
- 本机为 Windows，**未**在 Mac/Monterey 上编译或运行；验收仍开放。同步文件：`HANDOFF.md`、`docs/macos-notes.md`、`macos-gui/README.md`、根 `README.md`。

### 2026-07-19 iOS：iOS 16 底线 + Liquid Glass 合同（规格）；2026-07-24 源码切片

- 用户锁定：**最低 iOS 16**；**iOS 26/27 必须新版 Liquid Glass**（16–25 Material 回退）。
- **2026-07-19 当时事实**：规格写入、无工程。
- **2026-07-24 事实**：`ios-gui/` + `ios_engine_host` + `_ios_preview` 后缀代码已在仓库；**Windows 本机不能也不曾完成 iOS 编译/运行**；不得写成「可安装发布」或「已实机跑通」。
- 完整规格见 **§3.0.3**；`GlassCard` 已预留 `iOS 26.0`。

### 2026-07-18 WinUI WARP 命名与 Full Analysis 跳过状态

- `gui/MainWindow.xaml.cpp/.h`：probe 设备名新增与 GPU index 对齐的快照；软件设备显示统一为真实 probe 名称 + 当前 CPU 型号，worker 回传 `Microsoft Basic Render Driver`/`Microsoft WARP (CPU Software Renderer)` 时不会再在 Summary fallback 为 `GPU <index>`。
- Full Analysis one/all GPU 的任务计划会过滤 probe 明确不支持的 API；非 fragment workload 的 DX11 还检查 `dx11Compute`。跳过明细进入 Summary hint 与 Raw CLI output；最终状态分别报告 completed / failed / unsupported skipped。Custom、Flights、Particle 等路径仍保留显式启动并由 CLI 返回真实能力错误的既有诊断语义。
- `git diff --check`：通过。
- VS 18/v145 `MSBuild gui\gpu_bench_gui.vcxproj /p:Configuration=Release /p:Platform=x64 /p:GpuBuildDir=... /m:1`：成功，生成 `gui/x64/Release/gpu_bench_gui.exe`；仅有既有 MSB3774/MSB8027/C4996/LNK4042 warning。首次短超时留下的 MSBuild 占用同一 PDB，结束该遗留进程后在沙箱外单线程重跑成功；这不是源码编译失败。
- 本轮没有启动 GUI、Full Analysis、GPU workload 或 RenderDoc；WARP 文案视觉和实际 `9 completed; 3 unsupported combinations skipped` 矩阵仍待交互验收。

### 2026-07-18 Windows OpenGL 指定 GPU 错误说明

- `gui/MainWindow.xaml.cpp/.h`：worker 失败后在 backend 为 `opengl` 时识别两条 CLI 精确路径：(a) `OpenGL on Windows cannot select GPU index` + `active GL_RENDERER`；(b) 更早的 capability gate `does not report support for backend 'opengl'`。路径 (b) 还必须从 probe 快照确认**另一张** GPU 的 OpenGL capability 为 true，才记录“所选 GPU -> 当前 OpenGL renderer”；若没有这项交叉证据，generic unsupported 仍不会被误写成 Windows 设备路由限制。Summary hint 解释 WGL 没有标准的按 DXGI/Vulkan index 切换设备能力，并明确该说明不适用于其他 OpenGL 错误。
- 若本次所有真实失败均为上述 renderer mismatch，进度卡/底部状态直接写 `OpenGL could not use the selected GPU`；若还有其他失败则继续使用普通 failed/error 统计，同时仅为已识别 mismatch 附加说明。失败计数本身不改成 skipped，因为用户明确请求了该 GPU/API pass 而它没有运行成功。
- `git diff --check`：通过。
- VS 18/v145 Release x64 WinUI 单线程构建成功，更新 `gui/x64/Release/gpu_bench_gui.exe`；仅有既有 MSB3774/MSB8027/C4996/LNK4042 warning。
- 本轮未再次运行 15 秒核显 All APIs；实际中英文提示、所选/实际 renderer 名称和不会误分类其他 OpenGL 错误仍待 GUI 交互验收。
- 用户随后提供的 Raw CLI 实图确认实际失败发生在较早的路径 (b)：`GPU index 1 (AMD Radeon(TM) Graphics) does not report support for backend 'opengl'.`，因此首版只识别路径 (a) 时 Summary 没显示说明。补齐路径 (b) 后再次完成 VS 18/v145 Release x64 WinUI 构建，`gui/x64/Release/gpu_bench_gui.exe` 已更新；仍未代替用户重跑完整 15 秒矩阵。
- 用户实图验收路径 (b) 的说明已能显示，但指出它与 L2 small-working-set 提示贴得太近，且 WARP All APIs 的另一个 Vulkan 失败仍只有计数。随后把 ResultHint 改为段落式渲染（各问题/性能提示之间使用空行），并新增严格的常见 worker 错误分类：unsupported GPU/API、API/adapter 初始化失败、缺 Vulkan Runtime、OpenGL 4.3 不足、workload/feature 不支持、安全超时、device lost/driver reset、shader/link/pipeline 失败、swapchain out-of-date、GPU 资源分配失败；未知错误只提示展开 Raw CLI，不猜测原因。OpenGL routing mismatch 仍使用专门说明且不重复生成 generic unsupported。
- 后处理失败也会在 Summary 独立说明“成绩可能有效，但 RenderDoc 转换/图表/报告失败”。分类只基于 exit code 与明确的 CLI/backend 错误标记；不会把所有 OpenGL 或所有 non-zero exit 归成同一原因。VS 18/v145 Release x64 WinUI 再次构建成功，`git diff --check` 通过；仍未运行新的 15 秒矩阵做视觉验收。

### 2026-07-17 WinUI 秒数输入 Compact 样式

- `gui/MainWindow.xaml` 中四个相关 `NumberBox` 的 `SpinButtonPlacementMode` 均从 `Inline` 改为 `Compact`，并更新旧的 Inline/浮层规避注释；`git diff --check` 通过。
- 四控件统一使用 `InvalidInputOverwritten`、Loaded/GotFocus 动画配置与 Enter 处理；GPU/CPU 滚动内容接入外部点击失焦。空值由 `NumberBoxValueChangedEventArgs::OldValue` 同步恢复，无合法旧值时才回退 Minimum；不再保留关闭 XamlRoot 全部 Popup 的实验逻辑。
- `cmake --build build --config Release`：成功，`gpu_engine.lib` 与 `gpu_benchmark.exe` 均生成。
- VS 18/v145 WinUI Release x64 独立输出构建成功：`out/numberbox-ui-verify/gpu_bench_gui.exe` 与新 `MainWindow.xbf` 生成，0 error、7 个 MSB8004/MSB3774/MSB8027/C4996/LNK4042 warning（均为现有工具链/工程 warning）。
- 按用户要求确认目标 EXE 无占用后，以工程默认 `OutDir` 重建成功：`gui/x64/Release/gpu_bench_gui.exe` 与 `MainWindow.xbf` 已更新，0 error、4 个 MSB3774/MSB8027/LNK4042 warning；构建后未启动 GUI。
- 用户明确要求最终版本只改代码并重新编译，因此未启动 GUI、未做自动点击，也未重新 stage/打包；结论仅为代码与构建通过，交互视觉和发布包仍未验收。

### 2026-07-15 审计

- `git status --short`：审计开始时 clean；分支 `main`，HEAD `3237545`。
- 审计开始时的旧 `build/Release/gpu_benchmark.exe --help`：成功，当时二进制暴露 7 个 workload；GraphicsBurn 阶段为 8 个，当前 Plasma Bloom 重建后为 9 个。
- `build/Release/gpu_benchmark.exe --list-gpus`：成功；当前机为 RTX 5090、AMD iGPU、Microsoft Basic Render Driver。审计当时曾把两张硬件 GPU 都标记 OpenGL；现已按真实 GL_RENDERER 匹配修复。
- `results/results.json`：232 条，分布见 3.2；没有 volumetric/fluid 记录。
- `rdoc_captures/*.rdc`：只有 4 个旧的 16M particle 抓帧，没有新 workload 抓帧。
- `cmake --build build --config Release`：失败。原 cache 选中 `C:/Program Files/SVP 4/mpv64/python.exe`，缺 `glad`；改用可用 Python 后继续因缺 `jinja2` 失败。该失败作为“构建不锁定/不可复现”的直接证据保留。
- 未运行任何压力/性能 workload，避免在审计过程中改写结果库或无提示压满 GPU。

### 2026-07-15 Duck family v5 验证（晚间）

- 三个改动 shader（`mls_mpm_grid_update_v2.comp`、`mls_mpm_g2p_v2.comp`、`cinematic_liquid_render_v2.frag`）先经 `glslc --target-env=vulkan1.1` 独立编译通过，再随 `cmake --build build --config Release --target gpu_benchmark` 全量重建通过。
- 预览运行 1（RTX 5090 / Vulkan / `--time 6 --capture 5`，结果组 `cinematic_liquid_v2_duck_family_v5_preview`）：结果 id `20260715-174802-286`，Compute 8.794 ms、Render 1.641 ms、Total 10.435 ms、306.73 MParticle-step/s（预览口径，不可与正式 15 秒混比）；抓帧 0.099 秒排除，1 次尝试保存 1 个。第 5 秒帧为溃坝浪峰，鸭群大多被浪体遮挡：`rdoc_captures/cinematic-liquid-v2-5s-duck-family-v5.png`。
- 预览运行 2（`--time 11 --capture 10`）：结果 id `20260715-174916-139`；第 10 秒平静帧 `rdoc_captures/cinematic-liquid-v2-10s-duck-family-v5.png` 清晰显示 smooth-min 鸭妈妈（圆头/黑眼/白高光/橙嘴/上翘尾，无旧版椭球接缝）和三只小鸭直立漂浮，形态接近用户给出的经典大黄鸭参考图。鸭子造型视觉验收以该帧为准。
- 渲染侧 Render 1.641 ms（旧四刚体基线 1.553 ms）：sphere-trace 鸭子 + 3 只新鸭的开销可接受；Compute 预览均值 8.794 ms 低于旧基线 10.572 ms，主要来自新加入的刚体 bounding-sphere 剔除，但预览与正式 15 秒统计窗口不同，正式对比须等新版本 15 秒运行。
- CLI 摘要中硬编码的 “4 coupled rigid bodies” 已改为使用 `kCinematicLiquidV2BodyCount` 并重建。
- 注意：无参数 `--workload cinematic_liquid --time 6` 在多 GPU 机器上会停在 “Enter GPU index” 交互提示；脚本化运行必须显式 `--gpu 0`。
- WinUI GUI 已用新 `gpu_engine.lib` 重编（Release x64，0 error，仅既有 C4996/LNK4042 warning）；场景介绍文案更新为“子母鸭家族”。启动 smoke 通过：6 秒后 `HasExited=False`、`Responding=True`、标题 `GPU Benchmark`。GUI 按稳定 id `cinematic_liquid` 识别 workload，History/Charts 按结果 `workloadVersion` 分组，因此 `cinematic_liquid_v2_duck_family_v5(_preview)` 自动独立成组，无需 GUI 代码适配。GUI 交互式完整 run/history 实机验收仍是原有开放 P0；`out/stage` 打包目录仍为旧 optics_v4 资产，发布层面需要重新 stage。
- 本轮**未**运行 15 秒正式流程；`cinematic_liquid_v2_duck_family_v5` 只有上述历史预览。随后用户已恢复流体工作并依次升级到 v6/v7，因此 v5 不再是当前工作树合同，但其预览记录永久保留、不得与后续版本混分。

### 2026-07-15 Iterative optics v6 验证（晚间）

- 在用户调整后的 duck family v5 基础上完成 `cinematic_liquid_v2_iterative_optics_v6`（`shaderVersion=8`、`sceneVersion=3`）；鸭子 SDF、七刚体初始化、碰撞与渲染分支均保留。
- v6 当时的表面 resolve 为 5x5x5 binomial filter、`mix=0.90`；fragment shader 最多处理 4 次介质界面，逐段执行 Fresnel/Snell、Beer–Lambert 和 opaque scene depth sorting。v6 metadata 为 `extinction=(30,10,8)`、`toneMap=linear_exposure`、`densityBoundary=zero_clamped`。
- 最终必要短预览：RTX 5090 / Vulkan / `--time 6 --capture 5`，结果 id `20260715-221447-024`，Compute 9.450 ms、Render 3.003 ms、Total 12.454 ms、`257.01 MParticle-step/s`、284 measured frames；capture 0.117 秒排除，1 attempt/1 saved。核验图：`rdoc_captures/cinematic-liquid-v2-5s-iterative-optics-v6-final-preview.png`。
- 这是 `_preview` 结果，**没有运行 v6 的正式 15 秒流程**，不可与 optics_v4 的正式 `263.98 MParticle-step/s` 比分。水体厚区、薄片与边界观感较旧版改善，但仍未达到 jeantimex SPH 的自然动态。
- CLI Release 与 WinUI Release x64 重建通过；WinUI 仍只有 build 结论，实际 workload 选择、完整 run 与 History/Charts 分组待验收。

### 2026-07-15 Physical scene v7 验证（晚间）

- v7 当时的合同为 `cinematic_liquid_v2_physical_scene_v7`、`shaderVersion=9`、`sceneVersion=4`；128x64x96、10 substeps、320,920 粒子（142x14x98 base + 48x37x71 dam）。用户鸭子与七刚体 index ABI 均保留，尤其 boat=2、sink sphere=3、ducklings=4-6。
- shader 编译通过，最终 6 个 SPIR-V 均通过 Vulkan 1.1 校验；CLI Release 构建通过。WinUI Release x64 build 也通过：0 error，仅有既有 warning（最新增量构建 2 个 duplicate WinAppSDK warning，源文件重编时曾包含 C4996、合计 4 个）。最终物理/渲染静态复核均未发现必须修复项；这仍不替代实机视觉验收。
- 只运行了若干 `--time 6` / `--time 8` 自动停止 smoke，没有正式 15 秒流程。没有可靠保存的新 RenderDoc/核验帧，也没有完成船、沉球、水花、越沿、PVC、草地/天空的完整视觉验收；目录中若存在 v7 临时 PNG，也不得在缺少可靠 capture 记录时当作验收证据。
- 一次控制台曾显示 transient `241.13`，但它不是正式 15 秒值，也没有作为可靠持久结果保存，禁止写入排行榜或与 optics_v4/v6 比较。
- 用户反馈窗口约 2-3 秒后自行关闭；本轮使用的自动 smoke 命令包含 `--time 8`，因此自动退出是脚本生命周期，不是目前确认的 crash。正式 Benchmark 应按 15 秒流程验收；未来 Liquid Lab/Explore 才应无限运行至用户主动 Stop。
- 物理/场景代码事实：船为 34 kg 有限质量 + 软系泊，推进反冲可驱动/摇摆；沉球 1.06 水密度比、4.28 秒释放、-9.81 重力、0.015 空气阻尼，水阻按 displaced mass/浸没率门控；有限池壁 inset 0.22 + 有限 outer catch band；自适应 surface 保留低支持喷滴，entry crown/whitewater 来自 GPU body/local fluid，但无 secondary-spray 粒子；软 PVC 薄膜 IOR 1.50/Fresnel/弱吸收/wrinkle；无限程序化草地和无 cubemap 的大气天空/云。以上均是实现事实，不等于轨迹或最终画质已验收。
- render push 的 `pool.z` 已改为真实 ground/pool-floor `y=0`，PVC bottom、liner、草地与物理粒子底面统一；ring separation 由 `2*tube` 推导。该对齐通过源码/构建验证，仍需可靠新 capture 做最终视觉验收。

### 2026-07-16 SPH vertical slice（`cinematic_liquid_sph_slice_v1_preview`，实验性）

按既定判断（"不再盲调 MLS-MPM，建立独立 SPH 版本"）完成了第一个可运行的 SPH 纵切片。**这是实验性 preview 合同：15 秒视觉已验收，但正式计分未完成，不得与任何 MLS-MPM 版本混分。代码现在对所有时长强制 `_preview`。**

实现事实：
- **求解器**：忠实移植 MIT [jeantimex/fluid](https://github.com/jeantimex/fluid) `src/sph/3d/common` 的 Lague 式双密度 SPH——Spiky²/Spiky³ 双密度 + 对称压力/近压（EOS `P=k(ρ-ρ0)`）、Poly6 XSPH 粘性、预测位置（固定 1/120 前瞻）、Unity block-hash(50) 空间哈希 + 计数排序（三级扫描 + atomic scatter）。参数原封照搬：h=0.2、targetDensity=630、pressureMultiplier=288、nearPressure=2.16、viscosity=0.01、collisionDamping=0.95、g=-10，全部在参考单位制（~24x12x18 域）内运行。
- **关键教训（已修复的爆炸）**：参考的 `frameTime=min(dt*timeScale, 1/maxTimestepFPS)` 在 60fps 下把 timeScale=2 钳掉，**每 substep 实际 dt=1/120**；首次用 1/60 直接压力爆炸（全域水雾，见 `rdoc_captures/cinematic-liquid-sph-5s-first.png` 留档）。现固定 dtSim=1/120、2 substeps/帧（固定每帧推进，不随墙钟）。
- **世界映射**：长度 ×(5.12/24=0.21333)，时间 ×τ=sqrt(0.21333*10/9.81)≈0.4663，使 sim 重力 -10 精确落在世界 -9.81，流体与刚体自由落体一致。积分 pass 把世界坐标/速度写回共享 80B 粒子 buffer——surface splat、raymarch、相机、场景、成绩框架**全部原样复用 v7**。
- **刚体耦合**：per-particle SDF 推离 + 速度碰撞 + displaced-volume 浮力（镜像 grid_update 的动量交换），fixed-point 冲量进现有 `bodyImpulses`，rigid integrate pass 原样消费（含 linear.w 浸没门控）。第 10 秒实图验证：鸭妈妈+三小鸭+彩球+船漂浮端正，沉球半沉。
- **播种**：sim 单位确定性晶格 175x8x124 底床 + 30x46x96 左坝柱 = **306,080 粒子**（密度 600/unit³，同参考 spawnDensity），全部落在内嵌池壁内；4 秒重置与 v2 相同（含 SPH 位置/速度 buffer 的 seed 恢复）。
- **内嵌有限池壁（用户指正后补上，随后按用户要求加宽外圈）**：首版直接用整域 AABB 当边界，静水铺得比可见泳池宽；现 SPH integrate 实现内嵌圆角矩形有限池壁（壁顶 sim 4.017 = 世界 0.737，越顶水花自由落草地，外圈域 AABB 仅作 catch band）。2026-07-16 用户要求加宽外圈草地带：**渲染圈/PVC 膜/物理墙 inset 统一从 0.22 → 0.45**（frag `poolDistances` 与 `poolMembraneDimensions` 的系数 2.59→5.29·tube；MPM 路径 `pc.pool[1]`、SPH 墙常量同步）。**注意：这同时改变了 v7 MPM 场景的池几何，v7 此前的 smoke 观感基线随之漂移，属用户批准的场景变更。** 调试过程记录：(a) 坝柱高于壁顶太多导致整体涌浪漫顶、catch band 积成"护城河"→ 柱降至 30 层；(b) splat 核+5x5x5 重建使水面比粒子外扩 ~0.1m，物理墙需再内收 0.10m（half sim 9.328/6.328）才能让重建水面贴圈。最终播种 148x9x98 + 30x30x96 = **216,936 粒子**。最终验证图 `rdoc_captures/cinematic-liquid-sph-10s-wideband-v4.png`（宽外圈草地干净、水贴圈、玩具姿态正常）；中间态 `...-v2/-v3.png` 留档对照。刚体无内墙约束（沿用 v7 语义），个别玩具可被浪冲出池沿属真实行为。
- **深池/浅蓝水/沉底可见（2026-07-16 用户四项调整）**：(a) 草地吸收倒计时上限加长（0.25~0.65 → 0.50~1.80 sim 秒，水洼多留一会儿）；(b) 池壁加高：壁顶系数 0.34 → 0.42（世界 0.737 → 0.938），渲染圈/PVC 膜/g2p/grid_update/SPH 五处同步；(c) 水量 216,936 → **318,464 粒子**（床 148x16x98，静水深 ~0.49 m），0.40 m 沉球完全没顶；(d) `liquidTransmittance` 消光 (30,10,8) → (12,3.6,2.5)，保持红先吸收的水色光谱但整体透亮为浅蓝，水下沉球轮廓可读。**(b)(d) 同样改变共享 MPM 场景；连同 inset 0.45，这正是当前合同必须从历史 v7 升为 v8 的原因。**验证图 `rdoc_captures/cinematic-liquid-sph-10s-deep.png`（更高池壁、侧壁浅蓝、沉球在水下可见）。
- **草地吸收 / 空气墙消除（2026-07-16，用户指出加宽外圈的根本动机是大量溅水时能看到外框空气墙）**：SPH integrate 增加逃逸水回收机制——(a) 域外框 x/z 面从反弹改为"粘滞空气"（只清外向法向速度，不再出现半空撞隐形墙的反弹）；(b) 落在外圈草地的水按每粒子错开的倒计时（当前 **0.50~1.80 sim 秒**，存于 `position.w`，1.0=自由态）短暂积水后"渗入草地"；(c) 压在域边缘的水快速消退（≤0.10s）；(d) 渗完的粒子确定性回收到池内水线以下（hash 定位、零速度），粒子总数不变。验证：`rdoc_captures/cinematic-liquid-sph-5s-absorb.png`（溃坝晃荡期池外水舌边缘为自然圆弧、无垂直水墙）、`...sph-10s-absorb.png`（10 秒草地完全干净、无积水残留）。metadata 为 `grassAbsorb=catch_band_soak_recycle;grassSoakCountdownSimSec=0.50..1.80`。
- **文件**：9 个新 shader `shaders/cinematic_liquid_sph_*.comp`（external/hash_count/scan_block/scan_add/scatter/density/pressure/viscosity/integrate，共享 128B push ABI + 单一 14-binding set）；`src/vulkan_backend.{h,cpp}` 新增 `CreateCinematicLiquidSphResources`/`RecordCinematicLiquidSphFrame`；CLI `--liquid-solver <mpm|sph>`（默认 mpm，v7 路径零改动）；score 公式已按 SPH 的 2 substeps 计（三处修正，虚高 5 倍的首条结果已删除）。
- **验证（RTX 5090 / Vulkan / preview）**：`--time 6 --capture 5` 结果 `20260716-025211-784`，Avg GPU 7.775 ms；`--time 11 --capture 10` 结果 `20260716-025310-587`。第 5 秒帧 `rdoc_captures/cinematic-liquid-sph-5s-dt120.png`：溃坝浪翻卷 + 薄片 + 碎裂水花（明显优于 MLS-MPM 的黏稠感）；第 10 秒帧 `rdoc_captures/cinematic-liquid-sph-10s-dt120.png`：连续平滑水面 + 全部刚体姿态正确。
- **2026-07-16 实机视觉收口**：RTX 5090 / Vulkan / **318,464 粒子**实际启动并完整运行 15 秒；水池、水面、用户鸭子家族、彩球、船、草地/天空稳定显示，进程按 15 秒生命周期正常自动结束。用户接受当前视觉并要求停止外观迭代。此结论只关闭“当前看起来是否可接受”，**不等于**正式 15 秒 + 第 5 秒 RenderDoc 成绩合同通过。

已知边界/开放项（诚实清单）：
- 计数排序 scatter 的 cell 内顺序是原子竞争决定的 → 浮点求和 ULP 级非确定；正式合同前需换确定性排序（metadata 已标 `determinism=cell_order_race_ulp_open`）。
- SPH 目前固定每个**渲染帧**推进 2 个 1/120 子步，不同 GPU/帧率在墙钟第 5 秒会处于不同物理时刻；正式成绩前必须改成与渲染帧率解耦的轨迹合同。
- 两个 SPH 子步之间没有重新清零 `bodyImpulses`，第一子步刚体冲量可能在第二子步重复消费；必须按 substep 清零后重新验证船、鸭子与沉球。
- viscosity pass 原位读取邻居并写同一 velocity SSBO，存在跨 invocation 未同步读写竞争；正式成绩前须改为 ping-pong 或 delta/apply 两阶段。
- 无 secondary spray/foam 粒子（参考的 foam_spawn/foam_update 是下一切片）；whitewater 体积在 SPH 路径恒为零。
- 船的螺旋桨尾流不作用于 SPH 水体（v7 尾流实现在 grid_update，SPH 不跑该 pass）。
- 当前外观已获用户接受，不再优先调水色；secondary spray/foam 与螺旋桨尾流是未来增强，不阻塞本轮视觉收口。
- 正式 15 秒 + 第 5 秒 RenderDoc 成绩、timestamp 边界、确定性/轨迹合同与跨后端一律未完成。

### 2026-07-17 光效可见性根因修复 + 0.1.3 双架构（午间轮）

- **"调了亮度却看不出变化"的根因是 Reinhard 饱和**：`mapped = hdr/(1+hdr)` 在晶核的 HDR 5-10 区间输出被压死在 0.85-0.90，上游增益再乘系数视觉不可辨。修复：色调映射膝点 1.0 → **0.62**（全场提亮，晶核逼近白热，肉眼差异显著），并把光晕衰减带从 smoothstep(0.025,0.45) 加宽到 (0.05,1.05)（光雾半径约翻倍，"光"可见地向背景扩散）。三份 shader 同步；固定循环工作量不变（discard 在计分循环之后）。教训：调亮度先看色调映射曲线所在段。
- History 的 GPU/CPU 浏览器式标签页（TabView）已由并行会话实现在源码中，用户所装旧包未包含——随本轮包发布，无需另改。
- **0.1.3 双架构**：CMake 0.1.2→0.1.3；x64（含 RenderDoc）与 ARM64（-SkipRenderDoc，N/A 语义不变）先后重打，ARM64 首次带上 SPH/池子/GUI 改版/固定负载 GPU Burn/光效全部内容。

### 2026-07-17 晚间：MIT LICENSE + WiX 主路径（本轮交接）

- **License**：用户确认订根目录 `LICENSE` 为 **MIT**（Copyright 2026 Mangekyo contributors）。`GPU_BENCH_PACKAGE_LICENSE_FILE` 默认指向该文件；stage 写入 `licenses/LICENSE`，manifest `projectDistributionLicense=true`。`THIRD_PARTY_NOTICES.md` / `PACKAGE_LIMITATIONS.md` / packaging 文档已改为“项目 MIT + 第三方 notices”，不再写“无项目许可证阻塞 MSI”。
- **WiX 替代 Inno 为主路径**：
  - `cmake/Packaging.cmake`：默认 `GPU_BENCH_CPACK_GENERATORS=ZIP;WIX`；`CPACK_WIX_UI_REF=WixUI_InstallDir`；`CPACK_WIX_ARCHITECTURE` x64/arm64；无扩展名 `LICENSE` 自动 `COPYONLY` 为 `cpack-LICENSE.txt`（否则 CPack 报 `unsupported WiX License file extension ''`）。
  - 新脚本 `scripts/build-wix-installer.ps1`；`stage-windows-release.ps1` 强制带 LICENSE；`build-windows-github-release.ps1` 调用 WiX 并收集 `*.msi`（不再要求 `*-setup.exe`）。
  - Inno（`installer/GpuComputeBenchmark.iss` + 简体中文 `.isl`）保留为 **legacy**。
- **本机冒烟产物**（`out/installer/`，NotSigned）：
  - `Mangekyo-0.1.3-windows-x64.msi` ~103.9 MiB，SHA-256 `db82358f2dfd93542bac2659638d3fa91b45ec4c0d465ef978678c819dd5154e`，Summary Template=`x64;1033`
  - `Mangekyo-0.1.3-windows-arm64.msi` ~102.5 MiB，Template=`Arm64;1033`
  - 对应 stage 的 `gpu_benchmark.exe` / `gpu_bench_gui.exe` PE 分别为 AMD64 / ARM64；`compiledBackends` 两边均为 vulkan/dx12/dx11/opengl=true。
- **测项架构事实**：CPU 与 GPU 测试本体都是按包架构原生编译的两套二进制，不是同一套 x64 测项 + 两个安装壳。
- **互动水池**：仍 Vulkan-only；曾试 DX12 后撤回。未接线 HLSL 草稿若提交须标明为移植草稿，不可宣称 DX12 liquid 可用。
- **仍开放**：Authenticode、frozen report worker、干净机 MSI 安装/升级/卸载/抓帧；**Windows 7 GUI 已后移至 PS3 前**；macOS 真机编译与三主项验收（**含 macOS 12 Monterey 底线冒烟**）；**日语 GUI 已在 0.2.0 完成**，WiX 安装向导日语 UI 仍可选。

### 2026-07-17 GPU Burn v2 固定负载合同（用户拍板）+ 0.1.2

- **用户决策：废弃"目标帧时/自动标定"哲学**（"我不需要目标步数，我就要显卡尽可能生成高帧数"）。GPU Burn 改为 **FurMark 式固定负载**：所有硬件 GPU 每帧执行完全相同的 256 步 x 2 draw（`kGpuBurnV2FixedIter=256`），无标定、无渐升，FPS 本身成为直观对比信号。实测 RTX 5090 Vulkan 恒定 ~345 FPS（2.34 ms/帧），全程无帧数跳变——之前"开头帧数高后面骤降"的现象连根消除（那是标定探针→目标的设计残留）。
- **新成绩身份**：`workloadVersion=gpu_burn_v2_fixed256_kaleidoscope`、`shaderVersion=3`、metadata `loadModel=fixed_per_frame;autoTune=false`。此前标定制的 `gpu_burn_v1_plasma_kaleidoscope_r2` 及更早 `gpu_burn_v1` 成绩组全部独立保留退役。闭环标定代码保留休眠（`gpuBurnAutoTune` 默认 false，无 CLI 开关）。
- **安全边界保留**：软件设备（WARP/Basic Render/llvmpipe）双层钳制到 32 步——CLI 归一化按 `--warp` 钳一次，`AppBase::MainLoop` 起始按真实设备名再兜底一次（覆盖 `--run-all`/GUI 矩阵在逐设备处才知道 WARP 的情形；WARP 16 步已需 ~209 ms/帧，256 步会触发 watchdog）。显式 `--iter` 上限仍 2048。
- **晶核光效**（用户两轮加亮要求）：主增益 0.20→0.26→0.34→**0.42**，光晕合成权重 crystal 0.62→0.88 / base 0.36→0.50，配色紫罗兰系契合背景；三份 shader（frag/hlsl/gl）同步。
- **版本 0.1.2**：CMake project VERSION 0.1.1→0.1.2，x64 包重打（含固定负载合同、光效、此前全部 GUI/安装器/RenderDoc 改动）。
- 待办：15 秒正式流程在新 fixed256 合同下建立基线（每 API 的 FPS/Gpix-step/s 将天然不同——固定负载下这是真实差异而非标定伪差）；README/cli-reference 的 auto-tune 文案清理；GUI 中 gpu_burn 的 `--iter` 传参路径核对。

### 2026-07-17 GUI 改版 / GPU Burn v2 同心圆 / x64 重打（凌晨轮）

- **VM 反馈修复（VMware Win11 全 GPU 完整分析后 RenderDoc 弹错）**：崩溃本体是 RenderDoc 在虚拟 GPU（SVGA3D/WARP）上的上游限制，物理机验收为准；其 Bug Reporter 二次报错根因是官方包 OpenSSL 命名 `libcrypto/libssl-1_1-x64.dll` 与报告器探测的 `-1_1-64.dll` 不匹配——`prepare-renderdoc-portable.ps1` 现自动生成字节级同文件别名（记入 BUNDLE_SOURCE.json 的 localModifications），现有 dependencies/staged 目录已补。
- **GUI 改版（用户逐项要求）**：测试下拉默认只显示 粒子/等离子晶核（词序对调为"等离子晶核 —— GPU Burn"）/流体（去掉"电影化"与 v2 后缀，"流体 —— 互动水池"），其余 8 项藏于新增"显示旧版 / 高级测试"复选框（`ShowLegacyBox` + `applyWorkloadVisibility()`，隐藏时选中项自动回退到粒子）；全部"跑分"文案改"测试"（11 处）；GPU 导航图标由三角形改为自绘显卡 PathIcon；高级选项复选框间距收紧（VsyncBox MinWidth=0、Spacing 20→14）。History 分组标签同步改名（仅显示文案，id/版本不动）。
- **安装目录**：与并行会话的 `{pf}\Mangekyo + PrivilegesRequired=admin` 方案会合（本会话曾先改为 {autopf}+dialog，后被并行会话覆盖并升级为 {pf}+admin+SignedUninstaller，遵其现状；build-inno-installer 静态 invariant 已由并行会话同步）。第一次重打失败即因 invariant 与 .iss 撞改动窗口，重跑通过。
- **GPU Burn v2 背景不是同心圆（用户实图指出）**：`kaleidoscopeBackground` 金环 `sin(radius*3.85)` 的环距 ~1.6 大于可见半径 → 全屏只显一圈；细环权重 0.055 不可见；两者还叠加角向扰动使圆波浪化。修复：环族改纯径向相位、金环频率 3.85→7.6、细环 10.8→15.4/权重 0.14，三份 shader（frag/hlsl/gl.frag）同步。实抓帧验证 5-6 圈完美同心圆 + 中央晶核 + 保留的品红/青波浪臂。**注意：GPU Burn v2 视觉迭代（含本次）未重新标定成绩基线，定稿时须升 shaderVersion 并重跑正式 15 秒。**
- **x64 包**：`Mangekyo-0.1.1-windows-x64` setup（87.7 MiB，SHA256 94a3bbf8…60791874）与 zip（120.0 MiB）已重打，含以上全部 + SPH/池子几何 + Program Files 安装；staged gpu_burn.frag.spv (01:41:55) 晚于同心圆修复 (01:39:27) 已核对。一次编码事故已完整恢复：PowerShell 批量替换曾损坏 MainWindow.xaml.cpp 中文编码，经与 HEAD 零结构差异验证后 git 恢复并用 Python UTF-8 重做；后续中文文本处理一律禁用 PowerShell 管道。

### 2026-07-16 GUI History 打开目录按钮

- History 页新增两个按钮：“打开成绩目录”（`PathService ResultsDirectory()` = `%LOCALAPPDATA%/GpuComputeBenchmark/results`，含 results.json）与“打开抓帧目录”（`CapturesDirectory()` = 同根下 `captures`，含 RenderDoc `.rdc`）。两者共享 PathService 数据根但是不同子目录，因此各给一个入口而不是假装同一处。实现：`gui/MainWindow.xaml`（按钮）、`MainWindow.xaml.h`（handler 声明）、`MainWindow.xaml.cpp`（`ShellExecuteW` 打开 Explorer；PathService 目录创建失败会抛异常，handler 已 try/catch 防止 Click 崩掉整个 GUI；中英文案已本地化）。
- GUI Release x64 重编通过；启动 smoke：6 秒后 `Responding=True`、标题 `GPU Benchmark`。按钮点击的实机交互验收与其余 GUI 验收项一起归入既有 P0，尚未逐一点击验证。

### 2026-07-15 实现与发布 smoke（本轮）

- 核心 Release 构建成功：VS 18/v145，Vulkan、DX12、DX11、OpenGL；GLAD 改为仓库内可复现生成物，不再 configure-time 下载或依赖 Python/Jinja。
- WinUI Release x64 用最新 engine 链接成功：0 error；4 个既有 WinAppSDK 重复对象/C4996 warning。
- RTX 5090 Vulkan `gpu_stress` 3 秒校准 smoke：32 → 1416 iterations/draw，正式窗口 Render 8.826 ms、GPU utilisation 93.5%、`591.40 Gpix-iter/s`。这是校准 smoke，不是正式 15 秒成绩。
- DX12 WARP 3 秒自动标定：32 → 1 iteration/draw，device utilisation 95.6%；证明软件设备路径不会沿用对 dGPU 过重的固定迭代数。
- 低迭代初始化 smoke 已通过 Vulkan、DX12、DX11、OpenGL；DX11/DX12 WARP 亦通过。Metal 未在本机验证，且 GPU Stress v1 目前明确 unsupported。
- 从最终 staged 目录分别运行 Vulkan、DX12、DX11、OpenGL 6 秒，均在约 5.1 秒产生真实 `.rdc`；capture wall time 与 async timestamp samples 从 score 排除。发现并修复过一次“API detected 但 Vulkan capture=0”的误报，根因是 portable `renderdoc.json` 未在首次 Vulkan probe 前被 Loader 发现。
- 最终审查发现 frames-in-flight 排空不足以覆盖 DX11/OpenGL query ring；已改为抓帧后跳过至少 16 个异步样本（DX11 ring=8、OpenGL ring=4，最大 flights 时取 17），核心 Release 再次构建通过；复核确认计数只在成功取得 timing sample 时递减。
- result JSON v2 与 CSV export 通过；新记录包含 `gpu_stress_v1`/`gpu_burn_v1`、最终 steps/iterations/draws/shaderVersion/autoTune、captureAtSec/captureAttempts/captureCount/captureExcluded。历史 v1 结果可读。
- 三份 Plasma Bloom shader 已分别通过 Vulkan `glslc`、OpenGL `glslangValidator` 与 HLSL FXC VS/PS 编译；16-byte ABI、固定循环和防 DCE 数据依赖一致。
- RTX 5090 Vulkan 10 秒自动标定 smoke（`out/gpu-burn-plasma-smoke`）：16 → 1604 steps/draw，Render 14.899 ms、internal utilisation 93.0%、`198.44 Gpix-step/s`、stableScore 197.79/CV 0.97%；外部 NVML 稳定段 8/8 次 99%，约 599–600W。画面核验图为 `out/gpu-burn-plasma-smoke/plasma_bloom_vulkan_verified.png`。
- `--run-all --workload gpu_burn --iter 16 --time 3` 隔离矩阵通过 9/9：RTX 5090 四 API、AMD iGPU 三 API、WARP DX12/DX11；OpenGL 只出现一次，所有 burn 记录 `particleCount=0`。CLI comparison 现按 `gpu_burn_v1 + Gpix-step/s` 分组/score 排名。
- 安全回归：WARP 输入 `--iter 2048` 被限制为 32，正式 render 228.375 ms，未产生数十秒 draw；Windows OpenGL 指定 AMD index 时以 exit 1 拒绝并报告真实 RTX GL_RENDERER，不生成错标结果。
- Cinematic Liquid 新增的 8 份 SPIR-V 均从最新 staged 目录通过 Vulkan 1.1 `spirv-val`；静态复核确认 80B particle、16B grid cell、96B compute/render push constant ABI、P2G/G2P barriers 和 density compute→fragment barrier 一致。CLI 强制 v1 使用真实 device-local storage，GUI 会禁用不适用的 Host memory，避免结果误标。
- v2 早期 RenderDoc 缩略图已检查：`rdoc_captures/cinematic-liquid-v2-5s-particle-splat-128.png` 与 `rdoc_captures/cinematic-liquid-v2-10s-particle-splat.png` 只属于 surface-splat 视觉 smoke，不再代表最终 optics v4 合同。
- 最终 `cinematic_liquid_v2_surface_splat_optics_v4`（`shaderVersion=6`）正式流程通过：RTX 5090 / Vulkan / 1280x720 / 320,068 粒子 / 128x64x96 / 墙钟 15 秒；5.1 秒 RenderDoc 抓帧 0.103 秒已排除计分，1 次尝试保存 1 个 capture。正式 Compute 10.572 ms、Render 1.553 ms、Total 12.125 ms、`263.98 MParticle-step/s`、966 measured frames，结果 id `20260715-170629-492`。最终图为 `rdoc_captures/cinematic-liquid-v2-5s-formal-optics-v4.png`。
- 最终核心 CLI Release 与 WinUI Release x64 build 均成功；WinUI 为 0 error、2 个既有 duplicate WinAppSDK warning，但未做最终交互选择/run/history 验收。`verify-windows-stage` 已包含 3 个 surface SPIR-V；capture 名称区分 v1/v2；非 15 秒运行进入独立 `_preview` 结果组。
- Vulkan 换机兼容收口：`VK_EXT_debug_utils` 改为枚举后可选启用；设备选择要求同一 queue family 同时支持 graphics+compute，避免在分离队列设备上提交无效 mixed command buffer；swapchain out-of-date 现在明确中止而不再空转并污染计分。完整 resize/recreate 体验仍可后续补。
- 上述兼容改动后又跑 RTX 5090 Vulkan 3 秒 smoke：CLI 输入 `--host-memory` 会明确警告并归一化为 Device-local；窗口、combined queue、181,216 粒子 simulation 和结果保存均通过，`279.96 MParticle-step/s`。
- 最新 GUI-first staged 输入已从头配置/编译/安装到 `out/stage/cinematic-liquid-windows-x64`：483 个文件、257,748,185 bytes，包含 5 个 MLS-MPM compute SPIR-V、3 个 liquid resolve/render SPIR-V、WinUI self-contained payload、MSVC runtime 与完整 RenderDoc。stage verifier 与 Inno Setup `-StaticOnly` 均为 0 error（保留 5 个已知发布 warning）。
- 从该 staged 目录本身运行 6 秒验证通过：程序加载随包 `tools/RenderDoc/renderdoc.dll`，第 5.1 秒生成 19,477,795-byte `.rdc`；结果 id `20260715-044159-098`，`captureAttempts=1;captureExcluded=true;captureCount=1`。这证明最新 liquid 资产与 bundled capture 路径可用，但仍不是第二台/干净 VM 的安装验收。
- 同一 staged 目录内 WinUI 隐藏启动 5 秒后 `HasExited=false`、`Responding=true`、标题 `GPU Benchmark`；随后仅结束该测试进程。GUI Release 与 staged 启动均通过。
- 最新 liquid engineering ZIP 已生成：`out/packages/cinematic-liquid/GpuComputeBenchmark-0.1.0-windows-x64.zip`，124,447,344 bytes；SHA-256 `f35e6322b3a753cb57fbc9775b1e57a31ab5044ad3c227cf65a78b4857488ef3`，旁边有 `.sha256`。它用于第二台/VM 解压验收，不等同于正式 Setup。
- 最终 staged **完整产品流程**通过：RTX 5090 Vulkan 墙钟 15 秒，16 → 1604 steps/draw，第 5.1 秒由包内 RenderDoc 生成 184,384-byte `.rdc`；正式 Render 14.872 ms、`198.80 Gpix-step/s`、stableScore 198.64/CV 0.08%，结果记录 `captureAttempts=1;captureExcluded=true;captureCount=1`。最终 staged WinUI 启动 5 秒后仍 `Responding=true`、标题 `GPU Benchmark`。
- WinUI 最终重编通过：0 error、4 个既有 warning。Python 报告脚本 `py_compile` 通过；当前可用测试 Python 没有 matplotlib，因此未伪造图表运行成功，冻结 report worker 仍是明确发布门槛。
- 旧 engineering ZIP/RenderDoc 1.43 记录已被下面的 v0.1.0 release candidate 取代，不得再把旧 hash 当最终资产。
- 最新 staging verifier：**0 error、5 warning**；warning 为无-Vulkan-loader clean-machine 尚未测、staged GUI 尚未做 clean-machine orchestration、bundled RenderDoc 第 5 秒抓帧尚未在 clean machine 测、无 frozen report worker、无项目分发 LICENSE。`vulkan-1.dll` hard import 已修复，不再是 warning。
- 最终 packaging manifest 已复核 `vulkan/directX12/directX11/openGL=true`、GUI self-contained=true、MSVC runtime=true、RenderDoc portable=true、Vulkan delay-load=true、frozen report worker=false、project license=false。stage 为 511 files。
- 当前 GitHub Release 候选位于 `out/release/windows-x64`：ZIP `GpuComputeBenchmark-0.1.0-windows-x64.zip`，124,374,472 bytes，SHA-256 `7c5d89dab1b5a625f2ac9c7120b0098677b9cac4236a1b14ba2d798b1dcaa40e`；Setup `GpuComputeBenchmark-0.1.0-windows-x64-setup.exe`，91,887,918 bytes，SHA-256 `f301426776b8ad2bd816a3f6de55463fd4b2f6be0ca3c7f691bfd8108aca0436`，NotSigned。`sourceRevision=3237545...` 且 `sourceTreeDirty=true`，因为本轮改动尚未提交。
- 没有创建 tag/GitHub Release，也没有在第二台/干净 VM 验证；仓库无根 LICENSE，因此当前资产是**上传候选/换机验收包**，不是已公开发发布版本。

### 2026-07-16 Windows ARM64 平台全链路构建验证（本轮）

- **环境限制解决**：针对构建机没有 ARM64 Vulkan SDK 问题，首创编写了 `scripts/gen-vulkan-arm64-lib.ps1`，动态分析 x64 版本的 `vulkan-1.lib` 并通过 `lib.exe` 交叉生成原生 ARM64 的 `vulkan-1.lib` 存入 `$BuildDir`，成功解决引擎与 GUI 链接阶段的未解析符号与架构冲突。
- **构建环境与编译器**：升级 `stage-windows-release.ps1`，添加 `-Arch ARM64` 指令，调用 MSBuild v145 平台工具集对 WinUI GUI 与 `gpu_engine` 进行交叉编译，vcpkg 依赖（GLFW3 等）在 manifest 模式下顺利下载并还原原生 ARM64 版本。
- **MSVC 运行时架构审计豁免**：针对 Microsoft ARM64 Redist 目录中特有的 x64 `vcruntime140_1.dll`，在 `scripts/verify-windows-stage.ps1` 中加入专门豁免，使 PE 审计流程顺利在 ARM64 上取得 0 error 通过。
- **安装器与打包流程验证**：
  - 更新 `cmake/Packaging.cmake` 使 CPack 对 Windows ARM64 目标动态输出 `windows-arm64` 包名。
  - 更新 `installer/GpuComputeBenchmark.iss` 与 `scripts/build-inno-installer.ps1`，使 Inno Setup 自动映射 ARM64 兼容指令集，编译生成 ARM64 的专用 setup。
  - 成功一键跑通 `build-windows-github-release.ps1`，并于 `out/release/windows-arm64` 输出所有发布产物：
    * `Mangekyo-0.1.0-windows-arm64.zip` (24.4 MiB, SHA256: 44677d52e30664291906d633651498cbe77c32c9e8d5678fdb38556831b0b7d1)
    * `Mangekyo-0.1.0-windows-arm64-setup.exe` (16.0 MiB, SHA256: ab8a721bcfdc814040d71aa8f9d5898355e7abfff395f0a2af9b9d3150a20874)
    * `release-assets.json` 与 `SHA256SUMS.txt` 均生成通过。

## 11. WebGPU、Cinematic Liquid v2 与 TriangleBin 移植（2026-07-15）

### 11.1 当前项目的 WebGPU 路线

- [ ] **P0 capability registry**：先抽出统一 backend/workload capability registry，消除 `main.cpp` 多处 backend 工厂、固定 API 布尔字段和 WinUI `array<bool,4>`；否则第六后端会继续扩大状态漂移。
- [ ] **P1 原生 WebGPU**：当前仓库优先固定 Dawn 版本，以独立 C++20 bridge/PIMPL 接入现有 C++17 engine；实现 adapter/LUID 映射、surface/headless、device-loss、timestamp query ring、GUI/API 检测和打包 notices。没有可靠 GPU timestamp 时禁止产生正式 score。
- [ ] **P2 Stream WGSL**：先验证 1M，再根据 adapter limits 支持 4M/16M；大 dispatch 必须拆分。使用临时 `stream_webgpu_v1`，在字节模型、输出和 timestamp 与原生 API 对齐前不混分。
- [ ] **P3 GPU Burn WGSL**：保持 1280x720、两次 fullscreen draw、固定循环和 auto-tune；push constants 改 uniform ring，完成黄金图/checksum、15 秒流程与底层后端记录。使用临时 `gpu_burn_webgpu_v1`。
- [ ] **P4 Cinematic Liquid v2 跨后端**：Vulkan 与 WebGPU 共用固定 scene/pass/quality contract；WGSL 保留定点 atomic P2G，R32F 过滤必须用手写 trilinear 或明确 capability，不依赖可选 `float32-filterable` 后静默改变画质。
- [ ] **P5 浏览器 `/web`**：作为第二运行环境，共享 WGSL/manifest，但结果必须单独标记 browser、浏览器版本、adapter、底层实现（可得时）、timestamp mode/resolution。浏览器不能保证枚举当前电脑全部 GPU，也不承诺本地文件路径、WinUI 或固定第 5 秒 RenderDoc。
- [ ] 结果 schema 增加 `apiImplementation`、`underlyingBackend`、`implementationVersion`、`timingMode`、`timestampResolutionNs`；`WebGPU/Dawn/D3D12`、`WebGPU/Dawn/Vulkan`、浏览器 WebGPU 不得只凭相同 workload 名混排。

### 11.2 Cinematic Liquid v2 固定场景

- [x] **Vulkan 固定实现**：128x64x96 网格、`dx=0.04`、10 substeps、320,920 粒子（142x14x98 base + 48x37x71 dam）；stiffness 45,000、viscosity 0.035、maxSpeed 8。v1 的 181,216 粒子/96x56x64/10 substeps/160 ray steps 与历史成绩永久保留。
- [x] 用户鸭子与七刚体 index ABI 均保留：boat=2、sink=3、ducklings=4-6。船不再硬锚定，而是 34 kg 有限质量 + 软系泊，推进器反冲可驱动/摇摆；沉球是 1.06 水密度比、4.28 秒释放、-9.81 重力、0.015 空气阻尼，material 水阻按 displaced mass/浸没率门控。两者的精确状态和最终画面仍待验收。
- [x] 当前有限高度 pool wall 以 inset 0.45 内嵌，wall-top fraction 为 0.42；有限宽 outer catch band 允许越沿粒子真实落地，但不是无限流体域。`pool.z`、PVC bottom、liner、程序化草地与物理 floor 都对齐 `y=0`，ring separation 由 `2*tube` 推导。
- [x] 当前表面重建为独立 Spiky² fixed-u32 particle splat → 128x64x96 R32F volume，再做自适应 5x5x5 binomial resolve，保留低支持喷滴；renderer 延续最多 4 次界面的 Fresnel/Snell、分段 Beer–Lambert 和 opaque depth sorting。沉球 entry crown/whitewater 读取真实 GPU body/local fluid，不是 fragment 假水花；尚无 secondary-spray 粒子。
- [x] 前景透明侧壁是独立软 PVC 薄膜近似（IOR 1.50、Fresnel、弱吸收、wrinkle），不是完整 PVC 多介质 ray path；环境是无限程序化草地与大气天空/云，不使用 cubemap 资产。
- [x] 当前 MLS-MPM 结果身份为 `cinematic_liquid_v2_physical_scene_v8`，`shaderVersion=9`、`sceneVersion=5`；共享光学使用 `extinction=(12,3.6,2.5)`。用户调整后的 smooth-min 鸭妈妈 + 3 只小鸭及全部 7 刚体均保留。历史 v1、optics_v4、duck-family-v5、iterative-optics-v6、physical-scene-v7 与当前 v8 各自独立成组，不得混分。
- [x] 历史 optics_v4 的 RTX 5090 Vulkan 正式 15 秒 + 5.1 秒 RenderDoc 已通过：`20260715-170629-492`，Compute 10.572 ms、Render 1.553 ms、Total 12.125 ms、`263.98 MParticle-step/s`、966 measured frames；capture 0.103 秒排除，1 attempt/1 saved。最终图为 `rdoc_captures/cinematic-liquid-v2-5s-formal-optics-v4.png`；该成绩不得归给当前 v8。
- [x] iterative optics v6 的历史必要短预览已通过：`20260715-221447-024`，图为 `rdoc_captures/cinematic-liquid-v2-5s-iterative-optics-v6-final-preview.png`；这是 v6 `_preview`，没有正式 15 秒成绩且不得归到 v7/v8。
- [x] physical-scene v7 的 shader、6 SPIR-V、CLI Release 与 WinUI Release x64 build 通过；只有 6/8 秒自动停止 smoke，没有正式 15 秒、可靠新 RenderDoc 或完整视觉验收。transient `241.13` 不记录为结果；`--time 8` 自动关闭不是已确认 crash。
- [ ] **流体路线恢复后的最高优先级验收（与平台刀并行时由用户指定；Windows 7 已不挡在 macOS 前）**：SPH 视觉已通过，停止优先调外观；先关闭 frame-driven timestep、per-substep impulse clear、viscosity race、atomic scatter ordering 四项，之后才允许非 `_preview` 正式 15 秒 + 第 5 秒抓帧。并实机验证 WinUI selection/run/history、timestamp pass 边界与异常路径资源清理。
- [x] **SPH 重大视觉路线已建立并收口**：318,464 粒子 SPH vertical slice 已完整显示 15 秒并获用户视觉接受；secondary spray/foam 与 propeller wake 是未来增强，不阻塞视觉收口，也不能掩盖正式计分 blocker。
- [ ] 配置元数据继续补齐并核验 `sceneVersion/sceneHash/cameraPathVersion/poolType` 等固定合同字段；正式 Benchmark 禁止自由相机和会改变工作量的参数开关。相机或池体发布后如需修改，必须升新 workload version。
- [ ] DX12、DX11、OpenGL、Metal 尚无 v2 实现。先冻结并验收 Vulkan scene/pass/quality contract，再逐后端移植；不得静默 fallback 或显示伪支持。
- [ ] 玻璃水缸仍只属于未来不可计分的 Liquid Lab / Explore 环境预设；若以后升为正式 RT 场景，必须使用独立 workload/version。

### 11.3 TriangleBin WebGPU 外部移植判断

- 正确术语是 **TBR（Tile-Based Rendering）vs IMR（Immediate Mode Rendering）**，不是 IBR。TriangleBin 用 fragment atomic counter 与阈值显示“先执行的 fragment”空间图案；它是 shade/execution-order visualizer，不是综合跑分，也不能百分百证明物理架构。
- 上游 [Swung0x48/TriangleBin](https://github.com/Swung0x48/TriangleBin) 是 MIT，可以 fork、修改和再分发，但必须保留 `Copyright (c) 2025 Swung 0x48` 与完整 MIT 文本；未获上游认可前产品名应为 **Unofficial WebGPU port of TriangleBin**。
- WebGPU 可用 WGSL storage `atomic<u32>`/`atomicAdd` 忠实复现核心机制；但图案代表浏览器/Dawn 或 wgpu、底层 API、驱动与 GPU 的合成行为，结论必须标为 `Experimental / Observed shade order` 并记录浏览器和底层实现。
- **推荐另建正式 fork/独立仓库 `TriangleBin-WebGPU`**，保留上游 git 历史，使用 HTML + TypeScript/JavaScript + WGSL 重写 UI；当前 benchmark 仓库只提供 `Architecture Tools / Experimental` 入口、链接或结果导入。它与统一 15 秒 score/RenderDoc 流程的结果语义不同，直接合并会污染当前产品边界。
- TriangleBin README 表明其 SDL/ImGui boilerplate 改编自 [sfalexrog/Imgui_Android](https://github.com/sfalexrog/Imgui_Android)，而该仓库根目录没有统一 LICENSE/GitHub license metadata。WebGPU 移植不得复制这部分 Android/SDL glue；直接使用标准 Web UI，或使用许可证清晰的现代 Dear ImGui WebGPU backend。
- [ ] 若真正开始移植，先向上游开 issue 询问其偏好（上游 `web/` 目录还是独立 fork），再创建外部仓库；这属于新的外部项目，不应在本任务中未经用户确认自动创建 GitHub 仓库或发布页面。

## 12. Cinematic Liquid v2 后的三个立即任务（2026-07-15）

- [ ] 先建立通用 `SessionMode { Benchmark, Explore, Soak }` 和 GUI/CLI 生命周期：Benchmark 固定 15 秒、固定参数并可计分；Explore/Soak 无限运行直到用户停止，默认不自动抓帧，结果只进入独立 diagnostic session，不得污染正式 history。从 Explore/Soak 返回 Benchmark 必须重建资源、恢复固定 seed 并校验 scene/config hash。
- [ ] **Liquid Lab / Explore**：自由 orbit/WASD 相机、暂停/单步/重置；重力、黏度、刚度、物体密度/阻尼与螺旋桨速度可实时调整，网格/粒子/水位/substeps 必须 Apply & Reset；提供 Inflatable Pool / Glass Tank 环境预设，并永久标注“自由实验模式，不可与跑分比较”。
- [ ] **GPU Burn — Unlimited Soak**：最终持续烤机应使用当前主 `gpu_burn` Mangekyo Kaleidoscope v2，而不是 legacy `gpu_burn_v1` 或 Other/Legacy 中的旧 `gpu_stress_v1`；默认持续到 Stop/Esc/Ctrl+C，GUI 满载时仍须可响应，显示已运行时间、滚动 GPU time/FPS、利用率、功耗/温度（可得时）和降频趋势。固定 `15s + 第 5 秒 RenderDoc` 仍只用于 Burst score，Soak 不生成正式成绩或硬件错误认证。
- [ ] **VRAM Integrity Soak**：新增独立 `vram_memtest`，优先读取 memory budget，保留桌面/系统安全余量并区分独显和 UMA；分块写入 address/random/walking-bit 等 pattern、设备端校验并累计错误，报告已验证字节、循环数、带宽和错误块。OOM、device lost 与用户停止必须干净退出；`stream` 永远只表示带宽，不表示显存无错误。
- [ ] 开发验收：Liquid Lab 退出后正式 v2 的 scene hash/初始画面恢复；GPU Burn 连续 30 分钟无资源增长且 Stop 可用（发布前建议 2 小时）；VRAM Integrity 连续 30 分钟基线零误报并可从 OOM/device-lost 报告原因。

### 2026-07-18：软件渲染器序号与 OpenGL Run issues 归类

- GPU 下拉框重新保留所有设备的探测序号；软件渲染器现在显示为 `2: Microsoft Basic Render Driver (当前 CPU 型号)`。本次运行的 Summary 仍使用真实渲染器名与 CPU 型号，不恢复含义不清的 `GPU 2` 标题。
- Windows OpenGL 选卡失败不再作为 Summary 中独立的说明段落；经 CLI 精确标记确认的 WGL renderer mismatch 现在进入统一 `Run issues:` 列表，类别为 `GpuRunIssueKind::OpenGlRouting`，detail 显示 `所选设备 -> 实际 GL_RENDERER`。其他 OpenGL 初始化、shader、timeout、device-lost 等错误继续使用各自类别，不会误套选卡提示。
- 默认 Release 输出最初在链接阶段因用户正在运行 `gui/x64/Release/gpu_bench_gui.exe`（PID 12320）而无法覆盖；用户随后明确要求用 `taskkill` 强制关闭该窗口。进程结束后已按 `.agents/AGENTS.md` 指定的 VS 18/v145 MSBuild 命令成功重新生成默认 `gui/x64/Release/gpu_bench_gui.exe`。相同源码也曾使用独立 `OutDir` 成功完成 Release x64 编译与链接，验证产物为 `out/gui-run-issues-verify/gpu_bench_gui.exe`；尚需用户侧做一次下拉框与 Run issues 的视觉验收。

### 2026-07-18：1–3 秒短测统计与 RenderDoc 结束边界

- 根因确认：`BenchmarkConfig::warmupTimeSec=2.0` 包含在 `maxRunTimeSec` 总墙钟内，导致 1 秒运行没有 measured frame，2 秒只有极短 CPU FPS 窗口且异步 GPU timestamp 尚未回收，3 秒才出现显存速率。`AppBase` 现在对短于 8 秒的 time-mode 运行把有效 warmup 限制为总时长的 25%；默认/正式 15 秒运行仍保持原 2 秒 warmup，frame-mode 不变。
- Windows AMD Radeon(TM) Graphics / Vulkan / Stream 1M 的隔离数据目录实测：1 秒窗口化运行 warmup 0.25 秒，116 measured frames，Avg FPS 152，VRAM rate 27.97 GB/s（1.499 ms compute）；2 秒窗口化运行 warmup 0.5 秒，241 measured frames，Avg FPS 159，VRAM rate 29.44 GB/s（1.425 ms compute）。两种短时运行均同时产生 FPS 与 GPU timing/显存速率。
- 自动 RenderDoc 定时抓帧现在由 GUI 与引擎双重限制为不晚于 `duration - 1s`；1 秒或更短的 timed run 没有合法自动抓帧点，GUI 禁用 Capture 控件且引擎将其关闭，手动 F12 不受影响。2 秒 `--capture 5` 无窗口边界 smoke 明确调整为 1 秒；1 秒同参数明确禁用；两次仍有非零 FPS 与显存速率。
- Release engine、独立 OutDir GUI 和默认 `gui/x64/Release/gpu_bench_gui.exe` 均构建通过，仅有既有 WinAppSDK/VCLibs/重复 initializer 警告。用户最新明确要求：以后 GUI 占用默认 Release 输出时直接结束 `gpu_bench_gui.exe`，无需保留旧窗口或先改用独立 OutDir；本轮按该偏好结束 PID 38480 后刷新了默认 Release。
- 第一次交付后的用户截图证明 GUI 仍为 1 秒/0 FPS 且 Capture 保持启用。复盘确认有两个遗漏：(1) `findEngineExe()` 优先启动 GUI 同目录 worker，而 `gui/x64/Release/gpu_benchmark.exe` 仍为 2026-07-17 的旧文件（803328 bytes / SHA-256 `D78A...754F2`），不是刚验证的 `build/Release` 新引擎；(2) `ValueChanged` 内通过 `DurationValueBox().Text()` 读值时，WinUI NumberBox 尚未提交模板文本，所以仍读到旧的 15。
- 已把 Duration 相关逻辑改为以 `NumberBox.Value()` 为权威值；当时长为 1 秒时，GUI 会同时取消 Capture 勾选并禁用复选框/数值框。`gpu_bench_gui.vcxproj` 新增 `CopyGpuBenchmarkWorker` AfterTargets=Build，GUI 每次构建都从对应 `$(GpuBuildDir)/$(Configuration)` 同步 worker，缺失时直接构建失败，不再静默运行旧 EXE。修复后两个 worker 的 SHA-256 均为 `5B0D9E3E7B904748B4AEABB35F7724965C583714CC0412A369B2D8E0902ED4E9`（805376 bytes）。
- 最终不是仅编译验证：启动默认 GUI 后用 Windows UI Automation 把 Duration 设为 1，实读 `CaptureEnabled=False`、`CaptureToggle=Off`、CaptureValue disabled/max=1；再从该 GUI 点击 Run 完成 1 秒 Custom/Auto/Particle 矩阵，Summary 为 Vulkan 1736.19 GB/s / 2453 FPS、DX12 2643.67 GB/s / 2793 FPS、DX11 2161.67 GB/s / 3746 FPS、OpenGL 2088.87 GB/s / 1512 FPS。验证窗口 PID 58264 已按用户偏好关闭。

### 2026-07-18：RenderDoc 主开关改为真实引擎开关

- 用户确认旧 RenderDoc 开关实际为假：GUI 只用它决定是否追加自动 `--capture` 参数，而 `AppBase::Run()` 对所有有窗口运行仍无条件初始化 RenderDoc。现在 `BenchmarkConfig::renderDocEnabled` 是引擎级主开关；GUI 每个非 headless worker 都显式传 `--renderdoc` 或 `--no-renderdoc`。`--no-renderdoc` 会同时清空自动抓帧请求，跳过 Vulkan layer 配置、DLL/API 初始化与手动 F12；RenderDoc 主开关关闭时 GUI 也取消并禁用 Capture。Capture 仍只是主开关之下的自动定时抓帧子选项。
- Windows Vulkan 的可用 layer 搜索除安装包内 `tools/RenderDoc` 外，开发机还会识别 `C:\Program Files\RenderDoc` 中相互匹配的 `renderdoc.json`/`renderdoc.dll`。开启但找不到 DLL/API、以及配置明确关闭时，Raw CLI output 现在都会输出可核对状态，不再静默。
- Release engine 与 VS 18/v145 GUI 均重新构建通过；第一次 GUI post-build 因残留 worker PID 30940 占用 `glfw3.dll` 失败，按用户“以后直接关闭”的偏好强制结束后重跑成功，并由 `CopyGpuBenchmarkWorker` 同步 GUI worker。两个 worker 的 SHA-256 均为 `240A7C9ED4EB83469DAFFE682078CA01F367B4DECE6D13050001E212B36E101D`。仅保留既有 MSB3774/MSB8027/LNK4042 与 C4996 警告。
- 实机进程级验证：同一 Vulkan worker 的 `--no-renderdoc` 运行在检查时存活且 `renderdoc.dll=False`；`--renderdoc` 运行为 `renderdoc.dll=True`。GUI UI Automation 进一步把 RenderDoc 切到 Off 后确认 `CaptureEnabled=False`，点击 Run 后实际子进程命令行为 `... --backend vulkan --particles 1048576 --no-renderdoc`（PID 41072）。全部验证 GUI/worker 已强制关闭，无本项目残留进程。

### 2026-07-18：Full Analysis 开放 Headless

- 用户要求 Full Analysis 单 GPU/全部 GPU 也能运行 Headless。GUI 的 `headlessSupported` 现对 Custom 与两种 Full Analysis 开放，同时继续拒绝 GPU Burn、graphics stress、Render3D、Volumetric、Fluid 与 Cinematic Liquid 等必须渲染的 workload。
- 勾选 Headless 会明确取消 RenderDoc 与 Capture；Custom、Full-One、Full-All 三条 job 生成路径统一追加 `--headless`，不追加 RenderDoc/capture 参数。Full Analysis 仍保留所选 GPU/API 矩阵、报告和图表流程；只有本次命令行实际请求了 `--capture`/`--capture-frame` 且没有生成 `.rdc` 时，才报告缺失抓帧，Headless 或手动关闭 Capture 不再产生假错误。
- Release engine 与 VS 18/v145 GUI 均构建成功，GUI post-build 同步 worker，两个 worker SHA-256 一致。用户随后明确要求：此类修改写完后直接编译即可，不再自动启动 GUI、操作控件或运行 benchmark；因此本切片的运行时 GUI 点击验证未执行，不能写成已实机运行验证。
- 用户截图随后确认首次实现会永久清掉进入 Headless 前的 RenderDoc/Capture 勾选状态。现已增加临时覆盖记忆：第一次进入 Headless 时保存两项原状态并临时关闭；退出 Headless（包括切换到不支持 Headless 的 workload/preset）时原样恢复。恢复后仍会经过现有 capture 安全边界校验，例如时长已改为 1 秒时不会错误恢复自动 Capture。Release engine 与 VS 18/v145 GUI 再次构建成功；按用户要求未启动 GUI 或运行 benchmark，构建前直接关闭了旧 GUI PID 52940。

### 2026-07-18：GPU Burn 固定步数三档 + 自定义

- 按用户最终决定，GPU Burn 不做设备自适应或目标帧时标定。WinUI 现与 Particle 相同采用显式档位：Light 16（默认）、Medium 64、Heavy 256、Custom 16–2048；选择 Custom 时显示独立输入框，每次任务都明确传入 `--iter`。
- 引擎默认从旧 fixed-256 改为固定 16 步，`gpuBurnAutoTune` 继续保持 false；软件渲染器仍保留 32 步安全钳制。新结果使用 `gpu_burn_v3_fixed_steps_<实际步数>_kaleidoscope` 与 `loadModel=fixed_selectable_per_frame`，因此不同固定负载互不混分；旧 `gpu_burn_v2_fixed256_kaleidoscope` 成绩也保持原组。
- `cmake --build build --config Release` 成功。首次 WinUI 链接因正在运行的 `gui/x64/Release/gpu_bench_gui.exe`（PID 50784）锁定输出而报 LNK1104，按用户长期偏好直接强制关闭后，VS 18/v145 Release x64 重编译成功，0 error；仅保留既有 VCLibs/重复 WinAppSDK initializer 警告。按用户要求未启动 GUI、未运行 benchmark。

### 2026-07-24: Android/iOS 成绩后缀与文档去假

- `AppBase::CollectResult()`：`__ANDROID__` → 追加 `_android_preview` + `embedHost=android_anativewindow`；`TARGET_OS_IPHONE` → `_ios_preview` + `embedHost=ios_cametallayer`。与桌面 15s 正式组隔离。
- `CurrentPlatform()`：Apple 路径用 `TARGET_OS_IPHONE` 区分 iOS/macOS。
- Android `WorkloadRegistry`：去掉假 placeholder；仅列出已接线项，字符串与引擎一致（`stream_v1_android_preview`、`gpu_burn_v3_fixed_steps_16_kaleidoscope_android_preview`）。
- `HANDOFF.md` / `android/README.md` / `ios-gui/README.md` / `docs/TODO.md`：按交接规则分层写清「代码已写 / assembleDebug 通过 / 未真机 / iOS 本机未编译」。
- 本条未声称 Android 真机或 iOS 编译通过。

### 2026-07-24: DX11 16M particle dispatch chunking

- Root cause confirmed from the user-provided RTX 4070/5090 logs: 16,777,216 particles at 256 threads per group require 65,536 groups, while one DX11 `Dispatch` dimension is limited to 65,535. This was an engine dispatch-shaping limit, not a GPU performance or driver limit.
- `compute.hlsl` now has a DX11-only chunked-dispatch ABI with `particleOffset` and `particleCount`. `DX11Backend` compiles that variant only for `compute.hlsl` and splits Stream/Particle and Render3D particle updates into chunks of at most 65,535 groups. A 16M run therefore dispatches 65,535 groups and then 1 group. DX12 keeps the existing two-field constant-buffer ABI.
- The existing DX11 limit/error remains for non-chunked compute workloads such as NBody and SynthPeak; this change must not be described as removing the DX11 limit globally.
- Validation: `cmake --build build --config Release` succeeded. The first VS 18/v145 GUI Release link was blocked by the running `gui/x64/Release/gpu_bench_gui.exe` (PID 69016); it was force-closed per the user's standing preference, then the prescribed GUI build succeeded with 0 errors and only the existing MSB3774/MSB8027/LNK4042 warnings. The GUI post-build copied the updated worker and shaders beside the GUI executable.
- Per the user's standing instruction, no GUI or benchmark runtime was launched. RTX 4070/5090 DX11 16M runtime behavior still requires the user's manual validation; only code presence and both Release builds are verified here.
- Historical explanation: the 16M GUI preset existed before the explicit DX11 dispatch-limit guard. Before commit `4096371` (2026-07-16), DX11 still issued one invalid 65,536-group `Dispatch`, but `ID3D11DeviceContext::Dispatch` returns no status and Release did not enable the debug layer, so the bad call could be silently ignored while the run continued. Commit `4096371` added the clear fatal error and exposed the pre-existing invalid workload; it did not create the hardware limit.
- Product version advanced from 0.2.3 to 0.2.4 in the CMake/CPack project version, WinUI file/product resources, legacy Inno metadata, and Vulkan application metadata. The macOS/iOS/Android/OHOS scaffold versions remain their independent platform contracts and were not relabeled as Windows 0.2.4 artifacts.
- 0.2.4 validation/build artifacts: the normal Release engine and VS 18/v145 GUI builds succeeded; both the default and staged GUI report file/product version 0.2.4. A fresh x64 stage with the pinned portable RenderDoc payload passed verification with 0 errors and 4 existing clean-machine/report-worker warnings. `out/packages/Mangekyo-0.2.4-windows-x64.zip` was generated (SHA-256 `1f050431e4d7a4c22d0db9173bc38f6b0f52e3264a5ac727e093f2fd0b17d7a0`). The primary WiX artifact `out/installer/Mangekyo-0.2.4-windows-x64.msi` was generated (SHA-256 `d7c311bafe18456f78993a7e1461d0ec77718f8abebf33741ec1ef8404049778`, Authenticode `NotSigned`). No GUI or benchmark workload was launched; RTX 4070/5090 DX11 16M runtime validation remains user-side.
- `.gitignore` now excludes Android/Gradle generated state (`android/.gradle`, `.kotlin`, root/module `build`, native `.cxx`, and `local.properties`) so a repository-root `git add .` does not stage local Android build caches or outputs.
- Windows ARM64 release artifacts were subsequently built after the x64 release: the engine and self-contained WinUI GUI compiled successfully, and `dumpbin /headers` reports `AA64 machine (ARM64)` for both staged executables. The ARM64 stage verifier completed with 0 errors and 5 warnings (clean-machine/runtime validation, missing frozen report worker, portable RenderDoc not bundled, and host/target architecture mismatch). The manifest reports version `0.2.4`, architecture `arm64`, and `bundled.renderDocPortable=false`. Generated artifacts are `out/packages/Mangekyo-0.2.4-windows-arm64.zip` (SHA-256 `9b4c0fa81b02662670ebdfe7009c35f81e89fbdc81f85a85671287cd4f1493a9`) and unsigned `out/installer/Mangekyo-0.2.4-windows-arm64.msi` (SHA-256 `25d693e092f7cd1fc04771286980f6eaa3e7b395339f55c7494c6f55c250abbf`). The AMD64 build host cannot execute these ARM64 binaries, so no ARM64 GUI or benchmark runtime test was performed.

### 2026-07-30: Legacy Intel DX12/RenderDoc fallback and OpenGL timing repair

- Compared the two user archives for the same Intel HD Graphics 6000. `D:\学习\score\1\imac 2014` contains successful DX12 RenderDoc captures with driver `20.19.15.4531` (particle 64K/1M/16M and GPU Burn), while the failing archive uses driver `10.18.15.4279` and crashes at DX12 `CreateCommandQueue` under RenderDoc. This is a driver/RenderDoc compatibility regression rather than a lack of DX12 support on the GPU.
- Both direct GUI jobs and complete/fill suites now retry a failed DX12+RenderDoc job once without RenderDoc. A successful retry preserves the benchmark score, records a capture-incompatible status, and shows a clear missing-capture issue. The Intel HD 6000/`10.18.15.4279` case additionally recommends the previously proven `20.19.15.4531` or a compatible newer Intel driver. ARM behavior is unchanged because RenderDoc is unavailable there.
- Legacy Intel OpenGL `10.18.15.x` now uses synchronized wall-clock timing instead of unreliable timestamp queries. Generic timestamp validation also rejects non-monotonic, zero-duration, and implausibly high-throughput samples, then switches to the synchronized fallback.
- GUI issue reporting includes both DX12 capture fallback and OpenGL synchronized timing. The unsupported-combination heading no longer says that the condition is "not a test failure". The software-renderer GPU Burn ceiling text is synchronized at 64 steps.
- Restored the missing `endif()` around the concurrent iOS CMake source-list block so project generation remains valid; no unrelated iOS implementation was changed.
- Validation: Windows x64 Release CLI+GUI and Windows ARM64 Release CLI+GUI both compile successfully with 0 errors. Per the user's standing instruction, no GUI or benchmark runtime was launched. Actual DX12 capture restoration on the iMac still requires installing a working Intel display driver and user-side validation; the fallback cannot create an `.rdc` through the crashing driver.

### 2026-07-30: 0.2.6 legacy timing, result provenance, and History GPU naming

- Reviewed the archived machines under `D:\学习\score\2\bug报告`. Their zero-FPS and impossible-throughput rows are consistent with unreliable legacy GPU timestamp queries: Glenfly Arise2030, Radeon HD 5700, GeForce 9500 GT, and GeForce GTX 570 now use a synchronized wall-clock fallback on the affected OpenGL/DX11 driver paths. Generic validation also switches away from GPU timestamps when queries fail, never resolve, go backwards, return zero, or imply an impossible Stream rate. Synchronized measurements receive a distinct timing marker in `workloadConfig` and `_sync_timing` workload-version suffix so they cannot silently mix with timestamp-query scores. Existing bogus JSON rows are preserved; rerunning creates valid, separately identified results.
- GPU display normalization now strips OpenGL decorations such as `/PCIe/SSE2`, matches those aliases back to the DXGI/Vulkan adapter, and automatically supplies an omitted vendor prefix without rewriting the detailed model: RX/Radeon/Vega/HD families become AMD Radeon, GeForce/RTX/GTX/GT become NVIDIA GeForce, Fantasy/Fenghua becomes Innosilicon, and Arise becomes Glenfly. History vendor headings are localized in Chinese, including `英伟达`, `英特尔`, `摩尔线程`, `芯动科技`, and `格兰菲`; historical rows are normalized at display time, so old result files benefit without a migration. FantasyII remains the detailed model string while its vendor is shown as 芯动科技 (the product is 风华2号), per the rule not to translate model names.
- Result schema is now v4 and records `appVersion` in JSON/CSV. GPU results already carried `workloadVersion`, `driverVersion`, `osVersion`, `platform`, `osArchitecture`, and `processArchitecture`; CPU results now receive application/OS provenance as well. Old rows remain readable and show `Unknown (legacy)` when no application version was stored.
- Windows product version is `0.2.6` in CMake, WinUI resources, the Inno definition, and engine metadata. The WinUI project now links vcpkg libraries from each CMake build tree rather than a machine-global `C:\vcpkg` path, fixing the ARM64 GUI link on a clean/self-contained build.
- Validation: x64 Release engine/CLI and WinUI GUI compile successfully; ARM64 Release engine/CLI and WinUI GUI also compile successfully. `git diff --check` reports no whitespace errors (only existing line-ending conversion warnings). Per the user's standing instruction, no GUI or benchmark runtime was launched.

## 13. 后期水体 RT、路径追踪与超分可行性（2026-07-15，仅规划）

- **禁止提前实现**：本节当前只保存可行性和接口边界。先完善现有 Stream/Particle、GPU Burn、Cinematic Liquid v2、GUI、固定 15 秒/第 5 秒抓帧与结果合同；只有这些通过验收且用户再次明确提高优先级后，后续 AI 才能开始本节代码。2026-07-15 本轮未实现任何 RT、路径追踪或厂商超分代码。
- 结论：均可实现，但它们是 v2 与三个自由/无限模式之后的**独立高级图形套件**，不是 `cinematic_liquid_v2` 的补丁。至少拆成固定光栅/计算基线、`cinematic_liquid_rt_v1`、`water_pathtrace_v1`；更换渲染算法、光追能力或超分供应商不得共用一个 score contract。
- **Hybrid RT 优先于完整路径追踪，但不是小补丁**：当前水是 128x64x96 scalar density + fullscreen fragment raymarch，硬件 AS 只接受 triangle/AABB，不能直接接收 density volume。最小可行版本应为独立 `cinematic_liquid_hybrid_rt_v1`：保留 MLS-MPM、density/whitewater raymarch；把池体、鸭子、彩球、船、沉球做 triangle BLAS，七刚体由 GPU state 更新 TLAS；在现有 fragment 中用 Vulkan `VK_KHR_ray_query` 追踪太阳阴影、水面反射和折射后的实体命中。不要首版做每帧 marching-cubes+BLAS 重建，也不要逐液体粒子建 AABB。
- **跨厂商硬件路线**：Vulkan KHR ray query 可让同一实现运行于 NVIDIA RTX、AMD RDNA2+ 与 Intel Arc 的硬件 RT 设备；不需要 NVIDIA 专有 SDK。API 能证明使用 acceleration structure/ray query，但不能直接证明芯片内部是哪块单元，发布“RT Core/等价单元实际工作”时还需 Nsight Graphics、RGP、Intel GPA counter。Apple 需先完成 Cinematic Liquid Metal backend；Metal `supportsRaytracing` 只说明 API 可用，正式 hardware 组应额外要求明确带 fixed-function traversal 的 Apple family 9（A17 Pro/M3）或更新硬件。
- **硬件 pipeline 不会自动软降级**：同一安装包可以带 hardware/software 两条路径，但缺 extension、DXR tier 或 Metal capability 时硬件 RT pipeline 创建会失败。`Auto` 必须显式检测后选 hardware，否则运行当前 `density_raymarch_soft_v1`；`Hardware RT` 不支持时显示 N/A，绝不能静默 fallback 后混分；`Software` 显式运行现有 shader raymarch。当前 renderer 已经是软件 sphere trace/解析求交/密度 raymarch，足以作为首版 fallback；现在另写通用跨 API software BVH 属于中高工作量且收益不高。
- 硬件与软件结果必须使用独立 workloadVersion/score group，并记录 accelerationStructure/rayQuery/rayTracingPipeline、RT API/tier/hardware class、effective path、fallback reason、geometryVersion、AS mode、rays/bounces、denoiser、`avgAsBuildMs`、`avgRayTraceMs` 与 total GPU time。DXR 与 Metal RT 都要等对应 Cinematic Liquid 后端存在；当前未实现任何 RT 代码。
- **路径追踪首版冻结第 5 秒快照**：固定 RNG seed、SPP、反弹次数、光源和相机后累积折射、吸收与焦散。实时动态水体会每帧破坏 temporal accumulation，只有在 motion vector、denoiser 和时序稳定性成熟后再做 animated path tracing。WebGPU 当前标准没有 acceleration structure、ray query 或 RT pipeline；WGSL compute trace 必须另命名，不能和硬件 RT 混排。
- 建立三类可选插件接口：`IUpscalerPlugin`、`IDenoiserPlugin`、`IFrameGenerationPlugin`。核心渲染器先统一输出低分辨率 HDR color、depth、无 jitter motion vector、camera jitter、exposure、reactive/transparency mask 和 history reset；动态水面必须提供自身运动矢量与 reactive mask，不能只用相机运动。
- **支持边界**：DLSS SR 走 D3D12/Vulkan（并按 SDK 能力处理 D3D11），仅支持相应 NVIDIA 硬件；AMD 当前 FSR SDK 主线优先 D3D12，旧版 Vulkan 插件必须锁定并显示准确版本；XeSS-SR 可接 D3D12/Vulkan；MetalFX 只接 Metal/Apple 并运行时查询能力。WebGPU 标准成绩不得通过 Dawn 私有底层句柄调用厂商 SDK；如做 native interop，必须单列实现。
- **公平比较**分三组：`Native Baseline` 固定输入=输出；`Fixed-Scale Upscaler` 对所有插件使用相同输入/输出分辨率、相同水面回放与时序输入；`Vendor Recommended` 使用厂商推荐 Quality/Balanced，只作体验展示。分别报告 base render/upscale/total GPU time、VRAM 与 PSNR/SSIM/FLIP，不把画质和 FPS 合成单一总分。
- Frame Generation 与 Super Resolution、Ray Reconstruction/denoising 分开。FG 只报告真实渲染 FPS、显示 FPS、生成耗时与延迟，生成帧不得计入完成的模拟/渲染工作量。
- **安装包**：厂商库均做可选、运行时能力检测与动态加载。Streamline/DLSS 只分发 NVIDIA 签名 production DLL 并遵守 RTX SDK 通知/发布条款；当前 AMD SDK binary 按其许可证原样分发并保留 notices，不能笼统声称整个 SDK 都是 MIT；XeSS 允许未修改 binary 再分发但必须附 Intel 许可与第三方通知；MetalFX 是系统 framework，无需捆绑第三方 DLL。所有结果记录 provider、SDK version、API、driver、input/output resolution 与 capability path。

### 2026-07-30: Legacy anomalous-score display and Fluid wording

- Old result files remain byte-for-byte compatible and their raw measurements are not rewritten. In WinUI History only, schema-v1/v2/v3 (or otherwise legacy-labelled) bandwidth scores are hidden when the stored value is non-finite, exceeds 10,000 GB/s, or reports a positive bandwidth with zero FPS; the FPS column remains visible and score sorting treats the hidden value as zero.
- A newly completed run retains and displays its raw score, but the Summary adds a localised `Run issues` warning when the same implausible bandwidth pattern is detected. This preserves evidence without presenting an obviously invalid legacy rate as a normal History score.
- Cinematic fluid throughput wording is now `Fluid rate` / `流体速率`; legacy `Liquid rate` worker text is normalised at display time for compatibility.
- Validation: Windows x64 Release engine/CLI + WinUI GUI and Windows ARM64 Release engine/CLI + WinUI GUI all compiled successfully. No GUI or benchmark workload was launched.

### 2026-07-31: 0.2.6 Windows installers

> Historical intermediate state, superseded later the same day by the native
> bilingual x64/ARM64 Setup section below. The Inno files and raw MSI described
> here are not current release assets.

- Rebuilt the 0.2.6 x64 and native ARM64 release stages. Both stage verifiers completed with 0 errors; the AMD64 host did not execute the ARM64 binaries, and no GUI or benchmark workload was launched.
- WiX MSI remains the native deployment format and keeps its default English UI because an MSI has one compiled UI culture rather than an Inno-style language chooser. `Mangekyo-0.2.6-windows-arm64.msi` was inspected through Windows Installer/WiX metadata: summary template `Arm64;1033`, product language 1033 (English), and payload components are 64-bit.
- The optional Inno Setup packages now always show an English/Simplified Chinese language chooser. Both the x64 and ARM64 variants were generated; the ARM64 Setup installs the same native ARM64 application payload, although it is not a native ARM64 MSI.
- Generated unsigned artifacts: `out/installer/Mangekyo-0.2.6-windows-arm64.msi` (SHA-256 `06cc54374d7716b2cb2d4707c30c8470a67bb255c5ae215583498c1163dbc10c`), `Mangekyo-0.2.6-windows-arm64-setup.exe` (`fa433941781432cf420e381d41d6be7b2a8c7eb587a9dac12ad8e15358628412`), `Mangekyo-0.2.6-windows-x64.msi` (`8658db756fc3a2e14074a142452d0ccf76d59dce4bb7e7059546ffc3ac79ff62`), and `Mangekyo-0.2.6-windows-x64-setup.exe` (`e9ae878cd4881010820564e9bc67e3fd6b8cdaa3eed69b64746c314493afaff2`).

## 2026-07-31: native bilingual x64/ARM64 Setup

- Replaced the mixed Inno/MSI user-facing installer scheme with one implementation in `installer/native-bootstrapper/main.cpp`. It is compiled as native x64 and native ARM64, offers an explicit English/简体中文 selector, accepts the MIT licence, elevates only `msiexec`, and uses the matching WiX MSI for install/upgrade/repair registration.
- `scripts/build-native-bootstrapper.ps1` appends the corresponding MSI to each native Setup EXE, validates the PE machine and embedded OLE/MSI header, adds 0.2.6 version resources, writes SHA-256 sidecars, and supports signing. Authenticode-aware payload lookup permits signing the completed EXE without losing access to the embedded MSI.
- `scripts/build-windows-github-release.ps1` now builds the WiX MSI as an intermediate, builds the native multilingual bootstrapper, and publishes the Setup EXE rather than the raw MSI. Formal artifacts are exactly one x64 Setup and one ARM64 Setup; each contains both languages.
- The bootstrapper UI was upgraded from the initial functional prototype to a Mangekyo-styled card layout with custom rounded language/checkbox/button controls, a theme-drawn language popup, a borderless path edit inside a rounded input surface plus a modern folder picker, and the repository's `gui/app.ico` embedded as the window/Setup icon. It follows the Windows app light/dark setting across the title bar, surfaces, text, input and progress colours. Window sizing now targets a compact 720 x 531 client area so the bottom actions are not clipped by the non-client frame and idle progress space does not leave a large blank band. The selected path is passed to the MSI's real `INSTALL_ROOT` property; defaults are `%ProgramFiles%\\Mangekyo` (x64) and `%ProgramFiles%\\Mangekyo ARM64` (ARM64).
- The MIT licence now opens in a custom light/dark adaptive window rather than a legacy system MessageBox. It uses an installer-drawn Fluent-style information symbol and the same rounded primary action as the Setup. Installation exposes a custom theme-aware rounded progress track with a moving accent segment plus status text and mirrors the indeterminate state to the Windows taskbar; successful completion remains visible with a full accent bar, 100% taskbar progress, a localised completion heading and a Close action instead of immediately closing after a system dialog.
- Setup now offers MSI-owned desktop and Start menu shortcut choices before installation (desktop off, Start menu on by default). The bootstrapper passes `CREATE_DESKTOP_SHORTCUT` and `CREATE_START_MENU_SHORTCUT`; WiX conditional components create and uninstall the selected shortcuts, with architecture-specific Start menu directories for x64/ARM64 coexistence. Both generated MSIs were inspected through the Windows Installer `Property`, `Shortcut`, `Component`, and `FeatureComponents` tables to confirm the properties, conditions, targets, and root-feature references.
- The shortcut controls are enabled again after installation. Changing either selection switches Close to Apply and runs MSI maintenance with `REINSTALL=ALL`; both shortcut components are transitive, so their conditions are re-evaluated to create or remove the MSI-owned component in place. Reapplying the same state cannot create a duplicate `.lnk`. MSI table inspection confirms component attributes `324` (64-bit + registry key path + transitive) for both architectures.
- Diagnosed the Setup disappearing immediately after Install from Windows Error Reporting: the x64 process failed with `0xc00000fd` before UAC. `ExtractPayload` placed a 1 MiB byte array on a `CreateThread` worker whose default stack is also approximately 1 MiB. The embedded MSI extraction buffer now uses heap-backed storage, allowing execution to reach `ShellExecuteEx(..., "runas")`, UAC and the progress/completion path.
- The bootstrapper build now embeds an explicit `asInvoker`, Common Controls 6 and Per-Monitor V2 DPI manifest. This prevents Windows setup-name heuristics from deciding elevation at process launch, keeps the bootstrapper at normal privilege, requests UAC only for the `msiexec` `runas` step, and opts remaining system controls into modern Windows styling.
- Added `docs/windows-installer-packaging.md` as the authoritative future packaging guide and linked/synchronised README, building, installer, packaging, limitations and TODO documentation.
- Rebuilt without launching the app or benchmark: `Mangekyo-0.2.6-windows-x64-setup.exe` SHA-256 `3af0217751f68c33c3bc333fbd463cca85f184877614081db2e80a10f189419b`; `Mangekyo-0.2.6-windows-arm64-setup.exe` SHA-256 `74f7c80f5a30bb1cffdc2025cc72b17fa037fd2ca4c829b292d46cfaae58f054`. Both are currently unsigned and still require clean-machine install/custom-path/post-install shortcut/upgrade/uninstall validation before publication.

### 2026-07-19: iOS (iOS 16+) Porting Phase 1 Completed

- **Engine removal of GLFW**: In `CMakeLists.txt`, added iOS system check to force ENABLE_METAL and disable other backends. Defined `GPU_BENCH_NO_GLFW`. Removed GLFW link dependency and `gpu_benchmark` CLI target.
- **Native Apple timing**: Replaced all `glfwGetTime` calls in `src/app_base.cpp` with `gpuBenchGetTime` using high-resolution `mach_absolute_time`.
- **iOS CAMetalLayer compatibility**: Conditionalized `CAMetalLayer` window binding in `src/metal_backend.mm` for iOS to directly bind to a passed `void*` cast of `CAMetalLayer` instead of relying on GLFW and `NSWindow`.
- **SwiftUI iOS App XcodeGen Setup**: Created `ios-gui/project.yml` targeting iOS 16.0+, linking only Metal, UIKit, and other system framework dependencies.
- **iOS UI Adaptation**:
  - `MangekyoApp.swift` implements `scenePhase` lifecycle listener to cancel running benchmarks on background transitions.
  - `ContentView.swift` uses a `TabView` for iPhone (compact) and `NavigationSplitView` for iPad (regular).
  - `RunView.swift` locks backend selection to Metal, replaces checkbox styles with switches, and enforces number pads for value inputs.
  - `HistoryView.swift` replaces macOS `Table` with an iOS swipable `List` and custom rows.
  - `SettingsView.swift` hides working directory selector and displays read-only sandboxed path.
  - `AboutView.swift` queries model details using `utsname` to avoid macOS sysctl calls.
  - `ChartsView.swift` rebuilds chart graphics natively in SwiftUI using the `Charts` framework, removing python3/matplotlib dependencies.

## 2026-10-14: complete physical iPhone suite

- Connected iPhone 13 Pro (iPhone14,2), iOS 27.0.1; installed normal signed Release
  build after the user logged into Xcode and trusted their developer certificate.
  Device Apple ID does not need to match Xcode's signing account.
- iOS `RunView` now has a one-tap full suite: 13 native Metal jobs plus CPU compute
  and cache/memory/page-mapping/storage. Full-screen Metal layer persists between
  jobs; channels remain serialized. Each job verifies its newly saved IDs and
  workload contract. The app saves a manifest, raw log and thermal states in
  Documents/suite-runs, and keeps the screen awake through the whole suite.
- Real run completed 15/15, selecting 16 results. First attempt received a late
  cancellation during completion of CPU memory; all its original 16 records and
  log were preserved. Added cancellation-reason logging and a verified resume
  launch path; reran only the final CPU memory job, completing the suite. Raw
  phone history has 17 rows; selected suite-results.json has 16. No source test
  kernels were replaced and Mac canonical results hash remained unchanged.
- Native score examples: CPU single-thread sample mean 147.355 MWork/s; all 6
  cores 585.369 MWork/s; CPU memory all-core read 34.474 GB/s. GPU fixed-grid
  memory diagnostic 31.041 GB/s; do not call it verified DRAM bandwidth. CPU
  memory repeat includes page mapping (21.78 GB/s, 752 ns per 16 KiB page) and
  direct-I/O 1 GiB storage measurements. Temporary file was cleaned up.
- Saved curves restored with the production Swift parser: CPU 174 points, GPU
  52 points. No independently attributed GPU L1/L2/L3/SoC/SLC measurement. iOS
  single-thread samples are scheduler managed, not pinned physical cores.
- GPU Burn/liquid heated the phone to serious thermal state (2), affecting later
  compute observations; CPU memory repeat was nominal (0). Manifest retains
  per-job thermal states and links the prior attempt through resumedFrom.
- Device build succeeded; iOS 18.5 simulator bridge tests 5/5, shared Swift
  parser checks 151/151, complete-suite record/curve audit passed. Local output:
  out/test-results/iphone13pro-20261014/{report.md,suite-results.json,final,
  attempt-1,memory-curves.png,memory-curves.pdf,memory-curve-points.csv,
  verification.json,verification.txt,bridge-tests.xcresult}.

## 2026-10-14: expanded iOS APIs and complete iPhone suite

- User requested continued completion after identifying missing Vulkan/cache/ES.
  Added pinned SHA-verified official MoltenVK 1.4.2 device and simulator runtime
  preparation (`scripts/prepare-ios-vulkan.sh`), CMake optional iOS Vulkan,
  SPIR-V compilation/bundling, isolated SDK library paths and runtime license.
- Vulkan uses the existing backend through VK_EXT_metal_surface, fixed 1280x720,
  no GLFW. The iOS UI has a persisted translated-API toggle (default off), Vulkan
  picker and full-suite inclusion. Native bridge also requires explicit opt-in.
  Vulkan result serialization was extracted into a shared desktop/iOS helper;
  all bandwidth/L1 curve families and existing desktop contracts are retained.
- Probing accounts for direct MoltenVK lacking the loader-only portability
  enumeration extension. Vulkan liquid is excluded on iPhone 13 Pro because
  the GPU reports no R32_SFLOAT linear filtering. Empty timestamps trigger the
  existing synchronized fallback with a distinct score version.
- Added original ES 2.0 legacy GPU Burn via EAGL. It runs the existing ES2 shaders,
  additive two draws, fixed 1280x720, glFinish timing and display-paced present.
  Drawable allocation is main-thread-only; exclusive rendering context then
  moves to the worker. The software cap is applied before shader compilation;
  GUI/matrix respect its limit. iOS lacks ES3.1 compute and does not queue it.
- Added native GPU cache-state diagnostic to individual runs and suite. First
  full v1 cache run passed; a later 128 MiB warm traversal hit the iOS watchdog.
  Preserved failure log, bounded complete-cycle warmups into 8192-load commands,
  checked every sink, and versioned iOS warming as v2. Timed loops unchanged;
  hardware L1/L2/L3/SoC attribution remains unverified. Subsequent full v2 passed.
- Physical iPhone 13 Pro expanded suite passed 31/31, selecting 32 positive
  scores: Metal 14 including cache v2, Vulkan 12, ES2 3, CPU 3 records. By jobs:
  Metal variants 13 + cache 1 + Vulkan 12 + CPU groups 2 + ES2 3. Default switch
  off: 19 jobs; on:31. Passed prefix reuse preserves prior raw measurements.
- Production-parser audit: CPU curves 174, Metal curves52, Vulkan curves77,
  cache-state curves124; page mapping and storage restored. All prior17 phone
  history rows preserved field-for-field; raw history56 across all attempts.
  Mac history SHA remains b6284e581490616bb5892ff58ba3f19bdc4dd507086ff181ac2424b0083c96cf.
- Device/simulator SDK builds and signing succeeded; simulator bridge regression
  10 passed, 1 cache test skipped for virtual non-unified GPU. Three ES2 tiers
  and cache v2 passed on phone. Desktop tests5/5 including shared serialization.
- Documents exposed through Files/Finder sharing. Outputs and report:
  out/test-results/ios-api-completion-20261014/{report.md,suite-results.json,
  final-phone,verification.json,verification.txt,memory-cache-curves.png,
  memory-cache-curves.pdf,curve-points.csv,native-attempt,vulkan-attempt}.
  Complete manifest spans resumed runs, not controlled isothermal A/B data.

## 2026-10-14: second Apple device (iPad8,7)

- User swapped devices and requested continued complete testing. New device is
  iPad Pro 12.9-inch third generation, iPad8,7 / Apple A12X, iOS 26.7.1, 8 CPU cores,
  system RAM 3752 MiB. Nickname was Xiaomi Pad7 Ultra; hardware attribution uses
  iPad8,7. Registered normal development signing and installed Release after
  the user trusted the certificate on this iPad. Prior phone stays untouched.
- Supported matrix 31 jobs /32 selected records passed: Metal 13 + Vulkan 12
  (MoltenVK) + cache diagnostic 1 + CPU groups 2 + ES2 legacy 3. Vulkan liquid is
  excluded for the reported R32_SFLOAT linear-filter feature gap.
- First run passed 30 jobs; cache v2 twice recovered the GPU at 48 MiB parallel
  phase. Archived both errors. Older-than-Apple-family 6 GPU profile now uses512
  dependent loads per each of 4096 parallel chains, with the same128 MiB span,
  serial 65536 loads, checksum validation and bounded complete-cycle warmup.
  Version v3_parallel_512 keeps it distinct from phone v2/8192 and desktop v1.
  This passed all 124 curve points on A12X; GPU cache-level/SLC attribution stays
  unverified. Native timestamps remain the measurement; no failed data saved.
- Updated pure Swift resume validation to retain later passed jobs for an
  identical matrix or replacing only a failed cache profile; other plan changes
  reuse a verified prefix. Verified roles, positive scores and unique IDs.
  Retried only cache; all 31 original records remain byte-value equivalent in
  final history 32. Source regression checks against prior phone data cover
  failed-profile replacement and rejecting incompatible/zero-score records.
- Production parser audit: CPU 174, Metal 52, Vulkan 77, cache 124; all cache curves
  reach 128 MiB; VM/page mapping and storage measured. CPU single 109.415,
  all8 cores 491.864 MWork/s; CPU all-memory 51.204 GB/s; Metal memory 59.904,
  Vulkan memory 56.760 GB/s (distinct methods). Tests and app builds succeeded.
- Artifacts: out/test-results/ipadpro3-20261014/{report.md,suite-results.json,
  final-phone,first-run,attempt-1,retry-1,verification.json,verification.txt,
  memory-cache-curves.png,memory-cache-curves.pdf,curve-points.csv,
  iphone-comparison.json,resume-check,manifest-check,native-tests.xcresult}.
  Comparison contains 30 compatible records; cache version and65K Vulkan render
  timing differ and are not ratio-compared. Mac and prior iPhone hashes match.

## 2026-10-14: score validity audit supersedes completion claims

- Comparing iPad A12X with iPhone A15 exposed three invalid iPad Vulkan headless
  particle metrics: 1701.091 / 1290.151 / 619.881 GB/s for 1M / 4M / 16M.
  Sustained frame-derived crosschecks are 28.5–28.9 GB/s, matching Metal scale.
  Exclude these reported scores; crosschecks are not replacement official scores.
- Existing embedded Vulkan wall-clock validation skips headless. Exact driver
  cause remains unconfirmed. No engine timing change or device rerun this audit.
- 13 selected Metal rendered/liquid records have provisional timing flags;
  greater-than-100% command-interval utilization is not hardware utilization.
  ES2 light workloads show presentation pacing. CPU samples are scheduler-managed,
  not pinned per physical core. Cache v2/v3 configurations differ.
- Completion (31 tasks /32 records per device) is distinct from score validity.
  Prior raw JSON, logs and canonical Mac history remain untouched. Old derived
  reports now carry correction notices; invalid comparison ratios are excluded.
- Artifacts: out/test-results/apple-score-audit-20261014/{comparison-audit.md,
  score-audit.json,comparison-summary.json,comparison.png,source-hashes.json}.
  CPU compute about0.74–0.75x iPhone; CPU read1.48x; independent Vulkan memory
  scan1.94x; Metal16M headless1.55x. Single-run, non-isothermal observations.

## 2026-10-14: targeted timing corrections and physical retests completed

- User authorized only necessary retests; final scope iPad11 + iPhone7 jobs,
  plus four iPhone repeats after its first late jobs reached thermal state2.
  Final selected18 records all start nominal; iPad ends0 throughout, iPhone
 16-step Metal and heavy Vulkan end1, others0. No CPU, memory sweep, GPU cache
  state, ES2 or Mac reruns. No changes to original archived result JSON.
- Embedded Vulkan particle validation now includes headless, scales repeated
  dispatches with dependencies, excludes calibration samples, and stores its
  synchronized compute time. Invalid partial query spans are rejected. A12X
  consecutive-submission queries remain near zero despite24ms real work, so
  fallback now correctly runs. iPad1M/4M/16M=22.618/27.317/28.935 GB/s.
- iOS Metal rendered tests use one frame and wait for compute completion before
  render submission, then render completion before the next frame. Separate
  serial_timing_v2 contract prevents comparison with overlapping old timings.
  MoltenVK GPU Burn now single-frame with startup render-query/wall checks and
  single_frame_timing_v2 contract. Desktop Metal scheduling is unchanged.
- Device launch --ios-suite-jobs filters explicit supported IDs; no GUI option
  added, standard complete button stays full matrix. Targeted runs wait for
  reported nominal thermal state with600s bounded cooldown (no score on timeout).
- Heavy GPU Burn256: iPad Metal1.527 /Vulkan0.894; iPhone Metal1.586 /Vulkan0.967
  Gpix-step/s. FPS agrees in direction, unlike old iPhone2.098 Vulkan claim.
  Liquid: iPad6.903 /iPhone12.931 MParticle-step/s, no compute/render overlap.
- Old iPad32 and iPhone56 raw records preserved field-for-field; histories now
  43 and67. New selected results and curated full32-row mixed-run manifests are
  in out/test-results/apple-targeted-retest-20261014/. Raw logs and all thermal
  attempts are kept. Latest phone device ledger has only4 cooldown jobs; the
  derived full selected set combines original unaffected records and new retests.
- Device Release SDK and signed builds passed; result compatibility tests2/2,
  pure Swift production-model suite checks against both curated32-row sets pass;
  verification checks append-only history, explicit scope, timing intervals and
  thermal starts. Both apps returned to ordinary UI after data retrieval.
  Main report: out/test-results/apple-targeted-retest-20261014/report.md.

## 2026-10-14: all available device comparison (no new measurement)

- User asked to compare current computer and all previous scores. Read canonical
  Mac137, iPhone67, iPad43 and repository220 historical particle records; export
  467 source rows with versions, original IDs, source and timing flags. Extract
  all18 earlier Android/HarmonyOS device summaries from this file separately;
  raw Windows adb/hdc archives are unavailable on this Mac. Do not claim those
  documented summaries were reverified raw data. Original JSON hashes unchanged.
- Found the same command-span metrology issue in current selected Mac Metal
  1M/4M/16M rendered stream, all3 Burn tiers and liquid overlap (7 rows). Flag
  these provisional; earlier macOS complete-suite percentage claims superseded
  by notice. No Mac retest or engine scheduling changes in this comparison turn.
- Apple table selects latest original complete phone sets after targeted fixes;
  Mac uses corrected1280x720/no-capture selection, latest fullCPU3581.602 and
  CPUmem240.615, auto-single322.300, Vulkan independent read252.395. CPU QoS
  P/E312.254/80.762 separately noted; not conflated with phone unbound samples.
- Repository220 records are all legacy stream, most score0/unit empty/version
  missing; comparison uses raw1M headless compute milliseconds, never invents
  official scores or Burn/CPU/memory from old stream rows. Software/virtual
  devices kept distinct; full API/device inventory retained.
- Main output: out/test-results/all-device-comparison-20261014/report.md;
  apple-metrics.csv/json, selected-apple-results.json, all-raw-history.csv/json,
  historical-mobile-summary.csv/json, historical-mobile-excerpts.md, hashes.

## 2026-10-14: pending retest summary and iPhone thermal review

- User requested a complete retest-needed summary and asked whether iPhone
  thermal results still need repeats. No benchmarks launched in this audit.
- Read stableScore/stableVariancePct/throttlePct on all selected Apple records,
  plus per-job original thermal logs. List19 candidates: Mac8, iPhone7, iPad4.
  P0 timing7 Mac records; P1 throughput drift7 (Mac1/iPhone3/iPad3);
  P2 stability/old-contract followup5 (iPhone4/iPad1). These are not19 invalid
  scores: P2 is a qualification check, drift alone is not frequency telemetry.
- iPhone Metal4M drop8.82%, Burn16 drop9.73% remain after initial targeted run.
  Vulkan1M rendered old drop5.32%. Metal16M rises8.71% but lacks plateau,
  old Metal1M mean12.886/stable19.325 differ; neither proves thermal throttle.
  Vulkan Burn64 and cooled256 still lack stable plateau.
- Cooled iPhone Metal64/256 are stable (CV0.18/0.60%, 1.555/1.586 score), liquid
  cooled nominal and no timing overlap; no extra repeats for those. Liquid
  intentionally never detects stable plateau; stableScore0 is not a failure.
- iPad Metal1M drift9.09%, Vulkan rendered1M14.24%,4M6.87%; VkBurn64 lacks
  plateau. Previously fixed Vk headless large3 remain stable/validated.
  Mac Vk rendered1M old drops54.6%, unclear source; add independent cold check.
- Checklist: out/test-results/all-device-comparison-20261014/retest-needed.md
  and .json, preserving all source IDs. Use equal15s trials, cooling per job,
  validation first. If decline reproducible, report real sustained/curve values,
  do not chase maximum or silently mix changed duration. Existing CPU/memory
  and stable compute are outside this new retest list.

## 2026-10-14: one-click GPU, CPU and complete-machine suites

- User selected computer-local multi-GPU queues and each device's own buttons.
  Windows/macOS/iOS now expose GPU all-items, CPU all-items and combined suites.
  Desktop GPUs/APIs run sequentially; CPU runs once per machine. CPU all-items
  includes compute, cache/memory, page mapping and storage. Formal compute is
  15 seconds; memory/cache sweeps keep their own schedules. Score buttons use
  no automatic capture; the existing capture-oriented preset is retained.
- CLI adds --gpu-suite and GPU-independent --cpu-suite, both inspectable with
  --suite-list. Mac cache-state diagnostics are gated by unified-memory Metal
  availability. M4 Pro has GPU17/native or30/translated; CPU2; combined19/32.
  Native results.json remains137 records, phone67 and iPad43, hash-identical
  after builds/installations/cancel verification. New validations are isolated.
- Metal serialized rendered timing now covers desktop too, with
  serial_timing_v2 and one in-flight frame. MoltenVK desktop timing gets the
  synchronized wall-clock checks and labels validation only after a real pass.
  This supersedes the earlier note that desktop Metal scheduling was unchanged.
  Four isolated formal Mac retests passed interval checks: Metal rendered4M,
  Burn256 and liquid, plus Vulkan Burn256. Other earlier retest candidates were
  not rerun; no claim that all prior scores have been corrected.
- Completed but questionable GPU observations produce review status, preserving
  raw data and continuing the queue. Detect timing overlap, GPU span/frame
  mismatch, missing heavy-workload plateau and >5% throughput decline. Liquid
  does not require a plateau. iOS GUI suites now use bounded nominal-state
  cooldown before every job, and resume rejects flagged old observations.
- Windows tracks live SUITE_RUN stages/counts; cancellation kills the suite
  process tree using nested job objects. The Windows source/XAML were checked,
  but no Windows SDK/build/runtime exists on this host: Windows verification
  remains required on Windows. Do not claim a tested Windows binary.
- Mac arm64 app, signed iOS device app and simulator app build successfully;
  final signed app installed on both iPhone13Pro and iPad12.9thirdgen. Native
  regression tests6/6 pass, plus Swift production-model scope/resume checks.
  Actual Mac CPU all-items launch/cancel was verified in the GUI before any
  score completed. Report/logs: out/test-results/one-click-suite-20261014/.

## 2026-10-14: user reminder for the next macOS retest

- Keep the original seven Mac Metal timing issues open as a group until each
  item has been verified. The macOS timing fix is already merged locally;
  representative retests verified three of those seven: rendered4M, Burn256
  and liquid. Remaining four: rendered1M/16M, Burn16/64. Do not repeat the stale
  claim that no macOS fix/retest has occurred, or claim all seven are resolved.
- Next authorized retest must record per-item logs/curves, timing contract,
  GPU interval/frame crosscheck and saved-result ID. The three new records
  are isolated validation data; canonical Mac history remains unchanged and
  old seven scores retain their timing warnings. Mac Vulkan rendered1M drift
  remains a separate pending cold retest. Updated checklist/status/evidence:
  out/test-results/all-device-comparison-20261014/retest-needed.md and .json.
- This reminder turn only updates records; no benchmark started.

## 2026-10-14: Android/HarmonyOS follow-up audit

- User asked about Android and HarmonyOS after the Mac retest reminder.
  Their adb-suite.ps1 / hdc-suite.ps1 already define29 jobs: Vulkan13,
  ES3.1 eleven, ES2 three, CPU compute and CPU cache/memory two. Missing APIs,
  feature limitations and allocation limits must be distinguished from failed
  measurements. Their apps currently still have individual-test entries;
  GPU/CPU/combined in-app suite buttons have not been added by the Apple/Windows
  one-click work. An optional scope question is pending.
- Neither platform uses Metal or the MoltenVK-only Burn render validation.
  Embedded Vulkan particle timestamp/wall-clock validation is shared with
  Android/OHOS, including the later headless check, but current sources have
  not been rebuilt or physically retested on those platforms this session.
- Android SDK/Android Studio exist on this Mac; adb currently lists no devices.
  The available Huawei SDK folder has system images only, not a native/ArkTS
  build toolchain. No current Android/OHOS physical validation can be claimed.
- Historical evidence exists for18 devices, but original out/adb and out/hdc
  raw archives remain unavailable here. A precise per-result pending ledger
  requires those archives or new device measurements. Do not transfer the
  seven Metal issues to Android/OHOS or mark all29 old jobs invalid.
- For the next archive/device audit, check previously documented candidates:
  unstable Vulkan Burn256 on MIX2S / MiPad5; old16M validation/allocation limits
  on TVs; cache-test swap metadata on MIXFold3 / Quest3 / BlackShark5Pro.
  BlackShark's first thermal run and limited4-core CPU run were already replaced
  in prior records. Harmony MateBook/Mate60 windowed particle timestamps and
  CPU topology/affinity had earlier physical corrections; verify current-build
  fallback/metadata without calling those original failures still unresolved.

## 2026-10-14: commit checkpoint and Android/HarmonyOS complete-suite UI

- User explicitly requested committing existing work before further changes.
  Local commit5d4dbc4 saves75 source/document/test files for the Apple work,
  Windows/macOS/iOS suite buttons and timing corrections. No push; generated
  Xcode user state and ignored raw benchmark archives were excluded. The
  Android/OHOS changes described below remain a separate uncommitted diff.
- Android and HarmonyOS now have GPU all-items, CPU all-items and combined
  buttons, with application-wide sequential queues. CPU compute includes all
  logical processors, then cache/memory/page mapping/storage. Formal compute
  is15s. Actual API, SSBO/storage range, work-group and R32 linear-filter
  capability gates omit unavailable GPU combinations. Maximum29 jobs; actual
  count can be lower. GPU scope omits CPU; combined adds the two CPU phases once.
- Android/Harmony results append to their existing results/results.json;
  suite-runs/<id> stores plan IDs, per-attempt status/result IDs and raw logs.
  Newly saved rows must match API/workload/spec/duration. Timing overlap,
  GPU/frame disagreement, >5% throughput decline and missing heavy-workload
  plateau get review status; raw scores stay and later jobs continue. History
  shows reasons, and CPU math summaries/curves survive the memory phase.
- A real Android emulator check found RequestStop did not reach CPU loops.
  Shared cpu_cancel.h now cooperatively interrupts CPU warmup/rounds and wakes
  the multi-core observer; memory exits between samples and during storage.
  cpu_cli returns130 without saving partial results. app_base RequestStop and
  ClearStopRequest also update the CPU cancellation state. Applies to all
  embedded platforms; desktop process cancellation remains supported.
- Fixed30s rest between mobile jobs. Android29+ waits for OS thermal status0
  when the API can be read, bounded600s, with unknown=-1 logged. OHOS uses only
  fixed rest and explicitly records unverified temperature. Neither guarantees
  no frequency drift. Fullscreen startup waits for surface/orientation settling.
  Stop during startup/rest cannot launch another job; background cancels suite.
- Debug requests add cpu_suite/gpu_suite/all_suite/suite_stop. Existing adb/hdc
  individual29-job scripts remain. OHOS native shader staging now includes
  fluid and memory shader files, not only particle/Burn. Android shared asset
  configuration is locked to avoid concurrent ABI configure_file races.
- Android all4 ABI Debug APK built, JUnit4/4 pass, native7/7 pass including new
  CPU/memory cancellation regression. Native cancellation also checked again
  after the test assertion update. Actual Android35 arm64 emulator CPU scope
  had2 phases/formal15s and acknowledged cancel in0.51s, no memory launch or
  partial score. GPU native probe produced23 GPU jobs, no CPU; three software
  renderer observations completed with review, then queue cancelled. These
  are emulator execution checks, never physical mobile performance results.
- OHOS production models/state-machine harness passes scope/limit/matching,
  quality, CPU-without-GPU probe, cooldown cancel, stale-history and timeout
  checks. This harness transpiles ordinary ETS logic with mocks; it does not
  compile ArkUI/NAPI or prove HAP/physical operation. No complete native/ArkTS
  DevEco SDK found on this Mac; revised HAP and real-device checks remain open.
- Evidence and APK: out/test-results/mobile-one-click-20261014/report.md and
  ../one-click-suite-20261014/*mobile*/*android*/*ohos* logs. Canonical Mac137
  history hash remains b6284e581490616bb5892ff58ba3f19bdc4dd507086ff181ac2424b0083c96cf.
  No physical Android/HarmonyOS retest or remaining4 Mac Metal retest in this
  turn. Preserve the earlier pending retest checklist.

## 2026-10-14: installed Android Studio and DevEco Studio validation

- User installed both IDEs and asked to try the current mobile builds. DevEco
  now exists at /Applications/DevEco-Studio.app; its Contents/sdk/default has
  complete HarmonyOS API26 native/ETS toolchains. Earlier SDK-unavailable
  statements above are superseded. No physical Android/Harmony device attached.
- Android Studio-generated AGP9.4.1, Gradle9.6.0 and daemon JVM25 configuration
  preserved. Current4-ABI APK builds; forced JUnit rerun passes4/4.
- Actual ArkTS compilation found MobileSuite calling private consume methods.
  Fixed to consumeAll batches; model harness now loads production EngineOutput
  and checks actual CPU progress parsing and retained math summaries. Two-ABI
  native/NAPI and ArkTS builds now pass with DevEco Studio26.0/hvigor6.26.8.
- Real first-package check found0 SPIR-V resources. Native shader targets run
  after resources, and native clean deletes generated SPIR-V source outputs.
  New stageGpuShaders Hvigor task runs after clean, before default@PreBuild;
  portable stage-ohos-shaders.cjs incrementally generates39 SPIR-V +7 GLES
  sources. Moved old generated resources away, clean built and ZIP-verified
  all46 shader resources in the HAP. Mac entry: bash scripts/build-ohos.sh.
- Real OHOS phone emulator UI found controls laid out beyond the right edge.
  GPU suite controls now use a full-width column; updated installed UI confirms
  both suite buttons visible. SUITE_* milestones mirrored to hilog for audit.
- Android35 software emulator: GPU plan23;2 jobs completed (1success/1review),
  then cooldown cancellation. Combined25 plan starts, fresh first score flagged
  for throughput decline, then cooldown cancellation. CPU cancellation tested
  with no partial saved score. Pulled per-suite manifests/logs to evidence dir.
- HarmonyOS5.0.4(API16) Mac emulator supports GLES2 only, not Vulkan/ES3.1.
  Native plan3 GPU jobs, completed3/3 with0failures and2review flags. Scores
  saved to results/results.json. Clicking the updated all-items button produces
  plan5 (3GPU +2CPU), one job completes, cancellation stops before next.
  System logs show SUITE_END finished1,total5,review1,failed0,cancelledtrue.
- hdc on this retail emulator denies private filesystem export, even with
  debug-bundle paths. OHOS raw results were NOT exported to the repo; native
  save logs, suite milestones and actual UI states are the execution evidence.
  Emulator score names can resemble hardware but are translated/software
  measurements; do not merge them into physical phone/Mac comparisons.
- Evidence: out/test-results/mobile-one-click-20261014. HAP copied there as
  Mangekyo-ohos-debug-unsigned.hap (5685100 bytes), SHA256
  eaf835d445d1f8abd9dcdbf638a743266f29c6ba1ab44ba6b0911d7ea432fcb5.
  APK unchanged SHA4aa69e2cf72a0f69f629971bd2c095e7a3c7a06ffd5980efd717c08893ef7c57.
  New mobile changes remain uncommitted; no push. Canonical Mac137 result hash
  remains b6284e581490616bb5892ff58ba3f19bdc4dd507086ff181ac2424b0083c96cf.
  Physical validation/signing and the earlier Mac pending retests remain open.
- Final current-APK/HAP CPU suite runs completed naturally2/2, no cancellation,
  no review/failures. Both ran per-LP and all-core compute then cache/memory,
  page mapping and1GiB-file storage tests; saved results and retained compute
  summaries alongside memory curves. OHOS actual UI confirmed2/2 and all these
  sections. Android raw results exported as android-emulator-results.json and
  final manifests/logs in android-studio-final-suites. Complete native logs in
  android-cpu-complete.log/ohos-cpu-complete.log. Still emulator-only validation.

## 2026-10-14: Mac remaining targeted retests

- User requested testing this computer's pending candidates first. Rebuilt
  current5c4e7df arm64 Release app + bundled worker/signature. Executed only
  pending Metal stream render1M/16M, Burn16/64 and MoltenVK stream render1M.
  Vulkan showed variation, so repeated once after60s+, same15s/1280x720 spec.
  All6 runs AC, low-power mode false, thermal state0->0; per-job30s+ rest.
- New Metal:1M189.944 GB/s,16M152.212 GB/s,Burn16 8.480 Gpix-step/s,
  Burn64 9.401 Gpix-step/s. Serialized GPU/frame timing and stable CV pass.
  With earlier3 Metal checks, original7 timing candidates now7/7 verified.
- Vulkan particle stays in its original2-flight timestamp-query contract.
  Initialization compute batch checks against synchronized submit/wait pass;
  no GPU/frame overlap. Two means171.739/169.092 GB/s but within-run declines
  11.74%/14.34% recur. This remains a stability/variation diagnosis, not a
  proved thermal frequency event. stableScore182.345/182.608 refers to earlier
  detected windows, not final sustained throughput; do not report it as such.
  No endless repeats or score cherry-picking. Kept both observations.
- Evidence: out/test-results/macos-targeted-retest-20261014/report.md,
  audit.json,run-status.json,6 native logs,curves.csv(113 windows),PNG/PDF.
  Result audit validates timing contracts/window overlap/duration/specs.
- Canonical Mac history atomically merged137->147, retaining every existing
  object unchanged:6 new rows plus4 previously isolated validated rows.
  Backups/results/history-update.json record exact IDs and hashes; all raw
  scores remain in project evidence. Comparison selected Mac groups and
  retest-needed JSON/MD updated; iPhone/iPad unchanged(7/4 still pending).
  No CPU/memory/other good cases repeated. New documentation is uncommitted.

## 2026-10-14: continued Mac Vulkan drift controls

- User said continue testing;6 additional isolated runs, same packaged worker
  and1M/15s, all AC/low-power-off/thermal0->0,60s+ rest. Vulkan rendered
  flight1:164.016 GB/s/drop16.54%. Headless flight2:264.140/drop-1.33%;
  headless flight1:157.835/drop0.19%. Default rendered flight2:156.753/drop23.12%.
- Isolated vertex-only variant wraps display XY into[-1,1) to retain on-screen
  coverage (original Y and leftward X leave viewport over time). Identical
  worker Mach-O, compute and fragment SPIR-V verified; variant only in out.
  It measures181.467 GB/s/drop-0.01%, stableCV0.34%. Restore original shader
  using standard packaged worker:167.361/drop15.57%. A-B-A controls support
  dynamic render-load effects, not merely2-flight submission or a proven
  thermal frequency event. No production engine/shader change was made.
- Timestamp batch/synchronized wait checks and phase/frame checks pass all6.
  GPU frequency unavailable because powermetrics requires administrator and
  sudo -n has no authorization; did not prompt for/store a password. Per-run
  stableScore can describe earlier windows, not final sustained speed.
  Headless generic Render timestamp field is query-marker gap, not rendering.
- Evidence out/test-results/macos-vulkan-diagnosis-20261014:report.md,audit.json,
  curves.csv,control-curves.png/pdf,visibility-control.png/pdf,all logs,
  original diagnostic result arrays and wrapped variant JSON + sidecar contract.
  Import only2 default rendered2-flight rows; canonical149, all prior147
  objects preserved. Other configurations/vertex variant remain separate to
  avoid replacing standard scores. Checklist now records diagnosed dynamic
  render-load effect with standard-contract/frequency investigation pending.
  No phone/tablet/other good tests rerun; notes remain uncommitted.

## 2026-10-14: durable A15/A12X retest judgments

- User requested saving the prior judgments in Markdown. Added
  [docs/apple-retest-status-20261014.md](docs/apple-retest-status-20261014.md)
  outside ignored out/: A15 7 candidates, A12X 4, reasons, observed scores,
  original result IDs, priority versus stability follow-up, already verified
  exclusions and the retest method. Mac render-density diagnosis is recorded
  as an isolated variant whose findings do not close mobile candidates.
- No benchmarks launched or source/shader changes in this documentation step.

## 2026-10-14: A15 targeted retest completed

- User requested testing A15. Rebuilt current 5c4e7df iOS Release native/app,
  signed and installed on the connected, unlocked physical iPhone 13 Pro.
  Backed up Documents before install: 67 rows, SHA256
  230c46905751f01d741049ca9a051eb0ef54a8e2a62e75db646496a4231d1ce4.
- Ran the 7 documented candidates via production suite job subsets, one at
  a time with 20s rest. Repeated the 4 review cases once each. All 11 runs
  thermal0->0, original shaders, 1280x720,15s incl.2s warmup. No A12X, Mac,
  CPU or other already-good cases rerun. No source/shader edits.
- Passed Metal4M19.118 GB/s,16M17.525 GB/s,Burn16 1.408 Gpix-step/s.
  VulkanBurn64 repeated calibration0.216ms GPU vs115.976ms synchronized wait
  triggers fallback; synchronized result1.042/stable1.041/CV0.82 passes.
  Preserve separate timing contract from first query-timed1.121.
- Still review: Metal1M16.102/16.873, drop29.02/31.83%; Vulkan1M17.935/17.576,
  drop11.47/13.69%; VulkanBurn2561.367/1.623,drop9.10/14.98%,no stable plateau.
  1M render time falls while compute time increases near60FPS; not proven
  thermal frequency change. Burn256 initial timestamp check passes but later
  query windows vary; per-frame synchronized comparison remains needed.
  Aggregate phase/frame checks do not establish every timestamp's accuracy.
- Raw history now78, all67 prior objects unchanged; SHA256
  dc9e706a39b4548add4cf59fd37aa0119febf7802b9885f89858490caa61ace1.
  out/test-results/a15-targeted-retest-20261014 contains before, all11 suites,
  final Documents, console/native logs, build-info,verification,audit,report,
  curves.csv,a15-curves.png/pdf,7 latest rows and32-row curated suite (other
  25 selections unchanged). Updated docs/apple-retest-status-20261014.md.
  Stop repeating unchanged15s tests; next work is diagnosis of remaining3.

## 2026-10-14: A15 P/E separation and remaining3 GPU fixes verified

- User explicitly requested trying the Mac method on iOS, web research, and
  resolving remaining3 GPU cases. iOS SDK exposes public TASK_POWER_INFO_V2;
  Apple XNU maps task_ptime to performance CPU time. Add iOS task accounting
  under the existing thread-clock/process-time conservative >=99% gate.
  Runtime reports2P/4E and full accounting. No private SPI/root/numbered pinning.
  Mac keeps libproc V6, simulators/unavailable capabilities retain auto.
- First2 CPU attempts reject E lower bounds~97%, saving only validP+multi.
  Buffer E progress until its worker stops and background its observer; then
  CPU passes twice: P147.699/146.994,E29.611/26.740,all649.362/661.557 MWork/s.
  Both class stages meet99% each round; E minimum>99.99%. E is background
  policy throughput, not max-frequency capability. Kernel/threshold unchanged.
- Enable defaultP/E+all in iOS buttons; compute suite expects3 distinct rows,
  rejects duplicate/unverified types and preserves CPU cancellation. Cache
  sweep first rejected E amidst interactive detailed log output. Buffer its
  classified log until measurement completes: passes186 curve points>=99%.
  System capacities P128KiB/12MiB,E64KiB/4MiB; L1 reads101.1/13.8GB/s,
  L2 reads81.2/9.2,all-memory35.078. Page mapping/storage completed. No new
  independent SoC/SLC attribution. Public cycles unavailable on iOS (GHz0=N/A).
- iOS rendered stream defaults fixed vertex NDC wrapping, unchanged compute
  kernel/count. Metal new vertex function, Vulkan specialization constant;
  other platforms defaultlegacy. Persist fixed_coverage_v2+renderCoverage,
  explicitly separate legacy/fixed suite requests and disallow old coverage
  reuse. No new GUI setting; automation-only legacy launch argument for control.
  Fixed1M first Metal21.905/Vk20.298 GB/s; legacy control drop32.94/13.69%;
  restored fixed21.803/20.357,stable21.846/20.396,drop-0.15/-0.34%. All6 other
  rendered sizes65K/4M/16M on both APIs pass. Dynamic draw-load effect supported.
- iOS MoltenVK Burn always uses per-frame submit/fence elapsed timing instead
  of startup-only GPU-query validation. Mac Burn policy unchanged. Vk256
  first1.095,confirm1.101/stable1.102/drop-0.37%. Vk16/64 final0.844/1.036 pass.
  Resume rejects prior GPU-query Burn rows; histories are retained. Throughput
  summary no longer falsely diagnoses hardware throttling or early stable
  windows as final sustained values.
- Device rebuilt/signed/installed. iOS Release builds, Swift manifest tests
  (31 jobs/33 records actualA15 plan, negative class/coverage tests), native5
  relevant CTest checks pass. Final selected13 affected jobs pass. Historical
  one-click set now33 records, other18 selections preserved. Source is dirty,
  no commit/push requested. Leave Xcode xcuserdata untouched.
- Evidence out/test-results/a15-cpu-pe-20261014 contains all22 attempted jobs,
  successful/rejected native observations, console/suite manifests, original
  shader controls, build fingerprints, implementation.patch,audit,verification,
  report,curves.csv,gpu-fix-curves.png/pdf,selected and33-row curated set.
  Phone history78->105 with all78 old objects unchanged; final SHA256
  7583ffcf2d323e3775226a43c90b257860ee61b0c89fb35743a47e50a3b569c3.
  Latest Documents snapshot policy-final. A15 original7 candidates closed;
  A12X4 remain untested, Mac original-render diagnosis remains. Updated durable
  docs/apple-retest-status-20261014.md,docs/apple-cpu-core-types.md,TODO and
  ignored comparison checklist. No A12X/Android/HarmonyOS tests this turn.

### 2026-10-14 A15 CPU additivity and landscape/black-screen follow-up

- User questioned P~147/E~27 versus all-core~660. E is verified under
  background QoS, not peak E performance; its policy differs from all-core.
  Kept kernel/scoring unchanged. Added per-round worker work-unit/contribution
  logging and selected-round workerScores in saved config; workers remain
  unclassified, not numbered cores. Also saved worker CPU seconds for P/E.
- Two live CPU passes: P147.029/E27.009/all642.251 and
  P146.674/E26.706/all663.715. Both have >=99% typed residency, thermal0->0.
  Six selected-round worker contributions sum exactly to each unrounded total.
  E actual CPU time~4.8-5s per5s wall window; cannot mainly blame starvation.
  iOS GHz still unavailable; do not infer a measured maximum E frequency.
- GPU fullscreen suite wrapper caused CPU landscape/black screen. Added
  requiresRenderSurface routing: visible ordinary progress card for CPU,
  headless compute/cache; phone portrait during those stages, iPad follows
  device. Surface kept attached/hidden; rendered GPU alone forces landscape.
  CPU tap no longer cancels. Avoid placeholder-GPU rotation before suite starts.
  P/E policy labels and non-additivity explanation added to both Apple apps'
  shared results UI; iOS cache duration now says several minutes.
- Full CPU memory/cache/pages/storage pass:34.834GB/s,186 classified points
  verified>=99%. Physical screenshots show CPU1170x2532; final installed
  build GPU->CPU transition confirms landscape2532x1170 then portrait.
- iOS Release signed/install pass; Swift151 parser checks and manifest routing
  tests pass; native cpu_residency/cpu_cancel/complete_suite pass. Original
  105 rows preserved, history113, curated33. No extra devices tested.
- Evidence out/test-results/a15-cpu-score-ui-20261014/report.md,
  verification.json, screenshots, three manifests/native logs, latest complete
  Documents under transition/documents. Final results SHA256
  d5c39f3e074604c419f07622fc3031f8d518df1aad7e89fff1ffb1a22b28f18c.
  Docs core-types and apple-retest-status updated. Changes uncommitted;
  don't touch user's Xcode xcuserdata. No benchmark remains running.

### 2026-10-14 A15 real performance-priority E compute completed

- User rejected background E26 as insufficient. iOS CPU default now E
  user-initiated QoS with two P/user-interactive helper threads. Target score
  counts ONLY one E worker; helpers run original kernel but their work never
  enters E or the independent all-core score. No new GUI/CLI option.
- Resolve exported private `thread_selfcounts` SPI (flavor4,32-byte time/CPI
  entry x verified two perflevels) to read target per-type CPU time/cycles.
  Based on Apple resource_private.h/sys_recount.c. This is NOT public SDK or
  App Store acceptance. Current development-signed phone works without root
  or security changes. Public process bounds remain for P, macOS E and memory.
- Every E round >=99% typed residency and90% busy; both helpers >=99% P.
  Final reader sandwiches with public thread CPU clock, checks total within
  50us, rejects missing/regressing/inconsistent counters and saves no false E.
  Runtime failure is unsupported/invalid, never fallback to unverified score.
- First physical P146.981/E89.658/all661.517; final confirmation
  P147.120/E89.551/all661.962, all thermal0->0. E bounds~99.999%, busy~99.8%,
  measured mean2.016GHz. Repeat difference0.1193%. 2P+4E estimate652.444,
  1.4378% below all661.962; do not promise exact additivity or numbered binding.
- Stored E version foreground_e_p_load_v1 and explicit foreground policy,
  SPI method, per-round CPUtime/GHz/bounds, helper count/bounds. GUI labels
  performance priority; iOS resume rejects old background E as new E record.
  Memory curves retain previous background contract, not claimed peak E.
- iOS Release signed/build/install and two native CPU suite runs passed;
  153 shared parser checks, manifest compatibility tests, native residency,
  cancellation incl helper cleanup, complete-suite checks pass. Current app
  has final clock-conservation version. No other devices retested; no run active.
- Evidence out/test-results/a15-efficiency-performance-20261014/report.md,
  verification.json,first/confirm Documents+native logs,curated33. Phone history
  113->119, all113 old objects unchanged; final SHA256
  b11467296b2f74a3a336185f2442e87d9dcdb5028f66e4f376c01d86e7f2104f.
  Confirmation IDs20261014-164342-791-P /803-E /812-multi full suffixes in
  report. Updated core-types,retest-status,TODO,ios README. Dirty/uncommitted;
  preserve user's Xcode xcuserdata. No commit/push requested this turn.

### 2026-10-14 A15 foreground E memory completed; user stopped UI acceptance

- Extended verified foreground E + P helper load to CPU cache/memory. Shared
  helper lifecycle with compute; helper work excluded from all saved scores.
  App default and resume validation require the new foreground memory contract.
- A15 targeted CPU memory/cache/pages/storage passed. All-core read35.178GB/s;
  E L1/L2/memory read46.4/31.0/21.9GB/s. All186 typed points verified;
  minimum P99.0697%, E99.8922%; E frequency2.0148-2.0159GHz. Thermal0->0.
  Record20261014-171607-442-cpu-membw. Phone history119->120, old119 unchanged.
- Release built/signed/installed; UI test target build passed. Shared parser
  checks154/154, manifest compatibility, native complete_suite/cpu_residency/
  cpu_cancel including foreground memory helper cleanup passed.
- One-click card is at top and XCTest taps its real button. Physical UI test
  failed BEFORE entering test/tapping due automation-mode enable timeout.
  Full31-job/33-record one-click acceptance has NOT run or passed.
- User now explicitly said stop, phone can unplug, validate UI in simulator
  later. Stopped watch_full_suite.py PID98599; no xcodebuild test or watcher
  remains. Do NOT start simulator/full suite without a later continuation.
- Evidence out/test-results/a15-complete-app-20261014/report.md,
  memory-verification.json, memory-first-final/results/results.json,
  targeted native logs, build/sign/install logs and failed UI xcresult.
  A12X untouched; old13-job plan remains pending, method now implemented.
  No commit/push requested. Preserve user's Xcode xcuserdata.

### 2026-10-14 A15 GPU cache follow-up and phone-first steering

- Latest user clarified retain verified Vulkan jobs and the existing opt-in;
  cache attribution stays explicitly unverified. No global Vulkan ban made.
  Then user said finish phone first and do not rush other changes. No more
  platform/code work after that steering; only physical cache acquisition and
  evidence/audit. Existing simulator attempts remain archived.
- A15 cache follow-up ultimately5 complete scans,620 checksum-verified points:
  173652-182 profiled default,174431-970 default,174539-439 doubled serial count
  with unchanged8192 parallel loads,182221-741 GUI profile,183550-918 final
  exact profile3 capture. Full IDs have20261014 prefix/-metal-cache suffix.
  Phone history120->125, all120 rows unchanged; final Documents phone-final.
- Public MTLCounter inventory only timestamp/GPUTimestamp. External capture
  DID later succeed: Instruments actual Performance Limiters is profile3, not
  the speculative unsupported profile1. Saved real configuration via opening
  a copy of an existing .trace and saving settings (new save panel was disabled).
  xctrace profile3 final60s launch produced643MiB a15-limiters-final.trace and
  30 hardware counter definitions: GPU LLC limiter/utilization,MMU,external
  read/write BW etc. Bounded1000-interval sample includes2 derived metrics;
  it is before benchmark and only proves collection, not a performance result.
  Huge full XML export stopped; exporter removed its incomplete output.
- No independent named L2/L3/SLC traffic/hit counters. LLC percent is not
  isolated cache GB/s; external GPU BW may include SLC. Keep unverified flags
  and do NOT claim independent hardware tier speed/capacity now completed.
- Current selected31 measured jobs/33 records compiled against production
  Swift contract/parser PASS. CPU267 points incl186 P/E,Metal52,Vulkan77,
  cache124, pages/storage complete. Curated file replaces only memory/cache
  records, spans prior completed runs; no new full-phone button acceptance.
- Before phone-first steering, simulator real button first29 jobs failed2
  Vulkan16M allocations. Actual max buffer256MiB/8,388,608 particles. Added
  runtime buffer limit probe, plan filter, visible explanation; revised27
  full jobs/28 records completed and XCTest passed ui-confirm.xcresult.
  Review flags remain; this is UI workflow, not physical score validation.
  Added failed count/errors UI afterwards, built state may need later check.
  No simulator/platform changes after steering. Do not reinstall/start phone
  or new tests now; physical suite and trace finished, can unplug.
- Evidence out/test-results/a15-gpu-cache-final-20261014/report.md,
  phone-final-verification.json,curated-current-a15-results.json,trace,inventory,
  counter-sample.json,verify/output.txt; simulator separate archive
  out/test-results/ios-oneclick-simulator-20261014. No commit/push. Preserve
  user's xcuserdata. Original unsaved GUI Run1 still in Instruments; its temp
  snapshot is incomplete and must not be treated as the successful trace.

### 2026-10-14 A15 phone-first cache isolation controls completed

- Latest user was frustrated: finish the physical phone before other work.
  Added only necessary native cache acceptance controls, no new user-facing
  option or other-device tests. Experimental embedded args isolation_controls=1
  and alu_conditioning=1; results are separate gpu_cache_controls contracts.
- Physical A15 two passes:11 spans128KiB..128MiB ×11 CPU/GPU warm/eviction-dose
  conditions ×7 repetitions. First pass min(nodes,8192), no cycle wrap; paired
  immediate revisit of same addresses, every dispatch exact checksum.847pairs
  /242points each; total1694pairs484points. ALU-only131072steps×4096lanes exact
  endpoints checked, GPU6.40637ms median; zero-load baseline0.0025417ms.
  Both XCTest PASS, thermal0->0,55.2/61.2sec. IDs185659-720/190255-965 with
  20261014 prefix and -metal-cache-controls suffix; v1 vsALU-v2 separate.
- Full default native Metal cache suite then PASS124points37.2sec,thermal0->0,
  ID20261014-190645-149-metal-cache. Final phone history125->128,old125objects
  unchanged. Test app PID40389 terminated after saving; all native test/build/
  trace/export/console sessions ended. Phone can unplug. Do not keep repeating
  the same curve as though doing so would establish independent hardware tiers.
- External profile3 30sec --window2s trace46MiB retains1.134sec of96MiBtarget
  work, rebased timestamps. Counter first50000rows (~75ms) intersects actual
  labeled Mangekyo GPU intervals, solving former background-only sample issue.
  GPU LLC~0.0634%, external read~0.1876GB/s in this low-occupancy slice; not
  full-run mean/peak. Profiler time limit interrupted its separate run before
  saving a result; subsequent independent default run above completes it.
- Controls show 2MiB GPU-warm first170.20/revisit167.74ns,4MiB173.29/168.38,
  8MiB194.16/168.84,16MiB245.53/172.75. Dose0/2/8/32/128MiB first at2MiB:
  170.20/175.80/205.24/253.58/248.57ns. However128MiB control-first576.26ns
  baseline vs327.05ALU-conditioned: working-state matters, not solely cache
  hits. No frequency lock or private-cache flush verification. Public timestamp
  only; external counters no named separate SLC or L2/L3 hit/traffic. Keep
  cacheAttribution=unverified; independent GPU L2/L3/SoC-SLC speed uncompleted.
- Evidence out/test-results/a15-cache-isolation-20261014/report.md,phone-final,
  two xcresults/raw847pairCSVs,608newcurveCSV,PNG/PDF reviewed,active-counter
  sample/summary/rawtrace/labels/GPUintervals. Final history SHA256
  10beabc823de964d6d6e2d2d807fa7c90a6db576718c1b686c628319bce99eb3.
  Curated31jobs/33records retains priorCPU267,Metal52,Vulkan77;defaultcache124
  updated. Production Swift parser/contracts PASS. No new physical full-button
  acceptance, no A12X/Mac/simulator tests, no change to Vulkan opt-in policy.
- Release native/app builds and both physical controls passed; final source
  has an additional invalid-combination guard compiled in static library only,
  not reinstalled after allowing phone unplug. Valid paths actually tested match.
  Source/docs remain dirty, no commit/push requested; preserve user's xcuserdata.

### 2026-10-14 cache attribution methods and mobile Vulkan audit

- User wants independent GPU L2/L3/SLC confirmation, Android/Harmony Vulkan
  correctness, and future device-only scores. Async answer explicitly chose
  methods/code first; no phone reconnect/test. No new physical phone scores.
  Routine App scores/curves are computed and saved locally; external Instruments
  diagnostics are not an App-only independent hardware cache measurement.
- Researched Apple official counters/GPU cache naming, original metal-benchmarks
  (A15 SLC32MB external reference, L2 column blank), M1 EXAM original paper
  (CPU private eviction / GPU->CPU / mapping controls), Khronos synchronization,
  memory model, GLSL volatile, optional KHR_performance_query. A15 applicability
  is not established. Next protocol documented in docs/cache-attribution-methods.md;
  no new CPU-back-probe implemented this turn. Cache attribution remains unverified.
- Shared Vulkan memory v2_verified: fail closed when check not executed/readback
  unavailable; explicit compute/transfer/host dependencies; atomic error marker;
  after-measurement write and copy-destination validation for every dataset.
  Each write/copy chunk now has ONE workgroup owner; repeats in shader. Removed
  cross-workgroup non-atomic write races. All timed shader buffer accesses volatile
  to retain requested accesses; actual hardware cache path remains unspecified.
  Small write/copy footprints also change occupancy; not exact cache capacities.
- GPU timestamps checked for scaling and against synchronized fence interval,
  finite/positive/containment/major undercoverage. Initial failure can choose
  wall timing globally; any mid-sweep anomaly rejects entire result, no mixing.
  Renderer Burn startup check now includes Android/Harmony (previously MoltenVK).
  Vulkan fallback after measured timing flags invalid; AppBase cleans backend
  then rejects storing mixed timing via virtual IsTimingContractValid().
- Serializer blocks invalid/unverified/anomalous reports. Saves checks/counter
  discovery/cache-unverified metadata; non-plateau score explicitly largest
  datasets, not verified DRAM. Optional performance-query enumeration ONLY,
  status enumerated_not_sampled; no actual performance-counter capture enabled.
- Android/OHOS/iOS RAM UI contract updated to membw_v2_verified prefixes and
  requires dataVerified/writeCopyChecks/timestampAnomalies0. Legacy records
  preserved but cannot fulfill new verified RAM job. A15 optional Vulkan RAM
  single retest now pending; A12X original13jobs becomes14jobs/16records.
  No Metal/CPU stable score needs rerun solely due to this change. Mac new RAM
  contract likewise needs a new formal record. No global translated-Vulkan ban.
- Android4ABI APK build +5JUnit PASS; Harmony2ABI/native/ArkTS HAP +production
  model matching PASS; iOS unsigned device Release build PASS (not installed).
  Native serializer/quality/suite CTests PASS; Swift iOS contract tests PASS.
- Isolated actual shared Vulkan Runner linked directly to Mac libMoltenVK:
  1MiB shortened positive29points/checks2 PASS; fake notrun checker and fake
  no-copy shaders both rejected, no valid report. This proves software execution
  paths, NOT Android/Harmony/A15 performance. Main build linked libvulkan loader
  didn't expose MoltenVK devices; direct test linkage worked without reconfiguring
  main build. No actual normal Mac score run or shared history mutation.
- Mock AppBase lifecycle test proves mid-measurement timing change cleans up
  then appends no row; valid mock adds a row in isolated-history-2. LoadResults
  migrates220 legacy rows into each isolated directory. Those directories and
  the Timing contract test row are TEST FIXTURES, never compare as hardware scores.
- Evidence out/test-results/cache-attribution-vulkan-audit-20261014/report.md,
  builds,contracts,runtime-check.cpp/fault shaders,packages.json,verification.json.
  APK/HAP correction shaders verified in packages. Latest ownership/volatile
  changes are part of same pre-release v2 contract; final Android/Harmony/iOS
  rebuilds complete. Package hashes refreshed; volatile and unique-owner shader
  structure verified in APK/HAP. New physical-device validation remains pending.
- No new UI options, no commit/push requested. Preserve user's xcuserdata.

### 2026-10-14: User deferred independent GPU cache attribution

- User said to stop if independent attribution is too troublesome. Stop further
  GPU L2/L3/SoC-SLC attribution probes, implementation and retests until the user
  explicitly requests renewed work. Method proposals are archived, not active.
- This attribution is unresolved, not completed. Preserve measured cache-access
  curves and unverified attribution labels; no scores or code were removed.
- Normal Metal/CPU/Vulkan tests and the existing translated-Vulkan opt-in remain.
  Native Android/Harmony and new Vulkan contract physical validation remain
  separate pending work; this decision does not mark those as completed.
- This turn only records the decision in docs; no new builds or measurements.
- Latest user clarification: keep the existing cross-platform scope consistent
  with Android/Windows. Independent GPU L2/L3/SLC attribution is out of current
  acceptance scope and is not a missing A15 item or blocker. Keep existing CPU
  tier summaries, GPU cache plateaus/curves and unverified attribution metadata.
  Do not resume that research merely to declare A15 complete. The separate new
  Vulkan RAM retest and physical one-click acceptance remain pending.

### 2026-10-14: A12X 14-job retest completed and app stopped

- User requested continued iPad testing. Connected physical iPad8,7/A12X,
  UDID 00008027-000C39923E85002E, iOS26.7.1. Backed up original43 rows.
- Actual targeted14 jobs completed; selected16 new rows, 13 passed and Metal
  rendered16M review (no stable window, both runs <5% drop, timing checks pass).
  Previous4 candidates all retested with stable windows. Do not force the
  Metal16M review closed or repeat unchanged tests merely to obtain a pass.
- MoltenVK RAM startup calibration passed but later interval failed; first
  attempt saved no score. MoltenVK now chooses synchronized wall-clock for
  whole sweep, target>=50ms, metadata timingPolicy. Native Android/Harmony
  selection unchanged. A12X RAM57.903GB/s, 77points,34write/copy checks PASS.
- Foreground P-pressure E strategy failed A12X; utility WITH helpers also
  failed occupancy/helpers. No invalid E score saved. Utility WITHOUT helpers
  passed sustained compute: E60.100/60.013,1.572GHz,99.998% residency and >99%
  busy. A15 helper strategy unchanged. P latest111.527 MWork/s.
- Old inherited-QoS all8 threads varied due incomplete E participation.
  A12X now explicitly requests4P user-interactive +4E utility, preserves
  common wall time and actual work. New apple_pe_qos_workers_v1 all656.340,
  24worker-rounds verified; min residency99.7637%, min busy96.6134%. This is
  type/occupancy verification, NOT numbered-core binding. Nonverified requested
  placement is a review, aggregate remains valid per user's all-core policy.
- A12X public task/thread accounting differed by~5ms for short cache windows;
  use direct thread counters with clock-conservation and99% checks intact.
  Utility memory bursts failed busy requirement and saved no row. Default
  memory uses proven BACKGROUND E profile separately from compute. CPU all
  memory53.737GB/s,267points,P/E verified,page touch/storage measured. E cache
  L1/L2/memory7.5/5.2/5.1 are BACKGROUND, not normal-frequency peaks.
- Native bridge exposes compute/memory defaults separately; GUI uses them,
  no new public options. Typed memory contract requires verified P/E curves.
  Shared Apple result labels recognize foreground E; all-core review metadata
  distinguishes incomplete occupancy. CLI --ios-suite-jobs keeps app awake
  between automation jobs; normal GUI/cancel restores idle timer. CONTROLLER
  MUST terminate targeted automation app when finished.
- Release engine/app built,signed,installed; CPU residency/cancel and Vulkan
  save tests PASS; real shortened Mac MoltenVK functional29point wall test PASS
  (no normal Mac score saved). Updated Swift contract tests PASS. Production
  model validates selected14/16 and full curated31/33 from physical records.
- History72 rows, original43 unchanged;29new actual scores,16selected,13other
  real attempts/policies kept. No test fixture rows imported into phone.
  A15 archive128 SHA unchanged10beabc823de964d6d6e2d2d807fa7c90a6db576718c1b686c628319bce99eb3.
- Artifacts out/test-results/a12x-targeted-retest-20261014/: report.md,
  phone-final/results/results.json,curated-suite-results.json,verification.json,
  production-contract-audit.*,all-worker-rounds.json,GPU/CPU PNG/PDF/CSV curves,
  full per-attempt manifests/logs/builds. Latest selected, never highest-score
  selection. FINAL actual PID2544 killed, fresh process list confirms absent;
  PID2534 was an older app instance, not the final one. Device safe to unplug.
- Independent GPU L2/L3/SLC attribution remains out of scope per user. No
  A15/Mac/Android/Harmony physical measurements this turn. Full31-job physical
  button was NOT clicked this run; do not claim physical whole-button acceptance.
- No commit/push requested. Preserve existing dirty edits and xcuserdata.

### 2026-10-16: New iPad8,1 actual one-button suite and timing correction

- User switched to a new device, UDID 00008027-001969920A87002E, CoreDevice
  9A44542D-75A2-58C6-B79C-18BEAED61843. Physical iPad Pro11 first gen/A12X,
  iPadOS26.7.1, named Van的 iPad. Distinct from prior iPad8,7.
- User enabled Developer Mode and trusted developer signature. Signed team
  2W6PXCJAM6; fresh device had no app/history. Final signed app is in
  build-ios/DerivedData-ipad81-fix/Build/Products/Release-iphoneos/Mangekyo.app.
- Fixed iOS Auto language: preferredLanguages instead of Locale.current,
  declared en/zh-Hans/ja, restore saved appLanguage before initial status.
  Actual UI button Chinese; native log setting=auto preferred=zh-Hans-AU.
- Actual XCTest tapped complete-suite-all, no suite-autorun arguments, ran
  all31 jobs /33 records. Initial22 passed/9 review,0 execution failures.
  Strict physical test asserted review==passed and FAILED as designed.
  Afterwards Xcode stalled during log finalization. Only that completed host
  PID12431 was INT/TERM stopped; xcresult is incomplete, screenshots cannot
  be exported. Do not call the XCTest passed or cite incomplete result bundle
  as successful acceptance. Actual click log and native full manifests saved.
- Found wrong formal MoltenVK headless1M/4M timestamps despite startup check
  passing: 0.067/0.343ms vs1.539/5.842ms wall, scores624.626/488.988GB/s.
  iOS-only headless particles now whole-run synchronized timing, same existing
  fallback as Burn; native Android/OHOS and macOS desktop path unchanged.
  iOS result matching rejects query-timed headless resume. Updated Swift
  acceptance/rejection tests PASS, Release engine and signed app builds PASS.
- Actual corrected headless1M/4M/16M all PASS18.481/26.642/28.754GB/s, stable
  18.206/26.632/28.757, timing matches wall and thermal0->0. Original33 rows
  unchanged; final36. Current selection31 jobs/33 records =24 passed/7 review,
  production model PASS. This is original full UI run plus3 actual retests,
  NOT a second complete button run after the fix. GPU attribution out of scope.
- CPU P110.384,E60.661,all655.509MWork/s; single P/E verified, all8 worker
  aggregate valid but round2 P lower bound minimum98.372% fails full99% gate;
  do not replace all-round quality with selected-round stats. CPU mem51.790
  GB/s, cache curves/pages/storage complete, E memory remains background.
  Vulkan RAM58.051GB/s verified whole wall-clock sweep. Remaining reviews:
  Metal rendered1M/4M/16M, Vulkan rendered1M/4M/16M, CPU all-core residency.
- All measurements ended. Final actual appPID873 killed; fresh process list
  confirms absent. Safe unplug. No bench or test watcher active. No commit/push
  this turn; previous committed baseline13aeafd. Preserve user's xcuserdata.
- Evidence out/test-results/ipad8-1-full-suite-20261016/: report, phone-final,
  curated-suite-results, selected-run-sources, original UI and corrected native
  manifests/logs, verification and production-contract-audit,4 PNG/PDF/CSV
  curve groups. SHA finala44e14deb4ccde5acbd256e46e8d61960fb96bae8031058c1c3155205080bdb0.
  New concise tracked record docs/ipad8-1-one-click-20261016.md. Other devices
  NOT retested. Old query-based iOS headless rows cannot fulfill new policy
  automatically; do not silently relabel old measurements as synchronized.

### 2026-10-16: Remaining iPad8,1 seven reviews closed; app design fixes

- User explicitly requested further review and fixing genuine test-design issues.
  New root out/test-results/ipad81-design-review-20261016/. Do NOT rerun tests
  merely because the earlier report still has7 reviews; latest7 actual PASS.
- Uninstrumented original-app baseline4 jobs: Metal1M35.209 passed,4M29.369
  review(no stable window, drop-2.44%),16M28.340 passed; CPU111.097/60.795/
  656.464 passed. Preserve6 baseline rows. Different thermal/order conditions
  prevent quantifying UI-automation overhead; do not assert sole causation.
- iOS-only MoltenVK synchronized completion now covers ALL particles, rendered
  and headless. Startup-only query checking is insufficient. iOS model rejects
  old query-timed rendered particle rows as well. Native Android/OHOS and
  macOS desktop selection untouched; API toggle retained.
- A12X all-core score algorithm unchanged(common wall time,8 workers,3 rounds
  median). New aggregate_cpu_time_v1 validates actual work-thread P/E CPU time
  versus each group's physical capacity, and minimum worker occupancy, in
  EVERY round. Each group/worker >=90%, rejects missing/invalid/excess counters.
  Individual P/E type purity still saved; single-core99% requirement intact.
  Core-group participation is independent of individual-worker type purity.
  New workload version aggregate_occupancy_v1 and fields; do not relabel old
  failed-purity records retroactively. Apple Swift readers remain identical.
- Corrected actual7 jobs allPASS,0review/fail,selected9 new rows:
  Metal render1M35.076/4M30.661/16M28.470GB/s; Vulkan render1M21.318/4M26.999/
  16M28.952GB/s all synchronized; CPU P111.076,E60.791,all657.976MWork/s.
  Metal kernel/coverage and2% stability threshold UNCHANGED. Lowest all-round
  P occupancy99.6631745%,E96.9217504%,worker96.503048%;24 worker samples
  and3 participation rounds archived. New run individual purity also passes.
- Real current selection31 jobs/33rows production contract AND quality PASS,
  built from prior passed records plus corrected7. NOT a new31-job full-button
  execution. Old failed full-button XCTest remains failed; no false acceptance.
- Final history51; original36 records unchanged. SHA43d43fb16d87ade98bdb80c7c319d8b8e04434eb69cf1c8f8f777e2284b6531b.
  phone-final,curated-suite-results,selected-run-sources,production-contract-
  audit,verification,baseline and corrected7 raw logs/manifests,4 PNG/PDF/CSV
  plot groups saved. CPU memory267,Metal memory52,Vulkan memory77,cache124
  curve points unchanged(from prior passing jobs); no attribution research.
- Native CPU residency/cancel2/2 PASS; Swift contract tests include balanced
  migration, starved group and legacy query rejection PASS. Release native
  engine and signed app PASS; installed on iPad8,1. No tests of other devices.
- Device actual appPID914 killed; fresh process list confirms absent. All
  benchmarks/controllers complete, safe unplug. New tracked summary
  docs/ipad81-design-review-20261016.md; earlier docs link to latest status.
- Still uncommitted since baseline13aeafd; no commit/push requested this turn.
  Preserve user's Xcode xcuserdata. Normal subsequent noise still gets review;
  do not promise every future run reaches a stable plateau.

### 2026-10-16 Latest iOS simulator real-button full-flow acceptance

- User authorized an optional simulator full flow if useful; no new physical
  measurements needed. Built current Release engine/app and used the actual
  complete-suite-all button via XCTest, with only the existing translated API
  gate enabled. No autorun/target subset/resume; all27 supported jobs completed.
- iPhone18Pro/iOS27.0 arm64 simulator573DDAD2-9B49-4C01-AC4E-1FA12BE6D06B:
  XCTest1/1 Passed, xcodebuild exit0, complete readable ui-full.xcresult and
  before/after screenshots exported. Native status review:19pass,8review,
  0fail,28 unique records. Preserve variability reviews; physical gate unchanged.
- Production Swift audit PASS for all requests/result groups/versions/quality
  status/unique IDs and saved curves:CPU174,Metal52,Vulkan77; VM page mapping
  and storage present. CPU screens show normal portrait progress, not black.
  All6 Vulkan particle jobs use synchronized_wall_clock. Shared checks154/154.
- No new execution/saving defect found. Synced macOS Auto display localization
  to iOS (Chinese follows-system label), keeping all6 shared Apple files equal.
  No workload/timing/quality-threshold change this turn.
- Simulator capabilities exclude Vulkan16M2jobs, Vulkan liquid, GPU cache
  diagnostic, GLES2 256. CPU sweeps host14 logical processors, not iPad P/E.
  These are simulator workflow artifacts only, not physical performance.
- Native final Documents and all evidence under
  out/test-results/ios-simulator-full-suite-20261016; report.md/verification.json/
  production-contract-audit.json. Results SHA256
  a0cc55c8350074c31ae387761a8a74c2fb258d6a8b25f31678135e9d9e73c3af.
- Simulator app terminated and simulator shut down, matching initial state.
  All3 physical history hashes unchanged(A15 128,old iPad72,new iPad51).
  Tracked summary docs/ios-simulator-flow-20261016.md. No commit/push requested;
  preserve user's Xcode xcuserdata and all earlier uncommitted changes.

### 2026-10-16 A16 iPhone14 Pro Max partial run; USER UNPLUGGED

- New physical device iPhone15,3 / iPhone14 Pro Max / Apple A16 / iOS27.0.1,
  UDID00008120-000E2D3E0CEBC01E, CoreDevice256C41DC-AC03-5FF9-9D3F-8FDFFC83CDBE.
  User enabled Developer Mode and trusted the developer signature. Installed
  Release build build-ios/DerivedData-a16-full; team2W6PXCJAM6.
- Initial full-01 cancelled first render on surface_disappeared: large phones
  become regular-width in landscape, and ContentView replaced TabView with
  NavigationSplitView, tearing down fullscreen. Fixed by restricting regular
  sidebar branch to iPad. Added simulator short landscape real-button regression;
  XCTest1/1 Passed, readable xcresult/screenshot. Simulator shut down.
- Corrected native full-02 ran22 completed jobs:19pass,3review,0fail. Job23
  vulkan/gpu_burn-64 cancelled(reason user);8 later jobs not_run. Total plan31
  jobs/33records. Native auto full-suite launch, not a physical XCTest tap.
- Reviews:Metal gpu_burn-256 drop>5%;Vulkan stream-1048576 notstable;
  Vulkan gpu_burn-16 drop>5%. Still unmeasured:VkBurn256,VkMemory,GPUcache,
  CPUcompute,CPUmemory/cache/VM/storage,GLES2Burn16/64/256. CPU NOT TESTED.
- User asked progress because they need phone, then explicitly said unplugged.
  STOPPED; DO NOT restart tests. Actual appPID722 terminated before unplug;
  fresh process query absent. Controller complete, no active benchmark.
- out/test-results/a16-full-suite-20261016 preserves all originals, phone-final,
  completed-results22,verification/report, full-01/full-02 manifests/logs,
  max-navigation-regression simulator evidence. Raw24rows=22complete+2cancelled
  diagnostics; SHAda2a2612d9dfbb8381792987ea2b98f6a47e947f108cd7c39298498930dd7df1.
  Other3 physical and prior full simulator history hashes unchanged.
- Default same-plan resume retains19passed and runs12 jobs(3review+1cancelled+
  8notrun). Latest clarification:review is a variability warning, not mandatory
  retesting. On a later explicit continuation prioritize9 unfinished jobs;
  use a separate control for3reviews only if needed for stable comparisons.
  No full31pass claim. Prepared production
  full31 audit executable has NOT run because the suite remains incomplete.
  Tracked docs/a16-partial-run-20261016.md. All source edits uncommitted; no
  commit/push requested. Preserve user's Xcode xcuserdata.

### 2026-10-16 Documentation consolidated for the requested Git commit

- User asked to update Markdown and commit the accumulated changes. A16 doc
  now includes all22 completed scores/IDs and precise warnings:MetalBurn256
  early/late throughput drop16.69%,VulkanBurn16 drop19.08%,Vulkan1M no5-window
  CV<2% stable segment. Drops are not measured hardware frequency; keep mean,
  windows and curves. No repeated tests just to eliminate warnings.
- TODO,Apple status,iOS README,simulator report and A16 local report synchronized.
  Priority is9unfinished jobs after a future request;3quality controls optional.
  A16 CPU remains untested, phone unplugged; do not restart any device tests.
- This commit collects iOS synchronized Vulkan particles, versioned all-core
  group participation checks, retained99%single-core gate, Auto localization,
  stable navigation on large iPhones, regression tests and all4new device/flow
  Markdown reports. Raw results remain in ignored local out; personal Xcode
  xcuserdata is excluded. Native2/2,Swift contracts/shared154/154,27-job simulator
  full workflow and Max navigation regression passed as recorded previously.
- Earlier uncommitted-at-the-time statements above describe historical stages;
  source/tests/docs are consolidated in this requested local commit. No push.
