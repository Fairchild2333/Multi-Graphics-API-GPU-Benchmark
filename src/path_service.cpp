#include "path_service.h"

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace gpu_bench::paths {
namespace {

std::filesystem::path EnvPath(const char* name) {
    const char* value = std::getenv(name);
    return value && *value ? std::filesystem::u8path(value)
                           : std::filesystem::path{};
}

std::filesystem::path PlatformDataRoot() {
    if (auto configured = EnvPath("GPU_BENCH_DATA_DIR"); !configured.empty())
        return configured;

#ifdef _WIN32
    if (auto local = EnvPath("LOCALAPPDATA"); !local.empty())
        return local / "GpuComputeBenchmark";
    if (auto profile = EnvPath("USERPROFILE"); !profile.empty())
        return profile / "AppData" / "Local" / "GpuComputeBenchmark";
#elif defined(__APPLE__)
    if (auto home = EnvPath("HOME"); !home.empty())
        return home / "Library" / "Application Support" / "GpuComputeBenchmark";
#else
    if (auto xdg = EnvPath("XDG_DATA_HOME"); !xdg.empty())
        return xdg / "GpuComputeBenchmark";
    if (auto home = EnvPath("HOME"); !home.empty())
        return home / ".local" / "share" / "GpuComputeBenchmark";
#endif

    // A missing home directory is unusual, but a deterministic absolute
    // fallback is still safer than writing relative to Program Files/CWD.
    return std::filesystem::temp_directory_path() / "GpuComputeBenchmark";
}

std::filesystem::path EnsureDirectory(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        throw std::runtime_error("Unable to create GPU benchmark data directory '" +
                                 path.u8string() + "': " + ec.message());
    }
    return path;
}

}  // namespace

const std::filesystem::path& DataRoot() {
    static const std::filesystem::path root = EnsureDirectory(PlatformDataRoot());
    return root;
}

std::filesystem::path ResultsDirectory() {
    return EnsureDirectory(DataRoot() / "results");
}

std::filesystem::path CapturesDirectory() {
    return EnsureDirectory(DataRoot() / "captures");
}

std::filesystem::path ReportsDirectory() {
    return EnsureDirectory(DataRoot() / "reports");
}

std::filesystem::path LogsDirectory() {
    return EnsureDirectory(DataRoot() / "logs");
}

}  // namespace gpu_bench::paths
