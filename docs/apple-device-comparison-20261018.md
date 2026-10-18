# Apple设备实测对比（2026-10-18，A16补齐后）

以下按项目归档的最新匹配记录比较，不按最高分选择。A16此次只补原九项，其他设备
没有重新测试。CPU为本项目混合内核，GPU为指定单项；均不是通用CPU/GPU综合分。
所有源文件、SHA、结果ID、版本与必要配置见
[比较数据及版本](../out/test-results/apple-device-comparison-20261018/selected-results.json)、
[可导入表格的CSV](../out/test-results/apple-device-comparison-20261018/comparison.csv)。

## CPU算力

单位MWork/s，越高越好。

| 设备 | 大核单线程 | 小核单线程 | 全核 | 全核线程数 |
| --- | ---: | ---: | ---: | ---: |
| M4 Pro | 216.737 | 76.322＊ | 2436.328 | 14 |
| iPhone14 Pro Max / A16 | 169.945 | 100.672 | 721.677 | 6 |
| iPhone13 Pro / A15 | 147.000 | 89.287 | 634.868 | 6 |
| iPad Pro12.9 / A12X | 111.527 | 60.013 | 656.340 | 8 |
| iPad Pro11 / A12X | 111.076 | 60.791 | 657.976 | 8 |

＊Mac小核采用后台策略，iPhone采用前台辅助负载策略；A12X采用无辅助线程的前台
utility策略。**不能把小核一列直接作为相同调度条件下的硬件排名**。类型驻留均按各设备
保存的验证方法确认；不等于绑定具体核心编号。Mac只选保留实际分支的新版CPU结果，
旧约312/3582的无分支版本不混入。

A16相对A15：大核+15.61%、小核+12.75%、全核+13.67%。本项目大核测得M4 Pro
比A16高27.53%，不能外推所有工作负载。A12X单核落后于两台iPhone，其8核全核成绩
略高于6核A15、低于6核A16。

## GPU与内存

表中Metal1M是1,048,576粒子纯计算，GB/s为逻辑工作量吞吐；Burn256单位Gpix-step/s；
Vulkan内存为V3完整遍历后找到的平台，单位GB/s。三列的工作负载不同，不相互作倍率排名。

| 设备 | Metal1M纯计算 GB/s | Metal Burn256 Gpix-step/s | Vulkan内存读取 GB/s |
| --- | ---: | ---: | ---: |
| M4 Pro | 161.361 | 9.658 | 250.419 |
| A16 | 27.233 | 1.269† | 45.044 |
| A15 | 20.974 | 1.590 | 29.803 |
| A12X12.9 | 30.186 | 1.527 | 56.657 |
| A12X11 | 29.126 | 1.504 | 56.524 |

†A16 MetalBurn256是此前保留的下降16.69%轮次，不能作为稳定峰值。A16比A15的
Metal1M均值高29.84%、Vulkan内存平台高51.14%，但Burn256均值低20.19%。
不能因此概括A16所有GPU项目均更快。两台A12X的本次内存平台约56.5–56.7，
确实高于A16的45.044；没有用旧256MiB参考值或旧V2缓存重复结果替代。

Metal1M纯计算的较早iOS记录保存旧gpu_timestamp_query标签，后续记录准确标注
command_buffer_gpu_time；原生headless路径经此前核对按GPU命令缓冲执行时间计算，
这些记录仍按原版本保留。它们不是同一时间、同一系统条件下的同时对照。
Burn列均选serial_timing_v2，采用每轮原均值，不选稳定段或最高分。
Mac/iOS还分别保留桌面与移动scoreContract，不能称统一正式峰值榜。

所有Apple Vulkan数值均经MoltenVK转为Metal；Mac扫描至2048MiB，四台iOS扫描至512MiB，
均已通过平台判定及数据校验。它们是内核逻辑访问平台，没有硬件DRAM流量计数。
原生Metal内存扫描仍为preview，不混为上述Vulkan平台，也不作为正式DRAM带宽列。

## CPU内存、缓存与存储

下列是CPU全核read主分数，单位GB/s，各条all_memory_settled=1。
Mac最大数据集1024MiB，iOS为256MiB；访问方式与GPU不同，不能直接拿CPU/GPU两列
的比值推断相同内存的硬件带宽损失。

| 设备 | CPU全核读取 GB/s | 大核单线程内存分项 |
| --- | ---: | --- |
| M4 Pro | 240.615 | 平台标记已稳定 |
| A16 | 29.770 | 49.6，仅参考，平台未稳定 |
| A15 | 35.178 | 平台标记已稳定 |
| A12X12.9 | 53.737 | 平台未稳定，仅参考 |
| A12X11 | 51.790 | 平台标记已稳定 |

A16的CPU全核读取比A15低15.37%，与GPU读取高51.14%并存。记录核对一致，说明本次
两种工作负载的结果不同，**不能据CPU这一项断定A16物理内存更慢**。差异原因尚未归因，
也未追加重跑挑分。CPU任务passed只代表主分数及任务合同通过，不意味着每个单线程
内存分项均已达到平台；A16和12.9英寸A12X的大核分项都须保留限制。

四台iOS的CPU均保存267曲线点，A16本轮还保存186单线程类型驻留点；页面首次访问和
文件I/O齐全。页面与文件速度不写成系统swap换入换出速度。GPU缓存诊断虽有曲线和
校验，独立L2/L3/SLC归属仍未确认，按用户决定不再作为补测验收项。

## Watch单独列示

Apple Watch Ultra单线程53.672、双核116.499MWork/s，CPU内存5.614GB/s仍是平台
未确认的参考值。真实ABI为arm64_32，使用32位内核合同，不与上方64位设备混为统一
CPU排名；原始JSON旧ARM64标签保留，导出器修正另有记录。

## 提示与原始来源

A16现在31/31项覆盖，当前四个吞吐提示：MetalBurn256下降16.69%、VulkanBurn16
下降19.08%、Vulkan1M绘制无稳定窗口、本轮GLES2Burn256下降13.54%。A15与两台
A12X也保留各自已记录的提示；覆盖完整不表示所有结果均为峰值或无波动。
没有GPU频率遥测，不称这些为已证实热降频。此次仅做归档分析，没有连接设备或重测。

- [A16补测、35条历史、全部曲线与限制](../out/test-results/a16-budget-completion-20261018/report.md)
- [A15最新Metal确认](../out/test-results/a15-metal-confirm-20261017/report.md)
- [12.9英寸A12X最新补测](../out/test-results/ipad87-particle-completion-20261017/report.md)
- [11英寸A12X最新补测](../out/test-results/ipad81-budget-metal-check-20261018/report.md)
- [M4 Pro最新单核确认](../out/test-results/m4-single-core-check-20261017/report.md)
- [M4 Pro新版内存与粒子](../out/test-results/macos-measurement-fixes-20261016/report.md)
- [Watch原始结果](../out/test-results/watch-ultra-first-run-20261017/report.md)
