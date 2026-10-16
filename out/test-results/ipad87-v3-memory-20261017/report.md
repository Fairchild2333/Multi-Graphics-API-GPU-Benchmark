# 12.9英寸 iPad Pro / A12X：Vulkan V3 补测 · 2026-10-17

已在本次实际有线接入的 **iPad8,7（A12X，iPadOS 26.7.1）** 完成待补的一项
Vulkan V3内存扫描。读取参考值 **53.932 GB/s**，77个曲线点、34次数据校验通过，
热状态0→0。当前iOS软件预览上限仍为256 MiB，正式内存平台未建立，正确保留review。

这台不是11英寸iPad8,1，未误跑另一台设备的Metal纯计算16M待办。
没有重新执行整套、重跑CPU或其他稳定项目，也没有为了消除review修改门槛。

## 实际记录

| 项目 | 本轮结果 |
| --- | --- |
| 原生计划 | `complete-suite-C0C38F9A-E746-48C6-B64A-70C9E718276A` |
| App项目 | `vulkan/memory_sweep`，请求`memory_sweep?api=vulkan` |
| 唯一新成绩 | `20261017-015429-155-membw` |
| 版本 | `membw_v3_streaming_ios_preview_moltenvk` |
| 读取参考值 | 53.932 GB/s |
| 写入/复制参考值 | 53.0 / 51.4 GB/s |
| 执行时间 | 55.1秒 |
| 数据与时间 | 34次写入/复制校验通过，时间戳异常0，整轮同步墙钟 |
| 曲线 | 77点，从保存文件用生产Swift模型恢复；另导出CSV |
| 完整遍历 | 保存`ordered_full_dataset_passes`标记 |
| 平台与质量 | `memoryPlateauReached=0`，review：`Memory plateau was not reached` |
| 系统热状态 | nominal → nominal（0→0） |

Vulkan由MoltenVK转译到Metal，沿用已有转译开关。生产模型验证了请求、版本、
新记录ID、数量、质量标记和曲线恢复，没有复用旧结果冒充本次执行。

## 为什么仍标review

现有iOS代码的软件预览上限为256 MiB，这不是本轮证实的硬件缓冲上限。
V3要求平台尾部三个工作集全部使用完整遍历，即均不小于256 MiB。
本轮尾部128/192 MiB仍使用小数据集重复方法，只有256 MiB使用完整有序遍历。
即使数值相近，也不能将混合方法的尾部认定为通过新版平台合同：

| 数据集MiB | 读取GB/s | 写入GB/s | 复制GB/s |
| ---: | ---: | ---: | ---: |
| 128 | 53.932 | 52.762 | 51.378 |
| 192 | 54.924 | 53.829 | 51.630 |
| 256 | 51.953 | 52.999 | 49.828 |

以上是实际内核访问曲线，不是已归属的独立GPU L2/L3/SLC速度，也不是硬件流量
计数或DRAM峰值。旧V2的57.903 GB/s保留为历史，算法版本不同，不据此给出
本轮性能退步百分比。若以后要建立正式iOS Vulkan内存平台，需另行设计更大
工作集的安全扫描并真机验收，不能把这次review改成passed。

## 备份、安装和结束

- 安装前备份72条，与仓库原主历史SHA256完全一致；本轮仅追加1条，现73条，
  原72条对象逐项未变化，无重复ID。
- 复用本机已签名的最新版Release App；自原构建提交`5ec3730`以来生产源码未变，
  App可执行文件、新read SPIR-V哈希与A15已验证包一致，签名校验通过，
  provisioning profile包含本台iPad。没有为只改文档再次编译相同二进制。
- 完整原始Documents保留于`phone-before/`、项目的`documents/`和`phone-final/`；
  `results.json`包含全部曲线，`vulkan-memory-points.csv`提供77点的可读导出。
- `verification.json`、`production-contract-audit.json`及配套脚本核对本轮合同、
  数据校验、曲线、原历史及其他四台设备主历史/本机实时成绩哈希。
- 导出完成后已停止App **PID3132**，见`stop-device.json`；可以拔线。
- 其他已通过项目保持原记录；这台此前Metal16M绘制的稳定窗口提示保留，
  本轮没有为此反复挑分。另一台11英寸iPad8,1的Metal16M低分核查及V3补测仍待连接。

本轮只执行App现有单项流程并归档，未修改生产代码、负载或历史成绩。
