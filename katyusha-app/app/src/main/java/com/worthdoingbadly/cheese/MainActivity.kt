package com.worthdoingbadly.cheese

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.view.WindowManager
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.CloudSync
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.DeleteSweep
import androidx.compose.material.icons.filled.Fullscreen
import androidx.compose.material.icons.filled.FullscreenExit
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material.icons.filled.PhoneAndroid
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Save
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Terminal
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Badge
import androidx.compose.material3.BadgedBox
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CenterAlignedTopAppBar
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.lifecycleScope
import com.worthdoingbadly.cheese.ui.theme.CheeseTheme
import com.worthdoingbadly.cheese.ui.theme.TerminalAmber
import com.worthdoingbadly.cheese.ui.theme.TerminalBg
import com.worthdoingbadly.cheese.ui.theme.TerminalCyan
import com.worthdoingbadly.cheese.ui.theme.TerminalGreen
import com.worthdoingbadly.cheese.ui.theme.TerminalRed
import com.worthdoingbadly.cheese.ui.theme.TerminalText
import com.worthdoingbadly.cheese.ui.theme.TerminalTopBar
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.asFlow
import kotlinx.coroutines.flow.onCompletion
import kotlinx.coroutines.launch
import java.io.File
import java.io.InputStream
import java.nio.charset.StandardCharsets

private fun InputStream.toLineFlow() =
    bufferedReader(StandardCharsets.UTF_8).lineSequence().asFlow().onCompletion { close() }

enum class ExploitState {
    IDLE,
    RUNNING,
    SUCCESS,
    FAILED,
    ROOTED_EXTERNAL
}

class MainActivity : ComponentActivity() {

    companion object {
        const val SETTINGS_PREFS = "katyusha_settings"
        const val KEY_FORCE_RUN = "force_run"
        const val KEY_MANUAL_ROOTED_BOOT = "manual_rooted_boot"

        val logLines = mutableStateListOf<String>()
        val exploitState = mutableStateOf(ExploitState.IDLE)
        val showRunConfirm = mutableStateOf(false)
        val appScope = CoroutineScope(Dispatchers.IO + SupervisorJob())
        var bootStateRestored = false
        var externalRootAnnounced = false
        var forceModeWarned = false
        var managerHintShown = false
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        initLogBanner()
        evaluateRootGuard()

        setContent {
            CheeseTheme {
                MainAppScreen(
                    state = exploitState.value,
                    logs = logLines,
                    onStartExploit = { runExploit() },
                    onClearLogs = { clearLogs() },
                    onCopyLogs = { copyLogs() },
                    onSaveLogs = { saveLogs() },
                    onOpenKernelSU = { openKernelSU() },
                    onForceModeChanged = { evaluateRootGuard() },
                    onForceStart = { runExploit(confirmed = true) },
                    onProbeRoot = { probeSuRoot() },
                    currentBoot = currentBootCount()
                )
            }
        }
    }

    private fun initLogBanner() {
        val deviceModel = "${Build.MANUFACTURER.replaceFirstChar { it.uppercase() }} ${Build.MODEL}"
        val kernelVer = System.getProperty("os.version") ?: "Unknown"
        val hasKgsl = File("/dev/kgsl-3d0").exists()

        logLines.add("[*] Katyusha Temp-Root & KernelSU Engine v1.0")
        logLines.add("[*] Device: $deviceModel | Hardware: ${Build.HARDWARE}")
        logLines.add("[*] Linux Kernel: $kernelVer (GKI KMI Auto-detect)")
        logLines.add("[*] Qualcomm KGSL: ${if (hasKgsl) "/dev/kgsl-3d0 detected" else "node not present / sandbox"}")
        logLines.add("[*] Exploit Route: CVE-2025-21479 (Adreno KGSL UAF) -> Dynamic KernelSU")
        logLines.add("[*] Environment: 100% In-Memory (Non-persistent, safe)")

        // Check if there was an abnormal panic/reboot last time
        val panicLog = LogManager.getPreviousPanicLog(this)
        if (panicLog != null && panicLog.isNotEmpty()) {
            logLines.add(tr("[!] Notice: Detected incomplete session from last run (Kernel Panic / Reboot).", "[!] 提示：检测到上次运行中途异常中断（可能发生了内核 Panic 异常重启）。"))
            logLines.add(tr("[*] Auto-saved crash log (${panicLog.size} lines) preserved. Tap [Save Logs] above to export.", "[*] 系统已完整保留崩机前最后一刻的实时日志（共 ${panicLog.size} 行），点击上方【保存日志】可导出。"))
            logLines.add("------------------------------------------------------------")
        }

        logLines.add(tr("[*] Ready: Read requirements above, then tap [Activate] below to begin.", "[*] 就绪：请阅读环境要求与限制后，点击下方按钮开始提权。"))
    }

    // ---- Boot-scoped run state ------------------------------------------------
    // Temp root lives in kernel memory only and never survives a reboot, so
    // "engine already ran in THIS boot" (Settings.Global.BOOT_COUNT marker) is
    // authoritative: restore the state and lock the button instead of offering
    // another run (repeated runs per boot carry a real kernel-panic risk).

    private fun bootStatePrefs() = getSharedPreferences("katyusha_boot_state", Context.MODE_PRIVATE)

    private fun currentBootCount(): Int = try {
        Settings.Global.getInt(contentResolver, Settings.Global.BOOT_COUNT)
    } catch (_: Exception) {
        Int.MIN_VALUE
    }

    private fun markRunStarted() {
        val boot = currentBootCount()
        if (boot == Int.MIN_VALUE) return
        bootStatePrefs().edit()
            .putInt("ran_boot", boot)
            .putBoolean("last_success", false)
            .apply()
    }

    private fun markRunResult(success: Boolean) {
        bootStatePrefs().edit().putBoolean("last_success", success).apply()
    }

