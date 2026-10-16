// metal_math_probe.mm - How fast an Apple GPU computes the special functions
// GPU Burn spends its time on, against its own FMA rate (2026-10-16).
//
// GPU Burn scores about half as much per TFLOPS on Apple GPUs as on NVIDIA and
// AMD (docs/interpreting-gpu-burn.md). Each raymarch step of it calls atan2,
// sin, cos, exp and pow some twenty times. This probe times each of those
// functions and one GPU Burn step in compute kernels, so their cost can be read
// against the FMA rate of the same GPU.
//
// Added 2026-10-17, after the Windows probe (scripts/gpu-math-probe-windows):
// - `fma (literal)`: the FMA kernel with literal constants. On an RTX 5090 the
//   kernel reading `a` and `b` from the parameter buffer ran 25 % slower, so
//   6.96 TFLOPS may not be the M4 Pro's FMA peak.
// - `Burn fragment`: the same GPU Burn step drawn as a full-screen fragment
//   shader, as GPU Burn runs it. From the 16/64/256-step results, GPU Burn on
//   the M4 Pro runs each step at 67 % of the compute kernel's rate, with almost
//   no fixed cost after the loop; this row shows whether the fragment stage
//   alone accounts for that.
//
// Build and run on a Mac: bash scripts/metal-math-probe/run.sh [fast|precise]

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

const char* kSource = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct ProbeParams {
    float4 a;
    float4 b;
    uint iterations;
    uint pad0;
    uint pad1;
    uint pad2;
};

// Four independent float4 chains per thread (16 lanes), so the rate is set by
// throughput rather than by the latency of one chain. Each step applies EXPR
// to v; every expression keeps its values finite.
#define PROBE_KERNEL(NAME, EXPR)                                              \
kernel void NAME(device float4* out [[buffer(0)]],                            \
                 constant ProbeParams& prm [[buffer(1)]],                     \
                 uint gid [[thread_position_in_grid]]) {                      \
    const float4 a = prm.a;                                                   \
    const float4 b = prm.b;                                                   \
    const float4 s = float4(float(gid & 1023u), float((gid >> 10) & 1023u),   \
                            0.5, 0.25) * (1.0 / 1024.0);                      \
    float4 v0 = s + 0.11;                                                     \
    float4 v1 = s + 0.23;                                                     \
    float4 v2 = s + 0.37;                                                     \
    float4 v3 = s + 0.41;                                                     \
    for (uint i = 0; i < prm.iterations; ++i) {                               \
        { float4 v = v0; v0 = (EXPR); }                                       \
        { float4 v = v1; v1 = (EXPR); }                                       \
        { float4 v = v2; v2 = (EXPR); }                                       \
        { float4 v = v3; v3 = (EXPR); }                                       \
    }                                                                         \
    out[gid] = v0 + v1 + v2 + v3 + a * 0.0;                                   \
}

PROBE_KERNEL(k_fma,   fma(v, a, b))
PROBE_KERNEL(k_sin,   sin(v) + b)
PROBE_KERNEL(k_cos,   cos(v) + b)
PROBE_KERNEL(k_atan2, atan2(v, v.yzwx + 1.5) + b)
PROBE_KERNEL(k_exp,   exp(-abs(v)) + b)
PROBE_KERNEL(k_pow,   pow(abs(v) + 0.5, float4(1.65)) * 0.5)
PROBE_KERNEL(k_sqrt,  sqrt(abs(v) + b))
PROBE_KERNEL(k_div,   b / (abs(v) + 1.0))
// The same values as prm.a and prm.b, written as literals.
PROBE_KERNEL(k_fma_literal, fma(v, float4(0.999, 0.998, 0.997, 0.996),
                                float4(0.25, 0.26, 0.27, 0.28)))

// --- One GPU Burn step, copied from shaders/gpu_burn.metal -----------------

constant float kTau    = 6.283185307179586;
constant float kFar    = 6.0;
constant float kHitEps = 0.006;

