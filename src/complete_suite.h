#pragma once

#include <cstdint>
#include <vector>

namespace gpu_bench {

struct SuiteCapabilities {
    bool particles = false;
    bool burn = false;
    bool liquid = false;
    bool memory = false;
    bool cacheState = false;
};

struct SuiteVariant {
    const char* workload;
    std::uint32_t particles = 0;
    std::uint32_t burnSteps = 0;
    bool headless = false;
};

// Same sizes and three Burn tiers as adb-suite.ps1; unavailable API/workload
// combinations never enter the queue. CPU jobs are added once per machine.
inline std::vector<SuiteVariant> SupportedSuiteVariants(SuiteCapabilities caps) {
    std::vector<SuiteVariant> result;
    if (caps.particles)
        for (bool headless : {false, true})
            for (std::uint32_t count : {65536u, 1048576u, 4194304u, 16777216u})
                result.push_back({"stream", count, 0, headless});
    if (caps.burn)
        for (std::uint32_t steps : {16u, 64u, 256u})
            result.push_back({"gpu_burn", 0, steps, false});
    if (caps.liquid) result.push_back({"cinematic_liquid"});
    if (caps.memory) result.push_back({"membw", 0, 0, true});
    if (caps.cacheState) result.push_back({"gpu_cache", 0, 0, true});
    return result;
}

inline std::vector<SuiteVariant> CpuSuiteVariants() {
    return {{"cpu_all", 0, 0, true}, {"cpu_membw", 0, 0, true}};
}

}  // namespace gpu_bench
