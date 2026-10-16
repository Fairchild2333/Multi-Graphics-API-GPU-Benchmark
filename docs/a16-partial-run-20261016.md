# iPhone 14 Pro Max / A16 部分测试（2026-10-16）

用户需要使用手机，已拔线。测试 App 已在拔线前终止，新的进程查询确认不在运行；
全部已产生的原始成绩与日志均已保存。**不要自动重启测试。**

- 设备 iPhone15,3，iOS 27.0.1，6 核、512 GB。
- 完整清单 31 项／33 条成绩；已完成 22 项：19 passed、3 review。
- 第 23 项 Vulkan GPU Burn 64 取消；另 8 项未执行；0 项 failed。
- 尚未执行：Vulkan GPU Burn 256、Vulkan 内存、GPU 缓存诊断、CPU 算力、
  CPU 缓存/内存/虚拟内存/存储，以及 GLES2 GPU Burn 16/64/256。
- 稳定性待复核：Metal GPU Burn 256 前后吞吐下降 16.69%；Vulkan 1M 绘制
  未满足稳定窗口要求；Vulkan GPU Burn 16 前后吞吐下降 19.08%。
- 原始历史 24 条：22 条完整成绩及 2 条中断诊断；中断记录不纳入完整成绩。

## 三项稳定性警告的含义

| 项目 | 整轮均值 | 曾检测到的稳定窗口 | 警告 |
| --- | ---: | ---: | --- |
| Metal GPU Burn 256（重负载 GPU 算力） | 1.269 Gpix-step/s | 1.188 Gpix-step/s | 前后吞吐下降 16.69% |
| Vulkan GPU Burn 16（轻负载 GPU 算力） | 0.755 Gpix-step/s | 0.836 Gpix-step/s | 前后吞吐下降 19.08% |
| Vulkan 1M 粒子绘制 | 15.218 GB/s | 未检出 | 计算阶段吞吐持续波动 |

下降幅度比较最早与最后各 5 个计时窗口的平均吞吐；超过 5% 时标记 review。
稳定窗口要求连续 5 个窗口的变异系数小于 2%。稳定窗口可能位于运行早期，
不保证代表结束时性能，需同时保留整轮均值和曲线。

两项 GPU Burn 确实在运行中变慢，但没有直接频率遥测，不能据此确定降频原因。
三项系统热状态均为 0→0，也不能据此排除功耗或调度变化。Vulkan 1M 的画面约
39 FPS，主要波动在用于计算吞吐分数的计算阶段；未稳定不代表成绩为零或运行失败。

这些成绩和警告可以保留使用。无需为消除提示而反复重跑；需要稳定性能对比时，
才考虑做一次对照，保留原始轮次。软件计时、导航问题与运行稳定性分开记录。

## 大屏手机导航修复

发现并修复大屏 iPhone 横屏时的导航树切换问题：原有 regular 宽度判断会将
TabView 替换为 NavigationSplitView，拆除测试页面并取消流程。现在仅 iPad
使用该侧栏分支，iPhone 保持 TabView。修复版真机完成前 22 项，未再触发
页面拆除取消；第二次取消原因是显式 `user`，与首轮不同。
新增大屏 iPhone 模拟器短回归：真实点击全套按钮，确认横屏保持全屏，再主动
取消并确认状态；XCTest 1/1 Passed。未修改测试负载、计时或质量阈值。

原始资料：`out/test-results/a16-full-suite-20261016/`，包含 `report.md`、
`verification.json`、`completed-results.json`、`phone-final/`、两个真实运行
清单和 `max-navigation-regression/`。其他设备的历史哈希均未改变。

以上小体积原始 JSON、完整文字日志、导出截图和曲线资料现已纳入 Git；原路径和
字节保持不变。大型 Xcode bundle 仍在本地，已上传其导出摘要与截图。见
[归档策略](test-result-archives.md) 和 [五台 Apple 设备历史索引](../out/test-results/README.md)。

## 已完成项目的成绩

下表仅列本轮完整保存的 22 项；中断诊断记录不在其中，CPU 没有成绩。
均值与原始结果保持一致，review 标记保留，不按最高分筛选。

| 项目 | 整轮均值 | 状态 | 原始结果编号 |
| --- | ---: | --- | --- |
| stream-65536 | 7.051 GB/s | passed | `20261016-212124-848` |
| stream_headless-65536 | 64.556 GB/s | passed | `20261016-212140-771` |
| stream-1048576 | 25.466 GB/s | passed | `20261016-212156-664` |
| stream_headless-1048576 | 27.233 GB/s | passed | `20261016-212212-694` |
| stream-4194304 | 25.673 GB/s | passed | `20261016-212228-711` |
| stream_headless-4194304 | 27.423 GB/s | passed | `20261016-212244-710` |
| stream-16777216 | 25.268 GB/s | passed | `20261016-212301-414` |
| stream_headless-16777216 | 27.525 GB/s | passed | `20261016-212317-744` |
| gpu_burn-16 | 1.365 Gpix-step/s | passed | `20261016-212334-016` |
| gpu_burn-64 | 1.531 Gpix-step/s | passed | `20261016-212350-099` |
| gpu_burn-256 | 1.269 Gpix-step/s | review | `20261016-212406-668` |
| cinematic_liquid | 9.142 MParticle-step/s | passed | `20261016-212424-771` |
| memory_sweep | 40.957 GB/s | passed | `20261016-212427-313-metal-memsweep` |
| vulkan/stream-65536 | 2.122 GB/s | passed | `20261016-212445-219` |
| vulkan/stream_headless-65536 | 3.764 GB/s | passed | `20261016-212501-194` |
| vulkan/stream-1048576 | 15.218 GB/s | review | `20261016-212517-195` |
| vulkan/stream_headless-1048576 | 17.099 GB/s | passed | `20261016-212533-203` |
| vulkan/stream-4194304 | 22.833 GB/s | passed | `20261016-212549-272` |
| vulkan/stream_headless-4194304 | 23.928 GB/s | passed | `20261016-212605-300` |
| vulkan/stream-16777216 | 24.937 GB/s | passed | `20261016-212622-347` |
| vulkan/stream_headless-16777216 | 26.432 GB/s | passed | `20261016-212638-688` |
| vulkan/gpu_burn-16 | 0.755 Gpix-step/s | review | `20261016-212654-894` |

## 后续安排

下次用户明确要求继续这台设备时，优先补齐 1 项取消和 8 项未运行，共 9 项。
3 项 review 已有成绩，是否增加一次对照由稳定性能比较的需要决定。
现有默认续跑策略仅复用 passed，因此直接恢复整套会运行 12 项；这属于保守的
自动恢复策略，不表示三项 warning 必须重测。按未完成项目定向继续时可保留全部
22 项已完成记录。当前不能称为完整 31 项通过。
