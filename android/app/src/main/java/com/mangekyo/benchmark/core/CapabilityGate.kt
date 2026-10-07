package com.mangekyo.benchmark.core

import android.app.ActivityManager
import android.content.Context
import android.content.pm.ConfigurationInfo
import android.content.pm.PackageManager
import android.os.Build

/**
 * Capability gate (HANDOFF Android contract).
 * Missing capability → explicit unsupported, never silent fallback.
 */
enum class GraphicsBackend {
    Vulkan,
    Gles31,
    Unsupported,
}

data class CapabilitySnapshot(
    val vulkanLoaderPresent: Boolean,
    val vulkanHardwareFeature: Boolean,
    val glesVersionLabel: String,
    val glesMajor: Int,
    val glesMinor: Int,
    /** Work-group size of the particle test under OpenGL ES: 256, 128, or 0 when not known. */
    val esParticleWorkGroupSize: Int,
    val preferredBackend: GraphicsBackend,
    val abi: String,
    val androidRelease: String,
    val notes: List<String>,
)

object CapabilityGate {
    fun vulkanAvailable(): Boolean =
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && NativeBridge.probeVulkan()

    fun snapshot(context: Context): CapabilitySnapshot {
        val pm = context.packageManager
        val vulkanHw = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            pm.hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_LEVEL) ||
                pm.hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_COMPUTE)
        } else {
            false
        }
        val loader = vulkanAvailable()
        val (glesMajor, glesMinor, glesLabel) = glesVersion(context)
        val notes = mutableListOf<String>()

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            notes += tr("Vulkan loader requires API 24+", "Vulkan 加载器需要 Android 7.0（API 24）以上")
        } else if (!loader) {
            notes += tr(
                "No Vulkan device: no driver, or libvulkan.so cannot be loaded",
                "没有可用的 Vulkan 设备（没有驱动，或无法加载 libvulkan.so）",
            )
        }
        if (!vulkanHw) {
            notes += tr("No Vulkan hardware feature advertised", "设备没有声明 Vulkan 硬件特性")
        }

        val preferred = when {
            loader -> GraphicsBackend.Vulkan
            glesMajor >= 3 && glesMinor >= 1 -> GraphicsBackend.Gles31
            glesMajor >= 3 -> {
                notes += tr(
                    "GL ES $glesLabel present but 3.1+ required for main GLES path",
                    "设备支持 GL ES $glesLabel，但 GL ES 路径需要 3.1 以上",
                )
                GraphicsBackend.Unsupported
            }
            else -> GraphicsBackend.Unsupported
        }
        if (preferred == GraphicsBackend.Gles31) {
            notes += tr("Using GL ES path (Vulkan unavailable)", "Vulkan 不可用，改用 GL ES 路径")
        }
        if (preferred == GraphicsBackend.Unsupported) {
            notes += tr("No supported GPU backend on this device", "这台设备没有受支持的图形后端")
        }

        return CapabilitySnapshot(
            vulkanLoaderPresent = loader,
            vulkanHardwareFeature = vulkanHw,
            glesVersionLabel = glesLabel,
            glesMajor = glesMajor,
            glesMinor = glesMinor,
            esParticleWorkGroupSize =
                if (glesMajor > 3 || (glesMajor == 3 && glesMinor >= 1)) NativeBridge.esParticleWorkGroupSize() else 0,
            preferredBackend = preferred,
            abi = DeviceInfo.primaryAbi(),
            androidRelease = "Android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})",
            notes = notes,
        )
    }

    private fun glesVersion(context: Context): Triple<Int, Int, String> {
        val am = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager
        val info: ConfigurationInfo? = am?.deviceConfigurationInfo
        val encoded = info?.reqGlEsVersion ?: 0
        val major = encoded shr 16
        val minor = encoded and 0xffff
        val label = if (encoded != 0) "$major.$minor" else "unknown"
        return Triple(major, minor, label)
    }
}
