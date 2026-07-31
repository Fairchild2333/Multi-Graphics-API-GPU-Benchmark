// GpuBenchBridge.mm — ObjC++ bridge between Swift UI and C++ gpu_engine library on iOS.
// Handles stdout capture, working directory, and JSON serialization.

#define HAVE_METAL 1
#define GPU_BENCH_NO_GLFW 1

#import "GpuBenchBridge.h"

#include "gpu_engine.h"
#include "benchmark_results.h"
#include "ios_engine_host.h"
#include "metal_probe.h"
#include "path_service.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static char* strdup_alloc(const std::string& s) {
    char* p = static_cast<char*>(malloc(s.size() + 1));
    if (p) { memcpy(p, s.data(), s.size()); p[s.size()] = '\0'; }
    return p;
}

/// Escape a string for JSON (handles quotes, backslashes, control chars).
static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8]; snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

/// Serialize a BenchmarkResult to a JSON object string.
static std::string resultToJson(const gpu_bench::BenchmarkResult& r) {
    std::ostringstream o;
    o << "{";
    o << "\"id\":\""          << jsonEscape(r.id) << "\",";
    o << "\"timestamp\":\""   << jsonEscape(r.timestamp) << "\",";
    o << "\"appVersion\":\""  << jsonEscape(r.appVersion) << "\",";
    o << "\"workload\":\""    << jsonEscape(r.workload) << "\",";
    o << "\"workloadVersion\":\"" << jsonEscape(r.workloadVersion) << "\",";
    o << "\"workloadConfig\":\"" << jsonEscape(r.workloadConfig) << "\",";
    o << "\"graphicsApi\":\"" << jsonEscape(r.graphicsApi) << "\",";
    o << "\"deviceName\":\""  << jsonEscape(r.deviceName) << "\",";
    o << "\"driverVersion\":\"" << jsonEscape(r.driverVersion) << "\",";
    o << "\"cpuName\":\""     << jsonEscape(r.cpuName) << "\",";
    o << "\"osVersion\":\""   << jsonEscape(r.osVersion) << "\",";
    o << "\"memory\":\""      << jsonEscape(r.memory) << "\",";
    o << "\"vramMB\":"        << r.vramMB << ",";
    o << "\"resWidth\":"      << r.resWidth << ",";
    o << "\"resHeight\":"     << r.resHeight << ",";
    o << "\"particleCount\":" << r.particleCount << ",";
    o << "\"difficulty\":\""  << jsonEscape(r.difficulty) << "\",";
    o << "\"vsync\":"         << (r.vsync ? "true" : "false") << ",";
    o << "\"isSoftware\":"    << (r.isSoftware ? "true" : "false") << ",";
    o << "\"headless\":"      << (r.headless ? "true" : "false") << ",";
    o << "\"framesInFlight\":" << r.framesInFlight << ",";
    o << "\"durationSec\":"   << r.durationSec << ",";
    o << "\"warmupSec\":"     << r.warmupSec << ",";
    o << "\"measuredFrames\":" << r.measuredFrames << ",";
    o << "\"timingSamples\":" << r.timingSamples << ",";
    o << "\"avgComputeMs\":"  << r.avgComputeMs << ",";
    o << "\"minComputeMs\":"  << r.minComputeMs << ",";
    o << "\"maxComputeMs\":"  << r.maxComputeMs << ",";
    o << "\"avgRenderMs\":"   << r.avgRenderMs << ",";
    o << "\"minRenderMs\":"   << r.minRenderMs << ",";
    o << "\"maxRenderMs\":"   << r.maxRenderMs << ",";
    o << "\"avgTotalGpuMs\":" << r.avgTotalGpuMs << ",";
    o << "\"minTotalGpuMs\":" << r.minTotalGpuMs << ",";
    o << "\"maxTotalGpuMs\":" << r.maxTotalGpuMs << ",";
    o << "\"avgFps\":"        << r.avgFps << ",";
    o << "\"avgFrameTimeMs\":" << r.avgFrameTimeMs << ",";
    o << "\"gpuUtilisation\":" << r.gpuUtilisation << ",";
    o << "\"bottleneck\":\""  << jsonEscape(r.bottleneck) << "\",";
    o << "\"score\":"         << r.score << ",";
    o << "\"scoreUnit\":\""   << jsonEscape(r.scoreUnit) << "\",";
    o << "\"precision\":\""   << jsonEscape(r.precision) << "\"";
    o << "}";
    return o.str();
}

// Serialise is NOT reentrant — cliMain can't run concurrently anyway.
static std::mutex g_runMutex;

static std::string g_shaderDir;
static std::string g_dataDir;
static void* g_metalLayer = nullptr;
static std::mutex g_embedMutex;
static std::atomic<bool> g_workerAlive{false};
static std::mutex g_workerMutex;
static std::thread g_worker;
static std::string g_lastError;

