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
// Fluid        : legacy 2D Eulerian incompressible fluid (Stam stable fluids).
//                The stable id remains "fluid" so historical commands and
//                results keep working; it is not the planned 3D liquid test.
//                Five
//                compute passes per frame: advect (semi-Lagrangian), divergence,
//                N Jacobi pressure iterations, gradient-subtract, then a
//                fullscreen render of the dye field. Stresses compute with
//                irregular memory access (neighbour sampling), iteration
//                cache behaviour, and a real multi-pass pipeline — none of
//                which the other workloads exercise. Reports GCell/s.
// CinematicLiquidV1: preserved first 3D MLS-MPM dam-break score contract.
// CinematicLiquid: v2 pool scene.  It keeps MLS-MPM but adds GPU rigid-body
//                state/impulse coupling, buoyant toys, a sinking sphere and a
//                propeller wake.  V1 remains separately runnable so the new
//                scene cannot silently perturb historical results.
// GpuStressV1  : versioned product stress workload. Four fullscreen draws run
//                the same deterministic, register-resident fragment ALU/SFU
//                recurrence. A uint checksum is encoded into the visible output
//                and the float recurrence also contributes to every pixel, so
//                neither path is dead-code removable. Stable result id:
//                "gpu_stress"; changing the algorithm requires a new workload.
// GpuBurnV1    : Original visual GraphicsBurn. A rotating Plasma Bloom core
//                with dense crystalline spikes is evaluated with a fixed-count
//                fullscreen raymarch. This is deliberately a separate score
//                contract from gpu_stress_v1 and does not use particle data.
enum class Workload {
    Stream,
    NBody,
    StressFractal,
    SynthPeak,
    Render3D,
    Volumetric,
    Fluid,
    GpuStressV1,
    GpuBurnV1,
    CinematicLiquidV1,
    CinematicLiquid,
};

inline const char* workloadId(Workload workload) {
    switch (workload) {
        case Workload::Stream:        return "stream";
        case Workload::NBody:         return "nbody";
        case Workload::GpuStressV1:   return "gpu_stress";
        case Workload::GpuBurnV1:     return "gpu_burn";
        case Workload::StressFractal: return "stress";
        case Workload::SynthPeak:     return "synthpeak";
        case Workload::Render3D:      return "render3d";
        case Workload::Volumetric:    return "volumetric";
        case Workload::Fluid:         return "fluid";
        // Preserve the public workload key used by formal v1 results.  The
        // runnable contracts are separated by workloadVersion, while the CLI
        // accepts cinematic_liquid_v1 to select the legacy implementation.
        case Workload::CinematicLiquidV1:return "cinematic_liquid";
        case Workload::CinematicLiquid:return "cinematic_liquid";
    }
    return "unknown";
}

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
//                        gpu_burn / gpu_stress / legacy stress / volumetric
//   InstancedBillboard : 1 compute + 1 instanced quad render with depth test
//                        render3d
//   MultiPassCompute   : N compute passes + 1 fullscreen render
//                        fluid (legacy 2D), cinematic_liquid (3D MLS-MPM)
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
        case Workload::GpuStressV1:   return WorkloadShape::FullscreenTriangle;
        case Workload::GpuBurnV1:     return WorkloadShape::FullscreenTriangle;
        case Workload::Volumetric:    return WorkloadShape::FullscreenTriangle;
        case Workload::Render3D:      return WorkloadShape::InstancedBillboard;
        case Workload::Fluid:         return WorkloadShape::MultiPassCompute;
        case Workload::CinematicLiquidV1:return WorkloadShape::MultiPassCompute;
        case Workload::CinematicLiquid:return WorkloadShape::MultiPassCompute;
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

// GPU Stress v1 deliberately uses several short fullscreen draws instead of
// one very long draw. This keeps a frame pre-emptible at draw boundaries while
// sustaining high fragment ALU/SFU occupancy through continuous overdraw.
// The shader algorithm and these defaults are part of the gpu_stress v1 score
// contract; future algorithm changes must use a new workload/version.
// Auto mode starts deliberately low, then scales once during the first warmup
// timing window to target kGpuStressV1TargetFrameMs. This keeps old/slow GPUs
// responsive while allowing very fast GPUs to reach sustained occupancy.
constexpr std::uint32_t kGpuStressV1DefaultIter   = 32;
constexpr std::uint32_t kGpuStressV1MaxIter       = 2048;
constexpr std::uint32_t kGpuStressV1DrawsPerFrame = 4;
constexpr std::uint32_t kGpuStressV1ShaderVersion = 1;
constexpr double        kGpuStressV1TargetFrameMs = 8.0;

