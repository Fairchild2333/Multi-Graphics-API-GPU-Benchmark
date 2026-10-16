#pragma once
#include "gpu_common.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

namespace gpu_bench {
inline std::string CaptureTag(std::string value, const char* fallback) {
    const std::string original = value;
    for (char& ch : value) {
        const auto byte = static_cast<unsigned char>(ch);
        if (byte < 0x20 || ch == ' ' || ch == '.' || ch == '(' || ch == ')' ||
            std::strchr("<>:\"/\\|?*", ch) != nullptr)
            ch = '_';
    }
    value.erase(std::unique(value.begin(), value.end(),
        [](char a, char b) { return a == '_' && b == '_'; }), value.end());
    while (!value.empty() && value.front() == '_') value.erase(value.begin());
    while (!value.empty() && value.back() == '_') value.pop_back();
    if (value.empty()) value = fallback;
    if (value.size() > 80) {
        std::uint32_t hash = 2166136261u;
        for (unsigned char byte : original) {
            hash ^= byte;
            hash *= 16777619u;
        }
        std::size_t cut = 64;
        while (cut > 0 && cut < value.size() &&
               (static_cast<unsigned char>(value[cut]) & 0xC0u) == 0x80u)
            --cut;
        value.resize(cut);
        std::ostringstream suffix;
        suffix << '_' << std::hex << std::setw(8) << std::setfill('0') << hash;
        value += suffix.str();
    }
    return value;
}

inline std::string CaptureFileStem(const std::string& backend, const std::string& device,
                                   const BenchmarkConfig& config) {
    std::string stem = CaptureTag(backend, "api") + "_" + CaptureTag(device, "gpu")
        + "_" + workloadId(config.workload);
    if (config.workload == Workload::CinematicLiquidV1)
        stem += "_v1_shader" + std::to_string(kCinematicLiquidShaderVersion);
    else if (config.workload == Workload::CinematicLiquid)
        stem += "_v2_shader" + std::to_string(kCinematicLiquidV2ShaderVersion);
    else if (config.workload == Workload::GpuBurnV1)
        stem += "_v1_shader" + std::to_string(kGpuBurnV1ShaderVersion)
             + "_steps" + std::to_string(config.gpuBurnIter);
    if (config.framesInFlight != kMaxFramesInFlight)
        stem += "_flights" + std::to_string(config.framesInFlight);
    if (config.particlesOverridden &&
        (config.workload == Workload::Stream || config.workload == Workload::Render3D))
        stem += "_particles" + std::to_string(config.particleCount);
    if (config.workload == Workload::Stream && !config.headless && config.particleFixedCoverage)
        stem += "_coveragev2";
    return stem;
}
}  // namespace gpu_bench
