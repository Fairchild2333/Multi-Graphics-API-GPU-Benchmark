package com.mangekyo.benchmark.core

/**
 * The size of a GPU test, chosen on the GPU page as in the desktop app: the
 * same presets, the same names, the same limits.
 *
 * The engine host takes the choice as part of the name of the test, for
 * example "stream?particles=4194304" or "gpu_burn?steps=64"
 * (src/embedded_request.h).
 */
object GpuOptions {
    /** A preset: its value, and its name in English and Chinese. */
    data class Preset(val value: Int, val nameEn: String, val nameZh: String) {
        val label: String get() = tr(nameEn, nameZh)
    }

    /** Stands for "Custom…" in a row of presets. */
    const val CUSTOM = -1

    val particlePresets = listOf(
        Preset(65536, "Light — 65K", "轻量 —— 65K"),
        Preset(1048576, "Medium — 1M", "中等 —— 1M"),
        Preset(4194304, "Heavy — 4M", "重载 —— 4M"),
        Preset(16777216, "Extreme — 16M", "极限 —— 16M"),
    )
    val stepPresets = listOf(
        Preset(16, "Light — 16", "轻量 — 16"),
        Preset(64, "Medium — 64", "中等 — 64"),
        Preset(256, "Heavy — 256", "重载 — 256"),
    )
    val secondPresets = listOf(
        Preset(3, "3 s", "3 秒"),
        Preset(15, "15 s", "15 秒"),
        Preset(60, "60 s", "60 秒"),
    )

    // What the desktop app selects at first.
    const val DEFAULT_PARTICLES = 1048576
    const val DEFAULT_STEPS = 16
    const val DEFAULT_SECONDS = 15

    val particleRange = 1..67108864
    val stepRange = 16..2048
    val secondRange = 1..3600

    /** The value of a choice: the preset, or the number typed for "Custom…"; null when that is not valid. */
    fun resolve(choice: Int, custom: String, range: IntRange): Int? =
        if (choice != CUSTOM) choice else custom.trim().toIntOrNull()?.takeIf { it in range }

    /**
     * What the engine is asked to run. A headless particle test computes only:
     * it shows no picture and runs through the command-line entry.
     */
    fun request(testId: String, particles: Int, steps: Int, headless: Boolean = false): String = when (testId) {
        "stream" -> (if (headless) "stream_headless" else "stream") + "?particles=$particles"
        "gpu_burn" -> "gpu_burn?steps=$steps"
        else -> testId
    }

    /** The arguments of a test that runs through the engine's command-line entry. */
    fun commandLine(request: String, seconds: Int): Array<String> {
        val id = request.substringBefore('?')
        if (id == "stream_headless") {
            val particles = parse(request).particles ?: DEFAULT_PARTICLES
            return arrayOf(
                "--backend", "vulkan", "--gpu", "0", "--workload", "stream", "--particles", particles.toString(),
                "--headless", "--time", seconds.toString(), "--no-renderdoc",
            )
        }
        return arrayOf("--workload", id, "--backend", "vulkan", "--gpu", "0")
    }

    /** The name the engine saves a run under. */
    fun savedAs(testId: String, steps: Int, seconds: Int): String = when (testId) {
        "stream" -> "stream_v1_android_preview"
        "gpu_burn" -> "gpu_burn_v3_fixed_steps_${steps}_kaleidoscope_android_preview"
        "cinematic_liquid" ->
            if (seconds == 15) "cinematic_liquid_v2_physical_scene_v8_android_preview"
            else "cinematic_liquid_v2_physical_scene_v8_preview_android_preview"
        "membw" -> "membw_v1_android_preview"
        else -> testId
    }

    /** A request as adb names it, taken apart: the test of the page, and the size it asks for. */
    data class Parsed(val testId: String, val particles: Int?, val steps: Int?, val headless: Boolean = false)

    fun parse(request: String): Parsed {
        val id = request.substringBefore('?')
        val options = request.substringAfter('?', "").split('&').mapNotNull { pair ->
            val name = pair.substringBefore('=')
            val value = pair.substringAfter('=', "").toIntOrNull()
            if (value == null) null else name to value
        }.toMap()
        return when (id) {
            "stream", "particle" -> Parsed("stream", options["particles"] ?: DEFAULT_PARTICLES, null)
            "stream_heavy" -> Parsed("stream", options["particles"] ?: 1048576, null)
            "stream_headless" -> Parsed("stream", options["particles"] ?: DEFAULT_PARTICLES, null, headless = true)
            "gpu_burn" -> Parsed("gpu_burn", null, options["steps"] ?: DEFAULT_STEPS)
            "gpu_burn_medium" -> Parsed("gpu_burn", null, options["steps"] ?: 64)
            "gpu_burn_heavy" -> Parsed("gpu_burn", null, options["steps"] ?: 256)
            else -> Parsed(id, null, null)
        }
    }
}
