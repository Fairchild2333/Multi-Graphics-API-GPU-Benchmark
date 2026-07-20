package com.mangekyo.benchmark.ui.components

import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import com.mangekyo.benchmark.core.NativeBridge

/**
 * 3D 渲染表面占位：Surface 生命周期 → NativeBridge → 引擎 ANativeWindow。
 *
 * TODO(next-ai)：
 *  1. 引擎侧用 ANativeWindow_fromSurface 接管；Vulkan 建 VkAndroidSurfaceKHR，
 *     GL ES 走 EGL（替代桌面 GLFW 的表面层，见 HANDOFF 目标 C 第 3 条）；
 *  2. surfaceChanged 尺寸/旋转传引擎；surfaceDestroyed 必须同步等引擎释放，避免竞态；
 *  3. 正式 time-mode 需评估离屏路径以绕过合成器节流（对齐 macOS 离屏做法）。
 */
@Composable
fun BenchmarkSurface(modifier: Modifier = Modifier) {
    AndroidView(
        modifier = modifier,
        factory = { context ->
            SurfaceView(context).apply {
                holder.addCallback(object : SurfaceHolder.Callback {
                    override fun surfaceCreated(holder: SurfaceHolder) {
                        NativeBridge.setSurface(holder.surface)
                    }

                    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
                        // TODO(next-ai): NativeBridge.resize(width, height)
                    }

                    override fun surfaceDestroyed(holder: SurfaceHolder) {
                        NativeBridge.setSurface(null)
                    }
                })
            }
        },
    )
}
