# 小米显示器 Redmi G Pro 27U（MiniLED）—— Root 调研（2026-11-06）

## 设备身份（重要更正记录）

2026-11-06 确认：**G Pro 27U MiniLED 版不是普通显示器，是「显示器 + 智能电视」二合一**，
内置澎湃 OS 2（Android 14 底），2025-04 发布
（[官网](https://www.mi.com/redmi-monitors/g-pro-27u)、[上手体验](https://zhuanlan.zhihu.com/p/1931302490537703097)）。
本仓库第十五台设备「小米电视」即它，二者为同一台。

另外注意：它在本仓库 Linux 测试线里也被当**纯显示器**用过（飞腾/龙芯/树莓派的显示输出），
那两条坑依然有效——`HANDOFF.md:1845`（同时接另一台开机电脑时疑似 HDMI 5V 倒灌导致
飞腾机 PCIe 训练失败）与 `HANDOFF.md:1897`（龙芯 MS7210 DVO 转 HDMI 与它不兼容，画面卷动 1/3）。

## 本仓库档案

`HANDOFF.md` 2026-10-08 第十五台设备：

- `MiTV-MFFU1`（代号 **beekeeper**），联发科 **MT9655**（平台名 mt5877），
  4 × Cortex-A73 @ 1800 MHz，**Mali-G57**（驱动 r44p1），**Android 14 / 澎湃 OS 2**，
  2.8 GiB 内存（可用 1.4–1.5 GiB），zram 1.5 GiB，界面 1920×1080。
- 网络调试：`adb connect 192.168.20.17:5555`（遥控器点允许）。
- 成绩：`out\adb\xiaomi-MiTV-MFFU1-20261008-163938\`（51 条）+ 2026-10-17 显存带宽复测
  （261.5 MiB，按可用内存收缩后的新上限；副本 `…-20261017-041143`）。
- **空闲时后台占约一个核（400% 里 121%：系统服务、语音唤醒、音频服务）**，
  CPU 成绩是带着这个负载测的。
- 2026-10-17 某次快速检查：缩放芯片中断很忙，连没改过的项目都比下午低 16–28%，
  那批作废（目录有 `NOTE.txt`）。

## Root 可行性（2026-11-06 调研）

**条件在机队电视/显示器类里最好，但没有现成移植：**

- ✅ **arm64**（无 TCL 那样的位数疑虑），公开 GhostLock/DirtyFrag payload 架构直接适用
- ✅ Android 14，内核大概率 5.15 → 在 GhostLock（<7.1）与 DirtyFrag（4.14+）范围内；
  2025 年设备 + 电视 OTA 懒 → 未修复概率高（待验证）
- ✅ 网络 ADB 现成，exploit APK 投递通道零成本
- ❌ [awesome-temporary-root](https://github.com/fynks/awesome-temporary-root) 无任何小米电视条目
- ⚠️ 电视非 GKI（`uname -r` 预计无 `-androidXX-` 后缀）→ 没有 Google 统一内核可白嫖，
  需要拿到固件包（小米电视 OTA 无公开下载，第三方渠道/抓包）提内核做偏移
- ⚠️ MT9655/mt5877 电视平台无移植先例
- ❌ BL 无解锁渠道（智能显示器不走手机解锁体系）

第一步（待验证，成本≈0）：

```
adb connect <IP>:5555
adb shell uname -r                                  # 预期 5.15.x 厂商内核
adb shell getprop ro.build.version.security_patch   # 早于 2026-08 = GhostLock 面开
```

## Root 的预期收益（全机队最大）

1. **冻结后台**：常驻 121%/400% 的系统服务/语音唤醒/音频服务是 CPU 成绩的最大干扰，
   root 后跑分前冻结，成绩不再「带着这个量」。
2. **排查/规避缩放芯片中断事件**：2026-10-17 那次 16–28% 偏低即此类；root 至少能在
   跑分窗口停掉多余显示处理路径，或事后归因。
3. Mali-G57 `devfreq` 锁频（写 min_freq 前先确认调速器认不认，见 `root/README.md` 小米平板教训）。

## 建议路线

1. 跑上面三条命令确认漏洞面（结果回填本文件）。
2. 查小米电视/MiTV OTA 包获取渠道（第三方仓库/升级抓包），拿到包 → 提内核 →
   按 [A17 移植方法论](https://www.mobilehackinglab.com/blog/cve-2026-43499-ghostlock-a17-root-shell)
   做偏移（有 BTF 则偏移校准轻松）。
3. 无 BTF/无固件则搁置，优先用无 root 手段：跑分时采频率（读 devfreq 不需要 root）+
   跑前 `top` 确认空载。
4. 成绩归档：root 会话另起组，`DEVICE-STATE.txt` 记录冻结了哪些服务、锁频参数。

## 与 TCL 那台的对比

同代 MTK 电视平台，但本台是 **64 位 + Android 14 + 内核更新 + 后台更重**——
同样的移植投入，本台收益显著更高。优先做这台。
