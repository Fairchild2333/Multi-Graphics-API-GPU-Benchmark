#ifdef HAVE_VULKAN

#include "vulkan_backend.h"
#include "cinematic_liquid_v2_common.h"
#include "mini_mat.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

#if defined(__ANDROID__)
#include <android/native_window.h>
#endif


#if defined(__ANDROID__)
// NDK libvulkan is a Vulkan 1.0 stub: 1.1 device-group entry points are absent.
// Multi-GPU AFR is desktop-only; keep single-GPU Android paths compiling.
#endif
namespace gpu_bench {

void VulkanBackend::InitBackend() {
    if (config_.workload == Workload::Fluid ||
        isCinematicLiquidWorkload(config_.workload)) {
        if (config_.headless)
            throw std::invalid_argument("The selected fluid workload requires the windowed Vulkan render path");
        // The simulation has a deliberate frame-to-frame state dependency.
        // One in-flight submission preserves that order and avoids shared-state
        // hazards without duplicating the entire simulation per swapchain slot.
        if (config_.framesInFlight != 1) {
            std::cout << "[Fluid] forcing frames-in-flight=1 for ordered simulation state\n";
            config_.framesInFlight = 1;
        }
    }
    CreateInstance();
    if (!config_.headless) CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    if (!config_.headless) {
        CreateSwapChain();
        CreateImageViews();
        if (config_.workload == Workload::Render3D) CreateDepthResources();
        CreateRenderPass();
        if (config_.workload == Workload::StressFractal)
            CreateFullscreenPipeline("fractal.vert.spv", "fractal.frag.spv");
        else if (config_.workload == Workload::GpuStressV1)
            CreateFullscreenPipeline("fractal.vert.spv", "gpu_stress.frag.spv");
        else if (config_.workload == Workload::GpuBurnV1)
            CreateFullscreenPipeline("fractal.vert.spv", "gpu_burn.frag.spv");
        else if (config_.workload == Workload::Volumetric)
            CreateFullscreenPipeline("volumetric.vert.spv", "volumetric.frag.spv");
        else if (config_.workload == Workload::Render3D) CreateRender3DPipeline();
        else if (config_.workload != Workload::Fluid &&
                 !isCinematicLiquidWorkload(config_.workload)) CreateGraphicsPipeline();
    }
    CreateComputeDescriptorSetLayout();
    CreateCommandPool();
    CreateParticleBuffer();
    if (config_.workload == Workload::Render3D && !config_.headless)
        CreateQuadBuffer();
    if (config_.workload == Workload::Fluid)
        CreateFluidResources();
    if (config_.workload == Workload::CinematicLiquid)
        CreateCinematicLiquidV2Resources();
    else if (config_.workload == Workload::CinematicLiquidV1)
        CreateCinematicLiquidResources();
    CreateComputeDescriptorResources();
    CreateComputePipeline();
    if (!config_.headless) {
        CreateFramebuffers(); CreateCommandBuffers();
    }
    CreateSyncObjects();
    CreateTimestampQueryPool();

    SetObjectName(VK_OBJECT_TYPE_BUFFER,   reinterpret_cast<std::uint64_t>(particleBuffer_),   "Particle SSBO");
    SetObjectName(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<std::uint64_t>(computePipeline_),  "Compute Pipeline");
    if (!config_.headless)
        SetObjectName(VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<std::uint64_t>(graphicsPipeline_), "Graphics Pipeline");
    if (!config_.headless)
        SetObjectName(VK_OBJECT_TYPE_RENDER_PASS, reinterpret_cast<std::uint64_t>(renderPass_), "Main Render Pass");
}

void VulkanBackend::WaitIdle() {
    if (device_ != VK_NULL_HANDLE)
        vkDeviceWaitIdle(device_);
}

// -----------------------------------------------------------------------
// Instance & Surface
// -----------------------------------------------------------------------

void VulkanBackend::CreateInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Mangekyo";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 2, 4);
    appInfo.pEngineName        = "Mangekyo";
    appInfo.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
    // Android primary floor is Vulkan 1.0 (Tegra K1). Desktop keeps 1.1.
#if defined(__ANDROID__)
    appInfo.apiVersion         = VK_API_VERSION_1_0;
#else
    appInfo.apiVersion         = VK_API_VERSION_1_1;
#endif

    std::vector<const char*> extensions;
    if (!config_.headless) {
#if defined(__ANDROID__)
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#else
        std::uint32_t glfwExtCount = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        extensions.assign(glfwExts, glfwExts + glfwExtCount);
#endif
    }
    std::uint32_t availableExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionCount,
                                           availableExtensions.data());
    const bool hasDebugUtils = std::any_of(
        availableExtensions.begin(), availableExtensions.end(),
        [](const VkExtensionProperties& ext) {
            return std::strcmp(ext.extensionName,
                               VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0;
        });
    if (hasDebugUtils)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = static_cast<std::uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();

    if (vkCreateInstance(&ci, nullptr, &instance_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateInstance failed");

    LoadDebugUtilsFunctions();
}

// -----------------------------------------------------------------------
// Debug Utils (RenderDoc labels & object names)
// -----------------------------------------------------------------------

void VulkanBackend::LoadDebugUtilsFunctions() {
    vkCmdBeginDebugUtilsLabel_ = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance_, "vkCmdBeginDebugUtilsLabelEXT"));
    vkCmdEndDebugUtilsLabel_ = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(instance_, "vkCmdEndDebugUtilsLabelEXT"));
    vkSetDebugUtilsObjectName_ = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
        vkGetInstanceProcAddr(instance_, "vkSetDebugUtilsObjectNameEXT"));
    debugUtilsAvailable_ = vkCmdBeginDebugUtilsLabel_ && vkCmdEndDebugUtilsLabel_;
}

void VulkanBackend::SetObjectName(VkObjectType type, std::uint64_t handle, const char* name) const {
    if (!vkSetDebugUtilsObjectName_ || device_ == VK_NULL_HANDLE) return;
    VkDebugUtilsObjectNameInfoEXT ni{};
    ni.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    ni.objectType   = type;
    ni.objectHandle = handle;
    ni.pObjectName  = name;
    vkSetDebugUtilsObjectName_(device_, &ni);
}

void VulkanBackend::BeginDebugLabel(VkCommandBuffer cmd, const char* name,
                                     float r, float g, float b) const {
    if (!debugUtilsAvailable_) return;
    VkDebugUtilsLabelEXT label{};
    label.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = name;
    label.color[0] = r; label.color[1] = g; label.color[2] = b; label.color[3] = 1.0f;
    vkCmdBeginDebugUtilsLabel_(cmd, &label);
}

void VulkanBackend::EndDebugLabel(VkCommandBuffer cmd) const {
    if (!debugUtilsAvailable_) return;
    vkCmdEndDebugUtilsLabel_(cmd);
}

// -----------------------------------------------------------------------

void VulkanBackend::CreateSurface() {
#if defined(__ANDROID__)
    auto* nativeWindow = static_cast<ANativeWindow*>(window_);
    if (!nativeWindow)
        throw std::runtime_error("Android Vulkan surface requires ANativeWindow");
    VkAndroidSurfaceCreateInfoKHR ci{};
    ci.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    ci.window = nativeWindow;
    if (vkCreateAndroidSurfaceKHR(instance_, &ci, nullptr, &surface_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateAndroidSurfaceKHR failed");
#else
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS)
        throw std::runtime_error("glfwCreateWindowSurface failed");
#endif
}

// -----------------------------------------------------------------------
// Physical / Logical device
// -----------------------------------------------------------------------

QueueFamilyIndices VulkanBackend::FindQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;
    std::uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    // This backend records compute and graphics into the same command buffer
    // and submits it to graphicsQueue_. Therefore its command-pool family must
    // support both capabilities; accepting separate graphics-only/compute-only
    // families would create an invalid submission on otherwise legal devices.
    std::optional<std::uint32_t> combinedFamily;
    for (std::uint32_t i = 0; i < count; ++i) {
        const VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        if ((families[i].queueFlags & required) != required)
            continue;
        if (!combinedFamily) combinedFamily = i;
        if (!config_.headless) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &present);
            if (present) {
                combinedFamily = i;
                break;
            }
        }
    }

    if (!combinedFamily)
        return indices;

    indices.graphicsFamily = combinedFamily;
    indices.computeFamily = combinedFamily;
    if (config_.headless) {
        // No surface in headless mode; keep IsComplete() and queue creation on
        // the same combined family.
        indices.presentFamily = combinedFamily;
    } else {
        // On the D700 device group the only graphics-capable family exposes
        // one VkQueue, while the compute/transfer-only families can also
        // present.  Keeping vkQueuePresentKHR on the graphics queue places a
        // present operation between alternating device-mask submissions and
        // prevents the driver from overlapping them.  For AFR, prefer any
        // separate present-capable family (search backwards so the lightest
        // transfer-only family wins on this driver).
        if (deviceGroupAfr_) {
            for (std::uint32_t i = count; i-- > 0;) {
                if (i == *combinedFamily)
                    continue;
                VkBool32 present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &present);
                if (present) {
                    indices.presentFamily = i;
                    break;
                }
            }
        }

        if (indices.presentFamily)
            return indices;

        VkBool32 combinedCanPresent = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, *combinedFamily, surface_,
                                             &combinedCanPresent);
        if (combinedCanPresent) {
            indices.presentFamily = combinedFamily;
        } else {
            for (std::uint32_t i = 0; i < count; ++i) {
                VkBool32 present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &present);
                if (present) {
                    indices.presentFamily = i;
                    break;
                }
            }
        }
    }
    return indices;
}

SwapChainSupportDetails VulkanBackend::QuerySwapChainSupport(VkPhysicalDevice device) const {
    SwapChainSupportDetails d;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &d.capabilities);

    std::uint32_t n = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &n, nullptr);
    if (n) { d.formats.resize(n); vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &n, d.formats.data()); }

    n = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &n, nullptr);
    if (n) { d.presentModes.resize(n); vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &n, d.presentModes.data()); }

    return d;
}

bool VulkanBackend::CheckDeviceExtensionSupport(VkPhysicalDevice device) const {
    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    std::set<std::string> required(std::begin(kRequiredDeviceExtensions),
                                   std::end(kRequiredDeviceExtensions));
    for (auto& ext : available) required.erase(ext.extensionName);
    return required.empty();
}

bool VulkanBackend::IsDeviceSuitable(VkPhysicalDevice device) const {
    const auto idx = FindQueueFamilies(device);
    if (config_.headless) {
        // Only need compute queue for headless
        return idx.computeFamily.has_value() && idx.graphicsFamily.has_value();
    }
    if (!idx.IsComplete() || !CheckDeviceExtensionSupport(device)) return false;
    const auto sc = QuerySwapChainSupport(device);
    return !sc.formats.empty() && !sc.presentModes.empty();
}

const char* VulkanBackend::DeviceTypeName(VkPhysicalDeviceType type) {
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "Discrete GPU";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "Virtual GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "CPU";
        default:                                     return "Other";
    }
}

void VulkanBackend::PickPhysicalDevice() {
    std::uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) throw std::runtime_error("No Vulkan physical device found");

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    std::vector<std::uint32_t> suitable;
    std::cout << "Available GPUs:\n";
    for (std::uint32_t i = 0; i < count; ++i) {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(devices[i], &p);
        bool ok = IsDeviceSuitable(devices[i]);
        std::cout << "  [" << i << "] " << p.deviceName
                  << " (" << DeviceTypeName(p.deviceType) << ")"
                  << (ok ? "" : " [unsuitable]") << '\n';
        if (ok) suitable.push_back(i);
    }
    if (suitable.empty()) throw std::runtime_error("No suitable Vulkan device");

    std::uint32_t chosen = suitable[0];
    if (requestedGpuIndex_ >= 0) {
        auto idx = static_cast<std::uint32_t>(requestedGpuIndex_);
        if (idx >= count || !IsDeviceSuitable(devices[idx]))
            throw std::runtime_error("Requested GPU index unsuitable");
        chosen = idx;
    } else if (suitable.size() > 1) {
        std::cout << "Enter GPU index (or 'b' to go back): ";
        std::string line;
        if (std::getline(std::cin, line) && !line.empty()) {
            if (line == "b" || line == "B")
                throw gpu_bench::BackToMenuException();
            auto idx = static_cast<std::uint32_t>(std::stoi(line));
            if (idx >= count || !IsDeviceSuitable(devices[idx]))
                throw std::runtime_error("GPU index unsuitable");
            chosen = idx;
        }
    }

    physicalDevice_ = devices[chosen];

    if (config_.multiGpuMode == MultiGpuMode::Afr) {
#if defined(__ANDROID__)
        throw std::runtime_error("Vulkan AFR is not supported on Android");
#else
        std::uint32_t groupCount = 0;
        VkResult groupResult = vkEnumeratePhysicalDeviceGroups(instance_, &groupCount, nullptr);
        if (groupResult != VK_SUCCESS || groupCount == 0)
            throw std::runtime_error(
                "Vulkan AFR requested, but the driver exposes no physical-device group");

        std::vector<VkPhysicalDeviceGroupProperties> groups(groupCount);
        for (auto& group : groups)
            group.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES;
        groupResult = vkEnumeratePhysicalDeviceGroups(instance_, &groupCount, groups.data());
        if (groupResult != VK_SUCCESS)
            throw std::runtime_error("vkEnumeratePhysicalDeviceGroups failed");

        VkPhysicalDeviceProperties selectedProps{};
        vkGetPhysicalDeviceProperties(physicalDevice_, &selectedProps);
        for (const auto& group : groups) {
            bool containsSelected = false;
            for (std::uint32_t i = 0; i < group.physicalDeviceCount; ++i)
                containsSelected |= group.physicalDevices[i] == physicalDevice_;
            if (!containsSelected || group.physicalDeviceCount < 2)
                continue;

            physicalDeviceGroup_.clear();
            for (std::uint32_t i = 0; i < group.physicalDeviceCount &&
                                      physicalDeviceGroup_.size() < 2; ++i) {
                VkPhysicalDeviceProperties candidate{};
                vkGetPhysicalDeviceProperties(group.physicalDevices[i], &candidate);
                if (candidate.vendorID == selectedProps.vendorID &&
                    candidate.deviceID == selectedProps.deviceID &&
                    IsDeviceSuitable(group.physicalDevices[i])) {
                    physicalDeviceGroup_.push_back(group.physicalDevices[i]);
                }
            }
            if (physicalDeviceGroup_.size() == 2)
                break;
        }
        if (physicalDeviceGroup_.size() != 2)
            throw std::runtime_error(
                "Vulkan AFR requires a driver device group containing two compatible GPUs");
        deviceGroupAfr_ = true;
        afrDeviceCount_ = 2;
#endif
    }
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &props);
    deviceName_ = props.deviceName;

    // Decode driver version — NVIDIA uses a custom encoding, others use Vulkan standard.
    std::uint32_t dv = props.driverVersion;
    if (props.vendorID == 0x10de) {
        driverVersion_ = std::to_string((dv >> 22) & 0x3ff) + "."
                       + std::to_string((dv >> 14) & 0xff) + "."
                       + std::to_string((dv >> 6) & 0xff) + "."
                       + std::to_string(dv & 0x3f);
    } else {
        driverVersion_ = std::to_string(VK_VERSION_MAJOR(dv)) + "."
                       + std::to_string(VK_VERSION_MINOR(dv)) + "."
                       + std::to_string(VK_VERSION_PATCH(dv));
    }

    // Try Vulkan 1.2 driver properties for a more descriptive string (only if device supports 1.2+).
    // Use dynamic lookup to avoid linking against vkGetPhysicalDeviceProperties2,
    // which doesn't exist in Vulkan 1.0 loaders and would cause a startup crash.
    if (props.apiVersion >= VK_API_VERSION_1_2) {
        auto pfnGetProps2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
            vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceProperties2"));
        if (pfnGetProps2) {
            VkPhysicalDeviceDriverProperties driverProps{};
            driverProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
            VkPhysicalDeviceProperties2 props2{};
            props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            props2.pNext = &driverProps;
            pfnGetProps2(physicalDevice_, &props2);
            if (driverProps.driverInfo[0] != '\0')
                driverVersion_ = std::string(driverProps.driverName) + " " + driverProps.driverInfo;
            else if (driverProps.driverName[0] != '\0')
                driverVersion_ = std::string(driverProps.driverName) + " " + driverVersion_;
        }
    }

    std::cout << "Selected GPU [" << chosen << "]: " << deviceName_
              << "  |  Driver: " << driverVersion_ << std::endl;
    if (deviceGroupAfr_) {
        // AMD's 20.45.40.15 Windows ICD completes device-group rendering and
        // resource teardown, then faults inside vkDestroyDevice after a LOCAL
        // device-group swapchain was used.  This exact compatibility path is
        // intentionally narrow; newer drivers retain normal Vulkan teardown.
        deferAfrDeviceDestroyToProcessExit_ =
            props.vendorID == 0x1002 &&
            driverVersion_.find("20.45.40.15") != std::string::npos;
        std::cout << "[Vulkan AFR] Device group accepted: 2 physical GPUs, "
                     "alternate-frame device masks 0x1/0x2\n";
    }
}

void VulkanBackend::CreateLogicalDevice() {
    const auto indices = FindQueueFamilies(physicalDevice_);
    std::uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice_, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice_, &queueFamilyCount, queueFamilies.data());
    const std::uint32_t graphicsQueueCount =
        queueFamilies[indices.graphicsFamily.value()].queueCount;
    std::set<std::uint32_t> unique = {
        indices.graphicsFamily.value(),
        indices.computeFamily.value()
    };
    if (!config_.headless)
        unique.insert(indices.presentFamily.value());

    std::vector<VkDeviceQueueCreateInfo> queueCIs;
    float prio = 1.0f;
    for (auto fam : unique) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = fam;
        qi.queueCount       = 1u;
        qi.pQueuePriorities = &prio;
        queueCIs.push_back(qi);
    }

    VkPhysicalDeviceFeatures supported{};
    vkGetPhysicalDeviceFeatures(physicalDevice_, &supported);
    VkPhysicalDeviceFeatures features{};
    if (!config_.headless && supported.largePoints) features.largePoints = VK_TRUE;
    // SynthPeak FP64 variant needs double support in shaders.
    if (supported.shaderFloat64) features.shaderFloat64 = VK_TRUE;

    // Query FP16 (shaderFloat16) support for the SynthPeak FP16 variant.
#if defined(__ANDROID__)
    // Android NDK libvulkan stubs are 1.0; skip Features2 / FP16 for now.
    const bool fp16Supported = false;
#else
    VkPhysicalDeviceShaderFloat16Int8FeaturesKHR f16query{};
    f16query.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR;
    VkPhysicalDeviceFeatures2 feats2{};
    feats2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    feats2.pNext = &f16query;
    vkGetPhysicalDeviceFeatures2(physicalDevice_, &feats2);
    const bool fp16Supported = (f16query.shaderFloat16 == VK_TRUE);
#endif

    std::vector<const char*> deviceExts;
    if (!config_.headless)
        deviceExts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if (fp16Supported)
        deviceExts.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);

    VkPhysicalDeviceShaderFloat16Int8FeaturesKHR f16enable{};
    f16enable.sType         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR;
    f16enable.shaderFloat16 = VK_TRUE;

    VkDeviceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount    = static_cast<std::uint32_t>(queueCIs.size());
    ci.pQueueCreateInfos       = queueCIs.data();
    ci.pEnabledFeatures        = &features;
    ci.enabledExtensionCount   = static_cast<std::uint32_t>(deviceExts.size());
    ci.ppEnabledExtensionNames = deviceExts.empty() ? nullptr : deviceExts.data();
    VkDeviceGroupDeviceCreateInfo groupCI{};
    if (deviceGroupAfr_) {
        groupCI.sType = VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO;
        groupCI.physicalDeviceCount = afrDeviceCount_;
        groupCI.pPhysicalDevices = physicalDeviceGroup_.data();
        if (fp16Supported)
            groupCI.pNext = &f16enable;
        ci.pNext = &groupCI;
    } else if (fp16Supported) {
        ci.pNext = &f16enable;
    }

    if (vkCreateDevice(physicalDevice_, &ci, nullptr, &device_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateDevice failed");

    vkGetDeviceQueue(device_, indices.graphicsFamily.value(), 0, &graphicsQueue_);
    if (!config_.headless)
        vkGetDeviceQueue(device_, indices.presentFamily.value(),  0, &presentQueue_);
    vkGetDeviceQueue(device_, indices.computeFamily.value(),  0, &computeQueue_);

    if (deviceGroupAfr_) {
        std::cout << "[Vulkan AFR] Graphics queue family exposes "
                  << graphicsQueueCount << " queue(s).\n";
        std::cout << "[Vulkan AFR] Queue families: graphics="
                  << indices.graphicsFamily.value() << ", present="
                  << indices.presentFamily.value()
                  << (indices.presentFamily != indices.graphicsFamily
                          ? " (dedicated present queue)\n"
                          : " (shared graphics/present queue)\n");
        if (graphicsQueueCount < afrDeviceCount_) {
            std::cout << "[Vulkan AFR] Warning: fewer graphics queues than devices; "
                         "device-mask submissions share one VkQueue and this driver "
                         "may serialize them.\n";
        }
#if !defined(__ANDROID__)
        VkDeviceGroupPresentCapabilitiesKHR caps{};
        caps.sType = VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_CAPABILITIES_KHR;
        const VkResult capsResult = vkGetDeviceGroupPresentCapabilitiesKHR(device_, &caps);
        if (capsResult != VK_SUCCESS ||
            (caps.modes & VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR) == 0) {
            throw std::runtime_error(
                "Vulkan device group cannot present local AFR frames on this surface");
        }

        auto peerFlagsText = [](VkPeerMemoryFeatureFlags flags) {
            std::string text;
            if (flags & VK_PEER_MEMORY_FEATURE_COPY_SRC_BIT) text += "copy-src|";
            if (flags & VK_PEER_MEMORY_FEATURE_COPY_DST_BIT) text += "copy-dst|";
            if (flags & VK_PEER_MEMORY_FEATURE_GENERIC_SRC_BIT) text += "generic-src|";
            if (flags & VK_PEER_MEMORY_FEATURE_GENERIC_DST_BIT) text += "generic-dst|";
            if (text.empty()) return std::string("none");
            text.pop_back();
            return text;
        };
        VkPhysicalDeviceMemoryProperties memoryProperties{};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties);
        for (std::uint32_t heap = 0; heap < memoryProperties.memoryHeapCount; ++heap) {
            const VkMemoryHeapFlags flags = memoryProperties.memoryHeaps[heap].flags;
            if ((flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0 ||
                (flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) == 0)
                continue;
            VkPeerMemoryFeatureFlags peer01 = 0;
            VkPeerMemoryFeatureFlags peer10 = 0;
            vkGetDeviceGroupPeerMemoryFeatures(device_, heap, 0, 1, &peer01);
            vkGetDeviceGroupPeerMemoryFeatures(device_, heap, 1, 0, &peer10);
            std::cout << "[Vulkan AFR] Peer heap " << heap
                      << ": GPU0 reads GPU1=" << peerFlagsText(peer01)
                      << ", GPU1 reads GPU0=" << peerFlagsText(peer10) << "\n";
        }
#endif
    }
}

// -----------------------------------------------------------------------
// Buffers
// -----------------------------------------------------------------------

VkShaderModule VulkanBackend::CreateShaderModule(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode    = reinterpret_cast<const std::uint32_t*>(code.data());
    VkShaderModule m = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device_, &ci, nullptr, &m) != VK_SUCCESS)
        throw std::runtime_error("vkCreateShaderModule failed");
    return m;
}

std::uint32_t VulkanBackend::FindMemoryType(std::uint32_t filter, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties mem{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &mem);
    for (std::uint32_t i = 0; i < mem.memoryTypeCount; ++i)
        if ((filter & (1u << i)) && (mem.memoryTypes[i].propertyFlags & props) == props)
            return i;
    throw std::runtime_error("Failed to find suitable memory type");
}

