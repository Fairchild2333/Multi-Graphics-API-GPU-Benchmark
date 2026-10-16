# M4 Pro 补测和 iOS 重编译（2026-10-16）

最新 `b637637` 已在本机重编译并完成指定补测。生产测试内容、计时和默认参数保持不变。

| 项目 | 新成绩 | 状态 |
| --- | ---: | --- |
| CPU 大核 | 207.532 MWork/s | 分支保留版 formal，三轮驻留通过 |
| CPU 小核 | 69.902 MWork/s | 后台策略 formal，三轮驻留通过，不是峰值 |
| CPU 全核 | 2436.328 MWork/s | 14 线程，三轮中位数 |
| Metal 1M 纯计算 | 162.761 GB/s | 稳定；GPU 与帧间隔均约 0.258 ms |
| Vulkan 1M 绘制 | 177.361 GB/s | 本轮下降 4.66%；历史波动原因仍保留 |
| Vulkan v2 内存 | 300.885 GB/s | **preview**：46 次校验通过，但没有读取平台，不能当作 DRAM 峰值 |

CPU 新版本带 `_kept_branch`，旧 312/3582 等保留在旧组。Apple clang 五个内核循环
均保留 `tbz`，循环内没有 `fcsel`；大核选中轮次约 4.034 GHz、19.44 周期/工作单元。

Metal 数学探针未改即编译运行成功。fast sin/atan2 吞吐为 FMA 的约 12.2%/6.7%，
支持特殊函数成本影响 Burn；没有其他厂商同实现探针，尚不能量化跨厂商原因。
FMA 实测 6.964 TFLOPS，仅为该探针吞吐，文档的 8–10 预估不是验收阈值。

M4 的短测没有证明 Metal 命令缓冲跨度计时有错；旧 Vulkan 内部阶段与 Metal 整个
compute 缓冲的计时范围不同，不能直接把旧分数比值归因于 Metal 固定开销。未改计时。

iOS 真机和模拟器 Release App / 原生库均编译成功，源码着色器、56 个 SPIR-V
及架构已核对；脚本产物未签名，真机安装仍需开发签名。本轮没有新增 iOS 性能成绩。
Mac 原生 CTest 7/7 通过。所有测量进程已退出。

Mac 主历史 **149→155**，旧记录不变，四台 iOS 主历史哈希不变。
详见 [完整报告、曲线、原始日志及限制](../out/test-results/macos-targeted-retest-20261016/report.md)、
[最新 Mac 历史](../out/test-results/macos-targeted-retest-20261016/results-after.json)
和 [五台设备主历史索引](../out/test-results/README.md)。
