#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace gpu_bench {

class BackToMenuException : public std::exception {
public:
    const char* what() const noexcept override { return "User requested return to backend menu"; }
};

constexpr std::uint32_t kWindowWidth  = 1280;
constexpr std::uint32_t kWindowHeight = 720;
constexpr std::uint32_t kMaxFramesInFlight    = 2;
constexpr std::uint32_t kParticleCount        = 1048576;
constexpr std::uint32_t kComputeWorkGroupSize = 256;
constexpr double kTimingReportIntervalSec = 1.0;

// ---- Workload selection -------------------------------------------------
// Stream       : original particle-update kernel — memory-bandwidth bound.
// NBody        : all-pairs gravitational N-body — compute (FP32 ALU + SFU) bound.
// StressFractal: fullscreen per-pixel fractal — fill-rate + fragment ALU/SFU
//                bound; sustained max-load "stress test" (FurMark-style).
// SynthPeak    : register-bound FMA loop — measures peak ALU throughput per
//                precision (vkpeak-style); reports GFLOPS / GIOPS.
// Render3D     : true 3D rendering — perspective + orbiting camera + depth test,
//                particles drawn as instanced camera-facing billboard quads.
//                Stresses vertex transform + rasterisation + fill + ROP.
enum class Workload { Stream, NBody, StressFractal, SynthPeak, Render3D };

// Render3D default instance (billboard) count — moderate to keep overdraw sane.
constexpr std::uint32_t kRender3DDefaultParticles = 262144;  // 256K

// Numeric precision for the SynthPeak workload.
enum class Precision { FP32, FP16, FP64, INT32 };

// SynthPeak tuning: each thread runs `iters` loop passes, each doing
// kSynthPeakUnroll independent fused multiply-adds (2 ops each).
constexpr std::uint32_t kSynthPeakUnroll        = 8;
constexpr std::uint32_t kSynthPeakDefaultThreads = 1u << 20;  // 1,048,576 (mult. of 256)
constexpr std::uint32_t kSynthPeakDefaultIters  = 16384;

// Default per-pixel iteration count for the fractal stress test. Each pixel
// runs exactly this many iterations (no early-out) so the load is constant
// and scales linearly; tune with --iter to saturate a given GPU.
constexpr std::uint32_t kFractalDefaultIter = 2000;

// N-body is O(N^2); it uses a much smaller default body count than the
// 1M-particle Stream default, and the body count must be a multiple of the
// workgroup size (used as the shared-memory tile size).
constexpr std::uint32_t kNBodyDefaultBodies = 65536;   // 64K -> ~4.3e9 interactions/step
// Standard convention for flops per pairwise gravity interaction
// (Nyland et al., "Fast N-Body Simulation with CUDA", GPU Gems 3).
constexpr double        kNBodyFlopsPerInteraction = 20.0;

struct Particle {
    float px, py, pz, pw;
    float vx, vy, vz, vw;
};

struct ComputeParams {
    float deltaTime;
    float bounds;
};

// Push constants for the N-body compute shader (std430 scalar layout).
struct NBodyParams {
    float         deltaTime;
    float         softening;
    std::uint32_t numBodies;
    std::uint32_t _pad = 0;
};

// Push constants for the fractal stress fragment shader.
struct FractalParams {
    float         time;       // animates the colour palette (not the workload)
    float         zoom;
    std::uint32_t maxIter;    // per-pixel iterations (constant work)
    std::uint32_t _pad = 0;
};

// Push constants for the SynthPeak compute shaders. mul/add are runtime values
// so the compiler cannot fold the recurrence to a closed form.
struct PeakParams {
    std::uint32_t iters;
    float         mul = 0.9999f;
    float         add = 0.0001f;
    std::uint32_t _pad = 0;
};

// Push constants for the Render3D vertex shader (column-major, matches GLSL).
struct Render3DParams {
    float viewProj[16];   // perspective * view
    float camRight[4];    // world-space camera right (xyz)
    float camUp[4];       // world-space camera up (xyz)
    float pointSize;      // billboard half-extent in world units
    float _pad[3] = {0, 0, 0};
};

struct BenchmarkConfig {
    bool          vsync              = false;
    bool          benchmarkMode      = false;
    bool          hostMemory         = false;
    bool          particlesOverridden = false;
    bool          headless           = false;    // pure compute, no window/swapchain/present
    Workload      workload           = Workload::Stream;
    float         softening          = 0.01f;     // N-body: avoids 1/0 singularity
    std::uint32_t fractalIter        = kFractalDefaultIter;  // StressFractal per-pixel iterations
    Precision     peakPrecision      = Precision::FP32;       // SynthPeak data type
    std::uint32_t peakIters          = kSynthPeakDefaultIters;// SynthPeak loop passes
    double        maxRunTimeSec      = 15.0;
    double        warmupTimeSec      = 2.0;
    std::uint32_t benchFrames        = 2000;
    std::uint32_t warmupFrames       = 100;
    std::uint32_t particleCount      = kParticleCount;
    std::uint32_t framesInFlight     = kMaxFramesInFlight;  // runtime override
    const char*   difficultyLabel    = "Medium";
    double        captureAtSec       = -1.0;
    std::string   gpuDisplayName;           // if set, overrides deviceName_ for results/RenderDoc
    std::uint32_t vramMB             = 0;   // selected GPU's dedicated VRAM (MB, for results)
    // DXGI adapter LUID for precise GPU selection across factory instances.
    // When both are 0, backends fall back to index-based selection.
    std::int64_t  adapterLuidHigh   = 0;
    std::int64_t  adapterLuidLow    = 0;
};

}  // namespace gpu_bench
