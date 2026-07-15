#pragma once

#include <filesystem>

namespace gpu_bench::paths {

// GPU_BENCH_DATA_DIR overrides the platform default. The default is
// %LOCALAPPDATA%/GpuComputeBenchmark on Windows,
// ~/Library/Application Support/GpuComputeBenchmark on macOS, and
// $XDG_DATA_HOME/GpuComputeBenchmark (or ~/.local/share/...) elsewhere.
const std::filesystem::path& DataRoot();

std::filesystem::path ResultsDirectory();
std::filesystem::path CapturesDirectory();
std::filesystem::path ReportsDirectory();
std::filesystem::path LogsDirectory();

}  // namespace gpu_bench::paths
