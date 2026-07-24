#include "app_base.h"
#include "path_service.h"
#include "renderdoc_app.h"

#if !defined(GPU_BENCH_NO_GLFW)
#include <GLFW/glfw3.h>
#elif defined(__APPLE__)
#include <mach/mach_time.h>
#elif defined(__ANDROID__) || defined(__linux__)
#include <time.h>
#endif

#include <atomic>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winnt.h>
#elif defined(__linux__)
#include <dlfcn.h>
#include <fstream>
#include <sys/utsname.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#endif

namespace {
std::atomic<bool> g_stopRequested{false};

static double gpuBenchGetTime() {
#if !defined(GPU_BENCH_NO_GLFW)
    return glfwGetTime();
#elif defined(__APPLE__)
    static mach_timebase_info_data_t tb = {};
    if (tb.denom == 0) mach_timebase_info(&tb);
    return static_cast<double>(mach_absolute_time()) * tb.numer / tb.denom / 1e9;
#else
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
#endif
}
}  // namespace

namespace gpu_bench {

void RequestStop() {
    g_stopRequested.store(true, std::memory_order_release);
}

void ClearStopRequest() {
    g_stopRequested.store(false, std::memory_order_release);
}

bool StopRequested() {
    return g_stopRequested.load(std::memory_order_acquire);
}

namespace {

// Backend timestamp queries are deliberately buffered. DX11 currently uses an
// eight-slot query ring and OpenGL uses four slots, so frames-in-flight alone
// is not a sufficient drain after RenderDoc captures a frame. Keep this above
// the largest backend ring (plus margin) so the captured query never reaches
// the formal benchmark accumulator.
constexpr std::uint32_t kCaptureTimingDrainSamples = 16;
constexpr double kShortRunWarmupFraction = 0.25;
constexpr double kCaptureEndMarginSec = 1.0;

}  // namespace

std::string AppBase::GetCpuName() {
#ifdef _WIN32
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            0, KEY_READ, &key) == ERROR_SUCCESS) {
        char buf[256]{};
        DWORD size = sizeof(buf);
        if (RegQueryValueExA(key, "ProcessorNameString", nullptr, nullptr,
                reinterpret_cast<LPBYTE>(buf), &size) == ERROR_SUCCESS) {
            RegCloseKey(key);
            std::string name(buf);
            while (!name.empty() && name.front() == ' ') name.erase(name.begin());
            return name;
        }
        RegCloseKey(key);
    }
#elif defined(__linux__)
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.rfind("model name", 0) == 0) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                std::string name = line.substr(pos + 1);
                while (!name.empty() && name.front() == ' ') name.erase(name.begin());
                return name;
            }
        }
    }
#elif defined(__APPLE__)
    char buf[256]{};
    size_t len = sizeof(buf);
    if (sysctlbyname("machdep.cpu.brand_string", buf, &len, nullptr, 0) == 0)
        return std::string(buf);
#endif
    return "Unknown";
}

std::string AppBase::GetOsVersion() {
#ifdef _WIN32
    // RtlGetVersion gives the real version even on Windows 10+
    // where GetVersionExW may be shimmed.
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    auto ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        auto fn = reinterpret_cast<RtlGetVersionFn>(
            GetProcAddress(ntdll, "RtlGetVersion"));
        if (fn) {
            RTL_OSVERSIONINFOW vi{};
            vi.dwOSVersionInfoSize = sizeof(vi);
            if (fn(&vi) == 0) {
                const auto maj = vi.dwMajorVersion;
                const auto min = vi.dwMinorVersion;
                const auto bld = vi.dwBuildNumber;

                std::string friendly;
                if (maj == 10 && bld >= 22000)
                    friendly = "Windows 11";
                else if (maj == 10)
                    friendly = "Windows 10";
                else if (maj == 6 && min == 3)
                    friendly = "Windows 8.1";
                else if (maj == 6 && min == 2)
                    friendly = "Windows 8";
                else if (maj == 6 && min == 1)
                    friendly = "Windows 7";
                else if (maj == 6 && min == 0)
                    friendly = "Windows Vista";
                else
                    friendly = "Windows";

                return friendly + " (NT "
                     + std::to_string(maj) + "."
                     + std::to_string(min) + "."
                     + std::to_string(bld) + ")";
            }
        }
    }
#elif defined(__APPLE__)
    char buf[64]{};
    size_t len = sizeof(buf);
    if (sysctlbyname("kern.osproductversion", buf, &len, nullptr, 0) == 0) {
        return "macOS " + std::string(buf);
    }
#elif defined(__linux__)
    // Try /etc/os-release for a friendly distro name
    std::ifstream osrel("/etc/os-release");
    std::string line;
    while (std::getline(osrel, line)) {
        if (line.rfind("PRETTY_NAME=", 0) == 0) {
            std::string val = line.substr(12);
            if (!val.empty() && val.front() == '"') val.erase(0, 1);
            if (!val.empty() && val.back()  == '"') val.pop_back();
            // Append kernel version
            struct utsname un{};
            if (uname(&un) == 0) val += " (kernel " + std::string(un.release) + ")";
            return val;
        }
    }
    // Fallback to kernel version
    struct utsname un{};
    if (uname(&un) == 0)
        return std::string(un.sysname) + " " + un.release;
#endif
    return "Unknown";
}

AppBase::AppBase(std::int32_t gpuIndex, std::string shaderDir,
                 BenchmarkConfig config)
    : requestedGpuIndex_(gpuIndex),
      shaderDir_(std::move(shaderDir)),
      config_(config) {
    if (!config_.benchmarkMode && config_.maxRunTimeSec > 0.0) {
        // The duration is the complete wall-clock run, including warmup. A
        // fixed two-second warmup consumed all of a 1-2 second preview and
        // left no measured frames/timestamp samples. Preserve the published
        // 15 s + 2 s warmup contract, but reserve 75% of short previews for
        // real measurement.
        const double maxWarmup =
            config_.maxRunTimeSec * kShortRunWarmupFraction;
        if (config_.warmupTimeSec > maxWarmup) {
            std::cerr << "[warn] Short timed run: reducing warmup from "
                      << config_.warmupTimeSec << "s to " << maxWarmup
                      << "s so measured frames and GPU timestamps are available.\n";
            config_.warmupTimeSec = maxWarmup;
        }

        // A capture started at the duration boundary races process teardown.
        // Keep a full second after automatic capture; a one-second run has no
        // legal automatic capture point (manual F12 remains available).
        if (config_.captureAtSec > 0.0) {
            const double latestCapture =
                config_.maxRunTimeSec - kCaptureEndMarginSec;
            if (latestCapture <= 0.0) {
                std::cerr << "[warn] Automatic RenderDoc capture disabled: "
                             "the timed run must be longer than 1 second.\n";
                config_.captureAtSec = -1.0;
            } else if (config_.captureAtSec > latestCapture) {
                std::cerr << "[warn] Automatic RenderDoc capture moved from "
                          << config_.captureAtSec << "s to " << latestCapture
                          << "s to keep the final second capture-free.\n";
                config_.captureAtSec = latestCapture;
            }
        }
    }
}

AppBase::~AppBase() {
#if !defined(GPU_BENCH_NO_GLFW)
    CleanupWindow();
#endif
}

// -----------------------------------------------------------------------
// RenderDoc In-Application API
// -----------------------------------------------------------------------

void AppBase::InitRenderDoc() {
    std::string rdocModulePath;
#ifdef _WIN32
    HMODULE mod = GetModuleHandleA("renderdoc.dll");
    if (!mod) {
        const std::filesystem::path exeDir = std::filesystem::u8path(shaderDir_);
        const std::filesystem::path candidates[] = {
            exeDir / "tools" / "RenderDoc" / "renderdoc.dll",
            exeDir / ".." / "tools" / "RenderDoc" / "renderdoc.dll",
            exeDir / ".." / ".." / "tools" / "RenderDoc" / "renderdoc.dll",
            exeDir / "renderdoc.dll",
            "C:\\Program Files\\RenderDoc\\renderdoc.dll",
        };
        for (const auto& candidate : candidates) {
            mod = LoadLibraryW(candidate.lexically_normal().wstring().c_str());
            if (mod) break;
        }
    }
    if (!mod) {
        std::cout << "[RenderDoc] Enabled, but renderdoc.dll was not found.\n";
        return;
    }
    wchar_t modulePath[32768]{};
    const DWORD modulePathLength = GetModuleFileNameW(
        mod, modulePath, static_cast<DWORD>(std::size(modulePath)));
    if (modulePathLength > 0 && modulePathLength < std::size(modulePath)) {
        rdocModulePath = std::filesystem::path(
            std::wstring(modulePath, modulePathLength)).u8string();
    }
    auto RENDERDOC_GetAPI =
        reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(mod, "RENDERDOC_GetAPI"));
#elif defined(__linux__)
    void* mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD);
    if (!mod)
        mod = dlopen("librenderdoc.so", RTLD_NOW);
    if (!mod) {
        std::cout << "[RenderDoc] Enabled, but librenderdoc.so was not found.\n";
        return;
    }
    auto RENDERDOC_GetAPI =
        reinterpret_cast<pRENDERDOC_GetAPI>(dlsym(mod, "RENDERDOC_GetAPI"));
#else
    // macOS / other: RenderDoc is unavailable. Metal backends may still provide
    // MTLCaptureManager .gputrace after InitBackend — defer unavailable until then.
    if (config_.captureAtSec > 0.0 || config_.captureAtFrame > 0) {
        std::cout << "[Capture] RenderDoc unavailable on this platform; will try "
                     "native GPU capture (MTLCaptureManager) after backend init.\n";
    }
    return;
#endif

