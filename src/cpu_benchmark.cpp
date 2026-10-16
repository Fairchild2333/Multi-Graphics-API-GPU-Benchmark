#include "cpu_benchmark.h"
#include "cpu_cancel.h"

#include "cpu_parked_cores.h"
#include "system_info.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <mutex>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <thread>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <intrin.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <unistd.h>
#elif defined(__linux__) || defined(__ANDROID__)
#include <cerrno>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif
#endif

namespace gpu_bench {
namespace {

using Clock = std::chrono::steady_clock;
constexpr std::uint64_t kKernelBatch = 4096;
constexpr std::uint64_t kPerCoreKernelSeed = 0x4350555f53494e47ull;
constexpr auto kSingleProgressPeriod = std::chrono::milliseconds(250);
// Covers the 64-byte cache lines common on x86 and the 128-byte lines used by
// current Apple silicon.  Each multi-core worker publishes only once after the
// timed loop, but isolation also prevents the final writes from ping-ponging.
constexpr std::size_t kWorkerIsolationBytes = 128;
std::atomic<std::uint64_t> g_cpuBenchmarkSink{0};

struct KernelState {
    std::uint64_t a;
    std::uint64_t b;
    float f;
    double d;
    std::uint64_t checksum;
};

static std::uint64_t RotL(std::uint64_t value, unsigned shift) {
    return (value << shift) | (value >> (64u - shift));
}

// Keeps the kernel's data-dependent branch a branch on arm64 macOS. Clang
// targeting it (default CPU apple-m1) turned both arms into conditional
// selects (csel/fcsel), so an M4 Pro paid no misprediction where every other
// platform pays for one in half the work units, and scored about 1.6 times as
// much per clock (2026-10-16). MSVC (x64 and ARM64), the Android NDK and
// HarmonyOS keep the branch in the timed loops, and so does iOS. Elsewhere the
// barrier stays out: on 32-bit x86 Android it changed the register allocation
// of the timed loop, and every earlier result must keep its exact code. The
// two statements must differ: identical ones were hoisted above the branch
// and the selects came back.
#if defined(__APPLE__) && defined(__aarch64__) && !TARGET_OS_IPHONE
#define GPU_BENCH_KEEP_BRANCH(text) __asm__ __volatile__(text)
#else
#define GPU_BENCH_KEEP_BRANCH(text) ((void)0)
#endif

// One work unit deliberately combines integer dependency chains, branches,
// FP32 and FP64 arithmetic. State is carried between batches and consumed by
// an atomic sink after measurement, preventing dead-code elimination without
// turning the workload into a memory-bandwidth benchmark.
static void RunKernelBatch(KernelState& state) {
    std::uint64_t a = state.a;
    std::uint64_t b = state.b;
    float f = state.f;
    double d = state.d;

    for (std::uint64_t i = 0; i < kKernelBatch; ++i) {
        a = a * 6364136223846793005ull + 1442695040888963407ull;
        b ^= RotL(a + b + i, 17);
        b *= 0xd6e8feb86659fd93ull;

        const float inputF = static_cast<float>((a >> 40) & 0xffu) * (1.0f / 255.0f);
        const double inputD = static_cast<double>((b >> 39) & 0x1ffu) * (1.0 / 511.0);
        if ((a ^ b) & 1ull) {
            GPU_BENCH_KEEP_BRANCH(" ");
            f = f * 0.99991f + inputF * 0.00037f;
            d = d * 0.9999991 + inputD * 0.0000037;
            b ^= RotL(a, 29);
        } else {
            GPU_BENCH_KEEP_BRANCH("  ");
            f = f * 0.99973f - inputF * 0.00019f;
            d = d * 0.9999973 - inputD * 0.0000019;
            a ^= RotL(b, 11);
        }
    }

    std::uint32_t fBits = 0;
    std::uint64_t dBits = 0;
    std::memcpy(&fBits, &f, sizeof(fBits));
    std::memcpy(&dBits, &d, sizeof(dBits));
    state.a = a;
    state.b = b;
    state.f = f;
    state.d = d;
    state.checksum ^= a ^ RotL(b, 7) ^ dBits ^ static_cast<std::uint64_t>(fBits);
}

static KernelState MakeKernelState(std::uint64_t seed) {
    KernelState state{};
    state.a = 0x243f6a8885a308d3ull ^ (seed * 0x9e3779b97f4a7c15ull);
    state.b = 0x13198a2e03707344ull + (seed * 0xbf58476d1ce4e5b9ull);
    state.f = 0.625f + static_cast<float>(seed & 7u) * 0.015625f;
    state.d = 0.875 + static_cast<double>(seed & 15u) * 0.0078125;
    state.checksum = seed;
    return state;
}

static std::string SanitizeField(std::string value) {
    for (char& c : value) {
        if (c == '\t' || c == '\r' || c == '\n' || c == '=') c = '_';
    }
    return value;
}

static std::string Hex64(std::uint64_t value) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << value;
    return ss.str();
}

static std::string CpuName() {
    // Phones name their system-on-chip through the system, not /proc/cpuinfo.
    if (std::string soc = MobileSocName(); !soc.empty()) return soc;
#if defined(_M_X64) || defined(_M_IX86)
    int regs[4]{};
    __cpuid(regs, static_cast<int>(0x80000000u));
    if (static_cast<unsigned>(regs[0]) >= 0x80000004u) {
        char brand[49]{};
        for (unsigned leaf = 0; leaf < 3; ++leaf) {
            __cpuid(regs, static_cast<int>(0x80000002u + leaf));
            std::memcpy(brand + leaf * 16, regs, 16);
        }
        std::string result(brand);
        const auto first = result.find_first_not_of(' ');
        const auto last = result.find_last_not_of(' ');
        return first == std::string::npos ? "Unknown CPU" : result.substr(first, last - first + 1);
    }
#elif defined(__APPLE__)
    std::size_t size = 0;
    if (sysctlbyname("machdep.cpu.brand_string", nullptr, &size, nullptr, 0) == 0 && size > 1) {
        std::string value(size, '\0');
        if (sysctlbyname("machdep.cpu.brand_string", value.data(), &size, nullptr, 0) == 0) {
            if (!value.empty() && value.back() == '\0') value.pop_back();
            return value;
        }
    }
    size = 0;
    if (sysctlbyname("hw.model", nullptr, &size, nullptr, 0) == 0 && size > 1) {
        std::string value(size, '\0');
        if (sysctlbyname("hw.model", value.data(), &size, nullptr, 0) == 0) {
            if (!value.empty() && value.back() == '\0') value.pop_back();
            return value;
        }
    }
#elif defined(__i386__) || defined(__x86_64__)
    const unsigned maxLeaf = __get_cpuid_max(0x80000000u, nullptr);
    if (maxLeaf >= 0x80000004u) {
        char brand[49]{};
        unsigned a = 0, b = 0, c = 0, d = 0;
        for (unsigned leaf = 0; leaf < 3; ++leaf) {
            __cpuid(0x80000002u + leaf, a, b, c, d);
            const unsigned words[] = {a, b, c, d};
            std::memcpy(brand + leaf * 16, words, 16);
        }
        std::string result(brand);
        const auto first = result.find_first_not_of(' ');
        const auto last = result.find_last_not_of(' ');
        return first == std::string::npos ? "Unknown CPU" : result.substr(first, last - first + 1);
    }
#endif

#ifdef _WIN32
    // CPUID is unavailable on Windows ARM64. The registry value is also a
    // useful fallback on unusual x86 firmware that omits the extended brand
    // leaves.
    char registryName[256]{};
    DWORD registryBytes = sizeof(registryName);
    if (RegGetValueA(HKEY_LOCAL_MACHINE,
                     "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                     "ProcessorNameString", RRF_RT_REG_SZ, nullptr,
                     registryName, &registryBytes) == ERROR_SUCCESS) {
        std::string result(registryName);
        const auto first = result.find_first_not_of(" \t\r\n");
        const auto last = result.find_last_not_of(" \t\r\n\0");
        if (first != std::string::npos) return result.substr(first, last - first + 1);
    }
#endif

#if defined(__linux__) || defined(__ANDROID__)
    std::ifstream cpuInfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuInfo, line)) {
        const char* keys[] = {"model name", "Hardware", "Processor"};
        for (const char* key : keys) {
            if (line.rfind(key, 0) == 0) {
                const auto colon = line.find(':');
                if (colon != std::string::npos) {
                    const auto start = line.find_first_not_of(" \t", colon + 1);
                    if (start != std::string::npos) return line.substr(start);
                }
            }
        }
    }
#endif
    return "Unknown CPU";
}

static std::string CoreClassForRank(int rank, int count) {
    if (rank < 0) return "Unclassified";
    if (count <= 1) return "InferredUniform";
    if (rank == 0) return "InferredPerformance";
    if (count == 2) return "InferredEfficiency";
    if (count == 3 && rank == 1) return "InferredEfficiency";
    if (rank == count - 1)
        return "InferredLowPowerEfficiency";
    if (count >= 4 && rank == count - 2) return "InferredEfficiency";
    return "InferredMiddle" + std::to_string(rank);
}

