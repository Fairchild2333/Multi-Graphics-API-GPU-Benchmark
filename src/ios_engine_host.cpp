#include "ios_engine_host.h"

#include "embedded_request.h"
#include "gpu_engine.h"
#include "metal_backend.h"
#include "gpu_cache_probe.h"
#include "gpu_memory_sweep.h"
#include "ios_gles_backend.h"
#include "vulkan_membw_results.h"
#ifdef HAVE_VULKAN
#include "vulkan_backend.h"
#endif
#include <iostream>
#include <cstring>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace gpu_bench::ios_host {
namespace {

std::atomic<bool> g_running{false};
std::mutex g_runMutex;

std::string EnsureTrailingSlash(std::string path) {
    if (path.empty()) return path;
    const char c = path.back();
    if (c != '/' && c != '\\') path.push_back('/');
    return path;
}

}  // namespace

bool IsRunning() {
    return g_running.load(std::memory_order_acquire);
}

bool VulkanAvailable(bool* liquidSupported, std::uint64_t* maxParticleBufferBytes) {
    if (liquidSupported) *liquidSupported = false;
    if (maxParticleBufferBytes) *maxParticleBufferBytes = 0;
#ifdef HAVE_VULKAN
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.apiVersion = VK_API_VERSION_1_1;
    std::uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());
    const bool portability = std::any_of(availableExtensions.begin(), availableExtensions.end(),
        [](const auto& extension) {
            return std::strcmp(extension.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0;
        });
    const char* extensions[] = {VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};
    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app;
    info.flags = portability ? VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR : 0;
    info.enabledExtensionCount = portability ? 1 : 0;
    info.ppEnabledExtensionNames = extensions;
    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&info, nullptr, &instance) != VK_SUCCESS) return false;
    std::uint32_t count = 0;
    const auto status = vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (status == VK_SUCCESS && count > 0 && (liquidSupported || maxParticleBufferBytes)) {
        std::vector<VkPhysicalDevice> devices(count);
        if (vkEnumeratePhysicalDevices(instance, &count, devices.data()) == VK_SUCCESS) {
            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(devices.front(), VK_FORMAT_R32_SFLOAT, &properties);
            const auto required = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            if (liquidSupported) *liquidSupported = (properties.optimalTilingFeatures & required) == required;
            if (maxParticleBufferBytes) {
                VkPhysicalDeviceMaintenance3Properties allocation{};
                allocation.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
                VkPhysicalDeviceProperties2 limits{};
                limits.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                limits.pNext = &allocation;
                vkGetPhysicalDeviceProperties2(devices.front(), &limits);
                *maxParticleBufferBytes = limits.properties.limits.maxStorageBufferRange;
                if (allocation.maxMemoryAllocationSize > 0)
                    *maxParticleBufferBytes = std::min(*maxParticleBufferBytes,
                        static_cast<std::uint64_t>(allocation.maxMemoryAllocationSize));
            }
        }
    }
    vkDestroyInstance(instance, nullptr);
    return status == VK_SUCCESS && count > 0;
#else
    return false;
#endif
}

