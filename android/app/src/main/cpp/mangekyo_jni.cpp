// Mangekyo Android JNI — Surface + Vulkan probe + gpu_engine host thread.
#include "android_engine_host.h"
#include "gpu_engine.h"

#include <android/log.h>
#include <android/native_window_jni.h>
#include <dlfcn.h>
#include <jni.h>

#include <atomic>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <mutex>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

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

// Everything the engine prints to std::cout / std::cerr lands here, split into
// lines that the UI collects with nativePollOutput(). The engine prints from
// one thread at a time; the lock only guards against the collecting thread.
class LineQueueBuffer : public std::streambuf {
public:
    std::vector<std::string> Drain() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> lines;
        lines.swap(lines_);
        return lines;
    }

protected:
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof())) return traits_type::not_eof(ch);
        std::lock_guard<std::mutex> lock(mutex_);
        Append(traits_type::to_char_type(ch));
        return ch;
    }

    std::streamsize xsputn(const char* text, std::streamsize count) override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (std::streamsize i = 0; i < count; ++i) Append(text[i]);
        return count;
    }

private:
    // Nobody may be collecting; keep the newest lines only.
    static constexpr std::size_t kMaxPendingLines = 20000;

    void Append(char c) {
        if (c == '\n') {
            if (lines_.size() >= kMaxPendingLines) lines_.erase(lines_.begin(), lines_.begin() + 1000);
            // Also to logcat, so a run can be followed over adb:
            // adb logcat -s MangekyoEngine
            if (!current_.empty())
                __android_log_write(ANDROID_LOG_INFO, "MangekyoEngine", current_.c_str());
            lines_.push_back(current_);
            current_.clear();
        } else if (c != '\r') {
            current_.push_back(c);
        }
    }

    std::mutex mutex_;
    std::vector<std::string> lines_;
    std::string current_;
};

LineQueueBuffer g_output;

// CPU-only tests run through the engine's command-line entry point on their
// own thread; they need no surface.
std::atomic<bool> g_cliAlive{false};
std::atomic<int> g_cliExitCode{0};
std::thread g_cliThread;
}  // namespace

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM*, void*) {
    std::cout.rdbuf(&g_output);
    std::cerr.rdbuf(&g_output);
    return JNI_VERSION_1_6;
}

// Lines printed by the engine since the last call.
JNIEXPORT jobjectArray JNICALL
Java_com_mangekyo_benchmark_core_NativeBridge_nativePollOutput(JNIEnv* env, jobject) {
    const std::vector<std::string> lines = g_output.Drain();
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray result =
        env->NewObjectArray(static_cast<jsize>(lines.size()), stringClass, nullptr);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        jstring text = env->NewStringUTF(lines[i].c_str());
        env->SetObjectArrayElement(result, static_cast<jsize>(i), text);
        env->DeleteLocalRef(text);
    }
    return result;
}

// Runs `gpu_benchmark <args>` in-process on a worker thread. Only the CPU
// paths are meant to be started this way: they return before any GPU work.
JNIEXPORT jboolean JNICALL
Java_com_mangekyo_benchmark_core_NativeBridge_nativeStartCli(JNIEnv* env, jobject,
                                                             jobjectArray args) {
    std::vector<std::string> arguments{"gpu_benchmark"};
    const jsize count = args ? env->GetArrayLength(args) : 0;
    for (jsize i = 0; i < count; ++i) {
        auto item = static_cast<jstring>(env->GetObjectArrayElement(args, i));
        if (!item) continue;
        const char* text = env->GetStringUTFChars(item, nullptr);
        if (text) {
            arguments.emplace_back(text);
            env->ReleaseStringUTFChars(item, text);
        }
        env->DeleteLocalRef(item);
    }

    std::lock_guard<std::mutex> lock(g_workerMutex);
    if (g_cliAlive.load(std::memory_order_acquire) ||
        g_workerAlive.load(std::memory_order_acquire) || gpu_bench::android_host::IsRunning()) {
        g_lastError = "already running";
        return JNI_FALSE;
    }
    if (g_cliThread.joinable()) g_cliThread.join();
    g_lastError.clear();
    g_cliAlive.store(true, std::memory_order_release);
    g_cliThread = std::thread([arguments]() mutable {
        // path_service reads GPU_BENCH_DATA_DIR once; set it before any result is saved.
        if (!g_dataDir.empty()) setenv("GPU_BENCH_DATA_DIR", g_dataDir.c_str(), 1);
        std::vector<char*> argv;
        for (auto& argument : arguments) argv.push_back(argument.data());
        int exitCode = 1;
        try {
            exitCode = gpu_bench::cliMain(static_cast<int>(argv.size()), argv.data());
        } catch (const std::exception& ex) {
            std::cout << "[error] " << ex.what() << std::endl;
        } catch (...) {
            std::cout << "[error] unknown native exception" << std::endl;
        }
        std::cout.flush();
        g_cliExitCode.store(exitCode, std::memory_order_release);
        g_cliAlive.store(false, std::memory_order_release);
    });
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_mangekyo_benchmark_core_NativeBridge_nativeIsCliRunning(JNIEnv*, jobject) {
    return g_cliAlive.load(std::memory_order_acquire) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_mangekyo_benchmark_core_NativeBridge_nativeCliExitCode(JNIEnv*, jobject) {
    return g_cliExitCode.load(std::memory_order_acquire);
}

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
            g_cliAlive.load(std::memory_order_acquire) ||
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
