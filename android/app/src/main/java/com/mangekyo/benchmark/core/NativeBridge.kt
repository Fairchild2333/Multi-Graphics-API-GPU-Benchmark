package com.mangekyo.benchmark.core

import android.content.Context
import android.view.Surface
import java.io.File
import java.io.FileOutputStream

/**
 * JNI bridge to libmangekyo_jni (gpu_engine / VulkanBackend on a worker thread).
 * Formal score contract still preview — results use android path + preview versions.
 */
object NativeBridge {
    private val loaded: Boolean = try {
        System.loadLibrary("mangekyo_jni")
        true
    } catch (_: UnsatisfiedLinkError) {
        false
    }

    @Volatile
    private var pathsReady: Boolean = false

    val isEngineLoaded: Boolean get() = loaded

    fun engineVersion(): String =
        if (loaded) nativeEngineVersion() else "engine-not-loaded"

    fun probeVulkan(): Boolean = loaded && nativeProbeVulkan()

    fun isRunning(): Boolean = loaded && nativeIsRunning()

    fun lastError(): String = if (loaded) nativeLastError() else "engine-not-loaded"

    fun init(context: Context) {
        if (!loaded || pathsReady) return
        val shaderDir = File(context.filesDir, "shaders").apply { mkdirs() }
        copyAssetDir(context, "shaders", shaderDir)
        val dataDir = context.getExternalFilesDir(null) ?: context.filesDir
        File(dataDir, "results").mkdirs()
        nativeInitPaths(shaderDir.absolutePath, dataDir.absolutePath)
        pathsReady = true
    }

    fun setSurface(surface: Surface?) {
        if (loaded) nativeSetSurface(surface)
    }

    fun resizeSurface(width: Int, height: Int) {
        if (loaded) nativeResizeSurface(width, height)
    }

    fun startWorkload(workloadId: String, seconds: Double = 3.0): Boolean {
        if (!loaded || !pathsReady) return false
        return nativeStartWorkload(workloadId, seconds)
    }

    fun stopWorkload() {
        if (loaded) nativeStopWorkload()
    }

    /**
     * The page that started the current run, "gpu" or "cpu". Only that page
     * collects the lines the engine prints.
     */
    @Volatile
    var outputOwner: String = ""

    /** Lines the engine printed since the last call. */
    fun pollOutput(): Array<String> = if (loaded) nativePollOutput() else emptyArray()

    /**
     * Runs the engine's command-line entry point with [args] on a native
     * thread. Used for the CPU tests, which need no surface.
     */
    fun startCli(args: Array<String>): Boolean {
        if (!loaded || !pathsReady) return false
        return nativeStartCli(args)
    }

    fun isCliRunning(): Boolean = loaded && nativeIsCliRunning()

    fun cliExitCode(): Int = if (loaded) nativeCliExitCode() else -1

    private fun copyAssetDir(context: Context, assetPath: String, outDir: File) {
        val names = context.assets.list(assetPath) ?: return
        for (name in names) {
            val childAsset = if (assetPath.isEmpty()) name else "$assetPath/$name"
            val children = context.assets.list(childAsset)
            if (children != null && children.isNotEmpty()) {
                copyAssetDir(context, childAsset, File(outDir, name).apply { mkdirs() })
            } else {
                val out = File(outDir, name)
                if (out.exists() && out.length() > 0L) continue
                context.assets.open(childAsset).use { input ->
                    FileOutputStream(out).use { output -> input.copyTo(output) }
                }
            }
        }
    }

    private external fun nativeEngineVersion(): String
    private external fun nativeProbeVulkan(): Boolean
    private external fun nativeInitPaths(shaderDir: String, dataDir: String)
    private external fun nativeSetSurface(surface: Surface?)
    private external fun nativeResizeSurface(width: Int, height: Int)
    private external fun nativeStartWorkload(workloadId: String, seconds: Double): Boolean
    private external fun nativeStopWorkload()
    private external fun nativeIsRunning(): Boolean
    private external fun nativeLastError(): String
    private external fun nativePollOutput(): Array<String>
    private external fun nativeStartCli(args: Array<String>): Boolean
    private external fun nativeIsCliRunning(): Boolean
    private external fun nativeCliExitCode(): Int
}
