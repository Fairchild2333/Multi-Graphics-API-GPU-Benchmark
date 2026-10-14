# 缓存归属确认与设备独立测试（2026-10-14）

最新决定：按用户要求，与安卓／Windows 等平台保持现有测量口径，不再追查独立 GPU L2／L3／SoC-SLC 归属，也不将它列为 A15 当前范围的漏测或验收阻塞。CPU 保留系统缓存信息／曲线推断的分层汇总；GPU 保留已有缓存平台和访问曲线，未归属标记保持。独立归属研究本身未完成，下文方法只作存档。

A12X 后续补测已完成，新 Vulkan 内存实测 57.903 GB/s，数据校验通过；MoltenVK 从扫描开始就选择同步墙钟，避免中途混合来源。独立缓存归属范围保持上述决定不变。见 [A12X 报告](../out/test-results/a12x-targeted-retest-20261014/report.md)。

用户本轮选择先查测法和代码，暂不接回 A15。没有新增手机成绩。正常 CPU／GPU 全套、内存曲线、页面首次访问、文件存储测试均在设备 App 内执行并保存 `results/results.json`，电脑用于开发安装、收集归档和目前的外部硬件诊断。GPU L2／L3／SoC-SLC 的独立速度尚不能算作已支持的本机正式分数。

## 当前确认到了什么

- A15 公开 Metal 计数器清单只有 `timestamp/GPUTimestamp`。之前已完成的原生访问、预热、驱逐量和首次／重访对照是真实测量，曲线有校验；没有单独的缓存命中／分层流量计数。
- 外部 Instruments 的 GPU LLC 百分比、MMU 和 GPU 外部带宽已与实际目标访问关联，但不能把 LLC 百分比换算成缓存峰值 GB/s，也不能把 GPU 外部访问全部算作 DRAM。
- 现有 shader 测依赖读取的区间，另外有并发读取的逻辑载荷吞吐；它们都不是天然独立的缓存层级最大速度。ALU 运算预处理会明显改变大工作集表现，说明频率、时间间隔或内存工作状态仍是混杂因素。
- 没有证据证明本设备存在一层与 GPU L2、SoC/SLC 各自独立的“GPU L3”。缓存层级命名必须按芯片实际结构确认，不能因为界面要三个名字而建立三个分数。