float2 rotate2(float2 p, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float plasmaField(float3 p, float phase, thread float& structureOut) {
    p.xz = rotate2(p.xz, phase * 0.61);
    p.xy = rotate2(p.xy, phase * 0.37 + 0.43);
    p.yz = rotate2(p.yz, phase * 0.23 - 0.28);

    float radiusFromCentre = length(p);
    float3 direction = p / max(radiusFromCentre, 1e-5);
    float azimuth = atan2(p.z, p.x);
    float elevation = atan2(p.y, length(p.xz));

    float petalWave = 0.5 + 0.5 * cos(azimuth * 7.0
                                    + sin(elevation * 3.0) * 1.8);
    float crownWave = 0.5 + 0.5 * cos(elevation * 6.0
                                    - azimuth * 2.0
                                    + sin(azimuth * 3.0) * 0.65);
    float bloom = pow(clamp(petalWave * 0.62 + crownWave * 0.38,
                            0.0, 1.0), 1.65);

    float facet = clamp((abs(direction.x) + abs(direction.y)
                       + abs(direction.z) - 1.0) * 1.25, 0.0, 1.0);
    facet *= facet;

    float spike = 1.0 - abs(sin(azimuth * 11.0 - elevation * 9.0
                              + sin(azimuth * 4.0) * 1.4));
    spike *= spike;
    spike *= spike;
    spike *= 0.35 + bloom * 0.65;

    float crack = 1.0 - abs(sin(azimuth * 41.0 + elevation * 59.0
                              + sin((azimuth - elevation) * 7.0) * 2.1));
    crack *= crack;
    crack *= crack;

    float fibre = 1.0 - abs(sin(azimuth * 67.0 - elevation * 37.0
                              + sin(elevation * 13.0) * 1.6));
    fibre *= fibre;
    fibre *= fibre;

    structureOut = clamp(crack * 0.62 + fibre * 0.48 + spike * 0.60,
                         0.0, 1.0);
    float crystalRadius = 0.67 + bloom * 0.25 + facet * 0.08
                        + spike * 0.15 + crack * 0.028 + fibre * 0.020;
    return radiusFromCentre - crystalRadius;
}

// `iterations` raymarch steps for one pixel of a 1280 x 720 pass. The loop body
// is GPU Burn's, without the shading after the loop. Defined below.
float4 BurnSteps(uint px, uint py, float phase, uint iterations);

// One thread per pixel; prm.iterations is maxIter.
kernel void k_burn_step(device float4* out [[buffer(0)]],
                        constant ProbeParams& prm [[buffer(1)]],
                        uint gid [[thread_position_in_grid]]) {
    out[gid] = BurnSteps(gid % 1280u, (gid / 1280u) % 720u, prm.a.x, prm.iterations);
}

// The same steps as a full-screen fragment shader, one fragment per pixel of a
// 1280 x 720 target, as GPU Burn draws each of its passes.
struct FullscreenOut {
    float4 position [[position]];
};

vertex FullscreenOut v_fullscreen(uint vid [[vertex_id]]) {
    FullscreenOut out;
    float2 uv = float2((vid << 1) & 2, vid & 2);
    out.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    return out;
}

fragment float4 f_burn_step(FullscreenOut in [[stage_in]],
                            constant ProbeParams& prm [[buffer(0)]]) {
    const uint2 pixel = uint2(in.position.xy);
    return BurnSteps(pixel.x, pixel.y, prm.a.x, prm.iterations);
}

float4 BurnSteps(uint px, uint py, float phase, uint iterations) {
    float2 uv = (float2(float(px), float(py)) + 0.5) / float2(1280.0, 720.0) * 2.0 - 1.0;
    uv.x *= 16.0 / 9.0;

    float3 rayOrigin = float3(0.0, 0.0, 2.35);
    float3 rayDir = normalize(float3(uv, -1.75));

    uint y = min(py, 719u - py);
    uint checksum = 0xB5297A4Du ^ (px * 0x9E3779B9u) ^ (y * 0x85EBCA6Bu)
                  ^ (1u * 0x27D4EB2Fu);
    float4 core = fract(float4(uv, phase * 0.071, 0.173)
                    + float4(0.13, 0.31, 0.57, 0.83));
    float coreEnergy = 0.0;

    const float boundRadius = 1.32;
    float sphereB = dot(rayOrigin, rayDir);
    float sphereC = dot(rayOrigin, rayOrigin) - boundRadius * boundRadius;
    float sphereDisc = sphereB * sphereB - sphereC;
    float sphereEntry = max(-sphereB - sqrt(max(sphereDisc, 0.0)), 0.0);
    float t = mix(0.0, sphereEntry, step(0.0, sphereDisc));
    float hit = 0.0;
    float hitT = kFar;
    float nearestSurface = 10.0;
    float glowAccum = 0.0;

    for (uint i = 0u; i < iterations; ++i) {
        float3 p = rayOrigin + rayDir * t;
        float structure;
        float d = plasmaField(p, phase, structure);
        float ad = abs(d);
        nearestSurface = min(nearestSurface, ad);

        checksum ^= i + 0x9E3779B9u + (checksum << 6u) + (checksum >> 2u);
        checksum = checksum * 1664525u + 1013904223u;
        float jitter = float(checksum & 1023u) * (1.0 / 1024.0);

        core = fract(abs(core * float4(1.6181, 1.4142, 1.7321, 1.3247)
                       + core.yzwx * 0.719
                       + float4(ad, structure, jitter, t * 0.071)
                       + float4(0.103, 0.217, 0.331, 0.449)));
        core.xy = sin((core.xy + core.zw) * kTau) * 0.5 + 0.5;
        float coreStep = dot(core, float4(0.17, 0.23, 0.29, 0.31));
        coreEnergy += dot(core, core.wzyx);

        glowAccum += exp(-ad * 12.0) * (0.90 + coreStep * 0.18)
                   / (1.0 + t * t * 0.18);

        float withinFar = step(t, kFar);
        float newHit = (1.0 - hit) * (1.0 - step(kHitEps, ad)) * withinFar;
        hitT = mix(hitT, t, newHit);
        hit = max(hit, newHit);

        float marchWeight = (1.0 - hit) * withinFar;
        float stepLength = clamp(ad * (0.58 + coreStep * 0.055), 0.004, 0.28);
        t += marchWeight * stepLength;
    }
    return float4(t, glowAccum, coreEnergy,
                  hitT + nearestSurface + float(checksum & 255u));
}
)MSL";

