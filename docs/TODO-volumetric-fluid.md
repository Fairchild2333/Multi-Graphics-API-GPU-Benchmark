# TODO — Volumetric / Fluid / 跨平台扩展（2026-06-30 会话进度）

> 本文件记录 2026-06-30 会话完成的工作 + 剩余事项，包括向"跨平台 3D/GPU 跑分（含移动端 + WebGPU）"目标推进的下一步。
> 与 `TODO.md`（Metal 捕获 / 报告 / macOS 兼容性）正交，不冲突。

---

## ✅ 已完成（本会话）

### 1. Volumetric 体积渲染负载（5 个后端完整接入）

全屏光线步进程序化 3D 噪声场（fbm + domain warp），固定步数积分密度，评分 `GSample/s = pixels × steps / renderSec`。

- 4 套 shader：`shaders/volumetric.vert/frag`、`volumetric_gl.vert/frag`、`volumetric.hlsl`、`particle.metal` 追加 `volumetricVertex/volumetricFragment`
- 5 个后端全部接入：Vulkan（参数化 `CreateFullscreenPipeline`）、DX12、DX11、OpenGL、Metal
- `gpu_common.h` 加 `Workload::Volumetric`、`VolumetricParams`、`kVolumetricDefaultSteps=96`
- `main.cpp` 加 `--workload volumetric`、`--steps N`
- `CMakeLists.txt` 4 处 shader 列表更新

实测：RTX 5090 @ 1280×720 × 96 steps = 33.76 GSample/s（68% GPU 利用率，偏轻）；AMD 集显 @ 32 steps = 168ms/帧（~100% 利用率，跨 GPU 64 倍区分度）。

### 2. 热稳定 / 降频检测（P0，全后端共享）

每 1 秒窗口计算轴分数推入 30 个滚动队列；连续 5 窗口 CV < 2% 判定"热稳定"记录 `stableScore`；最早 5 vs 最新 5 均值差 > 5% 判定"持续降频"。

- `app_base.h` 加 `windowScores_` / `stableScore_` / `stableVariancePct_` / `thermalStable_` / `throttlePct_` 成员 + `computeAxisScore` / `recordWindowSample` 方法
- `app_base.cpp` `ReportTimingIfDue` 每窗口调 `recordWindowSample`；`PrintSummary` 加 "Thermal Stability" 块；`CollectResult` 填三个新字段
- `benchmark_results.h/cpp` 加 `stableScore` / `stableVariancePct` / `throttlePct` 字段 + JSON 序列化
- 阈值：5 窗口 CV < 2% = stable；10+ 窗口时算 throttle = (early5Mean - late5Mean) / early5Mean × 100%

实测：RTX 5090 volumetric @ 128 steps × 12s → `Stable score: 33.78 GSample/s (CV 0.09%)` + `>> Thermally stable`；fluid @ 1024² × 8s → `Stable score: 262.92 GCell/s (CV 0.20%)`。

### 3. 2D Eulerian 流体负载（P1，仅 Vulkan 后端）

Stam 1999 stable fluids：advect（半拉格朗日）→ divergence → N×Jacobi 压力求解 → gradient subtract → fullscreen dye 渲染。评分 `GCell/s = gridSize² × (4 + jacobiIters) / computeSec`。

- 6 个新 shader：`fluid_advect.comp` / `fluid_divergence.comp` / `fluid_jacobi.comp` / `fluid_subtract.comp` / `fluid_render.vert/frag`
- `gpu_common.h` 加 `Workload::Fluid`、`FluidParams`、`FluidRenderParams`、`kFluidDefaultGridSize=256`、`kFluidDefaultJacobiIters=30`
- `vulkan_backend.h/cpp` 加 `FluidResources` 隔离结构 + `CreateFluidResources` / `CleanupFluidResources` / `RecordFluidFrame`（~450 行），完全独立 descriptor set / pipeline / buffer，不污染其他 workload
- `app_base.cpp` 加流体评分公式 + summary + 热稳定单位
- `main.cpp` 加 `--workload fluid`、`--grid N`、`--jacobi N`
- `CMakeLists.txt` 加 6 个流体 shader 编译

实测：RTX 5090 @ 1024² × 80 Jacobi = 264.93 GCell/s（51% 利用率）；AMD 集显 @ 256² × 30 = 2.13 GCell/s（42% 利用率，跨 GPU 15 倍区分度）。

---

## 🚧 待办（按优先级）

### P0 — 流体负载扩展到其他 4 个后端

Vulkan 后端已验证，但 DX12/DX11/OpenGL/Metal 还没接。每个后端按同样模式：4 个 compute pipeline + 1 个 render pipeline + ping-pong buffer + descriptor rebind。

