# M4 Pro 核心绑定核对（2026-10-13）

手机对比按用户要求暂缓，本轮只核对 Mac 的调度接口，没有运行新的性能测试。

本机报告 10 个性能核、4 个能效核。对探测进程自己的线程调用
`thread_policy_set(... THREAD_AFFINITY_POLICY ...)`，返回 46：
`(os/kern) service not supported`。两个 QoS 自设置调用均返回 0（成功）；
探测进程随后退出，不修改其他进程或持久系统设置。

`THREAD_AFFINITY_POLICY` 的 tag 本来就是同缓存线程分组提示，并非 CPU 编号。
公开且受支持的接口不能用于将这个 M4 Pro 的普通应用线程严格绑定到 CPU 0–13。
QoS 可以影响系统对 P/E 核的选择，但不保证指定核心。若用 QoS 做大小核对照，
还需要调度轨迹核对实际核心归属，不能仅凭优先级给结果标注 P/E。

来源：[Apple 的 affinity 定义](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/mach/thread_policy.h#L173)
与 [Apple 调度说明](https://developer.apple.com/videos/play/tech-talks/110147/)。
实际调用输出见同目录 `probe.txt`，探测代码见 `probe.cpp`。
