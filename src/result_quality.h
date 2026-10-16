#pragma once
#include "benchmark_results.h"
#include "vulkan_membw.h"
#include <cmath>
#include <string>
#include <vector>
namespace gpu_bench {
inline std::string ResultConfigValue(const BenchmarkResult& r, const std::string& key) {
    const std::string prefix = key + "=";
    std::size_t start = 0;
    while (start < r.workloadConfig.size()) {
        const auto end = r.workloadConfig.find(';', start);
        const auto field = r.workloadConfig.substr(start, end == std::string::npos ? end : end - start);
        if (field.compare(0, prefix.size(), prefix) == 0) return field.substr(prefix.size());
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}

inline bool CurrentVulkanMemoryContractVerified(const BenchmarkResult& r) {
    if (r.workload != "membw" || r.graphicsApi != "Vulkan" ||
        r.workloadVersion.find(kMemBwWorkloadVersion) != 0) return false;
    try {
        return ResultConfigValue(r, "dataVerified") == "1" &&
            std::stoul(ResultConfigValue(r, "writeCopyChecks")) > 0 &&
            ResultConfigValue(r, "timestampAnomalies") == "0" &&
            ResultConfigValue(r, "largeDatasetTraversal") == "ordered_full_dataset_passes";
    } catch (...) { return false; }
}
// A V1 result that reached its plateau measured the same large-set bandwidth
// as V3: on an RTX 5090, M4 Pro, A12X and A15 the two agree within 0.2 - 3 %
// (2026-10-17). V2 does not (its repeats inside one chunk read 300.9 GB/s on an
// M4 Pro specified at 273), and V1 results saved before the data check may
// come from a buffer the driver could not address. The first saved result
// whose log shows the check is the MIX 4 rerun 20261008-042956; the checking
// build ran on four phones before it was committed (266a197, 04:50).
// Translated MoltenVK V1 rows carry their own suffix and stay out.
inline constexpr char kFirstDataCheckedMemBwId[] = "20261008-042956";
inline bool LegacyV1VulkanMemoryComparable(const BenchmarkResult& r) {
    if (r.workload != "membw" || r.graphicsApi != "Vulkan") return false;
    const std::string& v = r.workloadVersion;
    return (v == "membw_v1" || v == "membw_v1_android_preview" || v == "membw_v1_ohos_preview") &&
        r.id >= kFirstDataCheckedMemBwId &&
        ResultConfigValue(r, "memoryPlateauReached") == "1" &&
        ResultConfigValue(r, "timestampAnomalies") == "0";
}

// Completion and validity are separate: retain raw observations, but surface
// questionable score denominators or changing throughput for later review.
inline std::vector<std::string> MeasurementReviewReasons(const BenchmarkResult& r) {
    if (r.workload == "membw" && r.graphicsApi == "Vulkan" &&
        r.workloadVersion.find(kMemBwWorkloadVersion) == 0) {
        if (!CurrentVulkanMemoryContractVerified(r)) return {"Vulkan memory data or traversal was not verified"};
        return ResultConfigValue(r, "memoryPlateauReached") == "1"
            ? std::vector<std::string>{} : std::vector<std::string>{"Memory plateau was not reached"};
    }
    if (r.workload != "stream" && r.workload != "gpu_burn" && r.workload != "cinematic_liquid") return {};
    std::vector<std::string> reasons;
    if (r.platform == "macOS" && r.workload == "stream" && r.graphicsApi == "Vulkan" &&
        ResultConfigValue(r, "apiImplementation") == "MoltenVK" &&
        ResultConfigValue(r, "timingMode") != "synchronized_wall_clock")
        reasons.push_back("Translated particle timing requires completion verification");
    if (r.avgFrameTimeMs > 0 && r.avgTotalGpuMs > r.avgFrameTimeMs * 1.1)
        reasons.push_back("GPU timing span exceeds the frame interval");
    if (r.avgComputeMs + r.avgRenderMs > r.avgTotalGpuMs + 0.1)
        reasons.push_back("compute and render timing spans overlap");
    if (r.headless && r.graphicsApi == "Vulkan" && r.particleCount >= 1048576 && r.avgComputeMs < r.avgFrameTimeMs * 0.5)
        reasons.push_back("compute timing needs wall-clock verification");
    if (r.workload != "cinematic_liquid") {
        if (r.throttlePct > 5.0) reasons.push_back("throughput fell by more than 5 percent");
        const bool heavy = r.workload == "gpu_burn" || r.particleCount >= 1048576;
        if (heavy && r.stableScore <= 0) reasons.push_back("stable throughput was not reached");
    }
    return reasons;
}
}
