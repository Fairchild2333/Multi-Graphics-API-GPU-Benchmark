#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#if defined(__aarch64__)
#if !TARGET_OS_IPHONE
#include <libproc.h>
#else
#include <mach/mach.h>
#endif
#include <mach/mach_time.h>
#include <mach/machine.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>
#endif
#endif

namespace gpu_bench {
enum class CpuCoreType { Automatic, Performance, Efficiency, Both };
inline const char* CpuCoreTypeName(CpuCoreType type) {
    switch (type) {
    case CpuCoreType::Performance: return "performance";
    case CpuCoreType::Efficiency: return "efficiency";
    case CpuCoreType::Both: return "both";
    default: return "auto";
    }
}
inline std::uint32_t AppleCoreTypeCount(CpuCoreType type) {
#if defined(__APPLE__) && !TARGET_OS_WATCH && !TARGET_OS_SIMULATOR && defined(__aarch64__)
    std::uint32_t count = 0;
    std::size_t size = sizeof(count);
    const char* key = type == CpuCoreType::Performance
        ? "hw.perflevel0.physicalcpu" : "hw.perflevel1.physicalcpu";
    if (sysctlbyname(key, &count, &size, nullptr, 0) == 0) return count;
#else
    (void)type;
#endif
    return 0;
}
inline bool CpuRequiresDirectTypeAccounting() {
#if defined(__APPLE__) && TARGET_OS_IPHONE && !TARGET_OS_WATCH && !TARGET_OS_SIMULATOR && defined(__aarch64__)
    static const bool required = [] {
        std::uint32_t family = 0;
        std::size_t size = sizeof(family);
        return sysctlbyname("hw.cpufamily", &family, &size, nullptr, 0) == 0 &&
            family == CPUFAMILY_ARM_VORTEX_TEMPEST &&
            AppleCoreTypeCount(CpuCoreType::Performance) == 4;
    }();
    return required;
#else
    return false;
#endif
}
inline const char* CpuForegroundEfficiencySchedulingName() {
    return CpuRequiresDirectTypeAccounting() ? "utility" : "user_initiated";
}
inline const char* CpuForegroundEfficiencyPolicyName() {
    return CpuRequiresDirectTypeAccounting() ? "foreground_utility_without_helpers"
                                            : "foreground_with_performance_load";
}
inline const char* CpuForegroundEfficiencyVersionSuffix() {
    return CpuRequiresDirectTypeAccounting() ? "_foreground_e_utility_v1" : "_foreground_e_p_load_v1";
}
inline bool CpuCoreTypeSchedulingSupported() {
#if defined(__APPLE__) && !TARGET_OS_WATCH && !TARGET_OS_SIMULATOR && defined(__aarch64__)
    // The subtraction below is valid only for a verified two-kind P/E machine.
    std::uint32_t levels = 0;
    std::size_t size = sizeof(levels);
    if (sysctlbyname("hw.nperflevels", &levels, &size, nullptr, 0) != 0 || levels != 2)
        return false;
    for (int level = 0; level < 2; ++level) {
        char name[32]{};
        size = sizeof(name);
        const std::string key = "hw.perflevel" + std::to_string(level) + ".name";
        if (sysctlbyname(key.c_str(), name, &size, nullptr, 0) != 0 ||
            std::string(name) != (level == 0 ? "Performance" : "Efficiency")) return false;
    }
#if TARGET_OS_IPHONE
    task_power_info_v2_data_t usage{};
    mach_msg_type_number_t count = TASK_POWER_INFO_V2_COUNT;
    return task_info(mach_task_self(), TASK_POWER_INFO_V2,
        reinterpret_cast<task_info_t>(&usage), &count) == KERN_SUCCESS &&
        count >= TASK_POWER_INFO_V2_COUNT;
#else
    rusage_info_v6 usage{};
    return proc_pid_rusage(getpid(), RUSAGE_INFO_V6,
        reinterpret_cast<rusage_info_t*>(&usage)) == 0;
#endif
#else
    return false;
#endif
}
inline const char* CpuResidencyMethod() {
#if defined(__APPLE__) && TARGET_OS_WATCH
    return "unavailable_watchos";
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    return CpuRequiresDirectTypeAccounting() ? "thread_selfcounts_perf_level_spi"
                                            : "mach_task_power_thread_clock_bound";
#else
    return "process_accounting_thread_clock_bound";
#endif
}

// Default policy covers both types whenever placement can be verified. It is
// shared by CLI, GUI workers and suite runs, without a user-facing selector.
inline CpuCoreType DefaultCpuCoreType() {
    return CpuCoreTypeSchedulingSupported() ? CpuCoreType::Both : CpuCoreType::Automatic;
}

class CpuQosScope {
public:
    explicit CpuQosScope(CpuCoreType type, bool foregroundEfficiency = false) {
        applied_ = type == CpuCoreType::Automatic;
#if defined(__APPLE__) && !TARGET_OS_WATCH && !TARGET_OS_SIMULATOR && defined(__aarch64__)
        if (type == CpuCoreType::Performance || type == CpuCoreType::Efficiency) {
            old_ = qos_class_self();
            applied_ = pthread_set_qos_class_self_np(type == CpuCoreType::Performance
                ? QOS_CLASS_USER_INTERACTIVE : foregroundEfficiency
                    ? (CpuRequiresDirectTypeAccounting() ? QOS_CLASS_UTILITY : QOS_CLASS_USER_INITIATED)
                    : QOS_CLASS_BACKGROUND, 0) == 0;
            changed_ = applied_;
        }
#endif
        (void)foregroundEfficiency;
    }
    ~CpuQosScope() {
#if defined(__APPLE__) && !TARGET_OS_WATCH && !TARGET_OS_SIMULATOR && defined(__aarch64__)
        if (changed_) pthread_set_qos_class_self_np(old_, 0);
#endif
    }
    bool applied() const { return applied_; }
private:
    bool applied_ = false;
#if defined(__APPLE__) && !TARGET_OS_WATCH && !TARGET_OS_SIMULATOR && defined(__aarch64__)
    qos_class_t old_ = QOS_CLASS_DEFAULT;
    bool changed_ = false;
#endif
};

struct CpuResidencySnapshot {
    bool available = false;
    std::uint64_t processNs = 0, performanceNs = 0;
    std::uint64_t workerBeforeNs = 0, workerAfterNs = 0;
    std::uint64_t cycles = 0, performanceCycles = 0;
};
inline CpuResidencySnapshot CaptureCpuResidency() {
    CpuResidencySnapshot result;
#if defined(__APPLE__) && !TARGET_OS_WATCH && !TARGET_OS_SIMULATOR && defined(__aarch64__)
    timespec before{}, after{};
#if TARGET_OS_IPHONE
    // Public iOS task accounting contains performance-core CPU time. The
    // kernel's task_ptime is the sum on RCT_CPU_PERFORMANCE, not wall time.
    // Bound the worker with thread CPU clocks exactly as on macOS; never infer
    // placement from QoS alone or from the score. iOS provides no cycle count.
    task_power_info_v2_data_t usage{};
    mach_msg_type_number_t count = TASK_POWER_INFO_V2_COUNT;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &before) != 0) return result;
    if (task_info(mach_task_self(), TASK_POWER_INFO_V2,
            reinterpret_cast<task_info_t>(&usage), &count) != KERN_SUCCESS ||
        count < TASK_POWER_INFO_V2_COUNT) return result;
