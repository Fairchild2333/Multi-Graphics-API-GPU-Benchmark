#include "android_engine_host.h"

#include "embedded_request.h"
#include "gles_backend.h"
#include "gpu_engine.h"
#include "vulkan_backend.h"

#include <atomic>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>

namespace gpu_bench::android_host {
namespace {

std::atomic<bool> g_running{false};
std::mutex g_runMutex;

std::string EnsureTrailingSlash(std::string path) {
    if (path.empty()) return path;
    const char c = path.back();
    if (c != '/' && c != '\\') path.push_back('/');
    return path;
}

}  // namespace

bool IsRunning() {
    return g_running.load(std::memory_order_acquire);
}

int RunWorkload(const RunRequest& request, std::string& errorOut) {
    errorOut.clear();
    if (request.shaderDir.empty()) {
        errorOut = "shaderDir is empty";
        return 3;
    }

    std::unique_lock<std::mutex> lock(g_runMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        errorOut = "workload already running";
        return 4;
    }

    g_running.store(true, std::memory_order_release);
    ClearStopRequest();

    // path_service reads GPU_BENCH_DATA_DIR once; set before any Results path use.
    if (!request.dataDir.empty())
        setenv("GPU_BENCH_DATA_DIR", request.dataDir.c_str(), 1);

    try {
        BenchmarkConfig cfg;
        // The test and its size, as the desktop command line sets them.
        ApplyEmbeddedTest(request.workloadId, cfg);
        cfg.maxRunTimeSec = request.maxRunTimeSec > 0.0 ? request.maxRunTimeSec : 3.0;
        cfg.renderDocEnabled = false;
        cfg.captureAtSec = -1.0;
        cfg.captureAtFrame = -1;

        const std::string api = EmbeddedApi(request.workloadId);
        if (api == "gles2") ApplyEs2Legacy(cfg);
        // The headless Vulkan test runs through the command-line entry, as the
        // desktop one does; only OpenGL ES computes without a window here.
        if (cfg.headless && api != "gles") {
            throw std::invalid_argument(
                "the headless test runs here under OpenGL ES only; Vulkan uses the command line");
        }
        if (!cfg.headless && !request.window) {
            errorOut = "ANativeWindow is null";
            g_running.store(false, std::memory_order_release);
            return 2;
        }

        std::unique_ptr<AppBase> app;
        if (api == "gles" || api == "gles2") {
            app = std::make_unique<GlesBackend>(
                /*gpuIndex=*/0, EnsureTrailingSlash(request.shaderDir), cfg);
        } else {
            app = std::make_unique<VulkanBackend>(
                /*gpuIndex=*/0, EnsureTrailingSlash(request.shaderDir), cfg);
        }
        app->SetEmbeddedWindow(request.window);
        // Caller (JNI worker) must keep ANativeWindow alive for Run().
        app->Run();
        g_running.store(false, std::memory_order_release);
        return 0;
    } catch (const std::exception& ex) {
        errorOut = ex.what();
        g_running.store(false, std::memory_order_release);
        return 1;
    } catch (...) {
        errorOut = "unknown native exception";
        g_running.store(false, std::memory_order_release);
        return 1;
    }
}

std::uint32_t EsParticleWorkGroupSize() {
    return IsRunning() ? 0u : gpu_bench::EsParticleWorkGroupSize();
}

}  // namespace gpu_bench::android_host