static void FinalizeTopology(std::vector<CpuLogicalProcessor>& cpus) {
    std::sort(cpus.begin(), cpus.end(), [](const auto& a, const auto& b) {
        return std::tie(a.group, a.logicalIndex) < std::tie(b.group, b.logicalIndex);
    });
    for (std::uint32_t i = 0; i < cpus.size(); ++i) cpus[i].ordinal = i;

    std::map<std::pair<std::uint16_t, std::uint32_t>, std::vector<std::size_t>> siblings;
    for (std::size_t i = 0; i < cpus.size(); ++i)
        siblings[{cpus[i].group, cpus[i].physicalCore}].push_back(i);

    std::uint32_t physicalOrdinal = 0;
    for (auto& entry : siblings) {
        const std::uint32_t width = static_cast<std::uint32_t>(entry.second.size());
        for (std::uint32_t smt = 0; smt < width; ++smt) {
            auto& cpu = cpus[entry.second[smt]];
            cpu.physicalCore = physicalOrdinal;
            cpu.smtIndex = smt;
            cpu.smtWidth = width;
        }
        ++physicalOrdinal;
    }

    std::set<int, std::greater<int>> rawClasses;
    for (const auto& cpu : cpus) {
        if (cpu.efficiencyClass >= 0) rawClasses.insert(cpu.efficiencyClass);
    }
    std::vector<int> classes(rawClasses.begin(), rawClasses.end());
    for (auto& cpu : cpus) {
        const auto it = std::find(classes.begin(), classes.end(), cpu.efficiencyClass);
        cpu.performanceLevel = it == classes.end() ? -1 : static_cast<int>(it - classes.begin());
        cpu.coreClass = CoreClassForRank(cpu.performanceLevel, static_cast<int>(classes.size()));
    }
}

#ifdef _WIN32
static std::vector<CpuLogicalProcessor> ProbeWindowsCpuSets(std::string& source) {
    std::vector<CpuLogicalProcessor> result;
    ULONG bytes = 0;
    GetSystemCpuSetInformation(nullptr, 0, &bytes, GetCurrentProcess(), 0);
    if (bytes != 0) {
        std::vector<unsigned char> buffer(bytes);
        if (GetSystemCpuSetInformation(
                reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.data()),
                bytes, &bytes, GetCurrentProcess(), 0)) {
            for (ULONG offset = 0; offset < bytes;) {
                const auto* info = reinterpret_cast<const SYSTEM_CPU_SET_INFORMATION*>(
                    buffer.data() + offset);
                if (info->Size == 0 || offset + info->Size > bytes) break;
                if (info->Type == CpuSetInformation) {
                    const auto& set = info->CpuSet;
                    // A CPU set exclusively allocated to another process is not
                    // an available target. Parked processors remain valid: the
                    // scheduler can unpark them for a foreground benchmark.
                    if (!set.Allocated || set.AllocatedToTargetProcess) {
                        CpuLogicalProcessor cpu;
                        cpu.group = set.Group;
                        cpu.logicalIndex = set.LogicalProcessorIndex;
                        cpu.physicalCore = set.CoreIndex;
                        cpu.cpuSetId = set.Id;
                        cpu.efficiencyClass = set.EfficiencyClass;
                        cpu.classificationSource = "inferred_windows_efficiency_class_rank";
                        cpu.parked = set.Parked != 0;
                        result.push_back(cpu);
                    }
                }
                offset += info->Size;
            }
        }
    }
    if (!result.empty()) source = "WindowsCpuSet";
    return result;
}

static std::vector<CpuLogicalProcessor> ProbeWindowsCoreRelation(std::string& source) {
    std::vector<CpuLogicalProcessor> result;
    DWORD bytes = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bytes);
    if (bytes == 0) return result;
    std::vector<unsigned char> buffer(bytes);
    if (!GetLogicalProcessorInformationEx(
            RelationProcessorCore,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()),
            &bytes)) return result;

    std::uint32_t core = 0;
    for (DWORD offset = 0; offset < bytes;) {
        const auto* info = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
            buffer.data() + offset);
        if (info->Size == 0 || offset + info->Size > bytes) break;
        const auto& rel = info->Processor;
        for (WORD gm = 0; gm < rel.GroupCount; ++gm) {
            const auto& mask = rel.GroupMask[gm];
            for (std::uint32_t bit = 0; bit < sizeof(KAFFINITY) * 8u; ++bit) {
                if ((mask.Mask & (static_cast<KAFFINITY>(1) << bit)) == 0) continue;
                CpuLogicalProcessor cpu;
                cpu.group = mask.Group;
                cpu.logicalIndex = bit;
                cpu.physicalCore = core;
                cpu.efficiencyClass = rel.EfficiencyClass;
                cpu.classificationSource = "inferred_windows_efficiency_class_rank";
                result.push_back(cpu);
            }
        }
        ++core;
        offset += info->Size;
    }
    if (!result.empty()) source = "WindowsCoreRelation";
    return result;
}
#endif

#if defined(__linux__) || defined(__ANDROID__)
static int ReadIntFile(const std::string& path, int fallback) {
    std::ifstream file(path);
    int value = fallback;
    if (file) file >> value;
    return value;
}

static std::string ReadLineFile(const std::string& path) {
    std::ifstream file(path);
    std::string line;
    if (file) std::getline(file, line);
    return line;
}

// A core is known by the list of processors that share it. Package and
// core_id are the fallback only: HarmonyOS on its own kernel numbers core_id
// per processor, and a MateBook Pro (Kirin X90, ten cores of two threads
// each) came out as twenty cores (2026-10-08).
static std::string CoreKeyOf(int logicalId) {
    const std::string base = "/sys/devices/system/cpu/cpu" + std::to_string(logicalId);
    std::string key = ReadLineFile(base + "/topology/core_cpus_list");
    if (key.empty()) key = ReadLineFile(base + "/topology/thread_siblings_list");
    if (key.empty()) {
        const int package = ReadIntFile(base + "/topology/physical_package_id", 0);
        const int coreId = ReadIntFile(base + "/topology/core_id", logicalId);
        key = "package " + std::to_string(package) + " core " + std::to_string(coreId);
    }
    return key;
}

// The processors and cores the machine has, whether or not this process may
// run on them. A phone keeps some from the app in front: a Xiaomi 15S Pro
// (XRing O1, ten cores) gives it cpu2-9 and a Quest 2 four of its eight, and
// the counts of the allowed set alone read like a smaller chip (2026-10-10).
// False where the system does not say.
static bool CountSystemProcessors(std::uint32_t& logical, std::uint32_t& physical) {
    const std::string present = ReadLineFile("/sys/devices/system/cpu/present");
    if (present.empty()) return false;
    cpu_set_t processors;
    CPU_ZERO(&processors);
    parked_cores::AddCpuList(present, processors);
    std::set<std::string> cores;
    logical = 0;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
        if (!CPU_ISSET(cpu, &processors)) continue;
        ++logical;
        cores.insert(CoreKeyOf(cpu));
    }
    physical = static_cast<std::uint32_t>(cores.size());
    return logical > 0;
}

static std::vector<CpuLogicalProcessor> ProbeLinuxTopology(std::string& source) {
    // Enumerate the calling process's actual allowed set. Containers and
    // Android cpusets commonly expose sparse logical IDs, so 0..online-1 is
    // not a valid affinity target list.
    //
    // sched_getaffinity() leaves out the cores a phone has parked for want of
    // work, so the configured mask is read as well: a parked core belongs to
    // the machine and comes back when it is given work (cpu_parked_cores.h).
    //
    // Cores a hot-plug governor has switched off are brought back first and
    // held for as long as this function reads their files.
    const parked_cores::AllOnline allOnline;
    std::vector<int> logicalIds;
    cpu_set_t allowed;
    cpu_set_t configured;
    CPU_ZERO(&allowed);
    CPU_ZERO(&configured);
    const bool haveAllowed = sched_getaffinity(0, sizeof(allowed), &allowed) == 0;
    parked_cores::ReadConfiguredCpus(configured);
    if (haveAllowed) {
        for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
            if (CPU_ISSET(cpu, &allowed) || CPU_ISSET(cpu, &configured))
                logicalIds.push_back(cpu);
        }
        source = "LinuxAllowedCpuSet+Sysfs";
    }
    if (logicalIds.empty()) {
        long online = sysconf(_SC_NPROCESSORS_ONLN);
        if (online < 1) online = static_cast<long>(std::thread::hardware_concurrency());
        if (online < 1) online = 1;
        for (long cpu = 0; cpu < online && cpu < CPU_SETSIZE; ++cpu)
            logicalIds.push_back(static_cast<int>(cpu));
        source = "LinuxSysfsFallbackBestEffort";
    }

    std::vector<CpuLogicalProcessor> result;
    result.reserve(logicalIds.size());
    std::map<std::string, std::uint32_t> coreOrdinals;
    for (const int logicalId : logicalIds) {
        const std::string base = "/sys/devices/system/cpu/cpu" + std::to_string(logicalId);
        auto inserted = coreOrdinals.emplace(CoreKeyOf(logicalId),
                                             static_cast<std::uint32_t>(coreOrdinals.size()));

        CpuLogicalProcessor cpu;
        cpu.group = 0;
        cpu.logicalIndex = static_cast<std::uint32_t>(logicalId);
        cpu.physicalCore = inserted.first->second;
        // Linux/Android do not expose Windows EfficiencyClass. Maximum
        // frequency (or Android cpu_capacity) is used only to rank clusters;
        // the raw value is emitted so consumers can see this heuristic.
        int capacity = ReadIntFile(base + "/cpu_capacity", -1);
        if (capacity < 0)
            capacity = ReadIntFile(base + "/cpufreq/cpuinfo_max_freq", -1);
        cpu.efficiencyClass = capacity;
        if (capacity >= 0)
            cpu.classificationSource = "inferred_capacity_or_max_frequency_rank";
        cpu.parked = haveAllowed && !CPU_ISSET(logicalId, &allowed);
        result.push_back(cpu);
    }
    return result;
}
#endif

