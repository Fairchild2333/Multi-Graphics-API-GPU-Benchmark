# 11 英寸 A12X iPad 真机一键测试（2026-10-16）

设备：Van 的 iPad，iPad Pro 11 英寸（iPad8,1），iPadOS 26.7.1。它与此前测试的 12.9 英寸 iPad8,7 是两台不同的 A12X 设备。

**实际通过 XCTest 点击首页“一键测试全部项目”，完整执行 31/31 项，保存 33 条成绩。** 首轮 22 项 passed、9 项 review，无执行失败。严格界面测试要求全部 passed，因此该 XCTest 用例未通过，不能声称完整质量验收通过。

随后发现 Vulkan 1M／4M 纯计算的时间戳严重少算。修正 iOS 转译纯计算的整轮计时，并实际补测 1M／4M／16M 三档，全部 passed。当前 31 项／33 条选用结果为 24 项 passed、7 项 review，生产模型合同核对通过。**修正后没有重新点击完整 31 项；这份选用清单来自首轮按钮流程加三项真实补测。**

当前设备历史共 36 条，首轮 33 条逐字段保留，包括异常时间戳观察。没有按最高分挑选，异常查询计时不进入当前选用清单。测试 App 已停止（PID 873），确认进程不存在，可以拔线。

## Auto 语言

iOS 原来从 Locale.current 推断语言，安装包也未声明支持语言；现在与 macOS 一样读取首选语言列表，并声明英文、简体中文与日文。启动时恢复已保存的语言选择。没有强制传入中文启动参数：界面测试实际读到“一键测试全部项目”；补测运行日志也确认 setting=auto、preferred=zh-Hans-AU、effective=zh。

## CPU 与内存

| 项目 | 成绩 | 条件 |
| --- | ---: | --- |
| 大核 | 110.384 MWork/s | 已通过单线程核心类型驻留验证 |
| 小核 | 60.661 MWork/s | 已通过单线程核心类型驻留验证 |
| 全核 | 655.509 MWork/s | 8 线程；第二轮部分 P 线程驻留不足 99%，保留 review |
| CPU 全核内存读取 | 51.790 GB/s | 大小核缓存曲线、页面首次访问和文件存储已测；E 为后台策略 |
| Vulkan GPU 内存读取 | 58.051 GB/s | 整轮同步计时；34 次写入／复制检查 |

全核选中轮的线程贡献相加等于 655.509 MWork/s，三轮均使用 8 个工作线程。第二轮 P 线程最低类型驻留 98.372%，因此完整三轮的类型检查没有全部通过；不把这一提示改为通过。小核单线程 Utility 无辅助线程测试通过；CPU 小核缓存／内存仍为后台策略，不当作正常频率峰值。

## Vulkan 纯计算计时修正

启动时的多次 dispatch 校准能够通过，但 1M／4M 正式帧中查询值分别只有 0.067／0.343 ms，对比实际帧间隔 1.539／5.842 ms，形成约 625／489 GB/s 的可疑高分。iOS MoltenVK 纯计算现与已有 Burn 路径一样，整轮使用提交／栅栏完成的同步耗时。App 不再把旧查询计时记录复用为新版纯计算的通过成绩。Android／HarmonyOS 原生 Vulkan 和 macOS 桌面路径未变。

| 规格 | 修正后成绩 | 同步计算 / 实际帧间隔 | 稳定段 |
| --- | ---: | ---: | ---: |
| 1,048,576 | 18.481 GB/s | 2.270 / 2.283 ms | 18.206 GB/s |
| 4,194,304 | 26.642 GB/s | 6.297 / 6.313 ms | 26.632 GB/s |
| 16,777,216 | 28.754 GB/s | 23.339 / 23.359 ms | 28.757 GB/s |

## 当前完整选用成绩

粒子 GB/s 是既定工作量吞吐指数，不直接代表物理显存总线带宽。GPU 缓存诊断仍明确标为未归属，不声称独立测出 L2／L3／SoC-SLC。

