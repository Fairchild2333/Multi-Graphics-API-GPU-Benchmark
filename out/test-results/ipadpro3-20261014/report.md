# iPad Pro 12.9 英寸第三代 · 完整实机测试

**2026-10-14 计时审计更正：任务通过仅表示流程完成与成绩保存，不保证测量有效。iPad Vulkan 1M／4M／16M 纯计算分数因计时异常排除；两机部分 Metal 绘制／流体计时暂定。详见[成绩比较与可信度审计](../apple-score-audit-20261014/comparison-audit.md)。以下原始分数保留用于追溯。**

**31/31 项通过，32 条保存记录。**

真实硬件型号 iPad8,7，GPU Apple A12X GPU，系统 iOS 26.7.1，系统报告内存 3.66 GiB；CPU 报告 8 个物理核心和 8 个全核线程。设备昵称不用于硬件归属。安装前没有 Mangekyo，成绩从新库开始，原始历史共 32 条。最终清单的成绩可跨已记录的续跑，不能视为一次不间断等温运行。

本机支持清单为 31 项：Metal 13 项、Vulkan（MoltenVK → Metal）12 项、原生 GPU 缓存状态诊断 1 项、CPU 算力与缓存／内存／虚拟内存／存储 2 组，以及 OpenGL ES 2.0 旧版 GPU Burn 3 档。CPU 算力另存单线程和全核两条，合计 32 条成绩。

运行时能力查询未提供 Vulkan 流体所需的 R32_SFLOAT 线性过滤，Vulkan 流体不进入队列；Metal 流体已运行。iOS 没有 ES 3.1 计算接口，对应 Android 粒子测试没有进入队列。ES 2.0 三档使用原有旧版着色器，不能与现代 GPU Burn 混排。

本轮以新版一键测试流程启动，缓存 v2 在 48 MiB 并行阶段两次发生 GPU 恢复，失败日志保留；其余 30 项一次通过并原样沿用，只补测新版本的缓存诊断。Vulkan 转译显式开启。粒子、GPU Burn 和流体每项 15 秒（含预热），固定 1280 × 720，默认关闭垂直同步；ES 2.0 的呈现受显示刷新约束，使用 glFinish 同步计时。CPU 默认测试全核，并依次做由系统调度的单线程采样；每项测量 15 秒、预热 0.2 秒、分 3 轮。采样不是绑定到每个大核／小核。所有移动端成绩保留 ios_preview，转译结果保留 moltenvk 和实现标记。

各项开始／结束的系统温度状态均记录在清单；本轮结束状态出现 [0, 1]（0=正常，1=较热，2=较高，3=严重）。连续负载或不同设备散热会影响分数，本报告不是严格等温 A/B 测试。

| 项目 | API | 成绩 | 温度（开始→结束） |
|---|---|---:|---|
| Metal · Particle 65536 · Render | Metal | 23.338 GB/s | 0→0 |
| Metal · Particle 65536 · Compute | Metal | 32.845 GB/s | 0→0 |
| Metal · Particle 1048576 · Render | Metal | 10.566 GB/s | 0→0 |
| Metal · Particle 1048576 · Compute | Metal | 30.186 GB/s | 0→0 |
| Metal · Particle 4194304 · Render | Metal | 11.501 GB/s | 0→0 |
| Metal · Particle 4194304 · Compute | Metal | 29.678 GB/s | 0→0 |
| Metal · Particle 16777216 · Render | Metal | 23.404 GB/s | 0→0 |
| Metal · Particle 16777216 · Compute | Metal | 29.502 GB/s | 0→0 |
| Metal · GPU Burn · 16 | Metal | 0.923 Gpix-step/s | 0→0 |
| Metal · GPU Burn · 64 | Metal | 1.023 Gpix-step/s | 0→0 |
| Metal · GPU Burn · 256 | Metal | 0.808 Gpix-step/s | 0→0 |
| Metal · Cinematic Liquid | Metal | 3.870 MParticle-step/s | 0→0 |
| Metal · GPU Cache & Memory | Metal | 59.904 GB/s | 0→0 |
| Vulkan (MoltenVK) · Particle 65536 · Render | Vulkan | 2.287 GB/s | 0→0 |
| Vulkan (MoltenVK) · Particle 65536 · Compute | Vulkan | 23.373 GB/s | 0→0 |
| Vulkan (MoltenVK) · Particle 1048576 · Render | Vulkan | 22.833 GB/s | 0→0 |
| Vulkan (MoltenVK) · Particle 1048576 · Compute | Vulkan | 1701.091 GB/s（计时异常） | 0→0 |
| Vulkan (MoltenVK) · Particle 4194304 · Render | Vulkan | 27.392 GB/s | 0→0 |
| Vulkan (MoltenVK) · Particle 4194304 · Compute | Vulkan | 1290.151 GB/s（计时异常） | 0→0 |
| Vulkan (MoltenVK) · Particle 16777216 · Render | Vulkan | 27.497 GB/s | 0→0 |
| Vulkan (MoltenVK) · Particle 16777216 · Compute | Vulkan | 619.881 GB/s（计时异常） | 0→0 |
| Vulkan · GPU Burn · 16 | Vulkan | 0.668 Gpix-step/s | 0→0 |
| Vulkan · GPU Burn · 64 | Vulkan | 0.847 Gpix-step/s | 0→0 |
| Vulkan · GPU Burn · 256 | Vulkan | 0.884 Gpix-step/s | 0→0 |
| Vulkan · GPU Cache & Memory | Vulkan | 56.760 GB/s | 0→0 |
| Metal · GPU Cache-state Diagnostic | Metal | 356.150 ns/load | 0→0 |
| CPU Compute · Single + All Cores · 单线程采样平均 | CPU | 109.415 MWork/s | 1→1 |
| CPU Compute · Single + All Cores · 全核 | CPU | 491.864 MWork/s | 1→1 |
| CPU Cache, Memory, Virtual Memory & Storage | CPU | 51.204 GB/s | 1→1 |
| OpenGL ES 2.0 · GPU Burn · 16 | OpenGL ES 2.0 | 3.501 Gpix-step/s | 1→1 |
| OpenGL ES 2.0 · GPU Burn · 64 | OpenGL ES 2.0 | 9.675 Gpix-step/s | 1→1 |
| OpenGL ES 2.0 · GPU Burn · 256 | OpenGL ES 2.0 | 15.379 Gpix-step/s | 1→1 |

