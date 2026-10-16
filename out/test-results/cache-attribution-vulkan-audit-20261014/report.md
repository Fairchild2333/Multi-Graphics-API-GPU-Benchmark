# 缓存测法与 Vulkan 代码核查（2026-10-14）

按用户选择，这轮先查测法和代码，没有接回手机，也没有新增真实手机成绩。正常全套成绩由 App 在设备内计算，保存到 `results/results.json`；GPU L2／L3／SoC-SLC 独立归属仍未确认。新的交叉访问确认方法及资料依据已写入 [测法记录](../../../docs/cache-attribution-methods.md)。

## 已完成的修复

- Vulkan 内存校验不能执行、无法回读或校验失败时，拒绝保存未经校验的成绩。
- 补齐计算／传输／CPU 回读的显式内存依赖；多工作组错误标记采用原子操作。
- 每个写入和复制工作集在计时之后验证实际内容，复制检查覆盖目标缓冲。
- 写入／复制改为每块数据由一个工作组负责，重复读写在其内部执行，排除跨工作组写入竞争；volatile 保留要求执行的访问。载荷吞吐仍不等于缓存线或总线的直接流量。
- 起始计时核验失败可整轮采用同步墙钟。内存扫描中途时间戳异常会使整轮无效，避免混合计时来源。
- 安卓／鸿蒙 GPU Burn 增加同步提交／等待与 GPU 时间戳的启动核验。渲染测试在正式计分后切换计时来源，清理资源后拒绝保存混合成绩。
- 新内存合同为 `membw_v2_verified_*`，含校验标记和检查次数；三个移动端的新全套流程要求该合同。旧 v1 成绩保留。
- 没有达到内存平台段的预览明确使用最大工作集测量，保存为 `score=read_largest_datasets`，不写成已经确认的内存平台段。
- App 自行枚举可用 Vulkan 性能计数器，保存是否公布扩展、枚举状态及名称。仅枚举，尚未采样；没有把可用计数器名称冒充硬件层级实测。

## 验证结果与范围

| 检查 | 结果 |
| --- | --- |
| Android APK，4 个 ABI | 构建通过，JUnit 5／5 通过 |
| HarmonyOS HAP，native／ArkTS | 构建通过，实际生产模型／结果匹配／流程回归通过 |
| iOS 真机 Release 包 | 编译通过，未安装或启动真机 |
| iOS 完整队列合同 | 通过，拒绝旧未验证内存合同和异常计时标记 |
| C++ 原生合同／质量／队列检查 | 通过 |
| 真实 Vulkan 函数正向运行 | 29 个曲线点，数据校验、写入和复制检查通过 |
| 故意让校验标记不出现 | 无有效报告，拒绝结果 |
| 故意让复制 shader 不复制数据 | 发现复制内容错误，无有效报告 |
| 模拟正式计分后切换计时 | 清理完成后拒绝保存新行；正常模拟对照保存一行 |
| APK／HAP shader 包装检查 | 两包均包含改后的错误标记原子指令，与各自生成资源一致 |

正向 Vulkan 与两个故障注入使用 Mac 上的 MoltenVK，只验证共享代码真实调用和错误路径，不构成安卓／鸿蒙／A15 的性能验收。计时切换保存检查使用模拟 backend，数据在 `isolated-history*` 中，**其中的 Timing contract test 成绩是测试夹具，不应参加任何设备比较**。这些隔离目录自动迁入的旧历史也是夹具副本。

本轮没有连接安卓／鸿蒙真机。iPhone 已断开，按用户选择暂不继续真机测试。因此不能宣布新 Android／HarmonyOS 驱动路径实际性能已验收，也不能宣布 A15 缓存归属已确认。

## 成绩与下一次复测

原 128 条 iPhone 历史与此前精选成绩文件没有改写。新 `membw_v2_verified` 合同需要以后补测 Vulkan GPU 内存这一项；旧 v1 记录仍可查看，不能当作新校验合同的记录复用。A15 如继续使用 Vulkan，可只补这一项；此前通过的 Metal 和 CPU 项不需要因此重跑。A12X 原 13 项计划增加 Vulkan 内存 1 项，共 14 项／16 条记录。Mac 如升级到新 Vulkan 内存合同，也需要对应单项新记录。

GPU 缓存归属的下一轮应增加 GPU→CPU 首读及私有缓存、页面转换对照。研究方案来自 M1 的原始工作，但其映射／阈值尚未在 A15 验证，本轮没有新增该探针代码或伪造它的成绩。可用接口仍不足时，应保留未归属状态。

软件包仍在构建输出目录：[APK](../../../android/app/build/outputs/apk/debug/app-debug.apk)、[HAP](../../../ohos/entry/build/default/outputs/default/entry-default-unsigned.hap)。实际路径、大小和 SHA256 见 [包核对](packages.json)。

证据：[机器核对](verification.json)、[Android 构建](android-final-build.log)、[鸿蒙构建](ohos-final-build.log)、[iOS 构建](ios-final-build.log)、[正向运行](valid-vulkan-runtime.log)、[未执行校验故障](notrun-vulkan-runtime.log)、[复制故障](wrong-copy-vulkan-runtime.log)、[计时保存保护](contract-save-check.log)、[iOS 合同](swift-suite-check.log)。