void VulkanBackend::CreateParticleBuffer() {
    const VkDeviceSize size = sizeof(Particle) * config_.particleCount;

    auto createBuffer = [&](VkDeviceSize sz, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags memProps,
                            VkBuffer& buf, VkDeviceMemory& mem) {
        VkBufferCreateInfo bi{};
        bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size        = sz;
        bi.usage       = usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &bi, nullptr, &buf) != VK_SUCCESS)
            throw std::runtime_error("vkCreateBuffer failed");

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device_, buf, &req);

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = req.size;
        ai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, memProps);
        if (vkAllocateMemory(device_, &ai, nullptr, &mem) != VK_SUCCESS)
            throw std::runtime_error("vkAllocateMemory failed");

        vkBindBufferMemory(device_, buf, mem, 0);
    };

    const VkBufferUsageFlags gpuUsage =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    if (config_.hostMemory) {
        createBuffer(size, gpuUsage,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     particleBuffer_, particleBufferMemory_);

        void* data = nullptr;
        vkMapMemory(device_, particleBufferMemory_, 0, size, 0, &data);
        std::memcpy(data, initialParticles_.data(), static_cast<std::size_t>(size));
        vkUnmapMemory(device_, particleBufferMemory_);

        std::cout << "Created particle buffer (host-visible): "
                  << config_.particleCount << " particles\n";
    } else {
        createBuffer(size, gpuUsage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     particleBuffer_, particleBufferMemory_);

        VkBuffer       stagingBuf = VK_NULL_HANDLE;
        VkDeviceMemory stagingMem = VK_NULL_HANDLE;
        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuf, stagingMem);

        void* data = nullptr;
        vkMapMemory(device_, stagingMem, 0, size, 0, &data);
        std::memcpy(data, initialParticles_.data(), static_cast<std::size_t>(size));
        vkUnmapMemory(device_, stagingMem);

        VkCommandBufferAllocateInfo cmdAi{};
        cmdAi.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAi.commandPool        = commandPool_;
        cmdAi.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAi.commandBufferCount = 1;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(device_, &cmdAi, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkBufferCopy region{};
        region.size = size;
        vkCmdCopyBuffer(cmd, stagingBuf, particleBuffer_, 1, &region);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo si{};
        si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers    = &cmd;
        vkQueueSubmit(graphicsQueue_, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue_);

        vkFreeCommandBuffers(device_, commandPool_, 1, &cmd);
        vkDestroyBuffer(device_, stagingBuf, nullptr);
        vkFreeMemory(device_, stagingMem, nullptr);

        std::cout << "Created particle buffer (device-local via staging): "
                  << config_.particleCount << " particles\n";
    }
}

// -----------------------------------------------------------------------
// Swap chain
// -----------------------------------------------------------------------

VkSurfaceFormatKHR VulkanBackend::ChooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& available) const {
    for (auto& f : available)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    return available[0];
}

VkPresentModeKHR VulkanBackend::ChooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& available) const {
    if (!config_.vsync) {
        for (auto m : available)
            if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) return m;
        for (auto m : available)
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    } else {
        for (auto m : available)
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanBackend::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps) const {
    if (caps.currentExtent.width != std::numeric_limits<std::uint32_t>::max())
        return caps.currentExtent;
    int w = 0, h = 0;
#if defined(__ANDROID__)
    if (auto* nativeWindow = static_cast<ANativeWindow*>(window_)) {
        w = ANativeWindow_getWidth(nativeWindow);
        h = ANativeWindow_getHeight(nativeWindow);
    } else {
        w = static_cast<int>(kWindowWidth);
        h = static_cast<int>(kWindowHeight);
    }
#else
    glfwGetFramebufferSize(window_, &w, &h);
#endif
    return {
        std::clamp(static_cast<std::uint32_t>(w), caps.minImageExtent.width,  caps.maxImageExtent.width),
        std::clamp(static_cast<std::uint32_t>(h), caps.minImageExtent.height, caps.maxImageExtent.height)
    };
}

void VulkanBackend::CreateSwapChain() {
    auto sc  = QuerySwapChainSupport(physicalDevice_);
    auto fmt = ChooseSwapSurfaceFormat(sc.formats);
    auto pm  = ChooseSwapPresentMode(sc.presentModes);
    auto ext = ChooseSwapExtent(sc.capabilities);

    // Request at least framesInFlight+1 images so the GPU doesn't stall
    // waiting for the presentation engine to release an image.
    std::uint32_t imgCount = std::max(sc.capabilities.minImageCount + 1,
                                      config_.framesInFlight + 1);
    if (sc.capabilities.maxImageCount > 0 && imgCount > sc.capabilities.maxImageCount)
        imgCount = sc.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = surface_;
    ci.minImageCount    = imgCount;
    ci.imageFormat      = fmt.format;
    ci.imageColorSpace  = fmt.colorSpace;
    ci.imageExtent      = ext;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto idx = FindQueueFamilies(physicalDevice_);
    std::uint32_t qfi[] = { idx.graphicsFamily.value(), idx.presentFamily.value() };
    if (idx.graphicsFamily != idx.presentFamily) {
        ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices   = qfi;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    ci.preTransform   = sc.capabilities.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode    = pm;
    ci.clipped        = VK_TRUE;

    VkDeviceGroupSwapchainCreateInfoKHR groupSwapchainCI{};
    if (deviceGroupAfr_) {
        groupSwapchainCI.sType =
            VK_STRUCTURE_TYPE_DEVICE_GROUP_SWAPCHAIN_CREATE_INFO_KHR;
        groupSwapchainCI.modes = VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR;
        ci.pNext = &groupSwapchainCI;
    }

    if (vkCreateSwapchainKHR(device_, &ci, nullptr, &swapChain_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateSwapchainKHR failed");

    vkGetSwapchainImagesKHR(device_, swapChain_, &imgCount, nullptr);
    swapChainImages_.resize(imgCount);
    vkGetSwapchainImagesKHR(device_, swapChain_, &imgCount, swapChainImages_.data());
    swapChainImageFormat_ = fmt.format;
    swapChainExtent_      = ext;
}

void VulkanBackend::CreateImageViews() {
    swapChainImageViews_.resize(swapChainImages_.size());
    for (std::size_t i = 0; i < swapChainImages_.size(); ++i) {
        VkImageViewCreateInfo ci{};
        ci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image    = swapChainImages_[i];
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format   = swapChainImageFormat_;
        ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        if (vkCreateImageView(device_, &ci, nullptr, &swapChainImageViews_[i]) != VK_SUCCESS)
            throw std::runtime_error("vkCreateImageView failed");
    }
}

void VulkanBackend::CreateRenderPass() {
    VkAttachmentDescription color{};
    color.format         = swapChainImageFormat_;
    color.samples        = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    const bool useDepth = (config_.workload == Workload::Render3D);

    VkAttachmentDescription depth{};
    depth.format         = depthFormat_;
    depth.samples        = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &ref;
    if (useDepth)
        subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                      | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                      | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                      | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription atts[2] = { color, depth };
    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = useDepth ? 2u : 1u; ci.pAttachments = atts;
    ci.subpassCount    = 1; ci.pSubpasses    = &subpass;
    ci.dependencyCount = 1; ci.pDependencies = &dep;

    if (vkCreateRenderPass(device_, &ci, nullptr, &renderPass_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateRenderPass failed");
}

void VulkanBackend::CreateFramebuffers() {
    const bool useDepth = (config_.workload == Workload::Render3D);
    swapChainFramebuffers_.resize(swapChainImageViews_.size());
    for (std::size_t i = 0; i < swapChainImageViews_.size(); ++i) {
        VkImageView att[] = { swapChainImageViews_[i], depthImageView_ };
        VkFramebufferCreateInfo ci{};
        ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass      = renderPass_;
        ci.attachmentCount = useDepth ? 2u : 1u;
        ci.pAttachments    = att;
        ci.width           = swapChainExtent_.width;
        ci.height          = swapChainExtent_.height;
        ci.layers          = 1;
        if (vkCreateFramebuffer(device_, &ci, nullptr, &swapChainFramebuffers_[i]) != VK_SUCCESS)
            throw std::runtime_error("vkCreateFramebuffer failed");
    }
}

// -----------------------------------------------------------------------
// Pipelines
// -----------------------------------------------------------------------

void VulkanBackend::CreateGraphicsPipeline() {
    auto vertCode = ReadFileBytes(shaderDir_ + "particle.vert.spv");
    auto fragCode = ReadFileBytes(shaderDir_ + "particle.frag.spv");
    VkShaderModule vertMod = CreateShaderModule(vertCode);
    VkShaderModule fragMod = CreateShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName  = "main";

    VkVertexInputBindingDescription bind{ 0, sizeof(Particle), VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0] = { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Particle, px) };
    attrs[1] = { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Particle, vx) };

    VkPipelineVertexInputStateCreateInfo vertIn{};
    vertIn.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertIn.vertexBindingDescriptionCount   = 1; vertIn.pVertexBindingDescriptions   = &bind;
    vertIn.vertexAttributeDescriptionCount = 2; vertIn.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkViewport vp{ 0, 0, (float)swapChainExtent_.width, (float)swapChainExtent_.height, 0, 1 };
    VkRect2D sc{ {0,0}, swapChainExtent_ };
    VkPipelineViewportStateCreateInfo vs{};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.viewportCount = 1; vs.pViewports = &vp;
    vs.scissorCount  = 1; vs.pScissors  = &sc;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth   = 1.0f;
    rs.cullMode    = VK_CULL_MODE_NONE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                       | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba.blendEnable         = VK_TRUE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.colorBlendOp        = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cba.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cba;

    VkPipelineLayoutCreateInfo pli{};
    pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (vkCreatePipelineLayout(device_, &pli, nullptr, &graphicsPipelineLayout_) != VK_SUCCESS)
        throw std::runtime_error("vkCreatePipelineLayout failed");

    VkGraphicsPipelineCreateInfo pi{};
    pi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pi.stageCount          = 2;
    pi.pStages             = stages;
    pi.pVertexInputState   = &vertIn;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState      = &vs;
    pi.pRasterizationState = &rs;
    pi.pMultisampleState   = &ms;
    pi.pColorBlendState    = &cb;
    pi.layout              = graphicsPipelineLayout_;
    pi.renderPass          = renderPass_;
    pi.subpass             = 0;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &graphicsPipeline_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateGraphicsPipelines failed");

    vkDestroyShaderModule(device_, fragMod, nullptr);
    vkDestroyShaderModule(device_, vertMod, nullptr);
}

// Fractal / Volumetric stress-test pipeline: fullscreen triangle (no vertex
// input) + heavy fragment shader, driven by a 16-byte fragment push constant.
// Both StressFractal and Volumetric workloads share this exact pipeline shape
// (they differ only in which shader files are loaded and which params struct
// is pushed), so we parameterise the loader and reuse the same builder.
void VulkanBackend::CreateFullscreenPipeline(const char* vertSpv,
                                             const char* fragSpv) {
    auto vertCode = ReadFileBytes(shaderDir_ + vertSpv);
    auto fragCode = ReadFileBytes(shaderDir_ + fragSpv);
    VkShaderModule vertMod = CreateShaderModule(vertCode);
    VkShaderModule fragMod = CreateShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName  = "main";

    // No vertex input — the vertex shader generates positions from the index.
    VkPipelineVertexInputStateCreateInfo vertIn{};
    vertIn.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp{ 0, 0, (float)swapChainExtent_.width, (float)swapChainExtent_.height, 0, 1 };
    VkRect2D sc{ {0,0}, swapChainExtent_ };
    VkPipelineViewportStateCreateInfo vs{};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.viewportCount = 1; vs.pViewports = &vp;
    vs.scissorCount  = 1; vs.pScissors  = &sc;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth   = 1.0f;
    rs.cullMode    = VK_CULL_MODE_NONE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                       | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cba;

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    // Both FractalParams and VolumetricParams are 16 bytes (4 x uint32); the
    // pipeline layout is shared between the two fullscreen-pipeline workloads.
    pcr.size       = 16;

    VkPipelineLayoutCreateInfo pli{};
    pli.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges    = &pcr;
    if (vkCreatePipelineLayout(device_, &pli, nullptr, &graphicsPipelineLayout_) != VK_SUCCESS)
        throw std::runtime_error("vkCreatePipelineLayout (fractal) failed");

    VkGraphicsPipelineCreateInfo pi{};
    pi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pi.stageCount          = 2;
    pi.pStages             = stages;
    pi.pVertexInputState   = &vertIn;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState      = &vs;
    pi.pRasterizationState = &rs;
    pi.pMultisampleState   = &ms;
    pi.pColorBlendState    = &cb;
    pi.layout              = graphicsPipelineLayout_;
    pi.renderPass          = renderPass_;
    pi.subpass             = 0;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &graphicsPipeline_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateGraphicsPipelines (fractal) failed");

    vkDestroyShaderModule(device_, fragMod, nullptr);
    vkDestroyShaderModule(device_, vertMod, nullptr);
}

void VulkanBackend::CreateDepthResources() {
    VkImageCreateInfo ic{};
    ic.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ic.imageType     = VK_IMAGE_TYPE_2D;
    ic.extent        = { swapChainExtent_.width, swapChainExtent_.height, 1 };
    ic.mipLevels     = 1;
    ic.arrayLayers   = 1;
    ic.format        = depthFormat_;
    ic.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ic.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ic.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ic.samples       = VK_SAMPLE_COUNT_1_BIT;
    ic.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(device_, &ic, nullptr, &depthImage_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateImage (depth) failed");

    VkMemoryRequirements mr{};
    vkGetImageMemoryRequirements(device_, depthImage_, &mr);
    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = mr.size;
    ai.memoryTypeIndex = FindMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device_, &ai, nullptr, &depthImageMemory_) != VK_SUCCESS)
        throw std::runtime_error("vkAllocateMemory (depth) failed");
    vkBindImageMemory(device_, depthImage_, depthImageMemory_, 0);

    VkImageViewCreateInfo vi{};
    vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image            = depthImage_;
    vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    vi.format           = depthFormat_;
    vi.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView(device_, &vi, nullptr, &depthImageView_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateImageView (depth) failed");
}

void VulkanBackend::CreateQuadBuffer() {
    // 6 corners (two triangles) spanning [-1,1]^2 — the billboard quad.
    const float quad[12] = { -1,-1,  1,-1,  1,1,   -1,-1,  1,1,  -1,1 };
    const VkDeviceSize size = sizeof(quad);

    VkBufferCreateInfo bi{};
    bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size        = size;
    bi.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bi, nullptr, &quadBuffer_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateBuffer (quad) failed");

    VkMemoryRequirements mr{};
    vkGetBufferMemoryRequirements(device_, quadBuffer_, &mr);
    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = mr.size;
    ai.memoryTypeIndex = FindMemoryType(mr.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device_, &ai, nullptr, &quadBufferMemory_) != VK_SUCCESS)
        throw std::runtime_error("vkAllocateMemory (quad) failed");
    vkBindBufferMemory(device_, quadBuffer_, quadBufferMemory_, 0);

    void* p = nullptr;
    vkMapMemory(device_, quadBufferMemory_, 0, size, 0, &p);
    std::memcpy(p, quad, sizeof(quad));
    vkUnmapMemory(device_, quadBufferMemory_);
}

// True-3D instanced billboard pipeline: quad corners (per-vertex) + particle
// position/velocity (per-instance), depth-tested, MVP via push constants.
void VulkanBackend::CreateRender3DPipeline() {
    auto vertCode = ReadFileBytes(shaderDir_ + "render3d.vert.spv");
    auto fragCode = ReadFileBytes(shaderDir_ + "render3d.frag.spv");
    VkShaderModule vertMod = CreateShaderModule(vertCode);
    VkShaderModule fragMod = CreateShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod; stages[0].pName = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod; stages[1].pName = "main";

    VkVertexInputBindingDescription binds[2]{};
    binds[0] = { 0, sizeof(float) * 2, VK_VERTEX_INPUT_RATE_VERTEX };      // quad corner
    binds[1] = { 1, sizeof(Particle), VK_VERTEX_INPUT_RATE_INSTANCE };    // particle
    VkVertexInputAttributeDescription attrs[3]{};
    attrs[0] = { 0, 0, VK_FORMAT_R32G32_SFLOAT,          0 };
    attrs[1] = { 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Particle, px) };
    attrs[2] = { 2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Particle, vx) };

    VkPipelineVertexInputStateCreateInfo vertIn{};
    vertIn.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertIn.vertexBindingDescriptionCount   = 2; vertIn.pVertexBindingDescriptions   = binds;
    vertIn.vertexAttributeDescriptionCount = 3; vertIn.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp{ 0, 0, (float)swapChainExtent_.width, (float)swapChainExtent_.height, 0, 1 };
    VkRect2D sc{ {0,0}, swapChainExtent_ };
    VkPipelineViewportStateCreateInfo vs{};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.viewportCount = 1; vs.pViewports = &vp;
    vs.scissorCount  = 1; vs.pScissors  = &sc;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth   = 1.0f;
    rs.cullMode    = VK_CULL_MODE_NONE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable  = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp   = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                       | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba.blendEnable         = VK_TRUE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.colorBlendOp        = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cba.alphaBlendOp        = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments    = &cba;

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcr.size       = sizeof(Render3DParams);

    VkPipelineLayoutCreateInfo pli{};
    pli.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges    = &pcr;
    if (vkCreatePipelineLayout(device_, &pli, nullptr, &graphicsPipelineLayout_) != VK_SUCCESS)
        throw std::runtime_error("vkCreatePipelineLayout (render3d) failed");

    VkGraphicsPipelineCreateInfo pi{};
    pi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pi.stageCount          = 2;
    pi.pStages             = stages;
    pi.pVertexInputState   = &vertIn;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState      = &vs;
    pi.pRasterizationState = &rs;
    pi.pMultisampleState   = &ms;
    pi.pDepthStencilState  = &ds;
    pi.pColorBlendState    = &cb;
    pi.layout              = graphicsPipelineLayout_;
    pi.renderPass          = renderPass_;
    pi.subpass             = 0;

    if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &graphicsPipeline_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateGraphicsPipelines (render3d) failed");

    vkDestroyShaderModule(device_, fragMod, nullptr);
    vkDestroyShaderModule(device_, vertMod, nullptr);
}