Apple 的资料使用 GPU 本地缓存、GPU last-level cache 等名称，软件应查询设备真实开放的计数器：[GPU 缓存指标说明](https://developer.apple.com/videos/play/wwdc2020/10603/)、[设备计数器支持查询](https://developer.apple.com/documentation/metal/confirming-which-counters-and-counter-sets-a-gpu-supports)。

研究者的架构数据表将 A15 SLC 列为 32 MB，GPU L2 栏未填写；这只是外部参考，不能拿 32 MB 写成本 App 的实测容量或速度。[Metal Benchmarks 原作者数据](https://github.com/philipturner/metal-benchmarks/blob/main/README.md)。

## 存档的确认方法（暂不执行）

继续重复现有扫描不能独立确认共享缓存。下一轮应增加 **GPU 准备数据 → CPU 第一次读取** 的反向对照，并验证 CPU／GPU 私有缓存的排除效果。每个条件独立重新准备目标，避免在同一次试验里先读 GPU、再读 CPU，把第一个探针造成的预热算进第二个。

| 条件 | 需要核对的现象 | 可支持的结论与限制 |
| --- | --- | --- |
| CPU 预热，CPU 再读 | 与本机已经确认的 CPU L1／L2 曲线比较 | 提供 CPU 私有缓存参考区间 |
| GPU 预热，CPU 首读 | CPU 从未在准备后读取目标；线程 P/E 驻留验证，记录 CPU 时间／周期 | 检验 GPU→CPU 的数据保持，尚不能单独排除一致性转发 |
| GPU 预热，排除 GPU 私有缓存，CPU 首读 | 与独立 GPU 探针中的驱逐效果对应，同时排除 CPU 私有缓存影响 | 支持 CPU／GPU 共享层的解释；单次读取小缓冲不能保证已经刷新私有缓存 |
| 目标预热后逐级加大无关访问量 | 条件顺序随机化／轮转，跨轮复现，记录系统热状态和工作状态 | 检查保持区间与容量转折，不能直接把驱逐量当缓存容量 |
| 相同地址页、相同访问数的地址转换对照 | 地址跨度、页面数、实际访问字节数分别控制 | 排除将 TLB／页面转换转折误认成缓存层级 |

已有 M1 研究使用 CPU 私有缓存驱逐和 CPU／GPU 交叉访问来研究 SLC；还发现普通大缓冲访问可能混合私有缓存与共享缓存。其特殊地址步长和缓存映射规律是在 M1 上验证的，不能直接给 A15 使用同一阈值或命名结果。[EXAM 原始论文](https://arxiv.org/html/2504.13385v1)。

验收要求：校验所有目标访问，首次探针不重访；验证私有缓存驱逐效果而非只填写缓冲大小；线程类型、页面转换和工作状态的对照都必须通过。得到分层流量／命中计数，或足以排除替代解释的交叉对照之后，才允许把结果提升为有归属证据的层级速度。否则保留 `cacheAttribution=unverified`，展示原始曲线。

## 安卓／鸿蒙的 Vulkan

两个 App 都通过本机 Vulkan 驱动运行共享 C++ 实现。不能根据 Apple 的转译结果确认手机厂商驱动的性能。当前没有连接安卓／鸿蒙真机，本轮完成代码修复、打包和软件验证，真实 GPU 性能验收仍待连接设备。

已修复：

1. 读取校验无法执行或无法回读时，不再继续保存未经校验的成绩。
2. 计算写入、计算读取、传输和 CPU 回读之间补齐显式内存依赖；错误标记的多工作组写入改为原子操作。
   写入／复制的每块数据改由一个工作组负责，重复访问在该工作组内完成，消除不同工作组写同一位置的竞争。
   读写采用 volatile 内存访问约束，保留要求执行的重复读写；它不能独立证明命中了哪层硬件缓存。
3. 每个写入／复制工作集在计时之后检查实际内容，包括复制目标。
4. 内存测试先检查计时随负载增加的变化，再检查每个计时区间。预先发现不可靠时间戳可整轮使用同步墙钟；中途出现异常则整轮无有效成绩，避免同一曲线混合计时来源。
5. GPU Burn 启动核验扩展到安卓／鸿蒙。渲染测试正式计分后改变计时来源，会在清理资源后拒绝保存混合成绩。
6. 新的 `membw_v2_verified_*` 合同保存数据校验、写入／复制检查次数、计时来源和缓存归属状态；旧成绩保留。未到内存平台段时，明确保存为最大工作集的测量，不标作已确认的内存平台段。

同步依据：[Khronos 计算／传输／回读同步示例](https://docs.vulkan.org/guide/latest/synchronization_examples.html)。
写入相同值也仍然属于写操作，跨执行单元未排序的写入需要排除竞争：
[Vulkan 内存模型](https://docs.vulkan.org/spec/latest/appendices/memorymodel.html)。
访问约束依据：[GLSL 内存限定符](https://docs.vulkan.org/glsl/latest/chapters/variables.html)。

Vulkan 带宽数字计算的是 shader 请求的载荷字节数／计时区间，不是硬件总线或缓存线的直接流量计数。
小工作集的写入／复制现在使用较少的独占工作组，曲线还受占用率影响；不能仅凭曲线估算硬件层级。
目前名为 L1 的私有工作组测试也只是归属推断，16 KiB 的最低工作集不覆盖所有可能更小的缓存。

App 还会自行查询驱动是否公布 `VK_KHR_performance_query` 及其计数器名称，并保存 `performanceQueryAdvertised`、`performanceCounterStatus`、`performanceCounterInventory`。`enumerated_not_sampled` 只表示发现了计数器，**不表示已经采样**。该扩展不是所有 Vulkan 驱动都提供，开放哪些计数器也由驱动决定。[Khronos 扩展定义](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_performance_query.html)。

因此，正常成绩不依赖电脑计算或抓取。更深的独立缓存归属是否能完全在 App 里完成，要根据设备实际开放的接口及验证结果决定，当前不能承诺所有设备都会有独立 L2／L3／SLC 数值。

本轮验证与软件包见 [Vulkan 代码核查报告](../out/test-results/cache-attribution-vulkan-audit-20261014/report.md)。
