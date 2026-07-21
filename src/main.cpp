#include "app_base.h"
#include "benchmark_results.h"
#include "cpu_benchmark.h"
#include "gpu_engine.h"

#if defined(_WIN32) && defined(HAVE_VULKAN)
#include "renderdoc_app.h"
#endif

#ifdef HAVE_VULKAN
#include "vulkan_backend.h"
#endif
#ifdef HAVE_DX12
#include "dx12_backend.h"
#endif
#ifdef HAVE_DX11
#include "dx11_backend.h"
#endif
#ifdef HAVE_METAL
#include "metal_backend.h"
#include "metal_probe.h"
#endif
#ifdef HAVE_OPENGL
#include "opengl_backend.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#if !defined(GPU_BENCH_NO_GLFW)
#include <GLFW/glfw3.h>
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#pragma comment(lib, "dxgi.lib")

// Hint NVIDIA Optimus and AMD Switchable Graphics to prefer the discrete GPU
// for OpenGL contexts.  These must be exported from the final executable.
extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int   AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#ifdef HAVE_VULKAN
#include <vulkan/vulkan.h>
#endif

#ifdef HAVE_VULKAN
// On Windows the release binary delay-loads vulkan-1.dll.  Older systems can
// therefore still launch and use Direct3D even when their display driver did
// not install a Vulkan runtime (for example, Direct3D 10-era adapters).  Keep
// the module loaded once found so the delay-import thunk resolves against the
// same process-local loader.  Other platforms retain their normal link model.
static bool VulkanLoaderAvailable() {
#ifdef _WIN32
    static HMODULE loader = LoadLibraryW(L"vulkan-1.dll");
    return loader != nullptr;
#else
    return true;
#endif
}
#endif

static bool SafeStoi(const std::string& s, int& out) {
    try { out = std::stoi(s); return true; }
    catch (...) { return false; }
}

static bool SafeStod(const std::string& s, double& out) {
    try {
        std::size_t consumed = 0;
        out = std::stod(s, &consumed);
        return consumed == s.size() && std::isfinite(out);
    } catch (...) {
        return false;
    }
}

static bool ParseCpuBenchmarkMode(const std::string& value,
                                  gpu_bench::CpuBenchmarkMode& mode) {
    if (value == "per-core" || value == "per_core" || value == "percore" ||
        value == "single") {
        mode = gpu_bench::CpuBenchmarkMode::PerCore;
        return true;
    }
    if (value == "multi" || value == "multi-core" || value == "multi_core") {
        mode = gpu_bench::CpuBenchmarkMode::MultiCore;
        return true;
    }
    if (value == "all") {
        mode = gpu_bench::CpuBenchmarkMode::All;
        return true;
    }
    return false;
}

static gpu_bench::BenchmarkResult MakeStoredCpuResult(
    const gpu_bench::CpuBenchmarkReport& report,
    const gpu_bench::CpuBenchmarkConfig& config,
    bool multiCore) {
    gpu_bench::BenchmarkResult stored;
    stored.id = gpu_bench::GenerateResultId() +
                (multiCore ? "-cpu-multi" : "-cpu-single");
    stored.timestamp = gpu_bench::GenerateTimestamp();
    stored.resultSchemaVersion = 2;
    stored.workload = multiCore ? "cpu_multi_core" : "cpu_single_core";
    const auto measureMs = static_cast<long long>(std::llround(config.measureSeconds * 1000.0));
    const auto warmupMs = static_cast<long long>(std::llround(config.warmupSeconds * 1000.0));
    const bool formalContract = measureMs == 15000 && warmupMs == 200 &&
                                config.roundCount == 3;
    const bool afterPerCore = multiCore && config.mode == gpu_bench::CpuBenchmarkMode::All;
    std::ostringstream storedVersion;
    storedVersion << report.workloadVersion
                  << (formalContract ? "_formal" : "_preview")
                  << "_r" << config.roundCount
                  << "_t" << measureMs << "ms"
                  << "_w" << warmupMs << "ms"
                  << "_sequence_" << (afterPerCore ? "after_percore" : "standalone");
    stored.workloadVersion = storedVersion.str();
    stored.graphicsApi = "CPU";
    stored.deviceName = report.cpuName;
    stored.cpuName = report.cpuName;
    stored.difficulty = multiCore ? "Multi-core" : "Per-core";
    stored.headless = true;
    stored.framesInFlight = 0;
    stored.scoreUnit = "MWork/s";
    stored.precision = "Mixed";
    stored.bottleneck = "Native mixed CPU kernel";

    std::uint32_t physicalCount = 0;
    for (const auto& cpu : report.processors)
        physicalCount = (std::max)(physicalCount, cpu.physicalCore + 1u);

    std::ostringstream contract;
    contract << "kernel=" << gpu_bench::kCpuBenchmarkWorkloadVersion
             << ";rounds=" << config.roundCount
             << ";totalMeasureSeconds=" << config.measureSeconds
             << ";roundSeconds="
             << (config.measureSeconds / static_cast<double>(config.roundCount))
             << ";warmupSeconds=" << config.warmupSeconds
             << ";topologySource=" << report.topologySource
             << ";affinityCapability=" << report.affinityCapability
             << ";logicalProcessors=" << report.processors.size()
             << ";physicalCores=" << physicalCount
             << ";coreClassLabels=inferred_rank_not_architectural_identity"
             << ";checksumRole=dce_sink";
    contract << ";scoreContract=" << (formalContract ? "formal" : "preview")
             << ";sequence=" << (afterPerCore ? "after_percore" : "standalone");

    if (multiCore) {
        stored.score = report.multiCore.scoreMWorkPerSec;
        stored.durationSec = config.measureSeconds;
        stored.warmupSec = config.warmupSeconds;
        stored.timingSamples = report.multiCore.roundCount;
        contract << ";aggregation=median"
                 << ";affinityMode=" << report.multiCore.affinityMode
                 << ";threads=" << report.multiCore.threadCount
                 << ";pinnedThreads=" << report.multiCore.pinnedThreadCount;
    } else {
        double sum = 0.0;
        std::vector<std::string> affinityModes;
        for (const auto& item : report.perCore) {
            sum += item.scoreMWorkPerSec;
            if (std::find(affinityModes.begin(), affinityModes.end(), item.affinityMode) ==
                affinityModes.end()) {
                affinityModes.push_back(item.affinityMode);
            }
        }
        stored.score = report.perCore.empty()
            ? 0.0 : sum / static_cast<double>(report.perCore.size());
        stored.durationSec = config.measureSeconds * report.perCore.size();
        stored.warmupSec = config.warmupSeconds * report.perCore.size();
        stored.timingSamples = static_cast<std::uint32_t>(
            report.perCore.size() * config.roundCount);
        contract << ";aggregation=arithmetic_mean_of_per_core_medians"
                 << ";affinityModes=";
        if (affinityModes.empty()) {
            contract << "unavailable";
        } else {
            for (std::size_t i = 0; i < affinityModes.size(); ++i) {
                if (i != 0) contract << ',';
                contract << affinityModes[i];
            }
        }
    }
    stored.workloadConfig = contract.str();
    return stored;
}

static std::string ExeDirectory(const char* argv0) {
#ifdef _WIN32
    // argv is encoded with the active ANSI code page for a narrow main(), so
    // it cannot faithfully represent every valid Windows installation path.
    // Query the process image through the Unicode API and keep the project's
    // existing UTF-8 string contract for backend shader paths.
    std::wstring modulePath(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    if (length > 0 && length < modulePath.size()) {
        modulePath.resize(length);
        std::string directory =
            std::filesystem::path(modulePath).parent_path().u8string();
        if (!directory.empty() && directory.back() != '/' && directory.back() != '\\')
            directory += std::filesystem::path::preferred_separator;
        return directory;
    }
#endif

    std::string path(argv0);
    auto pos = path.find_last_of("\\/");
    return pos != std::string::npos ? path.substr(0, pos + 1) : "";
}

#ifdef _WIN32
// A RenderDoc DLL can hook DirectX directly, but Vulkan discovers its capture
// layer through a manifest. Point the loader at an available matching manifest
// before the early GPU probe creates a Vulkan instance. This is process-local:
// no SDK, installer, administrator rights, or registry mutation is required.
static std::filesystem::path ConfigureBundledRenderDocLayer(
    const std::string& shaderDir, bool captureRequested) {
    if (!captureRequested) return {};

    std::error_code ec;
    auto exeDir = std::filesystem::absolute(
        std::filesystem::u8path(shaderDir.empty() ? "." : shaderDir), ec);
    if (ec) return {};

    const std::filesystem::path candidates[] = {
        exeDir / "tools" / "RenderDoc",
        exeDir / ".." / "tools" / "RenderDoc",
        exeDir / ".." / ".." / "tools" / "RenderDoc",
        std::filesystem::path(L"C:\\Program Files\\RenderDoc"),
    };
    for (const auto& candidate : candidates) {
        const auto dir = candidate.lexically_normal();
        if (!std::filesystem::is_regular_file(dir / "renderdoc.json", ec) || ec) {
            ec.clear();
            continue;
        }
        if (!std::filesystem::is_regular_file(dir / "renderdoc.dll", ec) || ec) {
            ec.clear();
            continue;
        }

        // Override only the implicit-layer search for this capture process so
        // the manifest and DLL always come from the same packaged version.
        SetEnvironmentVariableW(L"VK_IMPLICIT_LAYER_PATH", dir.wstring().c_str());
        SetEnvironmentVariableW(L"ENABLE_VULKAN_RENDERDOC_CAPTURE", L"1");
        std::cout << "[RenderDoc] Configured Vulkan layer: "
                  << dir.u8string() << "\n";
        return dir / "renderdoc.dll";
    }
    return {};
}

#ifdef HAVE_VULKAN
// The bundled Vulkan layer is loaded by the early vkCreateInstance probe, well
// before AppBase::InitRenderDoc normally obtains the in-application API.  GUI
// workers must disable RenderDoc's modal crash reporter before that probe can
// fault. Keep this explicit reference for the worker lifetime so the Vulkan
// loader reuses the exact DLL selected by ConfigureBundledRenderDocLayer.
static HMODULE g_guiWorkerRenderDocModule = nullptr;

static bool PrepareGuiWorkerRenderDocForVulkanProbe(
    const std::filesystem::path& renderDocDll) {
    if (renderDocDll.empty() || g_guiWorkerRenderDocModule)
        return true;

    HMODULE module = LoadLibraryW(renderDocDll.c_str());
    if (!module) {
        std::cerr << "[RenderDoc] Could not preload the bundled DLL before GPU probe (Win32 "
                  << GetLastError() << ").\n";
        return false;
    }

    auto getApi = reinterpret_cast<pRENDERDOC_GetAPI>(
        GetProcAddress(module, "RENDERDOC_GetAPI"));
    RENDERDOC_API_1_6_0* api = nullptr;
    if (!getApi || getApi(eRENDERDOC_API_Version_1_6_0,
                          reinterpret_cast<void**>(&api)) != 1 ||
        !api || !api->UnloadCrashHandler) {
        std::cerr << "[RenderDoc] Bundled DLL did not expose the expected in-application API; "
                     "the early crash handler could not be disabled.\n";
        FreeLibrary(module);
        return false;
    }

    api->UnloadCrashHandler();
    g_guiWorkerRenderDocModule = module;
    std::cout << "[RenderDoc] Disabled the GUI worker crash handler before Vulkan GPU probe.\n";
    return true;
}
#endif
#endif

#ifdef _WIN32
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    std::cout << "\n[CRASH] Unhandled exception: 0x"
              << std::hex << code << std::dec << std::endl;

    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        std::cout << "  -> Access violation (segfault)" << std::endl; break;
    case EXCEPTION_STACK_OVERFLOW:
        std::cout << "  -> Stack overflow" << std::endl; break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        std::cout << "  -> Integer divide by zero" << std::endl; break;
    default:
        std::cout << "  -> SEH exception code: 0x" << std::hex << code << std::endl; break;
    }

    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

// ---------------------------------------------------------------------------
// Lightweight API probe — enumerates GPUs and checks which backends each one
// supports, *without* creating a full device or window.
// ---------------------------------------------------------------------------

struct GpuInfo {
    std::string name;
    bool isSoftware  = false;
    bool isDiscrete  = false;
    std::uint64_t vramMB = 0;
    bool supportsVulkan = false;
    bool supportsDX12   = false;
    bool supportsDX11   = false;
    bool supportsDX11Compute = false;
    std::uint32_t dx11FeatureLevel = 0;
    bool supportsMetal  = false;
    bool supportsOpenGL = false;
    std::int32_t dxgiRawIndex = -1;  // DXGI EnumAdapters1 index (stable across reordering)
    std::int32_t vkPhysDevIndex = -1; // Vulkan vkEnumeratePhysicalDevices index
    std::int64_t luidHigh = 0;  // DXGI adapter LUID for cross-factory matching
    std::int64_t luidLow  = 0;
    std::uint32_t dx12NodeIndex = 0; // physical node inside a linked DX12 adapter
    std::uint32_t dx12NodeCount = 1;
};

static bool NameLooksIntegrated(const std::string& name) {
    // Intel integrated: HD Graphics, UHD Graphics, Iris
    if (name.find("Intel") != std::string::npos) {
        if (name.find("Arc") != std::string::npos)
            return false;  // Intel Arc is discrete
        return true;       // all other Intel GPUs are integrated
    }
    // NVIDIA desktop GPUs are always discrete
    if (name.find("NVIDIA") != std::string::npos ||
        name.find("GeForce") != std::string::npos ||
        name.find("Quadro") != std::string::npos ||
        name.find("Tesla") != std::string::npos)
        return false;
    // AMD: discrete GPUs carry a model family (HD, RX, R9, R7, R5, Pro, VII, W)
    if (name.find("Radeon") != std::string::npos) {
        if (name.find("HD ")  != std::string::npos ||
            name.find("RX ")  != std::string::npos ||
            name.find("R9 ")  != std::string::npos ||
            name.find("R7 ")  != std::string::npos ||
            name.find("R5 ")  != std::string::npos ||
            name.find("VII")  != std::string::npos ||
            name.find("Pro ") != std::string::npos ||
            name.find(" W")   != std::string::npos)
            return false;  // discrete
        return true;       // generic "Radeon Graphics" = APU integrated
    }
    return false;  // unknown vendor, assume discrete
}

static std::vector<GpuInfo> ProbeGpus(
    [[maybe_unused]] const std::filesystem::path& guiWorkerRenderDocDll = {}) {
    std::vector<GpuInfo> gpus;

#ifdef _WIN32
    // --- DXGI enumeration (allow multiple identical GPUs, deduplicate by LUID) ---
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        struct DevLuid { LONG high; DWORD low; };
        std::vector<DevLuid> seen;

        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);

            bool dup = false;
            for (const auto& s : seen) {
                if (s.high == desc.AdapterLuid.HighPart &&
                    s.low  == desc.AdapterLuid.LowPart) {
                    dup = true; break;
                }
            }
            if (dup) { adapter.Reset(); continue; }
            seen.push_back({desc.AdapterLuid.HighPart, desc.AdapterLuid.LowPart});

            GpuInfo info;
            char name[256]{};
            size_t converted = 0;
            wcstombs_s(&converted, name, sizeof(name), desc.Description, _TRUNCATE);
            info.name = name;
            info.isSoftware = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
            info.vramMB = desc.DedicatedVideoMemory / (1024 * 1024);
            info.dxgiRawIndex = static_cast<std::int32_t>(i);
            info.luidHigh = desc.AdapterLuid.HighPart;
            info.luidLow  = desc.AdapterLuid.LowPart;

#ifdef HAVE_DX12
            {
                extern std::uint32_t ProbeDX12NodeCount(IDXGIAdapter1*);
                info.dx12NodeCount = ProbeDX12NodeCount(adapter.Get());
                info.supportsDX12 = info.dx12NodeCount > 0;
            }
