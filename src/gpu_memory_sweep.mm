#include "gpu_memory_sweep.h"
#include "gpu_engine.h"
#include "benchmark_results.h"
#include <iostream>
#include <sstream>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <ostream>

#ifdef HAVE_METAL

#import <Metal/Metal.h>
#import <TargetConditionals.h>
#if TARGET_OS_IPHONE
#include <os/proc.h>
#endif

namespace gpu_bench {
namespace {

// The update body is the shipped particle kernel verbatim; only the walk around
// it is new. Two rules make the walk work at every working-set size:
//
//   * Each thread wraps inside the set with a mask, so a fixed grid stays fully
//     occupied even when the set is far smaller than the grid. Sizing the grid
//     to the set instead would launch ~1,000 threads for 64 KiB and report
//     under-occupancy as if it were slow cache.
//   * The step is odd, so it is coprime with the power-of-two working set and
//     each thread's orbit covers every element. An even step aliases: with a
//     grid-sized step and count == k * grid, a thread revisits only k elements,
//     which keeps them in the innermost cache and reports that rate for a set
//     far too large to be there. That is what produced a 4663 GB/s spike at
//     16 MiB, where k was 2 and each thread saw exactly two addresses.
//
// The step is the same for every thread, so neighbours stay one element apart
// and access is coalesced regardless of its value.
constexpr const char* kSweepSource = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct Particle { float4 position; float4 velocity; };
struct SweepParams {
    uint  mask;      // element count - 1 (count is a power of two)
    uint  stride;    // odd, so it is coprime with the working set
    uint  iters;     // per-thread accesses; constant traffic across sizes
    float dt;
    float bounds;
};

kernel void sweepInit(device Particle* p [[buffer(0)]],
                      constant uint& count [[buffer(1)]],
                      uint tid [[thread_position_in_grid]]) {
    if (tid >= count) return;
    uint a = (tid + 17u) * 2654435761u;
    a ^= a >> 15;
    uint b = a * 2246822519u;
    p[tid].position = as_type<float4>((uint4(a, b, a >> 9, b >> 9) & 0x007fffffu) | 0x3f800000u);
    p[tid].velocity = float4(0.001f, 0.002f, 0.003f, 0.0f);
}

kernel void particleSweepRMW(device Particle* p [[buffer(0)]],
                             constant SweepParams& sp [[buffer(1)]],
                             uint tid [[thread_position_in_grid]]) {
    uint idx = tid & sp.mask;
    for (uint k = 0u; k < sp.iters; ++k) {
        float4 pos = p[idx].position;
        float4 vel = p[idx].velocity;
        pos.xyz += vel.xyz * sp.dt;
        if (pos.x > sp.bounds) pos.x = -sp.bounds;
        p[idx].position = pos;
        idx = (idx + sp.stride) & sp.mask;
    }
}

kernel void particleSweepRead(device const Particle* p [[buffer(0)]],
                              constant SweepParams& sp [[buffer(1)]],
                              device float* sink [[buffer(2)]],
                              uint tid [[thread_position_in_grid]]) {
    uint idx = tid & sp.mask;
    float4 acc = float4(0.0);
    for (uint k = 0u; k < sp.iters; ++k) {
        acc += p[idx].position + p[idx].velocity;
        idx = (idx + sp.stride) & sp.mask;
    }
    // Consumed by a comparison that cannot be true: keeps the loads alive
    // without adding store traffic to a read-only measurement.
    if (acc.x == 1.0e30f && acc.y == -1.0e30f) sink[tid] = acc.z;
}
)METAL";

struct SweepParams {
    std::uint32_t mask;
    std::uint32_t stride;
    std::uint32_t iters;
    float dt;
    float bounds;
};

// Particle = float4 position + float4 velocity.
constexpr double kBytesPerParticle = 32.0;
// Read 32, write 16 back. The particle score assumes 40; see
// docs/interpreting-stream-bandwidth.md.
constexpr double kRmwBytesPerAccess = 48.0;
constexpr double kReadBytesPerAccess = 32.0;
constexpr double kStreamScoreBytes = 40.0;
constexpr std::uint32_t kThreadsPerGroup = 256;

const char* StorageName(GpuMemoryStorage s) {
    return s == GpuMemoryStorage::DeviceLocal ? "device_local" : "host_visible";
}
const char* AccessName(GpuMemoryAccess a) {
    return a == GpuMemoryAccess::ReadModifyWrite ? "read_modify_write" : "read_only";
}

std::string FormatSize(std::uint64_t bytes) {
    char buf[64];
    if (bytes >= (1ull << 20))
        std::snprintf(buf, sizeof(buf), "%llu MiB",
                      static_cast<unsigned long long>(bytes >> 20));
    else
        std::snprintf(buf, sizeof(buf), "%llu KiB",
                      static_cast<unsigned long long>(bytes >> 10));
    return buf;
}

/// Group adjacent sizes whose rate stays within `tolerance` of the running
/// group mean. A group of two or more points is one level of the hierarchy.
std::vector<GpuMemoryTier> DetectTiers(
    const std::vector<GpuMemorySweepPoint>& points, double tolerance) {
    std::vector<GpuMemoryTier> tiers;
    std::size_t start = 0;
    while (start < points.size()) {
        double sum = points[start].bestGbPerSec;
        std::size_t end = start;
        while (end + 1 < points.size()) {
            const double mean = sum / double(end - start + 1);
            const double next = points[end + 1].bestGbPerSec;
            if (mean <= 0.0 || std::fabs(next - mean) / mean > tolerance) break;
            ++end;
            sum += next;
        }
        if (end > start) {
            tiers.push_back({points[start].workingSetBytes,
                             points[end].workingSetBytes,
                             sum / double(end - start + 1)});
        }
        start = end + 1;
    }
    return tiers;
}

}  // namespace

