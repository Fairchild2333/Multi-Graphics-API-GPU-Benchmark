# 黑鲨 5 Pro MP6：候选替代复制窗口的静态核验（poll 族与 sigreturn 族）

日期：2026-10-11。目标内核：`5.10.101-android12-9-gece0cc04d1a5`（Image SHA-256 `136177a1c360eb64a8556cb4e6385a06e34c2ceabc05c7da714428837309bf6f`）。

结论：在上一轮三条路径（multicast / select / tcp_zerocopy）被布局限制排除后，本轮检查的 6 个候选里，**rt_sigreturn + restore_fpsimd_context 的复制窗口几何与结构上可覆盖 stale waiter**；poll 族（ppoll / do_sys_poll）确认不可用。sigreturn 路径是一个新的“盖章”（stack stamp）机制，不是上游已有 route，落地需要 L 类适配（计划文档见 `PLAN-sigreturn-fpsimd-route.md`，须经用户认可后才写代码）。

本次全部为电脑上的离线静态分析：未连接手机、未执行提权、未刷写、未生成可导入的 profile。

## 判据

沿用 `replay_static_audit.py` 的几何模型。统一栈顶 = 系统调用 C 入口 sp；waiter 位于深度 448（futex 链帧和 592 − waiter 局部 0x90），其 task/lock 字段在 waiter+0x30 / +0x38，即深度 384..400 区间。某复制窗口低端深度 D、用户拷贝长度 N，覆盖深度区间 (D−N, D]；能盖住 task/lock 的条件为 **D ≥ 400 且 N ≥ D−384**。

## 各路径结论

| 路径 | 复制窗口（低端深度 D） | 用户可控长度 N | 结论 |
|---|---|---|---|
| ppoll / do_sys_poll | 1044（stack poll_list，sp+0x7c） | 首块 ≤ 240（min(nfds,30)×8） | **死路**：需 660，缺口 420 字节；超出 30 项的部分走 `__kmalloc`，不落栈 |
| ppoll timespec | 104（sp+0x18） | 16 | 死路：整个窗口比 waiter 浅 |
| rt_sigreturn 自身 | 56..80（sp+0） | 24（uc_stack） | 死路：太浅 |
| restore_sigframe GPR 恢复 | 无栈窗口 | — | 逐字段直写 pt_regs（统一栈顶之上），无覆盖能力 |
| **restore_fpsimd_context** | **768（sp+0）** | **520**（512 vregs + fpsr + fpcr） | **可行**：waiter 落在 vregs[20..24)，task/lock = vregs[23] |
| restore_sve_fpsimd_context | 784（sp+0x10） | 520 | 次选可行：task/lock = vregs[24]；需 ARM64_SVE 且约束更多 |

## sigreturn + fpsimd 路径的完整链

```
__arm64_sys_rt_sigreturn   帧 0x50   （校验 user sp 16 对齐、access_ok(sf, 0x1250)）
└─ restore_sigframe        帧 0x60
   ├─ set_current_blocked        ← 复制前，浅栈，不碰目标区
   ├─ GPR 循环 sf+0x138.. → pt_regs（栈顶之上）
   ├─ valid_user_regs            ← 复制前
   ├─ parse_user_sigframe  帧 0x30（全函数无 bl/blr，零拷贝，仅遍历校验记录）
   └─ restore_fpsimd_context  帧 0x250   ← 【盖章发生处，整个 syscall 最后一次深栈写】
      ├─ memset(sp, 0, 0x210)            栈缓冲 user_fpsimd_state @ sp+0
      ├─ 校验 magic==0x46508001、size==0x210（精确）
      ├─ __arch_copy_from_user(sp, rec+0x10, 0x200)   vregs[32] → sp+0..0x200
      ├─ fpsr → sp+0x200，fpcr → sp+0x204（各 4 字节）
      └─ fpsimd_update_current_state(sp)   （读缓冲，帧更深，不破坏）
   （restore_fpsimd 返回后 restore_sigframe 直接 b 到尾声，rt_sigreturn 仅再拷 24B 到深度 56..80）
```

复制后的存活时序与 multicast 盖章完全同构：stamp 是该线程内核栈上最后一次深写，返回用户态后残留保留，消费在另一线程完成；其间目标线程不得进入更深的系统调用（与现有 race 纪律一致）。

几何（已断言）：