#else
    rusage_info_v6 usage{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &before) != 0) return result;
    if (proc_pid_rusage(getpid(), RUSAGE_INFO_V6,
            reinterpret_cast<rusage_info_t*>(&usage)) != 0) return result;
#endif
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &after) != 0) return result;
    result.available = true;
    mach_timebase_info_data_t timebase{};
    if (mach_timebase_info(&timebase) != KERN_SUCCESS || timebase.denom == 0) return {};
    const auto nanos = [&](std::uint64_t ticks) {
        return std::uint64_t(static_cast<long double>(ticks) * timebase.numer / timebase.denom);
    };
#if TARGET_OS_IPHONE
    result.processNs = nanos(usage.cpu_energy.total_user + usage.cpu_energy.total_system);
    result.performanceNs = nanos(usage.task_ptime);
#else
    result.processNs = nanos(usage.ri_user_time + usage.ri_system_time);
    result.performanceNs = nanos(usage.ri_user_ptime + usage.ri_system_ptime);
    result.cycles = usage.ri_cycles;
    result.performanceCycles = usage.ri_pcycles;
#endif
    result.workerBeforeNs = std::uint64_t(before.tv_sec) * 1000000000ull + before.tv_nsec;
    result.workerAfterNs = std::uint64_t(after.tv_sec) * 1000000000ull + after.tv_nsec;
#endif
    return result;
}
struct CpuResidency {
    bool available = false;
    double performanceFraction = 0, requestedLowerBound = 0, effectiveGHz = 0;
    double processCpuNs = 0, workerCpuNs = 0;
    bool verified() const { return available && requestedLowerBound >= 0.99; }
};