struct ProbeParams {
    float a[4];
    float b[4];
    uint32_t iterations;
    uint32_t pad[3];
};
static_assert(sizeof(ProbeParams) == 48, "must match ProbeParams in the shader");

struct Probe {
    const char* function;
    const char* label;
    double lanesPerIteration;  // results per thread and loop iteration
};

const Probe kProbes[] = {
    {"k_fma", "fma", 16},       {"k_fma_literal", "fma (literal)", 16},
    {"k_sin", "sin", 16},       {"k_cos", "cos", 16},
    {"k_atan2", "atan2", 16},   {"k_exp", "exp", 16},   {"k_pow", "pow", 16},
    {"k_sqrt", "sqrt", 16},     {"k_div", "division", 16},
    {"k_burn_step", "GPU Burn step", 1},
};

// 1280 x 720: the burn-step kernel treats each thread as one pixel.
constexpr NSUInteger kThreads = 1280 * 720;

void SetMaths(MTLCompileOptions* options, bool fast) {
    if (@available(macOS 15.0, iOS 18.0, *)) {
        // Fast is what src/metal_backend.mm uses for GPU Burn.
        options.mathMode = fast ? MTLMathModeFast : MTLMathModeSafe;
        options.mathFloatingPointFunctions = fast ? MTLMathFloatingPointFunctionsFast
                                                  : MTLMathFloatingPointFunctionsPrecise;
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        options.fastMathEnabled = fast ? YES : NO;
#pragma clang diagnostic pop
    }
}

