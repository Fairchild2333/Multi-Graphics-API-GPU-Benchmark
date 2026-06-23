# N-Body 计算负载方案（compute-bound 子项）

> **本文是负载套件中 #2（N-body / 算力）的深入设计。** 套件总览（四条性能轴、
> `Workload` 枚举、Synthetic Peak、Stress/Fractal、综合评分）见
> [`benchmark-workload-suite.md`](benchmark-workload-suite.md)。
>
> 目的：在现有"流式带宽"负载之外，增加一个**真正吃 ALU / 特殊函数单元 / shared memory**
> 的 compute 子项，让跑分能区分 GPU 的**算力**而不只是显存带宽。
> 框架（`AppBase`、计时、结果持久化）零改动；只新增 shader + 在 5 个后端各做最小改动。

---

## 1. 为什么是 N-body

当前 `compute.comp` 每个粒子只做 3 个乘加 + 1 次比较，读写约 40 字节 →
**算术强度 ≈ 0.15 FLOP/byte，纯带宽瓶颈**，测不出算力。

N-body 引力（all-pairs）是业界公认的 GPU compute 标杆负载（NVIDIA *GPU Gems 3*）：
每个粒子要和**全部 N 个粒子**算引力，配合 shared-memory 分块复用数据。

| 指标 | 当前 Stream | N-body（本方案） |
|------|-------------|------------------|
| 每元素算术量 | ~6 FLOP | ~20 FLOP × N 次交互 |
| 含 rsqrt 特殊函数 | 无 | 有（每次交互 1 个） |
| shared memory | 不用 | 用（分块 tile） |
| 数据依赖 / 累加 | 无 | 有 |
| 瓶颈 | 显存带宽 | **FP32 ALU + SFU + 占用率** |
| 算术强度 | ~0.15 FLOP/B | 数十~上百 FLOP/B（tiling 复用） |

→ 它能拉开 FLOPS 差距，正是"算力子项"该测的东西。

---

## 2. 算法设计：tiled all-pairs（shared-memory 分块）

复杂度 **O(N²)**，但用 threadgroup/shared memory 分块把全局访存降到 O(N)，使其 compute-bound。

每个线程负责 1 个粒子 `i`，单次 dispatch 内：

```
acc = (0,0,0)
for tile in [0 .. N/256):
    # 协作把本 tile 的 256 个粒子 position 载入 shared memory
    shared_pos[localId] = particles[tile*256 + localId].position
    barrier()
    for j in [0 .. 256):
        d      = shared_pos[j].xyz - pos_i.xyz
        distSq = dot(d,d) + softening*softening      # softening 防奇点
        invDist= rsqrt(distSq)                        # ← 特殊函数单元
        invDist3 = invDist*invDist*invDist
        acc   += d * invDist3                          # 质量设为 1
    barrier()
# 单 kernel 内直接积分（leapfrog/半隐式 Euler）
vel_i += acc * dt
pos_i += vel_i * dt
particles[i].position = pos_i
particles[i].velocity = vel_i
```

要点：
- **TILE = 256 = `kComputeWorkGroupSize`**，与现有 workgroup 完全对齐，无需新常量。
- shared memory 占用 = 256 × `vec4`(16B) = **4 KB**，远低于任何平台上限（含树莓派/移动端的 16–32 KB），不影响占用率。
- **单 dispatch 完成 force+积分**，结构与当前 `compute.comp` 一模一样 → 不需要新增 pass、新增 barrier、新增 buffer。
- softening²（如 0.01）加进 distSq，避免两粒子重合时 `rsqrt` 爆炸。

---

## 3. 参数与规模分级

注意：当前默认 1,048,576 粒子是给**带宽测试**用的。N-body 是 **O(N²)**，1M 会变成 ~10¹² 次交互、卡死。
**N-body 必须用自己的、小得多的粒子数**，并分级：

| 档位 | N | 交互数/步 (N²) | ~FLOP/步 (×20) | 典型用途 |
|------|------|----------------|----------------|----------|
| Low  | 16384 (16K)  | 2.7×10⁸ | 5.4×10⁹  | 树莓派 / 移动端 / 弱 GPU |
| Medium | 32768 (32K) | 1.1×10⁹ | 2.1×10¹⁰ | 入门独显 |
| High | 65536 (64K)  | 4.3×10⁹ | 8.6×10¹⁰ | 主流~高端独显（默认） |
| Ultra | 131072 (128K)| 1.7×10¹⁰| 3.4×10¹¹ | 旗舰 / 数据中心卡 |

