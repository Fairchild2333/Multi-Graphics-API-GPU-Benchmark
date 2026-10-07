package com.mangekyo.benchmark.ui.screens

import com.mangekyo.benchmark.core.AutoRun
import com.mangekyo.benchmark.core.GpuOptions
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
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Stop
import androidx.compose.runtime.LaunchedEffect
import kotlinx.coroutines.delay
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
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
import com.mangekyo.benchmark.ui.components.KeepScreenOn

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
    // The size of the selected test and its duration, as in the desktop app.
    var particleChoice by rememberSaveable { mutableStateOf(GpuOptions.DEFAULT_PARTICLES) }
    var customParticles by rememberSaveable { mutableStateOf("") }
    var stepChoice by rememberSaveable { mutableStateOf(GpuOptions.DEFAULT_STEPS) }
    var customSteps by rememberSaveable { mutableStateOf("") }
    var secondChoice by rememberSaveable { mutableStateOf(GpuOptions.DEFAULT_SECONDS) }
    // Particle test only: compute without a picture, as "Headless" in the desktop app.
    var headless by rememberSaveable { mutableStateOf(false) }
    var customSeconds by rememberSaveable { mutableStateOf("") }
    val particles = GpuOptions.resolve(particleChoice, customParticles, GpuOptions.particleRange)
    val steps = GpuOptions.resolve(stepChoice, customSteps, GpuOptions.stepRange)
    val seconds = GpuOptions.resolve(secondChoice, customSeconds, GpuOptions.secondRange)
    val optionsValid = when (selectedWorkloadId) {
        "stream" -> particles != null && seconds != null
        "gpu_burn" -> steps != null && seconds != null
        "cinematic_liquid" -> seconds != null
        else -> true
    }
    fun testName(id: String): String =
        WorkloadRegistry.gpuWorkloads.firstOrNull { it.id == id }?.displayName ?: id
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
    // The user stopped that test.
    var commandLineStopped by remember { mutableStateOf(false) }
    LaunchedEffect(Unit) {
        while (true) {
            running = NativeBridge.isRunning() || (commandLineRun && NativeBridge.isCliRunning())
            // Collect what the engine printed while this page owns the run.
            if (NativeBridge.outputOwner == "gpu") {
                NativeBridge.pollOutput().forEach(GpuRun::consume)
                if (GpuRun.active && !running) {
                    NativeBridge.pollOutput().forEach(GpuRun::consume)
                    GpuRun.active = false
                    statusText = if (commandLineStopped) {
                        tr("Stopped; nothing was saved", "已停止，没有保存成绩")
                    } else {
                        NativeBridge.lastError().ifEmpty { tr("Finished", "已完成") }
                    }
                    GpuRun.fullscreen = false
                    commandLineRun = false
                    commandLineStopped = false
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
    // OpenGL ES 3.1 has compute shaders and so two of the tests; OpenGL ES 2.0
    // has a GPU Burn of its own.
    val glesAvailable = caps.glesMajor > 3 || (caps.glesMajor == 3 && caps.glesMinor >= 1)
    val gles2Available = caps.glesMajor >= 2
    fun apiAvailable(name: String): Boolean = when (name) {
        GpuOptions.API_GLES -> glesAvailable
        GpuOptions.API_GLES2 -> gles2Available
        else -> caps.vulkanLoaderPresent
    }
    val api = when (selectedBackend) {
        "GLES31" -> GpuOptions.API_GLES
        "GLES2" -> GpuOptions.API_GLES2
        else -> GpuOptions.API_VULKAN
    }
    val apiReady = NativeBridge.isEngineLoaded && selectedBackend != "Unsupported" && apiAvailable(api)
    val testOnApi = selectedWorkloadId in GpuOptions.testsOf(api)
    val runEnabled = apiReady && testOnApi && optionsValid &&
        WorkloadRegistry.gpuWorkloads.any { it.id == selectedWorkloadId }

    fun start(workloadId: String, seconds: Double): Boolean {
        NativeBridge.pollOutput()  // drop lines left over from an earlier run
        NativeBridge.outputOwner = "gpu"
        GpuRun.begin()
        val ok = NativeBridge.startWorkload(workloadId, seconds)
        if (!ok) GpuRun.active = false
        statusText = if (ok) {
            tr(
                "Running ${testName(selectedWorkloadId)}, ${seconds.toInt()} s",
                "正在运行 ${testName(selectedWorkloadId)}，${seconds.toInt()} 秒",
            )
        } else {
            NativeBridge.lastError().ifEmpty { tr("Start failed", "启动失败") }
        }
        running = NativeBridge.isRunning()
        return ok
    }

    // The VRAM / cache bandwidth sweep and the headless particle test compute
    // only: no picture, no full screen. They run through the command-line entry.
    fun startCommandLineTest(request: String, seconds: Int): Boolean {
        NativeBridge.pollOutput()  // drop lines left over from an earlier run
        NativeBridge.outputOwner = "gpu"
        GpuRun.begin()
        val ok = NativeBridge.startCli(GpuOptions.commandLine(request, seconds))
        commandLineRun = ok
        commandLineStopped = false
        if (!ok) GpuRun.active = false
        statusText = if (ok) {
            if (request.substringBefore('?') == "stream_headless") {
                tr(
                    "Running ${testName("stream")} (headless), $seconds s",
                    "正在运行 ${testName("stream")}（Headless），$seconds 秒",
                )
            } else {
                tr(
                    "Running ${testName(request)}; this takes about a minute, longer on a slow GPU",
                    "正在运行 ${testName(request)}，大约一分钟，GPU 慢的设备更久",
                )
            }
        } else {
            NativeBridge.lastError().ifEmpty { tr("Start failed", "启动失败") }
        }
        running = ok
        return ok
    }

    // The headless particle test under OpenGL ES: no picture and no full screen
    // either, but it runs in the engine host, which makes the context it needs.
    fun startHeadlessTest(request: String, seconds: Int): Boolean {
        NativeBridge.pollOutput()  // drop lines left over from an earlier run
        NativeBridge.outputOwner = "gpu"
        GpuRun.begin()
        val ok = NativeBridge.startWorkload(request, seconds.toDouble())
        commandLineRun = ok
        commandLineStopped = false
        if (!ok) GpuRun.active = false
        statusText = if (ok) {
            tr(
                "Running ${testName("stream")} (headless), $seconds s",
                "正在运行 ${testName("stream")}（Headless），$seconds 秒",
            )
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
        if (wanted.test.substringBefore('?') !in AutoRun.gpuTests) return@LaunchedEffect
        AutoRun.pending = null
        val asked = GpuOptions.parse(wanted.test)
        if (!NativeBridge.isEngineLoaded || !apiAvailable(asked.api)) {
            val missing = when (asked.api) {
                GpuOptions.API_GLES -> "gles31_or_engine_missing"
                GpuOptions.API_GLES2 -> "gles2_or_engine_missing"
                else -> "vulkan_or_engine_missing"
            }
            AutoRun.log("AUTORUN refused test=${wanted.test} reason=$missing")
            return@LaunchedEffect
        }
        if (asked.testId !in GpuOptions.testsOf(asked.api)) {
            AutoRun.log("AUTORUN refused test=${wanted.test} reason=test_not_under_this_api")
            return@LaunchedEffect
        }
        selectedBackend = when (asked.api) {
            GpuOptions.API_GLES -> "GLES31"
            GpuOptions.API_GLES2 -> "GLES2"
            else -> "Vulkan"
        }
        selectedWorkloadId = asked.testId
        asked.particles?.let { count ->
            particleChoice = if (GpuOptions.particlePresets.any { it.value == count }) count else GpuOptions.CUSTOM
            if (particleChoice == GpuOptions.CUSTOM) customParticles = count.toString()
        }
        asked.steps?.let { count ->
            stepChoice = if (GpuOptions.stepPresets.any { it.value == count }) count else GpuOptions.CUSTOM
            if (stepChoice == GpuOptions.CUSTOM) customSteps = count.toString()
        }
        headless = asked.headless
        // Without "--ei seconds" a test from adb runs for 3 s.
        val askedSeconds = if (wanted.seconds > 0) wanted.seconds else 3
        secondChoice = if (GpuOptions.secondPresets.any { it.value == askedSeconds }) askedSeconds else GpuOptions.CUSTOM
        if (secondChoice == GpuOptions.CUSTOM) customSeconds = askedSeconds.toString()
        if (wanted.test.substringBefore('?') in WorkloadRegistry.gpuCommandLineTests) {
            val started = if (GpuOptions.runsOnCommandLine(wanted.test)) {
                startCommandLineTest(wanted.test, askedSeconds)
            } else {
                startHeadlessTest(wanted.test, askedSeconds)
            }
            AutoRun.log(
                if (started) "AUTORUN started test=${wanted.test}"
                else "AUTORUN failed test=${wanted.test} reason=${NativeBridge.lastError()}",
            )
            return@LaunchedEffect
        }
        launchFromAdb = true
        launch = wanted.test to askedSeconds.toDouble()
        GpuRun.fullscreen = true
    }

    // A test takes the whole screen. The page first switches to the picture
    // alone; the test starts once that surface exists, which takes a moment
    // because the screen may have to turn to landscape first.
    fun requestRun(workloadId: String, seconds: Double) {
        if (workloadId.substringBefore('?') in WorkloadRegistry.gpuCommandLineTests) {
            if (GpuOptions.runsOnCommandLine(workloadId)) {
                startCommandLineTest(workloadId, seconds.toInt())
            } else {
                startHeadlessTest(workloadId, seconds.toInt())
            }
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

    KeepScreenOn(commandLineRun && running)

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
                title = { Text(tr("GPU Benchmark", "GPU 测试")) },
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
                    if (running) {
                        // A test with a picture ends at once; the sweep ends
                        // after the point it is measuring.
                        NativeBridge.stopWorkload()
                        if (commandLineRun) commandLineStopped = true
                        statusText = tr("Stop requested", "已请求停止")
                    } else if (runEnabled) {
                        requestRun(
                            GpuOptions.request(
                                selectedWorkloadId,
                                particles ?: GpuOptions.DEFAULT_PARTICLES,
                                steps ?: GpuOptions.DEFAULT_STEPS,
                                headless,
                                api,
                            ),
                            (seconds ?: GpuOptions.DEFAULT_SECONDS).toDouble(),
                        )
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
                                "A test takes over the screen in landscape. It always renders 1280 × 720.",
                                "测试开始后横屏占满屏幕。渲染尺寸固定为 1280 × 720。",
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
                            enabled = glesAvailable,
                            label = { Text("OpenGL ES 3.1") },
                        )
                        FilterChip(
                            selected = selectedBackend == "GLES2",
                            onClick = { selectedBackend = "GLES2" },
                            enabled = gles2Available,
                            label = { Text("OpenGL ES 2.0") },
                        )
                    }
                    // What the device has, as plain text: these are facts about it, not
                    // choices, and must not look like the chips above.
                    // The first line names the same three APIs as the chips, so that
                    // each one is said to be there or left out.
                    val offered = listOfNotNull(
                        if (caps.vulkanLoaderPresent) "Vulkan" else null,
                        if (glesAvailable) "OpenGL ES 3.1" else null,
                        if (gles2Available) "OpenGL ES 2.0" else null,
                    ).joinToString(tr(", ", "、")).ifEmpty { tr("none", "无") }
                    Text(
                        text = tr("This device supports: ", "本机支持：") + offered,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.padding(top = 8.dp),
                    )
                    Text(
                        text = tr("System: ", "系统：") + caps.androidRelease + " · " + caps.abi +
                            tr(" · driver's OpenGL ES version ", " · 驱动的 OpenGL ES 版本 ") + caps.glesVersionLabel,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    caps.notes.forEach { note ->
                        Text(
                            text = note,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.padding(top = 4.dp),
                        )
                    }
                }

                // Why "Run" is off, or what the last run left here; nothing otherwise.
                val notice = when {
                        // A custom value outside its range is why "Run" is off;
                        // that comes before what the last run left here.
                        !running && apiReady && testOnApi && !optionsValid -> tr(
                            "The custom value is not in its range.",
                            "自定义的数值不在允许的范围内。",
                        )
                        !running && apiReady && !testOnApi -> tr(
                            "This test is not available under the selected API. OpenGL ES 3.1 has the particle " +
                                "test and GPU Burn; OpenGL ES 2.0 has a GPU Burn of its own.",
                            "所选的图形 API 下没有这项测试。OpenGL ES 3.1 有粒子测试和 GPU Burn；OpenGL ES 2.0 只有它自己的 GPU Burn。",
                        )
                        statusText.isNotEmpty() -> statusText
                        !apiReady -> tr(
                            "A run needs Vulkan (Android 7.0 or later) or OpenGL ES.",
                            "运行需要 Vulkan（Android 7.0 以上）或 OpenGL ES。",
                        )
                        else -> ""
                    }
                if (notice.isNotEmpty()) {
                    Text(
                        text = notice,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            },
            secondary = {
                Column {
                    Text(tr("Workloads", "测试项目"), style = MaterialTheme.typography.titleMedium)
                    Text(
                        text = tr(
                            "A result is saved under the name shown below the test.",
                            "成绩按测试名下面那一行的名字保存。",
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
                                text = GpuOptions.savedAs(
                                    workload.id,
                                    steps ?: GpuOptions.DEFAULT_STEPS,
                                    seconds ?: GpuOptions.DEFAULT_SECONDS,
                                    api,
                                    caps.esParticleWorkGroupSize,
                                ),
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                            if (workload.id !in GpuOptions.testsOf(api)) {
                                Text(
                                    text = if (workload.id in GpuOptions.testsOf(GpuOptions.API_GLES)) {
                                        tr("Vulkan or OpenGL ES 3.1", "需要 Vulkan 或 OpenGL ES 3.1")
                                    } else {
                                        tr("Vulkan only", "仅 Vulkan")
                                    },
                                    style = MaterialTheme.typography.labelLarge,
                                    color = MaterialTheme.colorScheme.error,
                                    modifier = Modifier.padding(top = 4.dp),
                                )
                            }
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

                if (selectedWorkloadId != "membw") {
                    Card(
                        modifier = Modifier.fillMaxWidth(),
                        colors = CardDefaults.cardColors(
                            containerColor = MaterialTheme.colorScheme.surfaceContainerLow,
                        ),
                    ) {
                        Column(
                            modifier = Modifier.padding(14.dp),
                            verticalArrangement = Arrangement.spacedBy(12.dp),
                        ) {
                            Text(tr("Settings", "测试设置"), style = MaterialTheme.typography.titleMedium)
                            if (selectedWorkloadId == "stream") {
                                PresetChips(
                                    title = tr("Particles", "粒子数"),
                                    presets = GpuOptions.particlePresets,
                                    choice = particleChoice,
                                    onChoice = { particleChoice = it },
                                    custom = customParticles,
                                    onCustom = { customParticles = it },
                                    customLabel = tr("Custom particles", "自定义粒子数"),
                                    customHint = tr("multiple of 256", "256 的倍数"),
                                    valid = particles != null,
                                    enabled = !running,
                                )
                                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                                    FilterChip(
                                        selected = headless,
                                        onClick = { headless = !headless },
                                        enabled = !running,
                                        label = { Text("Headless") },
                                    )
                                    Text(
                                        tr(
                                            "Headless computes only: nothing is drawn and no picture is shown.",
                                            "Headless 只做计算：不渲染，也不显示画面。",
                                        ),
                                        style = MaterialTheme.typography.bodySmall,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    )
                                }
                            }
                            if (selectedWorkloadId == "gpu_burn") {
                                PresetChips(
                                    title = tr("GPU Burn steps", "GPU Burn 步数"),
                                    presets = GpuOptions.stepPresets,
                                    choice = stepChoice,
                                    onChoice = { stepChoice = it },
                                    custom = customSteps,
                                    onCustom = { customSteps = it },
                                    customLabel = tr("Custom steps", "自定义步数"),
                                    customHint = "16–2048",
                                    valid = steps != null,
                                    enabled = !running,
                                )
                            }
                            PresetChips(
                                title = tr("Duration", "运行时长"),
                                presets = GpuOptions.secondPresets,
                                choice = secondChoice,
                                onChoice = { secondChoice = it },
                                custom = customSeconds,
                                onCustom = { customSeconds = it },
                                customLabel = tr("Custom seconds", "自定义秒数"),
                                customHint = "1–3600",
                                valid = seconds != null,
                                enabled = !running,
                            )
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
                            if (GpuRun.workGroupSize > 0) {
                                val size = GpuRun.workGroupSize
                                Text(
                                    tr(
                                        "This device's OpenGL ES driver allows compute work groups of $size only, so " +
                                            "the particle test ran in groups of $size instead of 256. The result is " +
                                            "saved in a group of its own (its name has _wg$size) and is not one of " +
                                            "the results in groups of 256: on a GPU that runs both it read the same " +
                                            "at 65,536 particles and 2-3% higher at a million and more.",
                                        "这台设备的 OpenGL ES 驱动只支持 $size 线程的工作组，粒子测试改用 $size 分组运行" +
                                            "（标准是 256）。成绩单独保存为一组（名字带 _wg$size），不和 256 分组的成绩" +
                                            "混在一起：在两种都能跑的 GPU 上，65,536 粒子时两者相同，100 万以上时它高 2–3%。",
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

/**
 * One setting of a test: a row of presets with "Custom…" at its end, and a
 * number field while that is selected. The same choices as in the desktop app.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun PresetChips(
    title: String,
    presets: List<GpuOptions.Preset>,
    choice: Int,
    onChoice: (Int) -> Unit,
    custom: String,
    onCustom: (String) -> Unit,
    customLabel: String,
    customHint: String,
    valid: Boolean,
    enabled: Boolean,
) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Text(title, style = MaterialTheme.typography.titleSmall)
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            presets.forEach { preset ->
                FilterChip(
                    selected = choice == preset.value,
                    onClick = { onChoice(preset.value) },
                    enabled = enabled,
                    label = { Text(preset.label) },
                )
            }
            FilterChip(
                selected = choice == GpuOptions.CUSTOM,
                onClick = { onChoice(GpuOptions.CUSTOM) },
                enabled = enabled,
                label = { Text(tr("Custom…", "自定义…")) },
            )
        }
        if (choice == GpuOptions.CUSTOM) {
            OutlinedTextField(
                value = custom,
                onValueChange = { text -> onCustom(text.filter(Char::isDigit).take(9)) },
                label = { Text(customLabel) },
                placeholder = { Text(customHint) },
                isError = !valid,
                singleLine = true,
                enabled = enabled,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}
