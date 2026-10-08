// Fills in the Vulkan function pointers when the app's library is loaded; see
// vulkan_loader.h.

#include "vulkan_loader.h"

#include <android/log.h>
#include <dlfcn.h>

#include <cstdint>
#include <cstring>

extern "C" {
#define MANGEKYO_VK(name) PFN_##name name __attribute__((visibility("hidden"))) = nullptr;
#include "vulkan_functions.inc"
#undef MANGEKYO_VK
}

namespace {

// Without a Vulkan library every Vulkan path of the engine ends at its first
// call, which is one of these three.
VKAPI_ATTR VkResult VKAPI_CALL NoCreateInstance(const VkInstanceCreateInfo*,
                                                const VkAllocationCallbacks*, VkInstance*) {
    return VK_ERROR_INCOMPATIBLE_DRIVER;
}

VKAPI_ATTR VkResult VKAPI_CALL NoInstanceExtensions(const char*, std::uint32_t* count,
                                                    VkExtensionProperties*) {
    if (count) *count = 0;
    return VK_ERROR_INCOMPATIBLE_DRIVER;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL NoProcAddr(VkInstance, const char*) {
    return nullptr;
}

struct Entry {
    const char* name;
    void* pointer;  // the address of the pointer of that name
};

__attribute__((constructor)) void LoadVulkan() {
    static const Entry entries[] = {
#define MANGEKYO_VK(name) {#name, &name},
#include "vulkan_functions.inc"
#undef MANGEKYO_VK
    };

    void* library = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    const char* missing = nullptr;
    if (library) {
        for (const Entry& entry : entries) {
            void* symbol = dlsym(library, entry.name);
            std::memcpy(entry.pointer, &symbol, sizeof(symbol));
            if (!symbol && !missing) missing = entry.name;
        }
        if (!missing) return;
    }

    if (library) {
        __android_log_print(ANDROID_LOG_WARN, "Mangekyo",
                            "Vulkan: the system's libvulkan.so has no %s; Vulkan tests are off.", missing);
    } else {
        __android_log_print(ANDROID_LOG_INFO, "Mangekyo",
                            "Vulkan: this system has no libvulkan.so; Vulkan tests are off.");
    }
    for (const Entry& entry : entries) {
        void* none = nullptr;
        std::memcpy(entry.pointer, &none, sizeof(none));
    }
    vkCreateInstance = NoCreateInstance;
    vkEnumerateInstanceExtensionProperties = NoInstanceExtensions;
    vkGetInstanceProcAddr = NoProcAddr;
}

}  // namespace
