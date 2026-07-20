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
 * Charts 页（对齐 WinUI Charts）。
 *
 * TODO(next-ai)：结果对比图表；同 workloadVersion 内比较，跨版本/ABI 不出对比线。
 */
@Composable
fun ChartsScreen() {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
    ) {
        Text("Charts", style = MaterialTheme.typography.headlineSmall)
        Text(
            "Charts placeholder.",
            style = MaterialTheme.typography.bodyMedium,
            modifier = Modifier.padding(top = 12.dp),
        )
    }
}
