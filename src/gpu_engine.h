#pragma once

// Public entry point of the gpu_engine static library. The CLI executable uses
// this directly, and the WinUI executable keeps it for explicit command-line
// forwarding. Interactive GUI benchmarks are isolated gpu_benchmark.exe child
// processes so driver/capture-tool faults cannot take down the WinUI shell.

namespace gpu_bench {

// Runs the full benchmark CLI (probe GPUs, parse args, run, persist results).
// argv/argc follow the usual C convention. Returns a process-style exit code.
int cliMain(int argc, char* argv[]);

// Legacy embedding hook: when true, cliMain() skips glfwTerminate() on exit.
inline bool skipGlfwTerminate = false;

}  // namespace gpu_bench
