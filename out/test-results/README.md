# Benchmark results, curves and native logs

小体积成绩、完整曲线、原生运行日志、报告和导出截图现在纳入 Git。
本目录保留原始路径，便于查阅旧文档链接；JSON/CSV 中的曲线点与日志均不改写。
归档文件禁用换行转换，保持提交内容与本地原件逐字节一致。

## Apple 设备主历史快照

| 设备 | 型号 | 历史条数 | 原始文件 | 说明 |
| --- | --- | ---: | --- | --- |
| Mac M4 Pro | Mac16,8 | 171 | [最新主历史](m4-single-core-check-20261017/results-after.json) | 最新单核确认大核216.737、后台小核76.322；原169条不变，V3内存与粒子修复和诊断对照均保留。 |
| iPhone 13 Pro / A15 | iPhone14,2 | 155 | [results.json](a15-metal-confirm-20261017/phone-final/results/results.json) | Vulkan九项必补后额外一次14项Metal确认完成；31项有覆盖。Metal/Vulkan各一项纯计算波动提示保留，Metal内存30.679仍预览；原141条不变。 |
| 12.9-inch iPad Pro / A12X | iPad8,7 | 79 | [results.json](ipad87-particle-completion-20261017/phone-final/results/results.json) | 五项Vulkan粒子新计时已补，4passed/1review（1M绘制无稳定窗口），当前31项覆盖，旧74条不变。512MiB平台56.657沿用，Metal内存仍预览，旧Metal16M提示保留。 |
| 11-inch iPad Pro / A12X | iPad8,1 | 53 | [results.json](ipad81-budget-metal-check-20261018/phone-final/results/results.json) | 两项待办通过：Metal16M29.089、Vulkan512MiB平台56.524；83点/38校验。旧51条从归档原样恢复后追加2条，当前31项/33记录覆盖；Metal内存仍预览。 |
| iPhone 14 Pro Max / A16 | iPhone15,3 | 35 | [results.json](a16-budget-completion-20261018/phone-final/results/results.json) | 九项补齐8passed/1review；CPU169.945/100.672/721.677、267曲线点，Vulkan512MiB平台45.044。当前31项/33记录覆盖，旧24条不变；四个吞吐提示及大核内存分项平台未稳定保留。 |
| Apple Watch Ultra | Watch6,18 | 3 | [results.json](watch-ultra-first-run-20261017/watch-final/results/results.json) | 实体一键2/2完成，单线程53.672、双核116.499 MWork/s；126曲线点，页面／存储已测。内存5.614仍参考；实际arm64_32，原始ARM64标签保留，导出器已修正。 |

精确字节数及 SHA256 见 [archive-index.json](archive-index.json)。
最新跨设备比较见[Apple成绩对比](../../docs/apple-device-comparison-20261018.md)，
对应[CSV](apple-device-comparison-20261018/comparison.csv)与
[原始结果ID及版本](apple-device-comparison-20261018/selected-results.json)可用于跨电脑核对。
Mac 的现有导出报告一并备份在 [macos-history-20261016/reports](macos-history-20261016/reports)。
Mac 最新修复见 [报告、曲线、原生日志及跨平台构建验证](macos-measurement-fixes-20261016/report.md)；
[最新单核确认](m4-single-core-check-20261017/report.md) 保留了原207.532与本次216.737的条件差异；
[上一轮定向补测](macos-targeted-retest-20261016/report.md) 保留为历史。
原 149 条快照仍保留在 [macos-history-20261016/results/results.json](macos-history-20261016/results/results.json)。
仓库根目录的 [旧多设备历史](../../results/results.json) 也保留原样；其中包含更早的测试合同。

## Apple Watch 真机与模拟器

[Ultra首次完整一键](watch-ultra-first-run-20261017/report.md)：英语界面，标准配置，
UI1/1通过约88秒，保存3条成绩。用户调整亮屏／返回时钟设置后完成，
此前取消尝试也保留。六条曲线126点、页面／存储齐全，内存平台未确认=>review。
ARM64_32标签修复已安装，更新后原成绩逐字节不变；其他五台主历史和Mac live未变。

[watchOS CPU 实现与验收](watchos-cpu-implementation-20261017/report.md)：
独立App的一键、默认时长、取消、历史重启、六条曲线均通过模拟器实际点击验证；
该模拟器阶段的真机架构包仅编译。标准配置保存108曲线点和页面／存储子项，
平台未确认时保留review。15条累计模拟器记录均标为宿主Mac成绩，**不是Watch速度，
不加入上方真机的比较**。JSON、原始日志、曲线CSV及导出截图均在该目录。

## 砺算 LX 7G100（Windows，2026-10-20）

[完整记录](../../docs/lx-7g100-20261020.md)：完整测试中这张卡的46项，加修好引擎后重测的15项。
日志、这张卡在PC记录里的全部55条、已知错误清单（按要求未从PC记录删除）和临时显存测试
（读296.0 GB/s，单独版本名）都在 [lx-7g100-20261020](lx-7g100-20261020/)。

