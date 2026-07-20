package com.mangekyo.benchmark.core

import android.os.Build

/**
 * 能力门控（HANDOFF 目标 C 第 3 条）。
 * 规则：能力不齐显式 unsupported，不静默 fallback。
 */
object CapabilityGate {
    /**
     * Vulkan 运行时门控：系统 loader 自 API 24 (Android 7.0) 起存在；
     * 之下或 dlopen 失败 → 只提供 GL ES 路径（同 Windows delay-load 思路）。
     */
    fun vulkanAvailable(): Boolean =
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && NativeBridge.probeVulkan()

    // TODO(next-ai)：
    //  - GL ES 3.1/3.2 版本探测（EGL context 试建）；
    //  - EXT_disjoint_timer_query 探测——无可靠 GPU timestamp 不产生正式 score；
    //  - 探测结果进能力矩阵并写入结果 metadata。
}
