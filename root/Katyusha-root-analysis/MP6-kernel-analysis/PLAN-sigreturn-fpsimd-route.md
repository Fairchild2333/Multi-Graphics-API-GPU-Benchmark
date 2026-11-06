# 计划（L 类）：sigreturn-fpsimd 盖章 route 适配黑鲨 5 Pro MP6

状态：**草案，待用户认可后才会进入代码。** 依上游 `engineering-standards.md`，新增 route 属 L 级，须先有计划文档并获用户认可。

日期：2026-10-11。基线：上游 [YuKongA/ghostlock-app](https://github.com/YuKongA/ghostlock-app) commit `67215cfe204940fe71ff67213e76329e1adce7d2`（本地仅审阅，未修改）。目标内核：黑鲨 5 Pro MP6 `5.10.101-android12-9-gece0cc04d1a5`。

## 1. 现状与基线

- 三条上游 route（multicast_waiter / select_stack / tcp_zerocopy）在本内核均有独立布局限制，无法靠补参数修复（见 `MP6-static-route-audit.md`）。
- 静态核验发现替代盖章窗口：`rt_sigreturn → restore_sigframe → restore_fpsimd_context`，528 字节栈缓冲（520 字节用户可控）完整覆盖 stale waiter，task/lock 落在 vregs[23]；盖章是该系统调用最后一次深栈写，复制后无破坏（见 `MP6-alt-route-audit.md`，可由 `replay_alt_route_audit.py` 复核）。
- 已有 11 个 task_struct 候选字段与 cred 布局（0xa8）来自本镜像机器码；waiter 深度 448、字段 0x30/0x38 已断言。
- 边界：此前执行提权的请求曾被自动审批拒绝。本计划不改写、不绕过该拒绝；任何设备侧执行都必须由用户明确重新授权。

## 2. 目标与约束

目标：为 katyusha/MP6 增加一条以 rt_sigreturn（fpsimd 记录）为盖章原语的 route，复用上游既有的 waiter 插入（W1）与消费（consumer）机制，仅替换“stamp”一步。

约束：

- 不修改 remove_waiter / race / 写原语（fake_task/fake_lock）语义；共享步骤（setup/W1/handoff/retry）不复制进 route。
- route 由 profile 显式声明，不按内核版本/设备名推断。
- 无 BTF 的结构参数只能来自本镜像交叉验证过的候选，不引用其他机型偏移。
- 在用户明确批准设备测试之前，一切工作停留在离线。

## 3. 改动清单（逐文件，预估）

上游仓库（fork）：

| 文件 | 改动 |
|---|---|
| `src/core/route/route_policy.hpp` | 新增 `SigreturnFpsimdPolicy`：编译期能力声明、`kind`、`supported()`、`run()` |
| `src/core/route/sigreturn_fpsimd_route.cpp`（新） | Route 类 `prepare → execute → disarm → destroy`；execute 内构造 rt_sigframe 并 `svc` 触发 `__NR_rt_sigreturn`；Android-only 入口置于 `#if defined(__ANDROID__)` |
| `src/core/memory/payload_builder.*` | 复用 `encode_multicast_waiter` 的纯编码；新增 sigframe→vregs[23] 的映射函数（waiter_offset=320, task_offset=0x30, lock_offset=0x38, buffer_size=528） |
| `src/core/profile/model.h` / profile schema / Kotlin 注册表 | route kind 与字段登记（双侧注册表 + `route_catalog_test` / `RouteCatalogAgreementTest` 锁定） |
| `tools/extract_rs` | 可选：为无 BTF 目标输出 `sigreturn_fpsimd_layout` 常量（不改现有推导路径） |
| `docs/analysis/` | 本计划落地后的实现日志（计划/日志分离） |

配置形状：与 `MulticastWaiterLayout` 同构（buffer_size=528、waiter_offset=320、task_offset=0x30、lock_offset=0x38），便于复用 profile 校验。

## 4. 数据流/控制流差异

现状（multicast）：`setsockopt(MCAST_BLOCK_SOURCE, stamp)` 一次性盖章 → spin 等 consumer。

新 route 的 execute 差异：

1. prepare：预留/对齐用户栈帧（16 对齐，`sp+0x1250` 有效），装填 sigframe：
   - uc_sigmask @ +0xa8 = 全阻塞；GPR @ +0x138 保存线程现值；sp/pc @ +0x230/0x238 指向续体 stub；pstate @ +0x240 用安全值；
   - 记录链 @ +0x250：`fpsimd{0x46508001, 0x210, fpsr, fpcr, vregs}` + `END{0,0}`；
   - vregs[20..22]=0，vregs[23]={fake_task, fake_lock}（encode 纯函数产出）。
2. execute：内联 `svc`（x8=139）触发 rt_sigreturn；内核把 vregs 拷到深度 256..768 的栈窗口并覆盖 waiter；返回后线程从续体 stub 继续，立刻进入与 multicast 相同的 consumer 等待协议。
3. disarm/destroy：与现有 route 相同的清理与状态汇报（RouteStatus）。

控制流差异要点：rt_sigreturn 失败路径是 `arm64_notify_segfault`（杀进程），因此帧校验必须全过；sigmask 全程阻塞信号；盖章后到消费完成前，该线程不得发深栈系统调用。

## 5. 兼容性与回滚

- 新增 route 为显式选择，不影响其他机型/现有 route；回滚 = 不声明该 kind。
- profile 新字段向后兼容（未知 kind 由既有校验拒绝）。
- 不触碰 v1 offsets.json 路径与 kernelsnitch。

## 6. 验证矩阵

| 阶段 | 内容 | 门禁 |
|---|---|---|
| 离线（已完成） | 几何/调用链/无后置破坏断言 | `replay_alt_route_audit.py` exit 0 |
| 离线（待做） | sigframe 构造函数单元测试（字节级对照帧布局常量）；route_catalog 双侧一致性测试 | 测试全绿 |
| 设备（需用户重新明确授权） | 先只读验证：rt_sigreturn 帧往返不杀进程、vregs 残留位置（若可观察）；再谈提权步骤 | 每步用户确认 |
| 回退 | 任一步异常即 ROUTE_FALLBACK_SAFE，不留 dirty 状态 | RouteStatus 约定 |

## 7. 明确保留项

- 不改上游 remove_waiter 静态特征利用逻辑、不改 W1/consumer 竞争窗口实现。
- 不生成"试试看"的完整可执行配置；设备执行授权不在本计划范围内。
- 不把候选字段当作真机验证过的真值；设备测试前保持"候选"表述。

## 8. 未决问题

1. cpu_hwcaps bit0x10（ARM64_HAS_NO_FPSIMD）在 SM8450 上为 0 属推断，设备侧仍需一次无害验证（例如普通信号返回后浮点不损坏，或只读探针）。
2. 续体 stub 的具体形态（独立汇编 stub vs setjmp 式保存）与线程栈切换影响，实现期定稿。
3. SVE 变体（vregs[24]、waiter_offset=336）是否作为 fallback 一并实现，还是 fpsimd 优先、失败再补。
4. fake_task/fake_lock 写原语在本机的端到端行为仍受既有前提约束，超出本 route 范围。