void VulkanBackend::CreateComputeDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding ssbo{};
    ssbo.binding         = 0;
    ssbo.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    ssbo.descriptorCount = 1;
    ssbo.stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = 1;
    ci.pBindings    = &ssbo;

    if (vkCreateDescriptorSetLayout(device_, &ci, nullptr, &computeDescriptorSetLayout_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateDescriptorSetLayout failed");
}

void VulkanBackend::CreateComputeDescriptorResources() {
    VkDescriptorPoolSize ps{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
    VkDescriptorPoolCreateInfo pi{};
    pi.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.poolSizeCount = 1;  pi.pPoolSizes = &ps;
    pi.maxSets       = 1;
    if (vkCreateDescriptorPool(device_, &pi, nullptr, &descriptorPool_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateDescriptorPool failed");

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = descriptorPool_;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &computeDescriptorSetLayout_;
    if (vkAllocateDescriptorSets(device_, &ai, &computeDescriptorSet_) != VK_SUCCESS)
        throw std::runtime_error("vkAllocateDescriptorSets failed");

    VkDescriptorBufferInfo bi{ particleBuffer_, 0, VK_WHOLE_SIZE };
    VkWriteDescriptorSet wr{};
    wr.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr.dstSet          = computeDescriptorSet_;
    wr.dstBinding      = 0;
    wr.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wr.descriptorCount = 1;
    wr.pBufferInfo     = &bi;
    vkUpdateDescriptorSets(device_, 1, &wr, 0, nullptr);
}

void VulkanBackend::CreateComputePipeline() {
    const bool nbody = (config_.workload == Workload::NBody);
    const bool synth = (config_.workload == Workload::SynthPeak);
    std::string csFile;
    if (synth) {
        csFile = (config_.peakPrecision == Precision::FP64)  ? "synthpeak_fp64.comp.spv"
               : (config_.peakPrecision == Precision::FP16)  ? "synthpeak_fp16.comp.spv"
               : (config_.peakPrecision == Precision::INT32) ? "synthpeak_int32.comp.spv"
               :                                               "synthpeak_fp32.comp.spv";
    } else {
        csFile = nbody ? "nbody.comp.spv" : "compute.comp.spv";
    }
    auto code = ReadFileBytes(shaderDir_ + csFile);
    VkShaderModule mod = CreateShaderModule(code);

    VkPipelineShaderStageCreateInfo si{};
    si.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    si.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    si.module = mod;
    si.pName  = "main";

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.size       = synth ? sizeof(PeakParams)
                   : nbody ? sizeof(NBodyParams)
                   :         sizeof(ComputeParams);

    VkPipelineLayoutCreateInfo li{};
    li.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    li.setLayoutCount         = 1; li.pSetLayouts          = &computeDescriptorSetLayout_;
    li.pushConstantRangeCount = 1; li.pPushConstantRanges  = &pcr;
    if (vkCreatePipelineLayout(device_, &li, nullptr, &computePipelineLayout_) != VK_SUCCESS)
        throw std::runtime_error("vkCreatePipelineLayout failed");

    VkComputePipelineCreateInfo ci{};
    ci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ci.stage  = si;
    ci.layout = computePipelineLayout_;
    if (vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &ci, nullptr, &computePipeline_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateComputePipelines failed");

    vkDestroyShaderModule(device_, mod, nullptr);
}

// -----------------------------------------------------------------------
// Command infrastructure
// -----------------------------------------------------------------------

void VulkanBackend::CreateCommandPool() {
    auto idx = FindQueueFamilies(physicalDevice_);
    VkCommandPoolCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = idx.graphicsFamily.value();
    if (vkCreateCommandPool(device_, &ci, nullptr, &commandPool_) != VK_SUCCESS)
        throw std::runtime_error("vkCreateCommandPool failed");
}

void VulkanBackend::CreateCommandBuffers() {
    commandBuffers_.resize(swapChainFramebuffers_.size());
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = commandPool_;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = static_cast<std::uint32_t>(commandBuffers_.size());
    if (vkAllocateCommandBuffers(device_, &ai, commandBuffers_.data()) != VK_SUCCESS)
        throw std::runtime_error("vkAllocateCommandBuffers failed");
}

void VulkanBackend::CreateSyncObjects() {
    const auto flights = config_.framesInFlight;
    imageAvailableSemaphores_.resize(flights, VK_NULL_HANDLE);
    // A present wait is not covered by the frame submit fence.  Index the
    // render-finished semaphores by swapchain image so a semaphore is reused
    // only after that image has been acquired again (presentation completed).
    renderFinishedSemaphores_.resize(
        config_.headless ? 0u : swapChainImages_.size(), VK_NULL_HANDLE);
    inFlightFences_.resize(flights, VK_NULL_HANDLE);
    timestampSlotReady_.resize(flights, false);

    VkSemaphoreCreateInfo si{}; si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (std::uint32_t i = 0; i < flights; ++i) {
        if (config_.headless) {
            // Headless only needs fences, no semaphores
            if (vkCreateFence(device_, &fi, nullptr, &inFlightFences_[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create fence");
        } else {
            if (vkCreateSemaphore(device_, &si, nullptr, &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
                vkCreateFence(device_, &fi, nullptr, &inFlightFences_[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create sync objects");
        }
    }
    for (auto& semaphore : renderFinishedSemaphores_) {
        if (vkCreateSemaphore(device_, &si, nullptr, &semaphore) != VK_SUCCESS)
            throw std::runtime_error("Failed to create present semaphore");
    }
    if (!config_.headless)
        imagesInFlight_.resize(swapChainImages_.size(), VK_NULL_HANDLE);
}

void VulkanBackend::CreateTimestampQueryPool() {
    auto idx = FindQueueFamilies(physicalDevice_);
    std::uint32_t cnt = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &cnt, nullptr);
    std::vector<VkQueueFamilyProperties> fams(cnt);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &cnt, fams.data());

    if (fams[idx.graphicsFamily.value()].timestampValidBits == 0) {
        std::cout << "[Profiling] Timestamps not supported -- disabled.\n";
        return;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &props);
    timestampPeriodNs_ = props.limits.timestampPeriod;

    VkQueryPoolCreateInfo ci{};
    ci.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    ci.queryType  = VK_QUERY_TYPE_TIMESTAMP;
    ci.queryCount = kTimestampsPerFrame * config_.framesInFlight;
    if (vkCreateQueryPool(device_, &ci, nullptr, &timestampQueryPool_) != VK_SUCCESS) {
        std::cerr << "[Profiling] Failed to create query pool -- disabled.\n";
        return;
    }

    timestampsSupported_ = true;
    std::cout << "[Profiling] Timestamp queries enabled (period = "
              << timestampPeriodNs_ << " ns/tick)\n";
}

void VulkanBackend::CollectTimestampResults(std::uint32_t slot) {
    if (!timestampsSupported_ || slot >= timestampSlotReady_.size() ||
        !timestampSlotReady_[slot]) return;

    std::uint32_t first = slot * kTimestampsPerFrame;
    std::uint64_t ts[kTimestampsPerFrame]{};
    if (vkGetQueryPoolResults(device_, timestampQueryPool_, first, kTimestampsPerFrame,
            sizeof(ts), ts, sizeof(std::uint64_t),
            VK_QUERY_RESULT_64_BIT) != VK_SUCCESS)
        return;

    timestampSlotReady_[slot] = false;

    const double toMs = static_cast<double>(timestampPeriodNs_) / 1'000'000.0;
    AccumulateTiming(
        static_cast<double>(ts[1] - ts[0]) * toMs,
        static_cast<double>(ts[3] - ts[2]) * toMs,
        static_cast<double>(ts[3] - ts[0]) * toMs);
}

// -----------------------------------------------------------------------
// Per-frame recording
// -----------------------------------------------------------------------

void VulkanBackend::RecordCommandBuffer(std::uint32_t imageIndex, float deltaTime,
                                        std::uint32_t deviceMask) {
    VkCommandBuffer cmd = commandBuffers_[imageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkDeviceGroupCommandBufferBeginInfo groupBegin{};
    if (deviceGroupAfr_) {
        groupBegin.sType = VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO;
        groupBegin.deviceMask = deviceMask;
        bi.pNext = &groupBegin;
    }
    if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS)
        throw std::runtime_error("vkBeginCommandBuffer failed");

    const std::uint32_t tsBase = currentFrame_ * kTimestampsPerFrame;
    if (timestampsSupported_)
        vkCmdResetQueryPool(cmd, timestampQueryPool_, tsBase, kTimestampsPerFrame);

    if (timestampsSupported_)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampQueryPool_, tsBase);

    if (config_.workload == Workload::Fluid) {
        // Fluid places its compute-end/render-start/render-end timestamps at
        // the actual pass boundaries inside RecordFluidFrame.
        RecordFluidFrame(cmd, deltaTime, imageIndex, tsBase);
        if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
            throw std::runtime_error("vkEndCommandBuffer failed");
        return;
    }
    if (isCinematicLiquidWorkload(config_.workload)) {
        // Each version owns its complete MLS-MPM/resolve/raymarch sequence.
        // Keeping the v1 recorder untouched protects the old score contract.
        if (config_.workload == Workload::CinematicLiquid) {
            if (cinematicLiquid_.isSph)
                RecordCinematicLiquidSphFrame(cmd, deltaTime, imageIndex, tsBase);
            else
                RecordCinematicLiquidV2Frame(cmd, deltaTime, imageIndex, tsBase);
        } else {
            RecordCinematicLiquidFrame(cmd, deltaTime, imageIndex, tsBase);
        }
        if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
            throw std::runtime_error("vkEndCommandBuffer failed");
        return;
    }

    if (isFragmentOnlyWorkload(config_.workload)) {
        // Fragment-only pass: no compute, no particle barrier.
        if (timestampsSupported_)
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampQueryPool_, tsBase + 1);
    } else {
    // --- Compute pass ---
    BeginDebugLabel(cmd, "Particle Compute", 0.2f, 0.8f, 0.2f);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            computePipelineLayout_, 0, 1, &computeDescriptorSet_, 0, nullptr);
    if (config_.workload == Workload::SynthPeak) {
        PeakParams params{ config_.peakIters, 0.9999f, 0.0001f, 0 };
        vkCmdPushConstants(cmd, computePipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(PeakParams), &params);
    } else if (config_.workload == Workload::NBody) {
        NBodyParams params{ deltaTime, config_.softening, config_.particleCount, 0 };
        vkCmdPushConstants(cmd, computePipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(NBodyParams), &params);
    } else {
        ComputeParams params{ deltaTime, 0.9f };
        vkCmdPushConstants(cmd, computePipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                           sizeof(ComputeParams), &params);
    }
    vkCmdDispatch(cmd, config_.particleCount / kComputeWorkGroupSize, 1, 1);
    EndDebugLabel(cmd);

    if (timestampsSupported_)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool_, tsBase + 1);

    // --- SSBO barrier: compute write → vertex read ---
    BeginDebugLabel(cmd, "SSBO Barrier (Compute -> Vertex)", 0.9f, 0.9f, 0.2f);
    VkBufferMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask       = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer              = particleBuffer_;
    barrier.size                = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        0, 0, nullptr, 1, &barrier, 0, nullptr);
    EndDebugLabel(cmd);
    }

    if (timestampsSupported_)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampQueryPool_, tsBase + 2);

    // --- Render pass ---
    BeginDebugLabel(cmd, "Particle Render", 0.2f, 0.4f, 0.9f);
    const bool render3d = (config_.workload == Workload::Render3D);
    VkClearValue clears[2];
    clears[0].color        = {{0.04f, 0.08f, 0.14f, 1.0f}};
    clears[1].depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo rp{};
    rp.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass        = renderPass_;
    rp.framebuffer       = swapChainFramebuffers_[imageIndex];
    rp.renderArea.extent = swapChainExtent_;
    rp.clearValueCount   = render3d ? 2u : 1u;
    rp.pClearValues      = clears;

    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);
    if (config_.workload == Workload::StressFractal) {
        fractalElapsed_ += deltaTime;
        FractalParams fp{ fractalElapsed_, 1.0f, config_.fractalIter, 0 };
        vkCmdPushConstants(cmd, graphicsPipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(FractalParams), &fp);
        vkCmdDraw(cmd, 3, 1, 0, 0);   // fullscreen triangle, no vertex buffer
    } else if (config_.workload == Workload::GpuStressV1) {
        for (std::uint32_t pass = 0; pass < kGpuStressV1DrawsPerFrame; ++pass) {
            GpuStressV1Params sp{ static_cast<float>(pass), 1.0f,
                                  config_.gpuStressIter, kGpuStressV1ShaderVersion };
            vkCmdPushConstants(cmd, graphicsPipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(GpuStressV1Params), &sp);
            vkCmdDraw(cmd, 3, 1, 0, 0);  // bounded overdraw; deterministic pass salt
        }
    } else if (isGpuBurnWorkload(config_.workload)) {
        fractalElapsed_ += deltaTime;
        for (std::uint32_t pass = 0; pass < gpuBurnDrawsPerFrame(config_.workload); ++pass) {
            GpuBurnParams bp{ fractalElapsed_, static_cast<float>(pass),
                              config_.gpuBurnIter, gpuBurnShaderVersion(config_.workload) };
            vkCmdPushConstants(cmd, graphicsPipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(GpuBurnParams), &bp);
            vkCmdDraw(cmd, 3, 1, 0, 0);
        }
    } else if (config_.workload == Workload::Volumetric) {
        fractalElapsed_ += deltaTime;   // reused as noise-field animation time
        VolumetricParams vp{ fractalElapsed_, 0.05f, config_.volumetricSteps, 0 };
        vkCmdPushConstants(cmd, graphicsPipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(VolumetricParams), &vp);
        vkCmdDraw(cmd, 3, 1, 0, 0);   // fullscreen triangle, no vertex buffer
    } else if (render3d) {
        fractalElapsed_ += deltaTime;   // reused as camera orbit time
        Render3DParams r3{};
        const float aspect = (float)swapChainExtent_.width / (float)swapChainExtent_.height;
        render3dCamera(fractalElapsed_, aspect, /*flipY*/true, /*z01*/true,
                       r3.viewProj, r3.camRight, r3.camUp);
        r3.pointSize = 0.02f;
        vkCmdPushConstants(cmd, graphicsPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(Render3DParams), &r3);
        VkBuffer     bufs[] = { quadBuffer_, particleBuffer_ };
        VkDeviceSize offs[] = { 0, 0 };
        vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offs);
        vkCmdDraw(cmd, 6, config_.particleCount, 0, 0);  // 6 verts x N instances
    } else {
        VkBuffer     bufs[] = { particleBuffer_ };
        VkDeviceSize offs[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, bufs, offs);
        vkCmdDraw(cmd, config_.particleCount, 1, 0, 0);
    }
    vkCmdEndRenderPass(cmd);
    EndDebugLabel(cmd);

    if (timestampsSupported_)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, timestampQueryPool_, tsBase + 3);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
        throw std::runtime_error("vkEndCommandBuffer failed");
}

// -----------------------------------------------------------------------
// Frame
// -----------------------------------------------------------------------

void VulkanBackend::DrawFrame(float deltaTime) {
    vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);
    CollectTimestampResults(currentFrame_);

    if (config_.headless) {
        // --- Headless: compute-only path ---
        vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);

        // Record a compute-only command buffer
        if (headlessCmdBuffers_.empty()) {
            headlessCmdBuffers_.resize(config_.framesInFlight);
            VkCommandBufferAllocateInfo ai{};
            ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            ai.commandPool        = commandPool_;
            ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            ai.commandBufferCount = config_.framesInFlight;
            if (vkAllocateCommandBuffers(device_, &ai, headlessCmdBuffers_.data()) != VK_SUCCESS)
                throw std::runtime_error("vkAllocateCommandBuffers (headless) failed");
        }

        VkCommandBuffer cmd = headlessCmdBuffers_[currentFrame_];
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &bi);

        const std::uint32_t tsBase = currentFrame_ * kTimestampsPerFrame;
        if (timestampsSupported_)
            vkCmdResetQueryPool(cmd, timestampQueryPool_, tsBase, kTimestampsPerFrame);
        if (timestampsSupported_)
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampQueryPool_, tsBase);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                computePipelineLayout_, 0, 1, &computeDescriptorSet_, 0, nullptr);
        if (config_.workload == Workload::SynthPeak) {
            PeakParams params{ config_.peakIters, 0.9999f, 0.0001f, 0 };
            vkCmdPushConstants(cmd, computePipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               sizeof(PeakParams), &params);
        } else if (config_.workload == Workload::NBody) {
            NBodyParams params{ deltaTime, config_.softening, config_.particleCount, 0 };
            vkCmdPushConstants(cmd, computePipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               sizeof(NBodyParams), &params);
        } else {
            ComputeParams params{ deltaTime, 0.9f };
            vkCmdPushConstants(cmd, computePipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               sizeof(ComputeParams), &params);
        }
        vkCmdDispatch(cmd, config_.particleCount / kComputeWorkGroupSize, 1, 1);

        if (timestampsSupported_)
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool_, tsBase + 1);

        // For headless, timestamps 2 and 3 mirror 0 and 1 (no render)
        if (timestampsSupported_) {
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool_, tsBase + 2);
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool_, tsBase + 3);
        }

        vkEndCommandBuffer(cmd);

        VkSubmitInfo si{};
        si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers    = &cmd;

        VkResult res = vkQueueSubmit(computeQueue_, 1, &si, inFlightFences_[currentFrame_]);
        if (res != VK_SUCCESS)
            throw std::runtime_error("vkQueueSubmit (headless) failed");

        timestampSlotReady_[currentFrame_] = timestampsSupported_;

        currentFrame_ = (currentFrame_ + 1) % config_.framesInFlight;
        return;
    }

    // --- Normal (windowed) path ---
    std::uint32_t imageIndex = 0;
    const std::uint32_t afrDeviceIndex = deviceGroupAfr_
        ? (currentFrame_ % afrDeviceCount_) : 0u;
    const std::uint32_t afrDeviceMask = 1u << afrDeviceIndex;
    VkResult res = VK_SUCCESS;
#if defined(__ANDROID__)
    res = vkAcquireNextImageKHR(device_, swapChain_, UINT64_MAX,
                                imageAvailableSemaphores_[currentFrame_],
                                VK_NULL_HANDLE, &imageIndex);
#else
    if (deviceGroupAfr_) {
        VkAcquireNextImageInfoKHR acquire{};
        acquire.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR;
        acquire.swapchain = swapChain_;
        acquire.timeout = UINT64_MAX;
        acquire.semaphore = imageAvailableSemaphores_[currentFrame_];
        acquire.deviceMask = afrDeviceMask;
        res = vkAcquireNextImage2KHR(device_, &acquire, &imageIndex);
    } else {
        res = vkAcquireNextImageKHR(device_, swapChain_, UINT64_MAX,
                                    imageAvailableSemaphores_[currentFrame_],
                                    VK_NULL_HANDLE, &imageIndex);
    }
#endif
    if (res == VK_ERROR_OUT_OF_DATE_KHR)
        throw std::runtime_error("Vulkan swapchain became out of date; restart the fixed-resolution benchmark after resizing");
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("vkAcquireNextImageKHR failed");

    if (imagesInFlight_[imageIndex] != VK_NULL_HANDLE)
        vkWaitForFences(device_, 1, &imagesInFlight_[imageIndex], VK_TRUE, UINT64_MAX);
    imagesInFlight_[imageIndex] = inFlightFences_[currentFrame_];

    vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);
    RecordCommandBuffer(imageIndex, deltaTime, afrDeviceMask);

    VkSemaphore          waitSem[]   = { imageAvailableSemaphores_[currentFrame_] };
    VkPipelineStageFlags waitStage[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore          sigSem[]    = { renderFinishedSemaphores_[imageIndex] };

    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1; si.pWaitSemaphores   = waitSem;
    si.pWaitDstStageMask    = waitStage;
    si.commandBufferCount   = 1; si.pCommandBuffers   = &commandBuffers_[imageIndex];
    si.signalSemaphoreCount = 1; si.pSignalSemaphores = sigSem;

    VkDeviceGroupSubmitInfo groupSubmit{};
    if (deviceGroupAfr_) {
        groupSubmit.sType = VK_STRUCTURE_TYPE_DEVICE_GROUP_SUBMIT_INFO;
        groupSubmit.waitSemaphoreCount = 1;
        groupSubmit.pWaitSemaphoreDeviceIndices = &afrDeviceIndex;
        groupSubmit.commandBufferCount = 1;
        groupSubmit.pCommandBufferDeviceMasks = &afrDeviceMask;
        groupSubmit.signalSemaphoreCount = 1;
        groupSubmit.pSignalSemaphoreDeviceIndices = &afrDeviceIndex;
        si.pNext = &groupSubmit;
    }

    res = vkQueueSubmit(graphicsQueue_, 1, &si, inFlightFences_[currentFrame_]);
    if (res != VK_SUCCESS) {
        std::string msg = "vkQueueSubmit failed (VkResult " + std::to_string(static_cast<int>(res)) + ")";
        if (res == VK_ERROR_DEVICE_LOST)
            msg += " -- GPU device lost; try restarting the application or rebooting";
        throw std::runtime_error(msg);
    }
    timestampSlotReady_[currentFrame_] = timestampsSupported_;

    VkSwapchainKHR chains[] = { swapChain_ };
    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = sigSem;
    pi.swapchainCount     = 1; pi.pSwapchains     = chains;
    pi.pImageIndices      = &imageIndex;
    VkDeviceGroupPresentInfoKHR groupPresent{};
    if (deviceGroupAfr_) {
        groupPresent.sType = VK_STRUCTURE_TYPE_DEVICE_GROUP_PRESENT_INFO_KHR;
        groupPresent.swapchainCount = 1;
        groupPresent.pDeviceMasks = &afrDeviceMask;
        groupPresent.mode = VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR;
        pi.pNext = &groupPresent;
    }
    const VkResult presentResult = vkQueuePresentKHR(presentQueue_, &pi);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
        throw std::runtime_error("Vulkan swapchain became out of date; restart the fixed-resolution benchmark after resizing");
    if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("vkQueuePresentKHR failed (VkResult " +
                                 std::to_string(static_cast<int>(presentResult)) + ")");

    currentFrame_ = (currentFrame_ + 1) % config_.framesInFlight;
}

// -----------------------------------------------------------------------
// Cleanup
// -----------------------------------------------------------------------

void VulkanBackend::CleanupSwapChain() {
    for (auto fb : swapChainFramebuffers_) vkDestroyFramebuffer(device_, fb, nullptr);
    swapChainFramebuffers_.clear();
    for (auto iv : swapChainImageViews_)   vkDestroyImageView(device_, iv, nullptr);
    swapChainImageViews_.clear();
    if (depthImageView_)   { vkDestroyImageView(device_, depthImageView_, nullptr); depthImageView_ = VK_NULL_HANDLE; }
    if (depthImage_)       { vkDestroyImage(device_, depthImage_, nullptr); depthImage_ = VK_NULL_HANDLE; }
    if (depthImageMemory_) { vkFreeMemory(device_, depthImageMemory_, nullptr); depthImageMemory_ = VK_NULL_HANDLE; }
    imagesInFlight_.clear();
    if (renderPass_ != VK_NULL_HANDLE) { vkDestroyRenderPass(device_, renderPass_, nullptr); renderPass_ = VK_NULL_HANDLE; }
    if (swapChain_  != VK_NULL_HANDLE) { vkDestroySwapchainKHR(device_, swapChain_, nullptr); swapChain_ = VK_NULL_HANDLE; }
}

void VulkanBackend::CleanupBackend() {
    if (deviceGroupAfr_) std::cout << "[Vulkan AFR] Cleanup: sync/profiling\n";
    for (auto sem : imageAvailableSemaphores_)
        if (sem) vkDestroySemaphore(device_, sem, nullptr);
    for (auto sem : renderFinishedSemaphores_)
        if (sem) vkDestroySemaphore(device_, sem, nullptr);
    for (auto fen : inFlightFences_)
        if (fen) vkDestroyFence(device_, fen, nullptr);
    if (timestampQueryPool_)        { vkDestroyQueryPool(device_, timestampQueryPool_, nullptr); }
    // Stateful workload cleanup must run BEFORE the generic pipeline destroy
    // because their render pipelines are aliased into the generic slots.
    CleanupCinematicLiquidResources();
    CleanupFluidResources();
    if (deviceGroupAfr_) std::cout << "[Vulkan AFR] Cleanup: commands/resources\n";
    if (commandPool_)               { vkDestroyCommandPool(device_, commandPool_, nullptr); commandPool_ = VK_NULL_HANDLE; }
    if (particleBuffer_)            { vkDestroyBuffer(device_, particleBuffer_, nullptr); }
    if (particleBufferMemory_)      { vkFreeMemory(device_, particleBufferMemory_, nullptr); }
    if (quadBuffer_)                { vkDestroyBuffer(device_, quadBuffer_, nullptr); }
    if (quadBufferMemory_)          { vkFreeMemory(device_, quadBufferMemory_, nullptr); }
    if (computePipeline_)           { vkDestroyPipeline(device_, computePipeline_, nullptr); }
    if (computePipelineLayout_)     { vkDestroyPipelineLayout(device_, computePipelineLayout_, nullptr); }
    if (descriptorPool_)            { vkDestroyDescriptorPool(device_, descriptorPool_, nullptr); }
    if (computeDescriptorSetLayout_){ vkDestroyDescriptorSetLayout(device_, computeDescriptorSetLayout_, nullptr); }
    if (graphicsPipeline_)          { vkDestroyPipeline(device_, graphicsPipeline_, nullptr); }
    if (graphicsPipelineLayout_)    { vkDestroyPipelineLayout(device_, graphicsPipelineLayout_, nullptr); }
    if (deviceGroupAfr_) std::cout << "[Vulkan AFR] Cleanup: swapchain\n";
    CleanupSwapChain();
    if (deviceGroupAfr_) std::cout << "[Vulkan AFR] Cleanup: logical device\n";
    if (deferAfrDeviceDestroyToProcessExit_) {
        std::cout
            << "[Vulkan AFR] AMD 20.45.40.15 teardown workaround: all child "
               "resources destroyed; deferring device/instance release to process exit.\n";
        device_ = VK_NULL_HANDLE;
        surface_ = VK_NULL_HANDLE;
        instance_ = VK_NULL_HANDLE;
        return;
    }
    if (device_)   vkDestroyDevice(device_, nullptr);
    if (deviceGroupAfr_) std::cout << "[Vulkan AFR] Cleanup: instance\n";
    if (surface_)  vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (instance_) vkDestroyInstance(instance_, nullptr);
}

// -----------------------------------------------------------------------
// Fluid (Stam 2D Eulerian) — isolated resource set + per-pass orchestration.
// -----------------------------------------------------------------------

namespace {
// Helper: create a device-local buffer + backing memory of the given size and
// usage. Returns the buffer/handle via out-params; caller owns lifetime.
void CreateFluidBuffer(VkDevice device, VkPhysicalDeviceMemoryProperties memProps,
                       VkDeviceSize size, VkBufferUsageFlags usage,
                       VkBuffer* outBuf, VkDeviceMemory* outMem) {
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size  = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &bci, nullptr, outBuf) != VK_SUCCESS)
        throw std::runtime_error("vkCreateBuffer (fluid) failed");

    VkMemoryRequirements mr{};
    vkGetBufferMemoryRequirements(device, *outBuf, &mr);

    // Find a memory type matching the requirements in either device-local or
    // host-visible, preferring device-local.
    std::uint32_t typeIndex = ~0u;
    for (std::uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((mr.memoryTypeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            typeIndex = i; break;
        }
    }
    if (typeIndex == ~0u) {
        for (std::uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((mr.memoryTypeBits & (1u << i)) &&
                (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
                typeIndex = i; break;
            }
        }
    }
    if (typeIndex == ~0u)
        throw std::runtime_error("No suitable memory type for fluid buffer");

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = mr.size;
    mai.memoryTypeIndex = typeIndex;
    if (vkAllocateMemory(device, &mai, nullptr, outMem) != VK_SUCCESS)
        throw std::runtime_error("vkAllocateMemory (fluid) failed");
    if (vkBindBufferMemory(device, *outBuf, *outMem, 0) != VK_SUCCESS)
        throw std::runtime_error("vkBindBufferMemory (fluid) failed");
}
} // namespace

