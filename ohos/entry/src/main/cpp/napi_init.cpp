// NAPI bridge between the ArkTS app and gpu_engine on HarmonyOS; the
// counterpart of android/app/src/main/cpp/mangekyo_jni.cpp.
//
// The library is loaded twice: by the XComponent of the GPU page, which hands
// over the native window, and by `import bridge from 'libmangekyo.so'`, which
// gets the functions below. Both go through Init().

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <hilog/log.h>
#include <napi/native_api.h>
#include <native_window/external_window.h>

#include "gpu_engine.h"
#include "ohos_engine_host.h"

#include <atomic>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <mutex>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0000
#define LOG_TAG "Mangekyo"

namespace {

OHNativeWindow* g_window = nullptr;
std::mutex g_windowMutex;

std::string g_shaderDir;
std::string g_dataDir;

std::atomic<bool> g_workerAlive{false};
std::mutex g_workerMutex;
std::thread g_worker;
std::string g_lastError;

// CPU-only tests run through the engine's command-line entry point on their
// own thread; they need no surface.
std::atomic<bool> g_cliAlive{false};
std::atomic<int> g_cliExitCode{0};
std::thread g_cliThread;

// Everything the engine prints to std::cout / std::cerr lands here, split into
// lines that the UI collects with pollOutput(). The engine prints from one
// thread at a time; the lock only guards against the collecting thread.
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
            // Also to the system log, so a run can be followed from a PC:
            // hdc hilog | grep MangekyoEngine
            if (!current_.empty())
                OH_LOG_Print(LOG_APP, LOG_INFO, LOG_DOMAIN, "MangekyoEngine", "%{public}s", current_.c_str());
            if (lines_.size() >= kMaxPendingLines) lines_.erase(lines_.begin(), lines_.begin() + 1000);
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

// Redirects the engine output when the library is loaded.
struct OutputRedirect {
    OutputRedirect() {
        std::cout.rdbuf(&g_output);
        std::cerr.rdbuf(&g_output);
    }
} g_outputRedirect;

void JoinWorkerUnlocked() {
    if (g_worker.joinable()) g_worker.join();
    g_workerAlive.store(false, std::memory_order_release);
}

bool AnythingRunning() {
    return g_workerAlive.load(std::memory_order_acquire) || g_cliAlive.load(std::memory_order_acquire) ||
           gpu_bench::ohos_host::IsRunning();
}

// ---- Native window of the XComponent ------------------------------------

void OnSurfaceCreated(OH_NativeXComponent*, void* window) {
    std::lock_guard<std::mutex> lock(g_windowMutex);
    g_window = static_cast<OHNativeWindow*>(window);
    OH_LOG_INFO(LOG_APP, "surface created");
}

void OnSurfaceChanged(OH_NativeXComponent*, void*) {
    OH_LOG_INFO(LOG_APP, "surface changed");
}

void OnSurfaceDestroyed(OH_NativeXComponent*, void*) {
    // The engine draws into this window; stop it and wait before the window goes away.
    if (g_workerAlive.load(std::memory_order_acquire)) gpu_bench::RequestStop();
    // Join without holding the lock: the worker takes it to report its result.
    std::thread finished;
    {
        std::lock_guard<std::mutex> lock(g_workerMutex);
        finished = std::move(g_worker);
    }
    if (finished.joinable()) finished.join();
    std::lock_guard<std::mutex> lock(g_windowMutex);
    g_window = nullptr;
    OH_LOG_INFO(LOG_APP, "surface destroyed");
}

// The XComponent keeps the pointer, so the table must outlive this call.
OH_NativeXComponent_Callback g_surfaceCallbacks{};

void RegisterSurfaceCallbacks(napi_env env, napi_value exports) {
    napi_value wrapped = nullptr;
    if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &wrapped) != napi_ok || !wrapped) return;
    OH_NativeXComponent* component = nullptr;
    if (napi_unwrap(env, wrapped, reinterpret_cast<void**>(&component)) != napi_ok || !component) return;
    g_surfaceCallbacks.OnSurfaceCreated = OnSurfaceCreated;
    g_surfaceCallbacks.OnSurfaceChanged = OnSurfaceChanged;
    g_surfaceCallbacks.OnSurfaceDestroyed = OnSurfaceDestroyed;
    g_surfaceCallbacks.DispatchTouchEvent = nullptr;
    OH_NativeXComponent_RegisterCallback(component, &g_surfaceCallbacks);
    OH_LOG_INFO(LOG_APP, "XComponent callbacks registered");
}

