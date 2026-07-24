package com.mangekyo.benchmark.ui.screens

import androidx.compose.ui.*

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Stop
import androidx.compose.runtime.LaunchedEffect
import kotlinx.coroutines.delay
import androidx.compose.material3.AssistChip
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.mangekyo.benchmark.core.CapabilityGate
import com.mangekyo.benchmark.core.GraphicsBackend
import com.mangekyo.benchmark.core.NativeBridge
import com.mangekyo.benchmark.core.WorkloadRegistry
import com.mangekyo.benchmark.ui.components.BenchmarkSurface

/** GPU page shell aligned with WinUI. Engine start/cancel still TODO. */
@OptIn(ExperimentalMaterial3Api::class, ExperimentalLayoutApi::class)
@Composable
fun GpuScreen() {
    val context = LocalContext.current
    val caps = remember(context) {
        NativeBridge.init(context)
        CapabilityGate.snapshot(context)
    }
    var selectedWorkloadId by rememberSaveable {
        mutableStateOf(WorkloadRegistry.gpuWorkloads.firstOrNull()?.id.orEmpty())
    }
    var selectedBackend by rememberSaveable {
        mutableStateOf(
            when (caps.preferredBackend) {
                GraphicsBackend.Vulkan -> "Vulkan"
                GraphicsBackend.Gles31 -> "GLES31"
                GraphicsBackend.Unsupported -> "Unsupported"
            },
        )
    }
    var running by remember { mutableStateOf(NativeBridge.isRunning()) }
    var statusText by remember { mutableStateOf("") }
    LaunchedEffect(Unit) {
        while (true) {
            running = NativeBridge.isRunning()
            delay(250)
        }
    }
    val scrollBehavior = TopAppBarDefaults.pinnedScrollBehavior()
    val vulkanOk = selectedBackend == "Vulkan" && caps.vulkanLoaderPresent && NativeBridge.isEngineLoaded
    val runEnabled = vulkanOk && selectedWorkloadId.isNotEmpty() &&
        (selectedWorkloadId == "stream" || selectedWorkloadId == "gpu_burn")

    Scaffold(
        modifier = Modifier.nestedScroll(scrollBehavior.nestedScrollConnection),
        topBar = {
            TopAppBar(
                title = { Text("GPU Benchmark") },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surfaceContainer,
                    titleContentColor = MaterialTheme.colorScheme.onSurface,
                ),
                scrollBehavior = scrollBehavior,
            )
        },
        floatingActionButton = {
            FloatingActionButton(
                onClick = {
                    if (running) {
                        NativeBridge.stopWorkload()
                        statusText = "Stop requested"
                    } else if (runEnabled) {
                        val ok = NativeBridge.startWorkload(selectedWorkloadId, 3.0)
                        statusText = if (ok) {
                            "Running $selectedWorkloadId (3s preview)"
                        } else {
                            NativeBridge.lastError().ifEmpty { "Start failed" }
                        }
                        running = NativeBridge.isRunning()
                    }
                },
                containerColor = if (runEnabled || running) {
                    MaterialTheme.colorScheme.primaryContainer
                } else {
                    MaterialTheme.colorScheme.surfaceContainerHighest
                },
                contentColor = if (runEnabled || running) {
                    MaterialTheme.colorScheme.onPrimaryContainer
                } else {
                    MaterialTheme.colorScheme.onSurfaceVariant
                },
            ) {
                if (running) {
                    Icon(Icons.Filled.Stop, contentDescription = "Stop")
                } else {
                    Icon(Icons.Filled.PlayArrow, contentDescription = "Run")
                }
            }
        },
    ) { inner ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(inner)
                .padding(horizontal = 16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            item {
                Box(modifier = Modifier.height(4.dp))
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.surfaceContainerLow,
                    ),
                ) {
                    Column(
                        modifier = Modifier.padding(12.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        Text("Preview surface", style = MaterialTheme.typography.titleMedium)
                        BenchmarkSurface(
                            modifier = Modifier
                                .fillMaxWidth()
                                .height(200.dp),
                        )
                        Text(
                            text = "Engine: ${NativeBridge.engineVersion()}",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }
            }

            item {
                Text("Backend", style = MaterialTheme.typography.titleMedium)
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    FilterChip(
                        selected = selectedBackend == "Vulkan",
                        onClick = { selectedBackend = "Vulkan" },
                        enabled = caps.vulkanLoaderPresent,
                        label = { Text("Vulkan") },
                    )
                    FilterChip(
                        selected = selectedBackend == "GLES31",
                        onClick = { selectedBackend = "GLES31" },
                        label = { Text("GL ES 3.1") },
                    )
                }
                FlowRow(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.padding(top = 8.dp),
                ) {
                    AssistChip(
                        onClick = {},
                        label = {
                            Text(
                                if (caps.vulkanLoaderPresent) "Vulkan loader OK"
                                else "Vulkan unavailable",
                            )
                        },
                    )
                    AssistChip(
                        onClick = {},
                        label = { Text("GL ES ${caps.glesVersionLabel}") },
                    )
                    if (caps.vulkanHardwareFeature) {
                        AssistChip(onClick = {}, label = { Text("Vulkan HW feature") })
                    }
                    AssistChip(onClick = {}, label = { Text(caps.abi) })
                    AssistChip(onClick = {}, label = { Text(caps.androidRelease) })
                }
                caps.notes.forEach { note ->
                    Text(
                        text = note,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.padding(top = 4.dp),
                    )
                }
            }

            item {
                Text("Workloads", style = MaterialTheme.typography.titleMedium)
                Text(
                    text = "Versions are Android placeholders — must not mix with Windows score groups.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }

            items(WorkloadRegistry.gpuWorkloads, key = { it.id }) { workload ->
                val selected = workload.id == selectedWorkloadId
                Card(
                    onClick = { selectedWorkloadId = workload.id },
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = if (selected) {
                            MaterialTheme.colorScheme.secondaryContainer
                        } else {
                            MaterialTheme.colorScheme.surfaceContainerHighest
                        },
                    ),
                ) {
                    Column(modifier = Modifier.padding(14.dp)) {
                        Text(workload.displayName, style = MaterialTheme.typography.titleMedium)
                        Text(
                            text = workload.workloadVersion,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        if (workload.isPreview) {
                            Text(
                                text = "preview",
                                style = MaterialTheme.typography.labelLarge,
                                color = MaterialTheme.colorScheme.tertiary,
                                modifier = Modifier.padding(top = 4.dp),
                            )
                        }
                    }
                }
            }

            item {
                Text(
                    text = when {
                        statusText.isNotEmpty() -> statusText
                        !vulkanOk -> "Vulkan Run needs API 24+ loader + Surface. GLES path not wired yet."
                        else -> "Preview Run: 3s Stream/GPU Burn via gpu_engine. Scores are android_preview — do not mix with Windows."
                    },
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Box(modifier = Modifier.height(72.dp))
            }
        }
    }
}