// All-core work does not require each worker to remain on one core type.
// Verify actual participation of both core groups, independently of the
// stricter 99% purity contract used for single-core P/E measurements.
struct CpuParticipation {
    bool available = false;
    double performanceOccupancy = 0, efficiencyOccupancy = 0, minimumWorkerOccupancy = 0;
    bool verified() const {
        return available && performanceOccupancy >= .90 && performanceOccupancy <= 1.05 &&
            efficiencyOccupancy >= .90 && efficiencyOccupancy <= 1.05 &&
            minimumWorkerOccupancy >= .90 && minimumWorkerOccupancy <= 1.05;
    }
};
inline CpuParticipation EvaluateCpuParticipation(const std::vector<CpuResidency>& workers,
                                                 const std::vector<double>& busy,
                                                 std::uint32_t performanceCount,
                                                 std::uint32_t efficiencyCount,
                                                 double wallSeconds) {
    CpuParticipation result;
    if (!std::isfinite(wallSeconds) || wallSeconds <= 0 || !performanceCount || !efficiencyCount ||
        workers.size() != std::size_t(performanceCount) + efficiencyCount || busy.size() != workers.size()) return result;
    double performanceNs = 0, efficiencyNs = 0, minimumBusy = 1.05;
    for (std::size_t i = 0; i < workers.size(); ++i) {
        const auto& worker = workers[i];
        if (!worker.available || !std::isfinite(worker.workerCpuNs) || worker.workerCpuNs <= 0 ||
            !std::isfinite(worker.performanceFraction) || worker.performanceFraction < 0 || worker.performanceFraction > 1 ||
            !std::isfinite(busy[i]) || busy[i] < 0 || busy[i] > 1.05) return result;
        performanceNs += worker.workerCpuNs * worker.performanceFraction;
        efficiencyNs += worker.workerCpuNs * (1 - worker.performanceFraction);
        minimumBusy = std::min(minimumBusy, busy[i]);
    }
    result.available = true;
    result.performanceOccupancy = performanceNs / 1e9 / wallSeconds / performanceCount;
    result.efficiencyOccupancy = efficiencyNs / 1e9 / wallSeconds / efficiencyCount;
    result.minimumWorkerOccupancy = minimumBusy;
    return result;
}

// Recount's exported SPI is absent from the public SDK headers. Its layout
// and perf-level ordering are specified by Apple's resource_private.h and
// sys_recount.c. Resolve it at runtime, fail closed, and keep this method
// distinct from the public process-accounting bound in saved results.
struct CpuThreadCounters {
    bool available = false;
    std::uint64_t cpuNs[2]{}, cycles[2]{};
    std::uint64_t workerBeforeNs = 0, workerAfterNs = 0;
};
inline CpuThreadCounters CaptureCpuThreadCounters() {
    CpuThreadCounters result;
#if defined(__APPLE__) && !TARGET_OS_WATCH && !TARGET_OS_SIMULATOR && defined(__aarch64__)
    using Reader = int (*)(std::uint32_t, void*, std::size_t);
    static const auto reader = reinterpret_cast<Reader>(dlsym(RTLD_DEFAULT, "thread_selfcounts"));
    if (!reader || !CpuCoreTypeSchedulingSupported()) return result;
    struct Counts { std::uint64_t instructions, cycles, userMach, systemMach; } counts[2]{};
    static_assert(sizeof(Counts) == 32, "Recount time/CPI ABI");
    constexpr std::uint32_t timeCpiPerPerfLevel = 4;
    timespec before{}, after{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &before) != 0) return result;
    if (reader(timeCpiPerPerfLevel, counts, sizeof(counts)) != 0) return result;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &after) != 0) return result;
    mach_timebase_info_data_t timebase{};
    if (mach_timebase_info(&timebase) != KERN_SUCCESS || !timebase.denom) return result;
    for (unsigned i = 0; i < 2; ++i) {
        result.cpuNs[i] = static_cast<std::uint64_t>(
            static_cast<long double>(counts[i].userMach + counts[i].systemMach) * timebase.numer / timebase.denom);
        result.cycles[i] = counts[i].cycles;
    }
    result.workerBeforeNs = std::uint64_t(before.tv_sec) * 1000000000ull + before.tv_nsec;
    result.workerAfterNs = std::uint64_t(after.tv_sec) * 1000000000ull + after.tv_nsec;
    result.available = true;
