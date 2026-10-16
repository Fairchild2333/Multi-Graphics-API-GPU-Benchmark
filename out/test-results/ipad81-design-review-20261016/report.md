# iPad8,1 剩余七项复核与测试设计修正（2026-10-16）

剩余 7 项已完成同一轮真机复核，全部 passed。当前完整选用为 31 项／33 条真实成绩，生产模型逐项匹配和质量检查通过。设备历史共 51 条，原 36 条逐字段保留。App PID 914 已停止，新的进程查询确认不存在，可以拔线。

本轮实际执行的是 4 项独立运行对照，以及修正后的 7 项复核；完整选用由既有通过记录与本轮复核合并，**没有重新点击执行整套 31 项**。首轮 Xcode 完整界面测试的失败记录没有改成通过。

## 两处设计修正

1. **iOS Vulkan 粒子计时**：此前只对纯计算强制同步，绘制仍可能沿用仅通过启动校准的 GPU 查询。现在 iOS MoltenVK 粒子的计算／绘制均整轮使用提交、栅栏完成的同步边界。App 不再复用查询计时的旧粒子记录来满足新合同，原始记录保留。Android／HarmonyOS 原生 Vulkan 和 macOS 桌面选择路径不变。

2. **CPU 全核检查**：全核吞吐由全部线程工作量除以共同实际时间得到，不要求某一个工作线程始终属于同一核型。现在用工作线程的直接 CPU 时间，验证所有轮次的大小核组实际参与和最低线程占用。每组及每线程至少 90%，异常计数和不合理超容量仍拒绝；单核 P／E 的 99% 驻留门槛不变，原逐线程核型验证也保留为详情。新增版本／验证标记，不重新判定旧记录。

本轮三轮最低 P 组占用 99.663%、E 组 96.922%、任一线程最低占用 96.503%。实际仍运行 4P＋4E 的八个工作线程，没有新增用户选项。

## 七项复核成绩

| 项目 | 成绩 | 稳定段 | 前后下降 | 状态 |
| --- | ---: | ---: | ---: | --- |
| stream-1048576 | 35.076 GB/s | 35.079 GB/s | -0.54% | passed |
| stream-4194304 | 30.661 GB/s | 30.887 GB/s | -0.72% | passed |
| stream-16777216 | 28.470 GB/s | 28.428 GB/s | 0.78% | passed |
| vulkan/stream-1048576 | 21.318 GB/s | 21.247 GB/s | 0.62% | passed |
| vulkan/stream-4194304 | 26.999 GB/s | 27.344 GB/s | -1.58% | passed |
| vulkan/stream-16777216 | 28.952 GB/s | 29.166 GB/s | -0.89% | passed |
| CPU 大核 / 小核 / 全核 | 111.076 / 60.791 / 657.976 MWork/s | — | — | passed |

## Metal 判断

没有改 Metal 内核、画面覆盖或 2% 稳定段阈值。独立运行原版 App 的对照中，Metal 1M、16M 和 CPU 已通过，4M 仍有稳定性提示但没有超过 5% 的下降；修正版这轮三档 Metal 均检测到稳定段，下降均小于 1%。这些是不同运行条件下的观察，不能据此精确量化 Xcode 工具的开销，也没有硬件频率遥测来把波动直接定性为热降频。

对照的 4M review 和所有旧成绩保留。当前选用的是完整修正轮次的最新记录，没有按最高分挑选。真实波动、计时异常或参与不足仍会在后续运行标记为 review；本次通过不构成未来每次必定稳定的保证。

## 当前完整选用成绩

| 项目 | 成绩 | API |
| --- | ---: | --- |
| stream-65536 | 13.255 GB/s | Metal |
| stream_headless-65536 | 36.787 GB/s | Metal |
| stream-1048576 | 35.076 GB/s | Metal |
| stream_headless-1048576 | 29.126 GB/s | Metal |
| stream-4194304 | 30.661 GB/s | Metal |
| stream_headless-4194304 | 29.190 GB/s | Metal |
| stream-16777216 | 28.470 GB/s | Metal |
| stream_headless-16777216 | 20.086 GB/s | Metal |
| gpu_burn-16 | 1.176 Gpix-step/s | Metal |
| gpu_burn-64 | 1.409 Gpix-step/s | Metal |
| gpu_burn-256 | 1.504 Gpix-step/s | Metal |
| cinematic_liquid | 6.060 MParticle-step/s | Metal |
| memory_sweep | 49.821 GB/s | Metal |
| vulkan/stream-65536 | 2.135 GB/s | Vulkan |
| vulkan/stream_headless-65536 | 5.475 GB/s | Vulkan |
| vulkan/stream-1048576 | 21.318 GB/s | Vulkan |
| vulkan/stream_headless-1048576 | 18.481 GB/s | Vulkan |
| vulkan/stream-4194304 | 26.999 GB/s | Vulkan |
| vulkan/stream_headless-4194304 | 26.642 GB/s | Vulkan |
| vulkan/stream-16777216 | 28.952 GB/s | Vulkan |
| vulkan/stream_headless-16777216 | 28.754 GB/s | Vulkan |
| vulkan/gpu_burn-16 | 0.549 Gpix-step/s | Vulkan |
| vulkan/gpu_burn-64 | 0.752 Gpix-step/s | Vulkan |
| vulkan/gpu_burn-256 | 0.834 Gpix-step/s | Vulkan |
| vulkan/memory_sweep | 58.051 GB/s | Vulkan |
| gpu_cache-v3-parallel512 | 379.416 ns/load | Metal |
| cpu_compute | 111.076 MWork/s | CPU |
| cpu_compute | 60.791 MWork/s | CPU |
| cpu_compute | 657.976 MWork/s | CPU |
| cpu_memory | 51.790 GB/s | CPU |
| gles2/gpu_burn-16 | 3.543 Gpix-step/s | OpenGL ES 2.0 |
| gles2/gpu_burn-64 | 10.367 Gpix-step/s | OpenGL ES 2.0 |
| gles2/gpu_burn-256 | 15.342 Gpix-step/s | OpenGL ES 2.0 |

## 证据与曲线

- [全部 51 条设备历史](phone-final/results/results.json)／[当前选用 33 条](curated-suite-results.json)／[来源映射](selected-run-sources.json)
- [七项实际清单](corrected-seven/latest.json)／[原版独立对照](standalone-baseline/latest.json)
- [核对记录](verification.json)／[生产模型与质量检查](production-contract-audit.json)
- [三轮组占用](participation-rounds.json)／[24 个线程样本](all-worker-rounds.json)
- [GPU 运行曲线](gpu-timing-curves.png)／[PDF](gpu-timing-curves.pdf)／[CSV](gpu-timing-curves.csv)
- [CPU 缓存内存曲线](cpu-memory-curves.png)／[GPU 内存](gpu-memory-curves.png)／[GPU 缓存状态](gpu-cache-curves.png)
- Release 引擎与签名 App 构建、CPU 驻留／取消检查、Swift 成绩匹配与正反例检查均通过。没有测其他设备。GPU 独立 L2／L3／SoC-SLC 归属按既定范围保持未归属。
