package com.mangekyo.benchmark.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.mangekyo.benchmark.core.AutoRun
import com.mangekyo.benchmark.core.CpuRun
import com.mangekyo.benchmark.core.DeviceInfo
import com.mangekyo.benchmark.core.MemClass
import com.mangekyo.benchmark.core.MemLevel
import com.mangekyo.benchmark.core.NativeBridge
import com.mangekyo.benchmark.core.formatBytes
import com.mangekyo.benchmark.core.formatNumber
import com.mangekyo.benchmark.core.localizeEngineText
import com.mangekyo.benchmark.core.tr
import com.mangekyo.benchmark.ui.components.KeepScreenOn
import com.mangekyo.benchmark.ui.AdaptivePanes
import com.mangekyo.benchmark.ui.components.BarRows
import com.mangekyo.benchmark.ui.components.ChartMarker
import com.mangekyo.benchmark.ui.components.ChartSeries
import com.mangekyo.benchmark.ui.components.ColumnChart
import com.mangekyo.benchmark.ui.components.LineChart
import com.mangekyo.benchmark.ui.components.seriesColors
import kotlinx.coroutines.delay

/**
 * CPU page: the compute test (per logical processor and all cores) and the
 * cache / memory sweep. Both run in the native engine on a worker thread; this
 * page collects the lines it prints and shows the results and their charts.
 */
