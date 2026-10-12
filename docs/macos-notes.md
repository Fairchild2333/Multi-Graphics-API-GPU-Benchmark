# Mangekyo macOS Platform Notes

## 可复现构建与交付合同

默认的 macOS 开发交付入口是：

```bash
./scripts/build-macos.sh
```

脚本同时构建 CMake CLI/静态引擎与 SwiftUI App，并把最终产物放到：

```text
out/macos/<arch>/<Configuration>/GPUBenchmark.app
```

默认产物为单架构、**Metal-only** 的本地测试 App：

- `gpu_benchmark` 与三个 Metal shader 位于
  `Contents/Helpers/`，GUI 优先运行这个 bundle 内 worker；
- GLFW 3.4 由 CMake 通过固定 URL 与 SHA-256 从源码静态构建，继承
  `CMAKE_OSX_DEPLOYMENT_TARGET=12.0`；
- 不捆绑 Homebrew 的 GLFW、`vulkan-loader` 或 MoltenVK dylib；
- Mach-O 检查拒绝残留 `/opt/homebrew`、`/usr/local` 等非系统动态依赖；
- GUI 和 worker 都检查 `LC_BUILD_VERSION minos=12.0`；
- 完成 ad-hoc（或用户指定 identity）签名，并在临时工作目录运行 bundle 内
  CLI 的 `--help` 与 Metal 设备枚举 smoke。

因此正常构建不依赖固定 Homebrew 前缀，也不要求 App 位于仓库内或手动
选择仓库工作目录。结果数据走：

```text
~/Library/Application Support/GpuComputeBenchmark/
```

首次 CMake configure 需要下载 GLFW；离线构建可设置
`GPU_BENCH_GLFW_SOURCE_DIR` 指向已有 GLFW 3.4 源码目录。XcodeGen 仅在显式
传 `--regenerate-project` 时需要。

这仍不是公开发行流水线：目前没有 universal binary、Developer ID
发布签名、notarization、DMG/PKG 或 App Store archive。新 SDK 编出的
`minos=12.0` 也不等价于 Monterey 真机验收，发布前仍须在 macOS 12 上完成
启动、GUI、worker 与至少一条短时 Metal smoke。

### 2026-07-31 当前开发机验证

Apple M4 Pro / Xcode 26.6 / macOS SDK 26.5 上已验证：

- canonical Release 脚本成功，GUI 与 worker 均为 arm64、`minos 12.0`；
- `otool -L` 无 Homebrew/其他非系统依赖，Charts 为 weak framework；
- deep codesign 校验通过；
- App 在隔离临时工作目录启动并稳定存活到 smoke 主动结束；
- bundle 内 worker 以 65,536 粒子跑 1 秒 `stream`，成功写出真实约
  8.4 MB `.gputrace`，成绩/抓帧均写入 `/private/tmp` 隔离目录而非仓库。
- packaged helper 的正式参数 `stream`（1,048,576 粒子、15 秒）也已完成，
  并写出真实原生抓帧。
- packaged helper 的 15 秒 `gpu_burn` 与 6 秒 `cinematic_liquid` preview
  验收均已完成；最终 `.app` 也已脱离仓库工作目录独立打开并正常退出。

以上只证明当前开发机链路；完整正式矩阵与 macOS 12 真机验收仍开放。

## OS 版本底线

| 组件 | 最低系统 | 说明 |
|------|----------|------|
| **CLI / `gpu_engine`（CMake）** | **macOS 12.0** | `CMAKE_OSX_DEPLOYMENT_TARGET=12.0` |
| **SwiftUI GUI** | **macOS 12.0** | `MACOSX_DEPLOYMENT_TARGET` / `LSMinimumSystemVersion=12.0` |
| **UI 外壳** | 12：`NavigationView`；13+：`NavigationSplitView` | 功能对齐，布局略有差异 |
| **Liquid Glass** | 仅 26+ | 12–15 使用 Material 回退 |
| **构建机** | 建议 Xcode 26 / 较新 SDK | 在新系统上编译、部署到 Monterey 运行 |

macOS 11 及更旧系统不在支持范围。

## Graphics API Availability