// GPU Burn v1 is the first original visual burn workload. Every fragment runs
// exactly maxIter Plasma Bloom field samples in each draw, allowing a versioned
// Gpix-step/s score. Two short draws retain a pre-emption point without hiding
// the final animated core. Auto-tune always starts at the visually valid
// 16-step floor, so slow GPUs/WARP never see a surprise long first draw. Auto
// mode may reach the 2048-step ceiling only after measuring that safe probe.
// Unprobed fixed mode is intentionally capped at 32 steps: the 16-step matrix
// already measured ~209 ms/frame on WARP, so exposing 2048 as a fixed public
// knob could create multi-second draws or trip a hardware watchdog.
constexpr std::uint32_t kGpuBurnV1DefaultIter   = 16;
constexpr std::uint32_t kGpuBurnV1MaxIter       = 2048;
constexpr std::uint32_t kGpuBurnV1MaxFixedIter  = 32;
constexpr std::uint32_t kGpuBurnV1DrawsPerFrame = 2;
constexpr std::uint32_t kGpuBurnV1ShaderVersion = 1;
constexpr double        kGpuBurnV1TargetFrameMs = 14.0;

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

// Cinematic Liquid v1's fixed quality contract. These values affect both
// simulation work and density-raymarch cost, so changing any of them requires
// a new workload version rather than silently perturbing historical scores.
constexpr std::uint32_t kCinematicLiquidGridX = 96;
constexpr std::uint32_t kCinematicLiquidGridY = 56;
constexpr std::uint32_t kCinematicLiquidGridZ = 64;
constexpr std::uint32_t kCinematicLiquidSubsteps = 10;
constexpr std::uint32_t kCinematicLiquidRaySteps = 160;
constexpr std::uint32_t kCinematicLiquidShaderVersion = 1;
constexpr float         kCinematicLiquidDx = 0.035f;

// Cinematic Liquid v2's independent fixed-quality contract.  Unlike v1, the
// larger pool includes a seven-body duck-family scene and two-way fluid impulses.
constexpr std::uint32_t kCinematicLiquidV2GridX = 128;
constexpr std::uint32_t kCinematicLiquidV2GridY = 64;
constexpr std::uint32_t kCinematicLiquidV2GridZ = 96;
constexpr std::uint32_t kCinematicLiquidV2Substeps = 10;
constexpr std::uint32_t kCinematicLiquidV2RaySteps = 352;
constexpr std::uint32_t kCinematicLiquidV2ShaderVersion = 9;
// 0 mother duck, 1 play ball, 2 anchored boat, 3 sink sphere, 4-6 ducklings.
// The sink sphere's release choreography is keyed to lane 3 in the rigid
// integrate shader, so new bodies must only ever be appended after index 3.
constexpr std::uint32_t kCinematicLiquidV2BodyCount = 7;
constexpr float         kCinematicLiquidV2Dx = 0.040f;
constexpr std::uint32_t kCinematicLiquidV2SurfaceX = 128;
constexpr std::uint32_t kCinematicLiquidV2SurfaceY = 64;
constexpr std::uint32_t kCinematicLiquidV2SurfaceZ = 96;

