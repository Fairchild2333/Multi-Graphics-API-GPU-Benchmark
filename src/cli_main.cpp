// Thin CLI entry point. All logic lives in the gpu_engine library so the GUI
// can link and drive it in-process. See src/gpu_engine.h.
#include "gpu_engine.h"

int main(int argc, char* argv[]) {
    return gpu_bench::cliMain(argc, argv);
}
