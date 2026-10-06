#pragma once

// Facts about the machine a result was measured on. They go into every saved
// result, so a score can be traced to a device.

#include <cstdint>
#include <string>

namespace gpu_bench {

// The make and model of the machine, such as "Xiaomi 23049PCD8G",
// "HUAWEI Mate 60 Pro (ALN-AL00)", "iPhone16,1", "Mac14,7" or
// "NVIDIA Jetson Xavier NX Developer Kit". A self-built PC, whose firmware has
// only a placeholder for the system, is named by its mainboard, such as
// "ASUSTeK COMPUTER INC. ROG CROSSHAIR X670E HERO". Empty when nothing usable
// is found.
std::string SystemDeviceModel();

// Memory of the machine in MiB; 0 when unknown. Windows reports the installed
// amount (98304 for 2 x 48 GB); the other systems report what the kernel can
// use, which is a few percent less (about 11400 on a phone sold with 12 GB).
std::uint32_t SystemMemoryMB();

// The system-on-chip of a phone or tablet, such as "QTI SM8550". Empty on
// desktops and where the system gives no name; callers then fall back to
// their own processor lookup.
std::string MobileSocName();

// The release of a phone system, such as "Android 14 (API 34)" or
// "HarmonyOS 5.0.0 (API 12)". Empty on every other system.
std::string MobileOsVersion();

}  // namespace gpu_bench
