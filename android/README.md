# Mangekyo Android（主包）

> 状态（2026-07-24，事实分层）：
> - **代码已写**：`android/` Compose 壳 + `libmangekyo_jni` → 根 `gpu_engine`（Vulkan / `ANativeWindow`）。
> - **可编译（本机 Windows）**：`:app:assembleDebug` 已通过；产物约 24MB debug APK。
> - **未真机运行**：未在物理 Android 设备上冒烟。
> - **成绩身份（引擎真实写出）**：`CollectResult` 在 `__ANDROID__` 下追加 `_android_preview`
>   （例：`stream_v1_android_preview`、
>   `gpu_burn_v3_fixed_steps_16_kaleidoscope_android_preview`）。
>   默认时长为宿主 3s preview，**不是**桌面 15s 正式合同；禁止与 Windows 榜混排。
> - **未做**：GLES 3.1 降级、液体、温控 15s Burst、正式移动 duration 合同、发布包。
>
> 2026-10-06 追加：
> - **CPU 页已接入**：CPU 算力测试（`cpu_mixed_v1`）和缓存 / 内存带宽测试
>   （`cpu_membw_v1_android_preview`）。两项都在 App 进程内的原生线程上运行引擎的命令行入口。
> - **图表**：CPU 页的逐处理器柱状图、各类核心和全核的带宽曲线、存储读写条形图；
>   GPU 页的每帧耗时折线图；Charts 页的每项测试条形图。都用 Compose 绘制。
> - **可编译**：`:app:assembleDebug` 通过，debug APK 约 33 MB。
> - **界面语言**：中文和英文，跟随系统语言（`core/I18n.kt` 的 `tr(en, zh)`）；其他语言显示英文。
> - **adb 远程测试**：引擎输出同时写入 logcat，debug 包支持用启动参数直接开始一项测试，见下方"用 adb 远程测试"。
> - **仍未真机运行**；CPU 测试开始后不能取消。
>
> 2026-10-07 追加（在 Android 17 模拟器上实际跑过，真机没有）：
> - **流体**（`cinematic_liquid`）已接入，全屏运行，3 秒运行保存为
>   `cinematic_liquid_v2_physical_scene_v8_preview_android_preview`。上面 2026-07-24 的"未做"里的液体不再适用。
> - **显存 / GPU 缓存带宽**（`membw`）已接入，保存为 `membw_v1_android_preview`。它只做计算，没有画面，
>   和 CPU 测试一样走引擎的命令行入口；着色器目录通过环境变量 `GPU_BENCH_SHADER_DIR` 告诉引擎。
> - **粒子和 GPU Burn 的更重档位**：`stream_heavy`（100 万粒子）、`gpu_burn_medium`（64 步）、`gpu_burn_heavy`（256 步）。
> - **GPU 测试全屏横屏运行**，渲染尺寸固定 1280 × 720。
> - **显存带宽测试可以中途停止**（只编译通过，这一点没有在模拟器上点过）；被停止的运行不保存成绩。
> - **测试设置和 Windows 对齐**：GPU 页列出四项测试，选中后可以选粒子数（65K / 1M / 4M / 16M / 自定义）、
>   GPU Burn 步数（16 / 64 / 256 / 自定义 16–2048）和运行时长（3 / 15 / 60 秒 / 自定义），默认值和 Windows 相同
>   （1M、16 步、15 秒）。在模拟器上点选并运行过。

## 已完成（相对脚手架）

