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
    std::string workloadId;  // "stream" | "gpu_burn" (others rejected for now)
    double maxRunTimeSec = 3.0;
    std::uint32_t particleCount = 262144;  // lighter than desktop 1M for first slice
};

// Blocking: constructs VulkanBackend and runs AppBase::Run on the calling thread.
// Returns 0 on success, non-zero on failure. Cooperative cancel via RequestStop().
int RunWorkload(const RunRequest& request, std::string& errorOut);

bool IsRunning();

}  // namespace gpu_bench::android_host
