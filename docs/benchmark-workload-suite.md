# Mangekyo GPU Benchmark 负载套件方案

> 本项目的跑分负载从"单一粒子流"扩展为**一套覆盖 GPU 四大性能轴的子项**。
> 每个子项都复用现有框架（`AppBase` / 计时 / 结果持久化），通过 `Workload` 枚举切换。
> N-body 的详细设计见配套文档 [`nbody-workload-plan.md`](nbody-workload-plan.md)。

---

## 0. 套件总览：四条 GPU 性能轴

| # | 负载 | 测什么轴 | 渲染? | 主指标 | 参考对标 |
|---|------|----------|-------|--------|----------|
| 1 | **Stream**（现有 `compute.comp`） | 显存/内存**带宽** | 否（compute） | GB/s | STREAM / vkpeak copy |
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
| Stream | `GB/s = (40 × N × steps) / computeSec / 1e9` |
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
</content>
