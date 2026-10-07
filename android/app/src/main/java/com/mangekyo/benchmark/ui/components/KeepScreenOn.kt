package com.mangekyo.benchmark.ui.components

import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.ui.platform.LocalView

/**
 * Keeps the screen on while `active` is true. A CPU test or a memory sweep
 * takes minutes; a phone that turns its screen off in between slows the app
 * down and spoils the measurement.
 */
@Composable
fun KeepScreenOn(active: Boolean) {
    val view = LocalView.current
    DisposableEffect(view, active) {
        view.keepScreenOn = active
        onDispose { view.keepScreenOn = false }
    }
}
