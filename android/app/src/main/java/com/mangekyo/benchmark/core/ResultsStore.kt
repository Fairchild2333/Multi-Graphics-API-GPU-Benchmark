package com.mangekyo.benchmark.core

import android.content.Context
import org.json.JSONArray
import java.io.File

/**
 * Results live under app-specific storage (no storage permission), matching
 * Windows %LOCALAPPDATA%/GpuComputeBenchmark semantics.
 *
 * The engine is the only writer of results.json (src/benchmark_results.cpp);
 * this class reads it and can empty it. It reads every member as optional, so
 * files of older builds (members missing) and of newer builds (members it does
 * not know) both load.
 */
data class HistoryEntry(
    val id: String,
    val workload: String,
    val workloadVersion: String,
    val graphicsApi: String,
    val deviceName: String,
    val score: Double?,
    val scoreUnit: String?,
    val timestamp: String?,
    val abi: String?,
    /** Make and model of the device, its chip, system release and memory; empty or 0 in older results. */
    val deviceModel: String = "",
    val cpuName: String = "",
    val osVersion: String = "",
    val systemMemoryMB: Long = 0,
    val particleCount: Int = 0, val headless: Boolean = false, val durationSec: Double = 0.0,
    val workloadConfig: String = "", val avgComputeMs: Double = 0.0, val avgRenderMs: Double = 0.0,
    val avgTotalGpuMs: Double = 0.0, val avgFrameTimeMs: Double = 0.0,
    val stableScore: Double = 0.0, val throughputDropPct: Double = 0.0,
) {
    val measurementReviewReasons: List<String> get() = buildList {
        if (workload == "membw" && graphicsApi == "Vulkan" && workloadVersion.startsWith("membw_v3_streaming")) {
            if ("memoryPlateauReached=1" !in workloadConfig.split(';'))
                add(tr("Memory plateau was not reached", "尚未达到内存带宽平台"))
            return@buildList
        }
        if (workload !in listOf("stream", "gpu_burn", "cinematic_liquid")) return@buildList
        if (avgFrameTimeMs > 0 && avgTotalGpuMs > avgFrameTimeMs * 1.1) add(tr("GPU timing exceeds frame interval", "GPU 计时超过帧间隔"))
        if (avgComputeMs + avgRenderMs > avgTotalGpuMs + 0.1) add(tr("Compute/render timing overlaps", "计算与渲染计时重叠"))
        if (headless && graphicsApi == "Vulkan" && particleCount >= 1048576 && avgComputeMs < avgFrameTimeMs * 0.5)
            add(tr("Compute timing needs verification", "计算计时需要核验"))
        if (workload != "cinematic_liquid") {
            if (throughputDropPct > 5) add(tr("Throughput dropped by over 5%", "吞吐下降超过 5%"))
            if ((workload == "gpu_burn" || particleCount >= 1048576) && stableScore <= 0)
                add(tr("Stable throughput not reached", "尚未达到稳定吞吐"))
        }
    }
    fun matches(job: SuiteJob): Boolean {
        if (score == null || !score.isFinite() || score <= 0) return false
        if (job.cpu) return if (job.workload == "cpu_all") workload in listOf("cpu_single_core", "cpu_multi_core") else workload == "cpu_membw"
        val apiMatches = when (job.api) { "gles2" -> graphicsApi == "OpenGL ES 2.0"; "gles" -> graphicsApi == "OpenGL ES"; else -> graphicsApi == "Vulkan" }
        if (!apiMatches || workload != job.workload || headless != job.headless) return false
        if (job.workload == "membw") {
            val fields = workloadConfig.split(';').mapNotNull {
                val parts = it.split('=', limit = 2)
                if (parts.size == 2) parts[0] to parts[1] else null
            }.toMap()
            val v3 = workloadVersion.startsWith("membw_v3_streaming_") && fields["dataVerified"] == "1" &&
                fields["largeDatasetTraversal"] == "ordered_full_dataset_passes" &&
                (fields["writeCopyChecks"]?.toIntOrNull() ?: 0) > 0 && fields["timestampAnomalies"] == "0"
            // A V1 result with a plateau, saved since the data check ran (first on the MIX 4,
            // 20261008-042956), measured the same as V3 (within 0.2 - 3 %); see
            // LegacyV1VulkanMemoryComparable in src/result_quality.h.
            val v1 = workloadVersion == "membw_v1_android_preview" && id >= "20261008-042956" &&
                fields["memoryPlateauReached"] == "1" && fields["timestampAnomalies"] == "0"
            return v3 || v1
        }
        if (durationSec < 11) return false
        if (job.particles > 0 && particleCount != job.particles) return false
        return job.steps == 0 || workloadConfig.split(';').any { it == "steps=${job.steps}" } ||
            workloadVersion.contains("fixed_steps_${job.steps}_")
    }
    /** "Xiaomi 23049PCD8G · QTI SM8550 · Android 14 (API 34) · 11.2 GB RAM", from what the result has. */
    val machineLine: String
        get() = listOf(
            deviceModel,
            // A CPU result already shows the chip as its device name.
            cpuName.takeIf { it != deviceName && !it.startsWith("Unknown") }.orEmpty(),
            osVersion,
            if (systemMemoryMB > 0) String.format("%.1f GB RAM", systemMemoryMB / 1024.0) else "",
        ).filter { it.isNotBlank() }.joinToString(" · ")
}

