#include "vulkan_membw_results.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace gpu_bench;
    MemBwConfig config;
    MemBwReport report;
    report.deviceName = "Test GPU";
    report.unifiedMemory = true;
    report.read.memoryGBps = 35.25;
    report.chunkBytes = 512 * 1024;
    report.maxDatasetBytes = 256ull << 20;
    report.warmupRuns = 3;
    report.measuredRuns = 10;
    report.timingSource = "gpu_timestamp";
    for (const auto* kernel : {"read", "write", "copy", "host_read", "host_write", "l1", "l1_single"}) {
        MemBwPoint point;
        point.kernel = kernel;
        point.datasetBytes = 1ull << 20;
        point.medianGBps = 123.45;
        report.points.push_back(point);
    }
    auto result = MakeStoredVulkanMemBwResult(report, config, "", 0, "Test CPU");
    assert(result.deviceName == "Test GPU" && result.cpuName == "Test CPU");
    assert(result.graphicsApi == "Vulkan" && result.workload == "membw");
    assert(result.score == 35.25 && result.scoreUnit == "GB/s");
    assert(result.headless && result.framesInFlight == 0 && result.timingSamples == 70);
    assert(result.workloadVersion.find("preview") != std::string::npos);
    for (const auto* kernel : {"read", "write", "copy", "host_read", "host_write"})
        assert(result.workloadConfig.find(std::string("curve_") + kernel + "=1.0:123.5") != std::string::npos);
    for (const auto* kernel : {"l1", "l1_single"})
        assert(result.workloadConfig.find(std::string("curve_") + kernel + "=1024:123.5") != std::string::npos);
    assert(result.workloadConfig.find("memoryPlateauReached=0") != std::string::npos);
    report.read.memoryPlateauReached = true;
    result = MakeStoredVulkanMemBwResult(report, config, "Named GPU", 1024);
    assert(result.deviceName == "Named GPU" && result.vramMB == 1024);
    assert(result.workloadVersion.find("preview") == std::string::npos);
    config.maxDatasetBytes = 256ull << 20;
    result = MakeStoredVulkanMemBwResult(report, config);
    assert(result.workloadVersion.find("preview") != std::string::npos);
    std::cout << "Vulkan result contract and all curve families preserved\n";
}
