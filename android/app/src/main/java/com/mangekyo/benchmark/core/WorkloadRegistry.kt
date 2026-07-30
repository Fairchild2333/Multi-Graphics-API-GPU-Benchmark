package com.mangekyo.benchmark.core

/**
 * Android UI 展示的 workload 身份。
 *
 * 引擎侧 `AppBase::CollectResult()` 在 `__ANDROID__` 下会给桌面基线版本追加
 * `_android_preview`（并写入 `scoreContract=preview_not_desktop_15s`）。
 * 此处字符串必须与引擎写出一致，禁止再写假的 placeholder 名。
 *
 * 仅列出当前 JNI 宿主真正可启动的项（stream / gpu_burn light=16）。
 * 液体 / CPU mixed 未接线，不出现在可跑列表。
 */
data class WorkloadInfo(
    val id: String,
    val displayName: String,
    val workloadVersion: String,
    val isPreview: Boolean,
)

object WorkloadRegistry {
    val gpuWorkloads: List<WorkloadInfo> = listOf(
        WorkloadInfo(
            "stream",
            "Stream / Particle",
            "stream_v1_android_preview",
            isPreview = true,
        ),
        WorkloadInfo(
            "gpu_burn",
            "GPU Burn (light 16)",
            "gpu_burn_v3_fixed_steps_16_kaleidoscope_android_preview",
            isPreview = true,
        ),
    )

    /** CPU 页仍可展示合同名；宿主尚未接线，不可当作已实现。 */
    val cpuWorkloads: List<WorkloadInfo> = listOf(
        WorkloadInfo(
            "cpu_mixed",
            "CPU Mixed (not wired)",
            "cpu_mixed — Android host not wired",
            isPreview = true,
        ),
    )
}
