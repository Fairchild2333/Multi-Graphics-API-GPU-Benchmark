package com.mangekyo.benchmark.ui.screens

import androidx.compose.ui.*

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.DeleteSweep
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.mangekyo.benchmark.core.ResultsStore
import com.mangekyo.benchmark.core.tr
import com.mangekyo.benchmark.core.workloadTitle
import com.mangekyo.benchmark.ui.LocalWindowWidth

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HistoryScreen() {
    val context = LocalContext.current
    val store = remember(context) { ResultsStore(context) }
    var entries by remember { mutableStateOf(store.loadEntries()) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(tr("History", "历史")) },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surfaceContainer,
                ),
                actions = {
                    IconButton(
                        onClick = {
                            store.clearAll()
                            entries = store.loadEntries()
                        },
                        enabled = entries.isNotEmpty(),
                    ) {
                        Icon(Icons.Filled.DeleteSweep, contentDescription = tr("Clear all", "全部清除"))
                    }
                },
            )
        },
    ) { inner ->
        if (entries.isEmpty()) {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(inner)
                    .padding(LocalWindowWidth.current.margin),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Text(
                    tr("No results yet", "还没有成绩"),
                    style = MaterialTheme.typography.titleMedium,
                )
                Text(
                    tr(
                        "Scores will appear here after the engine writes results.json under app-specific storage. " +
                            "Group by workloadVersion / ABI — never mix Windows desktop lanes.",
                        "引擎把成绩写入 App 专属目录下的 results.json 后，会显示在这里。" +
                            "成绩按测试版本和 ABI 分组，不要和 Windows 桌面版的成绩放在一起比较。",
                    ),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Text(
                    store.resultsFile.absolutePath,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.outline,
                )
            }
        } else {
            // One column on a phone, two or three on an opened foldable or a tablet.
            LazyVerticalGrid(
                columns = GridCells.Adaptive(minSize = 320.dp),
                modifier = Modifier
                    .fillMaxSize()
                    .padding(inner)
                    .padding(horizontal = LocalWindowWidth.current.margin),
                contentPadding = PaddingValues(top = 8.dp, bottom = 16.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                items(entries, key = { it.id.ifBlank { it.hashCode().toString() } }) { e ->
                    Card(
                        modifier = Modifier.fillMaxWidth(),
                        colors = CardDefaults.cardColors(
                            containerColor = MaterialTheme.colorScheme.surfaceContainerHighest,
                        ),
                    ) {
                        Column(
                            modifier = Modifier.padding(14.dp),
                            verticalArrangement = Arrangement.spacedBy(2.dp),
                        ) {
                            Text(
                                "${workloadTitle(e.workload)} · ${e.graphicsApi}",
                                style = MaterialTheme.typography.titleMedium,
                            )
                            Text(
                                e.deviceName,
                                style = MaterialTheme.typography.bodyMedium,
                            )
                            Text(
                                e.workloadVersion,
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                            val scoreText = when {
                                e.score != null && e.scoreUnit != null ->
                                    "${e.score} ${e.scoreUnit}"
                                e.score != null -> e.score.toString()
                                else -> "—"
                            }
                            Text(scoreText, style = MaterialTheme.typography.labelLarge)
                            if (e.machineLine.isNotEmpty()) {
                                Text(
                                    e.machineLine,
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                            listOfNotNull(e.timestamp, e.abi)
                                .joinToString(" · ")
                                .takeIf { it.isNotBlank() }
                                ?.let {
                                    Text(
                                        it,
                                        style = MaterialTheme.typography.bodySmall,
                                        color = MaterialTheme.colorScheme.outline,
                                    )
                                }
                        }
                    }
                }
            }
        }
    }
}
