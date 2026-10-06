# Mangekyo GPU Benchmark 负载套件方案

> 本项目的跑分负载从"单一粒子流"扩展为**一套覆盖 GPU 四大性能轴的子项**。
> 每个子项都复用现有框架（`AppBase` / 计时 / 结果持久化），通过 `Workload` 枚举切换。
> N-body 的详细设计见配套文档 [`nbody-workload-plan.md`](nbody-workload-plan.md)。

---

## 0. 套件总览：四条 GPU 性能轴

| # | 负载 | 测什么轴 | 渲染? | 主指标 | 参考对标 |
|---|------|----------|-------|--------|----------|
| 1 | **Stream**（现有 `compute.comp`） | 原设计目标为显存/内存**带宽**；v1 实为按 40 B 估算的粒子更新吞吐（见 §6） | 否（compute） | GB/s | STREAM / vkpeak copy |
| 2 | **N-body** | **算力（实际可达）** FP32 ALU + SFU + shared mem | 否（compute） | GFLOP/s, interactions/s | NVIDIA N-body, LuxMark |
| 3 | **Synthetic Peak** | **算力（理论峰值）** 按精度 | 否（compute） | GFLOPS(FP32/FP16/FP64) / GIOPS(INT32/INT8) / GB/s | **vkpeak**, CUDABurner |
| 4 | **Stress / Fractal** | **填充率 + 片元 ALU + 功耗/散热** | 是（graphics） | FPS, iterations/s, 降频稳定度 | **FurMark**, Mandelbulb, Superposition |

> （可选 #5：真 3D 粒子场景 = "拟真图形"轴，见 [`winui3-render3d-plan.md`](winui3-render3d-plan.md) A 节。
> 与 #4 区别：#4 是合成最大压力的填充/ALU 测试，#5 是拟真渲染管线测试。）

四轴齐全后，本项目从"compute 微基准"升级为**横跨 compute + 图形 + 压力测试的多维 GPU 跑分套件**，
且仍保留独有的**跨 5 个图形 API 同负载对比**这一稀缺定位。

---

## 1. `Workload` 枚举（canonical）

```cpp
// gpu_common.h
enum class Workload {
    Stream,        // #1 带宽（现有 compute.comp，保留）
    NBody,         // #2 算力实测（见 nbody-workload-plan.md）
    SynthPeak,     // #3 合成峰值（按精度）
    StressFractal, // #4 压力/分形（片元拉满）
};
```

`BenchmarkConfig` 增加 `Workload workload = Workload::Stream;` 及各负载专属参数。
`BenchmarkResult` 增加 `workload` 字段——**不同轴的分数分开存储、分开展示**，综合总分再加权。

---

## 2. 负载 #3：Synthetic Peak（合成峰值，按精度）—— 对标 vkpeak

### 2.1 目标
测各数据类型的**理论峰值吞吐**，不渲染。与 N-body（实际可达算力）互补，形成"峰值 vs 可达"对照。

### 2.2 设计
每个线程在**寄存器内**跑一个高度展开的 FMA 循环（用多个累加器隐藏延迟），
确保是 **ALU-bound 而非访存-bound**：

```
// 伪代码：FP32 峰值 kernel
float a0=seed0, a1=seed1, a2=seed2, a3=seed3;   // 多累加器隐藏 FMA 延迟
for (i = 0; i < ITER; ++i) {
    a0 = a0 * b + c;   // 每次 = 1 FMA = 2 FLOP
    a1 = a1 * b + c;
    a2 = a2 * b + c;
    a3 = a3 * b + c;
}
out[gid] = a0+a1+a2+a3;   // 写出，防止编译器把循环优化掉（dead-code elimination）
```

- 已知 FLOP 数 = `线程数 × ITER × 累加器数 × 2`，除以 compute 段时间 → **GFLOPS**。
- **精度变体**：FP32 / FP16 / FP64 / INT32 / INT8 各一份 kernel。
  - FP16 需 `VK_KHR_shader_float16_int8` / HLSL `min16float` / Metal `half` / GL 扩展。
  - FP64 需 `shaderFloat64`（移动端/部分卡不支持）。
  - **按能力门控**：不支持的精度报告 "N/A"，不算崩溃。
- **纯带宽 kernel**：大 buffer 的 copy / triad（`a[i] = b[i] + scalar*c[i]`）→ **GB/s**
  （比 Stream 的"粒子更新"更纯，作为带宽峰值参考；Stream 保留作"拟真流式更新"代表）。

