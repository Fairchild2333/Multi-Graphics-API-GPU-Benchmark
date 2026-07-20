# Mangekyo Android（主包脚手架）

> 状态（2026-07-20）：**代码已写、未编译、未真机验证**。这是给后续 AI 的前端骨架，
> 合同与基调以根目录 `HANDOFF.md` 目标 C 第 3 条为准，先读它再动手。

## 接手第一步（按顺序）

1. 更新 `gradle/libs.versions.toml` 全部占位版本为当时最新稳定版。
2. 检查该版 Compose 的 minSdk 要求：若要求 23，把 `app/build.gradle.kts` 的 `minSdk` 提到 23
   （主包最老目标设备 Tegra K1 = API 24，零损失）；否则保持 21。
3. 生成 Gradle wrapper（本脚手架未含 wrapper 二进制）：`gradle wrapper`。
4. 首次构建 + 模拟器跑通四页导航，这时才允许把 TODO/HANDOFF 里的"未编译"改状态。
5. 补应用图标（当前 Manifest 未设 icon，使用系统默认）。

## 已锁定合同（勿改，改前问用户）

- minSdk 21（或按上述规则 23）/ targetSdk 最新；NDK r27+，`.so` 16 KB 页对齐（CMake 已加 max-page-size）。
- ABI：armeabi-v7a / arm64-v8a / x86 / x86_64 全原生编译；结果 metadata 记录真实 ABI，不混排。
- UI：单套 Compose + Material 3 能力递减；动态取色仅 12+，以下回退 `ui/theme/Color.kt` 品牌静态色；
  信息架构 GPU/CPU/History/Charts 对齐 WinUI/SwiftUI。
- Vulkan 运行时门控（API≥24 + dlopen），失败走 GL ES 3.1；能力不齐显式 unsupported，不静默 fallback。
- Android 计时/抓帧模型不同 → 新 workloadVersion 独立成组，绝不与 Windows 成绩混排。
- 满载时 Stop 必须可用；跑分不占 UI 线程。
- 结果写应用专属目录，schema 与 Windows results.json 同源。
- Tegra 3/4（ES 2.0）不在本工程：独立 legacy APK（armeabi-v7a only），见 HANDOFF。

## 目录

```
app/src/main/java/com/mangekyo/benchmark/
  MainActivity.kt            入口，edge-to-edge + 主题
  ui/AppRoot.kt              底部导航 + NavHost（四页）
  ui/theme/                  Material 3 主题：动态取色/静态回退
  ui/screens/                GpuScreen / CpuScreen / HistoryScreen / ChartsScreen（均含 TODO 注释）
  ui/components/BenchmarkSurface.kt  SurfaceView 占位 → NativeBridge
  core/NativeBridge.kt       JNI 桥（stub）
  core/CapabilityGate.kt     Vulkan/ES 能力门控（stub）
  core/WorkloadRegistry.kt   registry 占位——最终必须与 C++ registry 同源，删除硬编码
  core/ResultsStore.kt       results.json 占位
app/src/main/cpp/            JNI stub + CMake（含 16 KB 对齐；引擎接入 TODO）
```

## 主要待实现（详细 TODO 在各文件头部注释）

引擎接入（gpu_engine 进 CMake、Surface→ANativeWindow、EGL/VkAndroidSurfaceKHR）、
workload 启停与进度回调、ES 3.1 后端与 timer query 探测、结果读写与 History/Charts、
CPU strict affinity（回读验证）、温控评估与 15s Burst 语义记录。
