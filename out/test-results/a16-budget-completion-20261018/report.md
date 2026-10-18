# A16 九项补测完成（2026-10-18）

实体 iPhone 14 Pro Max（iPhone15,3、A16、iOS27.0.1），18:06:38–18:13:16
Australia/Sydney，约398秒。现有 App 流程定向补九项，**9/9完成、8 passed／1 review、
0执行失败**，每项系统热状态均0→0，低电量模式关闭。没有重复跑此前通过的项目。

## 新成绩

| 项目 | 成绩 | 状态 |
| --- | ---: | --- |
| Vulkan Burn64 | 1.016 Gpix-step/s | passed |
| Vulkan Burn256 | 1.097 Gpix-step/s | passed |
| Vulkan GPU内存读取 | 45.044 GB/s | passed，512MiB平台确认 |
| Metal GPU缓存诊断 | 379.452 ns/load | passed，归属仍未确认 |
| CPU大核单线程 | 169.945 MWork/s | passed |
| CPU小核单线程 | 100.672 MWork/s | passed |
| CPU全核（6线程） | 721.677 MWork/s | passed |
| CPU全核内存读取 | 29.770 GB/s | passed；分项限制见下文 |
| OpenGL ES2 Burn16 | 1.773 Gpix-step/s | passed |
| OpenGL ES2 Burn64 | 7.085 Gpix-step/s | passed |
| OpenGL ES2 Burn256 | 13.237 Gpix-step/s | review，运行中下降13.54% |

Vulkan 经 MoltenVK 转为 Metal；对应记录明确保存该实现和同步计时身份。
GLES2是独立的legacy工作负载，不把其Burn数值与Metal/Vulkan当相同着色器的API排名。
新旧成绩没有混改或挑选最高分。

## 内存与曲线

- Vulkan新版 `membw_v3_streaming_ios_preview_app_budget_v1_moltenvk`：读取45.044、
  写入46.4、复制43.4GB/s；512MiB、83点、38次write/copy校验、0时间戳异常。
  `dataVerified=1`、`memoryPlateauReached=1`，预算符合实时App余量四分之一策略。
  `ios_preview`保留iOS版本分组，不表示本次平台未达标。
- 256/384/512MiB三个尾部均完整遍历：读取44.898/45.344/45.044，写入
  45.962/46.071/46.381，复制43.417/43.645/43.405GB/s。它是内核访问平台，
  没有硬件DRAM流量计数，也不能由此独立归属GPU L2/L3/SLC。
- CPU保存**267个吞吐曲线点**，覆盖大核、小核、全核的read/write/copy；
  **186个单线程驻留验证点**，均达99%门槛。全核曲线没有逐线程类型驻留验证。
- CPU分项大核内存读取49.6GB/s的 `single_memory_settled=0`，**平台未稳定，
  只能参考**；小核读取24.6、全核29.8GB/s的对应settled标记为1。
  App主分数采用全核读取，故CPU内存任务passed不代表每条分项曲线均达到平台。
- CPU页面首次访问：16KiB页、1563ns/page；1GiB文件直接I/O顺序读/写563/390MB/s、
  随机4K读/写49/30MB/s。页面首次访问和文件I/O均已测，**不是系统swap换入换出速度**。
- Metal GPU缓存保存**124点**，checksumValid=1、cacheAttribution=unverified。
  沿用跨平台一致的未归属诊断，不声称测出了独立L2/L3/SLC速度。
- 原生Metal内存仍是旧的preview诊断，本轮未重测，也没有升级成正式DRAM平台。

## CPU类型与结果解释

大核三轮请求类型驻留下界最低99.935%，小核最低99.9989%；工作线程忙碌比例
最低约100.0%/99.685%。小核采用两条大核辅助负载的前台策略，辅助线程驻留也
通过99%门槛；小核有效频率约2.016GHz。大核该计数方法未提供有效频率，记录0
表示不可用，不是0GHz。

全核六条工作线程贡献合计721.677；这是scheduler-managed执行，pinnedThreads=0，
线程标签未归属具体编号／类型。不能把六个约120的贡献分数解释成六个相同核心，
也不能把独立单核分数简单相加预测全核结果。

## 状态、历史与验证

当前正式清单**31/31项有匹配的正数成绩，选用33条记录**。旧24条逐项不变，追加11条
（CPU算力一项产生三条），主历史现在35条。旧24条含22条完整结果和2条中断诊断；
这些诊断保留，但0分不会作为完整覆盖。生产匹配器、请求、版本、数量、结果组、
质量状态与保存曲线均通过复核；覆盖按设备能力及最新匹配记录选择，不按最高分选择。

当前仍保留四个稳定性提示：旧Metal Burn256下降16.69%、旧Vulkan Burn16下降19.08%、
旧Vulkan1M绘制无稳定窗口，以及本轮GLES2 Burn256下降13.54%（稳定段14.088、CV0.80%）。
本轮热状态均nominal；没有GPU频率遥测，不把这些直接称为已证实热降频。
CPU大核内存分项的平台未稳定另行记录，不在上述四个吞吐提示内。不为去掉提示重复挑分。

运行前已从设备备份Documents，24条JSON与16日主历史逐字节一致。复用此前通过验证的
签名Release包，无重新编译、生产代码或阈值修改；二进制SHA及签名／设备名单确认见
`package-provenance.json`、`install.json`。这是现有整套引擎的九项定向补测，
**没有声称本轮在真机点击一次按钮重跑了完整31项**；模拟器完整按钮验收见17日报告。

完成后已导出整个Documents并核对35条、九项完成状态和11个唯一新增ID，随后停止App
PID3140，已告知可以拔线。控制台退出1来自完成后的主动SIGTERM，不是测试失败。
旧六台主历史快照及Mac live文件的SHA均未改变；仅索引后续切换到A16新35条导出。

## 文件

- [完整35条原始历史](phone-final/results/results.json)、[套件清单](phone-final/suite-runs/latest.json)
- `phone-final/suite-runs/`：包含本轮完整原生日志；`native-console.log`保留控制台生命周期
- [当前31项选用成绩](current-selected-results.csv)、[新增成绩](new-results-summary.csv)
- [Vulkan83点](vulkan-memory-points.csv)、[CPU267点](cpu-memory-curves.csv)、
  [CPU驻留186点](cpu-residency-points.csv)、[GPU缓存124点](gpu-cache-curves.csv)、
  [Burn时间线](gpu-burn-timeline.csv)
- [机器复核结果](verification.json)、[生产模型审计](production-contract-audit.json)、
  `coverage-before.json`、`coverage-after.json`、`verify.py`
- 原始备份、历史SHA、签名和安装记录、停止与退出原因均在本目录；编译审计程序仍忽略。
