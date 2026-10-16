#include "vulkan_membw.h"

#include "gpu_common.h"
#include "gpu_engine.h"
#include "vulkan_timing_validation.h"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#include <os/proc.h>
#endif
#endif

namespace gpu_bench {
namespace {

constexpr std::uint32_t kGroupSize = 256;        // local_size_x of the kernels
constexpr std::uint32_t kStripesPerChunk = 128;  // 128 x 256 x 16 B = 512 KiB
constexpr std::uint64_t kElementBytes = 16;      // one vec4
constexpr std::uint64_t kChunkBytes =
    static_cast<std::uint64_t>(kStripesPerChunk) * kGroupSize * kElementBytes;
constexpr std::uint32_t kMaxGroupsX = 16384;
constexpr std::uint32_t kHashSeed = 0x4d42u;
constexpr std::uint64_t kMiB = 1ull << 20;
// The memory plateau needs three largest data sets that all streamed and agree
// within kMemoryTailSpread.
constexpr double kMemoryTailSpread = 1.2;
constexpr double kPlateauSpread = 1.3;
// Upper bound on the traffic of a single dispatch.
constexpr std::uint64_t kMaxDispatchBytes = 64ull << 30;
// L1 sweep: one stripe is 256 threads x 16 B = 4 KiB. Private blocks run from
// 16 KiB to 1 MiB (the kernel needs an even stripe count of at least 4), and
// the full-load pass gives every concurrently running workgroup its own block.
constexpr std::uint64_t kStripeBytes = kGroupSize * kElementBytes;
constexpr std::uint32_t kL1StripeCounts[] = {4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256};
constexpr std::uint32_t kL1FullLoadBlocks = 4096;
// System-memory pass: the host buffer is at most 2 GiB (4096 chunks), and a
// sweep stops once a single dispatch takes longer than this.
constexpr std::uint32_t kMaxHostChunks = 4096;
constexpr double kHostSlowDispatchMs = 500.0;
// Data check (membw_check.comp): the sink is read back as words. Group 0 sets
// the first kGroupSize of them to kCheckRan, and a thread that loaded a value
// other than the one stored sets its word among the next kGroupSize.
constexpr std::uint32_t kCheckRan = 0x4D42434Bu;
constexpr std::uint32_t kCheckWords = 2 * kGroupSize;
// The system-memory pass needs a buffer of at least this many chunks (64 MiB).
constexpr std::uint32_t kMinHostChunks = 128;

// Must match the membw_*.comp push constants. Repeats drives both private-block
// reads and uniquely-owned write/copy loops.
struct PushParams {
    std::uint32_t chunkCount;
    std::uint32_t stripesPerChunk;
    std::uint32_t totalGroups;
    std::uint32_t groupsX;
    std::uint32_t seed;
    float never;
    std::uint32_t repeats;
};

struct KernelInfo {
    const char* name;
    const char* spirvFile;
    std::uint64_t bytesPerChunkVisit;
    const char* description;
};

constexpr std::size_t kReadKernel = 0;
constexpr std::size_t kWriteKernel = 1;
constexpr std::size_t kCopyKernel = 2;
constexpr std::size_t kL1Kernel = 3;
constexpr std::size_t kCheckKernel = 4;  // never timed
// The first kSweepKernelCount entries run the data-set sweep; the L1 kernel
// has its own sweep over the private block size.
constexpr std::size_t kSweepKernelCount = 3;
const KernelInfo kKernels[] = {
    {"read",  "membw_read.comp.spv",  kChunkBytes,     "16 B loads"},
    {"write", "membw_write.comp.spv", kChunkBytes,     "16 B stores"},
    {"copy",  "membw_copy.comp.spv",  2 * kChunkBytes, "16 B load + 16 B store"},
    {"l1",    "membw_l1.comp.spv",    0,               "16 B loads from a private block"},
    {"check", "membw_check.comp.spv", 0,               "loads compared with what was stored"},
};
constexpr std::size_t kKernelCount = sizeof(kKernels) / sizeof(kKernels[0]);

// Memory the system can still give out, in bytes; 0 where that is not known
// On iOS this is the app's remaining allowance, not the device's free RAM.
std::uint64_t AvailableSystemMemoryBytes() {
#if defined(__linux__)
    std::ifstream file("/proc/meminfo");
    std::string key;
    std::string rest;
    std::uint64_t value = 0;
    while (file >> key >> value) {
        std::getline(file, rest);
        if (key == "MemAvailable:") return value * 1024;
    }
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    return os_proc_available_memory();
#endif
    return 0;
}

void Check(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(what) + " failed (VkResult " +
                                 std::to_string(static_cast<int>(result)) + ")");
    }
}

const char* DeviceTypeName(VkPhysicalDeviceType type) {
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "Discrete GPU";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "Virtual GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "CPU";
        default:                                     return "Other";
    }
}

double Median(std::vector<double> values) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const std::size_t mid = values.size() / 2;
    return values.size() % 2 == 0 ? 0.5 * (values[mid - 1] + values[mid]) : values[mid];
}

std::string FormatMiB(std::uint64_t bytes) {
    std::ostringstream text;
    if (bytes % kMiB == 0) {
        text << (bytes / kMiB);
    } else {
        text << std::fixed << std::setprecision(1)
             << static_cast<double>(bytes) / static_cast<double>(kMiB);
    }
    text << " MiB";
    return text.str();
}

// Block sizes of the L1 sweep are multiples of 4 KiB.
std::string FormatBlock(std::uint64_t bytes) {
    if (bytes >= kMiB) return FormatMiB(bytes);
    return std::to_string(bytes / 1024) + " KiB";
}

// A three-point running median removes single outliers before a curve is
// split into flat stretches. It keeps the position of a step between levels.
std::vector<double> SmoothedMedians(const std::vector<const MemBwPoint*>& curve) {
    const std::size_t count = curve.size();
    std::vector<double> smooth(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (i == 0 || i + 1 == count) {
            smooth[i] = curve[i]->medianGBps;
        } else {
            smooth[i] = Median({curve[i - 1]->medianGBps, curve[i]->medianGBps,
                                curve[i + 1]->medianGBps});
        }
    }
    return smooth;
}

// Doubles from 1 MiB with a x1.5 step in between, and always ends on the
// largest data set the device allows.
std::vector<std::uint32_t> SweepChunkCounts(std::uint32_t maxChunks) {
    std::vector<std::uint32_t> counts;
    for (std::uint64_t base = 2; base <= maxChunks; base *= 2) {
        counts.push_back(static_cast<std::uint32_t>(base));
        const std::uint64_t mid = base + base / 2;
        if (mid <= maxChunks) counts.push_back(static_cast<std::uint32_t>(mid));
    }
    if (counts.empty() || counts.back() != maxChunks) counts.push_back(maxChunks);
    return counts;
}

MemBwKernelSummary Summarise(const std::vector<MemBwPoint>& points, const char* kernel,
                             std::uint64_t streamingMin) {
    std::vector<const MemBwPoint*> curve;
    for (const auto& point : points)
        if (point.kernel == kernel) curve.push_back(&point);

    MemBwKernelSummary summary;
    if (curve.empty()) return summary;
    const std::size_t count = curve.size();

    // Memory plateau: median of the three largest data sets. It only counts as
    // reached when those points are themselves flat; otherwise the curve is
    // still descending and the value is cache-assisted.
    std::vector<double> tail;
    for (std::size_t i = count >= 3 ? count - 3 : 0; i < count; ++i)
        tail.push_back(curve[i]->medianGBps);
    summary.memoryGBps = Median(tail);
    const double tailLow = *std::min_element(tail.begin(), tail.end());
    const double tailHigh = *std::max_element(tail.begin(), tail.end());
    summary.memoryPlateauReached =
        tail.size() == 3 && MemBwStreamingTailEligible(curve[count - 3]->datasetBytes,
            curve[count - 2]->datasetBytes, curve[count - 1]->datasetBytes, streamingMin) &&
        tailLow > 0.0 && tailHigh / tailLow <= kMemoryTailSpread;
    if (!summary.memoryPlateauReached) return summary;

    const std::vector<double> smooth = SmoothedMedians(curve);

    // Flat stretch: consecutive points within kPlateauSpread of each other. A
    // stretch shorter than three points is a transition, not a plateau. The
    // cache plateau is the longest stretch that is clearly above the memory
    // plateau; the L1-assisted slope at the smallest data sets is short.
    std::size_t bestLength = 0;
    std::size_t start = 0;
    while (start < count) {
        double low = smooth[start];
        double high = low;
        std::size_t end = start;
        while (end + 1 < count) {
            const double newLow = (std::min)(low, smooth[end + 1]);
            const double newHigh = (std::max)(high, smooth[end + 1]);
            if (newLow <= 0.0 || newHigh / newLow > kPlateauSpread) break;
            low = newLow;
            high = newHigh;
            ++end;
        }
        const std::size_t length = end - start + 1;
        if (length >= 3) {
            std::vector<double> values;
            for (std::size_t i = start; i <= end; ++i) values.push_back(curve[i]->medianGBps);
            const double level = Median(values);
            if (level >= 1.2 * summary.memoryGBps) {
                summary.cacheKneeBytes = (std::max)(summary.cacheKneeBytes,
                                                    curve[end]->datasetBytes);
                if (length > bestLength) {
                    bestLength = length;
                    summary.cacheGBps = level;
                    summary.cacheFromBytes = curve[start]->datasetBytes;
                    summary.cacheToBytes = curve[end]->datasetBytes;
                    summary.smallSetsL1Assisted =
                        start > 0 && curve[0]->medianGBps > 1.3 * level;
                }
            }
        }
        start = end + 1;
    }
    return summary;
}

