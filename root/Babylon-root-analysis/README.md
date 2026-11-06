# MIX Fold 3 / GhostLock 检查记录

检查日期：2026-10-10（Australia/Sydney）。

最新结论：用户第二次手动执行 GhostLock 已完成 W1/W2/W3，日志确认 uid=0 的 root 脚本成功运行一次。因执行当时缺少 ksud，KernelSU 内核模块未加载。2026-10-11 已补装官方 KernelSU v3.3.0 并确认 libksud.so 存在，模块加载尚未验证。一次执行成功不能证明长期稳定性；此前仍有内核 panic 记录。详细证据见 [second-attempt/result.md](second-attempt/result.md)。以下保留先前各阶段的检查记录。

## 设备和安装

- 设备：小米 MIX Fold 3，型号 2308CPXD0C，代号 babylon。
- SoC：高通 SM8550。
- 系统：Android 16 / OS3.0.304.0.WMVCNXM，安全补丁 2026-07-01。
- 内核：5.15.194-android13-8-00019-gf4321180a397-ab15212794。
- Bootloader：locked，Verified Boot：green，SELinux：Enforcing。
- 已安装 com.ghostlock.app，版本 1.2 / versionCode 566。
- APK 内没有与上述完整内核版本匹配的 profile。

## 镜像分析

通过 HTTP Range 从小米 OTA 下载地址按需提取 boot、xbl_config、uefi，并逐操作及逐分区校验 OTA manifest 中的 SHA256。OTA 系统指纹与设备一致。

- 提取器恢复了 159574 个 kallsyms 符号。
- 提取器判断 remove_waiter@0x177b0a0 仍使用 current，报告目标漏洞 primitive 未修复。这是静态工具判断，不是成功利用证明。
- pselect 布局自动推导失败，无法唯一确定 futex waiter 局部变量。
- 提取器建议 multicast_waiter 路线，置信程度 medium；waiter_off 自动推导为 0x60。
- 从 uefi 内存映射提取 kernel_phys_load = 0xa8000000。
- 已生成 profile-candidate.conf；提取器明确标记为未验证候选，仍有空字段及 missing: struct_slab_cache 的提示，不能视为已支持。
- 本轮没有导入配置到 App，也没有运行 libghostlock、加载 KernelSU、刷写分区或解锁。

完整工具输出见 analysis.txt 和 profile-extraction-log.txt。设备属性与内核配置位于 device-capture；校验过的镜像与 SHA256 记录位于 firmware-OS3.0.304.0.WMVCNXM。

## 同内核的已有反馈

[GhostLock Issue #316](https://github.com/YuKongA/ghostlock-app/issues/316) 是小米 13 / SM8550 的用户报告，完整 uname -r 与本机相同。报告称静态检查通过并生成配置，但实际运行出现 Write 1 failed，其他尝试导致死机重启。机型不同，不能直接认定 MIX Fold 3 结果相同；目前也不能把该报告当作成功方案。

下一步需要检查候选配置完整性和该内核 multicast 写入路径的适配问题。重新连接真机后才能验证实际提权结果。

## 后续尝试状态

用户授权继续尝试提权后，设备仍连接，系统已完成启动。向手机写入候选配置并打开 GhostLock 的工具调用被自动审批检查拒绝，仅返回 `blocked by policy`，未提供更具体原因；该调用没有执行。随后仅复核本地源码、候选配置与已有分析日志，没有运行提权载荷，也没有获得 root。

## 用户手动执行后的黑屏（2026-10-10）

用户报告点击 GhostLock 执行后黑屏，并进行了强制重启。重启后设备重新连接 ADB，sys.boot_completed=1，SELinux Enforcing。GhostLock 弹窗为“上次攻击未完成”，并提示可能发生内核 panic；该弹窗本身不能证明内核 panic，启动原因也仅记录为 reboot。

已备份共享存储的运行目录 20261010-233051：ghostlock-direct-0.log.txt 为 389 字节、profile.conf 为 2337 字节，两者全部为空字节，没有可读的提权阶段信息。/sys/fs/pstore 不允许普通 ADB shell 读取，未取得上次启动的内核崩溃日志。相关截图、运行目录和重启状态保存在 crash-after-reboot。没有成功提权证据，未继续触发新的尝试。

## 深入检查的新证据

后续查到小米电源诊断文件最新记录为 `kernel reboot : kpanic`，与本次重启时间吻合，支持确实发生内核 panic。核对源码及实际 APK 的提示分支后，残留阶段标记可缩小到 W1/W2，精确子步骤仍在普通 shell 无法读取的应用私有文件中。网上已找到开发分支的 multicast 稳定性改动和线程超时保护，但没有该 Fold 3 系统版本的成功验证。完整证据、来源与适用范围见 [deep-investigation/findings.md](deep-investigation/findings.md)。
