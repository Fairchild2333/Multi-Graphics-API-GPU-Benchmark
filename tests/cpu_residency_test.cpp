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
    CpuThreadCounters threadBefore, threadAfter;
    threadBefore.available = threadAfter.available = true;
    threadAfter.cpuNs[1] = 1000000000;
    threadAfter.cycles[1] = 2000000000;
    threadAfter.workerBeforeNs = threadAfter.workerAfterNs = 1000000000;
    const auto directE = EvaluateCpuThreadCounters(threadBefore, threadAfter, CpuCoreType::Efficiency);
    if (!directE.verified() || directE.effectiveGHz != 2.0) return 8;
    if (EvaluateCpuThreadCounters(threadBefore, threadAfter, CpuCoreType::Performance).verified()) return 9;
    threadAfter.cpuNs[0] = 20000000; // A >1% P migration must fail even if E throughput looks plausible.
    threadAfter.workerBeforeNs = threadAfter.workerAfterNs = 1020000000;
    if (EvaluateCpuThreadCounters(threadBefore, threadAfter, CpuCoreType::Efficiency).verified()) return 10;
    threadBefore.cpuNs[0] = 30000000;
    if (EvaluateCpuThreadCounters(threadBefore, threadAfter, CpuCoreType::Efficiency).available) return 11;
    threadAfter.available = false;
    if (EvaluateCpuThreadCounters(threadBefore, threadAfter, CpuCoreType::Efficiency).verified()) return 12;
    threadBefore = {};
    threadBefore.available = threadAfter.available = true;
    threadAfter.cpuNs[0] = 0;
    threadAfter.workerBeforeNs = threadAfter.workerAfterNs = 1500000000;
    if (EvaluateCpuThreadCounters(threadBefore, threadAfter, CpuCoreType::Efficiency).verified()) return 13;
    // Four workers can exchange core types while both groups remain busy.
    // Their aggregate throughput must not acquire the single-core purity gate.
    std::vector<CpuResidency> workers(4);
    for (auto& worker : workers) {
        worker.available = true; worker.workerCpuNs = 5000000000.0;
    }
    workers[0].performanceFraction = .98; workers[1].performanceFraction = .97;
    workers[2].performanceFraction = .03; workers[3].performanceFraction = .02;
    std::vector<double> busy(4,1.0);
    if (!EvaluateCpuParticipation(workers,busy,2,2,5).verified()) return 14;
    workers[2].workerCpuNs = workers[3].workerCpuNs = 2500000000.0;
    if (EvaluateCpuParticipation(workers,busy,2,2,5).verified()) return 15;
    workers[2].workerCpuNs = workers[3].workerCpuNs = 5000000000.0;
    busy[3] = .70;
    if (EvaluateCpuParticipation(workers,busy,2,2,5).verified()) return 16;
    busy[3] = 1.0; workers[0].available = false;
    if (EvaluateCpuParticipation(workers,busy,2,2,5).verified()) return 17;
    std::cout << "CPU-type accounting rejects mixed/contaminated and unavailable observations.\n";
}
