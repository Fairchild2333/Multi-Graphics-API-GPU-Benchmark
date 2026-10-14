// GpuBenchBridge.mm — ObjC++ bridge between Swift UI and C++ gpu_engine library on iOS.
// Handles stdout capture, working directory, and JSON serialization.

#define HAVE_METAL 1
#define GPU_BENCH_NO_GLFW 1

#import "GpuBenchBridge.h"

#include "gpu_engine.h"
#include "cpu_cli.h"
#include "gpu_memory_sweep.h"
#include "gpu_cache_probe.h"
#include "benchmark_results.h"
#include "ios_engine_host.h"
#include "embedded_request.h"
#include "ios_gles_backend.h"
#include "metal_probe.h"
#include "path_service.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <streambuf>
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


// Serialise is NOT reentrant — cliMain can't run concurrently anyway.
static std::mutex g_runMutex;

static std::string g_shaderDir;
static std::string g_dataDir;
static void* g_metalLayer = nullptr;
static void* g_glesLayer = nullptr;
static std::mutex g_embedMutex;
static std::atomic<bool> g_workerAlive{false};
static std::mutex g_workerMutex;
static std::thread g_worker;
static std::string g_lastError;

// What an embedded run prints to std::cout lands here, split into lines that
// the app collects with gpb_poll_output(). The engine prints from one thread
// at a time; the lock only guards against the collecting thread.
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

static LineQueueBuffer g_embeddedOutput;

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
    return gpb_start_workload_with_options(workload_id, seconds, false);
}

void gpb_set_gles_layer(void* layer) {
    std::lock_guard<std::mutex> lock(g_embedMutex);
    if (gpu_bench::ios_host::IsRunning() || g_workerAlive.load()) return;
    g_glesLayer = layer;
}

