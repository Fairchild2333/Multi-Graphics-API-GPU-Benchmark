#ifdef HAVE_METAL

#include "metal_probe.h"

#import <Metal/Metal.h>
#import <TargetConditionals.h>

namespace gpu_bench {

std::vector<MetalGpuInfo> ProbeMetalDevices() {
    std::vector<MetalGpuInfo> result;

#if TARGET_OS_IPHONE
    id<MTLDevice> systemDevice = MTLCreateSystemDefaultDevice();
    if (!systemDevice) return result;
    MetalGpuInfo info;
    info.name         = [systemDevice.name UTF8String];
    info.vramBytes    = systemDevice.recommendedMaxWorkingSetSize;
    info.isHeadless   = false;
    info.isLowPower   = false;
    info.isRemovable  = false;
    info.registryID   = static_cast<std::uint32_t>(systemDevice.registryID);
    result.push_back(std::move(info));
#else
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
#pragma clang diagnostic pop

    if (!devices) return result;

    for (NSUInteger i = 0; i < devices.count; ++i) {
        id<MTLDevice> dev = devices[i];
        MetalGpuInfo info;
        info.name         = [dev.name UTF8String];
        info.vramBytes    = dev.recommendedMaxWorkingSetSize;
        info.isHeadless   = dev.isHeadless;
        info.isLowPower   = dev.isLowPower;
        info.isRemovable  = dev.isRemovable;
        info.registryID   = static_cast<std::uint32_t>(dev.registryID);
        result.push_back(std::move(info));
    }
#endif

    return result;
}

}  // namespace gpu_bench

#endif  // HAVE_METAL
