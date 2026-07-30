# Mangekyo Android（主包）

> 状态（2026-07-24，事实分层）：
> - **代码已写**：`android/` Compose 壳 + `libmangekyo_jni` → 根 `gpu_engine`（Vulkan / `ANativeWindow`）。
> - **可编译（本机 Windows）**：`:app:assembleDebug` 已通过；产物约 24MB debug APK。
> - **未真机运行**：未在物理 Android 设备上冒烟。
> - **成绩身份（引擎真实写出）**：`CollectResult` 在 `__ANDROID__` 下追加 `_android_preview`
>   （例：`stream_v1_android_preview`、
>   `gpu_burn_v3_fixed_steps_16_kaleidoscope_android_preview`）。
>   默认时长为宿主 3s preview，**不是**桌面 15s 正式合同；禁止与 Windows 榜混排。
> - **未做**：GLES 3.1 降级、液体、CPU mixed 宿主、温控 15s Burst、正式移动 duration 合同、发布包。

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

## 构建

```bash
cd android
# 需要 PATH 上有 glslc（Vulkan SDK），用于打包 SPIR-V
./gradlew :app:assembleDebug
```

产物：`app/build/outputs/apk/debug/app-debug.apk`。

## 运行说明

1. API 24+ 且 Vulkan loader 可用时，GPU 页可选 Stream / GPU Burn（light steps=16），点 Run（默认 3s）。
2. 跑分线程不占 UI；Stop 调用 `RequestStop()`。
3. 写出结果时 `workloadVersion` 带 `_android_preview`，`platform=Android`。

## Windows 开发注意

- 勿写 `import androidx.compose.ui.modifier`（类/包大小写冲突）；用 `import androidx.compose.ui.*`
- 命名参数必须小写 `modifier =`

## 主要待实现

真机冒烟、GLES 3.1 / EGL 降级、温控 15s Burst 合同、液体、CPU 宿主、
独立 ES 2.0 legacy APK、抓帧策略。
