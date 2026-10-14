# Verified CPU core-type tests on Apple Silicon macOS and iOS

The existing mixed compute and NEON read/write/copy kernels are unchanged.
Supported native Apple Silicon macOS and iOS hosts automatically run both performance and
efficiency single-thread stages. P stages use user-interactive QoS. iOS E compute
now uses user-initiated QoS with verified P-load helpers; macOS E compute and the
current P/E memory sweep retain their separately labelled background E policy.
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

The process-bound path uses public SDK interfaces: `pthread_set_qos_class_self_np`,
`proc_pid_rusage(RUSAGE_INFO_V6)` on macOS, `task_info(TASK_POWER_INFO_V2)` on iOS,
and `CLOCK_THREAD_CPUTIME_ID`. This path needs no private SPI. The new iOS
foreground E compute path also resolves Recount's exported `thread_selfcounts`
SPI at runtime to read per-thread P/E time and cycles. This is not a public SDK
API; it is validated on this signed development device, not an App Store
distribution claim. Neither method needs root access, security changes or
persistent process policies.
The capability check requires native arm64, two performance levels named
Performance/Efficiency, and the full accounting structure. iOS uses the public
`task_ptime` performance CPU-time counter and total user/system CPU time from
`TASK_POWER_INFO_V2`; macOS retains V6 accounting. Simulators, Intel and hosts
without verified capabilities retain the existing automatic path. The iOS
one-click CPU job expects P, E and all-core records when supported and rejects
duplicate types or unverified rows. Its current resume contract also rejects
background E records in place of the new performance-priority measurement.
No new user-facing selector is added.

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
Cycles are unavailable through the public iOS task API: `EffectiveGHz=0` on that
path means unavailable, not zero clock speed. The new per-thread SPI path does
report cycles and measured average GHz. On macOS the latter is an average over the measured policy context, not a fixed or
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

On iPhone 13 Pro / A15 (2026-10-14), live UI progress updates produced E
residency lower bounds around 97%, so those attempts were rejected. E-stage
progress is now buffered until the worker stops, and the observer uses
background QoS. P scored 147.699 MWork/s (minimum accepted residency 99.48%),
E scored 29.611 (minimum 99.9968%), and all-core scored 649.362, all at nominal
thermal state. The existing compute kernel and the 99% threshold are unchanged.
The confirmation round measured P 146.994, E 26.740, all-core 661.557. P/E
placement passed again; background E throughput varies with the policy context
and must not be interpreted as a fixed maximum clock. Classified memory
sweeps also buffer their detailed sample log until measurement completes, so
interactive GUI output cannot contaminate their short accounting windows.
Raw evidence is in `out/test-results/a15-cpu-pe-20261014`; earlier automatic
single-thread summaries remain in history and are not retroactively relabelled.

### A15 单线程与全核不能直接相加（2026-10-14）

此前 P 146.994、E 26.740 的简单加权和是
`2 × 146.994 + 4 × 26.740 = 400.948 MWork/s`，不是全核测得的
661.557。E 单线程明确设置 background QoS，全核线程按系统正常策略调度；
这两种测量的功耗／调度条件不同。99% 验证只证明核心类型，不能证明运行在
该类型的峰值频率。iOS 公共计数器没有可用的周期计数，本项目没有独立测出
本次 E 核频率，不能把这个差距直接说成某个确定的 GHz 数值。

增加每轮全核工作量及线程贡献后，原工作负载复测 P 147.029、后台 E
27.009、全核 642.251。选中的第 3 轮六个线程分别贡献 108.169、106.897、
106.650、106.770、107.699、106.066，未取整的总和等于 642.250805 MWork/s。
这些是可迁移线程的贡献，不能分别标成六个物理核心。公共进程计数器报告该轮 CPU 时间
28.108 秒，其中 P 类型 9.707 秒，其余类型约 18.401 秒；共同墙钟时间
5.000035 秒。单线程 E 三轮实际 CPU 时间 4.925／4.793／4.954 秒，也通过
99% 类型归属验证，因此不能主要用“5 秒里没有获得运行时间”解释低分。
全核的公共进程计数包含其他应用线程，仅作诊断，不用于给单个全核线程认证 P/E 类型。

