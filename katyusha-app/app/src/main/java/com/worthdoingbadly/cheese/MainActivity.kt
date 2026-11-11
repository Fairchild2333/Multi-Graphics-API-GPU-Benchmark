package com.worthdoingbadly.cheese

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.view.WindowManager
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.BorderStroke
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
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.DeleteSweep
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Launch
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material.icons.filled.PhoneAndroid
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.Terminal
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.AssistChip
import androidx.compose.material3.AssistChipDefaults
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CenterAlignedTopAppBar
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedCard
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
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
import kotlinx.coroutines.Dispatchers
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
    FAILED
}

class MainActivity : ComponentActivity() {

    private val logLines = mutableStateListOf<String>()
    private val exploitState = mutableStateOf(ExploitState.IDLE)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        initLogBanner()

        setContent {
            CheeseTheme {
                MainAppScreen(
                    state = exploitState.value,
                    logs = logLines,
                    onStartExploit = { runExploit() },
                    onClearLogs = { clearLogs() },
                    onCopyLogs = { copyLogs() },
                    onOpenKernelSU = { openKernelSU() }
                )
            }
        }
    }

    private fun initLogBanner() {
        if (logLines.isEmpty()) {
            logLines.add("[*] Katyusha Temp-Root & KernelSU Engine v1.0")
            logLines.add("[*] Target: Black Shark 5 Pro (KTUS-A0 / SM8450 / Adreno 730)")
            logLines.add("[*] Exploit: CVE-2025-21479 (Adreno KGSL UAF) -> LKM KernelSU")
            logLines.add("[*] Environment: 100% In-Memory (Non-persistent, reboot restores state)")
            logLines.add("[*] 就绪：请阅读执行规范后点击下方的【一键激活】按钮开始提权。")
        }
    }

    private fun runExploit() {
        if (exploitState.value == ExploitState.RUNNING) return

        exploitState.value = ExploitState.RUNNING
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        logLines.add("--- [开始执行漏洞利用与 KernelSU 加载] ---")
        logLines.add("[*] 锁定屏幕常亮 (FLAG_KEEP_SCREEN_ON)")

        lifecycleScope.launch(Dispatchers.IO) {
            try {
                val assetManager = assets
                val extractedDir = getDir("cheese", 0)

                logLines.add("[*] 正在释放攻击载荷与 KernelSU 组件到内存工作目录...")
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
                    logLines.add("  - 释放组件: $filename (executable=ok)")
                }

                val executablePath = applicationInfo.nativeLibraryDir + "/libcheese.so"
                val launchShPath = File(extractedDir, "launch.sh").path

                logLines.add("[*] 载入核心二进制: $executablePath")
                logLines.add("[*] 启动子进程执行 exploit + 提权链...")

                val processBuilder = ProcessBuilder().command(executablePath, "sh", launchShPath)
                    .redirectErrorStream(true)
                val process = processBuilder.start()

                var hasRootSuccess = false

                process.inputStream.toLineFlow().collect { line ->
                    println(line)
                    if (line.contains("uid=0") || line.contains("KernelSU setup completed successfully") || line.contains("KernelSU LKM module inserted")) {
                        hasRootSuccess = true
                    }
                    launch(Dispatchers.Main) {
                        logLines.add(line)
                    }
                }

                val returnVal = process.waitFor()
                println("cheese returned $returnVal")

                launch(Dispatchers.Main) {
                    window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                    logLines.add("[*] 进程退出，返回码: $returnVal")
                    if (returnVal == 0 || hasRootSuccess) {
                        exploitState.value = ExploitState.SUCCESS
                        logLines.add("[+] [SUCCESS] 提权流程成功完成！已获得 Root 权限并加载 KernelSU。")
                    } else {
                        exploitState.value = ExploitState.FAILED
                        logLines.add("[!] [NOTICE] 提权返回非零状态。如未获取 Root，请重启手机后重新尝试。")
                    }
                }
            } catch (e: Exception) {
                launch(Dispatchers.Main) {
                    window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                    exploitState.value = ExploitState.FAILED
                    logLines.add("[!] 发生异常: ${e.message}")
                }
            }
        }
    }

    private fun clearLogs() {
        logLines.clear()
        exploitState.value = ExploitState.IDLE
        initLogBanner()
    }

    private fun copyLogs() {
        val clipboardManager = getSystemService(CLIPBOARD_SERVICE) as ClipboardManager
        val allLogs = logLines.joinToString("\n")
        clipboardManager.setPrimaryClip(ClipData.newPlainText("Katyusha Logs", allLogs))
        Toast.makeText(this, "日志已复制到剪切板", Toast.LENGTH_SHORT).show()
    }

    private fun openKernelSU() {
        val ksuPkg = "me.weishu.kernelsu"
        val launchIntent = packageManager.getLaunchIntentForPackage(ksuPkg)
        if (launchIntent != null) {
            startActivity(launchIntent)
        } else {
            Toast.makeText(this, "未安装 KernelSU 管理器，正在前往下载页面...", Toast.LENGTH_LONG).show()
            try {
                val browserIntent = Intent(
                    Intent.ACTION_VIEW,
                    Uri.parse("https://github.com/tiann/KernelSU/releases")
                )
                startActivity(browserIntent)
            } catch (e: Exception) {
                Toast.makeText(this, "无法打开浏览器: ${e.message}", Toast.LENGTH_SHORT).show()
            }
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
    onOpenKernelSU: () -> Unit,
) {
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
                            text = "黑鲨 5 Pro · 临时 Root & KernelSU",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
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
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            // 1. Device & Exploit Info Card
            DeviceStatusCard(state = state)

            // 2. Safety / Discipline Card
            DisciplineCard()

            // 3. Monospace Terminal Execution Console
            TerminalCard(
                logs = logs,
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f),
                onClear = onClearLogs,
                onCopy = onCopyLogs
            )

            // 4. Hero Action Controls
            ActionSection(
                state = state,
                onStart = onStartExploit,
                onOpenKernelSU = onOpenKernelSU
            )
        }
    }
}