## CPU 缓存、内存、页面映射和存储

| 模式 | 层级／区间 | 读取 GB/s | 写入 GB/s | 复制 GB/s |
|---|---|---:|---:|---:|
| 单线程 | L1 | 78.9 | 78.7 | 114.3 |
| 单线程 | L2 | 52.3 | 52.0 | 63.6 |
| 单线程 | memory | 42.1 | 19.3 | 32.6 |
| 全核 | L1 | 342.8 | 324.3 | 465.0 |
| 全核 | L2 | 236.1 | 217.8 | 274.3 |
| 全核 | memory | 51.2 | 27.9 | 34.5 |

缓存容量来自系统报告，单线程由系统调度，不能将同一组成绩独立归给某个大核或小核。单线程最大数据集 256 MiB；全核曲线横轴表示每线程工作集。内存平台标志为单线程 `1`、全核 `1`。

页面映射首次触页 **13.74 GB/s**，每 16 KiB 页面 **1192 ns**；这不是磁盘换页测速。1 GiB 临时文件顺序读／写 **1253／1541 MB/s**，4 KiB 随机 QD1 读／写 **29／21 MB/s**。

## 保存的 GPU／CPU 曲线

已保存并用生产解析器恢复：CPU 174 点、Metal 内存 52 点、Vulkan 内存 77 点、GPU 缓存状态 124 点。缓存 v3 延续每段 8192 次读取的连续完整周期预热，每段校验索引与校验和。A12X 的并行测量改为每条链 512 次读取（4096 条链），与 iPhone 的 v2／8192 次配置单独分组。三种串行延迟测量仍为每条链 65536 次读取。

**GPU L1/L2/L3、SoC/SLC 仍未独立归属。**依赖读取延迟单位 ns/load，4096 条并行链的四字节有效负载吞吐单位 GB/s；它们不能混用，也不等同于顺序 DRAM 带宽。Metal 固定网格与 Vulkan 顺序读写是不同诊断方法，不能单靠两个分数推导转译开销。

![内存与缓存曲线](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/out/test-results/ipadpro3-20261014/memory-cache-curves.png>)

## 与上一台 iPhone 13 Pro 对照

此表仅记录原始对应关系，除明确排除项外，其余比值仍须结合计时审计解释。仅按相同测试项、API、版本和单位对应；缓存 v3 与 iPhone v2 的配置不同，未做比值比较。Vulkan 65K 渲染的计时方式版本也不同，未做比值比较。CPU 全核分别使用本机 8 线程和 iPhone 的 6 线程。比值为 iPad/iPhone，吞吐类越大越高，ns/load 延迟类越小越低。不跨 API 或不同诊断方法合并总分。

