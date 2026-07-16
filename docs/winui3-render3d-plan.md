# Mangekyo WinUI 3 对接 + 真 3D 渲染架构方案

> 配套文档：[`nbody-workload-plan.md`](nbody-workload-plan.md)（compute 算力子项）。
> 本文覆盖三件事：**① 真 3D 渲染负载、② WinUI3 对接所需的架构解耦、③ RenderDoc 进 WinUI3 的可行性**。
> 总原则：先做对所有目标（WinUI3 / 安卓 / iOS / 开发板）都受益的解耦，再做平台壳。

---

## A. 真 3D 渲染负载

### A.1 现状
顶点着色器只用 xy、无 MVP、无深度（`particle.vert`：`gl_Position = vec4(inPosition.xy, 0, 1)`）。
渲染段计时基本是噪声，压不到光栅化 / 填充率 / ROP。"3D" 名不副实。

### A.2 目标
让渲染真正成为一个可计分的 3D 子项，压到：**顶点变换吞吐 + 光栅化 + 填充率/ROP + 深度测试**。

### A.3 设计
- **MVP 矩阵 + 透视投影 + 相机**：每帧一个图形 UBO/cbuffer。相机做缓慢轨道动画，
  **由帧序号 / 固定步长驱动（绝不用墙钟）**，保证可复现。
- **深度缓冲 + 深度测试**：开 depth attachment，粒子有真实前后遮挡。
- **粒子 billboard 四边形**：从 2px 点 → 朝相机的四边形（2 三角形 / 粒子），
  距离衰减大小 + alpha 软粒子混合 → 产生真实 overdraw / 填充负载。
- N-body 子项天然产生 3D 位置，二者组合即"真 3D + 真算力"。

### A.4 为什么比 N-body 重（改动面）
N-body 复用了现有 SSBO 和管线；真 3D 要碰**每个后端的图形管线**：
1. **新增图形 UBO**（现在顶点着色器没有任何 uniform）→ 改 descriptor set layout / root signature / pipeline layout。
2. **render pass / framebuffer 加深度附件**：
   Vulkan render pass + depth image / DX12 DSV / DX11 depth-stencil view / GL depth renderbuffer / Metal depth attachment。
3. 顶点着色器改 billboard 展开（或用几何/实例化），片元着色器加软粒子混合。

→ 这是 5 后端里改动面最大的一项。**建议排在 N-body 之后**。headless 模式不渲染，不受影响。

### A.5 计分
3D 渲染分基于 render 段 GPU 时间，固定 N / 固定相机轨迹 / 固定步数 →
可比的"填充+几何"分。与带宽分、算力分一起进综合总分。

---

## B. WinUI3 对接：架构解耦（四目标共同前提）

> 不立即写 WinUI3，但现在就铺路。下面每一步对 WinUI3 / 安卓 / iOS / 开发板 headless 都有收益。

### B.1 拆库：引擎 ↔ 前端
现状：CMake 只产出可执行文件 `gpu_benchmark`，`main.cpp` 的交互菜单和引擎耦合在一起。

目标：
- **`gpu_engine`（库，static + 可选 DLL）**：`AppBase` + 5 后端 + shader + 计时 + 结果。
- **`gpu_benchmark_cli`（可执行）**：现有控制台菜单，链接 `gpu_engine`。
- 未来 **WinUI3 壳**：C# / C++WinRT，同样链接 / 调用 `gpu_engine`。

### B.2 渲染表面：**首选「独立窗口（启动器模式）」**

> **决策：WinUI3 只做控制面板（按钮/参数/结果表/图表），实际渲染在一个独立的顶层窗口里，
> 由引擎自己持有。** SwapChainPanel/共享纹理 interop 降级为"后期可选的嵌入式观感升级"（见 B.2.1）。

为什么独立窗口是首选：
- **5 个后端本来就渲染进 HWND**（Vulkan `VK_KHR_win32_surface` / 桌面 GL WGL / DX DXGI swapchain），
  这正是现在 GLFW 在做的事 → **引擎渲染那一半几乎不用改**。
