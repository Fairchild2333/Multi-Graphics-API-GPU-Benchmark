package com.ghostlock.app.data

import android.content.ContentValues
import android.content.Context
import android.media.MediaScannerConnection
import android.os.Environment
import android.provider.MediaStore
import java.io.BufferedWriter
import java.io.Closeable
import java.io.File
import java.io.FileOutputStream
import java.io.OutputStreamWriter
import java.nio.channels.FileChannel
import java.nio.charset.StandardCharsets
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.atomic.AtomicLong

/** Debug-only, immediately visible archive for one attack attempt. */
internal class DebugAttackLog private constructor(
    private val context: Context,
    val displayName: String,
    /** Downloads-relative folder that holds the attempt log and its dumps. */
    val folderPath: String,
    private val writer: BufferedWriter,
    /** Backing channel for fsync; null when MediaStore hands out a pipe. */
    private val channel: FileChannel?,
) : Closeable {
    /**
     * Absolute folder handed to the native process through --dump-kernel-log;
     * the root prepare script drops kernel/pstore/iomem dumps next to the log.
     */
    @Suppress("DEPRECATION")
    val folderFile: File
        get() = File(Environment.getExternalStorageDirectory(), folderPath)

    @Synchronized
    fun append(line: String) {
        writer.write(line)
        writer.newLine()
        writer.flush()
        /* flush() 只到页缓存：内核 panic 时未 fsync 的数据块会丢，而 ext4 日志
         * 已提交的元数据（文件大小）还在——这正是 v590 里 panic 后导出文件全是
         * 等长 NUL 的原因。每行一次 force，把日志钉到介质上。 */
        channel?.force(true)
    }

    /**
     * Writes an extra file next to the attempt log (e.g. the resolved runtime
     * profile), through MediaStore so it is visible without a storage permission.
     */
    @Synchronized
    fun writeSidecar(name: String, bytes: ByteArray): Boolean {
        val values = ContentValues().apply {
            put(MediaStore.MediaColumns.DISPLAY_NAME, name)
            put(MediaStore.MediaColumns.MIME_TYPE, "application/octet-stream")
            put(MediaStore.MediaColumns.RELATIVE_PATH, folderPath)
        }
        val uri = context.contentResolver
            .insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values) ?: return false
        return runCatching {
            context.contentResolver.openOutputStream(uri, "w")?.use { output ->
                output.write(bytes)
                output.flush()
                (output as? FileOutputStream)?.channel?.force(true)
            }
            MediaScannerConnection.scanFile(
                context, arrayOf(File(folderFile, name).absolutePath), null, null,
            )
        }.isSuccess
    }

    @Synchronized
    override fun close() {
        writer.close()
        publishSidecars()
    }

    /** Ask MediaStore to index the root-written dumps so file managers show them. */
    private fun publishSidecars() {
        val paths = buildList {
            for (name in SidecarNames) {
                val file = File(folderFile, name)
                if (file.isFile) add(file.absolutePath)
            }
            val pstore = folderFile.resolve("pstore")
            if (pstore.isDirectory) {
                pstore.listFiles()?.forEach { if (it.isFile) add(it.absolutePath) }
            }
        }
        if (paths.isEmpty()) return
        runCatching {
            MediaScannerConnection.scanFile(context, paths.toTypedArray(), null, null)
        }
    }

    companion object {
        private val SidecarNames = listOf(
            "kernel-dmesg.log", "kernel-info.txt", "iomem.txt",
            "ksu.log",
        )
        private val sequence = AtomicLong()

        fun open(context: Context, entry: String, folder: String): DebugAttackLog? {
            val stamp = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date())
            val suffix = sequence.getAndIncrement()
            val folderPath = "$folder/$stamp"
            val displayName = "ghostlock-$entry-$suffix.log"
            val values = ContentValues().apply {
                put(MediaStore.MediaColumns.DISPLAY_NAME, displayName)
                put(MediaStore.MediaColumns.MIME_TYPE, "text/plain")
                put(MediaStore.MediaColumns.RELATIVE_PATH, folderPath)
            }
            val resolver = context.contentResolver
            val uri = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
                ?: return null
            return try {
                val output = resolver.openOutputStream(uri, "w")
                    ?: error("cannot open debug log output")
                val channel = (output as? FileOutputStream)?.channel
                DebugAttackLog(
                    context.applicationContext,
                    displayName,
                    folderPath,
                    BufferedWriter(OutputStreamWriter(output, StandardCharsets.UTF_8)),
                    channel,
                )
            } catch (_: Exception) {
                resolver.delete(uri, null, null)
                null
            }
        }
    }
}
