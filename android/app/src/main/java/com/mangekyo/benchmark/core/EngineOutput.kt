package com.mangekyo.benchmark.core

import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlin.math.roundToLong

/**
 * Parsed output of the native engine. The engine prints tab-separated protocol
 * lines (CPU_*, CPUMEM_*) and periodic "[GPU Timing]" lines; the same lines
 * drive the Windows GUI, so the two front-ends show the same numbers.
 */

/** "TAG\tkey=value\tkey=value" -> {key: value}. */
fun protocolFields(line: String): Map<String, String> =
    line.split('\t').drop(1).mapNotNull { item ->
        val eq = item.indexOf('=')
        if (eq <= 0) null else item.substring(0, eq) to item.substring(eq + 1)
    }.toMap()

private fun Map<String, String>.number(key: String): Double = this[key]?.toDoubleOrNull() ?: 0.0
private fun Map<String, String>.flag(key: String): Boolean = number(key) > 0.5

/** The machine's own counts, which the engine names where the system gives an app fewer processors. */
private fun machineLabel(f: Map<String, String>): String? {
    val logical = f["system_logical_count"] ?: return null
    val physical = f["system_physical_count"] ?: return null
    return tr(
        "of the machine's $physical cores / $logical threads; the system keeps the rest from apps",
        "整机是 $physical 核 $logical 线程，其余的系统不给应用使用")
}

/** "48 KiB", "1.5 MiB", "1 GiB". */
fun formatBytes(bytes: Double): String {
    val kib = bytes / 1024.0
    if (kib < 1024.0) return "${kib.roundToLong()} KiB"
    val mib = kib / 1024.0
    if (mib >= 1024.0 && mib % 1024.0 < 0.01) return "${(mib / 1024.0).roundToLong()} GiB"
    return if (mib % 1.0 < 0.01) "${mib.roundToLong()} MiB" else String.format("%.1f MiB", mib)
}

/** "1,863", "74.7", "0.024": three significant digits. */
fun formatNumber(value: Double): String = when {
    value >= 1000.0 -> String.format("%,d", value.roundToLong())
    value >= 100.0 -> String.format("%.0f", value)
    value >= 10.0 -> String.format("%.1f", value)
    value >= 1.0 -> String.format("%.2f", value)
    value > 0.0 -> String.format("%.3f", value)
    else -> "0"
}

/** Axis tick: "0.5", "2", "200", "1,000". */
fun formatTick(value: Double): String = when {
    !value.isFinite() -> "-"
    value >= 1000.0 -> String.format("%,d", value.roundToLong())
    value == 0.0 -> "0"
    else -> java.math.BigDecimal(value).round(java.math.MathContext(3)).stripTrailingZeros().toPlainString()
}

data class CurvePoint(val series: String, val x: Double, val y: Double)

data class MemLevel(
    val mode: String,
    val level: String,
    val cacheBytes: Double,
    val read: Double,
    val write: Double,
    val copy: Double,
    val writeStream: Double,
    val copyStream: Double,
    val zeroFill: Double,
    val measured: Boolean,
    val settled: Boolean,
    val fromCurve: Boolean,
    val capacityLow: Double,
    val capacityHigh: Double,
)

data class MemCache(val cpuClass: Int, val level: Int, val sizeBytes: Double, val sharedCores: Int)

data class MemClass(val rank: Int, val mode: String, val name: String, val cores: Int)

data class StorageResult(
    val measured: Boolean,
    val directIo: Boolean,
    val backing: String,
    val skipped: String,
    val seqRead: Double,
    val seqWrite: Double,
    val randRead: Double,
    val randWrite: Double,
    val randReadIops: Double,
    val randWriteIops: Double,
)

data class CoreScore(val core: Int, val score: Double, val unit: String, val label: String)

data class GpuSample(val seconds: Double, val computeMs: Double, val renderMs: Double, val warmup: Boolean)

/** State of the CPU page. It outlives the screen, so a run survives navigation. */
object CpuRun {
    var running by mutableStateOf(false)
    var memoryTest by mutableStateOf(false)
    var finished by mutableStateOf(false)
    var failed by mutableStateOf(false)
    var progress by mutableStateOf(0f)
    var status by mutableStateOf("")
    var cpuLabel by mutableStateOf("")