#endif
    return result;
}
inline CpuResidency EvaluateCpuThreadCounters(const CpuThreadCounters& before,
                                               const CpuThreadCounters& after,
                                               CpuCoreType requested) {
    CpuResidency result;
    if (!before.available || !after.available ||
        after.workerBeforeNs <= before.workerAfterNs ||
        (requested != CpuCoreType::Performance && requested != CpuCoreType::Efficiency)) return result;
    double time[2]{}, cycles[2]{};
    for (unsigned i = 0; i < 2; ++i) {
        if (after.cpuNs[i] < before.cpuNs[i] || after.cycles[i] < before.cycles[i]) return result;
        time[i] = static_cast<double>(after.cpuNs[i] - before.cpuNs[i]);
        cycles[i] = static_cast<double>(after.cycles[i] - before.cycles[i]);
    }
    const double total = time[0] + time[1];
    if (total <= 100000.0) return result;
    const double lower = double(after.workerBeforeNs - before.workerAfterNs);
    const double upper = double(after.workerAfterNs - before.workerBeforeNs);
    // A changed SPI layout must not certify a core type. Independently check
    // conservation against the SDK's current-thread CPU-clock sandwich.
    if (total < lower - 50000.0 || total > upper + 50000.0) return result;
    const unsigned wanted = requested == CpuCoreType::Performance ? 0 : 1;
    result.available = true;
    result.workerCpuNs = total;
    result.performanceFraction = time[0] / total;
    // Charge an extra 50us against placement for the two boundary calls.
    result.requestedLowerBound = std::clamp((time[wanted] - 50000.0) / total, 0.0, 1.0);
    if (time[wanted] > 0.0) result.effectiveGHz = cycles[wanted] / time[wanted];
    return result;
}

inline bool DefaultCpuEfficiencyForeground() {
#if defined(__APPLE__) && TARGET_OS_IPHONE && !TARGET_OS_WATCH && !TARGET_OS_SIMULATOR && defined(__aarch64__)
    return CaptureCpuThreadCounters().available;
#else
    return false;
#endif
}
inline bool DefaultCpuMemoryEfficiencyForeground() {
    // The A12X utility worker passes sustained compute, but short memory
    // bursts did not meet the foreground occupancy contract. Keep the proven
    // background memory profile distinct from its utility compute profile.
    return !CpuRequiresDirectTypeAccounting() && DefaultCpuEfficiencyForeground();
}
inline CpuResidency EvaluateCpuResidency(const CpuResidencySnapshot& before,
                                         const CpuResidencySnapshot& after,
                                         CpuCoreType requested) {
    CpuResidency r;
    if (!before.available || !after.available || after.processNs <= before.processNs ||
        after.performanceNs < before.performanceNs || after.cycles < before.cycles ||
        after.performanceCycles < before.performanceCycles ||
        after.workerBeforeNs <= before.workerAfterNs ||
        (requested != CpuCoreType::Performance && requested != CpuCoreType::Efficiency)) return r;
    const double total = double(after.processNs - before.processNs);
    const double rawP = double(after.performanceNs - before.performanceNs);
    // Independently converting cumulative Mach counters can make a delta
    // exceed the total by one nanosecond. Reject material inconsistencies.
    if (rawP > total + 2.0) return r;
    const double p = std::min(rawP, total);
    const double lower = double(after.workerBeforeNs - before.workerAfterNs);
    const double upper = double(after.workerAfterNs - before.workerBeforeNs);
    r.processCpuNs = total;
    r.workerCpuNs = lower;
    r.performanceFraction = p / total;
    // Sandwich the process snapshot between thread CPU-clock reads. Attribute
    // ALL other-thread CPU time against the requested class, with an extra
    // 50 us accounting margin. Thus an active observer/GUI cannot certify a
    // worker's placement merely by spending time on the requested core type.
    constexpr double margin = 50000;
    if (lower > total + margin || upper <= 0) return r;
    const double otherMaximum = std::max(0.0, total - lower) + margin;
    const double wanted = requested == CpuCoreType::Performance ? p : total - p;
    r.available = true;
    r.performanceFraction = p / total;
    r.requestedLowerBound = std::clamp((wanted - otherMaximum - margin) / upper, 0.0, 1.0);
    const double pCycles = double(after.performanceCycles - before.performanceCycles);
    const double allCycles = double(after.cycles - before.cycles);
    if (wanted > 0 && pCycles <= allCycles)
        r.effectiveGHz = (requested == CpuCoreType::Performance ? pCycles : allCycles - pCycles) / wanted;
    return r;
}
} // namespace gpu_bench