// GPU time of one dispatch in seconds, or a negative value on failure.
double RunOnce(id<MTLCommandQueue> queue, id<MTLComputePipelineState> pipeline,
               id<MTLBuffer> out, const ProbeParams& params, NSUInteger group) {
    id<MTLCommandBuffer> commands = [queue commandBuffer];
    id<MTLComputeCommandEncoder> encoder = [commands computeCommandEncoder];
    [encoder setComputePipelineState:pipeline];
    [encoder setBuffer:out offset:0 atIndex:0];
    [encoder setBytes:&params length:sizeof(params) atIndex:1];
    [encoder dispatchThreadgroups:MTLSizeMake(kThreads / group, 1, 1)
            threadsPerThreadgroup:MTLSizeMake(group, 1, 1)];
    [encoder endEncoding];
    [commands commit];
    [commands waitUntilCompleted];
    if (commands.status != MTLCommandBufferStatusCompleted) return -1.0;
    return commands.GPUEndTime - commands.GPUStartTime;
}

// GPU time of one full-screen draw in seconds, or a negative value on failure.
double DrawOnce(id<MTLCommandQueue> queue, id<MTLRenderPipelineState> pipeline,
                id<MTLTexture> target, const ProbeParams& params) {
    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = target;
    pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLCommandBuffer> commands = [queue commandBuffer];
    id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];
    [encoder setRenderPipelineState:pipeline];
    [encoder setFragmentBytes:&params length:sizeof(params) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding];
    [commands commit];
    [commands waitUntilCompleted];
    if (commands.status != MTLCommandBufferStatusCompleted) return -1.0;
    return commands.GPUEndTime - commands.GPUStartTime;
}

// Grows the loop until one run takes 20 ms, then aims at 50 ms, and returns the
// shortest GPU time of five runs; negative when a run fails.
template <class Run>
double TimeBest(ProbeParams& params, Run run) {
    params.iterations = 8;
    double seconds = run(params);
    while (seconds >= 0.0 && seconds < 0.020 && params.iterations < (1u << 22)) {
        params.iterations *= 2;
        seconds = run(params);
    }
    if (seconds <= 0.0) return -1.0;
    params.iterations = std::max<uint32_t>(
        1u, static_cast<uint32_t>(params.iterations * 0.050 / seconds));
    double best = 1e30;
    for (int i = 0; i < 5; ++i) {
        const double t = run(params);
        if (t > 0.0) best = std::min(best, t);
    }
    return best;
}