@Composable
private fun DeviceStatusCard(
    state: ExploitState,
    modifier: Modifier = Modifier
) {
    val deviceModel = if (Build.MODEL.contains("KTUS", ignoreCase = true) || Build.PRODUCT.contains("katyusha", ignoreCase = true)) {
        "Black Shark 5 Pro"
    } else {
        "${Build.MANUFACTURER.replaceFirstChar { it.uppercase() }} ${Build.MODEL}"
    }
    val soc = if (Build.HARDWARE.contains("taro") || Build.HARDWARE.contains("qcom") || Build.BOARD.contains("taro")) {
        "SM8450 · Adreno 730"
    } else {
        Build.HARDWARE
    }
    val kernelVer = System.getProperty("os.version") ?: "5.10.101"

    Card(
        modifier = modifier.fillMaxWidth(),
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surfaceContainer
        ),
        elevation = CardDefaults.cardElevation(defaultElevation = 2.dp)
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
                        text = "设备与运行状态",
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
                    label = "型号",
                    value = deviceModel,
                    modifier = Modifier.weight(1f)
                )
                InfoChip(
                    icon = Icons.Default.Memory,
                    label = "SoC / GPU",
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
                    label = "内核版本",
                    value = kernelVer.take(18),
                    modifier = Modifier.weight(1f)
                )
                InfoChip(
                    icon = Icons.Default.Security,
                    label = "提权途径",
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
            "就绪"
        )
        ExploitState.RUNNING -> Triple(
            MaterialTheme.colorScheme.tertiaryContainer,
            MaterialTheme.colorScheme.onTertiaryContainer,
            "提权中..."
        )
        ExploitState.SUCCESS -> Triple(
            Color(0xFFDCFCE7),
            Color(0xFF166534),
            "已激活 Root"
        )
        ExploitState.FAILED -> Triple(
            MaterialTheme.colorScheme.errorContainer,
            MaterialTheme.colorScheme.onErrorContainer,
            "需要重启"
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
        color = MaterialTheme.colorScheme.surfaceContainerHigh
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
                    overflow = androidx.compose.ui.text.style.TextOverflow.Ellipsis
                )
            }
        }
    }
}

