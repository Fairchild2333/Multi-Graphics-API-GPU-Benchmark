// Mangekyo Android JNI — Surface + Vulkan probe + gpu_engine host thread.
#include "android_engine_host.h"
#include "gpu_engine.h"

#include <android/log.h>
#include <android/native_window_jni.h>
#include <dlfcn.h>
#include <jni.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#define LOG_TAG "MangekyoJni"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {
ANativeWindow* g_window = nullptr;
int g_width = 0;
int g_height = 0;
std::mutex g_windowMutex;

std::string g_shaderDir;
std::string g_dataDir;

std::atomic<bool> g_workerAlive{false};
std::mutex g_workerMutex;
std::thread g_worker;
std::string g_lastError;

void JoinWorkerUnlocked() {
    if (g_worker.joinable())
        g_worker.join();
    g_workerAlive.store(false, std::memory_order_release);
}
}  // namespace

extern "C" {

JNIEXPORT jstring JNICALL
Java_com_mangekyo_benchmark_core_NativeBridge_nativeEngineVersion(JNIEnv* env, jobject) {
    return env->NewStringUTF("mangekyo-android-0.2.0-gpu_engine");
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
Java_com_mangekyo_benchmark_core_NativeBridge_nativeInitPaths(JNIEnv* env, jobject,
                                                              jstring shaderDir,
                                                              jstring dataDir) {
    const char* sd = env->GetStringUTFChars(shaderDir, nullptr);
    const char* dd = env->GetStringUTFChars(dataDir, nullptr);
    g_shaderDir = sd ? sd : "";
    g_dataDir = dd ? dd : "";
    if (sd) env->ReleaseStringUTFChars(shaderDir, sd);
    if (dd) env->ReleaseStringUTFChars(dataDir, dd);
    ALOGI("paths shader=%s data=%s", g_shaderDir.c_str(), g_dataDir.c_str());
}

JNIEXPORT void JNICALL
Java_com_mangekyo_benchmark_core_NativeBridge_nativeSetSurface(JNIEnv* env, jobject,
                                                               jobject surface) {
    std::lock_guard<std::mutex> lock(g_windowMutex);
    // Do not release a window the engine thread still owns.
    if (gpu_bench::android_host::IsRunning()) {
        ALOGI("setSurface ignored while workload running");
        return;
    }
    if (g_window) {
        ANativeWindow_release(g_window);
        g_window = nullptr;
        g_width = 0;
        g_height = 0;
    }
    if (surface) {
        g_window = ANativeWindow_fromSurface(env, surface);
        if (g_window) {
            g_width = ANativeWindow_getWidth(g_window);
            g_height = ANativeWindow_getHeight(g_window);
            ALOGI("surface acquired %dx%d", g_width, g_height);
        }
    } else {
        ALOGI("surface released");
    }
}

JNIEXPORT void JNICALL
Java_com_mangekyo_benchmark_core_NativeBridge_nativeResizeSurface(JNIEnv*, jobject,
                                                                  jint width, jint height) {
    g_width = width;
    g_height = height;
    ALOGI("surface resize %dx%d", width, height);
}

JNIEXPORT jboolean JNICALL
Java_com_mangekyo_benchmark_core_NativeBridge_nativeIsRunning(JNIEnv*, jobject) {
    return (g_workerAlive.load(std::memory_order_acquire) ||
            gpu_bench::android_host::IsRunning())
               ? JNI_TRUE
               : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_mangekyo_benchmark_core_NativeBridge_nativeLastError(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lock(g_workerMutex);
    return env->NewStringUTF(g_lastError.c_str());
}

JNIEXPORT void JNICALL
Java_com_mangekyo_benchmark_core_NativeBridge_nativeStopWorkload(JNIEnv*, jobject) {
    gpu_bench::RequestStop();
    ALOGI("stop requested");
}

JNIEXPORT jboolean JNICALL
Java_com_mangekyo_benchmark_core_NativeBridge_nativeStartWorkload(JNIEnv* env, jobject,
                                                                  jstring workloadId,
                                                                  jdouble seconds) {
    if (g_shaderDir.empty()) {
        std::lock_guard<std::mutex> lock(g_workerMutex);
        g_lastError = "nativeInitPaths not called";
        return JNI_FALSE;
    }

    ANativeWindow* window = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_windowMutex);
        window = g_window;
        if (window)
            ANativeWindow_acquire(window);
    }
    if (!window) {
        std::lock_guard<std::mutex> lock(g_workerMutex);
        g_lastError = "no ANativeWindow (wait for SurfaceView)";
        return JNI_FALSE;
    }

    const char* wid = env->GetStringUTFChars(workloadId, nullptr);
    std::string workload = wid ? wid : "stream";
    if (wid) env->ReleaseStringUTFChars(workloadId, wid);

    {
        std::lock_guard<std::mutex> lock(g_workerMutex);
        if (g_workerAlive.load(std::memory_order_acquire) ||
            gpu_bench::android_host::IsRunning()) {
            ANativeWindow_release(window);
            g_lastError = "already running";
            return JNI_FALSE;
        }
        JoinWorkerUnlocked();
        g_lastError.clear();
        g_workerAlive.store(true, std::memory_order_release);

        g_worker = std::thread([window, workload, seconds]() {
            gpu_bench::android_host::RunRequest req;
            req.window = window;
            req.shaderDir = g_shaderDir;
            req.dataDir = g_dataDir;
            req.workloadId = workload;
            req.maxRunTimeSec = seconds > 0.0 ? seconds : 3.0;

            std::string err;
            const int rc = gpu_bench::android_host::RunWorkload(req, err);
            ANativeWindow_release(window);
            {
                std::lock_guard<std::mutex> lock(g_workerMutex);
                if (rc != 0)
                    g_lastError = err.empty() ? ("run failed rc=" + std::to_string(rc)) : err;
                else
                    g_lastError.clear();
            }
            if (rc != 0)
                ALOGE("workload failed: %s", err.c_str());
            else
                ALOGI("workload finished ok");
            g_workerAlive.store(false, std::memory_order_release);
        });
    }
    return JNI_TRUE;
}

}  // extern "C"