    private fun restoreBootState() {
        if (exploitState.value == ExploitState.RUNNING) return
        val prefs = bootStatePrefs()
        val ranBoot = prefs.getInt("ran_boot", Int.MIN_VALUE)
        if (ranBoot == Int.MIN_VALUE || ranBoot != currentBootCount()) return

        val success = prefs.getBoolean("last_success", false)
        val target = if (success) ExploitState.SUCCESS else ExploitState.FAILED
        if (exploitState.value == target && bootStateRestored) return
        exploitState.value = target

        if (!bootStateRestored) {
            bootStateRestored = true
            logLines.add("")
            if (success) {
                logLines.add(tr(
                    "[+] Detected active temp root from an earlier run in THIS boot (KernelSU already loaded).",
                    "[+] 检测到本次开机已成功激活临时 Root（KernelSU 已在内核中运行）。"
                ))
                logLines.add(tr(
                    "[*] No action needed — root stays available until the device reboots.",
                    "[*] 无需重复提权，直接使用即可；重启手机后自动恢复纯净系统。"
                ))
            } else {
                logLines.add(tr(
                    "[!] An exploit run was already attempted in THIS boot and did not complete.",
                    "[!] 检测到本次开机已运行过一次提权且未成功。"
                ))
                logLines.add(tr(
                    "[*] For safety (kernel panic risk), reboot the device before running again.",
                    "[*] 为避免内核 Panic 风险，请重启手机后再重新运行。"
                ))
            }
        }
    }

    // ---- External-root guard ---------------------------------------------------
    // The exploit itself can panic the kernel regardless of prior success, so
    // if the device is ALREADY rooted by any means (KernelSU, Magisk, another
    // temp-root, ...) we refuse to run — unless the user explicitly enables
    // "force run" in settings.

    private fun isForceModeEnabled(): Boolean =
        getSharedPreferences(SETTINGS_PREFS, Context.MODE_PRIVATE)
            .getBoolean(KEY_FORCE_RUN, false)

    /** Passive detection only: never executes su, so no grant prompt ever fires. */
    private fun detectExternalRootPath(): String? {
        val candidates = listOf(
            "/system/bin/su", "/system/xbin/su", "/sbin/su", "/system/sbin/su",
            "/vendor/bin/su", "/product/bin/su", "/su/bin/su", "/debug_ramdisk/su",
            "/data/local/tmp/su", "/data/adb/ksu/bin/su", "/data/adb/magisk/su"
        )
        for (p in candidates) {
            if (File(p).exists()) return p
        }
        return try {
            val proc = ProcessBuilder("sh", "-c", "command -v su")
                .redirectErrorStream(false)
                .start()
            val out = proc.inputStream.bufferedReader().readText().trim()
            proc.waitFor()
            out.takeIf { it.isNotEmpty() }
        } catch (_: Exception) {
            null
        }
    }

    /** Re-evaluates both guards: same-boot restore first, then external-root probe. */
    private fun evaluateRootGuard() {
        if (exploitState.value == ExploitState.RUNNING) return
        restoreBootState()          // our own run this boot wins (richer info)
        if (exploitState.value != ExploitState.IDLE) return

        if (isForceModeEnabled()) {
            if (!forceModeWarned) {
                forceModeWarned = true
                logLines.add(tr(
                    "[!] Force run enabled: exploit may be launched even on a rooted device (panic risk on you).",
                    "[!] 兼容模式已开启：即使检测到已有 Root 也允许运行引擎（内核 Panic 风险自负）。"
                ))
            }
            return                  // stay IDLE — button stays usable
        }

        // Manual "already rooted" mark, boot-scoped: auto-expires after reboot.
        val manualBoot = getSharedPreferences(SETTINGS_PREFS, Context.MODE_PRIVATE)
            .getInt(KEY_MANUAL_ROOTED_BOOT, Int.MIN_VALUE)
        if (manualBoot != Int.MIN_VALUE && manualBoot == currentBootCount()) {
            exploitState.value = ExploitState.ROOTED_EXTERNAL
            if (!externalRootAnnounced) {
                externalRootAnnounced = true
                logLines.add("")
                logLines.add(tr(
                    "[!] Manual mark active: device is already rooted in THIS boot.",
                    "[!] 手动标记生效：本次开机设备已处于 Root 状态。"
                ))
                logLines.add(tr(
                    "[*] Engine locked. The mark clears automatically after reboot (Settings to change).",
                    "[*] 引擎已锁定。重启手机后标记自动失效（可在设置中更改）。"
                ))
            }
            return
        }

        // Passive su-path probe (catches Magisk-style mounts; KernelSU hides su
        // from unauthorized apps, so also hint if a root manager is installed).
        appScope.launch {
            val suPath = detectExternalRootPath()
            val manager = detectRootManager()
            launch(Dispatchers.Main) {
                if (manager != null && !managerHintShown) {
                    managerHintShown = true
                    logLines.add(tr(
                        "[*] Root manager installed: $manager. Auto-detection cannot see through it — if the device is already rooted, mark it in Settings.",
                        "[*] 检测到已安装 Root 管理器：$manager。自动探测无法穿透其隐藏机制——若设备已 Root，请在设置中手动标记。"
                    ))
                }
                if (suPath == null || exploitState.value != ExploitState.IDLE) return@launch
                exploitState.value = ExploitState.ROOTED_EXTERNAL
                if (!externalRootAnnounced) {
                    externalRootAnnounced = true
                    logLines.add("")
                    logLines.add(tr(
                        "[!] Detected an existing root environment (su at $suPath).",
                        "[!] 检测到设备已处于 Root 状态（su 位于 $suPath）。"
                    ))
                    logLines.add(tr(
                        "[*] Running the exploit again is blocked — no need to re-root. Force run can be enabled in Settings (gear icon, top right).",
                        "[*] 已阻止再次提权 —— 已有 Root 无需重跑。如确有必要，可在右上角设置中开启「强制运行（兼容模式）」。"
                    ))
                }
            }
        }
    }

    private fun detectRootManager(): String? {
        val pkgs = listOf("me.weishu.kernelsu", "io.github.a13e300.ksu", "com.topjohnwu.magisk", "me.bmax.apatch")
        for (pkg in pkgs) {
            try {
                packageManager.getPackageInfo(pkg, 0)
                return pkg
            } catch (_: Exception) {
            }
        }
        return null
    }

