package com.mangekyo.benchmark.ui.theme

import android.os.Build
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp

private val LightBrandScheme = lightColorScheme(
    primary = MangekyoColors.Primary,
    onPrimary = MangekyoColors.OnPrimary,
    primaryContainer = MangekyoColors.PrimaryContainer,
    onPrimaryContainer = MangekyoColors.OnPrimaryContainer,
    secondary = MangekyoColors.Secondary,
    onSecondary = MangekyoColors.OnSecondary,
    secondaryContainer = MangekyoColors.SecondaryContainer,
    onSecondaryContainer = MangekyoColors.OnSecondaryContainer,
    tertiary = MangekyoColors.Tertiary,
    onTertiary = MangekyoColors.OnTertiary,
    tertiaryContainer = MangekyoColors.TertiaryContainer,
    onTertiaryContainer = MangekyoColors.OnTertiaryContainer,
    error = MangekyoColors.Error,
    onError = MangekyoColors.OnError,
    errorContainer = MangekyoColors.ErrorContainer,
    onErrorContainer = MangekyoColors.OnErrorContainer,
    background = MangekyoColors.Background,
    onBackground = MangekyoColors.OnBackground,
    surface = MangekyoColors.Surface,
    onSurface = MangekyoColors.OnSurface,
    surfaceVariant = MangekyoColors.SurfaceVariant,
    onSurfaceVariant = MangekyoColors.OnSurfaceVariant,
    outline = MangekyoColors.Outline,
    outlineVariant = MangekyoColors.OutlineVariant,
)

private val DarkBrandScheme = darkColorScheme(
    primary = MangekyoColors.PrimaryDark,
    onPrimary = MangekyoColors.OnPrimaryDark,
    primaryContainer = MangekyoColors.PrimaryContainerDark,
    onPrimaryContainer = MangekyoColors.OnPrimaryContainerDark,
    secondary = MangekyoColors.SecondaryDark,
    onSecondary = MangekyoColors.OnSecondaryDark,
    secondaryContainer = MangekyoColors.SecondaryContainerDark,
    onSecondaryContainer = MangekyoColors.OnSecondaryContainerDark,
    tertiary = MangekyoColors.TertiaryDark,
    onTertiary = MangekyoColors.OnTertiaryDark,
    tertiaryContainer = MangekyoColors.TertiaryContainerDark,
    onTertiaryContainer = MangekyoColors.OnTertiaryContainerDark,
    background = MangekyoColors.BackgroundDark,
    onBackground = MangekyoColors.OnBackgroundDark,
    surface = MangekyoColors.SurfaceDark,
    onSurface = MangekyoColors.OnSurfaceDark,
    surfaceVariant = MangekyoColors.SurfaceVariantDark,
    onSurfaceVariant = MangekyoColors.OnSurfaceVariantDark,
    outline = MangekyoColors.OutlineDark,
    outlineVariant = MangekyoColors.OutlineVariantDark,
)

private val MangekyoShapes = Shapes(
    extraSmall = RoundedCornerShape(4.dp),
    small = RoundedCornerShape(8.dp),
    medium = RoundedCornerShape(12.dp),
    large = RoundedCornerShape(16.dp),
    extraLarge = RoundedCornerShape(28.dp),
)

/**
 * Material 3 cascade (HANDOFF):
 * - API 31+ (Android 12+): wallpaper dynamic color (Material You)
 * - Below API 31: brand [LightBrandScheme] / [DarkBrandScheme]
 *
 * Same Compose Material 3 component set on all supported API levels; only color
 * source changes. Predictive back / edge-to-edge are handled by the activity shell.
 */
@Composable
fun MangekyoTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    dynamicColor: Boolean = true,
    content: @Composable () -> Unit,
) {
    val colorScheme = when {
        dynamicColor && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S -> {
            val context = LocalContext.current
            if (darkTheme) dynamicDarkColorScheme(context) else dynamicLightColorScheme(context)
        }
        darkTheme -> DarkBrandScheme
        else -> LightBrandScheme
    }

    MaterialTheme(
        colorScheme = colorScheme,
        typography = MangekyoTypography,
        shapes = MangekyoShapes,
        content = content,
    )
}