### 2.3 接入（compute 路径，最轻）
- 复用现有 compute pipeline 脚手架；输出只需一个极小 buffer（写累加器防优化）。
- 每后端：加各精度的 compute shader + 能力查询门控。kernel 都很小，但"精度 × 5 后端"是主要工作量。
- 天然 headless，开发板/无显示器可跑。
- dispatch 规模可调（线程数 × ITER），用来把任意卡压满又不超时。

### 2.4 指标
`GFLOPS(FP32/FP16/FP64)`、`GIOPS(INT32/INT8)`、`GB/s(triad)`。按精度成表，和 vkpeak 同形态。

---

## 3. 负载 #4：Stress / Fractal（压力测试 / 分形）—— 对标 FurMark / Mandelbulb

> 这是你**最想做**的那类：片元着色器拉满，压**填充率 + 片元 ALU + 持续功耗/散热**。

### 3.1 目标
- 让 GPU 满载、长时间稳定运行 → 测**功耗墙 / 散热 / 降频行为**。
- 负载随**分辨率（填充）× 每像素迭代数（片元 ALU）**线性放大，可压满任意 GPU。

### 3.2 设计：全屏分形光线步进
- **几何极简**：一个全屏三角形（顶点着色器用 `gl_VertexIndex`/`SV_VertexID` 程序生成，**无顶点 buffer**）。
- **片元着色器是负载主体**：每像素跑重活，二选一（可都做，作为子模式）：
  - **Mandelbulb 光线步进**（3D 分形）：每像素 march N 步，每步算距离场，含大量 `pow`/三角/迭代 → 片元 ALU 极重。
  - **Mandelbrot/Julia 迭代**（2D，带缩放动画）：每像素迭代到 `maxIter`，经典 FurMark 式 ALU 压力。
- **参数（小 cbuffer/push）**：`time`（帧序号驱动，**不用墙钟**）、`maxIter`/`maxSteps`、`zoom`/相机、分辨率/超采倍数。
- **可选**：开 MSAA / 内部超采样进一步压填充与 ROP；alpha 混合制造 overdraw（FurMark 的毛发就是高 overdraw）。

### 3.3 接入（graphics 路径）
- 复用现有 swapchain/render target，但**新建一条图形管线**：全屏三角 VS + 重片元 FS。
  - **不用** SSBO、不用 compute、不用粒子顶点数据。
  - 只需一个小 params buffer。
- 每后端：加一个图形 pipeline + 一对 VS/FS。改动量中等（无需深度/实例化等复杂度）。
- **headless/离屏**：压力测试本质要渲染，但可渲染到离屏 target（不 present）跑持续压力，无需窗口——
  适合无人值守烤机。
- 跨平台：凡是图形可用处皆可跑。

### 3.4 指标
- **FPS**（固定分辨率 + 固定 maxIter）。
- **填充调整的 ALU 率**：`像素数 × maxIter × 帧数 / 秒`（iterations/s）。
- **稳定度/烤机**：持续跑 N 分钟，记录 **min FPS、FPS 随时间方差**（降频指示）。
- **（可选，平台相关）功耗/温度**：NVML（NVIDIA）/ ADL（AMD）读取——单独标注为可选、平台特定，
  不作为跨平台核心指标。

---

## 4. 套件评分

每个子项给一个**确定性、可跨卡/跨 API 直接比**的绝对指标（非 FPS 相对值）：

| 负载 | 公式 |
|------|------|
| Stream | `GB/s = (40 × N × steps) / computeSec / 1e9`（40 为估算；实测硬件流量约 64 B/粒子，见 §6） |
| N-body | `GFLOP/s = (N² × steps × FLOP_per_interaction) / computeSec / 1e9` |
| Synthetic Peak | `GFLOPS = (threads × ITER × accum × 2) / computeSec / 1e9`（每精度一份） |
| Stress/Fractal | `iter/s = (像素 × maxIter × frames) / renderSec`；另记 min-FPS / 降频方差 |

综合总分 = 各轴归一化后加权（权重可配置；对外发布时固定一套基线）。
**跨 API 公平性**：同负载、同参数、同 workgroup/分辨率，5 后端口径一致 → 苹果对苹果。

---

## 5. 推进顺序（负载维度）

1. **N-body**（见配套文档）—— compute 路径最轻，先验证算力计分。
2. **Stream → 带宽分** —— 加 `Workload` 枚举时顺手，几乎零成本。
3. **Synthetic Peak** —— 同 compute 路径，加各精度 kernel + 能力门控。先做 FP32，再补 FP16/FP64/INT。
4. **Stress / Fractal** —— graphics 路径，新建全屏分形管线（你最想做的，可与上面并行推进，互不依赖）。
5. （可选）真 3D 粒子场景（拟真图形轴）。
6. 综合评分公式接入。