#if defined(_WIN32) || defined(__linux__)
    if (!RENDERDOC_GetAPI) {
        std::cout << "[RenderDoc] Enabled, but the in-application API is unavailable.\n";
        return;
    }
    RENDERDOC_API_1_6_0* api = nullptr;
    int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, reinterpret_cast<void**>(&api));
    if (ret != 1 || !api) {
        std::cout << "[RenderDoc] Enabled, but API 1.6.0 could not be initialised.\n";
        return;
    }
    rdocApi_ = api;

    // The CLI installs its own unhandled-exception filter. For an isolated GUI
    // worker, let that worker report a driver/device fault through stdout and
    // its exit code instead of opening RenderDoc's separate modal reporter.
    // Standalone CLI users retain RenderDoc's normal crash-reporting behaviour.
    if (config_.guiWorker)
        api->UnloadCrashHandler();

    api->SetCaptureOptionU32(eRENDERDOC_Option_CaptureCallstacks, 1);
    api->SetCaptureOptionU32(eRENDERDOC_Option_RefAllResources, 1);
    api->SetCaptureOptionU32(eRENDERDOC_Option_SaveAllInitials, 1);

    const auto captureDir = paths::CapturesDirectory();
    rdocCaptureDir_ = captureDir.u8string();
    if (!rdocCaptureDir_.empty() &&
        rdocCaptureDir_.back() != '/' && rdocCaptureDir_.back() != '\\') {
        rdocCaptureDir_ += std::filesystem::path::preferred_separator;
    }

    int major = 0, minor = 0, patch = 0;
    api->GetAPIVersion(&major, &minor, &patch);

    std::cout << "\n============================================\n"
              << "  RenderDoc detected! (API "
              << major << "." << minor << "." << patch << ")\n"
              << (rdocModulePath.empty() ? "" : "  Loaded from: " + rdocModulePath + "\n")
              << "  Press F12 during rendering to capture a frame.\n"
              << "  Captures saved to: " << rdocCaptureDir_ << "\n"
              << "============================================\n\n" << std::flush;
#endif
}

void AppBase::TriggerRenderDocCapture() {
    if (!rdocApi_) return;
    rdocCaptureRequested_ = true;
}

std::uint32_t AppBase::GetRenderDocCaptureCount() const {
    return rdocCaptureCount_;
}

// -----------------------------------------------------------------------

void AppBase::UpdateRenderDocCapturePath() {
    if (!rdocApi_) return;
    auto* api = static_cast<RENDERDOC_API_1_6_0*>(rdocApi_);

    auto captureTag = [](std::string value, const char* fallback) {
        const std::string original = value;
        for (auto& ch : value) {
            const auto byte = static_cast<unsigned char>(ch);
            if (byte < 0x20 || ch == ' ' || ch == '.' || ch == '(' || ch == ')' ||
                std::strchr("<>:\"/\\|?*", ch) != nullptr)
                ch = '_';
        }
        value.erase(std::unique(value.begin(), value.end(),
            [](char a, char b) { return a == '_' && b == '_'; }), value.end());
        while (!value.empty() && value.front() == '_') value.erase(value.begin());
        while (!value.empty() && value.back() == '_') value.pop_back();
        if (value.empty()) value = fallback;

        // Keep the complete capture path comfortably below legacy MAX_PATH,
        // while a stable suffix prevents two long GPU names from colliding.
        if (value.size() > 80) {
            std::uint32_t hash = 2166136261u;
            for (unsigned char byte : original) {
                hash ^= byte;
                hash *= 16777619u;
            }
            std::size_t cut = 64;
            while (cut > 0 && cut < value.size() &&
                   (static_cast<unsigned char>(value[cut]) & 0xC0u) == 0x80u)
                --cut;
            value.resize(cut);
            std::ostringstream suffix;
            suffix << '_' << std::hex << std::setw(8) << std::setfill('0') << hash;
            value += suffix.str();
        }
        return value;
    };

    std::string backendTag = captureTag(GetBackendName(), "api");
    std::string gpuTag = captureTag(
        config_.gpuDisplayName.empty() ? GetDeviceName() : config_.gpuDisplayName,
        "gpu");

    std::string pathTemplate = rdocCaptureDir_ + backendTag + "_" + gpuTag
                             + "_" + workloadId(config_.workload);
    if (config_.workload == Workload::CinematicLiquidV1)
        pathTemplate += "_v1_shader" +
                        std::to_string(kCinematicLiquidShaderVersion);
    else if (config_.workload == Workload::CinematicLiquid)
        pathTemplate += "_v2_shader" +
                        std::to_string(kCinematicLiquidV2ShaderVersion);
    else if (config_.workload == Workload::GpuBurnV1)
        pathTemplate += "_v1_shader" + std::to_string(kGpuBurnV1ShaderVersion);

    // Append flights/particles info when overridden
    if (config_.framesInFlight != kMaxFramesInFlight)
        pathTemplate += "_flights" + std::to_string(config_.framesInFlight);
    if (config_.particlesOverridden &&
        (config_.workload == Workload::Stream ||
         config_.workload == Workload::Render3D))
        pathTemplate += "_particles" + std::to_string(config_.particleCount);

    api->SetCaptureFilePathTemplate(pathTemplate.c_str());
}

void AppBase::Run() {
    if ((config_.workload == Workload::GpuStressV1 ||
         isGpuBurnWorkload(config_.workload)) && config_.headless) {
        throw std::runtime_error("GPU Stress/Burn is a graphics workload and does not support headless mode");
    }
    if (!config_.headless) {
        if (config_.renderDocEnabled) {
            InitRenderDoc();
        } else {
            std::cout << "[RenderDoc] Disabled by configuration; DLL/API initialisation and F12 capture are off.\n";
        }
    }

#if !defined(GPU_BENCH_NO_GLFW)
    // OpenGL always needs a window for its GL context, even in headless mode.
    // Other backends skip window creation entirely in headless mode.
    // glfwInit() is always needed for gpuBenchGetTime() used in MainLoop.
    if (!config_.headless || NeedsOpenGLContext()) {
        InitWindow();
    } else {
        // Headless non-OpenGL: still need glfwInit for timing
        if (glfwInit() != GLFW_TRUE)
            throw std::runtime_error("glfwInit failed");
    }
#endif

    GenerateInitialParticles();
    InitBackend();

    if (!config_.headless) {
        UpdateRenderDocCapturePath();
        // Capture was requested but neither RenderDoc nor a native Metal path
        // is available — mark honest captureUnavailable (do not fake success).
        if ((config_.captureAtSec > 0.0 || config_.captureAtFrame > 0) &&
            !rdocApi_ && !SupportsNativeGpuCapture()) {
            std::cout << "[Capture] Automatic capture was requested, but no "
                         "capture backend is available. Continuing without "
                         "capture (captureUnavailable).\n";
            captureUnavailable_ = true;
        } else if ((config_.captureAtSec > 0.0 || config_.captureAtFrame > 0) &&
                   !rdocApi_ && SupportsNativeGpuCapture()) {
            std::cout << "[Capture] Using native Metal GPU capture (.gputrace).\n";
        }
#if !defined(GPU_BENCH_NO_GLFW)
        glfwShowWindow(window_);
#endif
    }

    MainLoop();
    PrintSummary();

    auto result = CollectResult();

    // Commit the sample only after backend/RenderDoc cleanup succeeds. A
    // driver fault during teardown must not leave a false-success history row.
    CleanupBackend();

    if (AppendResult(result)) {
        std::cout << "[Results] Saved as " << result.id
                  << " -> " << ResultsFilePath() << std::endl;
    }
}

#if !defined(GPU_BENCH_NO_GLFW)
void AppBase::InitWindow() {
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("glfwInit failed");
    }

    if (NeedsOpenGLContext()) {
        glfwWindowHint(GLFW_CLIENT_API,            GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,  4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,  3);
        glfwWindowHint(GLFW_OPENGL_PROFILE,         GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,  GL_TRUE);
#endif
    } else {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }
    glfwWindowHint(GLFW_RESIZABLE,  GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE,    GLFW_FALSE);

    const std::string title = "Mangekyo | " + GetBackendName() + " GPU Workload";
    window_ = glfwCreateWindow(static_cast<int>(kWindowWidth),
                               static_cast<int>(kWindowHeight),
                               title.c_str(), nullptr, nullptr);
    if (window_ == nullptr) {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow failed");
    }
}
#endif

void AppBase::GenerateInitialParticles() {
    initialParticles_.resize(config_.particleCount);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> posDist(-0.8f, 0.8f);
    std::uniform_real_distribution<float> velDist(-0.2f, 0.2f);

    // Render3D spreads particles through a 3D volume (z populated); the other
    // workloads keep the original planar (z = 0) layout.
    const bool volume3d = (config_.workload == Workload::Render3D);
    for (std::uint32_t i = 0; i < config_.particleCount; ++i) {
        initialParticles_[i] = {
            posDist(rng), posDist(rng), volume3d ? posDist(rng) : 0.0f, 1.0f,
            velDist(rng), velDist(rng), volume3d ? velDist(rng) : 0.0f, 0.0f,
        };
    }
}

