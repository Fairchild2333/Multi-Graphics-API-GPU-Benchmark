package com.mangekyo.benchmark.ui

import androidx.compose.ui.*

import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.BarChart
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material.icons.filled.Speed
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarDefaults
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import com.mangekyo.benchmark.ui.screens.ChartsScreen
import com.mangekyo.benchmark.ui.screens.CpuScreen
import com.mangekyo.benchmark.ui.screens.GpuScreen
import com.mangekyo.benchmark.ui.screens.HistoryScreen

/**
 * Information architecture mirrors WinUI / SwiftUI: GPU / CPU / History / Charts.
 * Workload capability must eventually come from the shared C++ registry (HANDOFF rule 6).
 */
enum class AppDestination(val route: String, val label: String, val icon: ImageVector) {
    Gpu("gpu", "GPU", Icons.Filled.Speed),
    Cpu("cpu", "CPU", Icons.Filled.Memory),
    History("history", "History", Icons.Filled.History),
    Charts("charts", "Charts", Icons.Filled.BarChart),
}

@Composable
fun AppRoot() {
    val navController = rememberNavController()
    val backStackEntry by navController.currentBackStackEntryAsState()
    val currentRoute = backStackEntry?.destination?.route

    Scaffold(
        containerColor = MaterialTheme.colorScheme.surface,
        bottomBar = {
            NavigationBar(
                containerColor = MaterialTheme.colorScheme.surfaceContainer,
                windowInsets = NavigationBarDefaults.windowInsets,
            ) {
                AppDestination.entries.forEach { dest ->
                    NavigationBarItem(
                        selected = currentRoute == dest.route,
                        onClick = {
                            if (currentRoute != dest.route) {
                                navController.navigate(dest.route) {
                                    popUpTo(navController.graph.startDestinationId) {
                                        saveState = true
                                    }
                                    launchSingleTop = true
                                    restoreState = true
                                }
                            }
                        },
                        icon = { Icon(dest.icon, contentDescription = dest.label) },
                        label = { Text(dest.label) },
                    )
                }
            }
        },
    ) { innerPadding ->
        NavHost(
            navController = navController,
            startDestination = AppDestination.Gpu.route,
            modifier = Modifier.padding(innerPadding),
        ) {
            composable(AppDestination.Gpu.route) { GpuScreen() }
            composable(AppDestination.Cpu.route) { CpuScreen() }
            composable(AppDestination.History.route) { HistoryScreen() }
            composable(AppDestination.Charts.route) { ChartsScreen() }
        }
    }
}
