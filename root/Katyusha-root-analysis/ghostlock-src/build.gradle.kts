import java.util.Properties

plugins {
    id("com.android.application") version "9.5.1" apply false
    id("org.jetbrains.kotlin.plugin.compose") version "2.4.20" apply false
    id("org.jetbrains.kotlin.jvm") version "2.4.20" apply false
}

private fun localProperties(): Properties = Properties().also { properties ->
    val propertiesFile = rootProject.file("local.properties")
    if (propertiesFile.isFile) {
        propertiesFile.inputStream().use(properties::load)
    }
}

private fun ondkHome(): String? =
    System.getenv("ONDK_HOME")?.takeIf(String::isNotBlank)
        ?: localProperties().getProperty("ondk.dir")?.takeIf(String::isNotBlank)

private fun useOndk(): Boolean = !ondkHome().isNullOrBlank()

private fun resolveNdkDir(): String {
    val ondk = ondkHome()
    if (ondk != null) return ondk

    val properties = localProperties()
    val ndkEnvironment = System.getenv("ANDROID_NDK_HOME")
        ?: System.getenv("ANDROID_NDK_ROOT")
    if (!ndkEnvironment.isNullOrBlank()) return ndkEnvironment

    properties.getProperty("ndk.dir")?.takeIf(String::isNotBlank)?.let { return it }

    val sdkDir = properties.getProperty("sdk.dir") ?: System.getenv("ANDROID_HOME")
    if (!sdkDir.isNullOrBlank()) {
        val ndkRoot = File(sdkDir, "ndk")
        val versions = ndkRoot.listFiles()
            ?.filter(File::isDirectory)
            ?.map(File::getName)
            ?.sorted()
            .orEmpty()
        if (versions.isNotEmpty()) return File(ndkRoot, versions.last()).absolutePath
    }

    throw GradleException("NDK not found; set ANDROID_NDK_HOME or ndk.dir in local.properties")
}

private data class NdkTools(val clang: String, val ar: String)

private fun resolveCargoExecutable(): String {
    val cargoOnPath = System.getenv("PATH")
        .orEmpty()
        .split(File.pathSeparator)
        .asSequence()
        .map { File(it, "cargo") }
        .firstOrNull { it.isFile && it.canExecute() }
    if (cargoOnPath != null) return cargoOnPath.absolutePath

    val cargoInRustupHome = File(System.getProperty("user.home"), ".cargo/bin/cargo")
    return if (cargoInRustupHome.isFile && cargoInRustupHome.canExecute()) {
        cargoInRustupHome.absolutePath
    } else {
        "cargo"
    }
}

private fun extractNdkTools(): NdkTools {
    val ndk = resolveNdkDir()
    val osName = System.getProperty("os.name").lowercase()
    val isWindows = osName.contains("windows")
    val prebuilt = when {
        isWindows -> "windows-x86_64"
        osName.contains("mac") -> "darwin-x86_64"
        else -> "linux-x86_64"
    }
    val binDir = File(ndk, "toolchains/llvm/prebuilt/$prebuilt/bin")
    return NdkTools(
        clang = File(
            binDir,
            if (isWindows) "aarch64-linux-android34-clang.cmd" else "aarch64-linux-android34-clang",
        ).absolutePath,
        ar = File(binDir, if (isWindows) "llvm-ar.exe" else "llvm-ar").absolutePath,
    )
}

// Every module's output lives under the root build/ directory (native,
// host-test, extract, kernel-profiles, app). Delete the whole tree here so a
// single root `clean` resets all of them.
tasks.register<Delete>("clean") {
    description = "Delete the root build/ directory (all module outputs)."
    delete(layout.buildDirectory)
}

tasks.register<Exec>("buildGhostlockNative") {
    description = "buildGhostlockNative"
    /* 本机无 GNU make：build_native.sh 是 src/Makefile 的逐条 clang++ 等价实现
     * （同样的 flag 与源列表，产物同为 build/native/ghostlock）。
     * bash 必须用 Git 的绝对路径：PATH 里的 System32\bash.exe 是 WSL，
     * 跑 Windows 路径脚本会 127。 */
    val gitBash = System.getenv("GIT_BASH")?.takeIf(String::isNotBlank)
        ?: listOf(
            "C:/Program Files/Git/bin/bash.exe",
            "C:/Program Files (x86)/Git/bin/bash.exe",
        ).firstOrNull { File(it).isFile }
        ?: "bash"
    workingDir(rootDir)
    commandLine(gitBash, "build_native.sh")
    val ndk = resolveNdkDir()
    environment("ANDROID_NDK_HOME", ndk)
    environment("NDK_ROOT", ndk)
    inputs.files(
        fileTree("src") { include("**/*.c", "**/*.h", "**/*.cpp", "**/*.hpp") },
        file("src/Makefile"),
        file("build_native.sh"),
    )
    outputs.file(file("build/native/ghostlock"))
}

tasks.register<Copy>("prepareGhostlockJniLibs") {
    description = "prepareGhostlockJniLibs"
    dependsOn("buildGhostlockNative")
    from("build/native/ghostlock")
    into("app/src/main/jniLibs/arm64-v8a")
    rename { "libghostlock.so" }
    /* Strip only the packaged copy: static libc++ carries its DWARF into the
     * binary, while the top-level ghostlock keeps its symbols for the
     * disassembly comparisons. Paths are captured as plain strings so the
     * configuration cache can serialize this task. */
    val stripPath = File(extractNdkTools().clang)
        .resolveSibling("llvm-strip").absolutePath
    val packagedPath = File(rootDir, "app/src/main/jniLibs/arm64-v8a/libghostlock.so").absolutePath
    doLast {
        val code = ProcessBuilder(stripPath, "--strip-all", packagedPath)
            .inheritIO()
            .start()
            .waitFor()
        check(code == 0) { "llvm-strip failed with $code" }
    }
}

/* 本机无 Rust/cargo 工具链：extract_rs 的源码自 v590（2026-11-05 上游发布）起
 * 未做任何改动，因此直接复用官方发布 APK 中的 libextract.so——它已预置在
 * tools/extract_rs/prebuilt/ghostlock-extract，这里的产物路径与原 Exec 任务一致。 */
tasks.register<Copy>("buildGhostlockExtract") {
    description = "buildGhostlockExtract"
    from(file("tools/extract_rs/prebuilt/ghostlock-extract"))
    into(file("build/extract/aarch64-linux-android/release"))
    rename { "ghostlock-extract" }
}

tasks.register<Copy>("prepareGhostlockExtractJniLibs") {
    description = "prepareGhostlockExtractJniLibs"
    dependsOn("buildGhostlockExtract")
    from("build/extract/aarch64-linux-android/release/ghostlock-extract")
    into("app/src/main/jniLibs/arm64-v8a")
    rename { "libextract.so" }
}
