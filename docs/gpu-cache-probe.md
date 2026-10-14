# Native Metal cache-state diagnostic

Latest method review: [cache attribution and device-only execution](cache-attribution-methods.md).
Vulkan memory now uses `membw_v2_verified`, with mandatory data checks and a
single timing source per successful run. Its inferred workgroup/cache plateaus
remain distinct from independent hardware-level L2/L3/SLC attribution.

`--gpu-cache-probe` supplements the existing Vulkan bandwidth/L1 sweep and
particle-derived Metal memory sweep. It does not change either test or their
historical score contracts. It is a separate `gpu_cache` workload, saved as
`metal_dependent_cache_v1_preview` in the shared results.json.

```bash
gpu_benchmark --gpu-cache-probe
gpu_benchmark --gpu-cache-probe --gpu-cache-steps 131072
gpu_benchmark --gpu-cache-probe --gpu-cache-max-mib 1 --gpu-cache-repeats 3 --gpu-cache-no-save
```

Bounds: address span 1–512 MiB (default 128); serial loads 4096–1048576
(default 65536); repeats 3–15 (default 5). The default grid of the concurrent
test is 4096 threads, with one dependent chain per thread and one eighth of
the serial load count, at least 4096 loads per thread.

## Method

A seeded shuffled cycle has one uint32 node every 128 bytes. Each load supplies
the address of the next load. This dependency limits memory-level parallelism
in the serial pass. The shader writes the final index and the sum of all loaded
indices; the CPU checks both against the exact generated permutation after
every dispatch, including warm-up. The separate GPU eviction read also checks
its sink. The source is `src/gpu_cache_probe.mm`.

The address span increases by alternating ×1.5 and ×2 steps from 4 KiB. It is
not a verified cache capacity: the physical cache-line width, allocation,
associativity, TLB behaviour and dynamic GPU cache use affect these curves.
The 128-byte spacing is a probe choice, not a claim about hardware cache lines.

Every sample first touches a separate 128 MiB CPU buffer, then reads a separate
256 MiB GPU buffer. Three serial conditions follow:

- **GPU warmed:** the GPU traverses the complete target cycle before timing.
- **After eviction pass:** the timing follows the control reads directly.
- **CPU warmed:** the CPU reads every target node after the control reads.

The fourth condition GPU-warms the target and measures 4096 concurrent chains.
Condition order rotates across repeats. Timing is the Metal command buffer's
GPU execution interval; the score is the median, with minimum/maximum and
each raw sample printed. Serial ns/load includes loop, index accumulation and
dispatch overhead. Concurrent GB/s counts only four bytes of requested payload
per load, not all cache-line traffic, so it is not comparable with sequential
DRAM bandwidth. No time from the preceding warming/eviction command is included.

## What this can establish

The probe observes address-span dependent timing, concurrent logical throughput,
and changes after CPU/GPU warming. Doubling the timed load count checks whether
fixed dispatch overhead dominates; it also changes how often smaller cycles
are revisited, so the after-eviction result can improve with longer runs.

The eviction buffers are controls, not a hardware cache-flush instruction.
CPU warming may affect CPU private caches, coherence, shared cache state,
clock policy and translation. A timing difference alone does not independently
identify L2, L3 or SLC, or provide their isolated peak bandwidth. Hardware
attribution requires additional validated controls/counter evidence.

Apple's profiling documentation describes L1, GPU last-level cache and device
memory, and distinguishes cache stalls from MMU stalls:
[Metal profiling tools](https://developer.apple.com/videos/play/tech-talks/111374/).
Do not assume this chip exposes the same GPU L1/L2/L3 naming as another vendor.
The diagnostic records the public Metal counter-set names available on the
device; enumeration by itself does not collect cache counters.

The CPU benchmark suite is unchanged. On macOS it remains scheduler-managed;
fourteen single-thread runs do not prove each physical P/E core was individually
pinned and measured.

Saved curves are available from the Apple GUI's **Cache & memory curves** viewer.
They preserve latency and payload-bandwidth units separately from existing
CPU/Vulkan/Metal bandwidth results.

## A15 follow-up evidence (2026-10-14)

The physical iPhone 13 Pro completed three full scans (124 points each),
including a doubled serial-load control with unchanged parallel work. All
checksums passed. The public counter inventory was only
`timestamp/GPUTimestamp`; raw output and new saved configurations now retain
the full inventory plus `attributionEvidence=timing_and_checksum_only`.
The benchmark kernel and default measurement contract are unchanged.

An external Metal System Trace was also recorded, but its actual counter
profile was zero and the exported counter tables contained no samples.
Attempts to configure Instruments timed out. This is a limitation of the
evidence collected in this session, not proof that hardware counters can never
be collected. L2/L3/SLC attribution remains unverified and is not a completed
independent hardware measurement.

Later in the same session, Instruments opened successfully. Saving the actual
A15 Performance Limiters configuration established profile3; a native scan
recorded30 hardware metrics, including GPU LLC limiter/utilization, MMU and
external GPU bandwidth. The final trace and a bounded1000-interval sample are
archived. These are hardware observations, not isolated L2/L3/SLC speed or
capacity results: the available metrics do not expose separate named SLC
traffic/hits, and GPU-external bandwidth is not necessarily DRAM-only.
The five complete scans saved620 curve points; final phone history125 retains
all120 original rows. No hardware-level attribution flag was changed.

Evidence: [A15 follow-up report](../out/test-results/a15-gpu-cache-final-20261014/report.md),
raw physical Documents, two native-test log attachments, trace and curves.

## Additional physical A15 controls

The later first-pass experiment completed two 11-span × 11-condition × 7-repeat
physical runs, including a separately versioned ALU-only preconditioning control.
Each timed first pass visits at most8192 distinct nodes and immediately revisits
the same addresses, with 2/8/32/128 MiB CPU/GPU read-dose controls. Its484 points
are stored as `gpu_cache_controls`, separate from the default suite score.
The default124-point suite then completed again; phone history128 preserves all
125 previous objects. A bounded external counter sample now intersects actual
labeled GPU execution, rather than only background activity before the benchmark.
Working-state sensitivity remains substantial; neither these controls nor LLC
percent/external GPU bandwidth proves isolated L2/L3/SLC peak speeds.

See [first-pass report and curves](../out/test-results/a15-cache-isolation-20261014/report.md).

## Older iOS GPU parallel profile (device safety)

Physical iPad8,7 (Apple A12X, iOS 26.7.1) twice entered GPU recovery at the
48 MiB parallel pass with 4096 chains × 8192 dependent loads. Its three serial
conditions completed that point; no failed run was saved as a score. Devices
before Apple GPU family 6 now use an explicit shorter parallel profile:
4096 chains × 512 loads. The same shader, address-span sweep to 128 MiB,
8192-load complete-cycle warming chunks and per-dispatch checksum validation
are retained. Serial timed passes still use 65536 loads. This completed all
124 curve points on the iPad.

The result is `metal_dependent_cache_v3_parallel_512_preview_ios_preview`, with
`parallelSteps=512` in its saved configuration. It is separate from iPhone's
v2/8192-load profile and from the desktop v1 contract. Shorter dispatches can
change dispatch-overhead, occupancy and warming effects; neither this profile
nor the original one independently attributes hardware cache levels/SLC.
Native Metal timestamp intervals still time each complete dispatch; the test
does not substitute a CPU timer or fabricate values after a GPU recovery.
