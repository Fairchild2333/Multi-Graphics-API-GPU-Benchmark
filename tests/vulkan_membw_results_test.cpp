#include "vulkan_membw_results.h"
#include "vulkan_timing_validation.h"
#include "result_quality.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
#include <stdexcept>

int main() {
    using namespace gpu_bench;
    MemBwConfig config;
    assert(kIosMemBwDefaultMaxBytes == 512ull << 20);
    assert(IosMemBwBufferBudget(kIosMemBwDefaultMaxBytes, 2048ull << 20) == 512ull << 20);
    assert(IosMemBwBufferBudget(kIosMemBwDefaultMaxBytes, 1024ull << 20) == 256ull << 20);
    assert(IosMemBwBufferBudget(kIosMemBwDefaultMaxBytes, 0) == 256ull << 20);
    assert(IosMemBwBufferBudget(64ull << 20, 2048ull << 20) == 64ull << 20);
    assert(!MemBwStreamingTailEligible(128ull << 20, 192ull << 20, 256ull << 20));
    assert(MemBwStreamingTailEligible(256ull << 20, 384ull << 20, 512ull << 20));
    MemBwReport report;
    report.valid = true;
    report.dataVerified = true;
    report.writeCopyChecks = 2;
    report.deviceName = "Test GPU";
    report.unifiedMemory = true;
    report.read.memoryGBps = 35.25;
    report.chunkBytes = 512 * 1024;
    report.maxDatasetBytes = 256ull << 20;
    report.warmupRuns = 3;
    report.measuredRuns = 10;
    report.timingSource = "gpu_timestamp";
    for (const auto* kernel : {"read", "write", "copy", "host_read", "host_write", "l1", "l1_single"}) {
        MemBwPoint point;
        point.kernel = kernel;
        point.datasetBytes = 1ull << 20;
        point.medianGBps = 123.45;
        report.points.push_back(point);
    }
    auto result = MakeStoredVulkanMemBwResult(report, config, "", 0, "Test CPU");
    assert(result.deviceName == "Test GPU" && result.cpuName == "Test CPU");
    assert(result.graphicsApi == "Vulkan" && result.workload == "membw");
    assert(result.score == 35.25 && result.scoreUnit == "GB/s");
    assert(result.headless && result.framesInFlight == 0 && result.timingSamples == 70);
    assert(result.workloadVersion.find("preview") != std::string::npos);
    auto budgetReport = report;
    budgetReport.memoryBudgetPolicy = kIosMemBwBudgetPolicy;
    budgetReport.availableMemoryBytes = 2048ull << 20;
    budgetReport.perBufferBudgetBytes = 512ull << 20;
    const auto budgetResult = MakeStoredVulkanMemBwResult(budgetReport, config);
    assert(ResultConfigValue(budgetResult, "memoryBudgetPolicy") == kIosMemBwBudgetPolicy);
    assert(ResultConfigValue(budgetResult, "availableAppMemoryBytes") == "2147483648");
    assert(ResultConfigValue(budgetResult, "perBufferBudgetBytes") == "536870912");
    for (const auto* kernel : {"read", "write", "copy", "host_read", "host_write"})
        assert(result.workloadConfig.find(std::string("curve_") + kernel + "=1.0:123.5") != std::string::npos);
    for (const auto* kernel : {"l1", "l1_single"})
        assert(result.workloadConfig.find(std::string("curve_") + kernel + "=1024:123.5") != std::string::npos);
    assert(result.workloadConfig.find("memoryPlateauReached=0") != std::string::npos);
    assert(result.workloadConfig.find("score=read_largest_datasets;") == 0);
    assert(result.workloadConfig.find("dataVerified=1;writeCopyChecks=2") != std::string::npos);
    assert(result.workloadVersion.find("membw_v3_streaming") == 0);
    assert(CurrentVulkanMemoryContractVerified(result));
    assert(!MeasurementReviewReasons(result).empty());
    auto oldTraversal = result;
    oldTraversal.workloadVersion = "membw_v2_verified_preview";
    assert(!CurrentVulkanMemoryContractVerified(oldTraversal));
    assert(!LegacyV1VulkanMemoryComparable(oldTraversal));
    // V1 with a plateau, saved after the data check, compares with V3.
    BenchmarkResult legacy;
    legacy.workload = "membw";
    legacy.graphicsApi = "Vulkan";
    legacy.workloadVersion = "membw_v1_android_preview";
    legacy.id = "20261010-213410-151-membw";
    legacy.workloadConfig = "score=read_largest_datasets;memoryPlateauReached=1;timestampAnomalies=0";
    assert(LegacyV1VulkanMemoryComparable(legacy));
    auto beforeCheck = legacy;
    beforeCheck.id = "20261007-231700-000-membw";
    assert(!LegacyV1VulkanMemoryComparable(beforeCheck));
    beforeCheck.id = "20261008-034335-000-membw";  // Switch, before the check existed
    assert(!LegacyV1VulkanMemoryComparable(beforeCheck));
    auto checkedBeforeCommit = legacy;
    checkedBeforeCommit.id = "20261008-044302-872-membw";  // Black Shark 3 Pro, checked
    assert(LegacyV1VulkanMemoryComparable(checkedBeforeCommit));
    auto noPlateau = legacy;
    noPlateau.workloadConfig = "memoryPlateauReached=0;timestampAnomalies=0";
    assert(!LegacyV1VulkanMemoryComparable(noPlateau));
    auto translated = legacy;
    translated.workloadVersion = "membw_v1_moltenvk";
    assert(!LegacyV1VulkanMemoryComparable(translated));
    assert(result.workloadConfig.find("largeDatasetTraversal=ordered_full_dataset_passes") != std::string::npos);
    assert(VulkanTimestampIntervalVerified(4, 4.2));
    assert(VulkanTimestampIntervalVerified(0.05, 0.25));
    assert(!VulkanTimestampIntervalVerified(0.02, 11)); // known under-reporting pattern
    assert(!VulkanTimestampIntervalVerified(20, 4));
    assert(!VulkanTimestampIntervalVerified(0, 4));
    assert(!VulkanTimestampIntervalVerified(std::nan(""), 4));
    assert(!VulkanTimestampIntervalVerified(4, std::nan("")));
    for (int failure = 0; failure < 3; ++failure) {
        auto bad = report;
        if (failure == 0) bad.valid = false;
        if (failure == 1) bad.dataVerified = false;
        if (failure == 2) bad.timestampAnomalies = 1;
        bool rejected = false;
        try { MakeStoredVulkanMemBwResult(bad, config); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    report.read.memoryPlateauReached = true;
    result = MakeStoredVulkanMemBwResult(report, config, "Named GPU", 1024);
    assert(result.deviceName == "Named GPU" && result.vramMB == 1024);
    assert(result.workloadVersion.find("preview") == std::string::npos);
    assert(MeasurementReviewReasons(result).empty());
    config.maxDatasetBytes = 256ull << 20;
    result = MakeStoredVulkanMemBwResult(report, config);
    assert(result.workloadVersion.find("preview") != std::string::npos);
    std::cout << "Vulkan result contract and all curve families preserved\n";
}
