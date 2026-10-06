package com.mangekyo.benchmark

import android.content.Intent
import android.content.pm.ApplicationInfo
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import com.mangekyo.benchmark.core.AutoRun
import com.mangekyo.benchmark.core.NativeBridge
import com.mangekyo.benchmark.ui.AppRoot
import com.mangekyo.benchmark.ui.theme.MangekyoTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        installSplashScreen()
        super.onCreate(savedInstanceState)
        NativeBridge.init(this)
        // A test named on the adb command line; only a fresh start counts.
        if (savedInstanceState == null) AutoRun.accept(intent, isDebuggable())
        enableEdgeToEdge()
        setContent {
            MangekyoTheme {
                AppRoot()
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        AutoRun.accept(intent, isDebuggable())
    }

    private fun isDebuggable(): Boolean =
        (applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) != 0
}