    /**
     * Active root test via `su -c id`. Deliberately user-initiated (from the
     * confirm dialog): KernelSU may pop its grant prompt, and a denial or
     * timeout simply means "inconclusive" — never treated as "no root".
     */
    private fun probeSuRoot() {
        appScope.launch {
            var output = ""
            var code = -1
            try {
                val proc = ProcessBuilder("sh", "-c", "su -c id")
                    .redirectErrorStream(false)
                    .start()
                output = proc.inputStream.bufferedReader().readText().trim()
                code = if (proc.waitFor(3, java.util.concurrent.TimeUnit.SECONDS)) {
                    proc.exitValue()
                } else {
                    proc.destroy()
                    -1
                }
            } catch (_: Exception) {
            }
            val rooted = code == 0 && output.contains("uid=0")
            launch(Dispatchers.Main) {
                if (rooted) {
                    if (exploitState.value == ExploitState.IDLE) {
                        exploitState.value = ExploitState.ROOTED_EXTERNAL
                    }
                    if (!externalRootAnnounced) externalRootAnnounced = true
                    logLines.add("")
                    logLines.add(tr(
                        "[!] su probe: root IS active ($output) — exploit blocked, no need to re-run.",
                        "[!] su 检测：Root 已生效（$output）——已阻止再次提权，无需重跑。"
                    ))
                } else {
                    logLines.add(tr(
                        "[?] su probe inconclusive (exit=$code, output='$output'). Grant or deny did not prove root; decide yourself.",
                        "[?] su 检测无法确认（exit=$code，输出='$output'）。授权被拒或超时都不代表没有 Root，请自行判断。"
                    ))
                }
            }
        }
    }