static void JoinWorkerUnlocked() {
    if (g_worker.joinable())
        g_worker.join();
    g_workerAlive.store(false, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Public C API
// ---------------------------------------------------------------------------

extern "C" {

void gpb_set_working_dir(const char* path) {
    if (path && *path) chdir(path);
}

void gpb_init_paths(const char* shader_dir, const char* data_dir) {
    std::lock_guard<std::mutex> lock(g_embedMutex);
    g_shaderDir = shader_dir ? shader_dir : "";
    g_dataDir = data_dir ? data_dir : "";
    if (!g_dataDir.empty())
        setenv("GPU_BENCH_DATA_DIR", g_dataDir.c_str(), 1);
}

void gpb_set_metal_layer(void* ca_metal_layer) {
    std::lock_guard<std::mutex> lock(g_embedMutex);
    if (gpu_bench::ios_host::IsRunning() || g_workerAlive.load())
        return;  // keep stable surface while running
    g_metalLayer = ca_metal_layer;
}

bool gpb_start_workload(const char* workload_id, double seconds) {
    std::string workload = workload_id ? workload_id : "stream";
    void* layer = nullptr;
    std::string shaderDir;
    std::string dataDir;
    {
        std::lock_guard<std::mutex> lock(g_embedMutex);
        layer = g_metalLayer;
        shaderDir = g_shaderDir;
        dataDir = g_dataDir;
    }
    if (!layer) {
        std::lock_guard<std::mutex> lock(g_workerMutex);
        g_lastError = "CAMetalLayer not set";
        return false;
    }
    if (shaderDir.empty()) {
        std::lock_guard<std::mutex> lock(g_workerMutex);
        g_lastError = "gpb_init_paths not called";
        return false;
    }

    std::lock_guard<std::mutex> lock(g_workerMutex);
    if (g_workerAlive.load() || gpu_bench::ios_host::IsRunning()) {
        g_lastError = "already running";
        return false;
    }
    JoinWorkerUnlocked();
    g_lastError.clear();
    g_workerAlive.store(true, std::memory_order_release);
    g_worker = std::thread([layer, shaderDir, dataDir, workload, seconds]() {
        gpu_bench::ios_host::RunRequest req;
        req.metalLayer = layer;
        req.shaderDir = shaderDir;
        req.dataDir = dataDir;
        req.workloadId = workload;
        req.maxRunTimeSec = seconds > 0.0 ? seconds : 3.0;
        std::string err;
        const int rc = gpu_bench::ios_host::RunWorkload(req, err);
        {
            std::lock_guard<std::mutex> lock(g_workerMutex);
            if (rc != 0)
                g_lastError = err.empty() ? ("run failed rc=" + std::to_string(rc)) : err;
            else
                g_lastError.clear();
        }
        g_workerAlive.store(false, std::memory_order_release);
    });
    return true;
}

void gpb_stop_workload(void) {
    gpu_bench::RequestStop();
}

bool gpb_is_running(void) {
    return g_workerAlive.load(std::memory_order_acquire) ||
           gpu_bench::ios_host::IsRunning();
}

char* gpb_last_error(void) {
    std::lock_guard<std::mutex> lock(g_workerMutex);
    return strdup_alloc(g_lastError);
}

char* gpb_engine_version(void) {
    return strdup_alloc("mangekyo-ios-0.2.6-gpu_engine");
}

char* gpb_list_gpus(void) {
    // Use ProbeMetalDevices() directly — cliMain is not available on iOS.
    auto devices = gpu_bench::ProbeMetalDevices();
    std::ostringstream json;
    json << "[";
    for (size_t i = 0; i < devices.size(); ++i) {
        if (i > 0) json << ",";
        const auto& d = devices[i];
        json << "{\"index\":" << i
             << ",\"name\":\"" << jsonEscape(d.name) << "\""
             << ",\"vramMB\":" << (d.vramBytes / (1024 * 1024))
             << ",\"metal\":true"
             << ",\"vulkan\":false"
             << ",\"opengl\":false"
             << "}";
    }
    json << "]";
    return strdup_alloc(json.str());
}

int gpb_run(const char* const* argv, int argc,
            gpb_line_callback onLine, void* ctx) {
    // iOS uses the embedded host exclusively — cliMain is not available.
    // Parse minimal args to extract workload and time, then delegate to
    // ios_host::RunWorkload.
    std::lock_guard<std::mutex> lock(g_runMutex);

    std::string workload = "stream";
    double seconds = 3.0;
    for (int i = 0; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--workload" && i + 1 < argc) { workload = argv[++i]; }
        else if (a == "--time" && i + 1 < argc) { seconds = std::atof(argv[++i]); }
        else if (a == "stream" || a == "gpu_burn" || a == "cinematic_liquid") { workload = a; }
    }

    std::string shaderDir, dataDir;
    void* layer = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_embedMutex);
        layer = g_metalLayer;
        shaderDir = g_shaderDir;
        dataDir = g_dataDir;
    }

    gpu_bench::ios_host::RunRequest req;
    req.metalLayer = layer;
    req.shaderDir = shaderDir;
    req.dataDir = dataDir;
    req.workloadId = workload;
    req.maxRunTimeSec = seconds > 0.0 ? seconds : 3.0;

    if (onLine) {
        std::string msg = "[iOS] Running " + workload + " via embedded host";
        onLine(msg.c_str(), ctx);
    }

    std::string err;
    int rc = gpu_bench::ios_host::RunWorkload(req, err);
    if (rc != 0 && onLine) {
        std::string msg = "[iOS] Error: " + (err.empty() ? "rc=" + std::to_string(rc) : err);
        onLine(msg.c_str(), ctx);
    }
    return rc;
}

char* gpb_load_results(void) {
    auto results = gpu_bench::LoadResults();
    std::ostringstream json;
    json << "[";
    for (size_t i = 0; i < results.size(); ++i) {
        if (i > 0) json << ",";
        json << resultToJson(results[i]);
    }
    json << "]";
    return strdup_alloc(json.str());
}

bool gpb_delete_result(const char* resultId) {
    if (!resultId) return false;
    return gpu_bench::DeleteResult(std::string(resultId));
}

bool gpb_clear_results(void) {
    return gpu_bench::ClearResults();
}

void gpb_free(char* ptr) {
    free(ptr);
}

char* gpb_results_dir(void) {
    return strdup_alloc(gpu_bench::paths::ResultsDirectory().string());
}

char* gpb_captures_dir(void) {
    return strdup_alloc(gpu_bench::paths::CapturesDirectory().string());
}

} // extern "C"