- [ ] **DX12 后端**（优先级最高，Windows 主力）
  - 4 个 root signature（或共享一个 5-slot root sig）
  - PSO per pass
  - 用 `ResourceBarrier` 替代 Vulkan 的 `vkCmdPipelineBarrier`
  - 注意 UAV barrier 在 Jacobi ping-pong 间必需
- [ ] **OpenGL 4.3 后端**
  - 4 个 compute program + 1 个 render program
  - SSBO binding 用 `glBindBufferBase`
  - `glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT)` 在 pass 间
- [ ] **Metal 后端**（macOS）
  - 4 个 compute PSO + 1 个 render PSO
  - `MTLBuffer` ping-pong
  - 用 `blitEncoder` 或 `encoder.memoryBarrier` 在 pass 间
- [ ] **DX11 后端**
  - 4 个 compute shader + SRV/UAV
  - UAV barrier 用 `ID3D11UnorderedAccessView` counter
  - 注意 DX11 timestamp 在 headless 不解析（已知限制）

每个后端约 200-300 行 C++，参考 `vulkan_backend.cpp` 的 `CreateFluidResources` / `RecordFluidFrame` 实现。

### P1 — 综合评分公式

当前每个 workload 单独打分，但跨 workload 综合分还没接入。

- [ ] 定义综合分公式：`total = w1×bandwidth + w2×compute + w3×fill + w4×peak + w5×render3d + w6×volumetric + w7×fluid`
- [ ] 权重可配置，对外发布时固定一套基线
- [ ] 跨轴归一化（每个轴用旗舰卡分数作 100 分基准）
- [ ] `--full-analysis` 跑所有 workload 后输出综合分

### P2 — WebGPU 后端（跨平台 + 移动端关键）

- [ ] 调研 WebGPU timestamp query 精度限制（浏览器统一降到 100μs，防 Spectre）
- [ ] 实现"批量跑 N 帧累计 GPU 时间"对策（单帧 timestamp 不准）
- [ ] WebGPU compute pipeline + storage buffer 替代 SSBO
- [ ] WebGPU render pipeline + storage buffer in fragment（需要 storage binding）
- [ ] 7 个 workload 全部适配（volumetric 最容易，fluid 最难因多 pass）

### P3 — 移动端适配

- [ ] **Android（Vulkan）**：参照 `ohos/` 端口，用 `ANativeWindow` + `VK_KHR_android_surface`
- [ ] **iOS（Metal）**：`CAMetalLayer` + `MTKView`
- [ ] **HarmonyOS**：`ohos/` 已有 Vulkan 端口，验证 fluid/volumetric 能跑
- [ ] 移动端默认参数调优：volumetric steps 降到 48，fluid grid 降到 128²，Jacobi 降到 20

### P4 — 更复杂的 3DMark 风格负载（"真实 3D"轴）

当前 render3d 只是 instanced billboard，volumetric 是程序化噪声场。要做真正的"3DMark 对标"还需要：

- [ ] **`scene3d` 负载**：真实网格场景渲染
  - 资产：Sponza 或 Bistro 子集（10-50 万三角形），glTF 或自定义二进制格式
  - Forward rendering，1-3 个动态光，1 张 2K shadow map
  - 压：顶点吞吐 + 三角形 setup + 纹理采样带宽 + 深度复杂度
  - 评分 `MTri/s = triangles × frames / renderSec / 1e6`
  - 跨平台难点：纹理压缩格式（BC7 / ASTC / ETC2）
  - 工作量 2-3 周，风险最高但对标 3DMark 价值最大

- [ ] **`fluid3d` 负载**：3D Eulerian 流体（可选，比 2D 复杂 10 倍）
  - 3D 纹理 + 3D advection + 3D Jacobi
  - 移动端 fill rate 爆炸，可能只跑桌面
  - 评分 `GCell/s = gridSize³ × (4 + jacobiIters) / computeSec`

- [ ] **光线追踪负载**（可选，看 GPU 覆盖度）
  - `VK_KHR_ray_tracing_pipeline` / Metal ray tracing / DXR
  - Apple M3+ / Adreno 740+ / RTX / RDNA3 支持
  - 简单 ray-box 场景，评分 MRays/s
  - WebGPU 无 RT 支持，跳过

### P5 — WorkloadShape 抽象层（架构改造，🚧 进行中）

> **设计决策（已定稿）**：放弃"完全抽象资源/命令录制"的方案——5 个后端的资源类型
> （`VkBuffer` / `ComPtr<ID3D12Resource>` / `GLuint` / `id<MTLBuffer>`）、绑定模型、barrier
> 语义是根本性不同的，强行抽象会变成最低公分母或漏掉平台细节。
>
> **改为最小侵入式 shape 分类**：把"workload 是什么算法"（`Workload` 枚举，决定 shader/
> push constant/评分）和"workload 用什么管线形状"（`WorkloadShape` 枚举，决定后端如何建
> pipeline/录命令）分离。后端从 `switch(workload)` 改成 `switch(shapeOfWorkload(workload))`，
> 新增同形状的 workload 时后端代码不用改。

