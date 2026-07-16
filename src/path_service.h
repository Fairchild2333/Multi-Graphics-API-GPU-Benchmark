#pragma once

#include <filesystem>

namespace gpu_bench::paths {

// GPU_BENCH_DATA_DIR overrides the platform default. The on-disk default
// intentionally keeps the pre-Mangekyo product identifier:
// %LOCALAPPDATA%/GpuComputeBenchmark on Windows,
// ~/Library/Application Support/GpuComputeBenchmark on macOS, and
// $XDG_DATA_HOME/GpuComputeBenchmark (or ~/.local/share/...) elsewhere.
// Keeping this stable preserves existing results, captures and reports across
// the user-facing rename. GPU_BENCH_DATA_DIR remains the supported override.
const std::filesystem::path& DataRoot();

std::filesystem::path ResultsDirectory();
std::filesystem::path CapturesDirectory();
std::filesystem::path ReportsDirectory();
std::filesystem::path LogsDirectory();

}  // namespace gpu_bench::paths
