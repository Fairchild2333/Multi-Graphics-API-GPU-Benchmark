# iOS 模拟器完整一键流程验收 · 2026-10-16

这是模拟器的功能流程验证，不是 iPhone/iPad 真机性能成绩。

- iPhone 18 Pro 模拟器，iOS 27.0，arm64；Release 构建。
- XCTest 仅开启已有转译开关，点击真实 `complete-suite-all` 按钮；无自动运行或定向项目参数，无断点续跑。
- 原生清单 `complete-suite-E7AD6C20-DBEA-4736-9CBB-20A12736D278`，2026-10-16T09:11:36Z 至 2026-10-16T09:23:15Z。
- 27/27 项执行完成，28 条唯一成绩；19 项 passed、8 项 review、0 项 failed。
- XCTest 1/1 Passed，xcodebuild 退出 0；完整 xcresult 和两张附件截图已导出。
- 当前生产模型逐项匹配请求、成绩组、版本、正数分数、质量标记和唯一结果 ID，全部通过。
- 从保存文件恢复：CPU 174 个曲线点、Metal 内存 52 点、Vulkan 内存 77 点；虚拟内存缺页与存储明细均存在。
- CPU 算力与缓存/内存过程截图确认竖屏进度页正常。
- Vulkan 6 个粒子项目均为 synchronized_wall_clock。
- 共享模型检查 154/154 通过，6 份共享 Apple 文件一致。
- 本轮仅补齐 macOS 的 Auto 本地化，与 iOS 统一显示“跟随系统”；未改变测试负载、计时或质量阈值。
- 模拟器已关闭。A15 128 条、旧 iPad8,7 72 条、新 iPad8,1 51 条真机历史 SHA256 均未变。

## 范围与限制

清单按运行时能力生成。此模拟器 Vulkan 单缓冲上限 256 MiB，对应最多 8,388,608 粒子，故不加入 16M Vulkan 两项；Vulkan 液体、独立 GPU 缓存诊断及 GLES2 256 档也不在本环境能力清单中。CPU 在模拟器内遍历宿主机 14 个逻辑处理器，不提供 iPad 大小核归属验证。

8 项 review 均是未达到稳定吞吐或吞吐下降的质量提示，保留原始状态。流程验收允许模拟器 review；真机的严格 passed 门槛未降低。本报告不代替真机性能复测。

## 原始资料

- `final-documents/results/results.json`：28 条模拟器成绩。
- `final-documents/suite-runs/`：原生计划、逐项状态和日志。
- `ui-full.xcresult`、`xcresult-summary.json`、`ui-test.log`：完整 UI 验收证据。
- `attachments/manifest.json`：点击前与完成后的截图。
- `production-contract-audit.json`：生产模型/持久化曲线检查。
- `verification.json`、`hardware-histories-before.json`、`hardware-histories-after.json`：归档核验。

## 各项目状态

| 项目 | 状态 | 复核原因 |
| --- | --- | --- |
| stream-65536 | passed |  |
| stream_headless-65536 | passed |  |
| stream-1048576 | review | Stable throughput was not reached |
| stream_headless-1048576 | passed |  |
| stream-4194304 | review | Stable throughput was not reached |
| stream_headless-4194304 | passed |  |
| stream-16777216 | review | Stable throughput was not reached |
| stream_headless-16777216 | review | Stable throughput was not reached |
| gpu_burn-16 | review | Stable throughput was not reached; Throughput fell by more than 5% |
| gpu_burn-64 | review | Stable throughput was not reached |
| gpu_burn-256 | passed |  |
| cinematic_liquid | passed |  |
| memory_sweep | passed |  |
| vulkan/stream-65536 | passed |  |
| vulkan/stream_headless-65536 | passed |  |
| vulkan/stream-1048576 | passed |  |
| vulkan/stream_headless-1048576 | passed |  |
| vulkan/stream-4194304 | passed |  |
| vulkan/stream_headless-4194304 | passed |  |
| vulkan/gpu_burn-16 | review | Stable throughput was not reached |
| vulkan/gpu_burn-64 | passed |  |
| vulkan/gpu_burn-256 | passed |  |
| vulkan/memory_sweep | passed |  |
| cpu_compute | passed |  |
| cpu_memory | passed |  |
| gles2/gpu_burn-16 | passed |  |
| gles2/gpu_burn-64 | review | Stable throughput was not reached |
