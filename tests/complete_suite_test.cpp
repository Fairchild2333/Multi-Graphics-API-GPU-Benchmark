#include "complete_suite.h"
#include "capture_naming.h"
#include <iostream>
#include <string>

int main() {
    using namespace gpu_bench;
    const auto metal = SupportedSuiteVariants({true, true, true, true});
    const auto gl41 = SupportedSuiteVariants({false, true, false, false});
    const auto gl43 = SupportedSuiteVariants({true, true, false, false});
    if (metal.size() != 13 || gl41.size() != 3 || gl43.size() != 11 ||
        CpuSuiteVariants().size() != 2 || !SupportedSuiteVariants({}).empty()) return 1;
    for (std::size_t i = 0; i < gl41.size(); ++i)
        if (std::string(gl41[i].workload) != "gpu_burn" || gl41[i].headless ||
            gl41[i].burnSteps != (i == 0 ? 16u : i == 1 ? 64u : 256u)) return 2;
    for (std::size_t i = 0; i < 8; ++i)
        if (std::string(metal[i].workload) != "stream" || metal[i].headless != (i >= 4)) return 3;
    if (metal.size() + gl41.size() + CpuSuiteVariants().size() != 18) return 4;
    const auto unifiedMetal = SupportedSuiteVariants({true, true, true, true, true});
    if (unifiedMetal.size() != 14 || std::string(unifiedMetal.back().workload) != "gpu_cache" ||
        !unifiedMetal.back().headless) return 7;
    // Device-local cache state stays unavailable on non-unified/other APIs.
    if (SupportedSuiteVariants({true, true, true, true, false}).size() != 13) return 8;
    // Two independently enumerated devices each get their GPU matrix; CPU once.
    if (2 * metal.size() + CpuSuiteVariants().size() != 28) return 9;
    BenchmarkConfig capture;
    capture.workload = Workload::GpuBurnV1;
    capture.gpuBurnIter = 16;
    const auto light = CaptureFileStem("Metal", "Apple M4 Pro", capture);
    capture.gpuBurnIter = 64;
    const auto medium = CaptureFileStem("Metal", "Apple M4 Pro", capture);
    if (light == medium || light.find("_steps16") == std::string::npos ||
        medium.find("_steps64") == std::string::npos) return 5;
    capture.workload = Workload::Stream;
    capture.particlesOverridden = true;
    capture.particleCount = 65536;
    if (CaptureFileStem("Metal", "Apple M4 Pro", capture) != "Metal_Apple_M4_Pro_stream_particles65536") return 6;
    std::cout << "Available APIs produce 18 jobs: Metal 13 + GL4.1 Burn 3 + CPU 2; absent APIs produce no jobs.\n";
}
