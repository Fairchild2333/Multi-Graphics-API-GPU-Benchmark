# iPhone 13 Pro · API 补齐与完整实机测试

**2026-10-14 计时审计更正：任务通过仅表示流程完成与成绩保存，不保证测量有效。iPad Vulkan 1M／4M／16M 纯计算分数因计时异常排除；两机部分 Metal 绘制／流体计时暂定。详见[成绩比较与可信度审计](../apple-score-audit-20261014/comparison-audit.md)。以下原始分数保留用于追溯。**

**31/31 项通过，32 条保存记录。**原有手机历史 17 条均保留（逐字段核对：True）。

本轮范围：13 项 Metal、12 项 Vulkan（MoltenVK → Metal）、1 项 GPU 缓存状态诊断、CPU 算力和 CPU 缓存／内存／页面映射／存储，以及 OpenGL ES 2.0 旧版 GPU Burn 三档。CPU 算力分别保存单线程和全核成绩，因此 31 个任务产生 32 条记录。

iPhone 13 Pro 的 Vulkan 接口未提供流体所需的 R32_SFLOAT 线性过滤能力，流体从 Vulkan 清单排除；Metal 流体已实际测量。iOS 没有 ES 3.1 计算接口，所以原 Android 的 ES 3.1 粒子清单不在本机运行。OpenGL ES 2.0 是独立旧版着色器，不能与现代 GPU Burn 混排。

新版默认关闭转译 API。开启“允许转译 API（MoltenVK）”后，图形 API 可选 Vulkan，一键测试也会包含设备支持的 Vulkan 项目。这个开关控制执行权限；调用原生入口而未显式允许转译也会被拒绝。固定 1280 × 720，粒子／GPU Burn／流体每项 15 秒。ES 2.0 使用 glFinish 同步计时，其显示提交受屏幕刷新约束。所有移动端成绩单独保留 ios_preview 合约，Vulkan 另带 moltenvk 与实现标记。

首次 Vulkan 能力查询误请求了仅由 Vulkan loader 提供的 portability 枚举扩展，已按运行库实际提供的扩展修复。12 项 Vulkan 均已在真机跑完。第一次全尺寸缓存诊断通过，但第二轮的 128 MiB 单条预热命令触发系统 GPU watchdog；失败记录未当作成绩。iOS 已改成 8192 次读取一段的连续完整周期预热，每段验证索引和校验和。测量循环保持原样，版本更新为 metal_dependent_cache_v2_preview_ios_preview，避免与旧控制条件混排。修复后补跑通过，原始失败日志保留在 vulkan-attempt。

这份完整清单由可核验的续跑记录组成，保留此前通过的 Metal／Vulkan 测量，再补齐新诊断和剩余任务；各项的原始时间、温度状态和成绩 ID 全部可追溯。连续负载的温度可能影响分数，不能作为冷机峰值或严格等温 A/B 对比。

