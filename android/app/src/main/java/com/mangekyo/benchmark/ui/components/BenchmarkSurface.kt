package com.mangekyo.benchmark.ui.components

import androidx.compose.ui.*

import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.mangekyo.benchmark.core.NativeBridge

/**
 * Preview / future present target: Surface → NativeBridge → ANativeWindow.
 * Engine swapchain / EGL still TODO.
 */
@Composable
fun BenchmarkSurface(modifier: Modifier = Modifier) {
    val fill = MaterialTheme.colorScheme.surfaceContainerHighest.toArgb()
    val shape = RoundedCornerShape(12.dp)

    Box(
        modifier = modifier
            .clip(shape)
            .border(1.dp, MaterialTheme.colorScheme.outlineVariant, shape),
    ) {
        AndroidView(
            modifier = Modifier.fillMaxSize(),
            factory = { context ->
                SurfaceView(context).apply {
                    setBackgroundColor(fill)
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
            update = { view -> view.setBackgroundColor(fill) },
        )
    }
}
