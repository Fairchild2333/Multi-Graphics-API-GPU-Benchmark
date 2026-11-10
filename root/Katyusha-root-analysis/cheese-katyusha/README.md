# cheese-katyusha —— 黑鲨 5 Pro 临时 root（CVE-2025-21479，已实机成功）

2026-11-10 实机验证：UID 0 + SELinux Permissive，无 panic，设备健康。

## 这是什么

基于 [zhuowei/cheese](https://github.com/zhuowei/cheese)（Quest 3 原版）+
[sarabpal-dev/cheese-cake](https://github.com/sarabpal-dev/cheese-cake)（一加 10 Pro 移植）
的 katyusha（KTUS-A0 / SM8450 / 5.10.101-android12-9）适配版。

漏洞：CVE-2025-21479 —— Adreno A7xx GPU 微码 IB 级校验掩码错误，
非特权 `CP_SMMU_TABLE_UPDATE` → GPU 视角任意物理读写 → KSMA 脏页表 →
kallsyms 扫描 → `__do_sys_capset` 贴 shellcode（清 seccomp + `commit_creds(init_cred)`）
→ `capset()` 一调即 root → SELinux enforcing 字节清零。

## 构建

```
NDK clang -O2 -o cheesecake exploit.c
# 本机：aarch64-linux-android35-clang.cmd（NDK 28.2.13676358）
```

## 运行

```
adb push cheesecake-arm64 /data/local/tmp/cheesecake
adb shell "chmod 755 /data/local/tmp/cheesecake && /data/local/tmp/cheesecake"
# 需要亮屏；失败重跑即可（喷射落点有随机性，成功后 exec root sh）
```

效果：SELinux 翻 Permissive（**重启前持续有效**），exploit 进程本身 uid 0；
`cheesecake sh` 之类可跟命令。重启后需重跑（纯内存改动，无持久化）。

## katyusha 适配点（相对 cheese-cake）

| 项 | 值 | 来源 |
|---|---|---|
| init_task 增量（从 idmap） | `+0x31CE00` | symbols.json（替代一加的 0x321F80） |
| IDMAP_PG_DIR_OFFSET | `0x268f000` | 同上（自动探测其实也能命中 i=4） |
| DEFAULT_KERNEL_SIZE_MB | 46 | Image 头 image_size 0x2c90000 |
| selinux_state（从 idmap） | `+0x5c2b20` | 0x2c51b20-0x268f000 |
| __do_sys_capset（从 _text） | `0x13e9a0` | symbols.json |
| task_struct tasks/pid/seccomp | 0x4c8 / 0x5c8 / 0x848 | 与移植版一致，未改 |

## 关键修复（本仓库会话 11/12 系列）

1. **EDEADLK 自愈**：kgsl 提交遇 `EDEADLK`（上下文死锁检测，批读后 ctx 不 retire
   所致）→ `kgsl_ctx_recreate()` 销毁重建（flags=0x1812 本机唯一合法）→ 重试。
2. **DoWrite ctx_id 改指针传参**（`uint32_t*`）：否则重建只更新调用者，DoWrite 仍
   提交给死 ctx → EINVAL 刷屏（cc6 教训）。
3. ReadPhys64 整体 5 次重试 + 全失败路径带 errno/物理地址打印（原版静默 return）。
4. 诊断基建：O_SYNC stderr（cheese 原版侧）、syncer 保日志、md5+sync 推送纪律。

## 取证日志

- `cc7-root-success.log` —— 完整成功链路（enforcing 1→0、UID 0、代码恢复）
- `cc6-ctxrecreate-bug.log` —— ctx 重建生效但 ctx_id 按值传参 bug 现场

## 致谢

- Ben Hawkes（Project Zero Adrenaline）、Man Yue Mo（adreno_user）、zhuowei（cheese）、
  sarabpal-dev（cheese-cake 移植）、FreeXR 指南、xploitbengineer 与京东 DawnLab 分析。