| Backend | macOS Support | Details |
|---------|--------------|---------|
| **Metal** | Native | macOS 首选，Apple/AMD/Intel GPU 均支持 |
| **Vulkan** | MoltenVK 转译 | Vulkan → Metal 转译层，需安装 MoltenVK |
| **OpenGL 4.3** | 不可用 | macOS 最高支持 OpenGL 4.1，缺 compute shader + SSBO |
| **OpenGL 4.1** | 可用但功能受限 | Apple Silicon 通过 Metal 模拟；Intel Mac 原生驱动；自 2018 deprecated 但 macOS Tahoe 仍保留 |
| **DX12** | 不可用 | GPTK 仅用于 Wine 环境跑 Windows 游戏，不暴露原生 DX API |
| **DX11** | 不可用 | DXVK（DX11 → Vulkan）仅 Linux 生态，macOS 无维护 |

### 为什么 OpenGL 4.3 在 macOS 上不可行

macOS 上 OpenGL 止步于 4.1，而本项目的 OpenGL 后端依赖两个 4.3 特性：

- `GL_ARB_compute_shader` — 粒子物理计算
- `GL_ARB_shader_storage_buffer_object` — compute 与 render 间数据共享

版本号只差 0.2，但对 GPU compute 场景是能跑与不能跑的区别。

### 转译路径分析

| 路径 | 可行性 | 说明 |
|------|--------|------|
| Apple 官方升级 OpenGL | 不可能 | 2018 deprecated，只维护不升级 |
| MoltenGL（GL → Metal） | 商业收费 | 只支持 OpenGL ES，不支持桌面 GL 4.3 |
| Mesa Zink（GL → Vulkan → MoltenVK → Metal） | 理论可行 | 三层转译，Linux 上 Zink 可跑 GL 4.6，但 macOS 无人维护，性能损失大 |
| DX → Metal | 不暴露原生 API | GPTK 仅限 Wine 环境 |
| DXVK（DX11 → Vulkan → MoltenVK） | 理论可行 | 无人维护 macOS 支持 |

## macOS 图形架构

### 驱动模型

macOS 与 Windows/Linux 的驱动模型完全不同：

| | Windows / Linux | macOS |
|--|-----------------|-------|
| GPU 驱动来源 | GPU 厂商独立提供（NVIDIA/AMD/Intel） | Apple 统一提供，集成在系统中 |
| 图形 API 路径 | 应用 → OpenGL/Vulkan/DX → 厂商驱动 → GPU | 应用 → Metal → macOS 内置驱动 → GPU |
| 驱动更新 | 厂商单独发布 | 随 macOS 系统更新 |

### 为什么 macOS 不需要软件光栅器

每台 Mac 都有硬件 GPU：

- **Apple Silicon** — GPU 集成在 SoC 中，与 CPU 同一芯片
- **Intel Mac** — 至少有 Intel HD/Iris 集显，部分机型有独显（如 Mac Pro 2013 的双 FirePro D700）

WindowServer（macOS 窗口合成器）直接通过 Metal 渲染桌面，不存在纯 CPU 回退路径。

### 虚拟机中的软件渲染

在虚拟机中运行 macOS 时（无 GPU 直通），观察到：

- 所有半透明/模糊效果消失
- 动画明显卡顿
- CPU 占用显著升高

这是 WindowServer 的内部回退机制：

- 检测不到支持的 GPU → 自动降级为 CPU 软件渲染
- 此回退路径**不暴露 Metal API**，应用无法通过 Metal 调用
- `glGetString(GL_RENDERER)` 返回 "Apple Software Renderer"，无 compute shader 支持
- 本项目在虚拟机中**无法运行任何后端**（Metal 初始化失败，OpenGL 为无加速的软件渲染器）

唯一例外是虚拟机提供 GPU 直通（VMware 3D 加速 / Parallels Metal 虚拟化），但本质上仍在使用宿主机 GPU。

## 软件渲染器对比

| 软件渲染器 | 平台 | 类型 | macOS 可用 |
|-----------|------|------|-----------|
| **WARP** | Windows | DX11/DX12 CPU 光栅器 | 不可用 |
| **Mesa llvmpipe** | Linux | OpenGL CPU 光栅器（LLVM JIT） | 不可用 |
| **SwiftShader** | 跨平台 | Vulkan CPU 实现（Google） | 理论可行但非官方支持 |
| **macOS WindowServer 回退** | macOS（虚拟机） | 内部 CPU 渲染 | 不暴露 API，应用不可用 |

结论：macOS 上没有实用的软件渲染器方案，WARP 基线测试只能在 Windows 上完成。

