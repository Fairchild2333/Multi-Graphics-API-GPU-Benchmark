#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
namespace gpu_bench {
inline std::atomic<bool> cpuStopRequested{false};
inline std::mutex cpuStopMutex;
inline std::condition_variable cpuStopCondition;
inline bool CpuStopRequested() { return cpuStopRequested.load(std::memory_order_acquire); }
inline void RequestCpuStop() {
    { std::lock_guard<std::mutex> lock(cpuStopMutex); cpuStopRequested.store(true, std::memory_order_release); }
    cpuStopCondition.notify_all();
}
inline void ClearCpuStop() { cpuStopRequested.store(false, std::memory_order_release); }
inline void WaitUntilCpuStop(std::chrono::steady_clock::time_point deadline) {
    std::unique_lock<std::mutex> lock(cpuStopMutex);
    cpuStopCondition.wait_until(lock, deadline, [] { return CpuStopRequested(); });
}
}
