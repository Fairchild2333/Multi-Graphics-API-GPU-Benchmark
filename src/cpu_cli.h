#pragma once

#include "cpu_benchmark.h"
#include "cpu_membw.h"

namespace gpu_bench {

// CPU-only entry points shared by the desktop CLI and embedded Apple host.
// They preserve identical validation, result contracts and result storage.
int RunCpuBenchmarkCli(const CpuBenchmarkConfig& config, bool saveResult = true);
int RunCpuMemoryBandwidthCli(CpuMemBwConfig config, bool saveResult = true);
int RereadCpuMemoryBandwidthCli(const std::string& path);
int CpuCliMain(int argc, const char* const* argv);

}  // namespace gpu_bench
