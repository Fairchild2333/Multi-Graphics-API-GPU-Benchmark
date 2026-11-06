# GhostLock 黑鲨 5 Pro 适配 — 完整交接文档

日期：2026-10-12。供任何后续 AI/工程师继续。所有结论按"已真机验证 / 已静态验证 / 未验证"分级标注。

## 目标与当前状态

目标：让 GhostLock（CVE-2026-43499 futex PI UAF 临时 root）在黑鲨 5 Pro（katyusha, SM8450, MP6, 内核 5.10.101-android12-9-gece0cc04d1a5）上成功执行。**当前状态：利用链 90% 真机跑通，最后的"写原语落点"未通。**

## 已真机验证跑通的链路（日志 ghostlock-run-*.log）

1. profile 解码 + KASLR 滑移 ✓
2. **KernelSnitch mm_struct 泄漏 ✓**（leaked=0xffffff803b4a5640 等）
3. **PI 竞争原语 ✓**（`CMP_REQUEUE_PI ret=-1 errno=35` = EDEADLK——这正是 UAF 缺陷路径被触发的标志，不是错误）
4. **盖章 route ✓**（route_done status=0 success=1；本 fork 的 sigreturn 盖章正常工作）
5. **W1 写 SELinux = 失败**（"Write 1 failed"）——唯一未通环节。验证方式：读 /sys/fs/selinux/enforce。

## 运行方法

```
# 构建（无 make，用脚本；NDK 28）
bash ghostlock-src/build_native.sh
# 部署（手机亮屏）
adb push build/native/ghostlock /sdcard/ghostlock
adb shell "cp /sdcard/ghostlock /data/local/tmp/ && chmod 755 /data/local/tmp/ghostlock && /data/local/tmp/ghostlock --load-prebuilt-profile /data/local/tmp/katyusha.bin"
# profile 生成：/data/local/tmp/make_profile（tools/make_profile.cpp，值全部硬编码并注明证据来源）
```

注意：Git Bash 会把 /data/... 转成 Windows 路径——用 `export MSYS2_ARG_CONV_EXCL='*'`。

## 已做的修复（相对上游 67215cf，全部在 ghostlock-src/）

| # | 修复 | 证据 | 状态 |
|---|---|---|---|
| 1 | 新增 sigreturn-fpsimd 盖章 route（`src/core/route/sigreturn_stamp.{h,cpp}`，multicast_waiter_route.cpp 的 setsockopt 换成 rt_sigreturn+fpsimd 帧） | MP6-alt-route-audit.md：本内核三条原生 route 几何全不可达；fpsimd 缓冲（深度 768）覆盖 waiter（448）✓ | ✅ 跑通 |
| 2 | profile：mm_struct_sz=960（不是 extractor 的 1024） | proc_caches_init 的 kmem_cache_create_usercopy size=0x3c0；slabinfo: objsize 960 / 34 objs / 8 页 | ✅ 跑通 |
| 3 | profile：task_struct 11 个字段、cred 模板（copy 0xa8、caps 0x30×3=0x1ffffffffff、refs 0x80/0x88/0x90）、route.multicast_waiter{waiter_off=320,buffer_size=528,task=48,lock=56} | 全部有反汇编出处（replay_static_audit.py） | ✅ 跑通 |
| 4 | ~~futex_hash.h initval-16~~ **已撤销** | 内联常量是 0xdeadbeff（=JHASH_INITVAL+16），原版 jhash2 即正确；我曾误读为 0xdeadbeef | ✅ 撤销后泄漏跑通 |
| 5 | fake waiter prio 写 0x40 而非仅 0x44（5.10 无 wake_state，prio 在 +0x40） | task_blocks_on_rt_mutex: `ldr w8,[x20,#0x84]; str w8,[x22,#0x40]` | ⚠️ 已改，未解决 W1 |

## 未通环节：写原语——剩余假设（按可能性排序）

机制（已对照 v5.10.101 rtmutex.c 逐步验证）：consumer 的 sched_setattr → rt_mutex_adjust_pi(waiter) → 链行走 → [11] rt_mutex_dequeue_pi(fake_task, W0) → **rb_erase(W0 的伪造 pi_tree_entry)** → 经 parent=dest-8 的 relink 把 0 写进 selinux_state。

