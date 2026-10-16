# iPhone 13 Pro 与 iPad Pro A12X：成绩比较与可信度审计

**后续更新：异常项已完成定向复测（iPad 11项、iPhone 7项，另有4项降温补测）。请以[复测报告](../apple-targeted-retest-20261014/report.md)中的新成绩为准；以下保留首次审计证据。**

**有明确异常。iPad 的 Vulkan 1M／4M／16M 纯计算三条带宽分数应排除，不能解释为 32–80 倍的性能提升。**此前“31/31通过、32条有效”只验证任务完成、正数、版本和保存；没有验证每条计时是否覆盖真实工作。该描述过强，现更正为“31项完成、32条保存记录，其中有异常指标需重测”。原始 JSON 和日志原样保留。

目前较适合参考的同项观察如下。这些是单轮、非等温结果；CPU 单线程是调度采样，全核分别为 iPhone 6 线程、iPad 8 线程。

| 项目 | iPhone 13 Pro / A15 | iPad Pro / A12X | iPad/iPhone |
|---|---:|---:|---:|
| CPU 单线程采样平均 | 147.268 MWork/s | 109.415 MWork/s | 0.74× |
| CPU 全核 | 654.417 MWork/s | 491.864 MWork/s | 0.75× |
| CPU 内存全核读取 | 34.518 GB/s | 51.204 GB/s | 1.48× |
| GPU 顺序内存扫描 | 29.298 GB/s | 56.760 GB/s | 1.94× |
| Metal 粒子 16M 纯计算 | 18.983 GB/s | 29.502 GB/s | 1.55× |

这组数据里，iPad 的 CPU 算力约为 iPhone 的 74–75%；CPU 内存读高约48%，GPU顺序内存读约1.94倍。Metal大工作集纯计算约高55%。不能据此合并成一个“GPU整体强多少”的结论。

## 1. 明确异常：iPad Vulkan 纯计算时间戳

| 粒子数 | 报告分数 GB/s | GPU 计算时间 ms | 实际帧间隔 ms | 帧率交叉核对 GB/s |
|---|---:|---:|---:|---:|
| 1,048,576 | 1701.091 | 0.025 | 1.473 | 28.479 |
| 4,194,304 | 1290.151 | 0.130 | 5.855 | 28.655 |
| 16,777,216 | 619.881 | 1.083 | 23.256 | 28.857 |

交叉核对只按“每粒子40字节 × 粒子数 × 实际FPS”计算，用来检验数量级，**不是替换后的正式GPU成绩**。三档实际约28.5–28.9 GB/s，与同机Metal约29.5–30.2 GB/s接近；GPU查询却只覆盖帧间隔的一小部分，分数被放大约21–60倍。日志连续十余秒都显示该矛盾，不能用缓存加速或偶然波动解释。

本地 VulkanBackend 的 CheckTimestampsAgainstWallClock 明确跳过 headless，只校验窗口粒子路径；CollectTimestampResults仅检查空／非正间隔，没有对这些非零但严重偏小的间隔做外部校验。因此现有成功门槛漏掉了这三条。具体驱动／采样路径还需GPU trace或同步计时重测确认，不能直接断言GPU芯片故障。

GPU顺序内存扫描是另一条代码路径，带有工作量放大和独立提交／等待校验。iPad日志中4096 MiB读取GPU计时71.539 ms、墙钟72.358 ms；iPhone139.944／140.817 ms，两者接近，且报告timestampAnomalies=0。它们没有出现上述数十倍矛盾，因此56.760／29.298 GB/s这组比值更值得参考。

## 2. 可疑口径：Metal 绘制间隔与“利用率”

iPhone Metal GPU Burn 三档的 avgTotalGpuMs × FPS 为1.91、1.98、1.98；iPad重档为1.87。界面相当于显示190%–200%“利用率”，它不是实际GPU忙碌百分比。本地实现用命令缓冲开始／结束跨度，随后把跨度与帧间隔相除。并发提交、重叠或等待可能造成这些区间不等同于互斥执行时间；具体贡献尚未由trace拆分。

Metal流体也有重叠迹象：iPad compute358.243 + render471.078 ms，但total471.141 ms；iPhone174.623 +284.895 ms，但total285.005 ms。当前分数用compute+render作分母，需要核查重复计时。流体实际FPS为iPhone3.5、iPad2.1，可作为此次流程的观测；MParticle-step/s先不作严格硬件比较。

iPhone Vulkan GPU Burn256报2.098，Metal报0.811 Gpix-step/s，但实际FPS反而为Vulkan1.7、Metal3.4。GPU内核时间与呈现时间可能不同，但这组矛盾意味着不能直接宣称Vulkan比Metal快2.6倍。两种API的绘制计时均应做单帧、同步以及多帧校验。

## 3. ES2轻档：刷新／同步影响

ES2 16步时，iPad117.9 FPS／3.501 Gpix-step/s，iPhone49.3 FPS／1.458 Gpix-step/s，分数比2.40与FPS比2.39接近。64步两机各为81.6与48.7 FPS；256步变为32.5与32.1 FPS，分数也几乎相同。低档成绩明显受呈现／同步节奏影响，不能把2.4倍直接解释为纯GPU算力。ES2使用独立旧版着色器，也不能与Metal/Vulkan现代GPU Burn数值混排。

## 4. 看起来奇怪但已有解释的项目

- 大工作集粒子分数比小档更高：绘制版分数只统计计算阶段的GB/s，渲染又会改变争用与占空比；16M分数高不代表画面更流畅。应同时看FPS、计算时间和渲染时间。
- GPU缓存小工作集的几百GB/s不等于DRAM速度；缓存曲线的ns/load又是另一单位。iPad缓存v3每链512次、iPhonev2每链8192次，配置不同，不作两机比值。
- 单线程采样全部相近不是大小核一样快：iPad八次为108.4–109.8，iPhone六次为147.1–147.4 MWork/s。它们由系统调度，没有绑到八／六个不同物理核心。日志中的residency_verified=1同时带requested_lower_bound=0，不能当作P/E驻留证明。
- iPad CPU内存读取51.2、Vulkan内存56.8、Metal诊断59.9 GB/s在同一量级；不同读写／统计方法会有差异。Metal诊断仍标记memoryPlateauVerified=0，不应叫硬件标称带宽。

## 保存与结论限制

原始64条选定记录、原日志和已有结果文件都未改写。score-audit.json逐条标出需要排除、暂定或不可直接比较的原因；crosscheck仅辅助检验，不冒充复测成绩。优先需要修复并复测的项目：iPad Vulkan纯计算1M/4M/16M；两机Metal绘制／流体计时；iPhone Vulkan重档与刷新限制。CPU和独立内存扫描可暂用于比较，但不是经过多轮冷热控制的最终硬件定论。

MoltenVK的具体硬件／仿真时间戳路径可查其[固定1.4.2源码](https://github.com/KhronosGroup/MoltenVK/blob/v1.4.2/MoltenVK/MoltenVK/GPUObjects/MVKQueryPool.mm)。源码存在无计数缓冲时的回退路径；这不能单凭代码就证明本次用了该路径。其[官方时间戳说明](https://github.com/KhronosGroup/MoltenVK/blob/v1.4.2/Docs/MoltenVK_Runtime_UserGuide.md#timestamping)也不能替代本次外部计时校验。
