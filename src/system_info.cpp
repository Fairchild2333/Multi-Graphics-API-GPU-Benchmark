#include "system_info.h"

#include <algorithm>
#include <cctype>
#include <fstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#include <sys/sysctl.h>
#endif

#if defined(__ANDROID__)
#include <sys/system_properties.h>
#elif defined(__OHOS__)
#include <deviceinfo.h>
#endif

namespace gpu_bench {
namespace {

std::string Trim(std::string text) {
    const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0 || c == '\0'; };
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.back()))) text.pop_back();
    std::size_t start = 0;
    while (start < text.size() && isSpace(static_cast<unsigned char>(text[start]))) ++start;
    return text.substr(start);
}

std::string Lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// What board firmware reports when the maker filled nothing in.
[[maybe_unused]] bool IsPlaceholder(const std::string& text) {
    const std::string value = Lower(Trim(text));
    static const char* const kPlaceholders[] = {
        "", "system product name", "system manufacturer", "to be filled by o.e.m.", "default string",
        "not specified", "not applicable", "none", "unknown", "o.e.m.", "oem", "system version",
    };
    for (const char* placeholder : kPlaceholders) {
        if (value == placeholder) return true;
    }
    return false;
}

// "Maker Model", without repeating a maker the model already starts with.
[[maybe_unused]] std::string JoinMakerAndModel(const std::string& makerIn, const std::string& modelIn) {
    const std::string maker = Trim(makerIn);
    const std::string model = Trim(modelIn);
    if (model.empty()) return maker;
    if (maker.empty() || Lower(model).rfind(Lower(maker), 0) == 0) return model;
    return maker + " " + model;
}

// A brand-name PC names itself; a self-built one only has a placeholder there,
// and its mainboard is the best description of the machine.
[[maybe_unused]] std::string NamedSystemOrBoard(const std::string& maker, const std::string& model,
                                               const std::string& boardMaker, const std::string& board) {
    if (!IsPlaceholder(model)) return JoinMakerAndModel(IsPlaceholder(maker) ? std::string() : maker, model);
    if (!IsPlaceholder(board)) {
        return JoinMakerAndModel(IsPlaceholder(boardMaker) ? std::string() : boardMaker, board);
    }
    return {};
}

#if defined(__linux__) && !defined(__ANDROID__) && !defined(__OHOS__)
std::string FirstLine(const char* path) {
    std::ifstream file(path);
    std::string line;
    std::getline(file, line);
    return Trim(line);
}
#endif

#if defined(__linux__)
std::uint32_t MemTotalMB() {
    std::ifstream file("/proc/meminfo");
    std::string key;
    std::uint64_t value = 0;
    std::string unit;
    while (file >> key >> value) {
        std::getline(file, unit);
        if (key == "MemTotal:") return static_cast<std::uint32_t>(value / 1024);
    }
    return 0;
}
#endif

#if defined(__ANDROID__)
std::string Property(const char* name) {
    char value[PROP_VALUE_MAX + 1]{};
    return __system_property_get(name, value) > 0 ? Trim(value) : std::string();
}
#endif

#if defined(__OHOS__)
std::string Text(const char* value) {
    return value ? Trim(value) : std::string();
}
#endif

#if defined(__APPLE__)
std::string SysctlString(const char* name) {
    std::size_t size = 0;
    if (sysctlbyname(name, nullptr, &size, nullptr, 0) != 0 || size <= 1) return {};
    std::string value(size, '\0');
    if (sysctlbyname(name, value.data(), &size, nullptr, 0) != 0) return {};
    return Trim(value);
}
#endif

#ifdef _WIN32
std::string BiosValue(const char* name) {
    char value[256]{};
    DWORD bytes = sizeof(value);
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", name, RRF_RT_REG_SZ,
                     nullptr, value, &bytes) != ERROR_SUCCESS) {
        return {};
    }
    return Trim(value);
}
#endif

}  // namespace