// The L1 plateau is the stretch around the fastest block size that stays
// within kPlateauSpread of that peak. Returns the index of its last point and
// stores its level.
std::size_t PeakPlateauEnd(const std::vector<const MemBwPoint*>& curve, double& level) {
    const std::vector<double> smooth = SmoothedMedians(curve);
    const std::size_t peak = static_cast<std::size_t>(
        std::max_element(smooth.begin(), smooth.end()) - smooth.begin());
    const double floor = smooth[peak] / kPlateauSpread;
    std::size_t first = peak;
    while (first > 0 && smooth[first - 1] >= floor) --first;
    std::size_t last = peak;
    while (last + 1 < curve.size() && smooth[last + 1] >= floor) ++last;

    std::vector<double> values;
    for (std::size_t i = first; i <= last; ++i) values.push_back(curve[i]->medianGBps);
    level = Median(values);
    return last;
}

MemBwL1Summary SummariseL1(const std::vector<MemBwPoint>& points) {
    std::vector<const MemBwPoint*> fullLoad;
    std::vector<const MemBwPoint*> single;
    for (const auto& point : points) {
        if (point.kernel == "l1") fullLoad.push_back(&point);
        else if (point.kernel == "l1_single") single.push_back(&point);
    }

    MemBwL1Summary summary;
    if (!fullLoad.empty()) {
        const std::size_t end = PeakPlateauEnd(fullLoad, summary.fullLoadGBps);
        summary.fullLoadBlockBytes = fullLoad[end]->datasetBytes;
    }
    if (!single.empty()) {
        const std::size_t end = PeakPlateauEnd(single, summary.singleGroupGBps);
        // A drop inside the sweep brackets the capacity between two block sizes.
        if (end + 1 < single.size()) {
            summary.capacityLowBytes = single[end]->datasetBytes;
            summary.capacityHighBytes = single[end + 1]->datasetBytes;
        }
    }
    return summary;
}

void FillStatistics(MemBwPoint& point, const std::vector<double>& times) {
    point.medianMs = Median(times);
    point.minMs = *std::min_element(times.begin(), times.end());
    const double bytes = static_cast<double>(point.bytesPerDispatch);
    point.medianGBps = point.medianMs > 0.0 ? bytes / (point.medianMs / 1000.0) / 1.0e9 : 0.0;
    point.bestGBps = point.minMs > 0.0 ? bytes / (point.minMs / 1000.0) / 1.0e9 : 0.0;
}

class Runner {
public:
    Runner(const MemBwConfig& config, std::ostream& out) : config_(config), out_(out) {}
    ~Runner() { Destroy(); }
    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;

    void Run(MemBwReport& report);

private:
    struct Buffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };
    struct Sample {
        double gpuMs = -1.0;
        double wallMs = 0.0;
    };

    void CreateInstance();
    void SelectDevice(MemBwReport& report);
    void CreateDevice();
    void CreateBuffers(MemBwReport& report);
    void CreateDescriptors();
    void CreatePipelines();
    void CreateCommandObjects();
    void DecideTimingSource(MemBwReport& report);
    bool TryCreateBuffer(VkDeviceSize size, Buffer& buffer, bool hostMemory = false);
    void WriteDescriptorSet(VkDescriptorSet set, VkBuffer source);
    void MeasureHostMemory(MemBwReport& report);
    Sample SubmitCopy(VkBuffer source, VkBuffer destination, VkDeviceSize bytes);
    Sample SubmitClear(VkBuffer buffer);
    void CreateReadback();
    void RecordResourceBarrier();
    void VerifyWrittenData(std::size_t kernel, std::uint32_t chunks);
    enum class DataCheck { Good, Wrong, NotRun };
    DataCheck CheckData(std::uint32_t chunkCount, std::uint32_t& wrongThreads);
    bool FillChecked(bool hostBuffer, std::uint32_t& chunkCount, std::uint32_t minChunks);
    Sample Execute();
    void DestroyBuffer(Buffer& buffer);
    void Destroy();

    Sample Submit(std::size_t kernel, std::uint32_t chunkCount, std::uint32_t stripesPerChunk,
                  std::uint64_t totalGroups, std::uint32_t repeats,
                  std::uint32_t orderedPasses = 1);
    Sample Dispatch(std::size_t kernel, std::uint32_t chunkCount, std::uint64_t passes);
    double TimeOf(const Sample& sample);
    void BeginCoverage();
    void CheckCoverage();
    std::uint64_t MaxPasses(std::uint32_t chunkCount) const;
    std::uint64_t CalibratePasses(std::size_t kernel, std::uint32_t chunkCount);
    MemBwPoint MeasurePoint(std::size_t kernel, std::uint32_t chunkCount,
                            const char* name = nullptr);
    MemBwPoint MeasureL1Point(bool singleGroup, std::uint32_t stripes);
    void PrintPoint(const MemBwPoint& point, bool blockSweep);

    MemBwConfig config_;
    std::ostream& out_;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties props_{};
    VkPhysicalDeviceMemoryProperties memoryProps_{};
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    std::uint32_t queueFamily_ = 0;
    std::uint32_t timestampValidBits_ = 0;
    std::uint64_t deviceLocalHeapBytes_ = 0;
    std::uint64_t maxAllocationBytes_ = (std::numeric_limits<std::uint64_t>::max)();

    Buffer source_;
    Buffer destination_;
    Buffer sink_;
    Buffer host_;                    // system-memory buffer; null when skipped
    Buffer readback_;                // CPU-visible copy of the sink, for the data check
    void* readbackMapped_ = nullptr;
    std::uint32_t maxChunks_ = 0;
    std::uint32_t hostChunks_ = 0;
    // Set from the checked sweep size before any point is measured.
    std::uint64_t streamingMinBytes_ = kMemBwStreamingMinBytes;

    VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
    VkDescriptorSet hostSet_ = VK_NULL_HANDLE;    // binding 0 = host_
    VkDescriptorSet activeSet_ = VK_NULL_HANDLE;  // set bound by Submit()
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipelines_[kKernelCount]{};
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkCommandBuffer cmd_ = VK_NULL_HANDLE;
    VkFence fence_ = VK_NULL_HANDLE;
    VkQueryPool queryPool_ = VK_NULL_HANDLE;

    std::uint32_t groupsXLimit_ = 1;
    std::uint64_t maxTotalGroups_ = 1;
    bool useGpuTimestamps_ = false;
    double targetDispatchMs_ = 4.0;
    std::uint32_t timestampAnomalies_ = 0;
    std::uint32_t writeCopyChecks_ = 0;
    // The least delayed submission of the samples of one point; see CheckCoverage.
    Sample leastDelayed_{};
    bool haveLeastDelayed_ = false;
};

void Runner::CreateInstance() {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "Mangekyo memory bandwidth";
    app.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app;

    VkResult result = vkCreateInstance(&ci, nullptr, &instance_);
    if (result == VK_ERROR_INCOMPATIBLE_DRIVER) {
        // Vulkan 1.0 loader/driver: every call below is 1.0 core.
        app.apiVersion = VK_API_VERSION_1_0;
        result = vkCreateInstance(&ci, nullptr, &instance_);
    }
    Check(result, "vkCreateInstance");
}

