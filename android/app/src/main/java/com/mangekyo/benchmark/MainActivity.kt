package com.mangekyo.benchmark

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import com.mangekyo.benchmark.core.NativeBridge
import com.mangekyo.benchmark.ui.AppRoot
import com.mangekyo.benchmark.ui.theme.MangekyoTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        installSplashScreen()
        super.onCreate(savedInstanceState)
        NativeBridge.init(this)
        enableEdgeToEdge()
        setContent {
            MangekyoTheme {
                AppRoot()
            }
        }
    }
}
