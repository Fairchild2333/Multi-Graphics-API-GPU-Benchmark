package com.mangekyo.benchmark.core

import android.content.Context
import android.os.Build
import android.os.PowerManager
import android.os.SystemClock
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/** One application-wide queue; page navigation cannot lose the current job. */
object MobileSuite {
    var running by mutableStateOf(false); private set
    var status by mutableStateOf(""); private set
    var currentLabel by mutableStateOf(""); private set
    var total by mutableStateOf(0); private set
    var finished by mutableStateOf(0); private set
    var reviews by mutableStateOf(0); private set
    var failures by mutableStateOf(0); private set
    var cancelled by mutableStateOf(false); private set
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    private fun capabilities(): SuiteCapabilities = runCatching {
        val c = JSONObject(NativeBridge.suiteCapabilities())
        SuiteCapabilities(c.optBoolean("vulkan"), c.optBoolean("liquid"), c.optLong("vulkanMaxParticles"),
            c.optBoolean("gles31"), c.optLong("glesMaxParticles"), c.optBoolean("gles2"))
    }.getOrDefault(SuiteCapabilities())

    fun start(context: Context, selected: SuiteScope) {
        if (running || AutoRun.pending != null || NativeBridge.isRunning() || NativeBridge.isCliRunning() || !NativeBridge.isEngineLoaded) return
        NativeBridge.init(context)
        val jobs = supportedSuiteJobs(if (selected == SuiteScope.CPU) SuiteCapabilities() else capabilities(), selected)
        if (jobs.isEmpty()) { status = tr("No supported GPU jobs", "没有支持的 GPU 项目"); return }
        val app = context.applicationContext
        val root = app.getExternalFilesDir(null) ?: app.filesDir
        val folder = File(root, "suite-runs/${System.currentTimeMillis()}").apply { mkdirs() }
        if (!folder.isDirectory) { status = tr("Cannot prepare suite files", "无法准备测试记录文件"); return }
        running = true; cancelled = false; total = jobs.size; finished = 0; reviews = 0; failures = 0
        NativeBridge.outputOwner = "suite"
        val ledger = JSONArray()
        fun log(line: String) { File(folder, "run.log").appendText(line + "\n"); AutoRun.log(line) }
        fun saveLedger() { File(folder, "manifest.json").writeText(JSONObject().put("scope", selected.name)
            .put("total", total).put("finished", finished).put("review", reviews).put("failed", failures)
            .put("cooldownPolicy", "30s_pause_and_android29_thermal_when_available")
            .put("plannedJobIDs", JSONArray(jobs.map { it.id }))
            .put("cancelled", cancelled).put("jobs", ledger).toString(2)) }
        scope.launch {
            try {
                log("SUITE_PLAN\ttotal=$total\tscope=$selected")
                for ((index, job) in jobs.withIndex()) {
                    if (cancelled) break
                    currentLabel = job.label
                    // Fixed rest plus the OS thermal status where the API exists.
                    if (index > 0) for (second in 30 downTo 1) {
                        status = tr("Resting · $second s", "休息中 · $second 秒")
                        delay(1000); if (cancelled) break
                    }
                    if (cancelled) break
                    val power = app.getSystemService(Context.POWER_SERVICE) as? PowerManager
                    val coolingStart = SystemClock.elapsedRealtime()
                    var thermalTimeout = false
                    while (Build.VERSION.SDK_INT >= 29 && power != null &&
                        runCatching { power.currentThermalStatus > PowerManager.THERMAL_STATUS_NONE }.getOrDefault(false)) {
                        if (cancelled) break
                        if (SystemClock.elapsedRealtime() - coolingStart >= 600000) { thermalTimeout = true; break }
                        status = tr("Waiting for the device to cool", "正在等待设备降温")
                        delay(1000)
                    }
                    if (cancelled) break
                    log("SUITE_RUN\tindex=${index + 1}\ttotal=$total\tjob=${job.id}")
                    val thermalStart = if (Build.VERSION.SDK_INT >= 29 && power != null) runCatching { power.currentThermalStatus }.getOrDefault(-1) else -1
                    log("SUITE_THERMAL\tstart=$thermalStart\tunknown=-1")
                    val before = ResultsStore(app).loadEntries().map { it.id }.toSet()
                    var error = if (thermalTimeout) "thermal timeout" else ""
                    var usesCli = false
                    var started = false
                    if (!thermalTimeout) {
                        status = tr("Running ${index + 1}/$total", "运行中 ${index + 1}/$total")
                        NativeBridge.pollOutput()
                        if (job.cpu) {
                            usesCli = true
                            CpuRun.begin(job.workload == "cpu_membw", preserveCompute = index > 0 && jobs[index - 1].cpu)
                            val args = if (job.workload == "cpu_membw") arrayOf("--cpu-membw", "--cpu-membw-storage-dir", app.filesDir.absolutePath)
                                else arrayOf("--cpu-benchmark", "all", "--cpu-time", "15", "--cpu-warmup", "0.2")
                            started = NativeBridge.startCli(args)
                        } else {
                            GpuRun.begin()
                            usesCli = GpuOptions.runsOnCommandLine(job.request)
                            if (usesCli) started = NativeBridge.startCli(GpuOptions.commandLine(job.request, 15))
                            else {
                                GpuRun.fullscreen = !job.headless
                                if (!job.headless) delay(600) // let landscape/surface recreation settle
                                val startDeadline = SystemClock.elapsedRealtime() + 15000
                                do {
                                    if (cancelled) break
                                    started = NativeBridge.startWorkload(job.request, 15.0)
                                    if (started || cancelled) break
                                    delay(250)
                                } while (SystemClock.elapsedRealtime() < startDeadline)
                            }
                        }
                        if (!started) error = NativeBridge.lastError().ifEmpty { "start failed" }
                    }
                    val duration = if (job.cpu || job.workload == "membw")
                        maxOf(1800L, (Runtime.getRuntime().availableProcessors() + 1) * 45L + 120) else 180L
                    val deadline = SystemClock.elapsedRealtime() + duration * 1000
                    var stopDeadline = 0L
                    while (started && (NativeBridge.isRunning() || NativeBridge.isCliRunning())) {
                        for (line in NativeBridge.pollOutput()) {
                            log(line); if (job.cpu) CpuRun.consume(line) else GpuRun.consume(line)
                        }
                        if (cancelled || SystemClock.elapsedRealtime() >= deadline) {
                            NativeBridge.stopWorkload()
                            if (!cancelled) error = "worker timeout"
                            if (stopDeadline == 0L) stopDeadline = SystemClock.elapsedRealtime() + 30000
                            if (SystemClock.elapsedRealtime() >= stopDeadline)
                                throw IllegalStateException("worker did not stop; remaining jobs aborted")
                        }
                        delay(150)
                    }
                    for (line in NativeBridge.pollOutput()) {
                        log(line); if (job.cpu) CpuRun.consume(line) else GpuRun.consume(line)
                    }
                    if (job.cpu) CpuRun.finish(if (cancelled) 130 else if (usesCli) NativeBridge.cliExitCode() else -1)
                    GpuRun.active = false; GpuRun.fullscreen = false
                    if (cancelled) break
                    if (started && error.isEmpty()) {
                        if (usesCli && NativeBridge.cliExitCode() != 0) error = "worker exit ${NativeBridge.cliExitCode()}"
                        else if (!usesCli) error = NativeBridge.lastError()
                    }
                    val saved = ResultsStore(app).loadEntries().filter { it.id !in before && it.matches(job) }
                    if (saved.isEmpty() && error.isEmpty()) error = "no matching result saved"
                    val reasons = saved.flatMap { it.measurementReviewReasons }.distinct()
                    val verdict = if (error.isNotEmpty()) "failed" else if (reasons.isNotEmpty()) "review" else "success"
                    if (verdict == "failed") failures++ else if (verdict == "review") reviews++
                    finished++
                    ledger.put(JSONObject().put("id", job.id).put("status", verdict).put("error", error)
                        .put("thermalStart", thermalStart).put("thermalEnd", if (Build.VERSION.SDK_INT >= 29 && power != null)
                            runCatching { power.currentThermalStatus }.getOrDefault(-1) else -1)
                        .put("reviewReasons", JSONArray(reasons)).put("resultIDs", JSONArray(saved.map { it.id })))
                    log("SUITE_RESULT\tstatus=$verdict\tjob=${job.id}\treason=$error ${reasons.joinToString()}")
                    saveLedger()
                }
                status = if (cancelled) tr("Cancelled · $finished/$total", "已取消 · $finished/$total")
                    else tr("Completed $finished/$total · review $reviews · failed $failures", "完成 $finished/$total · 待复核 $reviews · 失败 $failures")
            } catch (e: Exception) {
                failures++; status = tr("Queue stopped: ${e.message}", "队列已停止：${e.message}")
                runCatching { log("SUITE_ERROR\treason=${e.message}") }
            } finally {
                GpuRun.fullscreen = false; GpuRun.active = false
                if (!NativeBridge.isCliRunning()) CpuRun.running = false
                running = false; NativeBridge.outputOwner = ""
                runCatching { saveLedger() }.onFailure { status = tr("Cannot save suite log; scores remain in history", "测试记录保存失败，请查看成绩历史") }
            }
        }
    }
    fun cancel() {
        if (!running) return
        cancelled = true; status = tr("Cancelling…", "正在取消…"); NativeBridge.stopWorkload()
    }
}
