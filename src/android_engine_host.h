#pragma once

// Android-embedded host for gpu_engine (ANativeWindow + VulkanBackend::Run).
// Formal score contract / GLES fallback are intentionally out of scope here.

#include <android/native_window.h>

#include <cstdint>
#include <string>

namespace gpu_bench::android_host {

struct RunRequest {
    ANativeWindow* window = nullptr;
    std::string shaderDir;   // trailing slash expected by ReadFileBytes callers
    std::string dataDir;     // sets GPU_BENCH_DATA_DIR for results path
    // The test and its size, e.g. "stream?particles=4194304" or "gpu_burn?steps=64";
    // see embedded_request.h.
    std::string workloadId;
    double maxRunTimeSec = 3.0;
};

// Blocking: constructs VulkanBackend and runs AppBase::Run on the calling thread.
// Returns 0 on success, non-zero on failure. Cooperative cancel via RequestStop().
int RunWorkload(const RunRequest& request, std::string& errorOut);

bool IsRunning();

}  // namespace gpu_bench::android_host
