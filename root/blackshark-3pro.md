# 黑鲨 3 Pro —— Root 完成实录 + 机制手册（2026-11-07 实机达成）

**状态：✅ 已 root**（2026-11-07）。解锁 BL + Magisk v31.1 + 原系统 V11.0.4.0（Android 11）完好。
本文前半是可用操作手册，后半是两天实测挖出的机制——下次碰到这台机（或同款黑鲨）直接按图索骥。

## 本仓库档案

- 机队实测记录见 `HANDOFF.md`（2026-10-17 显存带宽核对：256 MiB 堆）。
- 骁龙 865（SM8250），内核 4.19.113-perf+，**实测系统：Android 11 / MOBS2211210CN00MR6 / V11.0.4.0.JOYUI**（2022-11-21 国行包，出厂 Android 10 后升级）。
- 国行 **MBU-A0**（序列号 767b9490，代号 mobius，SOC serial 4e6b5320），全球版 MBU-H0。

## 最终成功的完整流程（实测顺序，照做即可）

前提：PC 端 `C:\adb\` 工具集（Unlock 目录 + MiFlash + fastboot）+ bkerler/edl（Python，走 Zadig WinUSB，
**绕开了 QDLoader 老驱动的签名墙**——Win11 上那驱动签名报错 52，F7 会话可用但每次重启都要重按）。

1. **EDL 写通用解锁 oeminfo**：`python edl.py --loader=prog_ufs_firehose_sm8250_ddr_lp5.elf w oeminfo unlock.img`
   （unlock 分区 = GPT 里的 `oeminfo`，起始扇区 3996200，64 MiB，2022 系统下扇区几何与 2020 模板完全一致）
2. 重启进 fastboot（**注意**：此时屏幕 `DEVICE STATE` 显示 locked 是显示 bug，别信）
3. `fastboot oem unlock`（OKAY）→ `flashing unlock`（already unlocked）→ `flashing unlock_critical`（OKAY）
4. `fastboot erase:userdata`（冒号语法！`erase userdata` 报 unknown command）
5. 重启 → 卡 logo（预期内！）→ 强制重启 → **引导器报 Unbootable 落回 fastboot** → `fastboot set_active:b` 清掉重试计数 → `continue`
6. **"Cannot load Android system" 错误页 → 进 stock recovery（电源+音量上）→ Factory data reset** ←
   **决定性一步**。TWRP 的 format data 不行（见下"机制"节），必须官方 rec 走完这次出厂重置
7. 开机向导 → 系统正常 → 装 Magisk（需开「USB 安装」）→ App 内修补 boot → EDL 写回 boot_b → root ✅

** Magisk 修补的 boot 镜像不走 fastboot flash**（下载缓冲只有 6.25 MB），走 EDL `w boot_b`（35 MB/s，几秒）。

## 手机当前形态与产物位置

- `/data/adb/lockfreq.sh`：一键锁频脚本（GPU 钉 0 档 905 MHz + CPU 三集群钉顶）。重启后跑一遍即生效。
- Magisk v31.1（com.topjohnwu.magisk），`su -v` → `31.1:MAGISKSU`，`verifiedbootstate=orange`（解锁正常警示色）。
- PC：`C:\adb\backup\`（68 个分区镜像）+ `C:\adb\oeminfo_backup_locked.img`（锁定态原版 oeminfo，重上锁素材）+
  `C:\adb\fb.py`（自写 Python fastboot 客户端， JoyUI 引导器命令集怪，标准 fastboot 部分命令不通）。
- NAS：`\\BigHouse\搞机\刷机\黑鲨3Pro\`（工具包、Magisk、教程 docx）+ `partition-backup-20261107\`（745 MB 全量镜像）。

## 机制手册（两天实验挖出的坑与规律）

### 1. oeminfo = 锁状态载体，且内容分代

- GPT 无 `unlock` 分区名，对应分区叫 `oeminfo`（PRODUCTLINE 魔数 + 厂商页结构：0x600 起是带标签的槽位页
  ——本机 LCDEXPAND/LCDEXPAND1/LCDEXPAND2/CALIBRATION/MISC，通用包里是 CALIBRATION/MISC/FCTINFO，**布局是按设备出厂内容定的**）。
- 引导器 `DEVICE STATE` 显示与实际锁行为**跟随 oeminfo 内容**：原版内容→显示 locked、EDL 重启被禁
  （"in lock mode"）、AVB 失败走红屏强制关机；通用包内容→显示 unlocked、允许 fastboot 写。
- 0x5000 页（4 KB）：原版是 base64url blob（内含 "undefined" 字样），通用包是 0x28 前缀的二进制结构——疑似锁状态/回滚记录页。

### 2. 状态矩阵（RPMB = `oem unlock` 写入的 UFS 内部标志，分区写不碰它，无法撤销）

| oeminfo 内容 | RPMB 解锁标志 | 开机 |
|---|---|---|
| 通用包 | 无 | ✅ |
| 通用包 | 有 | ❌ 无限 logo（复现 3 次，内核日志见 crash_dump 于 5-6 秒） |
| 原版 | 有 | ✅（T8/T12 两次验证） |
| 原版 | 无 | ✅（出厂态） |

**"通用包 + RPMB 解锁"必崩**是 V11 的 abl/TEE 校验通用凭证与本机不匹配所致；最终靠 stock recovery 的
出厂重置（走 TEE 密钥重建）解套。**TWRP 的 format data 修不好**——它够不着 Weaver/keymaster 的密钥销毁重生成。

### 3. JoyUI 引导器的怪癖清单

- 屏显 DEVICE STATE 可能与实际相反，以 `getvar unlocked` + 行为为准
- `erase:userdata` 冒号语法；`erase userdata` 不认
- fastboot 下载缓冲 6.25 MB（`DATA0063F400`）→ 大镜像 flash 不可用，一律 EDL
- `reboot-edl`：锁定态假 OKAY 不动作；解锁态真生效
- `flashing unlock` 在某些状态报 "already unlocked" 但未必真解锁——**两个 Okey**（unlock + unlock_critical）才是完整
- adb push 在 Git Bash 下必须 `export MSYS2_ARG_CONV_EXCL='*'`（Katyusha 交接文档 10 月就写过，勿忘）

### 4. OnFix/XDA 解锁文件真相

OnFix 88.23 MB 的"解锁文件"与 XDA KM 工具包内 unlock.img **MD5 一致**（71b9390e…），纯转卖，别买。
全网的通用文件对 3/3P/3S 机型通用，但**对固件版本不通用**：2020 年（Android 10）全成功，V11 上
"通用包+解锁"必崩、必须靠 stock recovery 收尾才成立。XDA #126 楼 outidd 的 JoyUI 12 CN 成功案例与本文流程一致。

### 5. 锁频能力（root 的兑现）

- GPU：`/sys/class/kgsl/kgsl-3d0/` 档位制，9 档 305M–905M。JoyUI 出厂 `max_pwrlevel=3`（587 MHz 封顶）。
  `max_pwrlevel/min_pwrlevel` 写 0 → **锁 905 MHz 完全生效**（devfreq min/max 窗口自动跟随）。Tegra 那套"调速器不认"不存在。
- CPU：schedutil 下 `scaling_min_freq` = 集群顶频即锁频（小核 1.8G/中核 2.42G/大核 2.84G），全部生效。
- 温度墙：`thermal_pwrlevel=0` 时无降档；跑分时仍需控温（老规矩）。

### 6. 救砖与回滚

- 9008 + firehose 全程可用（锁不锁都行），68 分区镜像在 NAS，理论可字节级复活
- `spinor` 分区（独立小芯片）firehose 读写不了，无关键引导数据
- 唯一没备份的是 `super`（15 GB 系统）——**在解锁实验全成后无需动它**；若日后刷写，先从 66ROM 找 V11 包
- 重上锁素材：`oeminfo_backup_locked.img`（写回即恢复锁定显示与行为，慎用，会重新触发清数据）

## 备查链接（2026-11-07 实测）

- XDA 解锁文件帖（含成功案例 #126 outidd JoyUI 12 CN）：<https://xdaforums.com/t/blackshark-3-3-pro-3s-unlock-bootloader-files.4140001/>
- XDA 完整教程帖（KM7）：<https://xdaforums.com/t/rooting-black-shark-3-3-pro-the-correct-easy-way-bl-unlocking-twrp-ota-fix.4147441/>
- bkerler/edl 工具：<https://github.com/bkerler/edl>
- OnFix（转卖确认，仅存档）：<https://onfix.cn/rom/566232>
- 66ROM 国行全量包（降级备用，需 9.9 邀请码）：<https://www.66rom.com/thread-27914-1-1.html>
- QDLoader 9008 驱动（archive.org 镜像，MD5 7cfe9c89…）：`https://archive.org/download/qdloader9008fixsoftware/Qualcomm_QDLoader_HS-USB_Driver_64bit_Setup.zip`

