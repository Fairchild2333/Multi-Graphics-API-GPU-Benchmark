#pragma once

// Vulkan through function pointers (Android only).
//
// The app's library used to name libvulkan.so among the libraries it needs.
// Android 6.0 (API 23, the oldest system the app installs on) has no such
// library, so there the whole library failed to load and no test could run,
// the CPU tests included (found 2026-10-08 on a Mi Pad 2). Now nothing is
// linked: every Vulkan function the engine calls is a pointer of the same
// name, filled in from libvulkan.so when the app's library is loaded
// (vulkan_loader.cpp). On a system without Vulkan, creating an instance fails
// with VK_ERROR_INCOMPATIBLE_DRIVER and everything else still works.
//
// This header is put in front of every source file of the engine and of the
// bridge by the build (CMakeLists.txt next to it), so no source file changes.

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

// Hidden: the app's library must not offer symbols with Vulkan's own names to
// anything loaded after it.
#define MANGEKYO_VK(name) extern PFN_##name name __attribute__((visibility("hidden")));
#include "vulkan_functions.inc"
#undef MANGEKYO_VK

#ifdef __cplusplus
}
#endif
