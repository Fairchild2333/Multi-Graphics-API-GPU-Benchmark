#pragma once
#include <string>
namespace gpu_bench {
// Probe actual devices and buffer limits while the embedded engine is idle.
std::string EmbeddedGpuCapabilitiesJson();
}
