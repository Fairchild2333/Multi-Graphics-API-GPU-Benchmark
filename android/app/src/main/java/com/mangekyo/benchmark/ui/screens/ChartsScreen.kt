package com.mangekyo.benchmark.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.mangekyo.benchmark.core.HistoryEntry
import com.mangekyo.benchmark.core.ResultsStore
import com.mangekyo.benchmark.core.tr
import com.mangekyo.benchmark.core.workloadTitle
import com.mangekyo.benchmark.ui.components.BarRows

/** One chart: the best score of every device and API under one version of a test. */
private data class WorkloadChart(
    val workload: String,
    val version: String,
    val unit: String,
    val bars: List<Pair<String, Double>>,
)

/**
 * Results of different versions of a test are not comparable, so a chart uses
 * the version of the newest result only.
 */
private fun buildCharts(entries: List<HistoryEntry>): List<WorkloadChart> {
    val order = listOf("stream", "gpu_burn", "cinematic_liquid", "membw", "cpu_single_core", "cpu_multi_core", "cpu_membw")
    return entries
        .filter { (it.score ?: 0.0) > 0.0 }
        .groupBy { it.workload }
        .toList()
        .sortedBy { (workload, _) -> order.indexOf(workload).let { if (it < 0) order.size else it } }
        .mapNotNull { (workload, rows) ->
            val newest = rows.maxByOrNull { it.timestamp.orEmpty() } ?: return@mapNotNull null
            val cpu = newest.graphicsApi.equals("CPU", ignoreCase = true)
            val bars = rows
                .filter { it.workloadVersion == newest.workloadVersion }
                .groupBy { if (cpu) it.deviceName else "${it.deviceName} — ${it.graphicsApi}" }
                .map { (label, group) -> label to group.maxOf { it.score ?: 0.0 } }
                .sortedBy { it.first }
            if (bars.isEmpty()) null
            else WorkloadChart(workload, newest.workloadVersion, newest.scoreUnit.orEmpty(), bars)
        }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ChartsScreen() {
    val context = LocalContext.current
    val store = remember(context) { ResultsStore(context) }
    // Read again whenever the page is opened.
    val charts = remember { buildCharts(store.loadEntries()) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(tr("Charts", "图表")) },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surfaceContainer,
                ),
            )
        },
    ) { inner ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(inner)
                .padding(horizontal = 16.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Spacer(Modifier.height(4.dp))
            if (charts.isEmpty()) {
                Text(tr("No results yet", "还没有成绩"), style = MaterialTheme.typography.titleMedium)
                Text(
                    tr(
                        "Run a test on the GPU or CPU page. Each test then gets a chart with the best score " +
                            "of every device here.",
                        "先在 GPU 页或 CPU 页运行一项测试，这里会为每项测试画一张图，显示各设备的最好成绩。",
                    ),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            charts.forEach { chart ->
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.surfaceContainerLow,
                    ),
                ) {
                    Column(
                        modifier = Modifier.padding(14.dp),
                        verticalArrangement = Arrangement.spacedBy(6.dp),
                    ) {
                        Text(workloadTitle(chart.workload), style = MaterialTheme.typography.titleMedium)
                        Text(
                            chart.unit + tr(", best saved score per device", "，每个设备保存过的最好成绩") +
                                " · ${chart.version}",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        BarRows(chart.bars, chart.unit)
                    }
                }
            }
            if (charts.isNotEmpty()) {
                Text(
                    tr(
                        "Each chart uses only the results of the newest version of its test. Every run is " +
                            "listed on the History page.",
                        "每张图只使用该测试最新版本的成绩；每一次运行都列在历史页里。",
                    ),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.outline,
                )
            }
            Spacer(Modifier.height(16.dp))
        }
    }
}