#endif
#ifdef HAVE_DX11
            {
                D3D_FEATURE_LEVEL requested[] = {
                    D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                    D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0
                };
                D3D_FEATURE_LEVEL actual{};
                Microsoft::WRL::ComPtr<ID3D11Device> probeDevice;
                Microsoft::WRL::ComPtr<ID3D11DeviceContext> probeContext;
                const D3D_DRIVER_TYPE driverType = info.isSoftware
                    ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_UNKNOWN;
                IDXGIAdapter* probeAdapter = info.isSoftware ? nullptr : adapter.Get();
                HRESULT probeHr = D3D11CreateDevice(
                    probeAdapter, driverType, nullptr, 0,
                    requested, _countof(requested), D3D11_SDK_VERSION,
                    &probeDevice, &actual, &probeContext);
                // The Windows 7 platform update's D3D11.0 runtime rejects an
                // array containing FL11_1 with E_INVALIDARG. Retry without it
                // so the native backend probe remains accurate there too.
                if (probeHr == E_INVALIDARG) {
                    probeDevice.Reset();
                    probeContext.Reset();
                    probeHr = D3D11CreateDevice(
                        probeAdapter, driverType, nullptr, 0,
                        requested + 1, _countof(requested) - 1,
                        D3D11_SDK_VERSION,
                        &probeDevice, &actual, &probeContext);
                }
                if (SUCCEEDED(probeHr) && actual >= D3D_FEATURE_LEVEL_10_0) {
                    info.supportsDX11 = true;
                    info.dx11FeatureLevel = static_cast<std::uint32_t>(actual);
                    info.supportsDX11Compute = actual >= D3D_FEATURE_LEVEL_11_0;
                    if (!info.supportsDX11Compute) {
                        D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS options{};
                        info.supportsDX11Compute = SUCCEEDED(
                            probeDevice->CheckFeatureSupport(
                                D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS,
                                &options, sizeof(options))) &&
                            options.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x;
                    }
                }
            }
#endif
            gpus.push_back(info);
            adapter.Reset();
        }

        // DXGI exposes a linked-adapter set as one adapter.  D3D12 exposes
        // its physical GPUs as nodes of that one device, so expand those
        // nodes into selectable benchmark rows while retaining the same LUID.
        // DX11 has no equivalent node selector and therefore remains on the
        // first/logical row only.
        std::vector<GpuInfo> nodeExpanded;
        for (const auto& gpu : gpus) {
            nodeExpanded.push_back(gpu);
            if (!gpu.isSoftware && gpu.supportsDX12 && gpu.dx12NodeCount > 1) {
                for (std::uint32_t node = 1; node < gpu.dx12NodeCount; ++node) {
                    GpuInfo nodeGpu = gpu;
                    nodeGpu.dx12NodeIndex = node;
                    nodeGpu.supportsDX11 = false;
                    nodeGpu.supportsDX11Compute = false;
                    nodeGpu.dx11FeatureLevel = 0;
                    nodeGpu.supportsOpenGL = false;
                    nodeGpu.supportsVulkan = false;
                    nodeGpu.vkPhysDevIndex = -1;
                    nodeExpanded.push_back(std::move(nodeGpu));
                }
            }
        }
        gpus = std::move(nodeExpanded);

        // --- DXGI 1.6: classify discrete vs integrated ---
        // EnumAdapterByGpuPreference(HIGH_PERFORMANCE) returns the adapter
        // Windows considers "high performance" first — typically the discrete GPU.
        // Only use this heuristic when there are 2+ non-software GPUs; with a
        // single GPU the preference order is meaningless and would incorrectly
        // label an integrated GPU as discrete.
        std::size_t hwGpuCount = 0;
        for (const auto& g : gpus) { if (!g.isSoftware) ++hwGpuCount; }

        if (hwGpuCount >= 2) {
            Microsoft::WRL::ComPtr<IDXGIFactory6> factory6;
            if (SUCCEEDED(factory.As(&factory6))) {
                Microsoft::WRL::ComPtr<IDXGIAdapter1> hpAdapter;
                for (UINT hp = 0;
                     SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                         hp, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                         IID_PPV_ARGS(&hpAdapter)));
                     ++hp)
                {
                    DXGI_ADAPTER_DESC1 hpDesc{};
                    hpAdapter->GetDesc1(&hpDesc);
                    if (hpDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                        hpAdapter.Reset();
                        continue;
                    }
                    char hpName[256]{};
                    size_t hpConverted = 0;
                    wcstombs_s(&hpConverted, hpName, sizeof(hpName), hpDesc.Description, _TRUNCATE);
                    for (auto& g : gpus) {
                        // A DX12 linked adapter is expanded into one selectable
                        // row per physical node.  Mark every row that belongs to
                        // the high-performance adapter, not only node 0.
                        if (g.name == hpName) g.isDiscrete = true;
                    }
                    hpAdapter.Reset();
                    break;  // only the first non-software high-perf adapter is discrete
                }
            }
        } else {
            // Single hardware GPU: DXGI preference API can't distinguish, and
            // old GPUs may lack Vulkan.  Fall back to a name-based heuristic.
            for (auto& g : gpus) {
                if (!g.isSoftware)
                    g.isDiscrete = !NameLooksIntegrated(g.name);
            }
        }
    }

#endif  // _WIN32

#ifdef HAVE_METAL
    {
        auto metalDevices = gpu_bench::ProbeMetalDevices();
        for (auto& md : metalDevices) {
            auto it = std::find_if(gpus.begin(), gpus.end(), [&](const GpuInfo& g) {
                return g.name.find(md.name) != std::string::npos
                    || md.name.find(g.name) != std::string::npos;
            });
            if (it != gpus.end()) {
                it->supportsMetal = true;
                if (it->vramMB == 0 && md.vramBytes > 0)
                    it->vramMB = static_cast<std::uint32_t>(md.vramBytes / (1024 * 1024));
            } else {
                GpuInfo info;
                info.name          = md.name;
                info.supportsMetal = true;
                info.vramMB        = static_cast<std::uint32_t>(md.vramBytes / (1024 * 1024));
                info.isDiscrete    = !md.isLowPower;
                info.isSoftware    = false;
                gpus.push_back(info);
            }
        }
    }
#endif

    // --- Vulkan probe (if compiled in) ---
