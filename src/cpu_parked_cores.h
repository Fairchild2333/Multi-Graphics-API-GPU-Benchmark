#pragma once

// Processors that the system has parked (Linux family: Linux, Android,
// HarmonyOS).
//
// Phone kernels take idle cores out of use: Qualcomm's core control keeps as
// few cores of a cluster awake as the load needs. While a core is parked
//   - sched_getaffinity() leaves it out, so a process asking which processors
//     it may use is told about fewer than it has, and
//   - sched_setaffinity() onto that core alone fails with EINVAL.
// The core is back within a fraction of a second once there is work for it.
//
// Found 2026-10-07 on a Snapdragon 8 Gen 2: the all-core test ran 7 threads on
// an 8-core phone, and the per-core test could not pin to the missing core.

#if defined(__linux__)

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace gpu_bench::parked_cores {

// How long a parked core is given to come back. On the phone above it took
// less than a quarter of a second.
inline constexpr std::chrono::milliseconds kWakeTimeout{3000};
inline constexpr std::chrono::milliseconds kWakePoll{2};

// "0-3,5,7-8" -> those processors, added to `set`.
inline void AddCpuList(const std::string& text, cpu_set_t& set) {
    const char* cursor = text.c_str();
    while (*cursor) {
        char* end = nullptr;
        const long first = std::strtol(cursor, &end, 10);
        if (end == cursor) break;
        long last = first;
        cursor = end;
        if (*cursor == '-') {
            last = std::strtol(cursor + 1, &end, 10);
            if (end == cursor + 1) break;
            cursor = end;
        }
        for (long cpu = first; cpu <= last && cpu < CPU_SETSIZE; ++cpu) {
            if (cpu >= 0) CPU_SET(static_cast<int>(cpu), &set);
        }
        if (*cursor != ',') break;
        ++cursor;
    }
}

// The processors the calling thread may run on, parked ones included: the mask
// /proc reports is the configured one, which the kernel does not narrow to the
// cores that happen to be awake. It is limited to the processors that are
// online, because an untouched mask names every processor the machine could
// ever have. False when either file cannot be read; `set` is then unchanged.
inline bool ReadConfiguredCpus(cpu_set_t& set) {
    std::ifstream status("/proc/thread-self/status");
    if (!status) {
        status.clear();
        status.open("/proc/self/task/" + std::to_string(syscall(SYS_gettid)) + "/status");
    }
    static const std::string key = "Cpus_allowed_list:";
    std::string line;
    std::string list;
    while (std::getline(status, line)) {
        if (line.compare(0, key.size(), key) == 0) {
            const auto start = line.find_first_not_of(" \t", key.size());
            if (start != std::string::npos) list = line.substr(start);
            break;
        }
    }
    std::ifstream onlineFile("/sys/devices/system/cpu/online");
    std::string onlineList;
    if (list.empty() || !std::getline(onlineFile, onlineList) || onlineList.empty()) return false;

    cpu_set_t configured;
    cpu_set_t online;
    CPU_ZERO(&configured);
    CPU_ZERO(&online);
    AddCpuList(list, configured);
    AddCpuList(onlineList, online);
    cpu_set_t result;
    CPU_AND(&result, &configured, &online);
    if (CPU_COUNT(&result) == 0) return false;
    set = result;
    return true;
}

// Work on every processor for as long as one object of this class lives. It is
// what makes the system bring parked cores back. Threads waiting for different
// cores share one set of busy threads.
class Load {
public:
    Load() {
        Shared& shared = State();
        std::lock_guard<std::mutex> lock(shared.mutex);
        if (shared.users++ > 0) return;
        long count = sysconf(_SC_NPROCESSORS_CONF);
        if (count < 1) count = 1;
        if (count > 64) count = 64;
        shared.stop.store(false, std::memory_order_relaxed);
        for (long i = 0; i < count; ++i) {
            try {
                shared.threads.emplace_back([&shared] {
                    // The thread that starts the load may itself be confined
                    // to a few processors; the load must reach all of them.
                    cpu_set_t everywhere;
                    CPU_ZERO(&everywhere);
                    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) CPU_SET(cpu, &everywhere);
                    sched_setaffinity(0, sizeof(everywhere), &everywhere);
                    while (!shared.stop.load(std::memory_order_relaxed)) {
                    }
                });
            } catch (const std::system_error&) {
                break;
            }
        }
    }

    ~Load() {
        Shared& shared = State();
        std::lock_guard<std::mutex> lock(shared.mutex);
        if (--shared.users > 0) return;
        shared.stop.store(true, std::memory_order_relaxed);
        for (auto& thread : shared.threads) thread.join();
        shared.threads.clear();
    }

    Load(const Load&) = delete;
    Load& operator=(const Load&) = delete;

private:
    struct Shared {
        std::mutex mutex;
        unsigned users = 0;
        std::atomic<bool> stop{false};
        std::vector<std::thread> threads;
    };
    static Shared& State() {
        static Shared shared;
        return shared;
    }
};

// Calls `set`, which pins the calling thread to one processor and returns 0 or
// an errno value. A parked processor is refused with EINVAL; the cores are then
// kept busy until it is accepted or kWakeTimeout has passed. Call this before
// any timing starts: the load disturbs every other thread while it lasts.
template <typename SetAffinity>
bool SetAffinityWaking(SetAffinity&& set) {
    int error = set();
    if (error != EINVAL) return error == 0;
    Load load;
    const auto deadline = std::chrono::steady_clock::now() + kWakeTimeout;
    do {
        std::this_thread::sleep_for(kWakePoll);
        error = set();
    } while (error == EINVAL && std::chrono::steady_clock::now() < deadline);
    return error == 0;
}

} // namespace gpu_bench::parked_cores

#endif // __linux__