#ifdef __APPLE__
static int SysctlInt(const char* name, int fallback) {
    int value = fallback;
    std::size_t size = sizeof(value);
    return sysctlbyname(name, &value, &size, nullptr, 0) == 0 ? value : fallback;
}

static std::vector<CpuLogicalProcessor> ProbeAppleTopology(std::string& source) {
    int logical = SysctlInt("hw.logicalcpu", static_cast<int>(std::thread::hardware_concurrency()));
    int physical = SysctlInt("hw.physicalcpu", logical);
    if (logical < 1) logical = 1;
    if (physical < 1) physical = logical;
    std::vector<CpuLogicalProcessor> result;
    result.reserve(static_cast<std::size_t>(logical));
    for (int i = 0; i < logical; ++i) {
        CpuLogicalProcessor cpu;
        cpu.logicalIndex = static_cast<std::uint32_t>(i);
        // macOS exposes counts but does not expose a public logical-CPU to
        // physical-core map or strict affinity API. This estimated identity is
        // metadata only and every result is marked scheduler_managed.
        cpu.physicalCore = static_cast<std::uint32_t>(
            (static_cast<long long>(i) * physical) / logical);
        cpu.efficiencyClass = -1;
        result.push_back(cpu);
    }
    source = "AppleSchedulerManagedEstimate";
    return result;
}
#endif

#if defined(__OHOS__)
// HarmonyOS on its own kernel does not hold a thread to any one processor. On
// a MateBook Pro (Kirin X90, 2026-10-08) a mask naming one of the eight
// big-core processors came back as "4-19", and sched_getcpu() counted cores
// where the masks count processors. Results from such a system are
// scheduler_managed, as on macOS; a thread still asks for its processor, and
// the system follows where it will. The emulators run a Linux kernel, hold
// every thread and stay strict.
struct OhosAffinity {
    bool everyProcessorPins = true;      // a one-processor mask comes back as asked
    bool getcpuNamesProcessors = true;   // and sched_getcpu() then names that processor
};

static const OhosAffinity& ProbedOhosAffinity() {
    static const OhosAffinity probed = [] {
        OhosAffinity result;
        cpu_set_t before;
        CPU_ZERO(&before);
        if (sched_getaffinity(0, sizeof(before), &before) != 0) return result;
        for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
            if (!CPU_ISSET(cpu, &before)) continue;
            cpu_set_t requested;
            CPU_ZERO(&requested);
            CPU_SET(cpu, &requested);
            cpu_set_t actual;
            CPU_ZERO(&actual);
            if (sched_setaffinity(0, sizeof(requested), &requested) != 0 ||
                sched_getaffinity(0, sizeof(actual), &actual) != 0 ||
                std::memcmp(&requested, &actual, sizeof(cpu_set_t)) != 0) {
                result.everyProcessorPins = false;
                continue;
            }
            // Give the move a moment before asking where the thread is.
            const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(2);
            while (std::chrono::steady_clock::now() < until) {}
            if (sched_getcpu() != cpu) result.getcpuNamesProcessors = false;
        }
        sched_setaffinity(0, sizeof(before), &before);
        return result;
    }();
    return probed;
}
#endif

class ThreadAffinityScope {
public:
    // `wakeParked`: when the system has parked the processor, put work on the
    // cores until it is back (Linux family). Only a thread that has not begun
    // its measurement may do that; an observer must not.
    explicit ThreadAffinityScope(const CpuLogicalProcessor& cpu, bool wakeParked = true)
        : logicalIndex_(cpu.logicalIndex) {
        (void)wakeParked;
#ifdef _WIN32
        GROUP_AFFINITY requested{};
        requested.Group = cpu.group;
        if (cpu.logicalIndex < sizeof(KAFFINITY) * 8u) {
            requested.Mask = static_cast<KAFFINITY>(1) << cpu.logicalIndex;
            changed_ = SetThreadGroupAffinity(GetCurrentThread(), &requested, &previous_) != FALSE;
            if (changed_) {
                GROUP_AFFINITY actual{};
                applied_ = GetThreadGroupAffinity(GetCurrentThread(), &actual) != FALSE &&
                           actual.Group == requested.Group && actual.Mask == requested.Mask;
            }
        }
#elif defined(__ANDROID__) || defined(__OHOS__)
        // Bionic and the HarmonyOS C library have sched_*affinity only; the
        // pthread_*affinity_np calls of glibc are unavailable.
        CPU_ZERO(&previous_);
        // The configured mask, not sched_getaffinity(): putting back a mask
        // that lacks a parked core would shut this thread out of that core
        // for good.
        havePrevious_ = parked_cores::ReadConfiguredCpus(previous_) ||
                        sched_getaffinity(0, sizeof(previous_), &previous_) == 0;
        cpu_set_t requested;
        CPU_ZERO(&requested);
        if (cpu.logicalIndex < CPU_SETSIZE) {
            CPU_SET(cpu.logicalIndex, &requested);
            const auto pin = [&requested] {
                return sched_setaffinity(0, sizeof(requested), &requested) == 0 ? 0 : errno;
            };
            changed_ = wakeParked ? parked_cores::SetAffinityWaking(pin) : pin() == 0;
            if (changed_) {
                cpu_set_t actual;
                CPU_ZERO(&actual);
                if (sched_getaffinity(0, sizeof(actual), &actual) == 0) {
                    int selected = 0;
                    for (int i = 0; i < CPU_SETSIZE; ++i)
                        selected += CPU_ISSET(i, &actual) ? 1 : 0;
                    applied_ = selected == 1 && CPU_ISSET(cpu.logicalIndex, &actual);
                }
            }
        }
#elif defined(__linux__)
        CPU_ZERO(&previous_);
        havePrevious_ = parked_cores::ReadConfiguredCpus(previous_) ||
            pthread_getaffinity_np(pthread_self(), sizeof(previous_), &previous_) == 0;
        cpu_set_t requested;
        CPU_ZERO(&requested);
        if (cpu.logicalIndex < CPU_SETSIZE) {
            CPU_SET(cpu.logicalIndex, &requested);
            const auto pin = [&requested] {
                return pthread_setaffinity_np(pthread_self(), sizeof(requested), &requested);
            };
            changed_ = wakeParked ? parked_cores::SetAffinityWaking(pin) : pin() == 0;
            if (changed_) {
                cpu_set_t actual;
                CPU_ZERO(&actual);
                if (pthread_getaffinity_np(pthread_self(), sizeof(actual), &actual) == 0) {
                    int selected = 0;
                    for (int i = 0; i < CPU_SETSIZE; ++i)
                        selected += CPU_ISSET(i, &actual) ? 1 : 0;
                    applied_ = selected == 1 && CPU_ISSET(cpu.logicalIndex, &actual);
                }
            }
        }
#else
        (void)cpu;
#endif
    }

    ~ThreadAffinityScope() {
#ifdef _WIN32
        if (changed_) SetThreadGroupAffinity(GetCurrentThread(), &previous_, nullptr);
#elif defined(__ANDROID__) || defined(__OHOS__)
        if (changed_ && havePrevious_)
            sched_setaffinity(0, sizeof(previous_), &previous_);
#elif defined(__linux__)
        if (changed_ && havePrevious_)
            pthread_setaffinity_np(pthread_self(), sizeof(previous_), &previous_);
#endif
    }

    bool strict() const { return applied_; }

    // Call on the pinned thread once its measurement is over: true when it is
    // still on its processor. A phone can park a core under a pinned thread
    // and carry the thread to another one, which would put that core's speed
    // under this core's name.
    bool held() const {
#if defined(__OHOS__)
        if (!ProbedOhosAffinity().getcpuNamesProcessors) return applied_;
#endif
#if defined(__linux__)
        return applied_ && sched_getcpu() == static_cast<int>(logicalIndex_);
#else
        (void)logicalIndex_;
        return applied_;
#endif
    }

private:
    std::uint32_t logicalIndex_ = 0;
    bool applied_ = false;
    bool changed_ = false;
#ifdef _WIN32
    GROUP_AFFINITY previous_{};
#elif defined(__linux__) || defined(__ANDROID__)
    cpu_set_t previous_{};
    bool havePrevious_ = false;
#endif
};

static std::string PlatformAffinityCapability() {
#if defined(__OHOS__)
    if (!ProbedOhosAffinity().everyProcessorPins) return "scheduler_managed";
#endif
#if defined(_WIN32)
    return "strict_group_affinity";
#elif defined(__linux__) || defined(__ANDROID__)
    // pthread_setaffinity_np/sched affinity is verifiable.  Failure is a hard
    // invalid result, so successful scores form a strict, separately versioned
    // contract rather than silently mixing pinned and unpinned samples.
    return "strict_sched_affinity";
#elif defined(__APPLE__)
    return "scheduler_managed";
#else
    return "unsupported";
#endif
}

