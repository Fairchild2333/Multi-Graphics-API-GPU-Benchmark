# TODO

## Metal 自动化捕获与分析（macOS）

### 前置条件
- [ ] 确认是否安装 Xcode（`.gputrace` 导出和可视化需要）
- [ ] 安装 MoltenVK（如需 macOS 上跑 Vulkan 后端）

### 实现步骤

#### 1. MTLCaptureManager 集成（C++ / ObjC）
- [ ] 在 `metal_backend.mm` 中添加 `MTLCaptureManager` 支持
- [ ] 复用现有 `--capture <seconds>` 参数，Metal 后端自动走 MTLCaptureManager
- [ ] 自动导出 `.gputrace` 到 `metal_captures/` 目录
- [ ] 捕获文件命名格式：`Metal_<GPU名>.gputrace`

#### 2. Timing JSON 导出（C++ / ObjC）
- [ ] 捕获帧时通过 `GPUStartTime` / `GPUEndTime` 采集 per-event timing
- [ ] 输出 `metal_timing_<GPU名>.json`，格式与 `rdoc_timing_*.json` 一致
- [ ] JSON 结构：events 数组（eventId, name, gpuDurationMs, category）+ summary

#### 3. Python 分析脚本
- [ ] 新建 `scripts/metal_export_timing.py` 或修改 `compare_rdoc_timing.py` 兼容 Metal JSON
- [ ] 对比 app 累计平均 timing vs 捕获帧精确 timing
- [ ] 输出偏差百分比 + verdict（<5% Excellent / <15% Good / >=15% Significant）

#### 4. 批量自动化
- [ ] `batch_benchmark.py` macOS 上自动加 `--capture` 参数
- [ ] 支持 `open *.gputrace` 自动在 Xcode 中可视化

### 备注
- 不装 Xcode 的情况下跳过 `.gputrace` 导出，只做 timing JSON
- Metal command buffer 自带时间戳，不需要外部工具解析

---

## Benchmark Report 待补充

### 数据采集
- [ ] 在 RX 6900 XT 上跑完所有测试，作为 AMD 基准线
- [ ] 所有 AMD GPU 跑完成绩（HD 5770 → RX 580 → Vega FE → RX 6600 XT → RX 6900 XT → iGPU）
- [ ] 3DMark Fire Strike + Time Spy 交叉验证数据
- [ ] RenderDoc 捕获分析数据

### 报告章节
- [ ] Section 4 — 3DMark 交叉验证表格填数据，计算 R²
- [ ] 新增 Section — AMD 代际架构分析（TeraScale 2 → GCN → GCN 5 → RDNA 2）
  - per-CU 性能对比
  - CU scaling 分析（6900 XT 80CU vs 6600 XT 32CU）
  - 内存带宽瓶颈对比（GDDR5 / HBM2 / GDDR6 / DDR5）
  - 跨 API 表现差异随架构变化趋势
- [ ] 所有成绩以 RX 6900 XT 为标杆（替换 RTX 5090）
- [ ] Workgroup Size 分析章节

---

## macOS 平台 API 兼容性

macOS 上只有两个后端可用，其余均无法原生转译：

| 后端 | macOS 可用 | 说明 |
|------|-----------|------|
| **Metal** | 可用 | 原生，macOS 首选 |
| **Vulkan** | 可用 | 通过 MoltenVK（Vulkan → Metal 转译） |
| **OpenGL 4.3** | 不可用 | macOS 最高支持 OpenGL 4.1（Apple Silicon 通过 Metal 模拟），缺 compute shader + SSBO |
| **DX12** | 不可用 | GPTK 仅用于 Wine 环境跑 Windows 游戏，不暴露 DX API 给原生应用 |
| **DX11** | 不可用 | DXVK（DX11 → Vulkan）仅 Linux 生态，macOS 无人维护 |

### 各平台可用后端汇总

| 平台 | 可用后端 |
|------|---------|
| Windows | Vulkan, DX12, DX11, OpenGL 4.3 |
| Linux | Vulkan, OpenGL 4.3 |
| macOS | Metal, Vulkan (MoltenVK) |

### OpenGL 4.1 备注
- macOS 上 OpenGL 最高 4.1（Intel Mac 原生驱动 / Apple Silicon Metal 模拟），自 2018 deprecated 但未移除
- macOS Tahoe 仍支持 OpenGL 4.1
- 4.1 缺少 `GL_ARB_compute_shader` 和 `GL_ARB_shader_storage_buffer_object`，无法运行现有 OpenGL 后端
- 理论转译路径（Mesa Zink: GL → Vulkan → MoltenVK → Metal）三层转译，性能损失大，无稳定支持

---

## OpenGL 4.1 后端支持（macOS 兼容）

- [ ] 使用 Transform Feedback 替代 Compute Shader 实现粒子物理计算
- [ ] 使用 Texture Buffer Object (TBO) 或 VBO 替代 SSBO
- [ ] Ping-pong buffer 方案：Buffer A → Vertex Shader 计算 → Buffer B（Transform Feedback 输出）→ 交替
- [ ] 预计帧率比 OpenGL 4.3 低 10-30%（图形管线固定功能开销）
- [ ] 可与未来 WebGL 2.0 后端复用 Transform Feedback 逻辑

---

## README 精简
- [ ] RenderDoc 详细用法替换为概述 + 链接到 `docs/renderdoc-capture-guide.md`
