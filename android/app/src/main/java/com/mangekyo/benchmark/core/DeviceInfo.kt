package com.mangekyo.benchmark.core

import android.os.Build

/** Device / ABI helpers for result metadata (must record real ABI, never mix lanes). */
object DeviceInfo {
    @Suppress("DEPRECATION")
    fun primaryAbi(): String {
        val abis = Build.SUPPORTED_ABIS
        return if (abis.isNotEmpty()) abis[0] else Build.CPU_ABI
    }

    fun supportedAbis(): List<String> = Build.SUPPORTED_ABIS.toList()

    fun socLabel(): String {
        val hardware = Build.HARDWARE.orEmpty()
        val board = Build.BOARD.orEmpty()
        val model = Build.MODEL.orEmpty()
        return listOf(model, board, hardware)
            .filter { it.isNotBlank() }
            .distinct()
            .joinToString(" / ")
            .ifBlank { "unknown" }
    }
}