bool gpb_start_workload_with_options(const char* workload_id, double seconds, bool allow_translated_apis) {
    std::string workload = workload_id ? workload_id : "stream";
    const auto id = workload.substr(0, workload.find('?'));
    const auto api = gpu_bench::embedded_detail::OptionValue(workload, "api");
    if (api == "vulkan" && !allow_translated_apis) {
        std::lock_guard<std::mutex> lock(g_workerMutex);
        g_lastError = "Vulkan uses MoltenVK translation; enable translated APIs first";
        return false;
    }
    void* layer = nullptr;
    void* glesLayer = nullptr;
    std::string shaderDir;
    std::string dataDir;
    {
        std::lock_guard<std::mutex> lock(g_embedMutex);
        layer = g_metalLayer;
        glesLayer = g_glesLayer;
        shaderDir = g_shaderDir;
        dataDir = g_dataDir;
    }
    if ((api == "gles2" && !glesLayer) || (api != "gles2" && !layer && id != "memory_sweep" && id != "gpu_cache" && id != "stream_headless")) {
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
    g_worker = std::thread([layer, glesLayer, shaderDir, dataDir, workload, seconds, allow_translated_apis]() {
        gpu_bench::ios_host::RunRequest req;
        req.metalLayer = layer;
        req.glesLayer = glesLayer;
        req.shaderDir = shaderDir;
        req.dataDir = dataDir;
        req.workloadId = workload;
        req.maxRunTimeSec = seconds > 0.0 ? seconds : 3.0;
        req.allowTranslatedApis = allow_translated_apis;
        std::string err;
        std::unique_lock<std::mutex> runLock(g_runMutex, std::try_to_lock);
        if (!runLock.owns_lock()) {
            std::lock_guard<std::mutex> lock(g_workerMutex);
            g_lastError = "CPU workload already running";
            g_workerAlive.store(false, std::memory_order_release);
            return;
        }
        // Keep the lines of this run for gpb_poll_output().
        std::cout.flush();
        std::streambuf* const consoleOutput = std::cout.rdbuf(&g_embeddedOutput);
        std::streambuf* const consoleError = std::cerr.rdbuf(&g_embeddedOutput);
        gpu_bench::ClearStopRequest();
        int rc = 1;
        try {
            rc = gpu_bench::ios_host::RunWorkload(req, err);
        } catch (const std::exception& e) {
            err = e.what();
        } catch (...) {
            err = "unknown native exception";
        }
        std::cout.flush();
        std::cout.rdbuf(consoleOutput);
        std::cerr.flush();
        std::cerr.rdbuf(consoleError);
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

char* gpb_poll_output(void) {
    std::string text;
    for (const std::string& line : g_embeddedOutput.Drain()) {
        text += line;
        text += '\n';
    }
    return strdup_alloc(text);
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
    bool vulkanLiquid = false;
    const bool vulkan = gpu_bench::ios_host::VulkanAvailable(&vulkanLiquid);
    const auto gles2MaxSteps = gpu_bench::IosGlesMaxSteps();
    const bool gles2 = gles2MaxSteps > 0;
    const auto cacheParallelSteps = gpu_bench::GpuCacheDefaultParallelSteps();
    std::ostringstream json;
    json << "[";
    for (size_t i = 0; i < devices.size(); ++i) {
        if (i > 0) json << ",";
        const auto& d = devices[i];
        json << "{\"index\":" << i
             << ",\"name\":\"" << jsonEscape(d.name) << "\""
             << ",\"vramMB\":" << (d.vramBytes / (1024 * 1024))
             << ",\"metal\":true"
             << ",\"vulkan\":" << (vulkan ? "true" : "false")
             << ",\"vulkanImplementation\":\"MoltenVK\""
             << ",\"vulkanLiquid\":" << (vulkanLiquid ? "true" : "false")
             << ",\"gpuCache\":" << (d.hasUnifiedMemory ? "true" : "false")
             << ",\"gpuCacheParallelSteps\":" << cacheParallelSteps
             << ",\"gles2\":" << (gles2 ? "true" : "false")
             << ",\"gles2MaxSteps\":" << gles2MaxSteps
             << ",\"opengl\":false"
             << "}";
    }
    json << "]";
    return strdup_alloc(json.str());
}

int gpb_run(const char* const* argv, int argc,
            gpb_line_callback onLine, void* ctx) {
    // CPU work has no Metal layer and shares desktop validation/result storage.
    std::lock_guard<std::mutex> lock(g_runMutex);
    if (gpb_is_running()) return 3;
    class CallbackBuffer final : public std::streambuf {
    public:
        CallbackBuffer(gpb_line_callback callback, void* context)
            : callback_(callback), context_(context) {}
        int sync() override { Flush(); return 0; }
    protected:
        int_type overflow(int_type ch) override {
            if (traits_type::eq_int_type(ch, traits_type::eof())) return traits_type::not_eof(ch);
            const char c = traits_type::to_char_type(ch);
            if (c == '\n') Flush();
            else if (c != '\r') line_ += c;
            return ch;
        }
    private:
        void Flush() {
            if (!line_.empty() && callback_) callback_(line_.c_str(), context_);
            line_.clear();
        }
        gpb_line_callback callback_;
        void* context_;
        std::string line_;
    } buffer(onLine, ctx);
    class Redirect final {
    public:
        explicit Redirect(std::streambuf* buffer)
            : out_(std::cout.rdbuf(buffer)), err_(std::cerr.rdbuf(buffer)) {}
        ~Redirect() {
            std::cout.flush(); std::cerr.flush();
            std::cout.rdbuf(out_); std::cerr.rdbuf(err_);
        }
    private:
        std::streambuf* out_;
        std::streambuf* err_;
    } redirect(&buffer);
    gpu_bench::ClearStopRequest();
    return gpu_bench::CpuCliMain(argc, argv);
}

char* gpb_load_results(void) {
    return strdup_alloc(gpu_bench::SerializeResultsJson(gpu_bench::LoadResults()));
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
