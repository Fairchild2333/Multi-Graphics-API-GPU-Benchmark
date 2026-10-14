#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace gpu_bench {

// Stable score contract of the memory-bandwidth sweep. It is a separate
// workload: its results never share a comparison group with `stream`.
inline constexpr char kMemBwWorkloadId[] = "membw";
// `MemBwReport::error` of a sweep that RequestStop() ended early.
inline constexpr char kMemBwStopped[] = "stopped";
inline constexpr char kMemBwWorkloadVersion[] = "membw_v2_verified";

struct MemBwConfig {
    // vkEnumeratePhysicalDevices index; negative selects the first device.
    std::int32_t physicalDeviceIndex = -1;
    std::string shaderDir;
    // Sweep ceiling. The device may lower it (buffer range, allocation size,
    // heap size); the report states whether that happened.
    std::uint64_t maxDatasetBytes = 2048ull << 20;
    std::uint32_t warmupRuns = 3;
    std::uint32_t measuredRuns = 10;
    double targetDispatchMs = 4.0;
};

struct MemBwPoint {
    // "read" | "write" | "copy" for the data-set sweep; "host_read" |
    // "host_write" for the same kernels on a system-memory buffer; "l1" |
    // "l1_single" for the L1 sweep, where `datasetBytes` is the private block
    // of one workgroup and `passes` is how often each workgroup re-reads it.
    std::string kernel;
    std::uint64_t datasetBytes = 0;
    std::uint32_t passes = 0;           // visits of every chunk per dispatch
    std::uint64_t bytesPerDispatch = 0; // bytes the kernel actually requests
    double medianMs = 0.0;
    double minMs = 0.0;
    double medianGBps = 0.0;
    double bestGBps = 0.0;
};

struct MemBwKernelSummary {
    // Median of the three largest data sets, beyond every cache level.
    double memoryGBps = 0.0;
    // Longest flat stretch of the curve that is clearly above the memory
    // plateau. 0 when the curve shows no distinct cache level.
    double cacheGBps = 0.0;
    std::uint64_t cacheFromBytes = 0;   // first data set of that stretch
    std::uint64_t cacheToBytes = 0;     // last data set of that stretch
    // Largest data set that still lies on a cache plateau (0 if none). This is
    // not an exact capacity: when the GPU runs more workgroups at once than
    // the data set has chunks, concurrent groups share chunks through the
    // cache and the transition to memory is stretched.
    std::uint64_t cacheKneeBytes = 0;
    // False when the sweep is shorter than 256 MiB or its three largest data
    // sets are not flat yet, so `memoryGBps` may still be cache-assisted.
    bool memoryPlateauReached = false;
    // True when the smallest data sets ran faster than the cache plateau:
    // several workgroups on one SM/CU then share a chunk through L1, so those
    // points are not an L2 measurement.
    bool smallSetsL1Assisted = false;
};

// L1 sweep: every workgroup re-reads a private block, so the block is served by
// the cache local to its SM/CU for as long as it fits there.
struct MemBwL1Summary {
    // All workgroups busy. The plateau around the fastest block size is the L1
    // read rate of the whole GPU as reached by this kernel (it is limited by
    // the instruction rate, not only by the cache). Several groups share one
    // SM/CU, so this plateau ends below the L1 capacity.
    double fullLoadGBps = 0.0;
    std::uint64_t fullLoadBlockBytes = 0;   // largest block still on that plateau
    // One workgroup, which runs on one SM/CU.
    double singleGroupGBps = 0.0;
    // The L1 capacity of one SM/CU lies between these two block sizes. Both
    // are 0 when the single-group curve shows no drop inside the sweep.
    std::uint64_t capacityLowBytes = 0;
    std::uint64_t capacityHighBytes = 0;
};

// System-memory pass (discrete GPUs only). The kernels and the buffer copies
// use a buffer that lives in host memory, so every byte crosses the PCIe link
// and the result is bounded by that link rather than by VRAM.
struct MemBwHostSummary {
    bool measured = false;
    std::string skippedReason;
    MemBwKernelSummary read;         // compute-shader loads: system memory -> GPU
    MemBwKernelSummary write;        // compute-shader stores: GPU -> system memory
    double copyToDeviceGBps = 0.0;   // vkCmdCopyBuffer: system memory -> VRAM
    double copyToHostGBps = 0.0;     // vkCmdCopyBuffer: VRAM -> system memory
    std::uint64_t copyBytes = 0;     // size of one buffer copy
};

struct MemBwReport {
    bool valid = false;
    bool dataVerified = false;
    std::uint32_t writeCopyChecks = 0;
    bool performanceQueryAdvertised = false;
    std::string performanceCounterInventory;
    std::string performanceCounterStatus = "extension_not_advertised";
    std::string error;

    std::string deviceName;
    std::string deviceType;
    std::string driverVersion;
    bool isSoftware = false;
    bool unifiedMemory = false;      // integrated GPU / CPU device
    std::uint32_t deviceLocalHeapMB = 0;

    std::string timingSource;        // "gpu_timestamp" | "wall_clock"
    std::uint32_t timestampAnomalies = 0;
    std::uint64_t chunkBytes = 0;
    std::uint64_t maxDatasetBytes = 0;
    bool datasetLimitedByDevice = false;
    std::uint32_t warmupRuns = 0;
    std::uint32_t measuredRuns = 0;
    double durationSec = 0.0;

    std::vector<MemBwPoint> points;
    MemBwKernelSummary read;
    MemBwKernelSummary write;
    MemBwKernelSummary copy;
    MemBwL1Summary l1;
    MemBwHostSummary host;
};

// Runs the sweep on one Vulkan device and prints a human-readable table plus
// tab-separated MEMBW_POINT / MEMBW_SUMMARY lines. Compute-only: it creates no
// window, surface or swapchain. Failures are reported through
// `MemBwReport::error` rather than exceptions.
MemBwReport RunVulkanMemoryBandwidth(const MemBwConfig& config, std::ostream& out);

}  // namespace gpu_bench
