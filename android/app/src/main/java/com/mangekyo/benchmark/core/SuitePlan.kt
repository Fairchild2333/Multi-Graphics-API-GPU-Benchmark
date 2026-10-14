package com.mangekyo.benchmark.core

enum class SuiteScope { ALL, GPU, CPU }
data class SuiteCapabilities(
    val vulkan: Boolean = false, val liquid: Boolean = false,
    val vulkanMaxParticles: Long = 0, val gles31: Boolean = false,
    val glesMaxParticles: Long = 0, val gles2: Boolean = false,
)
data class SuiteJob(
    val id: String, val api: String, val workload: String,
    val particles: Int = 0, val steps: Int = 0, val headless: Boolean = false,
) {
    val cpu: Boolean get() = api == "cpu"
    val request: String get() = GpuOptions.request(workload, particles, steps, headless, api)
    val label: String get() {
        if (cpu) return if (workload == "cpu_all") tr("CPU compute — single + all-core", "CPU 算力——单核与全核")
            else tr("CPU cache, memory and storage", "CPU 缓存、内存与存储")
        val backend = when (api) { "gles" -> "OpenGL ES 3.1"; "gles2" -> "OpenGL ES 2.0"; else -> "Vulkan" }
        val test = when (workload) {
            "stream" -> tr("Particles $particles", "粒子 $particles") + if (headless) tr(" (compute only)", "（纯计算）") else ""
            "gpu_burn" -> "GPU Burn $steps"
            "membw" -> tr("GPU memory", "GPU 内存")
            else -> tr("Fluid", "流体")
        }
        return "$backend · $test"
    }
}

fun supportedSuiteJobs(caps: SuiteCapabilities, scope: SuiteScope): List<SuiteJob> = buildList {
    if (scope != SuiteScope.CPU) {
        for (api in listOf("vulkan", "gles", "gles2")) {
            val available = when (api) { "vulkan" -> caps.vulkan; "gles" -> caps.gles31; else -> caps.gles2 }
            if (!available) continue
            val maximum = if (api == "vulkan") caps.vulkanMaxParticles else caps.glesMaxParticles
            if (api != "gles2") for (headless in listOf(false, true)) {
                for (count in listOf(65536, 1048576, 4194304, 16777216)) {
                    if (count > maximum) continue
                    add(SuiteJob("$api-stream-$count-$headless", api, "stream", count, headless = headless))
                }
            }
            for (steps in listOf(16, 64, 256)) add(SuiteJob("$api-burn-$steps", api, "gpu_burn", steps = steps))
            if (api == "vulkan") {
                if (caps.liquid) add(SuiteJob("vulkan-liquid", api, "cinematic_liquid"))
                add(SuiteJob("vulkan-memory", api, "membw", headless = true))
            }
        }
    }
    if (scope != SuiteScope.GPU) {
        add(SuiteJob("cpu-compute", "cpu", "cpu_all", headless = true))
        add(SuiteJob("cpu-memory", "cpu", "cpu_membw", headless = true))
    }
}
