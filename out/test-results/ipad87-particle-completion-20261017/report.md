# 12.9 英寸 A12X：补齐五项 Vulkan 粒子计时 · 2026-10-17

本轮设备为iPad Pro12.9英寸第3代，iPad8,7，iPadOS26.7.1，有线连接。
复用设备已安装的Mangekyo0.2.6，没有重编译、更新App或修改测法。
备份与此前主历史逐字节一致的74条成绩后，通过现有完整套件入口指定五个待补任务。
**5/5执行完成，4 passed、1 review、0执行失败，历史74→79条，旧74条不变。**
开始09:35:08Z、结束09:36:31Z，即悉尼20:35:08–20:36:31，约83秒。
这是一轮有选择的五项补测，没有重新点击31项整套按钮，也没有重跑CPU、Metal或GPU内存。

| 新增项目 | 算力负载吞吐 GB/s | Compute ms | Render ms | Frame ms | App状态 |
| --- | ---: | ---: | ---: | ---: | --- |
| Vulkan64K绘制 | 3.172 | 0.826 | 7.004 | 8.387 | passed |
| Vulkan64K纯计算 | 4.954 | 0.529 | 0 | 0.534 | passed |
| Vulkan1M绘制 | 20.363 | 2.060 | 58.190 | 60.815 | review：未找到稳定窗口 |
| Vulkan4M绘制 | 26.601 | 6.307 | 262.830 | 270.015 | passed |
| Vulkan16M绘制 | 27.364 | 24.525 | 1175.960 | 1198.203 | passed |

粒子吞吐是本项目根据compute时间及每粒子字节数计算的工作负载指标，不能当作独立DRAM带宽。
这五条均记录MoltenVK→Metal、`synchronized_wall_clock`及`sync_timing`版本。
四个绘制组合均采用固定覆盖`wrapped_ndc_v2`、1280×720视口。
虽然请求vsync=0，原生日志明确记录设备只能FIFO呈现；保留实际限制。

1M绘制`stableScore=0`、`stableVariancePct=-1`，App正确保留review。
前后吞吐变化指标为-2.68%（末段略有增长），不是明显持续下降；GPU/frame时间相符，
热状态逐项均0→0。没有GPU频率遥测，不能把未形成稳定窗口断言为热降频。
64K绘制也没有稳定窗口数值，但当前共享规则只对Burn或≥1M粒子强制该条件，
所以该项为passed；不将它表述为已验证稳定峰值。
没有为消除这些标记反复挑分。

## 当前清单核对

用生产`MobileSuiteJob.matches`及`matchesRecordGroup`重新核对：
**26/31→31/31项覆盖，选用33条成绩**，新增记录、请求、版本和质量状态全部匹配。
覆盖不表示每条是稳定峰值：当前选用记录还保留之前Metal16M绘制与本次Vulkan1M绘制的提示。
原Metal内存仍是预览，GPU缓存归属仍未确认，不因清单完整就升级这些测量结论。

核对使用A12X实际能力：GPU缓存诊断为parallel512，CPU算力采用前台小核策略，
CPU内存仍按已验证的后台小核策略。默认通用计划会错误查找gpu_cache-v2和前台CPU内存，
不能把这两个配置差异误判为设备缺测。参数依据生产Bridge及A12X分支，并未放宽匹配器。

保留原Vulkan512MiB平台56.657GB/s、83曲线点／38次校验记录，本轮没有重测。
CPU当前沿用大核111.527、小核60.013、全核656.340MWork/s；CPU缓存／内存267点，
页面和存储子项也仍在原记录中。以上CPU和内存值来自此前真实轮次。

## 保存与复核

- [新增五项摘要](new-results-summary.csv)、[逐秒GPU时间线](particle-timeline.csv)。
- [完整79条主历史](phone-final/results/results.json)、
  [套件manifest](phone-final/suite-runs/latest.json)、[运行日志](native-console.log)。
- [生产合同核对](production-contract-audit.json)、[全清单覆盖](coverage-after.json)、
  [备份保护](backup-verification.json)、[归档校验](verification.json)。

校验包含旧74条逐条相等、唯一新增ID、五条计时及固定视口合同、热状态、时间线，
31/31生产匹配，以及所有先前主历史快照和Mac live JSON的SHA256不变。
原主快照继续保留，新索引指向本目录79条导出；其他设备索引不变。
测试完成、导出后已对本次App PID4638发SIGTERM。console工具因此退出1，
它是收尾信号结果，不是基准执行失败；以App manifest及已保存结果判定任务状态。

复核命令：`python3 out/test-results/ipad87-particle-completion-20261017/verify.py`。