static std::string SingleAffinityMode(bool applied) {
#if defined(__OHOS__)
    if (!ProbedOhosAffinity().everyProcessorPins) return "scheduler_managed";
#endif
#if defined(_WIN32)
    return applied ? "strict" : "failed";
#elif defined(__linux__) || defined(__ANDROID__)
    return applied ? "strict" : "failed";
#elif defined(__APPLE__)
    (void)applied;
    return "scheduler_managed";
#else
    (void)applied;
    return "unsupported";
#endif
}

static bool SingleAffinityIsValid(bool applied) {
#if defined(__OHOS__)
    if (!ProbedOhosAffinity().everyProcessorPins) return true;
#endif
#if defined(_WIN32) || defined(__linux__) || defined(__ANDROID__)
    return applied;
#else
    (void)applied;
    return true;
#endif
}

static void EmitProgress(std::ostream& out, const char* mode, const char* phase,
                         int coreIndex, std::size_t coreCount, double coreFraction,
                         double overallFraction, double elapsed,
                         int roundIndex = -1, std::uint32_t roundCount = 0) {
    out << std::fixed << std::setprecision(6)
        << "CPU_PROGRESS\tmode=" << mode
        << "\tphase=" << phase
        << "\tcore_index=" << coreIndex
        << "\tcore_count=" << coreCount
        << "\tcore_fraction=" << std::clamp(coreFraction, 0.0, 1.0)
        << "\toverall_fraction=" << std::clamp(overallFraction, 0.0, 1.0)
        << "\telapsed=" << (std::max)(0.0, elapsed)
        << "\tround_index=" << roundIndex
        << "\tround_count=" << roundCount << '\n' << std::flush;
}

struct KernelRoundResult {
    std::uint64_t workUnits = 0;
    std::uint64_t checksum = 0;
    double seconds = 0.0;

    double score() const {
        return seconds > 0.0 ? static_cast<double>(workUnits) / seconds / 1.0e6 : 0.0;
    }
};

static std::size_t MedianRoundIndex(const std::vector<KernelRoundResult>& rounds) {
    std::vector<std::size_t> order(rounds.size());
    std::iota(order.begin(), order.end(), 0u);
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return rounds[a].score() < rounds[b].score();
    });
    return order.empty() ? 0u : order[order.size() / 2u];
}

struct TimedKernelResult {
    std::uint64_t workUnits = 0;
    std::uint64_t checksum = 0;
    double seconds = 0.0;
    std::uint32_t roundCount = 0;
    std::uint32_t medianRound = 0;
    bool strictAffinity = false;
    std::vector<CpuResidency> residencyRounds;
    std::uint32_t residencyRetries = 0;
    bool residencyValid = true;
    std::vector<CpuResidency> placementHelpers;
};

static TimedKernelResult RunSingleTimed(const CpuLogicalProcessor& cpu,
                                        const CpuLogicalProcessor* observerCpu,
                                        const CpuBenchmarkConfig& config,
                                        std::size_t coreCount,
                                        std::size_t stageIndex,
                                        std::size_t totalStages,
                                        std::ostream& out) {
    TimedKernelResult result;
    const bool foregroundE = config.coreType == CpuCoreType::Efficiency && config.efficiencyForeground;
    const bool directAccounting = config.coreType != CpuCoreType::Automatic &&
        (foregroundE || CpuRequiresDirectTypeAccounting());
    const bool useHelpers = foregroundE && !CpuRequiresDirectTypeAccounting();
    const auto helperCount = useHelpers ? AppleCoreTypeCount(CpuCoreType::Performance) : 0;
    if ((useHelpers && (helperCount == 0 || helperCount > 64)) ||
        (directAccounting && !CaptureCpuThreadCounters().available)) {
        result.residencyValid = false;
        out << "CPU_ERROR\tmessage=foreground_efficiency_thread_accounting_unavailable\n" << std::flush;
        return result;
    }

    enum class Phase { Starting, Warmup, Measure, Complete };
    struct ProgressState {
        std::mutex mutex;
        std::condition_variable cv;
        Phase phase = Phase::Starting;
        std::uint32_t round = 0;
        Clock::time_point phaseStart{};
        bool done = false;
    } progress;

    // A high-QoS observer waiting on the worker's progress mutex can donate
    // priority to a background worker on iOS. Keep both at background QoS for
    // the E stage; UI callbacks remain asynchronous. Residency still gates
    // every measured round, independently of this scheduling request.
    CpuQosScope observerQos(config.coreType == CpuCoreType::Efficiency
        ? CpuCoreType::Efficiency : CpuCoreType::Automatic);
    std::thread worker([&] {
        ThreadAffinityScope affinity(cpu);
        CpuQosScope qos(config.coreType, foregroundE);
        result.residencyValid = qos.applied();
        result.strictAffinity = affinity.strict();
        // Keep the P cores busy at higher priority while the measured E
        // thread uses normal performance priority. This is placement pressure,
        // not binding: independently verify both target and helper CPU types.
        CpuPerformanceLoad helpers(helperCount);
        // Every logical processor and every scored round must execute exactly
        // the same dependency chain.  Core identity is metadata, not a seed.
        KernelState state = MakeKernelState(kPerCoreKernelSeed);
        const auto warmupStart = Clock::now();
        const auto warmupEnd = warmupStart + std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(config.warmupSeconds));
        {
            std::lock_guard<std::mutex> lock(progress.mutex);
            progress.phase = Phase::Warmup;
            progress.phaseStart = warmupStart;
        }
        progress.cv.notify_one();
        while (Clock::now() < warmupEnd && !CpuStopRequested()) {
            RunKernelBatch(state);
        }

        const std::uint32_t roundCount = (std::max)(1u, config.roundCount);
        const double roundSeconds = config.measureSeconds / static_cast<double>(roundCount);
        std::vector<KernelRoundResult> rounds;
        rounds.reserve(roundCount);
        for (std::uint32_t round = 0; round < roundCount; ++round) {
            if (CpuStopRequested()) break;
            KernelRoundResult sample;
            CpuResidency residency;
            for (unsigned attempt = 0; attempt < 3; ++attempt) {
                state = MakeKernelState(kPerCoreKernelSeed);
                {
                    std::lock_guard<std::mutex> lock(progress.mutex);
                    progress.phase = Phase::Measure;
                    progress.round = round;
                    progress.phaseStart = Clock::now();
                }
                progress.cv.notify_one();
                const auto accountingStart = config.coreType == CpuCoreType::Automatic
                    || directAccounting ? CpuResidencySnapshot{} : CaptureCpuResidency();
                const auto threadStart = directAccounting ? CaptureCpuThreadCounters() : CpuThreadCounters{};
                const auto start = Clock::now();
                const auto end = start + std::chrono::duration_cast<Clock::duration>(
                    std::chrono::duration<double>(roundSeconds));
                sample = {};
                while (true) {
                    RunKernelBatch(state);
                    sample.workUnits += kKernelBatch;
                    if (Clock::now() >= end || CpuStopRequested()) break;
                }
                sample.seconds = std::chrono::duration<double>(Clock::now() - start).count();
                if (directAccounting) {
                    residency = EvaluateCpuThreadCounters(threadStart, CaptureCpuThreadCounters(), config.coreType);
                    const double busyFraction = residency.workerCpuNs / 1e9 / sample.seconds;
                    if (foregroundE && (busyFraction < 0.90 || busyFraction > 1.05)) residency.available = false;
                } else if (config.coreType != CpuCoreType::Automatic)
                    residency = EvaluateCpuResidency(accountingStart, CaptureCpuResidency(), config.coreType);
                if (CpuStopRequested() || config.coreType == CpuCoreType::Automatic || residency.verified()) break;
                if (attempt < 2) ++result.residencyRetries;
            }
            if (config.coreType != CpuCoreType::Automatic) {
                result.residencyRounds.push_back(residency);
                result.residencyValid = result.residencyValid && residency.verified();
            }
            sample.checksum = state.checksum ^ state.a ^ RotL(state.b, 13);
            g_cpuBenchmarkSink.fetch_xor(sample.checksum, std::memory_order_relaxed);
            rounds.push_back(sample);
        }

        result.strictAffinity = affinity.held();
        result.placementHelpers = helpers.Finish();
        for (const auto& helper : result.placementHelpers)
            result.residencyValid = result.residencyValid && helper.verified();
        if (!CpuStopRequested() && !rounds.empty()) {
            const std::size_t median = MedianRoundIndex(rounds);
            result.workUnits = rounds[median].workUnits;
            result.checksum = rounds[median].checksum;
            result.seconds = rounds[median].seconds;
            result.roundCount = static_cast<std::uint32_t>(rounds.size());
            result.medianRound = static_cast<std::uint32_t>(median + 1u);
        }

        {
            std::lock_guard<std::mutex> lock(progress.mutex);
            progress.phase = Phase::Complete;
            progress.done = true;
        }
        progress.cv.notify_one();
    });

    // Keep the low-frequency observer off the physical core under test when
    // topology and affinity APIs make that possible.  This is deliberately not
    // part of the result-validity contract: only the measured worker must pin.
    std::optional<ThreadAffinityScope> observerAffinity;
    if (observerCpu) observerAffinity.emplace(*observerCpu, false);

    // Keep formatting, flushing, and progress bookkeeping entirely outside the
    // measured thread.  The condition variable wakes immediately at phase
    // transitions and otherwise limits GUI updates to four per second.
    Phase lastPhase = Phase::Starting;
    std::uint32_t lastRound = std::numeric_limits<std::uint32_t>::max();
    // Main-thread SwiftUI progress/log redraws are themselves interactive
    // CPU work. During E measurements, retain progress in the raw log and
    // deliver it after the worker stops, keeping that observer out of the
    // process accounting window. Do not weaken the residency threshold.
    std::ostringstream deferredProgress;
    const bool deferProgress = config.coreType == CpuCoreType::Efficiency;
    std::ostream& progressOut = deferProgress ? deferredProgress : out;
    if (deferProgress)
        EmitProgress(out, "per_core", "warmup", static_cast<int>(cpu.ordinal), coreCount,
                     0.0, static_cast<double>(stageIndex) / static_cast<double>(totalStages),
                     0.0, -1, config.roundCount);
    auto nextPeriodicUpdate = Clock::now();
    std::unique_lock<std::mutex> lock(progress.mutex);
    while (!progress.done) {
        progress.cv.wait_until(lock, nextPeriodicUpdate);
        const Phase phase = progress.phase;
        const std::uint32_t round = progress.round;
        const auto phaseStart = progress.phaseStart;
        const bool transition = phase != lastPhase || round != lastRound;
        const auto now = Clock::now();
        const bool periodic = now >= nextPeriodicUpdate;
        const bool done = progress.done;
        lock.unlock();

        if (!done && (transition || periodic)) {
            if (phase == Phase::Warmup) {
                const double elapsed =
                    std::chrono::duration<double>(now - phaseStart).count();
                EmitProgress(progressOut, "per_core", "warmup", static_cast<int>(cpu.ordinal),
                             coreCount, 0.0,
                             static_cast<double>(stageIndex) /
                                 static_cast<double>(totalStages),
                             elapsed, -1, config.roundCount);
            } else if (phase == Phase::Measure) {
                const std::uint32_t roundCount = (std::max)(1u, config.roundCount);
                const double roundSeconds =
                    config.measureSeconds / static_cast<double>(roundCount);
                const double elapsed =
                    std::chrono::duration<double>(now - phaseStart).count();
                const double fraction = (static_cast<double>(round) +
                    std::clamp(elapsed / roundSeconds, 0.0, 1.0)) /
                    static_cast<double>(roundCount);
                EmitProgress(progressOut, "per_core", "measure",
                             static_cast<int>(cpu.ordinal), coreCount, fraction,
                             (static_cast<double>(stageIndex) + fraction) /
                                 static_cast<double>(totalStages),
                             static_cast<double>(round) * roundSeconds + elapsed,
                             static_cast<int>(round), roundCount);
            }
            lastPhase = phase;
            lastRound = round;
            nextPeriodicUpdate = now + kSingleProgressPeriod;
        }
        lock.lock();
    }
    lock.unlock();
    worker.join();
    for (std::size_t i = 0; i < result.placementHelpers.size(); ++i) {
        const auto& helper = result.placementHelpers[i];
        out << std::fixed << std::setprecision(9) << "CPU_PLACEMENT_HELPER\tindex=" << i
            << "\trequested_lower_bound=" << helper.requestedLowerBound
            << "\tperformance_fraction=" << helper.performanceFraction
            << "\tworker_cpu_seconds=" << helper.workerCpuNs / 1e9
            << "\teffective_ghz=" << helper.effectiveGHz
            << "\tverified=" << (helper.verified() ? 1 : 0) << '\n';
    }
    if (deferProgress) out << deferredProgress.str();
    EmitProgress(out, "per_core", "complete", static_cast<int>(cpu.ordinal),
                 coreCount, 1.0,
                 static_cast<double>(stageIndex + 1u) / static_cast<double>(totalStages),
                 config.measureSeconds, static_cast<int>(result.medianRound) - 1,
                 result.roundCount);
    return result;
}