bool RunMode(id<MTLDevice> device, bool fast) {
    NSError* error = nil;
    MTLCompileOptions* options = [MTLCompileOptions new];
    SetMaths(options, fast);
    id<MTLLibrary> library = [device newLibraryWithSource:@(kSource) options:options error:&error];
    if (!library) {
        std::fprintf(stderr, "shader compile failed: %s\n", error.localizedDescription.UTF8String);
        return false;
    }
    id<MTLCommandQueue> queue = [device newCommandQueue];
    id<MTLBuffer> out = [device newBufferWithLength:kThreads * 16 options:MTLResourceStorageModePrivate];

    ProbeParams params{};
    const float a[4] = {0.999f, 0.998f, 0.997f, 0.996f};
    const float b[4] = {0.25f, 0.26f, 0.27f, 0.28f};
    std::memcpy(params.a, a, sizeof(a));
    std::memcpy(params.b, b, sizeof(b));

    std::printf("\nMaths: %s\n", fast ? "fast (as the engine compiles GPU Burn)" : "precise");
    std::printf("%-14s %10s %9s %14s %10s\n", "function", "iterations", "GPU ms", "G results/s", "vs fma");

    double fmaRate = 0.0;
    double computeStepRate = 0.0;
    for (const Probe& probe : kProbes) {
        id<MTLFunction> function = [library newFunctionWithName:@(probe.function)];
        id<MTLComputePipelineState> pipeline =
            function ? [device newComputePipelineStateWithFunction:function error:&error] : nil;
        if (!pipeline) {
            std::fprintf(stderr, "%s: no pipeline (%s)\n", probe.function,
                         error ? error.localizedDescription.UTF8String : "function missing");
            return false;
        }
        NSUInteger group = 256;
        while (group > pipeline.maxTotalThreadsPerThreadgroup) group /= 2;

        const double best = TimeBest(params, [&](const ProbeParams& p) {
            return RunOnce(queue, pipeline, out, p, group);
        });
        if (best <= 0.0) {
            std::fprintf(stderr, "%s: dispatch failed\n", probe.function);
            return false;
        }
        const double rate = double(kThreads) * probe.lanesPerIteration * params.iterations / best;
        if (std::strcmp(probe.function, "k_fma") == 0) fmaRate = rate;
        if (std::strcmp(probe.function, "k_burn_step") == 0) computeStepRate = rate;

        if (probe.lanesPerIteration > 1) {
            std::printf("%-14s %10u %9.3f %14.2f %10.3f", probe.label, params.iterations,
                        best * 1e3, rate / 1e9, fmaRate > 0 ? rate / fmaRate : 0.0);
            if (std::strncmp(probe.function, "k_fma", 5) == 0)
                std::printf("   (%.2f FP32 TFLOPS)", 2.0 * rate / 1e12);
            std::printf("\n");
        } else {
            // Pixel-steps per second: what a GPU Burn frame of two such passes
            // would score, without the shading after the loop.
            std::printf("%-14s %10u %9.3f %14.3f %10s   (G pixel-steps/s)\n", probe.label,
                        params.iterations, best * 1e3, rate / 1e9, "-");
        }
        std::printf("MATHPROBE\tmaths=%s\tfunction=%s\titerations=%u\tgroup=%lu\tseconds=%.6f\trate=%.6e\n",
                    fast ? "fast" : "precise", probe.function, params.iterations,
                    static_cast<unsigned long>(group), best, rate);
    }

    // The same step drawn as a full-screen fragment shader into a 1280 x 720
    // target of the format GPU Burn presents (BGRA8).
    MTLRenderPipelineDescriptor* descriptor = [MTLRenderPipelineDescriptor new];
    descriptor.vertexFunction = [library newFunctionWithName:@"v_fullscreen"];
    descriptor.fragmentFunction = [library newFunctionWithName:@"f_burn_step"];
    descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    id<MTLRenderPipelineState> render =
        [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    if (!render) {
        std::fprintf(stderr, "f_burn_step: no pipeline (%s)\n",
                     error ? error.localizedDescription.UTF8String : "function missing");
        return false;
    }
    MTLTextureDescriptor* targetDescriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                           width:1280 height:720 mipmapped:NO];
    targetDescriptor.usage = MTLTextureUsageRenderTarget;
    targetDescriptor.storageMode = MTLStorageModePrivate;
    id<MTLTexture> target = [device newTextureWithDescriptor:targetDescriptor];
    const double drawBest = TimeBest(params, [&](const ProbeParams& p) {
        return DrawOnce(queue, render, target, p);
    });
    if (drawBest <= 0.0) {
        std::fprintf(stderr, "f_burn_step: draw failed\n");
        return false;
    }
    const double drawRate = double(kThreads) * params.iterations / drawBest;
    std::printf("%-14s %10u %9.3f %14.3f %10s   (G pixel-steps/s; %.3f of the compute step)\n",
                "Burn fragment", params.iterations, drawBest * 1e3, drawRate / 1e9, "-",
                computeStepRate > 0 ? drawRate / computeStepRate : 0.0);
    std::printf("MATHPROBE\tmaths=%s\tfunction=f_burn_step\titerations=%u\tgroup=0\tseconds=%.6f\trate=%.6e\n",
                fast ? "fast" : "precise", params.iterations, drawBest, drawRate);
    return true;
}

}  // namespace

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            std::fprintf(stderr, "no Metal device\n");
            return 1;
        }
        std::printf("GPU: %s\n", device.name.UTF8String);
        const char* which = argc > 1 ? argv[1] : "both";
        bool ok = true;
        if (std::strcmp(which, "precise") != 0) ok = RunMode(device, true) && ok;
        if (std::strcmp(which, "fast") != 0) ok = RunMode(device, false) && ok;
        return ok ? 0 : 1;
    }
}