class ResultsStore(private val context: Context) {
    val resultsFile: File
        get() = File(context.getExternalFilesDir(null) ?: context.filesDir, "results/results.json")

    fun ensureDir() {
        resultsFile.parentFile?.mkdirs()
    }

    fun loadEntries(): List<HistoryEntry> {
        ensureDir()
        if (!resultsFile.exists()) return emptyList()
        return try {
            val text = resultsFile.readText()
            if (text.isBlank()) return emptyList()
            val root = JSONArray(text)
            buildList {
                for (i in 0 until root.length()) {
                    val o = root.optJSONObject(i) ?: continue
                    add(
                        HistoryEntry(
                            id = o.optString("id"),
                            workload = o.optString("workload"),
                            workloadVersion = o.optString("workloadVersion"),
                            graphicsApi = o.optString("graphicsApi", o.optString("api")),
                            deviceName = o.optString("deviceName", o.optString("gpuName")),
                            score = o.optDouble("score").takeIf { o.has("score") && !o.isNull("score") },
                            scoreUnit = o.optString("scoreUnit").ifBlank { null },
                            timestamp = o.optString("timestamp").ifBlank { null },
                            // The engine saves the architecture of the process, not an ABI name.
                            abi = o.optString("abi").ifBlank {
                                o.optString("processArchitecture").takeIf { it.isNotBlank() && it != "Unknown" }
                            },
                            deviceModel = o.optString("deviceModel"),
                            cpuName = o.optString("cpuName"),
                            osVersion = o.optString("osVersion"),
                            systemMemoryMB = o.optLong("systemMemoryMB", 0),
                            particleCount = o.optInt("particleCount"), headless = o.optBoolean("headless"), durationSec = o.optDouble("durationSec", 0.0),
                            workloadConfig = o.optString("workloadConfig"), avgComputeMs = o.optDouble("avgComputeMs", 0.0),
                            avgRenderMs = o.optDouble("avgRenderMs", 0.0), avgTotalGpuMs = o.optDouble("avgTotalGpuMs", 0.0),
                            avgFrameTimeMs = o.optDouble("avgFrameTimeMs", 0.0), stableScore = o.optDouble("stableScore", 0.0),
                            throughputDropPct = o.optDouble("throttlePct", 0.0),
                        ),
                    )
                }
            }.sortedByDescending { it.timestamp.orEmpty() }  // newest first, as on the other platforms
        } catch (_: Exception) {
            emptyList()
        }
    }

    fun clearAll() {
        ensureDir()
        resultsFile.writeText("[]\n")
    }
}