## ProMotion VSync 锁定问题

### 问题

macOS ProMotion 显示器（120Hz）会在运行几秒后将帧率锁定在 120 FPS，即使设置了 `CAMetalLayer.displaySyncEnabled = NO`。这是 macOS 的已知行为——`nextDrawable` 会阻塞等待显示器释放 drawable。

### 解决方案

当 **未开 VSync**（含正式 15 秒 time-mode 与 `--benchmark`）时，Metal 后端创建离屏纹理（`MTLTexture`）作为渲染目标，大部分帧渲染到离屏纹理，每 60 帧才通过 `nextDrawable` 获取一次 drawable present 到屏幕。这样 GPU 不被显示器刷新率锁定，窗口仍有视觉反馈。开 VSync 时仍每帧 present（按刷新率测）。

粒子缓冲使用 `MTLResourceStorageModeShared`，成绩 JSON 的 `memory` 记为 `Unified-memory`（不是离散 VRAM）。`--capture` / F12 / `--capture-frame` 走 `MTLCaptureManager` 写出 `.gputrace`；失败时必须写明 `captureUnavailable`，不得假装 RenderDoc 成功。

SwiftUI bundle 的 `Info.plist` 必须带 `MetalCaptureEnabled=YES`。GUI 启动
`Contents/Helpers/gpu_benchmark` 时也设置 `MTL_CAPTURE_ENABLED=1`，覆盖
macOS 14+ 的外部 worker 路径；macOS 12/13 仍以 bundle Info key 为基础。
验收必须检查真实 `.gputrace` 文件，而不是只检查退出码或成绩存在。

## 三主项 + Metal 状态（2026-07-31）

| 主项 | Windows | Metal / macOS | 还差 |
|------|---------|---------------|------|
| **Particle (`stream`)** | 正式可用 | M4 Pro 15s + `.gputrace` 已通过 | macOS 12 真机验收 |
| **GPU Burn (`gpu_burn`)** | 正式可用（Vulkan/DX*/GL） | M4 Pro packaged helper 15s 验收已通过（**不再 unsupported**） | macOS 12 真机验收 |
| **Cinematic Liquid** | Vulkan v8 场景在、**无 v8 正式成绩**；SPH=`_preview` | MLS-MPM + raymarch present；M4 Pro packaged helper 6s smoke 已通过；成绩强制 `…_metal_preview` | 正式合同；macOS 12 真机；SPH/v1 未移植 |

验收命令（真 Mac）：

```bash
gpu_benchmark --backend metal --workload stream --particles 1048576 --time 15
gpu_benchmark --backend metal --workload gpu_burn --time 15 --iter 16
gpu_benchmark --backend metal --workload cinematic_liquid --time 6   # preview only
```

SwiftUI（`macos-gui/`）已有 Run/CPU/History/Charts/Settings/About、
Complete Suite、Fill Missing、Duration、Capture、API 多选与英/中/日 UI。
这代表工作流对齐，不代表平台能力完全相同：DX/WARP/AFR/SFR/RenderDoc
仍为 Windows 专属。液体 Metal 必须标 preview，不得混入 Vulkan v8 榜。

### 优化措施

- **dispatch_semaphore** 替代 `waitUntilCompleted`，CPU 不阻塞等待 GPU 完成
- **addCompletedHandler** 异步回调采集 GPU timing
- **Triple buffering**（3 帧 in-flight）替代原来的 double buffering，GPU 管线更充分

## Apple Silicon TBDR 架构对点云渲染的影响

### 架构差异

| | AMD/NVIDIA（Immediate-mode） | Apple Silicon（TBDR） |
|--|---|---|
| 渲染流程 | 顶点直接光栅化 | 先 tile binning，再逐 tile 渲染 |
| 点云开销 | 低——直接光栅化 | 高——1M 个分散的点需要大量 tile binning |
| 优势场景 | 大量小图元 | 复杂场景减少 overdraw |

### 实测表现（1280×720）

**1M 粒子（Medium）：**

| GPU | 架构 | FP32 TFLOPS | Compute | Render | Total GPU | FPS |
|-----|------|------------|---------|--------|-----------|-----|
| M4 Pro 16-core | TBDR | 7.4 | 0.608ms | 1.890ms | 2.503ms | 765 |
| RX 580 | GCN 4 (Immediate) | 6.2 | — | — | 1.070ms | 783 |
| RX 6600 XT | RDNA 2 (Immediate) | 10.6 | — | — | 0.649ms | 1,239 |

