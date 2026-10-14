#pragma once
#include "benchmark_results.h"
#include <cmath>
#include <string>
#include <vector>
namespace gpu_bench {
// Completion and validity are separate: retain raw observations, but surface
// questionable score denominators or changing throughput for later review.
inline std::vector<std::string> MeasurementReviewReasons(const BenchmarkResult& r) {
    if (r.workload != "stream" && r.workload != "gpu_burn" && r.workload != "cinematic_liquid") return {};
    std::vector<std::string> reasons;
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
