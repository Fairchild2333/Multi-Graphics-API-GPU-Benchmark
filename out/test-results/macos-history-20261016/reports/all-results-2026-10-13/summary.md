# 所有结果汇总（2026-10-13）

## 单线程成绩的含义

这批单线程成绩没有分别测量大核和小核。Mac 实现没有执行逐核绑定，14 条采样全部是 `scheduler_managed`、`Unclassified`、`performance_level=-1`。编号只是采样序号。系统可以反复选择同类核心，因此相近分数不能说明大小核性能相同。

正式单线程均值是 **322.3 MWork/s**，14 次采样范围 313.4–324.4；它可作为当前系统调度下的单线程吞吐。正式 14 线程总吞吐 **3726.7 MWork/s**。当前没有经过核心归属验证的 P 核/E 核各自成绩。
[实现依据](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/src/cpu_benchmark.cpp:428>)；[原始单线程日志](</Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/out/test-results/cache-cpu-20261013/cpu-compute-all.log>)；[Apple 调度说明](https://developer.apple.com/documentation/apple-silicon/tuning-your-code-s-performance-for-apple-silicon)。

## 数据范围

共 **367 条不同 ID**：当前 Mac 主成绩 **133**，仓库旧成绩 **220**，另有隔离验证用成绩 **14**。两个主历史文件合计 **353**。备份、精选列表和前后快照都是副本，不重复统计。所有原始成绩文件保持不变；本目录是独立只读汇总导出。

| 来源 | 条数 | 用途 |
|---|---:|---|
| 当前 Mac 主成绩 | 133 | 主历史 |
| 仓库旧成绩 | 220 | 主历史 |
| 隔离验证：Apple 适配 | 7 | 隔离验证，不能并入正式排名 |
| 隔离验证：API 流程 | 4 | 隔离验证，不能并入正式排名 |
| 隔离验证：OpenGL | 3 | 隔离验证，不能并入正式排名 |

| API | 条数 |
|---|---:|
| CPU | 15 |
| DX11 | 62 |
| DX12 | 59 |
| Metal | 109 |
| OpenGL | 54 |
| Vulkan | 68 |

| 负载 | 条数 |
|---|---:|
| cinematic_liquid | 11 |
| cpu_membw | 4 |
| cpu_multi_core | 6 |
| cpu_single_core | 5 |
| gpu_burn | 32 |
| gpu_cache | 3 |
| membw | 4 |
| stream | 302 |

## 当前 M4 Pro 主要成绩

下表保留 API、模式、粒子档位和版本；Vulkan 带渲染采用修正后的 1280×720 组。流体两种实现有不同计分合同，内存预览也不能与正式扫描直接排名。粒子 GB/s 是内核推算吞吐，并非统一内存实测。

| 项目 | Metal | Vulkan（MoltenVK） | OpenGL | 单位 |
|---|---:|---:|---:|---|
| 粒子 65K · 带渲染 | 151.353 | 50.542 | — | GB/s（推算） |
| 粒子 65K · 纯计算 | 310.199 | 239.127 | — | GB/s（推算） |
| 粒子 1M · 带渲染 | 52.401 | 85.926 | — | GB/s（推算） |
| 粒子 1M · 纯计算 | 146.696 | 219.681 | — | GB/s（推算） |
| 粒子 4M · 带渲染 | 54.918 | 132.732 | — | GB/s（推算） |
| 粒子 4M · 纯计算 | 142.220 | 145.311 | — | GB/s（推算） |
| 粒子 16M · 带渲染 | 57.878 | 127.469 | — | GB/s（推算） |
| 粒子 16M · 纯计算 | 143.277 | 133.438 | — | GB/s（推算） |
| GPU Burn 16 | 4.421 | 5.879 | 5.243 | Gpix-step/s |
| GPU Burn 64 | 4.517 | 6.460 | 5.967 | Gpix-step/s |
| GPU Burn 256 | 4.386 | 6.888 | 5.924 | Gpix-step/s |
| 水池 · 不同计分合同 | 36.987 | 67.361 | — | MParticle-step/s |
| GPU 内存读 · 不同测量方法 | 244.776（预览） | 252.395 | — | GB/s |

CPU：正式单线程采样 **322.3 MWork/s**；14 线程 **3726.7 MWork/s**；全核内存读/写/复制 **247.4 / 223.5 / 231.1 GB/s**。
CPU 单线程参考缓存读：L1 131.4、L2 111.5 GB/s；它们仍不代表已区分的 P/E 核各自缓存速度。
Vulkan 缓存命中段读 **1668.9 GB/s**；局部/L1 型全 GPU **1439.3**、单工作组 **125.1 GB/s**。不能据此独立确认 L2/L3/SLC。
[CPU 与 GPU 缓存完整曲线报告](</Users/fairchild/Library/Application Support/GpuComputeBenchmark/reports/cache-cpu-complete-2026-10-13.md>)。

## 全部成绩与原始条件

[可筛选的全部成绩表](</Users/fairchild/Library/Application Support/GpuComputeBenchmark/reports/all-results-2026-10-13/all-results.html>) · [完整 CSV](</Users/fairchild/Library/Application Support/GpuComputeBenchmark/reports/all-results-2026-10-13/all-results.csv>) · [367 条完整 JSON](</Users/fairchild/Library/Application Support/GpuComputeBenchmark/reports/all-results-2026-10-13/all-results.json>) · [353 条主历史 JSON](</Users/fairchild/Library/Application Support/GpuComputeBenchmark/reports/all-results-2026-10-13/main-results.json>)。
旧格式 220 条均未记录 workloadVersion；保留原分数与参数，不混入当前版本的排名。软件渲染器单独标注。隔离短测可能未越过缓存，较高 GB/s 不能作为 DRAM 带宽。时间列保留源文件原始字符串，不推断旧成绩的时区。

## 按原始设备名称计数

同一设备可能有不同 API 别名或节点编号，下表的名称数量不等于物理设备数量。

| 原始设备名 | 条数 |
|---|---:|
| Apple M4 Pro | 155 |
| NVIDIA GeForce GT 730 | 20 |
| NVIDIA GeForce GTX 680 | 20 |
| Moore Threads MTT S80 | 20 |
| Microsoft Basic Render Driver | 18 |
| NVIDIA GeForce RTX 2080 | 16 |
| AMD Radeon RX 9070 XT | 16 |
| NVIDIA GeForce GTX 570 | 15 |
| AMD Radeon RX 6900 XT | 15 |
| NVIDIA GeForce RTX 5090 | 7 |
| Microsoft WARP (CPU Software Renderer) | 6 |
| Mi Pad 5 Adreno 640 GPU | 5 |
| AMD Radeon(TM) Graphics | 5 |
| Intel(R) UHD Graphics 630 | 4 |
| NVIDIA GeForce RTX 4070 | 4 |
| NVIDIA GeForce RTX 4070 SUPER | 4 |
| NVIDIA GeForce RTX 5090 #1 | 4 |
| AMD FirePro D700 (FireGL V) #1 | 4 |
| AMD FirePro D700 (FireGL V) #2 | 3 |
| Radeon RX 580 Series | 3 |
| Radeon Vega Frontier Edition | 3 |
| AMD Radeon RX 6600 XT | 3 |
| SVGAMPWDDM Device | 2 |
| NVIDIA GeForce GTX 970 | 2 |
| AMD Radeon HD 5700 Series | 2 |
| AMD Radeon(TM) Graphics (FL 12_1) | 2 |
| Mi Pad 5 Adreno 640 GPU (FL 12_1) | 1 |
| NVIDIA GeForce GTX 970/PCIe/SSE2 | 1 |
| NVIDIA GeForce GTX 970 (FL 12_1) | 1 |
| Radeon RX 580 Series (FL 12_0) | 1 |
| Radeon Vega Frontier Edition (FL 12_1) | 1 |
| AMD Radeon RX 6600 XT (FL 12_1) | 1 |
| AMD Radeon RX 6900 XT (FL 12_1) | 1 |
| NVIDIA GeForce RTX 5090/PCIe/SSE2 | 1 |
| NVIDIA GeForce RTX 5090 (FL 12_1) | 1 |

## 全部 367 条明细

| ID | 来源 | 设备 | API | 项目 | 模式 | 粒子数 | 成绩 | 版本 | 说明 |
|---|---|---|---|---|---|---:|---|---|---|
| 20261013-201629-128-metal-cache | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU 缓存延迟诊断 | 纯计算 | — | 340.717 ns/load | metal_dependent_cache_v1_preview | 预览/诊断；L2/L3/SLC 未确认 |
| 20261013-201303-924-membw | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | GPU 内存扫描 | 纯计算 | — | 252.395 GB/s | membw_v1_moltenvk | MoltenVK 转译 |
| 20261013-201256-131-metal-cache | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU 缓存延迟诊断 | 纯计算 | — | 341.228 ns/load | metal_dependent_cache_v1_preview | 预览/诊断；L2/L3/SLC 未确认 |
| 20261013-201229-094-metal-cache | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU 缓存延迟诊断 | 纯计算 | — | 341.468 ns/load | metal_dependent_cache_v1_preview | 预览/诊断；L2/L3/SLC 未确认 |
| 20261013-195653-520-cpu-membw | 当前 Mac 主成绩 | Apple M4 Pro | CPU | CPU 缓存/内存 | 系统调度 | — | 248.742 GB/s | cpu_membw_v1_preview | 预览/诊断 |
| 20261013-195616-200-cpu-membw | 当前 Mac 主成绩 | Apple M4 Pro | CPU | CPU 缓存/内存 | 系统调度 | — | 247.379 GB/s | cpu_membw_v1 | 保留原始测试条件 |
| 20261013-195532-533-cpu-multi | 当前 Mac 主成绩 | Apple M4 Pro | CPU | CPU 多线程 | 系统调度 | — | 3726.693 MWork/s | cpu_mixed_v1_scheduler_managed_formal_r3_t15000ms_w200ms_sequence_before_percore | 保留原始测试条件 |
| 20261013-195532-528-cpu-single | 当前 Mac 主成绩 | Apple M4 Pro | CPU | 单线程采样（系统调度） | 系统调度 | — | 322.300 MWork/s | cpu_mixed_v1_scheduler_managed_formal_r3_t15000ms_w200ms_sequence_standalone | 未绑定核心，不能分 P/E |
| 20261013-183845-275 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 互动水池 | 带渲染 | 320920 | 36.987 MParticle-step/s | cinematic_liquid_v2_physical_scene_v8_metal_preview | 预览/诊断 |
| 20261013-183829-896 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 4.386 Gpix-step/s | gpu_burn_v3_fixed_steps_256_kaleidoscope | 保留原始测试条件 |
| 20261013-183814-515 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 4.517 Gpix-step/s | gpu_burn_v3_fixed_steps_64_kaleidoscope | 保留原始测试条件 |
| 20261013-183759-245 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 4.421 Gpix-step/s | gpu_burn_v3_fixed_steps_16_kaleidoscope | 保留原始测试条件 |
| 20261013-183743-995 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 16777216 | 57.878 GB/s | stream_v1 | 保留原始测试条件 |
| 20261013-183728-568 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 4194304 | 54.918 GB/s | stream_v1 | 保留原始测试条件 |
| 20261013-183713-285 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 65536 | 151.353 GB/s | stream_v1 | 保留原始测试条件 |
| 20261013-183538-229 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 52.401 GB/s | stream_v1 | 保留原始测试条件 |
| 20261013-183413-702 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | 互动水池 | 带渲染 | 320920 | 67.361 MParticle-step/s | cinematic_liquid_v2_physical_scene_v8_moltenvk_viewport1280x720_v1 | MoltenVK 转译 |
| 20261013-183356-361 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | GPU Burn | 带渲染 | — | 6.888 Gpix-step/s | gpu_burn_v3_fixed_steps_256_kaleidoscope_moltenvk_viewport1280x720_v1 | MoltenVK 转译 |
| 20261013-183340-938 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | GPU Burn | 带渲染 | — | 6.460 Gpix-step/s | gpu_burn_v3_fixed_steps_64_kaleidoscope_moltenvk_viewport1280x720_v1 | MoltenVK 转译 |
| 20261013-183325-631 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | GPU Burn | 带渲染 | — | 5.879 Gpix-step/s | gpu_burn_v3_fixed_steps_16_kaleidoscope_moltenvk_viewport1280x720_v1 | MoltenVK 转译 |
| 20261013-183310-238 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | 粒子吞吐（推算值） | 带渲染 | 16777216 | 127.469 GB/s | stream_v1_moltenvk_viewport1280x720_v1 | MoltenVK 转译 |
| 20261013-183254-796 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | 粒子吞吐（推算值） | 带渲染 | 4194304 | 132.732 GB/s | stream_v1_moltenvk_viewport1280x720_v1 | MoltenVK 转译 |
| 20261013-183239-499 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 85.926 GB/s | stream_v1_moltenvk_viewport1280x720_v1 | MoltenVK 转译 |
| 20261013-183224-222 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | 粒子吞吐（推算值） | 带渲染 | 65536 | 50.542 GB/s | stream_v1_moltenvk_viewport1280x720_v1 | MoltenVK 转译 |
| 20261013-183008-002-cpu-membw | 当前 Mac 主成绩 | Apple M4 Pro | CPU | CPU 缓存/内存 | 系统调度 | — | 245.834 GB/s | cpu_membw_v1 | 保留原始测试条件 |
| 20261013-182920-557-cpu-multi | 当前 Mac 主成绩 | Apple M4 Pro | CPU | CPU 多线程 | 系统调度 | — | 3010.644 MWork/s | cpu_mixed_v1_scheduler_managed_formal_r3_t15000ms_w200ms_sequence_before_percore | 保留原始测试条件 |
| 20261013-182920-552-cpu-single | 当前 Mac 主成绩 | Apple M4 Pro | CPU | 单线程采样（系统调度） | 系统调度 | — | 307.581 MWork/s | cpu_mixed_v1_scheduler_managed_formal_r3_t15000ms_w200ms_sequence_standalone | 未绑定核心，不能分 P/E |
| 20261013-182532-487 | 当前 Mac 主成绩 | Apple M4 Pro | OpenGL | GPU Burn | 带渲染 | — | 5.924 Gpix-step/s | gpu_burn_v3_fixed_steps_256_kaleidoscope_sync_timing | 保留原始测试条件 |
| 20261013-182517-137 | 当前 Mac 主成绩 | Apple M4 Pro | OpenGL | GPU Burn | 带渲染 | — | 5.967 Gpix-step/s | gpu_burn_v3_fixed_steps_64_kaleidoscope_sync_timing | 保留原始测试条件 |
| 20261013-182501-919 | 当前 Mac 主成绩 | Apple M4 Pro | OpenGL | GPU Burn | 带渲染 | — | 5.243 Gpix-step/s | gpu_burn_v3_fixed_steps_16_kaleidoscope_sync_timing | 保留原始测试条件 |
| 20261013-182446-724-metal-memsweep | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU 内存扫描 | 纯计算 | — | 244.776 GB/s | metal_memory_sweep_v1_preview | 预览/诊断；内存尾段未验证 |
| 20261013-182445-650 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 互动水池 | 带渲染 | 320920 | 35.535 MParticle-step/s | cinematic_liquid_v2_physical_scene_v8_metal_preview | 预览/诊断 |
| 20261013-182429-197 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 4.490 Gpix-step/s | gpu_burn_v3_fixed_steps_256_kaleidoscope | 保留原始测试条件 |
| 20261013-182413-910 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 4.442 Gpix-step/s | gpu_burn_v3_fixed_steps_64_kaleidoscope | 保留原始测试条件 |
| 20261013-182358-705 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 4.319 Gpix-step/s | gpu_burn_v3_fixed_steps_16_kaleidoscope | 保留原始测试条件 |
| 20261013-182343-374 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 16777216 | 143.277 GB/s | stream_v1 | 保留原始测试条件 |
| 20261013-182328-059 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 4194304 | 142.220 GB/s | stream_v1 | 保留原始测试条件 |
| 20261013-182312-870 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 1048576 | 146.696 GB/s | stream_v1 | 保留原始测试条件 |
| 20261013-182257-728 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 65536 | 310.199 GB/s | stream_v1 | 保留原始测试条件 |
| 20261013-182242-539 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 16777216 | 56.008 GB/s | stream_v1 | 保留原始测试条件 |
| 20261013-182227-148 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 4194304 | 46.785 GB/s | stream_v1 | 保留原始测试条件 |
| 20261013-182211-917 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 45.246 GB/s | stream_v1 | 保留原始测试条件 |
| 20261013-182156-718 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 65536 | 117.835 GB/s | stream_v1 | 保留原始测试条件 |
| 20261013-182141-555-membw | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | GPU 内存扫描 | 纯计算 | — | 238.796 GB/s | membw_v1_moltenvk | MoltenVK 转译 |
| 20261013-182133-384 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | 互动水池 | 带渲染 | 320920 | 45.350 MParticle-step/s | cinematic_liquid_v2_physical_scene_v8_moltenvk | MoltenVK 转译；旧 Retina 尺寸组 |
| 20261013-182115-525 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | GPU Burn | 带渲染 | — | 1.761 Gpix-step/s | gpu_burn_v3_fixed_steps_256_kaleidoscope_moltenvk | MoltenVK 转译；旧 Retina 尺寸组 |
| 20261013-182059-623 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | GPU Burn | 带渲染 | — | 1.661 Gpix-step/s | gpu_burn_v3_fixed_steps_64_kaleidoscope_moltenvk | MoltenVK 转译；旧 Retina 尺寸组 |
| 20261013-182044-234 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | GPU Burn | 带渲染 | — | 1.519 Gpix-step/s | gpu_burn_v3_fixed_steps_16_kaleidoscope_moltenvk | MoltenVK 转译；旧 Retina 尺寸组 |
| 20261013-182028-822 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | 粒子吞吐（推算值） | 纯计算 | 16777216 | 133.438 GB/s | stream_v1_moltenvk | MoltenVK 转译 |
| 20261013-182013-494 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | 粒子吞吐（推算值） | 纯计算 | 4194304 | 145.311 GB/s | stream_v1_moltenvk | MoltenVK 转译 |
| 20261013-181958-318 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | 粒子吞吐（推算值） | 纯计算 | 1048576 | 219.681 GB/s | stream_v1_moltenvk | MoltenVK 转译 |
| 20261013-181943-177 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | 粒子吞吐（推算值） | 纯计算 | 65536 | 239.127 GB/s | stream_v1_moltenvk | MoltenVK 转译 |
| 20261013-181927-994 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | 粒子吞吐（推算值） | 带渲染 | 16777216 | 120.419 GB/s | stream_v1_moltenvk | MoltenVK 转译；旧 Retina 尺寸组 |
| 20261013-181912-533 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | 粒子吞吐（推算值） | 带渲染 | 4194304 | 119.127 GB/s | stream_v1_moltenvk | MoltenVK 转译；旧 Retina 尺寸组 |
| 20261013-181857-298 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 138.090 GB/s | stream_v1_moltenvk | MoltenVK 转译；旧 Retina 尺寸组 |
| 20261013-181842-102 | 当前 Mac 主成绩 | Apple M4 Pro | Vulkan | 粒子吞吐（推算值） | 带渲染 | 65536 | 40.175 GB/s | stream_v1_moltenvk | MoltenVK 转译；旧 Retina 尺寸组 |
| 20261012-150956-946 | 隔离验证：API 流程 | Apple M4 Pro | OpenGL | GPU Burn | 带渲染 | — | 6.829 Gpix-step/s | gpu_burn_v3_fixed_steps_256_kaleidoscope_sync_timing | 隔离短测，验证用途 |
| 20261012-150953-690 | 隔离验证：API 流程 | Apple M4 Pro | OpenGL | GPU Burn | 带渲染 | — | 6.496 Gpix-step/s | gpu_burn_v3_fixed_steps_64_kaleidoscope_sync_timing | 隔离短测，验证用途 |
| 20261012-150950-505 | 隔离验证：API 流程 | Apple M4 Pro | OpenGL | GPU Burn | 带渲染 | — | 5.425 Gpix-step/s | gpu_burn_v3_fixed_steps_16_kaleidoscope_sync_timing | 隔离短测，验证用途 |
| 20261012-150947-317 | 隔离验证：API 流程 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 65536 | 127.476 GB/s | stream_v1 | 隔离短测，验证用途 |
| 20261012-145313-338 | 隔离验证：OpenGL | Apple M4 Pro | OpenGL | GPU Burn | 带渲染 | — | 1.751 Gpix-step/s | gpu_burn_v3_fixed_steps_256_kaleidoscope_sync_timing | 隔离短测，验证用途 |
| 20261012-145309-837 | 隔离验证：OpenGL | Apple M4 Pro | OpenGL | GPU Burn | 带渲染 | — | 1.675 Gpix-step/s | gpu_burn_v3_fixed_steps_64_kaleidoscope_sync_timing | 隔离短测，验证用途 |
| 20261012-145306-542 | 隔离验证：OpenGL | Apple M4 Pro | OpenGL | GPU Burn | 带渲染 | — | 5.009 Gpix-step/s | gpu_burn_v3_fixed_steps_16_kaleidoscope_sync_timing | 隔离短测，验证用途 |
| 20261012-143059-431 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 65536 | 158.597 GB/s | stream_v1 | 保留原始测试条件 |
| 20261012-141833-964-metal-memsweep | 隔离验证：Apple 适配 | Apple M4 Pro | Metal | GPU 内存扫描 | 纯计算 | — | 1715.244 GB/s | metal_memory_sweep_v1_preview | 隔离短测，验证用途；预览/诊断；内存尾段未验证 |
| 20261012-141833-816 | 隔离验证：Apple 适配 | Apple M4 Pro | Metal | 互动水池 | 带渲染 | 320920 | 41.161 MParticle-step/s | cinematic_liquid_v2_physical_scene_v8_metal_preview | 隔离短测，验证用途；预览/诊断 |
| 20261012-141830-568 | 隔离验证：Apple 适配 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 3.958 Gpix-step/s | gpu_burn_v3_fixed_steps_16_kaleidoscope | 隔离短测，验证用途 |
| 20261012-141829-318 | 隔离验证：Apple 适配 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 65536 | 312.738 GB/s | stream_v1 | 隔离短测，验证用途 |
| 20261012-141828-075-cpu-membw | 隔离验证：Apple 适配 | Apple M4 Pro | CPU | CPU 缓存/内存 | 系统调度 | — | 541.746 GB/s | cpu_membw_v1_preview | 隔离短测，验证用途；预览/诊断 |
| 20261012-141805-565-cpu-multi | 隔离验证：Apple 适配 | Apple M4 Pro | CPU | CPU 多线程 | 系统调度 | — | 3454.094 MWork/s | cpu_mixed_v1_scheduler_managed_preview_r3_t30ms_w0ms_sequence_before_percore | 隔离短测，验证用途；预览/诊断 |
| 20261012-141805-564-cpu-single | 隔离验证：Apple 适配 | Apple M4 Pro | CPU | 单线程采样（系统调度） | 系统调度 | — | 323.426 MWork/s | cpu_mixed_v1_scheduler_managed_preview_r3_t30ms_w0ms_sequence_standalone | 隔离短测，验证用途；预览/诊断；未绑定核心，不能分 P/E |
| 20260801-122157-106 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 互动水池 | 带渲染 | 320920 | 32.640 MParticle-step/s | cinematic_liquid_v2_physical_scene_v8_metal_preview | 预览/诊断 |
| 20260801-122146-810 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 互动水池 | 带渲染 | 320920 | 35.360 MParticle-step/s | cinematic_liquid_v2_physical_scene_v8_metal_preview | 预览/诊断 |
| 20260801-122027-021 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 4.836 Gpix-step/s | gpu_burn_v3_fixed_steps_256_kaleidoscope | 保留原始测试条件 |
| 20260801-122020-795 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 4.824 Gpix-step/s | gpu_burn_v3_fixed_steps_256_kaleidoscope | 保留原始测试条件 |
| 20260801-122014-466 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 4.756 Gpix-step/s | gpu_burn_v3_fixed_steps_64_kaleidoscope | 保留原始测试条件 |
| 20260801-122008-271 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 4.409 Gpix-step/s | gpu_burn_v3_fixed_steps_64_kaleidoscope | 保留原始测试条件 |
| 20260801-122001-984 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 3.545 Gpix-step/s | gpu_burn_v3_fixed_steps_16_kaleidoscope | 保留原始测试条件 |
| 20260801-121955-758 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 3.882 Gpix-step/s | gpu_burn_v3_fixed_steps_16_kaleidoscope | 保留原始测试条件 |
| 20260801-121456-357 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 16777216 | 118.425 GB/s | stream_v1 | 保留原始测试条件 |
| 20260801-121450-082 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 4194304 | 107.642 GB/s | stream_v1 | 保留原始测试条件 |
| 20260801-121443-861 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 1048576 | 104.565 GB/s | stream_v1 | 保留原始测试条件 |
| 20260801-121437-731 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 65536 | 223.534 GB/s | stream_v1 | 保留原始测试条件 |
| 20260801-120847-970 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 16777216 | 113.119 GB/s | stream_v1 | 保留原始测试条件 |
| 20260801-120841-695 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 4194304 | 115.549 GB/s | stream_v1 | 保留原始测试条件 |
| 20260801-120835-571 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 1048576 | 108.900 GB/s | stream_v1 | 保留原始测试条件 |
| 20260801-120829-467 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 65536 | 219.848 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-144013-167 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 1048576 | 157.532 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-144010-075 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 262144 | 321.195 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-144006-975 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 65536 | 319.419 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-144003-890 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 16384 | 167.235 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-144000-792 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 4096 | 62.865 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-143322-586 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 16777216 | 147.808 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-143319-336 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 65536 | 311.869 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-142457-470 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 33554432 | 148.217 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-142451-005 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 16777216 | 149.399 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-142444-734 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 4194304 | 148.470 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-142438-619 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 1048576 | 154.603 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-142432-529 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 262144 | 316.664 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-141808-178 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 65536 | 308.135 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-141546-191 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 65536 | 102.956 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-141539-998 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 72.537 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-141454-107 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 1048576 | 157.537 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-141447-977 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 53.734 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-141441-840 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 65536 | 317.664 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-141435-690 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 65536 | 32.315 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-141429-524 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 65536 | 96.596 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-141403-674 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 48.647 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-141357-489 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 53.716 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-141351-324 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 53.420 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-141345-137 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 53.877 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-141338-959 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 53.924 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-141312-193 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 65536 | 314.191 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-140844-092 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 65536 | 322.345 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-140803-638 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 50.375 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-140728-073 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 1048576 | 164.470 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-140637-343 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 65536 | 323.056 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-135844-148 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 65536 | 100.418 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-135814-516 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 50.494 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-135450-236 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 65536 | 32.643 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-135445-067 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 互动水池 | 带渲染 | 320920 | 37.150 MParticle-step/s | cinematic_liquid_v2_physical_scene_v8_metal_preview | 预览/诊断 |
| 20260731-135436-788 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 65536 | 96.984 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-135344-098 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 65536 | 31.610 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-135337-922 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 4.263 Gpix-step/s | gpu_burn_v3_fixed_steps_16_kaleidoscope | 保留原始测试条件 |
| 20260731-135331-714 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 互动水池 | 带渲染 | 320920 | 36.691 MParticle-step/s | cinematic_liquid_v2_physical_scene_v8_metal_preview | 预览/诊断 |
| 20260731-135302-024 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 1048576 | 158.121 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-135255-900 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 53.157 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-135249-772 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 65536 | 313.967 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-135243-655 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 65536 | 94.558 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-132842-969 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 1048576 | 155.249 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-132836-822 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 51.309 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-132830-687 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 65536 | 315.369 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-132824-568 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 65536 | 77.780 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-132719-894 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 互动水池 | 带渲染 | 320920 | 36.674 MParticle-step/s | cinematic_liquid_v2_physical_scene_v8_metal_preview | 预览/诊断 |
| 20260731-070023-220-cpu-single | 当前 Mac 主成绩 | Apple M4 Pro | CPU | 单线程采样（系统调度） | 系统调度 | — | 329.138 MWork/s | cpu_mixed_v1_scheduler_managed_preview_r3_t200ms_w50ms_sequence_standalone | 预览/诊断；未绑定核心，不能分 P/E |
| 20260731-070005-471-cpu-multi | 当前 Mac 主成绩 | Apple M4 Pro | CPU | CPU 多线程 | 系统调度 | — | 3620.802 MWork/s | cpu_mixed_v1_scheduler_managed_preview_r3_t1000ms_w200ms_sequence_standalone | 预览/诊断 |
| 20260731-062548-672-cpu-multi | 当前 Mac 主成绩 | Apple M4 Pro | CPU | CPU 多线程 | 系统调度 | — | 3588.447 MWork/s | cpu_mixed_v1_scheduler_managed_preview_r3_t1000ms_w200ms_sequence_standalone | 预览/诊断 |
| 20260731-062540-129-cpu-multi | 当前 Mac 主成绩 | Apple M4 Pro | CPU | CPU 多线程 | 系统调度 | — | 3734.093 MWork/s | cpu_mixed_v1_scheduler_managed_preview_r3_t1000ms_w200ms_sequence_before_percore | 预览/诊断 |
| 20260731-062540-128-cpu-single | 当前 Mac 主成绩 | Apple M4 Pro | CPU | 单线程采样（系统调度） | 系统调度 | — | 303.212 MWork/s | cpu_mixed_v1_scheduler_managed_preview_r3_t1000ms_w200ms_sequence_standalone | 预览/诊断；未绑定核心，不能分 P/E |
| 20260731-062420-921 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 互动水池 | 带渲染 | 320920 | 161.320 MParticle-step/s | cinematic_liquid_v2_physical_scene_v8_metal_preview | 预览/诊断 |
| 20260731-062359-605 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 4.531 Gpix-step/s | gpu_burn_v3_fixed_steps_256_kaleidoscope | 保留原始测试条件 |
| 20260731-062338-854 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 4.212 Gpix-step/s | gpu_burn_v3_fixed_steps_64_kaleidoscope | 保留原始测试条件 |
| 20260731-062318-477 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | GPU Burn | 带渲染 | — | 3.935 Gpix-step/s | gpu_burn_v3_fixed_steps_16_kaleidoscope | 保留原始测试条件 |
| 20260731-062248-149 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 16777216 | 62.069 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-062229-676 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 16777216 | 132.085 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-062205-334 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 1048576 | 138.611 GB/s | stream_v1 | 保留原始测试条件 |
| 20260731-062125-901 | 当前 Mac 主成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 52.348 GB/s | stream_v1 | 保留原始测试条件 |
| 20260627-102552-373 | 仓库旧成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 1048576 | 165.825 GB/s | 未记录 | 旧格式，版本未记录 |
| 20260627-101946-775 | 仓库旧成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 1048576 | 157.222 GB/s | 未记录 | 旧格式，版本未记录 |
| 20260627-101313-428 | 仓库旧成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 1048576 | 148.764 GB/s | 未记录 | 旧格式，版本未记录 |
| 20260627-101306-541 | 仓库旧成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 1048576 | 145.846 GB/s | 未记录 | 旧格式，版本未记录 |
| 20260627-101257-651 | 仓库旧成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 52.003 GB/s | 未记录 | 旧格式，版本未记录 |
| 20260627-101245-760 | 仓库旧成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 纯计算 | 1048576 | 166.809 GB/s | 未记录 | 旧格式，版本未记录 |
| 20260623-230407-583 | 仓库旧成绩 | NVIDIA GeForce RTX 5090 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 1814.434 GB/s | 未记录 | 旧格式，版本未记录 |
| 20260528-222132-886 | 仓库旧成绩 | NVIDIA GeForce GT 730 | OpenGL | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260528-222117-344 | 仓库旧成绩 | NVIDIA GeForce GT 730 | DX11 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260528-222102-238 | 仓库旧成绩 | NVIDIA GeForce GT 730 | DX12 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260528-222047-118 | 仓库旧成绩 | NVIDIA GeForce GT 730 | Vulkan | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260528-222018-707 | 仓库旧成绩 | NVIDIA GeForce GT 730 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260528-222001-452 | 仓库旧成绩 | NVIDIA GeForce GT 730 | DX11 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260528-221944-937 | 仓库旧成绩 | NVIDIA GeForce GT 730 | DX12 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260528-221928-556 | 仓库旧成绩 | NVIDIA GeForce GT 730 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260528-221857-000 | 仓库旧成绩 | NVIDIA GeForce GT 730 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260528-221841-307 | 仓库旧成绩 | NVIDIA GeForce GT 730 | DX11 | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260528-221825-845 | 仓库旧成绩 | NVIDIA GeForce GT 730 | DX12 | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260528-221810-348 | 仓库旧成绩 | NVIDIA GeForce GT 730 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260528-221353-867 | 仓库旧成绩 | NVIDIA GeForce GT 730 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260528-221338-541 | 仓库旧成绩 | NVIDIA GeForce GT 730 | DX11 | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260528-221323-450 | 仓库旧成绩 | NVIDIA GeForce GT 730 | DX12 | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260528-221307-211 | 仓库旧成绩 | NVIDIA GeForce GT 730 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260522-005620-353 | 仓库旧成绩 | NVIDIA GeForce GTX 570 | OpenGL | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260522-005605-171 | 仓库旧成绩 | NVIDIA GeForce GTX 570 | DX11 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260522-005550-054 | 仓库旧成绩 | NVIDIA GeForce GTX 570 | DX12 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260522-005526-750 | 仓库旧成绩 | NVIDIA GeForce GTX 570 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260522-005507-146 | 仓库旧成绩 | NVIDIA GeForce GTX 570 | DX11 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260522-005449-712 | 仓库旧成绩 | NVIDIA GeForce GTX 570 | DX12 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260522-005424-173 | 仓库旧成绩 | NVIDIA GeForce GTX 570 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260522-005408-817 | 仓库旧成绩 | NVIDIA GeForce GTX 570 | DX11 | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260522-005353-357 | 仓库旧成绩 | NVIDIA GeForce GTX 570 | DX12 | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260522-005328-501 | 仓库旧成绩 | NVIDIA GeForce GTX 570 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260522-005313-302 | 仓库旧成绩 | NVIDIA GeForce GTX 570 | DX11 | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260522-005258-206 | 仓库旧成绩 | NVIDIA GeForce GTX 570 | DX12 | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260522-005232-784 | 仓库旧成绩 | NVIDIA GeForce GTX 570 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260522-005217-377 | 仓库旧成绩 | NVIDIA GeForce GTX 570 | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260522-005202-189 | 仓库旧成绩 | NVIDIA GeForce GTX 570 | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230650-718 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | OpenGL | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230635-548 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | DX11 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230620-450 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | DX12 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230605-364 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | Vulkan | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230541-166 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230525-101 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | DX11 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230509-042 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | DX12 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230453-382 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230426-545 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230411-198 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | DX11 | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230355-868 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | DX12 | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230340-637 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230313-314 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230258-192 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | DX11 | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230243-095 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | DX12 | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230228-000 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230145-757 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230130-394 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230115-240 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260521-230059-008 | 仓库旧成绩 | NVIDIA GeForce GTX 680 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-212400-609 | 仓库旧成绩 | Moore Threads MTT S80 | OpenGL | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-212345-221 | 仓库旧成绩 | Moore Threads MTT S80 | DX11 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-212330-117 | 仓库旧成绩 | Moore Threads MTT S80 | DX12 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-212314-930 | 仓库旧成绩 | Moore Threads MTT S80 | Vulkan | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-212252-559 | 仓库旧成绩 | Moore Threads MTT S80 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-212236-023 | 仓库旧成绩 | Moore Threads MTT S80 | DX11 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-212220-014 | 仓库旧成绩 | Moore Threads MTT S80 | DX12 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-212204-140 | 仓库旧成绩 | Moore Threads MTT S80 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-212137-407 | 仓库旧成绩 | Moore Threads MTT S80 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-212121-652 | 仓库旧成绩 | Moore Threads MTT S80 | DX11 | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-212106-332 | 仓库旧成绩 | Moore Threads MTT S80 | DX12 | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-212050-955 | 仓库旧成绩 | Moore Threads MTT S80 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-212024-619 | 仓库旧成绩 | Moore Threads MTT S80 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-212009-434 | 仓库旧成绩 | Moore Threads MTT S80 | DX11 | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-211954-330 | 仓库旧成绩 | Moore Threads MTT S80 | DX12 | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-211939-122 | 仓库旧成绩 | Moore Threads MTT S80 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-211906-875 | 仓库旧成绩 | Moore Threads MTT S80 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-211851-400 | 仓库旧成绩 | Moore Threads MTT S80 | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-211836-243 | 仓库旧成绩 | Moore Threads MTT S80 | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260520-211819-776 | 仓库旧成绩 | Moore Threads MTT S80 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-164309-013 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | OpenGL | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-164253-868 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | DX11 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-164238-773 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | DX12 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-164223-680 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | Vulkan | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-164141-283 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-164125-950 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | DX11 | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-164110-667 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | DX12 | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-164055-444 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 4194304 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-163953-043 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-163937-244 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | DX11 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-163921-333 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | DX12 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-163905-725 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-163832-710 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-163817-343 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-163802-195 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260518-163745-864 | 仓库旧成绩 | NVIDIA GeForce RTX 2080 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260408-012947-268 | 仓库旧成绩 | Microsoft Basic Render Driver | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260408-012932-117 | 仓库旧成绩 | Microsoft Basic Render Driver | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260408-012916-371 | 仓库旧成绩 | Intel(R) UHD Graphics 630 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260408-012901-034 | 仓库旧成绩 | Intel(R) UHD Graphics 630 | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260408-012845-801 | 仓库旧成绩 | Intel(R) UHD Graphics 630 | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260408-012829-697 | 仓库旧成绩 | Intel(R) UHD Graphics 630 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260405-193533-098 | 仓库旧成绩 | Microsoft Basic Render Driver | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260405-193517-870 | 仓库旧成绩 | Microsoft Basic Render Driver | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260405-193502-596 | 仓库旧成绩 | Microsoft Basic Render Driver | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260405-193447-064 | 仓库旧成绩 | SVGAMPWDDM Device | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260405-191710-736 | 仓库旧成绩 | Microsoft Basic Render Driver | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260405-191655-433 | 仓库旧成绩 | Microsoft Basic Render Driver | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260405-191638-996 | 仓库旧成绩 | Microsoft Basic Render Driver | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260405-191622-390 | 仓库旧成绩 | SVGAMPWDDM Device | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260405-123032-248 | 仓库旧成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 65536 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260405-123006-196 | 仓库旧成绩 | Apple M4 Pro | Metal | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260403-191251-780 | 仓库旧成绩 | Microsoft Basic Render Driver | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260403-191236-360 | 仓库旧成绩 | Microsoft Basic Render Driver | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260402-213704-257 | 仓库旧成绩 | Microsoft Basic Render Driver | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260402-213649-072 | 仓库旧成绩 | Microsoft Basic Render Driver | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260402-213633-127 | 仓库旧成绩 | NVIDIA GeForce RTX 4070 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260402-213617-606 | 仓库旧成绩 | NVIDIA GeForce RTX 4070 | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260402-213602-321 | 仓库旧成绩 | NVIDIA GeForce RTX 4070 | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260402-213545-602 | 仓库旧成绩 | NVIDIA GeForce RTX 4070 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260402-014549-967 | 仓库旧成绩 | NVIDIA GeForce RTX 4070 SUPER | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260402-014534-722 | 仓库旧成绩 | NVIDIA GeForce RTX 4070 SUPER | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260402-014519-568 | 仓库旧成绩 | NVIDIA GeForce RTX 4070 SUPER | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260402-014503-514 | 仓库旧成绩 | NVIDIA GeForce RTX 4070 SUPER | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260331-014352-061 | 仓库旧成绩 | Microsoft Basic Render Driver | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260331-014336-703 | 仓库旧成绩 | Microsoft Basic Render Driver | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260330-152631-901 | 仓库旧成绩 | NVIDIA GeForce GT 730 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260330-152616-487 | 仓库旧成绩 | NVIDIA GeForce GT 730 | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260330-152601-236 | 仓库旧成绩 | NVIDIA GeForce GT 730 | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260330-152544-873 | 仓库旧成绩 | NVIDIA GeForce GT 730 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-151734-551 | 仓库旧成绩 | NVIDIA GeForce RTX 5090 | OpenGL | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-151719-344 | 仓库旧成绩 | NVIDIA GeForce RTX 5090 | DX11 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-151704-218 | 仓库旧成绩 | NVIDIA GeForce RTX 5090 | DX12 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-151648-829 | 仓库旧成绩 | NVIDIA GeForce RTX 5090 | Vulkan | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-150843-305 | 仓库旧成绩 | NVIDIA GeForce RTX 5090 #1 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-150827-701 | 仓库旧成绩 | NVIDIA GeForce RTX 5090 #1 | DX11 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-150812-181 | 仓库旧成绩 | NVIDIA GeForce RTX 5090 #1 | DX12 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-150756-136 | 仓库旧成绩 | NVIDIA GeForce RTX 5090 #1 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-150231-667 | 仓库旧成绩 | AMD Radeon RX 6900 XT | OpenGL | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-150216-452 | 仓库旧成绩 | AMD Radeon RX 6900 XT | DX11 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-150201-376 | 仓库旧成绩 | AMD Radeon RX 6900 XT | DX12 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-150146-310 | 仓库旧成绩 | AMD Radeon RX 6900 XT | Vulkan | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-150113-190 | 仓库旧成绩 | AMD Radeon RX 6900 XT | OpenGL | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-150057-296 | 仓库旧成绩 | AMD Radeon RX 6900 XT | DX11 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-150041-472 | 仓库旧成绩 | AMD Radeon RX 6900 XT | DX12 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-150026-021 | 仓库旧成绩 | AMD Radeon RX 6900 XT | Vulkan | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-145950-313 | 仓库旧成绩 | AMD Radeon RX 6900 XT | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-145934-900 | 仓库旧成绩 | AMD Radeon RX 6900 XT | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-145919-768 | 仓库旧成绩 | AMD Radeon RX 6900 XT | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-145904-065 | 仓库旧成绩 | AMD Radeon RX 6900 XT | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-021152-628 | 仓库旧成绩 | Microsoft Basic Render Driver | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260325-021136-489 | 仓库旧成绩 | Microsoft Basic Render Driver | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260325-021115-596 | 仓库旧成绩 | Mi Pad 5 Adreno 640 GPU | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-021059-199 | 仓库旧成绩 | Mi Pad 5 Adreno 640 GPU | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-021043-168 | 仓库旧成绩 | Mi Pad 5 Adreno 640 GPU | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-010928-652 | 仓库旧成绩 | AMD Radeon RX 9070 XT | OpenGL | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-010912-690 | 仓库旧成绩 | AMD Radeon RX 9070 XT | DX11 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-010856-901 | 仓库旧成绩 | AMD Radeon RX 9070 XT | DX12 | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-010841-049 | 仓库旧成绩 | AMD Radeon RX 9070 XT | Vulkan | 粒子吞吐（推算值） | 带渲染 | 16777216 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-005348-367 | 仓库旧成绩 | AMD Radeon RX 9070 XT | OpenGL | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-005333-263 | 仓库旧成绩 | AMD Radeon RX 9070 XT | DX11 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-005318-201 | 仓库旧成绩 | AMD Radeon RX 9070 XT | DX12 | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-005303-139 | 仓库旧成绩 | AMD Radeon RX 9070 XT | Vulkan | 粒子吞吐（推算值） | 纯计算 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-002652-653 | 仓库旧成绩 | AMD Radeon RX 9070 XT | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-002637-350 | 仓库旧成绩 | AMD Radeon RX 9070 XT | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-002622-242 | 仓库旧成绩 | AMD Radeon RX 9070 XT | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260325-002606-742 | 仓库旧成绩 | AMD Radeon RX 9070 XT | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-231244-080 | 仓库旧成绩 | AMD Radeon RX 9070 XT | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-231228-690 | 仓库旧成绩 | AMD Radeon RX 9070 XT | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-231213-566 | 仓库旧成绩 | AMD Radeon RX 9070 XT | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-231157-871 | 仓库旧成绩 | AMD Radeon RX 9070 XT | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-215213-271 | 仓库旧成绩 | Microsoft WARP (CPU Software Renderer) | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260324-215157-490 | 仓库旧成绩 | Microsoft WARP (CPU Software Renderer) | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260324-215141-210 | 仓库旧成绩 | Mi Pad 5 Adreno 640 GPU | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-215125-663 | 仓库旧成绩 | Mi Pad 5 Adreno 640 GPU (FL 12_1) | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-215110-099 | 仓库旧成绩 | Mi Pad 5 Adreno 640 GPU | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-215016-317 | 仓库旧成绩 | Microsoft Basic Render Driver | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260324-215001-077 | 仓库旧成绩 | Microsoft Basic Render Driver | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260324-214944-625 | 仓库旧成绩 | AMD FirePro D700 (FireGL V) #2 | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-214929-279 | 仓库旧成绩 | AMD FirePro D700 (FireGL V) #2 | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-214914-139 | 仓库旧成绩 | AMD FirePro D700 (FireGL V) #2 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-214858-878 | 仓库旧成绩 | AMD FirePro D700 (FireGL V) #1 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-214843-335 | 仓库旧成绩 | AMD FirePro D700 (FireGL V) #1 | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-214827-978 | 仓库旧成绩 | AMD FirePro D700 (FireGL V) #1 | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-214811-288 | 仓库旧成绩 | AMD FirePro D700 (FireGL V) #1 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-011359-268 | 仓库旧成绩 | NVIDIA GeForce GTX 970/PCIe/SSE2 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-011344-061 | 仓库旧成绩 | NVIDIA GeForce GTX 970 | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-011328-951 | 仓库旧成绩 | NVIDIA GeForce GTX 970 (FL 12_1) | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-011313-088 | 仓库旧成绩 | NVIDIA GeForce GTX 970 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-002028-521 | 仓库旧成绩 | AMD Radeon HD 5700 Series | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260324-002013-282 | 仓库旧成绩 | AMD Radeon HD 5700 Series | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-232925-630 | 仓库旧成绩 | Radeon RX 580 Series | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-232910-298 | 仓库旧成绩 | Radeon RX 580 Series | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-232855-134 | 仓库旧成绩 | Radeon RX 580 Series (FL 12_0) | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-232839-409 | 仓库旧成绩 | Radeon RX 580 Series | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-224312-579 | 仓库旧成绩 | Radeon Vega Frontier Edition | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-224257-285 | 仓库旧成绩 | Radeon Vega Frontier Edition | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-224242-100 | 仓库旧成绩 | Radeon Vega Frontier Edition (FL 12_1) | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-224226-337 | 仓库旧成绩 | Radeon Vega Frontier Edition | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-214631-206 | 仓库旧成绩 | AMD Radeon RX 6600 XT | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-214615-776 | 仓库旧成绩 | AMD Radeon RX 6600 XT | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-214600-652 | 仓库旧成绩 | AMD Radeon RX 6600 XT (FL 12_1) | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-214544-946 | 仓库旧成绩 | AMD Radeon RX 6600 XT | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-212016-890 | 仓库旧成绩 | AMD Radeon(TM) Graphics | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-174721-797 | 仓库旧成绩 | Microsoft WARP (CPU Software Renderer) | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260323-174706-713 | 仓库旧成绩 | Microsoft WARP (CPU Software Renderer) | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260323-174651-164 | 仓库旧成绩 | AMD Radeon(TM) Graphics | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-174636-026 | 仓库旧成绩 | AMD Radeon(TM) Graphics (FL 12_1) | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-174620-443 | 仓库旧成绩 | AMD Radeon(TM) Graphics | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-174605-321 | 仓库旧成绩 | AMD Radeon RX 6900 XT | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-174549-991 | 仓库旧成绩 | AMD Radeon RX 6900 XT | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-174534-888 | 仓库旧成绩 | AMD Radeon RX 6900 XT (FL 12_1) | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-174519-387 | 仓库旧成绩 | AMD Radeon RX 6900 XT | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-162351-879 | 仓库旧成绩 | Microsoft WARP (CPU Software Renderer) | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260323-162336-789 | 仓库旧成绩 | Microsoft WARP (CPU Software Renderer) | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录；软件渲染器 |
| 20260323-162321-310 | 仓库旧成绩 | AMD Radeon(TM) Graphics | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-162306-148 | 仓库旧成绩 | AMD Radeon(TM) Graphics (FL 12_1) | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-162250-591 | 仓库旧成绩 | AMD Radeon(TM) Graphics | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-162235-434 | 仓库旧成绩 | NVIDIA GeForce RTX 5090/PCIe/SSE2 | OpenGL | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-162220-204 | 仓库旧成绩 | NVIDIA GeForce RTX 5090 | DX11 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-162205-101 | 仓库旧成绩 | NVIDIA GeForce RTX 5090 (FL 12_1) | DX12 | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
| 20260323-162149-344 | 仓库旧成绩 | NVIDIA GeForce RTX 5090 | Vulkan | 粒子吞吐（推算值） | 带渲染 | 1048576 | 0.000  | 未记录 | 旧格式，版本未记录 |
