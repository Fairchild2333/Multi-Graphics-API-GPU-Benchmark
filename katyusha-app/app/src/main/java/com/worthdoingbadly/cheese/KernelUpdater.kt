package com.worthdoingbadly.cheese

import android.content.Context
import android.os.Build
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.net.HttpURLConnection
import java.net.URL

data class UpdateCheckResult(
    val hasUpdate: Boolean,
    val installedManagerVersion: String?,
    val installedManagerCode: Int?,
    val currentModuleVersion: String,
    val currentModuleCode: Int,
    val targetTag: String?,
    val downloadUrl: String?,
    val ksudDownloadUrl: String?,
    val kmi: String
)

object KernelUpdater {
    // Current bundled baseline version
    const val BUNDLED_VERSION_NAME = "v3.3.1"
    const val BUNDLED_VERSION_CODE = 32680

    private const val GITHUB_REPO = "tiann/KernelSU"
    private const val GITHUB_API_LATEST = "https://api.github.com/repos/$GITHUB_REPO/releases/latest"

    fun detectKmi(): String {
        return try {
            val unameProcess = Runtime.getRuntime().exec(arrayOf("uname", "-r"))
            val release = unameProcess.inputStream.bufferedReader().readLine()?.trim() ?: ""
            // Expected format: e.g. 5.10.101-android12-9-gece0cc04d1a5
            val kver = release.split("-").firstOrNull()?.split(".")?.take(2)?.joinToString(".") ?: "5.10"
            val averMatch = Regex("android[0-9]+").find(release)?.value ?: "android12"
            "$averMatch-$kver"
        } catch (_: Exception) {
            "android12-5.10"
        }
    }