    // Compute test.
    val coreScores = mutableStateListOf<CoreScore>()
    var singleSummary by mutableStateOf("")
    var multiSummary by mutableStateOf("")
    private val coreLabels = HashMap<Int, String>()

    private fun defaultCoreLabel(core: Int): String =
        tr("logical processor ${core + 1}", "逻辑处理器 ${core + 1}")

    // Cache and memory sweep.
    val classes = mutableStateListOf<MemClass>()
    val caches = mutableStateListOf<MemCache>()
    val levels = mutableStateListOf<MemLevel>()
    val points = mutableStateListOf<CurvePoint>()
    var allThreads by mutableStateOf(0)
    var pageMapping by mutableStateOf("")
    var storage by mutableStateOf<StorageResult?>(null)

    /** Tail of the raw output. */
    val log = mutableStateListOf<String>()

    fun begin(memory: Boolean, preserveCompute: Boolean = false) {
        running = true
        memoryTest = memory
        finished = false
        failed = false
        progress = 0f
        status = tr("Starting", "正在启动")
        if (!preserveCompute) {
            coreScores.clear()
            coreLabels.clear()
            singleSummary = ""
            multiSummary = ""
        }
        classes.clear()
        caches.clear()
        levels.clear()
        points.clear()
        allThreads = 0
        pageMapping = ""
        storage = null
        log.clear()
    }

    fun finish(exitCode: Int) {
        running = false
        finished = true
        failed = exitCode != 0 && exitCode != 130
        if (exitCode == 0) progress = 1f
        status = if (exitCode == 130) tr("Cancelled", "已取消") else if (failed) {
            tr("Failed (exit $exitCode); see the output below", "失败（退出码 $exitCode），详见下方的输出")
        } else {
            tr("Done", "完成")
        }
    }

