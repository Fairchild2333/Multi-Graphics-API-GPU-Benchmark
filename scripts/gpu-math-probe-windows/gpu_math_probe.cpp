// gpu_math_probe.cpp - The Metal maths probe (scripts/metal-math-probe) as a
// D3D11 compute shader, so the same kernels run on NVIDIA and AMD (2026-10-17).
//
// GPU Burn scores about half as much per TFLOPS on Apple GPUs as on NVIDIA and
// AMD (docs/interpreting-gpu-burn.md). The Metal probe measured how fast the
// M4 Pro computes atan2, sin, cos, exp and pow against its own FMA rate. This
// file runs the same kernels on a Windows GPU: the same expressions, four
// independent float4 chains per thread, 1280 x 720 threads, the same timing
// rule and the same output format. The GPU Burn step kernel is the loop body
// of shaders/gpu_burn.hlsl.
//
// The HLSL is compiled at run time with D3DCompile, cs_5_0 and flags 0, as
// src/dx11_backend.cpp compiles GPU Burn: default optimisation, no IEEE
// strictness. D3D has no precise mode for sin / cos / exp / log, so there is
// one table, to be read against the Metal "fast" one.
//
// Build and run: powershell -File scripts/gpu-math-probe-windows/run.ps1
// By hand: gpu_math_probe.exe --list | --adapter N [--dump-dxbc DIR]

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using Microsoft::WRL::ComPtr;

