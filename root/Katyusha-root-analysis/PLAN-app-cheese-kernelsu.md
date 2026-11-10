# 计划：cheese 手机端 App + KernelSU handoff（段 A/B；段 C 明确不做）

日期：2026-11-11。状态：**待用户认可**（L 类：先计划后代码）。
关联：`cheese-katyusha/`（已验证的胜利配方，md5 3abcf719…）、
`HANDOFF-GhostLock-黑鲨5Pro.md`（GhostLock 路线封存）、
上游 `upstream-check/readme.md`（GhostLock AGENTS：W1/W2/W3 后 "handoff 给 root child 做 KernelSU 模块加载"）。

## 0. 用户问题

"能做到 ghostlock 那种 app 吗？用它跑通，然后用 KernelSU 做超级用户管理器，后面全部在手机上实现？"

## 1. 现状证据（已核实）

- 设备：BL 锁（`ro.boot.flash.locked=1`）、verified boot green、dm-verity enforcing。
- 已装 `me.weishu.kernelsu` v3.3.0 (32601)（KernelSU-NEXT 管理器）与 Magisk 29.0，但
  `/data/adb` 不存在、无 ksud 守护进程 → KernelSU 是空壳（无内核部分）。
- GhostLock 上游自己的真机门禁写的是"KernelSU 未加载的干净启动"——上游也假设
  内核部分不在，handoff 发生在 root 之后。
- cheese 链路已在 fresh boot 复现成功两次（cc7/cw2），root 期间可执行任意命令。

## 2. 结论先行

- **段 A（app 形态，点一下跑 cheese）**：可行。zhuowei/cheese-app 是现成模板
  （Compose + ProcessBuilder 跑 native ELF）；本机 JDK 17 + SDK 37.2 + NDK 28 齐全。
- **段 B（root 期部署 KernelSU 用户态）**：可做，但**价值有限且需实验验证**：
  KernelSU 的授权/模块能力依赖内核钩子，BL 锁设备刷不进内核 → 只能得到
  "每次开机先跑 cheese 激活的 su"，不是开机自启的 KSU。ksud+APK 可手动部署，
  成功判据：cheese 跑一次后，KSU 管理器内能为 app 授权 su。
- **段 C（解锁 BL 刷 KSU 内核）**：不做。会清数据、失去 MP6 保固评估状态，
  与"临时 root"定位冲突。

## 3. 段 A 设计（app）

1. 结构照抄 cheese-app：Kotlin/Compose UI + native ELF（assets 或 jniLibs），
   UI 启动后 `ProcessBuilder` 执行（GhostLock 同款模式，非 JNI）。
2. native 用 `cheese-katyusha/exploit.c` 原样编译（NDK clang -O2），不做任何改动；
   md5 入库校验。
3. UI 必须内置使用纪律：同 boot 只跑一次提示、失败建议重启、亮屏提醒、
   panic 风险提示（实证：同 boot 第 4 次完整运行后 kernel_panic）。
4. 输出流（stderr 日志）实时显示 + 落盘 `Download/cheese-app-log/`。
5. 成功态：UID 0 展示 + Permissive 展示 + 可输入命令的 root shell 面板。

## 4. 段 B 设计（handoff，实验性）

序列（root 期内、permissive 下）：
1. 从 APK assets 释放 ksud（官方 release `ksud-aarch64-linux-android`）与
   KSU APK 到 /data/local/tmp；
2. 部署 /data/adb/{ksud,modules}（结构与权限按 KSU 文档）；
3. 启动 ksud，验证 `/data/adb/ksud.sock`（或等价接口）响应；
4. KSU 管理器内为一个测试 app 授权 su，判据 = 该 app 实际拿到 uid 0；
5. **重启验证**：预期 KSU 失效（无内核钩子）→ 记录"需 cheese 先跑"的激活流程，
   形成可重复的一键脚本。

已知限制（写进 UI 文案，不隐瞒）：KSU 无内核部分 = 只有用户态 su 转发；
模块加载/内核授权不可用；每次开机依赖 cheese 激活。

## 5. 风险与门禁

- 同 boot 多次运行有 kernel_panic 实证 → app 内强制"本 boot 已运行"标记 + 不自动重跑。
- /data/adb 残留与 KSU 版本（NEXT 3.3.0 vs 官方 3.3.1）选择 → 先只用 NEXT
  （与设备已装管理器一致），官方版作对照。
- 每次实验前用户确认手机亮屏；失败路径不扩大权限、不刷分区、不动 boot/img。
- 归档：所有日志/版本/判据按仓库格式入 `cheese-katyusha/`。

## 6. 工作量估计

- 段 A：APK 骨架 + native 打包 + 纪律 UI ≈ 半天；真机门禁（冷机单次运行）≈ 1 小时。
- 段 B：ksud 部署脚本 + 授权实验 ≈ 半天；重启激活流程验证 ≈ 1 小时。
- 段 C：不排期。

## 7. 认可后第一步

clone cheese-app 为骨架 → 替换 native 为 cheesecake 胜利版 → 本机构建 APK →
adb 安装 → 真机单次运行门禁 → 归档后进入段 B 实验。
