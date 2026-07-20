package com.mangekyo.benchmark.core

/**
 * Workload registry 占位。
 *
 * 合同（HANDOFF 交接规则 6）：CLI、WinUI、macOS GUI、Android 必须从**同一份**
 * 测试 metadata/registry 获取能力。此处硬编码仅为脚手架展示；
 * TODO(next-ai)：改为由 C++ registry（经 NativeBridge）或共享 manifest 生成，删除硬编码。
 *
 * Android 计时/抓帧模型与 Windows 不同 → 必须使用新的 android 后缀 workloadVersion
 * 独立成组，绝不与现有 Windows 成绩组混排。版本名以实际实现时的合同为准，下面是占位。
 */
data class WorkloadInfo(
    val id: String,
    val displayName: String,
    val workloadVersion: String,
    val isPreview: Boolean,
)

object WorkloadRegistry {
    val gpuWorkloads: List<WorkloadInfo> = listOf(
        WorkloadInfo("stream", "Stream / Particle", "stream_android_v1 (placeholder)", isPreview = true),
        WorkloadInfo("gpu_burn", "GPU Burn", "gpu_burn_android_v1 (placeholder)", isPreview = true),
        WorkloadInfo(
            "cinematic_liquid_v2", "Cinematic Liquid v2",
            "cinematic_liquid_v2_android_preview (placeholder)", isPreview = true,
        ),
    )

    val cpuWorkloads: List<WorkloadInfo> = listOf(
        WorkloadInfo("cpu_mixed", "CPU Mixed", "cpu_mixed_v1_android (placeholder)", isPreview = true),
    )
}
