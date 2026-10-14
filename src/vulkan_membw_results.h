#pragma once
#include "benchmark_results.h"
#include "vulkan_membw.h"
namespace gpu_bench {
// One serializer for desktop and embedded Vulkan sweeps, preserving all curves.
BenchmarkResult MakeStoredVulkanMemBwResult(const MemBwReport& report,
    const MemBwConfig& config, const std::string& deviceName = "",
    std::uint32_t vramMB = 0, const std::string& cpuName = "");
}