void Runner::SelectDevice(MemBwReport& report) {
    std::uint32_t count = 0;
    Check(vkEnumeratePhysicalDevices(instance_, &count, nullptr), "vkEnumeratePhysicalDevices");
    if (count == 0) throw std::runtime_error("No Vulkan physical device was found");
    std::vector<VkPhysicalDevice> devices(count);
    Check(vkEnumeratePhysicalDevices(instance_, &count, devices.data()),
          "vkEnumeratePhysicalDevices");

    const std::uint32_t index = config_.physicalDeviceIndex < 0
        ? 0u : static_cast<std::uint32_t>(config_.physicalDeviceIndex);
    if (index >= count) {
        throw std::runtime_error("Vulkan physical device index " + std::to_string(index) +
                                 " is out of range (0-" + std::to_string(count - 1) + ")");
    }
    physical_ = devices[index];
    vkGetPhysicalDeviceProperties(physical_, &props_);
    vkGetPhysicalDeviceMemoryProperties(physical_, &memoryProps_);

    const auto& limits = props_.limits;
    if (limits.maxComputeWorkGroupSize[0] < kGroupSize ||
        limits.maxComputeWorkGroupInvocations < kGroupSize) {
        throw std::runtime_error("Device does not support 256-thread compute workgroups");
    }
    groupsXLimit_ = (std::min)(kMaxGroupsX, limits.maxComputeWorkGroupCount[0]);
    maxTotalGroups_ = (std::min)(
        static_cast<std::uint64_t>(groupsXLimit_) * limits.maxComputeWorkGroupCount[1],
        static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)()));

    // First compute-capable queue family, preferring one with timestamps.
    std::uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_, &familyCount, families.data());
    bool found = false;
    for (std::uint32_t i = 0; i < familyCount; ++i) {
        if ((families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) continue;
        if (!found || (timestampValidBits_ == 0 && families[i].timestampValidBits > 0)) {
            queueFamily_ = i;
            timestampValidBits_ = families[i].timestampValidBits;
            found = true;
        }
    }
    if (!found) throw std::runtime_error("Device has no compute queue family");

    for (std::uint32_t i = 0; i < memoryProps_.memoryHeapCount; ++i) {
        if (memoryProps_.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            deviceLocalHeapBytes_ = (std::max)(
                deviceLocalHeapBytes_,
                static_cast<std::uint64_t>(memoryProps_.memoryHeaps[i].size));
        }
    }

    // Decode driver version — NVIDIA uses a custom encoding, others use Vulkan standard.
    const std::uint32_t dv = props_.driverVersion;
    std::string driver;
    if (props_.vendorID == 0x10de) {
        driver = std::to_string((dv >> 22) & 0x3ff) + "." + std::to_string((dv >> 14) & 0xff) +
                 "." + std::to_string((dv >> 6) & 0xff) + "." + std::to_string(dv & 0x3f);
    } else {
        driver = std::to_string(VK_VERSION_MAJOR(dv)) + "." + std::to_string(VK_VERSION_MINOR(dv)) +
                 "." + std::to_string(VK_VERSION_PATCH(dv));
    }

    // Use dynamic lookup to avoid linking against vkGetPhysicalDeviceProperties2,
    // which doesn't exist in Vulkan 1.0 loaders.
    if (props_.apiVersion >= VK_API_VERSION_1_1) {
        auto getProps2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
            vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceProperties2"));
        if (getProps2) {
            VkPhysicalDeviceMaintenance3Properties maintenance3{};
            maintenance3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
            VkPhysicalDeviceDriverProperties driverProps{};
            driverProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
            if (props_.apiVersion >= VK_API_VERSION_1_2) maintenance3.pNext = &driverProps;
            VkPhysicalDeviceProperties2 props2{};
            props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            props2.pNext = &maintenance3;
            getProps2(physical_, &props2);
            if (maintenance3.maxMemoryAllocationSize > 0)
                maxAllocationBytes_ = maintenance3.maxMemoryAllocationSize;
            if (driverProps.driverInfo[0] != '\0')
                driver = std::string(driverProps.driverName) + " " + driverProps.driverInfo;
            else if (driverProps.driverName[0] != '\0')
                driver = std::string(driverProps.driverName) + " " + driver;
        }
    }

    report.deviceName = props_.deviceName;
    report.deviceType = DeviceTypeName(props_.deviceType);
    report.driverVersion = driver;
    report.isSoftware = props_.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ||
                        isSoftwareRendererName(report.deviceName);
    report.unifiedMemory = props_.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ||
                           props_.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
    report.deviceLocalHeapMB = static_cast<std::uint32_t>(deviceLocalHeapBytes_ / kMiB);
    // Optional device-side discovery only. Advertising a counter is not
    // evidence that it was sampled, nor that a curve belongs to that cache.
    std::uint32_t extensionCount = 0;
    if (vkEnumerateDeviceExtensionProperties(physical_, nullptr, &extensionCount, nullptr) == VK_SUCCESS) {
        std::vector<VkExtensionProperties> extensions(extensionCount);
        if (vkEnumerateDeviceExtensionProperties(physical_, nullptr, &extensionCount, extensions.data()) == VK_SUCCESS)
            for (const auto& extension : extensions)
                if (std::strcmp(extension.extensionName, VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME) == 0)
                    report.performanceQueryAdvertised = true;
    }
    if (report.performanceQueryAdvertised) {
        const auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR>(
            vkGetInstanceProcAddr(instance_, "vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR"));
        report.performanceCounterStatus = "enumeration_unavailable";
        std::uint32_t count = 0;
        if (enumerate && enumerate(physical_, queueFamily_, &count, nullptr, nullptr) == VK_SUCCESS && count <= 4096) {
            std::vector<VkPerformanceCounterKHR> counters(count);
            std::vector<VkPerformanceCounterDescriptionKHR> descriptions(count);
            for (auto& counter : counters) counter.sType = VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_KHR;
            for (auto& description : descriptions) description.sType = VK_STRUCTURE_TYPE_PERFORMANCE_COUNTER_DESCRIPTION_KHR;
            if (enumerate(physical_, queueFamily_, &count, counters.data(), descriptions.data()) == VK_SUCCESS) {
                report.performanceCounterStatus = "enumerated_not_sampled";
                for (std::uint32_t i = 0; i < count; ++i) {
                    std::string name = descriptions[i].name;
                    for (char& ch : name) if (ch == ';' || ch == ',' || ch == '=' || ch == '\t' || ch == '\n') ch = '_';
                    if (!report.performanceCounterInventory.empty()) report.performanceCounterInventory += ',';
                    report.performanceCounterInventory += name;
                }
            }
        }
    }
    out_ << "MEMBW_COUNTERS\textension_advertised=" << (report.performanceQueryAdvertised ? 1 : 0)
         << "\tstatus=" << report.performanceCounterStatus
         << "\tinventory=" << report.performanceCounterInventory << "\tcache_attribution=unverified\n";
}

void Runner::CreateDevice() {
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamily_;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkDeviceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount = 1;
    ci.pQueueCreateInfos = &queueInfo;
#ifdef GPU_BENCH_MOLTENVK
    const char* portabilitySubset = "VK_KHR_portability_subset";
    ci.enabledExtensionCount = 1;
    ci.ppEnabledExtensionNames = &portabilitySubset;
#endif
    Check(vkCreateDevice(physical_, &ci, nullptr, &device_), "vkCreateDevice");
    vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);
}

