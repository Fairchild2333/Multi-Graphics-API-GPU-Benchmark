package com.mangekyo.benchmark.ui.components

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.mangekyo.benchmark.core.MobileSuite
import com.mangekyo.benchmark.core.NativeBridge
import com.mangekyo.benchmark.core.SuiteScope
import com.mangekyo.benchmark.core.tr

@Composable
fun SuiteControls(gpu: Boolean) {
    val context = LocalContext.current
    val enabled = NativeBridge.isEngineLoaded && !MobileSuite.running && !NativeBridge.isRunning() && !NativeBridge.isCliRunning()
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = { MobileSuite.start(context, if (gpu) SuiteScope.GPU else SuiteScope.CPU) }, enabled = enabled) {
                Text(if (gpu) tr("Run all GPU tests", "一键测试 GPU 全套") else tr("Run all CPU tests", "一键测试 CPU 全套"))
            }
            if (gpu) Button(onClick = { MobileSuite.start(context, SuiteScope.ALL) }, enabled = enabled) {
                Text(tr("Test all items", "一键测试全部项目"))
            }
            Text(tr("Supported tests run in sequence; formal compute uses 15 s. Scores are saved automatically.",
                "按设备支持项依次运行，正式算力步骤为 15 秒，成绩自动保存。"))
            if (MobileSuite.status.isNotEmpty()) Text(MobileSuite.status)
            if (MobileSuite.running) {
                Text(MobileSuite.currentLabel)
                Button(onClick = { MobileSuite.cancel() }) { Text(tr("Stop suite", "停止全套测试")) }
            }
        }
    }
}