- Compose BOM `2026.06.01` / Material3；AGP **9.3.1** + Kotlin **2.3.21**；minSdk **23**
- Material You：API 31+ 动态取色，以下品牌静态色
- 四页 Material 3 壳 + SplashScreen
- `CapabilityGate`：Vulkan dlopen 探针 + HW feature + GL ES 版本
- `ResultsStore`：应用专属目录；引擎经 `GPU_BENCH_DATA_DIR` 写同一根
- **gpu_engine**：根 CMake `ANDROID` 分支（无 GLFW / 无 CLI）；`libmangekyo_jni` 链引擎 + NDK `vulkan`
- JNI：Surface 生命周期、后台线程 Run、`RequestStop` 协作取消
- SPIR-V：构建时 `glslc` → `assets/shaders/`（particle + compute + gpu_burn）
- UI registry 展示与引擎一致的 `*_android_preview` 版本字符串（仅 stream / gpu_burn）
- CPU 页：JNI `nativeStartCli` 在原生线程上运行 `gpu_bench::cliMain`；引擎输出经
  `std::cout` / `std::cerr` 缓冲区收集，界面用 `nativePollOutput` 轮询（`core/EngineOutput.kt` 解析）
- 图表：`ui/components/Charts.kt`（折线图、柱状图、条形图，Compose `Canvas` 绘制）

## 构建

```bash
cd android
# 需要 PATH 上有 glslc（Vulkan SDK），用于打包 SPIR-V
./gradlew :app:assembleDebug
```

产物：`app/build/outputs/apk/debug/app-debug.apk`。

## 运行说明

1. API 24+ 且 Vulkan loader 可用时，GPU 页可选 Stream / GPU Burn（light steps=16），点 Run（默认 3s）。
   测试会横屏占满屏幕运行，跑完回到页面，"Last run" 卡片显示成绩和每帧耗时折线图。
   渲染尺寸固定为 1280 × 720（和桌面版相同），由系统缩放到屏幕，所以成绩不随屏幕大小和分辨率变化。
2. 跑分线程不占 UI；Stop 调用 `RequestStop()`。
3. 写出结果时 `workloadVersion` 带 `_android_preview`，`platform=Android`。
   每条成绩还记录这台手机的信息（2026-10-06 加入），历史页每条成绩下面有一行显示：
   - **机型 `deviceModel`：** 厂商加型号，取自 `ro.product.manufacturer` 和 `ro.product.model`；
   - **芯片 `cpuName`：** 取自 `ro.soc.manufacturer` 和 `ro.soc.model`（Android 12 起才有），
     更早的系统退回到 `ro.hardware.chipname`，再退回到 `ro.board.platform`；
   - **系统 `osVersion`：** 形如 `Android 14 (API 34)`，原来这里记的是内核版本；
   - **内存 `systemMemoryMB`：** 系统能用的内存总量，比标称容量少几个百分点。

   **这些字段只编译通过，没有在真机上核对过实际读到的文字。**
4. CPU 页选择测试模式后点 Run。缓存 / 内存带宽测试大约一分钟，手机会发热；
   存储测试会在 App 私有目录里写一个最大 1 GiB 的临时文件，测完删除。
   不需要安装 App 的另一条路：`scripts/build-android-cpumem.ps1` 编出的独立程序可以用 adb 直接运行。

## 窗口宽度和折叠屏

2026-10-06 加入；2026-10-07 在 Android 17 的模拟器上看过（用 `wm size` 改屏幕大小），真机和真正的折叠屏没有试过。

- 布局按窗口宽度切换（`ui/AdaptiveLayout.kt`），不看设备类型：
  - 600 dp 以下：底部导航栏，单列；
  - 600 dp 起：导航移到侧边；
  - 840 dp 起：每页分成左右两栏；1200 dp 起侧边导航展开显示文字。
- GPU 测试全屏运行，画面固定 16:9（2026-10-07 起；页面里不再有小预览）。
- 如果帧率被屏幕刷新率限制住、GPU 没有跑满，"最近一次运行"里会提示成绩可能偏低，这时请换更重的档位。
  logcat 里的 `[Vulkan] Present mode:` 一行说明这次运行是否跟随刷新率。