void AppBase::MainLoop() {
    lastFrameTime_ = gpuBenchGetTime();
    runStartTime_  = lastFrameTime_;

    // Runtime safety net for the fixed-load GPU Burn contract: --run-all and
    // GUI matrix runs reuse one config across devices, and only here is the
    // actual device known.  Keep software rasterizers at the shared 64-step
    // Medium tier so their score is comparable without exposing watchdog-scale
    // 256/2048-step frames.
    if (isGpuBurnWorkload(config_.workload)) {
        const std::string devName = GetDeviceName();
        const bool softwareDevice =
            devName.find("Basic Render") != std::string::npos ||
            devName.find("WARP") != std::string::npos ||
            devName.find("Software") != std::string::npos ||
            devName.find("llvmpipe") != std::string::npos;
        const auto softwareCap = gpuBurnMaxFixedIter(config_.workload);
        if (softwareDevice && config_.gpuBurnIter > softwareCap) {
            std::cout << "[GPU Burn] Software device detected ('" << devName
                      << "'); clamping fixed steps " << config_.gpuBurnIter
                      << " -> " << softwareCap << " to stay watchdog-safe.\n";
            config_.gpuBurnIter = softwareCap;
        }
    }

    const std::uint32_t totalBenchFrames =
        config_.benchFrames + config_.warmupFrames;

    bool f12WasPressed = false;
    bool timeCaptureTriggered = false;
    double captureWallStart = 0.0;
    bool captureDuringMeasurement = false;

    auto shouldContinue = [&]() -> bool {
        if (StopRequested()) return false;
        if (config_.headless) return true;  // headless: exit via time/frame limit only
#if !defined(GPU_BENCH_NO_GLFW)
        return glfwWindowShouldClose(window_) == GLFW_FALSE;
#else
        return true;
#endif
    };

    while (shouldContinue()) {
#if !defined(GPU_BENCH_NO_GLFW)
        if (!config_.headless)
            glfwPollEvents();
#endif

        auto* rdoc = static_cast<RENDERDOC_API_1_6_0*>(rdocApi_);
        const bool nativeCap = !rdoc && SupportsNativeGpuCapture();
        if ((rdoc || nativeCap) && !config_.headless) {
#if !defined(GPU_BENCH_NO_GLFW)
            bool f12Down = glfwGetKey(window_, GLFW_KEY_F12) == GLFW_PRESS;
            if (f12Down && !f12WasPressed) {
                if (rdoc)
                    rdocCaptureRequested_ = true;
                else
                    nativeCaptureRequested_ = true;
            }
            f12WasPressed = f12Down;
#endif

            const double elapsed = gpuBenchGetTime() - runStartTime_;
            if (config_.captureAtSec > 0.0 && !timeCaptureTriggered &&
                elapsed >= config_.captureAtSec) {
                if (rdoc)
                    rdocCaptureRequested_ = true;
                else
                    nativeCaptureRequested_ = true;
                timeCaptureTriggered = true;
            }
            // Capture the Nth drawn frame (totalFrameCount_ is post-increment).
            if (config_.captureAtFrame > 0 && !timeCaptureTriggered &&
                static_cast<std::int64_t>(totalFrameCount_) + 1 >=
                    config_.captureAtFrame) {
                if (rdoc)
                    rdocCaptureRequested_ = true;
                else
                    nativeCaptureRequested_ = true;
                timeCaptureTriggered = true;
            }
        }

        bool capturing = false;
        bool capturingNative = false;
        if (rdoc && rdocCaptureRequested_) {
            captureWallStart = gpuBenchGetTime();
            captureDuringMeasurement = warmupDone_;
            rdoc->StartFrameCapture(nullptr, nullptr);
            capturing = true;
            rdocCaptureRequested_ = false;
        } else if (nativeCap && nativeCaptureRequested_) {
            captureWallStart = gpuBenchGetTime();
            captureDuringMeasurement = warmupDone_;
            // Unique path under the platform captures directory.
            const auto capDir = paths::CapturesDirectory();
            std::string safeName = GetDeviceName();
            for (char& c : safeName) {
                if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '|')
                    c = '_';
            }
            if (safeName.empty())
                safeName = "GPU";
            const std::string hint =
                (capDir / ("Metal_" + safeName + "_" +
                           std::to_string(static_cast<long long>(
                               gpuBenchGetTime() * 1000.0)))).string();
            if (BeginNativeGpuCapture(hint)) {
                capturingNative = true;
            } else {
                std::cout << "[Capture] Native Metal capture failed to start "
                             "(captureUnavailable for this attempt).\n";
                captureUnavailable_ = true;
            }
            nativeCaptureRequested_ = false;
        }

        const double currentTime = gpuBenchGetTime();
        const auto   deltaTime   = static_cast<float>(currentTime - lastFrameTime_);
        lastFrameTime_ = currentTime;

        DrawFrame(deltaTime);
        ++totalFrameCount_;

        if (capturingNative) {
            std::string outPath;
            const bool ok = EndNativeGpuCapture(outPath);
            const double captureWallEnd = gpuBenchGetTime();
            ++rdocCaptureAttemptCount_;
            if (captureDuringMeasurement) {
                excludedCaptureSec_ += captureWallEnd - captureWallStart;
                rdocCaptureAttemptExcluded_ = true;
            }
            timingSamplesToSkip_ = (std::max)(
                timingSamplesToSkip_,
                (std::max)(kCaptureTimingDrainSamples,
                           config_.framesInFlight + 1u));
            double capTime = gpuBenchGetTime() - runStartTime_;
            if (ok) {
                ++rdocCaptureCount_;
                lastCapturePath_ = outPath;
                std::cout << "[Capture] Metal .gputrace at " << std::fixed
                          << std::setprecision(1) << capTime << "s (frame "
                          << totalFrameCount_ << ")\n";
                if (!lastCapturePath_.empty())
                    std::cout << "  -> " << lastCapturePath_ << "\n";
            } else {
                std::cout << "[Capture] Metal capture end failed at "
                          << std::fixed << std::setprecision(1) << capTime
                          << "s\n";
                captureUnavailable_ = true;
            }
        } else if (capturing && rdoc) {
            const std::uint32_t captureResult =
                rdoc->EndFrameCapture(nullptr, nullptr);
            const double captureWallEnd = gpuBenchGetTime();
            ++rdocCaptureAttemptCount_;
            if (captureDuringMeasurement) {
                excludedCaptureSec_ += captureWallEnd - captureWallStart;
                rdocCaptureAttemptExcluded_ = true;
            }
            // Timestamp query results arrive asynchronously through backend-
            // specific query rings. Drain the largest ring, not only the
            // configured frames-in-flight window, so the captured command
            // buffer cannot leak into the formal GPU score.
            timingSamplesToSkip_ = (std::max)(
                timingSamplesToSkip_,
                (std::max)(kCaptureTimingDrainSamples,
                           config_.framesInFlight + 1u));
            double capTime = gpuBenchGetTime() - runStartTime_;
            if (captureResult == 1u) {
                ++rdocCaptureCount_;

                // Use global capture count (not per-instance) since RenderDoc
                // accumulates captures across multiple backend runs in one session.
                uint32_t numCaptures = rdoc->GetNumCaptures();
                uint32_t idx = (numCaptures > 0) ? numCaptures - 1 : 0;
                char filePath[512] = {};
                uint32_t pathLen = sizeof(filePath);
                uint64_t timestamp = 0;
                if (rdoc->GetCapture(idx, filePath, &pathLen, &timestamp))
                    lastCapturePath_ = filePath;

                std::cout << "[RenderDoc] Captured at " << std::fixed
                          << std::setprecision(1) << capTime << "s (frame "
                          << totalFrameCount_ << ")\n";
                if (!lastCapturePath_.empty())
                    std::cout << "  -> " << lastCapturePath_ << "\n";
            } else {
                std::cout << "[RenderDoc] Capture failed at " << std::fixed
                          << std::setprecision(1) << capTime << "s (frame "
                          << totalFrameCount_ << ", EndFrameCapture returned "
                          << captureResult << ")\n";
            }
            std::cout << std::flush;
        }

        const double elapsed = currentTime - runStartTime_;

        // Smooth GPU Burn's post-probe ramp: double toward the calibrated
        // step target each frame so the workload rises over a handful of
        // frames instead of one sudden FPS cliff.  Only ever set for GPU
        // Burn auto-tune; the formal window runs at the final count.
        if (gpuBurnRampTarget_ > config_.gpuBurnIter)
            config_.gpuBurnIter = std::min(gpuBurnRampTarget_,
                                           config_.gpuBurnIter * 2u);

        if (config_.benchmarkMode) {
            if (totalFrameCount_ == config_.warmupFrames) {
                benchStartTime_ = gpuBenchGetTime();
                warmupDone_ = true;
            }
            if (totalFrameCount_ > config_.warmupFrames && !capturing) {
                ++benchMeasuredFrames_;
                benchMinFrameTime_ =
                    std::min(benchMinFrameTime_,
                             static_cast<double>(deltaTime));
            }
            if (totalFrameCount_ >= totalBenchFrames) {
                benchEndTime_ = gpuBenchGetTime();
                break;
            }
        } else {
            if (!warmupDone_ && elapsed >= config_.warmupTimeSec) {
                warmupDone_ = true;
                benchStartTime_ = currentTime;
            }
            if (warmupDone_ && !capturing) {
                ++benchMeasuredFrames_;
                benchMinFrameTime_ =
                    std::min(benchMinFrameTime_,
                             static_cast<double>(deltaTime));
            }
            if (config_.maxRunTimeSec > 0.0 && elapsed >= config_.maxRunTimeSec) {
                benchEndTime_ = gpuBenchGetTime();
                break;
            }
        }

        ReportTimingIfDue(static_cast<double>(deltaTime));
    }

    if (benchEndTime_ == 0.0)
        benchEndTime_ = gpuBenchGetTime();

    WaitIdle();
}

void AppBase::AccumulateTiming(double computeMs, double renderMs,
                               double totalGpuMs) {
    if (timingSamplesToSkip_ > 0) {
        --timingSamplesToSkip_;
        return;
    }
    accumComputeMs_  += computeMs;
    accumRenderMs_   += renderMs;
    accumTotalGpuMs_ += totalGpuMs;
    ++timingSampleCount_;

    if (warmupDone_) {
        benchMinComputeMs_  = std::min(benchMinComputeMs_,  computeMs);
        benchMaxComputeMs_  = std::max(benchMaxComputeMs_,  computeMs);
        benchMinRenderMs_   = std::min(benchMinRenderMs_,   renderMs);
        benchMaxRenderMs_   = std::max(benchMaxRenderMs_,   renderMs);
        benchMinTotalGpuMs_ = std::min(benchMinTotalGpuMs_, totalGpuMs);
        benchMaxTotalGpuMs_ = std::max(benchMaxTotalGpuMs_, totalGpuMs);
        benchSumComputeMs_  += computeMs;
        benchSumRenderMs_   += renderMs;
        benchSumTotalGpuMs_ += totalGpuMs;
        ++benchSampleCount_;
    }
}