static std::string MultiAffinityMode(std::uint32_t pinned, std::uint32_t threads) {
#if defined(__OHOS__)
    if (!ProbedOhosAffinity().everyProcessorPins) return "scheduler_managed";
#endif
#if defined(_WIN32)
    return pinned == threads ? "strict" : (pinned == 0 ? "failed" : "partial");
#elif defined(__linux__) || defined(__ANDROID__)
    return pinned == threads ? "strict" : (pinned == 0 ? "failed" : "partial");
#elif defined(__APPLE__)
    (void)pinned;
    (void)threads;
    return "scheduler_managed";
#else
    (void)pinned;
    (void)threads;
    return "unsupported";
#endif
}

static CpuMultiCoreResult RunMultiTimed(const std::vector<CpuLogicalProcessor>& cpus,
                                        const CpuBenchmarkConfig& config,
                                        std::size_t stageIndex,
                                        std::size_t totalStages,
                                        std::ostream& out) {
    CpuMultiCoreResult result;
    if (cpus.empty()) return result;
    const auto performanceCount = AppleCoreTypeCount(CpuCoreType::Performance);
    const auto efficiencyCount = AppleCoreTypeCount(CpuCoreType::Efficiency);
    const bool typedScheduling = CpuRequiresDirectTypeAccounting() && performanceCount > 0 &&
        efficiencyCount > 0 && performanceCount + efficiencyCount == cpus.size();

    struct alignas(kWorkerIsolationBytes) WorkerResult {
        std::uint64_t units = 0;
        std::uint64_t checksum = 0;
        Clock::time_point finished{};
        bool strict = false;
        bool qosApplied = false;
        CpuResidency residency;
        double busyFraction = 0;
    };
    struct MultiRoundResult {
        KernelRoundResult kernel;
        std::uint32_t pinned = 0;
        std::vector<double> workerScores;
        double processCpuSeconds = 0.0;
        double performanceCpuSeconds = 0.0;
        bool verifiedCoreTypes = true;
        std::vector<CpuResidency> workerResidencies;
        std::vector<double> workerBusyFractions;
        CpuParticipation participation;
    };

    const std::uint32_t roundCount = (std::max)(1u, config.roundCount);
    const double roundSeconds = config.measureSeconds / static_cast<double>(roundCount);
    std::vector<MultiRoundResult> rounds;
    rounds.reserve(roundCount);

    for (std::uint32_t round = 0; round < roundCount; ++round) {
        if (CpuStopRequested()) return result;
        struct Gate {
            std::mutex mutex;
            std::condition_variable cv;
            std::size_t ready = 0;
            bool start = false;
            Clock::time_point warmupEnd{};
            Clock::time_point measureEnd{};
        } gate;

        std::vector<WorkerResult> workerResults(cpus.size());
        std::vector<std::thread> workers;
        workers.reserve(cpus.size());
        for (std::size_t i = 0; i < cpus.size(); ++i) {
            workers.emplace_back([&, i] {
                ThreadAffinityScope affinity(cpus[i]);
                const auto requestedType = !typedScheduling ? CpuCoreType::Automatic :
                    i < performanceCount ? CpuCoreType::Performance : CpuCoreType::Efficiency;
                CpuQosScope qos(requestedType, typedScheduling);
                KernelState state = MakeKernelState(static_cast<std::uint64_t>(i) + 0x10001u);
                {
                    std::unique_lock<std::mutex> lock(gate.mutex);
                    ++gate.ready;
                    gate.cv.notify_all();
                    gate.cv.wait(lock, [&] { return gate.start; });
                }
                while (Clock::now() < gate.warmupEnd && !CpuStopRequested()) RunKernelBatch(state);
                state = MakeKernelState(static_cast<std::uint64_t>(i) + 0x10001u);
                const auto countersBefore = typedScheduling ? CaptureCpuThreadCounters() : CpuThreadCounters{};
                const auto workerStart = Clock::now();
                std::uint64_t units = 0;
                while (true) {
                    RunKernelBatch(state);
                    units += kKernelBatch;
                    if (Clock::now() >= gate.measureEnd || CpuStopRequested()) break;
                }
                const auto finished = Clock::now();
                const auto residency = typedScheduling
                    ? EvaluateCpuThreadCounters(countersBefore, CaptureCpuThreadCounters(), requestedType) : CpuResidency{};
                const bool strict = affinity.held();
                const std::uint64_t checksum =
                    state.checksum ^ state.a ^ RotL(state.b, 13);
                // One isolated publication after timing: the hot loop performs
                // no shared writes and cannot false-share with adjacent workers.
                auto& published = workerResults[i];
                published.units = units;
                published.finished = finished;
                published.checksum = checksum;
                published.strict = strict;
                published.qosApplied = qos.applied();
                published.residency = residency;
                published.busyFraction = typedScheduling
                    ? residency.workerCpuNs / 1e9 / std::chrono::duration<double>(finished - workerStart).count() : 0;
                g_cpuBenchmarkSink.fetch_xor(checksum, std::memory_order_relaxed);
            });
        }

        Clock::time_point start;
        CpuResidencySnapshot accountingStart;
        const double warmup = round == 0 ? config.warmupSeconds : 0.0;
        // Emit the phase boundary before starting the common timer so stdout
        // formatting is never charged to the benchmark.
        if (round == 0) {
            EmitProgress(out, "multi", warmup > 0.0 ? "warmup" : "measure", -1,
                         cpus.size(), 0.0,
                         static_cast<double>(stageIndex) /
                             static_cast<double>(totalStages),
                         0.0, warmup > 0.0 ? -1 : 0, roundCount);
        }
        {
            std::unique_lock<std::mutex> lock(gate.mutex);
            gate.cv.wait(lock, [&] { return gate.ready == cpus.size(); });
            start = Clock::now();
            gate.warmupEnd = start + std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(warmup));
            gate.measureEnd = gate.warmupEnd + std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(roundSeconds));
            accountingStart = CaptureCpuResidency();
            gate.start = true;
        }
        gate.cv.notify_all();

        // The observer does no stdout work at all while every logical CPU is
        // saturated.  Formal runs still update the GUI at each five-second
        // round boundary, after all workers have joined and timing has stopped.
        WaitUntilCpuStop(gate.measureEnd);
        for (auto& worker : workers) worker.join();
        const auto accountingEnd = CaptureCpuResidency();
        if (CpuStopRequested()) return result;

        MultiRoundResult sample;
        Clock::time_point lastFinish = gate.measureEnd;
        for (std::size_t i = 0; i < workerResults.size(); ++i) {
            const auto& worker = workerResults[i];
            lastFinish = (std::max)(lastFinish, worker.finished);
            sample.kernel.workUnits += worker.units;
            sample.kernel.checksum ^= RotL(worker.checksum,
                                           static_cast<unsigned>(i % 63u + 1u));
            if (worker.strict) ++sample.pinned;
            sample.workerResidencies.push_back(worker.residency);
            sample.workerBusyFractions.push_back(worker.busyFraction);
            if (typedScheduling)
                sample.verifiedCoreTypes = sample.verifiedCoreTypes && worker.qosApplied &&
                    worker.residency.verified() && worker.busyFraction >= .90 && worker.busyFraction <= 1.05;
        }
        // The last batch may cross the shared deadline. Charge that overrun to
        // the common elapsed time instead of dividing by the requested time
        // and inflating short-run scores.
        sample.kernel.seconds =
            std::chrono::duration<double>(lastFinish - gate.warmupEnd).count();
        if (typedScheduling)
            sample.participation = EvaluateCpuParticipation(sample.workerResidencies,
                sample.workerBusyFractions, performanceCount, efficiencyCount, sample.kernel.seconds);
        // Process counters include the first round's warm-up and a little
        // observer work; they diagnose scheduling, never classify workers.
        if (accountingStart.available && accountingEnd.available &&
            accountingEnd.processNs >= accountingStart.processNs &&
            accountingEnd.performanceNs >= accountingStart.performanceNs) {
            sample.processCpuSeconds = double(accountingEnd.processNs - accountingStart.processNs) / 1e9;
            sample.performanceCpuSeconds = double(accountingEnd.performanceNs - accountingStart.performanceNs) / 1e9;
        }
        for (std::size_t i = 0; i < workerResults.size(); ++i) {
            const double score = double(workerResults[i].units) / sample.kernel.seconds / 1e6;
            sample.workerScores.push_back(score);
            out << std::fixed << std::setprecision(9)
                << "CPU_MULTI_WORKER\tround=" << round + 1 << "\tworker=" << i
                << "\twork_units=" << workerResults[i].units
                << "\tcommon_seconds=" << sample.kernel.seconds << "\tscore=" << score
                << "\tclassification=" << (typedScheduling ?
                    (i < performanceCount ? "requested_performance_type" : "requested_efficiency_type") : "unclassified_worker")
                << "\trequested_lower_bound=" << workerResults[i].residency.requestedLowerBound
                << "\tworker_cpu_seconds=" << workerResults[i].residency.workerCpuNs / 1e9
                << "\tbusy_fraction=" << workerResults[i].busyFraction << '\n';
        }
        out << "CPU_MULTI_ACCOUNTING\tround=" << round + 1
            << "\tprocess_cpu_seconds=" << sample.processCpuSeconds
            << "\tperformance_cpu_seconds=" << sample.performanceCpuSeconds
            << "\tincludes_warmup_seconds=" << warmup << '\n';
        if (typedScheduling)
            out << "CPU_MULTI_PARTICIPATION\tround=" << round + 1
                << "\tperformance_occupancy=" << sample.participation.performanceOccupancy
                << "\tefficiency_occupancy=" << sample.participation.efficiencyOccupancy
                << "\tminimum_worker_occupancy=" << sample.participation.minimumWorkerOccupancy
                << "\tverified=" << sample.participation.verified()
                << "\tindividual_type_purity_verified=" << sample.verifiedCoreTypes << '\n';
        rounds.push_back(sample);

        const double roundComplete =
            static_cast<double>(round + 1u) / static_cast<double>(roundCount);
        EmitProgress(out, "multi", "measure", -1, cpus.size(), roundComplete,
                     (static_cast<double>(stageIndex) + roundComplete) /
                         static_cast<double>(totalStages),
                     static_cast<double>(round + 1u) * roundSeconds,
                     static_cast<int>(round), roundCount);
    }

    std::vector<KernelRoundResult> scoreRounds;
    scoreRounds.reserve(rounds.size());
    for (const auto& round : rounds) scoreRounds.push_back(round.kernel);
    const std::size_t median = MedianRoundIndex(scoreRounds);
    const auto& selected = rounds[median];

    result.threadCount = static_cast<std::uint32_t>(cpus.size());
    // Treat the affinity contract as a property of the whole multi-round run,
    // not just the selected median round.
    result.pinnedThreadCount = result.threadCount;
    for (const auto& round : rounds)
        result.pinnedThreadCount = (std::min)(result.pinnedThreadCount, round.pinned);
    result.workUnits = selected.kernel.workUnits;
    result.checksum = selected.kernel.checksum;
    result.measuredSeconds = selected.kernel.seconds;
    result.scoreMWorkPerSec = selected.kernel.score();
    result.roundCount = static_cast<std::uint32_t>(rounds.size());
    result.medianRound = static_cast<std::uint32_t>(median + 1u);
    result.affinityMode = MultiAffinityMode(result.pinnedThreadCount, result.threadCount);
    result.workerScores = selected.workerScores;
    result.processCpuSeconds = selected.processCpuSeconds;
    result.performanceCpuSeconds = selected.performanceCpuSeconds;
    result.typedScheduling = typedScheduling;
    result.verifiedCoreTypes = typedScheduling;
    for (const auto& round : rounds) {
        result.verifiedCoreTypes = result.verifiedCoreTypes && round.verifiedCoreTypes;
        if (typedScheduling) result.participationRounds.push_back(round.participation);
    }
    result.workerResidencies = selected.workerResidencies;
    result.workerBusyFractions = selected.workerBusyFractions;
    // As many busy threads as there are processors fill the machine wherever
    // the system puts them, so this result does not depend on the pinning (the
    // user's decision of 2026-10-08). How many threads were held is recorded
    // beside it. Which processor a thread is on matters to the per-processor
    // test, and there it stays a condition.
    result.valid = result.workUnits > 0;
    EmitProgress(out, "multi", "complete", -1, cpus.size(), 1.0,
                 static_cast<double>(stageIndex + 1u) / static_cast<double>(totalStages),
                 config.measureSeconds, static_cast<int>(result.medianRound) - 1,
                 result.roundCount);
    return result;
}

} // namespace

