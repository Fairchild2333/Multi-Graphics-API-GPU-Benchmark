# macOS 完整测试：Metal 与 Vulkan（MoltenVK）

**2026-10-14 判读更正：Mac 的旧 Metal 1M／4M／16M 绘制、GPU Burn 三档总命令跨度超过帧间隔，流体计算和绘制区间重叠。这些计时需复测，下面原有分数百分比仅保留追溯，不能视为真实 API 速度优势。请参阅[全部设备对比报告](../all-device-comparison-20261014/report.md)。**

测试日期：2026-10-13（Australia/Sydney）。设备：Apple M4 Pro，Mac16,8，48 GiB 统一内存，macOS 27.0.1，ARM64。测试时接通电源。

GUI 完整流程 **31/31 成功**：Vulkan 13、Metal 13、OpenGL 3、CPU 2。产生 32 条成绩；CPU 算力会保存单核和多核两条记录。8 个原生 Metal 抓帧全部存在。

实测中发现 Vulkan 的 1280×720 窗口在 Retina 上实际生成 2560×1440 渲染目标。修正后重新验证 Vulkan 有画面 8/8 项；再完成 Metal 无抓帧对照 8/8 项。修正后的两条路径均为 1280×720，关闭垂直同步、关闭抓帧，每项运行 15 秒，排除约 2 秒预热。纯计算、显存、OpenGL、CPU 使用本轮完整流程的结果。

原始新增成绩 48 条均保留在 results.json；本报告选择 32 条代表 31 个项目。旧的 Retina 尺寸不一致成绩和带抓帧的初轮 Metal 有画面成绩不进入下面的性能对比。修正后的 Vulkan 有画面成绩使用独立版本后缀 `_viewport1280x720_v1`。

## 同项目性能对比

粒子 GB/s 是按每粒子 40 字节和 GPU compute 时间推算的吞吐指标，不是硬件显存带宽。GPU Burn 的单位是 Gpix-step/s。表中的百分比为 Vulkan ÷ Metal − 1，正数表示本轮 Vulkan 得分更高。

| 项目 | Metal | Vulkan / MoltenVK | 单位 | Vulkan 相对 Metal |
|---|---:|---:|---|---:|
| 粒子 65K · 有画面 | 151.353 | 50.542 | GB/s | -66.6% |
| 粒子 1M · 有画面 | 52.401 | 85.926 | GB/s | +64.0% |
| 粒子 4M · 有画面 | 54.918 | 132.732 | GB/s | +141.7% |
| 粒子 16M · 有画面 | 57.878 | 127.469 | GB/s | +120.2% |
| 粒子 65K · 纯计算 | 310.199 | 239.127 | GB/s | -22.9% |
| 粒子 1M · 纯计算 | 146.696 | 219.681 | GB/s | +49.8% |
| 粒子 4M · 纯计算 | 142.220 | 145.311 | GB/s | +2.2% |
| 粒子 16M · 纯计算 | 143.277 | 133.438 | GB/s | -6.9% |
| GPU Burn 16 步 | 4.421 | 5.879 | Gpix-step/s | +33.0% |
| GPU Burn 64 步 | 4.517 | 6.460 | Gpix-step/s | +43.0% |
| GPU Burn 256 步 | 4.386 | 6.888 | Gpix-step/s | +57.0% |

这些是本应用两条执行路径的单轮结果，差异包括着色器实现、编译优化、调度和呈现方式，不能直接把百分比解释成 MoltenVK 的纯转译成本。未做跨运行顺序或长期温控的统计验收。

## 显存 / 统一内存扫描

- Vulkan 读带宽：**238.796 GB/s**；扫描到 2 GiB，达到内存平台，GPU timestamp，10 次测量取中位数，timestamp anomalies 为 0。
- Metal 原生预览读带宽：**244.776 GB/s**；粒子衍生固定网格扫描到 512 MiB，5 次取最好值，尚未验证内存平台。
- 这两项内核、扫描上限和统计方法不同，数值分别展示，不计算直接性能百分比。Apple 芯片的 CPU 和 GPU 使用同一套统一内存。

## 流体

同为 1280×720、15 秒、MLS-MPM 场景：Metal **36.987 MParticle-step/s**、平均 **20.7 FPS**；Vulkan **67.361 MParticle-step/s**、平均 **20.7 FPS**。Metal 使用独立 `_metal_preview` 计时/呈现版本，保留分组，以上作为两条实现路径的观察值。

## OpenGL 与 CPU

- OpenGL GPU Burn 16 步：5.243 Gpix-step/s（OpenGL 4.1，同步墙钟计时版本）。
- OpenGL GPU Burn 64 步：5.967 Gpix-step/s（OpenGL 4.1，同步墙钟计时版本）。
- OpenGL GPU Burn 256 步：5.924 Gpix-step/s（OpenGL 4.1，同步墙钟计时版本）。
- cpu_single_core：307.581 MWork/s。
- cpu_multi_core：3010.644 MWork/s。
- cpu_membw：245.834 GB/s。

## 文件与验证

- 共用成绩文件：`/Users/fairchild/Library/Application Support/GpuComputeBenchmark/results/results.json`。
- 本轮证据目录：`/Users/fairchild/VSCode Projects/Vulkan-GPU-Compute-Microbenchmark/out/test-results/macos-complete-20261013`，包含前后成绩、流程记录、渲染尺寸探针、8 项 Vulkan 复测日志、8 项 Metal 对照日志、逐项结果 ID、CSV 对比与选择后的 JSON。
- 修正后的 Apple Silicon / Intel macOS 构建通过；Intel 为交叉构建。CTest 3/3 通过。
- 本次没有重新运行 iOS 性能测试。
