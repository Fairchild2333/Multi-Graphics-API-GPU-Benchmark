# Mangekyo Android（主包）

> 状态（2026-07-24）：**gpu_engine 已接入（Vulkan 垂直切片）** —
> `:app:assembleDebug` 通过；SurfaceView → ANativeWindow → `VulkanBackend::Run`；
> FAB Run/Stop（3s preview：`stream` / `gpu_burn`）。
> minSdk **23**（Compose）；原生链接 API **24** stub（`libvulkan`）。
> **正式成绩合同未齐**（`*_android_preview`）；GLES 3.1 未接；液体未开。
> 合同见 `HANDOFF.md` 目标 C 第 3 条。真机验收仍待做。

## 已完成

- Compose BOM `2026.06.01` / Material3；AGP **9.3.1** + Kotlin **2.3.21**；minSdk **23**
- Material You：API 31+ 动态取色，以下品牌静态色
- 四页 Material 3 壳 + SplashScreen
- `CapabilityGate`：Vulkan dlopen 探针 + HW feature + GL ES 版本
- `ResultsStore`：应用专属目录；引擎经 `GPU_BENCH_DATA_DIR` 写同一根
- **gpu_engine**：根 CMake `ANDROID` 分支（无 GLFW / 无 CLI）；`libmangekyo_jni` 链引擎 + NDK `vulkan`
- JNI：Surface 生命周期、后台线程 Run、`RequestStop` 协作取消
- SPIR-V：构建时 `glslc` → `assets/shaders/`（particle + compute + gpu_burn）

## 构建

```bash
cd android
# 需要 PATH 上有 glslc（Vulkan SDK），用于打包 SPIR-V
./gradlew :app:assembleDebug
```

产物：`app/build/outputs/apk/debug/app-debug.apk`。

## 运行说明

1. API 24+ 且 Vulkan loader 可用时，GPU 页可选 Stream / GPU Burn，点 Run（默认 3s）。
2. 跑分线程不占 UI；Stop 调用 `RequestStop()`。
3. 结果若写出，版本带 android preview 后缀，**不得与 Windows 榜混排**。

## Windows 开发注意

- 勿写 `import androidx.compose.ui.Modifier`（类/包大小写冲突）；用 `import androidx.compose.ui.*`
- 命名参数必须小写 `modifier =`

## 主要待实现

GLES 3.1 / EGL 降级、完整 `workloadVersion` 合同、温控 15s Burst、液体、
独立 ES 2.0 legacy APK、真机冒烟与抓帧。