CpuPerformanceLoad::CpuPerformanceLoad(std::uint32_t count) : residency_(count) {
    try {
        for (std::uint32_t i = 0; i < count; ++i) {
            workers_.emplace_back([this, i] {
                CpuQosScope qos(CpuCoreType::Performance);
                auto state = MakeKernelState(kPerCoreKernelSeed);
                const auto before = CaptureCpuThreadCounters();
                ready_.fetch_add(1, std::memory_order_release);
                while (!stop_.load(std::memory_order_relaxed) && !CpuStopRequested()) RunKernelBatch(state);
                residency_[i] = EvaluateCpuThreadCounters(before, CaptureCpuThreadCounters(), CpuCoreType::Performance);
                if (!qos.applied()) residency_[i].available = false;
                g_cpuBenchmarkSink.fetch_xor(state.checksum, std::memory_order_relaxed);
            });
        }
        while (ready_.load(std::memory_order_acquire) < count && !CpuStopRequested()) std::this_thread::yield();
    } catch (...) { Finish(); throw; }
}
CpuPerformanceLoad::~CpuPerformanceLoad() { Finish(); }
const std::vector<CpuResidency>& CpuPerformanceLoad::Finish() {
    stop_.store(true, std::memory_order_relaxed);
    for (auto& worker : workers_) if (worker.joinable()) worker.join();
    return residency_;
}

