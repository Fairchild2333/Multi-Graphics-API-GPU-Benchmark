package com.mangekyo.benchmark.ui

import androidx.compose.ui.*

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.consumeWindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.systemBars
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.BarChart
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material.icons.filled.Speed
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ShortNavigationBar
import androidx.compose.material3.ShortNavigationBarItem
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.WideNavigationRail
import androidx.compose.material3.WideNavigationRailItem
import androidx.compose.material3.WideNavigationRailValue
import androidx.compose.material3.rememberWideNavigationRailState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import com.mangekyo.benchmark.ui.screens.ChartsScreen
import com.mangekyo.benchmark.ui.screens.CpuScreen
import com.mangekyo.benchmark.ui.screens.GpuScreen
import com.mangekyo.benchmark.core.AutoRun
import com.mangekyo.benchmark.core.tr
import com.mangekyo.benchmark.ui.screens.HistoryScreen

/**
 * Information architecture mirrors WinUI / SwiftUI: GPU / CPU / History / Charts.
 * Workload capability must eventually come from the shared C++ registry (HANDOFF rule 6).
 */
enum class AppDestination(
    val route: String,
    private val en: String,
    private val zh: String,
    val icon: ImageVector,
) {
    Gpu("gpu", "GPU", "GPU", Icons.Filled.Speed),
    Cpu("cpu", "CPU", "CPU", Icons.Filled.Memory),
    History("history", "History", "历史", Icons.Filled.History),
    Charts("charts", "Charts", "图表", Icons.Filled.BarChart);

    /** Read on every use, so it follows a change of the system language. */
    val label: String get() = tr(en, zh)
}

private fun NavHostController.open(route: String) {
    if (currentDestination?.route == route) return
    navigate(route) {
        popUpTo(graph.startDestinationId) {
            saveState = true
        }
        launchSingleTop = true
        restoreState = true
    }
}

/**
 * The shell of the app. A narrow window has the destinations in a bar at the
 * bottom; from 600 dp they move to a rail at the side, which also shows their
 * names beside the icons from 1200 dp. Bar and rail are the navigation
 * components of Material 3 Expressive.
 *
 * The pages sit at one place in the composition whatever the width, so opening
 * a foldable or resizing the window in the middle of a test only moves the
 * navigation and does not rebuild the page or its preview surface.
 */
@Composable
fun AppRoot() {
    val navController = rememberNavController()
    val backStackEntry by navController.currentBackStackEntryAsState()
    val currentRoute = backStackEntry?.destination?.route

    // A test requested over adb opens its page; the page then starts it.
    val request = AutoRun.pending
    LaunchedEffect(request) {
        val route = when {
            request == null -> return@LaunchedEffect
            request.test in AutoRun.gpuTests -> AppDestination.Gpu.route
            else -> AppDestination.Cpu.route
        }
        navController.open(route)
    }

    BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
        val windowWidth = windowWidthOf(maxWidth)
        val sideNavigation = windowWidth.sideNavigation
        val railState = rememberWideNavigationRailState()
        val railExpanded = railState.targetValue == WideNavigationRailValue.Expanded
        LaunchedEffect(windowWidth) {
            railState.snapTo(
                if (windowWidth == WindowWidth.Large) WideNavigationRailValue.Expanded
                else WideNavigationRailValue.Collapsed,
            )
        }

        CompositionLocalProvider(LocalWindowWidth provides windowWidth) {
            Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.surface) {
                Row(modifier = Modifier.fillMaxSize()) {
                    if (sideNavigation) {
                        WideNavigationRail(state = railState) {
                            AppDestination.entries.forEach { dest ->
                                WideNavigationRailItem(
                                    selected = currentRoute == dest.route,
                                    onClick = { navController.open(dest.route) },
                                    icon = { Icon(dest.icon, contentDescription = null) },
                                    label = { Text(dest.label) },
                                    railExpanded = railExpanded,
                                )
                            }
                        }
                    }
                    Column(modifier = Modifier.weight(1f)) {
                        // The pages lay out their own bars. The edge that the
                        // navigation already covers must not be padded again.
                        val covered = WindowInsets.systemBars.only(
                            if (sideNavigation) WindowInsetsSides.Start else WindowInsetsSides.Bottom,
                        )
                        Box(
                            modifier = Modifier
                                .weight(1f)
                                .consumeWindowInsets(covered),
                        ) {
                            NavHost(
                                navController = navController,
                                startDestination = AppDestination.Gpu.route,
                            ) {
                                composable(AppDestination.Gpu.route) { GpuScreen() }
                                composable(AppDestination.Cpu.route) { CpuScreen() }
                                composable(AppDestination.History.route) { HistoryScreen() }
                                composable(AppDestination.Charts.route) { ChartsScreen() }
                            }
                        }
                        if (!sideNavigation) {
                            ShortNavigationBar {
                                AppDestination.entries.forEach { dest ->
                                    ShortNavigationBarItem(
                                        selected = currentRoute == dest.route,
                                        onClick = { navController.open(dest.route) },
                                        icon = { Icon(dest.icon, contentDescription = null) },
                                        label = { Text(dest.label) },
                                    )
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
