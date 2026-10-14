#include "gpu_cache_probe.h"
#include "gpu_engine.h"
#include "benchmark_results.h"
#import <Metal/Metal.h>
#import <TargetConditionals.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace gpu_bench {
namespace {
constexpr std::uint32_t kStride = 128;
constexpr std::uint32_t kLanes = 4096;
constexpr const char* kSource = R"METAL(
#include <metal_stdlib>
using namespace metal;
struct Params { uint steps; uint lanes; };
kernel void chase(volatile device const uint* links [[buffer(0)]],
                  constant Params& p [[buffer(1)]],
                  device const uint* starts [[buffer(2)]],
                  device uint2* sink [[buffer(3)]], uint tid [[thread_position_in_grid]]) {
    if (tid >= p.lanes) return;
    uint idx = starts[tid], sum = 0;
    for (uint i = 0; i < p.steps; ++i) { idx = links[idx]; sum += idx; }
    sink[tid] = uint2(idx, sum);
}
kernel void eviction(device const uint4* data [[buffer(0)]],
                     constant uint& count [[buffer(1)]],
                     device uint* sink [[buffer(2)]], uint tid [[thread_position_in_grid]]) {
    uint4 sum = 0;
    for (uint i = tid; i < count; i += 65536) sum += data[i];
    sink[tid] = sum.x + sum.y + sum.z + sum.w;
}
)METAL";
struct Params { std::uint32_t steps, lanes; };
struct Sample { double ns = 0, gbps = 0, ms = 0; };
struct Point { std::string condition; std::uint64_t bytes; double value, minimum, maximum; };
volatile std::uint64_t cpuSink = 0;

double Median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}
void Check(id<MTLCommandBuffer> cb) {
    [cb commit]; [cb waitUntilCompleted];
    if (cb.status != MTLCommandBufferStatusCompleted)
        throw std::runtime_error(cb.error ? cb.error.localizedDescription.UTF8String : "Metal command failed");
}
void CpuTouch(id<MTLBuffer> buffer) {
    volatile const std::uint32_t* words = static_cast<const std::uint32_t*>(buffer.contents);
    std::uint64_t sum = 0;
    for (std::uint64_t i = 0; i < buffer.length / 4; i += kStride / 4) sum += words[i];
    cpuSink = sum;
}
}

std::uint32_t GpuCacheDefaultParallelSteps() {
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        return device && ![device supportsFamily:MTLGPUFamilyApple6] ? 512u : 0u;
    }
#else
    return 0;
#endif
}

bool GpuCacheProbeAvailable(std::uint32_t gpuIndex) {
    @autoreleasepool {
#if TARGET_OS_OSX
        NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
        return gpuIndex < devices.count && devices[gpuIndex].hasUnifiedMemory;
#else
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        return gpuIndex == 0 && device && device.hasUnifiedMemory;
#endif
    }
}