CpuBenchmarkReport ProbeCpuTopology() {
    CpuBenchmarkReport report;
    report.cpuName = CpuName();
    report.affinityCapability = PlatformAffinityCapability();
    report.workloadVersion = std::string(kCpuBenchmarkWorkloadVersion) + "_" +
                             report.affinityCapability;
    // The test does much 64-bit integer work, which 32-bit code does in several
    // instructions per operation: a Cortex-A15 running it as 32-bit code scored
    // less than half of a 64-bit Cortex-A53 per GHz (2026-10-08). Such results
    // are a group of their own and are not charted with 64-bit ones.
    if (sizeof(void*) == 4) report.workloadVersion += "_32bit";
#if defined(__APPLE__) && defined(__aarch64__) && !TARGET_OS_IPHONE
    // Before GPU_BENCH_KEEP_BRANCH an arm64 Mac ran the kernel without its
    // branch; its earlier results stay in the old group, out of comparisons.
    report.workloadVersion += "_kept_branch";
#endif
#ifdef _WIN32
    report.processors = ProbeWindowsCpuSets(report.topologySource);
    if (report.processors.empty())
        report.processors = ProbeWindowsCoreRelation(report.topologySource);
#elif defined(__linux__) || defined(__ANDROID__)
    report.processors = ProbeLinuxTopology(report.topologySource);
    std::uint32_t systemLogical = 0;
    std::uint32_t systemPhysical = 0;
    if (CountSystemProcessors(systemLogical, systemPhysical) &&
        systemLogical > report.processors.size()) {
        report.systemLogicalCount = systemLogical;
        report.systemPhysicalCount = systemPhysical;
    }
#elif defined(__APPLE__)
    report.processors = ProbeAppleTopology(report.topologySource);
#else
    const unsigned count = (std::max)(1u, std::thread::hardware_concurrency());
    for (unsigned i = 0; i < count; ++i) {
        CpuLogicalProcessor cpu;
        cpu.logicalIndex = i;
        cpu.physicalCore = i;
        report.processors.push_back(cpu);
    }
    report.topologySource = "CxxHardwareConcurrency";
#endif
    FinalizeTopology(report.processors);
    return report;
}

void PrintCpuTopology(const CpuBenchmarkReport& report, std::ostream& out) {
    std::set<std::uint32_t> physical;
    for (const auto& cpu : report.processors) physical.insert(cpu.physicalCore);
    out << "CPU_META\tworkload_version=" << report.workloadVersion
        << "\tname=" << SanitizeField(report.cpuName)
        << "\tlogical_count=" << report.processors.size()
        << "\tphysical_count=" << physical.size()
        << "\ttopology_source=" << SanitizeField(report.topologySource)
        << "\taffinity_capability=" << report.affinityCapability;
    if (report.systemLogicalCount > 0) {
        out << "\tsystem_logical_count=" << report.systemLogicalCount
            << "\tsystem_physical_count=" << report.systemPhysicalCount;
    }
    out << '\n';
    for (const auto& cpu : report.processors) {
        out << "CPU_TOPOLOGY\tcore_index=" << cpu.ordinal
            << "\tgroup=" << cpu.group
            << "\tlogical_index=" << cpu.logicalIndex
            << "\tphysical_core=" << cpu.physicalCore
            << "\tsmt_index=" << cpu.smtIndex
            << "\tsmt_width=" << cpu.smtWidth
            << "\tcpu_set_id=" << cpu.cpuSetId
            << "\tefficiency_class=" << cpu.efficiencyClass
            << "\tperformance_level=" << cpu.performanceLevel
            << "\tcore_class=" << cpu.coreClass
            << "\tclassification_source=" << cpu.classificationSource
            << "\tparked=" << (cpu.parked ? 1 : 0) << '\n';
    }
    out << std::flush;
}

