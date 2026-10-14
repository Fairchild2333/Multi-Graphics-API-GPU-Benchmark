#pragma once
#include <cmath>

namespace gpu_bench {
// A synchronized submit/fence interval contains its GPU interval. A very
// short reported GPU interval within a long dispatch is conservatively
// rejected too; CPU scheduling can cause a false rejection, never a score.
inline bool VulkanTimestampIntervalVerified(double gpuMs, double wallMs) {
    return std::isfinite(gpuMs) && std::isfinite(wallMs) && gpuMs > 0 && wallMs > 0 &&
        gpuMs <= wallMs * 1.1 + 0.1 &&
        !(gpuMs < wallMs * 0.5 && wallMs - gpuMs > 2.0);
}
}
