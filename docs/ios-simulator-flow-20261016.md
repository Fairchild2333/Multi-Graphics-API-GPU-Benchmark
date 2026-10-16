# iOS 模拟器完整一键流程复核（2026-10-16）

当时版本在 iPhone 18 Pro / iOS 27.0 / arm64 模拟器上，通过 XCTest
点击真实“一键测试全部项目”按钮，完整流程验收通过。

后续在真机 iPhone 14 Pro Max 发现大屏横屏导航切换问题，现已修复，并在
iPhone 18 Pro Max 模拟器通过短横屏回归；见 [A16 后续记录](a16-partial-run-20261016.md)。
以下完整 27 项记录属于标准尺寸模拟器，不能将它当作该后续修复的重新完整验收。

- Release 构建；仅开启已有 Vulkan 转译开关，没有自动启动、定向项目或续跑参数。
- 本环境支持的 27 项全部执行，保存 28 条唯一成绩：19 项 passed、8 项 review、0 项 failed。
- XCTest 1/1 Passed，xcodebuild 退出 0，完整 xcresult 及点击前后截图可读取。
- 生产模型逐项核对请求、成绩组、版本、质量标记和结果 ID，全部通过。
- 保存文件可恢复 CPU 174 个曲线点、Metal 内存 52 点、Vulkan 内存 77 点；虚拟内存缺页与存储明细均保存。
- CPU 算力与缓存/内存阶段正常显示竖屏进度页，没有复现横屏黑屏。
- Vulkan 6 个粒子项目均使用修正后的同步计时路径。
- 共享模型检查 154/154 通过，6 份共享 Apple 文件一致。

本轮未发现新的执行、漏存或切换流程缺陷。补齐了 macOS 语言选项的本地化，
与 iOS 一致显示“跟随系统”；没有修改测试负载、计时或质量阈值。

8 项 review 都是吞吐未稳定或吞吐下降提示，保留原始状态。
模拟器流程验收允许 review，真机严格通过标准保持原样。
本轮不能用于确认真机速度、大小核归属或独立缓存层级归属。

模拟器清单按能力生成：Vulkan 最大 8,388,608 粒子，排除 16M 两项；
Vulkan 液体、独立 GPU 缓存诊断和 GLES2 256 档也不在本环境支持清单中。
CPU 遍历宿主机 14 个逻辑处理器，并非 iPad 的核心拓扑。

模拟器已关闭。A15、旧 iPad8,7、新 iPad8,1 的真机历史文件哈希均未改变。
本轮完整原始资料位于 `out/test-results/ios-simulator-full-suite-20261016/`，
包括 `report.md`、`verification.json`、`production-contract-audit.json`、
`final-documents/`、`ui-full.xcresult` 和 `attachments/`。

新 iPad8,1 真机复核仍以 [真机设计复核记录](ipad81-design-review-20261016.md) 为准。

原始 JSON、曲线、独立文字日志和导出截图已按 [归档策略](test-result-archives.md)
纳入 Git；完整 `.xcresult` bundle 仍在本地，可读摘要和附件另行保存。
