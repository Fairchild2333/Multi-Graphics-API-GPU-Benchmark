# MIX Fold 3 第二次手动执行结果

执行日志目录：`20261010-234704`。用户手动执行 GhostLock，本轮助手仅读取并备份结果。

## 已确认

- GhostLock 1.2+566；CPU 主线程 3 / consumer 4；Direct 模式。
- W1 和 scratch repair 完成；设备当前 SELinux 为 Permissive。
- W2 完成，日志明确出现 `child uid = 0` 和 `child is root!`。
- W3 完成：`child seccomp fully bypassed`。
- `exploit complete`，root script 记录 `uid=0 euid=0`，native exit code=0。
- 随后 `ksud missing; cannot late-load`，最终 `temporary root ready; KernelSU module load pending`。因此取得过临时 root，但没有完成 KernelSU 内核模块加载。
- 执行后检查：root shell worker pid 10009 已为 zombie，Uid 仍为 0；父子进程 pid 25233 的 Uid 已回到 10387、Seccomp=0。ADB shell 仍是 uid=2000。不能据此声称当前存在可用的常驻 root shell。

## 管理器补装（2026-10-11）

官方下载 [KernelSU v3.3.0](https://github.com/tiann/KernelSU/releases/tag/v3.3.0) 的 `KernelSU_v3.3.0_32601-release.apk`。

- SHA256：`c197060ecb89702e7d54a4c95e29cf5e8d97369bbbb436979ab7fd6bcde7b077`，与 GitHub release asset digest 一致。
- 包名 `me.weishu.kernelsu`，versionCode 32601。
- 首次 USB 安装被手机端取消；用户明确表示没来得及确认并要求重试，第二次 adb install 返回 Success。
- 已验证安装路径下 `lib/arm64/libksud.so` 存在（4892712 字节）；GhostLock 源码会从该管理器的 nativeLibraryDir 复制它。
- 已打开管理器，界面版本 v3.3.0，状态“未安装”，表示 KernelSU 内核模块仍未加载。`/proc/modules` 未见 kernelsu。
- 官方同版本 release 提供 aarch64 android13-5.15 模块，但本机装载尚未验证，不能仅凭 KMI 系列匹配断言能成功加载。

本轮没有刷写启动镜像、解锁 bootloader 或再次触发漏洞。保存了成功运行日志、有效 profile.conf/profile.bin、kernel-dmesg.log、iomem.txt、ksu.log 及管理器截图。