- **零互操作胶水**：不需要 SwapChainPanel、不需要共享纹理、不需要给 Vulkan/GL 写 D3D 互操作层。
- **全部 5 后端天然可用**，无须区别对待。
- **RenderDoc 更顺**：普通 HWND swapchain 的 present 是 RenderDoc 最标准的抓帧场景，
  连"传 NULL window / 绕过 composition present"的技巧都不需要（见 C 节）。
- 唯一代价是**观感**：控制面板 + 渲染窗口是两个窗口，而非镶嵌在一起。
  对跑分软件这是惯例（3DMark / Superposition 皆为"启动器 + 独立渲染窗口"），符合用户预期。

实现：
- 引擎在**独立线程**创建并持有渲染窗口（继续用 GLFW，或换裸 Win32 HWND 以去掉 WinUI3 构建对 GLFW 的依赖）；
  WinUI3 UI 在主线程。窗口消息泵在创建它的线程上跑。
- 两边通过 B.4 的 C ABI + 线程安全指令队列通信（选后端 / 配置 / 开始 / 停止 / 取结果）。
- **最快落地**：可先做一个 WinUI3 控制面板去驱动**现在这个带 GLFW 窗口的引擎**，渲染代码一行不动就有 GUI。

surface 来源抽象（概念上的 `SurfaceSource`）仍保留，用于覆盖各场景：

| 来源 | 用途 |
|------|------|
| `OwnWindow`（GLFW / 裸 Win32 HWND，引擎自持） | **WinUI3 启动器模式（首选）** + 现有控制台路径 |
| `Headless`（无 surface） | 开发板 / 纯算力，**剥离 headless 路径里的 glfwInit 依赖** |
| （可选）`ExternalPanel`（SwapChainPanel/HWND） | 后期嵌入式观感升级，见 B.2.1 |
| （后续）`ANativeWindow` / `CAMetalLayer` | 安卓 / iOS |

#### B.2.1 （后期可选）嵌入式观感升级：SwapChainPanel / 互操作

若以后想要"3D 视图镶在 WinUI3 界面中间"的精致观感，再上这条（非必需）：

| 后端 | 嵌入 WinUI3 的方式 | 成本 |
|------|------------------|------|
| DX12 / DX11 | 直连 SwapChainPanel（`ISwapChainPanelNative::SetSwapChain` + `CreateSwapChainForComposition`） | 一层 |
| Vulkan | `VK_KHR_external_memory_win32` 共享 D3D11 纹理 → DXGI swapchain → panel（参考 repo: malstraem/vulkan-interop-directx） | 两层胶水 |
| OpenGL（桌面 4.3） | `WGL_NV_DX_interop` 共享纹理 → 同上 | 两层胶水 |
| ~~OpenGL via ANGLE~~ | ANGLE 是 GL **ES**，对不上桌面 GL 4.3 compute | 不适用 |

- interop 的共享纹理 copy/present **在 GPU 时间戳测量区间之外**，不污染跑分数据。
- 即便走嵌入式，RenderDoc 仍可显式 `Start/EndFrameCapture` 抓原生 `VkDevice`/GL context。

### B.3 可外部驱动的帧循环
现状：`AppBase::Run()` 拥有 `MainLoop()`，自己 `glfwPollEvents` + 跑到时间/帧数上限。

目标：拆成可被宿主驱动的形态：
- `Init(config, surface)` → `Tick(deltaTime)`（渲染一帧）→ `Shutdown()`。
- 控制台路径内部仍可用一个循环调 `Tick`；WinUI3 由合成/渲染回调驱动 `Tick`。
- 计时 / 累积 / 结果收集逻辑不变，只是循环的所有权从引擎移到宿主。

### B.4 C ABI / 扁平接口（给 C# / WinRT）
在 `gpu_engine` 上包一层 C 接口，供 WinUI3 P/Invoke 或 C++/WinRT 组件调用：

```c
EngineHandle engine_create(const EngineConfig* cfg, void* surface);
void         engine_tick(EngineHandle, float dt);
void         engine_get_result(EngineHandle, BenchmarkResultC* out);
void         engine_trigger_renderdoc(EngineHandle);   // 见 C 节
void         engine_destroy(EngineHandle);
```

与 `main.cpp` 交互菜单完全解耦——菜单只是 CLI 前端的一种实现。

