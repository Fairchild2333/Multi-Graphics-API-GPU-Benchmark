#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE && defined(__aarch64__)
#include <libproc.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>
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
#if defined(__APPLE__) && !TARGET_OS_IPHONE && defined(__aarch64__)
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
inline bool CpuCoreTypeSchedulingSupported() {
#if defined(__APPLE__) && !TARGET_OS_IPHONE && defined(__aarch64__)
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
    rusage_info_v6 usage{};
    return proc_pid_rusage(getpid(), RUSAGE_INFO_V6,
        reinterpret_cast<rusage_info_t*>(&usage)) == 0;
#else
    return false;
#endif
}

// Default policy covers both types whenever placement can be verified. It is
// shared by CLI, GUI workers and suite runs, without a user-facing selector.
inline CpuCoreType DefaultCpuCoreType() {
    return CpuCoreTypeSchedulingSupported() ? CpuCoreType::Both : CpuCoreType::Automatic;
}

class CpuQosScope {
public:
    explicit CpuQosScope(CpuCoreType type) {
        applied_ = type == CpuCoreType::Automatic;
#if defined(__APPLE__) && !TARGET_OS_IPHONE && defined(__aarch64__)
        if (type == CpuCoreType::Performance || type == CpuCoreType::Efficiency) {
            old_ = qos_class_self();
            applied_ = pthread_set_qos_class_self_np(type == CpuCoreType::Performance
                ? QOS_CLASS_USER_INTERACTIVE : QOS_CLASS_BACKGROUND, 0) == 0;
            changed_ = applied_;
        }
#endif
    }
    ~CpuQosScope() {
#if defined(__APPLE__) && !TARGET_OS_IPHONE && defined(__aarch64__)
        if (changed_) pthread_set_qos_class_self_np(old_, 0);
#endif
    }
    bool applied() const { return applied_; }
private:
    bool applied_ = false;
#if defined(__APPLE__) && !TARGET_OS_IPHONE && defined(__aarch64__)
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
#if defined(__APPLE__) && !TARGET_OS_IPHONE && defined(__aarch64__)
    timespec before{}, after{};
    rusage_info_v6 usage{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &before) != 0) return result;
    if (proc_pid_rusage(getpid(), RUSAGE_INFO_V6,
            reinterpret_cast<rusage_info_t*>(&usage)) != 0) return result;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &after) != 0) return result;
    result.available = true;
    mach_timebase_info_data_t timebase{};
    if (mach_timebase_info(&timebase) != KERN_SUCCESS || timebase.denom == 0) return {};
    const auto nanos = [&](std::uint64_t ticks) {
        return std::uint64_t(static_cast<long double>(ticks) * timebase.numer / timebase.denom);
    };
    result.processNs = nanos(usage.ri_user_time + usage.ri_system_time);
    result.performanceNs = nanos(usage.ri_user_ptime + usage.ri_system_ptime);
    result.cycles = usage.ri_cycles;
    result.performanceCycles = usage.ri_pcycles;
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