void AppBase::ReportTimingIfDue(double deltaTime) {
    timingReportTimer_ += deltaTime;
    ++frameCount_;

    // Short warmup windows give the auto-tune several closed-loop rounds
    // inside the fixed 2 s warmup; the reporting cadence after warmup (and
    // therefore every measured statistic) is unchanged.
    const double reportInterval = warmupDone_ ? kTimingReportIntervalSec : 0.30;
    if (timingReportTimer_ < reportInterval)
        return;

    const double fps = static_cast<double>(frameCount_) / timingReportTimer_;

    if (timingSampleCount_ > 0) {
        const double avgCompute = accumComputeMs_  / timingSampleCount_;
        const double avgRender  = accumRenderMs_   / timingSampleCount_;
        const double avgTotal   = accumTotalGpuMs_ / timingSampleCount_;

        const std::string devName = GetDeviceName();
        const bool sw = (devName.find("Basic Render") != std::string::npos ||
                         devName.find("WARP") != std::string::npos ||
                         devName.find("Software") != std::string::npos);

        std::cout << "[" << (sw ? "Timing" : "GPU Timing") << "] Compute: "
                  << std::fixed << std::setprecision(3)
                  << avgCompute << " ms | Render: " << avgRender
                  << " ms | Total: " << avgTotal
                  << " ms | FPS: " << static_cast<int>(fps);

        if (config_.benchmarkMode) {
            std::cout << " | Frame " << totalFrameCount_ << "/"
                      << (config_.benchFrames + config_.warmupFrames);
        }
        std::cout << std::endl;

        // Calibrate GraphicsBurn exactly once during the first warmup window.
        // The target is long enough to keep fast GPUs occupied, while four
        // independent draws retain pre-emption points and avoid a monolithic
        // watchdog-scale command on slower hardware. Explicit --iter disables
        // this path and remains fully deterministic.
        if (config_.workload == Workload::GpuStressV1 &&
            config_.gpuStressAutoTune && !gpuStressCalibrationDone_ &&
            !warmupDone_ && avgRender > 0.001) {
            const auto previous = config_.gpuStressIter;
            const double scaled = static_cast<double>(previous) *
                                  kGpuStressV1TargetFrameMs / avgRender;
            auto tuned = static_cast<std::uint32_t>(std::max(1.0, std::round(scaled)));
            tuned = std::min(tuned, kGpuStressV1MaxIter);
            if (tuned >= 8u)
                tuned = std::min(kGpuStressV1MaxIter, ((tuned + 7u) / 8u) * 8u);
            config_.gpuStressIter = tuned;
            gpuStressCalibrationDone_ = true;
            std::cout << "[GPU Stress auto-tune] " << previous << " -> " << tuned
                      << " iterations/draw (observed " << std::fixed
                      << std::setprecision(3) << avgRender << " ms, target "
                      << kGpuStressV1TargetFrameMs << " ms)\n";
        }

        // Each GPU Burn version has its own result contract while sharing the
        // safe calibration envelope. maxIter always means exact visible samples.
        //
        // Closed-loop calibration: a single probe-based projection badly
        // undershoots on APIs whose 16-step frame is dominated by fixed
        // per-frame overhead (DX11 probed 0.546 ms where Vulkan probed
        // 0.206 ms for identical work, landing at ~4.8 ms instead of 14 ms).
        // Re-projecting from each successive warmup window converges on the
        // target regardless of the overhead split; rounds only run while the
        // previous ramp has settled, and the measured window still executes
        // entirely at the final count.
        if (isGpuBurnWorkload(config_.workload) &&
            config_.gpuBurnAutoTune && !warmupDone_ && avgRender > 0.001 &&
            gpuBurnCalibrationRounds_ < 4u &&
            (gpuBurnRampTarget_ == 0u ||
             config_.gpuBurnIter == gpuBurnRampTarget_)) {
            const auto previous = config_.gpuBurnIter;
            const double target = gpuBurnTargetFrameMs(config_.workload);
            const double errorRatio = avgRender / target;
            if (gpuBurnRampTarget_ != 0u &&
                errorRatio > 0.92 && errorRatio < 1.08) {
                gpuBurnCalibrationRounds_ = 4u;  // converged; freeze
                gpuBurnCalibrationDone_ = true;
            } else {
                // Account for version-specific work outside the fixed loop
                // when projecting the current observation onto the target.
                const double fixedSampleEquivalent = 7.0;
                const double scaled =
                    (static_cast<double>(previous) + fixedSampleEquivalent) *
                    target / avgRender - fixedSampleEquivalent;
                auto tuned = static_cast<std::uint32_t>(std::max(
                    static_cast<double>(gpuBurnDefaultIter(config_.workload)),
                    std::round(scaled)));
                tuned = std::min(tuned, gpuBurnMaxIter(config_.workload));
                if (tuned >= 4u)
                    tuned = std::min(gpuBurnMaxIter(config_.workload),
                                     ((tuned + 3u) / 4u) * 4u);
                if (tuned >= previous) {
                    // Upward: per-frame doubling ramp (no FPS cliff).
                    gpuBurnRampTarget_ = tuned;
                } else {
                    // Downward correction after an overshoot: apply directly.
                    config_.gpuBurnIter = tuned;
                    gpuBurnRampTarget_ = tuned;
                }
                ++gpuBurnCalibrationRounds_;
                gpuBurnCalibrationDone_ = true;
                std::cout << "[GPU Burn v1 r2 auto-tune round "
                          << gpuBurnCalibrationRounds_ << "] "
                          << previous << " -> " << tuned
                          << " steps/draw, ramped (observed " << std::fixed
                          << std::setprecision(3) << avgRender
                          << " ms, target " << target << " ms)\n";
            }
        }

        // Thermal-stability tracking: push a window sample once warmup is done.
        if (warmupDone_)
            recordWindowSample(avgCompute, avgRender);
    } else {
        std::cout << "[FPS] " << static_cast<int>(fps) << std::endl;
    }

    if (!config_.headless && window_) {
        const double elapsed = gpuBenchGetTime() - runStartTime_;
        std::ostringstream oss;
        oss << GetBackendName();

        // Workload label
        switch (config_.workload) {
            case gpu_bench::Workload::Stream:        oss << " | Stream";    break;
            case gpu_bench::Workload::NBody:         oss << " | N-Body";    break;
            case gpu_bench::Workload::StressFractal: oss << " | Stress";    break;
            case gpu_bench::Workload::GpuStressV1:   oss << " | GPU Stress v1"; break;
            case gpu_bench::Workload::GpuBurnV1:     oss << " | Plasma x Kaleidoscope GPU Burn"; break;
            case gpu_bench::Workload::SynthPeak:     oss << " | SynthPeak"; break;
            case gpu_bench::Workload::Render3D:      oss << " | Render3D";  break;
            case gpu_bench::Workload::Volumetric:    oss << " | Volumetric"; break;
            case gpu_bench::Workload::Fluid:         oss << " | Legacy 2D Fluid"; break;
            case gpu_bench::Workload::CinematicLiquidV1: oss << " | Cinematic Liquid v1"; break;
            case gpu_bench::Workload::CinematicLiquid: oss << " | Cinematic Liquid v2"; break;
        }

        oss << "  |  FPS: " << static_cast<int>(fps);

        if (timingSampleCount_ > 0) {
            const double avgCompute = accumComputeMs_  / timingSampleCount_;
            const double avgRender  = accumRenderMs_   / timingSampleCount_;
            const double avgTotal   = accumTotalGpuMs_ / timingSampleCount_;
            oss << std::fixed << std::setprecision(2)
                << "  |  Compute: " << avgCompute << " ms"
                << "  Render: "     << avgRender  << " ms"
                << "  Total: "      << avgTotal   << " ms";
        }

        // Use static_cast<int> rather than setprecision(0) to avoid polluting
        // the stream's precision state for any code that follows.
        oss << "  |  " << static_cast<int>(elapsed) << "s";
        if (config_.maxRunTimeSec > 0.0) {
            oss << " / " << static_cast<int>(config_.maxRunTimeSec) << "s";
        } else if (config_.benchmarkMode) {
            oss << "  Frame " << totalFrameCount_ << "/"
                << (config_.benchFrames + config_.warmupFrames);
        }

#if !defined(GPU_BENCH_NO_GLFW)
        glfwSetWindowTitle(window_, oss.str().c_str());
#endif
    }

    accumComputeMs_    = 0.0;
    accumRenderMs_     = 0.0;
    accumTotalGpuMs_   = 0.0;
    timingSampleCount_ = 0;
    frameCount_        = 0;
    timingReportTimer_ = 0.0;
}

