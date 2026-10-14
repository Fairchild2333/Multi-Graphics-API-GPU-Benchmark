#include "cpu_scheduling.h"
#include <iostream>

int main() {
    using namespace gpu_bench;
    CpuResidencySnapshot before, after;
    before.available = after.available = true;
    after.processNs = 1000000000;
    after.performanceNs = 1000000000;
    after.workerBeforeNs = after.workerAfterNs = 1000000000;
    after.cycles = after.performanceCycles = 4000000000;
    auto p = EvaluateCpuResidency(before, after, CpuCoreType::Performance);
    if (!p.verified() || p.effectiveGHz != 4.0) return 1;
    if (EvaluateCpuResidency(before, after, CpuCoreType::Efficiency).verified()) return 2;
    after.performanceNs = after.performanceCycles = 0;
    if (!EvaluateCpuResidency(before, after, CpuCoreType::Efficiency).verified()) return 3;
    // Half the process time belongs to OTHER threads on P cores. That must
    // never certify a worker which could have spent all its time on E cores.
    after.performanceNs = 500000000;
    after.workerBeforeNs = after.workerAfterNs = 500000000;
    if (EvaluateCpuResidency(before, after, CpuCoreType::Performance).verified()) return 4;
    // A mixed-core worker and missing/inconsistent accounting also fail closed.
    after.workerBeforeNs = after.workerAfterNs = 1000000000;
    if (EvaluateCpuResidency(before, after, CpuCoreType::Performance).verified()) return 5;
    after.performanceNs = 1000100000;
    if (EvaluateCpuResidency(before, after, CpuCoreType::Performance).available) return 6;
    after.available = false;
    if (EvaluateCpuResidency(before, after, CpuCoreType::Performance).verified()) return 7;
    std::cout << "CPU-type accounting rejects mixed/contaminated and unavailable observations.\n";
}