CpuBenchmarkReport RunCpuBenchmark(const CpuBenchmarkConfig& requestedConfig,
                                   std::ostream& out) {
    auto config = requestedConfig;
    // The all-core stage uses every logical processor, irrespective of the
    // single-thread P/E policy. Short multi-only runs need no residency gate.
    if (config.mode == CpuBenchmarkMode::MultiCore) config.coreType = CpuCoreType::Automatic;
    CpuBenchmarkReport report = ProbeCpuTopology();
    PrintCpuTopology(report, out);
    out << "CPU_TYPE_CAPS\tverified_scheduling=" << (CpuCoreTypeSchedulingSupported() ? 1 : 0)
        << "\tperformance_count=" << AppleCoreTypeCount(CpuCoreType::Performance)
        << "\tefficiency_count=" << AppleCoreTypeCount(CpuCoreType::Efficiency)
        << "\tresidency_method=" << CpuResidencyMethod() << '\n';
    out << "CPU_SCHEDULING\tcore_type=" << CpuCoreTypeName(config.coreType)
        << "\tefficiency_policy=" << (config.efficiencyForeground ? CpuForegroundEfficiencyPolicyName() : "background")
        << "\tresidency_threshold=0.99\tscope=single_thread_stages\n" << std::flush;
    if (!std::isfinite(config.measureSeconds) || config.measureSeconds <= 0.0 ||
        !std::isfinite(config.warmupSeconds) || config.warmupSeconds < 0.0 ||
        config.roundCount == 0) {
        out << "CPU_ERROR\tmessage=invalid_cpu_benchmark_configuration\n" << std::flush;
        return report;
    }
    if (report.processors.empty()) {
        out << "CPU_ERROR\tmessage=no_available_logical_processors\n" << std::flush;
        return report;
    }
    if (config.coreType != CpuCoreType::Automatic &&
        (!CpuCoreTypeSchedulingSupported() || config.measureSeconds / config.roundCount < 0.05)) {
        out << "CPU_ERROR\tmessage=core_type_scheduling_unavailable_or_round_shorter_than_50ms\n" << std::flush;
        return report;
    }
    const bool formalContract = std::llround(config.measureSeconds * 1000.0) == 15000 &&
                                std::llround(config.warmupSeconds * 1000.0) == 200 &&
                                config.roundCount == 3;
    const char* scoreContract = formalContract ? "formal" : "preview";

    const bool runPerCore = config.mode == CpuBenchmarkMode::PerCore ||
                            config.mode == CpuBenchmarkMode::All;
    const bool runMulti = config.mode == CpuBenchmarkMode::MultiCore ||
                          config.mode == CpuBenchmarkMode::All;
    std::vector<CpuLogicalProcessor> singleStages = report.processors;
    if (config.coreType != CpuCoreType::Automatic) {
        singleStages.clear();
        for (auto type : {CpuCoreType::Performance, CpuCoreType::Efficiency}) {
            if (config.coreType != CpuCoreType::Both && config.coreType != type) continue;
            CpuLogicalProcessor representative;
            representative.ordinal = type == CpuCoreType::Performance ? 0 : 1;
            representative.performanceLevel = static_cast<int>(representative.ordinal);
            representative.coreClass = type == CpuCoreType::Performance ? "Performance" : "Efficiency";
            representative.classificationSource = "qos_verified_cpu_time_bound";
            singleStages.push_back(representative);
        }
    }
    const std::size_t totalStages = (runPerCore ? singleStages.size() : 0u) +
                                    (runMulti ? 1u : 0u);
    std::size_t stage = 0;

    out << "\n--- Native CPU mixed benchmark ---\n"
        << "CPU: " << report.cpuName << "\n";
    if (report.systemLogicalCount > 0) {
        std::set<std::uint32_t> usableCores;
        for (const auto& cpu : report.processors) usableCores.insert(cpu.physicalCore);
        out << "Processors: " << report.processors.size() << " of the machine's "
            << report.systemLogicalCount << " are open to this process (" << usableCores.size()
            << " of " << report.systemPhysicalCount << " cores); the system keeps the rest from it.\n";
    }
    out << "Topology: " << report.topologySource
        << ", affinity: " << report.affinityCapability << "\n"
        << "Workload: " << report.workloadVersion << ", " << config.roundCount
        << " rounds, median aggregation (the requested measurement time is split across rounds).\n"
        << "Score contract: " << scoreContract
        << " (formal = 15.0 s measurement, 0.2 s warm-up, 3 rounds).\n"
        << "Each score is million mixed integer/branch/FP32/FP64 work units per second.\n"
        << "The checksum is a dead-code-elimination sink, not a correctness oracle.\n";

    auto runMultiStage = [&]() {
        out << "\n--- CPU all-logical-processor test ---\n" << std::flush;
        report.multiCore = RunMultiTimed(report.processors, config, stage, totalStages, out);
        out << std::fixed << std::setprecision(2)
            << "All-core score (" << report.multiCore.threadCount << " threads): "
            << report.multiCore.scoreMWorkPerSec << " MWork/s"
            << (report.multiCore.valid ? "" : " [INVALID: nothing was measured]")
            << "\n";
        if (report.multiCore.affinityMode == "partial" || report.multiCore.affinityMode == "failed") {
            out << "  " << report.multiCore.pinnedThreadCount << " of " << report.multiCore.threadCount
                << " threads were held to their processor; the all-core score does not depend on it.\n";
        }
        out << std::fixed << std::setprecision(6)
            << "CPU_RESULT\tkind=multi\tmode=multi\tcore_index=-1"
            << "\tworkload_version=" << report.workloadVersion
            << "\tscore_contract=" << scoreContract
            << "\taggregation=median"
            << "\tcore_count=" << report.processors.size()
            << "\tthread_count=" << report.multiCore.threadCount
            << "\tpinned_threads=" << report.multiCore.pinnedThreadCount
            << "\taffinity=" << report.multiCore.affinityMode
            << "\tvalid=" << (report.multiCore.valid ? 1 : 0)
            << "\tscore=" << report.multiCore.scoreMWorkPerSec
            << "\tunit=MWork/s"
            << "\tseconds=" << report.multiCore.measuredSeconds
            << "\trequested_seconds=" << config.measureSeconds
            << "\tround_count=" << report.multiCore.roundCount
            << "\tmedian_round=" << report.multiCore.medianRound
            << "\tchecksum=" << Hex64(report.multiCore.checksum)
            << "\tchecksum_role=dce_sink" << '\n' << std::flush;
        ++stage;
    };

    // In All mode, measure the all-core score before the long per-core sweep.
    // This gives it the same cold-start ordering as a standalone all-core run
    // instead of measuring after minutes of accumulated heat and package power.
    if (runMulti && config.mode == CpuBenchmarkMode::All)
        runMultiStage();
    if (CpuStopRequested()) return report;

    if (runPerCore) {
        report.perCore.reserve(singleStages.size());
        for (const auto& cpu : singleStages) {
            if (CpuStopRequested()) return report;
            const CpuLogicalProcessor* observerCpu = nullptr;
            for (const auto& candidate : report.processors) {
                if (candidate.physicalCore != cpu.physicalCore) {
                    observerCpu = &candidate;
                    break;
                }
            }
            auto stageConfig = config;
            if (config.coreType == CpuCoreType::Both)
                stageConfig.coreType = cpu.performanceLevel == 0
                    ? CpuCoreType::Performance : CpuCoreType::Efficiency;
            if (config.coreType != CpuCoreType::Automatic) observerCpu = nullptr;
            const auto timed = RunSingleTimed(cpu, observerCpu, stageConfig,
                                              singleStages.size(), stage,
                                              totalStages, out);
            if (CpuStopRequested()) return report;
            CpuCoreResult result;
            result.processor = cpu;
            result.workUnits = timed.workUnits;
            result.checksum = timed.checksum;
            result.measuredSeconds = timed.seconds;
            result.scoreMWorkPerSec = timed.seconds > 0.0
                ? static_cast<double>(timed.workUnits) / timed.seconds / 1.0e6 : 0.0;
            result.roundCount = timed.roundCount;
            result.medianRound = timed.medianRound;
            result.affinityMode = SingleAffinityMode(timed.strictAffinity);
            result.coreType = stageConfig.coreType;
            result.residencyRounds = timed.residencyRounds;
            result.residencyRetries = timed.residencyRetries;
            result.foregroundEfficiency = stageConfig.coreType == CpuCoreType::Efficiency && stageConfig.efficiencyForeground;
            if (result.foregroundEfficiency || CpuRequiresDirectTypeAccounting())
                result.processor.classificationSource = "thread_perf_level_counters_spi";
            result.placementHelpers = timed.placementHelpers;
            result.valid = timed.workUnits > 0 && SingleAffinityIsValid(timed.strictAffinity)
                           && timed.residencyValid;
            for (std::size_t round = 0; round < timed.residencyRounds.size(); ++round) {
                const auto& residency = timed.residencyRounds[round];
                out << std::fixed << std::setprecision(6)
                    << "CPU_RESIDENCY\tcore_type=" << CpuCoreTypeName(result.coreType)
                    << "\tmethod=" << (result.foregroundEfficiency ? "thread_selfcounts_perf_level_spi" : CpuResidencyMethod())
                    << "\tround=" << round + 1 << "\tp_fraction=" << residency.performanceFraction
                    << "\trequested_lower_bound=" << residency.requestedLowerBound
                    << "\teffective_ghz=" << residency.effectiveGHz
                    << "\tworker_cpu_seconds=" << residency.workerCpuNs / 1e9
                    << "\tprocess_cpu_seconds=" << residency.processCpuNs / 1e9
                    << "\tverified=" << (residency.verified() ? 1 : 0)
                    << "\tretries=" << timed.residencyRetries << '\n' << std::flush;
            }
            report.perCore.push_back(result);

            if (result.coreType == CpuCoreType::Automatic) {
                out << "Logical processor sample " << cpu.ordinal + 1;
            } else {
                out << cpu.coreClass << " core type (QoS, verified CPU time; no numbered-core binding)";
            }
            out << std::fixed << std::setprecision(2) << " score: " << result.scoreMWorkPerSec
                << " MWork/s" << (result.valid ? "" : " [INVALID: placement contract not met]") << "\n";
            out << std::fixed << std::setprecision(6)
                << "CPU_RESULT\tkind=core\tmode=per_core"
                << "\tworkload_version=" << report.workloadVersion
                << "\tscore_contract=" << scoreContract
                << "\taggregation=median"
                << "\tcore_index=" << cpu.ordinal
                << "\tcore_count=" << report.processors.size()
                << "\tgroup=" << cpu.group
                << "\tlogical_index=" << (result.coreType == CpuCoreType::Automatic ? long(cpu.logicalIndex) : -1L)
                << "\tphysical_core=" << (result.coreType == CpuCoreType::Automatic ? long(cpu.physicalCore) : -1L)
                << "\tsmt_index=" << cpu.smtIndex
                << "\tsmt_width=" << cpu.smtWidth
                << "\tefficiency_class=" << cpu.efficiencyClass
                << "\tperformance_level=" << cpu.performanceLevel
                << "\tcore_class=" << cpu.coreClass
                << "\tcore_type=" << CpuCoreTypeName(result.coreType)
                << "\tefficiency_policy=" << (result.foregroundEfficiency ? "foreground_with_performance_load" : "background")
                << "\tresidency_verified=" << (timed.residencyValid ? 1 : 0)
                << "\trequested_lower_bound=" << (timed.residencyRounds.empty() ? 0.0 :
                    std::min_element(timed.residencyRounds.begin(), timed.residencyRounds.end(),
                        [](const auto& a, const auto& b) { return a.requestedLowerBound < b.requestedLowerBound; })->requestedLowerBound)
                << "\tclassification_source=" << result.processor.classificationSource
                << "\taffinity=" << result.affinityMode
                << "\tvalid=" << (result.valid ? 1 : 0)
                << "\tscore=" << result.scoreMWorkPerSec
                << "\tunit=MWork/s"
                << "\tseconds=" << result.measuredSeconds
                << "\trequested_seconds=" << config.measureSeconds
                << "\tround_count=" << result.roundCount
                << "\tmedian_round=" << result.medianRound
                << "\tchecksum=" << Hex64(result.checksum)
                << "\tchecksum_role=dce_sink" << '\n' << std::flush;
            ++stage;
        }

        std::vector<double> scores;
        std::uint64_t summaryChecksum = 0;
        std::size_t checksumIndex = 0;
        for (const auto& item : report.perCore) {
            if (item.valid) scores.push_back(item.scoreMWorkPerSec);
            summaryChecksum ^= RotL(item.checksum,
                                    static_cast<unsigned>(checksumIndex++ % 63u + 1u));
        }
        const double average = scores.empty() ? 0.0
            : std::accumulate(scores.begin(), scores.end(), 0.0) /
              static_cast<double>(scores.size());
        const double minimum = scores.empty() ? 0.0
            : *std::min_element(scores.begin(), scores.end());
        const double maximum = scores.empty() ? 0.0
            : *std::max_element(scores.begin(), scores.end());
        const std::size_t invalidCount = report.perCore.size() - scores.size();
        const bool summaryValid = invalidCount == 0 && !scores.empty();
        if (config.coreType == CpuCoreType::Automatic) {
        out << std::fixed << std::setprecision(2)
            << "Average per-logical-processor score: " << average << " MWork/s"
            << (summaryValid ? "" : " [INVALID/INCOMPLETE]") << "\n";
        out << std::fixed << std::setprecision(6)
            << "CPU_RESULT\tkind=summary\tmode=per_core"
            << "\tworkload_version=" << report.workloadVersion
            << "\tscore_contract=" << scoreContract
            << "\taggregation=arithmetic_mean_of_per_core_medians"
            << "\tcore_count=" << report.processors.size()
            << "\tcompleted=" << scores.size()
            << "\tinvalid_count=" << invalidCount
            << "\tvalid=" << (summaryValid ? 1 : 0)
            << "\taverage_score=" << average
            << "\tmin_score=" << minimum
            << "\tmax_score=" << maximum
            << "\tunit=MWork/s"
            << "\tround_count=" << config.roundCount
            << "\tchecksum=" << Hex64(summaryChecksum)
            << "\tchecksum_role=dce_sink\n" << std::flush;
        } else {
            out << "CPU_RESULT\tkind=class_summary\tcompleted=" << scores.size()
                << "\tinvalid_count=" << invalidCount << "\tvalid=" << (summaryValid ? 1 : 0)
                << "\tcore_type=" << CpuCoreTypeName(config.coreType) << '\n' << std::flush;
        }
#if defined(_WIN32) || defined(__linux__) || defined(__ANDROID__)
        if (!summaryValid) {
            out << "CPU_ERROR\tmessage=required_affinity_failed"
                << "\tinvalid_count=" << invalidCount << '\n' << std::flush;
        }
#endif
    }

    if (runMulti && config.mode != CpuBenchmarkMode::All)
        runMultiStage();

    return report;
}

} // namespace gpu_bench
