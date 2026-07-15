# Project Handoff — 当前事实、两条主线与下一步

> 最后更新：2026-07-16（Australia/Sydney）
> 分支 / 提交：`main` / `3237545`（本轮实现尚未提交，工作树为 dirty）
> 本文件是项目的**首要进度与交接入口**。后续 AI 或开发者开始工作前先完整阅读，结束工作前优先更新本文件，再更新专题 TODO、roadmap 和 README。

## 1. 交接规则

1. 事实优先级：**可复现测试结果 > 当前源码 > 本文件 > 专题 TODO > roadmap > README**。
2. “代码已写”“可编译”“已实机运行”“跨 API 可比”“GUI 已接入”“可安装发布”是六种不同状态，不得混写成“已完成”。
3. 新发现应带文件与行号；完成项应带命令、机器/API、结果或产物路径。
4. 每次会话结束前至少更新：`当前结论`、`正在进行`、`下一步`、`验证记录`。
5. 不删除原始粒子测试，不改变其历史 workload id（`stream`）、确定性 seed、默认参数或旧结果含义，除非同时做结果 schema/version 迁移。
6. GUI 是今后的主入口。CLI、WinUI、macOS GUI、图表和结果模型必须从同一份测试 metadata/registry 获取能力，避免再次出现 7/5/1 套状态漂移。

## 2. 用户锁定的目标（A/B 两条产品主线 + C 平台移植优先级）

### 目标 A — 测试产品线

- 保留原始粒子测试，继续支持当前电脑的全部可用 GPU、软件设备/WARP 和图形 API。
- 固定流程维持 **15 秒运行 + 第 5 秒 RenderDoc 抓取完整一帧**。
- 新增能达到 FurMark 类负载水平、但不照搬甜甜圈造型的短时极限压力测试。
- 新增真正的 3DMark 类综合图形测试；流体可以成为综合场景的重要 pass，而不是单独替代核心压力测试。
- 当前效果不够好的新原型保留到 `Other / Advanced / Legacy`，不能删除，也不能冒充正式主分数。
- 所有正式能力必须同步进入 GUI。

### 目标 B — 一装即用

- 发布的是预编译、依赖与资源均已配置好的应用，而不是要求用户复制作者开发环境。
- 换一台干净电脑后，用户不需要 Visual Studio、vcpkg、Vulkan SDK、shader compiler、Python 或手工补丁即可运行。
- RenderDoc 抓取与报告链也由安装包提供或受控安装，行为等价于当前开发机配置完成后的效果。

### 目标 C — 平台移植优先级（2026-07-16 用户锁定）

用户锁定的移植顺序：**1. Windows on ARM (ARM64) → 2. macOS → 3. Android → 4. iOS → 5. Debian Linux → 6. WebGPU → 7. HarmonyOS PC / 鸿蒙 → 8. PS3（探索性）→ 9. Dual-GPU Aggregate（双卡合力模式，功能项）**（2026-07-16 用户把 WebGPU 排入并位于 PS3 之前、把 HarmonyOS 排在 WebGPU 后与 PS3 前；同日把 Dual-GPU Aggregate 排在 PS3 之后）。除仓库已有的隔离 HarmonyOS Vulkan 粒子原型外，均未开始完整产品移植；该原型不等于主 workload suite 已移植。

通用规则：
- 逐平台落地时沿用现有合同规则：能力不齐明确 unsupported、不静默 fallback；**计时/抓帧模型不同的实现必须使用新 `workloadVersion` 独立成组**，现有 Windows 成绩组（`stream`、`gpu_burn_v1`、cinematic liquid 各版本）的 A/B 对比不受任何影响。
- 本列表不改变第 9 节当前切片顺序（液体 v7 正式验收、自由/无限模式、发布收口在前）；平台移植在其后展开，除非用户再调整。

逐平台要点：
1. **Win ARM64**：预计仅构建/打包移植——CLI/WinUI 出 ARM64 目标（当前仅 x64）、vcpkg `arm64-windows` triplet、WinAppSDK ARM64 payload；实机验证 Vulkan（Adreno）、DX12/DX11、WARP 与 OpenGL 兼容层。核心代码与成绩合同预计不动。
2. **macOS**：Metal 后端目前只覆盖粒子 workload（GPU Burn 明确 unsupported、液体无 Metal 实现）；需要主 workload 的 Metal 移植、SwiftUI GUI 与统一 registry 对齐、`MTLCaptureManager`(.gputrace) 替代 RenderDoc；PathService 路径已就绪。
3. **Android**：复用 Vulkan 后端；GLFW 不支持 Android，需 NativeActivity/ANativeWindow 表面层与新前端；RenderDoc 支持 Android 远程抓帧；须评估温控降频对 15 秒 Burst 语义的影响（可能需要独立移动端 duration 合同）。
4. **iOS**：仅 Metal（或 MoltenVK）；无 RenderDoc，抓帧走 `MTLCaptureManager`；与 macOS 共享 SwiftUI 前端；受 App Store 分发约束。
5. **Debian Linux**：技术摩擦最低——Vulkan/OpenGL 后端、GLFW、RenderDoc、XDG 数据路径全部已有；主要是构建修正、`.deb` 打包、CI 与实机验证。
6. **WebGPU**：按第 11 节既定路线执行——先统一 capability registry（P0），再固定 Dawn 版本的原生后端，依次移植 Stream、GPU Burn、Cinematic Liquid，最后 `/web` 浏览器前端。结果使用独立临时版本 id（如 `stream_webgpu_v1`），记录 `apiImplementation/underlyingBackend/timingMode`；timestamp-query 是可选能力，没有可靠 GPU timestamp 不产生正式 score；浏览器无 RenderDoc，抓帧标注不可用。
7. **HarmonyOS PC / 鸿蒙**：仓库已有 `ohos/` 的独立 Vulkan 粒子 demo，但没有统一 workload registry、GPU Burn/Cinematic Liquid、主 CLI/GUI、15 秒结果合同或抓帧编排。该移植项是把它升级为与主产品边界一致的正式端口；在能力与捕获模型明确前必须使用独立结果组，不能把现有 demo 写成已完成移植。
8. **PS3（探索性，永不进成绩体系）**：官方开发授权已停止，只能 homebrew（PSL1GHT/RSXGL）；RSX 无 compute/SSBO/原子/GPU timestamp，PSGL≈OpenGL ES 1.0+Cg 而非桌面 GL 4.3，三个主测试均不可直移。至多做隔离的固定管线情怀 demo，建议独立仓库，不进入正式成绩合同与 GUI 主界面。
9. **Dual-GPU Aggregate（双卡合力模式，功能项而非 OS 移植；首个验证目标：Boot Camp Windows 下的 Mac Pro 2013 双 FirePro D700）**：显式引擎级多 GPU 协作——驱动层 CrossFire 对自研引擎无效，不作依赖。切片顺序：(a) `stream` headless 双独立设备聚合：一进程两个 device 各算一半粒子、帧边界 CPU 同步、吞吐相加，新成绩组 `stream_dualgpu_v1`，结果必须记录两张 adapter 的完整元数据；(b) N-body 双卡（每步交换位置，~1MB/步）；(c) GPU Burn 分屏 SFR 或 AFR（DX12 unlinked 显式多适配器 + 跨适配器堆）。**不做** DX12 LDA / Vulkan device-group 依赖（D700 22.6.1 老驱动是否暴露不确定），**不做** Cinematic Liquid 域分解（每 substep 跨 PCIe 网格同步属科研级，不值）。单卡成绩组不受影响；混插不同型号也应能跑（聚合分数需标注非对称配置）。安全注意：双 D700 同时满载是 Mac Pro 2013 已知散热死穴，15 秒 Burst 可以，禁止双卡长时烤机。

## 3. 当前结论（先读这一节）

### 3.1 产品判断

建议最终只把三条路线放在主界面：

| 主测试 | 定位 | 当前状态 |
|---|---|---|
| **Particle (Original / Baseline)** | 原始粒子计算+绘制；历史基线，实际偏显存/内存带宽 | 已保留稳定 id `stream`；GUI 明确标为 Memory Throughput |
| **GPU Burn (15s Burst)** | 原创实心 Plasma Bloom 图形压力场景；后续扩展 CoreBurn + MixedBurn 与错误校验 | **`gpu_burn_v1` 已实现**：Vulkan/DX12/DX11/OpenGL + WARP；RTX 5090 自动标定实测连续 8 次 NVML=99%、约 600W；Metal 明确 unsupported |
| **Cinematic Liquid** | 固定镜头的真实 3D 粒子液体 + 粒子重建密度体积自由表面；后续叠加完整综合场景 | **v1 正式合同与历史 v2 optics_v4 正式成绩已验证并保留**；当前工作树为 `cinematic_liquid_v2_physical_scene_v7` / `shaderVersion=9` / `sceneVersion=4`：320,920 粒子、128x64x96 MLS-MPM、真实船/沉球/越沿响应、软 PVC 薄膜、程序化草地与大气天空。v7 仅通过构建/SPIR-V 和 6/8 秒自动 smoke，没有正式 15 秒或完整视觉验收；WinUI build 已通过，交互/history 与下列可靠性 P0 仍待验收 |

`15s` 应对外称为 **Burst / 短时峰值压力**，不能宣称完成热稳定认证。真正的热饱和、显存错误或超频稳定性通常需要可选的数分钟模式；这不改变用户要求的默认 15 秒流程。

### 3.1.1 2026-07-15 历史暂停点与恢复状态

用户曾明确要求**暂时停止新的实机测试、预览运行和 RenderDoc 抓帧**；该暂停在当时已遵守。此要求是历史上下文，不再是当前阻塞：用户随后先授权 duck family 视觉改造，之后又明确要求继续流体工作，本轮据此恢复了水体光学实现，并只做了必要的短 `_preview` 验证。

2026-07-15 晚间第一阶段：用户明确提出“照参考图优化鸭子造型，加大鸭子/小鸭子/子母鸭”，据此完成 duck family 场景改造（见 3.1.1a）及最短预览。该 v5 鸭群改动已原样保留到当前 v7；上一个四刚体基线 `cinematic_liquid_v2_surface_splat_optics_v4` / `shaderVersion=6` 及其 15 秒正式结果永久保留为独立成绩组。

### 3.1.1a 2026-07-15 Duck family 场景改造（v5）

- 鸭子 SDF 重做：旧的 7 块硬拼椭球（肚+胸+尾+双翅，接缝生硬）替换为 smooth-min 融合的经典大黄鸭（丰满蛋形身体 + 上翘尾 + 大圆头 + 扁宽橙嘴）。三处实现保持同一份形状数学：`shaders/mls_mpm_grid_update_v2.comp`、`shaders/mls_mpm_g2p_v2.comp`（碰撞）与 `shaders/cinematic_liquid_render_v2.frag`（渲染改为对同一 SDF 做 72 步 sphere-trace，黑眼珠/白高光/橙嘴均为贴花上色，不再是独立几何）。
- 新增 3 只约 0.45 倍的小鸭子（刚体 4-6，`kCinematicLiquidV2BodyCount` 4→7）；sink sphere 的释放编排硬编码在 lane 3，因此新刚体只允许追加在 index 3 之后。小鸭质量 ~1.82、扶正力矩更强（shape0.w=26）、线/角阻尼更高，初始间距均大于两两 bounding 半径之和，避免出生瞬间被 pair solver 弹开。
- grid-update / g2p 的刚体循环加入保守 bounding-sphere 剔除，七刚体循环对绝大多数节点/粒子只付一次距离平方判断。
- 结果身份升级：`workloadVersion=cinematic_liquid_v2_duck_family_v5`（预览为 `..._preview`）、`shaderVersion=7`、`sceneVersion=3`；不得与 optics_v4 混分。

### 3.1.1b 2026-07-15 Iterative optics v6（历史预览合同）

- 用户调整后的鸭子及 duck family v5 的 7 刚体/SDF/碰撞布局全部保留；本轮只在其上升级水体重建与光学，没有回退或覆盖鸭子造型。
- 当时结果身份为 `workloadVersion=cinematic_liquid_v2_iterative_optics_v6`（短运行写为 `..._preview`）、`shaderVersion=8`、`sceneVersion=3`。它与 v1、历史 optics_v4 正式组、duck-family-v5 预览组及当前 v7 均不得混分。
- 表面重建升级为 5x5x5 binomial filter（`mix=0.90`）；ray path 最多处理 4 次界面，逐段使用 Fresnel/Snell 与 Beer–Lambert，并在每一段重新做刚体/池体/地面深度排序。当时消光系数为 `extinction=(30,10,8)`，输出采用 linear exposure + sRGB，不再使用 ACES；密度体积边界强制归零，避免 clamp 边缘形成实体水盒。
- 最终短预览结果 `20260715-221447-024`（RTX 5090 / Vulkan / 6 秒、5.1 秒抓帧）：Compute 9.450 ms、Render 3.003 ms、Total 12.454 ms、`257.01 MParticle-step/s`、284 measured frames；0.117 秒抓帧开销已排除，1 attempt/1 saved。核验图为 `rdoc_captures/cinematic-liquid-v2-5s-iterative-optics-v6-final-preview.png`。
- **v6 从未运行正式 15 秒流程，没有正式成绩。** 当时 CLI Release 与 WinUI Release x64 已构建通过；该历史结果不能转记为 v7。

### 3.1.1c 2026-07-15 Physical scene v7（当前工作树）

