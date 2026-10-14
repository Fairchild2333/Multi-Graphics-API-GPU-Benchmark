#include "embedded_capabilities.h"
#include "gles_backend.h"
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <vector>

namespace gpu_bench {
std::string EmbeddedGpuCapabilitiesJson() {
    bool vulkan = false, liquid = false;
    std::uint64_t vkParticles = 0;
    void* library = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (library) {
        const auto proc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library, "vkGetInstanceProcAddr"));
        const auto create = proc ? reinterpret_cast<PFN_vkCreateInstance>(proc(VK_NULL_HANDLE, "vkCreateInstance")) : nullptr;
        VkApplicationInfo app{}; app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; app.apiVersion = VK_API_VERSION_1_0;
        VkInstanceCreateInfo info{}; info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; info.pApplicationInfo = &app;
        VkInstance instance = VK_NULL_HANDLE;
        if (create && create(&info, nullptr, &instance) == VK_SUCCESS) {
            const auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(proc(instance, "vkEnumeratePhysicalDevices"));
            const auto properties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(proc(instance, "vkGetPhysicalDeviceProperties"));
            const auto formats = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(proc(instance, "vkGetPhysicalDeviceFormatProperties"));
            const auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(proc(instance, "vkDestroyInstance"));
            std::uint32_t count = 0;
            if (enumerate && properties && enumerate(instance, &count, nullptr) == VK_SUCCESS && count) {
                std::vector<VkPhysicalDevice> devices(count);
                const auto result = enumerate(instance, &count, devices.data());
                if ((result == VK_SUCCESS || result == VK_INCOMPLETE) && count) {
                    vulkan = true;
                    VkPhysicalDeviceProperties limits{};
                    properties(devices.front(), &limits);
                    if (limits.limits.maxComputeWorkGroupInvocations >= 256 && limits.limits.maxComputeWorkGroupSize[0] >= 256)
                        vkParticles = (std::min)(std::uint64_t(limits.limits.maxStorageBufferRange) / 32,
                            std::uint64_t(limits.limits.maxComputeWorkGroupCount[0]) * 256);
                    if (formats) {
                        VkFormatProperties format{}; formats(devices.front(), VK_FORMAT_R32_SFLOAT, &format);
                        const auto required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
                            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
                        liquid = (format.optimalTilingFeatures & required) == required;
                    }
                }
            }
            if (destroy) destroy(instance, nullptr);
        }
        dlclose(library);
    }
    const auto es = ProbeEsSuiteCapabilities();
    std::ostringstream json;
    json << "{\"vulkan\":" << (vulkan ? "true" : "false")
         << ",\"liquid\":" << (liquid ? "true" : "false")
         << ",\"vulkanMaxParticles\":" << vkParticles
         << ",\"gles31\":" << (es.maxParticles ? "true" : "false")
         << ",\"glesMaxParticles\":" << es.maxParticles
         << ",\"gles2\":" << (es.es2 ? "true" : "false") << "}";
    return json.str();
}
}
