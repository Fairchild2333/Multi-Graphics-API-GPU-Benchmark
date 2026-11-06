# 其余设备 Root 调研汇总（2026-11-06）

机队内外讨论过的其他设备，均无现成 root 方案，按可行性从高到低记录。

## Chromecast with Google TV（sabrina）—— 有现成移植 ✅

- **ghostlock-sabrina**：<https://github.com/k-o-n-t-o-r/ghostlock-sabrina>（GhostLock 专用移植）。
- 2020 年设备（Amlogic S905X3，内核 4.9），Android 12（2023）后基本 EOL → 2026 补丁必无，理论必中。
- ADB 设置里连点版本号即开（网络 ADB），exploit 直接推。
- 另可解 BL 刷 LineageOS（社区有移植）——想要永久 root 也有路。
- 内核 4.9 < 4.14，**DirtyFrag 不适用**，只有 GhostLock 一条。

## Google TV Streamer（2024）—— 放弃 ❌

- MT8696、内核新、仍在维护期，软件路线基本封死。
- 唯一公开 root 是 raelize 的电磁故障注入研究（开盖硬件级）：
  <https://raelize.com/blog/setresuid-glitching-google-tv-streamer-from-adb-to-root>——不可复现，不投入。

## 华为 P10 Plus（Kirin 960，~2017）—— 仅付费解锁码 ⚠️

- 官方解锁码 2018-05 停发；唯一现实路线：DC-Unlocker 等第三方按 IMEI 算码
  （<https://www.dc-unlocker.com>，先查 VKY 型号当前支持状态再付钱）→ Magisk 永久 root。
- 临时 root 无戏：内核 ~4.1 在 GhostLock 理论范围（2.6.39+）但麒麟零移植、
  老内核无 BTF、EMUI fork 差异大，纯研究项目；DirtyFrag（4.14+）不覆盖 4.1。

## 华为 Mate 40 Pro（Kirin 9000，内核 4.14）—— 无解 ❌

- 纸面在 GhostLock + DirtyFrag 双范围，固件也能下（Firmware Finder/Huawei Update Extractor），
  但华为至今给旗舰推月度补丁，且麒麟全线无公开移植（awesome-temporary-root 无麒麟条目）。
- 剩维修行业付费底层 BL 写入（东海论坛类，需 USB 1.0 模式配合）或拆字库，不自行投入。

## 华为 Mate 60 Pro（Kirin 9000S，内核 5.10）—— 无解 ❌

- 鸿蒙 4.2（安卓兼容层）：理论在 GhostLock 范围，无公开工具，华为补丁勤。
- **已升 HarmonyOS NEXT：无安卓运行时，APK 提权工具根本跑不起来**——死路。
- 留可能性就别升 NEXT；但当前也没有可用 exploit，纯等待。

## 小米 15S Pro（玄戒 O1，澎湃 OS 4 beta）—— 无解 ❌

- 官方：玄戒机型（15S Pro、平板 7 Ultra）**从发布起就不在小米 BL 解锁支持名单**；
  且 2026 年小米官方通道本身近乎关闭（答题渠道 2026-01 删除、每年限 1 台、社区 5 级）。
- 社区：唯一成果是 B 站「宵宵狸猫」的**焚决**方案（2026-08，临时 root + Zygisk/LSPosed），
  **只支持到 OS3，澎湃 4 玩不了**，作者已表示不做后续（O3 也不会有）。
- 玄戒 O3 首销日的「越狱工具」视频已核实为引流骗局，勿付费。
- 若刷机包将来泄漏：只用来**解包做偏移**（读），绝不能刷——玄戒无救砖生态。
  前提链：内核未修 GhostLock（先跑 `uname -r`，玄戒 OS4 内核基线可能比骁龙版新、已带修复）
  → 栈布局可控 → `kernel_phys_load` 可从泄漏包设备树推 → 无未知澎湃缓解。每环都可能断。
- 对照：小米 15（骁龙）澎湃 4 beta 内核仍 6.6.118（低于 6.6.140），GhostLock 有公开成功演示
  ——同系统不同机型内核不同步，玄戒不能套用骁龙结论。

## 已 root 机队现存设备（对照）

- 小米平板 1（Tegra K1）：Magisk su 可用（`HANDOFF.md` 2026-10-08 记录）。
  已验证教训：`devfreq/min_freq` 该调速器不认、温度墙绕不过。
- Switch（Switchroot LineageOS 22.2 + hekate 6.5.4）：引导层超频（`bootloader/ini/android.ini`），
  不需要 root。hekate ini 段内空行会静默丢键。
