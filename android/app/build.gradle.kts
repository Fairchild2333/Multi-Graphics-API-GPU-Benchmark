plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "com.mangekyo.benchmark"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.mangekyo.benchmark"
        // Compose BOM 2026.06+ requires minSdk 23 (Android 6). HANDOFF allows 23.
        minSdk = 23
        targetSdk = 36
        versionCode = 2
        versionName = "0.2.0-android-engine"

        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86", "x86_64")
        }
        externalNativeBuild {
            cmake {
                // The native code targets the same system as the app. It was
                // built for android-24 to link libvulkan.so, and then did not
                // load on Android 6.0 at all; Vulkan is now loaded at run time
                // (src/main/cpp/vulkan_loader.h).
                arguments += listOf(
                    "-DANDROID_STL=c++_static",
                    "-DANDROID_PLATFORM=android-23",
                )
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
            isMinifyEnabled = false
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    buildFeatures {
        compose = true
    }
}

// The shaders under src/main/assets/shaders are written by the native build:
// CMake compiles the SPIR-V and copies the OpenGL sources there. They have to
// be in place before the assets are packaged, or a first build ships without
// them (found 2026-10-07: the OpenGL ES test could not open compute_gl.comp).
tasks.matching { it.name == "mergeDebugAssets" || it.name == "mergeReleaseAssets" }.configureEach {
    val nativeBuild = if (name == "mergeDebugAssets") "buildCMakeDebug" else "buildCMakeRelWithDebInfo"
    dependsOn(tasks.matching { it.name.startsWith(nativeBuild) })
}

kotlin {
    compilerOptions {
        jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.core.splashscreen)
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.navigation.compose)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.lifecycle.runtime.compose)
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.material3)
    implementation(libs.compose.material.icons)
    implementation(libs.compose.ui.tooling.preview)
    debugImplementation(libs.compose.ui.tooling)
}