- 当前结果身份为 `workloadVersion=cinematic_liquid_v2_physical_scene_v7`、`shaderVersion=9`、`sceneVersion=4`；固定 128x64x96 网格、10 substeps 与 **320,920 粒子**，分解为 142x14x98 base（194,824）+ 48x37x71 dam（126,096）。v1/v4/v5/v6 的正式结果或预览全部原样隔离。
- 用户鸭子和 7 刚体 index ABI 原样保留：母鸭 index 0、彩球 index 1、船 index 2、沉球 index 3、小鸭 index 4-6。船由硬锚定改为 34 kg 有限质量刚体和软系泊，推进器反冲与流体冲量可驱动/摇摆；这只说明实现路径已接通，**尚未视觉验收船的实际轨迹**。
- index 3 沉球仍为 1.06 水密度比，4.28 秒释放，使用 -9.81 重力与 0.015 空气阻尼；材料水阻按 displaced mass 和浸没率门控，以避免未入水时就产生水下式阻尼。沉球入水 crown/whitewater 来自真实 GPU body state 与局部流体响应，不是 fragment shader 画出的假水花；当前仍没有 secondary-spray 粒子 pass。
- 池体使用有限高度的内嵌碰撞壁（inset 0.22）；外层 simulation catch band 允许粒子真实越过池沿并落到地面，但带宽有限，不能描述成无限流体域。5x5x5 binomial surface resolve 自适应保留低支持喷滴。
- 前景侧壁为独立透明软 PVC 薄膜近似：IOR 1.50、Fresnel、弱吸收和程序化 wrinkle；这**不是完整的 PVC 多介质 ray path**。池底内衬使用独立 hit 分类，使水下焦散落到实际可见内衬而非被遮住的草地平面。场景加入无限程序化草地与大气天空/云，没有引入 cubemap 资产。
- 当前只完成 shader 编译、最终 6 个 SPIR-V 校验、CLI Release 与 WinUI Release x64 构建；WinUI 为 0 error，仅有既有 MSB8027/C4996/LNK4042 类 warning（最新增量构建显示 2 个重复 WinAppSDK warning，源文件重编时曾显示 4 个）。只运行了若干 `--time 6` / `--time 8` 自动停止 smoke：**没有正式 15 秒、没有可靠保存的新 RenderDoc、没有完整视觉验收**。控制台 transient `241.13` 不是正式或持久结果。用户看到窗口约 2-3 秒后关闭，是测试脚本 `--time 8` 的自动生命周期，不是目前证实的崩溃；正式 GUI/无限模式仍待实现/验收。

“是否基本完成”必须按层次回答：

- **Cinematic Liquid v2 的 Vulkan benchmark vertical slice：结构上基本完成，但当前 physical-scene v7 尚未正式验收**。固定流程、独立成绩合同、宏观流体、七刚体、surface splat、CLI 与 WinUI 构建已有实现或历史验证记录；只有 optics_v4 有正式 15 秒成绩，v6 只有历史短预览，v7 只有自动停止 smoke。
- **用户要求的参考级水体视觉：未完成且未通过用户验收**。用户针对最终 optics v4 抓帧仍明确评价“水体太假”；不能用“结构比旧版改善”替代视觉验收。
- **完整产品：未基本完成**。最终 WinUI 交互/history、跨 GPU 固定时间推进、其余四后端、异常资源清理、安装器 clean-machine 验收以及无限/自由模式仍开放。

