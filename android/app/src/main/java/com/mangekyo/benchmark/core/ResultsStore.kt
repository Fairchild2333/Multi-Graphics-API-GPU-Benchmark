package com.mangekyo.benchmark.core

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Results live under app-specific storage (no storage permission), matching
 * Windows %LOCALAPPDATA%/GpuComputeBenchmark semantics.
 *
 * Schema must stay identical to desktop results.json; Android only adds
 * metadata fields already allowed by the contract (ABI / OS / SoC).
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
)

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
                            abi = o.optString("abi").ifBlank {
                                o.optJSONObject("workloadConfig")?.optString("abi")
                            },
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

    /** Debug / future engine hook: append one JSON object (must already be contract-shaped). */
    fun appendRawObject(obj: JSONObject) {
        ensureDir()
        val arr = if (resultsFile.exists() && resultsFile.length() > 0) {
            try {
                JSONArray(resultsFile.readText())
            } catch (_: Exception) {
                JSONArray()
            }
        } else {
            JSONArray()
        }
        arr.put(obj)
        resultsFile.writeText(arr.toString(2) + "\n")
    }
}