GUI 与新成绩标注后台策略，小核成绩不当作峰值，也不用于预测全核加和。
`workerScores` 在 `workloadConfig` 保存选中一轮的贡献；所有原成绩保留。
原始证据和截图在 `out/test-results/a15-cpu-score-ui-20261014`。
Apple 的 [QoS 说明](https://developer.apple.com/documentation/dispatch/dispatchqos/qosclass-swift.enum)
说明优先级会影响执行与节能策略；不同政策影响本轮吞吐的判断不等于直接测量频率。

### iOS 性能优先级的小核算力（后续完成）

背景优先级的 26–30 分没有满足正常性能条件下的小核算力要求。新方法将 E 计分
线程设置为 user-initiated，用两个 user-interactive 辅助线程占用 A15 的 P 类型核心。
辅助线程只提供调度压力，工作量完全不进入 E 分数，也不进入此前已完成的全核分数。
每轮通过 `thread_selfcounts(THSC_TIME_CPI_PER_PERF_LEVEL)` 独立核对当前计分线程
P/E CPU 时间，E 驻留保守下界必须达到 99%，线程 CPU 占墙钟时间至少 90%；
两个辅助线程也必须通过至少 99% P 驻留验证。正常策略不能当作绑核保证。

接口来自 Apple 开源 [Recount 实现](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/sys_recount.c)
和 [ABI 定义](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/sys/resource_private.h)，
不是公开 SDK API。运行时缺失、计数倒退或计数总时间与独立线程 CPU 时钟不一致时
直接拒绝成绩。确认版本在两个快照外包一对 CPU 时钟读数，以 50us 边界容差
检查时间守恒；大核辅助线程亦接受该检查。

A15 两轮 E 分别 **89.658／89.551 MWork/s**，三轮归属下界均约 **99.999%**，
实测平均频率约 **2.016 GHz**。P 为 146.981／147.120，全核为 661.517／661.962，
所有热状态 0→0。后一次 `2P + 4E` 的独立单线程估算为 652.444，与全核相差约
1.44%；独立测量的时序、并发程度、初始状态仍不同，不能要求严格相等。
这是 P 负载并存时的一线程 E 算力，不是从总分反推，也不是六个线程的平均。
也不声称已经严格绑定到某一编号核心，或控制到固定最高主频。

新版成绩增加 `_foreground_e_p_load_v1` 和
`efficiencyPolicy=foreground_with_performance_load`，与旧背景策略分组。默认 iOS
CPU／整机按钮使用该路径，不增加设置；旧成绩原样保留，不能被续测当作新版小核成绩。
证据：`out/test-results/a15-efficiency-performance-20261014`。这一阶段的旧缓存／内存
曲线仍采用背景策略，应按各自字段解释；随后已完成下述性能优先级版本。

### 随后完成：A15 性能优先级小核缓存／内存

同一条默认 iOS 测法已用于 CPU 缓存／内存扫描。P 辅助线程复用算力路径的
生命周期和归属校验，辅助工作不计入小核带宽。全核读取 35.178 GB/s；小核
L1／L2／内存读取为 46.4／31.0／21.9 GB/s。186 个 P/E 点全部通过验证，小核
最低归属 99.8922%，实测频率约 2.015 GHz。页面首次写入及文件存储也已完成。
原 119 条历史保留，新增 `20261014-171607-442-cpu-membw`。

新结果保存 `efficiencyPolicy=foreground_with_performance_load`、独立 E 计数方法、
辅助线程数和归属证据；续测不接受旧背景／未分类曲线替代新测法。该方法的
未公开接口限制与上方算力测试一致。证据：
[A15 CPU 缓存／内存](../out/test-results/a15-complete-app-20261014/report.md)。
这套 CPU 分级测量不等于 GPU L2／L3 或 SoC/SLC 独立归属；后者仍未建立。

Sources: [Apple QoS guidance](https://developer.apple.com/documentation/apple-silicon/tuning-your-code-s-performance-for-apple-silicon),
[XNU Recount](https://github.com/apple-oss-distributions/xnu/blob/main/doc/observability/recount.md),
[task accounting fields](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/bsd_kern.c),
[public task structure](https://developer.apple.com/documentation/kernel/task_power_info_v2_data_t),
[XNU task power accounting](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/task.c).

## Virtual-memory coverage

The completed page test times first writes to fresh OS pages (first-touch page
mapping/allocation), with a 16 KiB page size on this Mac. The storage test uses
a temporary file on the drive backing /private/var/vm and measures sequential
and random 4 KiB I/O. It does not time actual swap-out / swap-in of application
pages, memory compression, TLB reach or virtual-memory remapping.
## A12X 2026-10-14 实测更新

A12X 的算力默认使用 Utility 小核、无辅助线程，正常频率约 1.572 GHz，60.100／60.013 MWork/s 两轮一致；背景策略约 0.54 GHz、20.419 MWork/s 单独保存。全核明确请求 4 个高优先级大核工作线程和 4 个 Utility 小核工作线程，三轮共 24 个样本验证类型驻留与执行占用，656.340 MWork/s；仍不能指定编号核心。新全核合同 `apple_pe_qos_workers_v1` 不复用旧继承优先级成绩。

A12X 的短内存窗口出现约 5 ms 进程／线程统计差异，单线程曲线改用线程分层统计，并保持 CPU 时钟守恒和 99% 归属门槛。Utility 内存突发没有满足占用条件；缓存／内存默认沿用已验证的后台策略，267 点曲线、页面和存储均已完成。A15 的前台＋大核辅助策略不变。见 [A12X 实测与条件](../out/test-results/a12x-targeted-retest-20261014/report.md)。
