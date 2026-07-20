package com.mangekyo.benchmark.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

/**
 * CPU 页（对齐 WinUI CPU 页；合同见 HANDOFF cpu_mixed_v1）。
 *
 * TODO(next-ai)：
 *  1. 模式 per-core | multi | all；正式合同 15.0s 测量 + 0.2s 预热 + r3 取中位数；
 *  2. Android affinity 合同：sched_setaffinity 设置后必须回读验证（strict_sched_affinity），
 *     失败 valid=0 / exit 3；不同 affinity capability 独立成组；
 *  3. 实时逐核/总进度、Run/Cancel；CPU 与 GPU 测试全局互斥；
 *  4. 大小核只写 Inferred* 排名标签，不宣称真实微架构识别；
 *  5. Android 无外置 CLI 进程模型 → in-process 运行，但 stdout 协议语义映射为回调，勿改结果 schema。
 */
@Composable
fun CpuScreen() {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("CPU Benchmark", style = MaterialTheme.typography.headlineSmall)
        Card(modifier = Modifier.fillMaxWidth()) {
            Text(
                text = "cpu_mixed_v1 — per-core / multi / all\n(15.0s + 0.2s warmup, r3 median, strict affinity)",
                style = MaterialTheme.typography.bodyMedium,
                modifier = Modifier.padding(12.dp),
            )
        }
        Button(
            onClick = { /* TODO(next-ai): NativeBridge 启动 cpu_mixed */ },
            enabled = false,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text("Run (engine not wired)")
        }
    }
}