    private fun runExploit(confirmed: Boolean = false) {
        if (exploitState.value == ExploitState.RUNNING) return

        if (exploitState.value == ExploitState.ROOTED_EXTERNAL && !isForceModeEnabled()) {
            // Belt & suspenders: the button is already disabled in this state.
            logLines.add(tr(
                "[!] Blocked: device is already rooted. Enable force run in Settings if you really mean it.",
                "[!] 已阻止：设备已处于 Root 状态。如确需强制运行，请在设置中开启兼容模式。"
            ))
            return
        }

        // High-risk action: always confirm before launching (KernelSU-style root
        // hides su from us, so auto-detection alone cannot prove "not rooted").
        if (!confirmed && !isForceModeEnabled()) {
            showRunConfirm.value = true
            return
        }

        exploitState.value = ExploitState.RUNNING
        markRunStarted()
        bootStateRestored = true // this run's own outcome now drives the UI
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // Start real-time auto-log session (overwrites previous auto-save log)
        LogManager.startAutoLogSession(this@MainActivity)

        val startMsg1 = tr("--- [Starting Qualcomm GPU Exploit & KernelSU Setup] ---", "--- [开始执行高通 GPU 提权链与 KernelSU 加载] ---")
        val startMsg2 = tr("[*] Screen kept ON (FLAG_KEEP_SCREEN_ON)", "[*] 锁定屏幕常亮 (FLAG_KEEP_SCREEN_ON)")
        logLines.add(startMsg1)
        LogManager.writeLine(startMsg1)
        logLines.add(startMsg2)
        LogManager.writeLine(startMsg2)

        appScope.launch {
            try {
                val assetManager = assets
                val extractedDir = getDir("cheese", 0)

                val msgExtract = tr("[*] Extracting payload & KernelSU components to memory workspace...", "[*] 正在释放攻击载荷与 KernelSU 组件到内存工作目录...")
                launch(Dispatchers.Main) { logLines.add(msgExtract) }
                LogManager.writeLine(msgExtract)

                for (filename in assetManager.list("cheese")!!) {
                    val targetFile = File(extractedDir, filename)
                    if (targetFile.exists()) {
                        targetFile.delete()
                    }
                    assetManager.open("cheese/$filename").use { input ->
                        targetFile.outputStream().use { output ->
                            input.copyTo(output)
                        }
                    }
                    targetFile.setExecutable(true, false)
                    targetFile.setReadable(true, false)
                    val msgExt = "  - " + tr("Extracted", "释放组件") + ": $filename (executable=ok)"
                    launch(Dispatchers.Main) { logLines.add(msgExt) }
                    LogManager.writeLine(msgExt)
                }

                // Apply any online-downloaded newer KernelSU modules or ksud
                KernelUpdater.applyDownloadedUpdates(this@MainActivity, extractedDir) { msg ->
                    launch(Dispatchers.Main) { logLines.add(msg) }
                    LogManager.writeLine(msg)
                }

                val executablePath = applicationInfo.nativeLibraryDir + "/libcheese.so"
                val launchShPath = File(extractedDir, "launch.sh").path

                val msgLoad = "[*] " + tr("Loading core binary: ", "载入核心二进制: ") + executablePath
                val msgSpawn = "[*] " + tr("Spawning child process for exploit chain...", "启动子进程执行 exploit + 提权链...")
                launch(Dispatchers.Main) {
                    logLines.add(msgLoad)
                    logLines.add(msgSpawn)
                }
                LogManager.writeLine(msgLoad)
                LogManager.writeLine(msgSpawn)

                val processBuilder = ProcessBuilder().command(executablePath, "sh", launchShPath)
                    .redirectErrorStream(true)
                val process = processBuilder.start()

                var hasRootSuccess = false

                process.inputStream.toLineFlow().collect { line ->
                    println(line)
                    if (line.contains("uid=0") || line.contains("KernelSU setup completed successfully") || line.contains("KernelSU LKM module inserted") || line.contains("ksud installed") || line.contains("ACTIVE in kernel")) {
                        hasRootSuccess = true
                    }
                    LogManager.writeLine(line)
                    launch(Dispatchers.Main) {
                        logLines.add(line)
                    }
                }

                val returnVal = process.waitFor()
                println("cheese returned $returnVal, hasRootSuccess=$hasRootSuccess")

                // Robust verification: check if KernelSU module is actually loaded in /proc/modules or su responsive
                val isKsuLoaded = try {
                    val p = Runtime.getRuntime().exec(arrayOf("sh", "-c", "grep -q kernelsu /proc/modules 2>/dev/null"))
                    p.waitFor() == 0
                } catch (_: Exception) {
                    false
                }

                val reallySuccess = hasRootSuccess || isKsuLoaded
                LogManager.endAutoLogSession(returnVal, reallySuccess)
                markRunResult(reallySuccess)

                launch(Dispatchers.Main) {
                    try { window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON) } catch (_: Exception) {}
                    val msgExit = "[*] " + tr("Process exited, return code: ", "进程退出，返回码: ") + returnVal
                    logLines.add(msgExit)
                    if (reallySuccess) {
                        exploitState.value = ExploitState.SUCCESS
                        val msgSucc = "[+] [SUCCESS] " + tr("Exploit succeeded! Root acquired & KernelSU ready.", "提权流程成功完成！已获得 Root 权限并加载 KernelSU。")
                        val msgJump = "[*] " + tr("Automatically launching KernelSU Manager...", "正在自动打开 KernelSU 管理器...")
                        logLines.add(msgSucc)
                        logLines.add(msgJump)
                        LogManager.writeLine(msgSucc)
                        LogManager.writeLine(msgJump)

                        launch {
                            delay(1000)
                            openKernelSU()
                        }
                    } else {
                        exploitState.value = ExploitState.FAILED
                        val msgFail = "[!] [NOTICE] " + tr("Exploit interrupted or incomplete. Root not verified, please keep screen steady and retry.", "提权过程被中断或未完全执行。未检测到 KernelSU 生效，请保持屏幕常亮且不要翻转手机，重启后重试。")
                        logLines.add(msgFail)
                    }
                }
            } catch (e: Exception) {
                LogManager.writeLine("[!] Exception: " + e.message)
                LogManager.endAutoLogSession(-1, false)
                markRunResult(false)
                launch(Dispatchers.Main) {
                    try { window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON) } catch (_: Exception) {}
                    exploitState.value = ExploitState.FAILED
                    logLines.add("[!] " + tr("Exception: ", "发生异常: ") + e.message)
                }
            }
        }
    }

    private fun saveLogs() {
        val (ok, path) = LogManager.saveLogsToDownloads(this, logLines)
        if (ok) {
            Toast.makeText(this, tr("Logs saved to: ", "日志已保存至: ") + path, Toast.LENGTH_LONG).show()
        } else {
            Toast.makeText(this, tr("Failed to save logs: ", "保存日志失败: ") + path, Toast.LENGTH_SHORT).show()
        }
    }

    private fun clearLogs() {
        logLines.clear()
        exploitState.value = ExploitState.IDLE
        initLogBanner()
        evaluateRootGuard()
    }

    private fun copyLogs() {
        val clipboardManager = getSystemService(CLIPBOARD_SERVICE) as ClipboardManager
        val allLogs = logLines.joinToString("\n")
        clipboardManager.setPrimaryClip(ClipData.newPlainText("Katyusha Logs", allLogs))
        Toast.makeText(this, tr("Logs copied to clipboard", "日志已复制到剪切板"), Toast.LENGTH_SHORT).show()
    }

    private fun openKernelSU() {
        val candidatePackages = listOf(
            "me.weishu.kernelsu",
            "io.github.a13e300.ksu"
        )
        for (pkg in candidatePackages) {
            val launchIntent = packageManager.getLaunchIntentForPackage(pkg)
            if (launchIntent != null) {
                startActivity(launchIntent)
                return
            }
        }

        // Fallback: try explicit ComponentName intent
        for (pkg in candidatePackages) {
            try {
                val intent = Intent(Intent.ACTION_MAIN).apply {
                    setClassName(pkg, "$pkg.ui.MainActivity")
                    addCategory(Intent.CATEGORY_LAUNCHER)
                    flags = Intent.FLAG_ACTIVITY_NEW_TASK
                }
                startActivity(intent)
                return
            } catch (_: Exception) {}
        }

        // Fallback: try root shell am start if available
        try {
            val process = Runtime.getRuntime().exec(arrayOf("su", "-c", "am start -n me.weishu.kernelsu/.ui.MainActivity"))
            if (process.waitFor() == 0) return
        } catch (_: Exception) {}

        Toast.makeText(this, tr("KernelSU Manager not installed, opening download page...", "未安装 KernelSU 管理器，正在前往下载页面..."), Toast.LENGTH_LONG).show()
        try {
            val browserIntent = Intent(
                Intent.ACTION_VIEW,
                Uri.parse("https://github.com/tiann/KernelSU/releases")
            )
            startActivity(browserIntent)
        } catch (e: Exception) {
            Toast.makeText(this, tr("Failed to open browser: ", "无法打开浏览器: ") + e.message, Toast.LENGTH_SHORT).show()
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainAppScreen(
    state: ExploitState,
    logs: List<String>,
    onStartExploit: () -> Unit,
    onClearLogs: () -> Unit,
    onCopyLogs: () -> Unit,
    onSaveLogs: () -> Unit,
    onOpenKernelSU: () -> Unit,
    onForceModeChanged: () -> Unit,
    onForceStart: () -> Unit,
    onProbeRoot: () -> Unit,
    currentBoot: Int,
) {
    val context = LocalContext.current
    val coroutineScope = rememberCoroutineScope()
    var showUpdateDialog by remember { mutableStateOf(false) }
    var showSettingsDialog by remember { mutableStateOf(false) }
    var updateCheckResult by remember { mutableStateOf<UpdateCheckResult?>(null) }
    var isChecking by remember { mutableStateOf(false) }
    var isDownloading by remember { mutableStateOf(false) }
    var updateStatusText by remember { mutableStateOf("") }
    var hasUpdateBadge by remember { mutableStateOf(false) }
    var consoleFullscreen by remember { mutableStateOf(false) }

    // System back exits console fullscreen first
    BackHandler(enabled = consoleFullscreen) { consoleFullscreen = false }

    LaunchedEffect(Unit) {
        val res = KernelUpdater.checkForUpdates(context)
        updateCheckResult = res
        hasUpdateBadge = res.hasUpdate
    }

    if (showSettingsDialog) {
        val settingsPrefs = context.getSharedPreferences(
            MainActivity.SETTINGS_PREFS, Context.MODE_PRIVATE
        )
        var forceRun by remember {
            mutableStateOf(settingsPrefs.getBoolean(MainActivity.KEY_FORCE_RUN, false))
        }
        var manualRooted by remember {
            mutableStateOf(
                settingsPrefs.getInt(MainActivity.KEY_MANUAL_ROOTED_BOOT, Int.MIN_VALUE) != Int.MIN_VALUE &&
                    settingsPrefs.getInt(MainActivity.KEY_MANUAL_ROOTED_BOOT, Int.MIN_VALUE) == currentBoot
            )
        }
        AlertDialog(
            onDismissRequest = {
                showSettingsDialog = false
                onForceModeChanged()
            },
            title = { Text(tr("Settings", "设置")) },
            text = {
                Column {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(12.dp)
                    ) {
                        Switch(
                            checked = manualRooted,
                            onCheckedChange = {
                                manualRooted = it
                                if (it) {
                                    settingsPrefs.edit()
                                        .putInt(MainActivity.KEY_MANUAL_ROOTED_BOOT, currentBoot)
                                        .apply()
                                } else {
                                    settingsPrefs.edit()
                                        .remove(MainActivity.KEY_MANUAL_ROOTED_BOOT)
                                        .apply()
                                }
                                onForceModeChanged()
                            }
                        )
                        Text(
                            text = tr("Device already rooted (this boot)", "设备已 Root（本次开机）"),
                            style = MaterialTheme.typography.titleSmall,
                            fontWeight = FontWeight.Bold
                        )
                    }
                    Text(
                        text = tr(
                            "Manually mark the device as rooted — the engine stays locked until reboot (auto-clears). Use this when root was gained by other means (KernelSU hides su, so auto-detection can miss it).",
                            "手动标记设备已处于 Root 状态，引擎将锁定直至重启（重启后自动失效）。当 Root 来自其他途径时请使用：KernelSU 会向应用隐藏 su，自动探测可能探测不到。"
                        ),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Spacer(modifier = Modifier.height(16.dp))
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(12.dp)
                    ) {
                        Switch(
                            checked = forceRun,
                            onCheckedChange = {
                                forceRun = it
                                settingsPrefs.edit().putBoolean(MainActivity.KEY_FORCE_RUN, it).apply()
                                onForceModeChanged()
                            }
                        )
                        Text(
                            text = tr("Force run (compat mode)", "强制运行（兼容模式）"),
                            style = MaterialTheme.typography.titleSmall,
                            fontWeight = FontWeight.Bold
                        )
                    }
                    Spacer(modifier = Modifier.height(10.dp))
                    Text(
                        text = tr(
                            "When enabled, the exploit can be launched even if the device is already rooted by any means (KernelSU, Magisk, other temp-root). Only for debugging: re-rooting an already rooted device has no benefit and can trigger a kernel panic.",
                            "开启后，即使设备已通过任何方式获得 Root（KernelSU、Magisk、其他临时 Root），也允许运行提权引擎。仅供调试使用：对已 Root 的设备重复提权没有任何收益，且可能引发内核 Panic。"
                        ),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        showSettingsDialog = false
                        onForceModeChanged()
                    }
                ) { Text(tr("Done", "完成")) }
            }
        )
    }

    if (MainActivity.showRunConfirm.value) {
        AlertDialog(
            onDismissRequest = { MainActivity.showRunConfirm.value = false },
            title = { Text(tr("Confirm exploit launch", "确认运行提权")) },
            text = {
                Text(
                    tr(
                        "Auto-detection cannot reliably rule out an existing root (KernelSU hides su from apps). Running the exploit can kernel-panic the device even on a clean boot. Continue?",
                        "自动检测无法可靠排除设备已处于 Root 状态（KernelSU 会向应用隐藏 su）。且即使设备未 Root，运行提权也存在内核 Panic 风险。确定继续？"
                    ),
                    style = MaterialTheme.typography.bodyMedium
                )
            },
            dismissButton = {
                TextButton(onClick = { MainActivity.showRunConfirm.value = false }) {
                    Text(tr("Cancel", "取消"))
                }
            },
            confirmButton = {
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    TextButton(onClick = {
                        MainActivity.showRunConfirm.value = false
                        onProbeRoot()
                    }) {
                        Text(tr("Test su first", "先检测 Root"))
                    }
                    TextButton(onClick = {
                        MainActivity.showRunConfirm.value = false
                        onForceStart()
                    }) {
                        Text(tr("Run exploit", "运行提权"), fontWeight = FontWeight.Bold)
                    }
                }
            }
        )
    }

    val scrollBehavior = TopAppBarDefaults.pinnedScrollBehavior()

    Scaffold(
        modifier = Modifier.fillMaxSize(),
        topBar = {
            CenterAlignedTopAppBar(
                title = {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Text(
                            text = "Cheese Katyusha",
                            style = MaterialTheme.typography.titleLarge,
                            fontWeight = FontWeight.Bold
                        )
                        Text(
                            text = tr("Qualcomm Adreno · Temp Root & KernelSU", "高通 Adreno · 临时 Root & KernelSU"),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                },
                actions = {
                    IconButton(onClick = { showSettingsDialog = true }) {
                        Icon(
                            imageVector = Icons.Default.Settings,
                            contentDescription = tr("Settings", "设置"),
                            tint = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                    IconButton(
                        onClick = {
                            showUpdateDialog = true
                            coroutineScope.launch {
                                isChecking = true
                                updateStatusText = ""
                                val res = KernelUpdater.checkForUpdates(context)
                                updateCheckResult = res
                                hasUpdateBadge = res.hasUpdate
                                isChecking = false
                            }
                        }
                    ) {
                        BadgedBox(
                            badge = {
                                if (hasUpdateBadge) {
                                    Badge()
                                }
                            }
                        ) {
                            Icon(
                                imageVector = Icons.Default.CloudSync,
                                contentDescription = tr("Check Kernel Update", "检查内核驱动更新"),
                                tint = MaterialTheme.colorScheme.primary
                            )
                        }
                    }
                },
                colors = TopAppBarDefaults.centerAlignedTopAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surface
                ),
                scrollBehavior = scrollBehavior
            )
        }
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
                .padding(horizontal = 16.dp, vertical = 8.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            // 1. Device & Runtime Info Card
            if (!consoleFullscreen) {
                DeviceStatusCard(state = state)

                // 2. Qualcomm GPU & Linux Kernel Requirements Card
                PlatformLimitationCard()
            }

            // 3. Clean Monospace Output Panel (Mangekyo benchmark style)
            TerminalCard(
                logs = logs,
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f),
                isFullscreen = consoleFullscreen,
                onToggleFullscreen = { consoleFullscreen = !consoleFullscreen },
                onClear = onClearLogs,
                onCopy = onCopyLogs,
                onSave = onSaveLogs
            )

            // 4. Action Controls
            if (!consoleFullscreen) {
                ActionSection(
                    state = state,
                    onStart = onStartExploit,
                    onOpenKernelSU = onOpenKernelSU
                )
            }
        }
    }

    if (showUpdateDialog) {
        AlertDialog(
            onDismissRequest = { if (!isDownloading) showUpdateDialog = false },
            icon = {
                Icon(
                    imageVector = Icons.Default.CloudSync,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.primary
                )
            },
            title = {
                Text(
                    text = tr("KernelSU Module Sync & Update", "内核驱动自动同步与更新"),
                    fontWeight = FontWeight.Bold
                )
            },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    val res = updateCheckResult
                    if (isChecking) {
                        LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                        Text(
                            text = tr("Checking for latest modules on GitHub...", "正在查询 KernelSU 官方最新驱动..."),
                            style = MaterialTheme.typography.bodyMedium
                        )
                    } else if (res != null) {
                        Text(
                            text = tr("• Local Module: ", "• 当前模块版本: ") + "${res.currentModuleVersion} (Build ${res.currentModuleCode})",
                            style = MaterialTheme.typography.bodyMedium
                        )
                        Text(
                            text = tr("• Target KMI: ", "• 匹配设备 KMI: ") + res.kmi,
                            style = MaterialTheme.typography.bodyMedium
                        )
                        if (res.installedManagerVersion != null) {
                            Text(
                                text = tr("• Manager App: ", "• 当前管理器版本: ") + "${res.installedManagerVersion} (Build ${res.installedManagerCode ?: "Unknown"})",
                                style = MaterialTheme.typography.bodyMedium
                            )
                        }
                        Spacer(modifier = Modifier.height(4.dp))
                        if (res.hasUpdate) {
                            Text(
                                text = tr(
                                    "✨ Found new matching module: ${res.targetTag}!\nTap [Update Now] to download automatically.",
                                    "✨ 发现可匹配新版本：${res.targetTag}！\n点击下方【立即自动更新】即可自动拉取最新 .ko 模块。"
                                ),
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.primary,
                                fontWeight = FontWeight.SemiBold
                            )
                        } else {
                            Text(
                                text = tr(
                                    "✔️ Kernel module matches installed manager perfectly and is up to date.",
                                    "✔️ 当前内核驱动与 KernelSU 管理器版本完全一致，处于最佳同步状态！"
                                ),
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                    }

                    if (updateStatusText.isNotEmpty()) {
                        Spacer(modifier = Modifier.height(4.dp))
                        Text(
                            text = updateStatusText,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.secondary
                        )
                    }

                    if (isDownloading) {
                        Spacer(modifier = Modifier.height(4.dp))
                        LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                    }
                }
            },
            confirmButton = {
                val res = updateCheckResult
                if (res != null && res.hasUpdate && !isChecking) {
                    Button(
                        enabled = !isDownloading,
                        onClick = {
                            coroutineScope.launch {
                                isDownloading = true
                                val ok = KernelUpdater.downloadUpdate(context, res) { msg ->
                                    updateStatusText = msg
                                }
                                if (ok) {
                                    val newRes = KernelUpdater.checkForUpdates(context)
                                    updateCheckResult = newRes
                                    hasUpdateBadge = newRes.hasUpdate
                                }
                                isDownloading = false
                            }
                        }
                    ) {
                        Text(tr("Update Now", "立即自动更新"))
                    }
                } else {
                    TextButton(
                        onClick = {
                            coroutineScope.launch {
                                isChecking = true
                                updateStatusText = ""
                                val newRes = KernelUpdater.checkForUpdates(context)
                                updateCheckResult = newRes
                                hasUpdateBadge = newRes.hasUpdate
                                isChecking = false
                            }
                        }
                    ) {
                        Text(tr("Check Again", "重新检查"))
                    }
                }
            },
            dismissButton = {
                TextButton(
                    enabled = !isDownloading,
                    onClick = { showUpdateDialog = false }
                ) {
                    Text(tr("Close", "关闭"))
                }
            }
        )
    }
}

@Composable
private fun DeviceStatusCard(
    state: ExploitState,
    modifier: Modifier = Modifier
) {
    val deviceModel = "${Build.MANUFACTURER.replaceFirstChar { it.uppercase() }} ${Build.MODEL}"
    val soc = Build.HARDWARE
    val kernelVer = System.getProperty("os.version") ?: "5.10.101"

    Card(
        modifier = modifier.fillMaxWidth(),
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceContainerLow
        ),
        elevation = CardDefaults.cardElevation(defaultElevation = 1.dp)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.PhoneAndroid,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.primary,
                        modifier = Modifier.size(20.dp)
                    )
                    Text(
                        text = tr("Device & Runtime Status", "设备与运行状态"),
                        style = MaterialTheme.typography.titleMedium,
                        fontWeight = FontWeight.SemiBold
                    )
                }

                StatusBadge(state = state)
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                InfoChip(
                    icon = Icons.Default.PhoneAndroid,
                    label = tr("Model", "设备型号"),
                    value = deviceModel,
                    modifier = Modifier.weight(1f)
                )
                InfoChip(
                    icon = Icons.Default.Memory,
                    label = tr("Platform / SoC", "平台 / SoC"),
                    value = soc,
                    modifier = Modifier.weight(1f)
                )
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                InfoChip(
                    icon = Icons.Default.Terminal,
                    label = tr("Linux Kernel", "Linux 内核"),
                    value = kernelVer.take(18),
                    modifier = Modifier.weight(1f)
                )
                InfoChip(
                    icon = Icons.Default.Security,
                    label = tr("Route", "提权方案"),
                    value = "CVE-2025-21479 · LKM",
                    modifier = Modifier.weight(1f)
                )
            }
        }
    }
}

