# 测试机队 Root 方案调研（2026-11-06）

机队设备多数 **BL 不可解锁**，调研临时 root（内核漏洞提权，纯内存、重启失效）
的目的是**跑分质量控制**：锁 GPU/CPU 频率、冻结后台服务、消除系统负载干扰。
永久 root（解 BL + Magisk/KernelSU）仍是测试机的最优形态，凡有免费解锁路线的优先走解锁。

## 为什么要 root：本仓库已有的两条记录

- `HANDOFF.md` 2026-10-08 小米平板（Tegra K1）：Magisk su 写 `devfreq/min_freq`
  调速器不认、关 `enable_3d_scaling` 也绕不过功耗/温度墙，GPU 稳在 750 MHz
  由散热决定。**教训：root 是锁频的必要条件，不是充分条件；降温同样重要。**
- 同日 TCL 电视记录：空闲时自有服务占 400% 里约 40–121%（电视/显示器类设备通病），
  **冻结后台是这类设备 root 的最大收益**，比锁频更直接。

## 通用检查（每台都先跑这三条）

```
adb shell uname -r                                  # 内核版本；带 -androidXX- 后缀 = GKI 内核
adb shell getprop ro.build.version.security_patch   # 安全补丁级别
adb shell uname -m                                  # 内核位数（32 位电视专用：aarch64 / armv7l / armv8l）
```

## 漏洞速查（2026-11-06 时点）

| 漏洞 | CVE | 受影响内核 | 修复线（安全补丁日期） | 特点 |
|---|---|---|---|---|
| GhostLock | CVE-2026-43499 | 2.6.39 – 7.1 之前（futex PI UAF） | 5.10.261 / 6.1.175 / 6.6.140 / 6.12.86（2026-07-24 进 stable；提交 3bfdc63936dd 已在 kernel.org 各分支 ChangeLog 逐条核对；随后的 2026-08 Android 公报纳入，具体分段未逐条核对） | 竞态型，需逐内核构建做偏移表 |
| DirtyFrag | CVE-2026-43284（+CVE-2026-43500） | 4.14+（xfrm-ESP / rxrpc 页缓存写） | 2026-05-07 披露，2026-05 补丁线 | 确定性逻辑漏洞，不赌竞态、失败不 panic |
| Adreno GPU | CVE-2025-21479 | 高通 Adreno 7xx 驱动 | 2025-08-05 | GPU 覆写自身页表，不依赖内核结构偏移 |

公开 payload 均为 **arm64**；32 位内核设备需从零移植 AArch32 版（无先例）。

## 设备总表

| 设备 | 临时 root | 永久 root | 推荐路线 | 文档 |
|---|---|---|---|---|
| 小米 MIX Fold 3（babylon，SM8550/5.15 GKI） | ✅ **已成功**（2026-10-10 GhostLock，W1–W3 全过、uid=0 脚本已运行） | ❌ BL 锁定 | 固件停在 OS3.0.304（补丁 2026-07-01，早于修复线）勿升级 OTA；待验证 KernelSU 模块加载 | [Babylon-root-analysis/README.md](Babylon-root-analysis/README.md) |
| 黑鲨 3 Pro（MBU，SD865/4.19） | 无移植（不必要） | ✅ 免费解锁文件 | XDA 文件解 BL → TWRP → Magisk | [blackshark-3pro.md](blackshark-3pro.md) |
| 黑鲨 5 Pro（KTUS-A0，SD8G1/5.10） | 无移植，GKI 自适配可行性高 | ⚠️ 仅付费 | 先查 GKI 后缀；临时 root 锁频 | [blackshark-5pro.md](blackshark-5pro.md) |
| Quest 2 / Quest 3 | ✅ 现成工具 | ❌ | Singularity（sideload） | [quest-2-quest-3.md](quest-2-quest-3.md) |
| Chromecast with Google TV（sabrina） | ✅ 现成移植 | ✅ 可解 BL | ghostlock-sabrina | [other-devices.md](other-devices.md) |
| 小米显示器 G Pro 27U（MiTV-MFFU1） | 无移植，条件好 | ❌ | 先跑 uname -r；root 收益全机队最大 | [redmi-g-pro-27u.md](redmi-g-pro-27u.md) |
| TCL 电视（MT5889，32 位系统） | 待 uname -m 定生死 | ❌ | 64 位内核=可尝试；32 位内核=放弃 | [tcl-smart-tv-pro.md](tcl-smart-tv-pro.md) |
| 华为 P10 Plus / Mate 40 Pro / Mate 60 Pro | ❌ 麒麟零移植 | P10P 仅付费码 | 基本无解 | [other-devices.md](other-devices.md) |
| 小米 15S Pro（玄戒 O1，澎湃4 beta） | ❌（焚决只到 OS3） | ❌ 官方从不支持玄戒 | 无解；勿升 NEXT | [other-devices.md](other-devices.md) |
| Google TV Streamer（2024） | ❌ 已补丁 | ❌ | 放弃 | [other-devices.md](other-devices.md) |

## 不 root 也能做的（优先做）

1. **读频率不需要 root**：多数设备 `devfreq/*/cur_freq`、`gpuclk` 节点全局可读，只有写才要 root。
   跑分时每 2 秒采样频率区间、随成绩归档（PC 上 `clocks-every-2s.txt` 的做法可标准化进引擎），
   跨设备对比按"实测频率"标注，比部分设备人为锁频更公平。
2. **降温比锁频便宜**：频率不稳的根源多是温度墙（见小米平板记录），风扇/散热架 + 固定室温 +
   预热到稳态再跑正式项，能把 CV 压下来而不碰系统。
3. **先看 `top` 再跑分**：电视/显示器类后台负载大（TCL 40%、MiTV 121%/400%），
   跑正式系列前确认空载状态。

## Root 状态与成绩归档

- 解 BL / 刷第三方系统 / root 会话期间的成绩：**另起结果组**，`DEVICE-STATE.txt`
  必须记录 root 状态、锁频参数（写了哪些节点、生效与否）、系统版本。
- 临时 root 的锁频只在当次开机周期有效，重启后需重新提权→锁频→跑分，适合"锁频专项测试"
  而不是日常跑分。

## 资源

- [awesome-temporary-root](https://github.com/fynks/awesome-temporary-root) —— 锁 BL 临时 root 工具/机型总表
- [lokey0905/rootWithoutUnlockBootloaderList](https://github.com/lokey0905/rootWithoutUnlockBootloaderList) —— 免解锁 BL 漏洞追踪
- [FreeXR/exploits](https://github.com/FreeXR/exploits) —— Quest 全系漏洞与工具
- [2026 全品牌 BL 解锁状态](https://github.com/AdaUnlocked/2026-All-Brands-Bootloader-Unlock-Status)
- 漏洞分析：[CloudLinux GhostLock](https://blog.cloudlinux.com/ghostlock-cve-2026-43499-local-root-exploit-kernel-update-for-cloudlinux) ·
  [Wiz DirtyFrag](https://www.wiz.io/blog/dirty-frag-linux-kernel-local-privilege-escalation-via-esp-and-rxrpc) ·
  [Mobile Hacking Lab：A17 GhostLock 移植笔记](https://www.mobilehackinglab.com/blog/cve-2026-43499-ghostlock-a17-root-shell)（自适配方法论）

> 本文件夹内容为 2026-11-06 网络调研 + 本仓库测试记录的汇总，标注"待验证"的项目
> 以实机 `uname` / `getprop` 输出为准。各文档含外部链接，链接失效属正常（刷机社区资源寿命短）。