void AppBase::PrintSummary() const {
    const double duration = std::max(0.0,
        benchEndTime_ - benchStartTime_ - excludedCaptureSec_);
    const double avgFps   = (duration > 0.0)
        ? static_cast<double>(benchMeasuredFrames_) / duration
        : 0.0;

    const std::string devName = GetDeviceName();
    const bool isSoftware = (devName.find("Basic Render") != std::string::npos ||
                             devName.find("WARP") != std::string::npos ||
                             devName.find("Software") != std::string::npos);
    const char* devLabel   = isSoftware ? "CPU Renderer:" : "GPU:";
    const char* timerLabel = isSoftware ? "Device Timing (ms)" : "GPU Timing (ms)";
    const char* totalLabel = isSoftware ? "Total:      " : "Total GPU:  ";

    std::cout << "\n"
        "==========================================================\n"
        "                   Benchmark Summary\n"
        "==========================================================\n";

    const std::string driverVer = GetDriverVersion();

    std::cout << std::left << std::fixed
        << std::setw(14) << "Graphics API:" << GetBackendName() << "\n"
        << std::setw(14) << devLabel      << devName  << "\n";
    if (!driverVer.empty())
        std::cout << std::setw(14) << "Driver:" << driverVer << "\n";
    std::cout
        << std::setw(14) << "CPU:"        << GetCpuName() << "\n"
        << std::setw(14) << "OS:"         << GetOsVersion() << "\n"
        << std::setw(14) << "System:"     << CurrentPlatform() << ' '
        << CurrentOsArchitecture()
        << (CurrentProcessArchitecture() != CurrentOsArchitecture()
            ? " (" + CurrentProcessArchitecture() + " process)" : std::string{})
        << "\n"
        << std::setw(14) << "System:"     << CurrentPlatform() << ' '
        << CurrentOsArchitecture()
        << (CurrentProcessArchitecture() != CurrentOsArchitecture()
            ? " (" + CurrentProcessArchitecture() + " process)" : std::string{})
        << "\n"
        << std::setw(14) << "Memory:"     << (config_.hostMemory ? "System RAM (host-visible)" : "Device-local VRAM") << "\n"
        << std::setw(14) << "Mode:"       << (config_.headless ? "Headless (compute only)" : "Windowed") << "\n"
        << std::setw(14) << "Resolution:" << kWindowWidth << "x" << kWindowHeight << "\n";
    if (isGpuBurnWorkload(config_.workload))
        std::cout << std::setw(14) << "Workload:"
                  << "GPU Burn v1 r2 / Plasma x Kaleidoscope (no particle data)\n";
    else
        std::cout << std::setw(14) << "Particles:" << config_.particleCount
                  << " (" << config_.difficultyLabel << ")\n";
    std::cout
        << std::setw(14) << "Flights:"    << config_.framesInFlight << "\n"
        << std::setw(14) << "Multi-GPU:"
        << (config_.multiGpuMode == MultiGpuMode::Afr
                ? "AFR (2 nodes/devices)"
                : config_.multiGpuMode == MultiGpuMode::Sfr
                    ? "SFR (2 linked nodes, one frame)" : "Off") << "\n"
        << std::setw(14) << "V-Sync:"     << (config_.vsync ? "ON" : "OFF") << "\n"
        << std::setw(14) << "Duration:"
            << std::setprecision(1) << duration << " s"
            << " (warmup: " << std::setprecision(1) << config_.warmupTimeSec << " s"
            << ", measured: " << benchMeasuredFrames_ << " frames)\n";
    if (excludedCaptureSec_ > 0.0) {
        std::cout << std::setw(14) << "Capture:"
                  << std::setprecision(3) << excludedCaptureSec_
                  << " s excluded from score; async samples discarded ("
                  << rdocCaptureAttemptCount_ << " attempt(s), "
                  << rdocCaptureCount_ << " saved)\n";
    }

    if (benchSampleCount_ > 0) {
        const double avgCompute  = benchSumComputeMs_  / benchSampleCount_;
        const double avgRender   = benchSumRenderMs_   / benchSampleCount_;
        const double avgTotal    = benchSumTotalGpuMs_ / benchSampleCount_;

        std::cout << "\n--- " << timerLabel << " ---\n"
            << std::right
            << "              " << std::setw(10) << "Avg"
                                << std::setw(10) << "Min"
                                << std::setw(10) << "Max" << "\n"
            << "Compute:    " << std::setw(10) << std::setprecision(3) << avgCompute
                              << std::setw(10) << benchMinComputeMs_
                              << std::setw(10) << benchMaxComputeMs_ << "\n"
            << "Render:     " << std::setw(10) << avgRender
                              << std::setw(10) << benchMinRenderMs_
                              << std::setw(10) << benchMaxRenderMs_ << "\n"
            << totalLabel     << std::setw(10) << avgTotal
                              << std::setw(10) << benchMinTotalGpuMs_
                              << std::setw(10) << benchMaxTotalGpuMs_ << "\n";

        std::cout << "\n--- Throughput ---\n"
            << "Avg FPS:      " << static_cast<int>(avgFps) << "\n";

        if (config_.workload == Workload::Stream && avgCompute > 0.0) {
            const double n = static_cast<double>(config_.particleCount);
            const double computeSec = avgCompute / 1000.0;
            const double memoryRate = 40.0 * n / computeSec / 1e9;
            const double workingSetMiB = n * sizeof(Particle) / (1024.0 * 1024.0);
            const char* rateLabel = config_.hostMemory ? "RAM rate:    " : "VRAM rate:   ";
            std::cout << "Particles:    " << config_.particleCount << "\n"
                << std::setprecision(2)
                << "Working set:  " << workingSetMiB << " MiB\n"
                << rateLabel << memoryRate << " GB/s  ("
                << std::setprecision(3) << avgCompute << " ms compute)\n";
        }

        if (config_.workload == Workload::NBody && avgCompute > 0.0) {
            const double n            = static_cast<double>(config_.particleCount);
            const double interactions = n * n;                 // per step (all-pairs)
            const double computeSec   = avgCompute / 1000.0;
            const double gflops = interactions * kNBodyFlopsPerInteraction
                                  / computeSec / 1e9;
            const double ginteractions = interactions / computeSec / 1e9;
            std::cout << "Bodies:       " << config_.particleCount << "\n"
                << std::setprecision(2)
                << "Compute rate: " << gflops << " GFLOP/s  ("
                << ginteractions << " G-interactions/s)\n"
                << "  (" << config_.particleCount << "^2 pairs x "
                << static_cast<int>(kNBodyFlopsPerInteraction)
                << " flop / " << std::setprecision(3) << avgCompute << " ms compute)\n";
        }

        if (config_.workload == Workload::SynthPeak && avgCompute > 0.0) {
            const double threads = static_cast<double>(config_.particleCount);
            // FP16 uses packed f16vec2 accumulators: 2 lanes per op.
            const double lanes = (config_.peakPrecision == Precision::FP16) ? 2.0 : 1.0;
            const double ops = threads * config_.peakIters * kSynthPeakUnroll * lanes * 2.0;
            const double computeSec = avgCompute / 1000.0;
            const double rate = ops / computeSec / 1e9;
            const bool isInt = (config_.peakPrecision == Precision::INT32);
            const char* prec = (config_.peakPrecision == Precision::FP64)  ? "FP64"
                             : (config_.peakPrecision == Precision::FP16)  ? "FP16"
                             : (config_.peakPrecision == Precision::INT32) ? "INT32"
                             :                                               "FP32";
            std::cout << "Threads:      " << config_.particleCount
                << " x " << config_.peakIters << " iters x "
                << kSynthPeakUnroll << " FMA\n"
                << std::setprecision(1)
                << "Peak " << prec << ":    " << rate
                << (isInt ? " GIOPS" : " GFLOPS")
                << "  (" << std::setprecision(3) << avgCompute << " ms compute)\n";
        }

        if (config_.workload == Workload::Render3D && avgRender > 0.0) {
            const double mquad = config_.particleCount / (avgRender / 1000.0) / 1e6;
            std::cout << "Billboards:   " << config_.particleCount << " instances\n"
                << std::setprecision(1)
                << "Render rate:  " << mquad << " MQuad/s  ("
                << std::setprecision(3) << avgRender << " ms render)\n";
        }

        if (config_.workload == Workload::StressFractal && avgRender > 0.0) {
            const double pixels = static_cast<double>(kWindowWidth) * kWindowHeight;
            const double renderSec = avgRender / 1000.0;
            const double giters = pixels * config_.fractalIter / renderSec / 1e9;
            std::cout << "Iterations:   " << config_.fractalIter << " /pixel  ("
                << kWindowWidth << "x" << kWindowHeight << " px)\n"
                << std::setprecision(2)
                << "Fill rate:    " << giters << " G-iter/s  ("
                << std::setprecision(3) << avgRender << " ms render)\n";
        }

        if (config_.workload == Workload::GpuStressV1 && avgRender > 0.0) {
            const double pixels = static_cast<double>(kWindowWidth) * kWindowHeight;
            const double renderSec = avgRender / 1000.0;
            const double giters = pixels * config_.gpuStressIter
                                * kGpuStressV1DrawsPerFrame / renderSec / 1e9;
            std::cout << "GPU Stress:   v1, " << kGpuStressV1DrawsPerFrame
                << " draws x " << config_.gpuStressIter << " iterations/pixel  ("
                << kWindowWidth << "x" << kWindowHeight << " px)\n"
                << "Anti-DCE signal: uint checksum in R/G + FP recurrence in B\n"
                << std::setprecision(2)
                << "Stress rate:  " << giters << " Gpix-iter/s  ("
                << std::setprecision(3) << avgRender << " ms render)\n";
        }

        if (isGpuBurnWorkload(config_.workload) && avgRender > 0.0) {
            const double pixels = static_cast<double>(kWindowWidth) * kWindowHeight;
            const double renderSec = avgRender / 1000.0;
            const double gsteps = pixels * config_.gpuBurnIter
                                * gpuBurnDrawsPerFrame(config_.workload) / renderSec / 1e9;
            std::cout << "GPU Burn:     v1 r2 Plasma x Kaleidoscope, "
                << gpuBurnDrawsPerFrame(config_.workload)
                << " draws x " << config_.gpuBurnIter << " fixed steps/pixel  ("
                << kWindowWidth << "x" << kWindowHeight << " px)\n"
                << "Visual:       "
                << "solid Plasma Bloom foreground + woven Mangekyo background"
                << '\n'
                << std::setprecision(2)
                << "Burn rate:    " << gsteps << " Gpix-step/s  ("
                << std::setprecision(3) << avgRender << " ms render)\n";
        }

        if (config_.workload == Workload::Volumetric && avgRender > 0.0) {
            const double pixels = static_cast<double>(kWindowWidth) * kWindowHeight;
            const double renderSec = avgRender / 1000.0;
            const double gsamples = pixels * config_.volumetricSteps / renderSec / 1e9;
            std::cout << "Steps:        " << config_.volumetricSteps << " /pixel  ("
                << kWindowWidth << "x" << kWindowHeight << " px)\n"
                << std::setprecision(2)
                << "Vol rate:     " << gsamples << " GSample/s  ("
                << std::setprecision(3) << avgRender << " ms render)\n";
        }

        if (config_.workload == Workload::Fluid && avgCompute > 0.0) {
            // Total cells updated per frame = gridSize^2 * (4 fixed passes +
            // N Jacobi iterations). Divided by compute time -> GCell/s.
            const double g  = static_cast<double>(config_.fluidGridSize);
            const double cellsPerFrame = g * g * (4.0 + config_.fluidJacobiIters);
            const double computeSec = avgCompute / 1000.0;
            const double gcells = cellsPerFrame / computeSec / 1e9;
            std::cout << "Grid:         " << config_.fluidGridSize << "x" << config_.fluidGridSize
                << "  Jacobi iters: " << config_.fluidJacobiIters << "\n"
                << std::setprecision(2)
                << "Fluid rate:   " << gcells << " GCell/s  ("
                << std::setprecision(3) << avgCompute << " ms compute)\n";
        }

        if (isCinematicLiquidWorkload(config_.workload) &&
            avgCompute > 0.0 && avgRender > 0.0) {
            const bool v2 = config_.workload == Workload::CinematicLiquid;
            const std::uint32_t gridX = v2 ? kCinematicLiquidV2GridX : kCinematicLiquidGridX;
            const std::uint32_t gridY = v2 ? kCinematicLiquidV2GridY : kCinematicLiquidGridY;
            const std::uint32_t gridZ = v2 ? kCinematicLiquidV2GridZ : kCinematicLiquidGridZ;
            const std::uint32_t substeps =
                v2 ? (config_.liquidSolverSph ? kCinematicLiquidSphSubsteps
                                              : kCinematicLiquidV2Substeps)
                   : kCinematicLiquidSubsteps;
            const std::uint32_t raySteps = v2 ? kCinematicLiquidV2RaySteps : kCinematicLiquidRaySteps;
            const double integratedSec = (avgCompute + avgRender) / 1000.0;
            const double particleSteps = double(config_.particleCount) * substeps;
            const double rate = particleSteps / integratedSec / 1e6;
            std::cout << "Liquid:       " << config_.particleCount
                << " particles, " << gridX << "x" << gridY << "x" << gridZ
                << " grid, " << substeps << " substeps\n"
                << "Surface:      R32F density volume, "
                << raySteps << "-step raymarch";
            if (v2) {
                std::cout << ", particle Spiky^2 splat "
                    << kCinematicLiquidV2SurfaceX << "x"
                    << kCinematicLiquidV2SurfaceY << "x"
                    << kCinematicLiquidV2SurfaceZ
                    << " + adaptive 5x5x5 filter, 4-interface optics"
                    << ", finite pool overflow + clear PVC environment"
                    << ", " << kCinematicLiquidV2BodyCount
                    << " coupled rigid bodies\n";
            } else {
                std::cout << "\n";
            }
            std::cout
                << std::setprecision(2)
                << "Liquid rate:  " << rate << " MParticle-step/s  ("
                << std::setprecision(3) << avgCompute << " ms compute + "
                << avgRender << " ms render)\n";
        }

        const double avgFrameMs = (avgFps > 0.0) ? 1000.0 / avgFps : 0.0;
        const double devUtil = (avgFrameMs > 0.0) ? avgTotal / avgFrameMs : 0.0;
        const bool afr = config_.multiGpuMode == MultiGpuMode::Afr;
        const bool sfr = config_.multiGpuMode == MultiGpuMode::Sfr;
        const bool implicitAfr = afr && GetBackendName() == "DX11";
        // SFR timestamps cover the one-frame critical path on the presenting
        // node, including its wait for the secondary half.  They are not the
        // sum of two independent GPU timelines, so do not divide them by two.
        const double perDeviceUtil = afr ? devUtil / 2.0 : devUtil;

        std::cout << "\n--- Analysis ---\n"
            << "Avg frame time:  " << std::setprecision(3) << avgFrameMs << " ms\n"
            << "Avg " << (isSoftware ? "device" : "GPU") << " time: "
            << std::string(isSoftware ? 1 : 3, ' ')
            << avgTotal << " ms\n"
            << (isSoftware ? "Device" : (afr ? "GPU-equivalent" : "GPU"))
            << " utilisation: "
            << std::setprecision(1) << (devUtil * 100.0) << "%";
        if (implicitAfr) {
            std::cout << " (DX11 physical split unverified)";
        } else if (afr) {
            std::cout << " (" << (perDeviceUtil * 100.0)
                      << "% average per GPU)";
        } else if (sfr) {
            std::cout << " (SFR critical path, not summed GPU time)";
        }
        std::cout << "\n";

        if (isSoftware) {
            std::cout << ">> Software renderer -- all work runs on CPU.\n";
        } else if (implicitAfr) {
            std::cout << ">> Driver-managed AFR did not improve throughput; "
                         "physical GPU participation is unverified.\n";
        } else if (afr && devUtil > 1.6) {
            std::cout << ">> GPU-bound: both AFR devices overlap and are saturated.\n";
        } else if (afr && devUtil > 0.8) {
            std::cout << ">> AFR overlap absent: throughput is limited to about one "
                         "GPU-equivalent despite alternating device assignments.\n";
        } else if (afr) {
            std::cout << ">> AFR is under-filled: neither device receives enough "
                         "overlapping work.\n";
        } else if (sfr && perDeviceUtil > 0.8) {
            std::cout << ">> SFR critical path is GPU-bound; compare FPS against single GPU "
                         "to determine whether half-frame shading repays composition cost.\n";
        } else if (sfr) {
            std::cout << ">> SFR is not saturating the presentation critical path; compare "
                         "against AFR and single-GPU throughput before keeping it.\n";
        } else if (perDeviceUtil < 0.5) {
            std::cout << ">> CPU-bound: GPU is idle "
                      << static_cast<int>((1.0 - perDeviceUtil) * 100.0)
                      << "% of the time.\n"
                      << "   -> Try a higher difficulty for more accurate GPU benchmarking.\n";
        } else if (perDeviceUtil > 0.8) {
            std::cout << ">> GPU-bound: GPU is the bottleneck.\n";
        } else {
            std::cout << ">> Balanced: CPU and GPU workloads are roughly matched.\n";
        }
    } else {
        std::cout << "\n--- Throughput ---\n"
            << "Avg FPS:      " << static_cast<int>(avgFps)  << "\n"
            << "\n(No timestamp data available for analysis.)\n";
    }

    // ---- Thermal-stability analysis ----------------------------------------
    // Only meaningful with >=5 1-second samples (i.e. >=5s of measured run).
    if (windowScores_.size() >= 5) {
        // Recover the axis unit string for the active workload.
        std::string unit;
        switch (config_.workload) {
            case Workload::Stream:        unit = "GB/s";      break;
            case Workload::NBody:         unit = "GFLOP/s";   break;
            case Workload::StressFractal: unit = "G-iter/s";  break;
            case Workload::GpuStressV1:   unit = "Gpix-iter/s"; break;
            case Workload::GpuBurnV1:     unit = "Gpix-step/s"; break;
            case Workload::Volumetric:    unit = "GSample/s"; break;
            case Workload::Render3D:      unit = "MQuad/s";   break;
            case Workload::Fluid:         unit = "GCell/s";   break;
            case Workload::CinematicLiquidV1:
            case Workload::CinematicLiquid: unit = "MParticle-step/s"; break;
            case Workload::SynthPeak:     unit = (config_.peakPrecision == Precision::INT32) ? "GIOPS" : "GFLOPS"; break;
        }
        std::cout << "\n--- Thermal Stability ---\n"
            << "Windows:      " << windowScores_.size() << " x 1s\n";
        if (thermalStable_) {
            std::cout << std::setprecision(2)
                      << "Stable score: " << stableScore_ << " " << unit
                      << "  (CV " << stableVariancePct_ << "%)\n";
        } else {
            std::cout << "Stable score: not reached — keep running to detect throttling.\n";
        }
        if (throttlePct_ > 0.0) {
            std::cout << std::setprecision(1)
                      << "Throttling:   early vs late mean dropped " << throttlePct_ << "%";
            if (throttlePct_ > 5.0) {
                std::cout << "  (>> sustained throttle / thermal limit)";
            } else if (throttlePct_ > 2.0) {
                std::cout << "  (mild — likely normal warmup drift)";
            }
            std::cout << "\n";
        }
        if (thermalStable_ && throttlePct_ < 2.0) {
            std::cout << ">> Thermally stable; stableScore is a fair comparison metric.\n";
        } else if (throttlePct_ > 5.0) {
            std::cout << ">> Throttled: report stableScore (post-throttle), not peak.\n";
        }
    }

    std::cout << "==========================================================\n"
        << std::endl;
}

