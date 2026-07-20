// Mangekyo Android JNI stub — 仅占位，未接引擎。
// TODO(next-ai)：
//  1. dlopen("libvulkan.so") + vkGetInstanceProcAddr 探测（API>=24 才尝试，Kotlin 侧已门控）；
//  2. ANativeWindow_fromSurface 接管 Surface，交给引擎表面层（替代 GLFW）；
//  3. start/cancel workload、进度回调（JNI 回调或 eventfd → Kotlin）；
//  4. 结果 JSON 走与 Windows 相同的合同代码，metadata 记录真实 ABI/SoC/驱动。
#include <jni.h>
#include <android/native_window_jni.h>
#include <dlfcn.h>

namespace {
ANativeWindow* g_window = nullptr;
}

extern "C" {

JNIEXPORT jstring JNICALL
Java_com_mangekyo_benchmark_core_NativeBridge_nativeEngineVersion(JNIEnv* env, jobject) {
    return env->NewStringUTF("mangekyo-android-scaffold-0.1.0 (engine not wired)");
}

JNIEXPORT jboolean JNICALL
Java_com_mangekyo_benchmark_core_NativeBridge_nativeProbeVulkan(JNIEnv*, jobject) {
    void* lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib) return JNI_FALSE;
    const bool ok = dlsym(lib, "vkGetInstanceProcAddr") != nullptr;
    dlclose(lib);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_mangekyo_benchmark_core_NativeBridge_nativeSetSurface(JNIEnv* env, jobject, jobject surface) {
    if (g_window) {
        ANativeWindow_release(g_window);
        g_window = nullptr;
    }
    if (surface) {
        g_window = ANativeWindow_fromSurface(env, surface);
        // TODO(next-ai): 通知引擎重建 swapchain / EGL surface
    }
    // TODO(next-ai): surface == null 时必须同步等待引擎释放，避免竞态
}

} // extern "C"
