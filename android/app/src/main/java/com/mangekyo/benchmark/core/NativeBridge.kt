package com.mangekyo.benchmark.core

import android.content.Context
import android.view.Surface
import java.io.File
import java.io.FileOutputStream

/**
 * JNI bridge to libmangekyo_jni (gpu_engine on a worker thread, with the
 * Vulkan or the OpenGL ES backend).
 * Formal score contract still preview — results use android path + preview versions.
 */
object NativeBridge {
    private val loaded: Boolean = try {
        System.loadLibrary("mangekyo_jni")
        true
    } catch (e: UnsatisfiedLinkError) {
        // Say why: on Android 6.0 this failed silently for a missing system
        // library, and every test was refused as "engine missing".
        android.util.Log.e("Mangekyo", "The engine library did not load: ${e.message}")
        false
    }

    @Volatile
    private var pathsReady: Boolean = false

    val isEngineLoaded: Boolean get() = loaded

    fun engineVersion(): String =
        if (loaded) nativeEngineVersion() else "engine-not-loaded"

    fun probeVulkan(): Boolean = loaded && nativeProbeVulkan()
    fun suiteCapabilities(): String = if (loaded && !isRunning() && !isCliRunning()) nativeSuiteCapabilities() else "{}"

    /**
     * The size of the compute work groups the particle test uses under OpenGL
     * ES here: 256, 128 where the driver allows no more, 0 where it cannot run
     * or while a test is running.
     */
    fun esParticleWorkGroupSize(): Int = if (loaded) nativeEsParticleWorkGroupSize() else 0

    fun isRunning(): Boolean = loaded && nativeIsRunning()

    fun lastError(): String = if (loaded) nativeLastError() else "engine-not-loaded"

    fun init(context: Context) {
        if (!loaded || pathsReady) return
        val shaderDir = File(context.filesDir, "shaders").apply { mkdirs() }
        // The shaders are unpacked again after every install or update: a file
        // left from an older build would otherwise stay in use under its name.
        val stamp = File(shaderDir, ".unpacked-for")
        val installed = runCatching {
            context.packageManager.getPackageInfo(context.packageName, 0).lastUpdateTime.toString()
        }.getOrDefault("")
        val current = installed.isNotEmpty() && runCatching { stamp.readText() }.getOrDefault("") == installed
        copyAssetDir(context, "shaders", shaderDir, overwrite = !current)
        if (!current && installed.isNotEmpty()) runCatching { stamp.writeText(installed) }
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

    private fun copyAssetDir(context: Context, assetPath: String, outDir: File, overwrite: Boolean) {
        val names = context.assets.list(assetPath) ?: return
        for (name in names) {
            val childAsset = if (assetPath.isEmpty()) name else "$assetPath/$name"
            val children = context.assets.list(childAsset)
            if (children != null && children.isNotEmpty()) {
                copyAssetDir(context, childAsset, File(outDir, name).apply { mkdirs() }, overwrite)
            } else {
                val out = File(outDir, name)
                if (!overwrite && out.exists() && out.length() > 0L) continue
                context.assets.open(childAsset).use { input ->
                    FileOutputStream(out).use { output -> input.copyTo(output) }
                }
            }
        }
    }

    private external fun nativeEngineVersion(): String
    private external fun nativeProbeVulkan(): Boolean
    private external fun nativeSuiteCapabilities(): String
    private external fun nativeEsParticleWorkGroupSize(): Int
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
