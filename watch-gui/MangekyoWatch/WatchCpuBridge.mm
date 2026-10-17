#import <Foundation/Foundation.h>
#import <WatchKit/WatchKit.h>
#include "WatchCpuBridge.h"
#include "cpu_cli.h"
#include "cpu_cancel.h"
#include "benchmark_results.h"
#include "path_service.h"
#include <os/proc.h>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <streambuf>
#include <stdexcept>
#include <thread>
#include <cstring>

namespace {
std::mutex stateMutex;
std::atomic<bool> running{false};
std::atomic<int> exitCode{0};
std::string lastError;
std::deque<std::string> pending;
std::thread worker;
struct WorkerCleanup {
    ~WorkerCleanup() { gpu_bench::RequestCpuStop(); if (worker.joinable()) worker.join(); }
} cleanup;
char* Copy(const std::string& value) {
    char* copy=static_cast<char*>(malloc(value.size()+1));
    if (copy) std::memcpy(copy,value.c_str(),value.size()+1);
    return copy;
}
class LogBuffer: public std::streambuf {
    std::ofstream file_;
    std::string line_;
    std::mutex mutex_;
    void Append(char c) {
        file_.put(c);
        if (c=='\n') {
            std::lock_guard<std::mutex> state(stateMutex);
            if (pending.size()>=512) pending.pop_front();
            pending.push_back(line_); line_.clear(); file_.flush();
        } else if (c!='\r') line_+=c;
    }
    int_type overflow(int_type c) override {
        if (traits_type::eq_int_type(c,traits_type::eof())) return traits_type::not_eof(c);
        std::lock_guard<std::mutex> lock(mutex_); Append(traits_type::to_char_type(c)); return c;
    }
    std::streamsize xsputn(const char* data,std::streamsize size) override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (std::streamsize i=0;i<size;++i) Append(data[i]);
        return size;
    }
public:
    explicit LogBuffer(const std::string& path):file_(path) {
        if (!file_) throw std::runtime_error("Unable to open benchmark log");
    }
};
struct Redirect {
    std::streambuf *out,*err;
    explicit Redirect(std::streambuf* b):out(std::cout.rdbuf(b)),err(std::cerr.rdbuf(b)) {}
    ~Redirect() {std::cout.rdbuf(out);std::cerr.rdbuf(err);}
};
}
extern "C" {
void wcpu_initialize(const char* directory) {
    if (directory && *directory) setenv("GPU_BENCH_DATA_DIR",directory,1);
    setenv("GPU_BENCH_WATCH_OS_VERSION",WKInterfaceDevice.currentDevice.systemVersion.UTF8String,1);
}
bool wcpu_start(int job,bool smoke) {
    if (job<0 || job>1 || running.exchange(true)) return false;
    if (worker.joinable()) worker.join();
    gpu_bench::ClearCpuStop(); exitCode.store(0);
    setenv("GPU_BENCH_WATCH_PROFILE",smoke?"smoke":"standard",1);
    {std::lock_guard<std::mutex> lock(stateMutex); lastError.clear();pending.clear();}
    try {
        worker=std::thread([job,smoke] {
        @autoreleasepool {
            int code=1;
            try {
                const auto before=gpu_bench::LoadResults().size();
                LogBuffer log((gpu_bench::paths::LogsDirectory()/
                    (gpu_bench::GenerateResultId()+(job==0?"-compute.log":"-memory.log"))).string());
                Redirect redirect(&log);
                std::cout<<"WATCH_PROFILE\tprofile="<<(smoke?"smoke":"standard")
                    <<"\tcore_placement=scheduler_managed_unclassified\n"<<std::flush;
                if (job==0) {
                    gpu_bench::CpuBenchmarkConfig config;
                    config.mode=gpu_bench::CpuBenchmarkMode::All;
                    config.coreType=gpu_bench::CpuCoreType::Automatic;
                    config.efficiencyForeground=false;
                    config.measureSeconds=smoke?0.12:15.0;
                    config.warmupSeconds=smoke?0.01:0.2;
                    config.roundCount=3;
                    code=gpu_bench::RunCpuBenchmarkCli(config,true);
                } else {
                    gpu_bench::CpuMemBwConfig config;
                    const auto available=static_cast<std::uint64_t>(os_proc_available_memory());
                    const auto limit=available?std::min<std::uint64_t>(16ull<<20,available/4):8ull<<20;
                    if (limit<(1ull<<20)) throw std::runtime_error("Not enough app memory for a CPU sweep");
                    config.maxDatasetBytes=smoke?1ull<<20:limit;
                    config.coreType=gpu_bench::CpuCoreType::Automatic;
                    config.efficiencyForeground=false;
                    config.warmupRuns=smoke?1:2;
                    config.measuredRuns=smoke?3:7;
                    config.sampleMs=smoke?1.0:10.0;
                    config.storageFileBytes=32ull<<20;
                    config.storageDir=gpu_bench::paths::DataRoot().string();
                    std::cout<<"WATCH_MEMORY_BUDGET\tavailable_app_bytes="<<available
                        <<"\tmax_dataset_bytes="<<config.maxDatasetBytes
                        <<"\tpage_region_cap_bytes="<<(8ull<<20)<<'\n'<<std::flush;
                    code=gpu_bench::RunCpuMemoryBandwidthCli(config,true);
                }
                if (gpu_bench::CpuStopRequested()) code=130;
                if (code==0 && gpu_bench::LoadResults().size()!=before+(job==0?2u:1u))
                    throw std::runtime_error("Result saving did not produce every expected record");
            } catch (const std::exception& e) {
                code=1;
                std::lock_guard<std::mutex> lock(stateMutex);lastError=e.what();
            }
            exitCode.store(code);running.store(false,std::memory_order_release);
        }
        });
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(stateMutex);
        lastError=e.what();exitCode.store(1);running.store(false);
        return false;
    }
    return true;
}
void wcpu_cancel() {gpu_bench::RequestCpuStop();}
bool wcpu_running() {return running.load(std::memory_order_acquire);}
int wcpu_exit_code() {return exitCode.load();}
char* wcpu_poll_output() {
    std::lock_guard<std::mutex> lock(stateMutex);std::string output;
    for (const auto& line:pending) output+=line+'\n';
    pending.clear();return Copy(output);
}
char* wcpu_error() {std::lock_guard<std::mutex> lock(stateMutex);return Copy(lastError);}
char* wcpu_load_results() {
    if (running.load()) return nullptr;
    try {return Copy(gpu_bench::SerializeResultsJson(gpu_bench::LoadResults()));}
    catch (...) {return nullptr;}
}
void wcpu_free(char* value) {free(value);}
}