namespace {

const char* kSource = R"HLSL(
cbuffer ProbeParams : register(b0) {
    float4 pa;
    float4 pb;
    uint iterations;
    uint pad0;
    uint pad1;
    uint pad2;
};

RWStructuredBuffer<float4> outBuf : register(u0);

// Four independent float4 chains per thread (16 lanes), so the rate is set by
// throughput rather than by the latency of one chain. Each step applies EXPR
// to v; every expression keeps its values finite. Same as the Metal probe;
// HLSL has fma for doubles only, so the FMA kernel uses mad.
#define PROBE_KERNEL(NAME, EXPR)                                              \
[numthreads(256, 1, 1)]                                                       \
void NAME(uint3 dtid : SV_DispatchThreadID) {                                 \
    const uint gid = dtid.x;                                                  \
    const float4 a = pa;                                                      \
    const float4 b = pb;                                                      \
    const float4 s = float4(float(gid & 1023u), float((gid >> 10) & 1023u),   \
                            0.5, 0.25) * (1.0 / 1024.0);                      \
    float4 v0 = s + 0.11;                                                     \
    float4 v1 = s + 0.23;                                                     \
    float4 v2 = s + 0.37;                                                     \
    float4 v3 = s + 0.41;                                                     \
    for (uint i = 0; i < iterations; ++i) {                                   \
        { float4 v = v0; v0 = (EXPR); }                                       \
        { float4 v = v1; v1 = (EXPR); }                                       \
        { float4 v = v2; v2 = (EXPR); }                                       \
        { float4 v = v3; v3 = (EXPR); }                                       \
    }                                                                         \
    outBuf[gid] = v0 + v1 + v2 + v3 + a * 0.0;                                \
}

PROBE_KERNEL(k_fma,   mad(v, a, b))
PROBE_KERNEL(k_sin,   sin(v) + b)
PROBE_KERNEL(k_cos,   cos(v) + b)
PROBE_KERNEL(k_atan2, atan2(v, v.yzwx + 1.5) + b)
PROBE_KERNEL(k_exp,   exp(-abs(v)) + b)
PROBE_KERNEL(k_pow,   pow(abs(v) + 0.5, float4(1.65, 1.65, 1.65, 1.65)) * 0.5)
PROBE_KERNEL(k_sqrt,  sqrt(abs(v) + b))
PROBE_KERNEL(k_div,   b / (abs(v) + 1.0))

// Not in the Metal probe: k_fma with a and b written as literals instead of
// read from the constant buffer. On the RTX 5090 this alone moved the FMA rate
// from 74 to 99 TFLOPS (2026-10-17), so the k_fma row is not every GPU's peak.
PROBE_KERNEL(k_fma_literal, mad(v, float4(0.999, 0.998, 0.997, 0.996),
                                   float4(0.25, 0.26, 0.27, 0.28)))

// --- One GPU Burn step, copied from shaders/gpu_burn.hlsl -------------------

static const float kTau    = 6.283185307179586;
static const float kFar    = 6.0;
static const float kHitEps = 0.006;

float2 rotate2(float2 p, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return float2(c * p.x - s * p.y, s * p.x + c * p.y);
}

float plasmaField(float3 p, float phase, out float structureOut) {
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
    float bloom = pow(saturate(petalWave * 0.62 + crownWave * 0.38), 1.65);

    float facet = saturate((abs(direction.x) + abs(direction.y)
                          + abs(direction.z) - 1.0) * 1.25);
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

    structureOut = saturate(crack * 0.62 + fibre * 0.48 + spike * 0.60);
    float crystalRadius = 0.67 + bloom * 0.25 + facet * 0.08
                        + spike * 0.15 + crack * 0.028 + fibre * 0.020;
    return radiusFromCentre - crystalRadius;
}

// One thread per pixel of a 1280 x 720 pass; iterations is maxIter. The loop
// body is GPU Burn's, without the shading after the loop. The set-up before the
// loop is the Metal probe's (pass 0, version 1, phase = pa.x).
[numthreads(256, 1, 1)]
void k_burn_step(uint3 dtid : SV_DispatchThreadID) {
    const uint gid = dtid.x;
    const uint px = gid % 1280u;
    const uint py = (gid / 1280u) % 720u;
    float2 uv = (float2(float(px), float(py)) + 0.5) / float2(1280.0, 720.0) * 2.0 - 1.0;
    uv.x *= 16.0 / 9.0;

    float3 rayOrigin = float3(0.0, 0.0, 2.35);
    float3 rayDir = normalize(float3(uv, -1.75));
    float phase = pa.x;

    uint y = min(py, 719u - py);
    uint checksum = 0xB5297A4Du ^ (px * 0x9E3779B9u) ^ (y * 0x85EBCA6Bu)
                  ^ (1u * 0x27D4EB2Fu);
    float4 core = frac(float4(uv, phase * 0.071, 0.173)
                     + float4(0.13, 0.31, 0.57, 0.83));
    float coreEnergy = 0.0;

    static const float boundRadius = 1.32;
    float sphereB = dot(rayOrigin, rayDir);
    float sphereC = dot(rayOrigin, rayOrigin) - boundRadius * boundRadius;
    float sphereDisc = sphereB * sphereB - sphereC;
    float sphereEntry = max(-sphereB - sqrt(max(sphereDisc, 0.0)), 0.0);
    float t = lerp(0.0, sphereEntry, step(0.0, sphereDisc));
    float hit = 0.0;
    float hitT = kFar;
    float nearestSurface = 10.0;
    float glowAccum = 0.0;

    [loop]
    for (uint i = 0u; i < iterations; ++i) {
        float3 p = rayOrigin + rayDir * t;
        float structure;
        float d = plasmaField(p, phase, structure);
        float ad = abs(d);
        nearestSurface = min(nearestSurface, ad);

        checksum ^= i + 0x9E3779B9u + (checksum << 6u) + (checksum >> 2u);
        checksum = checksum * 1664525u + 1013904223u;
        float jitter = float(checksum & 1023u) * (1.0 / 1024.0);

        core = frac(abs(core * float4(1.6181, 1.4142, 1.7321, 1.3247)
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
        hitT = lerp(hitT, t, newHit);
        hit = max(hit, newHit);

        float marchWeight = (1.0 - hit) * withinFar;
        float stepLength = clamp(ad * (0.58 + coreStep * 0.055), 0.004, 0.28);
        t += marchWeight * stepLength;
    }
    outBuf[gid] = float4(t, glowAccum, coreEnergy,
                         hitT + nearestSurface + float(checksum & 255u));
}
)HLSL";

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
    {"k_fma", "fma", 16},       {"k_sin", "sin", 16},   {"k_cos", "cos", 16},
    {"k_atan2", "atan2", 16},   {"k_exp", "exp", 16},   {"k_pow", "pow", 16},
    {"k_sqrt", "sqrt", 16},     {"k_div", "division", 16},
    {"k_burn_step", "GPU Burn step", 1},
};

// 1280 x 720: the burn-step kernel treats each thread as one pixel.
constexpr UINT kThreads = 1280 * 720;
constexpr UINT kGroup = 256;  // [numthreads] in the shader
static_assert(kThreads % kGroup == 0, "whole thread groups only");

struct Gpu {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Buffer> out;
    ComPtr<ID3D11UnorderedAccessView> outView;
    ComPtr<ID3D11Buffer> params;
    ComPtr<ID3D11Query> disjoint;
    ComPtr<ID3D11Query> start;
    ComPtr<ID3D11Query> end;
};

std::string Narrow(const wchar_t* text) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    std::string result(size > 0 ? size - 1 : 0, '\0');
    if (size > 1) WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::string DriverVersion(IDXGIAdapter1* adapter) {
    LARGE_INTEGER umd{};
    if (FAILED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd))) return "unknown";
    char text[64];
    std::snprintf(text, sizeof(text), "%u.%u.%u.%u", HIWORD(umd.HighPart), LOWORD(umd.HighPart),
                  HIWORD(umd.LowPart), LOWORD(umd.LowPart));
    return text;
}

