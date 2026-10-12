#include "ios_engine_host.h"

#include "embedded_request.h"
#include "gpu_engine.h"
#include "metal_backend.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace gpu_bench::ios_host {
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
    if (!request.metalLayer && request.workloadId.rfind("stream_headless", 0) != 0) {
        errorOut = "CAMetalLayer is null";
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
        // ApplyEmbeddedTest preserves compute-only requests and V-Sync.

        auto app = std::make_unique<MetalBackend>(
            /*gpuIndex=*/0, EnsureTrailingSlash(request.shaderDir), cfg);
        app->SetEmbeddedWindow(request.metalLayer);
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

}  // namespace gpu_bench::ios_host
