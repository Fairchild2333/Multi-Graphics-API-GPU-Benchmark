package com.mangekyo.benchmark.ui.screens

import com.mangekyo.benchmark.core.AutoRun
import com.mangekyo.benchmark.core.GpuRun
import com.mangekyo.benchmark.core.localizeScoreLine
import com.mangekyo.benchmark.core.tr
import com.mangekyo.benchmark.ui.components.ChartSeries
import com.mangekyo.benchmark.ui.components.LineChart
import com.mangekyo.benchmark.ui.components.seriesColors

import androidx.compose.ui.*

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Stop
import androidx.compose.runtime.LaunchedEffect
import kotlinx.coroutines.delay
import androidx.compose.material3.AssistChip
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.mangekyo.benchmark.core.CapabilityGate
import com.mangekyo.benchmark.core.GraphicsBackend
import com.mangekyo.benchmark.core.NativeBridge
import com.mangekyo.benchmark.core.WorkloadRegistry
import com.mangekyo.benchmark.ui.components.BenchmarkSurface

/** GPU page shell aligned with WinUI. Engine start/cancel still TODO. */
@OptIn(ExperimentalMaterial3Api::class, ExperimentalLayoutApi::class)
@Composable
fun GpuScreen() {
    val context = LocalContext.current
    val caps = remember(context) {
        NativeBridge.init(context)
        CapabilityGate.snapshot(context)
    }
    var selectedWorkloadId by rememberSaveable {
        mutableStateOf(WorkloadRegistry.gpuWorkloads.firstOrNull()?.id.orEmpty())
    }
    var selectedBackend by rememberSaveable {
        mutableStateOf(
            when (caps.preferredBackend) {
                GraphicsBackend.Vulkan -> "Vulkan"
                GraphicsBackend.Gles31 -> "GLES31"
                GraphicsBackend.Unsupported -> "Unsupported"
            },
        )
    }
    var running by remember { mutableStateOf(NativeBridge.isRunning()) }
    var statusText by remember { mutableStateOf("") }
    LaunchedEffect(Unit) {
        while (true) {
            running = NativeBridge.isRunning()
            // Collect what the engine printed while this page owns the run.
            if (NativeBridge.outputOwner == "gpu") {
                NativeBridge.pollOutput().forEach(GpuRun::consume)
                if (GpuRun.active && !running) {
                    NativeBridge.pollOutput().forEach(GpuRun::consume)
                    GpuRun.active = false
                    AutoRun.log(
                        "RUN finished kind=gpu score=" + GpuRun.scoreLine.ifEmpty { "none" } +
                            NativeBridge.lastError().let { if (it.isEmpty()) "" else " error=$it" },
                    )
                }
            }
            delay(250)
        }
    }
    val scrollBehavior = TopAppBarDefaults.pinnedScrollBehavior()
    val vulkanOk = selectedBackend == "Vulkan" && caps.vulkanLoaderPresent && NativeBridge.isEngineLoaded
    val runEnabled = vulkanOk && selectedWorkloadId.isNotEmpty() &&
        (selectedWorkloadId == "stream" || selectedWorkloadId == "gpu_burn")

    fun start(workloadId: String, seconds: Double): Boolean {
        NativeBridge.pollOutput()  // drop lines left over from an earlier run
        NativeBridge.outputOwner = "gpu"
        GpuRun.begin()
        val ok = NativeBridge.startWorkload(workloadId, seconds)
        if (!ok) GpuRun.active = false
        statusText = if (ok) {
            tr(
                "Running $workloadId (${seconds.toInt()}s preview)",
                "正在运行 $workloadId（${seconds.toInt()} 秒预览）",
            )
        } else {
            NativeBridge.lastError().ifEmpty { tr("Start failed", "启动失败") }
        }
        running = NativeBridge.isRunning()
        return ok
    }

    // A test requested over adb, see AutoRun.
    val request = AutoRun.pending
    LaunchedEffect(request) {
        val wanted = request ?: return@LaunchedEffect
        if (wanted.test !in AutoRun.gpuTests) return@LaunchedEffect
        AutoRun.pending = null
        if (!caps.vulkanLoaderPresent || !NativeBridge.isEngineLoaded) {
            AutoRun.log("AUTORUN refused test=${wanted.test} reason=vulkan_or_engine_missing")
            return@LaunchedEffect
        }
        selectedBackend = "Vulkan"
        selectedWorkloadId = wanted.test
        val seconds = if (wanted.seconds > 0) wanted.seconds.toDouble() else 3.0
        // The preview surface appears a moment after the page; wait up to 10 s.
        var started = false
        for (attempt in 0 until 40) {
            if (start(wanted.test, seconds)) {
                started = true
                break
            }
            delay(250)
        }
        AutoRun.log(
            if (started) "AUTORUN started test=${wanted.test}"
            else "AUTORUN failed test=${wanted.test} reason=${NativeBridge.lastError()}",
        )
    }

    Scaffold(
        modifier = Modifier.nestedScroll(scrollBehavior.nestedScrollConnection),
        topBar = {
            TopAppBar(
                title = { Text(tr("GPU Benchmark", "GPU 跑分")) },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surfaceContainer,
                    titleContentColor = MaterialTheme.colorScheme.onSurface,
                ),
                scrollBehavior = scrollBehavior,
            )
        },
        floatingActionButton = {
            FloatingActionButton(
                onClick = {
                    if (running) {
                        NativeBridge.stopWorkload()
                        statusText = tr("Stop requested", "已请求停止")
                    } else if (runEnabled) {
                        start(selectedWorkloadId, 3.0)
                    }
                },
                containerColor = if (runEnabled || running) {
                    MaterialTheme.colorScheme.primaryContainer
                } else {
                    MaterialTheme.colorScheme.surfaceContainerHighest
                },
                contentColor = if (runEnabled || running) {
                    MaterialTheme.colorScheme.onPrimaryContainer
                } else {
                    MaterialTheme.colorScheme.onSurfaceVariant
                },
            ) {
                if (running) {
                    Icon(Icons.Filled.Stop, contentDescription = tr("Stop", "停止"))
                } else {
                    Icon(Icons.Filled.PlayArrow, contentDescription = tr("Run", "运行"))
                }
            }
        },
    ) { inner ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(inner)
                .padding(horizontal = 16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            item {
                Box(modifier = Modifier.height(4.dp))
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.surfaceContainerLow,
                    ),
                ) {
                    Column(
                        modifier = Modifier.padding(12.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        Text(tr("Preview surface", "预览画面"), style = MaterialTheme.typography.titleMedium)
                        BenchmarkSurface(
                            modifier = Modifier
                                .fillMaxWidth()
                                .height(200.dp),
                        )
                        Text(
                            text = tr("Engine: ", "引擎：") + NativeBridge.engineVersion(),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }
            }

            item {
                Text(tr("Backend", "图形后端"), style = MaterialTheme.typography.titleMedium)
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    FilterChip(
                        selected = selectedBackend == "Vulkan",
                        onClick = { selectedBackend = "Vulkan" },
                        enabled = caps.vulkanLoaderPresent,
                        label = { Text("Vulkan") },
                    )
                    FilterChip(
                        selected = selectedBackend == "GLES31",
                        onClick = { selectedBackend = "GLES31" },
                        label = { Text("GL ES 3.1") },
                    )
                }
                FlowRow(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.padding(top = 8.dp),
                ) {
                    AssistChip(
                        onClick = {},
                        label = {
                            Text(
                                if (caps.vulkanLoaderPresent) tr("Vulkan loader OK", "Vulkan 加载器可用")
                                else tr("Vulkan unavailable", "Vulkan 不可用"),
                            )
                        },
                    )
                    AssistChip(
                        onClick = {},
                        label = { Text("GL ES ${caps.glesVersionLabel}") },
                    )
                    if (caps.vulkanHardwareFeature) {
                        AssistChip(onClick = {}, label = { Text(tr("Vulkan HW feature", "Vulkan 硬件特性")) })
                    }
                    AssistChip(onClick = {}, label = { Text(caps.abi) })
                    AssistChip(onClick = {}, label = { Text(caps.androidRelease) })
                }
                caps.notes.forEach { note ->
                    Text(
                        text = note,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.padding(top = 4.dp),
                    )
                }
            }

            item {
                Text(tr("Workloads", "测试项目"), style = MaterialTheme.typography.titleMedium)
                Text(
                    text = tr(
                        "Engine writes *_android_preview (3s embed, not desktop 15s) — do not mix with Windows groups.",
                        "成绩的版本带 _android_preview 后缀（内嵌运行 3 秒，不是桌面版的 15 秒），不要和 Windows 的成绩放在一起比较。",
                    ),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }

            items(WorkloadRegistry.gpuWorkloads, key = { it.id }) { workload ->
                val selected = workload.id == selectedWorkloadId
                Card(
                    onClick = { selectedWorkloadId = workload.id },
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = if (selected) {
                            MaterialTheme.colorScheme.secondaryContainer
                        } else {
                            MaterialTheme.colorScheme.surfaceContainerHighest
                        },
                    ),
                ) {
                    Column(modifier = Modifier.padding(14.dp)) {
                        Text(workload.displayName, style = MaterialTheme.typography.titleMedium)
                        Text(
                            text = workload.workloadVersion,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        if (workload.isPreview) {
                            Text(
                                text = tr("preview", "预览"),
                                style = MaterialTheme.typography.labelLarge,
                                color = MaterialTheme.colorScheme.tertiary,
                                modifier = Modifier.padding(top = 4.dp),
                            )
                        }
                    }
                }
            }

            if (GpuRun.scoreLine.isNotEmpty() || GpuRun.fpsLine.isNotEmpty() || GpuRun.samples.size >= 2) {
                item {
                    Card(
                        modifier = Modifier.fillMaxWidth(),
                        colors = CardDefaults.cardColors(
                            containerColor = MaterialTheme.colorScheme.surfaceContainerLow,
                        ),
                    ) {
                        Column(
                            modifier = Modifier.padding(14.dp),
                            verticalArrangement = Arrangement.spacedBy(6.dp),
                        ) {
                            Text(tr("Last run", "最近一次运行"), style = MaterialTheme.typography.titleMedium)
                            if (GpuRun.scoreLine.isNotEmpty()) {
                                Text(localizeScoreLine(GpuRun.scoreLine), style = MaterialTheme.typography.titleMedium)
                            }
                            if (GpuRun.fpsLine.isNotEmpty()) {
                                Text(localizeScoreLine(GpuRun.fpsLine), style = MaterialTheme.typography.bodyMedium)
                            }
                            // GPU time per frame over the measured part of the run.
                            val colors = seriesColors()
                            val measured = GpuRun.samples.filter { !it.warmup }
                            val series = listOf(
                                ChartSeries(tr("Compute", "计算"), colors[0], measured.map { it.seconds to it.computeMs }),
                                ChartSeries(tr("Render", "渲染"), colors[1], measured.map { it.seconds to it.renderMs }),
                            ).filter { line -> line.points.size >= 2 && line.points.any { it.second > 0.0005 } }
                            if (series.isNotEmpty()) {
                                Text(
                                    tr(
                                        "GPU time per frame (ms) over the run, after warm-up",
                                        "每帧 GPU 耗时（ms）随运行时间变化，不含预热",
                                    ),
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                                LineChart(series, logBytes = false)
                            }
                        }
                    }
                }
            }

            item {
                Text(
                    text = when {
                        statusText.isNotEmpty() -> statusText
                        !vulkanOk -> tr(
                            "Vulkan Run needs API 24+ loader + Surface. GLES path not wired yet.",
                            "运行需要 Android 7.0（API 24）以上的 Vulkan 加载器和可用的预览画面；GL ES 路径还没有接入。",
                        )
                        else -> tr(
                            "Preview Run: 3s Stream/GPU Burn via gpu_engine. Scores are android_preview — do not mix with Windows.",
                            "预览运行：粒子测试或 GPU Burn 跑 3 秒。成绩属于 android_preview，不要和 Windows 的成绩放在一起比较。",
                        )
                    },
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Box(modifier = Modifier.height(72.dp))
            }
        }
    }
}
