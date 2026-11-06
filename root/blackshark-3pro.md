# 黑鲨 3 Pro —— Root 方案（2026-11-06 调研）

## 本仓库档案

- 机队实测记录见 `HANDOFF.md`（2026-10-17 显存带宽核对：256 MiB 堆，256 MiB 起完整遍历受限）。
- 芯片骁龙 865（SM8250），内核 4.19，出厂 JoyUI（Android 10 底）。
- 国行型号 **MBU-A0**，全球版 **MBU-H0**（刷机文件按版本区分，见下）。

## 结论

**优先走免费解 BL → Magisk 永久 root。** 官方解锁渠道随黑鲨品牌终止（2022 年腾讯收编后停摆），
但 XDA 有社区免费解锁文件（John Eveline 发布），配套完整 root 教程（KM7 发布）。
临时 root 无现成移植，也没必要——能永久解锁就没理由玩临时的。

## 路线：免费解锁 → TWRP → Magisk

**解锁文件帖**：<https://xdaforums.com/t/blackshark-3-3-pro-3s-unlock-bootloader-files.4140001/>
（2020-07 发布，2026-11-06 验证仍可访问；MEGA 网盘：解锁文件、TWRP、全球版 ROM）

**完整教程帖**：<https://xdaforums.com/t/rooting-black-shark-3-3-pro-the-correct-easy-way-bl-unlocking-twrp-ota-fix.4147441/>

流程概要：

1. 开发者选项 → OEM 解锁 + USB 调试；PC 装驱动（管理员运行安装器）
2. `adb reboot edl` 进 EDL（黑屏正常，设备管理器确认 Qualcomm HS-USB QDLoader 9008）
3. MiFlash 选解锁文件目录刷入
4. `fastboot flashing unlock` → `fastboot erase userdata`（**清数据**）→
   `fastboot getvar current-slot`（记下槽位 a/b）
5. `fastboot boot twrp.img` 临时引导 TWRP，`adb pull` 拉出 `boot_a` / `boot_b`
6. Magisk「Select and Patch a File」修补对应槽位 stock boot
7. `fastboot flash boot_x magisk_patched.img` → 重启，Magisk 两个绿勾即成

## 注意事项

- **版本区分**：XDA 文件面向全球版 MBU-H0；国行 MBU-A0 不要直接刷 H0 文件，
  用国行按高低配区分的解锁文件（如 ONFIX：<https://onfix.cn/rom/566232>，含救砖线刷包）。
- **MiFlash 识别不到设备是最高频的坑**：换原装数据线，或换老版 MiFlash（20170425）。
- 解锁后指纹可能失效：拨号 `*#*#466349#*#*` → 售后校准（手指 + 黑色物体各一次全遮指纹模组）。
- 解锁**清全盘数据**：动手前确认 `out\adb\` 成绩副本已在 PC。
- root 后 OTA 会失效：教程帖附 OTA 修复（update.zip 放 ota 目录，拨 `*#*#1027#*#*` 本地升级）。

## 临时 root（仅备查，不推荐投入）

内核 4.19 在 GhostLock（2.6.39+）和 DirtyFrag（4.14+）范围内，品牌死后无补丁，
漏洞面必然敞开——但社区无黑鲨移植，公开 payload 也无人适配 4.19/JoyUI 内核。
能解锁的前提下没有投入价值。

## 第三方系统现状（2026-11-06）

无官方 LineageOS（整个黑鲨家族只有初代 SKW 有）；黑鲨从不放内核源码，深度移植无人做。
无澎湃/HyperOS 移植包。想要高版本 Android 只有 GSI（Treble）实验路线，相机/肩键/指纹大概率残废。
**对跑分而言原系统 + Magisk 就是最干净形态。**

## 建议的最终形态

永久 root 后装开机脚本：设定 `devfreq` min_freq（先确认调速器认不认，见 `root/README.md`
小米平板教训）、跑分前 `top` 确认空载。成绩按 root 状态另起组并记录 `DEVICE-STATE.txt`。
