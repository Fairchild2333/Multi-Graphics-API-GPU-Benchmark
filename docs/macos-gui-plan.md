# Mangekyo macOS GUI 实施计划（SwiftUI + Liquid Glass）

> 目标:为本项目做一个原生 macOS 图形前端,功能对齐现有的 Windows
> WinUI 3 GUI([`gui/`](../gui/)),并采用 **macOS Tahoe (macOS 26) 的
> Liquid Glass 设计语言**。本文件只描述方案,不含实现代码。

---

## 1. 目标与范围

### 1.1 要做什么

一个**原生 SwiftUI 控制面板**,与 Windows GUI 功能对等:

- **Run** —— 选择 GPU / 图形 API / workload / precision / frames / 额外参数 +
  高级开关(headless、V-Sync、host-memory),点按钮跑测,实时显示日志输出与分数。
- **History** —— 读取已保存的基准结果,支持排序、按 GPU 过滤、时间范围过滤、删除。
- **Charts** —— 调用 `scripts/plot_workloads.py` 生成 PNG 并展示。
- **Settings** —— 主题(跟随系统 / 浅 / 深)、语言(自动 / English / 简体中文)、
  工作目录(仓库根)选择。
- **About** —— 版本、说明、GitHub 链接。

### 1.2 明确不做的事(范围边界)

- **不重写 benchmark 逻辑**。所有探测、跑测、计分、结果持久化都复用现有
  `gpu_engine` 静态库。
- **不把渲染嵌入 GUI 窗口**。沿用 Windows 的「分离窗口」模型:渲染画面仍由引擎
  自己弹出的 GLFW/Metal 窗口显示,GUI 只是控制台。(嵌入式渲染列为后续可选项。)
- 第一版**不做 App Sandbox / 公证 / 上架**。先做本地可运行的开发版。

---

## 2. 架构:复用引擎,镜像 Windows 方案

Windows GUI 的精髓是**不用子进程**,而是把引擎编译成静态库 `gpu_engine`,在 GUI
进程内的工作线程上直接调用 [`gpu_bench::cliMain`](../src/gpu_engine.h)。macOS 版采用
**完全相同的架构**:

```
┌──────────────────────────────────────────────┐
│  SwiftUI App (GPUBenchmark.app)                │
│  ┌────────────┐   调用   ┌──────────────────┐  │
│  │ SwiftUI UI │ ───────► │ ObjC++/C++ 桥接层 │  │
│  │ (5 个页面) │ ◄─────── │  GpuBenchBridge   │  │
│  └────────────┘  日志/分数 └────────┬─────────┘  │
│                                     │ 链接       │
│                          ┌──────────▼─────────┐  │
│                          │  libgpu_engine.a   │  │
│                          │ (cliMain / Load…)  │  │
│                          └──────────┬─────────┘  │
└─────────────────────────────────────┼───────────┘
                                       │ 跑测时弹出
                              ┌────────▼─────────┐
                              │ 引擎自有渲染窗口  │
                              │ (Metal/Vulkan/GL) │
                              └──────────────────┘
```

**复用清单(零改动或极小改动):**

