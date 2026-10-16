# 最新 iOS 模拟器一键全套复核 · 2026-10-17

**功能流程通过：27/27 项执行完成，28 条唯一成绩，18 项 passed、9 项 review、0 项 failed/cancelled。**
这是 iPhone 18 Pro Max / iOS 27.0 arm64 模拟器的流程验证，不是手机真机性能成绩。
本轮以 `35ea4eb` 的最新版 Release 构建运行，没有修改负载、计时、门槛或生产代码。

## 实际验证

- 新建独立模拟器；XCTest 只开启已有 Vulkan 转译开关，再点击真实“一键测试全部项目”。
- 中文按钮显示正常；没有使用自动运行、定向项目或断点续跑参数。
- 原生清单 `complete-suite-61E17192-CFE4-412A-8E83-1DAE3891076F`，时间为 2026-10-16T13:49:26Z 至 2026-10-16T14:00:59Z（原始 UTC，悉尼日期为 10 月 17 日）。
- XCTest 1/1 Passed，xcodebuild 退出 0；保存完整 native 日志和结果清单，导出点击前、完成后的截图。
- 当前生产模型逐项验证请求、版本、成绩组、正数分数、唯一 ID、结果数量及质量标记，27 项均匹配。
- CPU 算力与缓存/内存进度页为正常竖屏，截图留档；没有重现横屏黑屏或队列中断。
- 从 results.json 还原 CPU **174 点**、Metal 内存 **52 点**、Vulkan 内存 **77 点**；页面首次访问与存储明细均存在。
- Vulkan 粒子和 Burn 均保存 `synchronized_wall_clock` 计时标记。

## V3 小容量边界

本环境 Vulkan 单缓冲上限为 256 MiB。新记录使用
`membw_v3_streaming_ios_preview_moltenvk`，完整遍历标记存在，34 次数据校验通过、
77 点保存完整，但尾部 128/192/256 MiB 的方法混合，`memoryPlateauReached=0`。
App 正确标记“尚未达到内存带宽平台”并继续 CPU/GLES2 项目，没有误当正式通过。

其余 8 项 review 为稳定性/吞吐变化提示，原样保留；模拟器流程验收允许这些
质量提示，不把它们变成真机通过或设备降频结论，也没有为它们反复挑分。
能力清单仍为 27 项：不支持的 Vulkan 16M、流体、独立缓存诊断、GLES2 256 档未强行运行。
CPU 遍历宿主逻辑处理器，不能据此验证手机大核/小核归属。

## 数据边界与资料

- 五台 Apple 设备的主历史与本机实时 results.json 哈希均未变化；Mac 仍为 169 条。
- 模拟器成绩单独保存在本目录，未混入五台真实设备的成绩索引。
- A16 未完成项目、iPad8,1 Metal 16M 和各手机/iPad 的 V3 内存真机补测仍按主文档执行。
- `final-documents/`：完整成绩、原生计划、逐项状态、执行日志。
- `production-contract-audit.json` / `.log`：实际生产模型的匹配及曲线恢复检查。
- `ui-test.log`、`xcresult-summary.json`、`attachments/`、过程 PNG：一键实际执行及画面证据。
- `environment.json`、`test-command.json`、`package-provenance.json`、`verification.json`、前后历史校验：范围与可追溯信息。
- 完整 `ui-full.xcresult` 及编译产物继续留在本地；可读日志、原始 JSON 与截图入库。

## 逐项状态

| 项目 | 状态 | 提示 |
| --- | --- | --- |
| stream-65536 | passed | — |
| stream_headless-65536 | passed | — |
| stream-1048576 | review | 尚未达到稳定平台 |
| stream_headless-1048576 | passed | — |
| stream-4194304 | review | 尚未达到稳定平台 |
| stream_headless-4194304 | review | 尚未达到稳定平台; 运行中吞吐下降超过 5% |
| stream-16777216 | review | 尚未达到稳定平台 |
| stream_headless-16777216 | review | 尚未达到稳定平台; 运行中吞吐下降超过 5% |
| gpu_burn-16 | passed | — |
| gpu_burn-64 | review | 尚未达到稳定平台 |
| gpu_burn-256 | review | 尚未达到稳定平台 |
| cinematic_liquid | passed | — |
| memory_sweep | passed | — |
| vulkan/stream-65536 | passed | — |
| vulkan/stream_headless-65536 | passed | — |
| vulkan/stream-1048576 | passed | — |
| vulkan/stream_headless-1048576 | passed | — |
| vulkan/stream-4194304 | passed | — |
| vulkan/stream_headless-4194304 | passed | — |
| vulkan/gpu_burn-16 | review | 尚未达到稳定平台 |
| vulkan/gpu_burn-64 | passed | — |
| vulkan/gpu_burn-256 | passed | — |
| vulkan/memory_sweep | review | 尚未达到内存带宽平台 |
| cpu_compute | passed | — |
| cpu_memory | passed | — |
| gles2/gpu_burn-16 | passed | — |
| gles2/gpu_burn-64 | passed | — |
