package com.example.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable

private val TacticalCinemaColorScheme = darkColorScheme(
    primary = CineAmber,
    onPrimary = OledBlack,
    primaryContainer = CineAmberDim,
    onPrimaryContainer = CineAmber,
    secondary = TacticalCyan,
    onSecondary = OledBlack,
    secondaryContainer = TacticalCyanDim,
    onSecondaryContainer = TacticalCyan,
    tertiary = ScopeEmerald,
    onTertiary = OledBlack,
    error = RecCrimson,
    onError = HudTextPrimary,
    background = OledBlack,
    onBackground = HudTextPrimary,
    surface = TacticalObsidian,
    onSurface = HudTextPrimary,
    surfaceVariant = ElevatedSlate,
    onSurfaceVariant = HudTextSecondary,
    outline = TacticalBorder
)

@Composable
fun MyApplicationTheme(
    content: @Composable () -> Unit
) {
    MaterialTheme(
        colorScheme = TacticalCinemaColorScheme,
        typography = Typography,
        content = content
    )
}