@Composable
private fun StatusBadge(state: ExploitState) {
    val (bgColor, textColor, text) = when (state) {
        ExploitState.IDLE -> Triple(
            MaterialTheme.colorScheme.secondaryContainer,
            MaterialTheme.colorScheme.onSecondaryContainer,
            tr("Ready", "就绪")
        )
        ExploitState.RUNNING -> Triple(
            MaterialTheme.colorScheme.tertiaryContainer,
            MaterialTheme.colorScheme.onTertiaryContainer,
            tr("Exploiting...", "提权中...")
        )
        ExploitState.SUCCESS -> Triple(
            Color(0xFFDCFCE7),
            Color(0xFF166534),
            tr("Root Active", "已激活 Root")
        )
        ExploitState.FAILED -> Triple(
            MaterialTheme.colorScheme.errorContainer,
            MaterialTheme.colorScheme.onErrorContainer,
            tr("Reboot Required", "需要重启")
        )
        ExploitState.ROOTED_EXTERNAL -> Triple(
            Color(0xFFFFEDD5),
            Color(0xFF9A3412),
            tr("Root Detected", "已有 Root")
        )
    }

    Box(
        modifier = Modifier
            .clip(CircleShape)
            .background(bgColor)
            .padding(horizontal = 10.dp, vertical = 4.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = text,
            style = MaterialTheme.typography.labelSmall,
            fontWeight = FontWeight.Bold,
            color = textColor
        )
    }
}

