# 11 英寸 A12X：Metal16M复核及Vulkan内存平台补测 · 2026-10-18

**两项均passed，0执行失败，当前31/31项有匹配记录，选用33条成绩。**
设备为iPad Pro11英寸第1代（iPad8,1），iPadOS26.7.1，有线连接。
现有待办只运行Metal16M纯计算复核和新版Vulkan内存扫描；其他项目没有重复跑。
任务于悉尼17:34:02–17:35:19完成，约77秒；App PID9797在导出后已停止，可拔线。

| 项目 | 本轮结果 | 关键检查 |
| --- | ---: | --- |
| Metal16M纯计算 | **29.089 GB/s** | 稳定段29.121，CV0.26%，前后变化-0.26%，passed |
| Vulkan读取平台 | **56.524 GB/s** | 512MiB，83曲线点、38次校验，异常0，passed |
| Vulkan写入／复制平台摘要 | **58.6／52.8 GB/s** | 三个大工作集均完整有序遍历 |

## Metal复核判断

旧记录为20.086GB/s，GPU／帧间隔33.411／33.455ms；本次为23.070／23.105ms。
两个轮次各自的GPU时间与帧间隔都一致，本次低吞吐没有复现，并形成稳定窗口。
不能据此把旧低分直接认定为计时错误、热降频，或宣称硬件性能提升约45%。
旧记录标记`gpu_timestamp_query`，新记录明确`command_buffer_gpu_time`与
`stream_v1_ios_preview_command_buffer_timing_v1`；不同计时合同和运行条件均保留。
本轮没有修改Metal内核、固定覆盖规则或稳定阈值，也没有反复挑高分。
粒子GB/s是该工作负载按计算时间折算的吞吐，不是独立DRAM带宽。

## Vulkan大工作集平台

使用17日已在12.9英寸A12X验收的签名Release包，二进制SHA256与其归档一致，
无重新编译。默认请求512MiB，按实时App剩余分配额度四分之一限制单缓冲。
本次余量2,976,053,712字节，两块512MiB逻辑缓冲，预算保护有效。

| 工作集MiB | 读取GB/s | 写入GB/s | 复制GB/s |
| ---: | ---: | ---: | ---: |
| 256 | 56.224 | 57.833 | 52.567 |
| 384 | 56.524 | 58.560 | 52.810 |
| 512 | 56.746 | 58.790 | 52.980 |

三个尾部点均完整遍历、方法相同且平台平坦，`memoryPlateauReached=1`、
`dataVerified=1`、`writeCopyChecks=38`、`timestampAnomalies=0`。
版本为`membw_v3_streaming_ios_preview_app_budget_v1_moltenvk`，使用MoltenVK→Metal。
83点及生产模型读取均核对，测量59.4秒；两项任务热状态均0→0。
这是大工作集内核访问平台，不等于硬件绝对峰值或独立GPU L2/L3/SLC归属。
原58.051GB/s旧版本记录保留，不将V2与V3的差值解释成设备性能变化。

## 原成绩恢复与完整性

连接时系统查询未发现已安装的Mangekyo，原App容器无法读取。
安装后Documents为空；仅将本机项目已有、对应同一iPad8,1的51条历史原样恢复进App。
恢复前核对主索引SHA256和设备型号，恢复后重新导出并逐字节核对。
**恢复的51条是此前成绩，只有新增两条属于本轮实测。**
新历史共53条，前51条与原归档逐项相等，结果ID唯一，原文件没有被改写。
旧曲线在results.json中完整保留；原生历史日志仍可在16日归档查看。

当前生产计划匹配30/31→31/31，33条选用记录，无measurementReviewReasons。
Metal正式内存平台仍未实现，已有Metal内存保留预览；GPU缓存归属依然未确认。
因此清单覆盖完整不代表所有硬件层级均已独立归属，也不是峰值保证。
生产核对使用A12X的parallel512缓存诊断、CPU前台小核算力与后台小核内存策略。

## 原始记录

- [完整53条结果](phone-final/results/results.json)、[套件manifest](phone-final/suite-runs/latest.json)。
- [83点内存曲线](vulkan-memory-points.csv)、[Metal时间线](metal-timeline.csv)、[原生控制台](native-console.log)。
- [生产合同匹配](production-contract-audit.json)、[当前清单覆盖](coverage-after.json)、[完整校验](verification.json)。
- [历史恢复来源](history-restoration-source.json)、[回读核对](backup-verification.json)、[安装包身份](package-info.json)。

原有六台设备历史快照及Mac live JSON的SHA256均未改变；主索引仅将iPad8,1推进到本轮53条导出。
console工具在任务全部保存和导出后因主动SIGTERM退出1；App套件为2/2passed，不能将
收尾退出码当作执行失败。没有声称实际点击整套31项按钮，本次使用同套件的两项定向入口。

复核命令：`python3 out/test-results/ipad81-budget-metal-check-20261018/verify.py`。