int RunWorkload(const RunRequest& request, std::string& errorOut) {
    errorOut.clear();
    const auto id = request.workloadId.substr(0, request.workloadId.find('?'));
    const auto api = embedded_detail::OptionValue(request.workloadId, "api");
    if (!api.empty() && api != "metal" && api != "vulkan" && api != "gles2") {
        errorOut = "iOS supports Metal, optional Vulkan (MoltenVK) and legacy OpenGL ES 2.0";
        return 2;
    }
    if (api == "vulkan" && !request.allowTranslatedApis) {
        errorOut = "Vulkan uses MoltenVK translation; enable translated APIs first";
        return 2;
    }
    if (api == "vulkan" && id == "gpu_cache") {
        errorOut = "Cache-state diagnostic uses native Metal only";
        return 2;
    }
    if (api == "gles2" && (id != "gpu_burn" || !request.glesLayer)) {
        errorOut = "iOS OpenGL ES 2.0 supports rendered legacy GPU Burn only";
        return 2;
    }
    if (api != "gles2" && !request.metalLayer && id != "stream_headless" && id != "memory_sweep" && id != "gpu_cache") {
        errorOut = "CAMetalLayer is null";
        return 2;
    }
    if (request.shaderDir.empty()) {
        errorOut = "shaderDir is empty";
        return 3;
    }

    std::unique_lock<std::mutex> lock(g_runMutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        errorOut = "workload already running";
        return 4;
    }

    g_running.store(true, std::memory_order_release);
    ClearStopRequest();

    if (!request.dataDir.empty())
        setenv("GPU_BENCH_DATA_DIR", request.dataDir.c_str(), 1);

    try {
        if (id == "gpu_cache") {
            GpuCacheProbeConfig config;
            config.parallelSteps = GpuCacheDefaultParallelSteps();
            const auto maximum = embedded_detail::OptionValue(request.workloadId, "max_mib");
            const auto steps = embedded_detail::OptionValue(request.workloadId, "steps");
            const auto repeats = embedded_detail::OptionValue(request.workloadId, "repeats");
            const auto parallel = embedded_detail::OptionValue(request.workloadId, "parallel_steps");
            if (!maximum.empty()) config.maxMiB = embedded_detail::ParseCount(maximum, "max_mib");
            if (!steps.empty()) config.steps = embedded_detail::ParseCount(steps, "steps");
            if (!repeats.empty()) config.repeats = embedded_detail::ParseCount(repeats, "repeats");
            if (!parallel.empty()) config.parallelSteps = embedded_detail::ParseCount(parallel, "parallel_steps");
            config.isolationControls = embedded_detail::OptionValue(request.workloadId, "isolation_controls") == "1";
            config.aluConditioning = embedded_detail::OptionValue(request.workloadId, "alu_conditioning") == "1";
            const bool save = embedded_detail::OptionValue(request.workloadId, "save") != "0";
            const int code = RunGpuCacheProbeCli(config, save);
            g_running.store(false, std::memory_order_release);
            if (code) errorOut = "GPU cache diagnostic failed rc=" + std::to_string(code);
            return code;
        }
        if (id == "memory_sweep") {
            int code;
            if (api == "vulkan") {
#ifdef HAVE_VULKAN_MEMBW
                MemBwConfig config;
                config.shaderDir = EnsureTrailingSlash(request.shaderDir);
                config.maxDatasetBytes = 256ull << 20;
                const auto maximum = embedded_detail::OptionValue(request.workloadId, "max_mib");
                if (!maximum.empty()) {
                    const auto count = embedded_detail::ParseCount(maximum, "max_mib");
                    if (count > 256) throw std::invalid_argument("iOS memory sweep max_mib must be 1–256");
                    config.maxDatasetBytes = std::uint64_t(count) << 20;
                }
                const auto report = RunVulkanMemoryBandwidth(config, std::cout);
                if (!report.valid) throw std::runtime_error(report.error);
                const bool save = embedded_detail::OptionValue(request.workloadId, "save") != "0";
                code = save && !AppendResult(MakeStoredVulkanMemBwResult(report, config,
                    report.deviceName, report.deviceLocalHeapMB, AppBase::HostCpuName())) ? 5 : 0;
#else
                throw std::runtime_error("Vulkan runtime not bundled in this build");
#endif
            } else {
                GpuMemorySweepConfig config;
                config.maxKiB = 256u * 1024u;
                config.trafficPerPointMiB = 256;
                code = RunGpuMemorySweepCli(config, true);
            }
            g_running.store(false, std::memory_order_release);
            if (code) errorOut = "GPU memory sweep failed rc=" + std::to_string(code);
            return code;
        }
        BenchmarkConfig cfg;
        // The test and its size, as the desktop command line sets them.
        ApplyEmbeddedTest(request.workloadId, cfg);
        if (cfg.workload == Workload::Stream && !cfg.headless &&
            embedded_detail::OptionValue(request.workloadId, "coverage").empty())
            cfg.particleFixedCoverage = true;
        if (api == "gles2") ApplyEs2Legacy(cfg);
        cfg.maxRunTimeSec = request.maxRunTimeSec > 0.0 ? request.maxRunTimeSec : 3.0;
        cfg.renderDocEnabled = false;
        cfg.captureAtSec = -1.0;
        cfg.captureAtFrame = -1;
        // ApplyEmbeddedTest preserves compute-only requests and V-Sync.

        std::unique_ptr<AppBase> app;
        if (api == "gles2") {
            app = std::make_unique<IosGlesBackend>(0, EnsureTrailingSlash(request.shaderDir), cfg);
        } else if (api == "vulkan") {
#ifdef HAVE_VULKAN
            std::cout << "[iOS] Vulkan via MoltenVK -> Metal (translated)\n";
            app = std::make_unique<VulkanBackend>(0, EnsureTrailingSlash(request.shaderDir), cfg);
#else
            throw std::runtime_error("Vulkan runtime not bundled in this build");
#endif
        } else {
            app = std::make_unique<MetalBackend>(0, EnsureTrailingSlash(request.shaderDir), cfg);
        }
        app->SetEmbeddedWindow(api == "gles2" ? request.glesLayer : request.metalLayer);
        app->Run();
        g_running.store(false, std::memory_order_release);
        return 0;
    } catch (const std::exception& ex) {
        errorOut = ex.what();
        g_running.store(false, std::memory_order_release);
        return 1;
    } catch (...) {
        errorOut = "unknown native exception";
        g_running.store(false, std::memory_order_release);
        return 1;
    }
}

}  // namespace gpu_bench::ios_host