BenchmarkResult AppBase::CollectResult() const {
    BenchmarkResult r;
    r.id          = GenerateResultId();
    r.timestamp   = GenerateTimestamp();
    r.workload    = workloadId(config_.workload);
    r.resultSchemaVersion = 3;
    std::ostringstream workloadConfig;
    switch (config_.workload) {
        case Workload::Stream:
            r.workloadVersion = "stream_v1";
            workloadConfig << "particles=" << config_.particleCount
                           << ";bytesPerParticle=40";
            break;
        case Workload::NBody:
            r.workloadVersion = "nbody_v1";
            workloadConfig << "bodies=" << config_.particleCount
                           << ";softening=" << config_.softening;
            break;
        case Workload::GpuStressV1:
            r.workloadVersion = "gpu_stress_v1";
            workloadConfig << "iterations=" << config_.gpuStressIter
                           << ";draws=" << kGpuStressV1DrawsPerFrame
                           << ";shaderVersion=" << kGpuStressV1ShaderVersion
                           << ";autoTune=" << (config_.gpuStressAutoTune ? "true" : "false");
            break;
        case Workload::GpuBurnV1:
            // v3 records the selected fixed step count. Keep v2 fixed-256 and
            // calibrated r2 results in their historical groups.
            r.workloadVersion = "gpu_burn_v3_fixed_steps_"
                              + std::to_string(config_.gpuBurnIter)
                              + "_kaleidoscope";
            workloadConfig << "steps=" << config_.gpuBurnIter
                           << ";draws=" << kGpuBurnV1DrawsPerFrame
                           << ";shaderVersion=" << kGpuBurnV1ShaderVersion
                           << ";loadModel=fixed_selectable_per_frame"
                           << ";autoTune=false"
                           << ";visual=plasma_bloom_concentric_kaleidoscope";
            break;
        case Workload::StressFractal:
            r.workloadVersion = "stress_legacy_v1";
            workloadConfig << "iterations=" << config_.fractalIter;
            break;
        case Workload::SynthPeak:
            r.workloadVersion = "synthpeak_v1";
            workloadConfig << "iterations=" << config_.peakIters;
            break;
        case Workload::Render3D:
            r.workloadVersion = "render3d_legacy_v1";
            workloadConfig << "instances=" << config_.particleCount;
            break;
        case Workload::Volumetric:
            r.workloadVersion = "volumetric_experimental_v1";
            workloadConfig << "steps=" << config_.volumetricSteps;
            break;
        case Workload::Fluid:
            r.workloadVersion = "fluid_legacy_v1";
            workloadConfig << "grid=" << config_.fluidGridSize
                           << ";jacobi=" << config_.fluidJacobiIters
                           << ";fixedCellPasses=4"
                           << ";solver=stable_fluids_2d"
                           << ";renderer=projected_dye"
                           << ";status=legacy"
                           << ";apiScope=vulkan_dx12_dx11_opengl";
            break;
        case Workload::CinematicLiquidV1:
            r.workloadVersion = "cinematic_liquid_v1";
            workloadConfig << "solver=mls_mpm_3d"
                           << ";particles=" << config_.particleCount
                           << ";particleLayoutBytes=80"
                           << ";grid=" << kCinematicLiquidGridX << "x"
                           << kCinematicLiquidGridY << "x" << kCinematicLiquidGridZ
                           << ";dx=" << kCinematicLiquidDx
                           << ";substeps=" << kCinematicLiquidSubsteps
                           << ";p2g=fixed_point_atomic"
                           << ";renderer=density_volume_raymarch"
                           << ";raySteps=" << kCinematicLiquidRaySteps
                           << ";shaderVersion=" << kCinematicLiquidShaderVersion
                           << ";apiScope=vulkan_only";
            break;
        case Workload::CinematicLiquid:
            if (config_.liquidSolverSph) {
                // Experimental SPH vertical slice: an independent solver and
                // score group.  It reuses the scene/rigid/presentation stack
                // but its dynamics are the Lague-style dual-density SPH from
                // MIT jeantimex/fluid, run in the reference's own units and
                // affinely mapped into the pool.  Never rank beside MLS-MPM.
                // This slice is not eligible for a formal score at any
                // duration yet.  A 15-second visual run does not close the
                // frame-rate-driven timestep, per-substep body-impulse clear,
                // in-place viscosity data race or atomic-scatter ordering
                // blockers.  Keep every SPH result in the preview group until
                // all four correctness contracts are implemented and tested.
                r.workloadVersion = "cinematic_liquid_sph_slice_v1_preview";
                workloadConfig << "solver=sph_dual_density_lague"
                               << ";particles=" << config_.particleCount
                               << ";particleLayoutBytes=80"
                               << ";rigidBodies=" << kCinematicLiquidV2BodyCount
                               << ";coupling=per_particle_sdf_buoyancy_fixed_point"
                               << ";grassAbsorb=catch_band_soak_recycle"
                               << ";neighborSearch=block_hash50_counting_sort"
                               << ";determinism=cell_order_race_ulp_open"
                               << ";formalEligibility=blocked_four_correctness_contracts"
                               << ";smoothingRadius=" << kCinematicLiquidSphSmoothingRadius
                               << ";dtSim=1/120"
                               << ";substeps=" << kCinematicLiquidSphSubsteps
                               << ";targetDensity=" << kCinematicLiquidSphTargetDensity
                               << ";pressureMultiplier=" << kCinematicLiquidSphPressureMultiplier
                               << ";nearPressureMultiplier=" << kCinematicLiquidSphNearPressureMultiplier
                               << ";viscosityStrength=" << kCinematicLiquidSphViscosityStrength
                               << ";worldScale=" << kCinematicLiquidSphWorldScale
                               << ";scene=clear_pvc_pool_dam_break_duck_family_motor_boat_sink_sphere_grass"
                               << ";sceneVersion=5"
                               << ";seed=sph_dam_restage_4s_v1"
                               << ";poolWallInset=0.45"
                               << ";poolWallTopFraction=0.42"
                               << ";grassSoakCountdownSimSec=0.50..1.80"
                               << ";renderer=particle_spiky2_density_raymarch_iterative_fresnel4"
                               << ";surfaceVolume=" << kCinematicLiquidV2SurfaceX << "x"
                               << kCinematicLiquidV2SurfaceY << "x"
                               << kCinematicLiquidV2SurfaceZ
                               << ";raySteps=" << kCinematicLiquidV2RaySteps
                               << ";extinction=12,3.6,2.5"
                               << ";shaderVersion=" << kCinematicLiquidSphShaderVersion
                               << ";apiScope=vulkan_only";
                break;
            }
            // Only the product's fixed 15-second run belongs to the formal
            // score group. Developer visual probes (--time 3/6/11) must not
            // rank beside a run that traverses the complete choreography.
            // Every physical/optical scene revision has an isolated score
            // contract. V7 preserved the duck-family ABI while changing the
            // water distribution, boat dynamics, sink entry and finite pool
            // wall.  The later 0.45 inset, 0.42 wall-top fraction and lighter
            // extinction changed the shared scene again, so the current MPM
            // contract is V8 and must not rank with any V7 smoke result.
            // Metal raymarch present is wired, but timing/capture contracts are
            // still independent — never share the Vulkan v8 formal score group.
            if (GetBackendName() == "Metal") {
                r.workloadVersion =
                    "cinematic_liquid_v2_physical_scene_v8_metal_preview";
            } else {
                r.workloadVersion =
                    std::abs(config_.maxRunTimeSec - 15.0) < 0.001
                        ? "cinematic_liquid_v2_physical_scene_v8"
                        : "cinematic_liquid_v2_physical_scene_v8_preview";
            }
            workloadConfig << "solver=mls_mpm_3d_rigid_coupled"
                           << ";particles=" << config_.particleCount
                           << ";particleLayoutBytes=80"
                           << ";grid=" << kCinematicLiquidV2GridX << "x"
                           << kCinematicLiquidV2GridY << "x" << kCinematicLiquidV2GridZ
                           << ";dx=" << kCinematicLiquidV2Dx
                           << ";substeps=" << kCinematicLiquidV2Substeps
                           << ";rigidBodies=" << kCinematicLiquidV2BodyCount
                           << ";coupling=gpu_fixed_point_two_way"
                           << ";scene=clear_pvc_pool_dam_break_duck_family_motor_boat_sink_sphere_grass"
                           << ";sceneVersion=5"
                           << ";seed=deep_pool_dam_restage_4s_v2"
                           << ";cameraPathVersion=3"
                           << ";heroCamera=low_side_5s_v2"
                           << ";durationContractSec=15"
                           << ";poolType=inflatable_clear_pvc_finite_wall"
                           << ";poolWallInset=0.45"
                           << ";poolWallTopFraction=0.42"
                           << ";overflow=finite_height_inner_wall_catch_band"
                           << ";boat=finite_mass_soft_tether_propeller_reaction"
                           << ";sinkBall=gravity_9.81_air_drag_0.015_water_drag_displaced_mass"
                           << ";stiffness=45000"
                           << ";viscosity=0.035"
                           << ";maxVelocity=8"
                           << ";renderer=particle_spiky2_density_raymarch_iterative_fresnel4"
                           << ";surfaceVolume=" << kCinematicLiquidV2SurfaceX << "x"
                           << kCinematicLiquidV2SurfaceY << "x"
                           << kCinematicLiquidV2SurfaceZ
                           << ";surfaceKernelRadiusToSpacing=1.70"
                           << ";surfaceIso=0.32"
                           << ";surfaceFixedScale=65536"
                           << ";surfaceFilter=binomial5x5x5_adaptive_spray_preserve"
                           << ";refraction=multi_interface_fresnel_path_v2"
                           << ";maxOpticalInterfaces=4"
                           << ";extinction=12,3.6,2.5"
                           << ";normalGradientVoxels=1.0_to_2.5"
                           << ";toneMap=linear_exposure"
                           << ";densityBoundary=zero_clamped"
                           << ";whitewater=grid_derived_sink_entry_crown"
                           << ";environment=procedural_grass_atmosphere_clouds"
                           << ";raySteps=" << kCinematicLiquidV2RaySteps
                           << ";shaderVersion=" << kCinematicLiquidV2ShaderVersion
                           << ";apiScope=vulkan_only";
            break;
    }
    workloadConfig << ";renderDoc="
                   << (config_.renderDocEnabled ? "enabled" : "disabled");
    workloadConfig << ";multiGpu=" << multiGpuModeId(config_.multiGpuMode);
    if (GetBackendName() == "DX12" &&
        config_.multiGpuMode == MultiGpuMode::Off &&
        config_.adapterNodeCount > 1) {
        const std::uint32_t nodeMask = 1u << config_.adapterNodeIndex;
        workloadConfig << ";dx12Node=" << config_.adapterNodeIndex
                       << ";dx12NodeMask=0x" << std::hex << nodeMask << std::dec
                       << ";dx12LinkedNodes=" << config_.adapterNodeCount;
    }
    if (config_.multiGpuMode == MultiGpuMode::Afr) {
        // Keep AFR scores in a separate comparison group.  A two-node frame
        // stream has a different scheduling contract from the single-GPU run.
        r.workloadVersion += "_afr2";
        workloadConfig << ";afrNodes=2";
        if (GetBackendName() == "DX12")
            workloadConfig << ";afrControl=explicit_dx12_linked_nodes";
        else if (GetBackendName() == "Vulkan")
            workloadConfig << ";afrControl=explicit_vulkan_device_group";
        else if (GetBackendName() == "DX11")
            workloadConfig << ";afrControl=implicit_driver_unverified";
    } else if (config_.multiGpuMode == MultiGpuMode::Sfr) {
        // SFR is a distinct score contract: both nodes cooperate on one frame,
        // followed by exactly one composition and one presentation.
        r.workloadVersion += "_sfr2";
        workloadConfig << ";sfrNodes=2"
                       << ";sfrControl=explicit_dx12_linked_nodes"
                       << ";sfrSplit=vertical_50_50"
                       << ";sfrComposition=node1_local_to_node0_cross_adapter_copy";
    }
    if (config_.captureAtSec > 0.0 || config_.captureAtFrame > 0) {
        if (config_.captureAtSec > 0.0)
            workloadConfig << ";captureAtSec=" << config_.captureAtSec;
        if (config_.captureAtFrame > 0)
            workloadConfig << ";captureAtFrame=" << config_.captureAtFrame;
        workloadConfig << ";captureAttempts=" << rdocCaptureAttemptCount_
                       << ";captureExcluded="
                       << (rdocCaptureAttemptExcluded_ ? "true" : "false")
                       << ";captureCount=" << rdocCaptureCount_;
        if (captureUnavailable_)
            workloadConfig << ";captureUnavailable=true";
    }
    r.workloadConfig = workloadConfig.str();
    r.graphicsApi    = GetBackendName();
    r.deviceName     = config_.gpuDisplayName.empty() ? GetDeviceName() : config_.gpuDisplayName;
    r.driverVersion  = GetDriverVersion();
    r.cpuName        = GetCpuName();
    r.osVersion      = GetOsVersion();
    r.platform       = CurrentPlatform();
    r.osArchitecture = CurrentOsArchitecture();
    r.processArchitecture = CurrentProcessArchitecture();
    if (!config_.memoryLabelOverride.empty())
        r.memory = config_.memoryLabelOverride;
    else
        r.memory = config_.hostMemory ? "System-RAM" : "Device-local";
    r.vramMB      = config_.vramMB;
    r.resWidth    = kWindowWidth;
    r.resHeight   = kWindowHeight;
    // GPU Burn allocates a tiny internal compatibility buffer but does not
    // process particles. Persist zero so History/capture metadata cannot imply
    // that the visual burn score came from a 256-particle workload.
    r.particleCount = isGpuBurnWorkload(config_.workload)
        ? 0u : config_.particleCount;
    r.difficulty  = config_.difficultyLabel;
    r.vsync       = config_.vsync;
    r.headless    = config_.headless;
    r.framesInFlight = config_.framesInFlight;

    const std::string devName = GetDeviceName();
    r.isSoftware = (devName.find("Basic Render") != std::string::npos ||
                    devName.find("WARP") != std::string::npos ||
                    devName.find("Software") != std::string::npos);

    const double duration = std::max(0.0,
        benchEndTime_ - benchStartTime_ - excludedCaptureSec_);
    r.durationSec    = duration;
    r.warmupSec      = config_.warmupTimeSec;
    r.measuredFrames = benchMeasuredFrames_;
    r.timingSamples  = benchSampleCount_;

    if (benchSampleCount_ > 0) {
        r.avgComputeMs  = benchSumComputeMs_  / benchSampleCount_;
        r.minComputeMs  = benchMinComputeMs_;
        r.maxComputeMs  = benchMaxComputeMs_;
        r.avgRenderMs   = benchSumRenderMs_   / benchSampleCount_;
        r.minRenderMs   = benchMinRenderMs_;
        r.maxRenderMs   = benchMaxRenderMs_;
        r.avgTotalGpuMs = benchSumTotalGpuMs_ / benchSampleCount_;
        r.minTotalGpuMs = benchMinTotalGpuMs_;
        r.maxTotalGpuMs = benchMaxTotalGpuMs_;
    }

    r.avgFps = (duration > 0.0)
        ? static_cast<double>(benchMeasuredFrames_) / duration : 0.0;
    r.avgFrameTimeMs = (r.avgFps > 0.0) ? 1000.0 / r.avgFps : 0.0;
    r.gpuUtilisation = (r.avgFrameTimeMs > 0.0 && benchSampleCount_ > 0)
        ? r.avgTotalGpuMs / r.avgFrameTimeMs : 0.0;

    if (r.isSoftware)
        r.bottleneck = "Software";
    else if (benchSampleCount_ == 0)
        r.bottleneck = "Unknown";
    else if (r.gpuUtilisation < 0.5)
        r.bottleneck = "CPU-bound";
    else if (r.gpuUtilisation > 0.8)
        r.bottleneck = "GPU-bound";
    else
        r.bottleneck = "Balanced";

    // ---- Derived axis metric (mirrors the per-workload summary lines) ----
    const double n          = static_cast<double>(config_.particleCount);
    const double computeSec = r.avgComputeMs / 1000.0;
    const double renderSec  = r.avgRenderMs  / 1000.0;
    if (config_.workload == Workload::Stream && computeSec > 0.0) {
        r.score = 40.0 * n / computeSec / 1e9;   // ~40 bytes moved per particle
        r.scoreUnit = "GB/s";
    } else if (config_.workload == Workload::NBody && computeSec > 0.0) {
        r.score = n * n * kNBodyFlopsPerInteraction / computeSec / 1e9;
        r.scoreUnit = "GFLOP/s";
    } else if (config_.workload == Workload::StressFractal && renderSec > 0.0) {
        const double pixels = static_cast<double>(kWindowWidth) * kWindowHeight;
        r.score = pixels * config_.fractalIter / renderSec / 1e9;
        r.scoreUnit = "G-iter/s";
    } else if (config_.workload == Workload::GpuStressV1 && renderSec > 0.0) {
        const double pixels = static_cast<double>(kWindowWidth) * kWindowHeight;
        r.score = pixels * config_.gpuStressIter * kGpuStressV1DrawsPerFrame
                / renderSec / 1e9;
        r.scoreUnit = "Gpix-iter/s";
    } else if (isGpuBurnWorkload(config_.workload) && renderSec > 0.0) {
        const double pixels = static_cast<double>(kWindowWidth) * kWindowHeight;
        r.score = pixels * config_.gpuBurnIter * gpuBurnDrawsPerFrame(config_.workload)
                / renderSec / 1e9;
        r.scoreUnit = "Gpix-step/s";
    } else if (config_.workload == Workload::Volumetric && renderSec > 0.0) {
        const double pixels = static_cast<double>(kWindowWidth) * kWindowHeight;
        r.score = pixels * config_.volumetricSteps / renderSec / 1e9;
        r.scoreUnit = "GSample/s";
    } else if (config_.workload == Workload::Render3D && renderSec > 0.0) {
        r.score = n / renderSec / 1e6;   // million billboards per second
        r.scoreUnit = "MQuad/s";
    } else if (config_.workload == Workload::Fluid && computeSec > 0.0) {
        const double g  = static_cast<double>(config_.fluidGridSize);
        r.score = g * g * (4.0 + config_.fluidJacobiIters) / computeSec / 1e9;
        r.scoreUnit = "GCell/s";
    } else if (isCinematicLiquidWorkload(config_.workload) &&
               computeSec > 0.0 && renderSec > 0.0) {
        // Integrated score: the denominator includes both the fixed solver
        // simulation and the fixed-quality density raymarch presentation.
        // The SPH slice advances 2 reference ticks per frame, not 10.
        const std::uint32_t substeps = config_.workload == Workload::CinematicLiquid
            ? (config_.liquidSolverSph ? kCinematicLiquidSphSubsteps
                                       : kCinematicLiquidV2Substeps)
            : kCinematicLiquidSubsteps;
        r.score = n * substeps / (computeSec + renderSec) / 1e6;
        r.scoreUnit = "MParticle-step/s";
    } else if (config_.workload == Workload::SynthPeak && computeSec > 0.0) {
        const double lanes = (config_.peakPrecision == Precision::FP16) ? 2.0 : 1.0;
        r.score = n * config_.peakIters * kSynthPeakUnroll * lanes * 2.0 / computeSec / 1e9;
        r.scoreUnit = (config_.peakPrecision == Precision::INT32) ? "GIOPS" : "GFLOPS";
        r.precision = (config_.peakPrecision == Precision::FP64)  ? "FP64"
                    : (config_.peakPrecision == Precision::FP16)  ? "FP16"
                    : (config_.peakPrecision == Precision::INT32) ? "INT32"
                    :                                               "FP32";
    }

    // Thermal-stability telemetry (only meaningful with >=5 1s windows).
    if (windowScores_.size() >= 5) {
        r.stableScore       = stableScore_;
        r.stableVariancePct = stableVariancePct_;
        r.throttlePct       = throttlePct_;
    }

    return r;
}

