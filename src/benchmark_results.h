#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gpu_bench {

struct BenchmarkResult {
    std::string id;
    std::string timestamp;

    std::uint32_t resultSchemaVersion = 1;
    std::string appVersion;       // Mangekyo build that produced this result
    std::string workload = "stream";
    std::string workloadVersion;  // stable score contract, e.g. "gpu_stress_v1"
    std::string workloadConfig;   // reproducibility parameters, key=value pairs
    std::string graphicsApi;
    std::string deviceName;
    std::string driverVersion;
    std::string cpuName;
    std::string osVersion;
    std::string platform;
    std::string osArchitecture;
    std::string processArchitecture;
    std::string memory;
    std::uint32_t vramMB = 0;          // dedicated VRAM (0 = unknown / shared)
    std::uint32_t resWidth  = 0;
    std::uint32_t resHeight = 0;
    std::uint32_t particleCount = 0;
    std::string difficulty;
    bool vsync      = false;
    bool isSoftware = false;
    bool headless   = false;
    std::uint32_t framesInFlight = 2;

    double durationSec   = 0.0;
    double warmupSec     = 0.0;
    std::uint32_t measuredFrames  = 0;
    std::uint32_t timingSamples   = 0;

    double avgComputeMs  = 0.0, minComputeMs  = 0.0, maxComputeMs  = 0.0;
    double avgRenderMs   = 0.0, minRenderMs   = 0.0, maxRenderMs   = 0.0;
    double avgTotalGpuMs = 0.0, minTotalGpuMs = 0.0, maxTotalGpuMs = 0.0;

    double avgFps          = 0.0;
    double avgFrameTimeMs  = 0.0;
    double gpuUtilisation  = 0.0;
    std::string bottleneck;

    // Derived per-workload axis metric (bandwidth / compute / fill / peak).
    double      score      = 0.0;
    std::string scoreUnit;        // e.g. "GB/s", "GFLOP/s", "G-iter/s", "GFLOPS", "GIOPS"
    std::string precision;        // SynthPeak only: "FP32"/"FP16"/"FP64"/"INT32"

    // Thermal-stability telemetry (only populated when the run had >=5 1s windows).
    // stableScore: trailing-5 mean once CV<2%; 0.0 if never stable.
    // stableVariancePct: CV(%) at the stable point; -1 if never stable.
    // throttlePct: (early5Mean - late5Mean)/early5Mean * 100; positive = throttled.
    double stableScore        = 0.0;
    double stableVariancePct  = -1.0;
    double throttlePct        = 0.0;
};

std::string ResultsFilePath();

std::string GenerateResultId();
std::string GenerateTimestamp();
std::string CurrentAppVersion();
std::string CurrentOsVersion();
std::string CurrentPlatform();
std::string CurrentOsArchitecture();
std::string CurrentProcessArchitecture();

std::vector<BenchmarkResult> LoadResults();
bool SaveResults(const std::vector<BenchmarkResult>& results);
bool AppendResult(const BenchmarkResult& r);
bool DeleteResult(const std::string& id);
bool ClearResults();

void PrintResultsTable(const std::vector<BenchmarkResult>& results);
void PrintComparisonTable(const std::vector<BenchmarkResult>& results);
void PrintDetailedComparison(const BenchmarkResult& a, const BenchmarkResult& b);
bool ExportResultsCsv(const std::string& path,
                      const std::vector<BenchmarkResult>& results);

}  // namespace gpu_bench