### B.5 改动定位小结（独立窗口模式）
| 区域 | 改动 |
|------|------|
| CMake | 拆 `gpu_engine` 库 + `gpu_benchmark_cli` 可执行（+ 可选 DLL 导出） |
| `AppBase` | 拆 `Run()` → `Init/Tick/Shutdown`；headless 去 glfw 依赖。**渲染窗口逻辑基本不动** |
| 后端 | **不改**（继续渲染进自持 HWND） |
| 新增 | C ABI 头 + 实现；引擎线程 + 线程安全指令队列 |
| WinUI3 壳 | C# 控制面板 + P/Invoke 驱动引擎（**无需 SwapChainPanel**） |

> 对比"嵌入式"方案：独立窗口省掉了"后端支持外部 panel/swapchain"和"Vulkan/GL 互操作胶水"两大块，
> 工作量与风险都降一档。嵌入式（B.2.1）作为后期可选升级。

---

## C. RenderDoc 进 WinUI3：可行性

### C.1 结论
**独立窗口模式下 RenderDoc 最顺：渲染窗口是普通 HWND swapchain，present 路径标准，
全部 5 后端都能用现有 In-App API 正常抓帧——无需任何特殊技巧。**

### C.2 为什么（独立窗口）最顺
- 项目已用 **In-App API**（自己加载 renderdoc.dll，`StartFrameCapture`/`EndFrameCapture` 包一帧），
  不依赖 RenderDoc UI 注入。
- 渲染在引擎自持的普通 HWND 窗口里 → present 是 RenderDoc **最标准**的抓帧场景，
  Vulkan/DX/GL 都走原生路径，现有 F12 / `--capture` 逻辑可直接复用。
- 与 WinUI3 的 XAML 合成完全无关（渲染窗口本就独立于 UI 窗口）。

### C.3 前提与坑（大幅减少）
| 项 | 说明 | 对策 |
|----|------|------|
| 打包限制 | MSIX/商店打包应用沙箱挡 dll 加载/注入 | **用非打包 WinUI3** |
| ~~present 接管~~ | 仅"嵌入式 SwapChainPanel"模式才有此问题 | 独立窗口模式不涉及；若走 B.2.1 嵌入式，再用显式 Start/End 绕过 |

### C.4 接入点
`engine_trigger_renderdoc()`（B.4）在下一次 `Tick` 时用现有 In-App API 抓帧；WinUI3 UI 给一个按钮调它。
现有 `AppBase` 的 RenderDoc 逻辑（自动探测、F12、`--capture`、`GetLastCapturePath`）几乎可原样复用——
独立窗口模式下无需改造。

---

## D. 总体推进顺序（含负载套件，见 [`benchmark-workload-suite.md`](benchmark-workload-suite.md)）

1. **N-body 算力子项**——最轻，风险最低，先验证算力计分。
2. **Stream → 带宽分**——加 `Workload` 枚举时顺手做，几乎零成本。
3. **Synthetic Peak / Stress-Fractal 负载**——按兴趣推进（Stress/Fractal 是你想做的图形压力轴）。
4. **架构解耦（B 节）**——拆库 + `Init/Tick/Shutdown` + C ABI + 引擎线程。WinUI3 / 安卓 / iOS / 开发板共同前提。
5. **WinUI3 壳（独立窗口模式）**——C# 控制面板 + P/Invoke 驱动引擎；渲染走引擎自持 HWND。**可早于真 3D 落地**。
6. **RenderDoc 接入**——独立窗口 + 现有 In-App API，几乎零改造。
7. （可选）**真 3D 渲染（A 节）** / **嵌入式观感升级（B.2.1）**——改动面最大，放最后，避免返工。
8. （后续）安卓（Vulkan + ANativeWindow，参照 ohos 港口）/ iOS（Metal + CAMetalLayer）。
9. （后续）综合评分公式：带宽 × 算力 × 峰值 × 填充 加权。

> 关键决策：**独立窗口模式让 WinUI3 提前**——不再卡在"真 3D / SwapChainPanel"之后，
> 因为渲染窗口由引擎自持、后端不改。解耦（第 4 步）仍是枢纽，但范围比嵌入式方案小一档。