// Waits for a query; false if the device was lost.
template <typename T>
bool WaitFor(ID3D11DeviceContext* context, ID3D11Query* query, T& data) {
    for (;;) {
        const HRESULT hr = context->GetData(query, &data, sizeof(T), 0);
        if (hr == S_OK) return true;
        if (hr != S_FALSE) return false;
    }
}

// GPU time of one dispatch in seconds, or a negative value on failure.
double RunOnce(Gpu& gpu, ID3D11ComputeShader* shader, const ProbeParams& params) {
    ID3D11DeviceContext* context = gpu.context.Get();
    context->UpdateSubresource(gpu.params.Get(), 0, nullptr, &params, 0, 0);
    for (int attempt = 0; attempt < 3; ++attempt) {
        context->CSSetShader(shader, nullptr, 0);
        context->Begin(gpu.disjoint.Get());
        context->End(gpu.start.Get());
        context->Dispatch(kThreads / kGroup, 1, 1);
        context->End(gpu.end.Get());
        context->End(gpu.disjoint.Get());

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT clock{};
        UINT64 begin = 0, finish = 0;
        if (!WaitFor(context, gpu.disjoint.Get(), clock) ||
            !WaitFor(context, gpu.start.Get(), begin) || !WaitFor(context, gpu.end.Get(), finish))
            return -1.0;
        // A disjoint interval (for example a clock change) cannot be timed: retry.
        if (!clock.Disjoint && clock.Frequency != 0 && finish > begin)
            return double(finish - begin) / double(clock.Frequency);
    }
    return -1.0;
}

// Doubles the loop until one dispatch takes 20 ms, then aims at 50 ms.
// Returns false if a dispatch failed.
bool Calibrate(Gpu& gpu, ID3D11ComputeShader* shader, ProbeParams& params) {
    params.iterations = 8;
    double seconds = RunOnce(gpu, shader, params);
    while (seconds >= 0.0 && seconds < 0.020 && params.iterations < (1u << 22)) {
        params.iterations *= 2;
        seconds = RunOnce(gpu, shader, params);
    }
    if (seconds <= 0.0) return false;
    params.iterations =
        std::max<uint32_t>(1u, static_cast<uint32_t>(params.iterations * 0.050 / seconds));
    return true;
}

double Best(Gpu& gpu, ID3D11ComputeShader* shader, const ProbeParams& params, int runs) {
    double best = 1e30;
    for (int run = 0; run < runs; ++run) {
        const double t = RunOnce(gpu, shader, params);
        if (t > 0.0) best = std::min(best, t);
    }
    return best < 1e29 ? best : -1.0;
}

bool ListAdapters(IDXGIFactory1* factory) {
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        // The same GPU can be listed more than once: a virtual (indirect)
        // display driver that renders on it adds an entry with its own LUID.
        std::printf("ADAPTER\t%u\t%s\tsoftware=%d\tluid=%08lx%08lx\tvram_mb=%llu\tdriver=%s\n", i,
                    Narrow(desc.Description).c_str(),
                    (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? 1 : 0,
                    static_cast<unsigned long>(desc.AdapterLuid.HighPart),
                    static_cast<unsigned long>(desc.AdapterLuid.LowPart),
                    static_cast<unsigned long long>(desc.DedicatedVideoMemory >> 20),
                    DriverVersion(adapter.Get()).c_str());
    }
    return true;
}

void DumpDxbc(const std::string& dir, const char* entry, ID3DBlob* code) {
    ComPtr<ID3DBlob> text;
    if (FAILED(D3DDisassemble(code->GetBufferPointer(), code->GetBufferSize(), 0, nullptr, &text)))
        return;
    const std::string path = dir + "\\" + entry + ".dxbc.txt";
    FILE* file = nullptr;
    if (fopen_s(&file, path.c_str(), "wb") != 0 || !file) return;
    std::fwrite(text->GetBufferPointer(), 1, text->GetBufferSize(), file);
    std::fclose(file);
}

