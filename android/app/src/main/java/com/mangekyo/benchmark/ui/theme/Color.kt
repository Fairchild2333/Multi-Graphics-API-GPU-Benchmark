package com.mangekyo.benchmark.ui.theme

import androidx.compose.ui.graphics.Color

/**
 * Brand static palette for API &lt; 31 (no Material You wallpaper colors).
 * On Android 12+ [MangekyoTheme] prefers dynamic color schemes instead.
 */
object MangekyoColors {
    val Primary = Color(0xFF3558C7)
    val OnPrimary = Color(0xFFFFFFFF)
    val PrimaryContainer = Color(0xFFDBE1FF)
    val OnPrimaryContainer = Color(0xFF00174B)

    val Secondary = Color(0xFF0F766E)
    val OnSecondary = Color(0xFFFFFFFF)
    val SecondaryContainer = Color(0xFFCCFBF1)
    val OnSecondaryContainer = Color(0xFF042F2E)

    val Tertiary = Color(0xFFB45309)
    val OnTertiary = Color(0xFFFFFFFF)
    val TertiaryContainer = Color(0xFFFDE68A)
    val OnTertiaryContainer = Color(0xFF451A03)

    val Error = Color(0xFFBA1A1A)
    val OnError = Color(0xFFFFFFFF)
    val ErrorContainer = Color(0xFFFFDAD6)
    val OnErrorContainer = Color(0xFF410002)

    val Background = Color(0xFFF8F9FF)
    val OnBackground = Color(0xFF191B23)
    val Surface = Color(0xFFF8F9FF)
    val OnSurface = Color(0xFF191B23)
    val SurfaceVariant = Color(0xFFE2E2EC)
    val OnSurfaceVariant = Color(0xFF45464F)
    val Outline = Color(0xFF757680)
    val OutlineVariant = Color(0xFFC5C6D0)

    // Dark
    val PrimaryDark = Color(0xFFB4C5FF)
    val OnPrimaryDark = Color(0xFF002A78)
    val PrimaryContainerDark = Color(0xFF163FAE)
    val OnPrimaryContainerDark = Color(0xFFDBE1FF)

    val SecondaryDark = Color(0xFF5EEAD4)
    val OnSecondaryDark = Color(0xFF003731)
    val SecondaryContainerDark = Color(0xFF115E59)
    val OnSecondaryContainerDark = Color(0xFFCCFBF1)

    val TertiaryDark = Color(0xFFFCD34D)
    val OnTertiaryDark = Color(0xFF78350F)
    val TertiaryContainerDark = Color(0xFF92400E)
    val OnTertiaryContainerDark = Color(0xFFFEF3C7)

    val BackgroundDark = Color(0xFF11131A)
    val OnBackgroundDark = Color(0xFFE2E2EB)
    val SurfaceDark = Color(0xFF11131A)
    val OnSurfaceDark = Color(0xFFE2E2EB)
    val SurfaceVariantDark = Color(0xFF45464F)
    val OnSurfaceVariantDark = Color(0xFFC5C6D0)
    val OutlineDark = Color(0xFF8F909A)
    val OutlineVariantDark = Color(0xFF45464F)
}
