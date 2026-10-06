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
   跑完后页面上的 "Last run" 卡片显示成绩和每帧耗时折线图。
2. 跑分线程不占 UI；Stop 调用 `RequestStop()`。
3. 写出结果时 `workloadVersion` 带 `_android_preview`，`platform=Android`。
4. CPU 页选择测试模式后点 Run。缓存 / 内存带宽测试大约一分钟，手机会发热；
   存储测试会在 App 私有目录里写一个最大 1 GiB 的临时文件，测完删除。
   不需要安装 App 的另一条路：`scripts/build-android-cpumem.ps1` 编出的独立程序可以用 adb 直接运行。

## 用 adb 远程测试

debug 包可以不碰手机就开始一项测试（2026-10-06 加入；**只编译通过，没有在任何设备上试过**）。

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb logcat -c
adb shell am start -S -n com.mangekyo.benchmark/.MainActivity --es run cpu_membw
adb logcat -s Mangekyo MangekyoEngine
adb pull /sdcard/Android/data/com.mangekyo.benchmark/files/results/results.json
```

- **`--es run <测试>`：** `cpu_membw`（缓存 / 内存带宽）、`cpu_all`（单核 + 全核）、`cpu_per_core`、`cpu_multi`、
  `stream`、`gpu_burn`。App 打开后自动切到对应页面并开始。
- **`--ei seconds <N>`（可选）：** CPU 算力测试每项的秒数（默认 1），或 GPU 测试的时长（默认 3）。
- **logcat 里有什么：**
  - 标签 `MangekyoEngine`：引擎打印的每一行，包括进度、各级带宽和成绩；
  - 标签 `Mangekyo`：`AUTORUN accepted / started / refused / failed` 和结束时的 `RUN finished kind=cpu exit=0`
    或 `RUN finished kind=gpu score=...`，脚本可以等这一行。
- **只在 debug 包里生效。** 正式包会拒绝并打印 `AUTORUN refused ... reason=release_build`，否则手机上任何 App 都能用这个参数让它开跑。
- **GPU 测试需要屏幕亮着并解锁**，因为它要等预览画面创建出来（最多等 10 秒）。
- 没有模拟点击，也不需要知道按钮坐标。

## Windows 开发注意

- 勿写 `import androidx.compose.ui.modifier`（类/包大小写冲突）；用 `import androidx.compose.ui.*`
- 命名参数必须小写 `modifier =`

## 主要待实现

真机冒烟、GLES 3.1 / EGL 降级、温控 15s Burst 合同、液体、CPU 测试的取消、
独立 ES 2.0 legacy APK、抓帧策略。
