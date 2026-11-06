# GhostLock 黑屏后重启：深入检查

检查日期：2026-10-10，目标 MIX Fold 3 / babylon / OS3.0.304.0.WMVCNXM。

## 本机新增证据

- `/sdcard/MIUI/debug_log/powerinfo/result_reason` 最新一条记录：`kernel reboot : kpanic`，记录时间 2026-10-10 23:31:56，与这次黑屏后重启相吻合。已经备份为 `miui-power-reasons.txt`。相比普通 bootreason 的 `reboot`，这是支持发生内核 panic 的更明确证据；仍缺具体 panic 堆栈。
- DropBox 在该时间只有 `SYSTEM_BOOT`，未发现对应的 `SYSTEM_LAST_KMSG`、watchdog 或 tombstone 条目。普通 shell 不能读取 `/sys/fs/pstore`；`/proc/last_kmsg` 不存在。
- GhostLock 的“上次攻击未完成”提示可以进一步缩小范围。源码读取 `.ghostlock_run_state.json`，取第一个 `in_progress` 步骤：w3 前缀会显示另一条 Shizuku 建议，非 w3 才显示本次提示。步骤全集为 w1a、w1b、w2a、w2b、w3a、w3b、w3c，因此本次残留状态属于 W1/W2。它是最后保存的阶段标记，不是具体内核崩溃栈。
- 以上提示分支还在用户提供 APK 的 DEX 中核对过：`"w3"` 判断后的两个分支分别引用 `0x7f0a011f/011e`（W3 提示）和 `0x7f0a00e8/00e7`（上次攻击未完成）。证据保存为 `apk-w3-hint-branch.txt`，资源名由 aapt 核对。
- 精确的 a/b 子步骤文件在应用私有目录。合法的 `adb shell run-as com.ghostlock.app` 返回 `package not debuggable`，无法读取。未绕过访问控制，也未再次触发提权。
- 已保存的运行日志与 profile.conf 都是 NUL 字节。源码 `DebugAttackLog` 每行 flush，但没有文件描述符 sync；突然崩溃时未持久化的数据是一个可能解释，不能证明全零文件的唯一原因。阶段记录文件则使用 `fd.sync()`。
- 当前 logcat 中 init 的 SIGABRT 出现在重启后启动期间，不能当作上次 GhostLock 原生进程的 panic 堆栈。

## 网上查到的对应问题与修复方向

1. [Issue #316](https://github.com/YuKongA/ghostlock-app/issues/316)：小米 13、SM8550、完整 uname 与本机完全一致。默认 Direct 路线 W1 写失败；把 waiter_off 从 96 改成 64 后死机重启；Shizuku 也死机且日志全为 NUL。检查时仍 open，comments=0。属于高度相关的失败报告，没有已确认解决方法。
2. [Issue #269](https://github.com/YuKongA/ghostlock-app/issues/269)：另一台 SM8550 / 5.15 android13 设备 W1 失败。切换 CPU 对、提高堆准备次数、使用 Shizuku 均未解决 W1。报告对 errno=35 的归因是作者推测，其他成功适配记录将它视作预期路径，不能仅凭这个返回值判断根因。
3. [PR #300](https://github.com/YuKongA/ghostlock-app/pull/300)：开发分支包含 multicast 重复注入/再次触发、严格写入成功验证、线程 join 超时保护等改动。其说明及测试针对 iQOO/vivo；没有本机验证。PR closed 且 merged=false，不能声称它已进入当前 main，也不能直接使用其机型配置或 KSU 模块。
4. [PR #228](https://github.com/YuKongA/ghostlock-app/pull/228)：上述 multicast 稳定性方向的较早改动，merged=true，但目标分支是 `vr-ko-bypass-dev`，并非 main。当前 main 的 `multicast_waiter_route.cpp` 仍使用单次 setsockopt；没有这些重复尝试参数。该区别由 API 与源码共同核对。
5. [官方适配指南](https://github.com/YuKongA/ghostlock-app/blob/main/docs/kernel_profiles/README_ZH.md) 明确说明提取成功只是未验证候选；task/cred/multicast 布局错误可能导致黑屏或重启。静态检查通过与真机利用成功是不同验证环节。

## 判断与后续方向

现有证据更支持研究 multicast 路线的适配与稳定性，而不能据此断定设备永远无法 root。仍不能区分布局错误、竞态失手、W1/W2 修复写问题或线程卡死，必须取得更细的阶段记录或 panic 堆栈。

可继续离线审查上述通用稳定性改动，对照本机镜像验证布局；若需要构建调试版，先补充可导出的阶段记录和持久化日志，再验证移植。PC 实时接收输出可以避免只依赖手机共享存储日志。这些是待验证的研究方案，不是已确认能解决 Fold 3 的方法。本轮只读取诊断与源码，没有安装其他分支、修改内核偏移或再次执行载荷。
