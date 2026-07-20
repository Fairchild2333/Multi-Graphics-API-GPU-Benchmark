package com.mangekyo.benchmark.core

import android.view.Surface

/**
 * JNI 桥。stub 阶段：libmangekyo_jni 只返回占位值；引擎接入见 cpp/CMakeLists.txt。
 *
 * TODO(next-ai)：
 *  - startWorkload(id, backend, params) / cancel() / 进度与实时指标回调（JNI → Kotlin Flow）；
 *  - 结果以 JSON 字符串返回，schema 与 Windows 侧 results.json 完全一致（同一份合同生成）；
 *  - metadata 必须含真实 ABI、SoC、驱动、Android 版本、温控事件。
 */
object NativeBridge {
    private val loaded: Boolean = try {
        System.loadLibrary("mangekyo_jni")
        true
    } catch (_: UnsatisfiedLinkError) {
        false
    }

    val isEngineLoaded: Boolean get() = loaded

    fun engineVersion(): String = if (loaded) nativeEngineVersion() else "engine-not-loaded"

    fun probeVulkan(): Boolean = loaded && nativeProbeVulkan()

    fun setSurface(surface: Surface?) {
        if (loaded) nativeSetSurface(surface)
    }

    private external fun nativeEngineVersion(): String
    private external fun nativeProbeVulkan(): Boolean
    private external fun nativeSetSurface(surface: Surface?)
}