GpuMemorySweepReport RunGpuMemorySweep(const GpuMemorySweepConfig& config,
                                       std::ostream& out) {
    GpuMemorySweepReport report;
    if (config.minKiB == 0 || config.maxKiB < config.minKiB || config.repeats == 0 ||
        config.threads > (1u << 20) || config.trafficPerPointMiB == 0 ||
        config.trafficPerPointMiB > 16384) {
        report.unsupportedReason = "invalid memory sweep bounds, traffic or grid";
        out << "MEMSWEEP_ERROR\tmessage=" << report.unsupportedReason << '\n' << std::flush;
        return report;
    }

    @autoreleasepool {
        id<MTLDevice> device = nil;
#if TARGET_OS_OSX
        NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
        if (devices.count > 0)
            device = devices[std::min<std::uint32_t>(
                config.gpuIndex, std::uint32_t(devices.count - 1))];
#endif
        if (!device) device = MTLCreateSystemDefaultDevice();
        if (!device) {
            report.unsupportedReason = "no Metal device available";
            out << "MEMSWEEP_ERROR\tmessage=no_metal_device\n" << std::flush;
            return report;
        }
        report.deviceName = [device.name UTF8String];

        NSError* error = nil;
        id<MTLLibrary> library =
            [device newLibraryWithSource:@(kSweepSource) options:nil error:&error];
        if (!library) {
            report.unsupportedReason =
                error ? [[error localizedDescription] UTF8String] : "compile failed";
            out << "MEMSWEEP_ERROR\tmessage=shader_compile_failed\n" << std::flush;
            return report;
        }

        id<MTLComputePipelineState> psoRMW =
            [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"particleSweepRMW"] error:&error];
        id<MTLComputePipelineState> psoRead =
            [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"particleSweepRead"] error:&error];
        id<MTLComputePipelineState> psoInit =
            [device newComputePipelineStateWithFunction:
                [library newFunctionWithName:@"sweepInit"] error:&error];
        if (!psoRMW || !psoRead || !psoInit) {
            report.unsupportedReason = "pipeline creation failed";
            out << "MEMSWEEP_ERROR\tmessage=pipeline_failed\n" << std::flush;
            return report;
        }

        const std::uint32_t threads = std::max<std::uint32_t>(config.threads,
                                                             kThreadsPerGroup);
        id<MTLCommandQueue> queue = [device newCommandQueue];
        id<MTLBuffer> sink =
            [device newBufferWithLength:threads * sizeof(float)
                                options:MTLResourceStorageModePrivate];

        if (!queue || !sink) {
            report.unsupportedReason = "queue or timing sink allocation failed";
            return report;
        }
        std::uint64_t budget = device.maxBufferLength;
        if (device.recommendedMaxWorkingSetSize > 0)
            budget = std::min<std::uint64_t>(budget, device.recommendedMaxWorkingSetSize / 4);
