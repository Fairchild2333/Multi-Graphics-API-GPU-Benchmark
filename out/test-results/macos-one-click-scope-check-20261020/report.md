# macOS GPU／CPU各一个全套按钮：范围复核（2026-10-20）

用户决定在其他Mac直接运行App，希望GPU按一次跑全套、CPU按一次跑全套。
当前实现已满足该分工，本轮核对源码入口、已打包Release worker的真实队列及相关回归，
没有改动测试引擎或生产按钮，也没有重新测分／操作手机或手表。

## 两个按钮

| 按钮 | 实际入口 | 范围 |
| --- | --- | --- |
| 一键测试GPU全套 | `--gui-worker --gpu-suite` | 所有枚举GPU与支持的API，逐个执行GPU项目；不跑CPU、不自动抓帧 |
| 一键测试CPU全套 | `--cpu-suite` | 标准算力全模式，接着跑缓存／内存／页面首次访问／存储；不依赖GPU枚举 |

按钮直接提交上述命令，不把页面的单项测试模式、粒子数量或自定义时长拼入。
全套算力步骤固定15秒；CPU自动覆盖可验证的大核／小核单线程及全核，缓存／内存
同样采用适用的核心类型策略，页面和临时文件I/O包含在内。全核无需另选核心类型。
没有L3的设备不会虚构L3速度；GPU缓存独立归属及系统swap速度也没有被声称已完成。

两按钮均受全局忙碌状态约束，CPU和GPU按顺序运行。GPU全套走所有GPU/API矩阵，
不受单项GPU选择限制；本机只有一个M4 Pro GPU，没有声称做了多GPU实体机验收。
MoltenVK仍遵守原有转译开关，开启后才进入GPU队列，成绩与原生Metal分组保存。

## 本机真实队列

执行的是App内`Contents/Helpers/gpu_benchmark`的只列清单命令，不是手写清单：

| 范围 | 本机任务数 | 组成 |
| --- | ---: | --- |
| GPU，转译关闭 | 17 | Metal14＋OpenGL Burn3 |
| GPU，转译开启 | 30 | 上述17＋Vulkan13 |
| CPU | 2组 | cpu_all＋cpu_membw，展开为全部算力／曲线／页面／存储阶段 |
| 整机，转译关闭 | 19 | GPU17＋CPU2 |
| 整机，转译开启 | 32 | GPU30＋CPU2 |

Metal14项为四档粒子各绘制／纯计算共8项、Burn三档、流体、内存及缓存诊断。
五份清单均与声明数量一致；GPU清单没有CPU，整机只追加一次CPU两组，所有本轮
列出的计分任务capture=0。不支持的组合不排队，其他Mac的实际数量按硬件能力变化。
CPU两组任务不等于两条成绩；Apple Silicon算力会分别保存大核、小核、全核记录。

## 验证与边界

- `result_quality`、`complete_suite`、`cpu_cancel`、`suite_process`：4/4通过，2.64秒。
  包括队列能力组合、取消CPU工作、停止整个子进程组以及质量提示规则。
- 已修正macOS README旧18/31计数；该旧清单尚未包含后来加入的GPU缓存诊断。
- 原六台设备主历史SHA不变，Mac live仍171条、逐字节不变。
- 本轮**未点击按钮执行新的完整GPU／CPU测分**，因此不把清单／源码与回归核对
  写成新一轮全套实机性能验收。此前按钮实际启动／取消验证和历史测分保留原记录。
- 包版本、GUI与worker哈希、源码提交、清单及历史校验见`verification.json`。

## 记录

- `gpu-native.log`、`gpu-translated.log`、`cpu.log`、`all-native.log`、`all-translated.log`
- [解析后的完整计划](plans.json)、[验证摘要](verification.json)、[回归原始日志](native-regressions.log)
- [App使用说明](../../../macos-gui/README.md)
