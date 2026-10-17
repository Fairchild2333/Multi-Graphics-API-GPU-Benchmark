# watchOS CPU 实现与流程验收（2026-10-17）

已新增独立 watchOS 10+ CPU App：一键顺序运行算力和缓存／内存两个任务，保存
单线程、全核心、缓存／内存三条成绩，包含页面首次访问和文件存储子项。
真机架构和模拟器 Release 包编译通过，模拟器实际点击流程通过。
**未连接或测试实体 Apple Watch；本目录所有速度均来自 Mac 宿主，不是手表成绩。**

## 代码与测量范围

- 工程及使用说明：[watch-gui/README.md](../../../watch-gui/README.md)。
  单目标 `WKApplication=YES`、`WKWatchOnly=YES`，不需要 iPhone 伴侣 App。
- 直接复用共享 CPU 整数／随机分支／FP32／FP64 混合内核、read/write/copy
  扫描、页面首次写入、文件读写和 JSON 保存。没有修改内核公式或计时方式。
- 标准算力为全部可用逻辑处理器的单线程轮次和全核心任务，每项总计15秒测量、
  0.2秒预热、三轮中位。Watch 调度不声称绑定到编号核心，也不输出未经验证的
  大小核成绩。watchOS 排除现有 Apple P/E 私有驻留计数路径。
- 单线程工作集最多16MiB，按实时 App 额度的1/4缩小；额度未知回退8MiB。
  全核另设16MiB总上限，页面区域上限8MiB，存储临时文件32MiB。
  不降低共享内存平台门槛；未确认的平台保存曲线并显示读取参考值／review。
- 结果采用共享 schema，`platform=watchOS`、版本带`_watchos_preview`。
  模拟器强制`isSoftware=true`、`_simulator`和`performanceScope=host_cpu_not_watch`。
  快速界面配置另带`_ui_smoke`，与默认标准配置分开。
- 本地首页一键、顶部停止、进度、独立历史页、独立六条曲线页、中文／英文自动
  选择均已接入；离开前台请求取消，重启识别未完成任务。没有使用后台健身会话。

## 验证结果

| 检查 | 实际结果 | 证据 |
| --- | --- | --- |
| Watch 真机 unsigned Release | arm64_32、arm64 构建成功；尚未签名安装 | `build-final-watchos.log`、`device-package-info.json` |
| Watch Simulator Release | 构建成功 | `build-final-watchsimulator.log` |
| 两种真机架构的混合热循环 | 随机最低位`tbnz`分支和FP32/FP64 FMA均保留，内核无`fcsel` | `cpu-hot-loop-*.txt`、`cpu-hot-loop-verification.json` |
| 标准配置真实按钮流程 | 1/1 UI测试通过，2/2任务完成，约258.4秒含UI开销 | `test-standard.log`、`test-standard-summary.json` |
| 最终模型／质量判定 | 3/3单元测试通过 | `test-verified.log`、`test-verified-summary.json` |
| 最终点击、曲线、重启和取消 | 2/2 UI测试通过 | 同上及`screenshots-final/` |
| 共享代码构建／回归 | 构建成功，CTest 7/7通过 | `native-build.log`、`native-tests.log` |
| 原正式设备历史保护 | 五台设备主历史及本机 live JSON 的SHA256全部未变 | `histories-before.json`、`histories-after.json` |

标准配置在 watchOS27.0 / Apple Watch Series12模拟器执行，实际模型标识
`Watch8,3 (Simulator)`，宿主为M4 Pro，使用14个逻辑处理器。因此算力包含14个
15秒单线程轮次；不能把此运行时间或分数当作双核手表表现。

标准任务`5D889FF8-A1B7-47FE-AF92-CB49F5E064ED`保存三条成绩，算力passed；
模拟器`os_proc_available_memory()=0`，实际扫描回退8MiB，保存**108个曲线点**。
页面8MiB首次访问、32MiB顺序／随机存储均测完，`pageFaultMeasured=1`、
`storageMeasured=1`，未确认内存平台，内存任务和整体正确为review。UI完整执行
和内存平台通过是不同状态。最终快速界面轮次保存78个曲线点，并实际进入图表页。

累计保留本次开发流程的15条模拟器记录：一轮标准配置和四轮快速配置；部分来自
修正布局前已完成的基准流程，并非15条手表正式成绩。九份任务manifest包含五份
完成、三份取消及一份中断。全部原始结果、日志和状态在
[watch-documents/](watch-documents/)，曲线另导出为[curves.csv](curves.csv)。

## 修正过程中保留的失败记录

- 初始包同时设置`WKWatchOnly`和`WKRunsIndependentlyOfCompanionApp`，安装被
  系统拒绝。已删除后者，仅保留watch-only标记；随后安装及测试通过。
- 第一版长提示把停止按钮和历史行挤出小屏，UI测试失败；已把停止放到顶部，
  历史放进独立页面，修正后的取消、历史访问和重启测试通过。
- 中间图表与详情挤在同一列表行，自动化找不到曲线点标签。已改为专用曲线页，
  最终截图确认实际六条曲线可见，而不只是“曲线点”文字。
- 失败后Xcode测试会话未自行结束，已对本任务的对应xcodebuild进程发TERM；
  相应`test*-exit.json`保留-15。最终标准及最终复核会话都自然返回0。
  中间标准配置的存储说明使用了分号，最终版改为逗号，避免切断配置字段；旧原始
  日志仍逐字保留。

没有删除失败日志或改写旧成绩。最终截图：
[一键完成](screenshots-final/BE6C77CC-9CB8-45AB-94CA-F5154133B6F1.png)、
[实际曲线](screenshots-final/54ACC026-F19B-4ABA-A8CB-CCC0B9E342FC.png)。

## 待真机确认与收尾

实体Watch的签名安装、真实性能、热状态、低内存适配及保持前台的实际体验仍待
接入设备验证。没有声称获得Watch的DRAM平台、独立L3/SLC速度或交换文件性能。
此工作未改变iOS/Mac已有测法或正式成绩，也没有为其他待补项目提供新的真机成绩。

测试后App已由测试器结束；单独终止返回“无进程”符合该状态，专用模拟器已关闭，
见`stop-and-shutdown.json`。大的xcresult和二进制包仍本地忽略，可读日志、摘要、
截图、完整JSON／曲线和源代码／包哈希入库。

可重新核对导出、任务、平台标记和历史哈希：

```bash
python3 out/test-results/watchos-cpu-implementation-20261017/verify_archive.py
```

结果见`archive-verification.json`；编译来源及unsigned包SHA256见
`source-and-package-hashes.json`。源代码基于`740cec2`，最终提交包含本报告。