| 复用项 | 来源 | 说明 |
|--------|------|------|
| `gpu_engine` 静态库 | [`CMakeLists.txt:135`](../CMakeLists.txt#L135) | macOS 上已带 Metal+Vulkan+OpenGL,平台无关 |
| `cliMain(argc, argv)` | [`src/gpu_engine.h`](../src/gpu_engine.h) | 唯一入口,合成 argv 即可驱动 |
| `LoadResults()` / `DeleteResult()` 等 | [`src/benchmark_results.h`](../src/benchmark_results.h) | History 直接进程内读写 |
| `--list-gpus` | cliMain | 枚举 GPU,解析 stdout |
| `scripts/plot_workloads.py` | [`scripts/`](../scripts/) | Charts 生成 PNG |
| `results/results.json` | 相对路径 | History 与 plot 脚本共同读写 |

---

## 3. 技术选型

- **UI 框架:SwiftUI**(已确认)。在 **Xcode 26 / macOS 26 SDK** 下编译,标准控件
  (`NavigationSplitView` 侧边栏、工具栏、表单、按钮)**自动**获得 Liquid Glass 外观;
  自定义卡片用 `.glassEffect()` 等修饰符补充。
- **Swift ↔ C++ 桥接:Objective-C++ (`.mm`) 薄封装层**。
  - 理由:`cliMain` 在 `namespace gpu_bench` 下、且涉及 `std::cout` 重定向、`chdir`、
    线程、`std::vector<BenchmarkResult>` 等 C++ 类型;用一个 `.mm` 桥把这些封装成
    Swift 友好的纯 C / ObjC 接口,最稳、最少坑。
  - 项目本来就有 Objective-C++([`src/metal_backend.mm`](../src/metal_backend.mm)),
    与现有代码风格一致。
  - (备选:Swift 6 的直接 C++ interop。但 `std::cout` 捕获 + 命名空间 + STL 容器
    跨界,直接 interop 反而更繁琐,故不采用。)
- **工具链(已确认可用):** macOS 26.5、Xcode 26.5、Swift 6.3.2(`arm64-apple-macosx26.0`)。

---

## 4. 桥接层设计(`GpuBenchBridge`)

一个 `.mm` 文件 + 一个 `.h`,导出供 Swift 调用的接口。要点:

### 4.1 接口草案(形态,非最终签名)

```
// 跑一次基准:合成 argv -> cliMain;通过回调把 stdout 逐行送回 Swift。
void  gpb_run(const char* const* argv, int argc,
              void (*onLine)(const char* line, void* ctx), void* ctx);

// 枚举 GPU:跑 "--list-gpus",返回整段 stdout 由 Swift 解析。
const char* gpb_list_gpus(void);

// History:进程内调用 LoadResults(),序列化成 JSON 字符串返回给 Swift 解码。
const char* gpb_load_results(void);
bool        gpb_delete_result(const char* id);

// 设置工作目录(仓库根),确保 results/ 相对路径正确。
void        gpb_set_working_dir(const char* path);
```

Swift 侧用一个 `@MainActor` 的 `BenchEngine`(`ObservableObject` / `@Observable`)包住这些 C 函数,
把回调桥接成 `@Published` 的日志流与状态。

### 4.2 三个必须处理的技术细节(相对 Windows 的差异)

1. **工作目录(cwd)** —— 结果路径是相对的 `results/results.json`
   ([`benchmark_results.cpp:33`](../src/benchmark_results.cpp#L33))。`.app` 启动时
   cwd 是 `/`,**必须先 `chdir` 到仓库根**(或用户在 Settings 选的工作目录),
   否则结果写到错误位置、History 读不到。WinUI 在 `MainWindow.xaml.cpp:372` 也做了等价处理。

2. **stdout 捕获要用管道,不能只换 `rdbuf`** —— WinUI 用 `std::cout.rdbuf()` 交换,
   只能抓到 C++ iostream。引擎里可能有 `printf` / C `stdio` 输出。macOS 版改用
   **`pipe()` + `dup2(fd, STDOUT_FILENO)`** 在后台线程读取,抓全所有 stdout/stderr,
   再逐行回调给 Swift。需注意线程安全与跑测结束后恢复 fd。

3. **线程模型** —— `cliMain` 在工作线程跑(可能弹窗、阻塞)。所有回到 UI 的更新
   必须切回主线程(`DispatchQueue.main` / `@MainActor`)。同一时刻只允许一个跑测
   任务(GLFW/Metal 窗口与全局 stdout 重定向都不可重入)。

---

## 5. 工程结构

新增目录 `macos-gui/`(与 `gui/` 平级):

```
macos-gui/
├── GPUBenchmark.xcodeproj/          # Xcode 工程(或用 SwiftPM,见 §6)
├── GPUBenchmark/
│   ├── App.swift                    # @main, WindowGroup
│   ├── ContentView.swift            # NavigationSplitView 外壳(侧边栏 5 项)
│   ├── Pages/
│   │   ├── RunView.swift
│   │   ├── HistoryView.swift
│   │   ├── ChartsView.swift
│   │   ├── SettingsView.swift
│   │   └── AboutView.swift
│   ├── Engine/
│   │   ├── BenchEngine.swift        # Swift 封装 + 状态(@Observable)
│   │   ├── GpuBenchBridge.h         # C 接口声明
│   │   ├── GpuBenchBridge.mm        # ObjC++ 实现(调 cliMain/LoadResults)
│   │   └── GPUBenchmark-Bridging-Header.h
│   ├── Models/
│   │   ├── BenchResult.swift        # 对应 BenchmarkResult 的 Codable
│   │   ├── Preset.swift             # 预设 -> argv 组装(对齐 WinUI 预设)
│   │   └── Localization.swift       # i18n(对齐 gui/i18n.h)
│   ├── Design/
│   │   └── GlassCard.swift          # Liquid Glass 卡片复用组件
│   └── Assets.xcassets              # 图标、强调色
├── README.md                        # 构建与运行说明
└── (链接) ../build/libgpu_engine.a  # 由 CMake 产出
```

---

## 6. 构建集成

引擎用 CMake 构建,GUI 用 Xcode/SwiftUI——两套构建系统需要衔接。两条路线:

### 方案 A(推荐):Xcode 工程链接 CMake 产物
1. 先用 CMake 构建引擎,产出 `build/libgpu_engine.a`(+ `build/libglad_gl43.a`)。
2. Xcode 工程把这些 `.a` 加入 *Link Binary With Libraries*,并链接所需系统框架:
   `Metal`、`MetalKit`、`QuartzCore`、`Foundation`、`AppKit`,以及 `glfw`、
   (可选)`MoltenVK`/`vulkan`、OpenGL(`-framework OpenGL`,旧版)。
3. 头文件搜索路径指向 `src/`(`gpu_engine.h`、`benchmark_results.h`)。
4. 加一个 *Run Script* 阶段在 GUI 构建前先 `cmake --build build`(可选,保证库最新)。

> 注意:CMake 当前只声明了 `gpu_engine STATIC`,需确认 macOS 下确实产出
> `libgpu_engine.a`(目前 `build/` 里只见到 `libglad_gl43.a` 与可执行文件
> `gpu_benchmark`——可能是增量构建未单独保留 .a,**第一步要先验证/触发其生成**)。

### 方案 B:全 SwiftPM
用 `Package.swift` 把 `.mm` 桥 + C++ 源做成 target。可行但要把引擎所有源码与编译选项
搬进 SPM,维护成本高、易与 CMake 配置漂移。**不推荐**,除非想彻底脱离 CMake。

---

## 7. UI 设计(Liquid Glass)

### 7.1 外壳
- `NavigationSplitView`:左侧 sidebar 列出 Run / History / Charts / Settings / About
  (带 SF Symbols 图标),右侧 detail 切换页面。sidebar 在 macOS 26 下自带玻璃材质。
- 顶部 `.toolbar` 放主操作(如 Run 页的「运行」按钮、History 的「刷新/删除」)。
- 窗口标题、强调色随系统;支持浅/深色与「跟随系统」。

### 7.2 卡片
- 复刻 WinUI 的卡片式表单:用 `GlassCard`(`RoundedRectangle` + `.glassEffect()`
  或 `.background(.regularMaterial)` 作旧系统回退)包裹各分组(Options / Advanced /
  Result / Output)。
- Run 页输出区:等宽字体的可滚动日志(对应 WinUI 的 `OutputBox`)+ 顶部高亮分数卡。

### 7.3 与 Windows 的设计对应表

| WinUI 元素 | SwiftUI 对应 |
|------------|--------------|
| `NavigationView` 左导航 | `NavigationSplitView` sidebar |
| Mica backdrop | Liquid Glass(`.glassEffect` / material) |
| `Border` + `CardBackgroundFill` 卡片 | `GlassCard`(material/glass) |
| `ComboBox` | `Picker`(menu 样式) |
| `CheckBox` | `Toggle` |
| `ProgressRing` | `ProgressView`(circular) |
| `AccentButtonStyle` | `.buttonStyle(.borderedProminent)` |
| `ListView`(History 表) | `Table`(原生多列 + 排序) |

> History 用 SwiftUI `Table` 会比 WinUI 的手搓表格更好——原生列排序、多选、自适应宽度。

### 7.4 预设(Preset)
对齐 WinUI 的 7 个预设(Quick / Custom / Full-one / Full-all / Flights / Particles /
Headless),每个预设映射成一组 `argv`。组装逻辑参考
`MainWindow.xaml.cpp` 中 `buildJobs`/preset 部分,移植到 `Preset.swift`。

---

## 8. 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| `libgpu_engine.a` 未单独产出 | 链接失败 | 第一步先确认/触发 CMake 生成静态库 |
| cwd 不对 → 结果写错位置 | History 空、结果丢失 | 桥接层 `chdir` 到工作目录;Settings 可改 |
| stdout 抓不全(printf) | 日志缺失 | 用 `pipe`+`dup2` 抓全,不只换 rdbuf |
| 跑测重入(GLFW/全局 fd) | 崩溃/串流 | 单任务串行,跑测中禁用 Run 按钮 |
| 引擎弹独立窗口被误以为「没反应」 | 体验困惑 | UI 文案提示「渲染将在独立窗口打开」 |
| Charts 依赖系统 Python + matplotlib | Charts 失败 | 检测 `python3`/依赖,缺失时给指引;沿用 `scripts/requirements.txt` |
| 后续若上架需 Sandbox | chdir/弹窗/Python 受限 | 第一版不沙盒;上架另立计划 |
| Vulkan(MoltenVK)在 macOS 可选 | 链接/运行差异 | 默认主推 Metal;Vulkan 后端按 CMake 现状处理 |

---

## 9. 分阶段实施步骤

> 每阶段结束都应能编译/运行,便于增量验证。

**阶段 0 — 构建验证(0.5 天)**
- 确认 CMake 在 macOS 产出 `libgpu_engine.a`;若无则调整 CMake/触发生成。
- 写一个最小命令行测试:链接该库,`chdir` 到仓库根,调 `cliMain(--list-gpus)`,
  确认能枚举 GPU、能跑一次 `--benchmark --headless`。**先验证桥接可行,再碰 UI。**

**阶段 1 — 桥接层(1 天)**
- 实现 `GpuBenchBridge.{h,mm}`:`gpb_set_working_dir` / `gpb_list_gpus` /
  `gpb_run`(管道捕获 stdout + 行回调)/ `gpb_load_results` / `gpb_delete_result`。
- Swift `BenchEngine` 封装 + 单元自测(控制台打印验证)。

**阶段 2 — SwiftUI 外壳 + Run 页(1.5 天)**
- `NavigationSplitView` 五页骨架 + Liquid Glass 卡片组件。
- Run 页:GPU/API/workload/precision/frames/extra + 高级开关 → argv → 跑测 →
  实时日志 + 分数。对齐 WinUI 预设。

**阶段 3 — History 页(1 天)**
- `Table` 展示 `LoadResults()`,排序 / GPU 过滤 / 时间范围 / 多选删除。

**阶段 4 — Charts + Settings + About(1 天)**
- Charts:跑 `plot_workloads.py`,展示 `docs/images/workload_*.png`。
- Settings:主题 / 语言 / 工作目录。About:版本 + GitHub 链接。

**阶段 5 — Liquid Glass 打磨 + 文档(0.5–1 天)**
- 统一玻璃材质、强调色、深浅色、SF Symbols;旧系统回退路径。
- 写 `macos-gui/README.md`;在根 `README.md` 增加 macOS GUI 段落。

**粗估:约 5–6 个工作日**(熟悉 SwiftUI 的话更快;阶段 0–1 是关键风险点)。

---

## 10. 待确认决策

1. **构建方式**:方案 A(Xcode 链接 CMake 产物,推荐)还是方案 B(全 SwiftPM)?
2. **目录命名**:`macos-gui/`?(与现有 `gui/` 区分)
3. **Vulkan 后端**:macOS GUI 是否暴露 Vulkan(MoltenVK)选项,还是只给 Metal/OpenGL?
4. **i18n**:第一版是否就做中/英双语(对齐 Windows),还是先英文?
5. **嵌入式渲染**:是否将来要把渲染画面用 `MetalKit`/`CAMetalLayer` 嵌进 GUI 窗口
   (而非独立窗口)?这会显著增加复杂度,建议列为 v2。

---

## 11. 参考

- Windows 对照实现:[`gui/README.md`](../gui/README.md)、
  [`gui/MainWindow.xaml`](../gui/MainWindow.xaml)、`gui/MainWindow.xaml.cpp`
- 引擎入口:[`src/gpu_engine.h`](../src/gpu_engine.h)
- 结果模型与读写:[`src/benchmark_results.h`](../src/benchmark_results.h)、
  [`src/benchmark_results.cpp`](../src/benchmark_results.cpp)
- 构建定义:[`CMakeLists.txt`](../CMakeLists.txt)
- 既有 Obj-C++ 范例:[`src/metal_backend.mm`](../src/metal_backend.mm)
- 平台说明:[`docs/macos-notes.md`](macos-notes.md)
