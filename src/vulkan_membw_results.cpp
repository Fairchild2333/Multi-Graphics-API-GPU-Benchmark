#include "vulkan_membw_results.h"
#include "system_info.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
namespace gpu_bench {
BenchmarkResult MakeStoredVulkanMemBwResult(
    const gpu_bench::MemBwReport& report,
    const gpu_bench::MemBwConfig& config,
    const std::string& deviceName, std::uint32_t vramMB, const std::string& cpuName) {
    if (!report.valid || !report.dataVerified || report.writeCopyChecks == 0 || report.timestampAnomalies != 0)
        throw std::invalid_argument("Cannot save an unverified Vulkan memory sweep");
    gpu_bench::BenchmarkResult stored;
    stored.id = gpu_bench::GenerateResultId() + "-membw";
    stored.timestamp = gpu_bench::GenerateTimestamp();
    stored.resultSchemaVersion = 4;
    stored.appVersion = gpu_bench::CurrentAppVersion();
    stored.workload = gpu_bench::kMemBwWorkloadId;
    // The formal contract is the default sweep reaching the memory plateau.
    // A shortened or cache-bound sweep is kept in a separate preview group.
    const gpu_bench::MemBwConfig defaults;
    const bool formal = report.read.memoryPlateauReached &&
                        config.maxDatasetBytes == defaults.maxDatasetBytes &&
                        config.warmupRuns == defaults.warmupRuns &&
                        config.measuredRuns == defaults.measuredRuns;
    stored.workloadVersion = std::string(gpu_bench::kMemBwWorkloadVersion) +
                             (formal ? "" : "_preview");
    // A phone result never shares a group with a desktop one, as with the
    // other mobile tests.
#if defined(__ANDROID__)
    stored.workloadVersion = std::string(gpu_bench::kMemBwWorkloadVersion) + "_android_preview";
#elif defined(__OHOS__)
    stored.workloadVersion = std::string(gpu_bench::kMemBwWorkloadVersion) + "_ohos_preview";
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    stored.workloadVersion = std::string(gpu_bench::kMemBwWorkloadVersion) + "_ios_preview";
#endif
#ifdef GPU_BENCH_MOLTENVK
    stored.workloadVersion += "_moltenvk";
#endif
    stored.graphicsApi = "Vulkan";
    stored.deviceName = deviceName.empty() ? report.deviceName : deviceName;
    stored.driverVersion = report.driverVersion;
    stored.cpuName = cpuName;
    stored.osVersion = gpu_bench::CurrentOsVersion();
    stored.platform = gpu_bench::CurrentPlatform();
    stored.osArchitecture = gpu_bench::CurrentOsArchitecture();
    stored.processArchitecture = gpu_bench::CurrentProcessArchitecture();
    stored.memory = report.unifiedMemory ? "Unified-memory" : "Device-local";
    stored.vramMB = vramMB > 0 ? vramMB
                                   : report.deviceLocalHeapMB;
    stored.difficulty = "Sweep";
    stored.isSoftware = report.isSoftware;
    stored.headless = true;
    stored.framesInFlight = 0;
    stored.durationSec = report.durationSec;
    stored.timingSamples = static_cast<std::uint32_t>(
        report.points.size() * report.measuredRuns);
    stored.score = report.read.memoryGBps;
    stored.scoreUnit = "GB/s";
    stored.bottleneck = report.isSoftware ? "Software" :
        (report.read.memoryPlateauReached ? "Memory bandwidth" : "Memory plateau unverified");

    constexpr double kMiBf = 1024.0 * 1024.0;
    std::ostringstream contract;
    contract << std::fixed << std::setprecision(1)
             << "score=" << (report.read.memoryPlateauReached ? "read_memory_plateau" : "read_largest_datasets")
             << ";readMemoryGBps=" << report.read.memoryGBps
             << ";readCacheGBps=" << report.read.cacheGBps
             << ";readCacheFromMiB=" << report.read.cacheFromBytes / kMiBf
             << ";readCacheToMiB=" << report.read.cacheToBytes / kMiBf
             << ";readCacheKneeMiB=" << report.read.cacheKneeBytes / kMiBf
             << ";writeMemoryGBps=" << report.write.memoryGBps
             << ";writeCacheGBps=" << report.write.cacheGBps
             << ";copyMemoryGBps=" << report.copy.memoryGBps
             << ";copyCacheGBps=" << report.copy.cacheGBps
             << ";l1FullLoadGBps=" << report.l1.fullLoadGBps
             << ";l1FullLoadBlockKiB=" << report.l1.fullLoadBlockBytes / 1024
             << ";l1SingleGroupGBps=" << report.l1.singleGroupGBps
             << ";l1CapacityLowKiB=" << report.l1.capacityLowBytes / 1024
             << ";l1CapacityHighKiB=" << report.l1.capacityHighBytes / 1024
             << ";hostMeasured=" << (report.host.measured ? 1 : 0)
             << ";hostReadGBps=" << report.host.read.memoryGBps
             << ";hostWriteGBps=" << report.host.write.memoryGBps
             << ";hostCopyToDeviceGBps=" << report.host.copyToDeviceGBps
             << ";hostCopyToHostGBps=" << report.host.copyToHostGBps
             << ";memoryKind=" << (report.unifiedMemory ? "unified" : "device_local")
             << ";chunkKiB=" << report.chunkBytes / 1024
             << ";elementBytes=16"
             << ";maxDatasetMiB=" << report.maxDatasetBytes / (1024 * 1024)
             << ";datasetLimitedByDevice=" << (report.datasetLimitedByDevice ? 1 : 0)
             << ";memoryPlateauReached=" << (report.read.memoryPlateauReached ? 1 : 0)
             << ";warmupRuns=" << report.warmupRuns
             << ";measuredRuns=" << report.measuredRuns
             << ";aggregation=median"
             << ";timing=" << report.timingSource
             << ";timestampAnomalies=" << report.timestampAnomalies
             << ";dataVerified=" << (report.dataVerified ? 1 : 0)
             << ";writeCopyChecks=" << report.writeCopyChecks
             << ";resourceSynchronization=explicit_compute_transfer_host"
             << ";writeCopyDispatch=one_owner_per_chunk;writeCopyRepeats=in_shader;memoryAccess=volatile"
             << ";cacheAttribution=unverified;l1Attribution=workgroup_footprint_inference"
             << ";performanceQueryAdvertised=" << (report.performanceQueryAdvertised ? 1 : 0)
             << ";performanceCounterStatus=" << report.performanceCounterStatus
             << ";performanceCounterInventory=" << report.performanceCounterInventory;
    // Full curves as "<data set MiB>:<median GB/s>" pairs.
    for (const char* kernel : {"read", "write", "copy", "host_read", "host_write"}) {
        contract << ";curve_" << kernel << "=";
        bool first = true;
        for (const auto& point : report.points) {
            if (point.kernel != kernel) continue;
            if (!first) contract << ',';
            first = false;
            contract << point.datasetBytes / kMiBf << ':' << point.medianGBps;
        }
    }
    // L1 curves as "<private block KiB>:<median GB/s>" pairs.
    for (const char* kernel : {"l1", "l1_single"}) {
        contract << ";curve_" << kernel << "=";
        bool first = true;
        for (const auto& point : report.points) {
            if (point.kernel != kernel) continue;
            if (!first) contract << ',';
            first = false;
            contract << point.datasetBytes / 1024 << ':' << point.medianGBps;
        }
    }
#ifdef GPU_BENCH_MOLTENVK
    contract << ";apiImplementation=MoltenVK;translatedTo=Metal"
             << ";timingPolicy=moltenvk_synchronized_sweep_v1";
#endif
    stored.workloadConfig = contract.str();
    return stored;
}

}
