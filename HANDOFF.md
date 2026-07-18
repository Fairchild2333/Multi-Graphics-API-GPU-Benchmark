# Mangekyo Project Handoff — 当前事实、两条主线与下一步

> 最后更新：2026-07-18（Australia/Sydney）
> 分支 / 提交：`main` / `2947589`（**本轮大量改动尚未提交**，工作树 dirty；勿把 `out/` 构建产物当仓库事实）
> 本文件是项目的**首要进度与交接入口**。后续 AI 或开发者开始工作前先完整阅读，结束工作前优先更新本文件，再更新专题 TODO、roadmap 和 README。

## 1. 交接规则

1. 事实优先级：**可复现测试结果 > 当前源码 > 本文件 > 专题 TODO > roadmap > README**。
2. “代码已写”“可编译”“已实机运行”“跨 API 可比”“GUI 已接入”“可安装发布”是六种不同状态，不得混写成“已完成”。
3. 新发现应带文件与行号；完成项应带命令、机器/API、结果或产物路径。
4. 每次会话结束前至少更新：`当前结论`、`正在进行`、`下一步`、`验证记录`。
5. 不删除原始粒子测试，不改变其历史 workload id（`stream`）、确定性 seed、默认参数或旧结果含义，除非同时做结果 schema/version 迁移。
6. GUI 是今后的主入口。CLI、WinUI、macOS GUI、图表和结果模型必须从同一份测试 metadata/registry 获取能力，避免再次出现 7/5/1 套状态漂移。
7. **不要随便创建新分支**：未经用户明确同意，不得 `git switch -c` / `git checkout -b` 新建分支。在默认/主干分支上的改动按用户指示直接提交或暂存；如确需分支，先说明用途并征得同意。

## 2. 用户锁定的目标（A/B 两条产品主线 + C 平台移植优先级）

### 2026-07-16 品牌与兼容性决定

- 用户可见产品名统一为 **Mangekyo**，说明语为 **Cross-API CPU & GPU Benchmark Suite**。产品定位仍是 **GPU 主测试 + CPU 补充测试**，不能因品牌调整把 CPU 写成与 GPU 后端/图形场景同等成熟的主线。
- 这次是显示品牌与信息架构调整，不是结果合同或内部 ABI 迁移。内部 CMake target、可执行文件和 CLI 命令暂时继续使用 `gpu_benchmark`；workload id、`workloadVersion` 与历史成绩名称保持不变。
- Windows 安装器继续复用既有 stable AppId；用户数据继续写入 `%LOCALAPPDATA%/GpuComputeBenchmark`（或 `GPU_BENCH_DATA_DIR` 覆盖路径）。不要为了改名生成第二套 AppId、数据目录或静默丢失旧 History；若以后迁移安装包文件名，必须做显式升级/兼容验证。
- WinUI 主导航已拆为独立 **GPU** 与 **CPU** 页面，History/Charts 继续共享同一结果存储。文档中的历史窗口标题、旧 ZIP/Setup 文件名和验证记录属于证据，不做盲目字符串替换。
- 2026-07-16 品牌验收：Release CLI 与 WinUI x64 均重新构建通过；`gpu_benchmark.exe --help` 首行显示 Mangekyo，GUI PE 的 `ProductName/FileDescription/FileVersion` 为 `Mangekyo / Mangekyo - Cross-API CPU & GPU Benchmark Suite / 0.1.0`。stage verifier 为 0 errors，并实际生成 `Mangekyo-0.1.0-windows-x64.zip` 与 `Mangekyo-0.1.0-windows-x64-setup.exe`，Setup VersionInfo 为 `Mangekyo / Mangekyo Setup / 0.1.0`。本次品牌产物是 CLI-only smoke，用于验证命名和安装器；它不替代带 WinUI、RenderDoc、report worker 与许可证的完整 GitHub Release 候选，完整发布仍须在提交后重跑正式 release 脚本。

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

### 目标 C — 平台与兼容前端优先级（2026-07-16 用户最新锁定）

用户最新锁定的顺序：**1. Windows on ARM (ARM64) → 2. Windows 7 专用 GUI（尽量使用 Aero）→ 3. macOS → 4. Android → 5. iOS → 6. Debian Linux → 7. WebGPU → 8. HarmonyOS PC / 鸿蒙 → 9. PS3（探索性）→ 10. Dual-GPU Aggregate（双卡合力模式，功能项）**。其中 Windows ARM64 是下一步，Windows 7 GUI 是紧接其后的下一步；这项最新决定覆盖此前“先关闭液体 SPH blocker / 先做三个自由与无限模式，再开始平台移植”的切片顺序。液体正确性、自由模式和 soak 任务仍然开放，但不再是当前两刀。

除仓库已有的隔离 HarmonyOS Vulkan 粒子原型外，均未开始完整产品移植；该原型不等于主 workload suite 已移植。Windows 7 GUI 是共享现有引擎与 worker 协议的兼容前端，不得为了支持旧系统复制出另一套含义不同的成绩合同。

通用规则：
- 逐平台落地时沿用现有合同规则：能力不齐明确 unsupported、不静默 fallback；**计时/抓帧模型不同的实现必须使用新 `workloadVersion` 独立成组**，现有 Windows 成绩组（`stream`、`gpu_burn_v2_mangekyo_faceted_glass_v1`、`gpu_burn_v1`、cinematic liquid 各版本）的 A/B 对比不受任何影响。
- 每个发布包必须记录主程序、GUI、安装器 bootstrap 与所有原生 DLL/EXE 的真实 PE 架构。ARM64 包优先使用原生 ARM64 依赖；任何 x64/ARM64EC/仿真组件必须显式列入 manifest/SBOM 和 UI 能力说明，不能静默混入并宣称“全原生 ARM64”。
- 本节顺序与第 9 节保持一致；后续 AI 不得再按旧的液体优先级自行调回顺序，除非用户再次明确调整。

逐平台要点：
1. **Windows ARM64（已于 2026-07-16 完工；2026-07-17 安装器主路径改 WiX）**：已实现本体、GUI、依赖与打包闭环。新增了 VS/CMake ARM64 配置，原生编译 `gpu_engine`、CLI 和 WinUI，使用 vcpkg `arm64-windows` 在 manifest 模式下引入原生 GLFW。完成了动态 ARM64 Vulkan 导入库自动生成，解决了 x64 SDK 链接冲突；豁免了 VC 运行时中特有的 x64 `vcruntime140_1.dll` 架构审计。**CPU 与 GPU 测项本体也是按架构分别原生编译**（x64 包 = AMD64 PE，ARM64 包 = ARM64 PE；同一套源码、两套二进制），不是 x64 测项装到 ARM 上。2026-07-17 起发布安装器主路径为 **CPack WiX MSI**（x64 / ARM64 各自原生 `Template`），Inno Setup 降为 legacy。
   - **依赖规则**：GLFW、WinAppSDK、VC runtime 等均使用原生 ARM64；豁免了特殊的 x64 `vcruntime140_1.dll`（Redist 目录自带）。RenderDoc 在 ARM64 发布语义上可为 Skip/N/A；Python 报告链仍未冻结。
   - **完成门槛**：ARM64 本体、GUI、ZIP/MSI 与依赖本地 staging 校验与 PE 架构审计已通过；clean-machine 与签名仍开放。
2. **Windows 7 专用 GUI（ARM64 后紧接的下一步）**：当前 WinUI 3 / Windows App SDK 与现有安装器最低版本是 Windows 10 1809，不能通过改 manifest 假装支持 Windows 7。另建共享 `gpu_engine`/CLI worker/结果 schema 的原生 Win32 兼容前端与独立安装包；优先使用 DWM/Aero 能力（例如玻璃区域、非客户区整合、主题化控件、Direct2D/DirectWrite），但必须运行时探测 DWM composition，并在 Aero Basic、经典主题、远程桌面或 DWM 关闭时正常回退，不能把透明/模糊当硬依赖。
   - Windows 7 包不得携带 WinUI/WinAppSDK payload；需要单独审计 toolset、Windows SDK、VC runtime、GLFW、DX11 FL10/SM4 与抓帧工具的 Win7 兼容版本。若没有可安全再分发且实测可用的 RenderDoc 组合，第 5 秒抓帧必须明确显示 unavailable，而不是降级到含义不同的捕获或伪造成功。GUI 外观不同不应改变 GPU/CPU 计时与成绩组；任何实际 worker、timer 或 capture 合同差异才触发新 `workloadVersion`。
3. **macOS**：Metal 后端目前只覆盖粒子 workload（GPU Burn 明确 unsupported、液体无 Metal 实现）；需要主 workload 的 Metal 移植、SwiftUI GUI 与统一 registry 对齐、`MTLCaptureManager`(.gputrace) 替代 RenderDoc；PathService 路径已就绪。
4. **Android**：复用 Vulkan 后端；GLFW 不支持 Android，需 NativeActivity/ANativeWindow 表面层与新前端；RenderDoc 支持 Android 远程抓帧；须评估温控降频对 15 秒 Burst 语义的影响（可能需要独立移动端 duration 合同）。
5. **iOS**：仅 Metal（或 MoltenVK）；无 RenderDoc，抓帧走 `MTLCaptureManager`；与 macOS 共享 SwiftUI 前端；受 App Store 分发约束。
6. **Debian Linux**：技术摩擦最低——Vulkan/OpenGL 后端、GLFW、RenderDoc、XDG 数据路径全部已有；主要是构建修正、`.deb` 打包、CI 与实机验证。
7. **WebGPU**：按第 11 节既定路线执行——先统一 capability registry（P0），再固定 Dawn 版本的原生后端，依次移植 Stream、GPU Burn、Cinematic Liquid，最后 `/web` 浏览器前端。结果使用独立临时版本 id（如 `stream_webgpu_v1`），记录 `apiImplementation/underlyingBackend/timingMode`；timestamp-query 是可选能力，没有可靠 GPU timestamp 不产生正式 score；浏览器无 RenderDoc，抓帧标注不可用。
8. **HarmonyOS PC / 鸿蒙**：仓库已有 `ohos/` 的独立 Vulkan 粒子 demo，但没有统一 workload registry、GPU Burn/Cinematic Liquid、主 CLI/GUI、15 秒结果合同或抓帧编排。该移植项是把它升级为与主产品边界一致的正式端口；在能力与捕获模型明确前必须使用独立结果组，不能把现有 demo 写成已完成移植。
9. **PS3（探索性，永不进成绩体系）**：官方开发授权已停止，只能 homebrew（PSL1GHT/RSXGL）；RSX 无 compute/SSBO/原子/GPU timestamp，PSGL≈OpenGL ES 1.0+Cg 而非桌面 GL 4.3，三个主测试均不可直移。至多做隔离的固定管线情怀 demo，建议独立仓库，不进入正式成绩合同与 GUI 主界面。
10. **Dual-GPU Aggregate（双卡合力模式，功能项而非 OS 移植；首个验证目标：Boot Camp Windows 下的 Mac Pro 2013 双 FirePro D700）**：显式引擎级多 GPU 协作——驱动层 CrossFire 对自研引擎无效，不作依赖。切片顺序：(a) `stream` headless 双独立设备聚合：一进程两个 device 各算一半粒子、帧边界 CPU 同步、吞吐相加，新成绩组 `stream_dualgpu_v1`，结果必须记录两张 adapter 的完整元数据；(b) N-body 双卡（每步交换位置，~1MB/步）；(c) GPU Burn 分屏 SFR 或 AFR（DX12 unlinked 显式多适配器 + 跨适配器堆）。**不做** DX12 LDA / Vulkan device-group 依赖（D700 22.6.1 老驱动是否暴露不确定），**不做** Cinematic Liquid 域分解（每 substep 跨 PCIe 网格同步属科研级，不值）。单卡成绩组不受影响；混插不同型号也应能跑（聚合分数需标注非对称配置）。安全注意：双 D700 同时满载是 Mac Pro 2013 已知散热死穴，15 秒 Burst 可以，禁止双卡长时烤机。

## 3. 当前结论（先读这一节）

### 3.1 产品判断

建议最终只把三条路线放在主界面：

| 主测试 | 定位 | 当前状态 |
|---|---|---|
| **Particle (Original / Baseline)** | 原始粒子计算+绘制；历史基线，实际偏显存/内存带宽 | 已保留稳定 id `stream`；GUI 明确标为 Memory Throughput |
| **GPU Burn (15s Burst)** | 主项为透视 3D 切割玻璃 Mangekyo Kaleidoscope v2；Plasma Bloom v1 作为公开 legacy 保留；后续扩展 CoreBurn + MixedBurn 与错误校验 | **`gpu_burn_v2_mangekyo_faceted_glass_v1` 已实现**：Vulkan/DX12/DX11/OpenGL；RTX 5090 Vulkan 自动标定 16→2048 后 13.199 ms render / 286.00 Gpix-step/s / 90.4% timestamp utilisation；Metal 明确 unsupported；v1 回归通过 |
| **Cinematic Liquid** | 固定镜头的真实 3D 粒子液体 + 粒子重建密度体积自由表面；后续叠加完整综合场景 | **v1 正式合同与历史 v2 optics_v4 正式成绩已验证并保留**；当前 MLS-MPM 为 `cinematic_liquid_v2_physical_scene_v8` / `sceneVersion=5`，SPH 为强制 `_preview`。SPH 15 秒视觉已获用户接受并正常结束，但四项正确性 blocker、正式第 5 秒抓帧/成绩、WinUI 交互/history 与跨后端仍开放 |