> 依赖关系：#1#2#3 共用 compute 脚手架；#4 共用 graphics 脚手架，与 compute 子项**互不阻塞**，
> 可按你的兴趣优先做 Stress/Fractal。

---

## 6. Stream v1 字节数与"显存带宽"含义审计（2026-09-30）

> **用户决定（2026-09-30）**：为保持与全部历史成绩兼容，`stream_v1` 的 shader、
> `bytesPerParticle=40` 计分公式和默认参数**暂不修改**。本节只记录实测事实和使用限制。

### 6.1 结论

- `stream_v1` 的 GB/s 是"按每粒子 40 字节估算的粒子更新吞吐"，**不是显存带宽，也不是 L2 带宽**。
- **实际硬件流量约为每粒子 64 字节。** `compute.comp` 读取整个 32 字节粒子（position 与 velocity
  位于同一个 32 字节段），再写回 position.xyz，而写回也按整个 32 字节段计。RTX 5090 上，原 kernel
  与"整粒子读 32 + 写 32"对照 kernel 的耗时相差不到 1.5%（见 6.3），所以在这张卡上 v1 分数低估了
  37.5% 的实际流量，乘以 1.6 即可还原。
- **跨厂商不一致。** AMD 核显（RDNA2 iGPU）上，原 kernel 的耗时比整粒子读写多 35–43%。原 kernel 先写回
  xyz，越界时又单独写一次 x；这种零碎写回在该架构上代价更高，改用 `glslc -O` 重编后结果不变。因此
  v1 分数对不同厂商的显存效率并不公平，**不能把 ×1.6 统一套用到旧成绩**。
- **默认 1M 粒子（32 MiB）落在大容量缓存内。** 它装得进 RTX 5090 的 96 MB L2、RX 6900 XT 的
  128 MB Infinity Cache 和 RX 9070 XT 的 64 MB Infinity Cache。5090 上 1M 按 64 B 折合约 3.2 TB/s，
  超过显存理论值 1792 GB/s。只有 ≥ 16M（512 MiB）的结果才接近显存行为。
- 16M 时窗口模式与 headless 一致（0.798 / 0.793 ms），渲染不影响 compute 段计时。
- 同一张卡、同一个 kernel 在不同驱动下存在漂移（见 6.4）。

### 6.2 方法

- 测试使用 `build/Release` 的隔离副本，并把 `GPU_BENCH_DATA_DIR` 指向临时目录，不写入用户 History。
- 实验只替换副本中的 `compute.comp.spv`，对照 kernel 未入库。三个对照 kernel 与原 kernel 使用相同的
  binding、push constant 和 256 线程工作组：
  - `full`：`Particle p = particles[i]`，更新 position 并做越界回绕，再执行 `p.velocity.w += dt`
    （渲染不使用 w，这一步保证速度确实被写回），最后 `particles[i] = p`，即读 32 + 写 32 字节；
  - `read`：读取整个粒子，只在一个永远不成立的条件下写回，即读 32 字节；
  - `write`：不读取，直接写整个粒子，即写 32 字节。
- 命令：`gpu_benchmark --time 8 --backend vulkan --gpu <n> --particles <N> --no-renderdoc --headless`。
  下表取 CLI 汇总中的 Avg compute，即 2 秒 warmup 之后各计分帧 GPU timestamp 的平均值；GB/s 按
  1e9 字节计算。
- 第一轮测量时，用户同时在用 GPU 做视频补帧，该轮数据作废；下表全部是 GPU 空闲时的复测结果。

### 6.3 数据（Vulkan，headless，2026-09-30）

**NVIDIA GeForce RTX 5090**（驱动 616.92，显存理论值 1792 GB/s）

| N | 数据量 | 原 kernel | 按 40 B | 按 64 B | `full`（64 B） | `read`（32 B） | `write`（32 B） |
|---|---|---|---|---|---|---|---|
| 1M | 32 MiB | 0.021 ms | 1997 GB/s | 3196 GB/s（L2 驻留） | — | — | — |
| 16M | 512 MiB | 0.793 ms | 846 | 1354（76%） | 0.796 ms → 1349 | 0.357 ms → 1504（84%） | 0.387 ms → 1387（77%） |
| 32M | 1 GiB | 1.590 ms | 844 | 1351（75%） | 1.614 ms → 1331 | 0.721 ms → 1489（83%） | 0.779 ms → 1378（77%） |
| 64M | 2 GiB | 3.189 ms | 842 | 1347（75%） | 3.186 ms → 1348 | 1.429 ms → 1503（84%） | 1.542 ms → 1392（78%） |

