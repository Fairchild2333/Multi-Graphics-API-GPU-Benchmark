# 黑鲨 5 Pro —— Root 方案（2026-11-06 调研）

## 本仓库档案

- 机队实测记录见 `HANDOFF.md`（2026-10-17：128 MiB 堆；成绩副本 `out\adb\blackshark-SHARK-KTUS-A0-20261017-035416`）。
- 国行型号 **KTUS-A0**，骁龙 8 Gen 1（SM8450），内核 5.10，JoyUI 13（Android 12 底）。
- 品牌 2022 年后停摆：**无 OTA、无安全补丁 → 内核漏洞面永久敞开**（这对临时 root 是利好）。

## 结论

**BL 解锁没有免费现成方案；临时 root（GhostLock 自适配）是免费路线里可行性最高的一条。**

### 为什么 3 Pro 有免费解锁文件、5 Pro 没有

3 Pro（SM8250 时代）的解锁文件走 **EDL（9008）直接刷解锁分区**——当年高通 firehose
加载器未强制签名授权。**SM8450 起高通 EDL 强制签名**（firehose 需过设备认证），
同样的套路无法复刻。这是技术分水岭，不是社区意愿问题。

### BL 解锁现状分级

| 路线 | 状态 |
|---|---|
| 官方/小米系解锁工具 | ❌ 死——黑鲨 4/5 代解锁需官方按 IMEI 开白名单，黑鲨倒后授权端消失 |
| EDL 刷解锁文件（3 Pro 套路） | ❌ 高通 SM8450 起签名墙，不可复刻 |
| 9008 免授权刷机 | ⚠️ 研究级：见 [2026 全品牌状态汇总](https://github.com/AdaUnlocked/2026-All-Brands-Bootloader-Unlock-Status)（EDL 能救砖 ≠ 能解 BL，解 BL 需引导链底层漏洞配合）；KTUS-A0 是否覆盖待查 |
| 付费远程解锁 | 有（B 站/淘宝，几十到几百元），非免费不在推荐内 |
| EDL 救砖包 | ✅ [FiimeROM 有 KTUS 线刷包](https://mi.fiime.cn/enginerom/62.html)（救砖用，非解锁）——刷机试错的兜底 |

注意：网上「黑鲨 2/3Pro/4Pro/5Pro 强开 BL 解小米账户锁」类教程（如
<https://zhuanlan.zhihu.com/p/612402553>）是**解账户锁**（拆字库），账户锁 ≠ BL 锁，别混。

## 临时 root：GKI 自适配路线（推荐投入方向）

内核 5.10 + 品牌死后零补丁 = **GhostLock（修复线 5.10.261）和 DirtyFrag（4.14+）双双中招**。

第一步（待验证）：

```
adb shell uname -r
```

- 带 `-android12-5.10.xxx-...` 后缀 = **GKI 内核**：boot.img 里的内核就是 Google 统一构建，
  无需黑鲨固件，直接从 Google GKI 发布渠道取**同构建号**内核做偏移表，自适配可行性全黑鲨最高。
- 无后缀 = 厂商内核：需要先搞到固件包（第三方 ROM 站/泄露包）提内核，难度上一档。

参考方法论：[ghostlock-app 的 SUPPORTED/提取流程](https://github.com/YuKongA/ghostlock-app)、
[A17 移植笔记](https://www.mobilehackinglab.com/blog/cve-2026-43499-ghostlock-a17-root-shell)
（BTF 校偏移 → QEMU 起真内核调 fatal bug → 逐个闯厂商缓解）。
已知结构性风险：GhostLock 要求 waiter 落在 pselect6 可控栈区（"waiter word + 11 ≤ 14"），
由编译器栈布局决定——**存在内核在漏洞范围内但布局不可利用的可能**（OPPO 6.1 GKI 机型先例）。

DirtyFrag 是备选：确定性逻辑漏洞不赌竞态，同样需要偏移。

## 第三方系统现状（2026-11-06）

- 无官方 LineageOS（[Reddit 上有人问过无人接盘](https://www.reddit.com/r/LineageOS/comments/1b9k1eg/blackshark_5_pro)）；
  根因是黑鲨从不放内核源码。
- XDA 黑鲨 5 Pro 版块有**基于 Android 13 的移植 ROM（一加底）**，英文社区唯一像样的第三方系统。
- 无澎湃/HyperOS 移植。GSI 可试（Treble 兼容），相机/肩键大概率残废。
- **刷第三方系统对跑分无收益**：多数移植包沿用黑鲨原 5.10 内核，GPU 驱动不变；
  且系统大版本变化会导致成绩不可比（换系统 = 另起结果组）。

## 建议路线

1. `uname -r` 确认 GKI 后缀（一块钱成本决定后面值不值得投入）
2. GKI 成立 → 做 GhostLock/DirtyFrag 偏移自适配（临时 root：提权 → devfreq 锁频 → 跑分 → 重启复原）
3. 期间 FiimeROM 的 EDL 包备着救砖；exploit 触发 panic 重启无损失（纯内存）
4. 成绩归档：root 会话期间另起组，记录锁频参数与实测频率
