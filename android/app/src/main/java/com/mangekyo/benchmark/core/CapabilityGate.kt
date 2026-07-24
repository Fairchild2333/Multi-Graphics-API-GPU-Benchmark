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
            notes += "Vulkan loader requires API 24+"
        } else if (!loader) {
            notes += "libvulkan.so not loadable"
        }
        if (!vulkanHw) {
            notes += "No Vulkan hardware feature advertised"
        }

        val preferred = when {
            loader -> GraphicsBackend.Vulkan
            glesMajor >= 3 && glesMinor >= 1 -> GraphicsBackend.Gles31
            glesMajor >= 3 -> {
                notes += "GL ES $glesLabel present but 3.1+ required for main GLES path"
                GraphicsBackend.Unsupported
            }
            else -> GraphicsBackend.Unsupported
        }
        if (preferred == GraphicsBackend.Gles31) {
            notes += "Using GL ES path (Vulkan unavailable)"
        }
        if (preferred == GraphicsBackend.Unsupported) {
            notes += "No supported GPU backend on this device"
        }

        return CapabilitySnapshot(
            vulkanLoaderPresent = loader,
            vulkanHardwareFeature = vulkanHw,
            glesVersionLabel = glesLabel,
            glesMajor = glesMajor,
            glesMinor = glesMinor,
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
