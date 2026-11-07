# 机队 GApps 部署手册（2026-11-07，四台实机验证）

按设备形态分三条路线，全部实测。先读"路线选择"，再对照各设备案例。

## 路线选择

| 设备形态 | 路线 | 参考 |
|---|---|---|
| 第三方 ROM（LineageOS 等）+ TWRP | **OpenGApps 官方 zip 经 TWRP 直刷**（写 system，最正规） | 小米平板 2 |
| 原厂系统 + Magisk root + 完整模块机制 | **手工组装 Magisk 模块**（systemless，可逆） | 小米 5c |
| 原厂系统 + Magisk root，但 ROM 对外来 priv-app 敏感（MIUI 底） | ⚠️ 同上但**需摘除冲突组件**，且可能卡开机——失败即回滚 | 小米 3（失败案例） |
| 无 root + BL 锁定 | 装不了。等临时 root（纯内存，装的用户 APK 可持久）或解锁 | 黑鲨 5 Pro |

## OpenGApps 包源（SourceForge，按架构/安卓版本分目录）

- 路径模式：`sourceforge.net/projects/opengapps/files/<arch>/<API level 日期目录>/`
- 实测可用：`arm/20220215`（4.4 最后存档）、`x86/20220503`（6.0）、`arm64/20220503`（11 也可用）
- ⚠️ 压缩格式 **.tar.lz（LZIP）**：Windows 工具链几乎全不认。**Windows 自带的
  `C:\Windows\System32\tar.exe`（libarchive bsdtar）原生支持**，Python lzma 手工拼头反而不行。
- ⚠️ 包内组件按 **common / all / nodpi / <屏幕密度>** 分目录，组装模块时按设备密度取件
  （密度查 `getprop ro.sf.lcd_density`）。

## 手工组装 Magisk 模块（无 root 依赖安装器的标准做法）

1. PC 上解 OpenGApps zip → `Core/*.tar.lz` → bsdtar 逐个解出 `system/` 树
2. 按密度合并成单一 `system/` 树（common/all/nodpi + 设备密度目录）
3. 写 `module.prop`（id/name/version…）
4. `tar -cf` 打包 → adb push → 手机上 root 解到 `/data/adb/modules_update/<id>/`
   （**暂存目录**，重启时 Magisk 自动移入 `/data/adb/modules/`）
5. 重启 → 验证 `pm list packages | grep vending/gms/gsf`

需要保留的核心组件（Android 7.1 pico 实测）：gmscore、gsfcore、gsflogin、vending、
defaultetc-common、defaultframework-common、extservicesgoogle、extsharedgoogle、
googlecontactssync、calsync、googlefeedback、googlepartnersetup、googleonetimeinitializer、configupdater。

## ⚠️ 高危组件（MIUI/JoyUI 底的机器必查）

- **SetupWizard / GoogleSetupWizard / PixelSetupWizard**：替换系统开机向导 → 首次开机流程崩溃
- **Provision / ManagedProvisioning**（.replace 去激活系统版）→ 老MIUI 对陌生 priv-app 敏感
- **AndroidAuto（AndroidAutoStub）**：特权权限不在原厂 privapp 白名单 →
  `ro.control_privapp_permissions=enforce` 的机器 **system_server 每 9 秒软循环**（黑鲨 3 Pro 实测，
  dropbox 76 份报告全点名它）
- **拨号框架（dialerframework）+ 谷歌联系人/日历替换件**：MIUI 底慎装
- 诊断口诀：**system_server 崩溃先查 `/data/system/dropbox/system_server_crash*`，
  别看 tombstones**（tombstones 里 init 子进程的 setexeccon 报错是每开机都有的噪音，
  poweroffm64/qti-testscripts 之类不影响开机）

## 设备案例

### 小米平板 2（latte，x86，LineageOS 13 / Android 6.0）✅ 最顺