#if TARGET_OS_IPHONE
        const auto available = os_proc_available_memory();
        if (available > 0) budget = std::min<std::uint64_t>(budget, available / 4);
        budget = std::min<std::uint64_t>(budget, 256ull << 20);
#endif
        const auto maxKiB = std::min<std::uint64_t>(config.maxKiB, budget / 1024);
        report.supported = true;
        report.threads = threads;

        out << "\n--- GPU memory hierarchy sweep ---\n"
            << "Device: " << report.deviceName << "\n"
            << "Method: the shipped particle update body, run by a fixed grid of "
            << threads << " threads over a working set swept from "
            << FormatSize(std::uint64_t(config.minKiB) * 1024) << " to "
            << FormatSize(std::uint64_t(config.maxKiB) * 1024) << ".\n"
            << "Every point moves the same "
            << config.trafficPerPointMiB / 1024 << " GiB, so the only variable "
               "is where the data is served from.\n"
            << "A plateau is one level of the hierarchy; the last one is memory.\n"
            << std::flush;
        out << "MEMSWEEP_META\tdevice=" << report.deviceName
            << "\tgrid_threads=" << threads
            << "\tbytes_per_particle=" << int(kRmwBytesPerAccess)
            << "\trepeats=" << config.repeats << '\n' << std::flush;

        std::vector<GpuMemoryStorage> storages{GpuMemoryStorage::DeviceLocal};
        if (config.includeHostVisible)
            storages.push_back(GpuMemoryStorage::HostVisible);
        std::vector<GpuMemoryAccess> accesses{GpuMemoryAccess::ReadModifyWrite};
        if (config.includeReadOnly)
            accesses.push_back(GpuMemoryAccess::ReadOnly);

        for (GpuMemoryStorage storage : storages) {
            const MTLResourceOptions options =
                storage == GpuMemoryStorage::DeviceLocal
                    ? MTLResourceStorageModePrivate
                    : MTLResourceStorageModeShared;

            for (GpuMemoryAccess access : accesses) {
                const bool rmw = access == GpuMemoryAccess::ReadModifyWrite;
                const double bytesPerAccess =
                    rmw ? kRmwBytesPerAccess : kReadBytesPerAccess;
                const std::uint32_t iters = std::max<std::uint32_t>(
                    1u,
                    std::uint32_t(double(config.trafficPerPointMiB) * 1024.0 *
                                  1024.0 / (double(threads) * bytesPerAccess)));
                const double traffic =
                    double(threads) * double(iters) * bytesPerAccess;

                out << "\n[" << StorageName(storage) << " / "
                    << AccessName(access) << "]\n"
                    << "  working set     particles       GB/s   stream-equiv"
                       "       ms\n" << std::flush;

                std::vector<GpuMemorySweepPoint> modePoints;
                for (std::uint64_t kib = config.minKiB; kib <= maxKiB;
                     kib *= 2) {
                    if (StopRequested()) {
                        report.supported = false;
                        report.unsupportedReason = "stopped";
                        return report;
                    }
                    const std::uint64_t bytes = kib * 1024ull;
                    const auto count =
                        std::uint32_t(double(bytes) / kBytesPerParticle);
                    if (count == 0 || (count & (count - 1)) != 0) continue;

                    id<MTLBuffer> buffer =
                        [device newBufferWithLength:bytes options:options];
                    if (!buffer) {
                        out << "  " << std::setw(11) << FormatSize(bytes)
                            << "   allocation failed; stopping this mode\n"
                            << std::flush;
                        break;
                    }

                    // Initialise every element through the GPU before measuring a load.
                    id<MTLCommandBuffer> init = [queue commandBuffer];
                    id<MTLComputeCommandEncoder> initEncoder = [init computeCommandEncoder];
                    [initEncoder setComputePipelineState:psoInit];
                    [initEncoder setBuffer:buffer offset:0 atIndex:0];
                    [initEncoder setBytes:&count length:sizeof(count) atIndex:1];
                    [initEncoder dispatchThreads:MTLSizeMake(count, 1, 1)
                           threadsPerThreadgroup:MTLSizeMake(kThreadsPerGroup, 1, 1)];
                    [initEncoder endEncoding];
                    [init commit];
                    [init waitUntilCompleted];
                    if (init.status == MTLCommandBufferStatusError) {
                        report.supported = false;
                        report.unsupportedReason = "GPU buffer initialisation failed";
                        return report;
                    }

                    // `| 1` forces an odd step; see the aliasing note above.
                    SweepParams params{count - 1u, threads | 1u,
                                       iters, 0.016f, 100.0f};

                    // One untimed pass: faults the buffer in, and lets the GPU
                    // leave its idle clocks before anything is measured.
                    {
                        id<MTLCommandBuffer> warm = [queue commandBuffer];
                        id<MTLComputeCommandEncoder> enc =
                            [warm computeCommandEncoder];
                        [enc setComputePipelineState:(rmw ? psoRMW : psoRead)];
                        [enc setBuffer:buffer offset:0 atIndex:0];
                        [enc setBytes:&params length:sizeof(params) atIndex:1];
                        [enc setBuffer:sink offset:0 atIndex:2];
                        [enc dispatchThreads:MTLSizeMake(threads, 1, 1)
                       threadsPerThreadgroup:MTLSizeMake(kThreadsPerGroup, 1, 1)];
                        [enc endEncoding];
                        [warm commit];
                        [warm waitUntilCompleted];
                    }

                    double bestSec = 0.0;
                    for (std::uint32_t r = 0; r < config.repeats; ++r) {
                        if (StopRequested()) {
                            report.supported = false;
                            report.unsupportedReason = "stopped";
                            return report;
                        }
                        id<MTLCommandBuffer> cb = [queue commandBuffer];
                        id<MTLComputeCommandEncoder> enc =
                            [cb computeCommandEncoder];
                        [enc setComputePipelineState:(rmw ? psoRMW : psoRead)];
                        [enc setBuffer:buffer offset:0 atIndex:0];
                        [enc setBytes:&params length:sizeof(params) atIndex:1];
                        [enc setBuffer:sink offset:0 atIndex:2];
                        [enc dispatchThreads:MTLSizeMake(threads, 1, 1)
                       threadsPerThreadgroup:MTLSizeMake(kThreadsPerGroup, 1, 1)];
                        [enc endEncoding];
                        [cb commit];
                        [cb waitUntilCompleted];
                        if (cb.status == MTLCommandBufferStatusError) {
                            report.supported = false;
                            report.unsupportedReason = "GPU dispatch failed";
                            return report;
                        }
                        const double sec = cb.GPUEndTime - cb.GPUStartTime;
                        if (sec > 0.0 && (bestSec == 0.0 || sec < bestSec))
                            bestSec = sec;
                    }
                    if (bestSec <= 0.0) continue;

                    GpuMemorySweepPoint point;
                    point.storage = storage;
                    point.access = access;
                    point.workingSetBytes = bytes;
                    point.particles = count;
                    point.bestGbPerSec = traffic / bestSec / 1e9;
                    point.streamEquivalentGbPerSec =
                        point.bestGbPerSec * kStreamScoreBytes / bytesPerAccess;
                    point.bestMs = bestSec * 1000.0;
                    modePoints.push_back(point);
                    report.points.push_back(point);

                    out << "  " << std::setw(11) << FormatSize(bytes)
                        << std::setw(12) << count << "   "
                        << std::setw(9) << std::fixed << std::setprecision(1)
                        << point.bestGbPerSec << "   "
                        << std::setw(11) << point.streamEquivalentGbPerSec
                        << "   " << std::setw(7) << std::setprecision(3)
                        << point.bestMs << "\n" << std::flush;
                    out << "MEMSWEEP_POINT\tstorage=" << StorageName(storage)
                        << "\taccess=" << AccessName(access)
                        << "\tparticles=" << count
                        << "\tbytes=" << bytes
                        << "\tgbps=" << std::setprecision(3) << point.bestGbPerSec
                        << "\tstream_equiv_gbps=" << point.streamEquivalentGbPerSec
                        << "\tms=" << point.bestMs << '\n' << std::flush;
                }

                if (modePoints.empty()) continue;
                const double memoryRate = modePoints.back().bestGbPerSec;
                if (storage == GpuMemoryStorage::DeviceLocal && rmw) {
                    report.deviceLocalMemoryGbPerSec = memoryRate;
                    report.deviceLocalTiers = DetectTiers(modePoints, 0.12);
                    const auto peak = std::max_element(
                        modePoints.begin(), modePoints.end(),
                        [](const GpuMemorySweepPoint& a,
                           const GpuMemorySweepPoint& b) {
                            return a.bestGbPerSec < b.bestGbPerSec;
                        });
                    report.peakCacheGbPerSec = peak->bestGbPerSec;
                    report.peakCacheBytes = peak->workingSetBytes;
                } else if (storage == GpuMemoryStorage::DeviceLocal) {
                    report.readOnlyMemoryGbPerSec = memoryRate;
                } else if (rmw) {
                    report.hostVisibleMemoryGbPerSec = memoryRate;
                }
            }
        }

        if (report.points.empty()) {
            report.supported = false;
            report.unsupportedReason = "no valid GPU timings within the memory budget";
            return report;
        }
        out << "\n--- Detected tiers (device-local, read-modify-write) ---\n"
            << std::flush;
        for (const auto& tier : report.deviceLocalTiers) {
            out << "  " << std::setw(11) << FormatSize(tier.fromBytes) << " - "
                << std::setw(11) << FormatSize(tier.toBytes) << "   "
                << std::fixed << std::setprecision(1) << tier.meanGbPerSec
                << " GB/s\n" << std::flush;
            out << "MEMSWEEP_TIER\tfrom_bytes=" << tier.fromBytes
                << "\tto_bytes=" << tier.toBytes
                << "\tgbps=" << std::setprecision(3) << tier.meanGbPerSec
                << '\n' << std::flush;
        }
        if (report.deviceLocalTiers.empty())
            out << "  (no stable plateau found; widen the size range)\n"
                << std::flush;

        out << "\nPeak on-chip read: " << std::fixed << std::setprecision(1)
            << report.peakCacheGbPerSec << " GB/s at "
            << FormatSize(report.peakCacheBytes) << "\n"
            << "Device-local memory: " << report.deviceLocalMemoryGbPerSec
            << " GB/s read-modify-write";
        if (report.readOnlyMemoryGbPerSec > 0.0)
            out << ", " << report.readOnlyMemoryGbPerSec << " GB/s read-only";
        out << "\n";
        if (report.hostVisibleMemoryGbPerSec > 0.0) {
            out << "Host-visible memory: " << report.hostVisibleMemoryGbPerSec
                << " GB/s\n";
            const double ratio = report.deviceLocalMemoryGbPerSec > 0.0
                ? report.hostVisibleMemoryGbPerSec /
                      report.deviceLocalMemoryGbPerSec
                : 0.0;
            if (ratio > 0.9 && ratio < 1.1) {
                out << "  The two storage modes match, which is what unified "
                       "memory looks like: there is no separate VRAM here.\n";
            } else {
                out << "  Host-visible is " << std::setprecision(2) << ratio
                    << "x device-local — a discrete pool across a slower link. "
                       "That is the link speed, not the host's DRAM bandwidth.\n";
            }
        }
        out << "MEMSWEEP_RESULT"
            << "\tpeak_cache_gbps=" << std::setprecision(3)
            << report.peakCacheGbPerSec
            << "\tpeak_cache_bytes=" << report.peakCacheBytes
            << "\tdevice_local_gbps=" << report.deviceLocalMemoryGbPerSec
            << "\tread_only_gbps=" << report.readOnlyMemoryGbPerSec
            << "\thost_visible_gbps=" << report.hostVisibleMemoryGbPerSec
            << "\ttiers=" << report.deviceLocalTiers.size()
            << '\n' << std::flush;
    }

    return report;
}