粗略校准：64K 档在一张 ~10 TFLOPS 的卡上单步约 8–9 ms（适合做基准）；
弱端用 16K 档保证单步远低于 1 秒（见 §7 TDR 风险）。

N 必须是 256 的整数倍（dispatch = N/256，与现有公式一致）。

---

## 4. 复用现有资源（关键，决定了改动量极小）

- **粒子 buffer 不变**：现有 `Particle { vec4 position; vec4 velocity; }`（std430，32B）
  正好就是 N-body 需要的布局。直接复用同一个 SSBO，**不新建任何 buffer / descriptor**。
- **渲染路径不变**：N-body 更新的还是这同一组粒子，现有点云渲染照常显示（xy 投影）。
  渲染段计时照旧；compute 段计时就是我们要的算力指标。
- **初始化**：粒子初始位置/速度的生成在 `AppBase::GenerateInitialParticles()`。
  N-body 最好用球状/盘状初始分布（更稳定好看），可按 workload 分支生成——
  这是 `AppBase` 里**唯一**可能需要碰的点，且是数据生成、非框架逻辑。
- **计时 / 结果 / 菜单框架**：完全不动。

---

## 5. 新增 shader 文件

与现有 shader 并存（**不替换** stream 负载），命名对齐现有约定：

| 文件 | 对应后端 | 接入点 |
|------|----------|--------|
| `shaders/nbody.comp` | Vulkan (GLSL→SPIR-V) | 加进 CMake `GLSL_SOURCES` 列表自动编译 |
| `shaders/nbody.hlsl` | DX12 / DX11 (`CSMain`) | 加进 CMake HLSL 拷贝列表 |
| `shaders/nbody_gl.comp` | OpenGL 4.3 | 加进 CMake GL 拷贝列表 |
| `particle.metal` 内新增 `nbodyKernel` | Metal | 已随 `particle.metal` 拷贝，无需改 CMake |

shared memory 声明各 API 写法：
GLSL/GL `shared`；HLSL `groupshared`；Metal `threadgroup`。**纯 shader 内部，无 host API 改动。**

---

## 6. 各后端改动清单（最小化）

每个后端只动两处，且都是已有代码点：

1. **选 shader**：`CreateComputePipeline()` 里按 `config_.workload` 选加载哪个 shader 文件
   （Vulkan: `nbody.comp.spv` vs `compute.comp.spv`；DX: `nbody.hlsl` vs `compute.hlsl`；以此类推）。
2. **多传 2 个参数**：扩展共享的 `ComputeParams`，在**已有的** params 上传那一行带上新字段。

```cpp
// gpu_common.h —— 扩展参数结构（共享，所有后端复用）
struct ComputeParams {
    float deltaTime;
    float bounds;        // stream 用
    float softening;     // nbody 用（如 0.01）
    uint  numBodies;     // nbody 用（= particleCount）
};
```