#ifdef HAVE_VULKAN
    if (VulkanLoaderAvailable()) {
#ifdef _WIN32
        // Delay the explicit preload until after DXGI/D3D probing. Loading
        // RenderDoc any earlier would unnecessarily change those probes by
        // installing its D3D hooks; this is the last safe point before the
        // Vulkan implicit layer can initialise its crash handler.
        PrepareGuiWorkerRenderDocForVulkanProbe(guiWorkerRenderDocDll);
#endif
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.apiVersion = VK_API_VERSION_1_1;

        VkInstanceCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.pApplicationInfo = &appInfo;

        VkInstance inst = VK_NULL_HANDLE;
        if (vkCreateInstance(&ci, nullptr, &inst) == VK_SUCCESS) {
            std::uint32_t count = 0;
            vkEnumeratePhysicalDevices(inst, &count, nullptr);
            std::vector<VkPhysicalDevice> devs(count);
            vkEnumeratePhysicalDevices(inst, &count, devs.data());

            for (std::uint32_t di = 0; di < devs.size(); ++di) {
                const auto& dev = devs[di];
                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(dev, &props);

                bool vkIsSoftware = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU);
                bool vkIsDiscrete = (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);

                // Match Vulkan device to an existing DXGI entry by name.
                // Prefer an unmatched entry first so that duplicate GPUs each get their own match.
                bool matched = false;
                auto nameMatches = [&](const GpuInfo& gpu) {
                    return gpu.name.find(props.deviceName) != std::string::npos ||
                           std::string(props.deviceName).find(gpu.name) != std::string::npos;
                };
                // First pass: match an entry that hasn't been marked yet.
                for (auto& gpu : gpus) {
                    if (!gpu.supportsVulkan && nameMatches(gpu)) {
                        gpu.supportsVulkan = true;
                        gpu.vkPhysDevIndex = static_cast<std::int32_t>(di);
                        if (vkIsSoftware) gpu.isSoftware = true;
                        if (vkIsDiscrete) gpu.isDiscrete = true;
                        matched = true;
                        break;
                    }
                }
                // Second pass: all matching entries already marked — this is an
                // additional GPU with the same name (e.g. dual identical cards).
                // Create a new entry right after the first match so ordering is logical.
                if (!matched) {
                    for (std::size_t gi = 0; gi < gpus.size(); ++gi) {
                        if (nameMatches(gpus[gi])) {
                            GpuInfo info;
                            info.name = gpus[gi].name;  // copy DXGI name so disambiguation matches
                            info.supportsVulkan = true;
                            info.isSoftware = vkIsSoftware;
                            info.isDiscrete = vkIsDiscrete;
                            info.vramMB = gpus[gi].vramMB;
                            // This is an extra Vulkan physical device, not an
                            // extra DXGI adapter.  A linked-adapter driver can
                            // expose one DXGI adapter with multiple D3D12 nodes
                            // while Vulkan exposes each physical GPU separately.
                            // Copying the first match's DXGI index/LUID and D3D
                            // capabilities makes run-all execute the same D3D
                            // adapter twice and merely label the second result
                            // as another GPU.  Leave this entry Vulkan-only;
                            // DX12 reaches its other node through explicit
                            // linked-adapter AFR/SFR instead.
                            info.vkPhysDevIndex = static_cast<std::int32_t>(di);
                            gpus.insert(gpus.begin() + static_cast<std::ptrdiff_t>(gi) + 1, info);
                            matched = true;
                            break;
                        }
                    }
                }
                // Partial match: try matching by a substring of the name.
                if (!matched) {
                    std::string vkName(props.deviceName);
                    for (auto& gpu : gpus) {
                        if (!gpu.isSoftware && !gpu.supportsVulkan) {
                            if (vkName.size() > 6 && gpu.name.size() > 6) {
                                std::string shortVk = vkName.substr(0, vkName.size() / 2);
                                if (gpu.name.find(shortVk) != std::string::npos) {
                                    gpu.supportsVulkan = true;
                                    gpu.vkPhysDevIndex = static_cast<std::int32_t>(di);
                                    if (vkIsDiscrete) gpu.isDiscrete = true;
                                    matched = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                if (!matched) {
                    GpuInfo info;
                    info.name = props.deviceName;
                    info.supportsVulkan = true;
                    info.isSoftware = vkIsSoftware;
                    info.isDiscrete = vkIsDiscrete;
                    info.supportsDX11 = false;
                    info.supportsDX12 = false;
                    info.vkPhysDevIndex = static_cast<std::int32_t>(di);
                    gpus.push_back(info);
                }
            }
            vkDestroyInstance(inst, nullptr);
        }
    }
#endif

    // --- OpenGL 4.3 probe (if compiled in) ---
#ifdef HAVE_OPENGL
    {
        if (glfwInit() == GLFW_FALSE)
            return gpus;  // GLFW not available — skip OpenGL probe

        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API,            GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,  4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,  3);
        glfwWindowHint(GLFW_OPENGL_PROFILE,         GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_VISIBLE,                GLFW_FALSE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,  GLFW_TRUE);
#endif
        GLFWwindow* probe = glfwCreateWindow(1, 1, "", nullptr, nullptr);
        if (probe) {
            glfwMakeContextCurrent(probe);

            // Read GL_RENDERER for software detection and fallback GPU entry.
            using GetStringFn = const unsigned char* (*)(unsigned int);
            auto glGetStringFn = reinterpret_cast<GetStringFn>(
                glfwGetProcAddress("glGetString"));
            std::string glRenderer;
            if (glGetStringFn) {
                const char* r = reinterpret_cast<const char*>(
                    glGetStringFn(0x1F01));  // GL_RENDERER
                if (r) glRenderer = r;
            }

            bool isSoftwareGL =
                glRenderer.find("llvmpipe") != std::string::npos ||
                glRenderer.find("softpipe") != std::string::npos ||
                glRenderer.find("swrast")   != std::string::npos ||
                glRenderer.find("lavapipe") != std::string::npos ||
                glRenderer.find("Software") != std::string::npos;

            // A GLFW context identifies only the OS-assigned OpenGL adapter;
            // Windows has no standard per-context adapter selector. Do not mark
            // every enumerated GPU as GL-capable or run-all will repeat the same
            // default adapter and mislabel those results. Match one renderer
            // entry; the fallback below creates a truthful GL-only row when the
            // Vulkan/DXGI name cannot be correlated.
            bool matchedOpenGlGpu = false;
            if (!glRenderer.empty()) {
                for (auto& gpu : gpus) {
                    if (gpu.isSoftware != isSoftwareGL) continue;
                    if (glRenderer.find(gpu.name) != std::string::npos ||
                        gpu.name.find(glRenderer) != std::string::npos) {
                        gpu.supportsOpenGL = true;
                        matchedOpenGlGpu = true;
                        break;
                    }
                }
            }
            if (!matchedOpenGlGpu && glRenderer.empty()) {
                for (auto& gpu : gpus) {
                    if (gpu.isSoftware == isSoftwareGL) {
                        gpu.supportsOpenGL = true;
                        matchedOpenGlGpu = true;
                        break;
                    }
                }
            }

            // On Linux without DXGI, gpus may be empty if no Vulkan SDK
            // is installed.  Create a GPU entry from GL_RENDERER so the
            // application has at least one usable device.
            if (!glRenderer.empty()) {
                bool alreadyListed = matchedOpenGlGpu;
                for (const auto& gpu : gpus) {
                    if (glRenderer.find(gpu.name) != std::string::npos ||
                        gpu.name.find(glRenderer) != std::string::npos) {
                        alreadyListed = true;
                        break;
                    }
                }
                if (!alreadyListed) {
                    GpuInfo info;
                    info.name = glRenderer;
                    info.supportsOpenGL = true;
                    info.isSoftware = isSoftwareGL;
                    info.supportsDX11 = false;
                    info.supportsDX12 = false;
                    gpus.push_back(info);
                }
            }

            glfwMakeContextCurrent(nullptr);
            glfwDestroyWindow(probe);
        }
        glfwTerminate();
    }
#endif

    // Remove ghost DXGI adapters: some drivers expose the same physical GPU
    // through multiple DXGI adapters with different LUIDs.  After all API
    // probing, if entries share the same name AND VRAM size but only some
    // have Vulkan support, the non-Vulkan ones are ghost adapters — remove
    // them.  In a real dual-GPU setup both instances would be Vulkan-capable.
    for (std::size_t i = 0; i < gpus.size(); ) {
        bool hasVulkanSibling = false;
        for (std::size_t j = 0; j < gpus.size(); ++j) {
            if (j != i && gpus[j].name == gpus[i].name &&
                gpus[j].vramMB == gpus[i].vramMB && gpus[j].supportsVulkan) {
                hasVulkanSibling = true;
                break;
            }
        }
        const bool linkedDx12Node = gpus[i].supportsDX12 &&
                                    gpus[i].dx12NodeCount > 1;
        if (hasVulkanSibling && !gpus[i].supportsVulkan && !linkedDx12Node) {
            gpus.erase(gpus.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }

    // Disambiguate GPUs with identical names by appending #1, #2, etc.
    for (std::size_t i = 0; i < gpus.size(); ++i) {
        const std::string original = gpus[i].name;
        int count = 0;
        for (std::size_t j = 0; j < gpus.size(); ++j)
            if (gpus[j].name == original) ++count;
        if (count > 1) {
            int idx = 1;
            for (std::size_t j = 0; j < gpus.size(); ++j) {
                if (gpus[j].name == original) {
                    gpus[j].name += " #" + std::to_string(idx++);
                }
            }
        }
    }

    return gpus;
}

#ifdef HAVE_DX12
#include <d3d12.h>
std::uint32_t ProbeDX12NodeCount(IDXGIAdapter1* adapter) {
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(&device))))
        return 0;
    return device->GetNodeCount();
}
#endif

// RenderDoc's D3D12 interpose crashes CreateCommandQueue when NodeMask selects
// a secondary linked-adapter node on the Mac Pro dual-D700 FireGL stack.
// Keep capture on node 0 / single adapters; force it off for node >= 1.
static void DisableRenderDocForSecondaryDx12Node(
    gpu_bench::BenchmarkConfig& cfg,
    const std::string& backendId,
    bool* renderDocLayerRequestedFlag = nullptr) {
    if (backendId != "dx12")
        return;
    if (!(cfg.adapterNodeCount > 1 && cfg.adapterNodeIndex > 0))
        return;
    if (!(cfg.renderDocEnabled || cfg.captureAtSec > 0.0 ||
          cfg.captureAtFrame > 0))
        return;
    std::cerr << "[DX12] RenderDoc capture/injection disabled for linked-adapter "
                 "node " << cfg.adapterNodeIndex
              << ": the capture layer crashes CreateCommandQueue on secondary "
                 "nodes of this AMD FireGL/UMD stack. Benchmark scores remain "
                 "valid; node 0 / the primary DX12 row remains capturable.\n";
    cfg.renderDocEnabled = false;
    cfg.captureAtSec = -1.0;
    cfg.captureAtFrame = -1;
    if (renderDocLayerRequestedFlag)
        *renderDocLayerRequestedFlag = false;
}

// ---------------------------------------------------------------------------

static void PrintGpuTable(const std::vector<GpuInfo>& gpus) {
    std::cout << "\n============================================================\n"
              << "                    GPU & API Detection\n"
              << "============================================================\n";
    std::cout << "  GPU";
    std::cout << std::string(41, ' ') << "VRAM     Vulkan  DX12  DX11  Metal  OpenGL\n";
    std::cout << "  ";
    std::cout << std::string(44, '-') << " ------  ------  ----  ----  -----  ------\n";

    for (std::uint32_t i = 0; i < gpus.size(); ++i) {
        const auto& g = gpus[i];
        std::string label = g.name;
        if (g.isSoftware)       label += " (Software)";
        else if (g.isDiscrete)  label += " (Discrete)";
        else                    label += " (Integrated)";
        if (label.size() > 44) label = label.substr(0, 41) + "...";

        std::cout << "  " << label;
        if (label.size() < 44) std::cout << std::string(44 - label.size(), ' ');

        if (g.vramMB > 0)
            std::cout << " " << g.vramMB << " MB";
        else
            std::cout << "     - ";

        auto yn = [](bool v) { return v ? "  YES " : "   -  "; };
        const char* dx11 = !g.supportsDX11 ? "   -  "
                           : g.supportsDX11Compute ? "  YES " : "  GFX ";
        std::cout << yn(g.supportsVulkan)
                  << yn(g.supportsDX12)
                  << dx11
                  << yn(g.supportsMetal)
                  << yn(g.supportsOpenGL)
                  << "\n";
    }
    if (std::any_of(gpus.begin(), gpus.end(), [](const GpuInfo& g) {
            return g.supportsDX11 && !g.supportsDX11Compute;
        })) {
        std::cout << "  DX11 GFX = raster workloads only; DirectCompute 4.x was not exposed.\n";
    }
    std::cout << "============================================================\n\n";
}

// ---------------------------------------------------------------------------

int gpu_bench::cliMain(int argc, char* argv[]) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(CrashHandler);
#endif

    std::string backend = "auto";
    std::int32_t gpuIndex = -1;
    bool useWarp = false;
    bool runAll = false;
    bool fullAnalysis = false;
    bool listGpus = false;
    bool timeArgGiven = false;   // explicit --time => run directly (no menu)
    bool benchmarkArgGiven = false; // remains true if an auto-tuned workload switches to timed mode
    bool headlessArgGiven = false;
    bool renderDocLayerRequested = false;
    bool cpuBenchmarkRequested = false;
    bool saveCpuResults = true;
    gpu_bench::CpuBenchmarkConfig cpuCfg;
    gpu_bench::BenchmarkConfig benchCfg;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--cpu-benchmark") == 0) {
            cpuBenchmarkRequested = true;
            // Preferred compact form: --cpu-benchmark per-core|multi|all.
            // Keep --cpu-benchmark --cpu-mode ... compatible with the GUI.
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                if (!ParseCpuBenchmarkMode(argv[i + 1], cpuCfg.mode)) {
                    std::cerr << "Unknown CPU benchmark mode '" << argv[i + 1]
                              << "' (expected per-core, multi, or all).\n";
                    return 2;
                }
                ++i;
            }
        } else if (std::strcmp(argv[i], "--cpu-mode") == 0 && i + 1 < argc) {
            cpuBenchmarkRequested = true;
            if (!ParseCpuBenchmarkMode(argv[++i], cpuCfg.mode)) {
                std::cerr << "Unknown CPU benchmark mode '" << argv[i]
                          << "' (expected per-core, multi, or all).\n";
                return 2;
            }
        } else if (std::strcmp(argv[i], "--cpu-time") == 0 && i + 1 < argc) {
            cpuBenchmarkRequested = true;
            if (!SafeStod(argv[++i], cpuCfg.measureSeconds) ||
                cpuCfg.measureSeconds < 0.03 || cpuCfg.measureSeconds > 3600.0) {
                std::cerr << "--cpu-time must be a finite value from 0.03 to 3600 seconds.\n";
                return 2;
            }
        } else if (std::strcmp(argv[i], "--cpu-warmup") == 0 && i + 1 < argc) {
            cpuBenchmarkRequested = true;
            if (!SafeStod(argv[++i], cpuCfg.warmupSeconds) ||
                cpuCfg.warmupSeconds < 0.0 || cpuCfg.warmupSeconds > 60.0) {
                std::cerr << "--cpu-warmup must be a finite value from 0 to 60 seconds.\n";
                return 2;
            }
        } else if (std::strcmp(argv[i], "--cpu-no-save") == 0) {
            cpuBenchmarkRequested = true;
            saveCpuResults = false;
        } else if (std::strcmp(argv[i], "--cpu-mode") == 0 ||
                   std::strcmp(argv[i], "--cpu-time") == 0 ||
                   std::strcmp(argv[i], "--cpu-warmup") == 0) {
            std::cerr << argv[i] << " requires a value.\n";
            return 2;
        } else if (std::strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
            backend = argv[++i];
        } else if (std::strcmp(argv[i], "--gpu") == 0 && i + 1 < argc) {
            gpuIndex = std::stoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--warp") == 0) {
            useWarp = true;
        } else if (std::strcmp(argv[i], "--vsync") == 0) {
            benchCfg.vsync = true;
        } else if (std::strcmp(argv[i], "--benchmark") == 0) {
            benchmarkArgGiven = true;
            benchCfg.benchmarkMode = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                benchCfg.benchFrames = static_cast<std::uint32_t>(
                    std::stoi(argv[++i]));
            }
        } else if (std::strcmp(argv[i], "--host-memory") == 0) {
            benchCfg.hostMemory = true;
        } else if (std::strcmp(argv[i], "--flights") == 0 && i + 1 < argc) {
            int n = std::stoi(argv[++i]);
            if (n < 1) n = 1;
            if (n > 16) n = 16;
            benchCfg.framesInFlight = static_cast<std::uint32_t>(n);
        } else if (std::strcmp(argv[i], "--multi-gpu") == 0 && i + 1 < argc) {
            const std::string mode = argv[++i];
            if (mode == "afr")
                benchCfg.multiGpuMode = gpu_bench::MultiGpuMode::Afr;
            else if (mode == "sfr")
                benchCfg.multiGpuMode = gpu_bench::MultiGpuMode::Sfr;
            else if (mode == "off" || mode == "none")
                benchCfg.multiGpuMode = gpu_bench::MultiGpuMode::Off;
            else {
                std::cerr << "Unknown --multi-gpu mode '" << mode
                          << "' (expected off, afr, or sfr)\n";
                return 2;
            }
        } else if (std::strcmp(argv[i], "--afr") == 0) {
            benchCfg.multiGpuMode = gpu_bench::MultiGpuMode::Afr;
        } else if (std::strcmp(argv[i], "--sfr") == 0) {
            benchCfg.multiGpuMode = gpu_bench::MultiGpuMode::Sfr;
        } else if (std::strcmp(argv[i], "--headless") == 0) {
            benchCfg.headless = true;
            headlessArgGiven = true;
        } else if (std::strcmp(argv[i], "--renderdoc") == 0) {
            benchCfg.renderDocEnabled = true;
            renderDocLayerRequested = true;
        } else if (std::strcmp(argv[i], "--no-renderdoc") == 0) {
            benchCfg.renderDocEnabled = false;
            renderDocLayerRequested = false;
            benchCfg.captureAtSec = -1.0;
            benchCfg.captureAtFrame = -1;
        } else if (std::strcmp(argv[i], "--gui-worker") == 0) {
            // Internal WinUI orchestration marker. GPU work is isolated in a
            // child process, so RenderDoc must not replace the worker's own
            // crash reporting with a separate modal Bug Reporter.
            benchCfg.guiWorker = true;
        } else if (std::strcmp(argv[i], "--time") == 0 && i + 1 < argc) {
            benchCfg.maxRunTimeSec = std::stod(argv[++i]);
            timeArgGiven = true;
        } else if (std::strcmp(argv[i], "--no-time-limit") == 0) {
            benchCfg.maxRunTimeSec = 0.0;
        } else if (std::strcmp(argv[i], "--particles") == 0 && i + 1 < argc) {
            auto n = static_cast<std::uint32_t>(std::stoi(argv[++i]));
            const std::uint32_t wg = gpu_bench::kComputeWorkGroupSize;
            benchCfg.particleCount = ((n + wg - 1) / wg) * wg;
            benchCfg.particlesOverridden = true;
        } else if (std::strcmp(argv[i], "--workload") == 0 && i + 1 < argc) {
            std::string w = argv[++i];
            if (w == "nbody")        benchCfg.workload = gpu_bench::Workload::NBody;
            else if (w == "stream")  benchCfg.workload = gpu_bench::Workload::Stream;
            else if (w == "gpu_stress" || w == "gpu-stress")
                                     benchCfg.workload = gpu_bench::Workload::GpuStressV1;
            else if (w == "gpu_burn" || w == "gpu-burn" || w == "burn")
                                     benchCfg.workload = gpu_bench::Workload::GpuBurnV1;
            else if (w == "gpu_burn_v1" || w == "gpu-burn-v1" || w == "plasma_bloom")
                                     benchCfg.workload = gpu_bench::Workload::GpuBurnV1;
            else if (w == "stress" || w == "fractal")
                                     benchCfg.workload = gpu_bench::Workload::StressFractal;
            else if (w == "synthpeak" || w == "peak")
                                     benchCfg.workload = gpu_bench::Workload::SynthPeak;
            else if (w == "render3d" || w == "3d")
                                     benchCfg.workload = gpu_bench::Workload::Render3D;
            else if (w == "volumetric" || w == "volume")
                                     benchCfg.workload = gpu_bench::Workload::Volumetric;
            else if (w == "cinematic_liquid" || w == "cinematic-liquid" || w == "liquid")
                                     benchCfg.workload = gpu_bench::Workload::CinematicLiquid;
            else if (w == "cinematic_liquid_v1" || w == "cinematic-liquid-v1" || w == "liquid_v1")
                                     benchCfg.workload = gpu_bench::Workload::CinematicLiquidV1;
            else if (w == "fluid")
                                     benchCfg.workload = gpu_bench::Workload::Fluid;
            else std::cerr << "Unknown workload '" << w << "' (use stream|nbody|gpu_burn|gpu_stress|stress|synthpeak|render3d|volumetric|cinematic_liquid|cinematic_liquid_v1|fluid)\n";
        } else if (std::strcmp(argv[i], "--precision") == 0 && i + 1 < argc) {
            std::string pr = argv[++i];
            if (pr == "fp32")       benchCfg.peakPrecision = gpu_bench::Precision::FP32;
            else if (pr == "fp16")  benchCfg.peakPrecision = gpu_bench::Precision::FP16;
            else if (pr == "fp64")  benchCfg.peakPrecision = gpu_bench::Precision::FP64;
            else if (pr == "int32") benchCfg.peakPrecision = gpu_bench::Precision::INT32;
            else std::cerr << "Unknown precision '" << pr << "' (use fp32|fp16|fp64|int32)\n";
        } else if (std::strcmp(argv[i], "--iter") == 0 && i + 1 < argc) {
            const int requested = std::stoi(argv[++i]);
            const auto n = static_cast<std::uint32_t>((std::max)(1, requested));
            benchCfg.fractalIter = n;   // fractal per-pixel iterations
            benchCfg.gpuStressIter = (std::min)(n, gpu_bench::kGpuStressV1MaxIter);
            benchCfg.gpuStressAutoTune = false;
            benchCfg.gpuBurnIter = (std::min)(n, gpu_bench::kGpuBurnV1MaxIter);
            benchCfg.gpuBurnAutoTune = false;
            benchCfg.gpuBurnIterOverridden = true;
            benchCfg.peakIters   = n;   // synthpeak loop passes (same flag)
        } else if (std::strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
            auto n = static_cast<std::uint32_t>(std::stoi(argv[++i]));
            if (n == 0) n = 1;
            benchCfg.volumetricSteps = n;   // volumetric per-pixel ray samples
        } else if (std::strcmp(argv[i], "--grid") == 0 && i + 1 < argc) {
            auto n = static_cast<std::uint32_t>(std::stoi(argv[++i]));
            // Round up to a multiple of 16 (the compute workgroup size).
            n = ((n + 15u) / 16u) * 16u;
            if (n < 16u) n = 16u;
            benchCfg.fluidGridSize = n;     // legacy fluid: 2D grid side length
        } else if (std::strcmp(argv[i], "--jacobi") == 0 && i + 1 < argc) {
            auto n = static_cast<std::uint32_t>(std::stoi(argv[++i]));
            benchCfg.fluidJacobiIters = n;  // legacy fluid: pressure iterations
        } else if (std::strcmp(argv[i], "--liquid-solver") == 0 && i + 1 < argc) {
            const std::string solver = argv[++i];
            if (solver == "sph") {
                benchCfg.liquidSolverSph = true;
            } else if (solver != "mpm") {
                std::cerr << "Unknown --liquid-solver '" << solver
                          << "' (expected mpm or sph)\n";
                return 1;
            }
        } else if (std::strcmp(argv[i], "--bodies") == 0 && i + 1 < argc) {
            auto n = static_cast<std::uint32_t>(std::stoi(argv[++i]));
            const std::uint32_t wg = gpu_bench::kComputeWorkGroupSize;
            benchCfg.workload = gpu_bench::Workload::NBody;
            benchCfg.particleCount = ((n + wg - 1) / wg) * wg;
            if (benchCfg.particleCount == 0) benchCfg.particleCount = wg;
            benchCfg.particlesOverridden = true;
        } else if (std::strcmp(argv[i], "--results") == 0) {
            auto results = gpu_bench::LoadResults();
            gpu_bench::PrintResultsTable(results);
            return 0;
        } else if (std::strcmp(argv[i], "--results-delete") == 0 && i + 1 < argc) {
            std::string id = argv[++i];
            if (gpu_bench::DeleteResult(id))
                std::cout << "Deleted result: " << id << "\n";
            else
                std::cout << "Result not found: " << id << "\n";
            return 0;
        } else if (std::strcmp(argv[i], "--results-clear") == 0) {
            gpu_bench::ClearResults();
            std::cout << "All benchmark results cleared.\n";
            return 0;
        } else if (std::strcmp(argv[i], "--compare") == 0) {
            auto results = gpu_bench::LoadResults();
            if (i + 2 < argc && argv[i + 1][0] != '-' && argv[i + 2][0] != '-') {
                std::string id1 = argv[++i];
                std::string id2 = argv[++i];
                const gpu_bench::BenchmarkResult* r1 = nullptr;
                const gpu_bench::BenchmarkResult* r2 = nullptr;
                for (const auto& r : results) {
                    if (r.id == id1) r1 = &r;
                    if (r.id == id2) r2 = &r;
                }
                if (!r1) { std::cerr << "Result not found: " << id1 << "\n"; return 1; }
                if (!r2) { std::cerr << "Result not found: " << id2 << "\n"; return 1; }
                gpu_bench::PrintDetailedComparison(*r1, *r2);
            } else {
                gpu_bench::PrintComparisonTable(results);
            }
            return 0;
        } else if (std::strcmp(argv[i], "--results-export") == 0 && i + 1 < argc) {
            std::string path = argv[++i];
            auto results = gpu_bench::LoadResults();
            if (results.empty()) {
                std::cout << "No results to export.\n";
            } else if (gpu_bench::ExportResultsCsv(path, results)) {
                std::cout << "Exported " << results.size()
                          << " result(s) to " << path << "\n";
            } else {
                std::cerr << "Failed to write to " << path << "\n";
                return 1;
            }
            return 0;
        } else if (std::strcmp(argv[i], "--capture") == 0) {
            benchCfg.renderDocEnabled = true;
            renderDocLayerRequested = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                benchCfg.captureAtSec = std::stod(argv[++i]);
            } else {
                benchCfg.captureAtSec = 5.0;
            }
        } else if (std::strcmp(argv[i], "--capture-frame") == 0) {
            benchCfg.renderDocEnabled = true;
            renderDocLayerRequested = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                benchCfg.captureAtFrame = std::stoll(argv[++i]);
            } else {
                benchCfg.captureAtFrame = 5;
            }
        } else if (std::strcmp(argv[i], "--list-gpus") == 0) {
            listGpus = true;
        } else if (std::strcmp(argv[i], "--run-all") == 0) {
            runAll = true;
        } else if (std::strcmp(argv[i], "--full-analysis") == 0) {
            fullAnalysis = true;
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout << "Mangekyo - Cross-API CPU & GPU Benchmark Suite\n\n"
                      << "Usage: " << argv[0] << " [options]\n"
                      << "  --backend <vulkan|dx12|dx11|metal|opengl>  Select rendering backend (default: auto)\n"
                      << "  --gpu <index>                       Select GPU by index\n"
                      << "  --warp                               Use WARP software renderer (DX11/DX12 only)\n"
                      << "  --vsync                              Enable vertical sync (default: off)\n"
                      << "  --host-memory                        Keep particle buffer in system RAM (slower on dGPU)\n"
                      << "  --flights <N>                       Set frames-in-flight count (default: 2, max: 16)\n"
                      << "  --multi-gpu <off|afr|sfr>           Split Plasma across two DX12 nodes or alternate frames\n"
                      << "  --afr                               Shorthand for --multi-gpu afr\n"
                      << "  --sfr                               Shorthand for --multi-gpu sfr\n"
                      << "  --headless                           Pure compute mode (no window/rendering/present)\n"
                      << "  --cpu-benchmark [per-core|multi|all] Run native CPU-only benchmark (default: all)\n"
                      << "  --cpu-mode <per-core|multi|all>      CPU mode alias used by the GUI\n"
                      << "  --renderdoc                         Enable RenderDoc injection/manual F12 capture\n"
                      << "  --no-renderdoc                      Disable RenderDoc DLL/API initialization\n"
                      << "  --cpu-time <seconds>                 Total measurement time per CPU test (default: 1)\n"
                      << "  --cpu-warmup <seconds>               CPU warm-up time per test (default: 0.15)\n"
                      << "  --cpu-no-save                        Do not append successful CPU summaries to results.json\n"
                      << "  --particles <count>                 Particle count (skips difficulty menu, rounded to 256)\n"
                      << "  --workload <stream|nbody|gpu_burn|gpu_stress|stress|synthpeak|render3d|volumetric|cinematic_liquid|cinematic_liquid_v1|fluid>\n"
                      << "                                      particle / N-body / Plasma x Kaleidoscope GPU Burn / GraphicsBurn component / legacy fractal / peak / 3D / volume / 3D liquid / legacy 2D fluid\n"
                      << "  --bodies <count>                    N-body body count (implies --workload nbody; default 65536)\n"
                      << "  --iter <count>                      GPU Burn fixed steps (16-2048) / GraphicsBurn / legacy fractal / SynthPeak\n"
                      << "  --steps <count>                     Volumetric per-pixel ray samples (default 96)\n"
                      << "  --grid <count>                      Legacy 2D fluid grid side length (default 256, rounded to 16)\n"
                      << "  --jacobi <count>                    Legacy 2D fluid pressure iterations (default 30)\n"
                      << "  --liquid-solver <mpm|sph>           Cinematic Liquid v2 solver (default mpm; sph = experimental preview slice)\n"
                      << "  --precision <fp32|fp16|fp64|int32>  SynthPeak data type (default fp32)\n"
                      << "  --time <seconds>                    Auto-stop after N seconds (default: 15)\n"
                      << "  --no-time-limit                     Run until window is closed\n"
                      << "  --benchmark [frames]                Run benchmark (default: 2000 frames), then exit\n"
                      << "  --results                           List all saved benchmark results\n"
                      << "  --results-delete <id>               Delete a saved result by ID\n"
                      << "  --results-clear                     Delete all saved results\n"
                      << "  --results-export <file.csv>         Export results to CSV file\n"
                      << "  --run-all                            Benchmark every GPU x API combination, then exit\n"
                      << "  --capture [seconds]                 Auto-capture via RenderDoc at T seconds (default: 5)\n"
                      << "  --capture-frame [N]                 Auto-capture via RenderDoc at frame N (default: 5)\n"
                      << "  --full-analysis                     Run all APIs + RenderDoc capture + Python charts (interactive)\n"
                      << "  --compare                           Compare saved results by workload/version score groups\n"
                      << "  --compare <id1> <id2>               Detailed side-by-side comparison of two results\n"
                      << "  --help                              Show this help\n\n"
                      << "Available Graphics APIs:";
#ifdef HAVE_VULKAN
            std::cout << " vulkan";
#endif
#ifdef HAVE_DX12
            std::cout << " dx12";
#endif
#ifdef HAVE_DX11
            std::cout << " dx11";
#endif
#ifdef HAVE_METAL
            std::cout << " metal";
#endif
#ifdef HAVE_OPENGL
            std::cout << " opengl";
#endif
            std::cout << '\n';
            return 0;
        }
    }

    // CPU benchmarking is a fully separate path. Return before GLFW, GPU/API
    // probing, shader discovery, or a graphics window is initialised.
    if (cpuBenchmarkRequested) {
        const auto report = gpu_bench::RunCpuBenchmark(cpuCfg, std::cout);
        const bool perCoreRequested = cpuCfg.mode == gpu_bench::CpuBenchmarkMode::PerCore ||
                                      cpuCfg.mode == gpu_bench::CpuBenchmarkMode::All;
        const bool multiRequested = cpuCfg.mode == gpu_bench::CpuBenchmarkMode::MultiCore ||
                                    cpuCfg.mode == gpu_bench::CpuBenchmarkMode::All;
        const bool perCoreValid = !perCoreRequested ||
            (report.perCore.size() == report.processors.size() &&
             !report.perCore.empty() &&
             std::all_of(report.perCore.begin(), report.perCore.end(),
                         [](const gpu_bench::CpuCoreResult& item) { return item.valid; }));
        const bool multiValid = !multiRequested || report.multiCore.valid;
        if (!perCoreValid || !multiValid) {
            std::cerr << "CPU benchmark did not meet its affinity/completion contract; "
                         "see CPU_ERROR and valid=0 records above.\n";
            return 3;
        }
        if (saveCpuResults) {
            bool saved = true;
            bool storageExceptionReported = false;
            try {
                if (perCoreRequested) {
                    const auto stored = MakeStoredCpuResult(report, cpuCfg, false);
                    const bool thisSaved = gpu_bench::AppendResult(stored);
                    saved = thisSaved && saved;
                    if (thisSaved)
                        std::cout << "[Results] Saved CPU per-core summary as " << stored.id
                                  << " -> " << gpu_bench::ResultsFilePath() << '\n';
                }
                if (multiRequested) {
                    const auto stored = MakeStoredCpuResult(report, cpuCfg, true);
                    const bool thisSaved = gpu_bench::AppendResult(stored);
                    saved = thisSaved && saved;
                    if (thisSaved)
                        std::cout << "[Results] Saved CPU multi-core summary as " << stored.id
                                  << " -> " << gpu_bench::ResultsFilePath() << '\n';
                }
            } catch (const std::exception& e) {
                saved = false;
                storageExceptionReported = true;
                std::cerr << "[Results] Warning: CPU result storage failed: "
                          << e.what() << '\n';
            }
            if (!saved && !storageExceptionReported)
                std::cerr << "[Results] Warning: failed to append one or more CPU summaries.\n";
        }
        return 0;
    }

    // ---- N-body workload normalisation ----
    // N-body is O(N^2): never inherit the 1M Stream default. Pick a sane body
    // count and skip the difficulty menu when the user didn't set one.
    if (benchCfg.workload == gpu_bench::Workload::NBody) {
        if (!benchCfg.particlesOverridden) {
            benchCfg.particleCount = gpu_bench::kNBodyDefaultBodies;
            benchCfg.difficultyLabel = "N-body 64K";
            benchCfg.particlesOverridden = true;
        } else {
            benchCfg.difficultyLabel = "N-body";
        }
        if (benchCfg.particleCount > 262144u) {
            std::cerr << "[warn] N-body is O(N^2): " << benchCfg.particleCount
                      << " bodies may run very slowly or trigger a GPU TDR/watchdog reset.\n";
        }
    }

    // ---- Fractal stress workload normalisation ----
    // It is a fragment-only render pass: needs a window (no headless), and the
    // particle buffer is unused so keep it tiny.
    if (benchCfg.workload == gpu_bench::Workload::StressFractal) {
        if (benchCfg.headless) {
            std::cerr << "[warn] Stress/Fractal requires rendering; ignoring --headless.\n";
            benchCfg.headless = false;
        }
        benchCfg.particleCount = gpu_bench::kComputeWorkGroupSize;  // unused; minimal alloc
        benchCfg.particlesOverridden = true;
        benchCfg.difficultyLabel = "Fractal";
    }

    // ---- Versioned GPU Stress v1 normalisation ----
    // Four bounded fullscreen draws keep the GPU continuously occupied without
    // turning a frame into one monolithic dispatch/draw. The particle buffer is
    // unused and remains at the minimum legal allocation size.
    if (benchCfg.workload == gpu_bench::Workload::GpuStressV1) {
        if (benchCfg.headless) {
            std::cerr << "[warn] GPU Stress v1 requires rendering; ignoring --headless.\n";
            benchCfg.headless = false;
        }
        if (benchCfg.gpuStressIter > gpu_bench::kGpuStressV1MaxIter) {
            std::cerr << "[warn] GPU Stress v1 clamps --iter to "
                      << gpu_bench::kGpuStressV1MaxIter
                      << " to keep each draw below watchdog-scale work.\n";
            benchCfg.gpuStressIter = gpu_bench::kGpuStressV1MaxIter;
        }
        benchCfg.particleCount = gpu_bench::kComputeWorkGroupSize;  // unused; minimal alloc
        benchCfg.particlesOverridden = true;
        benchCfg.difficultyLabel = "GPU Stress v1";
    }

    // ---- GPU Burn visual workload normalisation ----
    // The in-place GPU Burn v1 revision combines a solid Plasma Bloom subject
    // with the woven Mangekyo background. It consumes no particle data.
    if (gpu_bench::isGpuBurnWorkload(benchCfg.workload)) {
        const auto minIter = gpu_bench::gpuBurnDefaultIter(benchCfg.workload);
        const auto maxFixedIter = gpu_bench::gpuBurnMaxFixedIter(benchCfg.workload);
        if (benchCfg.headless) {
            std::cerr << "[warn] GPU Burn requires rendering; ignoring --headless.\n";
            benchCfg.headless = false;
        }
        // v3 selectable fixed-load contract. Hardware GPUs run the user's
        // chosen 16..2048 steps without auto-tuning. Software devices keep a
        // conservative cap here (and again at runtime for --run-all, where
        // the device is only known per matrix entry).
        if (useWarp && benchCfg.gpuBurnIter > maxFixedIter) {
            std::cerr << "[warn] GPU Burn on a software device is capped at "
                      << maxFixedIter
                      << " steps to avoid multi-second draws.\n";
            benchCfg.gpuBurnIter = maxFixedIter;
        }
        if (benchCfg.gpuBurnIter < minIter) {
            std::cerr << "[warn] GPU Burn requires at least "
                      << minIter
                      << " fixed steps to preserve the visual workload; clamping --iter.\n";
            benchCfg.gpuBurnIter = minIter;
        }
        benchCfg.particleCount = gpu_bench::kComputeWorkGroupSize;
        benchCfg.particlesOverridden = true;
        benchCfg.difficultyLabel = "GPU Burn v3 / Selectable fixed steps";
    }

    // ---- Volumetric workload normalisation ----
    // Same fragment-only shape as StressFractal: needs a window, particle buffer
    // unused. The per-pixel ray step count (config_.volumetricSteps) is the
    // sole knob and drives the score formula `pixels * steps / renderSec`.
    if (benchCfg.workload == gpu_bench::Workload::Volumetric) {
        if (benchCfg.headless) {
            std::cerr << "[warn] Volumetric requires rendering; ignoring --headless.\n";
            benchCfg.headless = false;
        }
        benchCfg.particleCount = gpu_bench::kComputeWorkGroupSize;  // unused; minimal alloc
        benchCfg.particlesOverridden = true;
        benchCfg.difficultyLabel = "Volumetric";
    }

    // ---- Cinematic Liquid normalisation ----
    // Formal scores remain Vulkan; Metal hosts an MLS-MPM v2 preview path only.
    if (gpu_bench::isCinematicLiquidWorkload(benchCfg.workload)) {
        const bool liquidV2 = benchCfg.workload == gpu_bench::Workload::CinematicLiquid;
        const char* liquidName = liquidV2 ? "Cinematic Liquid v2" : "Cinematic Liquid v1";
        if (backend == "auto") {
#if defined(__APPLE__)
            backend = liquidV2 ? "metal" : "vulkan";
#else
            backend = "vulkan";
#endif
        } else if (backend == "metal") {
            if (!liquidV2) {
                std::cerr << "Cinematic Liquid v1 is Vulkan-only; Metal hosts v2 only.\n";
                return 2;
            }
        } else if (backend != "vulkan") {
            std::cerr << liquidName
                      << " supports Vulkan"
                      << (liquidV2 ? " and Metal (preview)" : "")
                      << "; select --backend vulkan"
                      << (liquidV2 ? " or metal" : "")
                      << ".\n";
            return 2;
        }
        if (runAll || fullAnalysis) {
            std::cerr << liquidName
                      << " cannot run in a cross-API suite yet; use a custom run.\n";
            return 2;
        }
        if (useWarp) {
            std::cerr << liquidName << " does not support DXGI WARP; select a real GPU.\n";
            return 2;
        }
        if (benchCfg.headless) {
            std::cerr << "[warn] Cinematic Liquid requires its raymarched presentation; ignoring --headless.\n";
            benchCfg.headless = false;
        }
        if (benchCfg.hostMemory) {
            std::cerr << "[warn] " << liquidName
                      << " uses fixed device-local particle/grid storage; ignoring --host-memory.\n";
            benchCfg.hostMemory = false;
        }
        benchCfg.particleCount = gpu_bench::kComputeWorkGroupSize;  // generic buffer unused
        benchCfg.particlesOverridden = true;
        benchCfg.framesInFlight = 1;
        benchCfg.difficultyLabel = liquidName;
    }

    // ---- Legacy 2D fluid workload normalisation ----
    // The historical fluid prototype is a stateful Vulkan compute + render workload. The
    // particle buffer is unused; one in-flight windowed submission preserves
    // the simulation chain and exposes the cinematic render pass.
    if (benchCfg.workload == gpu_bench::Workload::Fluid) {
        if (benchCfg.headless) {
            std::cerr << "[warn] Legacy 2D Fluid requires rendering; ignoring --headless.\n";
            benchCfg.headless = false;
        }
        benchCfg.particleCount = gpu_bench::kComputeWorkGroupSize;  // unused; minimal alloc
        benchCfg.particlesOverridden = true;
        benchCfg.framesInFlight = 1;
        benchCfg.difficultyLabel = "Legacy 2D Fluid";
    }

    // ---- SynthPeak workload normalisation ----
    // Pure compute; thread count = particleCount (reuses the buffer as scratch
    // output). Headless by default, EXCEPT DX11 whose driver never resolves
    // timestamp queries headless — it runs windowed so GPU timing is available
    // (it harmlessly renders the scratch buffer as points).
    if (benchCfg.workload == gpu_bench::Workload::SynthPeak) {
        benchCfg.headless = (backend != "dx11");
        if (!benchCfg.particlesOverridden) {
            benchCfg.particleCount = gpu_bench::kSynthPeakDefaultThreads;
            benchCfg.particlesOverridden = true;
        }
        benchCfg.difficultyLabel =
            (benchCfg.peakPrecision == gpu_bench::Precision::FP64)  ? "Peak FP64"
          : (benchCfg.peakPrecision == gpu_bench::Precision::FP16)  ? "Peak FP16"
          : (benchCfg.peakPrecision == gpu_bench::Precision::INT32) ? "Peak INT32"
          :                                                           "Peak FP32";
    }

    // ---- Render3D workload normalisation ----
    // True-3D billboard rendering needs a window (no headless); moderate default
    // instance count to keep overdraw reasonable.
    if (benchCfg.workload == gpu_bench::Workload::Render3D) {
        if (benchCfg.headless) {
            std::cerr << "[warn] Render3D requires rendering; ignoring --headless.\n";
            benchCfg.headless = false;
        }
        if (!benchCfg.particlesOverridden) {
            benchCfg.particleCount = gpu_bench::kRender3DDefaultParticles;
            benchCfg.particlesOverridden = true;
        }
        benchCfg.difficultyLabel = "3D";
    }

    // AFR is currently a deliberately narrow, stateless vertical slice.  The
    // Plasma/GPU Burn pass has no cross-frame simulation data, which lets two
    // devices render alternate frames without duplicating mutable state.
    if (benchCfg.multiGpuMode == gpu_bench::MultiGpuMode::Afr) {
        if (!gpu_bench::isGpuBurnWorkload(benchCfg.workload)) {
            std::cerr << "--multi-gpu afr currently supports only --workload gpu_burn "
                         "(Plasma). Particle and liquid workloads have cross-frame state "
                         "and are intentionally not run as two independent tests.\n";
            return 2;
        }
        if (headlessArgGiven) {
            std::cerr << "--multi-gpu afr requires a windowed present path.\n";
            return 2;
        }
        if (useWarp) {
            std::cerr << "--multi-gpu afr requires hardware GPUs; WARP is unsupported.\n";
            return 2;
        }
        if (backend != "auto" && backend != "vulkan" &&
            backend != "dx12" && backend != "dx11") {
            std::cerr << "--multi-gpu afr supports Vulkan, DX12, or DX11 only.\n";
            return 2;
        }
        if (runAll || fullAnalysis) {
            std::cerr << "--multi-gpu afr is a custom-run mode and cannot be combined "
                         "with --run-all or --full-analysis.\n";
            return 2;
        }
        if (benchCfg.framesInFlight < gpu_bench::kAfrMinFramesInFlight) {
            std::cerr << "[AFR] Raising frames-in-flight from "
                      << benchCfg.framesInFlight << " to "
                      << gpu_bench::kAfrMinFramesInFlight
                      << ": two reusable frame slots per GPU are required for "
                         "linked-adapter overlap.\n";
            benchCfg.framesInFlight = gpu_bench::kAfrMinFramesInFlight;
        }

        // RenderDoc interposes queue submission/presentation.  On linked D3D12
        // adapters this can let the CPU run far ahead while GPU work drains only
        // during teardown, producing impossible FPS and no timestamp samples.
        // Vulkan device-group capture has the same multi-device ambiguity.  AFR
        // benchmarking must therefore run without the capture layer/API.
        if (benchCfg.renderDocEnabled || benchCfg.captureAtSec > 0.0 ||
            benchCfg.captureAtFrame > 0) {
            std::cerr << "[AFR] RenderDoc capture/injection is disabled for multi-GPU "
                         "runs because it invalidates AFR timing and queue overlap.\n";
            benchCfg.renderDocEnabled = false;
            renderDocLayerRequested = false;
            benchCfg.captureAtSec = -1.0;
            benchCfg.captureAtFrame = -1;
        }
    }

    // DX12 SFR renders one logical Plasma frame on both linked nodes: node 0
    // shades the left half, node 1 shades the right half, then node 0 performs
    // the single composition/present.  Keep the first implementation narrow so
    // it cannot silently turn stateful workloads into two independent tests.
    if (benchCfg.multiGpuMode == gpu_bench::MultiGpuMode::Sfr) {
        if (!gpu_bench::isGpuBurnWorkload(benchCfg.workload)) {
            std::cerr << "--multi-gpu sfr currently supports only --workload gpu_burn "
                         "(Plasma).\n";
            return 2;
        }
        if (headlessArgGiven) {
            std::cerr << "--multi-gpu sfr requires a windowed composition path.\n";
            return 2;
        }
        if (useWarp) {
            std::cerr << "--multi-gpu sfr requires hardware GPUs; WARP is unsupported.\n";
            return 2;
        }
        if (backend != "auto" && backend != "dx12") {
            std::cerr << "--multi-gpu sfr currently supports DX12 only.\n";
            return 2;
        }
        if (backend == "auto")
            backend = "dx12";
        if (runAll || fullAnalysis) {
            std::cerr << "--multi-gpu sfr is a custom-run mode and cannot be combined "
                         "with --run-all or --full-analysis.\n";
            return 2;
        }
        if (benchCfg.renderDocEnabled || benchCfg.captureAtSec > 0.0 ||
            benchCfg.captureAtFrame > 0) {
            std::cerr << "[SFR] RenderDoc capture/injection is disabled until linked-node "
                         "composition has independent capture validation.\n";
            benchCfg.renderDocEnabled = false;
            renderDocLayerRequested = false;
            benchCfg.captureAtSec = -1.0;
            benchCfg.captureAtFrame = -1;
        }
    }

    const std::string shaderDir = ExeDirectory(argv[0]);

