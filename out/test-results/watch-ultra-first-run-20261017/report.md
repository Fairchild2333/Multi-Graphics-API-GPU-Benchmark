# Apple Watch Ultra 首次完整 CPU 测试 · 2026-10-17

实体Apple Watch Ultra（Watch6,18），watchOS26.5，双核arm64_32。
用户调整亮屏／返回时钟设置并佩戴解锁后，实际自动点击首页一键按钮，
**2/2任务完成、保存3条成绩**。标准配置，无smoke、无保活触摸循环，界面跟随系统显示英语。
XCTest1/1通过、0失败，约88.1秒含App启动与截图；内核测量加内存任务约75秒。
全部JSON、六条曲线126点、原生日志和任务manifest已从手表导出。

## 成绩与限制

| 项目 | 本轮结果 | 解释 |
| --- | ---: | --- |
| 单线程混合算力 | 53.672 MWork/s | 两个逻辑处理器的单线程轮次汇总；系统调度，不能解释为编号核心绑定 |
| 全核心混合算力 | 116.499 MWork/s | 2线程，选中轮次贡献58.271+58.228，与总分相符 |
| 单线程L1／L2读取 | 19.3／15.8 GB/s | 缓存信息来自sysctl，48KiB L1、共享4MiB L2；使用项目共享工作集划分 |
| 双线程L1／L2读取 | 39.0／29.8 GB/s | 工作集扫描结果，不称硬件峰值 |
| 双线程大工作集读取参考 | 5.614 GB/s | **平台未确认，review**；不得称已验证DRAM峰值 |
| 页面首次访问 | 5549 ns/page | 16KiB页面，已测；不等于交换文件性能 |
| 顺序读／写 | 325／235 MB/s | 32MiB临时文件，uncached I/O |
| 随机4KiB读／写 | 14／5 MB/s | 同一存储任务，已测，临时文件结束后删除 |

算力每项15秒测量、0.2秒预热、三轮取中位，全核先测，随后两个单线程轮次。
单线程汇总的测量时长30秒，全核15秒，缓存／内存／页面／存储任务30.2秒。
单线程扫描4KiB→16MiB共23点，双线程扫描每线程4KiB→4MiB共19点，
每种分别保存read/write/copy三条曲线，共126点。non-temporal写入／复制路径在本目标未实现，
对应空字段原样保留，不虚填数据。预算与内存平台规则未降低。

算力任务passed，内存任务与整体review，原因仅为Memory plateau unconfirmed。
平台读取参考值和完整曲线可以保存，不能把“完整流程执行通过”写成“DRAM平台已确认”。
watchOS采用独立`_watchos_preview`、32bit版本与系统调度标签，不直接混排其他ABI的旧成绩。
GPU／独立L3或SoC-SLC归属不属于此CPU-only入口。
成功轮没有完整的起止热状态采样，不据此宣称峰值性能或全程固定热状态。

## 前台问题与本轮解决过程

最初尚未发现Watch；刷新配对后识别Ultra，用户开启开发者模式，DDI准备完成。
实际Watch加入签名profile，Release安装及亮屏时的远程启动成功。
测试包缺Bundle ID已在project.yml与生成工程补齐。

首次UI自动化因开启automation mode超时失败；解锁并保持亮屏后自动点击成功。
其后三次按钮运行分别约7、10、15秒后取消，没有保存任何零分或不完整成绩。
后两次lifecycle.log明确记录inactive后紧接background。App已改为只在background取消，
并保存状态及取消原因；[Apple说明inactive仍可能处于前台](https://developer.apple.com/documentation/watchkit/life-cycles)。
定时合成触摸也没有阻止后台转换，保留该次执行源码与日志，**已从最终测试撤回**。

最终先build-for-testing预编译，再test-without-building启动，缩短用户解锁后的等待。
用户按说明设置Wake for70Seconds、Return to Clock After1hour，并佩戴解锁后，本轮完成。
设置由用户操作，未独立读取其具体配置；只证明本轮成功，不保证以后离腕或切换应用仍可继续。
旧延长frontmost接口在watchOS7起不能由App请求；延长运行会话适用场景及CPU额度也受限制。
见[Apple前台说明](https://developer.apple.com/documentation/watchkit/taking-advantage-of-frontmost-app-state)、
[亮屏和返回时钟设置](https://support.apple.com/guide/watch/turn-on-and-wake-apple-watch-apd748b87e2a/watchos)。
没有关闭腕部检测、修改密码或伪装健身／健康后台会话。

此前测试强制中文，标准测试已取消语言覆盖；成功轮截图确认英语界面和Saved results(3)。

## 架构元数据修正

原始三条成绩的processArchitecture/osArchitecture写为ARM64。实际目标和ABI为arm64_32：
XCTest设备摘要、CoreDevice硬件信息及CPU`_32bit`版本一致。
Apple Clang定义`__arm64__`与`__ILP32__`，没有原判断所用的`__arm64_32__`。
已补ILP32识别；新Release双架构编译及安装通过，更新后手表三条历史逐字节未变。
**旧JSON保持原始标签，未改写旧分数，也未为标签修正另跑一轮。**
`physical-package-summary.json`保留实测包信息，后装元数据修复包另见
`architecture-label-fix-package.json`，不能将两者哈希混用。

## 证据与复核

- [完整原始成绩](watch-final/results/results.json)、[最近套件](watch-final/suite-runs/latest.json)、
  [原生日志](watch-final/logs/)、[曲线CSV](curves.csv)。
- [XCTest摘要](physical-system-settings-summary.json)、[运行日志](physical-system-settings.log)、
  [完成截图](screenshots/1115C68E-3CE5-4976-9955-52611F78884F.png)。
- [导出校验](archive-verification.json)：真机身份、标准参数、2线程贡献、六条曲线、
  页面和存储均核对；此前五台设备主历史及Mac live JSON的SHA256未变。
- [更新后历史保护](history-preserved-after-label-fix.json)、[架构宏](watchos-arm64-32-preprocessor.txt)。
- 失败尝试及取消manifest保留；失败会话在错误已记录后终止，部分xcresult未完成封装，
  不据此宣称通过。最终成功会话自然返回0且xcresult可读取。
- 大型xcresult仅本地忽略；可入库JSON、CSV、日志、截图和报告约数MiB。

复核：`python3 out/test-results/watch-ultra-first-run-20261017/verify_archive.py`。
成绩由Watch App自己测量和保存，电脑负责安装、UI启动及导出；后续可在手表直接点按钮。
真机实际曲线页点击和取消按钮的独立体验仍沿用此前模拟器验收，本轮只验收完整一键流程。
