# Benchmark results, curves and native logs

小体积成绩、完整曲线、原生运行日志、报告和导出截图现在纳入 Git。
本目录保留原始路径，便于查阅旧文档链接；JSON/CSV 中的曲线点与日志均不改写。
归档文件禁用换行转换，保持提交内容与本地原件逐字节一致。

## Apple 设备主历史快照

| 设备 | 型号 | 历史条数 | 原始文件 | 说明 |
| --- | --- | ---: | --- | --- |
| Mac M4 Pro | Mac16,8 | 155 | [最新主历史](macos-targeted-retest-20261016/results-after.json) | 新增分支保留版 CPU 3 条、粒子复核 2 条、Vulkan v2 内存 preview 1 条；旧 149 条不变。 |
| iPhone 13 Pro / A15 | iPhone14,2 | 128 | [results.json](a15-cache-isolation-20261014/phone-final/results/results.json) | 历史与缓存专项轮次；当前验收范围见 Apple 状态文档。 |
| 12.9-inch iPad Pro / A12X | iPad8,7 | 72 | [results.json](a12x-targeted-retest-20261014/phone-final/results/results.json) | 原 43 条保留；补测后 72 条，Metal 16M 稳定性提示仍保留。 |
| 11-inch iPad Pro / A12X | iPad8,1 | 51 | [results.json](ipad81-design-review-20261016/phone-final/results/results.json) | 51 条历史；当前完整选用 31 项/33 条来自不同真实轮次。 |
| iPhone 14 Pro Max / A16 | iPhone15,3 | 24 | [results.json](a16-full-suite-20261016/phone-final/results/results.json) | 24 条历史=22 条完整成绩+2 条中断诊断。19 passed、3 review；CPU 尚未测。 |

精确字节数及 SHA256 见 [archive-index.json](archive-index.json)。
Mac 的现有导出报告一并备份在 [macos-history-20261016/reports](macos-history-20261016/reports)。
Mac 最新定向补测见 [报告、曲线、原生日志和 iOS 构建验证](macos-targeted-retest-20261016/report.md)；
原 149 条快照仍保留在 [macos-history-20261016/results/results.json](macos-history-20261016/results/results.json)。
仓库根目录的 [旧多设备历史](../../results/results.json) 也保留原样；其中包含更早的测试合同。

## 如何解读

- 原始历史包含旧口径、对照、取消和未稳定轮次，不把所有 JSON 递归合并为当前成绩榜。
- A16 实际进度及 3 项警告数值见 [设备记录](../../docs/a16-partial-run-20261016.md)。
- iPad8,1 当前选用结果及源清单在 `ipad81-design-review-20261016/`，不是新跑的一轮整套。
- 模拟器资料如 `ios-simulator-full-suite-20261016/` 仅验证流程，不作真机性能结论。
- 其他日期文件夹保留完整 native 日志、套件 manifests、曲线 CSV/JSON 和 PNG/PDF 导出。

## 仍在本地的内容

大型 Xcode `.xcresult`、Instruments `.trace` / `.gputrace`、App/安装包、编译程序和
绘图库依赖继续忽略；没有删除。它们的可读摘要、原生运行日志和导出图片在相邻目录入库。
部分旧报告提到的完整诊断 bundle 路径只在原测试 Mac 上存在，不表示数据文件丢失。

归档策略及规则来源见 [测试资料入库说明](../../docs/test-result-archives.md)。
