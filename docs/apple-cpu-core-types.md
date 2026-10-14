# Verified CPU core-type tests on Apple Silicon macOS

The existing mixed compute and NEON read/write/copy kernels are unchanged.
Supported Apple Silicon macOS hosts automatically run both performance and
efficiency single-thread stages, using user-interactive and background QoS.
All-core computation always uses every logical processor. There is no
user-facing core-type selector or extra CLI flag; GUI, CLI and suite workers
share the same runtime-checked default. Other hosts retain their existing
scheduler-managed or affinity-based paths.

```
gpu_benchmark --cpu-benchmark all --cpu-time 15 --cpu-warmup 0.2
gpu_benchmark --cpu-membw
```

Compute applies the requested duration separately to each selected type,
split into three rounds with a median score. `all` first measures the original
all-core stage, then P and E single-thread stages. Each type saves its own
`cpu_single_core` record, with distinct `_qos_bound_v1_performance` or
`_qos_bound_v1_efficiency` workload versions. It does not average P/E scores.

Memory sweeps read each type's own `hw.perflevelN` cache capacities. Every
classified sample uses at least 50 ms, two warm-up samples and seven measured
samples by default. The default includes the all-core memory stage, plus the existing
page-mapping and storage tests; those stages retain system scheduling. Records keep the
full P/E curves, per-class capacities and per-point residency lower bounds.
The residency point encoding also keeps the last accepted sample's effective
GHz and process P-time share; full per-sample observations are in the CLI log.

## Verification and timing

Only public SDK interfaces are used: `pthread_set_qos_class_self_np`,
`proc_pid_rusage(RUSAGE_INFO_V6)`, and `CLOCK_THREAD_CPUTIME_ID`. No root access,
private SPI, security changes or persistent process policies are needed.
The capability check requires native arm64 macOS, two performance levels named
Performance/Efficiency, and available V6 process accounting. iOS and Intel
retain the existing automatic path; iOS displays imported verified records,
but does not expose an unverified core-type run option.

Process counters use **Mach time**, whereas the thread CPU clock uses
nanoseconds. Convert cumulative counters with `mach_timebase_info` before
comparing their deltas. P and total conversions can differ by one nanosecond;
only a two-nanosecond overshoot is tolerated.

Read the worker CPU clock before and after each process snapshot. From those
readings, form lower/upper bounds on the worker CPU time. Assign *all* possible
other-thread CPU time against the requested type and include an additional
50 microseconds of accounting margin. The requested worker-type lower bound
must be at least 99%. This prevents an observer or UI thread from certifying
the worker's placement by consuming CPU on that type. Unavailable or materially
inconsistent accounting fails closed. Compute retries a rejected round twice;
memory retries a rejected measured sample four times, automatically doubling
only the rejected sample window (up to 16 times its initial length). This
allows enough actual CPU time to verify a preempted background thread without
lowering the 99% threshold. Actual/max sample lengths are recorded in the log
and result configuration. Repeated failure makes
the run invalid and prevents its affected result from being saved.

`CPU_RESIDENCY` and `CPUMEM_RESIDENCY` record process P-time fraction, the
conservative worker-type lower bound, and observed cycles / CPU-time in GHz.
The latter is an average over the measured policy context, not a fixed or
maximum clock. Background QoS can reduce E-core frequency; compare these scores
as background-policy results, not maximum-frequency E-core capability.
Existing fourteen automatic samples cannot be retrospectively assigned P/E
identities. Their records remain unchanged.

## Validation

The residency unit test rejects mixed-core observations, missing counters,
inconsistent counters, and an observer consuming all P time while the target
worker could have been entirely on E cores. A live negative control starts a
P-requesting worker under `taskpolicy -b`: it must fail and append no result.
Apple parser tests also reject invalid/unverified rows and keep both class
names and saved curve verification metadata.

Sources: [Apple QoS guidance](https://developer.apple.com/documentation/apple-silicon/tuning-your-code-s-performance-for-apple-silicon),
[XNU Recount](https://github.com/apple-oss-distributions/xnu/blob/main/doc/observability/recount.md),
[task accounting fields](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/bsd_kern.c).

## Virtual-memory coverage

The completed page test times first writes to fresh OS pages (first-touch page
mapping/allocation), with a 16 KiB page size on this Mac. The storage test uses
a temporary file on the drive backing /private/var/vm and measures sequential
and random 4 KiB I/O. It does not time actual swap-out / swap-in of application
pages, memory compression, TLB reach or virtual-memory remapping.
