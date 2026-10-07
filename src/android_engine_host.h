#pragma once

// Android-embedded host for gpu_engine: runs one test on an ANativeWindow with
// the Vulkan backend or, when the request says "api=gles", the OpenGL ES one.

#include <android/native_window.h>

#include <cstdint>
#include <string>

namespace gpu_bench::android_host {

struct RunRequest {
    // Where the picture goes. Only a headless OpenGL ES test may leave it out.
    ANativeWindow* window = nullptr;
    std::string shaderDir;   // trailing slash expected by ReadFileBytes callers
    std::string dataDir;     // sets GPU_BENCH_DATA_DIR for results path
    // The test and its size, e.g. "stream?particles=4194304" or "gpu_burn?steps=64";
    // see embedded_request.h.
    std::string workloadId;
    double maxRunTimeSec = 3.0;
};

// Blocking: constructs the backend and runs AppBase::Run on the calling thread.
// Returns 0 on success, non-zero on failure. Cooperative cancel via RequestStop().
int RunWorkload(const RunRequest& request, std::string& errorOut);

bool IsRunning();

// See EsParticleWorkGroupSize in gles_backend.h; 0 while a test is running.
std::uint32_t EsParticleWorkGroupSize();

}  // namespace gpu_bench::android_host