恢复前的只读上游源码对照已从 v6 保留到 v7：MIT [jeantimex/fluid raymarch](https://github.com/jeantimex/fluid/blob/9daf3ae2add7dedc3eadcc10da5e4a44ef1b771f/src/sph/3d/webgpu_raymarch/shaders/raymarch.wgsl) 使用最多 4 次界面迭代、Fresnel、Beer–Lambert 与环境光；其 [raymarch 配置](https://github.com/jeantimex/fluid/blob/9daf3ae2add7dedc3eadcc10da5e4a44ef1b771f/src/sph/3d/webgpu_raymarch/main.ts) 为 `densityTextureRes=150`、`stepSize=0.02`、`maxSteps=512`、`IOR=1.33`、`numRefractions=4`、`extinction=(12,4,4)`、`renderScale=0.5`。本项目没有 vendor 上游代码。v7 增加真实局部入水响应并保留低支持喷滴，但没有 secondary-spray 粒子且仍是 MLS-MPM；在可靠抓帧与完整视觉验收前，不得宣称已达到参考效果。下一项重大视觉工作仍应转为独立 SPH solver vertical slice，或把 secondary spray 做成真实版本化模拟 pass，而不是 fragment 装饰。

### 3.2 源码真实状态

核心枚举现有 10 个稳定 workload id：

| Workload id | 真实负载 | 后端代码状态 | 建议归类 |
|---|---|---|---|
| `stream` | 原始粒子 Euler 更新 + 点绘制；低算术强度，偏带宽 | Vulkan/DX12/DX11/OpenGL/Metal | **主界面：Particle (Original)** |
| `gpu_burn` | 原创 Plasma Bloom 实心七瓣晶核；2 次全屏 opaque draw、固定 step raymarch + 晶面/裂纹/纤维/电弧计算；自动标定到约 14 ms render | Vulkan/DX12/DX11/OpenGL；DX11/DX12 WARP；Metal 明确拒绝 | **主界面：GPU Burn v1 (15s Burst)** |
| `gpu_stress` | 独立 shader 的 4 次全屏 opaque overdraw；FP32/SFU/INT 循环，warmup 首秒自动标定到约 8 ms/draw-group | Vulkan/DX12/DX11/OpenGL；DX11/DX12 WARP；Metal 明确拒绝 | Other / Advanced：GraphicsBurn component |
| `nbody` | tiled all-pairs 粒子计算 | 五后端 | Other / Advanced Compute |
| `stress` | 全屏固定次数 fractal + `sin()`，fragment ALU/SFU | 五后端 | **Legacy Stress v1** |
| `synthpeak` | 寄存器内合成峰值循环 | 五后端，精度能力不同 | Other / Advanced Synthetic |
| `render3d` | 6 顶点实例化 billboard + 深度 | 五后端 | **Legacy 3D Prototype** |
| `volumetric` | 程序化 FBM 体积 raymarch | 五后端代码已接入，但仓库结果无验证记录 | Other / Experimental；未来综合场景 pass |
| `cinematic_liquid` | 3D MLS-MPM 粒子/网格 + 独立粒子 splat/binomial R32F 密度体积 + 最多 4 界面自由表面 ray path | Vulkan v1 与历史 optics_v4 正式 15 秒 + 5.1 秒 capture 已通过；dirty working tree 中当前 physical-scene v7 为 320,920 粒子、128x64x96、10 substeps、自适应 5x5x5 surface、物理船/沉球/越沿与扩展环境，但仅有 6/8 秒自动 smoke；DX12/DX11/OpenGL/Metal 尚未实现 | **主界面：Cinematic Liquid**；v1、optics_v4、duck-family-v5、iterative-optics-v6 与 `cinematic_liquid_v2_physical_scene_v7` 必须按版本分组；v7 正式 15 秒、可靠新 capture、WinUI 交互/history 与可靠性 P0 仍开放 |
| `fluid` | 旧 2D Stable Fluids 多 pass | **只有 Vulkan 原型，且当前不正确** | Other / Legacy；不得产生正式成绩 |

仓库内历史 `results/results.json` 仍为 232 条：`stream=224`、`nbody=5`、`stress=1`、`synthpeak=1`、`render3d=1`、`volumetric=0`、`fluid=0`。本轮 smoke 数据刻意写到 `out/*/results`，没有污染历史库。新结果 schema 为 v2，记录 workloadVersion、最终标定参数与 capture 状态；旧结果读取时仍按 schema v1 兼容。

`gpu_burn_v1` 是当前正式图形 Burst：画面没有环形孔洞，也不是粒子测试；score 为版本化的 `Gpix-step/s`。它在 RTX 5090 上达到真实 99% NVML 利用率，但 15 秒默认流程仍不是热稳定/硬件错误认证。`gpu_stress_v1` 保留为 Advanced **GraphicsBurn component**；两者都没有 GPU readback/CPU 对照，shader recurrence/checksum 目前只用于防止 dead-code elimination。`cinematic_liquid_v1` 的已验证成绩合同继续保留，不复用旧 `fluid` 的 2D dye score 或错误状态。v2 的正式历史成绩属于 `cinematic_liquid_v2_surface_splat_optics_v4`；当前代码身份是 `cinematic_liquid_v2_physical_scene_v7`，只有自动 smoke，不能沿用 v4 的正式成绩或 v6 的预览值。GUI 交互、跨 GPU 轨迹可比性和异常路径资源清理仍开放。

### 3.3 为什么现有新测试“效果不好”的判断成立

- 旧 `stress` 的固定循环适合可重复的 fragment ALU/SFU 微测试，但没有 compute+graphics 混合与错误校验，所以继续作为 Legacy Stress v1；新的 `gpu_stress` 没有复用或修改旧 fractal shader。
- `render3d` 只有相机朝向 billboard，没有 mesh、材质、纹理、灯光、阴影、环境光照或后处理，不能称为 3DMark 类场景（`shaders/render3d.vert:23-32`、`shaders/render3d.frag:8-12`）。
- `volumetric` 是可用的独立 raymarch 技术积木，但文档记录 RTX 5090 默认仅约 68% 利用率（`docs/TODO-volumetric-fluid.md:20`），不足以承担“绝对压力”主项。
- `fluid` 的 fragment shader 自己注明“这是 benchmark，不是漂亮 renderer”，只做最近邻 dye/speed 色带（`shaders/fluid_render.frag:18-32`）；当前也没有持续 force/dye injection。

### 3.4 CPU 补充测试（2026-07-16，Windows vertical slice 已跑通）

定位已锁定：CPU 只是 GPU 跑分项目的补充，不创建 3D 窗口，也不调用
RenderDoc。当前已同时提供原生 C++ CLI 与独立 WinUI CPU 页面：
`per-core` 按可用逻辑 CPU 逐个顺序测试并实时更新当前核心/总进度，
`multi` 同时使用全部可用逻辑 CPU，`all` 顺序执行两者；测量时长与预热
时长可调。逐核结果必须显示逻辑 CPU、物理核、SMT sibling 与可识别的
性能层级，另给逐核平均与多核结果，不能把用户给出的旧 Python EXE 分数
或算法当成本项目成绩合同。

**已完成且有证据的边界：**

- `cpu_mixed_v1` 使用原生 mixed integer/branch/FP32/FP64 依赖链、单调时钟、
  固定三轮中位数和 DCE checksum。checksum 不是 known-answer 正确性验证。
  正式合同固定为每项总测量 15.0 秒 + 预热 0.2 秒 + 三轮；其他时长均为
  `preview`。per-core 汇总是各逻辑处理器中位数的算术平均，multi 是全部
  逻辑处理器线程总吞吐的三轮中位数。
- CLI 已接 `--cpu-benchmark [per-core|multi|all]`、`--cpu-time`、
  `--cpu-warmup`、`--cpu-no-save`，并在 GLFW/GPU/API probe 前直接返回。
  `CPU_META`/`CPU_TOPOLOGY`/`CPU_PROGRESS`/`CPU_RESULT` 协议包含版本、
  formal/preview、round/median、affinity、valid、classification source；
  checksum 明标 `dce_sink`。
- Windows 优先 `GetSystemCpuSetInformation`，回退
  `GetLogicalProcessorInformationEx`；按 processor group 用
  `SetThreadGroupAffinity` 严格绑核并恢复。严格绑核失败会 `valid=0`、从汇总
  排除、CLI exit 3 且不保存。2026-07-16 的 Ryzen 7 9800X3D Release smoke
  正确枚举 16 logical / 8 physical / SMT2，16/16 逐核与 16/16 multi 均
  `affinity=strict`、exit 0。该 0.09 秒运行只证明路径闭环，不是正式成绩；
  >64 logical processor-group 与真实混合核机器仍未验收。
- WinUI CPU 页已有模式、Quick/Formal、时长/预热、逐核/总进度、逐逻辑核
  结果、汇总、raw output、Run/Cancel；它通过无窗口子进程读取上述协议。
  CLI 与 WinUI Release x64 均已构建。成功运行会把 per-core average 与
  multi summary 分别追加到现有 `results.json`/History；详细逐逻辑核行只在
  本次页面/协议中，不单独持久化。
- 持久化 `workloadVersion` 包含 kernel、affinity capability、formal/preview、
  round/time/warmup 以及 multi 是 standalone 还是 after-percore，避免 Quick、
  Formal、不同绑核能力和不同热状态顺序静默混排。

**仍开放的发布/跨平台边界：**

- 当前已有本机 CLI 与 WinUI build/smoke，不等于新的 ZIP/Inno Setup 已重建；
  公开安装包前须重新 stage，并在干净 Windows 机器验证安装后 GUI 能找到
  相邻 `gpu_benchmark.exe`、Run/Cancel、结果写入与卸载保留用户数据。
- Windows 的 `EfficiencyClass`、Linux/Android 的 `cpu_capacity`/最大频率只
  用来生成 `InferredPerformance/Efficiency/Middle/LPE` 排名标签，**不是**
  CPUID/SoC 官方核心身份，不能宣称已精确识别所有 P/E/Mid/LPE 变体。
- Linux 会枚举 allowed cpuset 并尝试 `pthread_setaffinity_np`，但在原生构建、
  容器/cpuset、恢复路径实测前仍是 best-effort。macOS 只能
  `scheduler_managed`，logical→physical/SMT 是估计。两者的 affinity capability
  已进入版本，不能与 Windows strict 成绩混排。
- Android/iOS 与 Web/WASM CPU 模式均未构建验收。浏览器只能看到暴露的
  logical concurrency，不能选择、硬绑定或可靠分类宿主核心；移动端还需独立
  热状态/前后台合同，禁止把当前源代码存在写成已完成端口。

## 4. P0 正确性阻塞项

仍开放的问题修复前，不应把旧 `fluid` 计入正式排行榜。GPU Burn
`gpu_burn_v1` 已作为带版本的新主项接入，不与 `gpu_stress_v1` 或旧 `stress` 混分。

### 4.1 Fluid 会产生未定义或伪造结果

1. divergence shader 把输出声明为 binding 2（`shaders/fluid_divergence.comp:8-13`），C++ 却把 `divBuf` 写到 binding 3（`src/vulkan_backend.cpp:1698-1703`）。
2. advect descriptor 永久绑定 A→B（`src/vulkan_backend.cpp:1698-1700`、`1899-1904`），后续却用 `uint(simTime * 60)` 的奇偶假定每帧已经 A/B 交换（`1923-1931`）。这不是实际 ping-pong，并且结果依赖帧率取整。
3. 最终 buffer 的 barrier 目标 stage 仍只有 compute（`1886-1896`），随后 fragment shader 直接读取（`1978-1999`）；缺少 compute-write → fragment-read 同步。
4. 每帧更新仍可能被在途帧使用的共用 descriptor set；应按 frames-in-flight 预建 descriptor 组合或隔离每帧 set。
5. ~~DX12/DX11/OpenGL/Metal 会静默 fallback。~~ **已修复**：这些后端现在明确抛出 unsupported，不再伪造 `GCell/s`；Vulkan fluid 的前四项正确性问题仍开放。

### 4.2 “全部 GPU × API + 第 5 秒抓帧”目前不成立

- ~~`--run-all --capture 5` 丢失 `captureAtSec`。~~ **已修复**：run-all 复制 capture、GPU Burn/GraphicsBurn 参数与 autotune 配置；任一矩阵项失败时现在返回非零，GUI 不再把部分成功冒充 Full Analysis 完成。
- ~~Windows OpenGL 会给每张硬件 GPU 建重复/错标项。~~ **已修复**：probe 只把 OpenGL 能力赋给 renderer 名称匹配的 adapter；2026-07-15 `gpu_burn --run-all` 实测只产生一条 RTX 5090 OpenGL 记录。
- 软件设备自动 backend 与显式 DX11/DX12 现在都正确进入 WARP，并清空硬件 LUID/VRAM 元数据；统一 capability registry 仍待实现。

### 4.3 15 秒的当前语义

- 默认是墙钟 15 秒，前 2 秒 warmup（`src/gpu_common.h:215-216`）。
- 第 5 秒从整个 run 开始计时（`src/app_base.cpp:318-349`）。
- 正式 FPS/score 统计从第 2 秒后开始，所以约统计 13 秒（`src/app_base.cpp:407-420`）。
- ~~RenderDoc 抓取帧进入统计。~~ **已修复**：抓帧尝试的墙钟开销、当前帧及随后至少 16 个异步 timestamp samples（取 `max(16, framesInFlight + 1)`）均从正式统计排除；这覆盖当前最大的 DX11 8-slot 与 OpenGL 4-slot query ring。结果分别记录 `captureAttempts`、成功的 `captureCount` 和“这次尝试是否已排除计分”的 `captureExcluded`；即使 `EndFrameCapture` 失败也不会污染成绩，但不会误报生成了 `.rdc`。

Windows 打包版 Vulkan 另有一个已修复的关键点：单独复制
`renderdoc.dll` 能取得 API，却不能自动成为 Vulkan capture layer。CLI 现在在 GPU
probe 前把随包 `tools/RenderDoc` 设为进程级 `VK_IMPLICIT_LAYER_PATH` 并启用
`renderdoc.json`，不写系统注册表；最终 staged 目录已实测四个 Windows API 均在
第 5 秒生成 `.rdc`。提权运行时 Vulkan Loader 会忽略这类环境路径，因此产品仍应
保持 `asInvoker`。

为了保留旧粒子成绩语义，`stream` 暂时保持“总墙钟 15 秒 / 第 5 秒抓帧 / 2 秒后开始统计”。若将来改成“2 秒预热 + 15 秒正式窗口”，必须增加 suite/result schema version，不能与旧分数混排。

### 4.4 Cinematic Liquid v2：历史 optics_v4 正式通过，当前 physical-scene v7 仅 smoke

已完成并验证的 Vulkan 切片：

- [x] 固定 128x64x96 MLS-MPM 网格、10 substeps 与 320,920 粒子；粒子分解为 142x14x98 的浅水底（194,824）和 48x37x71 的溃坝体（126,096）。当前固定参数为 stiffness 45,000、viscosity 0.035、maxSpeed 8。
- [x] 用户鸭子和七刚体 ABI 保留：船 index 2、沉球 index 3、小鸭 index 4-6。船由硬锚定改为 34 kg 有限质量/软系泊，推进器反冲可驱动和摇摆，但轨迹尚未视觉验收。沉球保持 1.06 水密度比、4.28 秒释放、-9.81 重力、0.015 空气阻尼；材料水阻按 displaced mass 与浸没率门控。
- [x] 池壁为有限高度、inset 0.22 的内嵌碰撞体；有限宽 outer catch band 允许真实越沿和落地，但不是无限流体域。render push 的 `pool.z` 已对齐真实 ground/pool-floor `y=0`，PVC bottom、liner、无限草地与物理粒子底面共面；pool ring separation 由 `2*tube` 推导，避免视觉池沿与物理地面漂移。
- [x] 当前表面不直接读取 MLS-MPM 网格质量：粒子以 Spiky² 核和 fixed-u32 原子独立 splat 到 128x64x96 R32F 密度体积，5x5x5 binomial resolve 会自适应保留低支持喷滴。沉球入水 crown/whitewater 来自真实 GPU 刚体状态与局部流体，不是 fragment 假水花；没有 secondary-spray 粒子 pass。
- [x] 当前 renderer 保留最多 4 次水/空气界面的 Fresnel/Snell、Beer–Lambert 与 opaque depth sorting，以及 `extinction=(30,10,8)`、linear exposure 和 density 边界归零。新增独立透明软 PVC 前景薄膜（IOR 1.50、Fresnel、弱吸收、wrinkle），但不是完整 PVC 多介质 ray path；环境为无限程序化草地与大气天空/云，没有 cubemap 资产。
- [x] 结果身份严格隔离：历史正式 `cinematic_liquid_v2_surface_splat_optics_v4` / `shaderVersion=6`；鸭群预览 `cinematic_liquid_v2_duck_family_v5` / `shaderVersion=7`；iterative optics v6 预览 / `shaderVersion=8`；当前 `cinematic_liquid_v2_physical_scene_v7` / `shaderVersion=9` / `sceneVersion=4`。
- [x] RTX 5090 / Vulkan / 正式墙钟 15 秒 + 5.1 秒 RenderDoc 已通过：结果 id `20260715-170629-492`，Compute 10.572 ms、Render 1.553 ms、Total 12.125 ms、`263.98 MParticle-step/s`、966 measured frames；capture 0.103 秒已排除，`captureAttempts=1`、成功保存 1 个。最终核验图为 `rdoc_captures/cinematic-liquid-v2-5s-formal-optics-v4.png`。
- [x] 上一条只验证 optics_v4。历史 v6 的 6 秒短预览为 `20260715-221447-024`：Compute 9.450 ms、Render 3.003 ms、Total 12.454 ms、`257.01 MParticle-step/s`；它属于 v6 `_preview`，不是正式成绩，更不能归到 v7。
- [x] 当前 v7 的 shader、最终 6 个 SPIR-V、CLI Release 与 WinUI Release x64 build 均通过；WinUI 0 error，仅有既有 MSB8027/C4996/LNK4042 类 warning（最新增量构建 2 个，源文件重编时曾为 4 个）。只运行若干 `--time 6/8` 自动停止 smoke；无正式 15 秒、无可靠保存的新 RenderDoc、无完整视觉验收。transient console `241.13` 不得记为正式/持久成绩；窗口数秒后关闭来自 `--time 8` 测试生命周期，不是已证实崩溃。

视觉结论必须诚实：v7 已补入真实局部入水响应并自适应保留低支持喷滴，但没有可靠的新抓帧或完整视觉验收，不能声称船运动、越沿、PVC 或水花最终效果已通过。底层仍是 MLS-MPM，且没有 secondary-spray 粒子；细薄水片、水滴、小尺度飞溅与整体自然度仍未证明达到用户给出的 [jeantimex/fluid](https://github.com/jeantimex/fluid) SPH 截图。下一项重大视觉路线仍建议独立 SPH solver vertical slice；若先补 secondary spray，也必须成为真实、可计时、版本化的模拟 pass。光追能改善反射/阴影/焦散，但不会把 MLS-MPM 动态本身变成 SPH。

仍开放、因此**不得把整个 v2 标为完成或跨 API 完成**的 P0：

- [ ] 为 `cinematic_liquid_v2_physical_scene_v7` 运行正式 15 秒 + 第 5 秒 RenderDoc 并建立独立正式结果；在完成前不得引用 optics_v4 的 `263.98 MParticle-step/s` 或 v6 `257.01` 作为 v7 成绩。
- [ ] 用当前 v7 build 实机验收 WinUI 的 workload 选择、Vulkan-only 能力限制、启动/结束与 History/Charts 的 `workloadVersion` 分组；目前只有 WinUI 编译通过，不能写成 GUI 运行验收通过。
- [ ] 读取/记录船、沉球和越沿粒子的精确 GPU 状态轨迹，并用可靠新 capture 验收船运动、1G 下落/入水减速、局部 crown/whitewater、池壁/PVC 和室外环境；代码路径与自动 smoke 不能代替数值/视觉验收。
- [ ] 当前固定 dt/每个渲染帧推进固定 substeps 会把 GPU 帧率反馈进 5 秒场景状态。必须冻结跨 GPU 的时间推进/补步/丢步策略与第 5 秒 trajectory contract，否则同一 workload 在快慢 GPU 上可能捕获不同模拟时刻。
- [ ] 审计 Vulkan timestamp 起止点，确保 Compute/Render/Total 只覆盖合同规定的 P2G/G2P、surface clear/splat/blur/resolve 和 raymarch 边界，不漏计新增 pass，也不把 present/CPU 等非 GPU 工作混进分项。
- [ ] 补齐初始化失败、surface/swapchain 异常、device-lost 和提前退出路径的 surface atomic buffer、R32F image、descriptor、pipeline/layout 等资源释放；正常退出不泄漏不能替代异常路径审计。
- [ ] DX12、DX11、OpenGL 与 Metal 尚未实现 cinematic liquid v2。必须在 Vulkan scene/pass/quality/trajectory contract 冻结后逐后端移植并实机验证；当前不得显示为 supported，也不得静默 fallback。

## 5. GUI 与文档同步状态

### Windows WinUI 3

- 下拉框、参数映射、score parser、History label/filter 现已识别 9 个 id。
- 主项为 `Particle — Memory Throughput` 和 `GPU Burn — Plasma Bloom (15s Burst)`；`gpu_stress` 归入 Advanced，旧 `stress`、`render3d` 明确标 Legacy，volumetric 标 Experimental，fluid 标 Vulkan-only Developer Preview 并阻止非法后端。
- Custom 与 Quick 的非 headless 运行现在默认追加 `--capture 5`；默认 duration 仍为 15 秒。Full analysis / Flights / Particle 预设也继续抓帧。
- installed/staged 布局优先使用同目录 CLI/资产；若不存在外部 CLI，静态 engine 以 GUI module 目录运行，不再强制切到 repo CWD。
- Full Analysis 优先调用随包 `tools/RenderDoc/renderdoccmd.exe`；报告输出改到用户数据目录。若缺 Python/冻结 report worker 或任一后处理命令失败，GUI 现在显示“benchmark 已完成、报告不可用/失败”，不再误报图表与报告已成功生成。
- Charts 页从真实用户结果路径读取并写入 `%LOCALAPPDATA%/GpuComputeBenchmark/reports/images`；图表脚本与 GUI 现已包含 `gpu_burn`/`gpu_stress`，只比较同 workloadVersion，并按设备×API 诚实分组。
- Release x64 已用最终 v2 `gpu_engine.lib` 重编并成功（0 error，2 个既有 duplicate WinAppSDK warning）。
- 当前 physical-scene v7 的 WinUI **Release x64 build 已通过**（0 error，仅有既有 MSB8027/C4996/LNK4042 类 warning；最新增量构建 2 个，源文件重编时曾为 4 个），但尚未实际完成 workload 选择、Vulkan-only 限制、完整 run 和 History/Charts `cinematic_liquid_v2_physical_scene_v7` / 历史版本分组验收；不得把“可编译”或自动停止 smoke 写成“GUI 已同步并运行通过”。

### macOS / HarmonyOS

- macOS SwiftUI 只列 5 个 workload（`macos-gui/GPUBenchmark/Views/RunView.swift:40`），并大量使用固定 frame-mode，而不是默认 15 秒 time-mode。
- HarmonyOS 仍是原始 Vulkan 粒子 demo，没有 workload suite、15 秒结果模型或抓帧编排。

### 已知文档漂移

| 文件 | 主要漂移 |
|---|---|
| `README.md` | 本轮已同步 9 workload、Particle 带宽定位、GPU Burn Plasma Bloom 与 staging 入口 |
| `docs/cli-reference.md` | workload/GUI 表仍停在 5 项；未记录 full-all capture 丢失 |
| `docs/roadmap.md` | 历史内容仍有漂移；结果真实默认路径现已改为平台用户数据目录 |
| `docs/benchmark-workload-suite.md` | “canonical enum” 仍只有最初 4 项 |
| `docs/TODO.md` | 多项报告/RenderDoc 数据任务与仓库现有大报告、抓帧 JSON 状态不一致 |
| `docs/TODO-volumetric-fluid.md` | 最接近 HEAD，但遗漏 binding 2/3、静默非 Vulkan fallback 和 fragment barrier 问题 |

## 6. 目标 A 的推荐实现路线

### A0 — 先恢复可信度

- [ ] 增加共享 `TestDescriptor/WorkloadMetadata`：稳定 id、分类、状态、支持后端、参数 schema、score 单位、headless/capture 能力、默认 `duration=15`、`captureAt=5`。
- [x] Windows GUI 已把 fluid 标成 developer-only；非 Vulkan fluid 与 Metal GPU Burn/GraphicsBurn 明确 error，不再 fallback。
- [ ] 修复 fluid binding、真实 ping-pong、同步和 per-frame descriptor；加 validation-layer 单帧 smoke test。
- [x] run-all capture、capture 文件名 workload 与 Windows OpenGL 重复/错标已修；低强度 9 项矩阵通过。
- [ ] 捕获帧排除评分、result schema/version/config/captureCount 已做；app build hash 与统一 quality metadata 仍待补。
- [x] CLI 比较表按 workloadVersion/scoreUnit/precision 分组，优先 stableScore/score，禁止不同轴按 FPS 混排；图表也只取同版本。

### A1 — GUI 信息架构

- [ ] 主界面只展示 `Particle (Original)`、`GPU Burn`、`Cinematic Fluid`。
- [ ] `Other / Advanced / Legacy` 展示 NBody、SynthPeak、Legacy Stress v1、Legacy 3D、Volumetric 和 Developer Fluid。
- [ ] Flights、particle count、host memory、headless 是运行变体/诊断参数，不再伪装成 workload。
- [ ] CLI、WinUI、macOS、图表和报告从同一个 registry 生成显示与参数。

### A2 — GPU Burn (15s Burst)

- [ ] `CoreBurn`：寄存器/共享内存驻留的 FP32 FMA 热循环，少量全局访存，checksum 防止优化并做确定性错误检测。
- [x] `VisualBurn v1 / Plasma Bloom`：独立跨 API fragment shader、2 次 opaque draw、固定 step raymarch、无孔实心晶核、warmup 自动标定、版本化 `Gpix-step/s`；RTX 5090 稳态 NVML 99%。
- [x] `GraphicsBurn v1`：原 `gpu_stress_v1` 保留为 Advanced component；4 次 opaque overdraw、FP32/SFU/INT、版本化 `Gpix-iter/s`。
- [ ] `MixedBurn`：同帧 compute + graphics，覆盖 core、SFU、texture/fill/ROP；按设备自动校准到短而可取消的 dispatch/draw，单块工作不要逼近 Windows TDR。
- [ ] 三个分项分别报告，不宣传某一个 shader 对所有 GPU 都是“绝对最热”。

### A3 — Cinematic Liquid

- [x] v1 不再修补旧 2D dye：建立独立 3D MLS-MPM 路径，固定 seed/质量、粒子/网格 P2G-G2P、球体碰撞、密度体积重建与自由表面 raymarch。
- [x] v1 固定 181,216 粒子、96x56x64 网格、10 substeps、160 ray steps；score 为 `MParticle-step/s`，RenderDoc 抓帧开销排除计分；GUI/历史/图表识别独立 id。
- [ ] v2 综合场景：PBR mesh、阴影、环境光、后处理和可重复镜头；保持 v1 workloadVersion/score contract 不变，另开版本。
- [ ] 建立跨后端 scene/pass registry；当前 v1 明确 Vulkan-only，不能静默 fallback 或跨 API 混分。

## 7. 目标 B 的当前阻塞与发布路线

### 7.1 已完成的第一阶段与剩余阻塞

本轮已建立并实际执行 Windows x64 GitHub Release 候选链：CMake
`install()`/CPack、固定 `vcpkg.json` baseline、CMakePresets、staging verifier、
WinUI self-contained payload、MSVC runtime、GLFW、全部预编译 shader、GLAD
2.0.8 in-tree、完整官方 RenderDoc 1.45 portable、Inno Setup 6.7.3、逐文件
SHA-256 与 ZIP 解包复核。目标机运行核心 benchmark/GUI/抓帧不需要 VS、vcpkg、
Vulkan SDK、shader compiler、单独的 Windows App SDK 或预装 RenderDoc。

PathService 已把 results/captures/reports/logs 改到
`%LOCALAPPDATA%/GpuComputeBenchmark`（可用 `GPU_BENCH_DATA_DIR` 覆盖），并一次性
迁移旧相对 `results/results.json`。GUI/CLI 与 RenderDoc 能从 staged 布局运行。

已生成可供换机验收的 ZIP 与 Setup，但仍不能宣称“已经公开发布/完全验收”，原因是：

- CLI/GUI 已把 `vulkan-1.dll` 改为 delay-import，probe 与显式 Vulkan backend 创建前都有 loader guard；本机构建和 PE 审计通过。仍须在真正没有 `vulkan-1.dll` 的干净机确认 DX11/DX12/WARP 启动。
- 报告链仍只有 `.py` 源码，没有冻结的 `report_worker.exe`；核心 benchmark/GUI 不需要 Python，但自动报告仍需要开发环境。打包规则虽预留 `tools/report_worker`，GUI 尚未实现调用该 worker 的 argv 协议。
- 仓库没有经用户确认的根项目分发 LICENSE；已有 `THIRD_PARTY_NOTICES.md` 与 GLAD/GLFW/RenderDoc 许可证不能替代项目自身许可证，因此 public MSI/WiX、签名与发布被刻意阻止。
- 当前 Setup、GUI 与 CLI 均未做 Authenticode 签名；Release asset manifest 明确记录 `NotSigned`，公开下载会有 SmartScreen 风险。
- 尚未在真正干净 Windows VM 上验证 GUI 启动、无 RenderDoc/VC Redist/SDK 环境下的运行与升级/卸载。
- GUI 当前只有 x64；ARM64 不能据此宣称可发布。

### 7.2 推荐发布架构（Windows 第一版）

1. **路径产品化（第一阶段已做）**：统一 PathService；可写数据放 `%LOCALAPPDATA%/GpuComputeBenchmark/{results,captures,reports,logs}`；staged GUI/CLI 不依赖 repo CWD。
2. **进程隔离**：GUI 是唯一 orchestrator，每个 API×GPU 启动一个 worker；device loss、driver crash 和 RenderDoc 注入不拖垮 GUI。
3. **锁定构建（部分已做）**：已有 `vcpkg.json` baseline、CMakePresets、strict shader asset gate 与 in-tree GLAD；NuGet lock、完整 shader manifest/CI 仍待补。
4. **预编译资产（已做）**：发布 SPIR-V、HLSL/GLSL 与 DX12 FP16 DXIL；用户机器不装 SDK/compiler。
5. **报告 worker**：把 Python 报告链冻结为随包的 `report_worker`（嵌入 Python或独立 onedir），主程序只用绝对路径/argv 调用，不查 PATH、不拼 shell。
6. **安装器（已实际生成）**：`installer/GpuComputeBenchmark.iss` 使用 stable AppId、x64/按用户安装、升级/卸载和可选完整 RenderDoc；`scripts/build-windows-github-release.ps1` 一次产出 ZIP、Setup、`SHA256SUMS.txt` 与 `release-assets.json`。当前 v0.1.0 Setup 为 91,887,918 bytes、SHA-256 `f301426776b8ad2bd816a3f6de55463fd4b2f6be0ca3c7f691bfd8108aca0436`，尚未签名。
7. **RenderDoc 可选组件（构建机 staged 已验证）**：随包放完整官方 1.45 bundle 到 `{app}/tools/RenderDoc`，archive SHA-256 `bd665c348a8245d10a1f513e35b83603edc1a78006277583d09ec0769286eea4`；in-app API 在第 5 秒包住一帧，Vulkan 使用进程级 implicit-layer path。仍需 clean-machine 抓帧与 credits/签名审计。
8. **Vulkan loader（代码与 PE 已收口）**：SDK 只用于构建；Windows 运行时 delay-load 并探测 loader/ICD。缺 Vulkan 时隐藏该能力；若显式请求 Vulkan则返回可读错误，DX11/DX12/WARP 仍可启动。不随软件安装 GPU 驱动或复制系统 loader。

### 7.3 Fresh-machine 验收门槛

- [ ] Windows 10/11 干净 VM：无 VS、vcpkg、Python、Vulkan SDK、RenderDoc、VC Redist；离线安装后 GUI 能启动。
- [ ] 无 Vulkan loader 时仍可跑 DX11 WARP 的 15 秒流程并保存结果。
- [ ] AMD/NVIDIA/Intel 实机：每个受支持 API/GPU 精确 15 秒，第 5 秒产生 `.rdc`，转换与报告成功。
- [ ] 安装路径、用户名含空格与中文；普通用户运行，不向 `{app}` 写结果。
- [ ] 安装/升级/卸载通过；默认保留用户结果，另提供显式“删除数据”。
- [ ] 依赖扫描无漏 DLL；发布带 hashes、SBOM、LICENSE 与 THIRD_PARTY_NOTICES。

### 7.4 DirectX 10 时代 GPU / GT 120

- **决定：不实现 DX9 后端。** GT 120 已能由 D3D11 runtime 创建 FL10_0 设备；DX9 没有 compute/UAV，无法忠实承载 Stream/N-body/SynthPeak，且 RenderDoc 不支持 D3D9。正确路线是 DX11 downlevel，而不是把同一个测试改成含义不同的 pixel-shader 仿真。
- `ProbeGpus()` 现在实际创建探测设备，记录 DX11 Feature Level 与 DirectCompute；不再把每个 DXGI adapter 硬编码为 DX11 supported。WinUI 也读取 `dx11Compute`，Full Analysis 会跳过不支持计算的 DX11 组合，Custom Run 会给出明确错误。
- DX11 backend 在 FL10 使用 `cs_4_0/vs_4_0/ps_4_0`，FL11 使用 SM5；对 FL10 查询 `D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS::ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x`。fragment-only workload 完全跳过 compute shader、UAV 与粒子 buffer。
- 生产 shader 的 16 个 SM4 entry/profile 已由 Windows SDK FXC 全部编译通过；N-body `SV_GroupIndex` 已修正，FP64 在 SM4 明确拒绝，DX11 超过 65,535 dispatch groups 会拒绝，SM4 N-body 安全上限暂定 4,096 以避免 TDR。
- **尚未在 GT 120 实卡运行。** 正式支持结论必须等 Windows 10 1809+ / NVIDIA 342.01 上完成 Stream Light/Medium、fragment workload、4,096-body N-body、15 秒、第 5 秒 RenderDoc、timestamp 与 TDR 验收。若用户的卡在 Windows 7，当前 WinUI/安装器不支持该 OS，需另建 legacy CLI package。

## 8. 开源与官方参考（只借架构/算法，逐项审许可证）

- [gpu-burn](https://github.com/wilicc/gpu-burn)（BSD-2-Clause）：高算术强度 + 重复结果校验；可借 CoreBurn/checksum 思路，不复制 CUDA 路径。
- [memtest_vulkan](https://github.com/GpuZelenograd/memtest_vulkan)（zlib）：Vulkan 显存带宽/错误检测；数分钟才适合稳定性判断。
- [vkmark](https://github.com/vkmark/vkmark)（LGPL-2.1）：scene registry、可配置 option、有序 suite 与逐场景 duration；优先独立实现结构，避免无意引入 LGPL 代码义务。
- [Sascha Willems Vulkan](https://github.com/SaschaWillems/Vulkan)（MIT）与 [Khronos Vulkan Samples](https://github.com/KhronosGroup/Vulkan-Samples)（Apache-2.0）：PBR、shadow、deferred、N-body、timestamp 等技术积木；素材许可证需另审。
- [GPU Gems 2D Fluid](https://developer.nvidia.com/gpugems/gpugems/part-vi-beyond-triangles/chapter-38-fast-fluid-dynamics-simulation-gpu) / [3D Fluid](https://developer.nvidia.com/gpugems/gpugems3/part-v-physics-simulation/chapter-30-real-time-simulation-and-rendering-3d-fluids)：算法参考，不在许可不清时复制示例代码。
- [jeantimex/fluid](https://github.com/jeantimex/fluid)（MIT）：用户截图对应项目；其路线是 3D SPH/PIC-FLIP、粒子空间网格、独立密度体积 splat 与 free-surface raymarch，而不是 2D dye。Cinematic Liquid v2 只参考该架构和参数比例，shader 与宿主实现均为本项目独立代码，未 vendor 上游代码/资产。
- [matsuoka-601/Splash](https://github.com/matsuoka-601/Splash)（MIT）：WebGPU MLS-MPM + screen-space fluid；参考其粒子/网格管线和密度体积阴影思路。
- [luihabl/VkFluidSim](https://github.com/luihabl/VkFluidSim)（CC0）与 [Wumpf/blub](https://github.com/Wumpf/blub)（MIT）：原生 Vulkan SPH 与 APIC/MLS-MPM 结构参考。核心算法依据 [MLS-MPM/APIC paper](https://yuanming.taichi.graphics/publication/2018-mlsmpm/) 独立实现。
- [LegitEngine](https://github.com/Raikiri/LegitEngine)（代码 MIT）：Vulkan rendergraph、3D FFT/流体与 point-sprite 可视化参考；仓库素材另有权利人。
- [RenderDoc](https://github.com/baldurk/renderdoc)（MIT）：可再分发，但必须保留 [LICENSE](https://github.com/baldurk/renderdoc/blob/v1.x/LICENSE.md) 与 [第三方声明](https://github.com/baldurk/renderdoc/blob/v1.x/docs/credits_acknowledgements.rst)。程序化抓帧见 [in-application API](https://github.com/baldurk/renderdoc/blob/v1.x/docs/in_application_api.rst)。
- [Windows App SDK self-contained deployment](https://learn.microsoft.com/windows/apps/package-and-deploy/self-contained-deploy/deploy-self-contained-apps)、[VC Runtime redistribution](https://learn.microsoft.com/cpp/windows/redistributing-visual-cpp-files)、[vcpkg manifest mode](https://learn.microsoft.com/vcpkg/concepts/manifest-mode)。

## 9. 正在进行 / 下一步

### 当前正在进行

- [x] 2026-07-15：完成独立 `gpu_burn_v1` vertical slice。用户否定甜甜圈后改为原创 **Plasma Bloom / 等离子晶核**：实心七瓣晶体、无环形孔洞、非粒子；保留 `gpu_stress_v1` 成绩契约，默认 15 秒 / 第 5 秒抓帧，并同步 Vulkan/DX12/DX11/OpenGL、WARP、GUI、结果/图表与打包资产。
- [x] 2026-07-15：RTX 5090 Vulkan 自动标定 16 → 1604 steps/draw；正式 render 14.899 ms、`198.44 Gpix-step/s`，稳定段连续 8 次 NVML utilization 均为 99%，功耗约 599–600W（600W limit）。应用内 93% 是 GPU timestamp / 总帧墙钟比，包含 CPU/present 间隙，不等同于 NVML busy。
- [x] 2026-07-15：完成 README/TODO/roadmap/真实源码/结果文件/GUI/构建链审计。
- [x] 2026-07-15：建立本 handoff，并把其设为 AI 的优先交接入口。
- [x] 2026-07-15：交付 `gpu_stress_v1` GraphicsBurn vertical slice，旧 `stream`/`stress` 不删除、不改旧 shader；Vulkan/DX12/DX11/OpenGL 与 WARP 可运行，Metal 显式 unsupported。
- [x] 2026-07-15：WinUI 最终同步 10 项分类、Plasma Bloom 与 Cinematic Liquid 主测试标签、默认 15 秒与非 headless 第 5 秒抓帧；Full Analysis 会传递所选 workload，并拒绝 Vulkan-only liquid 的跨 API 伪运行。
- [x] 2026-07-15：PathService、结果 schema v2、抓帧排除计分、run-all capture 传递与 bundled RenderDoc Vulkan layer 修复。
- [x] 2026-07-15：生成并验证 Windows x64 GUI-first engineering ZIP；本机 staged 四 API 均实际产生 `.rdc`。
- [x] 2026-07-15：最终可靠性收口：抓帧后固定排空 16 个 query samples；GUI 后处理改为随包 RenderDoc 优先、缺报告运行时时诚实降级；Charts 接入 GPU Burn/GraphicsBurn。
- [x] 2026-07-15：GPU Burn 未探测的固定步数收紧为 16–32；默认自动标定仍可在 16-step 安全探针后升至 2048。Windows 直接/GUI Custom OpenGL 若请求的 GPU 与真实 GL_RENDERER 不符会明确失败，不能错标成绩。
- [x] 2026-07-15：完成独立 `cinematic_liquid_v1` vertical slice：181,216 粒子 3D MLS-MPM、96x56x64 定点网格、10 substeps、R32F 密度体积与 160-step 折射自由表面 raymarch；旧 `fluid` 明确留在 Other / Legacy，未删除。
- [x] 2026-07-15：Cinematic Liquid 已同步 CLI、WinUI、历史/图表和结果 schema；固定 15 秒、第 5.1 秒 RenderDoc 抓帧，首个 3.55 秒演示循环使抓帧落在明显撞击/飞溅阶段。
- [x] 2026-07-15：完成 `cinematic_liquid_v2_surface_splat_optics_v4` Vulkan 切片：320,068 粒子、128x64x96、10 substeps、四刚体/双向耦合、Spiky² surface + 3x3x3 Gaussian（mix 0.75）、352-step 两界面折射；`shaderVersion=6`。
- [x] 2026-07-15：最终低侧第 5 秒镜头和开放侧单层细池沿已进入固定场景；正式图 `rdoc_captures/cinematic-liquid-v2-5s-formal-optics-v4.png` 显示真实宏观溃坝，画面/光学显著改善，但仍未达到 jeantimex SPH 参考的自然微观细节。
- [x] 2026-07-15：RTX 5090 Vulkan 正式 15 秒 + 5.1 秒 RenderDoc 通过，结果 id `20260715-170629-492`，Compute 10.572 ms、Render 1.553 ms、Total 12.125 ms、`263.98 MParticle-step/s`、966 measured frames；capture 0.103 秒排除，1 attempt/1 saved。CLI Release 与 WinUI Release x64 build 通过。
- [x] 2026-07-15：stage verifier 已覆盖 3 个 surface SPIR-V；v1/v2 capture 命名已隔离；非 15 秒预览结果进入独立 `_preview` 组。
- [ ] `cinematic_liquid_v2` 的 WinUI 交互/run/history、船/沉球/越沿粒子精确数值与视觉轨迹、跨 GPU 时间推进合同、Vulkan timestamp 边界和异常路径资源清理仍待验收；DX12/DX11/OpenGL/Metal 仍未实现。
- [x] 2026-07-15：安装/Release 链完成并实跑：安装 Inno Setup 6.7.3，以官方 RenderDoc 1.45 固定 archive+SHA 为输入，从头构建 CLI/WinUI、511-file stage、PE delay-import 审计、CPack ZIP、ZIP 内容复核与 Inno Setup；最终 v0.1.0 ZIP/Setup、`SHA256SUMS.txt`、`release-assets.json` 已生成。
- [x] 2026-07-15：应用户要求完成 duck family v5 场景改造（经典造型大黄鸭 + 3 只小鸭、7 刚体、SDF/渲染/碰撞三处一致、bounding 剔除；`cinematic_liquid_v2_duck_family_v5`、`shaderVersion=7`、`sceneVersion=3`）。CLI Release 重建通过；验证细节见第 10 节。15 秒正式成绩尚未在新版本下重跑。
- [x] 2026-07-15：用户恢复流体工作后完成 iterative optics v6：保留用户的 duck family，surface 改为 5x5x5 binomial（mix 0.90），最多 4 界面 Fresnel/Snell + 分段 Beer–Lambert/opaque depth sorting，`extinction=(30,10,8)`、linear exposure、density 边界归零；`cinematic_liquid_v2_iterative_optics_v6`、`shaderVersion=8`、`sceneVersion=3`。CLI/WinUI Release 构建通过，只有 6 秒短预览，尚无正式 15 秒成绩。
- [x] 2026-07-15：完成 current physical-scene v7 代码切片：320,920 粒子（142x14x98 + 48x37x71）、`cinematic_liquid_v2_physical_scene_v7` / `shaderVersion=9` / `sceneVersion=4`；保留用户鸭子和 7-body ABI，船改 34 kg/软系泊/推进反冲，沉球改真实 1G/空气与浸没门控水阻，有限池壁/outer catch band 支持越沿落地，surface 自适应保留低支持喷滴，并加入真实 GPU 入水 crown/whitewater、软 PVC 薄膜、程序化草地与大气天空。视觉/物理地面统一到 `y=0`，ring separation 由 `2*tube` 推导。
- [x] 2026-07-15：v7 shader 编译、最终 6 个 SPIR-V、CLI Release、WinUI Release x64 build 通过（0 error，仅有既有 warning；最新增量构建 2 个，源文件重编时曾为 4 个）；仅做 6/8 秒自动停止 smoke，无正式 15 秒、无可靠保存的新 RenderDoc、无完整视觉验收。`241.13` transient console 值不得记录为结果；`--time 8` 窗口自动关闭不是已确认的崩溃。
- [ ] 旧 `fluid` Vulkan 正确性与统一跨后端 workload registry 仍开放，但不再阻塞独立 Cinematic Liquid v1。
- [x] 实际 Setup 与动态 Vulkan loader 已完成构建/静态审计；显式 Vulkan 缺 loader 的异常路径也已加 guard。
- [ ] 冻结 report worker、项目 LICENSE、Authenticode signing、GT120 实卡与 clean-machine 安装/升级/卸载/抓帧验收仍开放。

### 推荐下一个实现切片

用户在 2026-07-15 明确把近期顺序调整为“先完成液体 v2，再立即完成三个自由/无限模式”；这覆盖了此前“先打包”和“持续烤机永远放 TODO 最后”的旧顺序。下一刀按以下顺序执行：

1. **完成 Cinematic Liquid v2 剩余 P0，并把下一重大视觉切片转向 SPH**：当前 v7 尚无正式 Vulkan 15 秒成绩和可靠新 capture；先冻结与渲染帧率解耦的跨 GPU timestep/trajectory contract，再做 v7 正式 15 秒 + 第 5 秒抓帧、最终 WinUI 交互/run/history、Vulkan timestamp 边界、异常路径资源清理，以及船/沉球/越沿/PVC/环境的数值和视觉验收。所有 v1/v4/v5/v6/v7 结果按合同隔离。若目标仍是 jeantimex 同级自然细节，下一项重大视觉实现为独立 SPH solver vertical slice；secondary spray 若先做，也必须是真实模拟 pass。
2. **Liquid Lab / Explore**：复用 v2 资源，加入无限时间、自由视角、暂停/单步/重置和参数面板；与正式 Benchmark 硬隔离，不生成 score。
3. **GPU Burn Unlimited Soak**：持续运行主 `gpu_burn_v1` Plasma Bloom 直到用户停止，补 GUI Stop、滚动统计、遥测与安全退出；15 秒 Burst 成绩合同保持不变。
4. **VRAM Integrity Soak**：新建分块写入/读取/设备端校验与错误累计，不把 `stream` 带宽成绩冒充显存正确性测试。
5. **换机发布验收**：Vulkan delay-load、实际 ISCC、ZIP/Setup/hashes 与官方 RenderDoc 1.45 bundle 已完成；下一步冻结并接入 `report_worker`，在无 VS/vcpkg/Python/Vulkan SDK/独立 RenderDoc 安装的干净 Windows 10/11 VM 验收 GUI、DX11/DX12/WARP、结果保存、bundled RenderDoc、安装/升级/卸载和用户数据保留。GT120 另做 FL10/SM4 实卡验收；用户选择项目许可证和签名证书后才创建公开 Release。
6. 再做 WebGPU/capability registry、跨 GPU/API 正式矩阵及后期 RT/路径追踪/超分研究；不得让这些后续项阻塞前五项。

## 10. 验证记录

### 2026-07-15 审计

- `git status --short`：审计开始时 clean；分支 `main`，HEAD `3237545`。
- 审计开始时的旧 `build/Release/gpu_benchmark.exe --help`：成功，当时二进制暴露 7 个 workload；GraphicsBurn 阶段为 8 个，当前 Plasma Bloom 重建后为 9 个。
- `build/Release/gpu_benchmark.exe --list-gpus`：成功；当前机为 RTX 5090、AMD iGPU、Microsoft Basic Render Driver。审计当时曾把两张硬件 GPU 都标记 OpenGL；现已按真实 GL_RENDERER 匹配修复。
- `results/results.json`：232 条，分布见 3.2；没有 volumetric/fluid 记录。
- `rdoc_captures/*.rdc`：只有 4 个旧的 16M particle 抓帧，没有新 workload 抓帧。
- `cmake --build build --config Release`：失败。原 cache 选中 `C:/Program Files/SVP 4/mpv64/python.exe`，缺 `glad`；改用可用 Python 后继续因缺 `jinja2` 失败。该失败作为“构建不锁定/不可复现”的直接证据保留。
- 未运行任何压力/性能 workload，避免在审计过程中改写结果库或无提示压满 GPU。

### 2026-07-15 Duck family v5 验证（晚间）

- 三个改动 shader（`mls_mpm_grid_update_v2.comp`、`mls_mpm_g2p_v2.comp`、`cinematic_liquid_render_v2.frag`）先经 `glslc --target-env=vulkan1.1` 独立编译通过，再随 `cmake --build build --config Release --target gpu_benchmark` 全量重建通过。
- 预览运行 1（RTX 5090 / Vulkan / `--time 6 --capture 5`，结果组 `cinematic_liquid_v2_duck_family_v5_preview`）：结果 id `20260715-174802-286`，Compute 8.794 ms、Render 1.641 ms、Total 10.435 ms、306.73 MParticle-step/s（预览口径，不可与正式 15 秒混比）；抓帧 0.099 秒排除，1 次尝试保存 1 个。第 5 秒帧为溃坝浪峰，鸭群大多被浪体遮挡：`rdoc_captures/cinematic-liquid-v2-5s-duck-family-v5.png`。
- 预览运行 2（`--time 11 --capture 10`）：结果 id `20260715-174916-139`；第 10 秒平静帧 `rdoc_captures/cinematic-liquid-v2-10s-duck-family-v5.png` 清晰显示 smooth-min 鸭妈妈（圆头/黑眼/白高光/橙嘴/上翘尾，无旧版椭球接缝）和三只小鸭直立漂浮，形态接近用户给出的经典大黄鸭参考图。鸭子造型视觉验收以该帧为准。
- 渲染侧 Render 1.641 ms（旧四刚体基线 1.553 ms）：sphere-trace 鸭子 + 3 只新鸭的开销可接受；Compute 预览均值 8.794 ms 低于旧基线 10.572 ms，主要来自新加入的刚体 bounding-sphere 剔除，但预览与正式 15 秒统计窗口不同，正式对比须等新版本 15 秒运行。
- CLI 摘要中硬编码的 “4 coupled rigid bodies” 已改为使用 `kCinematicLiquidV2BodyCount` 并重建。
- 注意：无参数 `--workload cinematic_liquid --time 6` 在多 GPU 机器上会停在 “Enter GPU index” 交互提示；脚本化运行必须显式 `--gpu 0`。
- WinUI GUI 已用新 `gpu_engine.lib` 重编（Release x64，0 error，仅既有 C4996/LNK4042 warning）；场景介绍文案更新为“子母鸭家族”。启动 smoke 通过：6 秒后 `HasExited=False`、`Responding=True`、标题 `GPU Benchmark`。GUI 按稳定 id `cinematic_liquid` 识别 workload，History/Charts 按结果 `workloadVersion` 分组，因此 `cinematic_liquid_v2_duck_family_v5(_preview)` 自动独立成组，无需 GUI 代码适配。GUI 交互式完整 run/history 实机验收仍是原有开放 P0；`out/stage` 打包目录仍为旧 optics_v4 资产，发布层面需要重新 stage。
- 本轮**未**运行 15 秒正式流程；`cinematic_liquid_v2_duck_family_v5` 只有上述历史预览。随后用户已恢复流体工作并依次升级到 v6/v7，因此 v5 不再是当前工作树合同，但其预览记录永久保留、不得与后续版本混分。

### 2026-07-15 Iterative optics v6 验证（晚间）

- 在用户调整后的 duck family v5 基础上完成 `cinematic_liquid_v2_iterative_optics_v6`（`shaderVersion=8`、`sceneVersion=3`）；鸭子 SDF、七刚体初始化、碰撞与渲染分支均保留。
- 表面 resolve 当前为 5x5x5 binomial filter、`mix=0.90`；fragment shader 最多处理 4 次介质界面，逐段执行 Fresnel/Snell、Beer–Lambert 和 opaque scene depth sorting。当前 metadata 为 `extinction=(30,10,8)`、`toneMap=linear_exposure`、`densityBoundary=zero_clamped`。
- 最终必要短预览：RTX 5090 / Vulkan / `--time 6 --capture 5`，结果 id `20260715-221447-024`，Compute 9.450 ms、Render 3.003 ms、Total 12.454 ms、`257.01 MParticle-step/s`、284 measured frames；capture 0.117 秒排除，1 attempt/1 saved。核验图：`rdoc_captures/cinematic-liquid-v2-5s-iterative-optics-v6-final-preview.png`。
- 这是 `_preview` 结果，**没有运行 v6 的正式 15 秒流程**，不可与 optics_v4 的正式 `263.98 MParticle-step/s` 比分。水体厚区、薄片与边界观感较旧版改善，但仍未达到 jeantimex SPH 的自然动态。
- CLI Release 与 WinUI Release x64 重建通过；WinUI 仍只有 build 结论，实际 workload 选择、完整 run 与 History/Charts 分组待验收。

### 2026-07-15 Physical scene v7 验证（晚间）

- 当前合同为 `cinematic_liquid_v2_physical_scene_v7`、`shaderVersion=9`、`sceneVersion=4`；128x64x96、10 substeps、320,920 粒子（142x14x98 base + 48x37x71 dam）。用户鸭子与七刚体 index ABI 均保留，尤其 boat=2、sink sphere=3、ducklings=4-6。
- shader 编译通过，最终 6 个 SPIR-V 均通过 Vulkan 1.1 校验；CLI Release 构建通过。WinUI Release x64 build 也通过：0 error，仅有既有 warning（最新增量构建 2 个 duplicate WinAppSDK warning，源文件重编时曾包含 C4996、合计 4 个）。最终物理/渲染静态复核均未发现必须修复项；这仍不替代实机视觉验收。
- 只运行了若干 `--time 6` / `--time 8` 自动停止 smoke，没有正式 15 秒流程。没有可靠保存的新 RenderDoc/核验帧，也没有完成船、沉球、水花、越沿、PVC、草地/天空的完整视觉验收；目录中若存在 v7 临时 PNG，也不得在缺少可靠 capture 记录时当作验收证据。
- 一次控制台曾显示 transient `241.13`，但它不是正式 15 秒值，也没有作为可靠持久结果保存，禁止写入排行榜或与 optics_v4/v6 比较。
- 用户反馈窗口约 2-3 秒后自行关闭；本轮使用的自动 smoke 命令包含 `--time 8`，因此自动退出是脚本生命周期，不是目前确认的 crash。正式 Benchmark 应按 15 秒流程验收；未来 Liquid Lab/Explore 才应无限运行至用户主动 Stop。
- 物理/场景代码事实：船为 34 kg 有限质量 + 软系泊，推进反冲可驱动/摇摆；沉球 1.06 水密度比、4.28 秒释放、-9.81 重力、0.015 空气阻尼，水阻按 displaced mass/浸没率门控；有限池壁 inset 0.22 + 有限 outer catch band；自适应 surface 保留低支持喷滴，entry crown/whitewater 来自 GPU body/local fluid，但无 secondary-spray 粒子；软 PVC 薄膜 IOR 1.50/Fresnel/弱吸收/wrinkle；无限程序化草地和无 cubemap 的大气天空/云。以上均是实现事实，不等于轨迹或最终画质已验收。
- render push 的 `pool.z` 已改为真实 ground/pool-floor `y=0`，PVC bottom、liner、草地与物理粒子底面统一；ring separation 由 `2*tube` 推导。该对齐通过源码/构建验证，仍需可靠新 capture 做最终视觉验收。

### 2026-07-16 SPH vertical slice（`cinematic_liquid_sph_slice_v1`，实验性）

按既定判断（"不再盲调 MLS-MPM，建立独立 SPH 版本"）完成了第一个可运行的 SPH 纵切片。**这是实验性 preview 合同，未做正式 15 秒验收，不得与任何 MLS-MPM 版本混分。**

实现事实：
- **求解器**：忠实移植 MIT [jeantimex/fluid](https://github.com/jeantimex/fluid) `src/sph/3d/common` 的 Lague 式双密度 SPH——Spiky²/Spiky³ 双密度 + 对称压力/近压（EOS `P=k(ρ-ρ0)`）、Poly6 XSPH 粘性、预测位置（固定 1/120 前瞻）、Unity block-hash(50) 空间哈希 + 计数排序（三级扫描 + atomic scatter）。参数原封照搬：h=0.2、targetDensity=630、pressureMultiplier=288、nearPressure=2.16、viscosity=0.01、collisionDamping=0.95、g=-10，全部在参考单位制（~24x12x18 域）内运行。
- **关键教训（已修复的爆炸）**：参考的 `frameTime=min(dt*timeScale, 1/maxTimestepFPS)` 在 60fps 下把 timeScale=2 钳掉，**每 substep 实际 dt=1/120**；首次用 1/60 直接压力爆炸（全域水雾，见 `rdoc_captures/cinematic-liquid-sph-5s-first.png` 留档）。现固定 dtSim=1/120、2 substeps/帧（固定每帧推进，不随墙钟）。
- **世界映射**：长度 ×(5.12/24=0.21333)，时间 ×τ=sqrt(0.21333*10/9.81)≈0.4663，使 sim 重力 -10 精确落在世界 -9.81，流体与刚体自由落体一致。积分 pass 把世界坐标/速度写回共享 80B 粒子 buffer——surface splat、raymarch、相机、场景、成绩框架**全部原样复用 v7**。
- **刚体耦合**：per-particle SDF 推离 + 速度碰撞 + displaced-volume 浮力（镜像 grid_update 的动量交换），fixed-point 冲量进现有 `bodyImpulses`，rigid integrate pass 原样消费（含 linear.w 浸没门控）。第 10 秒实图验证：鸭妈妈+三小鸭+彩球+船漂浮端正，沉球半沉。
- **播种**：sim 单位确定性晶格 175x8x124 底床 + 30x46x96 左坝柱 = **306,080 粒子**（密度 600/unit³，同参考 spawnDensity），全部落在内嵌池壁内；4 秒重置与 v2 相同（含 SPH 位置/速度 buffer 的 seed 恢复）。
- **内嵌有限池壁（用户指正后补上，随后按用户要求加宽外圈）**：首版直接用整域 AABB 当边界，静水铺得比可见泳池宽；现 SPH integrate 实现内嵌圆角矩形有限池壁（壁顶 sim 4.017 = 世界 0.737，越顶水花自由落草地，外圈域 AABB 仅作 catch band）。2026-07-16 用户要求加宽外圈草地带：**渲染圈/PVC 膜/物理墙 inset 统一从 0.22 → 0.45**（frag `poolDistances` 与 `poolMembraneDimensions` 的系数 2.59→5.29·tube；MPM 路径 `pc.pool[1]`、SPH 墙常量同步）。**注意：这同时改变了 v7 MPM 场景的池几何，v7 此前的 smoke 观感基线随之漂移，属用户批准的场景变更。** 调试过程记录：(a) 坝柱高于壁顶太多导致整体涌浪漫顶、catch band 积成"护城河"→ 柱降至 30 层；(b) splat 核+5x5x5 重建使水面比粒子外扩 ~0.1m，物理墙需再内收 0.10m（half sim 9.328/6.328）才能让重建水面贴圈。最终播种 148x9x98 + 30x30x96 = **216,936 粒子**。最终验证图 `rdoc_captures/cinematic-liquid-sph-10s-wideband-v4.png`（宽外圈草地干净、水贴圈、玩具姿态正常）；中间态 `...-v2/-v3.png` 留档对照。刚体无内墙约束（沿用 v7 语义），个别玩具可被浪冲出池沿属真实行为。
- **深池/浅蓝水/沉底可见（2026-07-16 用户四项调整）**：(a) 草地吸收倒计时上限加长（0.25~0.65 → 0.50~1.80 sim 秒，水洼多留一会儿）；(b) 池壁加高：壁顶系数 0.34 → 0.42（世界 0.737 → 0.938），渲染圈/PVC 膜/g2p/grid_update/SPH 五处同步；(c) 水量 216,936 → **318,464 粒子**（床 148x16x98，静水深 ~0.49 m），0.40 m 沉球完全没顶；(d) `liquidTransmittance` 消光 (30,10,8) → (12,3.6,2.5)，保持红先吸收的水色光谱但整体透亮为浅蓝，水下沉球轮廓可读。**(b)(d) 同样作用于 v7 MPM 路径（共享 shader），v7 观感基线再次漂移，属用户批准的场景变更。**验证图 `rdoc_captures/cinematic-liquid-sph-10s-deep.png`（更高池壁、侧壁浅蓝、沉球在水下可见）。
- **草地吸收 / 空气墙消除（2026-07-16，用户指出加宽外圈的根本动机是大量溅水时能看到外框空气墙）**：SPH integrate 增加逃逸水回收机制——(a) 域外框 x/z 面从反弹改为"粘滞空气"（只清外向法向速度，不再出现半空撞隐形墙的反弹）；(b) 落在外圈草地的水按每粒子错开的倒计时（0.25~0.65 sim 秒，存于 `position.w`，1.0=自由态）短暂积水后"渗入草地"；(c) 压在域边缘的水快速消退（≤0.10s）；(d) 渗完的粒子确定性回收到池内水线以下（hash 定位、零速度），粒子总数不变、成绩合同不受影响。验证：`rdoc_captures/cinematic-liquid-sph-5s-absorb.png`（溃坝晃荡期池外水舌边缘为自然圆弧、无垂直水墙）、`...sph-10s-absorb.png`（10 秒草地完全干净、无积水残留）。metadata 补 `grassAbsorb=catch_band_soak_recycle`。
- **文件**：9 个新 shader `shaders/cinematic_liquid_sph_*.comp`（external/hash_count/scan_block/scan_add/scatter/density/pressure/viscosity/integrate，共享 128B push ABI + 单一 14-binding set）；`src/vulkan_backend.{h,cpp}` 新增 `CreateCinematicLiquidSphResources`/`RecordCinematicLiquidSphFrame`；CLI `--liquid-solver <mpm|sph>`（默认 mpm，v7 路径零改动）；score 公式已按 SPH 的 2 substeps 计（三处修正，虚高 5 倍的首条结果已删除）。
- **验证（RTX 5090 / Vulkan / preview）**：`--time 6 --capture 5` 结果 `20260716-025211-784`，Avg GPU 7.775 ms；`--time 11 --capture 10` 结果 `20260716-025310-587`。第 5 秒帧 `rdoc_captures/cinematic-liquid-sph-5s-dt120.png`：溃坝浪翻卷 + 薄片 + 碎裂水花（明显优于 MLS-MPM 的黏稠感）；第 10 秒帧 `rdoc_captures/cinematic-liquid-sph-10s-dt120.png`：连续平滑水面 + 全部刚体姿态正确。
- **2026-07-16 实机视觉收口**：RTX 5090 / Vulkan / **318,464 粒子**实际启动并完整运行 15 秒；水池、水面、用户鸭子家族、彩球、船、草地/天空稳定显示，进程按 15 秒生命周期正常自动结束。用户接受当前视觉并要求停止外观迭代。此结论只关闭“当前看起来是否可接受”，**不等于**正式 15 秒 + 第 5 秒 RenderDoc 成绩合同通过。

已知边界/开放项（诚实清单）：
- 计数排序 scatter 的 cell 内顺序是原子竞争决定的 → 浮点求和 ULP 级非确定；正式合同前需换确定性排序（metadata 已标 `determinism=cell_order_race_ulp_open`）。
- SPH 目前固定每个**渲染帧**推进 2 个 1/120 子步，不同 GPU/帧率在墙钟第 5 秒会处于不同物理时刻；正式成绩前必须改成与渲染帧率解耦的轨迹合同。
- 两个 SPH 子步之间没有重新清零 `bodyImpulses`，第一子步刚体冲量可能在第二子步重复消费；必须按 substep 清零后重新验证船、鸭子与沉球。
- viscosity pass 原位读取邻居并写同一 velocity SSBO，存在跨 invocation 未同步读写竞争；正式成绩前须改为 ping-pong 或 delta/apply 两阶段。
- 无 secondary spray/foam 粒子（参考的 foam_spawn/foam_update 是下一切片）；whitewater 体积在 SPH 路径恒为零。
- 船的螺旋桨尾流不作用于 SPH 水体（v7 尾流实现在 grid_update，SPH 不跑该 pass）。
- 当前外观已获用户接受，不再优先调水色；secondary spray/foam 与螺旋桨尾流是未来增强，不阻塞本轮视觉收口。
- 正式 15 秒 + 第 5 秒 RenderDoc 成绩、timestamp 边界、确定性/轨迹合同与跨后端一律未完成。

### 2026-07-16 GUI History 打开目录按钮

- History 页新增两个按钮：“打开成绩目录”（`PathService ResultsDirectory()` = `%LOCALAPPDATA%/GpuComputeBenchmark/results`，含 results.json）与“打开抓帧目录”（`CapturesDirectory()` = 同根下 `captures`，含 RenderDoc `.rdc`）。两者共享 PathService 数据根但是不同子目录，因此各给一个入口而不是假装同一处。实现：`gui/MainWindow.xaml`（按钮）、`MainWindow.xaml.h`（handler 声明）、`MainWindow.xaml.cpp`（`ShellExecuteW` 打开 Explorer；PathService 目录创建失败会抛异常，handler 已 try/catch 防止 Click 崩掉整个 GUI；中英文案已本地化）。
- GUI Release x64 重编通过；启动 smoke：6 秒后 `Responding=True`、标题 `GPU Benchmark`。按钮点击的实机交互验收与其余 GUI 验收项一起归入既有 P0，尚未逐一点击验证。

### 2026-07-15 实现与发布 smoke（本轮）

- 核心 Release 构建成功：VS 18/v145，Vulkan、DX12、DX11、OpenGL；GLAD 改为仓库内可复现生成物，不再 configure-time 下载或依赖 Python/Jinja。
- WinUI Release x64 用最新 engine 链接成功：0 error；4 个既有 WinAppSDK 重复对象/C4996 warning。
- RTX 5090 Vulkan `gpu_stress` 3 秒校准 smoke：32 → 1416 iterations/draw，正式窗口 Render 8.826 ms、GPU utilisation 93.5%、`591.40 Gpix-iter/s`。这是校准 smoke，不是正式 15 秒成绩。
- DX12 WARP 3 秒自动标定：32 → 1 iteration/draw，device utilisation 95.6%；证明软件设备路径不会沿用对 dGPU 过重的固定迭代数。
- 低迭代初始化 smoke 已通过 Vulkan、DX12、DX11、OpenGL；DX11/DX12 WARP 亦通过。Metal 未在本机验证，且 GPU Stress v1 目前明确 unsupported。
- 从最终 staged 目录分别运行 Vulkan、DX12、DX11、OpenGL 6 秒，均在约 5.1 秒产生真实 `.rdc`；capture wall time 与 async timestamp samples 从 score 排除。发现并修复过一次“API detected 但 Vulkan capture=0”的误报，根因是 portable `renderdoc.json` 未在首次 Vulkan probe 前被 Loader 发现。
- 最终审查发现 frames-in-flight 排空不足以覆盖 DX11/OpenGL query ring；已改为抓帧后跳过至少 16 个异步样本（DX11 ring=8、OpenGL ring=4，最大 flights 时取 17），核心 Release 再次构建通过；复核确认计数只在成功取得 timing sample 时递减。
- result JSON v2 与 CSV export 通过；新记录包含 `gpu_stress_v1`/`gpu_burn_v1`、最终 steps/iterations/draws/shaderVersion/autoTune、captureAtSec/captureAttempts/captureCount/captureExcluded。历史 v1 结果可读。
- 三份 Plasma Bloom shader 已分别通过 Vulkan `glslc`、OpenGL `glslangValidator` 与 HLSL FXC VS/PS 编译；16-byte ABI、固定循环和防 DCE 数据依赖一致。
- RTX 5090 Vulkan 10 秒自动标定 smoke（`out/gpu-burn-plasma-smoke`）：16 → 1604 steps/draw，Render 14.899 ms、internal utilisation 93.0%、`198.44 Gpix-step/s`、stableScore 197.79/CV 0.97%；外部 NVML 稳定段 8/8 次 99%，约 599–600W。画面核验图为 `out/gpu-burn-plasma-smoke/plasma_bloom_vulkan_verified.png`。
- `--run-all --workload gpu_burn --iter 16 --time 3` 隔离矩阵通过 9/9：RTX 5090 四 API、AMD iGPU 三 API、WARP DX12/DX11；OpenGL 只出现一次，所有 burn 记录 `particleCount=0`。CLI comparison 现按 `gpu_burn_v1 + Gpix-step/s` 分组/score 排名。
- 安全回归：WARP 输入 `--iter 2048` 被限制为 32，正式 render 228.375 ms，未产生数十秒 draw；Windows OpenGL 指定 AMD index 时以 exit 1 拒绝并报告真实 RTX GL_RENDERER，不生成错标结果。
- Cinematic Liquid 正式流程通过：RTX 5090 / Vulkan / 1280x720 / 181,216 粒子 / 96x56x64 网格 / 10 substeps，墙钟 15 秒；第 5.1 秒生成 19,474,099-byte `.rdc`，抓帧 0.080 秒和异步样本已排除计分。正式 Compute 6.117 ms、Render 0.159 ms、`288.74 MParticle-step/s`、GPU timestamp utilisation 90.5%；结果 id `20260715-042720-440`，`workloadVersion=cinematic_liquid_v1`，配置记录 `captureAttempts=1;captureExcluded=true;captureCount=1`。抓帧缩略图为 `out/cinematic-liquid-v1/capture-5s.png`。
- Cinematic Liquid 新增的 8 份 SPIR-V 均从最新 staged 目录通过 Vulkan 1.1 `spirv-val`；静态复核确认 80B particle、16B grid cell、96B compute/render push constant ABI、P2G/G2P barriers 和 density compute→fragment barrier 一致。CLI 强制 v1 使用真实 device-local storage，GUI 会禁用不适用的 Host memory，避免结果误标。
- v2 早期 RenderDoc 缩略图已检查：`rdoc_captures/cinematic-liquid-v2-5s-particle-splat-128.png` 与 `rdoc_captures/cinematic-liquid-v2-10s-particle-splat.png` 只属于 surface-splat 视觉 smoke，不再代表最终 optics v4 合同。
- 最终 `cinematic_liquid_v2_surface_splat_optics_v4`（`shaderVersion=6`）正式流程通过：RTX 5090 / Vulkan / 1280x720 / 320,068 粒子 / 128x64x96 / 墙钟 15 秒；5.1 秒 RenderDoc 抓帧 0.103 秒已排除计分，1 次尝试保存 1 个 capture。正式 Compute 10.572 ms、Render 1.553 ms、Total 12.125 ms、`263.98 MParticle-step/s`、966 measured frames，结果 id `20260715-170629-492`。最终图为 `rdoc_captures/cinematic-liquid-v2-5s-formal-optics-v4.png`。
- 最终核心 CLI Release 与 WinUI Release x64 build 均成功；WinUI 为 0 error、2 个既有 duplicate WinAppSDK warning，但未做最终交互选择/run/history 验收。`verify-windows-stage` 已包含 3 个 surface SPIR-V；capture 名称区分 v1/v2；非 15 秒运行进入独立 `_preview` 结果组。
- Vulkan 换机兼容收口：`VK_EXT_debug_utils` 改为枚举后可选启用；设备选择要求同一 queue family 同时支持 graphics+compute，避免在分离队列设备上提交无效 mixed command buffer；swapchain out-of-date 现在明确中止而不再空转并污染计分。完整 resize/recreate 体验仍可后续补。
- 上述兼容改动后又跑 RTX 5090 Vulkan 3 秒 smoke：CLI 输入 `--host-memory` 会明确警告并归一化为 Device-local；窗口、combined queue、181,216 粒子 simulation 和结果保存均通过，`279.96 MParticle-step/s`。
- 最新 GUI-first staged 输入已从头配置/编译/安装到 `out/stage/cinematic-liquid-windows-x64`：483 个文件、257,748,185 bytes，包含 5 个 MLS-MPM compute SPIR-V、3 个 liquid resolve/render SPIR-V、WinUI self-contained payload、MSVC runtime 与完整 RenderDoc。stage verifier 与 Inno Setup `-StaticOnly` 均为 0 error（保留 5 个已知发布 warning）。
- 从该 staged 目录本身运行 6 秒验证通过：程序加载随包 `tools/RenderDoc/renderdoc.dll`，第 5.1 秒生成 19,477,795-byte `.rdc`；结果 id `20260715-044159-098`，`captureAttempts=1;captureExcluded=true;captureCount=1`。这证明最新 liquid 资产与 bundled capture 路径可用，但仍不是第二台/干净 VM 的安装验收。
- 同一 staged 目录内 WinUI 隐藏启动 5 秒后 `HasExited=false`、`Responding=true`、标题 `GPU Benchmark`；随后仅结束该测试进程。GUI Release 与 staged 启动均通过。
- 最新 liquid engineering ZIP 已生成：`out/packages/cinematic-liquid/GpuComputeBenchmark-0.1.0-windows-x64.zip`，124,447,344 bytes；SHA-256 `f35e6322b3a753cb57fbc9775b1e57a31ab5044ad3c227cf65a78b4857488ef3`，旁边有 `.sha256`。它用于第二台/VM 解压验收，不等同于正式 Setup。
- 最终 staged **完整产品流程**通过：RTX 5090 Vulkan 墙钟 15 秒，16 → 1604 steps/draw，第 5.1 秒由包内 RenderDoc 生成 184,384-byte `.rdc`；正式 Render 14.872 ms、`198.80 Gpix-step/s`、stableScore 198.64/CV 0.08%，结果记录 `captureAttempts=1;captureExcluded=true;captureCount=1`。最终 staged WinUI 启动 5 秒后仍 `Responding=true`、标题 `GPU Benchmark`。
- WinUI 最终重编通过：0 error、4 个既有 warning。Python 报告脚本 `py_compile` 通过；当前可用测试 Python 没有 matplotlib，因此未伪造图表运行成功，冻结 report worker 仍是明确发布门槛。
- 旧 engineering ZIP/RenderDoc 1.43 记录已被下面的 v0.1.0 release candidate 取代，不得再把旧 hash 当最终资产。
- 最新 staging verifier：**0 error、5 warning**；warning 为无-Vulkan-loader clean-machine 尚未测、staged GUI 尚未做 clean-machine orchestration、bundled RenderDoc 第 5 秒抓帧尚未在 clean machine 测、无 frozen report worker、无项目分发 LICENSE。`vulkan-1.dll` hard import 已修复，不再是 warning。
- 最终 packaging manifest 已复核 `vulkan/directX12/directX11/openGL=true`、GUI self-contained=true、MSVC runtime=true、RenderDoc portable=true、Vulkan delay-load=true、frozen report worker=false、project license=false。stage 为 511 files。
- 当前 GitHub Release 候选位于 `out/release/windows-x64`：ZIP `GpuComputeBenchmark-0.1.0-windows-x64.zip`，124,374,472 bytes，SHA-256 `7c5d89dab1b5a625f2ac9c7120b0098677b9cac4236a1b14ba2d798b1dcaa40e`；Setup `GpuComputeBenchmark-0.1.0-windows-x64-setup.exe`，91,887,918 bytes，SHA-256 `f301426776b8ad2bd816a3f6de55463fd4b2f6be0ca3c7f691bfd8108aca0436`，NotSigned。`sourceRevision=3237545...` 且 `sourceTreeDirty=true`，因为本轮改动尚未提交。
- 没有创建 tag/GitHub Release，也没有在第二台/干净 VM 验证；仓库无根 LICENSE，因此当前资产是**上传候选/换机验收包**，不是已公开发布版本。

## 11. WebGPU、Cinematic Liquid v2 与 TriangleBin 移植（2026-07-15）

### 11.1 当前项目的 WebGPU 路线

- [ ] **P0 capability registry**：先抽出统一 backend/workload capability registry，消除 `main.cpp` 多处 backend 工厂、固定 API 布尔字段和 WinUI `array<bool,4>`；否则第六后端会继续扩大状态漂移。
- [ ] **P1 原生 WebGPU**：当前仓库优先固定 Dawn 版本，以独立 C++20 bridge/PIMPL 接入现有 C++17 engine；实现 adapter/LUID 映射、surface/headless、device-loss、timestamp query ring、GUI/API 检测和打包 notices。没有可靠 GPU timestamp 时禁止产生正式 score。
- [ ] **P2 Stream WGSL**：先验证 1M，再根据 adapter limits 支持 4M/16M；大 dispatch 必须拆分。使用临时 `stream_webgpu_v1`，在字节模型、输出和 timestamp 与原生 API 对齐前不混分。
- [ ] **P3 GPU Burn WGSL**：保持 1280x720、两次 fullscreen draw、固定循环和 auto-tune；push constants 改 uniform ring，完成黄金图/checksum、15 秒流程与底层后端记录。使用临时 `gpu_burn_webgpu_v1`。
- [ ] **P4 Cinematic Liquid v2 跨后端**：Vulkan 与 WebGPU 共用固定 scene/pass/quality contract；WGSL 保留定点 atomic P2G，R32F 过滤必须用手写 trilinear 或明确 capability，不依赖可选 `float32-filterable` 后静默改变画质。
- [ ] **P5 浏览器 `/web`**：作为第二运行环境，共享 WGSL/manifest，但结果必须单独标记 browser、浏览器版本、adapter、底层实现（可得时）、timestamp mode/resolution。浏览器不能保证枚举当前电脑全部 GPU，也不承诺本地文件路径、WinUI 或固定第 5 秒 RenderDoc。
- [ ] 结果 schema 增加 `apiImplementation`、`underlyingBackend`、`implementationVersion`、`timingMode`、`timestampResolutionNs`；`WebGPU/Dawn/D3D12`、`WebGPU/Dawn/Vulkan`、浏览器 WebGPU 不得只凭相同 workload 名混排。

### 11.2 Cinematic Liquid v2 固定场景

- [x] **Vulkan 固定实现**：128x64x96 网格、`dx=0.04`、10 substeps、320,920 粒子（142x14x98 base + 48x37x71 dam）；stiffness 45,000、viscosity 0.035、maxSpeed 8。v1 的 181,216 粒子/96x56x64/10 substeps/160 ray steps 与历史成绩永久保留。
- [x] 用户鸭子与七刚体 index ABI 均保留：boat=2、sink=3、ducklings=4-6。船不再硬锚定，而是 34 kg 有限质量 + 软系泊，推进器反冲可驱动/摇摆；沉球是 1.06 水密度比、4.28 秒释放、-9.81 重力、0.015 空气阻尼，material 水阻按 displaced mass/浸没率门控。两者的精确状态和最终画面仍待验收。
- [x] 有限高度 pool wall 以 inset 0.22 内嵌；有限宽 outer catch band 允许越沿粒子真实落地，但不是无限流体域。`pool.z`、PVC bottom、liner、程序化草地与物理 floor 都对齐 `y=0`，ring separation 由 `2*tube` 推导。
- [x] 当前表面重建为独立 Spiky² fixed-u32 particle splat → 128x64x96 R32F volume，再做自适应 5x5x5 binomial resolve，保留低支持喷滴；renderer 延续最多 4 次界面的 Fresnel/Snell、分段 Beer–Lambert 和 opaque depth sorting。沉球 entry crown/whitewater 读取真实 GPU body/local fluid，不是 fragment 假水花；尚无 secondary-spray 粒子。
- [x] 前景透明侧壁是独立软 PVC 薄膜近似（IOR 1.50、Fresnel、弱吸收、wrinkle），不是完整 PVC 多介质 ray path；环境是无限程序化草地与大气天空/云，不使用 cubemap 资产。
- [x] 当前结果身份为 `cinematic_liquid_v2_physical_scene_v7`，`shaderVersion=9`、`sceneVersion=4`。用户调整后的 smooth-min 鸭妈妈 + 3 只小鸭及全部 7 刚体均保留。历史 v1、`cinematic_liquid_v2_surface_splat_optics_v4`/`shaderVersion=6`、`cinematic_liquid_v2_duck_family_v5`/`shaderVersion=7` 与 `cinematic_liquid_v2_iterative_optics_v6`/`shaderVersion=8` 各自独立成组，不得混分。
- [x] 历史 optics_v4 的 RTX 5090 Vulkan 正式 15 秒 + 5.1 秒 RenderDoc 已通过：`20260715-170629-492`，Compute 10.572 ms、Render 1.553 ms、Total 12.125 ms、`263.98 MParticle-step/s`、966 measured frames；capture 0.103 秒排除，1 attempt/1 saved。最终图为 `rdoc_captures/cinematic-liquid-v2-5s-formal-optics-v4.png`；该成绩不得归给当前 v7。
- [x] iterative optics v6 的历史必要短预览已通过：`20260715-221447-024`，图为 `rdoc_captures/cinematic-liquid-v2-5s-iterative-optics-v6-final-preview.png`；这是 v6 `_preview`，没有正式 15 秒成绩且不得归到 v7。
- [x] physical-scene v7 的 shader、6 SPIR-V、CLI Release 与 WinUI Release x64 build 通过；只有 6/8 秒自动停止 smoke，没有正式 15 秒、可靠新 RenderDoc 或完整视觉验收。transient `241.13` 不记录为结果；`--time 8` 自动关闭不是已确认 crash。
- [ ] **当前最高优先级验收**：固定跨 GPU timestep/trajectory contract；运行 v7 正式 15 秒 + 第 5 秒抓帧；实机验证当前 WinUI selection/run/history；审计 Vulkan timestamp pass 边界与异常路径资源清理；读取船/沉球/越沿粒子轨迹并视觉验收 PVC/环境/局部水花。通过前 v2 整体状态仍为 in progress。
- [ ] **下一重大视觉路线**：当前 v7 仍为 MLS-MPM 且无 secondary spray。若用户要求 jeantimex SPH 同级效果，下一实现应为独立 SPH solver vertical slice/版本；secondary spray 若先做也必须是真实版本化模拟 pass。
- [ ] 配置元数据继续补齐并核验 `sceneVersion/sceneHash/cameraPathVersion/poolType` 等固定合同字段；正式 Benchmark 禁止自由相机和会改变工作量的参数开关。相机或池体发布后如需修改，必须升新 workload version。
- [ ] DX12、DX11、OpenGL、Metal 尚无 v2 实现。先冻结并验收 Vulkan scene/pass/quality contract，再逐后端移植；不得静默 fallback 或显示伪支持。
- [ ] 玻璃水缸仍只属于未来不可计分的 Liquid Lab / Explore 环境预设；若以后升为正式 RT 场景，必须使用独立 workload/version。

### 11.3 TriangleBin WebGPU 外部移植判断

- 正确术语是 **TBR（Tile-Based Rendering）vs IMR（Immediate Mode Rendering）**，不是 IBR。TriangleBin 用 fragment atomic counter 与阈值显示“先执行的 fragment”空间图案；它是 shade/execution-order visualizer，不是综合跑分，也不能百分百证明物理架构。
- 上游 [Swung0x48/TriangleBin](https://github.com/Swung0x48/TriangleBin) 是 MIT，可以 fork、修改和再分发，但必须保留 `Copyright (c) 2025 Swung 0x48` 与完整 MIT 文本；未获上游认可前产品名应为 **Unofficial WebGPU port of TriangleBin**。
- WebGPU 可用 WGSL storage `atomic<u32>`/`atomicAdd` 忠实复现核心机制；但图案代表浏览器/Dawn 或 wgpu、底层 API、驱动与 GPU 的合成行为，结论必须标为 `Experimental / Observed shade order` 并记录浏览器和底层实现。
- **推荐另建正式 fork/独立仓库 `TriangleBin-WebGPU`**，保留上游 git 历史，使用 HTML + TypeScript/JavaScript + WGSL 重写 UI；当前 benchmark 仓库只提供 `Architecture Tools / Experimental` 入口、链接或结果导入。它与统一 15 秒 score/RenderDoc 流程的结果语义不同，直接合并会污染当前产品边界。
- TriangleBin README 表明其 SDL/ImGui boilerplate 改编自 [sfalexrog/Imgui_Android](https://github.com/sfalexrog/Imgui_Android)，而该仓库根目录没有统一 LICENSE/GitHub license metadata。WebGPU 移植不得复制这部分 Android/SDL glue；直接使用标准 Web UI，或使用许可证清晰的现代 Dear ImGui WebGPU backend。
- [ ] 若真正开始移植，先向上游开 issue 询问其偏好（上游 `web/` 目录还是独立 fork），再创建外部仓库；这属于新的外部项目，不应在本任务中未经用户确认自动创建 GitHub 仓库或发布页面。

## 12. Cinematic Liquid v2 后的三个立即任务（2026-07-15）

- [ ] 先建立通用 `SessionMode { Benchmark, Explore, Soak }` 和 GUI/CLI 生命周期：Benchmark 固定 15 秒、固定参数并可计分；Explore/Soak 无限运行直到用户停止，默认不自动抓帧，结果只进入独立 diagnostic session，不得污染正式 history。从 Explore/Soak 返回 Benchmark 必须重建资源、恢复固定 seed 并校验 scene/config hash。
- [ ] **Liquid Lab / Explore**：自由 orbit/WASD 相机、暂停/单步/重置；重力、黏度、刚度、物体密度/阻尼与螺旋桨速度可实时调整，网格/粒子/水位/substeps 必须 Apply & Reset；提供 Inflatable Pool / Glass Tank 环境预设，并永久标注“自由实验模式，不可与跑分比较”。
- [ ] **GPU Burn — Unlimited Soak**：最终持续烤机应使用当前主 `gpu_burn_v1` Plasma Bloom，而不是 Other/Legacy 中的旧 `gpu_stress_v1`；默认持续到 Stop/Esc/Ctrl+C，GUI 满载时仍须可响应，显示已运行时间、滚动 GPU time/FPS、利用率、功耗/温度（可得时）和降频趋势。固定 `15s + 第 5 秒 RenderDoc` 仍只用于 Burst score，Soak 不生成正式成绩或硬件错误认证。
- [ ] **VRAM Integrity Soak**：新增独立 `vram_memtest`，优先读取 memory budget，保留桌面/系统安全余量并区分独显和 UMA；分块写入 address/random/walking-bit 等 pattern、设备端校验并累计错误，报告已验证字节、循环数、带宽和错误块。OOM、device lost 与用户停止必须干净退出；`stream` 永远只表示带宽，不表示显存无错误。
- [ ] 开发验收：Liquid Lab 退出后正式 v2 的 scene hash/初始画面恢复；GPU Burn 连续 30 分钟无资源增长且 Stop 可用（发布前建议 2 小时）；VRAM Integrity 连续 30 分钟基线零误报并可从 OOM/device-lost 报告原因。

## 13. 后期水体 RT、路径追踪与超分可行性（2026-07-15，仅规划）

- **禁止提前实现**：本节当前只保存可行性和接口边界。先完善现有 Stream/Particle、GPU Burn、Cinematic Liquid v2、GUI、固定 15 秒/第 5 秒抓帧与结果合同；只有这些通过验收且用户再次明确提高优先级后，后续 AI 才能开始本节代码。2026-07-15 本轮未实现任何 RT、路径追踪或厂商超分代码。
- 结论：均可实现，但它们是 v2 与三个自由/无限模式之后的**独立高级图形套件**，不是 `cinematic_liquid_v2` 的补丁。至少拆成固定光栅/计算基线、`cinematic_liquid_rt_v1`、`water_pathtrace_v1`；更换渲染算法、光追能力或超分供应商不得共用一个 score contract。
- **Hybrid RT 优先于完整路径追踪，但不是小补丁**：当前水是 128x64x96 scalar density + fullscreen fragment raymarch，硬件 AS 只接受 triangle/AABB，不能直接接收 density volume。最小可行版本应为独立 `cinematic_liquid_hybrid_rt_v1`：保留 MLS-MPM、density/whitewater raymarch；把池体、鸭子、彩球、船、沉球做 triangle BLAS，七刚体由 GPU state 更新 TLAS；在现有 fragment 中用 Vulkan `VK_KHR_ray_query` 追踪太阳阴影、水面反射和折射后的实体命中。不要首版做每帧 marching-cubes+BLAS 重建，也不要逐液体粒子建 AABB。
- **跨厂商硬件路线**：Vulkan KHR ray query 可让同一实现运行于 NVIDIA RTX、AMD RDNA2+ 与 Intel Arc 的硬件 RT 设备；不需要 NVIDIA 专有 SDK。API 能证明使用 acceleration structure/ray query，但不能直接证明芯片内部是哪块单元，发布“RT Core/等价单元实际工作”时还需 Nsight Graphics、RGP、Intel GPA counter。Apple 需先完成 Cinematic Liquid Metal backend；Metal `supportsRaytracing` 只说明 API 可用，正式 hardware 组应额外要求明确带 fixed-function traversal 的 Apple family 9（A17 Pro/M3）或更新硬件。
- **硬件 pipeline 不会自动软降级**：同一安装包可以带 hardware/software 两条路径，但缺 extension、DXR tier 或 Metal capability 时硬件 RT pipeline 创建会失败。`Auto` 必须显式检测后选 hardware，否则运行当前 `density_raymarch_soft_v1`；`Hardware RT` 不支持时显示 N/A，绝不能静默 fallback 后混分；`Software` 显式运行现有 shader raymarch。当前 renderer 已经是软件 sphere trace/解析求交/密度 raymarch，足以作为首版 fallback；现在另写通用跨 API software BVH 属于中高工作量且收益不高。
- 硬件与软件结果必须使用独立 workloadVersion/score group，并记录 accelerationStructure/rayQuery/rayTracingPipeline、RT API/tier/hardware class、effective path、fallback reason、geometryVersion、AS mode、rays/bounces、denoiser、`avgAsBuildMs`、`avgRayTraceMs` 与 total GPU time。DXR 与 Metal RT 都要等对应 Cinematic Liquid 后端存在；当前未实现任何 RT 代码。
- **路径追踪首版冻结第 5 秒快照**：固定 RNG seed、SPP、反弹次数、光源和相机后累积折射、吸收与焦散。实时动态水体会每帧破坏 temporal accumulation，只有在 motion vector、denoiser 和时序稳定性成熟后再做 animated path tracing。WebGPU 当前标准没有 acceleration structure、ray query 或 RT pipeline；WGSL compute trace 必须另命名，不能和硬件 RT 混排。
- 建立三类可选插件接口：`IUpscalerPlugin`、`IDenoiserPlugin`、`IFrameGenerationPlugin`。核心渲染器先统一输出低分辨率 HDR color、depth、无 jitter motion vector、camera jitter、exposure、reactive/transparency mask 和 history reset；动态水面必须提供自身运动矢量与 reactive mask，不能只用相机运动。
- **支持边界**：DLSS SR 走 D3D12/Vulkan（并按 SDK 能力处理 D3D11），仅支持相应 NVIDIA 硬件；AMD 当前 FSR SDK 主线优先 D3D12，旧版 Vulkan 插件必须锁定并显示准确版本；XeSS-SR 可接 D3D12/Vulkan；MetalFX 只接 Metal/Apple 并运行时查询能力。WebGPU 标准成绩不得通过 Dawn 私有底层句柄调用厂商 SDK；如做 native interop，必须单列实现。
- **公平比较**分三组：`Native Baseline` 固定输入=输出；`Fixed-Scale Upscaler` 对所有插件使用相同输入/输出分辨率、相同水面回放与时序输入；`Vendor Recommended` 使用厂商推荐 Quality/Balanced，只作体验展示。分别报告 base render/upscale/total GPU time、VRAM 与 PSNR/SSIM/FLIP，不把画质和 FPS 合成单一总分。
- Frame Generation 与 Super Resolution、Ray Reconstruction/denoising 分开。FG 只报告真实渲染 FPS、显示 FPS、生成耗时与延迟，生成帧不得计入完成的模拟/渲染工作量。
- **安装包**：厂商库均做可选、运行时能力检测与动态加载。Streamline/DLSS 只分发 NVIDIA 签名 production DLL 并遵守 RTX SDK 通知/发布条款；当前 AMD SDK binary 按其许可证原样分发并保留 notices，不能笼统声称整个 SDK 都是 MIT；XeSS 允许未修改 binary 再分发但必须附 Intel 许可与第三方通知；MetalFX 是系统 framework，无需捆绑第三方 DLL。所有结果记录 provider、SDK version、API、driver、input/output resolution 与 capability path。