| 项目 | API | 成绩 | 温度状态（开始→结束） |
|---|---|---:|---|
| Metal · Particle 65536 · Render | Metal | 26.313 GB/s | 0→0 |
| Metal · Particle 65536 · Compute | Metal | 52.955 GB/s | 0→0 |
| Metal · Particle 1048576 · Render | Metal | 12.886 GB/s | 0→0 |
| Metal · Particle 1048576 · Compute | Metal | 20.555 GB/s | 0→0 |
| Metal · Particle 4194304 · Render | Metal | 7.498 GB/s | 0→0 |
| Metal · Particle 4194304 · Compute | Metal | 18.951 GB/s | 0→0 |
| Metal · Particle 16777216 · Render | Metal | 12.122 GB/s | 0→0 |
| Metal · Particle 16777216 · Compute | Metal | 18.983 GB/s | 0→0 |
| Metal · GPU Burn · 16 | Metal | 0.740 Gpix-step/s | 0→0 |
| Metal · GPU Burn · 64 | Metal | 0.788 Gpix-step/s | 0→0 |
| Metal · GPU Burn · 256 | Metal | 0.811 Gpix-step/s | 0→0 |
| Metal · Cinematic Liquid | Metal | 6.984 MParticle-step/s | 0→0 |
| Metal · GPU Cache & Memory | Metal | 30.917 GB/s | 0→0 |
| Vulkan (MoltenVK) · Particle 65536 · Render | Vulkan | 16.767 GB/s | 0→0 |
| Vulkan (MoltenVK) · Particle 65536 · Compute | Vulkan | 38.168 GB/s | 0→0 |
| Vulkan (MoltenVK) · Particle 1048576 · Render | Vulkan | 19.614 GB/s | 0→0 |
| Vulkan (MoltenVK) · Particle 1048576 · Compute | Vulkan | 21.188 GB/s | 0→0 |
| Vulkan (MoltenVK) · Particle 4194304 · Render | Vulkan | 19.085 GB/s | 0→0 |
| Vulkan (MoltenVK) · Particle 4194304 · Compute | Vulkan | 19.149 GB/s | 0→0 |
| Vulkan (MoltenVK) · Particle 16777216 · Render | Vulkan | 18.733 GB/s | 0→0 |
| Vulkan (MoltenVK) · Particle 16777216 · Compute | Vulkan | 19.027 GB/s | 0→0 |
| Vulkan · GPU Burn · 16 | Vulkan | 0.936 Gpix-step/s | 0→0 |
| Vulkan · GPU Burn · 64 | Vulkan | 1.074 Gpix-step/s | 0→0 |
| Vulkan · GPU Burn · 256 | Vulkan | 2.098 Gpix-step/s | 0→1 |
| Vulkan · GPU Cache & Memory | Vulkan | 29.298 GB/s | 1→1 |
| Metal · GPU Cache-state Diagnostic | Metal | 311.192 ns/load | 0→0 |
| CPU Compute · Single + All Cores · 单线程采样平均 | CPU | 147.268 MWork/s | 0→0 |
| CPU Compute · Single + All Cores · 全核 | CPU | 654.417 MWork/s | 0→0 |
| CPU Cache, Memory, Virtual Memory & Storage | CPU | 34.518 GB/s | 0→0 |
| OpenGL ES 2.0 · GPU Burn · 16 | OpenGL ES 2.0 | 1.458 Gpix-step/s | 0→1 |
| OpenGL ES 2.0 · GPU Burn · 64 | OpenGL ES 2.0 | 5.759 Gpix-step/s | 1→1 |
| OpenGL ES 2.0 · GPU Burn · 256 | OpenGL ES 2.0 | 15.179 Gpix-step/s | 1→0 |

## 缓存、内存、虚拟内存和存储

CPU 单线程内存读取 **35.1 GB/s**，全核读取 **34.5 GB/s**。页面映射首次触页 **21.12 GB/s**，每 16 KiB 页面 **776 ns**。这不是磁盘换页测速。1 GiB 临时文件顺序读／写 **1382／1655 MB/s**；4 KiB 随机 QD1 读／写 **59／30 MB/s**。

CPU 的单线程由系统调度，不能标成绑定到每个大核或小核。缓存诊断保存 GPU 预热、干扰读后、CPU 预热三个依赖读取延迟系列，以及一个并行四字节有效负载吞吐系列。ns/load 与 GB/s 分开保存，不能彼此混用。**目前仍不能把这些曲线独立归属到 GPU L1/L2/L3、SoC/SLC，也不能把 Metal 固定网格诊断直接当成已验证的 DRAM 峰值。**

![保存的内存与缓存曲线](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/out/test-results/ios-api-completion-20261014/memory-cache-curves.png>)

## 文件与验证

- `suite-results.json`：本套选取的 32 条保存记录。
- `final-phone/results/results.json`：完整手机原始历史。
- `final-phone/suite-runs/latest.json` 与逐轮日志：完整清单、续跑来源、温度和全部原生输出。
- `before-phone`：此次改动前的手机数据；`native-attempt`、`vulkan-attempt`：中间运行与失败日志。
- `curve-points.csv`、`memory-cache-curves.png/.pdf`：保存曲线的原始点与图。
- `verification.txt/.json`：用生产 Swift 模型与曲线解析器核对任务、API、合约、成绩及曲线恢复。
- iOS 18.5 模拟器桥接回归：11 项中 10 项通过、1 项因虚拟 GPU 无统一内存缓存控制而跳过；缓存诊断已改在真机验证。桌面兼容／任务／序列化测试 5/5 通过。

手机数据可从“文件 → 我的 iPhone → Mangekyo”浏览，或从 Mac Finder 的设备文件共享读取。GPU 和 CPU 均保存到 Documents/results/results.json；诊断日志在 Documents/suite-runs。
