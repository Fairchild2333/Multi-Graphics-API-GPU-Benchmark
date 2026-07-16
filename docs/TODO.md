# Mangekyo TODO

## 当前产品主线（2026-07-15）

> 当前事实、验证记录与交接顺序以根目录 [`HANDOFF.md`](../HANDOFF.md) 为准；下面旧专题内容只保留历史上下文。

> **当前执行边界**：先完善现有 Stream/Particle、GPU Burn、Cinematic Liquid v2、GUI 同步、固定 `15s + 第 5 秒抓帧` 与结果合同。本轮新增的 RT、路径追踪、DLSS/FSR/XeSS/MetalFX 仅记录可行性；在现有测试完成验收且用户再次明确提优先级前，不开始实现。

- [x] 新主测试 `gpu_burn_v1`：原创 Plasma Bloom（实心、无甜甜圈孔洞、非粒子），Vulkan/DX12/DX11/OpenGL + WARP、WinUI、结果/图表与发布资产已接入。
- [x] RTX 5090 自动标定实测稳定段 NVML 99%，约 599–600W；默认仍是 15 秒 Burst + 第 5 秒 RenderDoc，不宣称长时热稳定或错误检测。
- [x] 低强度 9 项设备/API 矩阵、包内 RenderDoc 抓帧和 staged WinUI 启动通过；固定未探测步数限制为 16–32，推荐留空自动标定。
- [x] `cinematic_liquid_v1`：独立于旧 2D `fluid`，已完成 181,216 粒子 3D MLS-MPM、96x56x64 密度体积、160-step 折射自由表面 raymarch、球体碰撞、WinUI/结果接入；RTX 5090 正式 15 秒 + 第 5.1 秒 `.rdc` 通过，成绩 `288.74 MParticle-step/s`。
- [x] **历史 P0 — Cinematic Liquid v2 physical-scene v7 Vulkan 切片**：固定 128x64x96 MLS-MPM 网格、10 substeps 与 **320,920 粒子**（142x14x98 浅水底 + 48x37x71 溃坝体）；固定参数为 stiffness 45,000、viscosity 0.035、maxSpeed 8。历史身份为 `workloadVersion=cinematic_liquid_v2_physical_scene_v7`、`shaderVersion=9`、`sceneVersion=4`。用户调整后的母鸭、3 只小鸭以及 7 刚体 index ABI 均保留：船仍为 index 2、沉球仍为 index 3、小鸭仍追加在 4-6。
- [x] **当前合同隔离 — physical-scene v8**：共享池体/光学参数后来改为 `poolWallInset=0.45`、`poolWallTopFraction=0.42`、`extinction=(12,3.6,2.5)`，已将当前 MLS-MPM 结果身份升为 `cinematic_liquid_v2_physical_scene_v8`、`sceneVersion=5`，不得与 v7 smoke 或任何更早版本混排；v8 尚无正式成绩。
- [x] **P0 — 船与沉球物理修正**：船由硬锚定改为 34 kg 有限质量刚体、软系泊，并允许推进器反冲驱动/摇摆；目前只确认实现，**尚未视觉验收其运动轨迹**。index 3 水密实心球保持 1.06 水密度比，4.28 秒释放，重力 -9.81、空气阻尼 0.015；材料水阻按 displaced mass 与浸没率门控，不再把空气阶段错误地当成水下运动。
- [x] **P0 — 越沿流体与自适应表面**：当前池壁是有限高度的内嵌碰撞壁（inset 0.45、wall-top fraction 0.42），外层 simulation catch band 允许粒子真实越沿后落到地面；catch band 带宽有限，**不是无限流体域**。5x5x5 binomial 重建改为自适应保留低支持喷滴。沉球入水 crown/whitewater 由真实 GPU 刚体状态与局部流体产生，不是 fragment 假水花；尚无独立 secondary-spray 粒子系统。
- [x] **P0 — 场景材质与环境**：前景侧壁新增独立透明软 PVC 薄膜，使用 IOR 1.50、Fresnel、弱吸收与 wrinkle；这只是专用薄膜近似，**不是完整 PVC 多介质 ray path**。render `pool.z`、PVC bottom、liner、草地与物理粒子底面已统一到 `y=0`，ring separation 由 `2*tube` 推导。场景加入无限程序化草地及大气天空/云，没有 cubemap 资产。当前 v8 保留 4 界面 Fresnel/Snell、分段 Beer–Lambert、opaque depth sorting、linear exposure 与 density 边界归零，并使用 `extinction=(12,3.6,2.5)`；历史 v6 的 `(30,10,8)` 合同不回写。未 vendor MIT [jeantimex/fluid](https://github.com/jeantimex/fluid) 的代码或资产。
- [x] **历史合同隔离**：`cinematic_liquid_v1`、正式 `cinematic_liquid_v2_surface_splat_optics_v4` / `shaderVersion=6`、`cinematic_liquid_v2_duck_family_v5` / `shaderVersion=7` 预览、`cinematic_liquid_v2_iterative_optics_v6` / `shaderVersion=8` 预览、physical-scene v7 与当前 v8 必须各自分组，绝不混分。
- [x] **历史 optics_v4 正式 CLI/抓帧验收**：RTX 5090 Vulkan 正式 15 秒 + 5.1 秒 RenderDoc 已通过，结果 `20260715-170629-492`：Compute 10.572 ms、Render 1.553 ms、Total 12.125 ms、`263.98 MParticle-step/s`、966 个计分帧；0.103 秒抓帧开销已排除，1 次尝试/1 个文件成功。正式图为 `rdoc_captures/cinematic-liquid-v2-5s-formal-optics-v4.png`。该成绩只能归属 optics_v4。
- [x] **历史 v6 短预览**：RTX 5090 / Vulkan / 6 秒预览结果 `20260715-221447-024`：Compute 9.450 ms、Render 3.003 ms、Total 12.454 ms、`257.01 MParticle-step/s`、284 个计分帧；图为 `rdoc_captures/cinematic-liquid-v2-5s-iterative-optics-v6-final-preview.png`。它只属于 v6 `_preview`，v6 没有正式 15 秒成绩，也不能归到 v7。
- [x] **历史 v7 构建/自动 smoke 边界**：shader 编译、最终 6 个 SPIR-V 校验、CLI Release 与 WinUI Release x64 构建通过；WinUI 为 0 error，仅有既有 MSB8027/C4996/LNK4042 类 warning（最新增量构建显示 2 个重复 WinAppSDK warning，源文件重编时曾显示 4 个）。只做过若干 `--time 6` / `--time 8` 自动停止 smoke；没有正式 15 秒、没有可靠保存的新 RenderDoc，也没有完整视觉验收。控制台瞬时 `241.13` 不是正式或持久成绩。窗口约 2-3 秒后关闭来自测试脚本的 `--time 8` 自动生命周期，不是已确认的崩溃。
- [ ] **P0 — v8 正式流程与 GUI/场景最终验收**：在 fixed timestep 合同冻结后运行 v8 正式 15 秒 + 第 5 秒 RenderDoc；WinUI 仍需实机检查 workload 选择、Vulkan-only 限制、完整运行与 History 分组；读取并记录沉球、船、越沿粒子的精确 GPU 轨迹。不得把历史 v7 构建、短 smoke 或 transient console 值写成 v8 视觉验收/正式成绩。
- [ ] **P0 — v2 可比性与可靠性收口**：解决固定 dt/每渲染帧推进造成的跨 GPU 帧率反馈，冻结第 5 秒轨迹合同；审计 Vulkan timestamp 是否完整且只覆盖约定的 compute/surface/render 边界；补齐初始化失败、swapchain/设备异常与提前退出路径的 surface buffer/image/descriptor/pipeline 资源清理。
- [x] **v2 下一重大画质路线 — SPH vertical slice 与视觉收口（2026-07-16）**：`--liquid-solver sph` → 始终为 `cinematic_liquid_sph_slice_v1_preview`。当前深池版本为 **318,464 粒子**、计数排序邻域、per-particle SDF/浮力刚体耦合，复用当前 inset 0.45、wall-top fraction 0.42、`extinction=(12,3.6,2.5)` 的池体/场景和用户鸭子家族；草地吸收倒计时为 0.50–1.80 sim 秒。RTX 5090/Vulkan 已实际完整运行 15 秒，水池/水面/鸭子/球/船/草地稳定显示并正常自动结束，用户接受当前视觉并停止外观迭代。**这只完成视觉验收，不是正式成绩完成**；在 render-frame 驱动的 2×1/120 timestep、每 substep `bodyImpulses` 清零、viscosity 原位 SSBO race、atomic scatter cell-order 非确定性四项全部关闭前，任何时长（包括 15 秒）都必须强制 `_preview`。之后才能做正式 15 秒 + 第 5 秒 RenderDoc、timestamp/确定性合同。secondary spray/foam、SPH 螺旋桨尾流与跨 API 是后续增强；细节见 HANDOFF 的 SPH vertical slice。
- [ ] **P0 后的 v2 原生后端移植**：DX12、DX11、OpenGL 与 Metal 尚未实现；必须在 Vulkan scene/pass/quality contract 冻结并通过正式验收后逐后端实现和验证，不能把通用 workload fallback 当作 liquid 支持。
- [x] **CPU 补充测试 Windows vertical slice（2026-07-16）**：原生 `cpu_mixed_v1`、CLI `per-core|multi|all`、三轮中位数、独立 WinUI CPU 页、实时逐核/总进度、Run/Cancel、stdout 协议与 `results.json` summary 持久化已实现；不创建 3D 窗口、不调用 RenderDoc。正式计分热路径已改为所有 per-core 同 seed、被测线程零 stdout、multi 测量窗口零 stdout、线程局部/128-byte 隔离计数；Windows/Linux/Android affinity 均要求 set 后回读验证。GUI 已加 CPU/GPU/Charts 全局互斥、完整 15.0/0.2 Formal 预设、输出节流和协议完整性审计。9800X3D Release smoke 正确枚举 16 logical/8 physical/SMT2，逐核与 multi 全部 strict affinity、exit 0；隔离数据目录的 0.1 秒 GUI E2E 显示 16 条逐核、平均、多核、100%/Done。两者均是 preview，不是正式成绩。正式合同为 15.0 秒总测量 + 0.2 秒预热 + r3；版本隔离 affinity/time/warmup/sequence，JSON 只保存逐核平均与 multi summary。
- [ ] **CPU 发布与平台合同收口**：重建新的 stage/ZIP/Inno Setup，并在干净 Windows 安装后验收 GUI 相邻 CLI 查找、Run/Cancel、History 写入；补一次不受当前开发负载影响的正式 15.0/0.2/r3 成绩、>64 logical/processor-group 与真实混合核实机。Linux/Android 代码合同为回读验证的 `strict_sched_affinity`（失败 `valid=0`/exit 3），但原生 Linux、容器/cpuset 和 Android 设备尚未构建；macOS 为 `scheduler_managed`/估计拓扑，iOS/Web/WASM 未构建。P/E/Mid/LPE 只允许写 `Inferred*` 排名标签，不得宣称真实微架构识别，各 affinity capability 必须独立分组。
- [x] **GT 120 / DX10 时代代码路径**：不新增 DX9 后端；现有 DX11 后端实际探测 FL10_0/10_1 与可选 DirectCompute 4.x，按设备切换 `cs/vs/ps_4_0`，fragment-only 测试不再创建 compute/UAV，Vulkan loader 改为 delay-load。16/16 个生产 HLSL SM4 entry 已通过 FXC，DX11 Extreme 越界会拒绝，SM4 N-body 安全上限为 4,096。
- [ ] **GT 120 实卡验收**：在 Windows 10 1809+ / NVIDIA 342.01 环境先跑 Stream/Particle Light/Medium、GPU Burn 安全自动标定、Legacy Fractal/Volumetric 与 4,096-body N-body；确认 DirectCompute feature bit、GPU timestamp、15 秒生命周期、第 5 秒 RenderDoc、TDR 余量和结果 metadata。未完成前只能称“SM4 编译/代码路径通过”，不能称 GT 120 已验证。若目标机是 Windows 7，另建 legacy CLI/OS 包；当前 WinUI 安装器不支持 Win7。
- [ ] **紧接 v2 — Liquid Lab / Explore**：复用 v2 场景提供无限时间、自由 orbit/WASD 视角、暂停/单步/重置、物体/流体/螺旋桨参数调整，以及充气池/玻璃水缸环境预设；明确标为不可计分，默认不自动 RenderDoc，不得写入正式 benchmark history。正式 Benchmark 与 Explore 切换时必须重建固定资源、恢复 seed 并校验 scene hash。
- [ ] **紧接 v2 — GPU Burn Unlimited Soak**：使用当前主 `gpu_burn_v1` Plasma Bloom 管线持续运行直到用户停止，而不是复用 Other/Legacy 的旧 `gpu_stress_v1`；GUI 必须在满载时仍能 Stop，持续显示运行时长、滚动 GPU time/FPS、利用率、温度/功耗（可得时）和降频趋势。固定 `15s + 第 5 秒 RenderDoc` 继续只负责可比较跑分；Soak 默认不抓帧、不写正式 score。
- [ ] **紧接 v2 — VRAM Integrity Soak**：新增独立 `vram_memtest`，按显存 budget 的安全比例分块写入 address/random/walking-bit 等 pattern、设备端读回校验并累计错误，持续到用户停止；区分独显 VRAM 与 UMA，共享内存不能误标为显存，OOM/device-lost 必须可恢复。原始 `stream` 继续是 15 秒带宽成绩，不能冒充显存正确性测试。
- [x] **换机发布构建链**：Windows Vulkan loader 已 delay-load+双重 guard；官方 RenderDoc 1.45、self-contained WinUI/CLI、MSVC runtime、511-file stage、逐文件 SHA、ZIP 解包复核、Inno Setup 6.7.3、最终 ZIP/Setup/SHA256SUMS/release-assets 均已实际生成。候选位于 `out/release/windows-x64`；Setup SHA-256 `f301426776b8ad2bd816a3f6de55463fd4b2f6be0ca3c7f691bfd8108aca0436`。
- [ ] **公开发布 gate**：用户确认根项目 LICENSE；配置 Authenticode signing；冻结并接入 `report_worker.exe`；在无 VS/Python/Vulkan SDK/RenderDoc/VC Redist 的干净 Windows 10/11 上验证安装/升级/卸载、无 `vulkan-1.dll` 的 DX11/WARP 启动、GUI orchestration、四 API 第 5 秒 bundled RenderDoc 抓帧；再用 clean commit/tag 重建，移除 `sourceTreeDirty=true` 后上传 GitHub Release。
- [ ] 当前仓库新增原生 WebGPU 后端：优先固定 Dawn 版本并依次移植 Stream、GPU Burn、Cinematic Liquid v2；结果必须记录 Dawn/底层 D3D12/Vulkan/Metal、实现版本、timestamp 模式与精度，不能只写 `WebGPU` 后与原生成绩混排。
- [ ] 第二阶段建立 `/web` 浏览器前端，共享 WGSL/workload manifest；浏览器结果单独分组，不承诺“全部 GPU 精确选择”或第 5 秒 RenderDoc。
- [ ] TriangleBin WebGPU：建议另建正式 fork/独立 `TriangleBin-WebGPU` 仓库，保留上游 MIT 与历史，只移植 atomic-counter shade-order 核心并以 HTML/WGSL 重写 UI；当前项目仅在 Architecture Tools 中链接或导入结果，不直接合并源码。

## 平台移植优先级（用户锁定 2026-07-16）

顺序：**Win ARM64 → macOS → Android → iOS → Debian Linux → WebGPU → HarmonyOS PC / 鸿蒙 → PS3（探索性）→ Dual-GPU Aggregate（双卡合力，功能项）**（2026-07-16 用户把 WebGPU 排入、把 HarmonyOS 排在 WebGPU 后与 PS3 前；同日把 Dual-GPU Aggregate 排在 PS3 之后）。除现有隔离的 HarmonyOS Vulkan 粒子 demo 外，完整产品移植均未开始；该 demo 不等于 workload suite 已移植。不改变上方产品主线的切片顺序，平台移植在其后展开。逐平台落地时：能力不齐明确 unsupported、不静默 fallback；计时/抓帧模型不同的实现必须使用新 `workloadVersion` 独立成组，现有 Windows 成绩组的 A/B 对比不受影响。详细逐平台要点见 `HANDOFF.md` 目标 C。

- [ ] **1. Win ARM64**：CLI/WinUI ARM64 目标 + vcpkg `arm64-windows` + WinAppSDK ARM64 payload；实机验证 Vulkan(Adreno)/DX12/DX11/WARP/OpenGL 兼容层。预计不动核心代码与成绩合同。
- [ ] **2. macOS**：主 workload 的 Metal 移植（现仅粒子）、SwiftUI GUI 对齐统一 registry、`MTLCaptureManager`(.gputrace) 替代 RenderDoc。
- [ ] **3. Android**：NativeActivity/ANativeWindow 表面层替代 GLFW + 新前端；RenderDoc Android 远程抓帧；评估温控对 15 秒 Burst 语义的影响。
- [ ] **4. iOS**：仅 Metal/MoltenVK；抓帧走 `MTLCaptureManager`；与 macOS 共享 SwiftUI 前端；App Store 分发约束。
- [ ] **5. Debian Linux**：构建修正、`.deb` 打包、CI 与实机验证（后端/GLFW/RenderDoc/XDG 路径均已有，摩擦最低）。
- [ ] **6. WebGPU**：按既定路线——capability registry P0 → 固定 Dawn 版本原生后端（Stream → GPU Burn → Cinematic Liquid）→ `/web` 浏览器前端；独立版本 id（`stream_webgpu_v1` 等），无可靠 timestamp 不产生正式 score（对应上方“原生 WebGPU 后端”与 `/web` 两条任务）。
- [ ] **7. HarmonyOS PC / 鸿蒙**：把现有 `ohos/` 独立 Vulkan 粒子 demo 升级为正式产品端口；补统一 workload registry、主 CLI/GUI、GPU Burn/Cinematic Liquid、结果合同与适合该平台的抓帧编排。现有 demo 不能标为已完成移植。
- [ ] **8. PS3（探索性，永不进成绩体系）**：仅 homebrew（PSL1GHT/RSXGL）；RSX 无 compute/原子/GPU timestamp，PSGL≈GL ES 1.0+Cg，三主测试不可直移；至多独立仓库的固定管线情怀 demo。
- [ ] **9. Dual-GPU Aggregate（双卡合力模式，功能项；首个验证目标 Boot Camp 下 Mac Pro 2013 双 D700）**：引擎级显式多 GPU，不依赖驱动 CrossFire/LDA/device-group。切片：(a) `stream` headless 双设备聚合（新组 `stream_dualgpu_v1`，记录双 adapter 元数据）→ (b) N-body 双卡位置交换 → (c) GPU Burn 分屏 SFR/AFR（DX12 unlinked 跨适配器堆）。不做液体域分解。双 D700 满载注意散热，禁止双卡长时烤机。

> 状态提示：请先阅读根目录 [`HANDOFF.md`](../HANDOFF.md)。当前事实、两条产品主线、P0 阻塞和下一实现切片以 HANDOFF 为准；本文件保留专题任务与历史上下文。每次工作应先更新 HANDOFF，再同步这里。

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

## 后期图形研究（可行性，非当前主线）

- [ ] **水体 Hybrid RT**：在 v2 与三个自由/无限模式完成后，新增独立 `cinematic_liquid_hybrid_rt_v1`。首版只做 Vulkan `VK_KHR_acceleration_structure + VK_KHR_ray_query`：池体/鸭子/球/船进入 triangle BLAS，七刚体更新 TLAS；现有 MLS-MPM 密度水面与 whitewater 继续 raymarch，ray query 负责阴影、反射和折射后的实体命中。NVIDIA RTX、AMD RDNA2+、Intel Arc 共用 KHR 路径；Metal 只把明确有专用 RT 硬件的 Apple family 9+ 计入 hardware 组。DXR/Metal RT 等对应 liquid 后端存在后再移植。
- [ ] **RT 能力与 fallback 合同**：硬件 RT pipeline 不会自动在非 RT GPU 上运行。`Auto` 必须显式检测 acceleration structure/ray query/DXR tier/Metal family，有硬件才选 hybrid RT，否则选当前 `density_raymarch_soft_v1`；`Hardware RT` 在不支持时显示 N/A，`Software` 始终显式运行现有 fragment raymarch。硬件与软件使用不同 workloadVersion/榜单，并记录 RT API/tier/hardware class、fallback reason、AS build/update、trace 与 total GPU time。当前软件水体 raymarch 已可作为首版 fallback；不要现在另写跨 API 通用软件 BVH。
- [ ] **Water Path Trace**：另建 `water_pathtrace_v1`，第一阶段冻结确定性的第 5 秒水体快照，固定 seed、SPP、反弹次数、相机与光源后累积路径追踪；动态水体每帧都会破坏 temporal accumulation，必须留到降噪与时序资源成熟后。实时模拟、AS build/update、trace、denoise/upscale 与 total GPU time 分项记录。
- [ ] 动态密度水体优先作为固定水域 AABB + procedural intersection/raymarch，而不是每帧强制抽取并重建完整三角网格；静态池体与刚体进入 BLAS/TLAS。WebGPU 当前标准没有 acceleration structure/ray query/RT pipeline，只能另做明确标记的 WGSL compute trace，不能与硬件 RT 排名。
- [ ] 建立 `IUpscalerPlugin` / `IDenoiserPlugin` / `IFrameGenerationPlugin`，统一提供 HDR color、depth、无 jitter motion vector、camera jitter、exposure、reactive/transparency mask 与 history reset；水体必须有真实表面运动矢量和 reactive mask，否则 DLSS/FSR/XeSS/MetalFX 会出现明显拖影。
- [ ] 超分比较拆为三组：Native 固定输入=输出主基线；Fixed-Scale 使用完全相同输入/输出分辨率比较 DLSS/FSR/XeSS/MetalFX；Vendor Recommended 只展示厂商推荐模式，不进统一榜。分别报告 base render、upscale、total GPU time、VRAM 与 PSNR/SSIM/FLIP，不把 FPS 和画质强压成一个分数。
- [ ] Frame Generation 与 Super Resolution 分开：只报告真实渲染 FPS、显示 FPS、生成耗时和延迟；生成帧不得计入模拟/渲染工作量。厂商 SDK 使用可选动态插件和独立 notices/版本元数据，安装包只分发许可允许的 production binary；MetalFX 使用系统框架，无需捆绑运行库。
