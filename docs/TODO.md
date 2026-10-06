# Mangekyo TODO

## 当前产品主线（2026-07-15）

> 当前事实、验证记录与交接顺序以根目录 [`HANDOFF.md`](../HANDOFF.md) 为准；下面旧专题内容只保留历史上下文。

> **Windows 本地编译**：默认用 [`scripts/build-windows.ps1`](../scripts/build-windows.ps1) 编 CLI+WinUI，并同步同目录 shader；详见 [`docs/building.md`](building.md)。正式发布生成 ZIP，以及 x64/ARM64 各一个原生中英双语 Setup EXE；完整步骤见 [`windows-installer-packaging.md`](windows-installer-packaging.md)，勿把开发树、raw MSI 或 legacy Inno 产物当成正式安装包。

> **当前执行边界**：先完善 Original Particle、Plasma/GPU Burn、当前 Cinematic Liquid `physical_scene_v8`、GUI 同步、固定 `15s + 第 5 秒抓帧` 与结果合同。N-body 不进入双卡路线。本轮新增的 RT、路径追踪、DLSS/FSR/XeSS/MetalFX 仅记录可行性；在现有测试完成验收且用户再次明确提优先级前，不开始实现。

- [x] **2026-07-21 开发默认 CLI+GUI**：`scripts/build-windows.ps1`；`CopyGpuBenchmarkWorker` 同步 worker 与 HLSL/SPIR-V/OpenGL 资产（修复 GUI 同目录缺 `compute.hlsl` 导致 DX/OpenGL 失败）。
- [x] 主测试 `gpu_burn` 已升级为 `gpu_burn_v2_mangekyo_faceted_glass_v1`：透视 3D 切割玻璃晶冠、多层碎钻、平面 SDF 法线、Fresnel 反射、折射色散与相机视差已接入 Vulkan/DX12/DX11/OpenGL、WinUI、自动标定和版本化结果；原创 Plasma Bloom 以公开 `gpu_burn_v1` legacy 选择完整保留。
- [x] RTX 5090 自动标定实测稳定段 NVML 99%，约 599–600W；默认仍是 15 秒 Burst + 第 5 秒 RenderDoc，不宣称长时热稳定或错误检测。
- [x] 低强度 9 项设备/API 矩阵、包内 RenderDoc 抓帧和 staged WinUI 启动通过；固定未探测步数限制为 16–32，推荐留空自动标定。
- [x] **历史 P0 — Cinematic Liquid v2 physical-scene v7 Vulkan 切片**：固定 128x64x96 MLS-MPM 网格、10 substeps 与 **320,920 粒子**（142x14x98 浅水底 + 48x37x71 溃坝体）；固定参数为 stiffness 45,000、viscosity 0.035、maxSpeed 8。历史身份为 `workloadVersion=cinematic_liquid_v2_physical_scene_v7`、`shaderVersion=9`、`sceneVersion=4`。用户调整后的母鸭、3 只小鸭以及 7 刚体 index ABI 均保留：船仍为 index 2、沉球仍为 index 3、小鸭仍追加在 4-6。
- [x] **当前合同隔离 — physical-scene v8**：共享池体/光学参数后来改为 `poolWallInset=0.45`、`poolWallTopFraction=0.42`、`extinction=(12,3.6,2.5)`，已将当前 MLS-MPM 结果身份升为 `cinematic_liquid_v2_physical_scene_v8`、`sceneVersion=5`，不得与 v7 smoke 或任何更早版本混排；v8 尚无正式成绩。
- [x] **P0 — 船与沉球物理修正**：船由硬锚定改为 34 kg 有限质量刚体、软系泊，并允许推进器反冲驱动/摇摆；目前只确认实现，**尚未视觉验收其运动轨迹**。index 3 水密实心球保持 1.06 水密度比，4.28 秒释放，重力 -9.81、空气阻尼 0.015；材料水阻按 displaced mass 与浸没率门控，不再把空气阶段错误地当成水下运动。
- [x] **P0 — 越沿流体与自适应表面**：当前池壁是有限高度的内嵌碰撞壁（inset 0.45、wall-top fraction 0.42），外层 simulation catch band 允许粒子真实越沿后落到地面；catch band 带宽有限，**不是无限流体域**。5x5x5 binomial 重建改为自适应保留低支持喷滴。沉球入水 crown/whitewater 由真实 GPU 刚体状态与局部流体产生，不是 fragment 假水花；尚无独立 secondary-spray 粒子系统。
- [x] **P0 — 场景材质与环境**：前景侧壁新增独立透明软 PVC 薄膜，使用 IOR 1.50、Fresnel、弱吸收与 wrinkle；这只是专用薄膜近似，**不是完整 PVC 多介质 ray path**。render `pool.z`、PVC bottom、liner、草地与物理粒子底面已统一到 `y=0`，ring separation 由 `2*tube` 推导。场景加入无限程序化草地及大气天空/云，没有 cubemap 资产。当前 v8 保留 4 界面 Fresnel/Snell、分段 Beer–Lambert、opaque depth sorting、linear exposure 与 density 边界归零，并使用 `extinction=(12,3.6,2.5)`；历史 v6 的 `(30,10,8)` 合同不回写。未 vendor MIT [jeantimex/fluid](https://github.com/jeantimex/fluid) 的代码或资产。
- [x] **合同隔离**：当前 `physical_scene_v8` 与任何后继实现必须按 `workloadVersion` / shaderVersion / sceneVersion 独立分组；历史结果只读保留，绝不混分。
- [x] **历史 optics_v4 正式 CLI/抓帧验收**：RTX 5090 Vulkan 正式 15 秒 + 5.1 秒 RenderDoc 已通过，结果 `20260715-170629-492`：Compute 10.572 ms、Render 1.553 ms、Total 12.125 ms、`263.98 MParticle-step/s`、966 个计分帧；0.103 秒抓帧开销已排除，1 次尝试/1 个文件成功。正式图为 `rdoc_captures/cinematic-liquid-v2-5s-formal-optics-v4.png`。该成绩只能归属 optics_v4。
- [x] **历史 v6 短预览**：RTX 5090 / Vulkan / 6 秒预览结果 `20260715-221447-024`：Compute 9.450 ms、Render 3.003 ms、Total 12.454 ms、`257.01 MParticle-step/s`、284 个计分帧；图为 `rdoc_captures/cinematic-liquid-v2-5s-iterative-optics-v6-final-preview.png`。它只属于 v6 `_preview`，v6 没有正式 15 秒成绩，也不能归到 v7。
- [x] **历史 v7 构建/自动 smoke 边界**：shader 编译、最终 6 个 SPIR-V 校验、CLI Release 与 WinUI Release x64 构建通过；WinUI 为 0 error，仅有既有 MSB8027/C4996/LNK4042 类 warning（最新增量构建显示 2 个重复 WinAppSDK warning，源文件重编时曾显示 4 个）。只做过若干 `--time 6` / `--time 8` 自动停止 smoke；没有正式 15 秒、没有可靠保存的新 RenderDoc，也没有完整视觉验收。控制台瞬时 `241.13` 不是正式或持久成绩。窗口约 2-3 秒后关闭来自测试脚本的 `--time 8` 自动生命周期，不是已确认的崩溃。
- [ ] **P0 — v8 正式流程与 GUI/场景最终验收**：在 fixed timestep 合同冻结后运行 v8 正式 15 秒 + 第 5 秒 RenderDoc；WinUI 仍需实机检查 workload 选择、Vulkan-only 限制、完整运行与 History 分组；读取并记录沉球、船、越沿粒子的精确 GPU 轨迹。不得把历史 v7 构建、短 smoke 或 transient console 值写成 v8 视觉验收/正式成绩。
- [ ] **P0 — v2 可比性与可靠性收口**：解决固定 dt/每渲染帧推进造成的跨 GPU 帧率反馈，冻结第 5 秒轨迹合同；审计 Vulkan timestamp 是否完整且只覆盖约定的 compute/surface/render 边界；补齐初始化失败、swapchain/设备异常与提前退出路径的 surface buffer/image/descriptor/pipeline 资源清理。
- [x] **v2 下一重大画质路线 — SPH vertical slice 与视觉收口（2026-07-16）**：`--liquid-solver sph` → 始终为 `cinematic_liquid_sph_slice_v1_preview`。当前深池版本为 **318,464 粒子**、计数排序邻域、per-particle SDF/浮力刚体耦合，复用当前 inset 0.45、wall-top fraction 0.42、`extinction=(12,3.6,2.5)` 的池体/场景和用户鸭子家族；草地吸收倒计时为 0.50–1.80 sim 秒。RTX 5090/Vulkan 已实际完整运行 15 秒，水池/水面/鸭子/球/船/草地稳定显示并正常自动结束，用户接受当前视觉并停止外观迭代。**这只完成视觉验收，不是正式成绩完成**；在 render-frame 驱动的 2×1/120 timestep、每 substep `bodyImpulses` 清零、viscosity 原位 SSBO race、atomic scatter cell-order 非确定性四项全部关闭前，任何时长（包括 15 秒）都必须强制 `_preview`。之后才能做正式 15 秒 + 第 5 秒 RenderDoc、timestamp/确定性合同。secondary spray/foam、SPH 螺旋桨尾流与跨 API 是后续增强；细节见 HANDOFF 的 SPH vertical slice。
- [ ] **P0 后的 v2 原生后端移植**：DX12、DX11、OpenGL 液体尚未实现；Metal 已有 MLS-MPM + raymarch present（`…_metal_preview`），正式合同与真机验收未完成。必须在 Vulkan scene/pass/quality contract 冻结并通过正式验收后逐后端实现和验证，不能把 preview 当作正式 liquid 支持。
- [x] **CPU 补充测试 Windows vertical slice（2026-07-16）**：原生 `cpu_mixed_v1`、CLI `per-core|multi|all`、三轮中位数、独立 WinUI CPU 页、实时逐核/总进度、Run/Cancel、stdout 协议与 `results.json` summary 持久化已实现；不创建 3D 窗口、不调用 RenderDoc。正式计分热路径已改为所有 per-core 同 seed、被测线程零 stdout、multi 测量窗口零 stdout、线程局部/128-byte 隔离计数；Windows/Linux/Android affinity 均要求 set 后回读验证。GUI 已加 CPU/GPU/Charts 全局互斥、完整 15.0/0.2 Formal 预设、输出节流和协议完整性审计。9800X3D Release smoke 正确枚举 16 logical/8 physical/SMT2，逐核与 multi 全部 strict affinity、exit 0；隔离数据目录的 0.1 秒 GUI E2E 显示 16 条逐核、平均、多核、100%/Done。两者均是 preview，不是正式成绩。正式合同为 15.0 秒总测量 + 0.2 秒预热 + r3；版本隔离 affinity/time/warmup/sequence，JSON 只保存逐核平均与 multi summary。
- [ ] **CPU 发布与平台合同收口**：重建新的 stage/ZIP/原生双语 Setup，并在干净 Windows 安装后验收 GUI 相邻 CLI 查找、Run/Cancel、History 写入；补一次不受当前开发负载影响的正式 15.0/0.2/r3 成绩、>64 logical/processor-group 与真实混合核实机。Linux/Android 代码合同为回读验证的 `strict_sched_affinity`（失败 `valid=0`/exit 3），但原生 Linux、容器/cpuset 和 Android 设备尚未构建；macOS 为 `scheduler_managed`/估计拓扑，iOS/Web/WASM 未构建。P/E/Mid/LPE 只允许写 `Inferred*` 排名标签，不得宣称真实微架构识别，各 affinity capability 必须独立分组。
- [x] **GT 120 / DX10 时代代码路径**：不新增 DX9 后端；现有 DX11 后端实际探测 FL10_0/10_1 与可选 DirectCompute 4.x，按设备切换 `cs/vs/ps_4_0`，fragment-only 测试不再创建 compute/UAV，Vulkan loader 改为 delay-load。16/16 个生产 HLSL SM4 entry 已通过 FXC，DX11 Extreme 越界会拒绝，SM4 N-body 安全上限为 4,096。
- [ ] **GT 120 实卡验收**：在 Windows 10 1809+ / NVIDIA 342.01 环境先跑 Stream/Particle Light/Medium、GPU Burn 安全自动标定、Legacy Fractal/Volumetric 与 4,096-body N-body；确认 DirectCompute feature bit、GPU timestamp、15 秒生命周期、第 5 秒 RenderDoc、TDR 余量和结果 metadata。未完成前只能称“SM4 编译/代码路径通过”，不能称 GT 120 已验证。若目标机是 Windows 7，另建 legacy CLI/OS 包；当前 WinUI 安装器不支持 Win7。
- [ ] **紧接 v2 — Liquid Lab / Explore**：复用 v2 场景提供无限时间、自由 orbit/WASD 视角、暂停/单步/重置、物体/流体/螺旋桨参数调整，以及充气池/玻璃水缸环境预设；明确标为不可计分，默认不自动 RenderDoc，不得写入正式 benchmark history。正式 Benchmark 与 Explore 切换时必须重建固定资源、恢复 seed 并校验 scene hash。
- [ ] **紧接 v2 — GPU Burn Unlimited Soak**：使用当前主 `gpu_burn` Mangekyo Kaleidoscope v2 管线持续运行直到用户停止，而不是复用 legacy `gpu_burn_v1` 或 Other/Legacy 的旧 `gpu_stress_v1`；GUI 必须在满载时仍能 Stop，持续显示运行时长、滚动 GPU time/FPS、利用率、温度/功耗（可得时）和降频趋势。固定 `15s + 第 5 秒 RenderDoc` 继续只负责可比较跑分；Soak 默认不抓帧、不写正式 score。
- [ ] **独立的显存 / L2 带宽测试 `membw_v1`（用户 2026-09-30 排入；2026-10-05 完成第一步）**：粒子测试（`stream_v1`）测不出真正的显存和 L2 带宽（原因与数据见 [`benchmark-workload-suite.md`](benchmark-workload-suite.md) §6），所以另做一个专门的测试，粒子测试保持不变。用法、方法和限制见 [`cli-reference.md`](cli-reference.md) 的 "Memory-bandwidth sweep" 一节。
  - [x] **第一步：Vulkan 命令行版**（`--workload membw`，2026-10-05）。实现在 `src/vulkan_membw.cpp`，是独立路径：自己创建 Vulkan 设备，只做计算，不开窗口，不抓帧，没有改动五个后端和粒子测试。
    - **访存方式：** 只读（主成绩）、只写、复制三个 kernel。每个线程一次读写 16 字节（vec4），每轮访问两处地址；只读 kernel 用一个永远不成立的条件写回，防止读取被优化掉。
    - **测缓存的方法（参考 gpu-benches `gpu-l2-cache`）：** 每个工作组读一块 512 KiB 的数据，用不同数据块的个数控制数据量，从 1 MiB 扫到 2 GiB；工作组总数单独校准，让每次派发在任何数据量下都持续约 4 ms。
    - **计时与统计：** 每次派发单独提交，并用 GPU 时间戳包住；每个数据点预热 3 次、正式测 10 次，成绩取中位数，同时保存最快值。字节数按实际请求量计算，缓冲区内容由整数哈希生成，不可压缩。
    - **输出：** 每个 kernel 一张表，外加显存平台（最大三个数据量的中位数）和缓存平台（明显高于显存平台的最长平坦段）；同时输出制表符分隔的 `MEMBW_POINT` / `MEMBW_SUMMARY` 行。核显和软件设备标为"系统内存"。
    - **成绩：** workload 为 `membw`，分数是只读显存平台（GB/s）。默认扫描且到达显存平台时版本为 `membw_v1`，否则为 `membw_v1_preview`。
  - [x] **第一步验证（2026-10-05）：**
    - **RTX 5090**（驱动 616.92，GPU 空闲）：显存读 1536 GB/s，为理论值 1792 的 86%，连跑三次为 1542 / 1536 / 1528；写 1352–1364 GB/s，复制 1342–1354 GB/s；L2 平台约 6.6–6.7 TB/s（3–96 MiB），96 MiB 之后开始下降，与 96 MB 的 L2 容量相符。与 2026-09-30 粒子只读对照 kernel 的 1503 GB/s 一致。
    - **AMD 核显**（9800X3D，共用 DDR5-6400）：读 58 GB/s，写 55 GB/s，4 MiB 到 2 GiB 基本是平线；复制在 16 MiB 以内约 64 GB/s，更大时约 40 GB/s，原因未查。
    - **Linux**（`ubuntu:20.04` 容器，GCC 9.4）：新模块编译无警告，aarch64 交叉编译通过；在 lavapipe 上跑通，记为软件渲染和 `membw_v1_preview`。
  - [x] **L1 测试（2026-10-05，用户要求追加）**：新增 `shaders/membw_l1.comp`，每个工作组反复读自己私有的一块数据，数据块从 16 KiB 扫到 1 MiB。跑两遍：`l1` 用 4096 个工作组让全部 SM/CU 满载，`l1_single` 只用一个工作组（即一个 SM/CU）。汇总取最快数据块附近的平台作为 L1 读速率，并用单工作组曲线第一次下降的位置给出每个 SM/CU 的 L1 容量范围。
    - **RTX 5090：**
      - 单工作组在 16–96 KiB 稳定在约 287 GB/s，128 KiB 约 247 GB/s，192 KiB 掉到约 90 GB/s，256 KiB 以上约 82 GB/s（由 L2 提供）。程序判定 L1 容量在 128–192 KiB 之间，连跑三次一致。
      - 满载时 16–24 KiB 约 55 TB/s，折合每个 SM 约 326 GB/s，即每周期约 128 字节，与 gpu-benches 在其他 NVIDIA 卡上的结果一致；32 KiB 约 27 TB/s，64–96 KiB 回到约 6.2 TB/s 的 L2 速率，384 KiB 以上为约 1.5 TB/s 的显存速率。
      - **必须把工作组固定在一个 SM 上：** 不做处理时，NVIDIA 驱动会把单个工作组的 warp 分散到多个 SM，单工作组曲线每次运行都不同（速率在 288、410、540 GB/s 之间跳，容量拐点跑到 1 MiB）。kernel 里声明一个共享变量并调用一次 `barrier()` 之后，结果稳定。
    - **AMD 核显（RDNA2，9800X3D）：**
      - 单工作组出现三次下降：16 KiB 为 168 GB/s，24–64 KiB 为 127 GB/s，96–256 KiB 为 88 GB/s，384 KiB 以上为 38 GB/s。程序只报告第一次下降，即 L1 容量在 16–24 KiB 之间；后两级对应哪一层缓存没有确认。
      - 满载时 16–32 KiB 约 220–260 GB/s，48 KiB 以上回到约 60 GB/s 的内存速率。
    - **限制：**
      - L1 读速率同时受指令执行速率限制（每次 16 字节读取后跟 4 次浮点加法），是这个 kernel 能达到的速率，不是硬件上限；
      - 满载平台的终点低于 L1 容量，因为一个 SM/CU 上同时运行多个工作组；
      - 多级小缓存的设备只在表格里能看到后续台阶，汇总不报告。
  - [x] **独显读写系统内存（2026-10-05，用户要求追加）**：只读和只写 kernel 在一块位于系统内存的缓冲区上再跑一遍（`host_read` / `host_write`，最大 2 GiB），再用 `vkCmdCopyBuffer` 在系统内存和显存之间各复制一次。每个字节都要经过 PCIe，所以大数据量的平台就是链路在该方向上的可用带宽。核显和软件设备跳过这一段。
    - **RTX 5090 实测：** `host_read` 为 13.1 GB/s，`host_write` 为 13.0 GB/s，驱动复制两个方向都是 12.0 GB/s。
    - **链路宽度：** `nvidia-smi` 和 Windows 设备属性都显示这张卡当前跑在 **PCIe 5.0 x4**（最大 x16），单方向理论值为 15.75 GB/s，实测是它的 83%。这与之前记录的 x8 不一致，x4 的原因（主板通道分配、插槽或接触）没有查。
    - **小数据量被缓存：** `host_read` 在 1 MiB 时为 450 GB/s，8 MiB 为 62 GB/s，32 MiB 以上才稳定在 13 GB/s；`host_write` 从 1 MiB 起就是 12–13 GB/s。说明显卡会缓存已经取回的系统内存数据。
    - **限制：** 程序读不到链路代数和宽度（Vulkan 不提供），只在输出里给出每条通道的理论值供人工对照；没有在 x8 / x16 链路或 AMD、Intel 独显上测过。
  - [ ] **第一步遗留：**
    - **理论值校验没做：** Vulkan 不提供显存频率和位宽，所以程序不会自动判断"超过理论值即无效"。需要厂商表或 NVML/ADL 才能实现。
    - **缓存容量不精确：** GPU 同时运行的工作组多于数据块个数时，多个工作组会经由缓存共享同一块数据，缓存到显存的过渡段被拉宽（5090 上 128–384 MiB 偏高）。精确容量要靠第二步的延迟测试。
    - **小数据量受 L1 影响：** 5090 上 3 MiB 以下比 L2 平台快（1 MiB 时约 19 TB/s），程序已标注不算 L2 成绩。
    - **外部对照没做：** RX 6900 XT 的 Infinity Cache 平台（gpu-benches 公布约 1.92 TB/s）和 vkpeak `copy-d2d` 都还没对比。vkpeak 用 CPU 计时，只按缓冲区大小计一次字节，换算时要注意。
    - **其他设备没测：** Intel、摩尔线程、Adreno、Jetson。时间戳不随工作量变化的驱动会自动改用墙钟计时，这条路径只在代码里实现了，没有在真实设备上触发过。
  - [ ] **第二步：** 加随机顺序的访存延迟测试（每个缓存行只读一次），用来准确定位 L2 / Infinity Cache 的容量。
  - [x] **第三步（一）：接入 WinUI 的运行页和 History（2026-10-05）**：
    - **入口：** GPU 页的测试项目下拉框新增 "Memory Bandwidth — VRAM / Cache / PCIe"（中文为"显存带宽 —— 显存 / 缓存 / PCIe"），和粒子、Plasma、流体并列，默认可见。
    - **控件：** 选中后图形 API 只列 Vulkan；时长、Headless、V-Sync、系统内存、RenderDoc 和抓帧都禁用。
    - **运行：** 每个 GPU 启动一个 `--workload membw --backend vulkan --no-renderdoc` worker，不带 `--gui-worker`，避免预加载 RenderDoc；进度条按 30 秒估算。
    - **结果：** 结果区显示四行汇总（显存或系统内存带宽、缓存平台、L1、PCIe），由 worker 输出的 `MEMBW_*` 行生成，并支持中日文。
    - **History：** 显示正式名称，模式列为 "Compute"，帧率列为 "-"，预览成绩带 "[preview]"；不受"显示 headless"开关影响。
    - **验证：** 用界面自动化在 RTX 5090（GPU 选自动）和 AMD 核显上各跑通一次，结果区和 History 均截图确认。
  - [x] **第三步（二）：GUI 曲线图（2026-10-05）**：
    - **内容：** 结果区的汇总下方画出最多三张双对数折线图：显存（或系统内存）与缓存、经 PCIe 访问系统内存、L1。三张图分开画，是因为三组扫描相差几个数量级，而且 L1 的横轴是每个工作组的私有数据块大小。
    - **画法：** 直接用 XAML 的 Canvas 和 Polyline 绘制，不依赖 Python。线宽 2 px，线尾有圆点；两条以上的线一定有图例；一张图里所有线尾都彼此分得开时才在线尾标名字，否则都只靠图例（2026-10-06 由"逐条判断"改为"全标或全不标"）。每个数据点有一个 22 px 的悬停区域，提示框显示曲线名、数据量和带宽。
    - **配色：** 读、写、复制固定用蓝、橙、青三色，明暗两套色值分别通过了色觉障碍可分辨性校验；坐标和文字用文字色，不用曲线色。
    - **布局：** 按结果区宽度自动排成一到三列；窗口缩放、主题切换和语言切换时重画。
    - **验证：** 在 RTX 5090 上运行后截图检查了浅色英文和深色中文两种界面。
  - [ ] **第三步（三）：GUI 遗留**
    - 曲线图只显示最近一次运行；从 History 里选一条旧成绩不能重画曲线（数据已保存在成绩的 `workloadConfig` 里）。多个 GPU 一起跑时只画最后一个。
    - 悬停提示没有实际用鼠标验证；日文界面没有实际查看；窄窗口下的单列布局没有截图检查。
    - ~~Charts 页不包含这项测试~~：2026-10-06 起 Charts 页由 GUI 自己绘制，已包含这项测试。
    - "全部 GPU"预设和 Full Analysis 预设没有实际运行过；macOS / iOS / Android 前端没有接入。
    - GUI 打开时测得的显存读带宽比纯命令行低约 5%（约 1465 对 1536 GB/s），推测是窗口自身的绘制占用了 GPU。
  - [ ] **第三步（四）：** 移植到 DX12 / DX11 / OpenGL / Metal。
  - **许可证：** 代码全部自己写。gpu-benches 为 GPL-3.0，只借鉴方法；clamchowder/Microbenchmarks 为 Apache-2.0；BabelStream 为自定义许可证，改动后的版本不得称为 BabelStream 成绩。
- [x] **GUI 里所有测试的图表（用户 2026-10-06 提出，当天完成）**：图表都由 GUI 自己用 XAML 绘制，不依赖 Python。
  - **GPU 页，运行结束后：**
    - 一次运行有多个任务时，画各任务成绩的对比条形图（每个 GPU 和 API 一条）；
    - 每个任务画一张折线图：每帧 GPU 耗时随运行时间的变化，计算和渲染各一条线，不含预热；
    - 显存带宽测试仍显示它自己的带宽曲线；"Complete test"和"Fill missing"两个预设一个进程里跑很多项，不画运行图表。
  - **CPU 页，运行结束后：** 算力测试画每个逻辑处理器成绩的柱状图；缓存与内存测试仍显示带宽曲线。
  - **Charts 页：** 改为读取保存的成绩直接绘制，每项测试一张条形图（各设备和 API 在该测试最新版本下的最好成绩），包含两项带宽测试和 CPU 测试；原来这一页要靠 Python 脚本生成图片，安装包里不可用。
  - **引擎改动：** worker 每秒打印的 `[GPU Timing]` 行末尾新增 `T: 秒数`，预热期间的行带 `(warm-up)`，图表的时间轴取自这里。
  - **验证：** 见 `HANDOFF.md` 第 10 节。
  - **遗留：** 鼠标悬停提示和日文界面没有实际查看；多 GPU 一起跑的对比图没有实测；不能从 History 里选一条旧成绩重画它的运行图表。
- [ ] **CPU 缓存 / 内存带宽测试 `cpu_membw_v1`（用户 2026-10-05 提出；2026-10-06 完成命令行版和 WinUI 接入）**：测 CPU 在 L1、L2、L3（有 L4 时也包括）和内存上的读、写、复制速度，以及操作系统映射新页面的速度。用法、方法和限制见 [`cli-reference.md`](cli-reference.md) 的 "CPU cache and memory bandwidth sweep" 一节。
  - [x] **命令行版**（`--cpu-membw`，2026-10-06）。实现在 `src/cpu_membw.cpp`，是独立路径，不需要 GPU，只编进桌面版；原有的 CPU 算力测试没有改动。
    - **内核：** 读、写、复制三种，自动选用 CPU 支持的最宽向量（AVX-512 / AVX2 / SSE2 / NEON）。
    - **扫描：** 数据量从 4 KiB 到 1 GiB；先单线程，再每个物理核心一个线程，每个线程用自己的数据块。
    - **分层：** 缓存容量从操作系统读取，按容量把曲线上的点归到各级缓存和内存。
    - **内存写入的另外两种方式：** 流式写入（x86-64），以及 AMD 的 `CLZERO` 清零。
    - **虚拟内存：** 测"映射新页面"的速度，不测页面文件。
  - [x] **WinUI 接入（2026-10-06）**：CPU 页的测试模式新增"缓存与内存带宽"；结果区是一张各级带宽表（单线程和全核并排）和两张双对数曲线图，图上用虚线标出各级缓存容量；History 的 CPU 分类显示这项成绩。
  - [x] **验证（2026-10-06，Ryzen 7 9800X3D，2×48 GB DDR5-6400，Windows 11）：**
    - **单线程：** L1 读 588 GB/s、写 329 GB/s；L2 读 240 GB/s；L3 读 136 GB/s；内存读 55–61 GB/s。
    - **全核（8 线程）：** 内存读 54–55 GB/s，为双通道 DDR5-6400 理论值 102.4 GB/s 的 54%；写 27 GB/s；复制 40 GB/s。
    - **L1 与理论值：** 写入每周期 1 条 64 字节指令，329 GB/s 对应约 5.15 GHz，与实际频率一致；读取每周期约 1.8 条，是 2 条上限的 89%。
    - **与核显对比：** 核显（2026-10-05 测）读同一组内存为 58–62 GB/s，两者访问同一组内存条。
    - **稳定性：** 命令行跑 4 次、GUI 跑 2 次。单线程各级缓存读带宽的差别在 2% 以内；全核的缓存级成绩差别较大（L1 读 4242–4525 GB/s，另有一次 GUI 运行为 3535 GB/s）；单线程内存读为 55–61 GB/s。
    - **Linux：** Ubuntu 20.04 容器（GCC 9）里新文件编译无警告并跑通，单线程成绩与 Windows 一致；aarch64 交叉编译通过，没有运行。
  - [x] **大小核、SoC 缓存和存储读写（2026-10-06，用户提出"移动端大小核心"后追加）**：
    - **大小核：** 按系统报告的性能等级把核心分类，每一类各做一遍单线程扫描，用它自己的缓存容量分层；这类 CPU 的全核扫描只报告内存。
    - **系统不报告缓存容量时：** 从读曲线的平台识别各级缓存，容量给出一个范围。同样的方法也用来找系统没有报告的缓存（例如 SoC 的系统级缓存）。
    - **存储读写：** 在虚拟内存所在的硬盘上建一个最大 1 GiB 的临时文件，测顺序 1 MiB 和随机 4 KiB 的读写速度（不经过系统缓存，一次一个请求）。
    - **手机上怎么跑：** `scripts/build-android-cpumem.ps1` 用 NDK 编出独立程序 `mangekyo_cpumem`，用 adb 推到手机上运行，不需要安装 App。
    - **验证：** 见 `HANDOFF.md` 第 10 节。大小核和"从曲线识别"两条路径是在 9800X3D 上用诊断开关验证的；Android 程序只编译通过，没有在手机上运行。
  - [ ] **遗留：**
    - **没有在任何手机或大小核 CPU 上实际运行过。** 需要接上手机后用 adb 跑一遍，核对各级缓存与芯片资料是否一致。
    - ~~Android App 的 CPU 页还是空壳，iOS / macOS App 没有这项测试的入口~~：2026-10-06 三个前端都已接入，并带表格和曲线图。Android 编译通过；iOS 和 macOS 只做过类型检查，没有用 Xcode 编译；三端都没有在设备上运行过。
    - **iOS 和 macOS 不能绑定线程**，单线程扫描跑在系统挑选的核心上，能效核测不到。可以试的办法：用线程的 QoS 等级把线程放到能效核上（后台等级只在能效核上运行），并读取 `hw.perflevel1` 的缓存容量。还没有做。
    - **Android App 里的 CPU 测试不能取消**，开始后只能等它跑完。
    - **存储成绩没有和 CrystalDiskMark 等工具对比**（Windows 自带的 `winsat` 需要管理员权限）。GUI 里没有"跳过存储测试"的开关，每次运行都会写盘。
    - **流式写入偏慢的原因没有查：** 9800X3D 上流式写入只有 30 GB/s（单线程和 8 线程相同），C 运行库的 `memset` 和 `rep stosb` 也是这个速度，而 `CLZERO` 能到 75 GB/s。
    - **L2 曲线不平的原因没有查：** 64–384 KiB 为 230–245 GB/s，512–768 KiB 为 300–320 GB/s，与测试程序的分片方式无关。
    - **没有实测过的平台：** macOS、Windows ARM64、真实的 ARM 设备（包括 Jetson）、带 L4 的 CPU、大小核 CPU、超过 64 个逻辑处理器的机器、多路 NUMA 机器。
    - **GUI：** 鼠标悬停提示和日文界面没有实际查看；曲线图只显示最近一次运行，不能从 History 重画。
    - **页面文件速度没有测**，原因见命令行文档；如果需要，应当另做一个硬盘测试。
    - **没有做随机访问延迟测试**，所以不能直接量出各级缓存的访问延迟。
- [ ] **暂缓（用户 2026-09-30 决定）— `stream_v2`**：把粒子测试改为整粒子读写，每粒子流量确定为 64 B。这会让新成绩与旧成绩无法直接比较（AMD 核显上旧 kernel 慢 35–43%，不能统一乘以 1.6 换算），所以暂不做，v1 保持不变。
- [ ] **紧接 v2 — VRAM Integrity Soak**：新增独立 `vram_memtest`，按显存 budget 的安全比例分块写入 address/random/walking-bit 等 pattern、设备端读回校验并累计错误，持续到用户停止；区分独显 VRAM 与 UMA，共享内存不能误标为显存，OOM/device-lost 必须可恢复。原始 `stream` 继续是 15 秒带宽成绩，不能冒充显存正确性测试。
- [x] **换机发布构建链**：Windows Vulkan loader 已 delay-load+双重 guard；官方 RenderDoc 1.45、self-contained WinUI/CLI、MSVC runtime、511-file stage、逐文件 SHA、ZIP 解包复核、Inno Setup 6.7.3、最终 ZIP/Setup/SHA256SUMS/release-assets 均已实际生成。候选位于 `out/release/windows-x64`；Setup SHA-256 `f301426776b8ad2bd816a3f6de55463fd4b2f6be0ca3c7f691bfd8108aca0436`。
- [ ] **公开发布 gate**：用户确认根项目 LICENSE；配置 Authenticode signing；冻结并接入 `report_worker.exe`；在无 VS/Python/Vulkan SDK/RenderDoc/VC Redist 的干净 Windows 10/11 上验证安装/升级/卸载、无 `vulkan-1.dll` 的 DX11/WARP 启动、GUI orchestration、四 API 第 5 秒 bundled RenderDoc 抓帧；再用 clean commit/tag 重建，移除 `sourceTreeDirty=true` 后上传 GitHub Release。
- [ ] 当前仓库新增原生 WebGPU 后端：优先固定 Dawn 版本并依次移植 Stream、GPU Burn、Cinematic Liquid v2；结果必须记录 Dawn/底层 D3D12/Vulkan/Metal、实现版本、timestamp 模式与精度，不能只写 `WebGPU` 后与原生成绩混排。
- [ ] 第二阶段建立 `/web` 浏览器前端，共享 WGSL/workload manifest；浏览器结果单独分组，不承诺“全部 GPU 精确选择”或第 5 秒 RenderDoc。
- [ ] TriangleBin WebGPU：建议另建正式 fork/独立 `TriangleBin-WebGPU` 仓库，保留上游 MIT 与历史，只移植 atomic-counter shade-order 核心并以 HTML/WGSL 重写 UI；当前项目仅在 Architecture Tools 中链接或导入结果，不直接合并源码。

## 平台移植优先级（用户锁定；2026-07-19 将 Win7 GUI 挪到 PS3 前）

顺序：**Win ARM64 → macOS → Android → iOS → Debian Linux → WebGPU → HarmonyOS PC / 鸿蒙 → Windows 7 专用 GUI（Aero）→ PS3（探索性）→ Lumia 1520 legacy WP（探索性）→ iPhone 4 legacy iOS（探索性）→ Dual-GPU Collaboration（双卡协作，功能项）**。除现有隔离的 HarmonyOS Vulkan 粒子 demo 外，完整产品移植均未开始；该 demo 不等于 workload suite 已移植。不改变上方产品主线的切片顺序，平台移植在其后展开。逐平台落地时：能力不齐明确 unsupported、不静默 fallback；计时/抓帧模型不同的实现必须使用新 `workloadVersion` 独立成组，现有 Windows 成绩组的 A/B 对比不受影响。详细逐平台要点见 `HANDOFF.md` 目标 C。

- [x] **1. Win ARM64**：原生 CLI/WinUI + 原生 ARM64 中英双语 Setup 闭环已落地；x64 使用同一安装器实现，WiX MSI 仅为内嵌中间层；clean-machine/签名仍开放。
- [ ] **2. macOS（下一平台刀）**：
  - [x] Metal Particle 合同代码（离屏 / Unified-memory / capture 诚实）— **待真 Mac 15s 验收**
  - [x] Metal GPU Burn（`gpu_burn.metal` + 双 pass）— **待真 Mac 验收**
  - [x] SwiftUI ↔ WinUI 真对齐（Run/CPU/History/Duration/Capture/API 多选）— **待 Mac 编译验收**
  - [x] Metal 液体：raymarch present（`liquidFragment` / `_metal_preview`）— **待真 Mac 编译与冒烟**
  - [ ] 真 Mac 三主项冒烟 + 15s 正式流程
  - [x] CPU 页的缓存 / 内存带宽测试和三个页面的图表（2026-10-06）— **待 Mac 编译验收**。本轮在 Linux 容器里做类型检查时发现 `BenchEngine.swift` 有一处无法编译、一处会让界面卡死，都已修掉；`ContentView`、`HistoryView`、`SettingsView`、`AboutView` 没有检查过，第一次用 Xcode 编译时可能还会报错。
  - [x] `MTLCaptureManager`(.gputrace) 接线（F12/`--capture`/`--capture-frame`）— **待真 Mac 验证**
- [ ] **3. Android**：RenderDoc Android 远程抓帧；评估温控对 15 秒 Burst 语义的影响；GLES 降级 / 液体 / 正式移动 duration。
  - [x] **Vulkan 垂直切片（2026-07-24）**：ANativeWindow + `gpu_engine` + Compose GPU 页 Run/Stop（stream / gpu_burn light=16，默认 3s）。**Windows 本机 `assembleDebug` 通过；未真机。** 引擎真实写出 `*_android_preview`。
  - [ ] 主路径补齐：GL ES 3.1/3.2 后端（GLSL ES 重写、EGL、`EXT_disjoint_timer_query` 探测）；设备分界线为 **K1 及以后 = 主路径，Tegra 3/4 = legacy tier**。
  - [ ] **SDK/ABI 合同（2026-07-20 锁定）**：主包 minSdk 21 / targetSdk 最新（脚手架已抬到 minSdk 23）；NDK r27+ 开 16 KB 页对齐；结果写应用专属目录；Vulkan 运行时门控（API ≥24 + dlopen 成功，否则 ES 3.1）；四 ABI 全原生编译 **armeabi-v7a / arm64-v8a / x86 / x86_64**，结果 metadata 记录真实 ABI；legacy APK 仅 armeabi-v7a、targetSdk 可较低。详见 HANDOFF 目标 C 第 3 条。
  - [x] **UI 壳（2026-07-20/24）**：Compose + Material 3 四页；动态取色 12+；满载可 Stop。registry 仅展示已接线 GPU 项及真实 `*_android_preview` 字符串。
  - [ ] **ES 2.0 legacy tier（用户 2026-07-20 锁定底线：Tegra 3/4 必须支持）**：简化 fragment-only GPU Burn（避开 highp，Tegra 3 仅 FP20）新组 `gpu_burn_es2_legacy_v1`；粒子改 CPU(NEON) 物理 + GPU 渲染或纯 fragment 填充率测试（新 workloadVersion，不与 `stream` 混排）；液体 unsupported；墙钟计时、无 RenderDoc（用户已确认不要求）；独立 legacy APK（设备停在 Android 4.1–5.1，minSdk 约束），主包不背包袱。详见 HANDOFF 目标 C 第 3 条。
  - [ ] **Android 真机远程测试（adb；用户 2026-09-30 排入）**：
    - **模拟器（2026-10-07）：** 已在 Android 17 和 Android 7.0 的模拟器上跑通，并修了三个只有运行才能发现的问题，见 HANDOFF 第 10 节。下面说的"真机"仍然没有做。
    - **现状：** App 从没在真机上跑过。GPU 页接入了 Particle 和 GPU Burn（固定 16 步），默认 3 秒预览，成绩带 `_android_preview` 后缀；CPU 页在 2026-10-06 接入了算力测试和缓存 / 内存带宽测试，并带图表。2026-10-07 接入了流体、显存 / GPU 缓存带宽，以及粒子和 GPU Burn 的更重档位，GPU 测试改为全屏运行，都在模拟器上跑过。
    - **用户在手机上操作：** 打开"开发者选项"和"USB 调试"，插线后在手机弹窗里点"允许 USB 调试"。这一步只能由用户确认。
    - **Claude 用 adb 完成：**
      - 编译 `:app:assembleDebug`，再 `adb install` 安装并用 `adb shell am start` 启动；
      - ~~用 `adb shell input` 模拟点击~~：2026-10-06 起 debug 包支持启动参数，`adb shell am start -S -n com.mangekyo.benchmark/.MainActivity --es run <测试>` 打开后自动开跑，用法见 `android/README.md`；
      - 用 `adb logcat -s Mangekyo MangekyoEngine` 看引擎输出和结束标记（2026-10-06 起引擎输出同时写入 logcat），用 `adb exec-out screencap` 截图；
      - debug 包可用 `run-as` 从应用专属目录取回 `results.json`。
    - **CPU 测试的另一条路：** 引擎已有 Android 的 CPU 绑核代码，可以用 NDK 编一个命令行程序，推到 `/data/local/tmp` 后用 `adb shell` 直接跑。目前 CMake 在 Android 下不生成命令行目标，需要小改。
    - **验收：** 首次真机冒烟（启动、跑完、写出结果），并记录温控降频对成绩的影响。启动参数和 logcat 输出只编译通过，也要在这次冒烟里一并确认。成绩里的机型、芯片、系统版本和内存（2026-10-06 加入）同样没有在真机上看过，一并核对。
- [ ] **4. iOS**（**源码垂直切片已写；本机未编译、未设备验收**；排在 Android 后）：**最低 iOS 16**；仅 Metal；`MTLCaptureManager`；SwiftUI；App Store。**iOS 26/27 = Liquid Glass**，16–25 Material。引擎写出 `*_ios_preview`。规格见 `HANDOFF.md` §3.0.3；状态见 `ios-gui/README.md`。
  - [ ] **iPhone 真机远程测试（需要一台 Mac；用户 2026-09-30 排入）**：
    - **前提：** iOS 应用只能在 Mac 上用 Xcode 编译和签名，这台 Windows 工作站做不到。iOS 代码目前从没编译过，所以第一步是编译通过。
    - **用户在 Mac 和 iPhone 上操作：**
      - Mac 打开"远程登录"（SSH），并配置密钥登录；
      - 在 Xcode 里登录 Apple ID（免费账号签名 7 天过期）；
      - iPhone 打开"开发者模式"，并信任这台 Mac。
    - **Claude 通过 SSH 连进 Mac 完成：**
      - 用 `xcodegen` 生成工程，再用 `xcodebuild` 编译；
      - 用 `xcrun devicectl device install app` 安装，用 `xcrun devicectl device process launch` 启动；
      - 从应用沙盒拷回结果文件。
    - **自动开跑：** iOS 没有 `adb input` 那样简单的点击工具，需要给 App 加一个启动参数让它自动开跑。
    - **现状：** 2026-10-07 运行页改成和 Android 相同的六项测试（粒子两档、GPU Burn 三档、流体）加全屏运行；只做过 Swift 类型检查，没有用 Xcode 编译过，CPU 模式也没验收。没有显存带宽测试（只有 Vulkan 版）。
- [ ] **5. Debian Linux**：构建修正、`.deb` 打包、CI 与实机验证（后端/GLFW/RenderDoc/XDG 路径均已有，摩擦最低）。
  - [x] **2026-09-30 构建修复**（提交 `a894c8a`）：Ubuntu 20.04 / JetPack 5 可以编译和链接（补 `<cstring>`，链接 Threads 与 dl）。只做了容器验证：x86_64 完整构建 + aarch64 交叉编译全部引擎源文件。llvmpipe/lavapipe 的成绩现在标记为软件渲染。步骤见 [`building.md`](building.md) 的 Jetson 一节。
  - [ ] **NVIDIA Jetson Xavier NX 实机验收（NV 开发板；用户 2026-09-30 排入）**：
    - **用户准备：**
      - 刷好 JetPack 5.1.x，登录桌面（GPU 测试需要 `DISPLAY=:0`）；
      - 用 micro-USB 线（地址 `192.168.55.1`）或局域网连接这台电脑，并配置 SSH 密钥登录（Claude 不能代输密码）；
      - 需要 sudo 的步骤由用户执行：`apt install` 装依赖，`nvpmodel` 切到 6 核模式，然后运行 `jetson_clocks`。
    - **Claude 通过 SSH 完成：**
      - 编译（用 pip 安装的 CMake），并从 Windows 构建中复制 `.spv`；
      - 跑 Particle（Vulkan + OpenGL，1M 和 16M）、GPU Burn 64 步（Vulkan + OpenGL）、流体（Vulkan）、正式 CPU 测试（15 s / 0.2 s）；
      - 可选：用 lavapipe 跑一遍软件渲染对比，其中流体用 `--time 120`；
      - 取回 `~/.local/share/GpuComputeBenchmark/results/results.json`。
    - **合理性检查：**
      - Particle v1 分数超过 51.2 GB/s（LPDDR4x 理论值）就说明计时或统计出错；如果实际流量同样是每粒子 64 B，v1 分数的实际上限约为 32 GB/s；
      - 流体预计约 1 FPS；
      - CPU 应得到 6 行单核结果。
    - **顺带：** 在 Volta 核显上重复粒子字节数对照实验（原 kernel 对比整粒子读写），补齐第三个厂商架构的数据。
- [ ] **6. WebGPU**：按既定路线——capability registry P0 → 固定 Dawn 版本原生后端（Stream → GPU Burn → Cinematic Liquid）→ `/web` 浏览器前端；独立版本 id（`stream_webgpu_v1` 等），无可靠 timestamp 不产生正式 score（对应上方“原生 WebGPU 后端”与 `/web` 两条任务）。
- [ ] **7. HarmonyOS PC / 鸿蒙**：把 `ohos/` 升级为正式产品端口。
  - [x] **接入共享引擎（2026-10-06）**：原来的独立粒子演示已换成和安卓版对齐的 App（粒子测试、GPU Burn、CPU 算力测试、缓存 / 内存带宽扫描、历史、图表、中英文界面）。`scripts\build-ohos.ps1` 用 DevEco Studio 26.0 的工具链编出未签名的 HAP。**没有运行过。**
  - [ ] **上机验证**：
    - **用户要做的：** 在 DevEco Studio 里登录华为账号并生成自动签名；连接鸿蒙设备（打开开发者模式和 USB 调试），或者在 Device Manager 里下载模拟器镜像。
    - **要确认的：** 界面显示和图表；Vulkan 能否创建 `VK_OHOS_surface`；应用沙箱是否允许绑核（`sched_setaffinity`）和读取缓存容量；`hdc` 启动参数和 hilog 输出；成绩里的机型、系统版本和内存是否正确，芯片名能否从 `/proc/cpuinfo` 读到（系统没有提供芯片名的接口）。
  - [x] **流体和显存 / GPU 缓存带宽（2026-10-07）**：和 Android 一起接入，只编译通过，没有运行过。
  - [ ] **还没有做：** 抓帧编排、正式的结果合同（现在的成绩都带 `_ohos_preview` 后缀）、签名和发布包、CPU 测试的取消。
- [ ] **8. Windows 7 专用 GUI（PS3 之前）**：独立 Win32/DWM 前端与安装包，共享 `gpu_engine`/CLI/成绩 schema；Aero 能力探测与无 Aero 回退；不得携带 WinUI/WinAppSDK。
- [ ] **9. PS3（探索性，永不进成绩体系）**：仅 homebrew（PSL1GHT/RSXGL）；RSX 无 compute/原子/GPU timestamp，PSGL≈GL ES 1.0+Cg，三主测试不可直移；至多独立仓库的固定管线情怀 demo。
- [ ] **10. Lumia 1520 legacy Windows Phone（探索性，永不进成绩体系；2026-07-20 排入）**：骁龙 800 / Adreno 330,但 WP8.1/W10M 驱动仅暴露 D3D11 **FL 9_3**(无 compute/UAV)→ 形态同 ES 2.0 legacy tier:简化 fragment Burn、粒子 CPU(NEON) 物理、液体 unsupported;WinRT/UWP + CoreWindow 前端,VS2015/VS2013 工具链,开发者模式侧载(商店已死);墙钟计时,成绩自成体系;shader 优先经 archived ANGLE 复用 Tegra 3/4 的 GLSL ES legacy shader,备选 HLSL 9_3 重写;独立小工程。详见 HANDOFF 目标 C 第 10 条。
- [ ] **11. iPhone 4 legacy iOS（探索性，永不进成绩体系；2026-07-20 排入）**：OS X 10.11 + Xcode 7.3.1（free provisioning、armv7、deployment iOS 7.x，需老 Intel Mac/虚拟机）；不精简主工程，独立仓库或 `legacy-ios/` 小工程，仅复用 Android ES 2.0 legacy tier 的 GLSL ES shader/workload 定义；EAGL + UIKit + ObjC 宿主，粒子 CPU 物理，SGX535 单独更低预设；墙钟计时、无抓帧、成绩不进主 History；仅自签侧载（7 天过期），无 App Store。主 iOS 计划（iOS 16 + Metal）不受影响。详见 HANDOFF 目标 C 第 11 条。
- [ ] **12. Dual-GPU Collaboration（双卡协作模式，功能项；首个验证目标 Boot Camp 下 Mac Pro 2013 双 D700）**：Plasma/GPU Burn 的 DX12 AFR 已在强制 4 flights、禁用 RenderDoc 后实测约 2×；DX12 SFR 也已完成 50/50 半屏 + Tier-1 cross-node buffer 合成，16 steps 为 0.62× 负优化、128 steps 为 1.47×（仍低于 AFR），隔离为 `..._sfr2`。Vulkan 因 AMD 20.45.40.15 只暴露一个 graphics queue 而串行。DX11 窗口隐式 AFR 无提升；最终 AMD AGS 6.3.1 探针虽枚举到两张 D700，但显式/驱动 AFR 都返回 `crossfireAPI=0`、`crossfireGPUCount=1`，本机 FireGL 栈已判定不可用。OpenGL 虽由 `WGL_AMD_gpu_association` 枚举到两个 ID，但驱动拒绝为 GPU 2 创建 associated context，显式 SFR 已判定不可用并撤回原型。后续仅覆盖：(a) Original Particle 固定总粒子数分片并合成同一帧；(b) 当前 Cinematic Liquid 只验证模拟/渲染跨帧流水线，必须证明同一固定 workload 加速。传统 CrossFire/隐式 AFR 不可由应用强制；N-body、重复液体模拟、液体域分解和长时双卡烤机均不做。

> 状态提示：请先阅读根目录 [`HANDOFF.md`](../HANDOFF.md)。当前事实、两条产品主线、P0 阻塞和下一实现切片以 HANDOFF 为准；本文件保留专题任务与历史上下文。每次工作应先更新 HANDOFF，再同步这里。

## Metal 自动化捕获与分析（macOS）

### 前置条件
- [ ] 确认是否安装 Xcode（`.gputrace` 导出和可视化需要）
- [ ] 安装 MoltenVK（如需 macOS 上跑 Vulkan 后端）

### 实现步骤

#### 1. MTLCaptureManager 集成（C++ / ObjC）
- [x] 在 `metal_backend.mm` 中添加 `MTLCaptureManager` 支持
- [x] 复用现有 `--capture` / F12 / `--capture-frame`，Metal 走 MTLCaptureManager
- [x] 导出 `.gputrace` 到 PathService captures 目录（`Metal_<GPU>_<ms>.gputrace`）
- [ ] 真 Mac 验证抓帧文件可被 Xcode 打开

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
