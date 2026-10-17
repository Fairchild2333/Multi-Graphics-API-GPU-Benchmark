# A15 待补项目完成与全清单复核 · 2026-10-17

**iPhone 13 Pro / A15 本轮必补9项全部执行完成：8 passed、1 review、0执行失败。**
生产App模型确认当前31项都有匹配记录，共选用33条成绩；来源为本轮9条及此前已通过
的24条，不声称本轮重新跑过整套31项。历史132→141条，原132条逐项不变。
手机App PID48000已停止，可以拔线；没有启动其他手机、平板或Watch的测试。

## 为什么补9项，而不是旧清单的1项

先备份手机Documents，与此前A15主历史132条逐字节一致；再用当前生产
`MobileSuiteJob.makeJobs/matches/matchesRecordGroup`核对。结果仅22/31项匹配：
新版App预算Vulkan内存缺一条，另外八个Vulkan粒子组合仍只有旧
`timingMode=gpu_timestamp_query`。当前MoltenVK要求`synchronized_wall_clock`，
不能复用这些旧记录。因此在原来内存待办之外补了64K/1M/4M/16M的绘制、纯计算。

`coverage-before.json`记录九个缺项；`coverage-after.json`记录31/31覆盖、选用IDs、
版本及质量提示。没有降低匹配规则、把旧记录改为新版本或重复跑已匹配的CPU等22项。
本轮使用App已有定向套件入口，非真实点击“一键全部”的整套真机验收；后者仍是
可选流程验证，不能用本轮定向启动冒充。计算、判定和保存都由手机App自行完成，
电脑用于安装、启动和导出。

## Vulkan内存：512MiB平台通过

| 项目 | 本次 |
| --- | --- |
| 版本 | `membw_v3_streaming_ios_preview_app_budget_v1_moltenvk` |
| 读取平台 | **29.803 GB/s** |
| 写入／复制 | **30.4 / 27.8 GB/s** |
| 请求／实际最大工作集 | 512 / 512 MiB |
| App余量快照 | 3,182,786,720 bytes |
| 单缓冲预算 | 536,870,912 bytes，两块缓冲共1GiB |
| 曲线／数据校验 | **83点／38次通过**，时间戳异常0 |
| 尾部256/384/512MiB | 全部有序完整遍历；读、写、复制各自最大/最小≤1.2 |
| 热状态／套件 | nominal→nominal，passed |

这是本内核在统一内存上的访问平台，不是硬件流量计数或绝对DRAM峰值。
旧256MiB/28.842读取参考及61.3写入观测保留，不能与现在512MiB平台混称正式数据。
CSV：[83个原始曲线点](vulkan-memory-points.csv)。

## 八个Vulkan粒子组合

均使用当前同步计时版本，15秒运行含2秒预热。绘制固定1280×720、固定可见覆盖；
纯计算不渲染。所有项目热状态0→0，记录均为真机而非模拟器。

| 粒子数 | 绘制GB/s | 纯计算GB/s | 状态 |
| ---: | ---: | ---: | --- |
| 65536 | 2.864 | 3.898 | App状态passed |
| 1048576 | 14.555 | 14.337 | 1M纯计算保留5.41%下降提示 |
| 4194304 | 16.859 | 17.363 | App状态passed |
| 16777216 | 18.369 | 18.505 | App状态passed |

上表是粒子内核按40B/粒子定义的逻辑吞吐，**不是DRAM带宽**。新同步计时包含
提交、等待等成本，旧GPU查询值使用不同计时范围，不能据两者之差宣称硬件性能
下降；也不能把原生Metal时间和新同步计时当作完全相同的计时合同直接排名。
GPU总时间和帧间隔均有保存：[新成绩摘要](new-results-summary.csv)、
[逐秒时间曲线](particle-timeline.csv)，完整原生日志在各项目及`phone-final/suite-runs/`。

### 保留的运行波动

- **1M纯计算**：全程均值14.337 GB/s，稳定窗口13.835、CV0.56%，早晚下降5.41%，
  正确review。GPU总时间2.926ms、帧间隔2.932ms，没有发现旧查询计时不一致；
  热状态正常，没有直接GPU频率证据，不能认定已证明热降频。
- **64K纯计算**：App状态passed，但早晚吞吐增长30.22%（保存的drop=-30.22），
  全程均值3.898、所选稳定窗口3.499 GB/s。当前App规则针对下降报警；passed不
  表示各个时段完全恒定，也不称该轮峰值。原始变化全部保留。
- 本次目的是补齐新计时合同，未为清除提示或挑更高分重复同一项目；若后续专门
  比较稳定性能，上述两项可做明确标注的冷机对照，现有波动记录仍应保留。

## 原CPU及Metal等记录的核对

当前生产匹配仍接受原CPU大核**147.000**、前台小核**89.287**、全核
**634.868 MWork/s**；不是这次新测。CPU缓存／内存有**267点**，P/E驻留标记、
页面首次访问、顺序／随机存储都齐全，`pageFaultMeasured=storageMeasured=1`。
页面首次访问不代表实际交换文件性能。见`retained-cpu-coverage.json`及保存的原始行。

Metal粒子、Burn、流体、Metal内存诊断、缓存诊断和GLES2已有当前项目覆盖，未重复。
**原生Metal内存仍是preview诊断、平台未独立验证；GPU L2/L3/SLC也仍未独立归属。**
“31项有覆盖”不改变这些已说明的测法范围，不能解读为所有硬件峰值及缓存归属已确认。

## 证据与验证

- 基于`addfaee`重建最新iOS Release，并用已授权Team签名安装；未改生产代码。
  命令、构建／签名／安装日志和源代码／包／SPIR-V哈希均在本目录。
- `production-contract-audit.json`及日志：九个真实请求、版本、唯一新记录、质量状态
  全部通过生产Swift模型核对；Vulkan内存还原83点。31项覆盖来源没有混入模拟器。
- `phone-before/`保存安装前原始Documents；各项目保存本次日志、manifest和对应
  JSON；`phone-final/`保存最终完整Documents；原132条对象未变化。
- 五份此前主历史快照及Mac live JSON哈希均未变。本轮仅把A15最新索引推进到
  新141条快照，其他真设备成绩未更换。
- 停止前进程过滤的首次CLI谓词遇到URL类型错误，日志保留；改为读取进程列表、
  仅选择Mangekyo实际PID48000后SIGTERM成功，见`stop-device.json`。这不是测试失败。
- 同时只读检查其余iOS设备的Vulkan粒子合同：iPad8,7仍缺5个新版组合；iPad8,1
  和A16的八个粒子组合已匹配。见`other-devices-particle-contract-audit.json`。
  没有启动这些设备；对应待办已补进根目录最新清单。

复核命令：

```bash
python3 out/test-results/a15-app-budget-completion-20261017/verify.py
```

[verification.json](verification.json)给出九项明细及完整性断言，既有历史、异常提示
和质量门槛均原样保留。
