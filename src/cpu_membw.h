#pragma once

#include <cstdint>
#include <iosfwd>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#include <string>
#include <vector>

namespace gpu_bench {

inline constexpr const char* kCpuMemBwWorkloadId = "cpu_membw";
inline constexpr const char* kCpuMemBwWorkloadVersion = "cpu_membw_v1";

// Phones have smaller caches and less free memory than desktops, so their
// sweep ends earlier.
#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
inline constexpr std::uint64_t kCpuMemBwDefaultMaxBytes = 256ull << 20;
#else
inline constexpr std::uint64_t kCpuMemBwDefaultMaxBytes = 1024ull << 20;
#endif

struct CpuMemBwConfig {
    // Largest data set of the single-thread sweeps. The all-core sweep gives
    // every thread a quarter of it. Both are lowered to fit free memory.
    std::uint64_t maxDatasetBytes = kCpuMemBwDefaultMaxBytes;
    // 0 selects the widest vectors the CPU supports; 16, 32 or 64 forces one.
    std::uint32_t vectorBytes = 0;
    std::uint32_t warmupRuns = 2;
    std::uint32_t measuredRuns = 7;
    double sampleMs = 10.0;
    // Read the levels off the shape of the curve even where the OS reports
    // cache sizes. Systems that report none always use the curve.
    bool levelsFromCurve = false;
    // Storage test: sequential and random 4 KiB read and write of a temporary
    // file. An empty directory selects the temp directory on Windows and the
    // working directory elsewhere.
    bool storageTest = true;
    std::string storageDir;
    std::uint64_t storageFileBytes = 1024ull << 20;
    // Diagnostics for exercising the mixed-core and unreported-cache paths on
    // a CPU with one core type. Both make a stored result a preview.
    std::uint32_t debugSplitClasses = 0;  // treat the cores as this many classes
    bool debugHideLastCache = false;      // ignore the last cache the OS reports
};

// One data or unified cache of the reference core.
struct CpuCacheLevel {
    std::uint32_t level = 0;
    std::uint64_t sizeBytes = 0;
    std::uint32_t sharedByThreads = 1;
    std::uint32_t sharedByCores = 1;
    std::uint32_t lineBytes = 0;
};

struct CpuMemBwPoint {
    std::string mode;    // single | single_<class rank> | all
    std::string kernel;  // read | write | copy | write_nt | copy_nt | zero
    std::uint64_t blockBytes = 0;  // data set of one thread
    std::uint32_t threads = 1;
    double medianGBps = 0.0;
    double bestGBps = 0.0;
};

// Bandwidth of one level of the hierarchy: the median over the data sets that
// fit that level and no smaller one.
struct CpuMemBwLevelSummary {
    std::string name;             // L1 | L2 | L3 | L4 | memory
    std::uint64_t cacheBytes = 0;  // capacity of one cache instance, 0 for memory
    std::uint64_t fromBytes = 0;
    std::uint64_t toBytes = 0;
    double readGBps = 0.0;
    double writeGBps = 0.0;
    double copyGBps = 0.0;
    // Slowest and fastest read data set of the level.
    double readLowGBps = 0.0;
    double readHighGBps = 0.0;
    // Non-temporal stores, memory level on x86-64 only.
    double writeStreamGBps = 0.0;
    double copyStreamGBps = 0.0;
    // CLZERO zero fill, memory level on AMD x86-64 only.
    double zeroFillGBps = 0.0;
    // The level was read off a plateau of the curve because the OS reports no
    // size for it. Its capacity lies between these two data sets; the upper
    // one is 0 when the sweep ends before the next plateau.
    bool fromCurve = false;
    std::uint64_t capacityLowBytes = 0;
    std::uint64_t capacityHighBytes = 0;
    bool measured = false;
    // Memory level: the data sets used agree with each other.
    bool settled = true;
};

struct CpuMemBwModeSummary {
    std::string mode;
    std::uint32_t threads = 0;
    std::uint32_t pinnedThreads = 0;
    std::uint64_t maxBlockBytes = 0;
    std::vector<CpuMemBwLevelSummary> levels;
};

// One class of cores (performance, efficiency, ...) with the single-thread
// sweep of its reference core.
struct CpuMemBwClass {
    std::uint32_t rank = 0;  // 0 is the fastest class
    std::string name;
    std::uint32_t coreCount = 0;
    std::uint32_t referenceLogicalIndex = 0;
    std::vector<CpuCacheLevel> caches;
    std::string cacheSource;  // the OS interface, or "curve"
    CpuMemBwModeSummary summary;
};

// Speed of the drive that virtual memory pages to, or of the directory the
// test was pointed at.
struct CpuMemBwStorage {
    bool measured = false;
    std::string skippedReason;
    std::string backing;    // what the OS pages to
    std::string directory;  // where the test file was written
    bool onPagingDrive = false;
    bool directIo = false;  // false: the OS file cache may inflate the figures
    std::uint64_t fileBytes = 0;
    double seqReadMBps = 0.0;
    double seqWriteMBps = 0.0;
    double randReadMBps = 0.0;
    double randWriteMBps = 0.0;
    double randReadIops = 0.0;
    double randWriteIops = 0.0;
};

// Cost of mapping new pages: the first write to every page of a fresh region.
struct CpuMemBwPageFault {
    bool measured = false;
    bool transparentHugePages = false;
    std::uint64_t pageBytes = 0;
    std::uint64_t regionBytes = 0;
    double gbps = 0.0;
    double nsPerPage = 0.0;
};

struct CpuMemBwReport {
    bool valid = false;
    std::string error;
    std::string cpuName;
    std::string vectorName;
    std::uint32_t vectorBytes = 0;
    bool streamingStores = false;
    bool zeroFill = false;
    std::uint32_t logicalProcessors = 0;
    std::uint32_t physicalCores = 0;
    std::uint32_t referenceLogicalIndex = 0;
    bool mixedCoreTypes = false;
    std::string affinity;  // strict | partial | failed | scheduler_managed
    std::vector<CpuCacheLevel> caches;
    std::string cacheSource;
    std::uint64_t physicalMemoryBytes = 0;
    std::uint64_t availableMemoryBytes = 0;
    bool datasetLimitedByMemory = false;
    std::uint32_t warmupRuns = 0;
    std::uint32_t measuredRuns = 0;
    double sampleMs = 0.0;
    // Points measured again because other activity disturbed the first round.
    std::uint32_t remeasuredPoints = 0;
    double durationSec = 0.0;
    std::vector<CpuMemBwPoint> points;
    // One entry per core class, fastest first; `single` is the first entry.
    std::vector<CpuMemBwClass> classes;
    CpuMemBwModeSummary single;
    CpuMemBwModeSummary all;
    CpuMemBwPageFault pageFault;
    CpuMemBwStorage storage;
};

// Sweeps the data-set size from 4 KiB upwards with read, write and copy
// kernels: on one pinned thread of every core class, then with one thread per
// physical core. The points are assigned to L1-L4 and memory from the cache
// sizes the OS reports, or from the plateaus of the curve where it reports
// none. Also times page mapping and the drive behind virtual memory. Prints
// human-readable tables plus tab-separated CPUMEM_* records.
CpuMemBwReport RunCpuMemoryBandwidth(const CpuMemBwConfig& config, std::ostream& out);

}  // namespace gpu_bench