@OptIn(ExperimentalMaterial3Api::class, ExperimentalLayoutApi::class)
@Composable
fun CpuScreen() {
    val context = LocalContext.current
    remember(context) { NativeBridge.init(context) }
    var mode by rememberSaveable { mutableStateOf("all") }
    var duration by rememberSaveable { mutableStateOf("formal") }
    val scrollBehavior = TopAppBarDefaults.pinnedScrollBehavior()
    val cores = remember { Runtime.getRuntime().availableProcessors() }
    val run = CpuRun
    // A CPU test takes minutes; the screen must not turn off in between.
    KeepScreenOn(run.running || com.mangekyo.benchmark.core.MobileSuite.running)

    // Collect the engine output while this page owns the run.
    LaunchedEffect(Unit) {
        while (true) {
            if (NativeBridge.outputOwner == "cpu") {
                NativeBridge.pollOutput().forEach(run::consume)
                if (run.running && !NativeBridge.isCliRunning()) {
                    NativeBridge.pollOutput().forEach(run::consume)
                    val exitCode = NativeBridge.cliExitCode()
                    run.finish(exitCode)
                    AutoRun.log("RUN finished kind=cpu exit=$exitCode")
                }
            }
            delay(150)
        }
    }
    val canRun = NativeBridge.isEngineLoaded && !run.running && !com.mangekyo.benchmark.core.MobileSuite.running &&
        !NativeBridge.isRunning() && !NativeBridge.isCliRunning()

    fun start(testMode: String, seconds: Int) {
        val memory = testMode == "memory"
        val args = if (memory) {
            // Internal storage: the storage test needs unbuffered I/O.
            arrayOf("--cpu-membw", "--cpu-membw-storage-dir", context.filesDir.absolutePath)
        } else {
            arrayOf(
                "--cpu-benchmark", testMode,
                "--cpu-time", seconds.toString(),
                "--cpu-warmup", "0.2",
            )
        }
        NativeBridge.pollOutput()  // drop lines left over from an earlier run
        NativeBridge.outputOwner = "cpu"
        run.begin(memory)
        if (!NativeBridge.startCli(args)) {
            run.finish(-1)
            run.status = NativeBridge.lastError().ifEmpty { tr("Start failed", "启动失败") }
        }
    }

    // A test requested over adb, see AutoRun.
    val request = AutoRun.pending
    LaunchedEffect(request) {
        val wanted = request ?: return@LaunchedEffect
        val wantedMode = AutoRun.cpuModes[wanted.test] ?: return@LaunchedEffect
        AutoRun.pending = null
        if (wantedMode == "suite") {
            com.mangekyo.benchmark.core.MobileSuite.start(context, com.mangekyo.benchmark.core.SuiteScope.CPU)
            return@LaunchedEffect
        }
        if (!NativeBridge.isEngineLoaded || run.running) {
            AutoRun.log("AUTORUN refused test=${wanted.test} reason=busy_or_engine_missing")
            return@LaunchedEffect
        }
        mode = wantedMode
        start(wantedMode, if (wanted.seconds > 0) wanted.seconds else 1)
        AutoRun.log("AUTORUN started test=${wanted.test}")
    }

    Scaffold(
        modifier = Modifier.nestedScroll(scrollBehavior.nestedScrollConnection),
        topBar = {
            TopAppBar(
                title = { Text(tr("CPU Benchmark", "CPU 测试")) },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surfaceContainer,
                ),
                scrollBehavior = scrollBehavior,
            )
        },
        floatingActionButton = {
            ExtendedFloatingActionButton(
                onClick = {
                    if (com.mangekyo.benchmark.core.MobileSuite.running) {
                        com.mangekyo.benchmark.core.MobileSuite.cancel()
                        return@ExtendedFloatingActionButton
                    }
                    if (run.running) { NativeBridge.stopWorkload(); return@ExtendedFloatingActionButton }
                    if (!canRun) return@ExtendedFloatingActionButton
                    start(mode, if (duration == "formal") 15 else 1)
                },
                icon = { Icon(if (run.running) Icons.Filled.Stop else Icons.Filled.PlayArrow, contentDescription = null) },
                text = { Text(if (run.running) tr("Stop", "停止") else tr("Run selected test", "运行所选测试")) },
                containerColor = if (canRun) {
                    MaterialTheme.colorScheme.primaryContainer
                } else {
                    MaterialTheme.colorScheme.surfaceContainerHighest
                },
                contentColor = if (canRun) {
                    MaterialTheme.colorScheme.onPrimaryContainer
                } else {
                    MaterialTheme.colorScheme.onSurfaceVariant
                },
            )
        },
    ) { inner ->
        AdaptivePanes(
            modifier = Modifier.padding(inner),
            primary = {
                com.mangekyo.benchmark.ui.components.SuiteControls(gpu = false)
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.primaryContainer,
                    ),
                ) {
                    Column(
                        modifier = Modifier.padding(14.dp),
                        verticalArrangement = Arrangement.spacedBy(4.dp),
                    ) {
                        Text(
                            text = if (mode == "memory") {
                                tr("Cache & memory bandwidth", "缓存与内存带宽")
                            } else {
                                tr("CPU compute throughput", "CPU 计算吞吐")
                            },
                            style = MaterialTheme.typography.titleMedium,
                            color = MaterialTheme.colorScheme.onPrimaryContainer,
                        )
                        Text(
                            text = run.cpuLabel.ifEmpty {
                                DeviceInfo.socLabel() + " · " + tr("$cores logical", "$cores 个逻辑处理器") +
                                    " · ABI ${DeviceInfo.primaryAbi()}"
                            },
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onPrimaryContainer,
                        )
                    }
                }

                Text(tr("Test", "测试"), style = MaterialTheme.typography.titleMedium)
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    listOf(
                        "per-core" to tr("Single-core", "单核"),
                        "multi" to tr("All-core", "全核"),
                        "all" to tr("Single + all-core", "单核 + 全核"),
                        "memory" to tr("Cache & memory", "缓存与内存"),
                    ).forEach { (id, label) ->
                        FilterChip(
                            selected = mode == id,
                            onClick = { mode = id },
                            enabled = !run.running,
                            label = { Text(label) },
                        )
                    }
                }

                if (mode != "memory") {
                    Text(tr("Duration", "时长"), style = MaterialTheme.typography.titleMedium)
                    FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        FilterChip(
                            selected = duration == "quick",
                            onClick = { duration = "quick" },
                            enabled = !run.running,
                            label = { Text(tr("Quick 1 s", "快速 1 秒")) },
                        )
                        FilterChip(
                            selected = duration == "formal",
                            onClick = { duration = "formal" },
                            enabled = !run.running,
                            label = { Text(tr("Formal 15 s", "正式 15 秒")) },
                        )
                    }
                }

                Text(
                    text = if (mode == "memory") {
                        tr(
                            "Sweeps the data-set size on one thread of every core type, then on all cores, and reads " +
                                "L1, L2, the shared cache and memory off the curve. It ends with a storage test that " +
                                "writes a temporary file of up to 1 GiB. Stopping cancels unfinished tests.",
                            "先用每类核心的一个线程、再用全部核心扫描数据量，从曲线上读出 L1、L2、共享缓存和内存各自的带宽。" +
                                "最后是存储测试，会写一个最大 1 GiB 的临时文件。停止会取消尚未完成的测试。",
                        )
                    } else {
                        tr(
                            "The single-core test runs once on every logical processor; the formal preset takes " +
                                "15 s for each. Stopping cancels unfinished tests.",
                            "单核测试会在每个逻辑处理器上各跑一次；正式预设每次 15 秒。停止会取消尚未完成的测试。",
                        )
                    },
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )

                if (run.running || run.finished) {
                    Card(
                        modifier = Modifier.fillMaxWidth(),
                        colors = CardDefaults.cardColors(
                            containerColor = MaterialTheme.colorScheme.surfaceContainerLow,
                        ),
                    ) {
                        Column(
                            modifier = Modifier.padding(14.dp),
                            verticalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            Row {
                                Text(run.status, style = MaterialTheme.typography.titleSmall, modifier = Modifier.weight(1f))
                                Text(
                                    "${(run.progress * 100).toInt()}%",
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                            LinearProgressIndicator(progress = { run.progress }, modifier = Modifier.fillMaxWidth())
                        }
                    }
                }
            },
            secondary = {
                if (run.coreScores.isNotEmpty() || run.multiSummary.isNotEmpty()) ComputeResults()
                if (run.memoryTest) MemoryResults()

                if (run.log.isNotEmpty()) {
                    ResultCard(tr("Raw output (last lines)", "原始输出（最后几行）")) {
                        Text(
                            text = run.log.takeLast(40).joinToString("\n") { it.replace('\t', ' ') },
                            style = MaterialTheme.typography.bodySmall,
                            fontFamily = FontFamily.Monospace,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }
            },
        )
    }
}