    fun consume(line: String) {
        log.add(line)
        if (log.size > 400) log.removeRange(0, log.size - 300)
        val tag = line.substringBefore('\t')
        if (!tag.startsWith("CPU")) return
        val f = protocolFields(line)
        when (tag) {
            "CPU_META" -> cpuLabel = listOfNotNull(
                f["name"], f["logical_count"]?.let { tr("$it logical", "$it 个逻辑处理器") },
                f["physical_count"]?.let { tr("$it physical", "$it 个物理核心") }, machineLabel(f),
            ).joinToString(" · ")
            "CPU_TOPOLOGY" -> {
                val core = f["core_index"]?.toIntOrNull() ?: return
                coreLabels[core] = "LP ${f["logical_index"] ?: core} · core ${(f["physical_core"]?.toIntOrNull() ?: core) + 1}" +
                    (f["core_class"]?.let { " · $it" } ?: "")
            }
            "CPU_PROGRESS" -> {
                progress = (f["overall_fraction"] ?: f["fraction"])?.toFloatOrNull()?.coerceIn(0f, 1f) ?: progress
                val core = f["core_index"]?.toIntOrNull() ?: -1
                val phase = when (f["phase"]) {
                    "warmup" -> tr("warming up", "预热")
                    "measure" -> tr("measuring", "测量")
                    else -> tr("finishing", "收尾")
                }
                status = if (f["mode"] == "multi" || core < 0) {
                    tr("All cores", "全核") + " · $phase"
                } else {
                    "${coreLabels[core] ?: defaultCoreLabel(core)} · $phase"
                }
            }
            "CPU_ERROR" -> status =
                f["message"]?.replace('_', ' ') ?: tr("CPU engine error", "CPU 引擎出错")
            "CPU_RESULT" -> when (f["kind"]) {
                "core" -> {
                    val core = f["core_index"]?.toIntOrNull() ?: return
                    coreScores.add(CoreScore(core, f.number("score"), f["unit"].orEmpty(),
                        coreLabels[core] ?: defaultCoreLabel(core)))
                }
                "summary" -> singleSummary =
                    "${formatNumber(f["average_score"]?.toDoubleOrNull() ?: f.number("score"))} ${f["unit"].orEmpty()}"
                "multi" -> multiSummary = "${formatNumber(f.number("score"))} ${f["unit"].orEmpty()}" +
                    (f["thread_count"]?.let { tr(" ($it threads)", "（$it 线程）") } ?: "")
            }
            "CPUMEM_META" -> {
                memoryTest = true
                allThreads = f.number("all_threads").toInt()
                cpuLabel = listOfNotNull(
                    f["name"], f["logical_count"]?.let { tr("$it logical", "$it 个逻辑处理器") },
                    f["physical_count"]?.let { tr("$it physical", "$it 个物理核心") }, machineLabel(f), f["vector"],
                ).joinToString(" · ")
            }
            "CPUMEM_CLASS" -> classes.add(
                MemClass(f.number("rank").toInt(), f["mode"].orEmpty(), f["name"].orEmpty(), f.number("cores").toInt()))
            "CPUMEM_CACHE" -> caches.add(
                MemCache(f.number("class").toInt(), f.number("level").toInt(), f.number("size_bytes"),
                    f.number("shared_cores").toInt().coerceAtLeast(1)))
            "CPUMEM_POINT" -> {
                val x = f.number("block_bytes")
                val y = f.number("median_gbps")
                if (x > 0.0 && y > 0.0) points.add(CurvePoint("${f["mode"]}/${f["kernel"]}", x, y))
            }
            "CPUMEM_PROGRESS" -> {
                progress = f["fraction"]?.toFloatOrNull()?.coerceIn(0f, 1f) ?: progress
                val mode = f["mode"].orEmpty()
                status = when {
                    mode.startsWith("single") -> tr("One thread · sweeping data-set sizes", "单线程 · 扫描数据量")
                    mode == "all" -> tr("All cores · sweeping data-set sizes", "全核 · 扫描数据量")
                    mode == "storage" -> tr("Storage · reading and writing a test file", "存储 · 读写测试文件")
                    else -> tr("Virtual memory · mapping pages", "虚拟内存 · 映射页面")
                }
            }
            "CPUMEM_LEVEL" -> levels.add(
                MemLevel(
                    mode = f["mode"].orEmpty(), level = f["level"].orEmpty(),
                    cacheBytes = f.number("cache_bytes"),
                    read = f.number("read_gbps"), write = f.number("write_gbps"), copy = f.number("copy_gbps"),
                    writeStream = f.number("write_nt_gbps"), copyStream = f.number("copy_nt_gbps"),
                    zeroFill = f.number("zero_gbps"),
                    measured = f.flag("measured"), settled = f.flag("settled"), fromCurve = f.flag("from_curve"),
                    capacityLow = f.number("capacity_low_bytes"), capacityHigh = f.number("capacity_high_bytes"),
                ),
            )
            "CPUMEM_PAGEFAULT" -> if (f.flag("measured")) {
                pageMapping = "${formatNumber(f.number("gbps"))} GB/s · ${f.number("ns_per_page").roundToLong()} ns / " +
                    formatBytes(f.number("page_bytes")) + tr(" page", " 页")
            }
            "CPUMEM_STORAGE" -> storage = StorageResult(
                measured = f.flag("measured"), directIo = f.flag("direct_io"),
                backing = f["backing"].orEmpty(), skipped = f["skipped"].orEmpty(),
                seqRead = f.number("seq_read_mbps"), seqWrite = f.number("seq_write_mbps"),
                randRead = f.number("rand_read_mbps"), randWrite = f.number("rand_write_mbps"),
                randReadIops = f.number("rand_read_iops"), randWriteIops = f.number("rand_write_iops"),
            )
        }
    }
}

/** State of the last GPU run: its timing samples and the score lines of the summary. */
/** One measurement of the VRAM / cache bandwidth sweep. */
data class MemoryPoint(val kernel: String, val bytes: Double, val gbps: Double)

/** What one kernel of that sweep reached in the cache and in memory, GB/s. */
data class MemoryLevelLine(val kernel: String, val cacheGbps: Double, val memoryGbps: Double)

object GpuRun {
    var active by mutableStateOf(false)

    /** A test has the whole screen: the shell hides its navigation and the GPU page shows only the picture. */
    var fullscreen by mutableStateOf(false)
    val samples = mutableStateListOf<GpuSample>()
    var scoreLine by mutableStateOf("")
    var fpsLine by mutableStateOf("")