// Same per-workload formula as CollectResult's derived score, factored out so
// we can apply it to a single 1-second timing window for thermal-stability
// tracking. Returns 0.0 if the window has no usable timing (e.g. compute=0
// on a fragment-only workload uses render instead).
double AppBase::computeAxisScore(double computeMs, double renderMs) const {
    const double n          = static_cast<double>(config_.particleCount);
    const double computeSec = computeMs / 1000.0;
    const double renderSec  = renderMs  / 1000.0;
    if (config_.workload == Workload::Stream && computeSec > 0.0) {
        return 40.0 * n / computeSec / 1e9;
    } else if (config_.workload == Workload::NBody && computeSec > 0.0) {
        return n * n * kNBodyFlopsPerInteraction / computeSec / 1e9;
    } else if (config_.workload == Workload::StressFractal && renderSec > 0.0) {
        const double pixels = static_cast<double>(kWindowWidth) * kWindowHeight;
        return pixels * config_.fractalIter / renderSec / 1e9;
    } else if (config_.workload == Workload::GpuStressV1 && renderSec > 0.0) {
        const double pixels = static_cast<double>(kWindowWidth) * kWindowHeight;
        return pixels * config_.gpuStressIter * kGpuStressV1DrawsPerFrame
             / renderSec / 1e9;
    } else if (isGpuBurnWorkload(config_.workload) && renderSec > 0.0) {
        const double pixels = static_cast<double>(kWindowWidth) * kWindowHeight;
        return pixels * config_.gpuBurnIter * gpuBurnDrawsPerFrame(config_.workload)
             / renderSec / 1e9;
    } else if (config_.workload == Workload::Volumetric && renderSec > 0.0) {
        const double pixels = static_cast<double>(kWindowWidth) * kWindowHeight;
        return pixels * config_.volumetricSteps / renderSec / 1e9;
    } else if (config_.workload == Workload::Render3D && renderSec > 0.0) {
        return n / renderSec / 1e6;
    } else if (config_.workload == Workload::Fluid && computeSec > 0.0) {
        const double g = static_cast<double>(config_.fluidGridSize);
        return g * g * (4.0 + config_.fluidJacobiIters) / computeSec / 1e9;
    } else if (isCinematicLiquidWorkload(config_.workload) &&
               computeSec > 0.0 && renderSec > 0.0) {
        const std::uint32_t substeps = config_.workload == Workload::CinematicLiquid
            ? (config_.liquidSolverSph ? kCinematicLiquidSphSubsteps
                                       : kCinematicLiquidV2Substeps)
            : kCinematicLiquidSubsteps;
        return n * substeps / (computeSec + renderSec) / 1e6;
    } else if (config_.workload == Workload::SynthPeak && computeSec > 0.0) {
        const double lanes = (config_.peakPrecision == Precision::FP16) ? 2.0 : 1.0;
        return n * config_.peakIters * kSynthPeakUnroll * lanes * 2.0 / computeSec / 1e9;
    }
    return 0.0;
}