@Composable
private fun ResultCard(title: String, content: @Composable () -> Unit) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceContainerLow,
        ),
    ) {
        Column(
            modifier = Modifier.padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text(title, style = MaterialTheme.typography.titleSmall)
            content()
        }
    }
}

@Composable
private fun ComputeResults() {
    val run = CpuRun
    if (run.singleSummary.isEmpty() && run.multiSummary.isEmpty() && run.coreScores.isEmpty()) return
    ResultCard(tr("Summary", "摘要")) {
        if (run.singleSummary.isNotEmpty()) {
            Text(tr("Single-core average", "单核平均"), style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(run.singleSummary, style = MaterialTheme.typography.titleLarge)
        }
        if (run.multiSummary.isNotEmpty()) {
            Text(tr("All-core", "全核"), style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(run.multiSummary, style = MaterialTheme.typography.titleLarge)
        }
    }
    val scores = run.coreScores.sortedBy { it.core }
    if (scores.size >= 2) {
        ResultCard(
            tr("Score of every logical processor", "各逻辑处理器的成绩") + " (${scores.first().unit})",
        ) {
            ColumnChart(scores.map { (it.core + 1).toString() to it.score })
            scores.forEach { score ->
                Text(
                    "#${score.core + 1}  ${score.label}  ·  ${formatNumber(score.score)} ${score.unit}",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

@Composable
private fun LevelTable(levels: List<MemLevel>) {
    @Composable
    fun row(name: String, read: String, write: String, copy: String, header: Boolean = false) {
        val color = if (header) MaterialTheme.colorScheme.onSurfaceVariant else MaterialTheme.colorScheme.onSurface
        Row(Modifier.fillMaxWidth()) {
            Text(name, Modifier.weight(1.7f), style = MaterialTheme.typography.bodySmall, color = color)
            listOf(read, write, copy).forEach { cell ->
                Text(
                    cell, Modifier.weight(1f), textAlign = TextAlign.End,
                    style = MaterialTheme.typography.bodySmall, fontFamily = FontFamily.Monospace, color = color,
                )
            }
        }
    }
    fun rate(value: Double) = if (value > 0.0) formatNumber(value) else "-"

    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        row(
            tr("Level and size", "层级与容量"), tr("read", "读"), tr("write", "写"), tr("copy", "复制"),
            header = true,
        )
        var anyFromCurve = false
        levels.forEach { level ->
            val memory = level.level == "memory"
            val name = when {
                memory -> tr("Memory", "内存")
                level.fromCurve -> {
                    anyFromCurve = true
                    if (level.capacityHigh > 0.0) {
                        "${level.level} ${formatBytes(level.capacityLow)}–${formatBytes(level.capacityHigh)} *"
                    } else {
                        "${level.level} > ${formatBytes(level.capacityLow)} *"
                    }
                }
                else -> "${level.level} ${formatBytes(level.cacheBytes)}"
            }
            if (level.measured) row(name, rate(level.read), rate(level.write), rate(level.copy))
            else row(name, "-", "-", "-")
            if (memory && level.writeStream > 0.0) {
                row(tr("  streaming stores", "  流式写入"), "-", rate(level.writeStream), rate(level.copyStream))
            }
            if (memory && level.zeroFill > 0.0) row(tr("  CLZERO zero fill", "  CLZERO 清零"), "-", rate(level.zeroFill), "-")
            if (memory && !level.settled) {
                Text(
                    tr(
                        "Not a settled memory figure: the sweep ended before the curve went flat.",
                        "这不是稳定的内存数字：扫描在曲线变平之前就结束了。",
                    ),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.outline,
                )
            }
        }
        if (anyFromCurve) {
            Text(
                tr(
                    "* The system reports no size for this cache; its capacity was read off the curve and lies in this range.",
                    "* 系统没有报告这级缓存的容量；容量是从曲线上读出来的，落在这个范围内。",
                ),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.outline,
            )
        }
    }
}

@Composable
private fun MemoryResults() {
    val run = CpuRun
    if (run.levels.isEmpty()) return
    val colors = seriesColors()
    val classes = if (run.classes.isEmpty()) listOf(MemClass(0, "single", "", 0)) else run.classes.toList()
    val mixed = classes.size > 1

    fun curves(mode: String): List<ChartSeries> =
        listOf(
            "read" to tr("Read", "读"), "write" to tr("Write", "写"), "copy" to tr("Copy", "复制"),
        ).mapIndexed { index, (kernel, label) ->
            ChartSeries(
                label, colors[index],
                run.points.filter { it.series == "$mode/$kernel" }.map { it.x to it.y },
            )
        }

    fun markers(cpuClass: Int, threads: Int): List<ChartMarker> =
        run.caches.filter { it.cpuClass == cpuClass }.map { cache ->
            // A cache shared by several of the running threads counts per thread.
            val sharers = minOf(threads, cache.sharedCores).coerceAtLeast(1)
            ChartMarker(cache.sizeBytes / sharers, if (sharers > 1) "L${cache.level}÷$sharers" else "L${cache.level}")
        }

    classes.forEach { cpuClass ->
        val title = if (mixed) {
            tr(
                "Core class ${cpuClass.rank} · ${cpuClass.name} · ${cpuClass.cores} cores — one thread, GB/s",
                "核心类别 ${cpuClass.rank} · ${cpuClass.name} · ${cpuClass.cores} 个核心 —— 单线程，GB/s",
            )
        } else {
            tr("One thread, GB/s", "单线程，GB/s")
        }
        ResultCard(title) {
            LevelTable(run.levels.filter { it.mode == cpuClass.mode })
            LineChart(curves(cpuClass.mode), markers = markers(cpuClass.rank, 1))
        }
    }
    val all = run.levels.filter { it.mode == "all" }
    if (all.isNotEmpty()) {
        ResultCard(
            tr(
                "All cores — ${run.allThreads} threads, GB/s (sizes are per thread)",
                "全核 —— ${run.allThreads} 个线程，GB/s（数据量按每个线程计）",
            ),
        ) {
            LevelTable(all)
            LineChart(
                curves("all"),
                markers = if (mixed) emptyList() else markers(classes.first().rank, run.allThreads),
            )
        }
    }
    val storage = run.storage
    if (run.pageMapping.isNotEmpty() || storage != null) {
        ResultCard(tr("Virtual memory and storage", "虚拟内存与存储")) {
            if (run.pageMapping.isNotEmpty()) {
                Text(tr("Mapping new pages: ", "映射新页面：") + run.pageMapping, style = MaterialTheme.typography.bodySmall)
            }
            if (storage != null) {
                if (storage.backing.isNotEmpty()) {
                    Text(
                        tr("The system pages to: ", "系统换页到：") + localizeEngineText(storage.backing),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                if (storage.measured) {
                    Text(tr("Storage, sequential 1 MiB (MB/s)", "存储，顺序 1 MiB（MB/s）"), style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                    BarRows(
                        listOf(tr("Read", "读") to storage.seqRead, tr("Write", "写") to storage.seqWrite),
                        "MB/s",
                    )
                    Text(tr("Storage, random 4 KiB, one request at a time (MB/s)", "存储，随机 4 KiB，每次一个请求（MB/s）"),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant)
                    BarRows(
                        listOf(tr("Read", "读") to storage.randRead, tr("Write", "写") to storage.randWrite),
                        "MB/s",
                    )
                    Text(
                        tr(
                            "${formatNumber(storage.randReadIops)} read and ${formatNumber(storage.randWriteIops)} " +
                                "write requests per second",
                            "每秒 ${formatNumber(storage.randReadIops)} 次读请求、" +
                                "${formatNumber(storage.randWriteIops)} 次写请求",
                        ) + if (storage.directIo) {
                            ""
                        } else {
                            tr(
                                ". Buffered I/O was used, so the file cache may inflate these.",
                                "。走了系统文件缓存，数字可能偏高。",
                            )
                        },
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                } else {
                    Text(
                        tr("Storage not measured", "存储未测量") +
                            if (storage.skipped.isEmpty()) "" else tr(": ", "：") + localizeEngineText(storage.skipped),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
        }
    }
}