- 折叠屏展开或合上、分屏拖动、旋转、接上键盘时不重建 Activity（`AndroidManifest.xml` 的 `configChanges`），正在运行的测试和预览画面不受影响。
- 导航栏和导航侧栏是 Material 3 Expressive 的组件。material3 1.4 里 Expressive 的其余部分还没有公开；它们在 1.5 的预览版里，这个工程不用预览版，等 1.5 出正式版再换。
- 没有处理折叠屏的铰链位置和半开姿态。

## 用 adb 远程测试

**debug 包的引擎是按 `-O2` 编的。** 2026-10-07 之前它按 `-O0` 编译，那之前的 CPU 成绩都无效。

**一键跑完整的一套：** `scripts\adb-suite.ps1 -Install` 依次跑 13 项（粒子四档窗口和 Headless、GPU Burn 64 步、流体、
显存带宽、CPU 算力、CPU 缓存 / 内存 / 存储），GPU 测试每项 15 秒，结果收在 `out\adb\<机型>-<时间>\`。
手机要解锁；小米手机还要在开发者选项里打开"USB 安装"。脚本的说明见它开头的注释。

debug 包可以不碰手机就开始一项测试（2026-10-06 加入；2026-10-07 在 Android 17 和 Android 7.0 的模拟器上实际跑通，真机还没有试过）。

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb logcat -c
adb shell am start -S -n com.mangekyo.benchmark/.MainActivity --es run cpu_membw
adb logcat -s Mangekyo MangekyoEngine
adb pull /sdcard/Android/data/com.mangekyo.benchmark/files/results/results.json
```

- **`--es run <测试>`：** `cpu_membw`（缓存 / 内存带宽）、`cpu_all`（单核 + 全核）、`cpu_per_core`、`cpu_multi`、
  `stream`（粒子，默认 100 万）、`gpu_burn`（默认 16 步）、`cinematic_liquid`（流体）、
  `membw`（显存 / GPU 缓存带宽，约 20 秒）。
- **Headless 粒子测试：** `--es run "stream_headless?particles=1048576"`，只计算、不显示画面。
- **GPU 测试可以带设置：** `--es run "stream?particles=4194304"`、`--es run "gpu_burn?steps=128"`
  （粒子数向上取整到 256，步数 16–2048）。旧名字 `stream_heavy`、`gpu_burn_medium`、`gpu_burn_heavy` 仍然可用。
  App 打开后自动切到对应页面并开始。
- **`--ei seconds <N>`（可选）：** CPU 算力测试每项的秒数（默认 1），或 GPU 测试的时长（默认 3）。
- **logcat 里有什么：**
  - 标签 `MangekyoEngine`：引擎打印的每一行，包括进度、各级带宽和成绩；
  - 标签 `Mangekyo`：`AUTORUN accepted / started / refused / failed` 和结束时的 `RUN finished kind=cpu exit=0`
    或 `RUN finished kind=gpu score=...`，脚本可以等这一行。
- **只在 debug 包里生效。** 正式包会拒绝并打印 `AUTORUN refused ... reason=release_build`，否则手机上任何 App 都能用这个参数让它开跑。
- **GPU 测试需要屏幕亮着并解锁**，因为它要等预览画面创建出来（最多等 10 秒）。
- 没有模拟点击，也不需要知道按钮坐标。
- 在 Git Bash 里运行时，命令前要加 `MSYS_NO_PATHCONV=1`，否则 `/sdcard/...` 会被改写成 Windows 路径；PowerShell 不受影响。
- 模拟器里的成绩数字不可信（虚拟 GPU 的计时、虚拟机里的时钟都有问题），只用来确认功能。

## Windows 开发注意

- 勿写 `import androidx.compose.ui.modifier`（类/包大小写冲突）；用 `import androidx.compose.ui.*`
- 命名参数必须小写 `modifier =`

## 主要待实现

真机冒烟、GLES 3.1 / EGL 降级、温控 15s Burst 合同、液体、CPU 测试的取消、
独立 ES 2.0 legacy APK、抓帧策略。