#ifdef _WIN32
    // Vulkan capture needs the RenderDoc implicit layer before vkCreateInstance.
    // DX12/DX11 capture uses AppBase::InitRenderDoc (LoadLibrary) instead.
    // Enabling the Vulkan layer for a DX12 GUI/CLI worker makes ProbeGpus'
    // Vulkan instance load renderdoc.dll, which also hooks D3D12 and then
    // access-violates CreateCommandQueue on linked-adapter node >= 1.
    const bool configureVulkanRenderDocLayer =
        benchCfg.renderDocEnabled && renderDocLayerRequested &&
        (backend == "vulkan" ||
         (benchCfg.guiWorker && backend == "auto"));
    const auto bundledRenderDocDll = ConfigureBundledRenderDocLayer(
        shaderDir, configureVulkanRenderDocLayer);
    // GUI workers preload RenderDoc before the Vulkan probe so its crash
    // handler can be disabled.  Skip that preload for explicit DX12/DX11/
    // OpenGL workers: the D3D hooks would already be installed before we
    // know whether --gpu selected a secondary linked-adapter node, and on
    // FirePro D700 node 1 CreateCommandQueue then access-violates.
    const bool preloadRenderDocForVulkanProbe =
        benchCfg.guiWorker &&
        (backend == "auto" || backend == "vulkan");
    const auto guiWorkerRenderDocDll = preloadRenderDocForVulkanProbe
        ? bundledRenderDocDll : std::filesystem::path{};
