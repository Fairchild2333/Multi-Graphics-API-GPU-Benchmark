#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace gpu_bench {

inline constexpr const char* kCpuBenchmarkWorkloadVersion = "cpu_mixed_v1";

enum class CpuBenchmarkMode {
    PerCore,
    MultiCore,
    All,
};

struct CpuLogicalProcessor {
    std::uint32_t ordinal = 0;
    std::uint16_t group = 0;
    std::uint32_t logicalIndex = 0;
    std::uint32_t physicalCore = 0;
    std::uint32_t smtIndex = 0;
    std::uint32_t smtWidth = 1;
    std::uint32_t cpuSetId = 0;
    int efficiencyClass = -1;
    int performanceLevel = -1; // 0 is the fastest discovered class.
    std::string coreClass = "Unknown";
    std::string classificationSource = "unavailable";
    bool parked = false;
};

struct CpuBenchmarkConfig {
    CpuBenchmarkMode mode = CpuBenchmarkMode::All;
    double measureSeconds = 1.0;
    double warmupSeconds = 0.15;
    std::uint32_t roundCount = 3;
};

struct CpuCoreResult {
    CpuLogicalProcessor processor;
    double scoreMWorkPerSec = 0.0;
    double measuredSeconds = 0.0;
    std::uint64_t workUnits = 0;
    std::uint64_t checksum = 0;
    std::uint32_t roundCount = 0;
    std::uint32_t medianRound = 0;
    std::string affinityMode;
    bool valid = false;
};

struct CpuMultiCoreResult {
    double scoreMWorkPerSec = 0.0;
    double measuredSeconds = 0.0;
    std::uint64_t workUnits = 0;
    std::uint64_t checksum = 0;
    std::uint32_t threadCount = 0;
    std::uint32_t pinnedThreadCount = 0;
    std::uint32_t roundCount = 0;
    std::uint32_t medianRound = 0;
    std::string affinityMode;
    bool valid = false;
};

struct CpuBenchmarkReport {
    std::string workloadVersion = kCpuBenchmarkWorkloadVersion;
    std::string cpuName;
    std::string topologySource;
    std::string affinityCapability;
    std::vector<CpuLogicalProcessor> processors;
    std::vector<CpuCoreResult> perCore;
    CpuMultiCoreResult multiCore;
};

// Discovers CPU topology without initialising GLFW or any GPU API.
CpuBenchmarkReport ProbeCpuTopology();

// Prints CPU_META / CPU_TOPOLOGY machine-readable records.
void PrintCpuTopology(const CpuBenchmarkReport& topology, std::ostream& out);

// Runs the requested CPU-only benchmark and emits human-readable output plus
// TAB-separated CPU_PROGRESS / CPU_RESULT records. Every record is flushed so
// an in-process GUI can drive a live progress bar from redirected stdout.  The
// timed worker never formats or flushes output; progress is emitted by a
// low-frequency observer outside the measured hot path.
CpuBenchmarkReport RunCpuBenchmark(const CpuBenchmarkConfig& config,
                                   std::ostream& out);

} // namespace gpu_bench
