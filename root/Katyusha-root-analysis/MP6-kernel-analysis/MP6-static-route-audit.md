# 黑鲨 5 Pro MP6：结构参数与三条利用路径的静态核验

日期：2026-10-11。目标内核：`5.10.101-android12-9-gece0cc04d1a5`。

结论：缺 BTF 的结构参数可以从目标机器码恢复一部分，但当前上游 GhostLock 的三条路径还有独立的布局限制，补齐配置不能消除这些限制。本报告未证明整个手机无法临时 root；新的系统调用路径或代码适配仍是未评估事项。

本次全部工作在电脑上离线完成；未执行提权、刷机或清除数据。ADB 最后一次枚举的是华为 NOH-AN00，黑鲨没有连接。

## 内核与符号

- 已从经过 MP6 目标分区哈希核验的 boot.img 解出原始 ARM64 Image。
- Image SHA-256：`136177a1c360eb64a8556cb4e6385a06e34c2ceabc05c7da714428837309bf6f`。
- 用 vmlinux-to-elf 1.3.6 恢复出 136,128 个符号。
- 恢复后的 `vmlinux.recovered.elf` 只有重建的函数/数据符号，没有原始 DWARF 调试类型或 BTF。它不能当作“找到原厂调试 vmlinux”。
- `_text` 基址：`0xffffffc010000000`；`init_task`、`init_cred` 等偏移与此前 GhostLock 导出的配置一致。
- remove_waiter @ 0x1e5a04 中，0x1e5a40 读取 sp_el0 到 x20，0x1e5ac4 清空 [x20+0x898]；符合上游关注的未修复代码特征。静态特征不等同于端到端提权成功。

## 从目标机器码提取的 task_struct 候选

以下字段来自本镜像的具体指令，证据地址、字节和指令逐项保存在 JSON 中；未经过真机或同构建调试类型的最终验证。

| 字段 | 十六进制偏移 | 十进制偏移 |
|---|---|---|
| `prio` | `0x84` | 132 |
| `normal_prio` | `0x8c` | 140 |
| `sched_task_group` | `0x310` | 784 |
| `pi_lock` | `0x86c` | 2156 |
| `pi_waiters` | `0x880` | 2176 |
| `pi_top_task` | `0x890` | 2192 |
| `pi_blocked_on` | `0x898` | 2200 |
| `real_cred` | `0x778` | 1912 |
| `cred` | `0x780` | 1920 |
| `comm` | `0x790` | 1936 |
| `seccomp` | `0x848` | 2120 |

尚未推导：pid、tgid、tasks、atomic_flags。

## cred 候选

- `cred_init` @ 0x26b4c80 的 kmem_cache_create 参数给出 size=0xa8；`prepare_creds` @ 0x181270 的 memcpy 参数也给出 0xa8，二者一致。
- 拷贝大小候选：168 字节。
- init_cred 的 0x30、0x38、0x40 三个 capability 槽都是 0x1ffffffffff；0x28 和 0x48 为 0。
- 三个非零引用：0x80=root_user、0x88=init_user_ns、0x90=init_groups，均与本镜像符号地址核对。
- 本次没有把这些候选写入手机，也没有生成可执行的完整 profile。

## 当前三条路径的限制

### multicast_waiter

按上游相同算法，并核对本镜像函数序言：

```
setsockopt 链：16 + 128 + 64 + 16 + (96 + 576) = 896
复制窗口：sp + 0x20，距统一栈顶 896 - 32 = 864
futex 链：144 + 32 + 416 = 592
waiter：sp + 0x90，距统一栈顶 592 - 144 = 448
waiter 在多播复制窗口中的偏移：864 - 448 = 416
```

本镜像 ip_setsockopt @ 0x14f0f60 明确比较长度 0x108，0x14f0f6c 以 0x108 调用复制，因此输入窗口是 264 字节。

task / lock 分别位于 waiter+48 / waiter+56。要完整覆盖 lock 需要 416+56+8=480 字节，超过 264。

这与手机导出的 waiter_off=416、buffer_size=264 一致。直接增大 profile 的 buffer_size 不会改变内核对 264 字节长度的检查。

### select_stack

```
pselect6 包装函数帧 = 160
core_sys_select 帧 = 448，stack_fds 在 sp + 80
复制窗口距统一栈顶 = 160 + 448 - 80 = 528
waiter 距统一栈顶 = 448
waiter 相对输入窗口 = 80 字节（第 10 个 8 字节字）
实际 task / lock 位于第 16 / 17 个字
```

当前上游 route 使用 NFDS=320：每组 fd_set 40 字节，三组用户输入共 120 字节，只控制第 0..14 个字。实际 task/lock 字段超出用户输入覆盖范围。

本内核栈/堆阈值是每组字节数 < 43；40 字节仍在栈上，增加到下一档 48 字节会走堆分配。单纯增大 NFDS 或改 shift 不会保持原来的栈覆盖关系。

### tcp_zerocopy

本镜像 do_tcp_getsockopt @ 0x150699c 比较长度 25；超过时在 0x1506a80..0x1506a84 将拷贝长度设为 24。当前上游载荷放在偏移 40（task）和 48（lock），不会被这个旧 ABI 的输入复制覆盖。

这是目标镜像的实际行为；不能根据其他 5.10 或 6.x 的 ABI 大小替代。

## 可复核方式

```
python replay_static_audit.py
```

需要 `../tools/analysis-pydeps` 中保存的 Capstone 5.0.9。脚本会核对 Image SHA-256、每处指令的预期值、11 个候选字段的证据、凭据大小与引用地址，以及三条路径的计算。此次运行 exit_code=0，所有断言通过。

机器证据：`static-offsets-and-route-audit.json`。相关函数的完整反汇编片段也在当前目录。

核对的上游提交：`67215cfe204940fe71ff67213e76329e1adce7d2`。本报告的源码兼容性结论针对这个提交，不声称已逐字节核对手机上安装的 native 二进制。

## 后续所需工作

继续研究需要寻找本内核上能覆盖目标 waiter 且时机可用的其他系统调用/覆盖方式，再做代码适配与验证。当前没有可用的完整配置或已验证方案，不宜把候选字段与邻近内核的路线参数拼成“已适配”的配置。

相关公开的 5.10 无 BTF 适配请求 #301 最终是升级 ROM 后使用新的内核成功，并没有提供其旧内核的解决补丁。该案例不能视作 MP6 的可用方法。

## 上游链接

- https://github.com/YuKongA/ghostlock-app/blob/67215cfe204940fe71ff67213e76329e1adce7d2/tools/extract_rs/src/derive.rs#L122-L231
- https://github.com/YuKongA/ghostlock-app/blob/67215cfe204940fe71ff67213e76329e1adce7d2/src/core/route/select_stack_route.h
- https://github.com/YuKongA/ghostlock-app/blob/67215cfe204940fe71ff67213e76329e1adce7d2/src/core/route/tcp_zerocopy_route.cpp
- https://github.com/YuKongA/ghostlock-app/issues/301
- https://github.com/marin-m/vmlinux-to-elf