    /** The frame rate was held at the display's refresh rate, so the score may be too low. */
    var displayLimited by mutableStateOf(false)

    /**
     * The size of the compute work groups when the particle test did not run in
     * its own (256): the driver allows no more than this. 0 otherwise. Such a
     * result is in a score group of its own.
     */
    var workGroupSize by mutableStateOf(0)

    /**
     * The VRAM / cache bandwidth sweep: one point per kernel ("read", "write",
     * "copy") and data-set size, and for every kernel its cache and memory level.
     */
    val memoryPoints = mutableStateListOf<MemoryPoint>()
    val memoryLevels = mutableStateListOf<MemoryLevelLine>()

    private val rateKeys = listOf(
        "RAM rate:", "VRAM rate:", "Memory rate:", "Compute rate:", "Burn rate:", "Stress rate:",
        "Fill rate:", "Render rate:", "Vol rate:", "Fluid rate:", "Liquid rate:",
    )

    fun begin() {
        active = true
        samples.clear()
        scoreLine = ""
        fpsLine = ""
        displayLimited = false
        workGroupSize = 0
        memoryPoints.clear()
        memoryLevels.clear()
    }

    private fun field(line: String, key: String): String? =
        line.split('\t').firstOrNull { it.startsWith("$key=") }?.substring(key.length + 1)

    /** The "Avg FPS" of the run, or null before it is known. */
    val averageFps: Double? get() = numberAfter(fpsLine, "Avg FPS:")

    private fun numberAfter(line: String, key: String): Double? {
        val at = line.indexOf(key)
        if (at < 0) return null
        return line.substring(at + key.length).trimStart().takeWhile { it.isDigit() || it == '.' || it == '-' }
            .toDoubleOrNull()
    }

    fun consume(line: String) {
        if (line.startsWith("[GPU Timing] Compute:") || line.startsWith("[Timing] Compute:")) {
            samples.add(
                GpuSample(
                    seconds = numberAfter(line, "| T:") ?: (samples.size + 1).toDouble(),
                    computeMs = numberAfter(line, "Compute:") ?: 0.0,
                    renderMs = numberAfter(line, "Render:") ?: 0.0,
                    warmup = line.contains("(warm-up)"),
                ),
            )
            return
        }
        if (line.startsWith("MEMBW_POINT")) {
            val kernel = field(line, "kernel") ?: return
            val bytes = field(line, "dataset_bytes")?.toDoubleOrNull() ?: return
            val rate = field(line, "median_gbps")?.toDoubleOrNull() ?: return
            memoryPoints.add(MemoryPoint(kernel, bytes, rate))
            return
        }
        if (line.startsWith("MEMBW_SUMMARY")) {
            val kernel = field(line, "kernel") ?: return
            memoryLevels.add(
                MemoryLevelLine(
                    kernel,
                    field(line, "cache_gbps")?.toDoubleOrNull() ?: 0.0,
                    field(line, "memory_gbps")?.toDoubleOrNull() ?: 0.0,
                ),
            )
            // The score of this test is how fast the GPU reads its memory.
            if (kernel == "read" && scoreLine.isEmpty()) {
                scoreLine = "Memory rate: " + (field(line, "memory_gbps") ?: "0") + " GB/s"
            }
            return
        }
        val trimmed = line.trim()
        // The engine's own verdict: frames had to wait for the display.
        if (trimmed.startsWith(">> Display-bound")) displayLimited = true
        // The engine ran the particle test in smaller work groups than its own.
        if (trimmed.startsWith(">> Work groups of ")) {
            workGroupSize = trimmed.removePrefix(">> Work groups of ").takeWhile { it.isDigit() }.toIntOrNull() ?: 0
        }
        if (scoreLine.isEmpty() && rateKeys.any { trimmed.startsWith(it) }) {
            scoreLine = trimmed.replace(Regex(" {2,}"), " ")
        } else if (fpsLine.isEmpty() && trimmed.startsWith("Avg FPS:")) {
            fpsLine = trimmed.replace(Regex(" {2,}"), " ")
        }
    }
}
