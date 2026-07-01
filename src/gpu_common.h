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
// Volumetric   : fullscreen raymarch of a procedural 3D noise field (fbm +
//                domain warp) with a fixed step count. Stresses fragment ALU
//                (procedural noise + density integration) and volume fill —
//                fundamentally different from StressFractal's 2D pixel loop
//                because each pixel walks a 3D ray with N samples. Headless-
//                incapable (graphics-only).
// Fluid        : 2D Eulerian incompressible fluid (Stam stable fluids). Five
//                compute passes per frame: advect (semi-Lagrangian), divergence,
//                N Jacobi pressure iterations, gradient-subtract, then a
//                fullscreen render of the dye field. Stresses compute with
//                irregular memory access (neighbour sampling), iteration
//                cache behaviour, and a real multi-pass pipeline — none of
//                which the other workloads exercise. Reports GCell/s.
enum class Workload { Stream, NBody, StressFractal, SynthPeak, Render3D, Volumetric, Fluid };

// ---- WorkloadShape (pipeline shape classification) ----------------------
// Each workload maps to one of four pipeline "shapes" — the structural
// pattern of how the backend builds pipelines and records commands. The shape
// classification is the single source of truth that backends switch on; the
// per-workload details (shader file names, push constant contents, dispatch
// parameters) are queried via the helpers below.
//
//   ParticleCompute    : 1 compute dispatch (N particles) + 1 point-sprite render
//                        stream / nbody / synthpeak
//   FullscreenTriangle : 1 fullscreen tri + heavy fragment shader, no compute
//                        stress / volumetric
//   InstancedBillboard : 1 compute + 1 instanced quad render with depth test
//                        render3d
//   MultiPassCompute   : N compute passes (ping-pong buffers) + 1 fullscreen render
//                        fluid
enum class WorkloadShape {
    ParticleCompute,
    FullscreenTriangle,
    InstancedBillboard,
    MultiPassCompute,
};

inline WorkloadShape shapeOfWorkload(Workload w) {
    switch (w) {
        case Workload::Stream:        return WorkloadShape::ParticleCompute;
        case Workload::NBody:         return WorkloadShape::ParticleCompute;
        case Workload::SynthPeak:     return WorkloadShape::ParticleCompute;
        case Workload::StressFractal: return WorkloadShape::FullscreenTriangle;
        case Workload::Volumetric:    return WorkloadShape::FullscreenTriangle;
        case Workload::Render3D:      return WorkloadShape::InstancedBillboard;
        case Workload::Fluid:         return WorkloadShape::MultiPassCompute;
    }
    return WorkloadShape::ParticleCompute;  // unreachable
}

// Returns true for workloads that run a fragment-only pass (no compute, no
// particle buffer use). Equivalent to `shapeOfWorkload(w) == FullscreenTriangle`
// but clearer at call sites and stable if more fragment-only workloads are added.
inline bool isFragmentOnlyWorkload(Workload w) {
    return shapeOfWorkload(w) == WorkloadShape::FullscreenTriangle;
}

// Returns true for workloads that need a depth attachment (render3d currently).
inline bool needsDepthAttachment(Workload w) {
    return shapeOfWorkload(w) == WorkloadShape::InstancedBillboard;
}

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

// Volumetric raymarch default per-pixel step count. Each pixel walks a 3D ray
// with exactly this many samples (no early-out) so the work scales linearly
// with pixels x steps. Tune with --steps. Mobile GPUs may need ~48; desktop
// discrete GPUs saturate around 128-256.
constexpr std::uint32_t kVolumetricDefaultSteps = 96;

// Fluid simulation defaults. The grid is 2D; a power-of-two side keeps the
// dispatch math simple. Jacobi iteration count is the sole knob: more iters
// = tighter incompressibility but linearly more work. The score formula
// `gridW * gridH * (4 + jacobiIters) / computeSec` accounts for the 4 fixed
// passes (advect + divergence + jacobi setup + subtract) plus N Jacobi iters.
constexpr std::uint32_t kFluidDefaultGridSize  = 256;
constexpr std::uint32_t kFluidDefaultJacobiIters = 30;

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

// Push constants for the Volumetric raymarch fragment shader. `steps` is the
// fixed per-pixel sample count (constant work, drives the score formula).
struct VolumetricParams {
    float         time;       // animates the noise field (not the workload)
    float         stepSize;   // world-space ray step length
    std::uint32_t steps;      // per-pixel ray samples (constant work)
    std::uint32_t _pad = 0;
};

// Push constants for the Fluid compute passes (advect/divergence/jacobi/
// subtract all share this layout; each pass uses the fields it needs).
struct FluidParams {
    float         dt;
    float         dx;
    std::uint32_t gridSize;
    std::uint32_t _pad = 0;
};

// Push constants for the Fluid render fragment shader (just the grid size,
// so it can map a screen fragment to a simulation cell).
struct FluidRenderParams {
    std::uint32_t gridSize;
    std::uint32_t _pad[3] = {0, 0, 0};
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
    std::uint32_t volumetricSteps    = kVolumetricDefaultSteps; // Volumetric per-pixel ray samples
    std::uint32_t fluidGridSize      = kFluidDefaultGridSize;   // Fluid: 2D grid side length
    std::uint32_t fluidJacobiIters   = kFluidDefaultJacobiIters;// Fluid: pressure projection iterations
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
