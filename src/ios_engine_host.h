#pragma once

// iOS-embedded host for gpu_engine (CAMetalLayer + MetalBackend::Run).
// Mirrors android_engine_host. Formal score / liquid contracts are out of scope.

#include <cstdint>
#include <string>

namespace gpu_bench::ios_host {

struct RunRequest {
    void* metalLayer = nullptr;  // CAMetalLayer* (__bridge void*)
    std::string shaderDir;       // trailing slash expected by ReadFileBytes callers
    std::string dataDir;         // sets GPU_BENCH_DATA_DIR for sandbox results
    std::string workloadId;      // "stream" | "gpu_burn" | "cinematic_liquid"
    double maxRunTimeSec = 3.0;
    std::uint32_t particleCount = 262144;
};

// Blocking: constructs MetalBackend and runs AppBase::Run on the calling thread.
// Returns 0 on success, non-zero on failure. Cooperative cancel via RequestStop().
int RunWorkload(const RunRequest& request, std::string& errorOut);

bool IsRunning();

}  // namespace gpu_bench::ios_host