@Composable
private fun DisciplineCard(
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier.fillMaxWidth(),
        shape = MaterialTheme.shapes.medium,
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.tertiaryContainer.copy(alpha = 0.35f)
        ),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.tertiary.copy(alpha = 0.25f))
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
                    text = "执行纪律 (重要安全提示)",
                    style = MaterialTheme.typography.titleSmall,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.onTertiaryContainer
                )
            }

            Text(
                text = "• 保持屏幕常亮：提权需约 30~90 秒，锁屏会导致 GPU 驱动通道中断\n" +
                        "• 每次开机仅限运行一次：单次开机重复喷射已被证实会导致 kernel_panic\n" +
                        "• 失败立即重启：喷射地址随机化，若未成功请直接重启手机后再试\n" +
                        "• 纯内存非持久化：不修改 /system 或 boot 分区，重启即可完全复原",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onTertiaryContainer,
                fontWeight = FontWeight.Normal,
                lineHeight = 19.sp
            )
        }
    }
}

@Composable
private fun TerminalCard(
    logs: List<String>,
    modifier: Modifier = Modifier,
    onClear: () -> Unit,
    onCopy: () -> Unit
) {
    val listState = rememberLazyListState()

    LaunchedEffect(logs.size) {
        if (logs.isNotEmpty()) {
            listState.animateScrollToItem(logs.size - 1)
        }
    }

    Card(
        modifier = modifier
            .border(1.dp, Color(0xFF334155), MaterialTheme.shapes.medium),
        shape = MaterialTheme.shapes.medium,
        colors = CardDefaults.cardColors(
            containerColor = TerminalBg
        )
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Terminal Top Bar
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(TerminalTopBar)
                    .padding(horizontal = 12.dp, vertical = 6.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    // Terminal mock window dots
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        Box(Modifier.size(10.dp).clip(CircleShape).background(Color(0xFFEF4444)))
                        Box(Modifier.size(10.dp).clip(CircleShape).background(Color(0xFFF59E0B)))
                        Box(Modifier.size(10.dp).clip(CircleShape).background(Color(0xFF22C55E)))
                    }

                    Spacer(modifier = Modifier.width(4.dp))

                    Text(
                        text = "控制台输出",
                        style = MaterialTheme.typography.labelMedium,
                        fontWeight = FontWeight.Bold,
                        color = TerminalText
                    )

                    Surface(
                        shape = CircleShape,
                        color = Color(0xFF334155)
                    ) {
                        Text(
                            text = "${logs.size} 行",
                            style = MaterialTheme.typography.labelSmall,
                            color = TerminalCyan,
                            modifier = Modifier.padding(horizontal = 6.dp, vertical = 2.dp)
                        )
                    }
                }

                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    IconButton(
                        onClick = onCopy,
                        modifier = Modifier.size(32.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.ContentCopy,
                            contentDescription = "复制日志",
                            tint = TerminalText,
                            modifier = Modifier.size(16.dp)
                        )
                    }

                    IconButton(
                        onClick = onClear,
                        modifier = Modifier.size(32.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.DeleteSweep,
                            contentDescription = "清空控制台",
                            tint = TerminalText,
                            modifier = Modifier.size(16.dp)
                        )
                    }
                }
            }

            // Monospace Logs View
            SelectionContainer(modifier = Modifier.fillMaxSize()) {
                LazyColumn(
                    state = listState,
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(horizontal = 10.dp, vertical = 8.dp),
                    verticalArrangement = Arrangement.spacedBy(3.dp)
                ) {
                    itemsIndexed(logs) { index, line ->
                        TerminalLineItem(line = line)
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
                line.contains("释放组件") || line.contains("就绪") -> TerminalCyan

        line.contains("[!]") || line.contains("kernel_panic") || line.contains("NOTICE") ||
                line.contains("异常") || line.contains("失败") -> TerminalRed

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
            enabled = state != ExploitState.RUNNING,
            modifier = Modifier
                .fillMaxWidth()
                .height(52.dp),
            shape = MaterialTheme.shapes.medium,
            colors = ButtonDefaults.buttonColors(
                containerColor = MaterialTheme.colorScheme.primary
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
                    text = "正在提权与注入内核模块...",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold
                )
            } else {
                Icon(
                    imageVector = Icons.Default.PlayArrow,
                    contentDescription = null,
                    modifier = Modifier.size(22.dp)
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    text = if (state == ExploitState.SUCCESS) "再次激活 (若已重启)" else "一键激活 Root & KernelSU",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold
                )
            }
        }

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            OutlinedButton(
                onClick = onOpenKernelSU,
                modifier = Modifier
                    .weight(1f)
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
                    text = "打开 KernelSU",
                    style = MaterialTheme.typography.labelLarge
                )
            }
        }
    }
}