#else
    const std::filesystem::path guiWorkerRenderDocDll;
#endif

    // ---- Phase 1: Probe all GPUs and APIs ----
    auto gpus = ProbeGpus(guiWorkerRenderDocDll);

    // Machine-readable GPU list for external front-ends (WinUI launcher).
    // One line per GPU: GPU<TAB>index<TAB>name<TAB>vk<TAB>dx12<TAB>dx11<TAB>ogl
    // followed by optional downlevel DX11 metadata. Older GUI builds safely
    // ignore the appended fields.
    if (listGpus) {
        for (std::size_t i = 0; i < gpus.size(); ++i) {
            const auto& g = gpus[i];
            std::cout << "GPU\t" << i << '\t' << g.name << '\t'
                      << (g.supportsVulkan ? 1 : 0) << '\t'
                      << (g.supportsDX12   ? 1 : 0) << '\t'
                      << (g.supportsDX11   ? 1 : 0) << '\t'
                      << (g.supportsOpenGL ? 1 : 0) << '\t'
                      << g.dx11FeatureLevel << '\t'
                      << (g.supportsDX11Compute ? 1 : 0) << '\n';
        }
        return 0;
    }

    PrintGpuTable(gpus);

    bool directBenchmark = (backend != "auto") || benchmarkArgGiven || runAll || timeArgGiven;

    // ---- Build available backends ----
    struct BackendEntry { std::string id; bool hwOnly; };
    std::vector<BackendEntry> hwBackends, swBackends;

    auto hasHwSupport = [&](auto pred) {
        for (const auto& g : gpus) if (!g.isSoftware && pred(g)) return true;
        return false;
    };
    auto hasSwSupport = [&](auto pred) {
        for (const auto& g : gpus) if (g.isSoftware && pred(g)) return true;
        return false;
    };

    struct ApiProbe { const char* id; bool (*pred)(const GpuInfo&); };
    ApiProbe probes[] = {
#ifdef HAVE_METAL
        {"metal",  [](const GpuInfo& g){ return g.supportsMetal; }},
#endif
#ifdef HAVE_VULKAN
        {"vulkan", [](const GpuInfo& g){ return g.supportsVulkan; }},
#endif
#ifdef HAVE_DX12
        {"dx12",   [](const GpuInfo& g){ return g.supportsDX12; }},
#endif
#ifdef HAVE_DX11
        {"dx11",   [](const GpuInfo& g){ return g.supportsDX11; }},
#endif
#ifdef HAVE_OPENGL
        {"opengl", [](const GpuInfo& g){ return g.supportsOpenGL; }},
#endif
    };

    for (const auto& p : probes) {
        bool hw = hasHwSupport(p.pred);
        bool sw = hasSwSupport(p.pred);
        if (hw)       hwBackends.push_back({p.id, true});
        else if (sw)  swBackends.push_back({p.id, false});
    }

    std::vector<BackendEntry> available;
    available.insert(available.end(), hwBackends.begin(), hwBackends.end());
    available.insert(available.end(), swBackends.begin(), swBackends.end());

    if (available.empty()) {
        std::cerr << "No Graphics API available.\n";
        return 1;
    }

    // Determine recommended GPU: discrete hw > integrated hw > software.
    // Within the same tier, prefer more VRAM.
    auto gpuScore = [](const GpuInfo& g) -> int {
        if (g.isSoftware) return 0;
        if (g.isDiscrete) return 2;
        return 1;  // integrated
    };

    std::int32_t recommendedGpuIdx = 0;
    for (std::uint32_t i = 1; i < gpus.size(); ++i) {
        int scoreCur  = gpuScore(gpus[recommendedGpuIdx]);
        int scoreThis = gpuScore(gpus[i]);
        if (scoreThis > scoreCur ||
            (scoreThis == scoreCur && gpus[i].vramMB > gpus[recommendedGpuIdx].vramMB)) {
            recommendedGpuIdx = static_cast<std::int32_t>(i);
        }
    }
    std::string recommendedGpuName = gpus[recommendedGpuIdx].name;

    // Best API for the recommended GPU: Metal (macOS) > Vulkan > DX12 > DX11 > OpenGL.
    const auto& recGpu = gpus[recommendedGpuIdx];
    std::string recommendedApi;
    if (recGpu.supportsMetal)       recommendedApi = "metal";
    else if (recGpu.supportsVulkan) recommendedApi = "vulkan";
    else if (recGpu.supportsDX12)   recommendedApi = "dx12";
    else if (recGpu.supportsDX11)   recommendedApi = "dx11";
    else if (recGpu.supportsOpenGL) recommendedApi = "opengl";

    std::string recommendedApiLabel;
    if (recommendedApi == "metal")        recommendedApiLabel = "Metal";
    else if (recommendedApi == "vulkan")  recommendedApiLabel = "Vulkan";
    else if (recommendedApi == "dx12")    recommendedApiLabel = "DirectX 12";
    else if (recommendedApi == "dx11")    recommendedApiLabel = "DirectX 11";
    else if (recommendedApi == "opengl")  recommendedApiLabel = "OpenGL 4.3";

    bool hasLastRun = false;

    // ---- --run-all: benchmark every GPU × API combination, then exit ----
    if (runAll) {
        struct RunAllEntry {
            std::int32_t gpuIdx;
            std::string  backendId;
            std::string  gpuName;
            std::string  apiLabel;
            std::int64_t luidHigh = 0;
            std::int64_t luidLow  = 0;
            std::uint32_t vramMB  = 0;
            std::uint32_t nodeIndex = 0;
            std::uint32_t nodeCount = 1;
        };
        // Map gpus-array index + backend to the raw index each backend expects.
        auto rawIdx = [&](std::uint32_t gi, const std::string& bid) -> std::int32_t {
            const auto& g = gpus[gi];
            if (g.isSoftware) return -2;
            if (bid == "vulkan") return g.vkPhysDevIndex;
            if (bid == "dx11" || bid == "dx12") return g.dxgiRawIndex;
            return static_cast<std::int32_t>(gi);
        };
        std::vector<RunAllEntry> entries;
        for (std::uint32_t gi = 0; gi < gpus.size(); ++gi) {
            const auto& g = gpus[gi];
#ifdef HAVE_VULKAN
            if (g.supportsVulkan)
                entries.push_back({rawIdx(gi, "vulkan"), "vulkan", g.name, "Vulkan", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#endif
#ifdef HAVE_DX12
            if (g.supportsDX12)
                entries.push_back({rawIdx(gi, "dx12"), "dx12", g.name, "DirectX 12", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#endif
#ifdef HAVE_DX11
            if (g.supportsDX11)
                entries.push_back({rawIdx(gi, "dx11"), "dx11", g.name, "DirectX 11", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#endif
#ifdef HAVE_METAL
            if (g.supportsMetal)
                entries.push_back({rawIdx(gi, "metal"), "metal", g.name, "Metal", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#endif
#ifdef HAVE_OPENGL
            if (g.supportsOpenGL)
                entries.push_back({rawIdx(gi, "opengl"), "opengl", g.name, "OpenGL 4.3", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#endif
        }

        if (entries.empty()) {
            std::cerr << "No runnable GPU x API combinations found.\n";
            return 1;
        }

        std::cout << "========== Run All: " << entries.size()
                  << " benchmark(s) ==========\n";
        for (std::uint32_t i = 0; i < entries.size(); ++i)
            std::cout << "  [" << (i + 1) << "] " << entries[i].apiLabel
                      << " / " << entries[i].gpuName << "\n";
        std::cout << "============================================\n";

        gpu_bench::BenchmarkConfig allCfg;
        allCfg.particleCount = benchCfg.particlesOverridden
            ? benchCfg.particleCount : 1048576;
        allCfg.difficultyLabel = benchCfg.particlesOverridden
            ? benchCfg.difficultyLabel : "Medium";
        allCfg.particlesOverridden = true;
        allCfg.vsync = benchCfg.vsync;
        // Carry the selected workload through to every GPU x API run.
        allCfg.workload      = benchCfg.workload;
        allCfg.peakPrecision = benchCfg.peakPrecision;
        allCfg.peakIters     = benchCfg.peakIters;
        allCfg.fractalIter   = benchCfg.fractalIter;
        allCfg.gpuStressIter = benchCfg.gpuStressIter;
        allCfg.gpuStressAutoTune = benchCfg.gpuStressAutoTune;
        allCfg.gpuBurnIter = benchCfg.gpuBurnIter;
        allCfg.gpuBurnAutoTune = benchCfg.gpuBurnAutoTune;
        allCfg.volumetricSteps = benchCfg.volumetricSteps;
        allCfg.fluidGridSize   = benchCfg.fluidGridSize;
        allCfg.fluidJacobiIters= benchCfg.fluidJacobiIters;
        allCfg.softening     = benchCfg.softening;
        allCfg.headless      = benchCfg.headless;
        allCfg.framesInFlight = benchCfg.framesInFlight;
        allCfg.hostMemory    = benchCfg.hostMemory;
        allCfg.captureAtSec  = benchCfg.captureAtSec;
        allCfg.captureAtFrame = benchCfg.captureAtFrame;
        allCfg.renderDocEnabled = benchCfg.renderDocEnabled;
        allCfg.guiWorker     = benchCfg.guiWorker;
        if (benchCfg.maxRunTimeSec != 15.0)
            allCfg.maxRunTimeSec = benchCfg.maxRunTimeSec;
        if (benchCfg.benchmarkMode) {
            allCfg.benchmarkMode = true;
            allCfg.benchFrames = benchCfg.benchFrames;
        }

        std::uint32_t passed = 0, failed = 0;
        for (std::uint32_t i = 0; i < entries.size(); ++i) {
            const auto& e = entries[i];
            const bool runtimeNamedDevice =
                e.gpuIdx == -2 || e.backendId == "opengl";
            allCfg.gpuDisplayName = runtimeNamedDevice ? std::string{} : e.gpuName;
            allCfg.adapterLuidHigh = runtimeNamedDevice ? 0 : e.luidHigh;
            allCfg.adapterLuidLow  = runtimeNamedDevice ? 0 : e.luidLow;
            allCfg.vramMB          = e.gpuIdx == -2 ? 0 : e.vramMB;
            allCfg.adapterNodeIndex = e.nodeIndex;
            allCfg.adapterNodeCount = e.nodeCount;
            std::cout << "\n>>> [" << (i + 1) << "/" << entries.size()
                      << "] " << e.apiLabel << " / " << e.gpuName << " <<<\n";
            try {
                std::unique_ptr<gpu_bench::AppBase> app;
                // Same host-memory fallback as the single-run path: only
                // Vulkan/OpenGL implement it; report other passes honestly.
                gpu_bench::BenchmarkConfig entryCfg = allCfg;
                DisableRenderDocForSecondaryDx12Node(entryCfg, e.backendId);
                if (entryCfg.hostMemory &&
                    e.backendId != "vulkan" && e.backendId != "opengl") {
                    std::cerr << "[warn] --host-memory (system memory) is not implemented for "
                              << e.backendId
                              << "; this pass runs device-local (VRAM) instead.\n";
                    entryCfg.hostMemory = false;
                }
#ifdef HAVE_VULKAN
                if (e.backendId == "vulkan")
                    app = std::make_unique<gpu_bench::VulkanBackend>(
                        e.gpuIdx, shaderDir, entryCfg);
#endif
#ifdef HAVE_DX12
                if (e.backendId == "dx12")
                    app = std::make_unique<gpu_bench::DX12Backend>(
                        e.gpuIdx, shaderDir, entryCfg);
#endif
#ifdef HAVE_DX11
                if (e.backendId == "dx11")
                    app = std::make_unique<gpu_bench::DX11Backend>(
                        e.gpuIdx, shaderDir, entryCfg);
#endif
#ifdef HAVE_METAL
                if (e.backendId == "metal")
                    app = std::make_unique<gpu_bench::MetalBackend>(
                        e.gpuIdx, shaderDir, entryCfg);
#endif
#ifdef HAVE_OPENGL
                if (e.backendId == "opengl")
                    app = std::make_unique<gpu_bench::OpenGLBackend>(
                        e.gpuIdx, shaderDir, entryCfg);
#endif
                if (!app) {
                    std::cout << "  SKIPPED (backend not available)\n";
                    ++failed;
                    continue;
                }
                app->Run();
                ++passed;
            } catch (const gpu_bench::BackToMenuException&) {
                std::cout << "  SKIPPED (user cancelled)\n";
                ++failed;
            } catch (const std::exception& ex) {
                std::cout << "  FAILED: " << ex.what() << "\n";
                ++failed;
            }
        }

        std::cout << "\n========== Run All Complete ==========\n"
                  << "  Passed: " << passed << " / " << entries.size() << "\n";
        if (failed > 0)
            std::cout << "  Failed/Skipped: " << failed << "\n";
        std::cout << "======================================\n";

        auto allResults = gpu_bench::LoadResults();
        if (!allResults.empty())
            gpu_bench::PrintComparisonTable(allResults);

#if !defined(GPU_BENCH_NO_GLFW)
        if (!gpu_bench::skipGlfwTerminate)
            glfwTerminate();
#endif
        // GUI Full Analysis treats the worker exit code as its success signal.
        // A partial matrix is not a successful full analysis: preserve the
        // per-entry diagnostics above, but return non-zero if any run failed or
        // was cancelled instead of silently presenting incomplete charts.
        return failed > 0 ? 1 : 0;
    }

    // ---- Unified main loop ----
    while (true) {

        if (!directBenchmark) {
            auto saved = gpu_bench::LoadResults();

            std::cout << "\n======= Mangekyo | GPU Benchmark =======\n"
                      << "  [0] Quick run (" << recommendedApiLabel
                      << " / " << recommendedGpuName << " / Medium)  <- default\n"
                      << "  [1] Custom run (choose API / GPU / difficulty)\n";
            if (hasLastRun)
                std::cout << "  [2] Run again (same settings)\n";
            std::cout << "  [3] Compare results";
            if (!saved.empty()) std::cout << " (" << saved.size() << " saved)";
            std::cout << "\n  [4] Delete results\n"
                      << "  [5] Full analysis - one GPU (all APIs + RenderDoc + charts)\n"
                      << "  [6] Full analysis - all GPUs x APIs (+ RenderDoc + charts)\n"
                      << "  [7] Flights test  - one GPU (all APIs + RenderDoc, custom flights)\n"
                      << "  [8] Particle test - one GPU (all APIs + RenderDoc, custom particles)\n"
                      << "  [9] Headless compute - one GPU (all APIs, pure compute, no rendering)\n"
                      << "  [10] Exit\n"
                      << "========================================\n"
                      << "Select (default: 0): " << std::flush;

            std::string mline;
            int mchoice = 0;
            if (fullAnalysis) {
                mchoice = 5;
                fullAnalysis = false;
                std::cout << "5  (--full-analysis)\n";
            } else if (std::getline(std::cin, mline) && !mline.empty()) {
                if (!SafeStoi(mline, mchoice)) { std::cout << "Invalid input.\n"; continue; }
            }

            if (mchoice == 0) {
                backend = recommendedApi;
                gpuIndex = recommendedGpuIdx;
                benchCfg.particleCount = 1048576;
                benchCfg.difficultyLabel = "Medium";
                benchCfg.particlesOverridden = true;
            } else if (mchoice == 1) {
                backend = "auto";
                gpuIndex = -1;
                benchCfg.particlesOverridden = false;
            } else if (mchoice == 2 && hasLastRun) {
                benchCfg.particlesOverridden = true;
            } else if (mchoice == 3) {
                if (saved.empty()) {
                    std::cout << "No saved results.\n";
                    continue;
                }
                auto sorted = saved;
                std::sort(sorted.begin(), sorted.end(),
                    [](const gpu_bench::BenchmarkResult& a,
                       const gpu_bench::BenchmarkResult& b) {
                        return a.avgFps > b.avgFps;
                    });
                gpu_bench::PrintComparisonTable(saved);
                if (sorted.size() >= 2) {
                    std::cout << "Enter two rank numbers for detailed comparison (or Enter to skip):\n"
                              << "#1: " << std::flush;
                    std::string s1;
                    if (std::getline(std::cin, s1) && !s1.empty()) {
                        std::cout << "#2: " << std::flush;
                        std::string s2;
                        if (std::getline(std::cin, s2) && !s2.empty()) {
                            int i1 = 0, i2 = 0;
                            if (SafeStoi(s1, i1) && SafeStoi(s2, i2)) {
                                --i1; --i2;
                                if (i1 >= 0 && i1 < static_cast<int>(sorted.size()) &&
                                    i2 >= 0 && i2 < static_cast<int>(sorted.size()))
                                    gpu_bench::PrintDetailedComparison(sorted[i1], sorted[i2]);
                                else
                                    std::cout << "Invalid rank number.\n";
                            } else {
                                std::cout << "Invalid input.\n";
                            }
                        }
                    }
                }
                continue;
            } else if (mchoice == 4) {
                std::cout << "\n========== Delete Data ==========\n"
                          << "  [1] Delete benchmark results";
                if (!saved.empty()) std::cout << " (" << saved.size() << " saved)";
                std::cout << "\n  [2] Delete Python charts & reports\n"
                          << "  [3] Delete all (results + charts + reports)\n"
                          << "  [0] Back\n"
                          << "=================================\n"
                          << "Select: " << std::flush;

                std::string dline;
                int dchoice = 0;
                if (std::getline(std::cin, dline) && !dline.empty())
                    SafeStoi(dline, dchoice);

                auto deletePythonOutputs = [&]() {
#ifdef _WIN32
                    std::string sep = "\\";
#else
                    std::string sep = "/";
#endif
                    std::string projectRoot = shaderDir + ".." + sep + "..";
                    std::string docsDir = projectRoot + sep + "docs" + sep;
                    std::string imagesDir = docsDir + "images" + sep;
                    const char* files[] = {
                        "fps_by_gpu.png", "gpu_time_breakdown.png",
                        "cpu_overhead.png", "scaling.png"
                    };
                    int count = 0;
                    for (const char* f : files) {
                        std::string path = imagesDir + f;
                        if (std::remove(path.c_str()) == 0) ++count;
                    }
                    std::string mdPath = docsDir + "results-table.md";
                    std::string htmlPath = docsDir + "report.html";
                    if (std::remove(mdPath.c_str()) == 0) ++count;
                    if (std::remove(htmlPath.c_str()) == 0) ++count;
                    std::cout << "Deleted " << count << " Python output file(s).\n";
                };

                if (dchoice == 1) {
                    if (saved.empty()) {
                        std::cout << "No saved results.\n";
                    } else {
                        gpu_bench::PrintResultsTable(saved);
                        std::cout << "Enter numbers to delete (e.g. 1,3,5 or 'all', Enter to go back): #"
                                  << std::flush;
                        std::string did;
                        if (std::getline(std::cin, did) && !did.empty()) {
                            if (did == "all") {
                                gpu_bench::ClearResults();
                                std::cout << "All results cleared.\n";
                            } else {
                                std::vector<std::string> idsToDelete;
                                std::istringstream ss(did);
                                std::string token;
                                while (std::getline(ss, token, ',')) {
                                    int idx = 0;
                                    if (!SafeStoi(token, idx)) {
                                        std::cout << "Invalid input: " << token << "\n";
                                        continue;
                                    }
                                    --idx;
                                    if (idx >= 0 && idx < static_cast<int>(saved.size()))
                                        idsToDelete.push_back(saved[idx].id);
                                    else
                                        std::cout << "Invalid number: " << (idx + 1) << "\n";
                                }
                                for (const auto& id : idsToDelete) {
                                    if (gpu_bench::DeleteResult(id))
                                        std::cout << "Deleted: " << id << "\n";
                                }
                                if (!idsToDelete.empty())
                                    std::cout << "Deleted " << idsToDelete.size() << " result(s).\n";
                            }
                        }
                    }
                } else if (dchoice == 2) {
                    deletePythonOutputs();
                } else if (dchoice == 3) {
                    gpu_bench::ClearResults();
                    std::cout << "All benchmark results cleared.\n";
                    deletePythonOutputs();
                }
                continue;
            } else if (mchoice == 5 || mchoice == 6) {
                // ---- Full Analysis: benchmark + RenderDoc capture + Python charts ----
                // [5] = one GPU (user selects, default best), [6] = every GPU
                struct RunAllEntry {
                    std::int32_t gpuIdx;
                    std::string  backendId;
                    std::string  gpuName;
                    std::string  apiLabel;
                    std::int64_t luidHigh = 0;
                    std::int64_t luidLow  = 0;
                    std::uint32_t vramMB  = 0;
                    std::uint32_t nodeIndex = 0;
                    std::uint32_t nodeCount = 1;
                };

                std::int32_t selectedGpuForAll = -1;

                if (mchoice == 5) {
                    PrintGpuTable(gpus);
                    std::uint32_t defaultGpu = static_cast<std::uint32_t>(recommendedGpuIdx);
                    std::cout << "Select GPU [0-" << (gpus.size() - 1)
                              << "] (default: " << defaultGpu << "): " << std::flush;
                    std::string gline;
                    std::uint32_t gchoice = defaultGpu;
                    if (std::getline(std::cin, gline) && !gline.empty()) {
                        int tmp = 0;
                        if (SafeStoi(gline, tmp) && tmp >= 0 &&
                            static_cast<std::uint32_t>(tmp) < gpus.size())
                            gchoice = static_cast<std::uint32_t>(tmp);
                    }
                    selectedGpuForAll = static_cast<std::int32_t>(gchoice);
                }

                std::vector<RunAllEntry> entries;
                for (std::uint32_t gi = 0; gi < gpus.size(); ++gi) {
                    if (selectedGpuForAll >= 0 &&
                        gi != static_cast<std::uint32_t>(selectedGpuForAll))
                        continue;

                    const auto& g = gpus[gi];
                    auto faRawIdx = [&](const std::string& bid) -> std::int32_t {
                        if (g.isSoftware) return -2;
                        if (bid == "vulkan") return g.vkPhysDevIndex;
                        if (bid == "dx11" || bid == "dx12") return g.dxgiRawIndex;
                        return static_cast<std::int32_t>(gi);
                    };
#ifdef HAVE_METAL
                    if (g.supportsMetal)
                        entries.push_back({faRawIdx("metal"), "metal", g.name, "Metal", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#endif
#ifdef HAVE_VULKAN
                    if (g.supportsVulkan)
                        entries.push_back({faRawIdx("vulkan"), "vulkan", g.name, "Vulkan", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#endif
#ifdef HAVE_DX12
                    if (g.supportsDX12)
                        entries.push_back({faRawIdx("dx12"), "dx12", g.name, "DirectX 12", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#endif
#ifdef HAVE_DX11
                    if (g.supportsDX11)
                        entries.push_back({faRawIdx("dx11"), "dx11", g.name, "DirectX 11", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#endif
#ifdef HAVE_OPENGL
                    if (g.supportsOpenGL) {
#ifdef _WIN32
                        if (gi == 0)
                            entries.push_back({faRawIdx("opengl"), "opengl", g.name, "OpenGL 4.3", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#else
                        entries.push_back({faRawIdx("opengl"), "opengl", g.name, "OpenGL 4.3", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#endif
                    }
#endif
                }

                if (entries.empty()) {
                    std::cout << "No runnable API combinations found.\n";
                    continue;
                }

                std::string scope = (mchoice == 5) ? "One GPU" : "All GPUs";
                std::cout << "\n====== Full Analysis (" << scope << "): "
                          << entries.size() << " benchmark(s) ======\n";
                for (std::uint32_t i = 0; i < entries.size(); ++i) {
                    std::cout << "  [" << (i + 1) << "] " << entries[i].apiLabel
                              << " / " << entries[i].gpuName;
#ifdef _WIN32
                    if (entries[i].backendId == "opengl")
                        std::cout << "  (Win: always uses system default GPU)";
#endif
                    std::cout << "\n";
                }
                std::cout << "  + RenderDoc capture at 5s mark (if RenderDoc detected)\n"
                          << "  + Python chart generation after all runs\n"
                          << "=======================================================\n"
                          << "Proceed? (Y/n): " << std::flush;
                std::string confirm;
                std::getline(std::cin, confirm);
                if (!confirm.empty() && confirm[0] != 'Y' && confirm[0] != 'y')
                    continue;

                gpu_bench::BenchmarkConfig faCfg;
                faCfg.particleCount = 1048576;
                faCfg.difficultyLabel = "Medium";
                faCfg.particlesOverridden = true;
                faCfg.vsync = false;
                faCfg.captureAtSec = 5.0;

                std::uint32_t passed = 0, failed = 0;
                std::vector<std::string> rdcFiles;

                for (std::uint32_t i = 0; i < entries.size(); ++i) {
                    const auto& e = entries[i];
                    faCfg.gpuDisplayName = e.gpuName;
                    faCfg.adapterLuidHigh = e.luidHigh;
                    faCfg.adapterLuidLow  = e.luidLow;
                    faCfg.vramMB          = e.vramMB;
                    faCfg.adapterNodeIndex = e.nodeIndex;
                    faCfg.adapterNodeCount = e.nodeCount;
                    std::cout << "\n>>> [" << (i + 1) << "/" << entries.size()
                              << "] " << e.apiLabel << " / " << e.gpuName
                              << " (15s + RenderDoc @ 5s) <<<\n";
                    try {
                        std::unique_ptr<gpu_bench::AppBase> app;
                        gpu_bench::BenchmarkConfig entryCfg = faCfg;
                        DisableRenderDocForSecondaryDx12Node(entryCfg, e.backendId);
#ifdef HAVE_VULKAN
                        if (e.backendId == "vulkan")
                            app = std::make_unique<gpu_bench::VulkanBackend>(
                                e.gpuIdx, shaderDir, entryCfg);
#endif
#ifdef HAVE_DX12
                        if (e.backendId == "dx12")
                            app = std::make_unique<gpu_bench::DX12Backend>(
                                e.gpuIdx, shaderDir, entryCfg);
#endif
#ifdef HAVE_DX11
                        if (e.backendId == "dx11")
                            app = std::make_unique<gpu_bench::DX11Backend>(
                                e.gpuIdx, shaderDir, entryCfg);
#endif
#ifdef HAVE_METAL
                        if (e.backendId == "metal")
                            app = std::make_unique<gpu_bench::MetalBackend>(
                                e.gpuIdx, shaderDir, entryCfg);
#endif
#ifdef HAVE_OPENGL
                        if (e.backendId == "opengl") {
#ifdef _WIN32
                            std::cout << "  NOTE: OpenGL on Windows cannot select GPU "
                                         "- using system default.\n";
#endif
                            app = std::make_unique<gpu_bench::OpenGLBackend>(
                                e.gpuIdx, shaderDir, entryCfg);
                        }
#endif
                        if (!app) {
                            std::cout << "  SKIPPED (backend not available)\n";
                            ++failed;
                            continue;
                        }
                        app->Run();
                        if (!app->GetLastCapturePath().empty())
                            rdcFiles.push_back(app->GetLastCapturePath());
                        ++passed;
                    } catch (const gpu_bench::BackToMenuException&) {
                        std::cout << "  SKIPPED (user cancelled)\n";
                        ++failed;
                    } catch (const std::exception& ex) {
                        std::cout << "  FAILED: " << ex.what() << "\n";
                        ++failed;
                    }
                }

                std::cout << "\n========== Benchmark Phase Complete ==========\n"
                          << "  Passed: " << passed << " / " << entries.size() << "\n";
                if (failed > 0)
                    std::cout << "  Failed/Skipped: " << failed << "\n";

                auto allResults = gpu_bench::LoadResults();
                if (!allResults.empty())
                    gpu_bench::PrintComparisonTable(allResults);

                // shaderDir = exe directory (e.g. build/Release/)
                // project root is two levels up: build/Release/../../
#ifdef _WIN32
                std::string sep = "\\";
#else
                std::string sep = "/";
#endif
                std::string projectRoot = shaderDir + ".." + sep + "..";
                std::string scriptsDir = projectRoot + sep + "scripts" + sep;
                std::string docsDir = projectRoot + sep + "docs" + sep;
                std::string rdocCapDir = projectRoot + sep + "rdoc_captures" + sep;

                // Helper: run a command inside the project root to avoid
                // space-in-path issues with cmd.exe quoting.
                auto runInProjectRoot = [&](const std::string& innerCmd) -> int {
#ifdef _WIN32
                    std::string full = "\"cd /d \"" + projectRoot
                        + "\" && " + innerCmd + "\"";
#else
                    std::string full = "cd \"" + projectRoot
                        + "\" && " + innerCmd;
#endif
                    return std::system(full.c_str());
                };

                // ---- RenderDoc capture -> Chrome JSON conversion ----
                if (!rdcFiles.empty()) {
                    std::cout << "\n========== Converting RenderDoc Captures ==========\n";
                    for (std::uint32_t ci = 0; ci < rdcFiles.size(); ++ci) {
                        std::string jsonOut = rdcFiles[ci];
                        auto dotPos = jsonOut.rfind('.');
                        if (dotPos != std::string::npos)
                            jsonOut = jsonOut.substr(0, dotPos);
                        jsonOut += ".json";

#ifdef _WIN32
                        std::string cmd =
                            "\"\"C:\\Program Files\\RenderDoc\\renderdoccmd.exe\" convert"
                            " -f \"" + rdcFiles[ci] + "\""
                            " -c chrome.json"
                            " -o \"" + jsonOut + "\"\"";
#else
                        std::string cmd = "renderdoccmd convert"
                            " -f \"" + rdcFiles[ci] + "\""
                            " -c chrome.json"
                            " -o \"" + jsonOut + "\"";
#endif

                        std::cout << "  [" << (ci + 1) << "/" << rdcFiles.size()
                                  << "] " << rdcFiles[ci] << "\n";
                        int rcConv = std::system(cmd.c_str());
                        if (rcConv != 0)
                            std::cout << "    WARNING: conversion failed (exit "
                                      << rcConv << ")\n";
                        else
                            std::cout << "    -> " << jsonOut << "\n";
                    }
                    std::cout << "====================================================\n";
                }

                // ---- RenderDoc timing analysis ----
                std::string resultsPath = gpu_bench::ResultsFilePath();
                if (!rdcFiles.empty()) {
                    std::string rdocCmd =
                        "python scripts" + sep + "rdoc_analyse.py"
                        " --captures rdoc_captures"
                        " --results \"" + resultsPath + "\""
                        " --output docs" + sep + "rdoc_comparison.md";
                    std::cout << "\n========== RenderDoc Timing Analysis ==========\n";
                    int rcRdoc = runInProjectRoot(rdocCmd);
                    if (rcRdoc != 0)
                        std::cout << "  WARNING: rdoc_analyse.py failed (exit "
                                  << rcRdoc << ")\n";
                    else
                        std::cout << "  Report saved to docs/rdoc_comparison.md\n";
                    std::cout << "================================================\n";
                }

                // ---- Python chart generation ----
                std::cout << "\n========== Generating Python Charts ==========\n";

                std::string cmdPlot =
                    "python scripts" + sep + "plot_results.py --save docs" + sep + "images";
                std::string cmdExportMd =
                    "python scripts" + sep + "export_report.py --md docs" + sep + "results-table.md";
                std::string cmdExportHtml =
                    "python scripts" + sep + "export_report.py --html docs" + sep + "report.html";

                std::cout << "  [1/3] Generating charts...\n";
                int rc1 = runInProjectRoot(cmdPlot);
                if (rc1 != 0)
                    std::cout << "  WARNING: plot_results.py failed (exit " << rc1
                              << "). Is matplotlib installed? Run: pip install -r scripts/requirements.txt\n";
                else
                    std::cout << "  Charts saved to docs/images/\n";

                std::cout << "  [2/3] Exporting Markdown table...\n";
                int rc2 = runInProjectRoot(cmdExportMd);
                if (rc2 != 0)
                    std::cout << "  WARNING: export_report.py --md failed (exit " << rc2 << ")\n";
                else
                    std::cout << "  Markdown table saved to docs/results-table.md\n";

                std::cout << "  [3/3] Exporting HTML report...\n";
                int rc3 = runInProjectRoot(cmdExportHtml);
                if (rc3 != 0)
                    std::cout << "  WARNING: export_report.py --html failed (exit " << rc3 << ")\n";
                else
                    std::cout << "  HTML report saved to docs/report.html\n";

                std::cout << "\n========== Full Analysis Complete ==========\n";
                if (rc1 == 0 && rc2 == 0 && rc3 == 0)
                    std::cout << "All outputs generated successfully.\n";
                else
                    std::cout << "Some Python scripts failed. Check if dependencies are installed:\n"
                              << "  pip install -r scripts/requirements.txt\n";
                std::cout << "============================================\n";

                hasLastRun = (passed > 0);
                directBenchmark = false;
                continue;

            } else if (mchoice == 7 || mchoice == 8 || mchoice == 9) {
                // ---- Options 7/8/9: Flights test / Particle test / Headless compute ----
                // Similar to option 5 (Full analysis - one GPU) but with specific overrides.
                struct RunAllEntry {
                    std::int32_t gpuIdx;
                    std::string  backendId;
                    std::string  gpuName;
                    std::string  apiLabel;
                    std::int64_t luidHigh = 0;
                    std::int64_t luidLow  = 0;
                    std::uint32_t vramMB  = 0;
                    std::uint32_t nodeIndex = 0;
                    std::uint32_t nodeCount = 1;
                };

                PrintGpuTable(gpus);
                std::uint32_t defaultGpu = static_cast<std::uint32_t>(recommendedGpuIdx);
                std::cout << "Select GPU [0-" << (gpus.size() - 1)
                          << "] (default: " << defaultGpu << "): " << std::flush;
                std::string gline;
                std::uint32_t gchoice = defaultGpu;
                if (std::getline(std::cin, gline) && !gline.empty()) {
                    int tmp = 0;
                    if (SafeStoi(gline, tmp) && tmp >= 0 &&
                        static_cast<std::uint32_t>(tmp) < gpus.size())
                        gchoice = static_cast<std::uint32_t>(tmp);
                }
                gpu_bench::BenchmarkConfig testCfg;
                testCfg.particleCount = 1048576;
                testCfg.difficultyLabel = "Medium";
                testCfg.particlesOverridden = true;
                testCfg.vsync = false;

                std::string testLabel;
                if (mchoice == 7) {
                    // Flights test: ask for flights count
                    std::cout << "Enter frames-in-flight count (default: 2): " << std::flush;
                    std::string fline;
                    int flights = 2;
                    if (std::getline(std::cin, fline) && !fline.empty())
                        SafeStoi(fline, flights);
                    if (flights < 1) flights = 1;
                    if (flights > 16) flights = 16;
                    testCfg.framesInFlight = static_cast<std::uint32_t>(flights);
                    testCfg.captureAtSec = 5.0;  // RenderDoc capture
                    testLabel = "Flights=" + std::to_string(flights);
                } else if (mchoice == 8) {
                    // Particle count test: ask for particle count
                    struct Preset { const char* name; std::uint32_t count; };
                    static const Preset presets[] = {
                        {"Light",   65536},
                        {"Medium",  1048576},
                        {"Heavy",   4194304},
                        {"Extreme", 16777216},
                    };
                    std::cout << "\nParticle count presets:\n";
                    for (int p = 0; p < 4; ++p) {
                        std::cout << "  [" << p << "] " << presets[p].name
                                  << " (" << presets[p].count << ")";
                        if (p == 1) std::cout << "  <- default";
                        std::cout << "\n";
                    }
                    std::cout << "Select [0-3] or enter custom count (default: 1): " << std::flush;
                    std::string pline;
                    std::uint32_t pchoice = 1;
                    if (std::getline(std::cin, pline) && !pline.empty()) {
                        int tmp = 0;
                        if (SafeStoi(pline, tmp)) {
                            if (tmp >= 0 && tmp <= 3)
                                pchoice = static_cast<std::uint32_t>(tmp);
                            else if (tmp > 3) {
                                // Treat as raw particle count, round to 256
                                testCfg.particleCount = static_cast<std::uint32_t>((tmp / 256) * 256);
                                if (testCfg.particleCount == 0) testCfg.particleCount = 256;
                                testCfg.difficultyLabel = "Custom";
                                pchoice = 99;  // skip preset
                            }
                        }
                    }
                    if (pchoice <= 3) {
                        testCfg.particleCount = presets[pchoice].count;
                        testCfg.difficultyLabel = presets[pchoice].name;
                    }
                    testCfg.captureAtSec = 5.0;  // RenderDoc capture
                    testLabel = "Particles=" + std::to_string(testCfg.particleCount);
                } else {
                    // Headless compute: no rendering, no RenderDoc
                    testCfg.headless = true;
                    testCfg.captureAtSec = -1.0;  // no RenderDoc
                    testLabel = "Headless";
                }

                // Build entry list for the selected GPU
                std::vector<RunAllEntry> entries;
                {
                    const auto& g = gpus[gchoice];
                    auto faRawIdx = [&](const std::string& bid) -> std::int32_t {
                        if (g.isSoftware) return -2;
                        if (bid == "vulkan") return g.vkPhysDevIndex;
                        if (bid == "dx11" || bid == "dx12") return g.dxgiRawIndex;
                        return static_cast<std::int32_t>(gchoice);
                    };
#ifdef HAVE_METAL
                    if (g.supportsMetal)
                        entries.push_back({faRawIdx("metal"), "metal", g.name, "Metal", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#endif
#ifdef HAVE_VULKAN
                    if (g.supportsVulkan)
                        entries.push_back({faRawIdx("vulkan"), "vulkan", g.name, "Vulkan", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#endif
#ifdef HAVE_DX12
                    if (g.supportsDX12)
                        entries.push_back({faRawIdx("dx12"), "dx12", g.name, "DirectX 12", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#endif
#ifdef HAVE_DX11
                    if (g.supportsDX11)
                        entries.push_back({faRawIdx("dx11"), "dx11", g.name, "DirectX 11", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#endif
#ifdef HAVE_OPENGL
                    if (g.supportsOpenGL) {
#ifdef _WIN32
                        if (gchoice == 0)
                            entries.push_back({faRawIdx("opengl"), "opengl", g.name, "OpenGL 4.3", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#else
                        entries.push_back({faRawIdx("opengl"), "opengl", g.name, "OpenGL 4.3", g.luidHigh, g.luidLow, static_cast<std::uint32_t>(g.vramMB), g.dx12NodeIndex, g.dx12NodeCount});
#endif
                    }
#endif
                }

                if (entries.empty()) {
                    std::cout << "No runnable API combinations found.\n";
                    continue;
                }

                std::cout << "\n====== " << testLabel << " Test: "
                          << entries.size() << " benchmark(s) ======\n";
                for (std::uint32_t i = 0; i < entries.size(); ++i) {
                    std::cout << "  [" << (i + 1) << "] " << entries[i].apiLabel
                              << " / " << entries[i].gpuName << "\n";
                }
                if (testCfg.captureAtSec > 0.0)
                    std::cout << "  + RenderDoc capture at 5s mark (if RenderDoc detected)\n";
                if (testCfg.headless)
                    std::cout << "  + Pure compute mode (no window/rendering/present)\n";
                std::cout << "=======================================================\n"
                          << "Proceed? (Y/n): " << std::flush;
                std::string confirm;
                std::getline(std::cin, confirm);
                if (!confirm.empty() && confirm[0] != 'Y' && confirm[0] != 'y')
                    continue;

                std::uint32_t passed = 0, failed = 0;
                std::vector<std::string> rdcFiles;

                for (std::uint32_t i = 0; i < entries.size(); ++i) {
                    const auto& e = entries[i];
                    testCfg.gpuDisplayName = e.gpuName;
                    testCfg.adapterLuidHigh = e.luidHigh;
                    testCfg.adapterLuidLow  = e.luidLow;
                    testCfg.vramMB          = e.vramMB;
                    testCfg.adapterNodeIndex = e.nodeIndex;
                    testCfg.adapterNodeCount = e.nodeCount;
                    std::cout << "\n>>> [" << (i + 1) << "/" << entries.size()
                              << "] " << e.apiLabel << " / " << e.gpuName
                              << " (" << testLabel << ", 15s";
                    if (testCfg.captureAtSec > 0.0)
                        std::cout << " + RenderDoc @ 5s";
                    std::cout << ") <<<\n";
                    try {
                        std::unique_ptr<gpu_bench::AppBase> app;
                        gpu_bench::BenchmarkConfig entryCfg = testCfg;
                        DisableRenderDocForSecondaryDx12Node(entryCfg, e.backendId);
#ifdef HAVE_VULKAN
                        if (e.backendId == "vulkan")
                            app = std::make_unique<gpu_bench::VulkanBackend>(
                                e.gpuIdx, shaderDir, entryCfg);
#endif
#ifdef HAVE_DX12
                        if (e.backendId == "dx12")
                            app = std::make_unique<gpu_bench::DX12Backend>(
                                e.gpuIdx, shaderDir, entryCfg);
#endif
#ifdef HAVE_DX11
                        if (e.backendId == "dx11")
                            app = std::make_unique<gpu_bench::DX11Backend>(
                                e.gpuIdx, shaderDir, entryCfg);
#endif
#ifdef HAVE_METAL
                        if (e.backendId == "metal")
                            app = std::make_unique<gpu_bench::MetalBackend>(
                                e.gpuIdx, shaderDir, entryCfg);
#endif
#ifdef HAVE_OPENGL
                        if (e.backendId == "opengl") {
#ifdef _WIN32
                            std::cout << "  NOTE: OpenGL on Windows cannot select GPU "
                                         "- using system default.\n";
#endif
                            app = std::make_unique<gpu_bench::OpenGLBackend>(
                                e.gpuIdx, shaderDir, entryCfg);
                        }
#endif
                        if (!app) {
                            std::cout << "  SKIPPED (backend not available)\n";
                            ++failed;
                            continue;
                        }
                        app->Run();
                        if (!app->GetLastCapturePath().empty())
                            rdcFiles.push_back(app->GetLastCapturePath());
                        ++passed;
                    } catch (const gpu_bench::BackToMenuException&) {
                        std::cout << "  SKIPPED (user cancelled)\n";
                        ++failed;
                    } catch (const std::exception& ex) {
                        std::cout << "  FAILED: " << ex.what() << "\n";
                        ++failed;
                    }
                }

                std::cout << "\n========== " << testLabel << " Test Complete ==========\n"
                          << "  Passed: " << passed << " / " << entries.size() << "\n";
                if (failed > 0)
                    std::cout << "  Failed/Skipped: " << failed << "\n";

                auto allResults = gpu_bench::LoadResults();
                if (!allResults.empty())
                    gpu_bench::PrintComparisonTable(allResults);

                // RenderDoc capture conversion (same as option 5)
                if (!rdcFiles.empty()) {
                    std::cout << "\n========== Converting RenderDoc Captures ==========\n";
                    for (std::uint32_t ci = 0; ci < rdcFiles.size(); ++ci) {
                        std::string jsonOut = rdcFiles[ci];
                        auto dotPos = jsonOut.rfind('.');
                        if (dotPos != std::string::npos)
                            jsonOut = jsonOut.substr(0, dotPos);
                        jsonOut += ".json";

#ifdef _WIN32
                        std::string cmd =
                            "\"\"C:\\Program Files\\RenderDoc\\renderdoccmd.exe\" convert"
                            " -f \"" + rdcFiles[ci] + "\""
                            " -c chrome.json"
                            " -o \"" + jsonOut + "\"\"";
#else
                        std::string cmd = "renderdoccmd convert"
                            " -f \"" + rdcFiles[ci] + "\""
                            " -c chrome.json"
                            " -o \"" + jsonOut + "\"";
#endif
                        std::cout << "  [" << (ci + 1) << "/" << rdcFiles.size()
                                  << "] " << rdcFiles[ci] << "\n";
                        int rcConv = std::system(cmd.c_str());
                        if (rcConv != 0)
                            std::cout << "    WARNING: conversion failed (exit "
                                      << rcConv << ")\n";
                        else
                            std::cout << "    -> " << jsonOut << "\n";
                    }
                    std::cout << "====================================================\n";
                }

                std::cout << "============================================\n";

                hasLastRun = (passed > 0);
                directBenchmark = false;
                continue;

            } else if (mchoice == 10) {
                return 0;
            } else {
                continue;
            }
        }

        // ---- Backend selection (when backend == "auto") ----
        std::string selectedBackend = backend;
        bool warp = useWarp;

        // Non-interactive callers (notably the WinUI front-end) may select a
        // GPU while leaving the API on Auto. Resolve against that GPU's actual
        // capability set instead of choosing the first globally available
        // hardware API. A selected DXGI software adapter means WARP.
        if (selectedBackend == "auto" && directBenchmark) {
            std::int32_t targetIdx = gpuIndex;
            if (targetIdx < 0 || static_cast<std::size_t>(targetIdx) >= gpus.size())
                targetIdx = recommendedGpuIdx;
            if (targetIdx < 0 || static_cast<std::size_t>(targetIdx) >= gpus.size()) {
                std::cerr << "No GPU is available for automatic API selection.\n";
                return 1;
            }

            const auto& target = gpus[static_cast<std::size_t>(targetIdx)];
            if (target.supportsMetal)         selectedBackend = "metal";
            else if (target.supportsVulkan)   selectedBackend = "vulkan";
            else if (target.supportsDX12)     selectedBackend = "dx12";
            else if (target.supportsDX11 &&
                     (gpu_bench::isFragmentOnlyWorkload(benchCfg.workload) ||
                      target.supportsDX11Compute))
                                                selectedBackend = "dx11";
            else if (target.supportsOpenGL)   selectedBackend = "opengl";
            else {
                std::cerr << "The selected GPU exposes no supported graphics API.\n";
                return 1;
            }
            if (target.isSoftware &&
                (selectedBackend == "dx11" || selectedBackend == "dx12"))
                warp = true;
            std::cout << "[Graphics API] Auto-selected " << selectedBackend
                      << " for " << target.name << "\n";
        }

        if (selectedBackend == "auto") {
            if (available.empty()) {
                std::cerr << "No Graphics API available.\n";
                return 1;
            }

            // Find the best default: first hardware backend, else first overall.
            std::uint32_t defaultChoice = 0;
            for (std::uint32_t i = 0; i < available.size(); ++i) {
                if (available[i].hwOnly) { defaultChoice = i; break; }
            }

            PrintGpuTable(gpus);
            std::cout << "Available Graphics APIs:\n";
            for (std::uint32_t i = 0; i < available.size(); ++i) {
                std::string note;
                if (available[i].id == "metal")       note = "Metal";
                else if (available[i].id == "vulkan")  note = "Vulkan";
                else if (available[i].id == "dx12")    note = "DirectX 12";
                else if (available[i].id == "dx11")    note = "DirectX 11";
                else if (available[i].id == "opengl")  note = "OpenGL 4.3";
                if (!available[i].hwOnly)
                    note += "  [Software only - runs on CPU]";
                if (i == defaultChoice)
                    note += "  <- default";
                std::cout << "  [" << i << "] " << note << "\n";
            }

            std::cout << "Select Graphics API [0-" << (available.size() - 1)
                      << "] (default: " << defaultChoice << "): " << std::flush;
            std::string line;
            std::uint32_t choice = defaultChoice;
            if (std::getline(std::cin, line) && !line.empty()) {
                int tmp = 0;
                if (!SafeStoi(line, tmp) || tmp < 0 ||
                    static_cast<std::uint32_t>(tmp) >= available.size()) {
                    std::cerr << "Invalid index, try again.\n\n";
                    continue;
                }
                choice = static_cast<std::uint32_t>(tmp);
            }
            selectedBackend = available[choice].id;
            std::cout << "[Graphics API] Selected: " << selectedBackend << std::endl;
        }

        // --warp only has meaning for D3D. Normalize it before validating an
        // explicit GPU/API pair so it cannot bypass the capability check for
        // Vulkan, Metal, or OpenGL.
        if (warp && selectedBackend != "dx11" && selectedBackend != "dx12") {
            warp = false;
        }

        // A GUI may deliberately schedule an unsupported GPU/API pair so the
        // result matrix records a clear failure. Never translate a missing
        // backend-specific index (-1) into "automatic" and silently run a
        // different adapter.
        if (!warp && gpuIndex >= 0) {
            if (static_cast<std::size_t>(gpuIndex) >= gpus.size()) {
                std::cerr << "GPU index " << gpuIndex << " is out of range (0-"
                          << (gpus.empty() ? 0 : gpus.size() - 1) << ").\n";
                return 1;
            }
            const auto& requested = gpus[static_cast<std::size_t>(gpuIndex)];
            bool reportedSupport = false;
            if (selectedBackend == "vulkan")      reportedSupport = requested.supportsVulkan;
            else if (selectedBackend == "dx12")  reportedSupport = requested.supportsDX12;
            else if (selectedBackend == "dx11")  reportedSupport = requested.supportsDX11;
            else if (selectedBackend == "metal") reportedSupport = requested.supportsMetal;
            else if (selectedBackend == "opengl") reportedSupport = requested.supportsOpenGL;
            if (!reportedSupport) {
                std::cerr << "GPU index " << gpuIndex << " (" << requested.name
                          << ") does not report support for backend '"
                          << selectedBackend << "'.\n";
                return 1;
            }
        }

        if (!warp && gpuIndex >= 0 &&
            static_cast<std::size_t>(gpuIndex) < gpus.size() &&
            gpus[static_cast<std::size_t>(gpuIndex)].isSoftware &&
            (selectedBackend == "dx11" || selectedBackend == "dx12")) {
            warp = true;
        }

#ifdef _WIN32
        if (selectedBackend == "opengl") {
            // WGL exposes the renderer chosen by Windows/the driver but has no
            // standard API for selecting an adapter by our DXGI/Vulkan index.
            // ProbeGpuApis marks only the adapter whose name matched
            // GL_RENDERER.  Reject a contradictory --gpu selection instead of
            // running the default renderer and saving it under the wrong GPU.
            auto glGpu = std::find_if(gpus.begin(), gpus.end(),
                [](const GpuInfo& g) { return g.supportsOpenGL; });
            if (glGpu == gpus.end()) {
                std::cerr << "OpenGL was selected, but no probed adapter matches GL_RENDERER.\n";
                if (directBenchmark) return 1;
                continue;
            }
            const auto actualGlIndex = static_cast<std::int32_t>(
                std::distance(gpus.begin(), glGpu));
            if (gpuIndex >= 0 && gpuIndex != actualGlIndex) {
                std::cerr << "OpenGL on Windows cannot select GPU index " << gpuIndex
                          << "; the active GL_RENDERER is " << glGpu->name
                          << " (GPU index " << actualGlIndex << ").\n"
                          << "Use Windows Graphics settings to change the GPU assigned "
                             "to this executable, then re-run detection.\n";
                if (directBenchmark) return 1;
                continue;
            }
            gpuIndex = actualGlIndex;
        }
#endif

#ifdef __linux__
        // On Linux, let users interactively choose a GPU for OpenGL
        // via DRI_PRIME, since OpenGL has no built-in GPU enumeration.
        if (selectedBackend == "opengl" && gpuIndex < 0 && gpus.size() > 1) {
            std::cout << "\nSelect GPU for OpenGL (DRI_PRIME):\n";
            for (std::uint32_t i = 0; i < gpus.size(); ++i) {
                std::string label = gpus[i].name;
                if (gpus[i].isSoftware)       label += " (Software)";
                else if (gpus[i].isDiscrete)  label += " (Discrete)";
                else                          label += " (Integrated)";
                std::cout << "  [" << i << "] " << label;
                if (i == 0) std::cout << "  <- default";
                std::cout << "\n";
            }
            std::cout << "Select GPU [0-" << (gpus.size() - 1)
                      << "] (default: 0): " << std::flush;
            std::string gline;
            if (std::getline(std::cin, gline) && !gline.empty()) {
                int gi = 0;
                if (SafeStoi(gline, gi) && gi >= 0 &&
                    static_cast<std::uint32_t>(gi) < gpus.size())
                    gpuIndex = static_cast<std::int32_t>(gi);
            }
        }
#endif

        // -- Difficulty selection (only when --particles not given) --
        if (!benchCfg.particlesOverridden && !benchCfg.benchmarkMode) {
            struct Preset { const char* name; std::uint32_t count; const char* vram; };
            static const Preset presets[] = {
                {"Light",   65536,    "  2 MB"},
                {"Medium",  1048576,  " 32 MB"},
                {"Heavy",   4194304,  "128 MB"},
                {"Extreme", 16777216, "512 MB"},
            };
            std::cout << "\nDifficulty presets:\n";
            for (int p = 0; p < 4; ++p) {
                std::cout << "  [" << p << "] " << presets[p].name
                          << std::string(10 - std::strlen(presets[p].name), ' ')
                          << presets[p].count << " particles (" << presets[p].vram << ")";
                if (p == 1) std::cout << "  <- default";
                std::cout << "\n";
            }
            std::cout << "Select difficulty [0-3] (default: 1): " << std::flush;
            std::string dline;
            std::uint32_t dchoice = 1;
            if (std::getline(std::cin, dline) && !dline.empty()) {
                int tmp = 0;
                if (SafeStoi(dline, tmp) && tmp >= 0 && tmp <= 3)
                    dchoice = static_cast<std::uint32_t>(tmp);
            }
            benchCfg.particleCount = presets[dchoice].count;
            benchCfg.difficultyLabel = presets[dchoice].name;
        }

        // Map gpus-array index to backend-specific raw index.
        // DX11/DX12 use the DXGI EnumAdapters1 index; Vulkan uses the
        // vkEnumeratePhysicalDevices index.  This avoids mismatches when
        // gpus ordering differs from DXGI ordering (e.g. Vulkan-inserted entries).
        std::int32_t effectiveGpuIndex = -1;
        if (warp) {
            effectiveGpuIndex = -2;
        } else if (gpuIndex >= 0 && static_cast<std::size_t>(gpuIndex) < gpus.size()) {
            if (selectedBackend == "vulkan") {
                effectiveGpuIndex = gpus[gpuIndex].vkPhysDevIndex;
            } else if (selectedBackend == "dx11" || selectedBackend == "dx12") {
                effectiveGpuIndex = gpus[gpuIndex].dxgiRawIndex;
            } else if (selectedBackend == "opengl") {
#ifdef _WIN32
                // WGL adapter selection is controlled by Windows, not by an
                // application-visible GPU index. Metadata was resolved above
                // from the adapter that matched GL_RENDERER.
                effectiveGpuIndex = -1;
#else
                effectiveGpuIndex = gpuIndex;
#endif
            } else {
                effectiveGpuIndex = gpuIndex;  // Metal: use as-is
            }
        }

        if (!warp && gpuIndex >= 0 && effectiveGpuIndex < 0 &&
            (selectedBackend == "vulkan" || selectedBackend == "dx11" ||
             selectedBackend == "dx12")) {
            std::cerr << "GPU index " << gpuIndex
                      << " has no usable backend device index for '"
                      << selectedBackend << "'.\n";
            return 1;
        }

        if (selectedBackend == "opengl" && gpus.size() > 1) {
#ifdef __linux__
            // On Linux, DRI_PRIME selects the GPU for Mesa OpenGL drivers.
            // Must be set before GLFW creates the OpenGL context.
            if (gpuIndex >= 0) {
                std::string prime = std::to_string(gpuIndex);
                setenv("DRI_PRIME", prime.c_str(), 1);
                std::cout << "[OpenGL] Set DRI_PRIME=" << prime
                          << " for GPU selection.\n" << std::endl;
            } else {
                std::cout << "\n[OpenGL] Tip: use DRI_PRIME=N to select a GPU, "
                             "e.g. DRI_PRIME=1 ./gpu_benchmark --backend opengl\n"
                          << std::endl;
            }
#elif defined(_WIN32)
            // On Windows, OpenGL has no standard per-GPU selection API.
            // NvOptimusEnablement / AmdPowerXpressRequestHighPerformance
            // (exported above) hint the driver on Optimus/PowerXpress laptops,
            // but have no effect on desktop multi-GPU systems.
            std::cout << "\n[OpenGL] Windows assigned GL_RENDERER to "
                      << gpus[static_cast<std::size_t>(gpuIndex)].name << ".\n"
                      << "  To change it, use Windows Settings > System > "
                         "Display > Graphics, then re-run detection.\n"
                      << std::endl;
#endif
        }

        // Set display name and LUID for multi-GPU disambiguation.
        // When no --gpu is given (gpuIndex == -1), fall back to the auto-
        // recommended GPU so that VRAM and display name are always populated.
        if (warp) {
            // Let the backend's real WARP device string populate the result;
            // never inherit a previously recommended hardware GPU name/LUID.
            benchCfg.gpuDisplayName.clear();
            benchCfg.adapterLuidHigh = 0;
            benchCfg.adapterLuidLow = 0;
            benchCfg.vramMB = 0;
            benchCfg.adapterNodeIndex = 0;
            benchCfg.adapterNodeCount = 1;
        } else {
            std::int32_t resolvedIdx = gpuIndex;
            if (resolvedIdx < 0 && !gpus.empty())
                resolvedIdx = recommendedGpuIdx;
            if (resolvedIdx >= 0 && static_cast<std::size_t>(resolvedIdx) < gpus.size()) {
                benchCfg.gpuDisplayName = gpus[resolvedIdx].name;
                benchCfg.adapterLuidHigh = gpus[resolvedIdx].luidHigh;
                benchCfg.adapterLuidLow  = gpus[resolvedIdx].luidLow;
                benchCfg.vramMB          = static_cast<std::uint32_t>(gpus[resolvedIdx].vramMB);
                benchCfg.adapterNodeIndex = gpus[resolvedIdx].dx12NodeIndex;
                benchCfg.adapterNodeCount = gpus[resolvedIdx].dx12NodeCount;
            }
        }

        DisableRenderDocForSecondaryDx12Node(
            benchCfg, selectedBackend, &renderDocLayerRequested);

        // -- Create and run the backend --
        try {
            std::unique_ptr<gpu_bench::AppBase> app;

            // Only Vulkan/OpenGL implement an explicit host-memory demotion path.
            // Metal particle buffers are already Shared/UMA — --host-memory is a
            // no-op there (results use memory=Unified-memory). DX11/DX12 would
            // silently stay device-local, so refuse the flag honestly.
            gpu_bench::BenchmarkConfig runCfg = benchCfg;
            if (runCfg.hostMemory && selectedBackend == "metal") {
                std::cerr << "[info] --host-memory is a no-op on Metal: particle "
                             "buffers already use Shared / Unified-memory.\n";
            } else if (runCfg.hostMemory &&
                       selectedBackend != "vulkan" &&
                       selectedBackend != "opengl") {
                std::cerr << "[warn] --host-memory (system memory) is not implemented for "
                          << selectedBackend
                          << "; this pass runs device-local (VRAM) instead.\n";
                runCfg.hostMemory = false;
            }

#ifdef HAVE_VULKAN
            if (selectedBackend == "vulkan") {
                if (!VulkanLoaderAvailable()) {
                    throw std::runtime_error(
                        "Vulkan was selected, but vulkan-1.dll is not installed. "
                        "Install a Vulkan-capable display driver or select DirectX 11/12.");
                }
                app = std::make_unique<gpu_bench::VulkanBackend>(
                    effectiveGpuIndex, shaderDir, runCfg);
            }
#endif
#ifdef HAVE_DX12
            if (selectedBackend == "dx12")
                app = std::make_unique<gpu_bench::DX12Backend>(
                    effectiveGpuIndex, shaderDir, runCfg);
#endif
#ifdef HAVE_DX11
            if (selectedBackend == "dx11")
                app = std::make_unique<gpu_bench::DX11Backend>(
                    effectiveGpuIndex, shaderDir, runCfg);
#endif
#ifdef HAVE_METAL
            if (selectedBackend == "metal")
                app = std::make_unique<gpu_bench::MetalBackend>(
                    effectiveGpuIndex, shaderDir, runCfg);
#endif
#ifdef HAVE_OPENGL
            if (selectedBackend == "opengl")
                app = std::make_unique<gpu_bench::OpenGLBackend>(
                    effectiveGpuIndex, shaderDir, runCfg);
#endif

            if (!app) {
                std::cerr << "Graphics API '" << selectedBackend
                          << "' is not compiled in or failed to initialise.\n";
                return 1;
            }

            std::cout << "Backend: " << app->GetBackendName()
                      << "  |  V-Sync: " << (benchCfg.vsync ? "ON" : "OFF")
                      << "  |  Memory: " << (runCfg.hostMemory ? "Host-visible" : "Device-local");
            if (gpu_bench::isGpuBurnWorkload(benchCfg.workload))
                std::cout << "  |  Workload: GPU Burn v1 r2 / Plasma x Kaleidoscope";
            else if (benchCfg.workload == gpu_bench::Workload::CinematicLiquid)
                std::cout << "  |  Workload: Cinematic Liquid v2 (fixed pool quality)";
            else if (benchCfg.workload == gpu_bench::Workload::CinematicLiquidV1)
                std::cout << "  |  Workload: Cinematic Liquid v1 (legacy fixed quality)";
            else
                std::cout << "  |  Particles: " << benchCfg.particleCount
                          << " (" << benchCfg.difficultyLabel << ")";
            if (benchCfg.benchmarkMode)
                std::cout << "  |  Benchmark: " << benchCfg.benchFrames << " frames";
            else if (benchCfg.maxRunTimeSec > 0.0)
                std::cout << "  |  Auto-stop: " << static_cast<int>(benchCfg.maxRunTimeSec) << "s";
            std::cout << '\n';

            app->Run();

            if (directBenchmark) break;

            hasLastRun = true;
            backend = selectedBackend;
            continue;

        } catch (const gpu_bench::BackToMenuException&) {
            std::cout << "\nReturning to menu...\n" << std::endl;
            backend = "auto";
            gpuIndex = -1;
            benchCfg.particlesOverridden = false;
            continue;
        } catch (const std::exception& ex) {
            std::cout << "Fatal error: " << ex.what() << std::endl;
            return 1;
        } catch (...) {
            std::cout << "Fatal error: unknown exception" << std::endl;
            return 1;
        }
    }

#if !defined(GPU_BENCH_NO_GLFW)
    if (!gpu_bench::skipGlfwTerminate)
        glfwTerminate();
#endif
    return 0;
}
