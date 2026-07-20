// Mangekyo Android 主包（Vulkan / GL ES 3.1+ 主路径）。
// legacy APK（Tegra 3/4, ES 2.0）是独立工程，不放在本目录，见 HANDOFF 目标 C 第 3 条。
plugins {
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.kotlin.android) apply false
    alias(libs.plugins.kotlin.compose) apply false
}
