package com.mangekyo.benchmark.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.mangekyo.benchmark.core.CapabilityGate
import com.mangekyo.benchmark.core.WorkloadRegistry
import com.mangekyo.benchmark.ui.components.BenchmarkSurface

/**
 * GPU 页（对齐 WinUI GPU 页）。
 *
 * TODO(next-ai) 按 HANDOFF 目标 C 第 3 条实现：
 *  1. workload 列表来自 WorkloadRegistry（最终与 C++ registry 同源，勿手写漂移）；
 *  2. 后端选择：Vulkan（CapabilityGate 运行时门控，API>=24 + dlopen 成功）/ GL ES 3.1；
 *     能力不齐显式 unsupported，不静默 fallback；
 *  3. Duration 预设与固定 15s 合同；Run/Cancel；满载时 Stop 必须始终可用（跑分不占 UI 线程）；
 *  4. 进度/实时指标经 NativeBridge 回调；结果写 ResultsStore（schema 与 Windows 侧一致）；
 *  5. 温控评估：记录降频事实，勿静默修改 Burst 语义。
 */
@Composable
fun GpuScreen() {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("GPU Benchmark", style = MaterialTheme.typography.headlineSmall)

        // 3D 渲染表面占位：真正的 ANativeWindow 交给引擎，Material 3 只管壳层。
        BenchmarkSurface(
            modifier = Modifier
                .fillMaxWidth()
                .height(220.dp),
        )

        Text(
            text = "Vulkan available: ${CapabilityGate.vulkanAvailable()} (stub)",
            style = MaterialTheme.typography.bodySmall,
        )

        LazyColumn(
            modifier = Modifier.weight(1f),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            items(WorkloadRegistry.gpuWorkloads) { workload ->
                Card(modifier = Modifier.fillMaxWidth()) {
                    Column(modifier = Modifier.padding(12.dp)) {
                        Text(workload.displayName, style = MaterialTheme.typography.titleMedium)
                        Text(workload.workloadVersion, style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
        }

        Button(
            onClick = { /* TODO(next-ai): NativeBridge 启动 workload */ },
            enabled = false, // 引擎未接入前禁用
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text("Run (engine not wired)")
        }
    }
}
