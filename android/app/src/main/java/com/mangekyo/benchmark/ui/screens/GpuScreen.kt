package com.mangekyo.benchmark.ui.screens

import com.mangekyo.benchmark.core.AutoRun
import com.mangekyo.benchmark.core.GpuRun
import com.mangekyo.benchmark.core.formatNumber
import com.mangekyo.benchmark.core.localizeScoreLine
import com.mangekyo.benchmark.core.tr
import com.mangekyo.benchmark.ui.components.ChartSeries
import com.mangekyo.benchmark.ui.components.LineChart
import com.mangekyo.benchmark.ui.components.seriesColors

import androidx.compose.ui.*

import android.app.Activity
import android.content.pm.ActivityInfo
import android.view.WindowManager
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.FilledTonalButton
import androidx.compose.runtime.DisposableEffect
import androidx.compose.ui.graphics.Color
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.spring
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
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
import androidx.compose.material3.ExtendedFloatingActionButton
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
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.dp
import com.mangekyo.benchmark.core.CapabilityGate
import com.mangekyo.benchmark.core.GraphicsBackend
import com.mangekyo.benchmark.core.NativeBridge
import com.mangekyo.benchmark.core.WorkloadRegistry
import com.mangekyo.benchmark.ui.AdaptivePanes
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
    val refreshRate = LocalView.current.display?.refreshRate ?: 0f
    var statusText by remember { mutableStateOf("") }
    // The test that waits for the full-screen surface, and whether adb asked for it.
    var launch by remember { mutableStateOf<Pair<String, Double>?>(null) }
    var launchFromAdb by remember { mutableStateOf(false) }
    // A test without a picture runs through the command-line entry.
    var commandLineRun by remember { mutableStateOf(false) }
    LaunchedEffect(Unit) {
        while (true) {
            running = NativeBridge.isRunning() || (commandLineRun && NativeBridge.isCliRunning())
            // Collect what the engine printed while this page owns the run.
            if (NativeBridge.outputOwner == "gpu") {
                NativeBridge.pollOutput().forEach(GpuRun::consume)
                if (GpuRun.active && !running) {
                    NativeBridge.pollOutput().forEach(GpuRun::consume)
                    GpuRun.active = false
                    statusText = NativeBridge.lastError().ifEmpty { tr("Finished", "已完成") }
                    GpuRun.fullscreen = false
                    commandLineRun = false
                    // Besides the engine's own verdict: a frame rate that sits on
                    // the refresh rate of the display was held there by it.
                    val fps = GpuRun.averageFps
                    if (fps != null && refreshRate > 1f && kotlin.math.abs(fps - refreshRate) < refreshRate * 0.04) {
                        GpuRun.displayLimited = true
                    }
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
    val runEnabled = vulkanOk && WorkloadRegistry.gpuWorkloads.any { it.id == selectedWorkloadId }

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

    // The VRAM / cache bandwidth sweep computes only: no picture, no full screen.
    fun startCommandLineTest(workloadId: String): Boolean {
        NativeBridge.pollOutput()  // drop lines left over from an earlier run
        NativeBridge.outputOwner = "gpu"
        GpuRun.begin()
        val ok = NativeBridge.startCli(arrayOf("--workload", workloadId, "--api", "vulkan", "--gpu", "0"))
        commandLineRun = ok
        if (!ok) GpuRun.active = false
        statusText = if (ok) {
            tr("Running $workloadId; this takes a minute or two", "正在运行 $workloadId，需要一两分钟")
        } else {
            NativeBridge.lastError().ifEmpty { tr("Start failed", "启动失败") }
        }
        running = ok
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
        if (wanted.test in WorkloadRegistry.gpuCommandLineTests) {
            val started = startCommandLineTest(wanted.test)
            AutoRun.log(
                if (started) "AUTORUN started test=${wanted.test}"
                else "AUTORUN failed test=${wanted.test} reason=${NativeBridge.lastError()}",
            )
            return@LaunchedEffect
        }
        launchFromAdb = true
        launch = wanted.test to (if (wanted.seconds > 0) wanted.seconds.toDouble() else 3.0)
        GpuRun.fullscreen = true
    }

    // A test takes the whole screen. The page first switches to the picture
    // alone; the test starts once that surface exists, which takes a moment
    // because the screen may have to turn to landscape first.
    fun requestRun(workloadId: String, seconds: Double) {
        if (workloadId in WorkloadRegistry.gpuCommandLineTests) {
            startCommandLineTest(workloadId)
            return
        }
        launchFromAdb = false
        launch = workloadId to seconds
        GpuRun.fullscreen = true
        statusText = tr("Starting…", "正在启动…")
    }
    LaunchedEffect(launch) {
        val (workloadId, seconds) = launch ?: return@LaunchedEffect
        delay(600)
        var started = false
        for (attempt in 0 until 40) {
            if (start(workloadId, seconds)) {
                started = true
                break
            }
            delay(250)
        }
        if (!started) GpuRun.fullscreen = false
        if (launchFromAdb) {
            AutoRun.log(
                if (started) "AUTORUN started test=$workloadId"
                else "AUTORUN failed test=$workloadId reason=${NativeBridge.lastError()}",
            )
        }
        launch = null
    }

    if (GpuRun.fullscreen) {
        FullscreenRun(
            title = WorkloadRegistry.gpuWorkloads.firstOrNull { it.id == selectedWorkloadId }?.displayName
                ?: selectedWorkloadId,
            status = statusText,
            onStop = {
                NativeBridge.stopWorkload()
                statusText = tr("Stop requested", "已请求停止")
                // Nothing to stop yet: leave the full screen at once.
                if (!NativeBridge.isRunning() && launch == null) GpuRun.fullscreen = false
            },
        )
        return
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
            ExtendedFloatingActionButton(
                onClick = {
                    if (running && commandLineRun) {
                        // The sweep has no stop; it ends by itself.
                    } else if (running) {
                        NativeBridge.stopWorkload()
                        statusText = tr("Stop requested", "已请求停止")
                    } else if (runEnabled) {
                        requestRun(selectedWorkloadId, 3.0)
                    }
                },
                icon = {
                    Icon(if (running) Icons.Filled.Stop else Icons.Filled.PlayArrow, contentDescription = null)
                },
                text = { Text(if (running) tr("Stop", "停止") else tr("Run", "运行")) },
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
            )
        },
    ) { inner ->
        AdaptivePanes(
            modifier = Modifier.padding(inner),
            primary = {
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
                        Text(tr("Runs full screen", "全屏运行"), style = MaterialTheme.typography.titleMedium)
                        Text(
                            text = tr(
                                "A test takes over the screen in landscape. It always renders 1280 × 720, as the " +
                                    "desktop version does, and the system scales the picture to the screen, so a " +
                                    "score does not depend on the size or resolution of the display.",
                                "测试开始后横屏占满屏幕。渲染尺寸固定为 1280 × 720，和桌面版相同，再由系统缩放到屏幕，" +
                                    "所以成绩不随屏幕大小和分辨率变化。",
                            ),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        Text(
                            text = tr("Engine: ", "引擎：") + NativeBridge.engineVersion(),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }

                Column {
                    Text(tr("API", "图形 API"), style = MaterialTheme.typography.titleMedium)
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
            },
            secondary = {
                Column {
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

                WorkloadRegistry.gpuWorkloads.forEach { workload ->
                    val selected = workload.id == selectedWorkloadId
                    // Material 3 Expressive marks a selection by shape as well as
                    // by colour, and moves between the two with a spring. Its own
                    // components for this are not in the stable library yet.
                    val corner by animateDpAsState(
                        targetValue = if (selected) 28.dp else 16.dp,
                        animationSpec = spring(
                            dampingRatio = Spring.DampingRatioMediumBouncy,
                            stiffness = Spring.StiffnessMediumLow,
                        ),
                        label = "workloadCorner",
                    )
                    val container by animateColorAsState(
                        targetValue = if (selected) {
                            MaterialTheme.colorScheme.secondaryContainer
                        } else {
                            MaterialTheme.colorScheme.surfaceContainerHighest
                        },
                        label = "workloadContainer",
                    )
                    Card(
                        onClick = { selectedWorkloadId = workload.id },
                        modifier = Modifier.fillMaxWidth(),
                        shape = RoundedCornerShape(corner),
                        colors = CardDefaults.cardColors(containerColor = container),
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

                if (GpuRun.scoreLine.isNotEmpty() || GpuRun.fpsLine.isNotEmpty() || GpuRun.samples.size >= 2 ||
                    GpuRun.memoryPoints.isNotEmpty()
                ) {
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
                            if (GpuRun.displayLimited) {
                                Text(
                                    tr(
                                        "The frame rate was held at the refresh rate of the display, so the GPU was " +
                                            "not kept busy and this score may be too low. Run a heavier test.",
                                        "帧率被屏幕刷新率限制住了，GPU 没有跑满，这个成绩可能偏低、不准确。请换负载更高的测试。",
                                    ),
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.error,
                                )
                            }
                            // The memory sweep: GB/s over the size of the data set, one line per kernel.
                            if (GpuRun.memoryPoints.isNotEmpty()) {
                                val sweepColors = seriesColors()
                                val kernels = listOf(
                                    "read" to tr("Read", "读"),
                                    "write" to tr("Write", "写"),
                                    "copy" to tr("Copy", "复制"),
                                )
                                GpuRun.memoryLevels.forEach { level ->
                                    val name = kernels.firstOrNull { it.first == level.kernel }?.second ?: level.kernel
                                    Text(
                                        tr(
                                            "$name: cache ${formatNumber(level.cacheGbps)} GB/s, " +
                                                "memory ${formatNumber(level.memoryGbps)} GB/s",
                                            "$name：缓存 ${formatNumber(level.cacheGbps)} GB/s，" +
                                                "显存 ${formatNumber(level.memoryGbps)} GB/s",
                                        ),
                                        style = MaterialTheme.typography.bodyMedium,
                                    )
                                }
                                Text(
                                    tr("GB/s over the size of the data set", "GB/s 随数据量变化"),
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                                LineChart(
                                    kernels.mapIndexedNotNull { index, (id, label) ->
                                        val points = GpuRun.memoryPoints.filter { it.kernel == id }
                                            .map { it.bytes to it.gbps }
                                        if (points.size >= 2) ChartSeries(label, sweepColors[index], points) else null
                                    },
                                    logBytes = true,
                                )
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
            },
        )
    }
}

/**
 * A running GPU test: the picture alone, as large as the screen allows, in
 * landscape with the system bars hidden and the screen kept on. Back or the
 * button stops the test. The picture is 16:9; a screen of another shape gets
 * black bars instead of a stretched image.
 */
@Composable
private fun FullscreenRun(title: String, status: String, onStop: () -> Unit) {
    val activity = LocalContext.current as? Activity
    DisposableEffect(activity) {
        val window = activity?.window
        val controller = window?.let { WindowCompat.getInsetsController(it, it.decorView) }
        val previousOrientation = activity?.requestedOrientation ?: ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
        controller?.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        controller?.hide(WindowInsetsCompat.Type.systemBars())
        activity?.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        window?.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        onDispose {
            controller?.show(WindowInsetsCompat.Type.systemBars())
            activity?.requestedOrientation = previousOrientation
            window?.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        }
    }
    BackHandler(onBack = onStop)

    BoxWithConstraints(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black),
        contentAlignment = Alignment.Center,
    ) {
        val widerThanPicture = maxWidth / maxHeight > 16f / 9f
        BenchmarkSurface(
            modifier = if (widerThanPicture) {
                Modifier
                    .fillMaxHeight()
                    .aspectRatio(16f / 9f, matchHeightConstraintsFirst = true)
            } else {
                Modifier
                    .fillMaxWidth()
                    .aspectRatio(16f / 9f)
            },
            framed = false,
        )
        Column(
            modifier = Modifier
                .align(Alignment.TopStart)
                .padding(16.dp),
        ) {
            Text(title, style = MaterialTheme.typography.titleMedium, color = Color.White)
            if (status.isNotEmpty()) {
                Text(status, style = MaterialTheme.typography.bodySmall, color = Color.White.copy(alpha = 0.8f))
            }
        }
        FilledTonalButton(
            onClick = onStop,
            modifier = Modifier
                .align(Alignment.BottomEnd)
                .padding(24.dp),
        ) {
            Icon(Icons.Filled.Stop, contentDescription = null)
            Text(tr("Stop", "停止"), modifier = Modifier.padding(start = 8.dp))
        }
    }
}

