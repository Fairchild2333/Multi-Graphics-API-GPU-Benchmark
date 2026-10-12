package com.mangekyo.benchmark.ui.components

import androidx.compose.ui.*

import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.draw.clip
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.mangekyo.benchmark.core.NativeBridge

/**
 * The surface the engine presents to: Surface → NativeBridge → ANativeWindow.
 *
 * The SurfaceView must not have a background of its own. A view background is
 * drawn into the window, in front of the surface, and hides everything the
 * engine renders (seen on the Android 17 emulator, 2026-10-07). Until a test
 * has drawn something, [showPlaceholder] covers the empty surface instead.
 *
 * The engine always renders 1280 x 720 into it; the system scales that to the
 * size of this view. [framed] rounds and outlines it for use inside a page.
 */
@Composable
fun BenchmarkSurface(
    modifier: Modifier = Modifier,
    showPlaceholder: Boolean = false,
    framed: Boolean = true,
) {
    val shape = RoundedCornerShape(12.dp)

    Box(
        modifier = if (framed) {
            modifier
                .clip(shape)
                .border(1.dp, MaterialTheme.colorScheme.outlineVariant, shape)
        } else {
            modifier
        },
    ) {
        AndroidView(
            modifier = Modifier.fillMaxSize(),
            factory = { context ->
                SurfaceView(context).apply {
                    holder.addCallback(object : SurfaceHolder.Callback {
                        override fun surfaceCreated(holder: SurfaceHolder) {
                            NativeBridge.setSurface(holder.surface)
                        }

                        override fun surfaceChanged(
                            holder: SurfaceHolder,
                            format: Int,
                            width: Int,
                            height: Int,
                        ) {
                            NativeBridge.resizeSurface(width, height)
                        }

                        override fun surfaceDestroyed(holder: SurfaceHolder) {
                            NativeBridge.setSurface(null)
                        }
                    })
                }
            },
        )
        if (showPlaceholder) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(MaterialTheme.colorScheme.surfaceContainerHighest),
            )
        }
    }
}
