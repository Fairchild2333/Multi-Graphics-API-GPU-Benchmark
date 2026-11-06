# Meta Quest 2 / Quest 3 —— 临时 Root（2026-11-06 调研）

## 本仓库档案

- 两台均在机队（`HANDOFF.md` 2026-10-17 显存核对：Quest 2 / Quest 3 都是 128 MiB 堆）。
- HorizonOS（Android 底），开发者模式 + ADB 可开，exploit 直接 sideload，投递通道现成。

## 结论

**机队里唯一"工具现成、今天就干"的设备。** 全系四代都有公开漏洞链，全部为临时 root
（重启失效；Quest 1 在 2021-07 前老固件上另有 fastboot 漏洞可解 BL，不在机队）。

## 漏洞与修复线

| 路线 | 适用 | 修复线（安全补丁） | 说明 |
|---|---|---|---|
| Adreno GPU（CVE-2025-21479） | Quest 3 / 3S | 2025-08-05 | GPU 覆写自身页表 |
| GhostLock（CVE-2026-43499） | Quest 1/2/Pro/3/3S 全系 | 2026-08-01 | 内核 futex PI UAF |
| DirtyFrag（CVE-2026-43284 + 43500） | Quest 2/3/3S/Pro | 见下表 build 上限 | **确定性，无竞态，优先走这条** |

DirtyFrag 最高支持固件 build（来自 [FreeXR/exploits](https://github.com/FreeXR/exploits)）：

| 设备 | 最高支持 build |
|---|---|
| Quest 3 | 52433670036000520 |
| Quest 3S | 3814840024700610 |
| Quest 2 | 52242990024200150 |
| Quest Pro | 51503870024400340 |

## 工具

- **Singularity**（FreeXR 系，一体化 App，打包 GhostLock/DirtyFrag 多条链 + safe-root-things）：
  开发者模式 → sideload → 运行。Quest Pro 上实测覆盖 HorizonOS v2.1–v2.7。
- 基于 fuguquest 的 Lumince/singularity 分支。
- 原 PoC：[zhuowei/cheese](https://github.com/zhuowei/cheese)（CVE-2025-21479，Quest 3）。

## 判断当前固件能不能打

```
adb shell getprop ro.build.version.security_patch
```

- 早于 2025-08-05 → 三条路全开
- 早于 2026-08-01 → GhostLock + DirtyFrag 可用
- 最新固件（当前 HorizonOS v2.9 带安全加固）→ 已全补，需先降级

降级路线：[QuestStack](https://github.com/starseed12345/QuestStack)（临时 root → 备份 inactive slot →
推送低版本固件），固件包从 [Quest 固件归档](https://ptrpa.ws/quest-firmware-archive) 取。

## 风险与纪律（重要）

- **变砖不可修**：Quest BL 锁死、无高通 9008 救砖通道——降级/写分区操作前必须备份，
  zhuowei 在 cheese 仓库明确警告过。
- 拿到临时 root 的第一件事：**关自动更新**（一次 OTA 就可能把漏洞补掉）。
- soft restart（软重启）代替完整重启可保留 root 状态。
- root 会话期间不要登录支付类账号；Beat Saber mod 等 LSPosed 场景有成熟隐藏方案。
- Quest 2 优势：已停止大版本更新，停留在修复线之前的存量固件多；Quest 3 联网自动更新大概率已补。

## 与跑分的配合

提权 → devfreq 锁 GPU/CPU 频率 → 跑专项系列 → 重启复原。每次开机周期需重新提权，
适合「锁频专项测试」；日常跑分按设备原生状态记录并采样实测频率。
Quest 塑料壳无风扇，**散热（外部风扇）对成绩稳定性收益可能大于锁频**。