**65K 粒子（Light）：**

| GPU | 架构 | Compute | Render | Total GPU | FPS |
|-----|------|---------|--------|-----------|-----|
| M4 Pro 16-core | TBDR | 0.023ms | 0.096ms | 0.145ms | 9,599 |

**Scaling（M4 Pro，65K → 1M = 16× 粒子）：**

| 指标 | 65K | 1M | 倍数 |
|------|-----|-----|------|
| Compute | 0.023ms | 0.608ms | 26.4× |
| Render | 0.096ms | 1.890ms | 19.7× |
| Total GPU | 0.145ms | 2.503ms | 17.3× |

Compute scaling 超线性（26.4× vs 16×），可能与缓存压力和内存带宽相关。Render scaling 也超线性（19.7×），印证 TBDR tile binning 开销与粒子数量非线性增长。

M4 Pro 的理论算力比 RX 580 高 20%（7.4 vs 6.2 TFLOPS），但 1M 粒子实测 FPS 接近。原因：

- **Compute 性能正常**（0.608ms），与 TFLOPS 比例一致
- **Render 是瓶颈**（1.890ms，比 compute 贵 3.1×），TBDR tile binning 1M 个分散点开销大
- M4 Pro 通过 triple buffering + 异步提交补回了部分 FPS

### 结论

Apple Silicon 在 GPU compute 上性能合理，但点云/粒子渲染（大量分散小图元）是 TBDR 架构的弱势场景。这不是 Metal API 或代码的问题，而是 GPU 架构特性。

## 编译注意事项

### CMake / GLFW

主工程先声明 C/C++，在 Apple source-build GLFW 与 Metal 路径中再显式
`enable_language(OBJC)` / `enable_language(OBJCXX)`。macOS 默认
`GPU_BENCH_BUILD_GLFW_FROM_SOURCE=ON`，避免 `find_package(glfw3)` 误取一个
最低系统版本高于 Monterey 的 Homebrew bottle。交付脚本另行固定：

```text
ENABLE_METAL=ON
ENABLE_VULKAN=OFF
ENABLE_OPENGL=OFF
GPU_BENCH_BUILD_GLFW_FROM_SOURCE=ON
```

若开发者关闭 source-build 开关并使用外部 GLFW，必须自行检查该库的架构与
`LC_BUILD_VERSION`；“能在当前 Mac 链接”不能证明它能在 macOS 12 加载。

### RenderDoc

RenderDoc 不支持 macOS。代码中 RenderDoc 相关逻辑需用 `#if defined(_WIN32) || defined(__linux__)` 包裹，否则 macOS 编译报错（`RENDERDOC_GetAPI` undeclared）。

### 可选 Python 报告脚本

macOS 新版（Homebrew Python 3.12+）默认启用 PEP 668 外部管理环境，`pip install` 会报错。解决方案：

```bash
# 方案1：加 --break-system-packages
pip3 install jinja2 --break-system-packages

# 方案2：用虚拟环境
python3 -m venv venv && source venv/bin/activate && pip install jinja2
```

原生 macOS Charts 页面不依赖 Python：macOS 13+ 使用 Swift Charts，Monterey
使用 SwiftUI bar fallback。上面的 Python 环境仅适用于仓库里的可选报告/
批处理脚本。

## 测试环境

### MacBook Pro (2024) — Apple Silicon

| 项目 | 值 |
|------|---|
| 机型 | MacBook Pro (2024) |
| CPU | Apple M4 Pro |
| GPU | Apple M4 Pro 16-core GPU（7.4 TFLOPS FP32, 273 GB/s 统一内存） |
| macOS | Tahoe 26.4 |
| 可用后端 | Metal |
| 编译器 | Apple Clang 21.0.0 |
| 显示器 | ProMotion 120Hz（需离屏纹理绕过 VSync 锁定） |

### Mac Pro (Late 2013) — Intel

| 项目 | 值 |
|------|---|
| 机型 | Mac Pro (Late 2013) |
| CPU | Intel Xeon E5 |
| GPU | AMD FirePro D700 × 2（GCN 1.0, 6GB VRAM each） |
| macOS | Tahoe |
| 可用后端 | Metal |
| 编译器 | Apple Clang 21.0.0 |
