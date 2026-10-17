# Benchmark results, curves and native logs

小体积成绩、完整曲线、原生运行日志、报告和导出截图现在纳入 Git。
本目录保留原始路径，便于查阅旧文档链接；JSON/CSV 中的曲线点与日志均不改写。
归档文件禁用换行转换，保持提交内容与本地原件逐字节一致。

## Apple 设备主历史快照

| 设备 | 型号 | 历史条数 | 原始文件 | 说明 |
| --- | --- | ---: | --- | --- |
| Mac M4 Pro | Mac16,8 | 171 | [最新主历史](m4-single-core-check-20261017/results-after.json) | 最新单核确认大核216.737、后台小核76.322；原169条不变，V3内存与粒子修复和诊断对照均保留。 |
| iPhone 13 Pro / A15 | iPhone14,2 | 132 | [results.json](a15-v3-cpu-confirm-20261017/phone-final/results/results.json) | 新增V3内存参考28.842（未达平台，review）及CPU147.000/89.287/634.868；原128条不变。 |
| 12.9-inch iPad Pro / A12X | iPad8,7 | 74 | [results.json](ios-vulkan-memory-platform-fix-20261017/phone-final/results/results.json) | 新预算版本512 MiB读取平台56.657，83点/38次校验通过；原73条含53.932参考和旧Metal16M稳定性提示保留。 |
| 11-inch iPad Pro / A12X | iPad8,1 | 51 | [results.json](ipad81-design-review-20261016/phone-final/results/results.json) | 51 条历史；当前完整选用 31 项/33 条来自不同真实轮次。 |
| iPhone 14 Pro Max / A16 | iPhone15,3 | 24 | [results.json](a16-full-suite-20261016/phone-final/results/results.json) | 24 条历史=22 条完整成绩+2 条中断诊断。19 passed、3 review；CPU 尚未测。 |

精确字节数及 SHA256 见 [archive-index.json](archive-index.json)。
Mac 的现有导出报告一并备份在 [macos-history-20261016/reports](macos-history-20261016/reports)。
Mac 最新修复见 [报告、曲线、原生日志及跨平台构建验证](macos-measurement-fixes-20261016/report.md)；
[最新单核确认](m4-single-core-check-20261017/report.md) 保留了原207.532与本次216.737的条件差异；
[上一轮定向补测](macos-targeted-retest-20261016/report.md) 保留为历史。
原 149 条快照仍保留在 [macos-history-20261016/results/results.json](macos-history-20261016/results/results.json)。
仓库根目录的 [旧多设备历史](../../results/results.json) 也保留原样；其中包含更早的测试合同。

## Apple Watch CPU 模拟器流程记录

[watchOS CPU 实现与验收](watchos-cpu-implementation-20261017/report.md)：
独立App的一键、默认时长、取消、历史重启、六条曲线均通过模拟器实际点击验证；
真机架构包编译通过但尚未接入手表。标准配置保存108曲线点和页面／存储子项，
平台未确认时保留review。15条累计模拟器记录均标为宿主Mac成绩，**不是Watch速度，
不加入上方五台真机的比较**。JSON、原始日志、曲线CSV及导出截图均在该目录。

## 如何解读

- 原始历史包含旧口径、对照、取消和未稳定轮次，不把所有 JSON 递归合并为当前成绩榜。
- Mac 本轮 14 条新增记录中 10 条通过、3 条旧查询对照保留 review、1 条小容量负向复核正确保留 preview/review。默认内存三轮读取 245.897/245.908/250.419 GB/s。新版身份为 `membw_v3_streaming`，旧 V1/V2 不自动变成新版设备成绩。
- A16 实际进度及 3 项警告数值见 [设备记录](../../docs/a16-partial-run-20261016.md)。
- [A15最新两项真机补测](a15-v3-cpu-confirm-20261017/report.md)：CPU通过，V3内存77点/34次校验通过，但iOS软件预览上限256 MiB，正式平台未建立。没有重跑其他稳定项目。
- [M4 GPU Burn最新诊断](metal-math-diagnosis-20261017/report.md)：固定步数简化片元达到计算的约98%；完整着色器约50%额外FS调用，discard控制支持可见性执行为主要因素。正式成绩未改，不能把无discard的遮挡剔除速率当新分数。
- [M4独立单核研究](m4-single-core-research-20261017/report.md)：本机Geekbench 6.7.1为3675/22129，另做编译目标和分支对照。约6%的参考单核差距尚未归因；独立基准及控制程序均未混入项目主历史。
- [12.9英寸A12X最新V3补测](ipad87-v3-memory-20261017/report.md)：仅跑待补内存，77点/34次校验通过，53.932 GB/s仍为未达平台的参考值；原72条保留，App已停止。
- [随后完成iOS内存平台修复](ios-vulkan-memory-platform-fix-20261017/report.md)：默认512 MiB并按App余量缩小；iPad8,7平台56.657通过，模拟器预算未知时正确回退review。A15/另一台iPad尚需新配置实测。
- iPad8,1 当前选用结果及源清单在 `ipad81-design-review-20261016/`，不是新跑的一轮整套。
- [最新模拟器一键验收](ios-simulator-v3-flow-20261017/report.md)：27/27 项完成、28 条成绩、0 执行失败，18 passed / 9 review；V3 小容量平台提示正确，保存曲线完整。仅验证流程，不作真机性能结论。上一轮 `ios-simulator-full-suite-20261016/` 保留。
- 其他日期文件夹保留完整 native 日志、套件 manifests、曲线 CSV/JSON 和 PNG/PDF 导出。

## 仍在本地的内容

大型 Xcode `.xcresult`、Instruments `.trace` / `.gputrace`、App/安装包、编译程序和
绘图库依赖继续忽略；没有删除。它们的可读摘要、原生运行日志和导出图片在相邻目录入库。
部分旧报告提到的完整诊断 bundle 路径只在原测试 Mac 上存在，不表示数据文件丢失。

归档策略及规则来源见 [测试资料入库说明](../../docs/test-result-archives.md)。