四种 shape：
- `ParticleCompute`：1 compute dispatch + 点 sprite render（stream/nbody/synthpeak）
- `FullscreenTriangle`：1 全屏三角 + 重 fragment，无 compute（stress/volumetric）
- `InstancedBillboard`：1 compute + instanced quad render with depth（render3d）
- `MultiPassCompute`：N compute pass ping-pong + 全屏 render（fluid）

#### ✅ 已完成
- [x] `gpu_common.h` 加 `WorkloadShape` 枚举 + `shapeOfWorkload()` 映射（单一真相源）
- [x] 加 `isFragmentOnlyWorkload()` / `needsDepthAttachment()` 语义 helper
- [x] 编译验证通过（inline helper 已定义，暂未被后端调用）

#### 🚧 待办
- [ ] **重构 Vulkan 后端**：`InitBackend` / `RecordCommandBuffer` / `CleanupBackend` 里的
      `if (workload == StressFractal || workload == Volumetric)` 等条件改成
      `if (shapeOfWorkload(config_.workload) == WorkloadShape::FullscreenTriangle)`，
      或直接用 `isFragmentOnlyWorkload()` / `needsDepthAttachment()` helper
- [ ] **重构 DX12 后端**：同上（`CreateRootSignatures` / `CreatePipelineStates` / DrawFrame）
- [ ] **重构 DX11 后端**：同上
- [ ] **重构 OpenGL 后端**：同上（`CreateShaders` / `DrawFrame`）
- [ ] **重构 Metal 后端**：同上
- [ ] 抽出 `workloadComputeShaderFile()` / `workloadRenderShaderFile()` helper，把散落在
      5 个后端的 "if fractal else if volumetric else ..." shader 文件名选择集中到一处
- [ ] 验证 7 个 workload × 各后端无回归

> **现实评估**：现有代码其实已经按 shape 组织了（只是用 workload 名做条件），所以这次重构
> 主要是"让意图显式化 + 集中 shader 文件名映射"，不会大幅减少代码量。真正的收益在加
> `scene3d` 时显现：如果它复用 `InstancedBillboard` 或新增一个 `ForwardScene` shape，
> 后端改动可控。
>
> **关键原则**：不抽象资源类型、不抽象命令录制，只抽象"形状选择 + shader 文件名映射"。

### P5b — （未来）若 shape 数量增长，再考虑模板化
- [ ] 如果 shape 超过 6-7 种，考虑 `WorkloadBackend<Shape>` 模板特化
- [ ] 但在那之前，简单的 `switch(shape)` 足够，不要过度工程化

### P6 — 已知小问题

- [ ] 流体 `fluid_.simTime` 用 `deltaTime * 60` 估算帧号做 ping-pong，不严格；改成真实帧计数器
- [ ] 流体 render pass 的 swapchain 同步关系没在 RenderDoc 下长时间验证
- [ ] 流体初始只有 dye blob，可加动态扰动注入（鼠标 / 程序化扰动源）
- [ ] RTX 5090 上 1024²+80 Jacobi 也只到 51% 利用率，旗舰卡需要更大网格（2048²？）或更多 Jacobi
- [ ] 旗舰卡上 volumetric 128 steps 也只到 ~70% 利用率，可考虑加 `--steps 256` 默认值

---

## 📊 当前 7 个 workload 状态总览

| Workload | Vulkan | DX12 | DX11 | OpenGL | Metal | 评分单位 | 跨 GPU 区分度 |
|----------|:------:|:----:|:----:|:------:|:-----:|---------|--------------|
| stream | ✅ | ✅ | ✅ | ✅ | ✅ | GB/s | 中 |
| nbody | ✅ | ✅ | ✅ | ✅ | ✅ | GFLOP/s | 中 |
| stress | ✅ | ✅ | ✅ | ✅ | ✅ | G-iter/s | 高 |
| synthpeak | ✅ | ✅ | ✅ | ✅ | ✅ | GFLOPS/GIOPS | 高 |
| render3d | ✅ | ✅ | ✅ | ✅ | ✅ | MQuad/s | 中 |
| **volumetric** | ✅ | ✅ | ✅ | ✅ | ✅ | GSample/s | 极高（64×） |
| **fluid** | ✅ | ❌ | ❌ | ❌ | ❌ | GCell/s | 高（15×） |

热稳定 / 降频检测：✅ 全 workload 共享