void VulkanBackend::CreateFluidResources() {
    fluid_.gridSize = config_.fluidGridSize;
    const std::uint32_t N = fluid_.gridSize;
    if (N < 64 || N > 512)
        throw std::invalid_argument("Fluid --grid must be between 64 and 512");
    if (config_.fluidJacobiIters < 1 || config_.fluidJacobiIters > 64)
        throw std::invalid_argument("Fluid --jacobi must be between 1 and 64");
    const VkDeviceSize stateSize = sizeof(float) * 4 * N * N;   // vec4 per cell
    const VkDeviceSize pressSize = sizeof(float) * N * N;

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);

    const VkBufferUsageFlags computeUsage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    CreateFluidBuffer(device_, memProps, stateSize, computeUsage, &fluid_.stateA, &fluid_.stateAMem);
    CreateFluidBuffer(device_, memProps, stateSize, computeUsage, &fluid_.stateB, &fluid_.stateBMem);
    CreateFluidBuffer(device_, memProps, pressSize, computeUsage, &fluid_.pressA, &fluid_.pressAMem);
    CreateFluidBuffer(device_, memProps, pressSize, computeUsage, &fluid_.pressB, &fluid_.pressBMem);
    CreateFluidBuffer(device_, memProps, pressSize, computeUsage, &fluid_.divBuf, &fluid_.divMem);

    // ---- Seed the canonical state with two pigment wisps and a restrained
    // rotational field. The advect pass continuously replenishes both sources.
    {
        std::vector<float> init(static_cast<size_t>(stateSize / sizeof(float)));
        for (std::uint32_t y = 0; y < N; ++y)
            for (std::uint32_t x = 0; x < N; ++x) {
                const size_t i = (size_t(y) * N + x) * 4;
                const float fx = (float(x) + 0.5f) / float(N) - 0.5f;
                const float fy = (float(y) + 0.5f) / float(N) - 0.5f;
                const float envelope = std::exp(-(fx * fx + fy * fy) * 7.0f);
                init[i + 0] = -fy * 0.32f * envelope;
                init[i + 1] =  fx * 0.32f * envelope;
                const float ax = fx + 0.25f, ay = fy - 0.04f;
                const float bx = fx - 0.25f, by = fy + 0.04f;
                init[i + 2] = std::exp(-(ax * ax + ay * ay) * 120.0f);
                init[i + 3] = std::exp(-(bx * bx + by * by) * 120.0f);
            }
        // Upload via a staging buffer + a one-shot command buffer allocated
        // from commandPool_ (commandBuffers_[] doesn't exist yet — it is
        // created later in InitBackend).
        VkBuffer       staging = VK_NULL_HANDLE;
        VkDeviceMemory stagingMem = VK_NULL_HANDLE;
        VkBufferCreateInfo sbci{};
        sbci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        sbci.size  = stateSize;
        sbci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        sbci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device_, &sbci, nullptr, &staging);
        VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(device_, staging, &mr);
        std::uint32_t ht = ~0u;
        for (std::uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
            if ((mr.memoryTypeBits & (1u << i)) &&
                ((memProps.memoryTypes[i].propertyFlags &
                  (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                 (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
                ht = i; break;
            }
        if (ht == ~0u)
            throw std::runtime_error("No host-visible coherent memory for fluid staging");
        VkMemoryAllocateInfo mai{};
        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = mr.size;
        mai.memoryTypeIndex = ht;
        if (vkAllocateMemory(device_, &mai, nullptr, &stagingMem) != VK_SUCCESS)
            throw std::runtime_error("vkAllocateMemory (fluid staging) failed");
        if (vkBindBufferMemory(device_, staging, stagingMem, 0) != VK_SUCCESS)
            throw std::runtime_error("vkBindBufferMemory (fluid staging) failed");
        void* mapped = nullptr;
        if (vkMapMemory(device_, stagingMem, 0, stateSize, 0, &mapped) != VK_SUCCESS)
            throw std::runtime_error("vkMapMemory (fluid staging) failed");
        std::memcpy(mapped, init.data(), init.size() * sizeof(float));
        vkUnmapMemory(device_, stagingMem);

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo aci{};
        aci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        aci.commandPool = commandPool_;
        aci.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        aci.commandBufferCount = 1;
        vkAllocateCommandBuffers(device_, &aci, &cmd);
        VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &bi);
        VkBufferCopy cp{ 0, 0, stateSize };
        vkCmdCopyBuffer(cmd, staging, fluid_.stateA, 1, &cp);
        vkCmdFillBuffer(cmd, fluid_.pressA, 0, VK_WHOLE_SIZE, 0);
        vkCmdFillBuffer(cmd, fluid_.pressB, 0, VK_WHOLE_SIZE, 0);
        vkCmdFillBuffer(cmd, fluid_.divBuf, 0, VK_WHOLE_SIZE, 0);
        vkEndCommandBuffer(cmd);
        VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
        vkQueueSubmit(graphicsQueue_, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue_);
        vkFreeCommandBuffers(device_, commandPool_, 1, &cmd);
        vkDestroyBuffer(device_, staging, nullptr);
        vkFreeMemory(device_, stagingMem, nullptr);
    }

    // ---- Compute descriptor set layout: 5 bindings (inCells, outCells,
    // inP, outP, div) — every pass uses the subset it needs.
    {
        VkDescriptorSetLayoutBinding b[5]{};
        const VkShaderStageFlags cs = VK_SHADER_STAGE_COMPUTE_BIT;
        for (int i = 0; i < 5; ++i) {
            b[i].binding = i;
            b[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            b[i].descriptorCount = 1;
            b[i].stageFlags = cs;
        }
        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = 5; ci.pBindings = b;
        if (vkCreateDescriptorSetLayout(device_, &ci, nullptr, &fluid_.computeSetLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDescriptorSetLayout (fluid compute) failed");
    }

    // ---- Allocate descriptor pool + sets (advect/div/jacA/jacB/sub).
    {
        VkDescriptorPoolSize ps{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5 * 5 };  // 5 sets x 5 bindings
        VkDescriptorPoolCreateInfo pi{};
        pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pi.poolSizeCount = 1; pi.pPoolSizes = &ps;
        pi.maxSets = 5;
        vkCreateDescriptorPool(device_, &pi, nullptr, &fluid_.computePool);

        VkDescriptorSetLayout layouts[5] = {
            fluid_.computeSetLayout, fluid_.computeSetLayout, fluid_.computeSetLayout,
            fluid_.computeSetLayout, fluid_.computeSetLayout
        };
        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = fluid_.computePool;
        ai.descriptorSetCount = 5;
        ai.pSetLayouts = layouts;
        VkDescriptorSet sets[5];
        vkAllocateDescriptorSets(device_, &ai, sets);
        fluid_.setAdvect = sets[0];
        fluid_.setDiv    = sets[1];
        fluid_.setJacA   = sets[2];
        fluid_.setJacB   = sets[3];
        fluid_.setSub    = sets[4];

        auto writeSet = [&](VkDescriptorSet s, std::uint32_t binding, VkBuffer buf) {
            VkDescriptorBufferInfo bi{ buf, 0, VK_WHOLE_SIZE };
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = s; w.dstBinding = binding;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            w.descriptorCount = 1; w.pBufferInfo = &bi;
            vkUpdateDescriptorSets(device_, 1, &w, 0, nullptr);
        };
        // Advect: in=A (0), out=B (1)
        writeSet(fluid_.setAdvect, 0, fluid_.stateA);
        writeSet(fluid_.setAdvect, 1, fluid_.stateB);
        // Divergence: in=B (0), out=div (4)
        writeSet(fluid_.setDiv, 0, fluid_.stateB);
        writeSet(fluid_.setDiv, 4, fluid_.divBuf);
        // Jacobi A: in=pressA (2), out=pressB (3), div (4)
        writeSet(fluid_.setJacA, 2, fluid_.pressA);
        writeSet(fluid_.setJacA, 3, fluid_.pressB);
        writeSet(fluid_.setJacA, 4, fluid_.divBuf);
        // Jacobi B: in=pressB (2), out=pressA (3), div (4)
        writeSet(fluid_.setJacB, 2, fluid_.pressB);
        writeSet(fluid_.setJacB, 3, fluid_.pressA);
        writeSet(fluid_.setJacB, 4, fluid_.divBuf);
        // Iteration zero writes B, so odd counts finish in B and even counts
        // finish in A. This choice is immutable for the lifetime of the run.
        VkBuffer finalPressure = (config_.fluidJacobiIters & 1u)
                               ? fluid_.pressB : fluid_.pressA;
        // Subtract: in=B (0), out=A (1), inP=final pressure (2)
        writeSet(fluid_.setSub, 0, fluid_.stateB);
        writeSet(fluid_.setSub, 1, fluid_.stateA);
        writeSet(fluid_.setSub, 2, finalPressure);
    }

    // ---- Compute pipeline layout (shared across the 4 passes; push constants
    // carry FluidParams).
    {
        VkPushConstantRange pcr{};
        pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pcr.size = sizeof(FluidParams);
        VkPipelineLayoutCreateInfo li{};
        li.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        li.setLayoutCount = 1; li.pSetLayouts = &fluid_.computeSetLayout;
        li.pushConstantRangeCount = 1; li.pPushConstantRanges = &pcr;
        vkCreatePipelineLayout(device_, &li, nullptr, &fluid_.computeLayout);
    }

    // ---- 4 compute pipelines (one per pass).
    auto createComputePipe = [&](const char* spvName, VkPipeline* pipeOut) {
        auto code = ReadFileBytes(shaderDir_ + spvName);
        VkShaderModule mod = CreateShaderModule(code);
        VkPipelineShaderStageCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        si.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        si.module = mod; si.pName = "main";
        VkComputePipelineCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        ci.stage = si; ci.layout = fluid_.computeLayout;
        VkResult rc = vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &ci, nullptr, pipeOut);
        vkDestroyShaderModule(device_, mod, nullptr);
        if (rc != VK_SUCCESS)
            throw std::runtime_error(std::string("vkCreateComputePipelines (") + spvName + ") failed");
    };
    createComputePipe("fluid_advect.comp.spv",    &fluid_.advectPipe);
    createComputePipe("fluid_divergence.comp.spv", &fluid_.divPipe);
    createComputePipe("fluid_jacobi.comp.spv",    &fluid_.jacobiPipe);
    createComputePipe("fluid_subtract.comp.spv",  &fluid_.subtractPipe);

    // ---- Render pipeline: fullscreen tri + fragment reads the dye SSBO.
    if (!config_.headless) {
        VkDescriptorSetLayoutBinding rb{};
        rb.binding = 0;
        rb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        rb.descriptorCount = 1;
        rb.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo rci{};
        rci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        rci.bindingCount = 1; rci.pBindings = &rb;
        vkCreateDescriptorSetLayout(device_, &rci, nullptr, &fluid_.renderSetLayout);

        // Render descriptor pool — separate from the compute pool so we don't
        // overwrite its handle (which would leak the compute pool).
        VkDescriptorPoolSize rps{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
        VkDescriptorPoolCreateInfo rpi{};
        rpi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        rpi.poolSizeCount = 1; rpi.pPoolSizes = &rps;
        rpi.maxSets = 1;
        vkCreateDescriptorPool(device_, &rpi, nullptr, &fluid_.renderPool);

        VkDescriptorSetAllocateInfo rai{};
        rai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        rai.descriptorPool = fluid_.renderPool;
        rai.descriptorSetCount = 1;
        rai.pSetLayouts = &fluid_.renderSetLayout;
        vkAllocateDescriptorSets(device_, &rai, &fluid_.setRender);
        VkDescriptorBufferInfo bi{ fluid_.stateA, 0, VK_WHOLE_SIZE };
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = fluid_.setRender; w.dstBinding = 0;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.descriptorCount = 1; w.pBufferInfo = &bi;
        vkUpdateDescriptorSets(device_, 1, &w, 0, nullptr);

        // Render pipeline layout: render descriptor set + 16-byte fragment/vertex push constant.
        VkPushConstantRange rpcr{};
        rpcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        rpcr.size = sizeof(FluidRenderParams);
        VkPipelineLayoutCreateInfo rli{};
        rli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        rli.setLayoutCount = 1; rli.pSetLayouts = &fluid_.renderSetLayout;
        rli.pushConstantRangeCount = 1; rli.pPushConstantRanges = &rpcr;
        vkCreatePipelineLayout(device_, &rli, nullptr, &fluid_.renderLayout);

        auto vcode = ReadFileBytes(shaderDir_ + "fluid_render.vert.spv");
        auto fcode = ReadFileBytes(shaderDir_ + "fluid_render.frag.spv");
        VkShaderModule vmod = CreateShaderModule(vcode);
        VkShaderModule fmod = CreateShaderModule(fcode);

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;   stages[0].module = vmod; stages[0].pName = "main";
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fmod; stages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo ia{};
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport vp{ 0, 0, (float)swapChainExtent_.width, (float)swapChainExtent_.height, 0, 1 };
        VkRect2D sc{ {0,0}, swapChainExtent_ };
        VkPipelineViewportStateCreateInfo vs{};
        vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vs.viewportCount = 1; vs.pViewports = &vp; vs.scissorCount = 1; vs.pScissors = &sc;
        VkPipelineRasterizationStateCreateInfo rs{};
        rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.polygonMode = VK_POLYGON_MODE_FILL; rs.lineWidth = 1.0f; rs.cullMode = VK_CULL_MODE_NONE;
        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState cba{};
        cba.colorWriteMask = 0xF;
        cba.blendEnable = VK_FALSE;
        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 1; cb.pAttachments = &cba;

        VkGraphicsPipelineCreateInfo pi{};
        pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pi.stageCount = 2; pi.pStages = stages;
        pi.pVertexInputState = &vi; pi.pInputAssemblyState = &ia;
        pi.pViewportState = &vs; pi.pRasterizationState = &rs;
        pi.pMultisampleState = &ms; pi.pColorBlendState = &cb;
        pi.layout = fluid_.renderLayout; pi.renderPass = renderPass_; pi.subpass = 0;
        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &fluid_.renderPipe) != VK_SUCCESS)
            throw std::runtime_error("vkCreateGraphicsPipelines (fluid render) failed");
        vkDestroyShaderModule(device_, vmod, nullptr);
        vkDestroyShaderModule(device_, fmod, nullptr);

        // Use the fluid render pipeline as the "graphics" pipeline for the
        // command-buffer recording path's bind point.
        graphicsPipeline_ = fluid_.renderPipe;
        graphicsPipelineLayout_ = fluid_.renderLayout;
    }
}

void VulkanBackend::CleanupFluidResources() {
    if (device_ == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(device_);
    // The fluid render pipeline was assigned to graphicsPipeline_ so the
    // shared command-recording path binds it; clear that alias before the
    // fluid-specific destroy below to avoid a double-free.
    if (graphicsPipeline_ == fluid_.renderPipe)    graphicsPipeline_ = VK_NULL_HANDLE;
    if (graphicsPipelineLayout_ == fluid_.renderLayout) graphicsPipelineLayout_ = VK_NULL_HANDLE;
    auto destroyPipe = [](VkDevice d, VkPipeline& p) { if (p) { vkDestroyPipeline(d, p, nullptr); p = VK_NULL_HANDLE; } };
    destroyPipe(device_, fluid_.advectPipe);
    destroyPipe(device_, fluid_.divPipe);
    destroyPipe(device_, fluid_.jacobiPipe);
    destroyPipe(device_, fluid_.subtractPipe);
    destroyPipe(device_, fluid_.renderPipe);
    if (fluid_.computeLayout) { vkDestroyPipelineLayout(device_, fluid_.computeLayout, nullptr); fluid_.computeLayout = VK_NULL_HANDLE; }
    if (fluid_.renderLayout)  { vkDestroyPipelineLayout(device_, fluid_.renderLayout,  nullptr); fluid_.renderLayout  = VK_NULL_HANDLE; }
    if (fluid_.computePool)   { vkDestroyDescriptorPool(device_, fluid_.computePool,   nullptr); fluid_.computePool   = VK_NULL_HANDLE; }
    if (fluid_.renderPool)    { vkDestroyDescriptorPool(device_, fluid_.renderPool,    nullptr); fluid_.renderPool    = VK_NULL_HANDLE; }
    if (fluid_.computeSetLayout) { vkDestroyDescriptorSetLayout(device_, fluid_.computeSetLayout, nullptr); fluid_.computeSetLayout = VK_NULL_HANDLE; }
    if (fluid_.renderSetLayout)  { vkDestroyDescriptorSetLayout(device_, fluid_.renderSetLayout,  nullptr); fluid_.renderSetLayout  = VK_NULL_HANDLE; }

    auto destroyBuf = [](VkDevice d, VkBuffer& b, VkDeviceMemory& m) {
        if (b) { vkDestroyBuffer(d, b, nullptr); b = VK_NULL_HANDLE; }
        if (m) { vkFreeMemory(d, m, nullptr); m = VK_NULL_HANDLE; }
    };
    destroyBuf(device_, fluid_.stateA, fluid_.stateAMem);
    destroyBuf(device_, fluid_.stateB, fluid_.stateBMem);
    destroyBuf(device_, fluid_.pressA, fluid_.pressAMem);
    destroyBuf(device_, fluid_.pressB, fluid_.pressBMem);
    destroyBuf(device_, fluid_.divBuf, fluid_.divMem);
}

void VulkanBackend::RecordFluidFrame(VkCommandBuffer cmd, float deltaTime,
                                     std::uint32_t imageIndex, std::uint32_t timestampBase) {
    const std::uint32_t N = fluid_.gridSize;
    const float stepDt = std::clamp(deltaTime, 0.0f, 1.0f / 30.0f);
    fluid_.simTime += std::clamp(deltaTime, 0.0f, 0.1f);
    const FluidParams fp{ stepDt, 1.0f / float(N), N, fluid_.simTime };
    const std::uint32_t groups = (N + 15u) / 16u;

    auto barrier = [&](VkBuffer b, VkPipelineStageFlags dstStages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
        VkBufferMemoryBarrier bm{};
        bm.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bm.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        bm.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        bm.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bm.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bm.buffer = b; bm.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                  dstStages,
                                  0, 0, nullptr, 1, &bm, 0, nullptr);
    };

    // Pass 1: advect  (in=stateA, out=stateB)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, fluid_.advectPipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, fluid_.computeLayout, 0, 1, &fluid_.setAdvect, 0, nullptr);
    vkCmdPushConstants(cmd, fluid_.computeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(fp), &fp);
    vkCmdDispatch(cmd, groups, groups, 1);
    barrier(fluid_.stateB);

    // stateA is canonical at frame entry; advect always writes the complete
    // stateB scratch field. No descriptor mutation or frame-parity guessing.
    // Pass 2: divergence (in=stateB, out=div)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, fluid_.divPipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, fluid_.computeLayout, 0, 1, &fluid_.setDiv, 0, nullptr);
    vkCmdPushConstants(cmd, fluid_.computeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(fp), &fp);
    vkCmdDispatch(cmd, groups, groups, 1);
    barrier(fluid_.divBuf);

    // Pass 3: Jacobi pressure (N iterations, ping-pong pressA <-> pressB).
    // Before iteration 0, clear pressA (the "previous" pressure) to zero so
    // the first Jacobi step is well-defined.
    vkCmdFillBuffer(cmd, fluid_.pressA, 0, VK_WHOLE_SIZE, 0);
    VkBufferMemoryBarrier clearBm{};
    clearBm.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    clearBm.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    clearBm.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    clearBm.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    clearBm.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    clearBm.buffer = fluid_.pressA; clearBm.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              0, 0, nullptr, 1, &clearBm, 0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, fluid_.jacobiPipe);
    vkCmdPushConstants(cmd, fluid_.computeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(fp), &fp);
    VkDescriptorSet sets[2] = { fluid_.setJacA, fluid_.setJacB };
    for (std::uint32_t i = 0; i < config_.fluidJacobiIters; ++i) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, fluid_.computeLayout,
                                0, 1, &sets[i & 1u], 0, nullptr);
        vkCmdDispatch(cmd, groups, groups, 1);
        VkBuffer out = (i & 1u) ? fluid_.pressA : fluid_.pressB;
        barrier(out);
    }
    // Pass 4: gradient subtract (in=stateB, out=canonical stateA). The final
    // pressure buffer was selected once when the immutable set was created.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, fluid_.subtractPipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, fluid_.computeLayout, 0, 1, &fluid_.setSub, 0, nullptr);
    vkCmdPushConstants(cmd, fluid_.computeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(fp), &fp);
    vkCmdDispatch(cmd, groups, groups, 1);
    barrier(fluid_.stateA, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    if (timestampsSupported_) {
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            timestampQueryPool_, timestampBase + 1);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            timestampQueryPool_, timestampBase + 2);
    }

    // Render pass: read the canonical stateA and present.
    if (!config_.headless) {
        VkClearValue clear{};
        clear.color = {{0.04f, 0.08f, 0.14f, 1.0f}};
        VkRenderPassBeginInfo rp{};
        rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp.renderPass = renderPass_;
        rp.framebuffer = swapChainFramebuffers_[imageIndex];
        rp.renderArea.extent = swapChainExtent_;
        rp.clearValueCount = 1; rp.pClearValues = &clear;
        vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fluid_.renderPipe);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fluid_.renderLayout, 0, 1, &fluid_.setRender, 0, nullptr);
        FluidRenderParams rparam{ N, fluid_.simTime, 0.88f, 1u };
        vkCmdPushConstants(cmd, fluid_.renderLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(rparam), &rparam);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }
    if (timestampsSupported_)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            timestampQueryPool_, timestampBase + 3);
}

// -----------------------------------------------------------------------
// Cinematic Liquid v1: 3D MLS-MPM + density-volume free-surface raymarch.
// -----------------------------------------------------------------------

namespace {

struct alignas(16) MlsMpmPushConstants {
    std::uint32_t gridSizeAndCount[4];
    float simulation[4];
    float material[4];
    float gridOriginDx[4];
    float sphere[4];
    float collision[4];
};
static_assert(sizeof(MlsMpmPushConstants) == 96,
              "MLS-MPM push constants must match all compute shaders");

struct alignas(16) CinematicLiquidRenderPushConstants {
    float cameraTime[4];
    float targetAspect[4];
    float volumeMinIso[4];
    float volumeMaxStep[4];
    float sphere[4];
    std::uint32_t render[4];
};
static_assert(sizeof(CinematicLiquidRenderPushConstants) == 96,
              "Cinematic Liquid render push constants must match GLSL");

// Shared 128-byte ABI for every cinematic_liquid_sph_* pass.
struct alignas(16) CinematicLiquidSphPushConstants {
    std::uint32_t counts[4];   // particleCount, tableSize, scanCount, scanLevel/bodyCount
    float sim[4];              // dtSim, gravitySim, smoothingRadius, collisionDamping
    float fluid[4];            // targetDensity, pressureMult, nearPressureMult, viscosity
    float kernels[4];          // spikyPow2, spikyPow3, spikyPow2Deriv, spikyPow3Deriv
    float boundsMin[4];        // sim bounds min, w = poly6Scale
    float boundsMax[4];        // sim bounds max, w = worldScale
    float world[4];            // world origin, w = dtWorld
    float coupling[4];         // impulseFixedScale, particleMassWorld, restitution, friction
};
static_assert(sizeof(CinematicLiquidSphPushConstants) == 128,
              "Cinematic Liquid SPH push constants must match GLSL");

constexpr float kLiquidOriginX = -1.68f;
constexpr float kLiquidOriginY = -0.08f;
constexpr float kLiquidOriginZ = -1.12f;
constexpr float kLiquidRestDensity = 1000.0f;
constexpr float kLiquidFixedPointScale = 65'536.0f;
constexpr float kLiquidSphereX = 0.0f;
constexpr float kLiquidSphereY = 0.62f;
constexpr float kLiquidSphereZ = 0.0f;
constexpr float kLiquidSphereRadius = 0.34f;

void appendLiquidBlock(std::vector<MlsMpmParticleGpu>& particles,
                       V3 lo, V3 hi, float spacing, V3 initialVelocity,
                       std::uint32_t seedBase) {
    std::uint32_t serial = 0;
    for (float z = lo.z; z <= hi.z; z += spacing) {
        for (float y = lo.y; y <= hi.y; y += spacing) {
            for (float x = lo.x; x <= hi.x; x += spacing, ++serial) {
                const float jitterScale = spacing * 0.045f;
                MlsMpmParticleGpu p{};
                p.position[0] = x + liquidJitter(seedBase + serial * 3u + 0u) * jitterScale;
                p.position[1] = y + liquidJitter(seedBase + serial * 3u + 1u) * jitterScale;
                p.position[2] = z + liquidJitter(seedBase + serial * 3u + 2u) * jitterScale;
                p.position[3] = 1.0f;
                p.velocity[0] = initialVelocity.x;
                p.velocity[1] = initialVelocity.y;
                p.velocity[2] = initialVelocity.z + 0.08f * std::sin(x * 7.0f + z * 5.0f);
                p.velocity[3] = 0.0f;
                particles.push_back(p);
            }
        }
    }
}

} // namespace

