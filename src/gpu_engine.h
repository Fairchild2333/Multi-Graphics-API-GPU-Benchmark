#pragma once

// Public entry point of the gpu_engine static library. Both the CLI executable
// and the WinUI 3 (C++/WinRT) GUI link gpu_engine and call cliMain in-process —
// the GUI runs it on a worker thread with synthesized argv, so the benchmark
// runs in the GUI's own process (no subprocess). See winui/ and docs/.

namespace gpu_bench {

// Runs the full benchmark CLI (probe GPUs, parse args, run, persist results).
// argv/argc follow the usual C convention. Returns a process-style exit code.
int cliMain(int argc, char* argv[]);

}  // namespace gpu_bench