| 项目 | API | iPad | iPhone 13 Pro | 比值 |
|---|---|---:|---:|---:|
| Metal · Particle 65536 · Render | Metal | 23.338 GB/s | 26.313 GB/s | 0.89× |
| Metal · Particle 65536 · Compute | Metal | 32.845 GB/s | 52.955 GB/s | 0.62× |
| Metal · Particle 1048576 · Render | Metal | 10.566 GB/s | 12.886 GB/s | 0.82× |
| Metal · Particle 1048576 · Compute | Metal | 30.186 GB/s | 20.555 GB/s | 1.47× |
| Metal · Particle 4194304 · Render | Metal | 11.501 GB/s | 7.498 GB/s | 1.53× |
| Metal · Particle 4194304 · Compute | Metal | 29.678 GB/s | 18.951 GB/s | 1.57× |
| Metal · Particle 16777216 · Render | Metal | 23.404 GB/s | 12.122 GB/s | 1.93× |
| Metal · Particle 16777216 · Compute | Metal | 29.502 GB/s | 18.983 GB/s | 1.55× |
| Metal · GPU Burn · 16 | Metal | 0.923 Gpix-step/s | 0.740 Gpix-step/s | 1.25× |
| Metal · GPU Burn · 64 | Metal | 1.023 Gpix-step/s | 0.788 Gpix-step/s | 1.30× |
| Metal · GPU Burn · 256 | Metal | 0.808 Gpix-step/s | 0.811 Gpix-step/s | 1.00× |
| Metal · Cinematic Liquid | Metal | 3.870 MParticle-step/s | 6.984 MParticle-step/s | 0.55× |
| Metal · GPU Cache & Memory | Metal | 59.904 GB/s | 30.917 GB/s | 1.94× |
| Vulkan (MoltenVK) · Particle 65536 · Compute | Vulkan | 23.373 GB/s | 38.168 GB/s | 0.61× |
| Vulkan (MoltenVK) · Particle 1048576 · Render | Vulkan | 22.833 GB/s | 19.614 GB/s | 1.16× |
| Vulkan (MoltenVK) · Particle 1048576 · Compute | Vulkan | 1701.091 GB/s（计时异常） | 21.188 GB/s | 排除：计时异常 |
| Vulkan (MoltenVK) · Particle 4194304 · Render | Vulkan | 27.392 GB/s | 19.085 GB/s | 1.44× |
| Vulkan (MoltenVK) · Particle 4194304 · Compute | Vulkan | 1290.151 GB/s（计时异常） | 19.149 GB/s | 排除：计时异常 |
| Vulkan (MoltenVK) · Particle 16777216 · Render | Vulkan | 27.497 GB/s | 18.733 GB/s | 1.47× |
| Vulkan (MoltenVK) · Particle 16777216 · Compute | Vulkan | 619.881 GB/s（计时异常） | 19.027 GB/s | 排除：计时异常 |
| Vulkan · GPU Burn · 16 | Vulkan | 0.668 Gpix-step/s | 0.936 Gpix-step/s | 0.71× |
| Vulkan · GPU Burn · 64 | Vulkan | 0.847 Gpix-step/s | 1.074 Gpix-step/s | 0.79× |
| Vulkan · GPU Burn · 256 | Vulkan | 0.884 Gpix-step/s | 2.098 Gpix-step/s | 0.42× |
| Vulkan · GPU Cache & Memory | Vulkan | 56.760 GB/s | 29.298 GB/s | 1.94× |
| CPU Compute · Single + All Cores · 单线程 | CPU | 109.415 MWork/s | 147.268 MWork/s | 0.74× |
| CPU Compute · Single + All Cores · 全核 | CPU | 491.864 MWork/s | 654.417 MWork/s | 0.75× |
| CPU Cache, Memory, Virtual Memory & Storage | CPU | 51.204 GB/s | 34.518 GB/s | 1.48× |
| OpenGL ES 2.0 · GPU Burn · 16 | OpenGL ES 2.0 | 3.501 Gpix-step/s | 1.458 Gpix-step/s | 2.40× |
| OpenGL ES 2.0 · GPU Burn · 64 | OpenGL ES 2.0 | 9.675 Gpix-step/s | 5.759 Gpix-step/s | 1.68× |
| OpenGL ES 2.0 · GPU Burn · 256 | OpenGL ES 2.0 | 15.379 Gpix-step/s | 15.179 Gpix-step/s | 1.01× |

## 文件和核验

- `suite-results.json`：本套选取的 32 条成绩。
- `final-phone/results/results.json`：iPad 原始完整历史。
- `final-phone/suite-runs/latest.json` 与对应日志：清单、成绩 ID、时间、温度及原生输出。
- `first-run`、`attempt-1`、`retry-1`：保留原始运行及失败日志。
- `verification.txt/.json`：生产 Swift 模型核对工作量、API、合约及全部曲线恢复。
- `curve-points.csv` 和 `memory-cache-curves.png/.pdf`：保存的原始曲线点与图。
- `iphone-comparison.json`：与上一台 iPhone 相同项目的对应成绩及 ID。

成绩保留在 iPad 的“文件 → 我的 iPad → Mangekyo → results/results.json”，也已取回 Mac 单独归档。原 iPhone 与 Mac 的成绩不导入、不重写。