// ---- Argument and result helpers ----------------------------------------

std::vector<napi_value> Arguments(napi_env env, napi_callback_info info, std::size_t wanted) {
    std::vector<napi_value> values(wanted, nullptr);
    std::size_t count = wanted;
    napi_get_cb_info(env, info, &count, values.data(), nullptr, nullptr);
    return values;
}

std::string StringOf(napi_env env, napi_value value) {
    std::size_t length = 0;
    if (!value || napi_get_value_string_utf8(env, value, nullptr, 0, &length) != napi_ok) return {};
    std::string text(length, '\0');
    napi_get_value_string_utf8(env, value, text.data(), length + 1, &length);
    return text;
}

napi_value Boolean(napi_env env, bool value) {
    napi_value result = nullptr;
    napi_get_boolean(env, value, &result);
    return result;
}

napi_value String(napi_env env, const std::string& text) {
    napi_value result = nullptr;
    napi_create_string_utf8(env, text.c_str(), text.size(), &result);
    return result;
}

napi_value Undefined(napi_env env) {
    napi_value result = nullptr;
    napi_get_undefined(env, &result);
    return result;
}

// ---- Functions seen from ArkTS ------------------------------------------

// initPaths(shaderDir: string, dataDir: string): void
napi_value InitPaths(napi_env env, napi_callback_info info) {
    const auto args = Arguments(env, info, 2);
    g_shaderDir = StringOf(env, args[0]);
    g_dataDir = StringOf(env, args[1]);
    OH_LOG_INFO(LOG_APP, "paths set");
    return Undefined(env);
}

// engineVersion(): string
napi_value EngineVersion(napi_env env, napi_callback_info) {
    return String(env, "mangekyo-ohos-" GPU_BENCH_APP_VERSION "-gpu_engine");
}

// hasSurface(): boolean
napi_value HasSurface(napi_env env, napi_callback_info) {
    std::lock_guard<std::mutex> lock(g_windowMutex);
    return Boolean(env, g_window != nullptr);
}

// isRunning(): boolean
napi_value IsRunning(napi_env env, napi_callback_info) {
    return Boolean(env, g_workerAlive.load(std::memory_order_acquire) || gpu_bench::ohos_host::IsRunning());
}

// lastError(): string
napi_value LastError(napi_env env, napi_callback_info) {
    std::lock_guard<std::mutex> lock(g_workerMutex);
    return String(env, g_lastError);
}

// stopWorkload(): void
napi_value StopWorkload(napi_env env, napi_callback_info) {
    gpu_bench::RequestStop();
    return Undefined(env);
}

// startWorkload(workloadId: string, seconds: number): boolean
napi_value StartWorkload(napi_env env, napi_callback_info info) {
    const auto args = Arguments(env, info, 2);
    const std::string workload = StringOf(env, args[0]);
    double seconds = 3.0;
    if (args[1]) napi_get_value_double(env, args[1], &seconds);

    std::lock_guard<std::mutex> lock(g_workerMutex);
    if (g_shaderDir.empty()) {
        g_lastError = "initPaths not called";
        return Boolean(env, false);
    }
    if (AnythingRunning()) {
        g_lastError = "already running";
        return Boolean(env, false);
    }
    OHNativeWindow* window = nullptr;
    {
        std::lock_guard<std::mutex> windowLock(g_windowMutex);
        window = g_window;
        // Keep the window alive for the whole run.
        if (window) OH_NativeWindow_NativeObjectReference(window);
    }
    if (!window) {
        g_lastError = "no native window (wait for the preview surface)";
        return Boolean(env, false);
    }
    JoinWorkerUnlocked();
    g_lastError.clear();
    g_workerAlive.store(true, std::memory_order_release);
    g_worker = std::thread([window, workload, seconds]() {
        gpu_bench::ohos_host::RunRequest request;
        request.window = window;
        request.shaderDir = g_shaderDir;
        request.dataDir = g_dataDir;
        request.workloadId = workload.empty() ? "stream" : workload;
        request.maxRunTimeSec = seconds > 0.0 ? seconds : 3.0;

        std::string error;
        const int rc = gpu_bench::ohos_host::RunWorkload(request, error);
        std::cout.flush();
        OH_NativeWindow_NativeObjectUnreference(window);
        {
            std::lock_guard<std::mutex> errorLock(g_workerMutex);
            if (rc != 0) g_lastError = error.empty() ? ("run failed rc=" + std::to_string(rc)) : error;
            else g_lastError.clear();
        }
        if (rc != 0) OH_LOG_ERROR(LOG_APP, "workload failed: %{public}s", error.c_str());
        g_workerAlive.store(false, std::memory_order_release);
    });
    return Boolean(env, true);
}

