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
) {
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
                        ),
                    )
                }
            }
        } catch (_: Exception) {
            emptyList()
        }
    }

    fun clearAll() {
        ensureDir()
        resultsFile.writeText("[]\n")
    }
}
