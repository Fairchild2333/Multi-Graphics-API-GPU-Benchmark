// GpuBenchBridge.mm — ObjC++ bridge between Swift UI and C++ gpu_engine library on iOS.
// Handles stdout capture, working directory, and JSON serialization.

#import "GpuBenchBridge.h"

#include "gpu_engine.h"
#include "benchmark_results.h"
#include "ios_engine_host.h"
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
    return strdup_alloc("mangekyo-ios-0.2.0-gpu_engine");
}

char* gpb_list_gpus(void) {
    // Run cliMain with --list-gpus and capture stdout via pipe.
    int pipefd[2];
    if (pipe(pipefd) != 0) return strdup_alloc("[]");

    int savedStdout = dup(STDOUT_FILENO);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);

    // Flush C++ buffers before redirect
    std::cout.flush();
    fflush(stdout);

    const char* argv[] = { "gpu_benchmark", "--list-gpus" };
    gpu_bench::cliMain(2, const_cast<char**>(argv));

    // Restore stdout
    std::cout.flush();
    fflush(stdout);
    dup2(savedStdout, STDOUT_FILENO);
    close(savedStdout);

    // Read captured output
    std::string output;
    char buf[4096];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0)
        output.append(buf, n);
    close(pipefd[0]);

    // Parse "GPU\t<index>\t<name>\t..." lines into JSON array
    std::ostringstream json;
    json << "[";
    bool first = true;
    std::istringstream ss(output);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("GPU\t", 0) != 0) continue;
        // Split by tab
        std::vector<std::string> fields;
        std::stringstream ls(line);
        std::string tok;
        while (std::getline(ls, tok, '\t')) fields.push_back(tok);
        // Expected: GPU, index, name, luidHi, luidLo, supportsMetal, supportsVulkan, supportsOpenGL, vramMB, ...
        if (fields.size() < 3) continue;
        if (!first) json << ",";
        first = false;
        json << "{\"index\":" << fields[1]
             << ",\"name\":\"" << jsonEscape(fields[2]) << "\"";
        // Parse additional fields if available
        if (fields.size() > 8)
            json << ",\"vramMB\":" << fields[8];
        if (fields.size() > 5)
            json << ",\"metal\":" << (fields[5] == "1" ? "true" : "false");
        if (fields.size() > 6)
            json << ",\"vulkan\":" << (fields[6] == "1" ? "true" : "false");
        if (fields.size() > 7)
            json << ",\"opengl\":" << (fields[7] == "1" ? "true" : "false");
        json << "}";
    }
    json << "]";
    return strdup_alloc(json.str());
}

int gpb_run(const char* const* argv, int argc,
            gpb_line_callback onLine, void* ctx) {
    std::lock_guard<std::mutex> lock(g_runMutex);

    // Create pipe for stdout capture
    int pipefd[2];
    if (pipe(pipefd) != 0) return -1;

    int savedStdout = dup(STDOUT_FILENO);
    int savedStderr = dup(STDERR_FILENO);
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[1]);

    // Reader thread: read from pipe and call back line by line
    std::string lineBuffer;
    bool readerDone = false;
    std::thread reader([&]() {
        char buf[4096];
        ssize_t n;
        while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
            for (ssize_t i = 0; i < n; ++i) {
                if (buf[i] == '\n') {
                    if (onLine) onLine(lineBuffer.c_str(), ctx);
                    lineBuffer.clear();
                } else if (buf[i] != '\r') {
                    lineBuffer += buf[i];
                }
            }
        }
        // Flush remaining partial line
        if (!lineBuffer.empty() && onLine)
            onLine(lineBuffer.c_str(), ctx);
        close(pipefd[0]);
        readerDone = true;
    });

    // Run the engine
    int result = gpu_bench::cliMain(argc, const_cast<char**>(argv));

    // Restore stdout/stderr — this closes the write end of the pipe,
    // causing the reader thread to exit.
    std::cout.flush();
    fflush(stdout);
    fflush(stderr);
    dup2(savedStdout, STDOUT_FILENO);
    dup2(savedStderr, STDERR_FILENO);
    close(savedStdout);
    close(savedStderr);

    reader.join();
    return result;
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