假设 A：**consume 端触发条件未满足**——rt_mutex_adjust_pi 首个等值检查 `rt_mutex_waiter_equal(stale_waiter, task_to_waiter(task))` 读 stale->prio(+0x40) 与 task->prio；或 waiter_task->pi_blocked_on 实际未悬挂（remove_waiter 在本内核的调用上下文与 6.x 不同）。**验证法**：在 consumer 的 sched_setattr 后加内核行为探针（如对 waiter tid 连续不同 nice 调用，或用 futex_lock_pi(target) 兜底路径对比），或在 dmesg 找 WARN_ON（rb 路径有 WARN_ON_ONCE——内核日志读取需 root，可用 last_kmsg/pstore 事后看）。

假设 B：**[7] 对零值 tree_entry 的 rb_erase 行为差异**——stale waiter 的 tree_entry 全零，rb_erase(parent=0) 会把 fake_lock->waiters.rb_node 清成 NULL；随后 enqueue 重新插入。若 5.10 的 rb_erase_cached 对 empty/leftmost 处理与作者验证的 6.x 有出入，W0 的 erase 可能没被调用。**验证法**：给 stamp 的 waiter+0x00..0x18 也填树字段（像 encode_compact_waiter 对 0x18/0x20/0x28 做的那样——即把写参数同时铺到 tree_entry 和 pi_tree_entry 两组槽位）。

假设 C：**伪页还有常量错位**——FAKE_TASK_USAGE_OFF=0x40、FAKE_TASK_PRIO 等是编译期常量（target.h，为 oplus/6.x 设定），compact 分支的 fake_task 字段偏移用的是 profile 值（✓ 对）但 usage@0x40 未参数化。本内核 task->usage 的真实偏移需验证（可从 __arm64_sys_gettid 的引用计数行为或 atomic 变量访问点反推）。若 get_task_struct(fake_task) 打到错误偏移一般无害，但若触发 put_task_struct 释放路径会崩——日志未见崩，倾向无害。

假设 D：**W1 目标别名问题**——target=data_alias(selinux_enforcing)=0xffffff802ac51b20（直接映射别名）。若本内核的 linear map 基址/偏移换算与 app 假设有差（app 的 kImageTextBase=0xffffffc080000000 是 oplus 的，但 alias 用 delta 换算已自洽），写会落到错地址。**验证法**：改写一个可回读的测试目标（如 app 自己页内某变量）验证写确实到达。

## 未使用的备选/注意

- SVE 盖章变体（waiter_off=336, vregs[24]）已静态验证但未使用（fpsimd 足够）。
- 手机上原 GhostLock 1.2 (v566) 未改装；最终目标是编译 APK（compileSdk 37 + JDK 21，本机未备）把本 fork 的 native 打进去，实现"重启后手机上点一下就 root"。
- ADB 设备序列号 9893218b；执行前保持亮屏（用户提示：息屏后 adb 掉线）。
- **不得在未获用户同意下扩大到设备破坏性操作**；本 fork 的失败路径安全（ROUTE_FALLBACK_SAFE），三次运行无 panic。

## 工具与文档索引（Katyusha-root-analysis/）

- `MP6-kernel-analysis/`：全部静态审计（replay_static_audit.py、replay_alt_route_audit.py 均可复跑）、符号表、反汇编
- `ghostlock-run-20261011*.log` / `ghostlock-run-20261012-5.log`：真机运行日志
- `tools/kernelsnitch_probe.cpp`：哈希/碰撞判别探针
- `/tmp/ksrc/{futex.c,rtmutex.c,sched_core.c}`：v5.10.101 参考源码（gregkh/linux）
- 内核事实速查：waiter{tree_entry@0, pi_tree_entry@0x18, task@0x30, lock@0x38, prio@0x40, deadline@0x48}；task{prio@0x84, pi_lock@0x86c, pi_waiters@0x880, pi_top_task@0x890, pi_blocked_on@0x898, cred@0x780, comm@0x790, seccomp@0x848, usage 疑@0x40 未验证}；mm_struct 960B/34每slab/order-3；futex 哈希=标准 jhash2(key,4,offset)；表 2048。
