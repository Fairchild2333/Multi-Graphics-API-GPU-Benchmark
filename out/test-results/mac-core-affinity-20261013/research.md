# Apple Silicon core-type scheduling research — 2026-10-13

Online source review and a disposable current-host verification. No benchmark engine changes or formal scores.

## Verified on this M4 Pro

- Normal user account, no root, no persistent system changes.
- Public SDK proc_pid_rusage(getpid(), RUSAGE_INFO_V6) succeeds and supplies total and performance-core CPU accounting.
- Single-threaded disposable CLI process; each policy has three 0.5 s windows. Before/after deltas cover the measured window.
- P share = delta(user_ptime + system_ptime) / delta(user_time + system_time). On this two-kind P/E system, E share = 1 - P share.
- These are CPU-time shares, not wall-time shares, numbered-core identities, guaranteed affinity, or new formal performance scores.

| Requested policy | P CPU-time share by round | E CPU-time share by round |
| --- | --- | --- |
| high-qos | 98.412%, 99.997%, 100.000% | 1.588%, 0.003%, 0.000% |
| background-qos | 0.000%, 0.000%, 0.000% | 100.000%, 100.000%, 100.000% |
| process-background | 0.000%, 0.000%, 0.000% | 100.000%, 100.000%, 100.000% |

High QoS first window includes 1.588% E time: requesting QoS alone is insufficient proof of placement. taskpolicy -b also kept a thread reporting user_interactive entirely on E cores, demonstrating the need to measure effective placement rather than trust qos_class_self().

## Feasible benchmark integration

Use separate QoS modes for single-thread compute and cache/memory sweeps. Capture accounting around every measured round, save requested policy plus measured P/E shares, and retry or mark mixed-core windows. Keep affinity=scheduler_managed; do not assign CPU IDs. Define new workload versions/configurations so these records remain distinct from old scheduler-managed samples. Process accounting is suitable for an isolated CLI worker; extra GUI/observer/worker CPU must not be misattributed to the measured thread. Background policy can influence energy/performance management, so record cycles/time and describe scores as measured under that policy rather than maximum-frequency E-core capability. No iOS runtime verification was performed.

## Binding findings

THREAD_AFFINITY_POLICY returned KERN_NOT_SUPPORTED in the earlier current-host probe. Apple ARM cpu_affinity.h returns zero affinity sets. Numbered-CPU and cluster-binding helpers in the research are guarded by DEVELOPMENT || DEBUG; the queried kern.sched_thread_bind_cpu, kern.sched_thread_bind_cluster_type, kern.sched_task_set_cluster_type, and kern.sched_thread_bind names are absent on this host. The underlying thread_bind function explicitly is not exported to user space. Running as root does not make an absent release-kernel interface appear.

## Sources

- Apple QoS guidance: https://developer.apple.com/documentation/apple-silicon/tuning-your-code-s-performance-for-apple-silicon
- Apple taskpolicy implementation: https://github.com/apple-oss-distributions/system_cmds/blob/main/taskpolicy/taskpolicy.c
- Apple Recount accounting: https://github.com/apple-oss-distributions/xnu/blob/main/doc/observability/recount.md
- SDK rusage layout: https://github.com/apple-oss-distributions/xnu/blob/main/bsd/sys/resource.h
- Accounting fields filled by the kernel: https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/bsd_kern.c
- Kernel-only binding and debug guards: https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/sched_prim.c
- ARM affinity sets: https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/arm/cpu_affinity.h

## Preservation

Formal results unchanged: SHA256 84a458ff354c261becb05ba345c12b40f5faef0c3d002bc70bb65eeddac07562 before and after. Existing 14 samples still lack verified P/E identities. Raw observations: residency-verification.json and three policy-specific JSONL logs.
