# 黑鲨 5 Pro MP6：GhostLock 配置校验诊断

核验日期：2026-10-11。以下为离线固件与上游源码分析；未执行提权、刷写、解锁或清除数据。

## 已确认的事实

- 当前系统此前通过 ADB 读取的内核：`5.10.101-android12-9-gece0cc04d1a5`。
- MP6 boot.img 解出的 Linux banner 与上述内核一致。
- MP6 的 boot.img、去除零填充后的 xbl_config.img、uefi.img 均通过官方增量 OTA 的目标分区 SHA-256 核验，手机存储中的三份文件已读回核验。
- boot.img SHA-256：`2db977b918e1b3af0ecb37d9c77f89816db9f7e2e65495cbdfd94b88cfcc550b`。
- 解出的 ARM64 Image 大小：48902580 字节；SHA-256：`136177a1c360eb64a8556cb4e6385a06e34c2ceabc05c7da714428837309bf6f`。
- 完整内核没有上游提取器使用的 BTF 头签名，有效嵌入 BTF 为 0。
- 内核 IKCONFIG 已解压：`# CONFIG_DEBUG_INFO_BTF is not set`，`CONFIG_DEBUG_INFO=y`，`CONFIG_KALLSYMS_ALL=y`。
- `CONFIG_DEBUG_INFO=y` 不表示发行版 boot.img 携带 DWARF 类型信息；需要同次构建的未剥离 vmlinux 才能利用那些调试信息。
- 手机 GhostLock 导出的合并配置 `ghostlock-config-audit-20261011-1610.conf` 中，task_struct 的各字段、cred 模板、multicast_waiter 的 lock_offset / task_offset 均为 null；kernel_phys_load 已有值。

## 上游代码与现象一致

核对的主分支提交：`67215cfe204940fe71ff67213e76329e1adce7d2`。

- `tools/extract_rs/src/symbols.rs:147-161`：resolve_structs(None) 将结构字段置 None。
- `tools/extract_rs/src/main.rs:700-719`：5.x 的 cred 模板推导依赖 BTF；无 BTF 则省略。
- `docs/analysis/extractor-no-btf-struct-derivation-plan.md` 描述了离线反汇编恢复这些字段的方案，但明确写着“计划，非本轮实现”，各阶段仍未勾选。
- 当前提交没有计划中的 tools/extract_rs/src/structs.rs，也没有匹配 katyusha / 该 release 的内置 profile。
- 本次查询的公开问题与未合并 PR 未找到此机型的已完成兼容补丁。这是本次检索范围内的结果，不能证明所有公开分支或私人配置都不存在。

## 结论与可行路径

当前 UI 卡在配置校验，尚未进入漏洞执行。镜像匹配与 UEFI 缺失问题已经解决；现在缺的是准确的内核结构布局。

可用材料：同次构建的带调试信息 vmlinux、经目标镜像交叉验证的完整 profile，或以该目标 Image 为依据的离线反汇编适配。

不能将邻近版本/其他手机的字段直接视作该内核的真值，也不能把 null 改成 0 当作修复。

缺少 BTF 不代表内核必然无法临时 root；本次尚未证明整条漏洞利用链在这台手机上可行，也没有生成可执行的完整适配配置。

## 证据文件

- MP6-kernel-analysis/verification.json
- MP6-kernel-analysis/kernel.config
- MP6-kernel-analysis/Image
- MP6-local-ROM-verification.json
- MP6-phone-readback-20261011-160453/verification.json
- ghostlock-config-audit-20261011-1610.conf

## 上游链接

- https://github.com/YuKongA/ghostlock-app/blob/67215cfe204940fe71ff67213e76329e1adce7d2/tools/extract_rs/src/symbols.rs#L147-L161
- https://github.com/YuKongA/ghostlock-app/blob/67215cfe204940fe71ff67213e76329e1adce7d2/tools/extract_rs/src/main.rs#L700-L719
- https://github.com/YuKongA/ghostlock-app/blob/67215cfe204940fe71ff67213e76329e1adce7d2/docs/analysis/extractor-no-btf-struct-derivation-plan.md


## 后续静态核验补充

已从同一目标镜像恢复 136,128 个符号并推导出 11 个 task_struct 候选字段与 cred 候选布局。但进一步发现当前上游三条路线均存在独立的目标布局限制：多播偏移 416 超过 264 字节复制窗口；Select 的 task/lock 落在可控字 0..14 之外；TCP 的旧 ABI 只复制 24 字节。详细证据和限制范围见 `MP6-kernel-analysis/MP6-static-route-audit.md`，机器证据见同目录 JSON。此次没有生成或安装可运行的完整配置。

## 2026-10-11 晚间补充：替代路径结果

6 个候选替代路径核验完成，见 `MP6-kernel-analysis/MP6-alt-route-audit.md`。poll 族（ppoll/do_sys_poll）排除；rt_sigreturn + restore_fpsimd_context 的复制窗口（深度 768、vregs 520B 可控、task/lock=vregs[23]）在几何与结构上可覆盖 stale waiter，属新的盖章机制。落地为 L 类适配，计划草案 `MP6-kernel-analysis/PLAN-sigreturn-fpsimd-route.md` 待用户认可；尚未写代码、未做设备测试、未生成可执行配置。