- 缓冲低端深度 D = 0x50+0x60+0x250 = **768**；vregs 覆盖深度 256..768。
- waiter 起点 = 缓冲偏移 **320** = vregs[20]；task = 缓冲偏移 **368** = vregs[23] 低 8 字节；lock = **376** = vregs[23] 高 8 字节。
- 换算到用户 sigframe 内的 fpsimd 记录（记录头 16 字节 + vregs）：task 在记录偏移 0x180、lock 在 0x188。
- fpsr/fpcr 会被 `fpsimd_update_current_state` 做寄存器掩码，但栈上残留是原始拷贝值，不影响 waiter 区（384..448 不在 248..256 的 fpsr/fpcr 槽内）。

## 帧构造的静态要求（route 侧）

- user sp 16 字节对齐；`sp + 0x1250` 在用户空间有效（rt_sigreturn 的 access_ok）。
- `uc_sigmask` @ sf+0xa8（8B）：应保持/设置为阻塞全部信号的状态，避免恢复途中投递信号。
- GPR @ sf+0x138..0x230、sp @ sf+0x230、pc @ sf+0x238、pstate @ sf+0x240：pstate 必须过 `valid_user_regs`（保守值 0/原值回写）；pc/sp 指向 route 的用户态续体 stub。
- 记录链 @ sf+0x250，各记录偏移 16 字节对齐：`fpsimd{magic=0x46508001, size=0x210, fpsr, fpcr, vregs[32]}` + `END{magic=0, size=0}`（本内核 END 要求 size=0，`cbnz w20 → -EINVAL`）。
- vregs[20..22] 置 0（与 multicast 盖章 memset 后只写 task/lock 的形状一致），vregs[23] = {fake_task, fake_lock}。
- 任一步校验失败走 `arm64_notify_segfault` 杀进程，因此帧必须整体有效——这是 route 帧构造正确性的硬约束，也是设备测试前必须离线定死的部分。

## cpu_hwcaps 门（分支选择）

parse/restore 里的位测试对应的 cap（AOSP `android12-5.10` `arch/arm64/include/asm/cpucaps.h`，GPL-2.0）：

- bit 4 = `ARM64_HAS_PAN`：拷贝助手的 PAN 切换门，两条路径都兼容。
- bit 0x10 = `ARM64_HAS_NO_FPSIMD`：**置位才拒绝 fpsimd 记录并跳过恢复**。SM8450 有 FPSIMD，必为 0 → fpsimd 路径即本机生效路径。功能上亦可反证：若该位为 1，普通信号返回后浮点状态会损坏，整机应用无法工作。
- bit 0x16 = `ARM64_SVE`：SVE 记录/恢复的门。8 Gen 1（ARMv9）大概率置位，故 SVE 变体可作备选；fpsimd 路径不依赖它。

## poll 族的排除证据

- `do_sys_poll` 帧 0x410（`stp x29,x30,[sp,#-0x60]!` + `sub sp,#0x3b0`），stack poll_list 头在 sp+0x70、entries 在 +0xc；首块 `min(nfds, 30)` 项（`mov w9,#0x1e; csel`），拷贝 len×8 ≤ 240；后续块 `__kmalloc(len*8+0x10)` 走堆。
- 链深 0x80+0x410 = 1168，缓冲低端 1044，覆盖 804..1044；waiter 字段在 384..400，需 660 字节拷贝，实际 240。结构性差距，与 nfds 无关。

## 可复核方式

```
python replay_alt_route_audit.py
```

需要 `../tools/analysis-pydeps` 的 Capstone 5.0.9；不连接手机。本次 exit_code=0，全部断言通过（含 parse_user_sigframe 全函数无 bl/blr 的“无后置破坏”断言）。机器证据：`alt-route-audit.json`。上一轮三条路径的复核仍用 `replay_static_audit.py`（本次亦重跑通过）。

## 限制与未验证项

- 候选字段与 waiter 深度仍是静态推导，未经真机验证。
- sigreturn 路径未做设备测试；帧构造（pc/sp 续体、sigmask、consumer 竞争时序）未实现。
- 本报告证明的是“存在几何与结构上合格的盖章窗口”，不等于端到端提权成功；fake_task/fake_lock 写原语与 W1/consumer 阶段沿用上游既有机制，其在本机的可用性仍受此前审计的 remove_waiter 静态特征等前提约束。
- 未评估：扫描全镜像 copy_from_user 调用点的兜底方案（若 sigreturn 路径在设备上被否，再启动）。

## 上游链接

- https://github.com/YuKongA/ghostlock-app/tree/67215cfe204940fe71ff67213e76329e1adce7d2/src/core/route
- https://android.googlesource.com/kernel/common/+/refs/heads/android12-5.10/arch/arm64/include/asm/cpucaps.h