@Composable
private fun InfoChip(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    label: String,
    value: String,
    modifier: Modifier = Modifier
) {
    Surface(
        modifier = modifier,
        shape = MaterialTheme.shapes.small,
        color = MaterialTheme.colorScheme.surfaceContainerHighest
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 10.dp, vertical = 7.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            Icon(
                imageVector = icon,
                contentDescription = null,
                modifier = Modifier.size(18.dp),
                tint = MaterialTheme.colorScheme.primary
            )
            Column(modifier = Modifier.weight(1f, fill = false)) {
                Text(
                    text = label,
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    fontSize = 11.sp
                )
                Text(
                    text = value,
                    style = MaterialTheme.typography.bodySmall,
                    fontWeight = FontWeight.SemiBold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }
        }
    }
}

@Composable
private fun PlatformLimitationCard(
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier.fillMaxWidth(),
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceContainerLow
        ),
        elevation = CardDefaults.cardElevation(defaultElevation = 1.dp)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.Warning,
                    contentDescription = null,
                    tint = MaterialTheme.colorScheme.tertiary,
                    modifier = Modifier.size(20.dp)
                )
                Text(
                    text = tr("Hardware & Runtime Requirements", "硬件架构与运行环境要求"),
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold
                )
            }

            Text(
                text = tr(
                    "• Qualcomm GPU: Qualcomm Adreno GPUs only. Exploits CVE-2025-21479 UAF in Qualcomm proprietary KGSL driver (/dev/kgsl-3d0); non-Snapdragon platforms (Dimensity / Exynos / Tensor / Xuanjie / Kirin) are not supported.",
                    "• 高通 GPU 限制：仅支持 Qualcomm Adreno 系列 GPU。利用基于高通专有 KGSL 驱动 (/dev/kgsl-3d0) 的 CVE-2025-21479 UAF 漏洞；非骁龙平台 (天玑 / 猎户座 / Tensor / 玄戒 / 麒麟) 无法运行。"
                ),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                lineHeight = 18.sp
            )

            Text(
                text = tr(
                    "• Linux Kernel & Auto-Sync: Dynamic KMI auto-matching (supports 5.10 / 5.15 / 6.1 / 6.6 GKI kernels). Supports auto-checking and one-tap updating latest official kernel modules (.ko), seamlessly syncing with installed KernelSU manager.",
                    "• Linux 内核与驱动自动更新：采用动态 KMI 匹配架构（支持 5.10 / 5.15 / 6.1 / 6.6 等 GKI 内核）。支持联网自动检测与一键更新最新官方内核模块（.ko），自动与已安装的 KernelSU 管理器版本对齐。"
                ),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                lineHeight = 18.sp
            )

            Text(
                text = tr(
                    "• Execution Discipline: Exploit takes ~30-90s, keep screen ON (screen timeout interrupts GPU driver). Run only once per boot; reboot if failed. Runs in-memory only; rebooting restores completely clean state.",
                    "• 运行纪律：提权耗时约 30~90 秒，期间屏幕需保持常亮；单次开机仅限运行一次，失败请直接重启设备后再试；纯内存运行，重启手机即恢复纯净状态。"
                ),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                lineHeight = 18.sp
            )
        }
    }
}

