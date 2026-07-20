plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "com.mangekyo.benchmark"
    compileSdk = 36 // 占位：开工时改为当时最新

    defaultConfig {
        applicationId = "com.mangekyo.benchmark"
        // 合同（HANDOFF 目标 C 第 3 条）：minSdk 21；若所用 Compose 版本要求 23，提到 23（K1 = API 24，零损失）。
        minSdk = 21
        targetSdk = 36 // 占位：跟随最新
        versionCode = 1
        versionName = "0.1.0-android-scaffold"

        // ABI 合同：四 ABI 全原生编译；结果 metadata 必须记录真实 ABI。
        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += "-DANDROID_STL=c++_static"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false // TODO(release): 发布前评估 R8 与 native 符号策略
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
    buildFeatures { compose = true }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.navigation.compose)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.material3)
    implementation(libs.compose.material.icons)
    implementation(libs.compose.ui.tooling.preview)
    debugImplementation(libs.compose.ui.tooling)
}
