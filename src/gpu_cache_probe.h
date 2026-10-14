#pragma once
#include <cstdint>

namespace gpu_bench {
struct GpuCacheProbeConfig {
    std::uint32_t gpuIndex = 0;
    std::uint32_t maxMiB = 128;
    std::uint32_t steps = 65536;
    std::uint32_t repeats = 5;
    // Zero preserves the existing 4096+ load parallel contract. Older iOS
    // GPUs use an explicitly versioned shorter dispatch to avoid recovery.
    std::uint32_t parallelSteps = 0;
};
// Native Metal dependent-load and CPU/GPU warming diagnostic. This measures
// address-span curves, not hardware-named L2/L3/SLC speeds or capacities.
int RunGpuCacheProbeCli(const GpuCacheProbeConfig& config, bool saveResult);
std::uint32_t GpuCacheDefaultParallelSteps();
bool GpuCacheProbeAvailable(std::uint32_t gpuIndex);
}
