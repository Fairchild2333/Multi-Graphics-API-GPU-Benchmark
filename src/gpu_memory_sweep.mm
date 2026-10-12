#include "gpu_memory_sweep.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>

#ifdef HAVE_METAL

#import <Metal/Metal.h>

namespace gpu_bench {
namespace {

// Each thread walks the working set with a stride equal to the grid size, so
// neighbouring threads touch neighbouring addresses and every access is
// coalesced. The working set is always a power of two, which turns the wrap
// into a mask instead of a modulo.
//
// The accumulator is consumed by a comparison that can never be true, which
// stops the compiler from eliminating the loads without costing a store.
constexpr const char* kSweepSource = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct SweepParams {
    uint mask;            // elementCount - 1
    uint stride;          // grid size in elements
    uint itersPerThread;
    uint pad;
};

kernel void readSweep(device const float4* buf   [[buffer(0)]],
                      constant SweepParams& p    [[buffer(1)]],
                      device float* sink         [[buffer(2)]],
                      uint tid [[thread_position_in_grid]]) {
    float4 acc = float4(0.0);
    uint idx = tid & p.mask;
    for (uint k = 0u; k < p.itersPerThread; ++k) {
        acc += buf[idx];
        idx = (idx + p.stride) & p.mask;
    }
    if (acc.x == 1.0e30f && acc.y == -1.0e30f) sink[tid] = acc.z + acc.w;
}
)METAL";

struct SweepParams {
    std::uint32_t mask;
    std::uint32_t stride;
    std::uint32_t itersPerThread;
    std::uint32_t pad;
};

constexpr std::uint32_t kThreads = 1u << 18;      // 262144
constexpr std::uint32_t kBytesPerAccess = 16;     // one float4
constexpr std::uint32_t kThreadsPerGroup = 256;

const char* StorageName(GpuMemoryStorage storage) {
    return storage == GpuMemoryStorage::DeviceLocal ? "device_local"
                                                    : "host_visible";
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

/// Group adjacent sizes whose rates stay within `tolerance` of the group mean.
/// Each surviving group of two or more points is one level of the hierarchy.
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

    @autoreleasepool {
        NSArray<id<MTLDevice>>* devices = nil;
#if TARGET_OS_OSX
        devices = MTLCopyAllDevices();
#endif
        id<MTLDevice> device = nil;
        if (devices && devices.count > 0)
            device = devices[std::min<std::uint32_t>(
                config.gpuIndex, std::uint32_t(devices.count - 1))];
        else
            device = MTLCreateSystemDefaultDevice();

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
        id<MTLFunction> function = [library newFunctionWithName:@"readSweep"];
        id<MTLComputePipelineState> pso =
            [device newComputePipelineStateWithFunction:function error:&error];
        if (!pso) {
            report.unsupportedReason = "pipeline creation failed";
            out << "MEMSWEEP_ERROR\tmessage=pipeline_failed\n" << std::flush;
            return report;
        }

        id<MTLCommandQueue> queue = [device newCommandQueue];
        id<MTLBuffer> sink =
            [device newBufferWithLength:kThreads * sizeof(float)
                                options:MTLResourceStorageModePrivate];

        report.supported = true;
        report.threads = kThreads;

        const std::uint64_t accesses =
            std::uint64_t(config.trafficPerPointMiB) * 1024ull * 1024ull /
            kBytesPerAccess;
        const std::uint32_t itersPerThread = std::max<std::uint32_t>(
            1u, std::uint32_t(accesses / kThreads));
        const double trafficBytes =
            double(kThreads) * double(itersPerThread) * double(kBytesPerAccess);

        out << "\n--- GPU memory hierarchy sweep ---\n"
            << "Device: " << report.deviceName << "\n"
            << "Method: " << kThreads << " threads x " << itersPerThread
            << " coalesced float4 loads per dispatch ("
            << std::fixed << std::setprecision(2)
            << trafficBytes / (1024.0 * 1024.0 * 1024.0)
            << " GiB read per point, identical at every size).\n"
            << "Bytes are exact: read-only, 16 B per access, nothing written.\n"
            << "A plateau is one level of the hierarchy; the last one is memory.\n"
            << std::flush;

        out << "MEMSWEEP_META\tdevice=" << report.deviceName
            << "\tthreads=" << kThreads
            << "\titers_per_thread=" << itersPerThread
            << "\tbytes_per_access=" << kBytesPerAccess
            << "\trepeats=" << config.repeats << '\n' << std::flush;

        std::vector<GpuMemoryStorage> modes{GpuMemoryStorage::DeviceLocal};
        if (config.includeHostVisible)
            modes.push_back(GpuMemoryStorage::HostVisible);

        for (GpuMemoryStorage storage : modes) {
            const MTLResourceOptions options =
                storage == GpuMemoryStorage::DeviceLocal
                    ? MTLResourceStorageModePrivate
                    : MTLResourceStorageModeShared;

            out << "\n[" << StorageName(storage) << "]\n"
                << "  working set        GB/s      ms\n" << std::flush;

            std::vector<GpuMemorySweepPoint> modePoints;
            for (std::uint64_t kib = config.minKiB; kib <= config.maxKiB;
                 kib *= 2) {
                const std::uint64_t bytes = kib * 1024ull;
                const std::uint32_t elements =
                    std::uint32_t(bytes / kBytesPerAccess);
                if (elements == 0 || (elements & (elements - 1)) != 0) continue;

                id<MTLBuffer> buffer =
                    [device newBufferWithLength:bytes options:options];
                if (!buffer) {
                    out << "  " << std::setw(10) << FormatSize(bytes)
                        << "   allocation failed; stopping this mode\n"
                        << std::flush;
                    break;
                }

                SweepParams params{elements - 1u, kThreads, itersPerThread, 0u};
                double bestSec = 0.0;
                for (std::uint32_t r = 0; r < config.repeats; ++r) {
                    id<MTLCommandBuffer> cb = [queue commandBuffer];
                    id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
                    [enc setComputePipelineState:pso];
                    [enc setBuffer:buffer offset:0 atIndex:0];
                    [enc setBytes:&params length:sizeof(params) atIndex:1];
                    [enc setBuffer:sink offset:0 atIndex:2];
                    [enc dispatchThreads:MTLSizeMake(kThreads, 1, 1)
                   threadsPerThreadgroup:MTLSizeMake(kThreadsPerGroup, 1, 1)];
                    [enc endEncoding];
                    [cb commit];
                    [cb waitUntilCompleted];
                    const double sec = cb.GPUEndTime - cb.GPUStartTime;
                    if (sec > 0.0 && (bestSec == 0.0 || sec < bestSec))
                        bestSec = sec;
                }
                if (bestSec <= 0.0) continue;

                GpuMemorySweepPoint point;
                point.storage = storage;
                point.workingSetBytes = bytes;
                point.bestGbPerSec = trafficBytes / bestSec / 1e9;
                point.bestMs = bestSec * 1000.0;
                modePoints.push_back(point);
                report.points.push_back(point);

                out << "  " << std::setw(10) << FormatSize(bytes) << "   "
                    << std::setw(8) << std::fixed << std::setprecision(1)
                    << point.bestGbPerSec << "  " << std::setw(7)
                    << std::setprecision(3) << point.bestMs << "\n"
                    << std::flush;
                out << "MEMSWEEP_POINT\tstorage=" << StorageName(storage)
                    << "\tbytes=" << bytes
                    << "\tgbps=" << std::setprecision(3) << point.bestGbPerSec
                    << "\tms=" << point.bestMs << '\n' << std::flush;
            }

            if (modePoints.empty()) continue;

            const double memoryRate = modePoints.back().bestGbPerSec;
            if (storage == GpuMemoryStorage::DeviceLocal) {
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
            } else {
                report.hostVisibleMemoryGbPerSec = memoryRate;
            }
        }

        out << "\n--- Detected tiers (device-local) ---\n" << std::flush;
        for (const auto& tier : report.deviceLocalTiers) {
            out << "  " << std::setw(10) << FormatSize(tier.fromBytes) << " - "
                << std::setw(10) << FormatSize(tier.toBytes) << "   "
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

        out << "\nPeak cache read: " << std::fixed << std::setprecision(1)
            << report.peakCacheGbPerSec << " GB/s at "
            << FormatSize(report.peakCacheBytes) << "\n"
            << "Device-local memory: " << report.deviceLocalMemoryGbPerSec
            << " GB/s\n";
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
                    << "x device-local — a discrete memory pool reached over a "
                       "slower link.\n";
            }
        }
        out << "MEMSWEEP_RESULT"
            << "\tpeak_cache_gbps=" << std::setprecision(3)
            << report.peakCacheGbPerSec
            << "\tpeak_cache_bytes=" << report.peakCacheBytes
            << "\tdevice_local_gbps=" << report.deviceLocalMemoryGbPerSec
            << "\thost_visible_gbps=" << report.hostVisibleMemoryGbPerSec
            << "\ttiers=" << report.deviceLocalTiers.size()
            << '\n' << std::flush;
    }

    return report;
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
