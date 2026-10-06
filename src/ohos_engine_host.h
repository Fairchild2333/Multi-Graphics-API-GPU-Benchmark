#pragma once

// HarmonyOS host for gpu_engine: runs a workload of the shared Vulkan backend
// in the window of an XComponent (OHNativeWindow). The counterpart of
// android_engine_host; the formal score contract is out of scope here, so
// results are saved with the "_ohos_preview" suffix.

#include <cstdint>
#include <string>

struct NativeWindow;  // OHNativeWindow, from <native_window/external_window.h>

namespace gpu_bench::ohos_host {

struct RunRequest {
    NativeWindow* window = nullptr;
    std::string shaderDir;   // SPIR-V files; a trailing slash is added if missing
    std::string dataDir;     // sets GPU_BENCH_DATA_DIR for the results path
    // The test and its size, e.g. "stream?particles=4194304" or "gpu_burn?steps=64";
    // see embedded_request.h.
    std::string workloadId;
    double maxRunTimeSec = 3.0;
};

// Blocking: constructs VulkanBackend and runs it on the calling thread.
// Returns 0 on success. Cancel with gpu_bench::RequestStop().
int RunWorkload(const RunRequest& request, std::string& errorOut);

bool IsRunning();

}  // namespace gpu_bench::ohos_host
