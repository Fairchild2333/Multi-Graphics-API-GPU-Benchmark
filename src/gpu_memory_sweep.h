#pragma once

// GPU memory-hierarchy bandwidth sweep.
//
// Derived from the particle (`stream`) workload: same data layout, same update
// body, same read-modify-write traffic pattern. The existing test is not
// modified -- its kernel, its score formula, and its stored results are
// untouched, and this probe writes nothing to results.json.
//
// The particle score cannot answer "where are the bandwidth tiers" because the
// shipped kernel launches one thread per particle, welding working-set size to
// parallelism: shrinking the buffer to reach the caches also starves the GPU,
// and the measured curve inverts. This probe changes only the launch geometry --
// a fixed, large grid walks a working set whose size is swept independently --
// so the tiers show up as plateaus.
//
// See docs/gpu-memory-sweep-plan.md for the method, the measured M4 Pro curve,
// and docs/interpreting-stream-bandwidth.md for why the particle score's byte
// accounting cannot be reused here.

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace gpu_bench {

enum class GpuMemoryStorage {
    /// Device-local / GPU-private allocation. VRAM on a discrete GPU.
    DeviceLocal,
    /// CPU-visible allocation. System RAM across PCIe on a discrete GPU; the
    /// same physical DRAM as DeviceLocal on unified-memory parts, which the
    /// sweep reports rather than implying a split that does not exist.
    HostVisible,
};

enum class GpuMemoryAccess {
    /// The shipped particle update: reads 32 B, writes 16 B back.
    ReadModifyWrite,
    /// Same struct and walk, reads only. No write contention, so the small
    /// working sets stay meaningful, and it gives the honest peak.
    ReadOnly,
};

struct GpuMemorySweepConfig {
    std::uint32_t gpuIndex = 0;
    /// Working-set bounds in KiB, swept by doubling. Both are clamped to powers
    /// of two; the walk uses a mask instead of a modulo.
    std::uint32_t minKiB = 64;
    std::uint32_t maxKiB = 512u * 1024u;   // 512 MiB
    /// Bytes moved per point, so every size does equal work.
    std::uint32_t trafficPerPointMiB = 2048;
    /// Dispatches per point; the fastest is kept, so scheduler noise and clock
    /// ramp bias the result down, never up.
    std::uint32_t repeats = 5;
    /// Fixed grid size. Must saturate the device at every working-set size;
    /// raising it must not move the plateaus (plan section 9, check 5).
    std::uint32_t threads = 1u << 18;      // 262144
    bool includeHostVisible = true;
    bool includeReadOnly = true;
};

struct GpuMemorySweepPoint {
    GpuMemoryStorage storage = GpuMemoryStorage::DeviceLocal;
    GpuMemoryAccess access = GpuMemoryAccess::ReadModifyWrite;
    std::uint64_t workingSetBytes = 0;
    std::uint32_t particles = 0;
    double bestGbPerSec = 0.0;
    /// Same figure scaled by the particle score's assumed 40 B/particle, so a
    /// point can be checked directly against a real `stream` run.
    double streamEquivalentGbPerSec = 0.0;
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
    /// Sustained rate of the largest working set — the memory plateau.
    double deviceLocalMemoryGbPerSec = 0.0;
    double hostVisibleMemoryGbPerSec = 0.0;
    double readOnlyMemoryGbPerSec = 0.0;
    /// Fastest point overall — the innermost cache the sweep reached. Reported
    /// as a peak on-chip read rate, never as an "L1" figure: at the smallest
    /// sizes the grid covers few distinct elements and the hardware may service
    /// many lanes from one fetch.
    double peakCacheGbPerSec = 0.0;
    std::uint64_t peakCacheBytes = 0;
};

/// Runs the sweep, writing a human-readable table plus TAB-separated MEMSWEEP_*
/// records to `out`. Every record is flushed so a GUI can stream it.
GpuMemorySweepReport RunGpuMemorySweep(const GpuMemorySweepConfig& config,
                                       std::ostream& out);

}  // namespace gpu_bench