- **Vulkan**：`CreateComputePipeline` 选 spv；push constant 已有，结构变大即可
  （[vulkan_backend.cpp:886](../src/vulkan_backend.cpp#L886) 那行带上新字段）。
- **DX12 / DX11**：`CompileShader` 选 `nbody.hlsl`；cbuffer 结构同步加字段。
- **OpenGL**：`CompileShaderGL` 选 `nbody_gl.comp`；uniform 多设 2 个。
- **Metal**：`newFunctionWithName` 选 `nbodyKernel`；params buffer 结构加字段。
- **dispatch 数量公式不变**：`numBodies / 256`。让 N-body 档把 `config_.particleCount`
  直接设为对应 N（16K/32K/64K/128K），dispatch 代码一行不用改。

> 共享/积分都在 shader 内完成，**不需要新增 descriptor、buffer、barrier 或额外 pass**。
> 单个后端预计改动 < 15 行 + 一个 shader 文件。

---

## 7. 框架与入口改动

- `gpu_common.h`：加 `enum class Workload { Stream, NBody };`，`BenchmarkConfig` 加
  `Workload workload = Workload::Stream;` + `softening`。（这是 config，非框架逻辑。）
- `main.cpp` 菜单/CLI：加一个选项选 workload + N 档位（如 `--workload nbody --bodies 65536`）。
- `BenchmarkResult` + 结果序列化：加 `workload` 字段，让结果表/CSV 能区分两类负载
  （否则带宽分和算力分会混在一起没法比）。
- `AppBase`：仅 `GenerateInitialParticles()` 可选按 workload 改初始分布。其余**不动**。

---

## 8. 评分指标（这才是"算力分"）

N-body 的工作量是**确定的**：`interactions = N² × steps`，每次交互固定 FLOP 数。
因此可直接由 compute 段时间算出硬件算力指标：

```
有效算力 GFLOP/s = (N² × steps × FLOP_per_interaction) / (computeMs_total / 1000) / 1e9
交互率 = N² × steps / computeSeconds      (interactions/sec)
```

- 这是个**绝对、可跨卡/跨 API 直接比**的数（不像 FPS 受呈现影响）。
- 跨 API 公平性：5 个后端用同一算法、同一 N、同一 softening/dt/步数、同一 TILE=256 → 苹果对苹果。
- 综合总分（上一轮讨论的）= 带宽分 与 算力分 的加权，比单一负载更代表"GPU 性能"。

### 旧 Stream 负载 → 正式的「显存/内存带宽分」

保留旧的 `compute.comp` 作为 `Workload::Stream`，并给它一个对称的确定性指标
（不再只看 FPS）：

```
搬运字节/步 ≈ (读 ~24B + 写 ~16B) × N ≈ 40 × N
有效带宽 GB/s = (40 × N × steps) / (computeMs_total / 1000) / 1e9
```

- 这是个跨卡/跨 API 可直接比的**显存带宽分**。
- 结果里 `workload` 字段区分 stream / nbody，带宽分与算力分**分开存储、分开展示**，
  综合总分再做加权。

---

## 9. 风险与对策

| 风险 | 说明 | 对策 |
|------|------|------|
| **TDR / 看门狗超时** | Windows 上单次 dispatch >~2s 会触发驱动复位（DX 尤其） | N 分级，保证单步远低于 1s；必要时把外层 tile 循环拆成多次 dispatch 累加 |
| 弱 GPU 太慢 | Pi / 移动端 64K 跑不动 | 默认按设备给低档（16K），档位可调 |
| FP32 精度差异 | 不同驱动 rsqrt 实现略有差异 → 轨迹微异 | 只用 compute **时间**评分，不校验轨迹结果；softening 提升数值稳定性 |
| 占用率 | shared mem / 寄存器压力影响占用 | TILE=256、shared 仅 4KB，已是保守配置 |
| 与带宽分混淆 | 两类负载分数不可直接相加 | 结果里带 `workload` 字段分开存储与展示 |

---

## 10. 落地顺序

1. `gpu_common.h`：加 `Workload` 枚举 + 扩展 `ComputeParams` / `BenchmarkConfig`。
2. 写 `shaders/nbody.comp`（GLSL 参考实现），跑通 Vulkan 单后端 + headless 验证算力数。
3. 移植到另外 4 个 shader（hlsl / gl / metal），逐后端接 `CreateComputePipeline` 选择逻辑。
4. CMake 把新 shader 加进各自的编译/拷贝列表。
5. `main.cpp` 加 workload/N 选项；`BenchmarkResult` 加字段。
6. 校准各档 N，确认无 TDR，记录基线数。
7. （后续）把算力分接入综合评分公式。

---

## 11. 验收标准

- [ ] 5 个后端都能在 `--workload nbody` 下跑出稳定的 compute 段时间。
- [ ] 同一 N 下，强卡的 GFLOP/s 明显高于弱卡（证明它测的是算力，不是带宽）。
- [ ] 同一卡上不同 API 的 GFLOP/s 接近（证明跨 API 口径公平）。
- [ ] headless 模式可纯算力跑（无显示器的开发板可用）。
- [ ] 无 TDR / 崩溃；结果表能区分 stream 与 nbody 两类负载。
</content>
</invoke>