`15s` 应对外称为 **Burst / 短时峰值压力**，不能宣称完成热稳定认证。真正的热饱和、显存错误或超频稳定性通常需要可选的数分钟模式；这不改变用户要求的默认 15 秒流程。

### 3.1.1 2026-07-15 历史暂停点与恢复状态

用户曾明确要求**暂时停止新的实机测试、预览运行和 RenderDoc 抓帧**；该暂停在当时已遵守。此要求是历史上下文，不再是当前阻塞：用户随后先授权 duck family 视觉改造，之后又明确要求继续流体工作，本轮据此恢复了水体光学实现，并只做了必要的短 `_preview` 验证。

2026-07-15 晚间第一阶段：用户明确提出“照参考图优化鸭子造型，加大鸭子/小鸭子/子母鸭”，据此完成 duck family 场景改造（见 3.1.1a）及最短预览。该 v5 鸭群改动当时原样保留到 v7，并继续保留到当前场景；上一个四刚体基线 `cinematic_liquid_v2_surface_splat_optics_v4` / `shaderVersion=6` 及其 15 秒正式结果永久保留为独立成绩组。

### 3.1.1a 2026-07-15 Duck family 场景改造（v5）

- 鸭子 SDF 重做：旧的 7 块硬拼椭球（肚+胸+尾+双翅，接缝生硬）替换为 smooth-min 融合的经典大黄鸭（丰满蛋形身体 + 上翘尾 + 大圆头 + 扁宽橙嘴）。三处实现保持同一份形状数学：`shaders/mls_mpm_grid_update_v2.comp`、`shaders/mls_mpm_g2p_v2.comp`（碰撞）与 `shaders/cinematic_liquid_render_v2.frag`（渲染改为对同一 SDF 做 72 步 sphere-trace，黑眼珠/白高光/橙嘴均为贴花上色，不再是独立几何）。
- 新增 3 只约 0.45 倍的小鸭子（刚体 4-6，`kCinematicLiquidV2BodyCount` 4→7）；sink sphere 的释放编排硬编码在 lane 3，因此新刚体只允许追加在 index 3 之后。小鸭质量 ~1.82、扶正力矩更强（shape0.w=26）、线/角阻尼更高，初始间距均大于两两 bounding 半径之和，避免出生瞬间被 pair solver 弹开。
- grid-update / g2p 的刚体循环加入保守 bounding-sphere 剔除，七刚体循环对绝大多数节点/粒子只付一次距离平方判断。
- 结果身份升级：`workloadVersion=cinematic_liquid_v2_duck_family_v5`（预览为 `..._preview`）、`shaderVersion=7`、`sceneVersion=3`；不得与 optics_v4 混分。

### 3.1.1b 2026-07-15 Iterative optics v6（历史预览合同）

- 用户调整后的鸭子及 duck family v5 的 7 刚体/SDF/碰撞布局全部保留；本轮只在其上升级水体重建与光学，没有回退或覆盖鸭子造型。
- 当时结果身份为 `workloadVersion=cinematic_liquid_v2_iterative_optics_v6`（短运行写为 `..._preview`）、`shaderVersion=8`、`sceneVersion=3`。它与 v1、历史 optics_v4 正式组、duck-family-v5、v7 及当前 v8/SPH 均不得混分。
- 表面重建升级为 5x5x5 binomial filter（`mix=0.90`）；ray path 最多处理 4 次界面，逐段使用 Fresnel/Snell 与 Beer–Lambert，并在每一段重新做刚体/池体/地面深度排序。当时消光系数为 `extinction=(30,10,8)`，输出采用 linear exposure + sRGB，不再使用 ACES；密度体积边界强制归零，避免 clamp 边缘形成实体水盒。
- 最终短预览结果 `20260715-221447-024`（RTX 5090 / Vulkan / 6 秒、5.1 秒抓帧）：Compute 9.450 ms、Render 3.003 ms、Total 12.454 ms、`257.01 MParticle-step/s`、284 measured frames；0.117 秒抓帧开销已排除，1 attempt/1 saved。核验图为 `rdoc_captures/cinematic-liquid-v2-5s-iterative-optics-v6-final-preview.png`。
- **v6 从未运行正式 15 秒流程，没有正式成绩。** 当时 CLI Release 与 WinUI Release x64 已构建通过；该历史结果不能转记为 v7。

### 3.1.1c 2026-07-15 Physical scene v7（历史合同）

- 历史结果身份为 `workloadVersion=cinematic_liquid_v2_physical_scene_v7`、`shaderVersion=9`、`sceneVersion=4`；固定 128x64x96 网格、10 substeps 与 **320,920 粒子**，分解为 142x14x98 base（194,824）+ 48x37x71 dam（126,096）。v1/v4/v5/v6 的正式结果或预览全部原样隔离。
- 用户鸭子和 7 刚体 index ABI 原样保留：母鸭 index 0、彩球 index 1、船 index 2、沉球 index 3、小鸭 index 4-6。船由硬锚定改为 34 kg 有限质量刚体和软系泊，推进器反冲与流体冲量可驱动/摇摆；这只说明实现路径已接通，**尚未视觉验收船的实际轨迹**。
- index 3 沉球仍为 1.06 水密度比，4.28 秒释放，使用 -9.81 重力与 0.015 空气阻尼；材料水阻按 displaced mass 和浸没率门控，以避免未入水时就产生水下式阻尼。沉球入水 crown/whitewater 来自真实 GPU body state 与局部流体响应，不是 fragment shader 画出的假水花；当前仍没有 secondary-spray 粒子 pass。
- v7 当时的池体合同使用有限高度的内嵌碰撞壁（inset 0.22）；外层 simulation catch band 允许粒子真实越过池沿并落到地面，但带宽有限，不能描述成无限流体域。5x5x5 binomial surface resolve 自适应保留低支持喷滴。
- 前景侧壁为独立透明软 PVC 薄膜近似：IOR 1.50、Fresnel、弱吸收和程序化 wrinkle；这**不是完整的 PVC 多介质 ray path**。池底内衬使用独立 hit 分类，使水下焦散落到实际可见内衬而非被遮住的草地平面。场景加入无限程序化草地与大气天空/云，没有引入 cubemap 资产。
- v7 只完成 shader 编译、最终 6 个 SPIR-V 校验、CLI Release 与 WinUI Release x64 构建；WinUI 为 0 error，仅有既有 MSB8027/C4996/LNK4042 类 warning（最新增量构建显示 2 个重复 WinAppSDK warning，源文件重编时曾显示 4 个）。只运行了若干 `--time 6` / `--time 8` 自动停止 smoke：**没有正式 15 秒、没有可靠保存的新 RenderDoc、没有完整视觉验收**。控制台 transient `241.13` 不是正式或持久结果。用户看到窗口约 2-3 秒后关闭，是测试脚本 `--time 8` 的自动生命周期，不是目前证实的崩溃。

### 3.1.1d 2026-07-16 当前 shared scene v8 与 SPH 收口

- 后续共享池体/光学参数已经变为 `poolWallInset=0.45`、`poolWallTopFraction=0.42`、`extinction=(12,3.6,2.5)`，因此当前 MLS-MPM 身份升为 `cinematic_liquid_v2_physical_scene_v8`、`sceneVersion=5`；任何历史 v7 smoke 都不得转记为 v8，v8 尚无正式成绩。
- SPH vertical slice 为 318,464 粒子。RTX 5090/Vulkan 已完整运行 15 秒且正常结束；用户接受水池、水面、鸭群、球、船、草地/天空的当前外观，**视觉迭代已经收口**。
- 视觉通过不等于成绩通过。SPH 在以下四项全部关闭前，任何时长（包括 15 秒）都强制写入 `cinematic_liquid_sph_slice_v1_preview`：渲染帧驱动 2×1/120 时间推进、每 substep 未清 `bodyImpulses`、viscosity 原位 SSBO race、atomic scatter cell-order 非确定性。草地吸收/回收倒计时的当前实现是 0.50–1.80 sim 秒。

“是否基本完成”必须按层次回答：

- **Cinematic Liquid v2 的 Vulkan benchmark vertical slice：视觉上基本完成，正式计分仍未完成**。SPH 的 15 秒外观已获用户接受；只有 optics_v4 有正式历史成绩，v7 只有历史 smoke，当前 MPM v8 没有正式成绩，SPH 则被代码强制为 preview。
- **用户要求的当前水体视觉：已在 SPH 切片上通过验收**。这取代旧 optics_v4 “水体太假”的当前视觉结论，但不抹除该历史反馈，也不关闭上述四个正确性 blocker。
- **完整产品：未基本完成**。最终 WinUI 交互/history、跨 GPU 固定时间推进、其余四后端、异常资源清理、安装器 clean-machine 验收以及无限/自由模式仍开放。