std::string SystemDeviceModel() {
#if defined(_WIN32)
    return NamedSystemOrBoard(BiosValue("SystemManufacturer"), BiosValue("SystemProductName"),
                              BiosValue("BaseBoardManufacturer"), BiosValue("BaseBoardProduct"));
#elif defined(__ANDROID__)
    return JoinMakerAndModel(Property("ro.product.manufacturer"), Property("ro.product.model"));
#elif defined(__OHOS__)
    // The name on the box, with the model code that tells variants apart.
    const std::string name = JoinMakerAndModel(Text(OH_GetBrand()), Text(OH_GetMarketName()));
    const std::string code = Text(OH_GetProductModel());
    if (name.empty()) return code;
    if (code.empty() || name.find(code) != std::string::npos) return name;
    return name + " (" + code + ")";
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
    return SysctlString("hw.machine");  // "iPhone16,1"
#else
    return SysctlString("hw.model");    // "Mac14,7"
#endif
#elif defined(__linux__)
    // ARM boards name themselves in the device tree; PCs in the firmware tables.
    const std::string board = FirstLine("/proc/device-tree/model");
    if (!board.empty()) return board;
    return NamedSystemOrBoard(FirstLine("/sys/devices/virtual/dmi/id/sys_vendor"),
                              FirstLine("/sys/devices/virtual/dmi/id/product_name"),
                              FirstLine("/sys/devices/virtual/dmi/id/board_vendor"),
                              FirstLine("/sys/devices/virtual/dmi/id/board_name"));
#else
    return {};
#endif
}

std::uint32_t SystemMemoryMB() {
#if defined(_WIN32)
    // The installed amount from the firmware tables; the amount Windows can
    // use is a little smaller and is the fallback.
    ULONGLONG installedKB = 0;
    if (GetPhysicallyInstalledSystemMemory(&installedKB) && installedKB > 0) {
        return static_cast<std::uint32_t>(installedKB / 1024);
    }
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) return static_cast<std::uint32_t>(status.ullTotalPhys >> 20);
    return 0;
#elif defined(__APPLE__)
    std::uint64_t bytes = 0;
    std::size_t size = sizeof(bytes);
    if (sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) != 0) return 0;
    return static_cast<std::uint32_t>(bytes >> 20);
#elif defined(__linux__)
    return MemTotalMB();
#else
    return 0;
#endif
}

std::string MobileSocName() {
#if defined(__ANDROID__)
    // Android 12 and later name the chip; older systems only the board platform.
    const std::string model = Property("ro.soc.model");
    if (!model.empty()) return JoinMakerAndModel(Property("ro.soc.manufacturer"), model);
    const std::string chip = Property("ro.hardware.chipname");
    if (!chip.empty()) return chip;
    return Property("ro.board.platform");
#elif defined(__APPLE__) && TARGET_OS_IPHONE
    // iOS gives an app no chip name on most releases; the device identifier,
    // which determines the chip, then stands in for it.
    const std::string brand = SysctlString("machdep.cpu.brand_string");
    if (!brand.empty()) return brand;
    const std::string machine = SysctlString("hw.machine");
    return machine.empty() ? std::string() : "Apple Silicon (" + machine + ")";
#else
    return {};
#endif
}

std::string MobileOsVersion() {
#if defined(__ANDROID__)
    const std::string release = Property("ro.build.version.release");
    if (release.empty()) return {};
    const std::string api = Property("ro.build.version.sdk");
    return "Android " + release + (api.empty() ? std::string() : " (API " + api + ")");
#elif defined(__OHOS__)
    std::string name = Text(OH_GetDistributionOSName());
    std::string version = Text(OH_GetDistributionOSVersion());
    if (name.empty()) name = "HarmonyOS";
    if (version.empty()) version = Text(OH_GetDisplayVersion());
    if (version.empty()) return {};
    return name + " " + version + " (API " + std::to_string(OH_GetSdkApiVersion()) + ")";
#else
    return {};
#endif
}

}  // namespace gpu_bench
