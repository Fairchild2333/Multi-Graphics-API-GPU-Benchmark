#include "android_engine_host.h"

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

Workload ParseWorkload(const std::string& id) {
    if (id == "stream" || id == "particle") return Workload::Stream;
    if (id == "gpu_burn") return Workload::GpuBurnV1;
    throw std::invalid_argument("unsupported Android workload id: " + id);
}

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
    if (!request.window) {
        errorOut = "ANativeWindow is null";
        return 2;
    }
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
        cfg.workload = ParseWorkload(request.workloadId);
        cfg.maxRunTimeSec = request.maxRunTimeSec > 0.0 ? request.maxRunTimeSec : 3.0;
        cfg.warmupTimeSec = std::min(0.5, cfg.maxRunTimeSec * 0.2);
        cfg.particleCount = request.particleCount;
        cfg.renderDocEnabled = false;
        cfg.captureAtSec = -1.0;
        cfg.captureAtFrame = -1;
        cfg.headless = false;
        if (cfg.workload == Workload::GpuBurnV1) {
            cfg.gpuBurnIter = kGpuBurnV3LightIter;
            cfg.difficultyLabel = "Light";
        }

        auto app = std::make_unique<VulkanBackend>(
            /*gpuIndex=*/0, EnsureTrailingSlash(request.shaderDir), cfg);
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

}  // namespace gpu_bench::android_host