恢复前的只读上游源码对照已从 v6 保留到 v7：MIT [jeantimex/fluid raymarch](https://github.com/jeantimex/fluid/blob/9daf3ae2add7dedc3eadcc10da5e4a44ef1b771f/src/sph/3d/webgpu_raymarch/shaders/raymarch.wgsl) 使用最多 4 次界面迭代、Fresnel、Beer–Lambert 与环境光；其 [raymarch 配置](https://github.com/jeantimex/fluid/blob/9daf3ae2add7dedc3eadcc10da5e4a44ef1b771f/src/sph/3d/webgpu_raymarch/main.ts) 为 `densityTextureRes=150`、`stepSize=0.02`、`maxSteps=512`、`IOR=1.33`、`numRefractions=4`、`extinction=(12,4,4)`、`renderScale=0.5`。本项目没有 vendor 上游代码。该历史对照后来促成独立 SPH vertical slice；当前 SPH 外观已经验收，**完成 ARM64 与 Windows 7 两个最新优先切片并回到流体路线时**，应先关闭四项正式计分 blocker；secondary spray 只能作为真实版本化模拟增强，不能用 fragment 装饰冒充。

### 3.2 源码真实状态

CLI 与 WinUI 当前暴露 11 个 workload 选择（`cinematic_liquid_v1` 是独立公开选择，持久化时沿用历史 `workload=cinematic_liquid` 并靠 `workloadVersion` 隔离）：

| Workload id | 真实负载 | 后端代码状态 | 建议归类 |
|---|---|---|---|
| `stream` | 原始粒子 Euler 更新 + 点绘制；低算术强度，偏带宽 | Vulkan/DX12/DX11/OpenGL/Metal | **主界面：Particle (Original)** |
| `gpu_burn` | Mangekyo faceted glass v2：透视相机、截顶宝石/拉伸八面体碎钻、前后深度层、SDF 平面法线、Fresnel 反射、RGB 折射色散与吸收；2 次全屏 opaque draw、固定 step FP32/SFU/INT 循环；自动标定目标约 14 ms render | Vulkan/DX12/DX11/OpenGL；DX11/DX12 WARP；Metal 明确拒绝 | **主界面：Mangekyo Kaleidoscope — GPU Burn**；`gpu_burn_v1` 为 Plasma Bloom legacy |
| `gpu_stress` | 独立 shader 的 4 次全屏 opaque overdraw；FP32/SFU/INT 循环，warmup 首秒自动标定到约 8 ms/draw-group | Vulkan/DX12/DX11/OpenGL；DX11/DX12 WARP；Metal 明确拒绝 | Other / Advanced：GraphicsBurn component |
| `nbody` | tiled all-pairs 粒子计算 | 五后端 | Other / Advanced Compute |
| `stress` | 全屏固定次数 fractal + `sin()`，fragment ALU/SFU | 五后端 | **Legacy Stress v1** |
| `synthpeak` | 寄存器内合成峰值循环 | 五后端，精度能力不同 | Other / Advanced Synthetic |
| `render3d` | 6 顶点实例化 billboard + 深度 | 五后端 | **Legacy 3D Prototype** |
| `volumetric` | 程序化 FBM 体积 raymarch | 五后端代码已接入，但仓库结果无验证记录 | Other / Experimental；未来综合场景 pass |
| `cinematic_liquid` | 3D MLS-MPM 或 `--liquid-solver sph` + 独立粒子 splat/binomial R32F 密度体积 + 最多 4 界面自由表面 ray path；GUI 文案为「流体 —— 互动水池」 | **仍为 Vulkan-only**（CLI `--backend dx12` 拒绝；GUI InfoBar + API picker 只露 Vulkan）。曾短暂尝试 DX12 HLSL 移植并做过 smoke，用户要求后已撤回 DX12 宿主路径；仓库里可能仍有未提交的 `shaders/cinematic_liquid_v2_*.hlsl` / `cinematic_liquid_v2_common.*` 草稿，**不等于** DX12 已支持。DX11/OpenGL/Metal 尚未实现 | **主界面：Cinematic Liquid / 互动水池**；所有历史版本、当前 v8 与 SPH preview 必须按 `workloadVersion` 分组 |

| `cinematic_liquid_v1` | 原始 181,216 粒子/96x56x64 MLS-MPM 溃坝 | Vulkan 正式 15 秒 + 5.1 秒 capture 已通过 | **Legacy Cinematic Liquid v1**；结果使用历史 `workload=cinematic_liquid` + `workloadVersion=cinematic_liquid_v1` |
| `fluid` | 旧 2D Stable Fluids 多 pass | **只有 Vulkan 原型，且当前不正确** | Other / Legacy；不得产生正式成绩 |

仓库内历史 `results/results.json` 仍为 232 条：`stream=224`、`nbody=5`、`stress=1`、`synthpeak=1`、`render3d=1`、`volumetric=0`、`fluid=0`。本轮 smoke 数据刻意写到 `out/*/results`，没有污染历史库。新结果 schema 为 v2，记录 workloadVersion、最终标定参数与 capture 状态；旧结果读取时仍按 schema v1 兼容。

`gpu_burn` 当前选择 `gpu_burn_v2_mangekyo_faceted_glass_v1` 正式图形 Burst，score 仍为版本化的 `Gpix-step/s`；公开 `gpu_burn_v1` 选择保留原 Plasma Bloom shader 与历史成绩身份。被用户否定的二维 plasma chamber 原型没有沿用成绩身份；当前 shaderVersion=3 使用真实 3D SDF 晶体表面、透视/视差、Fresnel 和 RGB 色散。RTX 5090 / Vulkan 自动标定到 2048 steps/draw 后 render 13.199 ms、286.00 Gpix-step/s、90.4% timestamp utilisation，5 个 1 秒窗口 stableScore 288.21 / CV 1.50%。这次短测没有重新声明 v1 的 600W/NVML 热稳定证据，15 秒默认流程也仍不是热稳定/硬件错误认证。`gpu_stress_v1` 保留为 Advanced **GraphicsBurn component**；三者都没有 GPU readback/CPU 对照，shader recurrence/checksum 目前只用于防止 dead-code elimination。`cinematic_liquid_v1` 的已验证成绩合同继续保留，不复用旧 `fluid` 的 2D dye score 或错误状态。v2 的正式历史成绩属于 `cinematic_liquid_v2_surface_splat_optics_v4`；当前 MPM 代码身份是 `cinematic_liquid_v2_physical_scene_v8`，没有正式成绩，SPH 只有强制 preview，均不能沿用 v4 的正式成绩或 v6/v7 的值。GUI 交互、跨 GPU 轨迹可比性和异常路径资源清理仍开放。

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
- 正式计分复核已关闭三类污染：所有 per-core 现在使用相同 seed/依赖轨迹；
  stdout 格式化/flush 完全移出被测线程；multi 测量窗口内完全不输出，只在
  轮次边界更新。multi worker 的热计数为线程局部并按 128-byte cacheline
  隔离，结束计时后才发布，避免 false sharing。
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
  CPU/GPU/Charts 入口现在全局互斥；Formal 会同时恢复 15.0/0.2，任一值偏离
  即取消 Formal 标识；raw/progress 批量节流，并审计 meta/version、topology、
  逐核 summary 与 multi 完整性，不能把“exit 0 但协议截断”显示为完成。
  CLI 与 WinUI Release x64 均已构建；隔离临时数据目录的 0.1 秒 GUI E2E 已
  显示 16 条逐核、平均、多核、100%/Done 且恢复 Run。该运行只是 preview
  orchestration smoke。成功运行会把 per-core average 与 multi summary 分别
  追加到现有 `results.json`/History；详细逐逻辑核行只在本次页面/协议中。
- 持久化 `workloadVersion` 包含 kernel、affinity capability、formal/preview、
  round/time/warmup 以及 multi 是 standalone 还是 after-percore，避免 Quick、
  Formal、不同绑核能力和不同热状态顺序静默混排。

**仍开放的发布/跨平台边界：**

- 当前已有本机 CLI 与 WinUI build/smoke，不等于干净机安装验收已完成；公开安装包前须在干净 Windows 机器验证 MSI 安装后 GUI 能找到
  相邻 `gpu_benchmark.exe`、Run/Cancel、结果写入与卸载保留用户数据。
- 2026-07-17 起主发布产物是 **ZIP + WiX MSI**（不是 Inno `*-setup.exe`）；x64/ARM64 测项本体均为对应原生 ISA。
- Windows 的 `EfficiencyClass`、Linux/Android 的 `cpu_capacity`/最大频率只
  用来生成 `InferredPerformance/Efficiency/Middle/LPE` 排名标签，**不是**
  CPUID/SoC 官方核心身份，不能宣称已精确识别所有 P/E/Mid/LPE 变体。
- Linux/Android 会枚举 allowed cpuset，并要求 `pthread_setaffinity_np` 成功后
  回读为唯一目标 CPU；成功身份为独立的 `strict_sched_affinity`，失败会
  `valid=0`/exit 3。原生 Linux、容器/cpuset 与 Android 设备构建仍未实测。
  macOS 只能 `scheduler_managed`，logical→physical/SMT 是估计；各 affinity
  capability 已进入版本，不能与 Windows strict 成绩混排。
- Android/iOS 与 Web/WASM CPU 模式均未构建验收。浏览器只能看到暴露的
  logical concurrency，不能选择、硬绑定或可靠分类宿主核心；移动端还需独立
  热状态/前后台合同，禁止把当前源代码存在写成已完成端口。

### 3.5 WinUI 秒数输入样式（2026-07-17）

- 用户要求把当前横向内联上下箭头改成参考图中的竖向浮层效果。`DurationValueBox`、`CaptureValueBox`、`CpuTimeBox` 与 `CpuWarmupBox` 现统一使用 WinUI `NumberBox` 的 `SpinButtonPlacementMode="Compact"`；默认值、范围、步长与成绩合同未改变。
- Compact 交互补层现包括：Enter 提交后把焦点移到当前 GPU/CPU 导航项；GPU/CPU 滚动内容的外部点击会移走 NumberBox 焦点；× 或编辑产生 `NaN` 时立即恢复 `OldValue`（无合法旧值才取 Minimum），避免原生上下按钮进入全灰状态；`UpDownPopup` 追加轻微水平 `PopupThemeTransition`，不复制整份 WinUI 控件模板。
- 源码、独立输出目录以及正式 `gui/x64/Release` 目录的 Release x64 构建均已通过。按用户明确要求，本轮最终版本只改代码和重新编译，没有启动 GUI；逐控件视觉与手动交互仍由用户侧验收。

## 4. P0 正确性阻塞项

仍开放的问题修复前，不应把旧 `fluid` 计入正式排行榜。GPU Burn
`gpu_burn_v2_mangekyo_faceted_glass_v1` 已作为带版本的新主项接入；`gpu_burn_v1` 只作为公开 legacy 保留，两者均不与 `gpu_stress_v1` 或旧 `stress` 混分。

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

### 4.4 Cinematic Liquid v2：历史 optics_v4 正式通过；当前 v8/SPH 尚未正式计分

已完成并验证的 Vulkan 切片：

- [x] 固定 128x64x96 MLS-MPM 网格、10 substeps 与 320,920 粒子；粒子分解为 142x14x98 的浅水底（194,824）和 48x37x71 的溃坝体（126,096）。当前固定参数为 stiffness 45,000、viscosity 0.035、maxSpeed 8。
- [x] 用户鸭子和七刚体 ABI 保留：船 index 2、沉球 index 3、小鸭 index 4-6。船由硬锚定改为 34 kg 有限质量/软系泊，推进器反冲可驱动和摇摆，但轨迹尚未视觉验收。沉球保持 1.06 水密度比、4.28 秒释放、-9.81 重力、0.015 空气阻尼；材料水阻按 displaced mass 与浸没率门控。
- [x] 当前池壁为有限高度、inset 0.45、wall-top fraction 0.42 的内嵌碰撞体；有限宽 outer catch band 允许真实越沿和落地，但不是无限流体域。render push 的 `pool.z` 已对齐真实 ground/pool-floor `y=0`，PVC bottom、liner、无限草地与物理粒子底面共面；pool ring separation 由 `2*tube` 推导，避免视觉池沿与物理地面漂移。
- [x] 当前表面不直接读取 MLS-MPM 网格质量：粒子以 Spiky² 核和 fixed-u32 原子独立 splat 到 128x64x96 R32F 密度体积，5x5x5 binomial resolve 会自适应保留低支持喷滴。沉球入水 crown/whitewater 来自真实 GPU 刚体状态与局部流体，不是 fragment 假水花；没有 secondary-spray 粒子 pass。
- [x] 当前 renderer 保留最多 4 次水/空气界面的 Fresnel/Snell、Beer–Lambert 与 opaque depth sorting，并使用 `extinction=(12,3.6,2.5)`、linear exposure 和 density 边界归零。新增独立透明软 PVC 前景薄膜（IOR 1.50、Fresnel、弱吸收、wrinkle），但不是完整 PVC 多介质 ray path；环境为无限程序化草地与大气天空/云，没有 cubemap 资产。历史 v6 的 `(30,10,8)` metadata 不回写。
- [x] 结果身份严格隔离：历史正式 `cinematic_liquid_v2_surface_splat_optics_v4` / `shaderVersion=6`；鸭群预览 `cinematic_liquid_v2_duck_family_v5` / `shaderVersion=7`；iterative optics v6 预览 / `shaderVersion=8`；历史 physical-scene v7 / `sceneVersion=4`；当前 `cinematic_liquid_v2_physical_scene_v8` / `shaderVersion=9` / `sceneVersion=5`；SPH 始终 `cinematic_liquid_sph_slice_v1_preview`。
- [x] RTX 5090 / Vulkan / 正式墙钟 15 秒 + 5.1 秒 RenderDoc 已通过：结果 id `20260715-170629-492`，Compute 10.572 ms、Render 1.553 ms、Total 12.125 ms、`263.98 MParticle-step/s`、966 measured frames；capture 0.103 秒已排除，`captureAttempts=1`、成功保存 1 个。最终核验图为 `rdoc_captures/cinematic-liquid-v2-5s-formal-optics-v4.png`。
- [x] 上一条只验证 optics_v4。历史 v6 的 6 秒短预览为 `20260715-221447-024`：Compute 9.450 ms、Render 3.003 ms、Total 12.454 ms、`257.01 MParticle-step/s`；它属于 v6 `_preview`，不是正式成绩，更不能归到 v7。
- [x] 历史 v7 的 shader、最终 6 个 SPIR-V、CLI Release 与 WinUI Release x64 build 均通过；只运行若干 `--time 6/8` 自动停止 smoke，无正式 v7 成绩。当前 SPH 已实际完整显示 15 秒并获用户视觉接受，但该运行仍是 preview，不是正式成绩。

视觉结论必须诚实：当前 SPH 外观已获用户接受，视觉迭代收口；这不代表正式 benchmark 完成。secondary spray/foam 与 SPH propeller wake 仍是后续增强，光追也只改善反射/阴影/焦散。当前计分 blocker 是渲染帧驱动时间、每 substep 冲量清零、viscosity 原位 race、atomic scatter 顺序四项，而不是继续调水色。

仍开放、因此**不得把整个 v2 标为完成或跨 API 完成**的 P0：

- [ ] 为 `cinematic_liquid_v2_physical_scene_v8` 运行正式 15 秒 + 第 5 秒 RenderDoc 并建立独立正式结果；在完成前不得引用 optics_v4 的 `263.98 MParticle-step/s` 或 v6/v7 的值作为 v8 成绩。
- [ ] SPH 先关闭四项正确性 blocker；在此之前任何时长都必须 `_preview`。关闭后再运行正式 15 秒 + 第 5 秒 RenderDoc/timestamp/确定性验收。
- [ ] 用当前 v8/SPH build 实机验收 WinUI 的 workload/solver 选择、Vulkan-only 能力限制、启动/结束与 History/Charts 的 `workloadVersion` 分组。
- [ ] 读取/记录船、沉球和越沿粒子的精确 GPU 状态轨迹，并用可靠新 capture 验收船运动、1G 下落/入水减速、局部 crown/whitewater、池壁/PVC 和室外环境；代码路径与自动 smoke 不能代替数值/视觉验收。
- [ ] 当前固定 dt/每个渲染帧推进固定 substeps 会把 GPU 帧率反馈进 5 秒场景状态。必须冻结跨 GPU 的时间推进/补步/丢步策略与第 5 秒 trajectory contract，否则同一 workload 在快慢 GPU 上可能捕获不同模拟时刻。
- [ ] 审计 Vulkan timestamp 起止点，确保 Compute/Render/Total 只覆盖合同规定的 P2G/G2P、surface clear/splat/blur/resolve 和 raymarch 边界，不漏计新增 pass，也不把 present/CPU 等非 GPU 工作混进分项。
- [ ] 补齐初始化失败、surface/swapchain 异常、device-lost 和提前退出路径的 surface atomic buffer、R32F image、descriptor、pipeline/layout 等资源释放；正常退出不泄漏不能替代异常路径审计。
- [ ] DX12、DX11、OpenGL 与 Metal 尚未实现 cinematic liquid v2。必须在 Vulkan scene/pass/quality/trajectory contract 冻结后逐后端移植并实机验证；当前不得显示为 supported，也不得静默 fallback。

## 5. GUI 与文档同步状态

### Windows WinUI 3

- 下拉框当前实际暴露 12 个 workload 选择，参数映射、score parser 与 History label/filter 覆盖对应公开 id；`gpu_burn_v1` 与 `cinematic_liquid_v1` 作为独立 legacy 选择展示。
- GPU Burn 主项为 `Mangekyo Kaleidoscope — GPU Burn`，Plasma Bloom v1 收入 legacy；`gpu_stress` 归入 Advanced，旧 `stress`、`render3d` 明确标 Legacy，volumetric 标 Experimental，fluid 标 Vulkan-only Developer Preview 并阻止非法后端。
- Custom 与 Quick 的非 headless 运行现在默认追加 `--capture 5`；默认 duration 仍为 15 秒。Full analysis / Flights / Particle 预设也继续抓帧。
- installed/staged 布局优先使用同目录 CLI/资产；若不存在外部 CLI，静态 engine 以 GUI module 目录运行，不再强制切到 repo CWD。
- Full Analysis 优先调用随包 `tools/RenderDoc/renderdoccmd.exe`；报告输出改到用户数据目录。若缺 Python/冻结 report worker 或任一后处理命令失败，GUI 现在显示“benchmark 已完成、报告不可用/失败”，不再误报图表与报告已成功生成。
- Charts 页从真实用户结果路径读取并写入 `%LOCALAPPDATA%/GpuComputeBenchmark/reports/images`；图表脚本与 GUI 现已包含 `gpu_burn`/`gpu_stress`，只比较同 workloadVersion，并按设备×API 诚实分组。
- Release x64 已用最终 v2 `gpu_engine.lib` 重编并成功（0 error，2 个既有 duplicate WinAppSDK warning）。
- 当前 v8/SPH 的 WinUI 仍须实际完成 workload/solver 选择、Vulkan-only 限制、完整 run 和 History/Charts 当前/历史版本分组验收；不得把“可编译”或 SPH 独立 CLI 视觉运行写成 GUI 全流程通过。

### macOS / HarmonyOS

- macOS SwiftUI 只列 5 个 workload（`macos-gui/GPUBenchmark/Views/RunView.swift:40`），并大量使用固定 frame-mode，而不是默认 15 秒 time-mode。
- HarmonyOS 仍是原始 Vulkan 粒子 demo，没有 workload suite、15 秒结果模型或抓帧编排。

### 已知文档漂移

| 文件 | 主要漂移 |
|---|---|
| `README.md` | 已同步 12 个公开选择、Mangekyo GPU Burn v2 与 Plasma Bloom v1 legacy 边界 |
| `docs/cli-reference.md` | 已同步 12 个 CLI/GUI 选择及两代 GPU Burn selector/结果分组 |
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
- [x] `VisualBurn v2 / Mangekyo faceted glass`：独立跨 API fragment shader、透视 3D 截顶宝石/八面体碎钻、前后层视差、SDF 平面法线、Fresnel 反射、RGB 折射色散、2 次 opaque draw、固定 step FP32/SFU/INT、warmup 自动标定、版本化 `Gpix-step/s`；Plasma Bloom v1 以公开 legacy selector 与独立结果身份保留。
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

本轮已建立并实际执行 Windows x64/ARM64 GitHub Release 候选链：CMake
`install()`/CPack、固定 `vcpkg.json` baseline、CMakePresets、staging verifier、
WinUI self-contained payload、MSVC runtime、GLFW、全部预编译 shader、GLAD
2.0.8 in-tree、完整官方 RenderDoc 1.45 portable（x64 常规捆绑）、**WiX MSI**、
逐文件 SHA-256 与 ZIP 解包复核。目标机运行核心 benchmark/GUI/抓帧不需要 VS、
vcpkg、Vulkan SDK、shader compiler、单独的 Windows App SDK 或预装 RenderDoc。
Inno Setup 仍保留为 legacy 工程路径，**不再是** `build-windows-github-release.ps1`
的默认安装器。

PathService 已把 results/captures/reports/logs 改到
`%LOCALAPPDATA%/GpuComputeBenchmark`（可用 `GPU_BENCH_DATA_DIR` 覆盖），并一次性
迁移旧相对 `results/results.json`。GUI/CLI 与 RenderDoc 能从 staged 布局运行。

已生成可供换机验收的 ZIP 与 MSI，但仍不能宣称“已经公开发布/完全验收”，原因是：

- CLI/GUI 已把 `vulkan-1.dll` 改为 delay-import，probe 与显式 Vulkan backend 创建前都有 loader guard；本机构建和 PE 审计通过。仍须在真正没有 `vulkan-1.dll` 的干净机确认 DX11/DX12/WARP 启动。
- 报告链仍只有 `.py` 源码，没有冻结的 `report_worker.exe`；核心 benchmark/GUI 不需要 Python，但自动报告仍需要开发环境。打包规则虽预留 `tools/report_worker`，GUI 尚未实现调用该 worker 的 argv 协议。
- **项目分发 LICENSE 已订为 MIT**（仓库根 `LICENSE`，staged `licenses/LICENSE`，`projectDistributionLicense=true`）。CPack WiX 需要 `.txt`/`.rtf`，构建时会复制为 `cpack-LICENSE.txt`。
- 当前 MSI、GUI 与 CLI 均未做 Authenticode 签名；Release asset manifest 会记录 `NotSigned`，公开下载有 SmartScreen 风险。
- 尚未在真正干净 Windows VM 上验证 GUI 启动、无 RenderDoc/VC Redist/SDK 环境下的运行与升级/卸载。
- GUI 与 CLI 均已有 **x64 与 ARM64 原生**产物；勿再写成“GUI 只有 x64”。

### 7.2 推荐发布架构（当前 Windows x64 基线）

1. **路径产品化（第一阶段已做）**：统一 PathService；可写数据放 `%LOCALAPPDATA%/GpuComputeBenchmark/{results,captures,reports,logs}`；staged GUI/CLI 不依赖 repo CWD。
2. **进程隔离**：GUI 是唯一 orchestrator，每个 API×GPU 启动一个 worker；device loss、driver crash 和 RenderDoc 注入不拖垮 GUI。
3. **锁定构建（部分已做）**：已有 `vcpkg.json` baseline、CMakePresets、strict shader asset gate 与 in-tree GLAD；NuGet lock、完整 shader manifest/CI 仍待补。
4. **预编译资产（已做）**：发布 SPIR-V、HLSL/GLSL 与 DX12 FP16 DXIL；用户机器不装 SDK/compiler。
5. **报告 worker**：把 Python 报告链冻结为随包的 `report_worker`（嵌入 Python或独立 onedir），主程序只用绝对路径/argv 调用，不查 PATH、不拼 shell。
6. **安装器（2026-07-17 起主路径为 WiX MSI）**：`scripts/build-wix-installer.ps1` + CPack `ZIP;WIX`，`WixUI_InstallDir` 可选安装目录；产物名 `Mangekyo-<ver>-windows-{x64,arm64}.msi`。`build-windows-github-release.ps1` 已切到该脚本。Inno（`installer/GpuComputeBenchmark.iss`）保留作 legacy。本机已冒烟：`Mangekyo-0.1.3-windows-x64.msi`（Template `x64;1033`，SHA-256 `db82358f2dfd93542bac2659638d3fa91b45ec4c0d465ef978678c819dd5154e`）与 `Mangekyo-0.1.3-windows-arm64.msi`（Template `Arm64;1033`），均 NotSigned。
7. **RenderDoc 可选组件（构建机 staged 已验证）**：随包放完整官方 1.45 bundle 到 `{app}/tools/RenderDoc`，archive SHA-256 `bd665c348a8245d10a1f513e35b83603edc1a78006277583d09ec0769286eea4`；in-app API 在第 5 秒包住一帧，Vulkan 使用进程级 implicit-layer path。仍需 clean-machine 抓帧与 credits/签名审计。
8. **Vulkan loader（代码与 PE 已收口）**：SDK 只用于构建；Windows 运行时 delay-load 并探测 loader/ICD。缺 Vulkan 时隐藏该能力；若显式请求 Vulkan则返回可读错误，DX11/DX12/WARP 仍可启动。不随软件安装 GPU 驱动或复制系统 loader。

### 7.3 Fresh-machine 验收门槛

- [ ] Windows 10/11 干净 VM：无 VS、vcpkg、Python、Vulkan SDK、RenderDoc、VC Redist；离线安装后 GUI 能启动。
- [ ] Windows ARM64 干净实机/VM：安装前无开发环境或独立 RenderDoc；本体、GUI、全部原生依赖和可选抓帧组件通过 PE 架构清单核对，安装后完成 GPU/CPU smoke、结果保存、升级/卸载与数据保留。任何 x64/ARM64EC 组件均在 manifest 和 UI 中显式标明。
- [ ] Windows 7 SP1 干净实机/VM：独立 Win32 GUI 与安装包启动；分别验收 Aero 开/关、Basic/经典主题、DWM composition 关闭与远程桌面 fallback，不依赖 WinUI/WinAppSDK；GT 120 另做 DX11 FL10/SM4 实卡流程。
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
- **尚未在 GT 120 实卡运行。** 正式支持结论必须在目标驱动/系统上完成 Stream Light/Medium、fragment workload、4,096-body N-body、15 秒、可用时的第 5 秒抓帧、timestamp 与 TDR 验收。当前 WinUI/安装器不支持 Windows 7；用户已把独立 Windows 7 GUI + 安装包排在 ARM64 之后，计划共享现有 DX11 FL10/SM4 worker，并以 Aero/DWM 原生前端取代此前仅做 legacy CLI package 的设想。Windows 10 与 Windows 7 的驱动、抓帧工具和 TDR 结论必须分别记录，不得由一端外推另一端。

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

- [x] 2026-07-18：按用户对 Full Analysis / All GPUs 实图反馈修正 WinUI 结果编排与软件设备显示。WARP/Basic Render 在 GPU 下拉框和本次 Summary 中保留真实软件渲染器名称，并追加当前 CPU 型号（例如 `Microsoft WARP (AMD Ryzen …)`），不再退化显示为 `GPU 2`。Full Analysis 会在启动前跳过 probe 已知不支持的 GPU×API（含非 fragment workload 的 DX11 compute 不可用）并在 Summary hint/raw output 列表说明；这些组合计为 unsupported/skipped、使用绿色完成状态，只有实际启动后非零退出才计为 failed/红色。另为“所选核显但 Windows/WGL 实际分配到另一 GL_RENDERER”增加专用 Summary 说明和状态文案；分类严格要求 OpenGL worker + CLI 精确 `cannot select GPU index`/`active GL_RENDERER` 标记，shader/context/driver/timeout 等其他 OpenGL 错误不会套用此说明。Release WinUI x64 已编译通过；尚未运行新的 Full Analysis/核显 OpenGL 矩阵或做 GUI 视觉点击验收。
- [x] 2026-07-17：WinUI 四个秒数/时长 `NumberBox`（GPU duration、RenderDoc capture、CPU per-test、CPU warm-up）由 `Inline` 统一改为参考图对应的 `Compact` 竖向浮层按钮；补齐 Enter/页面外点击失焦、空值恢复与 `PopupThemeTransition`，避免 × 清空后 `NaN` 导致按钮全灰。数值合同未变，Release engine 与 WinUI x64 build 通过；按用户要求未启动最终 GUI，视觉手测仍待用户侧验收。
- [x] 2026-07-16/17：用户否定首个二维 plasma chamber 原型后，将公开 `gpu_burn` 重做为 **Mangekyo faceted glass v2**：Vulkan/HLSL/OpenGL 三份一致 shader 现使用透视 3D SDF 截顶宝石/八面体碎钻、前后深度层、平面法线、Fresnel、RGB 色散和吸收；两次全屏固定循环使用独立 `gpu_burn_v2_mangekyo_faceted_glass_v1` / shaderVersion=3 身份，被否定原型不混分。Plasma Bloom 通过 `gpu_burn_v1` selector、原 shader 与历史合同完整保留。Release core 编译通过；RTX 5090 Vulkan 自动标定 16→2048 后 render 13.199 ms / 286.00 Gpix-step/s / 90.4% timestamp utilisation，stableScore 288.21 / CV 1.50%；DX12/DX11/OpenGL v2 低步数 runtime smoke 均通过。视觉核验图为 `out/kaleidoscope-prototype/mangekyo_faceted_final.png`。
- [x] 2026-07-15：完成独立 `gpu_burn_v1` vertical slice。用户否定甜甜圈后改为原创 **Plasma Bloom / 等离子晶核**：实心七瓣晶体、无环形孔洞、非粒子；保留 `gpu_stress_v1` 成绩契约，默认 15 秒 / 第 5 秒抓帧，并同步 Vulkan/DX12/DX11/OpenGL、WARP、GUI、结果/图表与打包资产。
- [x] 2026-07-15：RTX 5090 Vulkan 自动标定 16 → 1604 steps/draw；正式 render 14.899 ms、`198.44 Gpix-step/s`，稳定段连续 8 次 NVML utilization 均为 99%，功耗约 599–600W（600W limit）。应用内 93% 是 GPU timestamp / 总帧墙钟比，包含 CPU/present 间隙，不等同于 NVML busy。
- [x] 2026-07-15：完成 README/TODO/roadmap/真实源码/结果文件/GUI/构建链审计。
- [x] 2026-07-15：建立本 handoff，并把其设为 AI 的优先交接入口。
- [x] 2026-07-15：交付 `gpu_stress_v1` GraphicsBurn vertical slice，旧 `stream`/`stress` 不删除、不改旧 shader；Vulkan/DX12/DX11/OpenGL 与 WARP 可运行，Metal 显式 unsupported。
- [x] 2026-07-15/16：WinUI 最终同步 11 项 workload 选择、Plasma Bloom 与 Cinematic Liquid 主测试标签、默认 15 秒与非 headless 第 5 秒抓帧；Full Analysis 会传递所选 workload，并拒绝 Vulkan-only liquid 的跨 API 伪运行。
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
- [x] 2026-07-15：完成历史 physical-scene v7 代码切片：320,920 粒子（142x14x98 + 48x37x71）、`cinematic_liquid_v2_physical_scene_v7` / `shaderVersion=9` / `sceneVersion=4`；后续共享 scene 参数变化已升为 v8，不能继续把 v7 写成当前合同。
- [x] 2026-07-15：v7 shader 编译、最终 6 个 SPIR-V、CLI Release、WinUI Release x64 build 通过（0 error，仅有既有 warning；最新增量构建 2 个，源文件重编时曾为 4 个）；仅做 6/8 秒自动停止 smoke，无正式 15 秒、无可靠保存的新 RenderDoc、无完整视觉验收。`241.13` transient console 值不得记录为结果；`--time 8` 窗口自动关闭不是已确认的崩溃。
- [x] 2026-07-16：当前 scene 合同升为 `cinematic_liquid_v2_physical_scene_v8` / `sceneVersion=5`（inset 0.45、wall-top fraction 0.42、extinction 12/3.6/2.5）；SPH 318,464 粒子完整运行 15 秒并获用户视觉验收。SPH 仍无正式 score，所有时长强制 `_preview`。
- [x] 2026-07-16：完成 Windows ARM64 原生本体、WinUI GUI、ZIP、Inno Setup 安装包、依赖与全链路构建审计。通过自动构建 ARM64 `vulkan-1.lib` 并处理 `vcruntime140_1.dll`，全量测试及 CPack 打包全部通过，产物生成于 `out/release/windows-arm64/`。
- [x] 2026-07-17：用户订 **MIT** 根 `LICENSE`；CMake/CPack 默认 `ZIP;WIX`；发布主路径改为 WiX MSI（x64+ARM64 原生 Template）；Inno 降为 legacy。本机已产出 `Mangekyo-0.1.3-windows-{x64,arm64}.msi`（stage `projectDistributionLicense=true`）。
- [x] 2026-07-17：互动水池（`cinematic_liquid`）跨 API 需求曾短暂接 DX12，后按用户要求 **撤回**，恢复 Vulkan-only；勿把未接线的 HLSL 草稿写成已支持。
- [ ] **当前下一步（用户 2026-07-16 指定，仍有效）**：完成独立 Windows 7 GUI 与安装包，复用现有引擎/worker/成绩 schema，并在能力存在时尽可能使用 Aero/DWM；Aero 不可用时必须可用且可读地降级。
- [ ] 旧 `fluid` Vulkan 正确性与统一跨后端 workload registry 仍开放，但不再阻塞独立 Cinematic Liquid v1。
- [x] 实际 MSI/ZIP 与动态 Vulkan loader 已完成构建/静态审计；显式 Vulkan 缺 loader 的异常路径也已加 guard。
- [ ] 冻结 report worker、Authenticode signing、GT120 实卡与 clean-machine 安装/升级/卸载/抓帧验收仍开放（**项目 LICENSE 阻塞已解除**）。
- [ ] **日语本地化（用户待办）**：WinUI 目前只有 Auto / English / 简体中文（`gui/i18n.h` 的 `tr(en, zh)` + `LangBox`）；需扩展 **日本語**（至少 GUI 全表文案、语言下拉、OS UI 自动探测 `LANG_JAPANESE`/`ja`）。安装器侧：WiX MSI 向导目前默认英文；若产品要日语安装体验，需另加 WiX 日语 UI/本地化字符串（Inno legacy 目前也只有 EN + 简体中文 `.isl`）。不改变成绩合同或内部 id，只做显示语言。

### 推荐下一个实现切片

用户在 2026-07-16 最新调整了近期顺序：**先 Windows ARM64 完整发布闭环，再 Windows 7 Aero GUI**。ARM64 + WiX 主路径已落地。下一刀严格按以下顺序执行：

本次秒数控件没有剩余代码修改；下次启动 WinUI 做 GUI 实机验收时，补验四个 Compact 浮层的聚焦、上下调节、失焦、切页和窗口停用关闭行为。项目主线下一步仍是下列 Windows 7 GUI vertical slice，不因该显示样式调整改变优先级。

1. **Windows ARM64 vertical slice（已完成）**：含原生测项、GUI、ZIP/MSI。
2. **Windows 7 GUI vertical slice（当前下一步）**：保留 `gpu_engine` 与 CLI worker 为唯一跑分实现，另建不依赖 WinUI/WinAppSDK 的 Win32/DWM 前端；实现 Aero glass/主题化非客户区/Direct2D/DirectWrite 等能力检测 and 无 Aero fallback。建立 Win7 专用 toolchain/runtime/dependency/installer gate，并优先用 GT 120 验证 DX11 FL10/SM4、窗口响应、15 秒流程、timestamp/TDR 与可用的抓帧路径。
3. **回到未关闭的正确性与自由模式**：依次关闭 Cinematic Liquid SPH 的 frame-driven timestep、per-substep impulse clear、viscosity race、atomic scatter ordering；之后再做 Liquid Lab / Explore、GPU Burn Unlimited Soak 和 VRAM Integrity Soak。四项关闭前 SPH 始终 `_preview`，旧结果合同不变。
4. **Windows 完整公开发布收口**仍开放：冻结 `report_worker`，签名证书，在干净 Windows 10/11 VM 验收 bundled RenderDoc 和完整 GUI-first MSI 安装/升级/卸载。
5. 之后按第 2 节新顺序做 macOS、Android、iOS、Debian、WebGPU、HarmonyOS、PS3（探索）与 Dual-GPU Aggregate；后期 RT/路径追踪/厂商超分不得抢占当前 Windows 7 刀。

## 10. 验证记录

### 2026-07-18 WinUI WARP 命名与 Full Analysis 跳过状态

- `gui/MainWindow.xaml.cpp/.h`：probe 设备名新增与 GPU index 对齐的快照；软件设备显示统一为真实 probe 名称 + 当前 CPU 型号，worker 回传 `Microsoft Basic Render Driver`/`Microsoft WARP (CPU Software Renderer)` 时不会再在 Summary fallback 为 `GPU <index>`。
- Full Analysis one/all GPU 的任务计划会过滤 probe 明确不支持的 API；非 fragment workload 的 DX11 还检查 `dx11Compute`。跳过明细进入 Summary hint 与 Raw CLI output；最终状态分别报告 completed / failed / unsupported skipped。Custom、Flights、Particle 等路径仍保留显式启动并由 CLI 返回真实能力错误的既有诊断语义。
- `git diff --check`：通过。
- VS 18/v145 `MSBuild gui\gpu_bench_gui.vcxproj /p:Configuration=Release /p:Platform=x64 /p:GpuBuildDir=... /m:1`：成功，生成 `gui/x64/Release/gpu_bench_gui.exe`；仅有既有 MSB3774/MSB8027/C4996/LNK4042 warning。首次短超时留下的 MSBuild 占用同一 PDB，结束该遗留进程后在沙箱外单线程重跑成功；这不是源码编译失败。
- 本轮没有启动 GUI、Full Analysis、GPU workload 或 RenderDoc；WARP 文案视觉和实际 `9 completed; 3 unsupported combinations skipped` 矩阵仍待交互验收。

### 2026-07-18 Windows OpenGL 指定 GPU 错误说明

- `gui/MainWindow.xaml.cpp/.h`：worker 失败后在 backend 为 `opengl` 时识别两条 CLI 精确路径：(a) `OpenGL on Windows cannot select GPU index` + `active GL_RENDERER`；(b) 更早的 capability gate `does not report support for backend 'opengl'`。路径 (b) 还必须从 probe 快照确认**另一张** GPU 的 OpenGL capability 为 true，才记录“所选 GPU -> 当前 OpenGL renderer”；若没有这项交叉证据，generic unsupported 仍不会被误写成 Windows 设备路由限制。Summary hint 解释 WGL 没有标准的按 DXGI/Vulkan index 切换设备能力，并明确该说明不适用于其他 OpenGL 错误。
- 若本次所有真实失败均为上述 renderer mismatch，进度卡/底部状态直接写 `OpenGL could not use the selected GPU`；若还有其他失败则继续使用普通 failed/error 统计，同时仅为已识别 mismatch 附加说明。失败计数本身不改成 skipped，因为用户明确请求了该 GPU/API pass 而它没有运行成功。
- `git diff --check`：通过。
- VS 18/v145 Release x64 WinUI 单线程构建成功，更新 `gui/x64/Release/gpu_bench_gui.exe`；仅有既有 MSB3774/MSB8027/C4996/LNK4042 warning。
- 本轮未再次运行 15 秒核显 All APIs；实际中英文提示、所选/实际 renderer 名称和不会误分类其他 OpenGL 错误仍待 GUI 交互验收。
- 用户随后提供的 Raw CLI 实图确认实际失败发生在较早的路径 (b)：`GPU index 1 (AMD Radeon(TM) Graphics) does not report support for backend 'opengl'.`，因此首版只识别路径 (a) 时 Summary 没显示说明。补齐路径 (b) 后再次完成 VS 18/v145 Release x64 WinUI 构建，`gui/x64/Release/gpu_bench_gui.exe` 已更新；仍未代替用户重跑完整 15 秒矩阵。
- 用户实图验收路径 (b) 的说明已能显示，但指出它与 L2 small-working-set 提示贴得太近，且 WARP All APIs 的另一个 Vulkan 失败仍只有计数。随后把 ResultHint 改为段落式渲染（各问题/性能提示之间使用空行），并新增严格的常见 worker 错误分类：unsupported GPU/API、API/adapter 初始化失败、缺 Vulkan Runtime、OpenGL 4.3 不足、workload/feature 不支持、安全超时、device lost/driver reset、shader/link/pipeline 失败、swapchain out-of-date、GPU 资源分配失败；未知错误只提示展开 Raw CLI，不猜测原因。OpenGL routing mismatch 仍使用专门说明且不重复生成 generic unsupported。
- 后处理失败也会在 Summary 独立说明“成绩可能有效，但 RenderDoc 转换/图表/报告失败”。分类只基于 exit code 与明确的 CLI/backend 错误标记；不会把所有 OpenGL 或所有 non-zero exit 归成同一原因。VS 18/v145 Release x64 WinUI 再次构建成功，`git diff --check` 通过；仍未运行新的 15 秒矩阵做视觉验收。

### 2026-07-17 WinUI 秒数输入 Compact 样式

- `gui/MainWindow.xaml` 中四个相关 `NumberBox` 的 `SpinButtonPlacementMode` 均从 `Inline` 改为 `Compact`，并更新旧的 Inline/浮层规避注释；`git diff --check` 通过。
- 四控件统一使用 `InvalidInputOverwritten`、Loaded/GotFocus 动画配置与 Enter 处理；GPU/CPU 滚动内容接入外部点击失焦。空值由 `NumberBoxValueChangedEventArgs::OldValue` 同步恢复，无合法旧值时才回退 Minimum；不再保留关闭 XamlRoot 全部 Popup 的实验逻辑。
- `cmake --build build --config Release`：成功，`gpu_engine.lib` 与 `gpu_benchmark.exe` 均生成。
- VS 18/v145 WinUI Release x64 独立输出构建成功：`out/numberbox-ui-verify/gpu_bench_gui.exe` 与新 `MainWindow.xbf` 生成，0 error、7 个 MSB8004/MSB3774/MSB8027/C4996/LNK4042 warning（均为现有工具链/工程 warning）。
- 按用户要求确认目标 EXE 无占用后，以工程默认 `OutDir` 重建成功：`gui/x64/Release/gpu_bench_gui.exe` 与 `MainWindow.xbf` 已更新，0 error、4 个 MSB3774/MSB8027/LNK4042 warning；构建后未启动 GUI。
- 用户明确要求最终版本只改代码并重新编译，因此未启动 GUI、未做自动点击，也未重新 stage/打包；结论仅为代码与构建通过，交互视觉和发布包仍未验收。

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
- v6 当时的表面 resolve 为 5x5x5 binomial filter、`mix=0.90`；fragment shader 最多处理 4 次介质界面，逐段执行 Fresnel/Snell、Beer–Lambert 和 opaque scene depth sorting。v6 metadata 为 `extinction=(30,10,8)`、`toneMap=linear_exposure`、`densityBoundary=zero_clamped`。
- 最终必要短预览：RTX 5090 / Vulkan / `--time 6 --capture 5`，结果 id `20260715-221447-024`，Compute 9.450 ms、Render 3.003 ms、Total 12.454 ms、`257.01 MParticle-step/s`、284 measured frames；capture 0.117 秒排除，1 attempt/1 saved。核验图：`rdoc_captures/cinematic-liquid-v2-5s-iterative-optics-v6-final-preview.png`。
- 这是 `_preview` 结果，**没有运行 v6 的正式 15 秒流程**，不可与 optics_v4 的正式 `263.98 MParticle-step/s` 比分。水体厚区、薄片与边界观感较旧版改善，但仍未达到 jeantimex SPH 的自然动态。
- CLI Release 与 WinUI Release x64 重建通过；WinUI 仍只有 build 结论，实际 workload 选择、完整 run 与 History/Charts 分组待验收。

### 2026-07-15 Physical scene v7 验证（晚间）

- v7 当时的合同为 `cinematic_liquid_v2_physical_scene_v7`、`shaderVersion=9`、`sceneVersion=4`；128x64x96、10 substeps、320,920 粒子（142x14x98 base + 48x37x71 dam）。用户鸭子与七刚体 index ABI 均保留，尤其 boat=2、sink sphere=3、ducklings=4-6。
- shader 编译通过，最终 6 个 SPIR-V 均通过 Vulkan 1.1 校验；CLI Release 构建通过。WinUI Release x64 build 也通过：0 error，仅有既有 warning（最新增量构建 2 个 duplicate WinAppSDK warning，源文件重编时曾包含 C4996、合计 4 个）。最终物理/渲染静态复核均未发现必须修复项；这仍不替代实机视觉验收。
- 只运行了若干 `--time 6` / `--time 8` 自动停止 smoke，没有正式 15 秒流程。没有可靠保存的新 RenderDoc/核验帧，也没有完成船、沉球、水花、越沿、PVC、草地/天空的完整视觉验收；目录中若存在 v7 临时 PNG，也不得在缺少可靠 capture 记录时当作验收证据。
- 一次控制台曾显示 transient `241.13`，但它不是正式 15 秒值，也没有作为可靠持久结果保存，禁止写入排行榜或与 optics_v4/v6 比较。
- 用户反馈窗口约 2-3 秒后自行关闭；本轮使用的自动 smoke 命令包含 `--time 8`，因此自动退出是脚本生命周期，不是目前确认的 crash。正式 Benchmark 应按 15 秒流程验收；未来 Liquid Lab/Explore 才应无限运行至用户主动 Stop。
- 物理/场景代码事实：船为 34 kg 有限质量 + 软系泊，推进反冲可驱动/摇摆；沉球 1.06 水密度比、4.28 秒释放、-9.81 重力、0.015 空气阻尼，水阻按 displaced mass/浸没率门控；有限池壁 inset 0.22 + 有限 outer catch band；自适应 surface 保留低支持喷滴，entry crown/whitewater 来自 GPU body/local fluid，但无 secondary-spray 粒子；软 PVC 薄膜 IOR 1.50/Fresnel/弱吸收/wrinkle；无限程序化草地和无 cubemap 的大气天空/云。以上均是实现事实，不等于轨迹或最终画质已验收。
- render push 的 `pool.z` 已改为真实 ground/pool-floor `y=0`，PVC bottom、liner、草地与物理粒子底面统一；ring separation 由 `2*tube` 推导。该对齐通过源码/构建验证，仍需可靠新 capture 做最终视觉验收。

### 2026-07-16 SPH vertical slice（`cinematic_liquid_sph_slice_v1_preview`，实验性）

按既定判断（"不再盲调 MLS-MPM，建立独立 SPH 版本"）完成了第一个可运行的 SPH 纵切片。**这是实验性 preview 合同：15 秒视觉已验收，但正式计分未完成，不得与任何 MLS-MPM 版本混分。代码现在对所有时长强制 `_preview`。**

实现事实：
- **求解器**：忠实移植 MIT [jeantimex/fluid](https://github.com/jeantimex/fluid) `src/sph/3d/common` 的 Lague 式双密度 SPH——Spiky²/Spiky³ 双密度 + 对称压力/近压（EOS `P=k(ρ-ρ0)`）、Poly6 XSPH 粘性、预测位置（固定 1/120 前瞻）、Unity block-hash(50) 空间哈希 + 计数排序（三级扫描 + atomic scatter）。参数原封照搬：h=0.2、targetDensity=630、pressureMultiplier=288、nearPressure=2.16、viscosity=0.01、collisionDamping=0.95、g=-10，全部在参考单位制（~24x12x18 域）内运行。
- **关键教训（已修复的爆炸）**：参考的 `frameTime=min(dt*timeScale, 1/maxTimestepFPS)` 在 60fps 下把 timeScale=2 钳掉，**每 substep 实际 dt=1/120**；首次用 1/60 直接压力爆炸（全域水雾，见 `rdoc_captures/cinematic-liquid-sph-5s-first.png` 留档）。现固定 dtSim=1/120、2 substeps/帧（固定每帧推进，不随墙钟）。
- **世界映射**：长度 ×(5.12/24=0.21333)，时间 ×τ=sqrt(0.21333*10/9.81)≈0.4663，使 sim 重力 -10 精确落在世界 -9.81，流体与刚体自由落体一致。积分 pass 把世界坐标/速度写回共享 80B 粒子 buffer——surface splat、raymarch、相机、场景、成绩框架**全部原样复用 v7**。
- **刚体耦合**：per-particle SDF 推离 + 速度碰撞 + displaced-volume 浮力（镜像 grid_update 的动量交换），fixed-point 冲量进现有 `bodyImpulses`，rigid integrate pass 原样消费（含 linear.w 浸没门控）。第 10 秒实图验证：鸭妈妈+三小鸭+彩球+船漂浮端正，沉球半沉。
- **播种**：sim 单位确定性晶格 175x8x124 底床 + 30x46x96 左坝柱 = **306,080 粒子**（密度 600/unit³，同参考 spawnDensity），全部落在内嵌池壁内；4 秒重置与 v2 相同（含 SPH 位置/速度 buffer 的 seed 恢复）。
- **内嵌有限池壁（用户指正后补上，随后按用户要求加宽外圈）**：首版直接用整域 AABB 当边界，静水铺得比可见泳池宽；现 SPH integrate 实现内嵌圆角矩形有限池壁（壁顶 sim 4.017 = 世界 0.737，越顶水花自由落草地，外圈域 AABB 仅作 catch band）。2026-07-16 用户要求加宽外圈草地带：**渲染圈/PVC 膜/物理墙 inset 统一从 0.22 → 0.45**（frag `poolDistances` 与 `poolMembraneDimensions` 的系数 2.59→5.29·tube；MPM 路径 `pc.pool[1]`、SPH 墙常量同步）。**注意：这同时改变了 v7 MPM 场景的池几何，v7 此前的 smoke 观感基线随之漂移，属用户批准的场景变更。** 调试过程记录：(a) 坝柱高于壁顶太多导致整体涌浪漫顶、catch band 积成"护城河"→ 柱降至 30 层；(b) splat 核+5x5x5 重建使水面比粒子外扩 ~0.1m，物理墙需再内收 0.10m（half sim 9.328/6.328）才能让重建水面贴圈。最终播种 148x9x98 + 30x30x96 = **216,936 粒子**。最终验证图 `rdoc_captures/cinematic-liquid-sph-10s-wideband-v4.png`（宽外圈草地干净、水贴圈、玩具姿态正常）；中间态 `...-v2/-v3.png` 留档对照。刚体无内墙约束（沿用 v7 语义），个别玩具可被浪冲出池沿属真实行为。
- **深池/浅蓝水/沉底可见（2026-07-16 用户四项调整）**：(a) 草地吸收倒计时上限加长（0.25~0.65 → 0.50~1.80 sim 秒，水洼多留一会儿）；(b) 池壁加高：壁顶系数 0.34 → 0.42（世界 0.737 → 0.938），渲染圈/PVC 膜/g2p/grid_update/SPH 五处同步；(c) 水量 216,936 → **318,464 粒子**（床 148x16x98，静水深 ~0.49 m），0.40 m 沉球完全没顶；(d) `liquidTransmittance` 消光 (30,10,8) → (12,3.6,2.5)，保持红先吸收的水色光谱但整体透亮为浅蓝，水下沉球轮廓可读。**(b)(d) 同样改变共享 MPM 场景；连同 inset 0.45，这正是当前合同必须从历史 v7 升为 v8 的原因。**验证图 `rdoc_captures/cinematic-liquid-sph-10s-deep.png`（更高池壁、侧壁浅蓝、沉球在水下可见）。
- **草地吸收 / 空气墙消除（2026-07-16，用户指出加宽外圈的根本动机是大量溅水时能看到外框空气墙）**：SPH integrate 增加逃逸水回收机制——(a) 域外框 x/z 面从反弹改为"粘滞空气"（只清外向法向速度，不再出现半空撞隐形墙的反弹）；(b) 落在外圈草地的水按每粒子错开的倒计时（当前 **0.50~1.80 sim 秒**，存于 `position.w`，1.0=自由态）短暂积水后"渗入草地"；(c) 压在域边缘的水快速消退（≤0.10s）；(d) 渗完的粒子确定性回收到池内水线以下（hash 定位、零速度），粒子总数不变。验证：`rdoc_captures/cinematic-liquid-sph-5s-absorb.png`（溃坝晃荡期池外水舌边缘为自然圆弧、无垂直水墙）、`...sph-10s-absorb.png`（10 秒草地完全干净、无积水残留）。metadata 为 `grassAbsorb=catch_band_soak_recycle;grassSoakCountdownSimSec=0.50..1.80`。
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

### 2026-07-17 光效可见性根因修复 + 0.1.3 双架构（午间轮）

- **"调了亮度却看不出变化"的根因是 Reinhard 饱和**：`mapped = hdr/(1+hdr)` 在晶核的 HDR 5-10 区间输出被压死在 0.85-0.90，上游增益再乘系数视觉不可辨。修复：色调映射膝点 1.0 → **0.62**（全场提亮，晶核逼近白热，肉眼差异显著），并把光晕衰减带从 smoothstep(0.025,0.45) 加宽到 (0.05,1.05)（光雾半径约翻倍，"光"可见地向背景扩散）。三份 shader 同步；固定循环工作量不变（discard 在计分循环之后）。教训：调亮度先看色调映射曲线所在段。
- History 的 GPU/CPU 浏览器式标签页（TabView）已由并行会话实现在源码中，用户所装旧包未包含——随本轮包发布，无需另改。
- **0.1.3 双架构**：CMake 0.1.2→0.1.3；x64（含 RenderDoc）与 ARM64（-SkipRenderDoc，N/A 语义不变）先后重打，ARM64 首次带上 SPH/池子/GUI 改版/固定负载 GPU Burn/光效全部内容。

### 2026-07-17 晚间：MIT LICENSE + WiX 主路径（本轮交接）

- **License**：用户确认订根目录 `LICENSE` 为 **MIT**（Copyright 2026 Mangekyo contributors）。`GPU_BENCH_PACKAGE_LICENSE_FILE` 默认指向该文件；stage 写入 `licenses/LICENSE`，manifest `projectDistributionLicense=true`。`THIRD_PARTY_NOTICES.md` / `PACKAGE_LIMITATIONS.md` / packaging 文档已改为“项目 MIT + 第三方 notices”，不再写“无项目许可证阻塞 MSI”。
- **WiX 替代 Inno 为主路径**：
  - `cmake/Packaging.cmake`：默认 `GPU_BENCH_CPACK_GENERATORS=ZIP;WIX`；`CPACK_WIX_UI_REF=WixUI_InstallDir`；`CPACK_WIX_ARCHITECTURE` x64/arm64；无扩展名 `LICENSE` 自动 `COPYONLY` 为 `cpack-LICENSE.txt`（否则 CPack 报 `unsupported WiX License file extension ''`）。
  - 新脚本 `scripts/build-wix-installer.ps1`；`stage-windows-release.ps1` 强制带 LICENSE；`build-windows-github-release.ps1` 调用 WiX 并收集 `*.msi`（不再要求 `*-setup.exe`）。
  - Inno（`installer/GpuComputeBenchmark.iss` + 简体中文 `.isl`）保留为 **legacy**。
- **本机冒烟产物**（`out/installer/`，NotSigned）：
  - `Mangekyo-0.1.3-windows-x64.msi` ~103.9 MiB，SHA-256 `db82358f2dfd93542bac2659638d3fa91b45ec4c0d465ef978678c819dd5154e`，Summary Template=`x64;1033`
  - `Mangekyo-0.1.3-windows-arm64.msi` ~102.5 MiB，Template=`Arm64;1033`
  - 对应 stage 的 `gpu_benchmark.exe` / `gpu_bench_gui.exe` PE 分别为 AMD64 / ARM64；`compiledBackends` 两边均为 vulkan/dx12/dx11/opengl=true。
- **测项架构事实**：CPU 与 GPU 测试本体都是按包架构原生编译的两套二进制，不是同一套 x64 测项 + 两个安装壳。
- **互动水池**：仍 Vulkan-only；曾试 DX12 后撤回。未接线 HLSL 草稿若提交须标明为移植草稿，不可宣称 DX12 liquid 可用。
- **仍开放**：Authenticode、frozen report worker、干净机 MSI 安装/升级/卸载/抓帧、Windows 7 GUI 下一刀、**日语（日本語）GUI/安装器本地化**。

### 2026-07-17 GPU Burn v2 固定负载合同（用户拍板）+ 0.1.2

- **用户决策：废弃"目标帧时/自动标定"哲学**（"我不需要目标步数，我就要显卡尽可能生成高帧数"）。GPU Burn 改为 **FurMark 式固定负载**：所有硬件 GPU 每帧执行完全相同的 256 步 x 2 draw（`kGpuBurnV2FixedIter=256`），无标定、无渐升，FPS 本身成为直观对比信号。实测 RTX 5090 Vulkan 恒定 ~345 FPS（2.34 ms/帧），全程无帧数跳变——之前"开头帧数高后面骤降"的现象连根消除（那是标定探针→目标的设计残留）。
- **新成绩身份**：`workloadVersion=gpu_burn_v2_fixed256_kaleidoscope`、`shaderVersion=3`、metadata `loadModel=fixed_per_frame;autoTune=false`。此前标定制的 `gpu_burn_v1_plasma_kaleidoscope_r2` 及更早 `gpu_burn_v1` 成绩组全部独立保留退役。闭环标定代码保留休眠（`gpuBurnAutoTune` 默认 false，无 CLI 开关）。
- **安全边界保留**：软件设备（WARP/Basic Render/llvmpipe）双层钳制到 32 步——CLI 归一化按 `--warp` 钳一次，`AppBase::MainLoop` 起始按真实设备名再兜底一次（覆盖 `--run-all`/GUI 矩阵在逐设备处才知道 WARP 的情形；WARP 16 步已需 ~209 ms/帧，256 步会触发 watchdog）。显式 `--iter` 上限仍 2048。
- **晶核光效**（用户两轮加亮要求）：主增益 0.20→0.26→0.34→**0.42**，光晕合成权重 crystal 0.62→0.88 / base 0.36→0.50，配色紫罗兰系契合背景；三份 shader（frag/hlsl/gl）同步。
- **版本 0.1.2**：CMake project VERSION 0.1.1→0.1.2，x64 包重打（含固定负载合同、光效、此前全部 GUI/安装器/RenderDoc 改动）。
- 待办：15 秒正式流程在新 fixed256 合同下建立基线（每 API 的 FPS/Gpix-step/s 将天然不同——固定负载下这是真实差异而非标定伪差）；README/cli-reference 的 auto-tune 文案清理；GUI 中 gpu_burn 的 `--iter` 传参路径核对。

### 2026-07-17 GUI 改版 / GPU Burn v2 同心圆 / x64 重打（凌晨轮）

- **VM 反馈修复（VMware Win11 全 GPU 完整分析后 RenderDoc 弹错）**：崩溃本体是 RenderDoc 在虚拟 GPU（SVGA3D/WARP）上的上游限制，物理机验收为准；其 Bug Reporter 二次报错根因是官方包 OpenSSL 命名 `libcrypto/libssl-1_1-x64.dll` 与报告器探测的 `-1_1-64.dll` 不匹配——`prepare-renderdoc-portable.ps1` 现自动生成字节级同文件别名（记入 BUNDLE_SOURCE.json 的 localModifications），现有 dependencies/staged 目录已补。
- **GUI 改版（用户逐项要求）**：测试下拉默认只显示 粒子/等离子晶核（词序对调为"等离子晶核 —— GPU Burn"）/流体（去掉"电影化"与 v2 后缀，"流体 —— 互动水池"），其余 8 项藏于新增"显示旧版 / 高级测试"复选框（`ShowLegacyBox` + `applyWorkloadVisibility()`，隐藏时选中项自动回退到粒子）；全部"跑分"文案改"测试"（11 处）；GPU 导航图标由三角形改为自绘显卡 PathIcon；高级选项复选框间距收紧（VsyncBox MinWidth=0、Spacing 20→14）。History 分组标签同步改名（仅显示文案，id/版本不动）。
- **安装目录**：与并行会话的 `{pf}\Mangekyo + PrivilegesRequired=admin` 方案会合（本会话曾先改为 {autopf}+dialog，后被并行会话覆盖并升级为 {pf}+admin+SignedUninstaller，遵其现状；build-inno-installer 静态 invariant 已由并行会话同步）。第一次重打失败即因 invariant 与 .iss 撞改动窗口，重跑通过。
- **GPU Burn v2 背景不是同心圆（用户实图指出）**：`kaleidoscopeBackground` 金环 `sin(radius*3.85)` 的环距 ~1.6 大于可见半径 → 全屏只显一圈；细环权重 0.055 不可见；两者还叠加角向扰动使圆波浪化。修复：环族改纯径向相位、金环频率 3.85→7.6、细环 10.8→15.4/权重 0.14，三份 shader（frag/hlsl/gl.frag）同步。实抓帧验证 5-6 圈完美同心圆 + 中央晶核 + 保留的品红/青波浪臂。**注意：GPU Burn v2 视觉迭代（含本次）未重新标定成绩基线，定稿时须升 shaderVersion 并重跑正式 15 秒。**
- **x64 包**：`Mangekyo-0.1.1-windows-x64` setup（87.7 MiB，SHA256 94a3bbf8…60791874）与 zip（120.0 MiB）已重打，含以上全部 + SPH/池子几何 + Program Files 安装；staged gpu_burn.frag.spv (01:41:55) 晚于同心圆修复 (01:39:27) 已核对。一次编码事故已完整恢复：PowerShell 批量替换曾损坏 MainWindow.xaml.cpp 中文编码，经与 HEAD 零结构差异验证后 git 恢复并用 Python UTF-8 重做；后续中文文本处理一律禁用 PowerShell 管道。

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
- 没有创建 tag/GitHub Release，也没有在第二台/干净 VM 验证；仓库无根 LICENSE，因此当前资产是**上传候选/换机验收包**，不是已公开发发布版本。

### 2026-07-16 Windows ARM64 平台全链路构建验证（本轮）

- **环境限制解决**：针对构建机没有 ARM64 Vulkan SDK 问题，首创编写了 `scripts/gen-vulkan-arm64-lib.ps1`，动态分析 x64 版本的 `vulkan-1.lib` 并通过 `lib.exe` 交叉生成原生 ARM64 的 `vulkan-1.lib` 存入 `$BuildDir`，成功解决引擎与 GUI 链接阶段的未解析符号与架构冲突。
- **构建环境与编译器**：升级 `stage-windows-release.ps1`，添加 `-Arch ARM64` 指令，调用 MSBuild v145 平台工具集对 WinUI GUI 与 `gpu_engine` 进行交叉编译，vcpkg 依赖（GLFW3 等）在 manifest 模式下顺利下载并还原原生 ARM64 版本。
- **MSVC 运行时架构审计豁免**：针对 Microsoft ARM64 Redist 目录中特有的 x64 `vcruntime140_1.dll`，在 `scripts/verify-windows-stage.ps1` 中加入专门豁免，使 PE 审计流程顺利在 ARM64 上取得 0 error 通过。
- **安装器与打包流程验证**：
  - 更新 `cmake/Packaging.cmake` 使 CPack 对 Windows ARM64 目标动态输出 `windows-arm64` 包名。
  - 更新 `installer/GpuComputeBenchmark.iss` 与 `scripts/build-inno-installer.ps1`，使 Inno Setup 自动映射 ARM64 兼容指令集，编译生成 ARM64 的专用 setup。
  - 成功一键跑通 `build-windows-github-release.ps1`，并于 `out/release/windows-arm64` 输出所有发布产物：
    * `Mangekyo-0.1.0-windows-arm64.zip` (24.4 MiB, SHA256: 44677d52e30664291906d633651498cbe77c32c9e8d5678fdb38556831b0b7d1)
    * `Mangekyo-0.1.0-windows-arm64-setup.exe` (16.0 MiB, SHA256: ab8a721bcfdc814040d71aa8f9d5898355e7abfff395f0a2af9b9d3150a20874)
    * `release-assets.json` 与 `SHA256SUMS.txt` 均生成通过。

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
- [x] 当前有限高度 pool wall 以 inset 0.45 内嵌，wall-top fraction 为 0.42；有限宽 outer catch band 允许越沿粒子真实落地，但不是无限流体域。`pool.z`、PVC bottom、liner、程序化草地与物理 floor 都对齐 `y=0`，ring separation 由 `2*tube` 推导。
- [x] 当前表面重建为独立 Spiky² fixed-u32 particle splat → 128x64x96 R32F volume，再做自适应 5x5x5 binomial resolve，保留低支持喷滴；renderer 延续最多 4 次界面的 Fresnel/Snell、分段 Beer–Lambert 和 opaque depth sorting。沉球 entry crown/whitewater 读取真实 GPU body/local fluid，不是 fragment 假水花；尚无 secondary-spray 粒子。
- [x] 前景透明侧壁是独立软 PVC 薄膜近似（IOR 1.50、Fresnel、弱吸收、wrinkle），不是完整 PVC 多介质 ray path；环境是无限程序化草地与大气天空/云，不使用 cubemap 资产。
- [x] 当前 MLS-MPM 结果身份为 `cinematic_liquid_v2_physical_scene_v8`，`shaderVersion=9`、`sceneVersion=5`；共享光学使用 `extinction=(12,3.6,2.5)`。用户调整后的 smooth-min 鸭妈妈 + 3 只小鸭及全部 7 刚体均保留。历史 v1、optics_v4、duck-family-v5、iterative-optics-v6、physical-scene-v7 与当前 v8 各自独立成组，不得混分。
- [x] 历史 optics_v4 的 RTX 5090 Vulkan 正式 15 秒 + 5.1 秒 RenderDoc 已通过：`20260715-170629-492`，Compute 10.572 ms、Render 1.553 ms、Total 12.125 ms、`263.98 MParticle-step/s`、966 measured frames；capture 0.103 秒排除，1 attempt/1 saved。最终图为 `rdoc_captures/cinematic-liquid-v2-5s-formal-optics-v4.png`；该成绩不得归给当前 v8。
- [x] iterative optics v6 的历史必要短预览已通过：`20260715-221447-024`，图为 `rdoc_captures/cinematic-liquid-v2-5s-iterative-optics-v6-final-preview.png`；这是 v6 `_preview`，没有正式 15 秒成绩且不得归到 v7/v8。
- [x] physical-scene v7 的 shader、6 SPIR-V、CLI Release 与 WinUI Release x64 build 通过；只有 6/8 秒自动停止 smoke，没有正式 15 秒、可靠新 RenderDoc 或完整视觉验收。transient `241.13` 不记录为结果；`--time 8` 自动关闭不是已确认 crash。
- [ ] **流体路线恢复后的最高优先级验收（当前排在 ARM64、Windows 7 之后）**：SPH 视觉已通过，停止优先调外观；先关闭 frame-driven timestep、per-substep impulse clear、viscosity race、atomic scatter ordering 四项，之后才允许非 `_preview` 正式 15 秒 + 第 5 秒抓帧。并实机验证 WinUI selection/run/history、timestamp pass 边界与异常路径资源清理。
- [x] **SPH 重大视觉路线已建立并收口**：318,464 粒子 SPH vertical slice 已完整显示 15 秒并获用户视觉接受；secondary spray/foam 与 propeller wake 是未来增强，不阻塞视觉收口，也不能掩盖正式计分 blocker。
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
- [ ] **GPU Burn — Unlimited Soak**：最终持续烤机应使用当前主 `gpu_burn` Mangekyo Kaleidoscope v2，而不是 legacy `gpu_burn_v1` 或 Other/Legacy 中的旧 `gpu_stress_v1`；默认持续到 Stop/Esc/Ctrl+C，GUI 满载时仍须可响应，显示已运行时间、滚动 GPU time/FPS、利用率、功耗/温度（可得时）和降频趋势。固定 `15s + 第 5 秒 RenderDoc` 仍只用于 Burst score，Soak 不生成正式成绩或硬件错误认证。
- [ ] **VRAM Integrity Soak**：新增独立 `vram_memtest`，优先读取 memory budget，保留桌面/系统安全余量并区分独显和 UMA；分块写入 address/random/walking-bit 等 pattern、设备端校验并累计错误，报告已验证字节、循环数、带宽和错误块。OOM、device lost 与用户停止必须干净退出；`stream` 永远只表示带宽，不表示显存无错误。
- [ ] 开发验收：Liquid Lab 退出后正式 v2 的 scene hash/初始画面恢复；GPU Burn 连续 30 分钟无资源增长且 Stop 可用（发布前建议 2 小时）；VRAM Integrity 连续 30 分钟基线零误报并可从 OOM/device-lost 报告原因。

### 2026-07-18：软件渲染器序号与 OpenGL Run issues 归类

- GPU 下拉框重新保留所有设备的探测序号；软件渲染器现在显示为 `2: Microsoft Basic Render Driver (当前 CPU 型号)`。本次运行的 Summary 仍使用真实渲染器名与 CPU 型号，不恢复含义不清的 `GPU 2` 标题。
- Windows OpenGL 选卡失败不再作为 Summary 中独立的说明段落；经 CLI 精确标记确认的 WGL renderer mismatch 现在进入统一 `Run issues:` 列表，类别为 `GpuRunIssueKind::OpenGlRouting`，detail 显示 `所选设备 -> 实际 GL_RENDERER`。其他 OpenGL 初始化、shader、timeout、device-lost 等错误继续使用各自类别，不会误套选卡提示。
- 默认 Release 输出最初在链接阶段因用户正在运行 `gui/x64/Release/gpu_bench_gui.exe`（PID 12320）而无法覆盖；用户随后明确要求用 `taskkill` 强制关闭该窗口。进程结束后已按 `.agents/AGENTS.md` 指定的 VS 18/v145 MSBuild 命令成功重新生成默认 `gui/x64/Release/gpu_bench_gui.exe`。相同源码也曾使用独立 `OutDir` 成功完成 Release x64 编译与链接，验证产物为 `out/gui-run-issues-verify/gpu_bench_gui.exe`；尚需用户侧做一次下拉框与 Run issues 的视觉验收。

### 2026-07-18：1–3 秒短测统计与 RenderDoc 结束边界

- 根因确认：`BenchmarkConfig::warmupTimeSec=2.0` 包含在 `maxRunTimeSec` 总墙钟内，导致 1 秒运行没有 measured frame，2 秒只有极短 CPU FPS 窗口且异步 GPU timestamp 尚未回收，3 秒才出现显存速率。`AppBase` 现在对短于 8 秒的 time-mode 运行把有效 warmup 限制为总时长的 25%；默认/正式 15 秒运行仍保持原 2 秒 warmup，frame-mode 不变。
- Windows AMD Radeon(TM) Graphics / Vulkan / Stream 1M 的隔离数据目录实测：1 秒窗口化运行 warmup 0.25 秒，116 measured frames，Avg FPS 152，VRAM rate 27.97 GB/s（1.499 ms compute）；2 秒窗口化运行 warmup 0.5 秒，241 measured frames，Avg FPS 159，VRAM rate 29.44 GB/s（1.425 ms compute）。两种短时运行均同时产生 FPS 与 GPU timing/显存速率。
- 自动 RenderDoc 定时抓帧现在由 GUI 与引擎双重限制为不晚于 `duration - 1s`；1 秒或更短的 timed run 没有合法自动抓帧点，GUI 禁用 Capture 控件且引擎将其关闭，手动 F12 不受影响。2 秒 `--capture 5` 无窗口边界 smoke 明确调整为 1 秒；1 秒同参数明确禁用；两次仍有非零 FPS 与显存速率。
- Release engine、独立 OutDir GUI 和默认 `gui/x64/Release/gpu_bench_gui.exe` 均构建通过，仅有既有 WinAppSDK/VCLibs/重复 initializer 警告。用户最新明确要求：以后 GUI 占用默认 Release 输出时直接结束 `gpu_bench_gui.exe`，无需保留旧窗口或先改用独立 OutDir；本轮按该偏好结束 PID 38480 后刷新了默认 Release。
- 第一次交付后的用户截图证明 GUI 仍为 1 秒/0 FPS 且 Capture 保持启用。复盘确认有两个遗漏：(1) `findEngineExe()` 优先启动 GUI 同目录 worker，而 `gui/x64/Release/gpu_benchmark.exe` 仍为 2026-07-17 的旧文件（803328 bytes / SHA-256 `D78A...754F2`），不是刚验证的 `build/Release` 新引擎；(2) `ValueChanged` 内通过 `DurationValueBox().Text()` 读值时，WinUI NumberBox 尚未提交模板文本，所以仍读到旧的 15。
- 已把 Duration 相关逻辑改为以 `NumberBox.Value()` 为权威值；当时长为 1 秒时，GUI 会同时取消 Capture 勾选并禁用复选框/数值框。`gpu_bench_gui.vcxproj` 新增 `CopyGpuBenchmarkWorker` AfterTargets=Build，GUI 每次构建都从对应 `$(GpuBuildDir)/$(Configuration)` 同步 worker，缺失时直接构建失败，不再静默运行旧 EXE。修复后两个 worker 的 SHA-256 均为 `5B0D9E3E7B904748B4AEABB35F7724965C583714CC0412A369B2D8E0902ED4E9`（805376 bytes）。
- 最终不是仅编译验证：启动默认 GUI 后用 Windows UI Automation 把 Duration 设为 1，实读 `CaptureEnabled=False`、`CaptureToggle=Off`、CaptureValue disabled/max=1；再从该 GUI 点击 Run 完成 1 秒 Custom/Auto/Particle 矩阵，Summary 为 Vulkan 1736.19 GB/s / 2453 FPS、DX12 2643.67 GB/s / 2793 FPS、DX11 2161.67 GB/s / 3746 FPS、OpenGL 2088.87 GB/s / 1512 FPS。验证窗口 PID 58264 已按用户偏好关闭。

### 2026-07-18：RenderDoc 主开关改为真实引擎开关

- 用户确认旧 RenderDoc 开关实际为假：GUI 只用它决定是否追加自动 `--capture` 参数，而 `AppBase::Run()` 对所有有窗口运行仍无条件初始化 RenderDoc。现在 `BenchmarkConfig::renderDocEnabled` 是引擎级主开关；GUI 每个非 headless worker 都显式传 `--renderdoc` 或 `--no-renderdoc`。`--no-renderdoc` 会同时清空自动抓帧请求，跳过 Vulkan layer 配置、DLL/API 初始化与手动 F12；RenderDoc 主开关关闭时 GUI 也取消并禁用 Capture。Capture 仍只是主开关之下的自动定时抓帧子选项。
- Windows Vulkan 的可用 layer 搜索除安装包内 `tools/RenderDoc` 外，开发机还会识别 `C:\Program Files\RenderDoc` 中相互匹配的 `renderdoc.json`/`renderdoc.dll`。开启但找不到 DLL/API、以及配置明确关闭时，Raw CLI output 现在都会输出可核对状态，不再静默。
- Release engine 与 VS 18/v145 GUI 均重新构建通过；第一次 GUI post-build 因残留 worker PID 30940 占用 `glfw3.dll` 失败，按用户“以后直接关闭”的偏好强制结束后重跑成功，并由 `CopyGpuBenchmarkWorker` 同步 GUI worker。两个 worker 的 SHA-256 均为 `240A7C9ED4EB83469DAFFE682078CA01F367B4DECE6D13050001E212B36E101D`。仅保留既有 MSB3774/MSB8027/LNK4042 与 C4996 警告。
- 实机进程级验证：同一 Vulkan worker 的 `--no-renderdoc` 运行在检查时存活且 `renderdoc.dll=False`；`--renderdoc` 运行为 `renderdoc.dll=True`。GUI UI Automation 进一步把 RenderDoc 切到 Off 后确认 `CaptureEnabled=False`，点击 Run 后实际子进程命令行为 `... --backend vulkan --particles 1048576 --no-renderdoc`（PID 41072）。全部验证 GUI/worker 已强制关闭，无本项目残留进程。

### 2026-07-18：Full Analysis 开放 Headless

- 用户要求 Full Analysis 单 GPU/全部 GPU 也能运行 Headless。GUI 的 `headlessSupported` 现对 Custom 与两种 Full Analysis 开放，同时继续拒绝 GPU Burn、graphics stress、Render3D、Volumetric、Fluid 与 Cinematic Liquid 等必须渲染的 workload。
- 勾选 Headless 会明确取消 RenderDoc 与 Capture；Custom、Full-One、Full-All 三条 job 生成路径统一追加 `--headless`，不追加 RenderDoc/capture 参数。Full Analysis 仍保留所选 GPU/API 矩阵、报告和图表流程；只有本次命令行实际请求了 `--capture`/`--capture-frame` 且没有生成 `.rdc` 时，才报告缺失抓帧，Headless 或手动关闭 Capture 不再产生假错误。
- Release engine 与 VS 18/v145 GUI 均构建成功，GUI post-build 同步 worker，两个 worker SHA-256 一致。用户随后明确要求：此类修改写完后直接编译即可，不再自动启动 GUI、操作控件或运行 benchmark；因此本切片的运行时 GUI 点击验证未执行，不能写成已实机运行验证。
- 用户截图随后确认首次实现会永久清掉进入 Headless 前的 RenderDoc/Capture 勾选状态。现已增加临时覆盖记忆：第一次进入 Headless 时保存两项原状态并临时关闭；退出 Headless（包括切换到不支持 Headless 的 workload/preset）时原样恢复。恢复后仍会经过现有 capture 安全边界校验，例如时长已改为 1 秒时不会错误恢复自动 Capture。Release engine 与 VS 18/v145 GUI 再次构建成功；按用户要求未启动 GUI 或运行 benchmark，构建前直接关闭了旧 GUI PID 52940。

### 2026-07-18：GPU Burn 固定步数三档 + 自定义

- 按用户最终决定，GPU Burn 不做设备自适应或目标帧时标定。WinUI 现与 Particle 相同采用显式档位：Light 16（默认）、Medium 64、Heavy 256、Custom 16–2048；选择 Custom 时显示独立输入框，每次任务都明确传入 `--iter`。
- 引擎默认从旧 fixed-256 改为固定 16 步，`gpuBurnAutoTune` 继续保持 false；软件渲染器仍保留 32 步安全钳制。新结果使用 `gpu_burn_v3_fixed_steps_<实际步数>_kaleidoscope` 与 `loadModel=fixed_selectable_per_frame`，因此不同固定负载互不混分；旧 `gpu_burn_v2_fixed256_kaleidoscope` 成绩也保持原组。
- `cmake --build build --config Release` 成功。首次 WinUI 链接因正在运行的 `gui/x64/Release/gpu_bench_gui.exe`（PID 50784）锁定输出而报 LNK1104，按用户长期偏好直接强制关闭后，VS 18/v145 Release x64 重编译成功，0 error；仅保留既有 VCLibs/重复 WinAppSDK initializer 警告。按用户要求未启动 GUI、未运行 benchmark。

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
