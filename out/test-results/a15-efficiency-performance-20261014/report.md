# A15 性能优先级小核算力：完成复测（2026-10-14）

设备：iPhone 13 Pro / Apple A15，iOS 27.0.1，开发签名真机。用户要求测出小核正常
性能条件下的成绩；旧背景策略的 26–30 MWork/s 不满足此用途。本轮已用新的
性能优先级方法分别测出小核算力，两轮相差 0.12%。

## 实测成绩

| 轮次 | 大核单线程 | 小核单线程 | 六线程全核 | 小核平均频率 |
| --- | ---: | ---: | ---: | ---: |
| 首轮 | 146.981 | 89.658 | 661.517 | 约 2.016 GHz |
| 确认轮 | 147.120 | 89.551 | 661.962 | 约 2.016 GHz |

算力单位 MWork/s，仍使用原 mixed integer/branch/FP32/FP64 内核，15 秒测量、
0.2 秒预热、三轮取中位数。两轮所有任务的热状态都是 0→0。

小核分数来自一个 E 计分线程的实际工作量，不是六线程平均，也不是从全核分数
减掉两个大核后推算。三轮 E 归属保守下界均约 99.999%；确认轮实际线程 CPU
时间 4.986／4.990／4.992 秒，占每轮五秒墙钟约 99.72–99.84%。平均 GHz 是
实际周期／CPU 时间，不声称控制住固定最高频率或绑定到某一编号核心。

确认轮简单估算 `2×147.120 + 4×89.551 = 652.444`，比独立全核实测 661.962
低约 1.44%。独立单线程与全核的运行顺序、并发和调度上下文不同，不能要求
严格相等；现在不再出现把背景 E=26 与正常全核 660 混为同一条件的解释。

## 方法和有效性

E 计分线程使用 `QOS_CLASS_USER_INITIATED`，两个 P 辅助线程使用
`QOS_CLASS_USER_INTERACTIVE`。辅助线程只使 P 类型核心保持忙碌、提供调度压力，
其任何工作量均不进入 E 分数；全核阶段在这之前单独完成，亦不包含辅助线程。
辅助线程运行原 CPU 内核，使用独立寄存器状态，没有新增计分公式或额外内核。

公开的进程时间差法无法在两个同时工作的 P 辅助线程存在时严格证明 E 目标线程
的归属，因此新方法使用 Apple Recount 的按线程、按性能等级 CPU 时间与周期计数。
`thread_selfcounts` 是未公开 SPI，运行时解析，来自
[Apple ABI 定义](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/sys/resource_private.h)
及 [Recount 实现](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/sys_recount.c)。
本轮是当前开发签名设备的验证，不代表 App Store 分发已经验收。

每轮要求至少 99% CPU 时间在 E 类型，至少 90% 墙钟时间实际获得 CPU；两个
辅助线程也必须通过至少 99% P 类型验证。确认轮辅助线程的下界分别为
99.852%、99.721%。每对快照以公开线程 CPU 时钟包夹，并以 50us 容差核对时间
守恒。接口缺失、计数倒退、ABI 不一致、核心迁移超过门槛或辅助负载未通过时
拒绝成绩，不能只凭 QoS 或分数大小认定是小核。无需 root 或改变设备系统设置。

## 接入、历史与检查

- iOS 默认 CPU 全套和整机全套使用新 E 算力方法，没有新增界面选项。
- 新 E 保存 `scheduling=user_initiated`、`efficiencyPolicy=foreground_with_performance_load`、
  `residencyMethod=thread_selfcounts_perf_level_spi`、每轮驻留／GHz／CPU 时间、辅助
  线程数及归属下界；版本追加 `_foreground_e_p_load_v1`，与背景策略分组。
- 新版续测不接受旧背景 E 成绩替代性能策略 E；P/E 类型不能重复或未验证。
  界面显示“小核（性能优先级）”，解释辅助线程不计分。
- 113 条原成绩逐对象原样保留，新增两轮共 6 条，总数 119；完整设备选择仍为
  33 条。旧缓存／内存曲线按其背景策略合同保留，本轮更改的是 CPU 算力 E 方法。
- iOS Release 构建、签名安装通过；153 项共享 Swift 解析检查、清单和续测合同
  检查通过；CPU 类型计数、取消（包括辅助线程回收）及整套流程原生检查通过。

首轮编号：
`20261014-163612-715-cpu-single-performance`、
`20261014-163612-726-cpu-single-efficiency`、
`20261014-163612-735-cpu-multi`。

确认轮编号：
`20261014-164342-791-cpu-single-performance`、
`20261014-164342-803-cpu-single-efficiency`、
`20261014-164342-812-cpu-multi`。

最新原始成绩：[results.json](foreground-confirm-final/results/results.json)。
完整选择：[curated-suite-results.json](curated-suite-results.json)。
核对数据：[verification.json](verification.json)。
两个完整原生日志、清单与当时的 Documents 分别保存在
`foreground-first-01-cpu_compute`、`foreground-confirm-01-cpu_compute`。
源码修改尚未提交或推送；本轮未重测其他设备。
