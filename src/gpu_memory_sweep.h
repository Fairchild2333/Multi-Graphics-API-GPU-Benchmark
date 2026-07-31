#pragma once

// GPU memory-hierarchy bandwidth sweep.
//
// A standalone probe, deliberately separate from the Workload/AppBase pipeline
// and from the `stream` particle score: it neither reuses nor changes them.
// The particle workload answers "how fast does this workload run"; this one
// answers "where are the bandwidth tiers on this device".
//
// Method: a fixed, large thread grid repeatedly reads a working set whose size
// is swept from a few KB up to hundreds of MB. Every dispatch performs the same
// number of loads, so the only variable is where those loads are served from.
// Plotting bandwidth against working-set size exposes the cache levels as
// plateaus, and the final plateau is the memory bandwidth.
//
// Byte accounting here is exact, unlike the particle score's estimated constant:
// each access loads one float4 and nothing is written, so traffic is precisely
// accesses x 16 bytes. See docs/interpreting-stream-bandwidth.md.

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace gpu_bench {

enum class GpuMemoryStorage {
    /// Device-local / GPU-private allocation. On a discrete GPU this is VRAM.
    DeviceLocal,
    /// CPU-visible allocation. On a discrete GPU this is system RAM reached
    /// over PCIe; on unified-memory parts it is the same physical DRAM as
    /// DeviceLocal, which the sweep will show.
    HostVisible,
};

struct GpuMemorySweepConfig {
    std::uint32_t gpuIndex = 0;
    /// Working-set bounds, in KiB. Swept by doubling.
    std::uint32_t minKiB = 16;
    std::uint32_t maxKiB = 512u * 1024u;   // 512 MiB
    /// Loads issued per dispatch, expressed as total traffic in MiB. Constant
    /// across sizes so every point does equal work.
    std::uint32_t trafficPerPointMiB = 2048;
    /// Dispatches per size; the best is kept, to reject scheduling noise.
    std::uint32_t repeats = 5;
    /// Both storage modes are measured when true.
    bool includeHostVisible = true;
};

struct GpuMemorySweepPoint {
    GpuMemoryStorage storage = GpuMemoryStorage::DeviceLocal;
    std::uint64_t workingSetBytes = 0;
    double bestGbPerSec = 0.0;
    double bestMs = 0.0;
};

/// A run of adjacent sizes that sustained a similar rate — one level of the
/// hierarchy.
struct GpuMemoryTier {
    std::uint64_t fromBytes = 0;
    std::uint64_t toBytes = 0;
    double meanGbPerSec = 0.0;
};

struct GpuMemorySweepReport {
    bool supported = false;
    std::string unsupportedReason;
    std::string deviceName;
    std::uint32_t threads = 0;
    std::vector<GpuMemorySweepPoint> points;
    std::vector<GpuMemoryTier> deviceLocalTiers;
    /// Sustained rate of the largest working set, i.e. the memory plateau.
    double deviceLocalMemoryGbPerSec = 0.0;
    double hostVisibleMemoryGbPerSec = 0.0;
    /// Fastest point overall — the innermost cache the sweep reached.
    double peakCacheGbPerSec = 0.0;
    std::uint64_t peakCacheBytes = 0;
};

/// Runs the sweep and writes a human-readable table plus TAB-separated
/// MEMSWEEP_* records to `out`. Every record is flushed so a GUI can stream it.
GpuMemorySweepReport RunGpuMemorySweep(const GpuMemorySweepConfig& config,
                                       std::ostream& out);

}  // namespace gpu_bench