// pollOutput(): string[]  Lines the engine printed since the last call.
napi_value PollOutput(napi_env env, napi_callback_info) {
    const std::vector<std::string> lines = g_output.Drain();
    napi_value result = nullptr;
    napi_create_array_with_length(env, lines.size(), &result);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        napi_set_element(env, result, static_cast<uint32_t>(i), String(env, lines[i]));
    }
    return result;
}

// startCli(args: string[]): boolean  Runs `gpu_benchmark <args>` in-process on
// a worker thread. Only the CPU paths are meant to be started this way: they
// return before any GPU work.
napi_value StartCli(napi_env env, napi_callback_info info) {
    const auto args = Arguments(env, info, 1);
    std::vector<std::string> arguments{"gpu_benchmark"};
    uint32_t count = 0;
    if (args[0]) napi_get_array_length(env, args[0], &count);
    for (uint32_t i = 0; i < count; ++i) {
        napi_value item = nullptr;
        if (napi_get_element(env, args[0], i, &item) == napi_ok) arguments.push_back(StringOf(env, item));
    }

    std::lock_guard<std::mutex> lock(g_workerMutex);
    if (AnythingRunning()) {
        g_lastError = "already running";
        return Boolean(env, false);
    }
    if (g_cliThread.joinable()) g_cliThread.join();
    g_lastError.clear();
    g_cliAlive.store(true, std::memory_order_release);
    g_cliThread = std::thread([arguments]() mutable {
        // path_service reads GPU_BENCH_DATA_DIR once; set it before any result is saved.
        if (!g_dataDir.empty()) setenv("GPU_BENCH_DATA_DIR", g_dataDir.c_str(), 1);
        // The command-line entry looks for shaders beside an executable; here they
        // are the files unpacked from the app's resources.
        if (!g_shaderDir.empty()) setenv("GPU_BENCH_SHADER_DIR", g_shaderDir.c_str(), 1);
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
    return Boolean(env, true);
}

// isCliRunning(): boolean
napi_value IsCliRunning(napi_env env, napi_callback_info) {
    return Boolean(env, g_cliAlive.load(std::memory_order_acquire));
}

// cliExitCode(): number
napi_value CliExitCode(napi_env env, napi_callback_info) {
    napi_value result = nullptr;
    napi_create_int32(env, g_cliExitCode.load(std::memory_order_acquire), &result);
    return result;
}

napi_value Init(napi_env env, napi_value exports) {
    const napi_property_descriptor functions[] = {
        {"initPaths", nullptr, InitPaths, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"engineVersion", nullptr, EngineVersion, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"hasSurface", nullptr, HasSurface, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"isRunning", nullptr, IsRunning, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"lastError", nullptr, LastError, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"stopWorkload", nullptr, StopWorkload, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"startWorkload", nullptr, StartWorkload, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"pollOutput", nullptr, PollOutput, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"startCli", nullptr, StartCli, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"isCliRunning", nullptr, IsCliRunning, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"cliExitCode", nullptr, CliExitCode, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(functions) / sizeof(functions[0]), functions);
    RegisterSurfaceCallbacks(env, exports);
    return exports;
}

napi_module g_module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "mangekyo",
    .nm_priv = nullptr,
    .reserved = {nullptr},
};

}  // namespace

extern "C" __attribute__((constructor)) void RegisterMangekyoModule() {
    napi_module_register(&g_module);
}