void AppBase::recordWindowSample(double avgComputeMs, double avgRenderMs) {
    // Liquid cost legitimately evolves as spray and the propeller wake occupy
    // more grid cells. A falling per-second score is therefore scene-state
    // drift, not evidence of thermal throttling; only report the fixed-run
    // integrated average for these workloads.
    if (isCinematicLiquidWorkload(config_.workload)) return;
    const double s = computeAxisScore(avgComputeMs, avgRenderMs);
    if (s <= 0.0) return;
    windowScores_.push_back(s);
    windowRenderMs_.push_back(avgRenderMs);
    // Keep up to 30 samples (covers ~30s — the typical mobile throttle window).
    if (windowScores_.size() > 30) {
        windowScores_.erase(windowScores_.begin());
        windowRenderMs_.erase(windowRenderMs_.begin());
    }

    const size_t n = windowScores_.size();
    if (n < 5) return;

    // Coefficient of variation over the trailing 5 windows.
    auto meanVar = [](const std::vector<double>& v, size_t end) {
        const size_t start = (end >= 5) ? end - 5 : 0;
        double sum = 0.0, sum2 = 0.0; size_t k = 0;
        for (size_t i = start; i < end; ++i) { sum += v[i]; sum2 += v[i]*v[i]; ++k; }
        const double mean = sum / k;
        const double var  = (sum2 / k) - mean * mean;
        return std::make_pair(mean, std::sqrt(std::max(0.0, var)));
    };
    auto [mean, sd] = meanVar(windowScores_, n);
    const double cvPct = (mean > 0.0) ? (sd / mean * 100.0) : 0.0;

    // Stable = trailing-5 CV < 2%. Record once; later windows may overwrite
    // with a fresher stable point if it stays stable.
    if (cvPct < 2.0) {
        thermalStable_     = true;
        stableScore_       = mean;
        stableVariancePct_ = cvPct;
    }

    // Throttle detection: compare earliest 5 vs latest 5 (only meaningful once
    // we have >=10 windows). Positive = throttled.
    if (n >= 10) {
        auto [earlyMean,  eSd] = meanVar(windowScores_, 5);
        auto [lateMean,   lSd] = meanVar(windowScores_, n);
        if (earlyMean > 0.0)
            throttlePct_ = (earlyMean - lateMean) / earlyMean * 100.0;
    }
}

std::vector<char> AppBase::ReadFileBytes(const std::string& filename) {
    std::ifstream file(std::filesystem::u8path(filename),
                       std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    const auto fileSize = static_cast<std::size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
}

#if !defined(GPU_BENCH_NO_GLFW)
void AppBase::CleanupWindow() {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
}
#endif

}  // namespace gpu_bench
