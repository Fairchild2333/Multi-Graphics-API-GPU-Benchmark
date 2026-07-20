package com.mangekyo.benchmark.core

import android.content.Context
import java.io.File

/**
 * 结果存储占位。
 *
 * 合同：
 *  - 写应用专属目录（scoped storage 下无需任何权限），对应 Windows 侧
 *    %LOCALAPPDATA%/GpuComputeBenchmark 语义；
 *  - schema 与 Windows results.json 完全一致，由同一份合同生成——勿在 Android 侧发明字段；
 *  - metadata 必须记录真实 ABI / SoC / 驱动 / Android 版本；不同 workloadVersion 与 ABI 不混排。
 *
 * TODO(next-ai)：读写实现 + History/Charts 数据源。
 */
class ResultsStore(private val context: Context) {
    val resultsFile: File
        get() = File(context.getExternalFilesDir(null) ?: context.filesDir, "results.json")
}