bool Runner::TryCreateBuffer(VkDeviceSize size, Buffer& buffer, bool hostMemory) {
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size;
    bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
               VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bi, nullptr, &buffer.buffer) != VK_SUCCESS) {
        buffer.buffer = VK_NULL_HANDLE;
        return false;
    }

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, buffer.buffer, &requirements);
    // Device buffers prefer device-local memory and fall back to any
    // compatible type. A host buffer must be host-visible memory in a heap
    // that is not device-local, i.e. real system memory rather than a
    // CPU-mappable window into VRAM.
    std::uint32_t typeIndex = memoryProps_.memoryTypeCount;
    for (std::uint32_t i = 0; i < memoryProps_.memoryTypeCount; ++i) {
        if ((requirements.memoryTypeBits & (1u << i)) == 0) continue;
        const VkMemoryType& type = memoryProps_.memoryTypes[i];
        const bool deviceLocal = (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
        if (hostMemory) {
            const bool hostVisible =
                (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
            const bool deviceHeap = (memoryProps_.memoryHeaps[type.heapIndex].flags &
                                     VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0;
            if (hostVisible && !deviceLocal && !deviceHeap) { typeIndex = i; break; }
            continue;
        }
        if (deviceLocal) { typeIndex = i; break; }
        if (typeIndex == memoryProps_.memoryTypeCount) typeIndex = i;
    }
    if (typeIndex == memoryProps_.memoryTypeCount) {
        DestroyBuffer(buffer);
        return false;
    }

    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = requirements.size;
    ai.memoryTypeIndex = typeIndex;
    if (vkAllocateMemory(device_, &ai, nullptr, &buffer.memory) != VK_SUCCESS) {
        buffer.memory = VK_NULL_HANDLE;
        DestroyBuffer(buffer);
        return false;
    }
    if (vkBindBufferMemory(device_, buffer.buffer, buffer.memory, 0) != VK_SUCCESS) {
        DestroyBuffer(buffer);
        return false;
    }
    return true;
}

void Runner::DestroyBuffer(Buffer& buffer) {
    if (buffer.buffer) vkDestroyBuffer(device_, buffer.buffer, nullptr);
    if (buffer.memory) vkFreeMemory(device_, buffer.memory, nullptr);
    buffer = Buffer{};
}

void Runner::CreateBuffers(MemBwReport& report) {
    // Two full-size buffers (source + copy destination) must fit, so keep each
    // within a quarter of the device-local heap.
    std::uint64_t limit = config_.maxDatasetBytes;
    limit = (std::min)(limit, static_cast<std::uint64_t>(props_.limits.maxStorageBufferRange));
    limit = (std::min)(limit, maxAllocationBytes_);
    if (deviceLocalHeapBytes_ > 0) limit = (std::min)(limit, deviceLocalHeapBytes_ / 4);
    // A GPU that shares the system's memory reports all of that memory as its
    // heap, in use or not. A TV with 2.8 GiB installed and 1.5 GiB available
    // (2026-10-08) was given two buffers of 700 MiB and did nothing but page
    // for minutes. Half of what is available may go to the two buffers.
    if (report.unifiedMemory) {
        const std::uint64_t available = AvailableSystemMemoryBytes();
        report.availableMemoryBytes = available;
#if defined(__APPLE__) && TARGET_OS_IPHONE
        report.memoryBudgetPolicy = kIosMemBwBudgetPolicy;
        report.perBufferBudgetBytes = IosMemBwBufferBudget(config_.maxDatasetBytes, available);
        limit = (std::min)(limit, report.perBufferBudgetBytes);
#else
        if (available > 0) limit = (std::min)(limit, available / 4);
#endif
    }

    std::uint64_t chunks = limit / kChunkBytes;
    while (true) {
        if (chunks < 2)
            throw std::runtime_error("Could not allocate buffers for a 1 MiB data set");
        const VkDeviceSize size = chunks * kChunkBytes;
        if (TryCreateBuffer(size, source_) && TryCreateBuffer(size, destination_)) break;
        DestroyBuffer(source_);
        DestroyBuffer(destination_);
        chunks /= 2;
    }
    if (!TryCreateBuffer(kGroupSize * kElementBytes, sink_))
        throw std::runtime_error("Could not allocate the sink buffer");
    CreateReadback();

    maxChunks_ = static_cast<std::uint32_t>(chunks);
    report.maxDatasetBytes = chunks * kChunkBytes;
    report.datasetLimitedByDevice = report.maxDatasetBytes < config_.maxDatasetBytes;

    // System-memory buffer for the PCIe pass. An integrated or software device
    // has no separate VRAM, so the pass would repeat the main sweep.
    if (report.unifiedMemory) {
        report.host.skippedReason = "integrated or software device: all of its memory is system memory";
        return;
    }
    std::uint32_t hostChunks = (std::min)(maxChunks_, kMaxHostChunks);
    while (hostChunks >= kMinHostChunks && !TryCreateBuffer(hostChunks * kChunkBytes, host_, true))
        hostChunks /= 2;
    if (host_.buffer) {
        hostChunks_ = hostChunks;
    } else {
        report.host.skippedReason = "no system-memory buffer of at least 64 MiB could be allocated";
    }
}

// A small buffer the CPU can read. Without one the data check cannot run; the
// sweep then goes ahead unchecked and says so.
void Runner::CreateReadback() {
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = kCheckWords * sizeof(std::uint32_t);
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bi, nullptr, &readback_.buffer) != VK_SUCCESS) {
        readback_.buffer = VK_NULL_HANDLE;
        return;
    }
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, readback_.buffer, &requirements);
    // Coherent memory if there is any; other host-visible memory is brought up
    // to date by the invalidate call in CheckData().
    std::uint32_t typeIndex = memoryProps_.memoryTypeCount;
    for (std::uint32_t i = 0; i < memoryProps_.memoryTypeCount; ++i) {
        if ((requirements.memoryTypeBits & (1u << i)) == 0) continue;
        const VkMemoryPropertyFlags flags = memoryProps_.memoryTypes[i].propertyFlags;
        if ((flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0) continue;
        if ((flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0) { typeIndex = i; break; }
        if (typeIndex == memoryProps_.memoryTypeCount) typeIndex = i;
    }
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = requirements.size;
    ai.memoryTypeIndex = typeIndex;
    if (typeIndex == memoryProps_.memoryTypeCount ||
        vkAllocateMemory(device_, &ai, nullptr, &readback_.memory) != VK_SUCCESS) {
        readback_.memory = VK_NULL_HANDLE;
        DestroyBuffer(readback_);
        return;
    }
    if (vkBindBufferMemory(device_, readback_.buffer, readback_.memory, 0) != VK_SUCCESS ||
        vkMapMemory(device_, readback_.memory, 0, VK_WHOLE_SIZE, 0, &readbackMapped_) != VK_SUCCESS) {
        readbackMapped_ = nullptr;
        DestroyBuffer(readback_);
    }
}

void Runner::CreateDescriptors() {
    VkDescriptorSetLayoutBinding bindings[3]{};
    for (std::uint32_t i = 0; i < 3; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings = bindings;
    Check(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &setLayout_),
          "vkCreateDescriptorSetLayout");

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 2;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    Check(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_),
          "vkCreateDescriptorPool");

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &setLayout_;
    Check(vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet_),
          "vkAllocateDescriptorSets");
    WriteDescriptorSet(descriptorSet_, source_.buffer);
    activeSet_ = descriptorSet_;

    // The host set only swaps binding 0, so the read and write kernels run
    // unchanged against system memory.
    if (host_.buffer) {
        Check(vkAllocateDescriptorSets(device_, &allocInfo, &hostSet_),
              "vkAllocateDescriptorSets");
        WriteDescriptorSet(hostSet_, host_.buffer);
    }
}

void Runner::WriteDescriptorSet(VkDescriptorSet set, VkBuffer source) {
    const VkBuffer buffers[3] = {source, destination_.buffer, sink_.buffer};
    VkDescriptorBufferInfo bufferInfos[3]{};
    VkWriteDescriptorSet writes[3]{};
    for (std::uint32_t i = 0; i < 3; ++i) {
        bufferInfos[i].buffer = buffers[i];
        bufferInfos[i].offset = 0;
        bufferInfos[i].range = VK_WHOLE_SIZE;
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &bufferInfos[i];
    }
    vkUpdateDescriptorSets(device_, 3, writes, 0, nullptr);
}

void Runner::CreatePipelines() {
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    range.size = sizeof(PushParams);
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &setLayout_;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &range;
    Check(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_),
          "vkCreatePipelineLayout");

    for (std::size_t k = 0; k < kKernelCount; ++k) {
        std::string path = config_.shaderDir;
        if (!path.empty() && path.back() != '/' && path.back() != '\\') path += '/';
        path += kKernels[k].spirvFile;
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Failed to open file: " + path);
        const auto size = static_cast<std::size_t>(file.tellg());
        if (size == 0 || size % 4 != 0)
            throw std::runtime_error("Invalid SPIR-V file: " + path);
        std::vector<std::uint32_t> code(size / 4);
        file.seekg(0);
        file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(size));

        VkShaderModuleCreateInfo moduleInfo{};
        moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleInfo.codeSize = size;
        moduleInfo.pCode = code.data();
        VkShaderModule module = VK_NULL_HANDLE;
        Check(vkCreateShaderModule(device_, &moduleInfo, nullptr, &module),
              "vkCreateShaderModule");

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = module;
        pipelineInfo.stage.pName = "main";
        pipelineInfo.layout = pipelineLayout_;
        const VkResult result = vkCreateComputePipelines(
            device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelines_[k]);
        vkDestroyShaderModule(device_, module, nullptr);
        Check(result, "vkCreateComputePipelines");
    }
}

void Runner::CreateCommandObjects() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamily_;
    Check(vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_), "vkCreateCommandPool");

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    Check(vkAllocateCommandBuffers(device_, &allocInfo, &cmd_), "vkAllocateCommandBuffers");

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    Check(vkCreateFence(device_, &fenceInfo, nullptr, &fence_), "vkCreateFence");

    if (timestampValidBits_ > 0 && props_.limits.timestampPeriod > 0.0f) {
        VkQueryPoolCreateInfo queryInfo{};
        queryInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryInfo.queryCount = 2;
        if (vkCreateQueryPool(device_, &queryInfo, nullptr, &queryPool_) != VK_SUCCESS)
            queryPool_ = VK_NULL_HANDLE;
    }
}

// Data-set sweep dispatch: every chunk is visited `passes` times.
void Runner::RecordResourceBarrier() {
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                            VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                            VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd_, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

Runner::Sample Runner::Dispatch(std::size_t kernel, std::uint32_t chunkCount,
                                std::uint64_t passes) {
    if (chunkCount * kChunkBytes >= streamingMinBytes_) {
        // Each dispatch visits every chunk once. A compute execution/memory
        // dependency orders the next complete pass, preventing private-chunk
        // write/copy loops from recycling the same cache-resident block.
        return Submit(kernel, chunkCount, kStripesPerChunk, chunkCount, 1,
                      static_cast<std::uint32_t>(passes));
    }
    if (kernel == kWriteKernel || kernel == kCopyKernel)
        return Submit(kernel, chunkCount, kStripesPerChunk, chunkCount,
                      static_cast<std::uint32_t>(passes));
    return Submit(kernel, chunkCount, kStripesPerChunk,
                  static_cast<std::uint64_t>(chunkCount) * passes, 0);
}

Runner::Sample Runner::Submit(std::size_t kernel, std::uint32_t chunkCount,
                              std::uint32_t stripesPerChunk, std::uint64_t total,
                              std::uint32_t repeats, std::uint32_t orderedPasses) {
    const auto groupsX = static_cast<std::uint32_t>(
        (std::min)(total, static_cast<std::uint64_t>(groupsXLimit_)));
    const auto groupsY = static_cast<std::uint32_t>((total + groupsX - 1) / groupsX);
    const PushParams params{chunkCount, stripesPerChunk, static_cast<std::uint32_t>(total),
                            groupsX, kHashSeed, -1.0f, repeats};

    Check(vkResetCommandBuffer(cmd_, 0), "vkResetCommandBuffer");
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    Check(vkBeginCommandBuffer(cmd_, &beginInfo), "vkBeginCommandBuffer");
    RecordResourceBarrier();
    if (queryPool_) vkCmdResetQueryPool(cmd_, queryPool_, 0, 2);
    vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_COMPUTE, pipelines_[kernel]);
    vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_, 0, 1,
                            &activeSet_, 0, nullptr);
    vkCmdPushConstants(cmd_, pipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(PushParams), &params);
    // The queue is idle here (every dispatch is submitted and waited on its
    // own), so the timestamps bracket the entire ordered dispatch batch.
    if (queryPool_)
        vkCmdWriteTimestamp(cmd_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool_, 0);
    for (std::uint32_t pass = 0; pass < orderedPasses; ++pass) {
        vkCmdDispatch(cmd_, groupsX, groupsY, 1);
        if (pass + 1 < orderedPasses) RecordResourceBarrier();
    }
    if (queryPool_)
        vkCmdWriteTimestamp(cmd_, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, queryPool_, 1);
    return Execute();
}

