package com.mangekyo.benchmark.core

import java.util.Locale

/**
 * Interface text in English and Simplified Chinese. The language follows the
 * system, like the "Auto" setting of the desktop front ends; every other
 * language gets English.
 */
fun tr(en: String, zh: String): String =
    if (Locale.getDefault().language == "zh") zh else en

/** Display name of a test, the same wording as the desktop front ends. */
fun workloadTitle(id: String): String = when (id) {
    "stream" -> tr("Particle — memory throughput", "粒子 —— 内存吞吐")
    "gpu_burn" -> tr("GPU Burn", "GPU Burn")
    "cinematic_liquid" -> tr("Fluid — interactive pool", "流体 —— 互动水池")
    "membw" -> tr("Memory bandwidth — VRAM / cache", "显存带宽 —— 显存 / 缓存")
    "cpu_single_core" -> tr("CPU — single-core", "CPU —— 单核")
    "cpu_multi_core" -> tr("CPU — all-core", "CPU —— 全核")
    "cpu_membw" -> tr("CPU — cache & memory bandwidth", "CPU —— 缓存与内存带宽")
    else -> id
}

/**
 * The engine prints its summary in English. This replaces the label at the
 * start of a score line, such as "VRAM rate:", and leaves the figures alone.
 * The wording matches the Windows app.
 */
fun localizeScoreLine(line: String): String {
    // "VRAM rate:" comes before "RAM rate:", which it contains.
    val labels = listOf(
        "VRAM rate:" to "显存速率:",
        "RAM rate:" to "内存速率:",
        "Memory rate:" to "显存速率:",
        "Avg FPS:" to "平均帧率:",
        "Compute rate:" to "计算速率:",
        "Burn rate:" to "Burn 速率:",
        "Stress rate:" to "压力速率:",
        "Fill rate:" to "填充速率:",
        "Render rate:" to "渲染速率:",
        "Vol rate:" to "体积速率:",
        "Fluid rate:" to "流体速率:",
        "Liquid rate:" to "流体速率:",
    )
    for ((en, zh) in labels) {
        if (line.startsWith(en)) {
            val rest = line.substring(en.length).trim()
                .replace("ms compute", "ms 计算").replace("ms render", "ms 渲染")
            return tr(line, "$zh $rest")
        }
    }
    return line
}

/**
 * The engine names the paging target and the reason for skipping the storage
 * test in English. These are all the phrases it uses.
 */
fun localizeEngineText(text: String): String {
    if (text.startsWith("page file on ")) {
        return tr(text, text.removePrefix("page file on ") + " 盘上的页面文件")
    }
    if (text.startsWith("swap on ")) {
        return tr(text, text.removePrefix("swap on ") + " 上的交换分区")
    }
    val phrases = mapOf(
        "no page file" to
            "没有页面文件",
        "zram (memory is compressed in RAM; no drive is involved)" to
            "zram（内存在 RAM 里压缩，不经过硬盘）",
        "no swap" to
            "没有交换分区",
        "none (iOS compresses memory and does not page to storage)" to
            "无（iOS 只压缩内存，不向存储换页）",
        "swap files in /private/var/vm" to
            "/private/var/vm 里的交换文件",
        "unknown" to
            "未知",
        "no temporary directory" to
            "没有可用的临时目录",
        "the directory is on tmpfs, which is memory and not a drive" to
            "目录位于 tmpfs，它是内存而不是硬盘",
        "the directory does not exist" to
            "目录不存在",
        "not enough free space for the test file" to
            "剩余空间不足以写测试文件",
        "allocation failed" to
            "内存分配失败",
        "the test file could not be created" to
            "无法创建测试文件",
        "a read or write on the test file failed" to
            "读写测试文件时出错",
        "switched off" to
            "已关闭",
    )
    return phrases[text]?.let { tr(text, it) } ?: text
}
