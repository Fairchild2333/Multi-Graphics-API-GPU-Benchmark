package com.mangekyo.benchmark

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import com.mangekyo.benchmark.ui.AppRoot
import com.mangekyo.benchmark.ui.theme.MangekyoTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            MangekyoTheme {
                AppRoot()
            }
        }
    }
}