// One driver buffer copy, timed like a dispatch.
Runner::Sample Runner::SubmitCopy(VkBuffer source, VkBuffer destination, VkDeviceSize bytes) {
    Check(vkResetCommandBuffer(cmd_, 0), "vkResetCommandBuffer");
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    Check(vkBeginCommandBuffer(cmd_, &beginInfo), "vkBeginCommandBuffer");
    RecordResourceBarrier();
    if (queryPool_) {
        vkCmdResetQueryPool(cmd_, queryPool_, 0, 2);
        vkCmdWriteTimestamp(cmd_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool_, 0);
    }
    const VkBufferCopy region{0, 0, bytes};
    vkCmdCopyBuffer(cmd_, source, destination, 1, &region);
    if (queryPool_)
        vkCmdWriteTimestamp(cmd_, VK_PIPELINE_STAGE_TRANSFER_BIT, queryPool_, 1);
    if (destination == readback_.buffer) {
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(cmd_, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
            0, 1, &barrier, 0, nullptr, 0, nullptr);
    }
    return Execute();
}

// Sets a whole buffer to zero, submitted like a dispatch.
Runner::Sample Runner::SubmitClear(VkBuffer buffer) {
    Check(vkResetCommandBuffer(cmd_, 0), "vkResetCommandBuffer");
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    Check(vkBeginCommandBuffer(cmd_, &beginInfo), "vkBeginCommandBuffer");
    RecordResourceBarrier();
    if (queryPool_) {
        vkCmdResetQueryPool(cmd_, queryPool_, 0, 2);
        vkCmdWriteTimestamp(cmd_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool_, 0);
    }
    vkCmdFillBuffer(cmd_, buffer, 0, VK_WHOLE_SIZE, 0);
    if (queryPool_)
        vkCmdWriteTimestamp(cmd_, VK_PIPELINE_STAGE_TRANSFER_BIT, queryPool_, 1);
    return Execute();
}

// Runs the check kernel once over the first chunkCount chunks of the buffer
// bound at binding 0, which the write kernel has just filled. wrongThreads is
// how many of the kGroupSize thread positions met a value other than the one
// stored (a count of positions, not of elements).
Runner::DataCheck Runner::CheckData(std::uint32_t chunkCount, std::uint32_t& wrongThreads) {
    wrongThreads = 0;
    if (!readbackMapped_) return DataCheck::NotRun;
    SubmitClear(sink_.buffer);
    Submit(kCheckKernel, chunkCount, kStripesPerChunk, chunkCount, 0);
    SubmitCopy(sink_.buffer, readback_.buffer, kCheckWords * sizeof(std::uint32_t));
    VkMappedMemoryRange range{};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = readback_.memory;
    range.size = VK_WHOLE_SIZE;
    Check(vkInvalidateMappedMemoryRanges(device_, 1, &range), "vkInvalidateMappedMemoryRanges");
    std::uint32_t words[kCheckWords];
    std::memcpy(words, readbackMapped_, sizeof(words));
    for (std::uint32_t i = 0; i < kGroupSize; ++i)
        if (words[i] != kCheckRan) return DataCheck::NotRun;
    for (std::uint32_t i = kGroupSize; i < kCheckWords; ++i)
        if (words[i] != 0) ++wrongThreads;
    return wrongThreads == 0 ? DataCheck::Good : DataCheck::Wrong;
}

// Fills the source buffer (or the system-memory buffer) through the write
// kernel and checks that the kernels load from it what was stored. A buffer
// that fails is replaced by one of half the size: some drivers advertise a
// storage-buffer range they cannot address (seen on an Adreno 660, which
// loads zeros from a 512 MiB buffer and is correct at 256 MiB). Returns false
// when no buffer of at least minChunks holds its data. The destination buffer
// always has the size of the source buffer and is not checked on its own.
bool Runner::FillChecked(bool hostBuffer, std::uint32_t& chunkCount, std::uint32_t minChunks) {
    const char* what = hostBuffer ? "system-memory buffer" : "buffer";
    activeSet_ = hostBuffer ? hostSet_ : descriptorSet_;
    while (true) {
        Dispatch(kWriteKernel, chunkCount, 1);
        std::uint32_t wrongThreads = 0;
        const DataCheck check = CheckData(chunkCount, wrongThreads);
        if (check == DataCheck::Good) {
            out_ << "[membw] Data check: the kernels load from the " << what << " of "
                 << FormatMiB(chunkCount * kChunkBytes) << " what was stored in it.\n";
            return true;
        }
        if (check == DataCheck::NotRun) {
            throw std::runtime_error("Vulkan memory data check did not execute or cannot be read back; no unchecked score is saved");
        }
        out_ << "[membw] Data check: a " << what << " of " << FormatMiB(chunkCount * kChunkBytes)
             << " does not give back what was stored in it (" << wrongThreads << " of "
             << kGroupSize << " thread positions met a wrong value)";
        const std::uint32_t smaller = chunkCount / 2;
        if (smaller < minChunks) {
            out_ << ".\n";
            return false;
        }
        out_ << "; trying " << FormatMiB(smaller * kChunkBytes) << ".\n";
        const VkDeviceSize size = smaller * kChunkBytes;
        if (hostBuffer) {
            DestroyBuffer(host_);
            if (!TryCreateBuffer(size, host_, true)) return false;
        } else {
            DestroyBuffer(source_);
            DestroyBuffer(destination_);
            if (!TryCreateBuffer(size, source_) || !TryCreateBuffer(size, destination_))
                throw std::runtime_error("Could not allocate the smaller buffers");
        }
        chunkCount = smaller;
        WriteDescriptorSet(descriptorSet_, source_.buffer);
        if (hostSet_ && host_.buffer) WriteDescriptorSet(hostSet_, host_.buffer);
    }
}