## 如何解读

- [A16九项补测](a16-budget-completion-20261018/report.md)：18日398秒、8passed/1review、0执行失败，当前31/31覆盖、历史24→35。Vulkan平台45.044、83点/38校验；CPU267点/186驻留点及页面/存储齐全。新GLES2 Burn256下降13.54%，大核内存分项49.6的平台未稳定，仅参考；全核内存主分数29.770的settled为1，App已停止。
- [11英寸A12X两项补测](ipad81-budget-metal-check-20261018/report.md)：18日约77秒、2 passed，Metal16M稳定CV0.26%，新版Vulkan内存平台56.524确认；旧51条恢复后新增2条，当前53条，覆盖31/31。原始恢复来源、GPU/frame时间及83点校验齐全，App已停止。
- [12.9英寸A12X五项粒子补测](ipad87-particle-completion-20261017/report.md)：约83秒、4 passed/1 review，无执行失败，历史74→79。生产匹配31/31项、33条选用记录；CPU／内存／其他稳定项目未重跑，完整时间线及旧记录保护均核对。
- 原始历史包含旧口径、对照、取消和未稳定轮次，不把所有 JSON 递归合并为当前成绩榜。
- Mac 本轮 14 条新增记录中 10 条通过、3 条旧查询对照保留 review、1 条小容量负向复核正确保留 preview/review。默认内存三轮读取 245.897/245.908/250.419 GB/s。新版身份为 `membw_v3_streaming`，旧 V1/V2 不自动变成新版设备成绩。
- A16的16日阶段与当时3项警告见 [历史设备记录](../../docs/a16-partial-run-20261016.md)；18日已补齐，当前四个吞吐提示及分项内存限制见上述最新报告。
- [A15最新Metal全套确认](a15-metal-confirm-20261017/report.md)：一次14项用时236秒、13 passed/1 review/0失败，新增14条到155条。多数接近原成绩，64K绘制−14.50%、流体−11.45%、64K纯计算18.18%下降均保留。内存52点/缓存124点及原始日志完整，Metal平台仍未验证，App已停止。
- [A15此前九项必补与全清单核对](a15-app-budget-completion-20261017/report.md)：512MiB内存平台29.803通过，另补八个粒子同步计时合同。8 passed/1 review/0执行失败，31/31项有当前匹配记录，历史当时141条；完整JSON、83点内存曲线及逐秒粒子时间线齐全。原CPU与其他稳定记录保留。
- [A15此前两项真机补测](a15-v3-cpu-confirm-20261017/report.md)：CPU通过，旧256MiB V3内存77点/34次校验，仅参考未达平台；旧记录未改写，平台待办已由上述新一轮完成。
- [M4 GPU Burn最新诊断](metal-math-diagnosis-20261017/report.md)：固定步数简化片元达到计算的约98%；完整着色器约50%额外FS调用，discard控制支持可见性执行为主要因素。正式成绩未改，不能把无discard的遮挡剔除速率当新分数。
- [M4独立单核研究](m4-single-core-research-20261017/report.md)：本机Geekbench 6.7.1为3675/22129，另做编译目标和分支对照。约6%的参考单核差距尚未归因；独立基准及控制程序均未混入项目主历史。
- [12.9英寸A12X最新V3补测](ipad87-v3-memory-20261017/report.md)：仅跑待补内存，77点/34次校验通过，53.932 GB/s仍为未达平台的参考值；原72条保留，App已停止。
- [随后完成iOS内存平台修复](ios-vulkan-memory-platform-fix-20261017/report.md)：默认512 MiB并按App余量缩小；iPad8,7平台56.657通过，原模拟器验证保留review。此后A15确认29.803、11英寸iPad于18日确认56.524；12.9英寸五个新计时粒子组合也已补齐，见上方最新报告。
- iPad8,1 的16日原选用清单在 `ipad81-design-review-20261016/`；18日只新增两项补测到53条。当前31项覆盖来自不同真实轮次，没有声称新跑整套。
- [最新模拟器一键验收](ios-simulator-v3-flow-20261017/report.md)：27/27 项完成、28 条成绩、0 执行失败，18 passed / 9 review；V3 小容量平台提示正确，保存曲线完整。仅验证流程，不作真机性能结论。上一轮 `ios-simulator-full-suite-20261016/` 保留。
- 其他日期文件夹保留完整 native 日志、套件 manifests、曲线 CSV/JSON 和 PNG/PDF 导出。

## 仍在本地的内容

大型 Xcode `.xcresult`、Instruments `.trace` / `.gputrace`、App/安装包、编译程序和
绘图库依赖继续忽略；没有删除。它们的可读摘要、原生运行日志和导出图片在相邻目录入库。
部分旧报告提到的完整诊断 bundle 路径只在原测试 Mac 上存在，不表示数据文件丢失。

归档策略及规则来源见 [测试资料入库说明](../../docs/test-result-archives.md)。
