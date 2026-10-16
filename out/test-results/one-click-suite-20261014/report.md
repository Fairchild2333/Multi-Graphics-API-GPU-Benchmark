# 一键测试改进与验收（2026-10-14）

已完成电脑内多 GPU 队列，以及 macOS、Windows、iOS/iPadOS 各自的三个按钮。

| 按钮 | 执行内容 |
| --- | --- |
| 一键测试 GPU 全套 | 当前电脑的所有可用 GPU × 支持的 API，按顺序运行粒子四档有画面/纯计算、GPU Burn 三档、流体、GPU 内存，以及适用的 GPU 缓存诊断 |
| 一键测试 CPU 全套 | 完整 CPU 算力模式，再运行缓存/内存曲线、页面映射和存储测试；全核心默认包含 |
| 一键测试全部项目 | GPU 全套 + CPU 全套；电脑多 GPU 队列完成后，CPU 只测一次 |

手机和平板在各自应用里启动全套。项目由设备能力生成；无对应 API 的项目不会进入队列。Apple 的 Vulkan 仍是 MoltenVK 转译，只有打开现有“允许转译 API”开关才会加入。

当前 M4 Pro：GPU 原生 17 项，允许转译后 30 项；CPU 为两组任务；整机对应 19 / 32 项。任务数不等于成绩条数，CPU 算力会保存多个成绩。正式 GPU / CPU 算力步骤采用 15 秒，缓存、内存和存储按自身测量计划运行。新成绩按钮不自动抓帧，原有抓帧预设保留。

## 成绩和异常处理

- CPU/GPU 共用 results/results.json，追加保存；Mac/Windows 可直接打开结果文件夹，iOS Documents 可通过文件 App / Finder 文件共享访问。
- iOS 每项开始前等待系统热状态回到正常，最多等 600 秒；超时会记录失败并继续后续项目，不生成该项成绩。
- 计时重叠、GPU 时间明显超过帧间隔、固定重负载没有稳定阶段，以及吞吐下降超过 5% 会标记待复核。原始成绩保留，剩余队列继续。流体场景变化，不要求固定负载的稳定阶段。
- 停止会中断当前队列。Mac 已实际验证 CPU 全套启动与取消；Windows 使用进程树终止，需在 Windows 实机完成验收。
- macOS Metal 有画面测试同步为串行 GPU 计时，修复此前计算/渲染区间重叠的问题；新版合同与旧成绩分组。Mac MoltenVK 也启用同步耗时核对。

## 验收范围

- Mac arm64 完整应用构建、签名与独立 worker 运行检查通过。
- iOS 真机、模拟器构建通过；最终签名应用已安装到 iPhone 13 Pro 和 iPad 12.9 第三代。
- 两台 iOS 设备目前锁屏，系统拒绝远程打开应用；安装成功且原成绩已读回核对，可解锁后自行打开新版。
- C++ 回归测试 6 / 6 通过；Swift 实际生产模型的队列范围、旧成绩兼容、断点续测和异常成绩检查通过。
- Mac 界面确认三个入口；CPU 全套正式 15 秒启动后取消，按钮恢复可用。
- Windows XAML 与代码已更新，包括真实队列序号/当前 GPU/API，以及 CPU 算力和内存结果处理。本机没有 Windows SDK，未构建或运行 Windows 版本，也未在多 GPU 实机验收。
- 本次隔离实测四个代表项目，全部 GPU 区间与帧间隔核对通过：

| Mac 项目 | 新成绩 | GPU 总时间 / 帧间隔（ms） |
| --- | ---: | ---: |
| Metal 粒子 4M，有画面 | 153.275 GB/s | 6.861 / 7.030 |
| Metal GPU Burn 256 | 9.658 Gpix-step/s | 48.857 / 49.151 |
| Metal 流体 | 69.352 MParticle-step/s | 46.453 / 46.746 |
| Vulkan GPU Burn 256 | 7.102 Gpix-step/s | 66.446 / 66.753 |

这四项用于验证新计时，未重跑整套 19 / 32 项；其他先前复测候选仍保留在原复测清单。真实热降频仍可能发生，待复核状态不能保证硬件不降频。

原有正式历史经文件哈希确认保持不变：Mac 137 条、iPhone 67 条、iPad 43 条。隔离验证目录首次加载了仓库 220 条旧成绩，只新增了上述 4 条，精选文件 mac-validation-selected.json 仅包含这 4 条。

详细依据：native-tests.log、swift-suite.log、plan-*.log、plans.json、mac-validation-selected.json、preserved-history.json、macos-build-delivery.log、signed-ios-final.log、simulator-build.log、iphone-install-final.json、ipad-install-final.json。
