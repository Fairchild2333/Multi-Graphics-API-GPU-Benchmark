#include "result_quality.h"
#include <iostream>
int main() {
    using namespace gpu_bench;
    BenchmarkResult r{};
    r.workload = "stream"; r.graphicsApi = "Vulkan"; r.headless = true;
    r.particleCount = 1048576; r.avgComputeMs = 0.025; r.avgTotalGpuMs = 0.05;
    r.avgFrameTimeMs = 1.473; r.stableScore = 1701.091;
    if (MeasurementReviewReasons(r).empty()) return 1; // Saved A12X false timestamp.
    r.avgComputeMs = r.avgTotalGpuMs = 1.854; r.avgFrameTimeMs = 1.862; r.stableScore = 22.752;
    if (!MeasurementReviewReasons(r).empty()) return 2; // Verified synchronized rerun.
    r.graphicsApi = "Metal"; r.headless = false; r.avgComputeMs = 24.674;
    r.avgRenderMs = 474.972; r.avgTotalGpuMs = 500.856; r.avgFrameTimeMs = 502.066;
    r.stableScore = 27.302;
    if (!MeasurementReviewReasons(r).empty()) return 3;
    r.avgTotalGpuMs = 800;
    if (MeasurementReviewReasons(r).empty()) return 4;
    r.workload = "cinematic_liquid"; r.avgComputeMs = 350.461; r.avgRenderMs = 114.414;
    r.avgTotalGpuMs = 465.732; r.avgFrameTimeMs = 467.362; r.stableScore = 0;
    if (!MeasurementReviewReasons(r).empty()) return 5; // Liquid has no plateau contract.
    r.avgComputeMs = 358.243; r.avgRenderMs = 471.078; r.avgTotalGpuMs = 471.141;
    if (MeasurementReviewReasons(r).empty()) return 6;
    r.workload = "gpu_burn"; r.avgComputeMs = 0; r.avgRenderMs = r.avgTotalGpuMs = 24.518;
    r.avgFrameTimeMs = 25.217; r.stableScore = 1.154; r.throttlePct = 9.73;
    if (MeasurementReviewReasons(r).empty()) return 7; // Falling but locally stable is still review.
    r.workload = "cpu_membw";
    if (!MeasurementReviewReasons(r).empty()) return 8;
    std::cout << "Real invalid timestamps, overlap and falling throughput flagged; verified reruns and liquid accepted.\n";
}
