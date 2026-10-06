#pragma once

// What an app asks an embedded host (Android, HarmonyOS, iOS) to run:
//
//   "<test>"   or   "<test>?<option>=<value>&<option>=<value>"
//
//   stream?particles=4194304   particle count, rounded up to a whole work group
//   gpu_burn?steps=64          fixed steps per pixel, 16 to 2048
//   cinematic_liquid           no options
//
// A test without options runs what the desktop app selects at first:
// 1,048,576 particles, 16 steps. The names of the first mobile builds keep
// working: stream_heavy, gpu_burn_medium (64 steps), gpu_burn_heavy (256).
//
// The settings are the ones the desktop command line makes for the same choice
// (main.cpp), so a result reads the same on every platform.

#include "gpu_common.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace gpu_bench {

namespace embedded_detail {

// The largest particle count a host accepts: a working set of 2 GiB.
inline constexpr std::uint32_t kMaxParticles = 67108864u;

inline std::uint32_t ParseCount(const std::string& text, const std::string& option) {
    std::size_t used = 0;
    unsigned long long value = 0;
    try {
        value = std::stoull(text, &used);
    } catch (const std::exception&) {
        used = 0;
    }
    if (used == 0 || used != text.size() || value == 0 || value > 0xFFFFFFFFull) {
        throw std::invalid_argument("option '" + option + "' needs a positive whole number, got '" + text + "'");
    }
    return static_cast<std::uint32_t>(value);
}

}  // namespace embedded_detail

// Sets the test and its size in `cfg`. Throws std::invalid_argument for a test
// or an option the hosts do not know.
inline void ApplyEmbeddedTest(const std::string& request, BenchmarkConfig& cfg) {
    const std::size_t mark = request.find('?');
    const std::string id = request.substr(0, mark);

    std::uint32_t particles = 0;
    std::uint32_t steps = 0;
    if (mark != std::string::npos) {
        std::size_t at = mark + 1;
        while (at < request.size()) {
            std::size_t end = request.find('&', at);
            if (end == std::string::npos) end = request.size();
            const std::string pair = request.substr(at, end - at);
            const std::size_t equals = pair.find('=');
            const std::string name = pair.substr(0, equals);
            const std::string value = equals == std::string::npos ? std::string() : pair.substr(equals + 1);
            if (name == "particles") {
                particles = embedded_detail::ParseCount(value, name);
            } else if (name == "steps") {
                steps = embedded_detail::ParseCount(value, name);
            } else {
                throw std::invalid_argument("unknown option '" + name + "' in '" + request + "'");
            }
            at = end + 1;
        }
    }

    if (id == "stream" || id == "particle" || id == "stream_heavy") {
        if (steps != 0) throw std::invalid_argument("'steps' is an option of gpu_burn, not of " + id);
        if (particles > embedded_detail::kMaxParticles) {
            throw std::invalid_argument("at most " + std::to_string(embedded_detail::kMaxParticles) + " particles");
        }
        // The presets of the desktop app: Light, Medium, Heavy, Extreme.
        std::uint32_t count = particles != 0 ? particles : 1048576u;
        count = (count + kComputeWorkGroupSize - 1) / kComputeWorkGroupSize * kComputeWorkGroupSize;
        cfg.workload = Workload::Stream;
        cfg.particleCount = count;
        cfg.particlesOverridden = true;
        cfg.difficultyLabel = count == 65536u      ? "Light"
                            : count == 1048576u    ? "Medium"
                            : count == 4194304u    ? "Heavy"
                            : count == 16777216u   ? "Extreme"
                                                   : "Custom";
        return;
    }

    if (id == "gpu_burn" || id == "gpu_burn_medium" || id == "gpu_burn_heavy") {
        if (particles != 0) throw std::invalid_argument("'particles' is an option of stream, not of " + id);
        const std::uint32_t wanted = steps != 0                ? steps
                                   : id == "gpu_burn_heavy"    ? kGpuBurnV3HeavyIter
                                   : id == "gpu_burn_medium"   ? kGpuBurnV3MediumIter
                                                               : kGpuBurnV3LightIter;
        cfg.workload = Workload::GpuBurnV1;
        cfg.gpuBurnIter = std::clamp(wanted, gpuBurnDefaultIter(cfg.workload), kGpuBurnV3MaxCustomIter);
        cfg.gpuBurnAutoTune = false;
        cfg.gpuBurnIterOverridden = true;
        cfg.particleCount = kComputeWorkGroupSize;   // GPU Burn uses no particle data
        cfg.particlesOverridden = true;
        cfg.difficultyLabel = "GPU Burn v3 / Selectable fixed steps";
        return;
    }

    if (id == "cinematic_liquid") {
        if (particles != 0 || steps != 0) throw std::invalid_argument("cinematic_liquid has no options");
        cfg.workload = Workload::CinematicLiquid;
        cfg.particleCount = kComputeWorkGroupSize;   // the generic particle buffer is unused
        cfg.particlesOverridden = true;
        cfg.framesInFlight = 1;
        cfg.difficultyLabel = "Cinematic Liquid v2";
        return;
    }

    throw std::invalid_argument("unsupported test: " + id);
}

}  // namespace gpu_bench