@Composable
private fun TerminalCard(
    logs: List<String>,
    modifier: Modifier = Modifier,
    isFullscreen: Boolean = false,
    onToggleFullscreen: () -> Unit = {},
    onClear: () -> Unit,
    onCopy: () -> Unit,
    onSave: () -> Unit
) {
    val listState = rememberLazyListState()

    LaunchedEffect(logs.size) {
        if (logs.isNotEmpty()) {
            if (logs.size > 7) {
                listState.animateScrollToItem(logs.size - 1)
            } else {
                listState.scrollToItem(0)
            }
        }
    }

    Card(
        modifier = modifier.fillMaxWidth(),
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceContainerLow
        ),
        elevation = CardDefaults.cardElevation(defaultElevation = 1.dp)
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Native Material 3 Console Header
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 14.dp, vertical = 8.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.Terminal,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.primary,
                        modifier = Modifier.size(20.dp)
                    )

                    Text(
                        text = tr("Console Output", "控制台输出"),
                        style = MaterialTheme.typography.titleMedium,
                        fontWeight = FontWeight.SemiBold
                    )

                    Surface(
                        shape = CircleShape,
                        color = MaterialTheme.colorScheme.surfaceContainerHighest
                    ) {
                        Text(
                            text = "${logs.size} " + tr("lines", "行"),
                            style = MaterialTheme.typography.labelSmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.padding(horizontal = 8.dp, vertical = 2.dp)
                        )
                    }
                }

                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    IconButton(
                        onClick = onToggleFullscreen,
                        modifier = Modifier.size(34.dp)
                    ) {
                        Icon(
                            imageVector = if (isFullscreen) Icons.Default.FullscreenExit else Icons.Default.Fullscreen,
                            contentDescription = if (isFullscreen) {
                                tr("Exit fullscreen", "退出全屏")
                            } else {
                                tr("Fullscreen console", "控制台全屏")
                            },
                            tint = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.size(18.dp)
                        )
                    }

                    IconButton(
                        onClick = onSave,
                        modifier = Modifier.size(34.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.Save,
                            contentDescription = tr("Save logs", "保存日志"),
                            tint = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.size(18.dp)
                        )
                    }

                    IconButton(
                        onClick = onCopy,
                        modifier = Modifier.size(34.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.ContentCopy,
                            contentDescription = tr("Copy logs", "复制日志"),
                            tint = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.size(18.dp)
                        )
                    }

                    IconButton(
                        onClick = onClear,
                        modifier = Modifier.size(34.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.DeleteSweep,
                            contentDescription = tr("Clear console", "清空控制台"),
                            tint = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.size(18.dp)
                        )
                    }
                }
            }

            // Dark Monospace Log Panel
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .padding(horizontal = 10.dp, vertical = 8.dp)
                    .clip(MaterialTheme.shapes.medium)
                    .background(TerminalBg)
            ) {
                SelectionContainer(modifier = Modifier.fillMaxSize()) {
                    LazyColumn(
                        state = listState,
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(10.dp),
                        verticalArrangement = Arrangement.spacedBy(3.dp)
                    ) {
                        itemsIndexed(logs) { _, line ->
                            TerminalLineItem(line = line)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun TerminalLineItem(line: String) {
    val color = when {
        line.contains("[+]") || line.contains("uid=0") || line.contains("SUCCESS") ||
                line.contains("KernelSU setup completed") || line.contains("module inserted") -> TerminalGreen

        line.contains("[*]") || line.contains("Starting") || line.contains("载入核心") ||
                line.contains("释放组件") || line.contains("就绪") || line.contains("Ready") -> TerminalCyan

        line.contains("[!]") || line.contains("kernel_panic") || line.contains("NOTICE") ||
                line.contains("异常") || line.contains("失败") || line.contains("Exception") -> TerminalRed

        line.contains("spray") || line.contains("尝试") || line.contains("waiting") -> TerminalAmber

        else -> TerminalText
    }

    Text(
        text = line,
        fontFamily = FontFamily.Monospace,
        fontSize = 11.sp,
        lineHeight = 15.sp,
        color = color
    )
}

@Composable
private fun ActionSection(
    state: ExploitState,
    onStart: () -> Unit,
    onOpenKernelSU: () -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Button(
            onClick = onStart,
            enabled = state == ExploitState.IDLE,
            modifier = Modifier
                .fillMaxWidth()
                .height(52.dp),
            shape = MaterialTheme.shapes.medium,
            colors = ButtonDefaults.buttonColors(
                containerColor = MaterialTheme.colorScheme.primary,
                contentColor = MaterialTheme.colorScheme.onPrimary
            )
        ) {
            if (state == ExploitState.RUNNING) {
                CircularProgressIndicator(
                    modifier = Modifier.size(20.dp),
                    color = MaterialTheme.colorScheme.onPrimary,
                    strokeWidth = 2.5.dp
                )
                Spacer(modifier = Modifier.width(10.dp))
                Text(
                    text = tr("Exploiting & injecting KernelSU...", "正在提权与注入内核模块..."),
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold
                )
            } else if (state == ExploitState.SUCCESS) {
                Icon(
                    imageVector = Icons.Default.CheckCircle,
                    contentDescription = null,
                    modifier = Modifier.size(22.dp)
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    text = tr("Root Active — Reboot to Reset", "Root 已激活 · 重启后自动解除"),
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold
                )
            } else if (state == ExploitState.FAILED) {
                Icon(
                    imageVector = Icons.Default.Warning,
                    contentDescription = null,
                    modifier = Modifier.size(22.dp)
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    text = tr("Reboot Device Before Retrying", "请重启手机后再重试"),
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold
                )
            } else if (state == ExploitState.ROOTED_EXTERNAL) {
                Icon(
                    imageVector = Icons.Default.CheckCircle,
                    contentDescription = null,
                    modifier = Modifier.size(22.dp)
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    text = tr("Already Rooted — Run Blocked", "已有 Root · 已阻止运行"),
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold
                )
            } else {
                Icon(
                    imageVector = Icons.Default.PlayArrow,
                    contentDescription = null,
                    modifier = Modifier.size(22.dp)
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    text = tr("Activate Root & KernelSU", "一键激活 Root & KernelSU"),
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold
                )
            }
        }

        OutlinedButton(
            onClick = onOpenKernelSU,
            modifier = Modifier
                .fillMaxWidth()
                .height(44.dp),
            shape = MaterialTheme.shapes.medium
        ) {
            Icon(
                imageVector = Icons.Default.Security,
                contentDescription = null,
                modifier = Modifier.size(18.dp)
            )
            Spacer(modifier = Modifier.width(6.dp))
            Text(
                text = tr("Open KernelSU Manager", "打开 KernelSU 管理器"),
                style = MaterialTheme.typography.labelLarge
            )
        }
    }
}
