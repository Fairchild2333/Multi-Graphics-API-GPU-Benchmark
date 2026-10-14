package com.mangekyo.benchmark.core

/**
 * Android UI 展示的 workload 身份。
 *
 * 引擎侧 `AppBase::CollectResult()` 在 `__ANDROID__` 下会给桌面基线版本追加
 * `_android_preview`（并写入 `scoreContract=preview_not_desktop_15s`）。
 * 此处字符串必须与引擎写出一致，禁止再写假的 placeholder 名。
 *
 * 仅列出当前 JNI 宿主真正可启动的项。
 */
data class WorkloadInfo(
    val id: String,
    val nameEn: String,
    val nameZh: String,
    val workloadVersion: String,
    val isPreview: Boolean,
) {
    val displayName: String get() = tr(nameEn, nameZh)
}

object WorkloadRegistry {
    /**
     * The GPU tests, named as in the desktop app. The size of a test (particle
     * count, GPU Burn steps) and its duration are chosen on the page, see
     * [GpuOptions]; `workloadVersion` here is the name with the first choice.
     */
    val gpuWorkloads: List<WorkloadInfo> = listOf(
        WorkloadInfo(
            "stream",
            "Particle — Memory Throughput",
            "粒子 —— 内存吞吐",
            "stream_v1_android_preview",
            isPreview = true,
        ),
        WorkloadInfo(
            "gpu_burn",
            "Plasma x Kaleidoscope — GPU Burn",
            "等离子晶核 × 万花镜 —— GPU Burn",
            "gpu_burn_v3_fixed_steps_16_kaleidoscope_android_preview",
            isPreview = true,
        ),
        WorkloadInfo(
            "cinematic_liquid",
            "Fluid — Interactive Pool",
            "流体 —— 互动水池",
            "cinematic_liquid_v2_physical_scene_v8_android_preview",
            isPreview = true,
        ),
        WorkloadInfo(
            "membw",
            "Memory Bandwidth — VRAM / Cache",
            "显存带宽 —— 显存 / 缓存",
            "membw_v2_verified_android_preview",
            isPreview = true,
        ),
    )

    /** Tests of the GPU page that need no picture: they run through the engine's command-line entry. */
    val gpuCommandLineTests: Set<String> = setOf("membw", "stream_headless")

    /** CPU 页的两项测试，均通过引擎的命令行入口在本机线程上运行。 */
    val cpuWorkloads: List<WorkloadInfo> = listOf(
        WorkloadInfo(
            "cpu_mixed",
            "CPU compute",
            "CPU 算力",
            "cpu_mixed_v1",
            isPreview = true,
        ),
        WorkloadInfo(
            "cpu_membw",
            "Cache & memory bandwidth",
            "缓存与内存带宽",
            "cpu_membw_v1_android_preview",
            isPreview = true,
        ),
    )
}