bool Run(IDXGIAdapter1* adapter, UINT index, const std::string& dumpDir) {
    DXGI_ADAPTER_DESC1 desc{};
    adapter->GetDesc1(&desc);
    std::printf("GPU: %s (DXGI adapter %u, driver %s)\n", Narrow(desc.Description).c_str(), index,
                DriverVersion(adapter).c_str());

    Gpu gpu;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    HRESULT hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, levels, 2,
                                   D3D11_SDK_VERSION, &gpu.device, nullptr, &gpu.context);
    if (FAILED(hr)) {
        std::fprintf(stderr, "D3D11CreateDevice failed: 0x%08lx\n", static_cast<unsigned long>(hr));
        return false;
    }

    D3D11_BUFFER_DESC outDesc{};
    outDesc.ByteWidth = kThreads * 16;
    outDesc.Usage = D3D11_USAGE_DEFAULT;
    outDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    outDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    outDesc.StructureByteStride = 16;
    D3D11_BUFFER_DESC paramDesc{};
    paramDesc.ByteWidth = sizeof(ProbeParams);
    paramDesc.Usage = D3D11_USAGE_DEFAULT;
    paramDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_QUERY_DESC disjointDesc{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
    D3D11_QUERY_DESC stampDesc{D3D11_QUERY_TIMESTAMP, 0};
    if (FAILED(gpu.device->CreateBuffer(&outDesc, nullptr, &gpu.out)) ||
        FAILED(gpu.device->CreateUnorderedAccessView(gpu.out.Get(), nullptr, &gpu.outView)) ||
        FAILED(gpu.device->CreateBuffer(&paramDesc, nullptr, &gpu.params)) ||
        FAILED(gpu.device->CreateQuery(&disjointDesc, &gpu.disjoint)) ||
        FAILED(gpu.device->CreateQuery(&stampDesc, &gpu.start)) ||
        FAILED(gpu.device->CreateQuery(&stampDesc, &gpu.end))) {
        std::fprintf(stderr, "resource creation failed\n");
        return false;
    }
    gpu.context->CSSetUnorderedAccessViews(0, 1, gpu.outView.GetAddressOf(), nullptr);
    gpu.context->CSSetConstantBuffers(0, 1, gpu.params.GetAddressOf());

    auto compile = [&](const char* entry, ComPtr<ID3D11ComputeShader>& shader) {
        ComPtr<ID3DBlob> code, errors;
        // Flags 0, as src/dx11_backend.cpp compiles GPU Burn.
        HRESULT result = D3DCompile(kSource, std::strlen(kSource), "gpu_math_probe.hlsl", nullptr,
                                    nullptr, entry, "cs_5_0", 0, 0, &code, &errors);
        if (FAILED(result)) {
            std::fprintf(stderr, "%s: shader compile failed\n%s\n", entry,
                         errors ? static_cast<const char*>(errors->GetBufferPointer()) : "");
            return false;
        }
        if (!dumpDir.empty()) DumpDxbc(dumpDir, entry, code.Get());
        if (FAILED(gpu.device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(),
                                                   nullptr, &shader))) {
            std::fprintf(stderr, "%s: CreateComputeShader failed\n", entry);
            return false;
        }
        return true;
    };
    ComPtr<ID3D11ComputeShader> shaders[std::size(kProbes)];
    for (size_t i = 0; i < std::size(kProbes); ++i)
        if (!compile(kProbes[i].function, shaders[i])) return false;
    ComPtr<ID3D11ComputeShader> literalFma;
    if (!compile("k_fma_literal", literalFma)) return false;

    ProbeParams params{};
    const float a[4] = {0.999f, 0.998f, 0.997f, 0.996f};
    const float b[4] = {0.25f, 0.26f, 0.27f, 0.28f};
    std::memcpy(params.a, a, sizeof(a));
    std::memcpy(params.b, b, sizeof(b));

    // Not in the Metal probe: one second of FMA first, so the clocks have
    // left idle before the rate that every other row is divided by.
    {
        ProbeParams warm = params;
        if (!Calibrate(gpu, shaders[0].Get(), warm)) {
            std::fprintf(stderr, "k_fma: dispatch failed\n");
            return false;
        }
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (std::chrono::steady_clock::now() < until) RunOnce(gpu, shaders[0].Get(), warm);
    }

    std::printf("\nMaths: D3DCompile cs_5_0, flags 0 (as the engine compiles GPU Burn for DX11/DX12)\n");
    std::printf("%-14s %10s %9s %14s %10s\n", "function", "iterations", "GPU ms", "G results/s", "vs fma");

    double fmaRate = 0.0;
    uint32_t fmaIterations = 0;
    for (size_t i = 0; i < std::size(kProbes); ++i) {
        const Probe& probe = kProbes[i];
        if (!Calibrate(gpu, shaders[i].Get(), params)) {
            std::fprintf(stderr, "%s: dispatch failed\n", probe.function);
            return false;
        }
        const double best = Best(gpu, shaders[i].Get(), params, 5);
        if (best <= 0.0) {
            std::fprintf(stderr, "%s: no timed dispatch\n", probe.function);
            return false;
        }
        const double rate = double(kThreads) * probe.lanesPerIteration * params.iterations / best;
        if (std::strcmp(probe.function, "k_fma") == 0) {
            fmaRate = rate;
            fmaIterations = params.iterations;
        }

        if (probe.lanesPerIteration > 1) {
            std::printf("%-14s %10u %9.3f %14.2f %10.3f", probe.label, params.iterations,
                        best * 1e3, rate / 1e9, fmaRate > 0 ? rate / fmaRate : 0.0);
            if (std::strcmp(probe.function, "k_fma") == 0)
                std::printf("   (%.2f FP32 TFLOPS)", 2.0 * rate / 1e12);
            std::printf("\n");
        } else {
            // Pixel-steps per second: what a GPU Burn frame of two such passes
            // would score, without the shading after the loop.
            std::printf("%-14s %10u %9.3f %14.3f %10s   (G pixel-steps/s)\n", probe.label,
                        params.iterations, best * 1e3, rate / 1e9, "-");
        }
        std::printf("MATHPROBE\tmaths=d3d_default\tfunction=%s\titerations=%u\tgroup=%u\tseconds=%.6f\trate=%.6e\n",
                    probe.function, params.iterations, kGroup, best, rate);
        std::fflush(stdout);
    }

    // Not in the Metal probe: FMA once more at the end. If it differs much from
    // the first row, the clocks moved during the run and the ratios are suspect.
    params.iterations = fmaIterations;
    const double again = Best(gpu, shaders[0].Get(), params, 5);
    if (again > 0.0) {
        const double rate = double(kThreads) * 16 * params.iterations / again;
        std::printf("\nCheck: fma again after all rows: %.2f G results/s, %.3f of the first fma row\n",
                    rate / 1e9, fmaRate > 0 ? rate / fmaRate : 0.0);
        std::printf("MATHPROBE\tmaths=d3d_default\tfunction=k_fma_again\titerations=%u\tgroup=%u\tseconds=%.6f\trate=%.6e\n",
                    params.iterations, kGroup, again, rate);
    }

    // Not in the Metal probe: the FMA chain with literal a and b. If this is
    // much faster than the fma row, the fma row is not the GPU's FMA peak and
    // the "vs fma" column overstates the other functions against that peak.
    if (!Calibrate(gpu, literalFma.Get(), params)) {
        std::fprintf(stderr, "k_fma_literal: dispatch failed\n");
        return false;
    }
    const double literal = Best(gpu, literalFma.Get(), params, 5);
    if (literal > 0.0) {
        const double rate = double(kThreads) * 16 * params.iterations / literal;
        std::printf("Check: fma with literal a and b: %.2f G results/s, %.3f of the first fma row"
                    "   (%.2f FP32 TFLOPS)\n",
                    rate / 1e9, fmaRate > 0 ? rate / fmaRate : 0.0, 2.0 * rate / 1e12);
        std::printf("MATHPROBE\tmaths=d3d_default\tfunction=k_fma_literal\titerations=%u\tgroup=%u\tseconds=%.6f\trate=%.6e\n",
                    params.iterations, kGroup, literal, rate);
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    int adapterIndex = -1;
    bool list = false;
    std::string dumpDir;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--list") == 0) {
            list = true;
        } else if (std::strcmp(argv[i], "--adapter") == 0 && i + 1 < argc) {
            adapterIndex = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--dump-dxbc") == 0 && i + 1 < argc) {
            dumpDir = argv[++i];
        } else {
            std::fprintf(stderr, "usage: gpu_math_probe --list | --adapter N [--dump-dxbc DIR]\n");
            return 2;
        }
    }

    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        std::fprintf(stderr, "CreateDXGIFactory1 failed\n");
        return 1;
    }
    if (list || adapterIndex < 0) return ListAdapters(factory.Get()) ? 0 : 1;

    ComPtr<IDXGIAdapter1> adapter;
    if (factory->EnumAdapters1(static_cast<UINT>(adapterIndex), &adapter) == DXGI_ERROR_NOT_FOUND) {
        std::fprintf(stderr, "no DXGI adapter %d (see --list)\n", adapterIndex);
        return 2;
    }
    return Run(adapter.Get(), static_cast<UINT>(adapterIndex), dumpDir) ? 0 : 1;
}