void VulkanBackend::CreateCinematicLiquidResources() {
    auto& r = cinematicLiquid_;
    r.gridX = kCinematicLiquidGridX;
    r.gridY = kCinematicLiquidGridY;
    r.gridZ = kCinematicLiquidGridZ;
    r.dx = kCinematicLiquidDx;
    r.particleSpacing = kCinematicLiquidDx * 0.72f;
    r.particleMass = kLiquidRestDensity * r.particleSpacing *
                     r.particleSpacing * r.particleSpacing;

    std::vector<MlsMpmParticleGpu> initial;
    initial.reserve(300'000);
    // Two asymmetrical reservoirs collide around the central obstacle.  The
    // result is a recognisable dam-break/splash scene instead of a generic
    // blob or a FurMark-like torus silhouette.
    appendLiquidBlock(initial,
        V3{-1.45f, 0.10f, -0.75f}, V3{-0.40f, 1.45f, 0.75f},
        r.particleSpacing, V3{1.40f, 0.0f, 0.0f}, 0x10203040u);
    appendLiquidBlock(initial,
        V3{0.65f, 0.80f, -0.65f}, V3{1.42f, 1.50f, 0.65f},
        r.particleSpacing, V3{-1.20f, -0.24f, 0.0f}, 0x90abcdefu);
    r.particleCount = static_cast<std::uint32_t>(initial.size());
    // Publish the true internal particle count to AppBase after the generic
    // compatibility buffer has already been created. Result metadata and the
    // integrated score must never report that tiny unused buffer instead.
    config_.particleCount = r.particleCount;

    const std::uint64_t gridCellCount64 = std::uint64_t(r.gridX) * r.gridY * r.gridZ;
    if (gridCellCount64 > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("Cinematic Liquid grid is too large");
    const VkDeviceSize particleBytes = VkDeviceSize(initial.size()) * sizeof(MlsMpmParticleGpu);
    const VkDeviceSize gridBytes = VkDeviceSize(gridCellCount64) * sizeof(std::int32_t) * 4u;

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);
    CreateFluidBuffer(device_, memProps, particleBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        &r.particles, &r.particlesMem);
    CreateFluidBuffer(device_, memProps, particleBytes,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        &r.seedParticles, &r.seedParticlesMem);
    CreateFluidBuffer(device_, memProps, gridBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        &r.grid, &r.gridMem);

    // R32F is both a storage target for the resolve pass and a filtered 3D
    // texture for the raymarch.  Fall back to nearest filtering only if a
    // device lacks linear filtering for this otherwise core format.
    const VkFormat volumeFormat = VK_FORMAT_R32_SFLOAT;
    VkFormatProperties formatProps{};
    vkGetPhysicalDeviceFormatProperties(physicalDevice_, volumeFormat, &formatProps);
    if ((formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0 ||
        (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0) {
        throw std::runtime_error("Cinematic Liquid requires sampled+storage R32_SFLOAT 3D images");
    }
    const bool linearFilter =
        (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_3D;
    imageInfo.format = volumeFormat;
    imageInfo.extent = { r.gridX, r.gridY, r.gridZ };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device_, &imageInfo, nullptr, &r.densityImage) != VK_SUCCESS)
        throw std::runtime_error("vkCreateImage (Cinematic Liquid density) failed");

    VkMemoryRequirements imageReq{};
    vkGetImageMemoryRequirements(device_, r.densityImage, &imageReq);
    VkMemoryAllocateInfo imageAlloc{};
    imageAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imageAlloc.allocationSize = imageReq.size;
    imageAlloc.memoryTypeIndex = FindMemoryType(imageReq.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device_, &imageAlloc, nullptr, &r.densityImageMem) != VK_SUCCESS ||
        vkBindImageMemory(device_, r.densityImage, r.densityImageMem, 0) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid density image allocation failed");

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = r.densityImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
    viewInfo.format = volumeFormat;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView(device_, &viewInfo, nullptr, &r.densityImageView) != VK_SUCCESS)
        throw std::runtime_error("vkCreateImageView (Cinematic Liquid density) failed");

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = linearFilter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    samplerInfo.minFilter = linearFilter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 0.0f;
    if (vkCreateSampler(device_, &samplerInfo, nullptr, &r.densitySampler) != VK_SUCCESS)
        throw std::runtime_error("vkCreateSampler (Cinematic Liquid density) failed");

    // Host-visible staging upload, copied into both the live particle buffer
    // and an immutable device-local seed used for periodic long-run resets.
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = particleBytes;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &stagingInfo, nullptr, &staging) != VK_SUCCESS)
        throw std::runtime_error("vkCreateBuffer (Cinematic Liquid staging) failed");
    VkMemoryRequirements stagingReq{};
    vkGetBufferMemoryRequirements(device_, staging, &stagingReq);
    VkMemoryAllocateInfo stagingAlloc{};
    stagingAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAlloc.allocationSize = stagingReq.size;
    stagingAlloc.memoryTypeIndex = FindMemoryType(stagingReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device_, &stagingAlloc, nullptr, &stagingMem) != VK_SUCCESS ||
        vkBindBufferMemory(device_, staging, stagingMem, 0) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid staging allocation failed");
    void* mapped = nullptr;
    if (vkMapMemory(device_, stagingMem, 0, particleBytes, 0, &mapped) != VK_SUCCESS)
        throw std::runtime_error("vkMapMemory (Cinematic Liquid staging) failed");
    std::memcpy(mapped, initial.data(), static_cast<std::size_t>(particleBytes));
    vkUnmapMemory(device_, stagingMem);

    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = commandPool_;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    VkCommandBuffer uploadCmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device_, &cmdAlloc, &uploadCmd) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid upload command allocation failed");
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(uploadCmd, &begin);
    VkBufferCopy copy{ 0, 0, particleBytes };
    vkCmdCopyBuffer(uploadCmd, staging, r.particles, 1, &copy);
    vkCmdCopyBuffer(uploadCmd, staging, r.seedParticles, 1, &copy);
    vkCmdFillBuffer(uploadCmd, r.grid, 0, VK_WHOLE_SIZE, 0);
    VkMemoryBarrier initialBufferBarrier{};
    initialBufferBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    initialBufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    initialBufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                                         VK_ACCESS_SHADER_WRITE_BIT |
                                         VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(uploadCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         1, &initialBufferBarrier, 0, nullptr, 0, nullptr);
    VkImageMemoryBarrier initialImageBarrier{};
    initialImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    initialImageBarrier.srcAccessMask = 0;
    initialImageBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    initialImageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    initialImageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    initialImageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    initialImageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    initialImageBarrier.image = r.densityImage;
    initialImageBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(uploadCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &initialImageBarrier);
    vkEndCommandBuffer(uploadCmd);
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &uploadCmd;
    if (vkQueueSubmit(graphicsQueue_, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid upload submit failed");
    vkQueueWaitIdle(graphicsQueue_);
    vkFreeCommandBuffers(device_, commandPool_, 1, &uploadCmd);
    vkDestroyBuffer(device_, staging, nullptr);
    vkFreeMemory(device_, stagingMem, nullptr);

    // Compute descriptors are shared by every MLS-MPM pass and the density
    // resolve: binding 0 particles, binding 1 fixed-point grid, binding 2 R32F
    // storage image.  Each shader declares only the subset it consumes.
    VkDescriptorSetLayoutBinding computeBindings[3]{};
    computeBindings[0].binding = 0;
    computeBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeBindings[0].descriptorCount = 1;
    computeBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computeBindings[1] = computeBindings[0];
    computeBindings[1].binding = 1;
    computeBindings[2].binding = 2;
    computeBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    computeBindings[2].descriptorCount = 1;
    computeBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo computeLayoutInfo{};
    computeLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    computeLayoutInfo.bindingCount = 3;
    computeLayoutInfo.pBindings = computeBindings;
    if (vkCreateDescriptorSetLayout(device_, &computeLayoutInfo, nullptr,
                                    &r.computeSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid compute descriptor layout failed");

    VkDescriptorPoolSize computePoolSizes[2]{};
    computePoolSizes[0] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 };
    computePoolSizes[1] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 };
    VkDescriptorPoolCreateInfo computePoolInfo{};
    computePoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    computePoolInfo.poolSizeCount = 2;
    computePoolInfo.pPoolSizes = computePoolSizes;
    computePoolInfo.maxSets = 1;
    if (vkCreateDescriptorPool(device_, &computePoolInfo, nullptr, &r.computePool) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid compute descriptor pool failed");
    VkDescriptorSetAllocateInfo computeSetAlloc{};
    computeSetAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    computeSetAlloc.descriptorPool = r.computePool;
    computeSetAlloc.descriptorSetCount = 1;
    computeSetAlloc.pSetLayouts = &r.computeSetLayout;
    if (vkAllocateDescriptorSets(device_, &computeSetAlloc, &r.computeSet) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid compute descriptor set failed");

    VkDescriptorBufferInfo particleInfo{ r.particles, 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo gridInfo{ r.grid, 0, VK_WHOLE_SIZE };
    VkDescriptorImageInfo storageImageInfo{};
    storageImageInfo.imageView = r.densityImageView;
    storageImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkWriteDescriptorSet computeWrites[3]{};
    for (auto& write : computeWrites) {
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = r.computeSet;
        write.descriptorCount = 1;
    }
    computeWrites[0].dstBinding = 0;
    computeWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeWrites[0].pBufferInfo = &particleInfo;
    computeWrites[1].dstBinding = 1;
    computeWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    computeWrites[1].pBufferInfo = &gridInfo;
    computeWrites[2].dstBinding = 2;
    computeWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    computeWrites[2].pImageInfo = &storageImageInfo;
    vkUpdateDescriptorSets(device_, 3, computeWrites, 0, nullptr);

    VkPushConstantRange computePushRange{};
    computePushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computePushRange.size = sizeof(MlsMpmPushConstants);
    VkPipelineLayoutCreateInfo computePipelineLayoutInfo{};
    computePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computePipelineLayoutInfo.setLayoutCount = 1;
    computePipelineLayoutInfo.pSetLayouts = &r.computeSetLayout;
    computePipelineLayoutInfo.pushConstantRangeCount = 1;
    computePipelineLayoutInfo.pPushConstantRanges = &computePushRange;
    if (vkCreatePipelineLayout(device_, &computePipelineLayoutInfo, nullptr,
                               &r.computeLayout) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid compute pipeline layout failed");

    auto createComputePipeline = [&](const char* file, VkPipeline* out) {
        const auto code = ReadFileBytes(shaderDir_ + file);
        VkShaderModule module = CreateShaderModule(code);
        VkPipelineShaderStageCreateInfo stage{};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = module;
        stage.pName = "main";
        VkComputePipelineCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        info.stage = stage;
        info.layout = r.computeLayout;
        const VkResult result = vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1,
                                                          &info, nullptr, out);
        vkDestroyShaderModule(device_, module, nullptr);
        if (result != VK_SUCCESS)
            throw std::runtime_error(std::string("Cinematic Liquid compute pipeline failed: ") + file);
    };
    createComputePipeline("mls_mpm_clear_grid.comp.spv", &r.clearGridPipe);
    createComputePipeline("mls_mpm_p2g_mass_momentum.comp.spv", &r.p2gMassPipe);
    createComputePipeline("mls_mpm_p2g_density_stress.comp.spv", &r.p2gStressPipe);
    createComputePipeline("mls_mpm_grid_update.comp.spv", &r.gridUpdatePipe);
    createComputePipeline("mls_mpm_g2p.comp.spv", &r.g2pPipe);
    createComputePipeline("cinematic_liquid_resolve.comp.spv", &r.resolveDensityPipe);

    // The raymarch has one combined sampler descriptor and a 96-byte fragment
    // push block containing camera/domain data.  No vertex buffers are used.
    VkDescriptorSetLayoutBinding renderBinding{};
    renderBinding.binding = 0;
    renderBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    renderBinding.descriptorCount = 1;
    renderBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo renderLayoutInfo{};
    renderLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    renderLayoutInfo.bindingCount = 1;
    renderLayoutInfo.pBindings = &renderBinding;
    if (vkCreateDescriptorSetLayout(device_, &renderLayoutInfo, nullptr,
                                    &r.renderSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid render descriptor layout failed");
    VkDescriptorPoolSize renderPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };
    VkDescriptorPoolCreateInfo renderPoolInfo{};
    renderPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    renderPoolInfo.poolSizeCount = 1;
    renderPoolInfo.pPoolSizes = &renderPoolSize;
    renderPoolInfo.maxSets = 1;
    if (vkCreateDescriptorPool(device_, &renderPoolInfo, nullptr, &r.renderPool) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid render descriptor pool failed");
    VkDescriptorSetAllocateInfo renderSetAlloc{};
    renderSetAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    renderSetAlloc.descriptorPool = r.renderPool;
    renderSetAlloc.descriptorSetCount = 1;
    renderSetAlloc.pSetLayouts = &r.renderSetLayout;
    if (vkAllocateDescriptorSets(device_, &renderSetAlloc, &r.renderSet) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid render descriptor set failed");
    VkDescriptorImageInfo sampledImageInfo{};
    sampledImageInfo.sampler = r.densitySampler;
    sampledImageInfo.imageView = r.densityImageView;
    sampledImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkWriteDescriptorSet renderWrite{};
    renderWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    renderWrite.dstSet = r.renderSet;
    renderWrite.dstBinding = 0;
    renderWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    renderWrite.descriptorCount = 1;
    renderWrite.pImageInfo = &sampledImageInfo;
    vkUpdateDescriptorSets(device_, 1, &renderWrite, 0, nullptr);

    VkPushConstantRange renderPushRange{};
    renderPushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    renderPushRange.size = sizeof(CinematicLiquidRenderPushConstants);
    VkPipelineLayoutCreateInfo renderPipelineLayoutInfo{};
    renderPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    renderPipelineLayoutInfo.setLayoutCount = 1;
    renderPipelineLayoutInfo.pSetLayouts = &r.renderSetLayout;
    renderPipelineLayoutInfo.pushConstantRangeCount = 1;
    renderPipelineLayoutInfo.pPushConstantRanges = &renderPushRange;
    if (vkCreatePipelineLayout(device_, &renderPipelineLayoutInfo, nullptr,
                               &r.renderLayout) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid render pipeline layout failed");

    const auto vertexCode = ReadFileBytes(shaderDir_ + "cinematic_liquid_render.vert.spv");
    const auto fragmentCode = ReadFileBytes(shaderDir_ + "cinematic_liquid_render.frag.spv");
    VkShaderModule vertexModule = CreateShaderModule(vertexCode);
    VkShaderModule fragmentModule = CreateShaderModule(fragmentCode);
    VkPipelineShaderStageCreateInfo renderStages[2]{};
    renderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    renderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    renderStages[0].module = vertexModule;
    renderStages[0].pName = "main";
    renderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    renderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    renderStages[1].module = fragmentModule;
    renderStages[1].pName = "main";
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport viewport{ 0.0f, 0.0f, float(swapChainExtent_.width),
                         float(swapChainExtent_.height), 0.0f, 1.0f };
    VkRect2D scissor{ {0, 0}, swapChainExtent_ };
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;
    VkGraphicsPipelineCreateInfo renderPipelineInfo{};
    renderPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    renderPipelineInfo.stageCount = 2;
    renderPipelineInfo.pStages = renderStages;
    renderPipelineInfo.pVertexInputState = &vertexInput;
    renderPipelineInfo.pInputAssemblyState = &assembly;
    renderPipelineInfo.pViewportState = &viewportState;
    renderPipelineInfo.pRasterizationState = &raster;
    renderPipelineInfo.pMultisampleState = &multisample;
    renderPipelineInfo.pColorBlendState = &blend;
    renderPipelineInfo.layout = r.renderLayout;
    renderPipelineInfo.renderPass = renderPass_;
    renderPipelineInfo.subpass = 0;
    const VkResult renderResult = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1,
        &renderPipelineInfo, nullptr, &r.renderPipe);
    vkDestroyShaderModule(device_, fragmentModule, nullptr);
    vkDestroyShaderModule(device_, vertexModule, nullptr);
    if (renderResult != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid raymarch pipeline failed");

    graphicsPipeline_ = r.renderPipe;
    graphicsPipelineLayout_ = r.renderLayout;

    std::cout << "[Cinematic Liquid] MLS-MPM " << r.particleCount
              << " particles, grid " << r.gridX << "x" << r.gridY << "x" << r.gridZ
              << ", density raymarch " << (linearFilter ? "linear" : "nearest")
              << " filtering\n";
}

void VulkanBackend::CreateCinematicLiquidV2Resources() {
    auto& r = cinematicLiquid_;
    r.isV2 = true;
    r.isSph = config_.liquidSolverSph;
    r.gridX = kCinematicLiquidV2GridX;
    r.gridY = kCinematicLiquidV2GridY;
    r.gridZ = kCinematicLiquidV2GridZ;
    r.surfaceX = kCinematicLiquidV2SurfaceX;
    r.surfaceY = kCinematicLiquidV2SurfaceY;
    r.surfaceZ = kCinematicLiquidV2SurfaceZ;
    r.substeps = r.isSph ? kCinematicLiquidSphSubsteps
                         : kCinematicLiquidV2Substeps;
    r.raySteps = kCinematicLiquidV2RaySteps;
    r.shaderVersion = r.isSph ? kCinematicLiquidSphShaderVersion
                              : kCinematicLiquidV2ShaderVersion;
    r.bodyCount = kCinematicLiquidV2BodyCount;
    r.dx = kCinematicLiquidV2Dx;
    if (r.isSph) {
        // The SPH slice reconstructs its surface from the same shared
        // particle buffer; spacing/mass describe the world-space footprint of
        // one reference particle (spawn density 600 per sim unit^3).
        r.particleSpacing = kCinematicLiquidSphSpawnSpacing *
                            kCinematicLiquidSphWorldScale;
    } else {
        r.particleSpacing = r.dx * 0.72f;
    }
    r.particleMass = kLiquidV2RestDensity * r.particleSpacing *
                     r.particleSpacing * r.particleSpacing;

    // The reference raymarch scene gets its thin sheets and spray from a real
    // dam-break, not from shading a calm surface.  V2 keeps a shallow pool bed
    // under the toys and places the remaining particles in a tall left-hand
    // reservoir.  A deterministic GPU gate releases that wall during the
    // scored camera path.  Integer lattice counts keep the score contract
    // identical on every compiler. V7 redistributes the same ~321k budget into
    // a deeper 14-layer play pool so the 0.40 m sink sphere can generate a
    // resolved entry crown, while retaining a tall dam reservoir:
    // 142*14*98 + 48*37*71 = 320,920 particles.
    std::vector<MlsMpmParticleGpu> initial;
    // SPH slice only: sim-space seed positions (4 floats per particle) that
    // parallel `initial`'s world-space presentation copies.
    std::vector<float> sphInitial;
    if (r.isSph) {
        // Deterministic SPH lattice in the reference units: a deep bed plus a
        // left dam column that collapses at t=0 and again after the 4 s
        // restage.  148*16*98 + 30*30*96 = 318,464.  Every lattice point
        // sits inside the physical pool wall (sim-space rounded rect centre
        // (11.906, 8.906), half extents (9.328, 6.328) = drawn-ring inset
        // 0.45 plus a 0.10 m reconstruction-inflation allowance, corner
        // radius 1.406, corner margin 0.50); the water body must start
        // inside the visible pool, and only spray that clears the rim may
        // land on the grass outside.  Sixteen bed layers put the resting
        // waterline near 0.49 m so the 0.40 m sink sphere reads as fully
        // submerged; the raised 0.42 wall keeps the deeper fill contained
        // while the column still tops out near the rim for a visible surge.
        constexpr std::uint32_t bedX = 148, bedY = 16, bedZ = 98;
        constexpr std::uint32_t colX = 30, colY = 30, colZ = 96;
        constexpr std::size_t expectedParticles =
            std::size_t(bedX) * bedY * bedZ + std::size_t(colX) * colY * colZ;
        initial.reserve(expectedParticles);
        sphInitial.reserve(expectedParticles * 4u);
        const float spacing = kCinematicLiquidSphSpawnSpacing;
        const float worldScale = kCinematicLiquidSphWorldScale;
        std::uint32_t serial = 0;
        auto appendSimLattice = [&](std::uint32_t nx, std::uint32_t ny,
                                    std::uint32_t nz, float x0, float y0,
                                    float z0) {
            for (std::uint32_t iz = 0; iz < nz; ++iz) {
                for (std::uint32_t iy = 0; iy < ny; ++iy) {
                    for (std::uint32_t ix = 0; ix < nx; ++ix, ++serial) {
                        const float jitter = spacing * 0.20f;
                        const float x = x0 + float(ix) * spacing +
                            liquidJitter(0x5b17aa01u + serial * 3u) * jitter;
                        const float y = y0 + float(iy) * spacing +
                            liquidJitter(0x5b17aa02u + serial * 3u) * jitter;
                        const float z = z0 + float(iz) * spacing +
                            liquidJitter(0x5b17aa03u + serial * 3u) * jitter;
                        sphInitial.push_back(x);
                        sphInitial.push_back(y);
                        sphInitial.push_back(z);
                        sphInitial.push_back(1.0f);
                        MlsMpmParticleGpu particle{};
                        particle.position[0] = kLiquidV2OriginX + x * worldScale;
                        particle.position[1] = kLiquidV2OriginY + y * worldScale;
                        particle.position[2] = kLiquidV2OriginZ + z * worldScale;
                        particle.position[3] = 1.0f;
                        initial.push_back(particle);
                    }
                }
            }
        };
        appendSimLattice(bedX, bedY, bedZ, 3.08f, 0.66f, 3.08f);
        appendSimLattice(colX, colY, colZ, 3.08f, 2.55f, 3.08f);
        if (initial.size() != expectedParticles)
            throw std::runtime_error(
                "Cinematic Liquid SPH deterministic seed drifted");
        r.particleCount = static_cast<std::uint32_t>(initial.size());
        config_.particleCount = r.particleCount;
    } else {
        BuildCinematicLiquidV2ParticleSeed(initial, r.dx, r.particleSpacing,
                                           r.particleMass);
        r.particleCount = static_cast<std::uint32_t>(initial.size());
        config_.particleCount = r.particleCount;
    }

    std::vector<CinematicLiquidBodyStateGpu> bodies;
    BuildCinematicLiquidV2BodySeed(bodies);

    const std::uint64_t gridCellCount64 = std::uint64_t(r.gridX) * r.gridY * r.gridZ;
    const std::uint64_t surfaceCellCount64 =
        std::uint64_t(r.surfaceX) * r.surfaceY * r.surfaceZ;
    const VkDeviceSize particleBytes = VkDeviceSize(initial.size()) * sizeof(MlsMpmParticleGpu);
    const VkDeviceSize gridBytes = VkDeviceSize(gridCellCount64) * sizeof(std::int32_t) * 4u;
    const VkDeviceSize bodyBytes = VkDeviceSize(bodies.size()) * sizeof(CinematicLiquidBodyStateGpu);
    const VkDeviceSize impulseBytes = VkDeviceSize(bodies.size()) * sizeof(CinematicLiquidBodyImpulseGpu);
    const VkDeviceSize densityAtomicBytes =
        VkDeviceSize(surfaceCellCount64) * sizeof(std::uint32_t);

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);
    CreateFluidBuffer(device_, memProps, particleBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        &r.particles, &r.particlesMem);
    CreateFluidBuffer(device_, memProps, particleBytes,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        &r.seedParticles, &r.seedParticlesMem);
    CreateFluidBuffer(device_, memProps, gridBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        &r.grid, &r.gridMem);
    CreateFluidBuffer(device_, memProps, bodyBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        &r.bodies, &r.bodiesMem);
    CreateFluidBuffer(device_, memProps, bodyBytes,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        &r.seedBodies, &r.seedBodiesMem);
    CreateFluidBuffer(device_, memProps, impulseBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        &r.bodyImpulses, &r.bodyImpulsesMem);
    CreateFluidBuffer(device_, memProps, densityAtomicBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        &r.densityAtomicBuffer, &r.densityAtomicBufferMem);

    const VkFormat volumeFormat = VK_FORMAT_R32_SFLOAT;
    VkFormatProperties formatProps{};
    vkGetPhysicalDeviceFormatProperties(physicalDevice_, volumeFormat, &formatProps);
    if ((formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) == 0 ||
        (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) == 0)
        throw std::runtime_error("Cinematic Liquid v2 requires sampled+storage R32_SFLOAT 3D images");
    const bool linearFilter =
        (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
    if (!linearFilter)
        throw std::runtime_error(
            "Cinematic Liquid v2 fixed score requires linear-filterable R32_SFLOAT 3D images");

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_3D;
    imageInfo.format = volumeFormat;
    imageInfo.extent = {r.surfaceX, r.surfaceY, r.surfaceZ};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device_, &imageInfo, nullptr, &r.densityImage) != VK_SUCCESS)
        throw std::runtime_error("vkCreateImage (Cinematic Liquid v2 density) failed");
    VkMemoryRequirements imageReq{};
    vkGetImageMemoryRequirements(device_, r.densityImage, &imageReq);
    VkMemoryAllocateInfo imageAlloc{};
    imageAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    imageAlloc.allocationSize = imageReq.size;
    imageAlloc.memoryTypeIndex = FindMemoryType(imageReq.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device_, &imageAlloc, nullptr, &r.densityImageMem) != VK_SUCCESS ||
        vkBindImageMemory(device_, r.densityImage, r.densityImageMem, 0) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 density allocation failed");
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = r.densityImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
    viewInfo.format = volumeFormat;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(device_, &viewInfo, nullptr, &r.densityImageView) != VK_SUCCESS)
        throw std::runtime_error("vkCreateImageView (Cinematic Liquid v2 density) failed");

    // Keep whitewater independent from density.  Packing it into the density
    // scalar would move the iso-surface and turn a presentation feature into
    // a simulation/score change.  The second R32F volume is written by the
    // same deterministic resolve dispatch and sampled by the raymarch.
    VkImageCreateInfo whitewaterImageInfo = imageInfo;
    whitewaterImageInfo.extent = {r.gridX, r.gridY, r.gridZ};
    if (vkCreateImage(device_, &whitewaterImageInfo, nullptr, &r.whitewaterImage) != VK_SUCCESS)
        throw std::runtime_error("vkCreateImage (Cinematic Liquid v2 whitewater) failed");
    vkGetImageMemoryRequirements(device_, r.whitewaterImage, &imageReq);
    imageAlloc.allocationSize = imageReq.size;
    imageAlloc.memoryTypeIndex = FindMemoryType(imageReq.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device_, &imageAlloc, nullptr,
                         &r.whitewaterImageMem) != VK_SUCCESS ||
        vkBindImageMemory(device_, r.whitewaterImage,
                          r.whitewaterImageMem, 0) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 whitewater allocation failed");
    viewInfo.image = r.whitewaterImage;
    if (vkCreateImageView(device_, &viewInfo, nullptr,
                          &r.whitewaterImageView) != VK_SUCCESS)
        throw std::runtime_error("vkCreateImageView (Cinematic Liquid v2 whitewater) failed");
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = linearFilter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    samplerInfo.minFilter = linearFilter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 0.0f;
    if (vkCreateSampler(device_, &samplerInfo, nullptr, &r.densitySampler) != VK_SUCCESS)
        throw std::runtime_error("vkCreateSampler (Cinematic Liquid v2 density) failed");

    auto makeStaging = [&](const void* data, VkDeviceSize bytes,
                           VkBuffer& buffer, VkDeviceMemory& memory) {
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = bytes;
        info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &info, nullptr, &buffer) != VK_SUCCESS)
            throw std::runtime_error("Cinematic Liquid v2 staging buffer failed");
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device_, buffer, &req);
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = FindMemoryType(req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(device_, &alloc, nullptr, &memory) != VK_SUCCESS ||
            vkBindBufferMemory(device_, buffer, memory, 0) != VK_SUCCESS)
            throw std::runtime_error("Cinematic Liquid v2 staging allocation failed");
        void* mapped = nullptr;
        if (vkMapMemory(device_, memory, 0, bytes, 0, &mapped) != VK_SUCCESS)
            throw std::runtime_error("Cinematic Liquid v2 staging map failed");
        std::memcpy(mapped, data, static_cast<std::size_t>(bytes));
        vkUnmapMemory(device_, memory);
    };
    VkBuffer particleStaging = VK_NULL_HANDLE, bodyStaging = VK_NULL_HANDLE;
    VkDeviceMemory particleStagingMem = VK_NULL_HANDLE, bodyStagingMem = VK_NULL_HANDLE;
    makeStaging(initial.data(), particleBytes, particleStaging, particleStagingMem);
    makeStaging(bodies.data(), bodyBytes, bodyStaging, bodyStagingMem);

    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = commandPool_;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    VkCommandBuffer uploadCmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device_, &cmdAlloc, &uploadCmd) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 upload command allocation failed");
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(uploadCmd, &begin);
    VkBufferCopy particleCopy{0, 0, particleBytes};
    VkBufferCopy bodyCopy{0, 0, bodyBytes};
    vkCmdCopyBuffer(uploadCmd, particleStaging, r.particles, 1, &particleCopy);
    vkCmdCopyBuffer(uploadCmd, particleStaging, r.seedParticles, 1, &particleCopy);
    vkCmdCopyBuffer(uploadCmd, bodyStaging, r.bodies, 1, &bodyCopy);
    vkCmdCopyBuffer(uploadCmd, bodyStaging, r.seedBodies, 1, &bodyCopy);
    vkCmdFillBuffer(uploadCmd, r.grid, 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(uploadCmd, r.bodyImpulses, 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(uploadCmd, r.densityAtomicBuffer, 0, VK_WHOLE_SIZE, 0);
    VkMemoryBarrier initialBufferBarrier{};
    initialBufferBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    initialBufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    initialBufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                                         VK_ACCESS_SHADER_WRITE_BIT |
                                         VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(uploadCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         1, &initialBufferBarrier, 0, nullptr, 0, nullptr);
    VkImageMemoryBarrier initialImageBarriers[2]{};
    for (std::uint32_t i = 0; i < 2; ++i) {
        initialImageBarriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        initialImageBarriers[i].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        initialImageBarriers[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        initialImageBarriers[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        initialImageBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        initialImageBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        initialImageBarriers[i].subresourceRange =
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    }
    initialImageBarriers[0].image = r.densityImage;
    initialImageBarriers[1].image = r.whitewaterImage;
    vkCmdPipelineBarrier(uploadCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 2, initialImageBarriers);
    vkEndCommandBuffer(uploadCmd);
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &uploadCmd;
    if (vkQueueSubmit(graphicsQueue_, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 upload submit failed");
    vkQueueWaitIdle(graphicsQueue_);
    vkFreeCommandBuffers(device_, commandPool_, 1, &uploadCmd);
    vkDestroyBuffer(device_, particleStaging, nullptr);
    vkFreeMemory(device_, particleStagingMem, nullptr);
    vkDestroyBuffer(device_, bodyStaging, nullptr);
    vkFreeMemory(device_, bodyStagingMem, nullptr);

    // Compute set: particles, fixed-point grid, density volume, body state,
    // atomic body impulses and the independent whitewater volume.  The
    // 128-byte push range remains the fixed v2 ABI.
    VkDescriptorSetLayoutBinding computeBindings[6]{};
    for (std::uint32_t i = 0; i < 6; ++i) {
        computeBindings[i].binding = i;
        computeBindings[i].descriptorCount = 1;
        computeBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        computeBindings[i].descriptorType = (i == 2 || i == 5)
            ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    VkDescriptorSetLayoutCreateInfo computeLayoutInfo{};
    computeLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    computeLayoutInfo.bindingCount = 6;
    computeLayoutInfo.pBindings = computeBindings;
    if (vkCreateDescriptorSetLayout(device_, &computeLayoutInfo, nullptr,
                                    &r.computeSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 compute descriptor layout failed");
    VkDescriptorPoolSize computePoolSizes[2] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2}
    };
    VkDescriptorPoolCreateInfo computePoolInfo{};
    computePoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    computePoolInfo.poolSizeCount = 2;
    computePoolInfo.pPoolSizes = computePoolSizes;
    computePoolInfo.maxSets = 1;
    if (vkCreateDescriptorPool(device_, &computePoolInfo, nullptr, &r.computePool) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 compute descriptor pool failed");
    VkDescriptorSetAllocateInfo computeSetAlloc{};
    computeSetAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    computeSetAlloc.descriptorPool = r.computePool;
    computeSetAlloc.descriptorSetCount = 1;
    computeSetAlloc.pSetLayouts = &r.computeSetLayout;
    if (vkAllocateDescriptorSets(device_, &computeSetAlloc, &r.computeSet) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 compute descriptor set failed");

    VkDescriptorBufferInfo particleInfo{r.particles, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo gridInfo{r.grid, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo bodyInfo{r.bodies, 0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo impulseInfo{r.bodyImpulses, 0, VK_WHOLE_SIZE};
    VkDescriptorImageInfo storageImageInfo{};
    storageImageInfo.imageView = r.densityImageView;
    storageImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo storageWhitewaterInfo{};
    storageWhitewaterInfo.imageView = r.whitewaterImageView;
    storageWhitewaterInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkWriteDescriptorSet computeWrites[6]{};
    for (std::uint32_t i = 0; i < 6; ++i) {
        computeWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        computeWrites[i].dstSet = r.computeSet;
        computeWrites[i].dstBinding = i;
        computeWrites[i].descriptorCount = 1;
        computeWrites[i].descriptorType = computeBindings[i].descriptorType;
    }
    computeWrites[0].pBufferInfo = &particleInfo;
    computeWrites[1].pBufferInfo = &gridInfo;
    computeWrites[2].pImageInfo = &storageImageInfo;
    computeWrites[3].pBufferInfo = &bodyInfo;
    computeWrites[4].pBufferInfo = &impulseInfo;
    computeWrites[5].pImageInfo = &storageWhitewaterInfo;
    vkUpdateDescriptorSets(device_, 6, computeWrites, 0, nullptr);

    VkPushConstantRange computePushRange{};
    computePushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computePushRange.size = sizeof(CinematicLiquidV2PushConstants);
    VkPipelineLayoutCreateInfo computePipelineLayoutInfo{};
    computePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computePipelineLayoutInfo.setLayoutCount = 1;
    computePipelineLayoutInfo.pSetLayouts = &r.computeSetLayout;
    computePipelineLayoutInfo.pushConstantRangeCount = 1;
    computePipelineLayoutInfo.pPushConstantRanges = &computePushRange;
    if (vkCreatePipelineLayout(device_, &computePipelineLayoutInfo, nullptr,
                               &r.computeLayout) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 compute pipeline layout failed");
    auto createComputePipeline = [&](const char* file, VkPipeline* out) {
        const auto code = ReadFileBytes(shaderDir_ + file);
        VkShaderModule module = CreateShaderModule(code);
        VkPipelineShaderStageCreateInfo stage{};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = module;
        stage.pName = "main";
        VkComputePipelineCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        info.stage = stage;
        info.layout = r.computeLayout;
        const VkResult result = vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1,
                                                          &info, nullptr, out);
        vkDestroyShaderModule(device_, module, nullptr);
        if (result != VK_SUCCESS)
            throw std::runtime_error(std::string("Cinematic Liquid v2 pipeline failed: ") + file);
    };
    createComputePipeline("mls_mpm_clear_grid_v2.comp.spv", &r.clearGridPipe);
    createComputePipeline("mls_mpm_p2g_mass_momentum_v2.comp.spv", &r.p2gMassPipe);
    createComputePipeline("mls_mpm_p2g_density_stress_v2.comp.spv", &r.p2gStressPipe);
    createComputePipeline("mls_mpm_grid_update_v2.comp.spv", &r.gridUpdatePipe);
    createComputePipeline("mls_mpm_g2p_v2.comp.spv", &r.g2pPipe);
    createComputePipeline("cinematic_liquid_rigid_integrate_v2.comp.spv", &r.rigidIntegratePipe);
    createComputePipeline("cinematic_liquid_resolve_v2.comp.spv", &r.resolveDensityPipe);

    // Independent particle-to-density reconstruction.  Its ABI is separate
    // from the simulation set so the render volume can evolve without
    // changing the MLS-MPM pass bindings or 128-byte push contract.
    VkDescriptorSetLayoutBinding surfaceBindings[3]{};
    surfaceBindings[0].binding = 0;
    surfaceBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    surfaceBindings[0].descriptorCount = 1;
    surfaceBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    surfaceBindings[1] = surfaceBindings[0];
    surfaceBindings[1].binding = 1;
    surfaceBindings[2].binding = 2;
    surfaceBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    surfaceBindings[2].descriptorCount = 1;
    surfaceBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo surfaceSetLayoutInfo{};
    surfaceSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    surfaceSetLayoutInfo.bindingCount = 3;
    surfaceSetLayoutInfo.pBindings = surfaceBindings;
    if (vkCreateDescriptorSetLayout(device_, &surfaceSetLayoutInfo, nullptr,
                                    &r.surfaceSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 surface descriptor layout failed");

    VkDescriptorPoolSize surfacePoolSizes[2] = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
    };
    VkDescriptorPoolCreateInfo surfacePoolInfo{};
    surfacePoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    surfacePoolInfo.poolSizeCount = 2;
    surfacePoolInfo.pPoolSizes = surfacePoolSizes;
    surfacePoolInfo.maxSets = 1;
    if (vkCreateDescriptorPool(device_, &surfacePoolInfo, nullptr,
                               &r.surfacePool) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 surface descriptor pool failed");
    VkDescriptorSetAllocateInfo surfaceSetAlloc{};
    surfaceSetAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    surfaceSetAlloc.descriptorPool = r.surfacePool;
    surfaceSetAlloc.descriptorSetCount = 1;
    surfaceSetAlloc.pSetLayouts = &r.surfaceSetLayout;
    if (vkAllocateDescriptorSets(device_, &surfaceSetAlloc,
                                 &r.surfaceSet) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 surface descriptor set failed");

    VkDescriptorBufferInfo surfaceAtomicInfo{
        r.densityAtomicBuffer, 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSet surfaceWrites[3]{};
    for (std::uint32_t i = 0; i < 3; ++i) {
        surfaceWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        surfaceWrites[i].dstSet = r.surfaceSet;
        surfaceWrites[i].dstBinding = i;
        surfaceWrites[i].descriptorCount = 1;
        surfaceWrites[i].descriptorType = surfaceBindings[i].descriptorType;
    }
    surfaceWrites[0].pBufferInfo = &particleInfo;
    surfaceWrites[1].pBufferInfo = &surfaceAtomicInfo;
    surfaceWrites[2].pImageInfo = &storageImageInfo;
    vkUpdateDescriptorSets(device_, 3, surfaceWrites, 0, nullptr);

    VkPushConstantRange surfacePushRange{};
    surfacePushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    surfacePushRange.size = sizeof(CinematicLiquidV2SurfacePushConstants);
    VkPipelineLayoutCreateInfo surfacePipelineLayoutInfo{};
    surfacePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    surfacePipelineLayoutInfo.setLayoutCount = 1;
    surfacePipelineLayoutInfo.pSetLayouts = &r.surfaceSetLayout;
    surfacePipelineLayoutInfo.pushConstantRangeCount = 1;
    surfacePipelineLayoutInfo.pPushConstantRanges = &surfacePushRange;
    if (vkCreatePipelineLayout(device_, &surfacePipelineLayoutInfo, nullptr,
                               &r.surfaceLayout) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 surface pipeline layout failed");
    auto createSurfacePipeline = [&](const char* file, VkPipeline* out) {
        const auto code = ReadFileBytes(shaderDir_ + file);
        VkShaderModule module = CreateShaderModule(code);
        VkPipelineShaderStageCreateInfo stage{};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = module;
        stage.pName = "main";
        VkComputePipelineCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        info.stage = stage;
        info.layout = r.surfaceLayout;
        const VkResult result = vkCreateComputePipelines(
            device_, VK_NULL_HANDLE, 1, &info, nullptr, out);
        vkDestroyShaderModule(device_, module, nullptr);
        if (result != VK_SUCCESS)
            throw std::runtime_error(
                std::string("Cinematic Liquid v2 surface pipeline failed: ") + file);
    };
    createSurfacePipeline("cinematic_liquid_surface_clear_v2.comp.spv",
                          &r.surfaceClearPipe);
    createSurfacePipeline("cinematic_liquid_surface_splat_v2.comp.spv",
                          &r.surfaceSplatPipe);
    createSurfacePipeline("cinematic_liquid_surface_resolve_v2.comp.spv",
                          &r.surfaceResolvePipe);

    // Render set intentionally retains binding numbers 2/3/4: density, body
    // state, then the derived whitewater volume.
    VkDescriptorSetLayoutBinding renderBindings[3]{};
    renderBindings[0].binding = 2;
    renderBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    renderBindings[0].descriptorCount = 1;
    renderBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    renderBindings[1].binding = 3;
    renderBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    renderBindings[1].descriptorCount = 1;
    renderBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    renderBindings[2].binding = 4;
    renderBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    renderBindings[2].descriptorCount = 1;
    renderBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo renderLayoutInfo{};
    renderLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    renderLayoutInfo.bindingCount = 3;
    renderLayoutInfo.pBindings = renderBindings;
    if (vkCreateDescriptorSetLayout(device_, &renderLayoutInfo, nullptr,
                                    &r.renderSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 render descriptor layout failed");
    VkDescriptorPoolSize renderPoolSizes[2] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
    };
    VkDescriptorPoolCreateInfo renderPoolInfo{};
    renderPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    renderPoolInfo.poolSizeCount = 2;
    renderPoolInfo.pPoolSizes = renderPoolSizes;
    renderPoolInfo.maxSets = 1;
    if (vkCreateDescriptorPool(device_, &renderPoolInfo, nullptr, &r.renderPool) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 render descriptor pool failed");
    VkDescriptorSetAllocateInfo renderSetAlloc{};
    renderSetAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    renderSetAlloc.descriptorPool = r.renderPool;
    renderSetAlloc.descriptorSetCount = 1;
    renderSetAlloc.pSetLayouts = &r.renderSetLayout;
    if (vkAllocateDescriptorSets(device_, &renderSetAlloc, &r.renderSet) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 render descriptor set failed");
    VkDescriptorImageInfo sampledImageInfo{};
    sampledImageInfo.sampler = r.densitySampler;
    sampledImageInfo.imageView = r.densityImageView;
    sampledImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkDescriptorImageInfo sampledWhitewaterInfo{};
    sampledWhitewaterInfo.sampler = r.densitySampler;
    sampledWhitewaterInfo.imageView = r.whitewaterImageView;
    sampledWhitewaterInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkWriteDescriptorSet renderWrites[3]{};
    renderWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    renderWrites[0].dstSet = r.renderSet;
    renderWrites[0].dstBinding = 2;
    renderWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    renderWrites[0].descriptorCount = 1;
    renderWrites[0].pImageInfo = &sampledImageInfo;
    renderWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    renderWrites[1].dstSet = r.renderSet;
    renderWrites[1].dstBinding = 3;
    renderWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    renderWrites[1].descriptorCount = 1;
    renderWrites[1].pBufferInfo = &bodyInfo;
    renderWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    renderWrites[2].dstSet = r.renderSet;
    renderWrites[2].dstBinding = 4;
    renderWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    renderWrites[2].descriptorCount = 1;
    renderWrites[2].pImageInfo = &sampledWhitewaterInfo;
    vkUpdateDescriptorSets(device_, 3, renderWrites, 0, nullptr);

    VkPushConstantRange renderPushRange{};
    renderPushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    renderPushRange.size = sizeof(CinematicLiquidV2RenderPushConstants);
    VkPipelineLayoutCreateInfo renderPipelineLayoutInfo{};
    renderPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    renderPipelineLayoutInfo.setLayoutCount = 1;
    renderPipelineLayoutInfo.pSetLayouts = &r.renderSetLayout;
    renderPipelineLayoutInfo.pushConstantRangeCount = 1;
    renderPipelineLayoutInfo.pPushConstantRanges = &renderPushRange;
    if (vkCreatePipelineLayout(device_, &renderPipelineLayoutInfo, nullptr,
                               &r.renderLayout) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 render pipeline layout failed");

    const auto vertexCode = ReadFileBytes(shaderDir_ + "cinematic_liquid_render_v2.vert.spv");
    const auto fragmentCode = ReadFileBytes(shaderDir_ + "cinematic_liquid_render_v2.frag.spv");
    VkShaderModule vertexModule = CreateShaderModule(vertexCode);
    VkShaderModule fragmentModule = CreateShaderModule(fragmentCode);
    VkPipelineShaderStageCreateInfo renderStages[2]{};
    renderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    renderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    renderStages[0].module = vertexModule;
    renderStages[0].pName = "main";
    renderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    renderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    renderStages[1].module = fragmentModule;
    renderStages[1].pName = "main";
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport viewport{0.0f, 0.0f, float(swapChainExtent_.width),
                        float(swapChainExtent_.height), 0.0f, 1.0f};
    VkRect2D scissor{{0, 0}, swapChainExtent_};
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;
    VkGraphicsPipelineCreateInfo renderPipelineInfo{};
    renderPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    renderPipelineInfo.stageCount = 2;
    renderPipelineInfo.pStages = renderStages;
    renderPipelineInfo.pVertexInputState = &vertexInput;
    renderPipelineInfo.pInputAssemblyState = &assembly;
    renderPipelineInfo.pViewportState = &viewportState;
    renderPipelineInfo.pRasterizationState = &raster;
    renderPipelineInfo.pMultisampleState = &multisample;
    renderPipelineInfo.pColorBlendState = &blend;
    renderPipelineInfo.layout = r.renderLayout;
    renderPipelineInfo.renderPass = renderPass_;
    renderPipelineInfo.subpass = 0;
    const VkResult renderResult = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1,
        &renderPipelineInfo, nullptr, &r.renderPipe);
    vkDestroyShaderModule(device_, fragmentModule, nullptr);
    vkDestroyShaderModule(device_, vertexModule, nullptr);
    if (renderResult != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid v2 raymarch pipeline failed");

    graphicsPipeline_ = r.renderPipe;
    graphicsPipelineLayout_ = r.renderLayout;
    if (r.isSph) {
        CreateCinematicLiquidSphResources(sphInitial);
        std::cout << "[Cinematic Liquid SPH] dual-density SPH " << r.particleCount
                  << " particles, h " << kCinematicLiquidSphSmoothingRadius
                  << " (sim units), counting-sort neighbours, "
                  << r.bodyCount << " coupled bodies, particle-splat surface "
                  << r.surfaceX << "x" << r.surfaceY << "x" << r.surfaceZ
                  << ", raymarch " << (linearFilter ? "linear" : "nearest")
                  << " filtering\n";
        return;
    }
    std::cout << "[Cinematic Liquid v2] MLS-MPM " << r.particleCount
              << " particles, grid " << r.gridX << "x" << r.gridY << "x" << r.gridZ
              << ", particle-splat surface " << r.surfaceX << "x"
              << r.surfaceY << "x" << r.surfaceZ
              << ", " << r.bodyCount << " coupled bodies, whitewater raymarch "
              << (linearFilter ? "linear" : "nearest") << " filtering\n";
}

void VulkanBackend::CleanupCinematicLiquidResources() {
    if (device_ == VK_NULL_HANDLE) return;
    auto& r = cinematicLiquid_;
    if (graphicsPipeline_ == r.renderPipe) graphicsPipeline_ = VK_NULL_HANDLE;
    if (graphicsPipelineLayout_ == r.renderLayout) graphicsPipelineLayout_ = VK_NULL_HANDLE;

    auto destroyPipeline = [&](VkPipeline& pipeline) {
        if (pipeline) {
            vkDestroyPipeline(device_, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
    };
    destroyPipeline(r.clearGridPipe);
    destroyPipeline(r.p2gMassPipe);
    destroyPipeline(r.p2gStressPipe);
    destroyPipeline(r.gridUpdatePipe);
    destroyPipeline(r.g2pPipe);
    destroyPipeline(r.rigidIntegratePipe);
    destroyPipeline(r.resolveDensityPipe);
    destroyPipeline(r.surfaceClearPipe);
    destroyPipeline(r.surfaceSplatPipe);
    destroyPipeline(r.surfaceResolvePipe);
    destroyPipeline(r.renderPipe);
    destroyPipeline(r.sphExternalPipe);
    destroyPipeline(r.sphHashCountPipe);
    destroyPipeline(r.sphScanBlockPipe);
    destroyPipeline(r.sphScanAddPipe);
    destroyPipeline(r.sphScatterPipe);
    destroyPipeline(r.sphDensityPipe);
    destroyPipeline(r.sphPressurePipe);
    destroyPipeline(r.sphViscosityPipe);
    destroyPipeline(r.sphIntegratePipe);
    if (r.sphLayout) {
        vkDestroyPipelineLayout(device_, r.sphLayout, nullptr);
        r.sphLayout = VK_NULL_HANDLE;
    }
    if (r.sphPool) {
        vkDestroyDescriptorPool(device_, r.sphPool, nullptr);
        r.sphPool = VK_NULL_HANDLE;
    }
    if (r.sphSetLayout) {
        vkDestroyDescriptorSetLayout(device_, r.sphSetLayout, nullptr);
        r.sphSetLayout = VK_NULL_HANDLE;
    }
    if (r.computeLayout) {
        vkDestroyPipelineLayout(device_, r.computeLayout, nullptr);
        r.computeLayout = VK_NULL_HANDLE;
    }
    if (r.renderLayout) {
        vkDestroyPipelineLayout(device_, r.renderLayout, nullptr);
        r.renderLayout = VK_NULL_HANDLE;
    }
    if (r.surfaceLayout) {
        vkDestroyPipelineLayout(device_, r.surfaceLayout, nullptr);
        r.surfaceLayout = VK_NULL_HANDLE;
    }
    if (r.computePool) {
        vkDestroyDescriptorPool(device_, r.computePool, nullptr);
        r.computePool = VK_NULL_HANDLE;
    }
    if (r.renderPool) {
        vkDestroyDescriptorPool(device_, r.renderPool, nullptr);
        r.renderPool = VK_NULL_HANDLE;
    }
    if (r.surfacePool) {
        vkDestroyDescriptorPool(device_, r.surfacePool, nullptr);
        r.surfacePool = VK_NULL_HANDLE;
    }
    if (r.computeSetLayout) {
        vkDestroyDescriptorSetLayout(device_, r.computeSetLayout, nullptr);
        r.computeSetLayout = VK_NULL_HANDLE;
    }
    if (r.renderSetLayout) {
        vkDestroyDescriptorSetLayout(device_, r.renderSetLayout, nullptr);
        r.renderSetLayout = VK_NULL_HANDLE;
    }
    if (r.surfaceSetLayout) {
        vkDestroyDescriptorSetLayout(device_, r.surfaceSetLayout, nullptr);
        r.surfaceSetLayout = VK_NULL_HANDLE;
    }
    if (r.densitySampler) {
        vkDestroySampler(device_, r.densitySampler, nullptr);
        r.densitySampler = VK_NULL_HANDLE;
    }
    if (r.densityImageView) {
        vkDestroyImageView(device_, r.densityImageView, nullptr);
        r.densityImageView = VK_NULL_HANDLE;
    }
    if (r.densityImage) {
        vkDestroyImage(device_, r.densityImage, nullptr);
        r.densityImage = VK_NULL_HANDLE;
    }
    if (r.densityImageMem) {
        vkFreeMemory(device_, r.densityImageMem, nullptr);
        r.densityImageMem = VK_NULL_HANDLE;
    }
    if (r.whitewaterImageView) {
        vkDestroyImageView(device_, r.whitewaterImageView, nullptr);
        r.whitewaterImageView = VK_NULL_HANDLE;
    }
    if (r.whitewaterImage) {
        vkDestroyImage(device_, r.whitewaterImage, nullptr);
        r.whitewaterImage = VK_NULL_HANDLE;
    }
    if (r.whitewaterImageMem) {
        vkFreeMemory(device_, r.whitewaterImageMem, nullptr);
        r.whitewaterImageMem = VK_NULL_HANDLE;
    }
    auto destroyBuffer = [&](VkBuffer& buffer, VkDeviceMemory& memory) {
        if (buffer) {
            vkDestroyBuffer(device_, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }
        if (memory) {
            vkFreeMemory(device_, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    };
    destroyBuffer(r.particles, r.particlesMem);
    destroyBuffer(r.seedParticles, r.seedParticlesMem);
    destroyBuffer(r.grid, r.gridMem);
    destroyBuffer(r.bodies, r.bodiesMem);
    destroyBuffer(r.seedBodies, r.seedBodiesMem);
    destroyBuffer(r.bodyImpulses, r.bodyImpulsesMem);
    destroyBuffer(r.densityAtomicBuffer, r.densityAtomicBufferMem);
    destroyBuffer(r.sphPositions, r.sphPositionsMem);
    destroyBuffer(r.sphVelocities, r.sphVelocitiesMem);
    destroyBuffer(r.sphPredicted, r.sphPredictedMem);
    destroyBuffer(r.sphDensities, r.sphDensitiesMem);
    destroyBuffer(r.sphKeys, r.sphKeysMem);
    destroyBuffer(r.sphCellCounts, r.sphCellCountsMem);
    destroyBuffer(r.sphCellStarts, r.sphCellStartsMem);
    destroyBuffer(r.sphCellCursor, r.sphCellCursorMem);
    destroyBuffer(r.sphSortedIndices, r.sphSortedIndicesMem);
    destroyBuffer(r.sphScanSums, r.sphScanSumsMem);
    destroyBuffer(r.sphScanSums2, r.sphScanSums2Mem);
    destroyBuffer(r.sphSeedPositions, r.sphSeedPositionsMem);
}

void VulkanBackend::RecordCinematicLiquidV2Frame(VkCommandBuffer cmd, float deltaTime,
                                                  std::uint32_t imageIndex,
                                                  std::uint32_t timestampBase) {
    auto& r = cinematicLiquid_;
    const std::uint32_t gridCellCount = r.gridX * r.gridY * r.gridZ;
    const std::uint32_t particleGroups = (r.particleCount + 255u) / 256u;
    const std::uint32_t gridClearGroups = (gridCellCount + 255u) / 256u;
    const float wallDt = std::max(deltaTime, 0.0f);
    // Ten substeps keep a 30 Hz wall-clock frame stable while preserving the
    // same dispatch count. This prevents the 5s choreography from running at
    // half speed on GPUs that render between 30 and 60 FPS.
    const float frameDt = std::clamp(wallDt, 0.0f, 1.0f / 30.0f);
    const float substepDt = std::max(frameDt / float(r.substeps), 1e-6f);
    // Keep capture choreography on real elapsed time, not on the stability-
    // clamped simulation clock.  This clock is never reset with particles.
    r.presentationTime += wallDt;
    r.simTime += frameDt;

    CinematicLiquidV2PushConstants pc{};
    FillCinematicLiquidV2ComputePush(pc, r.gridX, r.gridY, r.gridZ,
                                     r.particleCount, substepDt, r.particleMass,
                                     r.dx, r.bodyCount, r.shaderVersion,
                                     r.presentationTime);

    BeginDebugLabel(cmd, "Cinematic Liquid v2: Coupled MLS-MPM", 0.02f, 0.62f, 0.82f);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.computeLayout,
                            0, 1, &r.computeSet, 0, nullptr);
    vkCmdPushConstants(cmd, r.computeLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(pc), &pc);

    auto bufferBarrier = [&](VkBuffer buffer,
                             VkAccessFlags srcAccess = VK_ACCESS_SHADER_WRITE_BIT,
                             VkAccessFlags dstAccess = VK_ACCESS_SHADER_READ_BIT |
                                                       VK_ACCESS_SHADER_WRITE_BIT) {
        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = srcAccess;
        barrier.dstAccessMask = dstAccess;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = buffer;
        barrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                             0, nullptr, 1, &barrier, 0, nullptr);
    };

    // Complete the previous frame's fragment reads before rigid integration
    // can update body transforms for this frame.
    VkBufferMemoryBarrier bodyFromRender{};
    bodyFromRender.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bodyFromRender.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    bodyFromRender.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    bodyFromRender.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bodyFromRender.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bodyFromRender.buffer = r.bodies;
    bodyFromRender.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                         0, nullptr, 1, &bodyFromRender, 0, nullptr);

    // Stage one fresh, high-potential-energy dam break immediately before the
    // formal 5 s capture.  Holding only a vertical gate was insufficient: the
    // column first collapsed downward and had already become a deep, calm
    // reservoir by the time the gate opened.  Reset particles and coupled
    // bodies together so the wave/toy interaction remains deterministic.
    if (!r.captureChoreographyResetDone && r.presentationTime >= 4.0f) {
        VkBufferMemoryBarrier toTransfer[3]{};
        const VkBuffer resetTargets[3] = {r.particles, r.bodies, r.bodyImpulses};
        for (std::uint32_t i = 0; i < 3; ++i) {
            toTransfer[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            toTransfer[i].srcAccessMask = VK_ACCESS_SHADER_READ_BIT |
                                           VK_ACCESS_SHADER_WRITE_BIT;
            toTransfer[i].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransfer[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransfer[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransfer[i].buffer = resetTargets[i];
            toTransfer[i].size = VK_WHOLE_SIZE;
        }
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr, 3, toTransfer, 0, nullptr);

        const VkDeviceSize particleBytes =
            VkDeviceSize(r.particleCount) * sizeof(MlsMpmParticleGpu);
        const VkDeviceSize bodyBytes =
            VkDeviceSize(r.bodyCount) * sizeof(CinematicLiquidBodyStateGpu);
        VkBufferCopy particleCopy{0, 0, particleBytes};
        VkBufferCopy bodyCopy{0, 0, bodyBytes};
        vkCmdCopyBuffer(cmd, r.seedParticles, r.particles, 1, &particleCopy);
        vkCmdCopyBuffer(cmd, r.seedBodies, r.bodies, 1, &bodyCopy);
        vkCmdFillBuffer(cmd, r.bodyImpulses, 0, VK_WHOLE_SIZE, 0);

        VkBufferMemoryBarrier fromTransfer[3]{};
        for (std::uint32_t i = 0; i < 3; ++i) {
            fromTransfer[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            fromTransfer[i].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            fromTransfer[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                                             VK_ACCESS_SHADER_WRITE_BIT;
            fromTransfer[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            fromTransfer[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            fromTransfer[i].buffer = resetTargets[i];
            fromTransfer[i].size = VK_WHOLE_SIZE;
        }
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                             0, nullptr, 3, fromTransfer, 0, nullptr);
        r.simTime = 0.0f;
        r.captureChoreographyResetDone = true;
    }

    for (std::uint32_t substep = 0; substep < r.substeps; ++substep) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.clearGridPipe);
        vkCmdDispatch(cmd, gridClearGroups, 1, 1);
        bufferBarrier(r.grid);
        bufferBarrier(r.bodyImpulses);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.p2gMassPipe);
        vkCmdDispatch(cmd, particleGroups, 1, 1);
        bufferBarrier(r.grid);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.p2gStressPipe);
        vkCmdDispatch(cmd, particleGroups, 1, 1);
        bufferBarrier(r.grid);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.gridUpdatePipe);
        vkCmdDispatch(cmd, (r.gridX + 7u) / 8u,
                      (r.gridY + 7u) / 8u, (r.gridZ + 3u) / 4u);
        bufferBarrier(r.grid);
        bufferBarrier(r.bodyImpulses);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.g2pPipe);
        vkCmdDispatch(cmd, particleGroups, 1, 1);
        bufferBarrier(r.particles);
        bufferBarrier(r.bodyImpulses);

        // One 32-lane workgroup owns every rigid state, so all pairwise
        // contacts (21 pairs with the seven-body duck-family scene) are
        // resolved from a shared snapshot without cross-invocation races.
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.rigidIntegratePipe);
        vkCmdDispatch(cmd, 1, 1, 1);
        bufferBarrier(r.bodies);
        bufferBarrier(r.bodyImpulses);
    }

    VkImageMemoryBarrier beforeResolve[2]{};
    for (std::uint32_t i = 0; i < 2; ++i) {
        beforeResolve[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        beforeResolve[i].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        beforeResolve[i].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        beforeResolve[i].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        beforeResolve[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        beforeResolve[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        beforeResolve[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        beforeResolve[i].subresourceRange =
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    }
    beforeResolve[0].image = r.densityImage;
    beforeResolve[1].image = r.whitewaterImage;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 2, beforeResolve);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.resolveDensityPipe);
    vkCmdDispatch(cmd, (r.gridX + 7u) / 8u,
                  (r.gridY + 7u) / 8u, (r.gridZ + 3u) / 4u);

    const float surfaceVoxelSize =
        (float(r.gridX) * r.dx) / float(r.surfaceX);
    CinematicLiquidV2SurfacePushConstants surfacePc{};
    FillCinematicLiquidV2SurfacePush(surfacePc, r.surfaceX, r.surfaceY,
                                     r.surfaceZ, r.particleCount,
                                     surfaceVoxelSize, r.particleSpacing,
                                     r.particleMass, r.shaderVersion);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            r.surfaceLayout, 0, 1, &r.surfaceSet, 0, nullptr);
    vkCmdPushConstants(cmd, r.surfaceLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(surfacePc), &surfacePc);
    bufferBarrier(r.densityAtomicBuffer, VK_ACCESS_SHADER_READ_BIT,
                  VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.surfaceClearPipe);
    vkCmdDispatch(cmd, (r.surfaceX + 7u) / 8u,
                  (r.surfaceY + 7u) / 8u, (r.surfaceZ + 3u) / 4u);
    bufferBarrier(r.densityAtomicBuffer);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.surfaceSplatPipe);
    vkCmdDispatch(cmd, particleGroups, 1, 1);
    bufferBarrier(r.densityAtomicBuffer);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.surfaceResolvePipe);
    vkCmdDispatch(cmd, (r.surfaceX + 7u) / 8u,
                  (r.surfaceY + 7u) / 8u, (r.surfaceZ + 3u) / 4u);

    VkImageMemoryBarrier afterResolve[2]{};
    for (std::uint32_t i = 0; i < 2; ++i) {
        afterResolve[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        afterResolve[i].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        afterResolve[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        afterResolve[i].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        afterResolve[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        afterResolve[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        afterResolve[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        afterResolve[i].subresourceRange =
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    }
    afterResolve[0].image = r.densityImage;
    afterResolve[1].image = r.whitewaterImage;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 2, afterResolve);
    VkBufferMemoryBarrier bodyToRender{};
    bodyToRender.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bodyToRender.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bodyToRender.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    bodyToRender.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bodyToRender.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bodyToRender.buffer = r.bodies;
    bodyToRender.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr, 1, &bodyToRender, 0, nullptr);
    EndDebugLabel(cmd);

    if (timestampsSupported_) {
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            timestampQueryPool_, timestampBase + 1);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            timestampQueryPool_, timestampBase + 2);
    }

    BeginDebugLabel(cmd, "Cinematic Liquid v2: Pool Raymarch", 0.03f, 0.36f, 0.94f);
    VkClearValue clear{};
    clear.color = {{0.045f, 0.16f, 0.36f, 1.0f}};
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = swapChainFramebuffers_[imageIndex];
    renderPassInfo.renderArea.extent = swapChainExtent_;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.renderPipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.renderLayout,
                            0, 1, &r.renderSet, 0, nullptr);

    const bool swapchainIsSrgb =
        swapChainImageFormat_ == VK_FORMAT_B8G8R8A8_SRGB ||
        swapChainImageFormat_ == VK_FORMAT_R8G8B8A8_SRGB ||
        swapChainImageFormat_ == VK_FORMAT_A8B8G8R8_SRGB_PACK32;
    CinematicLiquidV2RenderPushConstants renderPc{};
    FillCinematicLiquidV2RenderPush(
        renderPc, r.presentationTime,
        float(swapChainExtent_.width) /
            float(std::max(swapChainExtent_.height, 1u)),
        r.gridX, r.gridY, r.gridZ, r.dx, r.raySteps, r.shaderVersion,
        swapChainExtent_.width, swapChainExtent_.height, r.bodyCount,
        swapchainIsSrgb);
    vkCmdPushConstants(cmd, r.renderLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(renderPc), &renderPc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    EndDebugLabel(cmd);

    if (timestampsSupported_)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            timestampQueryPool_, timestampBase + 3);
}

void VulkanBackend::CreateCinematicLiquidSphResources(
        const std::vector<float>& simSeed) {
    auto& r = cinematicLiquid_;
    const std::uint32_t particleCount = r.particleCount;
    // Hash table size equals particle count, exactly like the reference.
    r.sphTableSize = particleCount;
    const VkDeviceSize vec4Bytes = VkDeviceSize(particleCount) * 16u;
    const VkDeviceSize vec2Bytes = VkDeviceSize(particleCount) * 8u;
    const VkDeviceSize uintBytes = VkDeviceSize(particleCount) * 4u;
    const std::uint32_t scanBlocks = (particleCount + 255u) / 256u;
    const VkDeviceSize scanSumsBytes =
        VkDeviceSize(std::max(scanBlocks, 256u)) * 4u;

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);
    const VkBufferUsageFlags storageDst =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    CreateFluidBuffer(device_, memProps, vec4Bytes, storageDst,
                      &r.sphPositions, &r.sphPositionsMem);
    CreateFluidBuffer(device_, memProps, vec4Bytes, storageDst,
                      &r.sphVelocities, &r.sphVelocitiesMem);
    CreateFluidBuffer(device_, memProps, vec4Bytes, storageDst,
                      &r.sphPredicted, &r.sphPredictedMem);
    CreateFluidBuffer(device_, memProps, vec2Bytes, storageDst,
                      &r.sphDensities, &r.sphDensitiesMem);
    CreateFluidBuffer(device_, memProps, uintBytes, storageDst,
                      &r.sphKeys, &r.sphKeysMem);
    CreateFluidBuffer(device_, memProps, uintBytes, storageDst,
                      &r.sphCellCounts, &r.sphCellCountsMem);
    CreateFluidBuffer(device_, memProps, uintBytes,
                      storageDst | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      &r.sphCellStarts, &r.sphCellStartsMem);
    CreateFluidBuffer(device_, memProps, uintBytes, storageDst,
                      &r.sphCellCursor, &r.sphCellCursorMem);
    CreateFluidBuffer(device_, memProps, uintBytes, storageDst,
                      &r.sphSortedIndices, &r.sphSortedIndicesMem);
    CreateFluidBuffer(device_, memProps, scanSumsBytes, storageDst,
                      &r.sphScanSums, &r.sphScanSumsMem);
    CreateFluidBuffer(device_, memProps, scanSumsBytes, storageDst,
                      &r.sphScanSums2, &r.sphScanSums2Mem);
    CreateFluidBuffer(device_, memProps, vec4Bytes,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      &r.sphSeedPositions, &r.sphSeedPositionsMem);

    // Upload the sim-space seed and zero every derived buffer.
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    {
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = vec4Bytes;
        info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &info, nullptr, &staging) != VK_SUCCESS)
            throw std::runtime_error("Cinematic Liquid SPH staging failed");
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device_, staging, &req);
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = FindMemoryType(req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (vkAllocateMemory(device_, &alloc, nullptr, &stagingMem) != VK_SUCCESS ||
            vkBindBufferMemory(device_, staging, stagingMem, 0) != VK_SUCCESS)
            throw std::runtime_error("Cinematic Liquid SPH staging alloc failed");
        void* mapped = nullptr;
        if (vkMapMemory(device_, stagingMem, 0, vec4Bytes, 0, &mapped) != VK_SUCCESS)
            throw std::runtime_error("Cinematic Liquid SPH staging map failed");
        std::memcpy(mapped, simSeed.data(),
                    static_cast<std::size_t>(vec4Bytes));
        vkUnmapMemory(device_, stagingMem);
    }
    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = commandPool_;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    VkCommandBuffer uploadCmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device_, &cmdAlloc, &uploadCmd) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid SPH upload alloc failed");
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(uploadCmd, &begin);
    VkBufferCopy seedCopy{0, 0, vec4Bytes};
    vkCmdCopyBuffer(uploadCmd, staging, r.sphPositions, 1, &seedCopy);
    vkCmdCopyBuffer(uploadCmd, staging, r.sphSeedPositions, 1, &seedCopy);
    vkCmdCopyBuffer(uploadCmd, staging, r.sphPredicted, 1, &seedCopy);
    vkCmdFillBuffer(uploadCmd, r.sphVelocities, 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(uploadCmd, r.sphDensities, 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(uploadCmd, r.sphKeys, 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(uploadCmd, r.sphCellCounts, 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(uploadCmd, r.sphCellStarts, 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(uploadCmd, r.sphCellCursor, 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(uploadCmd, r.sphSortedIndices, 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(uploadCmd, r.sphScanSums, 0, VK_WHOLE_SIZE, 0);
    vkCmdFillBuffer(uploadCmd, r.sphScanSums2, 0, VK_WHOLE_SIZE, 0);
    VkMemoryBarrier uploadBarrier{};
    uploadBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    uploadBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    uploadBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                                  VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(uploadCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                         1, &uploadBarrier, 0, nullptr, 0, nullptr);
    vkEndCommandBuffer(uploadCmd);
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &uploadCmd;
    if (vkQueueSubmit(graphicsQueue_, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid SPH upload submit failed");
    vkQueueWaitIdle(graphicsQueue_);
    vkFreeCommandBuffers(device_, commandPool_, 1, &uploadCmd);
    vkDestroyBuffer(device_, staging, nullptr);
    vkFreeMemory(device_, stagingMem, nullptr);

    // One 14-binding storage set shared by all nine SPH pipelines; bindings
    // 11-13 alias the shared particle/body/impulse buffers so presentation
    // and rigid integration stay solver-agnostic.
    VkDescriptorSetLayoutBinding sphBindings[14]{};
    for (std::uint32_t i = 0; i < 14; ++i) {
        sphBindings[i].binding = i;
        sphBindings[i].descriptorCount = 1;
        sphBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        sphBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
    VkDescriptorSetLayoutCreateInfo sphLayoutInfo{};
    sphLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    sphLayoutInfo.bindingCount = 14;
    sphLayoutInfo.pBindings = sphBindings;
    if (vkCreateDescriptorSetLayout(device_, &sphLayoutInfo, nullptr,
                                    &r.sphSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid SPH descriptor layout failed");
    VkDescriptorPoolSize sphPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 14};
    VkDescriptorPoolCreateInfo sphPoolInfo{};
    sphPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    sphPoolInfo.poolSizeCount = 1;
    sphPoolInfo.pPoolSizes = &sphPoolSize;
    sphPoolInfo.maxSets = 1;
    if (vkCreateDescriptorPool(device_, &sphPoolInfo, nullptr,
                               &r.sphPool) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid SPH descriptor pool failed");
    VkDescriptorSetAllocateInfo sphSetAlloc{};
    sphSetAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    sphSetAlloc.descriptorPool = r.sphPool;
    sphSetAlloc.descriptorSetCount = 1;
    sphSetAlloc.pSetLayouts = &r.sphSetLayout;
    if (vkAllocateDescriptorSets(device_, &sphSetAlloc, &r.sphSet) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid SPH descriptor set failed");

    const VkBuffer sphBuffers[14] = {
        r.sphPositions, r.sphVelocities, r.sphPredicted, r.sphDensities,
        r.sphKeys, r.sphCellCounts, r.sphCellStarts, r.sphCellCursor,
        r.sphSortedIndices, r.sphScanSums, r.sphScanSums2,
        r.particles, r.bodies, r.bodyImpulses};
    VkDescriptorBufferInfo sphBufferInfos[14]{};
    VkWriteDescriptorSet sphWrites[14]{};
    for (std::uint32_t i = 0; i < 14; ++i) {
        sphBufferInfos[i] = {sphBuffers[i], 0, VK_WHOLE_SIZE};
        sphWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sphWrites[i].dstSet = r.sphSet;
        sphWrites[i].dstBinding = i;
        sphWrites[i].descriptorCount = 1;
        sphWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        sphWrites[i].pBufferInfo = &sphBufferInfos[i];
    }
    vkUpdateDescriptorSets(device_, 14, sphWrites, 0, nullptr);

    VkPushConstantRange sphPushRange{};
    sphPushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    sphPushRange.size = sizeof(CinematicLiquidSphPushConstants);
    VkPipelineLayoutCreateInfo sphPipelineLayoutInfo{};
    sphPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    sphPipelineLayoutInfo.setLayoutCount = 1;
    sphPipelineLayoutInfo.pSetLayouts = &r.sphSetLayout;
    sphPipelineLayoutInfo.pushConstantRangeCount = 1;
    sphPipelineLayoutInfo.pPushConstantRanges = &sphPushRange;
    if (vkCreatePipelineLayout(device_, &sphPipelineLayoutInfo, nullptr,
                               &r.sphLayout) != VK_SUCCESS)
        throw std::runtime_error("Cinematic Liquid SPH pipeline layout failed");
    auto createSphPipeline = [&](const char* file, VkPipeline* out) {
        const auto code = ReadFileBytes(shaderDir_ + file);
        VkShaderModule module = CreateShaderModule(code);
        VkPipelineShaderStageCreateInfo stage{};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = module;
        stage.pName = "main";
        VkComputePipelineCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        info.stage = stage;
        info.layout = r.sphLayout;
        const VkResult result = vkCreateComputePipelines(
            device_, VK_NULL_HANDLE, 1, &info, nullptr, out);
        vkDestroyShaderModule(device_, module, nullptr);
        if (result != VK_SUCCESS)
            throw std::runtime_error(
                std::string("Cinematic Liquid SPH pipeline failed: ") + file);
    };
    createSphPipeline("cinematic_liquid_sph_external.comp.spv", &r.sphExternalPipe);
    createSphPipeline("cinematic_liquid_sph_hash_count.comp.spv", &r.sphHashCountPipe);
    createSphPipeline("cinematic_liquid_sph_scan_block.comp.spv", &r.sphScanBlockPipe);
    createSphPipeline("cinematic_liquid_sph_scan_add.comp.spv", &r.sphScanAddPipe);
    createSphPipeline("cinematic_liquid_sph_scatter.comp.spv", &r.sphScatterPipe);
    createSphPipeline("cinematic_liquid_sph_density.comp.spv", &r.sphDensityPipe);
    createSphPipeline("cinematic_liquid_sph_pressure.comp.spv", &r.sphPressurePipe);
    createSphPipeline("cinematic_liquid_sph_viscosity.comp.spv", &r.sphViscosityPipe);
    createSphPipeline("cinematic_liquid_sph_integrate.comp.spv", &r.sphIntegratePipe);
}

void VulkanBackend::RecordCinematicLiquidSphFrame(VkCommandBuffer cmd,
                                                  float deltaTime,
                                                  std::uint32_t imageIndex,
                                                  std::uint32_t timestampBase) {
    auto& r = cinematicLiquid_;
    const std::uint32_t particleGroups = (r.particleCount + 255u) / 256u;
    const std::uint32_t gridCellCount = r.gridX * r.gridY * r.gridZ;
    const std::uint32_t gridClearGroups = (gridCellCount + 255u) / 256u;
    const std::uint32_t tableSize = std::max(r.sphTableSize, 1u);
    const std::uint32_t scanBlocksL0 = (tableSize + 255u) / 256u;
    const std::uint32_t scanBlocksL1 = (scanBlocksL0 + 255u) / 256u;
    const float wallDt = std::max(deltaTime, 0.0f);

    // Fixed per-frame advance: exactly kCinematicLiquidSphSubsteps reference
    // ticks per rendered frame, decoupled from wall-clock jitter.  The world
    // time map keeps reference gravity (-10 sim) at -9.81 m/s^2 in the pool.
    const float dtSim = kCinematicLiquidSphDtSim;
    const float worldScale = kCinematicLiquidSphWorldScale;
    const float timeScale = std::sqrt(worldScale *
        (-kCinematicLiquidSphGravitySim) / 9.81f);
    const float dtWorld = dtSim * timeScale;
    r.presentationTime += wallDt;
    r.simTime += dtWorld * float(r.substeps);

    // Reused fixed-point/rigid ABI for the shared rigid-integrate, grid-clear
    // and whitewater-resolve passes (grid stays zeroed: SPH has no P2G).
    CinematicLiquidV2PushConstants pc{};
    FillCinematicLiquidV2ComputePush(pc, r.gridX, r.gridY, r.gridZ,
                                     r.particleCount, dtWorld, r.particleMass,
                                     r.dx, r.bodyCount, r.shaderVersion,
                                     r.presentationTime);

    const float h = kCinematicLiquidSphSmoothingRadius;
    const float pi = 3.14159265358979323846f;
    CinematicLiquidSphPushConstants sph{};
    sph.counts[0] = r.particleCount;
    sph.counts[1] = tableSize;
    sph.counts[2] = 0u;
    sph.counts[3] = 0u;
    sph.sim[0] = dtSim;
    sph.sim[1] = kCinematicLiquidSphGravitySim;
    sph.sim[2] = h;
    sph.sim[3] = kCinematicLiquidSphCollisionDamping;
    sph.fluid[0] = kCinematicLiquidSphTargetDensity;
    sph.fluid[1] = kCinematicLiquidSphPressureMultiplier;
    sph.fluid[2] = kCinematicLiquidSphNearPressureMultiplier;
    sph.fluid[3] = kCinematicLiquidSphViscosityStrength;
    sph.kernels[0] = 15.0f / (2.0f * pi * std::pow(h, 5.0f));
    sph.kernels[1] = 15.0f / (pi * std::pow(h, 6.0f));
    sph.kernels[2] = 15.0f / (pi * std::pow(h, 5.0f));
    sph.kernels[3] = 45.0f / (pi * std::pow(h, 6.0f));
    // Sim-space bounds: pool floor (world y = 0) and the physical wall inset.
    sph.boundsMin[0] = 0.5f;
    sph.boundsMin[1] = 0.5625f;
    sph.boundsMin[2] = 0.5f;
    sph.boundsMin[3] = 315.0f / (64.0f * pi * std::pow(h, 9.0f));
    sph.boundsMax[0] = 23.5f;
    sph.boundsMax[1] = 11.5f;
    sph.boundsMax[2] = 17.5f;
    sph.boundsMax[3] = worldScale;
    sph.world[0] = kLiquidV2OriginX;
    sph.world[1] = kLiquidV2OriginY;
    sph.world[2] = kLiquidV2OriginZ;
    sph.world[3] = dtWorld;
    sph.coupling[0] = kLiquidV2BodyImpulseScale;
    sph.coupling[1] = r.particleMass;
    sph.coupling[2] = 0.45f;
    sph.coupling[3] = 0.035f;

    BeginDebugLabel(cmd, "Cinematic Liquid SPH: Dual-Density Solver",
                    0.05f, 0.55f, 0.90f);

    auto bufferBarrier = [&](VkBuffer buffer,
                             VkAccessFlags srcAccess = VK_ACCESS_SHADER_WRITE_BIT,
                             VkAccessFlags dstAccess = VK_ACCESS_SHADER_READ_BIT |
                                                       VK_ACCESS_SHADER_WRITE_BIT) {
        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = srcAccess;
        barrier.dstAccessMask = dstAccess;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = buffer;
        barrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                             0, nullptr, 1, &barrier, 0, nullptr);
    };
    auto computeToTransfer = [&](VkBuffer buffer) {
        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT |
                                VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT |
                                VK_ACCESS_TRANSFER_READ_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = buffer;
        barrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr, 1, &barrier, 0, nullptr);
    };
    auto transferToCompute = [&](VkBuffer buffer) {
        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                                VK_ACCESS_SHADER_WRITE_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = buffer;
        barrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                             0, nullptr, 1, &barrier, 0, nullptr);
    };
    auto pushSph = [&]() {
        vkCmdPushConstants(cmd, r.sphLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(sph), &sph);
    };

    // Complete the previous frame's fragment reads before rigid integration.
    VkBufferMemoryBarrier bodyFromRender{};
    bodyFromRender.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bodyFromRender.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    bodyFromRender.dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                                   VK_ACCESS_SHADER_WRITE_BIT;
    bodyFromRender.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bodyFromRender.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bodyFromRender.buffer = r.bodies;
    bodyFromRender.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                         0, nullptr, 1, &bodyFromRender, 0, nullptr);

    // Restage the dam and every coupled body at 4 s, exactly like v2, so the
    // fixed 5 s capture lands on a fresh, deterministic collapse.
    if (!r.captureChoreographyResetDone && r.presentationTime >= 4.0f) {
        const VkBuffer resetTargets[5] = {r.particles, r.bodies,
                                          r.bodyImpulses, r.sphPositions,
                                          r.sphVelocities};
        VkBufferMemoryBarrier toTransfer[5]{};
        for (std::uint32_t i = 0; i < 5; ++i) {
            toTransfer[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            toTransfer[i].srcAccessMask = VK_ACCESS_SHADER_READ_BIT |
                                          VK_ACCESS_SHADER_WRITE_BIT;
            toTransfer[i].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toTransfer[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransfer[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toTransfer[i].buffer = resetTargets[i];
            toTransfer[i].size = VK_WHOLE_SIZE;
        }
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr, 5, toTransfer, 0, nullptr);

        const VkDeviceSize particleBytes =
            VkDeviceSize(r.particleCount) * sizeof(MlsMpmParticleGpu);
        const VkDeviceSize bodyBytes =
            VkDeviceSize(r.bodyCount) * sizeof(CinematicLiquidBodyStateGpu);
        const VkDeviceSize simBytes = VkDeviceSize(r.particleCount) * 16u;
        VkBufferCopy particleCopy{0, 0, particleBytes};
        VkBufferCopy bodyCopy{0, 0, bodyBytes};
        VkBufferCopy simCopy{0, 0, simBytes};
        vkCmdCopyBuffer(cmd, r.seedParticles, r.particles, 1, &particleCopy);
        vkCmdCopyBuffer(cmd, r.seedBodies, r.bodies, 1, &bodyCopy);
        vkCmdCopyBuffer(cmd, r.sphSeedPositions, r.sphPositions, 1, &simCopy);
        vkCmdFillBuffer(cmd, r.sphVelocities, 0, VK_WHOLE_SIZE, 0);
        vkCmdFillBuffer(cmd, r.bodyImpulses, 0, VK_WHOLE_SIZE, 0);

        VkBufferMemoryBarrier fromTransfer[5]{};
        for (std::uint32_t i = 0; i < 5; ++i) {
            fromTransfer[i].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            fromTransfer[i].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            fromTransfer[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
                                            VK_ACCESS_SHADER_WRITE_BIT;
            fromTransfer[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            fromTransfer[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            fromTransfer[i].buffer = resetTargets[i];
            fromTransfer[i].size = VK_WHOLE_SIZE;
        }
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                             0, nullptr, 5, fromTransfer, 0, nullptr);
        r.simTime = 0.0f;
        r.captureChoreographyResetDone = true;
    }

    for (std::uint32_t substep = 0; substep < r.substeps; ++substep) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                r.sphLayout, 0, 1, &r.sphSet, 0, nullptr);
        sph.counts[2] = 0u;
        sph.counts[3] = 0u;
        pushSph();

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.sphExternalPipe);
        vkCmdDispatch(cmd, particleGroups, 1, 1);
        bufferBarrier(r.sphVelocities);
        bufferBarrier(r.sphPredicted);

        computeToTransfer(r.sphCellCounts);
        vkCmdFillBuffer(cmd, r.sphCellCounts, 0, VK_WHOLE_SIZE, 0);
        transferToCompute(r.sphCellCounts);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.sphHashCountPipe);
        vkCmdDispatch(cmd, particleGroups, 1, 1);
        bufferBarrier(r.sphKeys);
        bufferBarrier(r.sphCellCounts);

        // Three-level exclusive scan of the cell counts.
        sph.counts[2] = tableSize;
        sph.counts[3] = 0u;
        pushSph();
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.sphScanBlockPipe);
        vkCmdDispatch(cmd, scanBlocksL0, 1, 1);
        bufferBarrier(r.sphCellStarts);
        bufferBarrier(r.sphScanSums);
        sph.counts[2] = scanBlocksL0;
        sph.counts[3] = 1u;
        pushSph();
        vkCmdDispatch(cmd, scanBlocksL1, 1, 1);
        bufferBarrier(r.sphScanSums);
        bufferBarrier(r.sphScanSums2);
        sph.counts[2] = scanBlocksL1;
        sph.counts[3] = 2u;
        pushSph();
        vkCmdDispatch(cmd, 1, 1, 1);
        bufferBarrier(r.sphScanSums2);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.sphScanAddPipe);
        sph.counts[2] = scanBlocksL0;
        sph.counts[3] = 1u;
        pushSph();
        vkCmdDispatch(cmd, scanBlocksL1, 1, 1);
        bufferBarrier(r.sphScanSums);
        sph.counts[2] = tableSize;
        sph.counts[3] = 0u;
        pushSph();
        vkCmdDispatch(cmd, scanBlocksL0, 1, 1);
        bufferBarrier(r.sphCellStarts);

        // cursor := starts, then the scatter hands out unique sorted slots.
        computeToTransfer(r.sphCellStarts);
        computeToTransfer(r.sphCellCursor);
        VkBufferCopy cursorCopy{0, 0, VkDeviceSize(tableSize) * 4u};
        vkCmdCopyBuffer(cmd, r.sphCellStarts, r.sphCellCursor, 1, &cursorCopy);
        transferToCompute(r.sphCellCursor);
        transferToCompute(r.sphCellStarts);

        sph.counts[2] = 0u;
        sph.counts[3] = 0u;
        pushSph();
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.sphScatterPipe);
        vkCmdDispatch(cmd, particleGroups, 1, 1);
        bufferBarrier(r.sphSortedIndices);
        bufferBarrier(r.sphCellCursor);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.sphDensityPipe);
        vkCmdDispatch(cmd, particleGroups, 1, 1);
        bufferBarrier(r.sphDensities);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.sphPressurePipe);
        vkCmdDispatch(cmd, particleGroups, 1, 1);
        bufferBarrier(r.sphVelocities);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.sphViscosityPipe);
        vkCmdDispatch(cmd, particleGroups, 1, 1);
        bufferBarrier(r.sphVelocities);

        sph.counts[2] = 0u;
        sph.counts[3] = r.bodyCount;
        pushSph();
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.sphIntegratePipe);
        vkCmdDispatch(cmd, particleGroups, 1, 1);
        bufferBarrier(r.sphPositions);
        bufferBarrier(r.sphVelocities);
        bufferBarrier(r.particles);
        bufferBarrier(r.bodyImpulses);

        // Shared rigid integrate consumes the SPH impulse sums unchanged.
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                r.computeLayout, 0, 1, &r.computeSet, 0, nullptr);
        vkCmdPushConstants(cmd, r.computeLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(pc), &pc);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          r.rigidIntegratePipe);
        vkCmdDispatch(cmd, 1, 1, 1);
        bufferBarrier(r.bodies);
        bufferBarrier(r.bodyImpulses);
    }

    // Presentation: identical to v2.  The grid is cleared (never fed by SPH)
    // so the grid-derived whitewater resolves to zero, then the particle
    // splat reconstructs the render density volume from world positions.
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            r.computeLayout, 0, 1, &r.computeSet, 0, nullptr);
    vkCmdPushConstants(cmd, r.computeLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(pc), &pc);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.clearGridPipe);
    vkCmdDispatch(cmd, gridClearGroups, 1, 1);
    bufferBarrier(r.grid);

    VkImageMemoryBarrier beforeResolve[2]{};
    for (std::uint32_t i = 0; i < 2; ++i) {
        beforeResolve[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        beforeResolve[i].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        beforeResolve[i].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        beforeResolve[i].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        beforeResolve[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        beforeResolve[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        beforeResolve[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        beforeResolve[i].subresourceRange =
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    }
    beforeResolve[0].image = r.densityImage;
    beforeResolve[1].image = r.whitewaterImage;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 2, beforeResolve);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.resolveDensityPipe);
    vkCmdDispatch(cmd, (r.gridX + 7u) / 8u,
                  (r.gridY + 7u) / 8u, (r.gridZ + 3u) / 4u);

    const float surfaceVoxelSize =
        (float(r.gridX) * r.dx) / float(r.surfaceX);
    CinematicLiquidV2SurfacePushConstants surfacePc{};
    FillCinematicLiquidV2SurfacePush(surfacePc, r.surfaceX, r.surfaceY,
                                     r.surfaceZ, r.particleCount,
                                     surfaceVoxelSize, r.particleSpacing,
                                     r.particleMass, r.shaderVersion);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            r.surfaceLayout, 0, 1, &r.surfaceSet, 0, nullptr);
    vkCmdPushConstants(cmd, r.surfaceLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(surfacePc), &surfacePc);
    bufferBarrier(r.densityAtomicBuffer, VK_ACCESS_SHADER_READ_BIT,
                  VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.surfaceClearPipe);
    vkCmdDispatch(cmd, (r.surfaceX + 7u) / 8u,
                  (r.surfaceY + 7u) / 8u, (r.surfaceZ + 3u) / 4u);
    bufferBarrier(r.densityAtomicBuffer);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.surfaceSplatPipe);
    vkCmdDispatch(cmd, particleGroups, 1, 1);
    bufferBarrier(r.densityAtomicBuffer);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.surfaceResolvePipe);
    vkCmdDispatch(cmd, (r.surfaceX + 7u) / 8u,
                  (r.surfaceY + 7u) / 8u, (r.surfaceZ + 3u) / 4u);

    VkImageMemoryBarrier afterResolve[2]{};
    for (std::uint32_t i = 0; i < 2; ++i) {
        afterResolve[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        afterResolve[i].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        afterResolve[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        afterResolve[i].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        afterResolve[i].newLayout = VK_IMAGE_LAYOUT_GENERAL;
        afterResolve[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        afterResolve[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        afterResolve[i].subresourceRange =
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    }
    afterResolve[0].image = r.densityImage;
    afterResolve[1].image = r.whitewaterImage;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 2, afterResolve);
    VkBufferMemoryBarrier bodyToRender{};
    bodyToRender.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bodyToRender.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bodyToRender.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    bodyToRender.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bodyToRender.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bodyToRender.buffer = r.bodies;
    bodyToRender.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr, 1, &bodyToRender, 0, nullptr);
    EndDebugLabel(cmd);

    if (timestampsSupported_) {
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            timestampQueryPool_, timestampBase + 1);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            timestampQueryPool_, timestampBase + 2);
    }

    // Camera path and raymarch identical to v2 so the SPH slice can be
    // compared frame-for-frame against the MLS-MPM captures.
    BeginDebugLabel(cmd, "Cinematic Liquid SPH: Pool Raymarch", 0.03f, 0.36f, 0.94f);
    VkClearValue clear{};
    clear.color = {{0.045f, 0.16f, 0.36f, 1.0f}};
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = swapChainFramebuffers_[imageIndex];
    renderPassInfo.renderArea.extent = swapChainExtent_;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.renderPipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.renderLayout,
                            0, 1, &r.renderSet, 0, nullptr);

    const bool swapchainIsSrgb =
        swapChainImageFormat_ == VK_FORMAT_B8G8R8A8_SRGB ||
        swapChainImageFormat_ == VK_FORMAT_R8G8B8A8_SRGB ||
        swapChainImageFormat_ == VK_FORMAT_A8B8G8R8_SRGB_PACK32;
    CinematicLiquidV2RenderPushConstants renderPc{};
    FillCinematicLiquidV2RenderPush(
        renderPc, r.presentationTime,
        float(swapChainExtent_.width) /
            float(std::max(swapChainExtent_.height, 1u)),
        r.gridX, r.gridY, r.gridZ, r.dx, r.raySteps, r.shaderVersion,
        swapChainExtent_.width, swapChainExtent_.height, r.bodyCount,
        swapchainIsSrgb);
    vkCmdPushConstants(cmd, r.renderLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(renderPc), &renderPc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    EndDebugLabel(cmd);

    if (timestampsSupported_)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            timestampQueryPool_, timestampBase + 3);
}

void VulkanBackend::RecordCinematicLiquidFrame(VkCommandBuffer cmd, float deltaTime,
                                                std::uint32_t imageIndex,
                                                std::uint32_t timestampBase) {
    auto& r = cinematicLiquid_;
    const std::uint32_t gridCellCount = r.gridX * r.gridY * r.gridZ;
    const std::uint32_t particleGroups = (r.particleCount + 255u) / 256u;
    const std::uint32_t gridClearGroups = (gridCellCount + 255u) / 256u;
    const float frameDt = std::clamp(deltaTime, 0.0f, 1.0f / 60.0f);
    const float substepDt = std::max(frameDt / float(kCinematicLiquidSubsteps), 1e-6f);

    BeginDebugLabel(cmd, "Cinematic Liquid: MLS-MPM", 0.03f, 0.58f, 0.78f);

    // The first short loop deliberately re-stages the dam break at 3.55 s so
    // the product's fixed 5 s RenderDoc capture lands on the most informative
    // collision/spray phase. Subsequent 11.5 s loops keep an eventual long
    // run visually active without changing the fixed per-frame workload.
    const float resetAt = r.captureChoreographyResetDone ? 11.5f : 3.55f;
    if (r.simTime >= resetAt) {
        const VkDeviceSize particleBytes = VkDeviceSize(r.particleCount) * sizeof(MlsMpmParticleGpu);
        VkBufferCopy copy{ 0, 0, particleBytes };
        vkCmdCopyBuffer(cmd, r.seedParticles, r.particles, 1, &copy);
        VkBufferMemoryBarrier resetBarrier{};
        resetBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        resetBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        resetBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        resetBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        resetBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        resetBarrier.buffer = r.particles;
        resetBarrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                             0, nullptr, 1, &resetBarrier, 0, nullptr);
        r.simTime = 0.0f;
        r.captureChoreographyResetDone = true;
    }
    r.simTime += frameDt;

    MlsMpmPushConstants pc{};
    pc.gridSizeAndCount[0] = r.gridX;
    pc.gridSizeAndCount[1] = r.gridY;
    pc.gridSizeAndCount[2] = r.gridZ;
    pc.gridSizeAndCount[3] = r.particleCount;
    pc.simulation[0] = substepDt;
    pc.simulation[1] = -9.81f;
    pc.simulation[2] = kLiquidRestDensity;
    pc.simulation[3] = 15'000.0f;
    pc.material[0] = 0.50f;
    pc.material[1] = r.particleMass;
    pc.material[2] = kLiquidFixedPointScale;
    pc.material[3] = 2.5f;
    pc.gridOriginDx[0] = kLiquidOriginX;
    pc.gridOriginDx[1] = kLiquidOriginY;
    pc.gridOriginDx[2] = kLiquidOriginZ;
    pc.gridOriginDx[3] = r.dx;
    pc.sphere[0] = kLiquidSphereX;
    pc.sphere[1] = kLiquidSphereY;
    pc.sphere[2] = kLiquidSphereZ;
    pc.sphere[3] = kLiquidSphereRadius;
    pc.collision[0] = 0.0f;
    pc.collision[1] = 0.05f;
    pc.collision[2] = 0.05f;
    pc.collision[3] = 12.0f;

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.computeLayout,
                            0, 1, &r.computeSet, 0, nullptr);
    vkCmdPushConstants(cmd, r.computeLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(pc), &pc);

    auto computeBufferBarrier = [&](VkBuffer buffer) {
        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = buffer;
        barrier.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                             0, nullptr, 1, &barrier, 0, nullptr);
    };

    for (std::uint32_t substep = 0; substep < kCinematicLiquidSubsteps; ++substep) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.clearGridPipe);
        vkCmdDispatch(cmd, gridClearGroups, 1, 1);
        computeBufferBarrier(r.grid);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.p2gMassPipe);
        vkCmdDispatch(cmd, particleGroups, 1, 1);
        computeBufferBarrier(r.grid);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.p2gStressPipe);
        vkCmdDispatch(cmd, particleGroups, 1, 1);
        computeBufferBarrier(r.grid);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.gridUpdatePipe);
        vkCmdDispatch(cmd, (r.gridX + 7u) / 8u,
                      (r.gridY + 7u) / 8u, (r.gridZ + 3u) / 4u);
        computeBufferBarrier(r.grid);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.g2pPipe);
        vkCmdDispatch(cmd, particleGroups, 1, 1);
        computeBufferBarrier(r.particles);
    }

    // The previous frame sampled this GENERAL-layout image in the fragment
    // stage.  Make that read complete before overwriting it with the current
    // density resolve, then publish the new voxels to this frame's raymarch.
    VkImageMemoryBarrier beforeResolve{};
    beforeResolve.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    beforeResolve.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    beforeResolve.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    beforeResolve.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    beforeResolve.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    beforeResolve.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    beforeResolve.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    beforeResolve.image = r.densityImage;
    beforeResolve.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &beforeResolve);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r.resolveDensityPipe);
    vkCmdDispatch(cmd, (r.gridX + 7u) / 8u,
                  (r.gridY + 7u) / 8u, (r.gridZ + 3u) / 4u);

    VkImageMemoryBarrier afterResolve{};
    afterResolve.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    afterResolve.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    afterResolve.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    afterResolve.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    afterResolve.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    afterResolve.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    afterResolve.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    afterResolve.image = r.densityImage;
    afterResolve.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &afterResolve);
    EndDebugLabel(cmd);

    if (timestampsSupported_) {
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            timestampQueryPool_, timestampBase + 1);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            timestampQueryPool_, timestampBase + 2);
    }

    BeginDebugLabel(cmd, "Cinematic Liquid: Density Raymarch", 0.04f, 0.35f, 0.92f);
    VkClearValue clear{};
    clear.color = {{0.055f, 0.18f, 0.42f, 1.0f}};
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = swapChainFramebuffers_[imageIndex];
    renderPassInfo.renderArea.extent = swapChainExtent_;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.renderPipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.renderLayout,
                            0, 1, &r.renderSet, 0, nullptr);

    CinematicLiquidRenderPushConstants renderPc{};
    renderPc.cameraTime[0] = 2.75f;
    renderPc.cameraTime[1] = 1.48f;
    renderPc.cameraTime[2] = 2.95f;
    renderPc.cameraTime[3] = r.simTime;
    renderPc.targetAspect[0] = 0.0f;
    renderPc.targetAspect[1] = 0.66f;
    renderPc.targetAspect[2] = 0.0f;
    renderPc.targetAspect[3] = float(swapChainExtent_.width) /
                               float(std::max(swapChainExtent_.height, 1u));
    renderPc.volumeMinIso[0] = kLiquidOriginX;
    renderPc.volumeMinIso[1] = kLiquidOriginY;
    renderPc.volumeMinIso[2] = kLiquidOriginZ;
    renderPc.volumeMinIso[3] = 0.22f;
    renderPc.volumeMaxStep[0] = kLiquidOriginX + float(r.gridX) * r.dx;
    renderPc.volumeMaxStep[1] = kLiquidOriginY + float(r.gridY) * r.dx;
    renderPc.volumeMaxStep[2] = kLiquidOriginZ + float(r.gridZ) * r.dx;
    renderPc.volumeMaxStep[3] = 1.0f;
    renderPc.sphere[0] = kLiquidSphereX;
    renderPc.sphere[1] = kLiquidSphereY;
    renderPc.sphere[2] = kLiquidSphereZ;
    renderPc.sphere[3] = kLiquidSphereRadius;
    renderPc.render[0] = kCinematicLiquidRaySteps;
    renderPc.render[1] = kCinematicLiquidShaderVersion;
    renderPc.render[2] = swapChainExtent_.width;
    renderPc.render[3] = swapChainExtent_.height;
    vkCmdPushConstants(cmd, r.renderLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(renderPc), &renderPc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    EndDebugLabel(cmd);

    if (timestampsSupported_)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            timestampQueryPool_, timestampBase + 3);
}

}  // namespace gpu_bench

#endif  // HAVE_VULKAN
