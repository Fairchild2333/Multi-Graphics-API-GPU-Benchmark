// Stand-alone entry point for the CPU cache / memory bandwidth sweep, for
// targets where the full gpu_benchmark CLI is not built, such as an Android
// phone reached over adb. It prints what `gpu_benchmark --cpu-membw` prints
// and saves nothing. Build it with scripts/build-android-cpumem.ps1.

#include "cpu_membw.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void PrintUsage() {
    std::cerr << "Usage: mangekyo_cpumem [options]\n"
              << "  --max-mib <MiB>       largest single-thread data set (1-65536)\n"
              << "  --curve-levels        read the cache levels off the curve, not from the OS\n"
              << "  --no-storage          skip the storage test (writes no file)\n"
              << "  --storage-dir <path>  directory for the storage test file (default: .)\n"
              << "  --storage-mib <MiB>   size of the storage test file (32-65536)\n";
}

// Parses a whole number in [low, high]. False on anything else.
bool ParseMiB(const char* text, unsigned long low, unsigned long high, std::uint64_t& bytes) {
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value < low || value > high) return false;
    bytes = static_cast<std::uint64_t>(value) << 20;
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    gpu_bench::CpuMemBwConfig config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--max-mib" && hasValue) {
            if (!ParseMiB(argv[++i], 1, 65536, config.maxDatasetBytes)) {
                PrintUsage();
                return 2;
            }
        } else if (arg == "--curve-levels") {
            config.levelsFromCurve = true;
        } else if (arg == "--no-storage") {
            config.storageTest = false;
        } else if (arg == "--storage-dir" && hasValue) {
            config.storageDir = argv[++i];
        } else if (arg == "--storage-mib" && hasValue) {
            if (!ParseMiB(argv[++i], 32, 65536, config.storageFileBytes)) {
                PrintUsage();
                return 2;
            }
        } else {
            PrintUsage();
            return 2;
        }
    }

    const gpu_bench::CpuMemBwReport report = gpu_bench::RunCpuMemoryBandwidth(config, std::cout);
    if (!report.valid) {
        std::cerr << "CPU memory bandwidth sweep failed: "
                  << (report.error.empty() ? "no valid result" : report.error) << "\n";
        return 1;
    }
    return 0;
}
