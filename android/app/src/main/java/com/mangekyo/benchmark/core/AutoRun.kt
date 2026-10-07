package com.mangekyo.benchmark.core

import android.content.Intent
import android.util.Log
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

/**
 * Starts a test named on the command line, so a phone can be tested over adb
 * without touching it:
 *
 *     adb shell am start -S -n com.mangekyo.benchmark/.MainActivity --es run cpu_membw
 *
 * Tests: cpu_membw, cpu_all, cpu_per_core, cpu_multi, stream, gpu_burn. The
 * optional `--ei seconds N` sets the seconds per CPU compute test or the
 * length of a GPU run. A GPU test may name its size and OpenGL ES:
 * "stream?particles=4194304&api=gles". Debug builds only: in a release build
 * any other app on the phone could start a run this way.
 *
 * Progress goes to logcat: `adb logcat -s Mangekyo MangekyoEngine`.
 */
object AutoRun {
    data class Request(val test: String, val seconds: Int)

    private const val TAG = "Mangekyo"

    /** CPU tests and the mode each one selects on the CPU page. */
    val cpuModes = mapOf(
        "cpu_membw" to "memory",
        "cpu_all" to "all",
        "cpu_per_core" to "per-core",
        "cpu_multi" to "multi",
    )

    val gpuTests = setOf(
        "stream", "stream_heavy", "stream_headless", "gpu_burn", "gpu_burn_medium", "gpu_burn_heavy",
        "cinematic_liquid", "membw",
    )

    /** The request that no page has started yet. */
    var pending by mutableStateOf<Request?>(null)

    fun accept(intent: Intent?, debuggable: Boolean) {
        val test = intent?.getStringExtra("run") ?: return
        // A recreated activity gets the same intent again and must not rerun it.
        intent.removeExtra("run")
        if (!debuggable) {
            log("AUTORUN refused test=$test reason=release_build")
            return
        }
        // A GPU test may carry its size: "stream?particles=4194304", "gpu_burn?steps=64".
        if (test !in cpuModes && test.substringBefore('?') !in gpuTests) {
            log("AUTORUN refused test=$test reason=unknown_test")
            return
        }
        pending = Request(test, intent.getIntExtra("seconds", 0))
        log("AUTORUN accepted test=$test")
    }

    /** One line in logcat for a script to wait on. */
    fun log(message: String) {
        Log.i(TAG, message)
    }
}
