package com.mangekyo.benchmark.ui.screens

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

/**
 * History 页（对齐 WinUI History）。
 *
 * TODO(next-ai)：
 *  1. 从 ResultsStore 读 results.json（应用专属目录）；
 *  2. 按 workloadVersion 分组过滤——不同版本/preview/ABI 绝不混排（HANDOFF 合同）；
 *  3. 清空、导出/分享（SAF 或系统分享，不申请存储权限）。
 */
@Composable
fun HistoryScreen() {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
    ) {
        Text("History", style = MaterialTheme.typography.headlineSmall)
        Text(
            "No results yet (ResultsStore stub).",
            style = MaterialTheme.typography.bodyMedium,
            modifier = Modifier.padding(top = 12.dp),
        )
    }
}
