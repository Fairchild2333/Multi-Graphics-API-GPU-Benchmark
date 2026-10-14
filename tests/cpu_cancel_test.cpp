#include "cpu_cancel.h"
#include "cpu_benchmark.h"
#include "cpu_membw.h"
#include <future>
#include <sstream>
#include <thread>
#include <iostream>

int main() {
    using namespace gpu_bench;
    for (auto mode : {CpuBenchmarkMode::MultiCore, CpuBenchmarkMode::PerCore}) {
        ClearCpuStop();
        auto run = std::async(std::launch::async, [mode] {
            CpuBenchmarkConfig config;
            config.mode = mode; config.measureSeconds = 60; config.warmupSeconds = 60;
            std::ostringstream output;
            return RunCpuBenchmark(config, output);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        RequestCpuStop();
        if (run.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
            std::cerr << "CPU cancellation did not interrupt the warmup\n";
            std::terminate();
        }
        auto report = run.get();
        if (report.multiCore.valid || !report.perCore.empty()) return 1;
    }
    if (CpuCoreTypeSchedulingSupported() && CaptureCpuThreadCounters().available) {
        ClearCpuStop();
        auto foreground = std::async(std::launch::async, [] {
            CpuBenchmarkConfig config;
            config.mode = CpuBenchmarkMode::PerCore;
            config.coreType = CpuCoreType::Efficiency;
            config.efficiencyForeground = true;
            config.measureSeconds = 60; config.warmupSeconds = 60;
            std::ostringstream output;
            return RunCpuBenchmark(config, output);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        RequestCpuStop();
        if (foreground.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
            std::cerr << "Foreground E cancellation did not join its P-load helpers\n";
            std::terminate();
        }
        if (!foreground.get().perCore.empty()) return 3;
        ClearCpuStop();
        auto foregroundMemory = std::async(std::launch::async, [] {
            CpuMemBwConfig config;
            config.coreType = CpuCoreType::Efficiency;
            config.efficiencyForeground = true;
            config.maxDatasetBytes = 8u << 20;
            config.storageTest = false;
            config.sampleMs = 1000; config.warmupRuns = 1; config.measuredRuns = 1;
            std::ostringstream output;
            return RunCpuMemoryBandwidth(config, output);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        RequestCpuStop();
        if (foregroundMemory.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
            std::cerr << "Foreground E memory cancellation did not join helpers\n";
            std::terminate();
        }
        if (foregroundMemory.get().valid) return 4;
    }
    ClearCpuStop();
    auto memory = std::async(std::launch::async, [] {
        CpuMemBwConfig config;
        config.maxDatasetBytes = 8u * 1024u * 1024u; config.storageTest = false;
        config.sampleMs = 1000; config.warmupRuns = 1; config.measuredRuns = 1;
        std::ostringstream output;
        return RunCpuMemoryBandwidth(config, output);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    RequestCpuStop();
    if (memory.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
        std::cerr << "Memory cancellation did not stop after the current sample\n";
        std::terminate();
    }
    const auto report = memory.get();
    if (report.valid) return 2;
    ClearCpuStop();
}