// Experimental SPH vertical slice (`--liquid-solver sph`).  A faithful port
// of the Lague-style dual-density SPH used by MIT jeantimex/fluid: the solver
// runs in the reference's own units (bounds ~24x12x18, h = 0.2, dt = 1/60,
// g = -10, targetDensity 630, pressureMultiplier 288, nearPressure 2.16,
// viscosity 0.01) so its published tuning applies verbatim; an affine map
// (length x kCinematicLiquidSphWorldScale, dt -> world dt chosen so sim
// gravity lands on -9.81 m/s^2) presents the result in the shared pool scene.
// Its results form an independent version group and must never rank beside
// the MLS-MPM versions.
constexpr std::uint32_t kCinematicLiquidSphSubsteps = 2;
constexpr std::uint32_t kCinematicLiquidSphShaderVersion = 1;
constexpr float         kCinematicLiquidSphSmoothingRadius = 0.2f;
// The reference clamps frameTime = min(dt * timeScale, 1/maxTimestepFPS) and
// divides by iterationsPerFrame = 2, so each solver tick is 1/120 s.
constexpr float         kCinematicLiquidSphDtSim = 1.0f / 120.0f;
constexpr float         kCinematicLiquidSphGravitySim = -10.0f;
constexpr float         kCinematicLiquidSphCollisionDamping = 0.95f;
constexpr float         kCinematicLiquidSphTargetDensity = 630.0f;
constexpr float         kCinematicLiquidSphPressureMultiplier = 288.0f;
constexpr float         kCinematicLiquidSphNearPressureMultiplier = 2.16f;
constexpr float         kCinematicLiquidSphViscosityStrength = 0.01f;
constexpr float         kCinematicLiquidSphSpawnSpacing = 0.1186f; // 600/unit^3
// worldScale maps the 24-unit sim x-extent exactly onto the 5.12 m pool.
constexpr float         kCinematicLiquidSphWorldScale = 5.12f / 24.0f;

inline bool isCinematicLiquidWorkload(Workload workload) {
    return workload == Workload::CinematicLiquid ||
           workload == Workload::CinematicLiquidV1;
}

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

// Dedicated GPU Stress v1 shader constants. Keeping this layout and shader
// separate from FractalParams prevents the new shader's register allocation
// from perturbing legacy `stress` performance/results.
struct GpuStressV1Params {
    float         passIndex;  // deterministic salt; never wall-clock time
    float         loadScale;
    std::uint32_t maxIter;
    std::uint32_t version = kGpuStressV1ShaderVersion;
};
static_assert(sizeof(GpuStressV1Params) == 16,
              "GPU Stress v1 constants must match GLSL std140/HLSL layout");

// Dedicated GPU Burn v1 constants. The 16-byte ABI matches push constants,
// HLSL cbuffer packing and the OpenGL std140 UBO, but its semantics and score
// contract remain independent from GpuStressV1Params.
struct GpuBurnV1Params {
    float         time;       // deterministic animation clock for the Plasma Bloom
    float         passIndex;  // overdraw salt within the current frame
    std::uint32_t maxIter;    // exact raymarch/fur samples per pixel/draw
    std::uint32_t version = kGpuBurnV1ShaderVersion;
};
static_assert(sizeof(GpuBurnV1Params) == 16,
              "GPU Burn v1 constants must match GLSL std140/HLSL layout");

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
    float         time;
};
static_assert(sizeof(FluidParams) == 16,
              "Fluid compute constants must match the GLSL push-constant layout");

// Push constants for the cinematic fluid renderer. Keep the ABI at 16 bytes:
// Vulkan guarantees enough push-constant space and the layout is shared by
// every build configuration.
struct FluidRenderParams {
    std::uint32_t gridSize;
    float         time;
    float         exposure;
    std::uint32_t version;
};
static_assert(sizeof(FluidRenderParams) == 16,
              "Fluid render constants must match the GLSL push-constant layout");

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
    bool          guiWorker          = false;    // internal: isolated WinUI child process
    Workload      workload           = Workload::Stream;
    float         softening          = 0.01f;     // N-body: avoids 1/0 singularity
    std::uint32_t fractalIter        = kFractalDefaultIter;  // StressFractal per-pixel iterations
    std::uint32_t gpuStressIter      = kGpuStressV1DefaultIter; // GPU Stress v1 iterations per pixel/draw
    bool          gpuStressAutoTune  = true;  // disabled by an explicit --iter
    std::uint32_t gpuBurnIter        = kGpuBurnV1DefaultIter; // GPU Burn v1 samples per pixel/draw
    bool          gpuBurnAutoTune    = true;  // disabled by an explicit --iter
    std::uint32_t volumetricSteps    = kVolumetricDefaultSteps; // Volumetric per-pixel ray samples
    std::uint32_t fluidGridSize      = kFluidDefaultGridSize;   // Fluid: 2D grid side length
    std::uint32_t fluidJacobiIters   = kFluidDefaultJacobiIters;// Fluid: pressure projection iterations
    // Cinematic Liquid v2 solver selection (`--liquid-solver sph`).  The SPH
    // vertical slice is an experimental separate version group; default stays
    // the shipping MLS-MPM contract.
    bool          liquidSolverSph    = false;
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