| 项目 | 状态 | 成绩 | 提示 |
| --- | --- | --- | --- |
| stream-65536 | passed | 13.255 GB/s |  |
| stream_headless-65536 | passed | 36.787 GB/s |  |
| stream-1048576 | review | 19.061 GB/s | 尚未达到稳定平台 |
| stream_headless-1048576 | passed | 29.126 GB/s |  |
| stream-4194304 | review | 25.943 GB/s | 尚未达到稳定平台; 运行中吞吐下降超过 5% |
| stream_headless-4194304 | passed | 29.190 GB/s |  |
| stream-16777216 | review | 26.915 GB/s | 尚未达到稳定平台 |
| stream_headless-16777216 | passed | 20.086 GB/s |  |
| gpu_burn-16 | passed | 1.176 Gpix-step/s |  |
| gpu_burn-64 | passed | 1.409 Gpix-step/s |  |
| gpu_burn-256 | passed | 1.504 Gpix-step/s |  |
| cinematic_liquid | passed | 6.060 MParticle-step/s |  |
| memory_sweep | passed | 49.821 GB/s |  |
| vulkan/stream-65536 | passed | 2.135 GB/s |  |
| vulkan/stream_headless-65536 | passed | 5.475 GB/s |  |
| vulkan/stream-1048576 | review | 32.032 GB/s | 尚未达到稳定平台 |
| vulkan/stream_headless-1048576 | passed | 18.481 GB/s |  |
| vulkan/stream-4194304 | review | 25.133 GB/s | 尚未达到稳定平台 |
| vulkan/stream_headless-4194304 | passed | 26.642 GB/s |  |
| vulkan/stream-16777216 | review | 25.662 GB/s | 尚未达到稳定平台 |
| vulkan/stream_headless-16777216 | passed | 28.754 GB/s |  |
| vulkan/gpu_burn-16 | passed | 0.549 Gpix-step/s |  |
| vulkan/gpu_burn-64 | passed | 0.752 Gpix-step/s |  |
| vulkan/gpu_burn-256 | passed | 0.834 Gpix-step/s |  |
| vulkan/memory_sweep | passed | 58.051 GB/s |  |
| gpu_cache-v3-parallel512 | passed | 379.416 ns/load |  |
| cpu_compute | review | 110.384 MWork/s / 60.661 MWork/s / 655.509 MWork/s | 全核线程的驻留或占用未全部验证 |
| cpu_memory | passed | 51.790 GB/s |  |
| gles2/gpu_burn-16 | passed | 3.543 Gpix-step/s |  |
| gles2/gpu_burn-64 | passed | 10.367 Gpix-step/s |  |
| gles2/gpu_burn-256 | passed | 15.342 Gpix-step/s |  |

## 文件与验证边界

- [最终完整 36 条历史](phone-final/results/results.json)／[当前选用 33 条](curated-suite-results.json)／[选用来源](selected-run-sources.json)
- [原始按钮流程清单](ui-watch-01/latest.json)／[三档补测清单](headless-fix-01/latest.json)
- [核对记录](verification.json)／[生产模型匹配](production-contract-audit.json)
- [实际点击日志](ui-click.log)：ONE_BUTTON_READY、Tap、ONE_BUTTON_TAPPED 均保留。
- XCTest 用例结束后，Xcode 卡在测试日志收尾，已终止该宿主。xcresult 未完整封装，截图导出不可用；不把这个文件夹当成成功的验收证据。原生清单、日志和设备成绩均已独立保存。
- Release 引擎与签名 App 构建通过，更新的原始／同步计时复用检查通过；三档修正后真机测试均通过。没有重跑其他设备。
- 剩余 7 个 review：Metal 1M／4M／16M 绘制，Vulkan 1M／4M／16M 绘制，以及 CPU 全核驻留。没有继续重复测试挑分。

## 曲线

- [CPU 缓存／内存 PNG](cpu-memory-curves.png)／[PDF](cpu-memory-curves.pdf)／[CSV](cpu-memory-curves.csv)
- [GPU 内存 PNG](gpu-memory-curves.png)／[PDF](gpu-memory-curves.pdf)／[CSV](gpu-memory-curves.csv)
- [GPU 缓存状态 PNG](gpu-cache-curves.png)／[PDF](gpu-cache-curves.pdf)／[CSV](gpu-cache-curves.csv)
- [GPU 计时曲线 PNG](gpu-timing-curves.png)／[PDF](gpu-timing-curves.pdf)／[CSV](gpu-timing-curves.csv)