## 跑分纪律（沿用 root/README.md）

root 会话期间成绩**另起结果组**；`DEVICE-STATE.txt` 记录：锁频脚本已跑（GPU 905 / CPU 顶频）、
`verifiedbootstate=orange`、系统 V11.0.4.0。跑前 `top` 确认空载 + 温度回稳。

## 谷歌框架（2026-11-07 实测完成）

- **NikGapps 在本机走不通**：Magisk 模块方式被安装器拒绝（"cannot be flashed as a module"）；
  recovery 方式卡在 wzsx150 TWRP 3.4.2B 无法把 Android 11 动态分区挂为可写（Mount.log: `readwrite → Status 255`）。
- **最终方案：LiteGapps nano（arm64/30）Magisk 模块**，systemless 叠加不碰 super。
  SourceForge 路径 `litegapps/arm64/30/<变体>/<日期>/`。
- ⚠️ **Magisk App 图形界面刷 LiteGapps 会静默失败**（模块目录只剩 module.prop，无任何 APK）——
  必须命令行刷：`su -c magisk --install-module /sdcard/Download/LiteGapps.zip`，看到逐项 "Installing ..." + Done 才算数。
- ⚠️ 刷完**第一次重启可能崩溃循环**（GMS 首次 dexopt/挂载竞态），TWRP 里禁用再启用、或直接再重启一次即自愈。
  第二次开机 GMS/Play 商店/GSF 全部就位。
- 教训：bootloop 后若 su 消失但 magiskd 还在，先重启一次再看——脏关机后的半死状态会被下次开机清掉。
- 槽位策略：A 槽保持出厂原版当恢复网（B 槽崩溃时 `fastboot set_active a` 回干净系统），B 槽承载 root+GApps。
