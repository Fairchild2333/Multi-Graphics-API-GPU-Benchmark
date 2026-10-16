# iPhone 14 Pro Max / A16 部分测试记录 · 2026-10-16

用户需要使用手机，测试已停止，App PID 722 已终止，新的进程查询确认 App 不在运行。不要自动恢复本轮。

## 当前进度

- 设备 iPhone15,3，iPhone 14 Pro Max，iOS 27.0.1，6 核、512 GB。
- 完整能力清单 31 项，计划保存 33 条成绩。
- 完成 22 项：19 passed、3 review；第 23 项取消，剩余 8 项未运行，0 项 failed。
- CPU 算力与 CPU 缓存/内存/虚拟内存/存储尚未执行；GPU 缓存诊断和 Vulkan 内存也尚未执行。
- 原始历史 24 条，其中 22 条完整项目成绩、2 条中断诊断记录。中断成绩不纳入完整选用结果。
- `completed-results.json` 仅收录本轮已完成的 22 条真实成绩，标明原生状态和源清单。
- `phone-final/` 保存原始 Documents 与所有清单/日志；其他设备历史文件哈希均未改变。

## 已修复的大屏手机导航问题

第一次尝试在首项旋转横屏时触发 `surface_disappeared`。大屏 iPhone 横屏会进入 regular 宽度，原有 ContentView 会从 TabView 切换到 NavigationSplitView，拆除承载测试的页面。

现只让 iPad 在 regular 宽度使用侧栏，iPhone 保持 TabView。修复版真机连续完成前 22 项，未再出现页面拆除取消；第二次在第 23 项收到显式 user 取消，与首次原因不同。大屏 iPhone 模拟器的短回归测试通过（真实点击全套按钮、确认横屏仍显示测试、主动取消并核对取消状态），完整 xcresult 与截图在 `max-navigation-regression/`。该模拟器的中断记录不属于真机成绩。

## 仍需复核或继续的项目

3 项已完成但有质量提示：
- gpu_burn-256：前后吞吐下降 16.69%；均值 1.269、稳定窗口 1.188 Gpix-step/s
- vulkan/stream-1048576：均值 15.218 GB/s；计算吞吐未满足连续 5 窗口 CV<2% 的稳定性要求
- vulkan/gpu_burn-16：前后吞吐下降 19.08%；均值 0.755、稳定窗口 0.836 Gpix-step/s

尚未完成的 9 项（1 项取消、8 项未运行）：
- vulkan/gpu_burn-64：cancelled
- vulkan/gpu_burn-256：not_run
- vulkan/memory_sweep：not_run
- gpu_cache-v2：not_run
- cpu_compute：not_run
- cpu_memory：not_run
- gles2/gpu_burn-16：not_run
- gles2/gpu_burn-64：not_run
- gles2/gpu_burn-256：not_run

后续经用户要求继续时，优先补齐 1 项取消和 8 项未运行，共 9 项。3 项 review 已有成绩，保留均值、警告和曲线；需要稳定性能对比时再考虑一次对照，不为消除提示反复重测。默认完整续跑仅复用 passed，故会运行 12 项，这是恢复策略而非三项必须重测。当前不是完整 31 项通过。

## 全部项目及成绩

| 项目 | 状态 | 成绩 | 原因 |
| --- | --- | --- | --- |
| stream-65536 | passed | 7.051 GB/s |  |
| stream_headless-65536 | passed | 64.556 GB/s |  |
| stream-1048576 | passed | 25.466 GB/s |  |
| stream_headless-1048576 | passed | 27.233 GB/s |  |
| stream-4194304 | passed | 25.673 GB/s |  |
| stream_headless-4194304 | passed | 27.423 GB/s |  |
| stream-16777216 | passed | 25.268 GB/s |  |
| stream_headless-16777216 | passed | 27.525 GB/s |  |
| gpu_burn-16 | passed | 1.365 Gpix-step/s |  |
| gpu_burn-64 | passed | 1.531 Gpix-step/s |  |
| gpu_burn-256 | review | 1.269 Gpix-step/s | 运行中吞吐下降超过 5% |
| cinematic_liquid | passed | 9.142 MParticle-step/s |  |
| memory_sweep | passed | 40.957 GB/s |  |
| vulkan/stream-65536 | passed | 2.122 GB/s |  |
| vulkan/stream_headless-65536 | passed | 3.764 GB/s |  |
| vulkan/stream-1048576 | review | 15.218 GB/s | 尚未达到稳定平台 |
| vulkan/stream_headless-1048576 | passed | 17.099 GB/s |  |
| vulkan/stream-4194304 | passed | 22.833 GB/s |  |
| vulkan/stream_headless-4194304 | passed | 23.928 GB/s |  |
| vulkan/stream-16777216 | passed | 24.937 GB/s |  |
| vulkan/stream_headless-16777216 | passed | 26.432 GB/s |  |
| vulkan/gpu_burn-16 | review | 0.755 Gpix-step/s | 运行中吞吐下降超过 5% |
| vulkan/gpu_burn-64 | cancelled |  |  |
| vulkan/gpu_burn-256 | not_run |  |  |
| vulkan/memory_sweep | not_run |  |  |
| gpu_cache-v2 | not_run |  |  |
| cpu_compute | not_run |  |  |
| cpu_memory | not_run |  |  |
| gles2/gpu_burn-16 | not_run |  |  |
| gles2/gpu_burn-64 | not_run |  |  |
| gles2/gpu_burn-256 | not_run |  |  |