**AMD Radeon(TM) Graphics**（Ryzen 7 9800X3D 核显，与 CPU 共用 DDR5-6400 双通道，理论 102.4 GB/s）

| N | 数据量 | 原 kernel | 按 40 B | 按 64 B | `full`（64 B） | `read`（32 B） | `write`（32 B） |
|---|---|---|---|---|---|---|---|
| 4M | 128 MiB | 5.362 ms | 31.3 GB/s | 50.1（49%） | 3.979 ms → 67.5（66%） | 2.217 ms → 60.5（59%） | 2.126 ms → 63.1（62%） |
| 16M | 512 MiB | 21.626 ms | 31.0 | 49.6（48%） | 15.148 ms → 70.9（69%） | 8.801 ms → 61.0（60%） | 8.506 ms → 63.1（62%） |

- 核显 16M 复核：产品 SPIR-V 为 22.635 ms，`glslc -O` 重编为 21.994 ms，`full` 为 15.225 ms。
- 核显只读反而低于读写混合。推测原因是该核显 CU 数很少，自身的并发访存不足以跑满内存，所以测到的是
  这块 GPU 实际能拉到的带宽，而不是 DRAM 上限。
- RTX 5090 窗口模式 16M：原 kernel 0.798 ms，`full` 0.788 ms。

### 6.4 同卡历史漂移（RTX 5090，16M，v1 kernel，取自历史结果库）

| 日期 | API | 驱动 | Avg compute | Min compute |
|---|---|---|---|---|
| 2026-03-25 | Vulkan | 591.86 | 0.628 ms | 0.553 ms |
| 2026-03-25 | DX12 | 591.86 | 0.581 ms | 0.558 ms |
| 2026-07-24 | DX11 | 610.47 | 0.677 ms | 0.574 ms |
| 2026-09-30 | Vulkan | 616.92 | 0.793 ms | 0.664 ms |

原因未排查。可能与驱动、后台 GPU 程序有关（2026-09-30 测试时 NVIDIA Broadcast 在运行），也可能与
功耗或频率设置有关。同一张卡做前后对比时，需要相同驱动和空闲的 GPU。

### 6.5 不改代码时的使用建议

- 在 History 和报告中，把 stream 的 GB/s 称为"粒子更新吞吐（v1，按 40 B 估算）"，不要称为显存带宽；
  也不要拿它做跨厂商的显存带宽比较。
- 需要接近显存行为时，用 ≥ 16M 粒子；1M 的结果只能作为缓存驻留吞吐的参考。
- GUI 中的"Particle — Memory Throughput"和 CLI 的"VRAM rate"文案仍沿用旧说法，这是已知的文案偏差。

### 6.6 后续方案（用户 2026-09-30 决定）

- **`stream_v2`（暂缓）**：五套 shader 统一改为整粒子读、整粒子写，每粒子流量确定为 64 B；使用新版本号，
  旧 v1 成绩单独成组；数据量 ≥ 512 MiB 时才标"显存"。
- **`membw_v1`（2026-10-05 已完成第一步：Vulkan 命令行版）**：独立的显存和缓存层级带宽测试，只做 compute，
  扫描数据量，按实际请求的字节数计算。用法和方法见 [`cli-reference.md`](cli-reference.md)，剩余步骤见
  [`TODO.md`](TODO.md)。RTX 5090 上测得显存读 1536 GB/s（理论值的 86%），与 6.3 节只读对照 kernel 的
  1503 GB/s 一致；L2 平台约 6.7 TB/s。
  方法参考以下项目：
  - [RRZE-HPC/gpu-benches](https://github.com/te42kyfo/gpu-benches)：GPL-3.0，只借鉴方法，不复制代码；
  - [clamchowder/Microbenchmarks](https://github.com/clamchowder/Microbenchmarks)：Apache-2.0；
  - [BabelStream](https://github.com/UoB-HPC/BabelStream)：自定义许可证，修改后的版本不得称为
    BabelStream 结果。

  外部对照用 [vkpeak](https://github.com/nihui/vkpeak) 的 `copy-d2d`：它用 CPU 计时，并且只按缓冲区
  大小计一次字节。可用的外部验证点是 gpu-benches 公布的 RX 6900 XT Infinity Cache 带宽，约 1.92 TB/s。
</content>