int RunGpuCacheProbeCli(const GpuCacheProbeConfig& config, bool saveResult) {
    if (config.maxMiB < 1 || config.maxMiB > 512 || config.steps < 4096 ||
        config.steps > 1048576 || config.repeats < 3 || config.repeats > 15 ||
        (config.parallelSteps != 0 && (config.parallelSteps < 256 || config.parallelSteps > 131072))) {
        std::cerr << "GCACHE_ERROR\tmessage=invalid_bounds\n";
        return 2;
    }
    try { @autoreleasepool {
        const auto start = std::chrono::steady_clock::now();
        const auto parallelSteps = config.parallelSteps ? config.parallelSteps : std::max(4096u, config.steps / 8);
        id<MTLDevice> device = nil;
#if TARGET_OS_OSX
        NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
        if (config.gpuIndex >= devices.count) throw std::runtime_error("Metal device index unavailable");
        device = devices[config.gpuIndex];
#else
        device = MTLCreateSystemDefaultDevice();
#endif
        if (!device || !device.hasUnifiedMemory)
            throw std::runtime_error("CPU/GPU warming diagnostic requires a unified-memory Metal device");
        NSError* error = nil;
        MTLCompileOptions* options = [MTLCompileOptions new];
        options.fastMathEnabled = NO;
        id<MTLLibrary> library = [device newLibraryWithSource:@(kSource) options:options error:&error];
        if (!library) throw std::runtime_error(error.localizedDescription.UTF8String);
        id<MTLComputePipelineState> chase = [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"chase"] error:&error];
        id<MTLComputePipelineState> evict = [device newComputePipelineStateWithFunction:[library newFunctionWithName:@"eviction"] error:&error];
        if (!chase || !evict) throw std::runtime_error("pipeline creation failed");
        id<MTLCommandQueue> queue = [device newCommandQueue];
        const std::uint64_t evictionBytes = 256ull << 20, cpuEvictionBytes = 128ull << 20;
        const std::uint64_t maximum = std::uint64_t(config.maxMiB) << 20;
        if (maximum > device.maxBufferLength || evictionBytes > device.maxBufferLength ||
            (device.recommendedMaxWorkingSetSize > 0 && maximum + evictionBytes + cpuEvictionBytes > device.recommendedMaxWorkingSetSize / 2))
            throw std::runtime_error("insufficient memory budget for cache eviction controls");
        id<MTLBuffer> eviction = [device newBufferWithLength:evictionBytes options:MTLResourceStorageModeShared];
        id<MTLBuffer> cpuEviction = [device newBufferWithLength:cpuEvictionBytes options:MTLResourceStorageModeShared];
        id<MTLBuffer> starts = [device newBufferWithLength:kLanes * 4 options:MTLResourceStorageModeShared];
        id<MTLBuffer> sink = [device newBufferWithLength:65536 * 8 options:MTLResourceStorageModeShared];
        if (!queue || !eviction || !cpuEviction || !starts || !sink)
            throw std::runtime_error("control buffer allocation failed");
        // Commit physical pages before any measurement. Every eviction load
        // contributes to a visible sink, so zero-filled buffers cannot erase it.
        std::memset(eviction.contents, 0x35, evictionBytes);
        std::memset(cpuEviction.contents, 0x67, cpuEvictionBytes);
        std::vector<Point> points;
        std::string counterSets;
        for (id<MTLCounterSet> set in device.counterSets) {
            if (!counterSets.empty()) counterSets += ',';
            counterSets += set.name.UTF8String;
        }
        std::cout << "GCACHE_META\tdevice=" << device.name.UTF8String
                  << "\tmethod=random_dependent_uint32_cycle\tstride_bytes=" << kStride
                  << "\tsteps=" << config.steps << "\trepeats=" << config.repeats
                  << "\tparallel_lanes=" << kLanes << "\teviction_bytes=" << evictionBytes
                  << "\tparallel_steps=" << parallelSteps
                  << "\tcounter_sets=" << counterSets
                  << "\tcache_attribution=unverified\teviction_verified=0\n" << std::flush;

        const std::array<std::string, 4> conditions{"gpu_warm", "eviction_pass", "cpu_warm", "parallel_gpu_warm"};
        for (std::uint64_t base = 4096; base <= maximum; base *= 2) {
          for (const auto bytes : {base, base + base / 2}) {
            if (bytes > maximum) continue;
            if (StopRequested()) return 4;
            id<MTLBuffer> links = [device newBufferWithLength:bytes options:MTLResourceStorageModeShared];
            if (!links) throw std::runtime_error("chain allocation failed");
            const auto nodes = static_cast<std::uint32_t>(bytes / kStride);
            std::vector<std::uint32_t> order(nodes);
            std::iota(order.begin(), order.end(), 0);
            std::mt19937 rng(0x4d345052u ^ nodes);
            std::shuffle(order.begin(), order.end(), rng);
            auto* words = static_cast<std::uint32_t*>(links.contents);
            std::memset(words, 0, bytes);
            std::vector<std::uint64_t> prefix(nodes + 1, 0);
            for (std::uint32_t i = 0; i < nodes; ++i) {
                words[order[i] * (kStride / 4)] = order[(i + 1) % nodes] * (kStride / 4);
                prefix[i + 1] = prefix[i] + std::uint64_t(order[i]) * (kStride / 4);
            }
            auto* seed = static_cast<std::uint32_t*>(starts.contents);
            for (std::uint32_t lane = 0; lane < kLanes; ++lane)
                seed[lane] = order[(std::uint64_t(lane) * nodes / kLanes) % nodes] * (kStride / 4);
            std::uint32_t warmPosition = 0;
            auto expected = [&](std::uint32_t lane, std::uint32_t steps) {
                const std::uint64_t position = (std::uint64_t(lane) * nodes / kLanes + warmPosition) % nodes;
                const auto first = (position + 1) % nodes;
                const auto remainder = steps % nodes;
                const auto last = first + remainder;
                const auto sum = (steps / nodes) * prefix.back() +
                    (last <= nodes ? prefix[last] - prefix[first] : prefix.back() - prefix[first] + prefix[last % nodes]);
                return std::array<std::uint32_t, 2>{order[(position + steps) % nodes] * (kStride / 4), static_cast<std::uint32_t>(sum)};
            };
            auto dispatch = [&](std::uint32_t steps, std::uint32_t lanes) {
                Params params{steps, lanes};
                id<MTLCommandBuffer> cb = [queue commandBuffer];
                id<MTLComputeCommandEncoder> encoder = [cb computeCommandEncoder];
                [encoder setComputePipelineState:chase];
                [encoder setBuffer:links offset:0 atIndex:0];
                [encoder setBytes:&params length:sizeof(params) atIndex:1];
                [encoder setBuffer:starts offset:0 atIndex:2];
                [encoder setBuffer:sink offset:0 atIndex:3];
                [encoder dispatchThreads:MTLSizeMake(lanes, 1, 1) threadsPerThreadgroup:MTLSizeMake(std::min(lanes, 256u), 1, 1)];
                [encoder endEncoding]; Check(cb);
                const auto* result = static_cast<const std::uint32_t*>(sink.contents);
                for (std::uint32_t lane = 0; lane < lanes; ++lane) {
                    const auto correct = expected(lane, steps);
                    if (result[lane * 2] != correct[0] || result[lane * 2 + 1] != correct[1])
                        throw std::runtime_error("dependent chain checksum mismatch");
                }
                const double seconds = cb.GPUEndTime - cb.GPUStartTime;
                if (!std::isfinite(seconds) || seconds <= 0) throw std::runtime_error("invalid GPU timestamp interval");
                return Sample{seconds * 1e9 / steps, double(steps) * lanes * 4 / seconds / 1e9, seconds * 1000};
            };
            auto warm = [&] {
#if TARGET_OS_IPHONE
                // Traverse the same complete permutation without one long iOS
                // command buffer. Preserve the chain between chunks, check every
                // sink, then restore its original start before the timed sample.
                for (std::uint32_t visited = 0; visited < nodes;) {
                    if (StopRequested()) throw std::runtime_error("stopped");
                    const auto count = std::min(8192u, nodes - visited);
                    dispatch(count, 1);
                    visited += count;
                    warmPosition = visited % nodes;
                    seed[0] = order[warmPosition] * (kStride / 4);
                }
#else
                dispatch(nodes, 1);
#endif
            };
            auto evictionPass = [&] {
                const auto vectors = static_cast<std::uint32_t>(evictionBytes / 16);
                id<MTLCommandBuffer> cb = [queue commandBuffer];
                id<MTLComputeCommandEncoder> encoder = [cb computeCommandEncoder];
                [encoder setComputePipelineState:evict];
                [encoder setBuffer:eviction offset:0 atIndex:0];
                [encoder setBytes:&vectors length:sizeof(vectors) atIndex:1];
                [encoder setBuffer:sink offset:0 atIndex:2];
                [encoder dispatchThreads:MTLSizeMake(65536, 1, 1) threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
                [encoder endEncoding]; Check(cb);
                const auto* result = static_cast<const std::uint32_t*>(sink.contents);
                const auto correct = static_cast<std::uint32_t>(0x35353535u * (vectors / 65536u) * 4u);
                if (result[0] != correct || result[65535] != correct)
                    throw std::runtime_error("eviction read checksum mismatch");
            };
            std::array<std::vector<double>, 4> measured;
            for (std::uint32_t repeat = 0; repeat < config.repeats; ++repeat) {
              for (std::uint32_t slot = 0; slot < conditions.size(); ++slot) {
                if (StopRequested()) return 4;
                const auto condition = (slot + repeat) % conditions.size();
                CpuTouch(cpuEviction); evictionPass();
                if (condition == 2) CpuTouch(links);
                else if (condition == 0 || condition == 3) warm();
                const bool parallel = condition == 3;
                const auto sample = dispatch(parallel ? parallelSteps : config.steps, parallel ? kLanes : 1);
                measured[condition].push_back(parallel ? sample.gbps : sample.ns);
                std::cout << "GCACHE_SAMPLE\tcondition=" << conditions[condition] << "\tbytes=" << bytes
                          << "\trepeat=" << repeat << "\tms=" << std::setprecision(9) << sample.ms
                          << "\tvalue=" << measured[condition].back() << "\tchecksum_valid=1\n" << std::flush;
              }
            }
            for (std::size_t condition = 0; condition < conditions.size(); ++condition) {
                const auto& values = measured[condition];
                Point point{conditions[condition], bytes, Median(values), *std::min_element(values.begin(), values.end()), *std::max_element(values.begin(), values.end())};
                points.push_back(point);
                std::cout << "GCACHE_POINT\tcondition=" << point.condition << "\tbytes=" << point.bytes
                          << "\tmedian=" << point.value << "\tmin=" << point.minimum << "\tmax=" << point.maximum
                          << "\tunit=" << (condition == 3 ? "GB/s_payload" : "ns/load") << '\n' << std::flush;
            }
          }
        }
        const double duration = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (saveResult) {
            BenchmarkResult stored;
            stored.id = GenerateResultId() + "-metal-cache";
            stored.timestamp = GenerateTimestamp(); stored.resultSchemaVersion = 4;
            stored.appVersion = CurrentAppVersion(); stored.workload = "gpu_cache";
            stored.workloadVersion = "metal_dependent_cache_v1_preview";
#if TARGET_OS_IPHONE
            stored.workloadVersion = "metal_dependent_cache_v2_preview_ios_preview";
            if (config.parallelSteps)
                stored.workloadVersion = "metal_dependent_cache_v3_parallel_" + std::to_string(parallelSteps) + "_preview_ios_preview";
#endif
            stored.graphicsApi = "Metal"; stored.deviceName = device.name.UTF8String;
            stored.osVersion = CurrentOsVersion(); stored.platform = CurrentPlatform();
            stored.osArchitecture = CurrentOsArchitecture(); stored.processArchitecture = CurrentProcessArchitecture();
            stored.memory = "Unified-memory"; stored.headless = true; stored.framesInFlight = 0;
            stored.durationSec = duration; stored.scoreUnit = "ns/load";
            for (const auto& point : points) if (point.condition == "gpu_warm") stored.score = point.value;
            std::ostringstream contract;
            contract << "method=random_dependent_uint32_cycle;aggregation=median;strideBytes=" << kStride
                     << ";steps=" << config.steps << ";repeats=" << config.repeats << ";parallelLanes=" << kLanes
                     << ";evictionBytes=" << evictionBytes << ";cpuEvictionBytes=" << cpuEvictionBytes
                     << ";evictionVerified=0;cacheAttribution=unverified;checksumValid=1;counterSets=" << counterSets;
#if TARGET_OS_IPHONE
            contract << ";warming=bounded_complete_cycle;warmChunkSteps=8192";
            contract << ";parallelSteps=" << parallelSteps;
#endif
            for (const auto& condition : conditions) {
                contract << ";curve_" << condition << '=';
                bool first = true;
                for (const auto& point : points) if (point.condition == condition) {
                    if (!first) contract << ',';
                    contract << (point.bytes / 1024) << ':' << std::setprecision(9) << point.value; first = false;
                }
            }
            stored.workloadConfig = contract.str();
            if (!AppendResult(stored)) return 5;
            std::cout << "GCACHE_SAVED\tid=" << stored.id << '\n';
        }
        std::cout << "GCACHE_DONE\tpoints=" << points.size() << "\tchecksum_valid=1\tduration=" << duration
                  << "\tcache_attribution=unverified\n" << std::flush;
        return 0;
    }} catch (const std::exception& error) {
        std::cerr << "GCACHE_ERROR\tmessage=" << error.what() << '\n';
        return 3;
    }
}
}
