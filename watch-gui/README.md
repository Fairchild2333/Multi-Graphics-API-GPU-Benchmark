# Mangekyo CPU for Apple Watch

独立 watchOS App，最低 watchOS 10。无需先安装 iPhone 伴侣 App；本入口仅包含 CPU
测试，不链接 GPU 引擎、Metal 或 MoltenVK。真机性能验收仍需连接 Apple Watch，
模拟器只用于检查流程，成绩来自运行模拟器的 Mac。

## 一键流程

首页点“一键测试 CPU 全部项目”，顺序执行两个任务，保存三条原始成绩：

1. 共享混合整数、分支、FP32/FP64 算力内核：先测全核，再测所有可用逻辑处理器的单线程轮次，
   保存单线程汇总及全核心成绩。
   两条成绩均完成后才算任务完成。每项总计 15 秒测量、0.2 秒预热，三轮取中位。
2. 缓存／内存：单线程及全核 read/write/copy 工作集曲线，并测页面首次访问、
   顺序及随机文件读写，保存一条包含全部曲线和子项的 `cpu_membw` 成绩。

Watch 没有本项目已验证的编号核心绑定或大小核驻留测法，所以使用系统自动调度，
不输出未经验证的 P/E 核成绩。全核使用引擎发现的全部可用逻辑处理器；单线程轮次
在 Watch 上不能解释为绑定到了不同物理核心。混合内核与其他平台复用，但 Watch
编译目标、调度和内存预算单独记录，不直接混排旧平台成绩。

运行时“停止测试”位于顶部。离开前台会请求取消；系统终止进程后，重启会把未完成
任务标为中断。保持 App 前台，测试前等待设备冷却。没有创建后台健身会话。

## 内存和结果解释

- 单线程扫描最大 16 MiB，且不超过 `os_proc_available_memory()/4`；额度未知时
  回退 8 MiB，少于 1 MiB 时拒绝开始。全核工作集另设 16 MiB 总上限，保留共享
  引擎按实时可用内存进一步缩小的保护。页面区域上限 8 MiB。
- 文件读写使用 32 MiB 临时文件，结束后删除。`storageDirectIo` 记录是否成功启用
  uncached I/O；不能把文件访问成绩当作虚拟内存或交换文件速度。
- 仍使用共享平台判定门槛。小工作集未达到内存平台时显示“读取参考值”，任务为
  `review`，保留曲线，不冒充正式 DRAM 速度。缓存层级沿用共享引擎的 OS 信息及
  工作集推断，不声称独立归属 GPU L2/L3 或 SoC/SLC。
- “已保存成绩”可以查看历史曲线、页面和存储子项。界面按系统首选语言自动使用
  中文或英文。

数据位于手表 App 沙盒 `Documents/Mangekyo/`：

```text
results/results.json       # 共享 schema 的原始成绩及全部曲线
logs/*-compute.log         # 算力原生日志
logs/*-memory.log          # 缓存、页面和存储原生日志
suite-runs/latest.json     # 最近一键任务状态
suite-runs/<UUID>.json     # 每次任务及对应 result IDs
```

原始成绩 `platform=watchOS`、版本包含 `_watchos_preview`，并记录
`watchProfile=cpu_watch_v1`、`watchRunProfile=standard` 及实际处理器架构。
模拟器另加 `_simulator`、`isSoftware=true` 和
`performanceScope=host_cpu_not_watch`。开发用 `--watch-smoke` 缩短算力时间并将
扫描限制为 1 MiB，版本另加 `_ui_smoke`；正式界面默认不使用此配置。

## 编译和测试

需要 macOS、Xcode 的 watchOS SDK；生成项目时使用 XcodeGen。生成后的 Xcode
项目和共享 scheme 已纳入仓库，正常构建无需再次安装 XcodeGen。

```bash
# 两种 Release 包；仅编译，不签名或安装
bash scripts/build-watchos.sh
# 或单独编译真机／模拟器
bash scripts/build-watchos.sh --sdk watchos
bash scripts/build-watchos.sh --sdk watchsimulator

# 修改 project.yml 后重新生成
xcodegen generate --spec watch-gui/project.yml --project watch-gui

# UI 测试实际点击按钮；标准配置在高核心数 Mac 模拟器上可能需要数分钟
xcodebuild test -project watch-gui/MangekyoWatch.xcodeproj \
  -scheme MangekyoWatch -configuration Release \
  -destination 'platform=watchOS Simulator,id=<WATCH_SIMULATOR_UDID>' \
  -derivedDataPath build-watchos/test ENABLE_TESTABILITY=YES \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO -parallel-testing-enabled NO
```

真机安装：在 Xcode 打开 `MangekyoWatch.xcodeproj`，选择开发 Team 和实际 Watch，
由 Xcode 完成开发签名。按手表系统要求启用开发者模式并信任签名；当前构建脚本的
unsigned 包不能直接安装。尚未验证真机的执行时长、前台保持、热状态或速度。
当前流程验收及原始模拟器记录见
[实现与验证报告](../out/test-results/watchos-cpu-implementation-20261017/report.md)。