// Ends the recorded command buffer, runs it alone on the queue and reads back
// the two timestamps written around the work.
Runner::Sample Runner::Execute() {
    Check(vkEndCommandBuffer(cmd_), "vkEndCommandBuffer");

    Check(vkResetFences(device_, 1, &fence_), "vkResetFences");
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd_;

    Sample sample;
    const auto start = std::chrono::steady_clock::now();
    Check(vkQueueSubmit(queue_, 1, &submit, fence_), "vkQueueSubmit");
    Check(vkWaitForFences(device_, 1, &fence_, VK_TRUE, 300ull * 1000ull * 1000ull * 1000ull),
          "vkWaitForFences");
    const auto end = std::chrono::steady_clock::now();
    sample.wallMs = std::chrono::duration<double, std::milli>(end - start).count();

    if (queryPool_) {
        std::uint64_t ticks[2] = {0, 0};
        const VkResult result = vkGetQueryPoolResults(
            device_, queryPool_, 0, 2, sizeof(ticks), ticks, sizeof(std::uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        if (result == VK_SUCCESS) {
            const std::uint64_t mask = timestampValidBits_ >= 64
                ? ~0ull : ((1ull << timestampValidBits_) - 1ull);
            const std::uint64_t delta = (ticks[1] - ticks[0]) & mask;
            sample.gpuMs = static_cast<double>(delta) *
                           static_cast<double>(props_.limits.timestampPeriod) / 1.0e6;
        }
    }
    return sample;
}

// Each sample is checked for what a correct clock cannot give: no interval, or
// more GPU time than the submission took. Whether the stamps cover too little
// of the work is judged once per point by CheckCoverage, because the wall
// clock of a single submission may include milliseconds of wake-up latency:
// judged per sample, three sweeps on an RTX 5090 under Windows aborted at
// random points (12 MiB read, 256 MiB read, 192 MiB copy; 2026-10-17).
double Runner::TimeOf(const Sample& sample) {
    if (!std::isfinite(sample.wallMs) || sample.wallMs <= 0)
        throw std::runtime_error("Invalid synchronized Vulkan wall-clock interval");
    if (!useGpuTimestamps_) return sample.wallMs;
    if (!std::isfinite(sample.gpuMs) || sample.gpuMs <= 0 ||
        sample.gpuMs > sample.wallMs * 1.1 + 0.1) {
        ++timestampAnomalies_;
        // Per-sample fallback used to mix two timing contracts inside one
        // median/curve, while labeling everything gpu_timestamp.
        throw std::runtime_error("Vulkan timestamps became unreliable during the sweep; mixed timing is rejected and no score is saved");
    }
    if (!haveLeastDelayed_ || sample.wallMs < leastDelayed_.wallMs) {
        leastDelayed_ = sample;
        haveLeastDelayed_ = true;
    }
    return sample.gpuMs;
}

void Runner::BeginCoverage() { haveLeastDelayed_ = false; }

// Stamps that are latched too early read far less than the work took. The
// least delayed submission of a point is the one whose wall clock is closest
// to the work itself, so it decides; one late wake-up no longer does.
void Runner::CheckCoverage() {
    const bool checked = haveLeastDelayed_;
    haveLeastDelayed_ = false;
    if (!useGpuTimestamps_ || !checked) return;
    if (!VulkanTimestampIntervalVerified(leastDelayed_.gpuMs, leastDelayed_.wallMs)) {
        ++timestampAnomalies_;
        throw std::runtime_error("Vulkan timestamps became unreliable during the sweep; mixed timing is rejected and no score is saved");
    }
}

// GPU timestamps are only trusted if they scale with the work: some drivers
// latch both timestamps when the commands are processed rather than executed.
void Runner::DecideTimingSource(MemBwReport& report) {
    useGpuTimestamps_ = false;
#ifdef GPU_BENCH_MOLTENVK
    // A12X passed startup scaling but later dispatch intervals failed the
    // synchronized check. Select one source before measuring any curve point;
    // long dispatches below limit submission and wake-up overhead.
    out_ << "[membw] MoltenVK uses synchronized wall-clock timing for the entire sweep.\n";
#else
    if (queryPool_) {
        const std::uint32_t chunks = (std::min)(maxChunks_, 512u);
        const std::uint64_t maxPasses = MaxPasses(chunks);
        std::uint64_t passes = (std::max<std::uint64_t>)(1, (1024 * kMiB) / (chunks * kChunkBytes));
        passes = (std::min)(passes, (std::max<std::uint64_t>)(1, maxPasses / 4));
        Dispatch(kReadKernel, chunks, passes * 4);  // warm-up
        double small = (std::numeric_limits<double>::max)();
        double large = (std::numeric_limits<double>::max)();
        double largeWall = (std::numeric_limits<double>::max)();
        for (int i = 0; i < 2; ++i) {
            small = (std::min)(small, Dispatch(kReadKernel, chunks, passes).gpuMs);
            const Sample sample = Dispatch(kReadKernel, chunks, passes * 4);
            large = (std::min)(large, sample.gpuMs);
            largeWall = (std::min)(largeWall, sample.wallMs);
        }
        const double ratio = small > 0.0 ? large / small : 0.0;
        useGpuTimestamps_ = ratio >= 2.5 && ratio <= 6.0 &&
                           VulkanTimestampIntervalVerified(large, largeWall);
        out_ << "[membw] Timing check: a read of " << FormatMiB(passes * 4 * chunks * kChunkBytes)
             << " took " << std::fixed << std::setprecision(3) << large << " ms by the GPU's clock, "
             << largeWall << " ms by wall clock.\n";
        if (!useGpuTimestamps_) {
            out_ << "[membw] GPU timestamps failed scaling or synchronized interval checks (x4 work -> x"
                 << std::fixed << std::setprecision(2) << ratio
                 << " time); falling back to wall-clock timing.\n";
        }
    }
#endif
    report.timingSource = useGpuTimestamps_ ? "gpu_timestamp" : "wall_clock";
    // Wall-clock timing includes submit and wake-up latency, so use longer
    // dispatches to keep that overhead small.
    targetDispatchMs_ = useGpuTimestamps_ ? config_.targetDispatchMs
                                          : (std::max)(config_.targetDispatchMs, 50.0);
}

std::uint64_t Runner::MaxPasses(std::uint32_t chunkCount) const {
    const std::uint64_t byGroups = maxTotalGroups_ / chunkCount;
    const std::uint64_t byBytes = kMaxDispatchBytes / (chunkCount * kChunkBytes);
    return (std::max<std::uint64_t>)(1, (std::min)(byGroups, byBytes));
}

// Chooses how many times each chunk is visited so one dispatch lasts about
// the target time, independent of the data-set size.
std::uint64_t Runner::CalibratePasses(std::size_t kernel, std::uint32_t chunkCount) {
    const std::uint64_t maxPasses = MaxPasses(chunkCount);
    std::uint64_t passes = (std::max<std::uint64_t>)(1, (256 * kMiB) / (chunkCount * kChunkBytes));
    passes = (std::min)(passes, maxPasses);
    for (int round = 0; round < 4; ++round) {
        // Best of two: a single dispatch can be stretched by other GPU work.
        const double first = TimeOf(Dispatch(kernel, chunkCount, passes));
        const double ms = (std::min)(first, TimeOf(Dispatch(kernel, chunkCount, passes)));
        if (ms >= 0.6 * targetDispatchMs_ && ms <= 2.5 * targetDispatchMs_) break;
        const double scale = (std::min)(targetDispatchMs_ / (std::max)(ms, 0.01), 64.0);
        const auto next = static_cast<std::uint64_t>(
            std::ceil(static_cast<double>(passes) * scale));
        const std::uint64_t clamped = (std::min)((std::max<std::uint64_t>)(1, next), maxPasses);
        if (clamped == passes) break;
        passes = clamped;
    }
    return passes;
}

MemBwPoint Runner::MeasurePoint(std::size_t kernel, std::uint32_t chunkCount,
                                const char* name) {
    // An app that embeds the engine may stop the sweep (RequestStop). A sweep
    // that was cut short has no levels to report, so it ends as an error and
    // nothing is saved.
    if (StopRequested()) throw std::runtime_error(kMemBwStopped);
    const std::uint64_t passes = CalibratePasses(kernel, chunkCount);
    for (std::uint32_t i = 0; i < config_.warmupRuns; ++i) Dispatch(kernel, chunkCount, passes);

    std::vector<double> times;
    times.reserve(config_.measuredRuns);
    BeginCoverage();
    for (std::uint32_t i = 0; i < config_.measuredRuns; ++i)
        times.push_back(TimeOf(Dispatch(kernel, chunkCount, passes)));
    CheckCoverage();

    MemBwPoint point;
    point.kernel = name ? name : kKernels[kernel].name;
    point.datasetBytes = chunkCount * kChunkBytes;
    point.passes = static_cast<std::uint32_t>(passes);
    point.bytesPerDispatch = chunkCount * passes * kKernels[kernel].bytesPerChunkVisit;
    point.dispatchesPerSample = point.datasetBytes >= streamingMinBytes_
        ? static_cast<std::uint32_t>(passes) : 1;
    FillStatistics(point, times);
    VerifyWrittenData(kernel, chunkCount);
    return point;
}

void Runner::VerifyWrittenData(std::size_t kernel, std::uint32_t chunks) {
    if (kernel != kWriteKernel && kernel != kCopyKernel) return;
    const VkDescriptorSet saved = activeSet_;
    // Verify the copy destination rather than only its source. The GPU is
    // idle after each dispatch; restoring binding0 preserves the next kernel.
    if (kernel == kCopyKernel) {
        activeSet_ = descriptorSet_;
        WriteDescriptorSet(descriptorSet_, destination_.buffer);
    }
    std::uint32_t wrong = 0;
    const auto check = CheckData(chunks, wrong);
    if (kernel == kCopyKernel) WriteDescriptorSet(descriptorSet_, source_.buffer);
    activeSet_ = saved;
    if (check != DataCheck::Good)
        throw std::runtime_error("Vulkan measured write/copy failed data verification; no score is saved");
    ++writeCopyChecks_;
}

// L1 sweep point. Under full load every concurrently running workgroup gets
// its own block; the single-group variant runs one workgroup, i.e. one SM/CU.
MemBwPoint Runner::MeasureL1Point(bool singleGroup, std::uint32_t stripes) {
    if (StopRequested()) throw std::runtime_error(kMemBwStopped);
    const std::uint64_t blockBytes = stripes * kStripeBytes;
    const std::uint64_t totalStripes = static_cast<std::uint64_t>(maxChunks_) * kStripesPerChunk;
    const std::uint32_t blocks = singleGroup ? 1u : static_cast<std::uint32_t>(
        (std::min<std::uint64_t>)(kL1FullLoadBlocks, totalStripes / stripes));
    const std::uint64_t groups = blocks;
    const std::uint64_t passBytes = groups * blockBytes;  // one pass of every group
    // The kernel's loop counter is repeats x (stripes / 2) and must fit 32 bits.
    const std::uint64_t maxRepeats = (std::max<std::uint64_t>)(
        1, (std::min<std::uint64_t>)(kMaxDispatchBytes / passBytes,
                                     0xFFFFFFFFull / (stripes / 2)));
    auto run = [&](std::uint64_t repeats) {
        return TimeOf(Submit(kL1Kernel, blocks, stripes, groups,
                             static_cast<std::uint32_t>(repeats)));
    };

    // Calibrate the repeat count so one dispatch lasts about the target time.
    std::uint64_t repeats = (std::max<std::uint64_t>)(
        1, ((singleGroup ? 64 : 1024) * kMiB) / passBytes);
    repeats = (std::min)(repeats, maxRepeats);
    for (int round = 0; round < 4; ++round) {
        const double first = run(repeats);
        const double ms = (std::min)(first, run(repeats));
        if (ms >= 0.6 * targetDispatchMs_ && ms <= 2.5 * targetDispatchMs_) break;
        const double scale = (std::min)(targetDispatchMs_ / (std::max)(ms, 0.01), 64.0);
        const auto next = static_cast<std::uint64_t>(
            std::ceil(static_cast<double>(repeats) * scale));
        const std::uint64_t clamped = (std::min)((std::max<std::uint64_t>)(1, next), maxRepeats);
        if (clamped == repeats) break;
        repeats = clamped;
    }

    for (std::uint32_t i = 0; i < config_.warmupRuns; ++i) run(repeats);
    std::vector<double> times;
    times.reserve(config_.measuredRuns);
    BeginCoverage();
    for (std::uint32_t i = 0; i < config_.measuredRuns; ++i) times.push_back(run(repeats));
    CheckCoverage();

    MemBwPoint point;
    point.kernel = singleGroup ? "l1_single" : "l1";
    point.datasetBytes = blockBytes;
    point.passes = static_cast<std::uint32_t>(repeats);
    point.bytesPerDispatch = passBytes * repeats;
    FillStatistics(point, times);
    return point;
}

// System-memory pass: the read and write kernels run against a buffer in host
// memory, then the driver copies that buffer to and from VRAM. Every byte
// crosses the PCIe link, so the largest data sets give the usable link rate.
void Runner::MeasureHostMemory(MemBwReport& report) {
    if (!host_.buffer) return;
    // Fill the host buffer through the GPU so the read kernel sees valid data.
    if (!FillChecked(true, hostChunks_, kMinHostChunks)) {
        report.host.skippedReason =
            "no system-memory buffer of at least 64 MiB gives back the data stored in it";
        activeSet_ = descriptorSet_;
        return;
    }

    const struct { std::size_t kernel; const char* name; const char* description; } passes[] = {
        {kReadKernel, "host_read", "16 B loads from system memory (system memory -> GPU)"},
        {kWriteKernel, "host_write", "16 B stores to system memory (GPU -> system memory)"}};
    for (const auto& pass : passes) {
        out_ << "\n[" << pass.name << "] " << pass.description << "\n"
             << "    Data set   Passes   Dispatch MiB   Median ms     Min ms   Median GB/s   Best GB/s\n";
        for (std::uint64_t chunkCount = 2; chunkCount <= hostChunks_; chunkCount *= 2) {
            MemBwPoint point = MeasurePoint(pass.kernel, static_cast<std::uint32_t>(chunkCount),
                                            pass.name);
            PrintPoint(point, false);
            const bool slow = point.medianMs > kHostSlowDispatchMs;
            report.points.push_back(std::move(point));
            if (slow) {
                out_ << "    (sweep stopped: one dispatch already takes more than 0.5 s)\n";
                break;
            }
        }
    }
    activeSet_ = descriptorSet_;
    report.host.read = Summarise(report.points, "host_read", streamingMinBytes_);
    report.host.write = Summarise(report.points, "host_write", streamingMinBytes_);

    // Driver copies in each direction. Size them from the measured read rate
    // so that one copy takes about a quarter of a second at most.
    const double rateBytesPerSec = (std::max)(report.host.read.memoryGBps, 0.05) * 1.0e9;
    std::uint64_t copyChunks = static_cast<std::uint64_t>(rateBytesPerSec * 0.25 / kChunkBytes);
    copyChunks = (std::min<std::uint64_t>)(hostChunks_, (std::max<std::uint64_t>)(128, copyChunks));
    const VkDeviceSize copyBytes = copyChunks * kChunkBytes;
    auto measureCopy = [&](VkBuffer source, VkBuffer destination) {
        for (std::uint32_t i = 0; i < config_.warmupRuns; ++i)
            SubmitCopy(source, destination, copyBytes);
        std::vector<double> times;
        BeginCoverage();
        for (std::uint32_t i = 0; i < config_.measuredRuns; ++i)
            times.push_back(TimeOf(SubmitCopy(source, destination, copyBytes)));
        CheckCoverage();
        const double ms = Median(times);
        return ms > 0.0 ? static_cast<double>(copyBytes) / (ms / 1000.0) / 1.0e9 : 0.0;
    };
    report.host.copyToDeviceGBps = measureCopy(host_.buffer, destination_.buffer);
    report.host.copyToHostGBps = measureCopy(destination_.buffer, host_.buffer);
    report.host.copyBytes = copyBytes;
    report.host.measured = true;
}

// One table row plus its machine-readable line. `blockSweep` rows show the
// private block size of the L1 sweep instead of a data-set size.
void Runner::PrintPoint(const MemBwPoint& point, bool blockSweep) {
    out_ << std::setw(12)
         << (blockSweep ? FormatBlock(point.datasetBytes) : FormatMiB(point.datasetBytes))
         << std::setw(9) << point.passes
         << std::setw(15) << (point.bytesPerDispatch / kMiB)
         << std::fixed << std::setprecision(3)
         << std::setw(12) << point.medianMs << std::setw(11) << point.minMs
         << std::setprecision(1)
         << std::setw(14) << point.medianGBps << std::setw(12) << point.bestGBps << "\n";
    out_ << "MEMBW_POINT\tkernel=" << point.kernel
         << "\tdataset_bytes=" << point.datasetBytes
         << "\tpasses=" << point.passes
         << "\tdispatch_bytes=" << point.bytesPerDispatch
         << "\tdispatches_per_sample=" << point.dispatchesPerSample
         << std::setprecision(6)
         << "\tmedian_ms=" << point.medianMs << "\tmin_ms=" << point.minMs
         << std::setprecision(3)
         << "\tmedian_gbps=" << point.medianGBps << "\tbest_gbps=" << point.bestGBps
         << "\n" << std::flush;
}

void Runner::Run(MemBwReport& report) {
    const auto runStart = std::chrono::steady_clock::now();
    report.chunkBytes = kChunkBytes;
    report.warmupRuns = config_.warmupRuns;
    report.measuredRuns = config_.measuredRuns;

    CreateInstance();
    SelectDevice(report);
    CreateDevice();
    CreateBuffers(report);
    CreateDescriptors();
    CreatePipelines();
    CreateCommandObjects();

    // Fill the whole source buffer with incompressible data before any read,
    // and make sure the kernels see in it what was stored.
    if (!FillChecked(false, maxChunks_, 2))
        throw std::runtime_error("The device does not give back the data stored in a 1 MiB buffer");
    report.dataVerified = true;
    report.maxDatasetBytes = maxChunks_ * kChunkBytes;
    report.datasetLimitedByDevice = report.maxDatasetBytes < config_.maxDatasetBytes;
    streamingMinBytes_ = MemBwStreamingMinBytes(report.maxDatasetBytes);
    // The driver copies of the system-memory pass go through the destination buffer.
    hostChunks_ = (std::min)(hostChunks_, maxChunks_);
    DecideTimingSource(report);

    const char* memoryLabel = report.unifiedMemory ? "system memory" : "VRAM";
    out_ << "\n========== Memory Bandwidth Sweep (" << kMemBwWorkloadVersion << ") ==========\n"
         << "Device:    " << report.deviceName << " (" << report.deviceType << ")\n"
         << "Driver:    " << report.driverVersion << "\n"
         << "Memory:    " << report.deviceLocalHeapMB << " MiB device-local heap, measured as "
         << memoryLabel << "\n"
         << "Sweep:     " << FormatMiB(2 * kChunkBytes) << " - " << FormatMiB(report.maxDatasetBytes)
         << " in " << FormatMiB(kChunkBytes) << " chunks"
         << (report.datasetLimitedByDevice ? " (ceiling lowered by the device)" : "") << "\n"
         << "Timing:    " << (useGpuTimestamps_ ? "GPU timestamps" : "wall clock") << ", "
         << config_.warmupRuns << " warm-up + " << config_.measuredRuns
         << " measured samples per point, median reported\n";
    out_ << "Traversal: complete ordered data-set passes from "
         << FormatMiB(streamingMinBytes_)
         << "; smaller points retain cache-workgroup repetition.\n";
    if (!report.memoryBudgetPolicy.empty()) {
        out_ << "MEMBW_BUDGET\tpolicy=" << report.memoryBudgetPolicy
             << "\trequested_dataset_bytes=" << config_.maxDatasetBytes
             << "\tavailable_app_bytes=" << report.availableMemoryBytes
             << "\tper_buffer_budget_bytes=" << report.perBufferBudgetBytes
             << "\tactual_dataset_bytes=" << report.maxDatasetBytes << "\n";
    }

    const auto chunkCounts = SweepChunkCounts(maxChunks_);
    for (std::size_t k = 0; k < kSweepKernelCount; ++k) {
        out_ << "\n[" << kKernels[k].name << "] " << kKernels[k].description << "\n"
             << "    Data set   Passes   Dispatch MiB   Median ms     Min ms   Median GB/s   Best GB/s\n";
        for (const std::uint32_t chunkCount : chunkCounts) {
            MemBwPoint point = MeasurePoint(k, chunkCount);
            PrintPoint(point, false);
            report.points.push_back(std::move(point));
        }
    }

    // L1 sweep: a private block per workgroup, first with all workgroups busy
    // and then with a single workgroup.
    for (const bool singleGroup : {false, true}) {
        out_ << "\n[" << (singleGroup ? "l1_single" : "l1") << "] "
             << (singleGroup ? "one workgroup re-reads one block (one SM/CU)"
                             : "every workgroup re-reads a private block (all SMs/CUs busy)")
             << "\n"
             << "       Block  Repeats   Dispatch MiB   Median ms     Min ms   Median GB/s   Best GB/s\n";
        for (const std::uint32_t stripes : kL1StripeCounts) {
            MemBwPoint point = MeasureL1Point(singleGroup, stripes);
            PrintPoint(point, true);
            report.points.push_back(std::move(point));
        }
    }

    MeasureHostMemory(report);

    report.read = Summarise(report.points, "read", streamingMinBytes_);
    report.write = Summarise(report.points, "write", streamingMinBytes_);
    report.copy = Summarise(report.points, "copy", streamingMinBytes_);
    report.l1 = SummariseL1(report.points);
    report.timestampAnomalies = timestampAnomalies_;
    report.writeCopyChecks = writeCopyChecks_;
    report.durationSec = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - runStart).count();

    out_ << "MEMBW_DEVICE\tname=" << report.deviceName
         << "\tmemory=" << (report.unifiedMemory ? "system" : "vram")
         << "\ttiming=" << report.timingSource
         << "\tdata_verified=" << (report.dataVerified ? 1 : 0)
         << "\twrite_copy_checks=" << report.writeCopyChecks
         << "\tmax_dataset_bytes=" << report.maxDatasetBytes << "\n";
    out_ << "\n--- Summary (" << memoryLabel << " plateau = median of the largest data sets) ---\n";
    const struct { const char* name; const MemBwKernelSummary* summary; } rows[] = {
        {"read", &report.read}, {"write", &report.write}, {"copy", &report.copy}};
    for (const auto& row : rows) {
        out_ << std::left << std::setw(6) << row.name << std::right
             << std::fixed << std::setprecision(1) << std::setw(9) << row.summary->memoryGBps
             << " GB/s ";
        if (!row.summary->memoryPlateauReached) {
            // Do not label a cache-assisted figure as memory bandwidth.
            out_ << "at the largest data sets; " << memoryLabel
                 << " plateau not reached (the largest data sets did not all stream, or the curve is still descending)";
        } else if (row.summary->cacheGBps > 0.0) {
            out_ << memoryLabel << " | cache plateau " << row.summary->cacheGBps << " GB/s ("
                 << FormatMiB(row.summary->cacheFromBytes) << " - "
                 << FormatMiB(row.summary->cacheToBytes) << ")";
            if (row.summary->cacheKneeBytes > row.summary->cacheToBytes) {
                out_ << ", cache-served up to " << FormatMiB(row.summary->cacheKneeBytes);
            }
        } else {
            out_ << memoryLabel << " | no distinct cache plateau";
        }
        out_ << "\n";
        out_ << "MEMBW_SUMMARY\tkernel=" << row.name
             << std::setprecision(3)
             << "\tmemory_gbps=" << row.summary->memoryGBps
             << "\tcache_gbps=" << row.summary->cacheGBps
             << "\tcache_from_bytes=" << row.summary->cacheFromBytes
             << "\tcache_to_bytes=" << row.summary->cacheToBytes
             << "\tcache_knee_bytes=" << row.summary->cacheKneeBytes
             << "\tmemory_plateau_reached=" << (row.summary->memoryPlateauReached ? 1 : 0)
             << "\n";
    }
    out_ << std::left << std::setw(6) << "L1" << std::right
         << std::fixed << std::setprecision(1) << std::setw(9) << report.l1.fullLoadGBps
         << " GB/s with all SMs/CUs busy, for private blocks up to "
         << FormatBlock(report.l1.fullLoadBlockBytes) << " per workgroup\n"
         << std::left << std::setw(6) << "L1" << std::right
         << std::setw(9) << report.l1.singleGroupGBps << " GB/s on one SM/CU";
    if (report.l1.capacityHighBytes > 0) {
        out_ << " | L1 capacity between " << FormatBlock(report.l1.capacityLowBytes)
             << " and " << FormatBlock(report.l1.capacityHighBytes);
    } else {
        out_ << " | no drop inside the 16 KiB - 1 MiB block sweep";
    }
    out_ << "\nMEMBW_L1" << std::setprecision(3)
         << "\tfull_load_gbps=" << report.l1.fullLoadGBps
         << "\tfull_load_block_bytes=" << report.l1.fullLoadBlockBytes
         << "\tsingle_group_gbps=" << report.l1.singleGroupGBps
         << "\tcapacity_low_bytes=" << report.l1.capacityLowBytes
         << "\tcapacity_high_bytes=" << report.l1.capacityHighBytes << "\n";
    if (report.host.measured) {
        out_ << std::setprecision(1)
             << std::left << std::setw(6) << "host" << std::right << std::setw(9)
             << report.host.read.memoryGBps << " GB/s system memory -> GPU, compute-shader loads";
        if (!report.host.read.memoryPlateauReached)
            out_ << " (largest data sets, not a settled plateau)";
        out_ << "\n" << std::left << std::setw(6) << "host" << std::right << std::setw(9)
             << report.host.write.memoryGBps << " GB/s GPU -> system memory, compute-shader stores";
        if (!report.host.write.memoryPlateauReached)
            out_ << " (largest data sets, not a settled plateau)";
        out_ << "\n" << std::left << std::setw(6) << "host" << std::right << std::setw(9)
             << report.host.copyToDeviceGBps << " GB/s to VRAM and " << report.host.copyToHostGBps
             << " GB/s to system memory, driver buffer copies of "
             << FormatMiB(report.host.copyBytes) << "\n"
             << "      Every byte of the host figures crosses PCIe. One lane carries 0.985 GB/s (gen 3),\n"
             << "      1.969 GB/s (gen 4) or 3.938 GB/s (gen 5) per direction before protocol overhead.\n";
    } else {
        out_ << std::left << std::setw(6) << "host" << std::right
             << "  skipped: " << report.host.skippedReason << "\n";
    }
    out_ << "MEMBW_HOST\tmeasured=" << (report.host.measured ? 1 : 0) << std::setprecision(3)
         << "\tread_gbps=" << report.host.read.memoryGBps
         << "\twrite_gbps=" << report.host.write.memoryGBps
         << "\tcopy_to_device_gbps=" << report.host.copyToDeviceGBps
         << "\tcopy_to_host_gbps=" << report.host.copyToHostGBps
         << "\tcopy_bytes=" << report.host.copyBytes << "\n" << std::setprecision(1);
    if (report.read.smallSetsL1Assisted) {
        out_ << "Note: data sets below " << FormatMiB(report.read.cacheFromBytes)
             << " ran faster than the cache plateau because several workgroups on one\n"
             << "      SM/CU share a chunk through L1; they are not an L2 measurement.\n";
    }
    if (report.read.cacheGBps > 0.0) {
        out_ << "Note: the cache range is not an exact capacity. Between the cache and "
             << memoryLabel << " plateaus,\n"
             << "      concurrent workgroups share chunks through the cache and stretch the transition.\n";
    }
    if (timestampAnomalies_ > 0) {
        out_ << "Note: " << timestampAnomalies_
             << " dispatch(es) had an implausible GPU timestamp and used wall-clock time.\n";
    }
    out_ << "Duration:  " << std::setprecision(1) << report.durationSec << " s\n"
         << "==========================================================\n" << std::flush;
    report.valid = report.read.memoryGBps > 0.0;
}

void Runner::Destroy() {
    if (device_) {
        vkDeviceWaitIdle(device_);
        if (queryPool_) vkDestroyQueryPool(device_, queryPool_, nullptr);
        if (fence_) vkDestroyFence(device_, fence_, nullptr);
        if (commandPool_) vkDestroyCommandPool(device_, commandPool_, nullptr);
        for (auto& pipeline : pipelines_)
            if (pipeline) vkDestroyPipeline(device_, pipeline, nullptr);
        if (pipelineLayout_) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        if (descriptorPool_) vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        if (setLayout_) vkDestroyDescriptorSetLayout(device_, setLayout_, nullptr);
        DestroyBuffer(host_);
        DestroyBuffer(readback_);
        DestroyBuffer(sink_);
        DestroyBuffer(destination_);
        DestroyBuffer(source_);
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (instance_) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

}  // namespace

MemBwReport RunVulkanMemoryBandwidth(const MemBwConfig& config, std::ostream& out) {
    MemBwReport report;
    try {
        Runner runner(config, out);
        runner.Run(report);
    } catch (const std::exception& e) {
        report.valid = false;
        report.error = e.what();
    }
    return report;
}

}  // namespace gpu_bench