int RunGpuMemorySweepCli(const GpuMemorySweepConfig& config, bool saveResult) {
    const auto report = RunGpuMemorySweep(config, std::cout);
    if (!report.supported) {
        std::cerr << "MEMSWEEP_ERROR\tmessage=" << report.unsupportedReason << '\n';
        return report.unsupportedReason == "stopped" ? 4 : 3;
    }
    if (saveResult) {
        BenchmarkResult stored;
        stored.id = GenerateResultId() + "-metal-memsweep";
        stored.timestamp = GenerateTimestamp();
        stored.resultSchemaVersion = 4;
        stored.appVersion = CurrentAppVersion();
        stored.workload = "membw";
        stored.workloadVersion = "metal_memory_sweep_v1_preview";
#if TARGET_OS_IPHONE
        stored.workloadVersion += "_ios_preview";
#endif
        stored.graphicsApi = "Metal";
        stored.deviceName = report.deviceName;
        stored.osVersion = CurrentOsVersion();
        stored.platform = CurrentPlatform();
        stored.osArchitecture = CurrentOsArchitecture();
        stored.processArchitecture = CurrentProcessArchitecture();
        stored.headless = true;
        stored.score = report.readOnlyMemoryGbPerSec;
        stored.scoreUnit = "GB/s";
        std::ostringstream contract;
        contract << "method=particle_fixed_grid;aggregation=best;grid=" << report.threads
                 << ";repeats=" << config.repeats << ";trafficMiB=" << config.trafficPerPointMiB
                 << ";memoryPlateauVerified=0";
        for (const auto& point : report.points)
            contract << ';' << StorageName(point.storage) << '_' << AccessName(point.access)
                     << '_' << point.workingSetBytes << '=' << point.bestGbPerSec;
        stored.workloadConfig = contract.str();
        if (!AppendResult(stored)) return 5;
    }
    return 0;
}

}  // namespace gpu_bench

#else  // !HAVE_METAL

namespace gpu_bench {

GpuMemorySweepReport RunGpuMemorySweep(const GpuMemorySweepConfig&,
                                       std::ostream& out) {
    GpuMemorySweepReport report;
    report.unsupportedReason =
        "the memory sweep is currently implemented for the Metal backend only";
    out << "MEMSWEEP_ERROR\tmessage=backend_not_implemented\n" << std::flush;
    return report;
}

}  // namespace gpu_bench

#endif  // HAVE_METAL
