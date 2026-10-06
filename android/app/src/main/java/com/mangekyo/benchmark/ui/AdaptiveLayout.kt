package com.mangekyo.benchmark.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

/**
 * How wide the window of the app is, by the Material breakpoints. It is the
 * window that counts, not the device: a phone in split screen, a foldable that
 * is opened and a tablet window that is resized all change class while the
 * app runs.
 */
enum class WindowWidth {
    /** Below 600 dp: a phone in portrait, the cover screen of a foldable. */
    Compact,

    /** From 600 dp: an opened foldable, a small tablet in portrait. */
    Medium,

    /** From 840 dp: a tablet, an opened foldable in landscape. */
    Expanded,

    /** From 1200 dp: a large tablet, a desktop window. */
    Large;

    /** The destinations move from the bar at the bottom to a rail at the side. */
    val sideNavigation: Boolean get() = this != Compact

    /** A page shows its two halves beside each other instead of one below the other. */
    val twoPane: Boolean get() = this >= Expanded

    /** The margin between the content of a page and the edge of the window. */
    val margin: Dp get() = if (this == Compact) 16.dp else 24.dp
}

fun windowWidthOf(width: Dp): WindowWidth = when {
    width >= 1200.dp -> WindowWidth.Large
    width >= 840.dp -> WindowWidth.Expanded
    width >= 600.dp -> WindowWidth.Medium
    else -> WindowWidth.Compact
}

/** Set once in [AppRoot]; pages read it to choose their layout. */
val LocalWindowWidth = compositionLocalOf { WindowWidth.Compact }

/** One column is not stretched beyond this; a line of text or a chart wider than it reads badly. */
private val SinglePaneMaxWidth = 720.dp

/** Room at the end of a page for the button that floats over it. */
private val FloatingButtonSpace = 88.dp

/**
 * The content of a page. A narrow window gets one scrolling column, [primary]
 * above [secondary]; a wide window gets the two beside each other, each
 * scrolling on its own.
 *
 * [primary] is composed at the same place in both layouts. What it holds, the
 * preview surface of a running test in particular, therefore survives a fold,
 * a rotation or a resize of the window.
 */
@Composable
fun AdaptivePanes(
    modifier: Modifier = Modifier,
    primary: @Composable ColumnScope.() -> Unit,
    secondary: @Composable ColumnScope.() -> Unit,
) {
    val width = LocalWindowWidth.current
    val twoPane = width.twoPane
    Row(
        modifier = modifier
            .fillMaxSize()
            .padding(horizontal = width.margin),
        horizontalArrangement = Arrangement.spacedBy(if (twoPane) 24.dp else 0.dp),
    ) {
        Box(
            modifier = Modifier
                .weight(1f)
                .fillMaxHeight(),
            contentAlignment = Alignment.TopCenter,
        ) {
            Column(
                modifier = Modifier
                    .widthIn(max = if (twoPane) Dp.Unspecified else SinglePaneMaxWidth)
                    .fillMaxWidth()
                    .verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                Spacer(Modifier.height(4.dp))
                primary()
                if (!twoPane) secondary()
                Spacer(Modifier.height(FloatingButtonSpace))
            }
        }
        if (twoPane) {
            Column(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxHeight()
                    .verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                Spacer(Modifier.height(4.dp))
                secondary()
                Spacer(Modifier.height(FloatingButtonSpace))
            }
        }
    }
}
