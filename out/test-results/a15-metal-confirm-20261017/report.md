# A15 原生Metal全套确认 · 2026-10-17

按用户“时间不长就跑一轮”的要求，使用已有签名Release包，在有线连接且解锁的
**iPhone 13 Pro / A15（iPhone14,2，iOS27.0.1）**执行一次14项Metal套件。
**14/14完成，13 passed、1 review、0执行失败，用时236秒（约4分钟）。**
App PID48051已停止，可以拔线；没有启动其他设备或重跑CPU/Vulkan。

## 新旧成绩

下表使用本轮之前最新匹配记录作为旧值，并非挑最高分。全部新旧行均保留。

| 项目 | 旧值 | 本次 | 变化 | 单位 | 状态 |
| --- | ---: | ---: | ---: | --- | --- |
| stream-65536 | 27.939 | 23.889 | -14.50% | GB/s | passed |
| stream_headless-65536 | 52.955 | 56.627 | +6.93% | GB/s | review |
| stream-1048576 | 21.803 | 21.960 | +0.72% | GB/s | passed |
| stream_headless-1048576 | 20.555 | 20.974 | +2.04% | GB/s | passed |
| stream-4194304 | 19.283 | 19.227 | -0.29% | GB/s | passed |
| stream_headless-4194304 | 18.951 | 19.041 | +0.47% | GB/s | passed |
| stream-16777216 | 18.161 | 18.370 | +1.15% | GB/s | passed |
| stream_headless-16777216 | 18.983 | 19.150 | +0.88% | GB/s | passed |
| gpu_burn-16 | 1.408 | 1.409 | +0.07% | Gpix-step/s | passed |
| gpu_burn-64 | 1.555 | 1.558 | +0.19% | Gpix-step/s | passed |
| gpu_burn-256 | 1.586 | 1.590 | +0.25% | Gpix-step/s | passed |
| cinematic_liquid | 12.931 | 11.451 | -11.45% | MParticle-step/s | passed |
| memory_sweep | 30.917 | 30.679 | -0.77% | GB/s | passed |
| gpu_cache-v2 | 318.438 | 315.163 | -1.03% | ns/load | passed |

- 1M/4M/16M粒子的绘制、纯计算变化约−0.3%至+2.1%，Burn三档约+0.1%至+0.3%。
  内存诊断约−0.8%，缓存诊断延迟约−1.0%。这些基本接近原记录。
- **64K绘制−14.50%**：同版本、相同1280×720及固定覆盖，平均Compute0.094→0.110ms，
  Render3.525→3.628ms，总GPU4.464→4.525ms，帧间隔16.737→16.733ms。
  分数来自极短计算段，不能把该分数变化等同于整帧性能下降14.5%；本轮早晚吞吐
  增长11.28%，App规则针对下降报警，因此status为passed，不称全时段恒定。
- **64K纯计算+6.93%，但运行中下降18.18%**：正确review，稳定窗口52.011 GB/s、
  CV0.69%，原均值56.627保留。新记录带`command_buffer_timing_v1`及准确的
  `command_buffer_gpu_time`标签；四个纯计算项目均已用当前版本重新保存。
  表中的旧标签/新标签数据保留分组，百分比是观测，不据此声称计时范围完全相同。
- **流体−11.45%**：同版本及相同配置；模拟154.141→172.304ms、渲染94.045→107.947ms，
  总GPU249.198→281.234ms、帧间隔251.000→283.238ms，相互一致。没有查出本轮
  计时不一致或执行错误，具体差距原因未确认，不直接归因于已证实热降频。
- 各项热状态均0→0；没有GPU频率遥测。仅跑这一次，保留全部波动和平均成绩，
  没有为清除提示或挑高分继续复测。

## 内存、缓存和曲线的实际范围

Metal内存诊断参考为**30.679 GB/s**，从保存的JSON恢复**52点**；原GPU缓存诊断
校验通过，恢复**124点**。曲线CSV：[内存](metal-memory-curves.csv)、
[缓存状态](metal-cache-curves.csv)，原始字段和完整日志也保留。

**这次重复现有诊断，不表示补成正式Metal内存平台。**版本仍为
`metal_memory_sweep_v1_preview_ios_preview`，采用`particle_fixed_grid`、best-of-repeats
统计，iOS仍限制256MiB，`memoryPlateauVerified=0`。不能把30.679与Vulkan
29.803的数值接近当作两种测法等价或Metal平台通过；GPU L2/L3/SLC仍未独立归属。
缓存记录明确`cacheAttribution=unverified`、`checksumValid=1`，没有虚填层级速度。

## 历史和流程验证

- 141→155条历史，安装/运行前备份与上一轮141条主历史逐字节相同，原141条对象
  完全不变；14个新ID唯一。此前Vulkan512MiB平台29.803、83点/38校验及CPU
  147.000/89.287/634.868、CPU内存267点及页面/存储均保留。
- 当前生产App模型核对14个请求、版本、结果组、质量判定与曲线恢复。
  全设备计划仍**31/31项有匹配记录、选用33条**，其中两项稳定性提示：
  本次Metal64K纯计算18.18%下降、此前Vulkan1M纯计算5.41%下降。
  项目覆盖完整不表示所有测法都达到硬件峰值或没有运行波动。
- 直接使用App现有完整套件流程、选择14个原生Metal项目，一次启动顺序执行；
  不是逐项手工点击，也没有以此冒充真机点击全31项“一键全部”的验收。
  App负责计算、判定和JSON保存；电脑只启动及导出。
- 已安装包与上一轮`a15-app-budget-completion-20261017/package-provenance.json`
  的二进制SHA256一致，签名包来源为`addfaee`；本轮未修改生产代码、重建或安装。
  原先签名、源代码、SPIR-V哈希可追溯，见`package-provenance.json`。
- 手机完整Documents在`phone-before/`和`phone-final/`；`manifest.json`、`console.log`
  和套件原生日志保留每项状态、热状态和曲线。所有其他主历史及Mac live JSON哈希未变。
- `stop-command.json`、`stop-device.json`记录对实际App PID48051的SIGTERM成功。
  本轮已结束，无后台测试。小体积可读资料入库，大型构建产物没有复制。

## 资料和复核

[comparison.csv](comparison.csv)保留精确新旧值、单位、版本、时间曲线变化和状态；
[production-contract-audit.json](production-contract-audit.json)保留生产Swift模型确认；
[coverage-after.json](coverage-after.json)列出31项最新选用记录；
[verification.json](verification.json)包含完整性断言和14项明细。

```bash
python3 out/test-results/a15-metal-confirm-20261017/verify.py
```

原有A15待补已完成；本轮只有上述一次确认，不因为成绩差异继续增加重复测量。
其他设备待办和原生Metal平台的测法限制仍见根目录`APPLE-NEXT-TESTS.md`。
