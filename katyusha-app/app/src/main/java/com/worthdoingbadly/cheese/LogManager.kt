package com.worthdoingbadly.cheese

import android.content.ContentValues
import android.content.Context
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import java.io.BufferedWriter
import java.io.File
import java.io.FileWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object LogManager {
    const val AUTO_LOG_FILENAME = "katyusha_latest.log"
    private var currentWriter: BufferedWriter? = null

    /**
     * Start a new auto-save session.
     * Overwrites any previous auto-save log to keep storage clean and single-instance.
     */
    fun startAutoLogSession(context: Context) {
        try {
            closeAutoLog()
            // Store in app's external files directory so user/ADB can access it easily without root
            val dir = context.getExternalFilesDir(null) ?: context.filesDir
            val file = File(dir, AUTO_LOG_FILENAME)
            // Truncate / overwrite previous auto-save log (append = false)
            currentWriter = BufferedWriter(FileWriter(file, false))
            val timeStr = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault()).format(Date())
            writeLine("[SESSION_START] Cheese Katyusha Exploit Session started at $timeStr")
            writeLine("[DEVICE] ${Build.MANUFACTURER} ${Build.MODEL} | Android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
            writeLine("[KERNEL] ${System.getProperty("os.version")}")
            writeLine("------------------------------------------------------------")
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    /**
     * Synchronously write a log line and immediately FLUSH to disk.
     * If the kernel panics, this guarantees the output up to the panic is persisted!
     */
    @Synchronized
    fun writeLine(line: String) {
        try {
            currentWriter?.let { writer ->
                writer.write(line)
                writer.newLine()
                writer.flush()
            }
        } catch (_: Exception) {}
    }

    /**
     * Mark the session as completed cleanly.
     */
    fun endAutoLogSession(returnCode: Int, success: Boolean) {
        try {
            val timeStr = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault()).format(Date())
            writeLine("------------------------------------------------------------")
            writeLine("[SESSION_END] Completed at $timeStr | exitCode=$returnCode | success=$success")
            closeAutoLog()
        } catch (_: Exception) {}
    }

    fun closeAutoLog() {
        try {
            currentWriter?.flush()
            currentWriter?.close()
        } catch (_: Exception) {}
        currentWriter = null
    }

    /**
     * Check if there is an uncompleted (likely panicked or crashed) previous log.
     */
    fun getPreviousPanicLog(context: Context): List<String>? {
        val dir = context.getExternalFilesDir(null) ?: context.filesDir
        val file = File(dir, AUTO_LOG_FILENAME)
        if (!file.exists() || file.length() == 0L) return null

        try {
            val lines = file.readLines()
            val text = lines.joinToString("\n")
            // If it started but didn't have [SESSION_END], it panicked or crashed!
            if (text.contains("[SESSION_START]") && !text.contains("[SESSION_END]")) {
                return lines
            }
        } catch (_: Exception) {}
        return null
    }

    /**
     * Manually save the current in-memory console logs to public Downloads folder.
     */
    fun saveLogsToDownloads(context: Context, logs: List<String>): Pair<Boolean, String> {
        if (logs.isEmpty()) {
            return Pair(false, tr("No logs to save", "控制台没有日志可保存"))
        }

        val timestamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.getDefault()).format(Date())
        val filename = "katyusha_log_$timestamp.txt"
        val content = logs.joinToString("\n")

        return try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                val values = ContentValues().apply {
                    put(MediaStore.MediaColumns.DISPLAY_NAME, filename)
                    put(MediaStore.MediaColumns.MIME_TYPE, "text/plain")
                    put(MediaStore.MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS)
                }
                val uri = context.contentResolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
                if (uri != null) {
                    context.contentResolver.openOutputStream(uri)?.use { os ->
                        os.write(content.toByteArray(Charsets.UTF_8))
                    }
                    Pair(true, "Download/$filename")
                } else {
                    saveToAppDir(context, filename, content)
                }
            } else {
                val downloadDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
                downloadDir.mkdirs()
                val file = File(downloadDir, filename)
                file.writeText(content, Charsets.UTF_8)
                Pair(true, file.absolutePath)
            }
        } catch (e: Exception) {
            saveToAppDir(context, filename, content)
        }
    }

    private fun saveToAppDir(context: Context, filename: String, content: String): Pair<Boolean, String> {
        return try {
            val dir = context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS) ?: context.filesDir
            val file = File(dir, filename)
            file.writeText(content, Charsets.UTF_8)
            Pair(true, file.name)
        } catch (e: Exception) {
            Pair(false, e.message ?: "Failed")
        }
    }
}