TWRP（已有）→ OpenGApps `x86/20220503/pico` → `twrp install /sdcard/gapps.zip` →
重启即得。注意事项：TWRP 的 `twrp install` **跑一次就够**，连跑第二次会因 /system
已被改写而报库错误（无害，但别误判为失败）。OpenGApps 官方安装器写 system 是
LineageOS 的设计场景，比任何 systemless 方案都稳。

### 小米 5c（song，MIUI 11 / Android 7.1.2，Magisk 30.6）✅ 手工模块

LiteGapps 安装器在此机**静默假安装**（日志全绿、模块目录 8KB 空壳，两轮复现）——
放弃安装器，OpenGApps `arm64/20220503` 无 7.1 包（只有 .lz 格式的老组件可解），
用 bsdtar 解包后手工组装（gmscore/gsfcore/gsflogin/vending + common），
root 直接铺 `/data/adb/modules/`。密度 480。重启后 GMS/Play 全活。

### 黑鲨 3 Pro（mobius，JoyUI 11 / Android 11，Magisk 31.1）✅ 配置精简装

LiteGapps nano 全量装 = system_server 崩溃循环（AndroidAuto 白名单，见上）。
正确做法：`/sdcard/litegapps.config` 精简（28 件 false，只留 Common/GoogleExtShared）+
**命令行刷**（`su -c magisk --install-module ...`；Magisk App 图形界面会静默失败）。
详见 blackshark-3pro.md。

### 小米 3（pisces，MIUI 7 / Android 4.4.4，Magisk 22.1）❌ 模块卡开机，已回滚

OpenGApps 4.4 pico 手工模块（同 5c 流程，Magisk 22.1 机制兼容）→ 开机卡动画死循环。
无 TWRP（pisces 不在官方支持列表，社区版分 Tegra4/骁龙800 两个变体需先定硬件）、
无 dropbox 诊断（4.4 太老）。救援实录：
1. 卡死状态下 **adbd 和 su 仍然活着**（magiskd 在 post-fs 阶段就活了）
2. fastboot 模式需先修驱动：Zadig 绑 WinUSB（旧 Google 驱动 oem62.inf 是坏的）
3. 标准安卓 fastboot（VID_18D1&PID_D00D）在此机只有 Zadig+Python 客户端可用，
   MiFlash2022 的 fastboot.exe 看不见
4. `fastboot reboot` 后抢 adbd 上线（本机 63 秒），root 立删模块 → 90 秒干净开机
- 结论：**4.4/MIUI 7 上 systemless GApps 不可行**（无 TWRP 兜底 + 模块必卡）。
  该机保持无 GApps；跑分应用 sideload。

## 通用教训

1. **Magisk App 图形界面刷 LiteGapps 会静默失败**（两个机型复现）——一律命令行
   `su -c magisk --install-module <zip>`，输出逐项 "Installing ..." 才算数
2. **火绒/Defender 会吞解压出来的 ARM ELF**（tar-arm 三次消失）——加信任区，或
   base64 编码推送后手机上解码
3. **卡开机 ≠ 无路可走**：只要 adbd 起来且 root 活着（Magisk 的 post-fs 很早），
   卡死状态下就能禁用模块自救；fastboot + Zadig WinUSB 是最后的门
4. 装完必验三件套：`pm list packages | grep -E 'vending|gms|gsf'`
5. 成绩纪律不变：装 GApps 的设备成绩另起结果组

## 机队 GApps 现状（2026-11-07）

| 设备 | 系统 | GApps | 方式 |
|---|---|---|---|
| 黑鲨 3 Pro | JoyUI 11 / A11 | ✅ | LiteGapps 精简模块（config 跳 28 件） |
| 小米平板 2 | LOS 13 / A6 | ✅ | OpenGApps pico，TWRP 直刷 |
| 小米 5c | MIUI 11 / A7.1 | ✅ | OpenGApps 手工模块 |
| 小米 3 | MIUI 7 / A4.4 | ❌ 不可行 | 模块卡开机已回滚；sideload 替代 |
| 黑鲨 5 Pro | JoyUI 11 / A12 | ❌ 待临时 root | GhostLock E3 待续 |