    fun getInstalledManagerInfo(context: Context): Pair<String, Int>? {
        val packages = listOf("me.weishu.kernelsu", "io.github.a13e300.ksu")
        for (pkg in packages) {
            try {
                val pInfo = context.packageManager.getPackageInfo(pkg, 0)
                val versionName = pInfo.versionName ?: ""
                @Suppress("DEPRECATION")
                val versionCode = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                    pInfo.longVersionCode.toInt()
                } else {
                    pInfo.versionCode
                }
                return Pair(versionName, versionCode)
            } catch (_: Exception) {}
        }
        return null
    }

    fun getCurrentModuleInfo(context: Context): Pair<String, Int> {
        val infoFile = File(context.filesDir, "ksu_updates/version.info")
        if (infoFile.exists()) {
            try {
                val lines = infoFile.readLines()
                if (lines.size >= 2) {
                    val name = lines[0].trim()
                    val code = lines[1].trim().toIntOrNull() ?: BUNDLED_VERSION_CODE
                    return Pair(name, code)
                }
            } catch (_: Exception) {}
        }
        return Pair(BUNDLED_VERSION_NAME, BUNDLED_VERSION_CODE)
    }

    suspend fun checkForUpdates(context: Context): UpdateCheckResult = withContext(Dispatchers.IO) {
        val kmi = detectKmi()
        val currentMod = getCurrentModuleInfo(context)
        val managerInfo = getInstalledManagerInfo(context)

        // 1. If KernelSU Manager is installed and has a higher build than current module, match the manager!
        if (managerInfo != null && managerInfo.second > currentMod.second) {
            val targetTag = if (managerInfo.first.startsWith("v")) managerInfo.first else "v${managerInfo.first}"
            val downloadUrl = "https://github.com/$GITHUB_REPO/releases/download/$targetTag/lkm-aarch64-${kmi}_kernelsu.ko"
            val ksudUrl = "https://github.com/$GITHUB_REPO/releases/download/$targetTag/ksud-aarch64-linux-android"
            return@withContext UpdateCheckResult(
                hasUpdate = true,
                installedManagerVersion = managerInfo.first,
                installedManagerCode = managerInfo.second,
                currentModuleVersion = currentMod.first,
                currentModuleCode = currentMod.second,
                targetTag = targetTag,
                downloadUrl = downloadUrl,
                ksudDownloadUrl = ksudUrl,
                kmi = kmi
            )
        }

        // 2. Query GitHub Releases for the absolute latest version
        try {
            val url = URL(GITHUB_API_LATEST)
            val conn = (url.openConnection() as HttpURLConnection).apply {
                connectTimeout = 5000
                readTimeout = 5000
                setRequestProperty("User-Agent", "Cheese-Katyusha-Updater")
                setRequestProperty("Accept", "application/vnd.github.v3+json")
            }
            if (conn.responseCode == 200) {
                val responseText = conn.inputStream.bufferedReader().use { it.readText() }
                val json = JSONObject(responseText)
                val tagName = json.optString("tag_name", "")
                val assets = json.optJSONArray("assets")

                var koUrl: String? = null
                var ksudUrl: String? = null
                val targetKoName = "lkm-aarch64-${kmi}_kernelsu.ko"
                val targetKsudName = "ksud-aarch64-linux-android"

                if (assets != null) {
                    for (i in 0 until assets.length()) {
                        val asset = assets.getJSONObject(i)
                        val name = asset.optString("name")
                        val dl = asset.optString("browser_download_url")
                        if (name == targetKoName) koUrl = dl
                        if (name == targetKsudName) ksudUrl = dl
                    }
                }

                // If online tag is different from current module version and we found the assets
                if (tagName.isNotEmpty() && tagName != currentMod.first && koUrl != null) {
                    return@withContext UpdateCheckResult(
                        hasUpdate = true,
                        installedManagerVersion = managerInfo?.first,
                        installedManagerCode = managerInfo?.second,
                        currentModuleVersion = currentMod.first,
                        currentModuleCode = currentMod.second,
                        targetTag = tagName,
                        downloadUrl = koUrl,
                        ksudDownloadUrl = ksudUrl,
                        kmi = kmi
                    )
                }
            }
        } catch (_: Exception) {}

        UpdateCheckResult(
            hasUpdate = false,
            installedManagerVersion = managerInfo?.first,
            installedManagerCode = managerInfo?.second,
            currentModuleVersion = currentMod.first,
            currentModuleCode = currentMod.second,
            targetTag = null,
            downloadUrl = null,
            ksudDownloadUrl = null,
            kmi = kmi
        )
    }

    suspend fun downloadUpdate(
        context: Context,
        result: UpdateCheckResult,
        onProgress: (String) -> Unit
    ): Boolean = withContext(Dispatchers.IO) {
        val koUrl = result.downloadUrl ?: return@withContext false
        val updatesDir = File(context.filesDir, "ksu_updates").apply { mkdirs() }

        try {
            onProgress(tr("Downloading KernelSU module: ", "正在下载内核模块: ") + "lkm-aarch64-${result.kmi}_kernelsu.ko...")
            downloadFile(koUrl, File(updatesDir, "kernelsu.ko"))
            // Also save with KMI specific name
            File(updatesDir, "kernelsu.ko").copyTo(File(updatesDir, "lkm-aarch64-${result.kmi}_kernelsu.ko"), overwrite = true)

            if (result.ksudDownloadUrl != null) {
                onProgress(tr("Downloading updated ksud daemon...", "正在下载匹配的 ksud 守护进程..."))
                val ksudFile = File(updatesDir, "ksud")
                downloadFile(result.ksudDownloadUrl, ksudFile)
                ksudFile.setExecutable(true, false)
            }

            // Record version metadata
            val infoFile = File(updatesDir, "version.info")
            val targetTag = result.targetTag ?: "latest"
            val targetCode = result.installedManagerCode ?: (result.currentModuleCode + 1)
            infoFile.writeText("$targetTag\n$targetCode\n${result.kmi}\n")

            onProgress(tr("Update installed successfully! Effective upon next activation.", "内核组件已更新成功！下次激活时生效。"))
            true
        } catch (e: Exception) {
            onProgress(tr("Download failed: ", "下载失败: ") + e.message)
            false
        }
    }

    private fun downloadFile(urlStr: String, destination: File) {
        // Try direct first, if failed try with ghproxy
        val urlsToTry = listOf(
            urlStr,
            "https://ghproxy.net/$urlStr"
        )
        var success = false
        var lastEx: Exception? = null

        for (u in urlsToTry) {
            try {
                val conn = (URL(u).openConnection() as HttpURLConnection).apply {
                    connectTimeout = 8000
                    readTimeout = 15000
                    instanceFollowRedirects = true
                    setRequestProperty("User-Agent", "Cheese-Katyusha-Updater")
                }
                if (conn.responseCode in 200..299) {
                    conn.inputStream.use { input ->
                        FileOutputStream(destination).use { output ->
                            input.copyTo(output)
                        }
                    }
                    success = true
                    break
                }
            } catch (e: Exception) {
                lastEx = e
            }
        }
        if (!success) {
            throw lastEx ?: RuntimeException("Unable to download from $urlStr")
        }
    }

    fun applyDownloadedUpdates(context: Context, extractedDir: File, log: (String) -> Unit) {
        val updatesDir = File(context.filesDir, "ksu_updates")
        if (!updatesDir.exists()) return

        val infoFile = File(updatesDir, "version.info")
        val infoTag = if (infoFile.exists()) infoFile.readLines().firstOrNull()?.trim() ?: "" else ""

        val updatedFiles = updatesDir.listFiles { _, name -> name.endsWith(".ko") || name == "ksud" } ?: return
        for (f in updatedFiles) {
            val target = File(extractedDir, f.name)
            try {
                f.copyTo(target, overwrite = true)
                target.setExecutable(true, false)
                target.setReadable(true, false)
                log("[*] " + tr("Applied updated KernelSU component: ", "载入已更新的 KernelSU 组件: ") + "${f.name} ($infoTag)")
            } catch (e: Exception) {
                log("[!] " + tr("Failed to apply update for ", "载入更新组件失败: ") + "${f.name}: ${e.message}")
            }
        }
    }
}
