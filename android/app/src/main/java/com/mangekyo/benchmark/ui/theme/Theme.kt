package com.mangekyo.benchmark.ui.theme

import android.os.Build
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalContext

/**
 * UI 基调（HANDOFF 目标 C 第 3 条）：单套 Material 3，能力递减。
 * - Android 12+ (API 31)：动态取色 Material You。
 * - 以下：回退 Mangekyo 品牌静态配色（Color.kt）。
 */
@Composable
fun MangekyoTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit,
) {
    val colorScheme = when {
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.S -> {
            val context = LocalContext.current
            if (darkTheme) dynamicDarkColorScheme(context) else dynamicLightColorScheme(context)
        }
        darkTheme -> darkColorScheme(
            primary = MangekyoColors.PrimaryDark,
            secondary = MangekyoColors.SecondaryDark,
            tertiary = MangekyoColors.TertiaryDark,
        )
        else -> lightColorScheme(
            primary = MangekyoColors.Primary,
            secondary = MangekyoColors.Secondary,
            tertiary = MangekyoColors.Tertiary,
        )
    }
    MaterialTheme(colorScheme = colorScheme, content = content)
}
