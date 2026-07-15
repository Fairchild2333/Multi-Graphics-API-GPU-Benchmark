#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.xaml.g.hpp"

#include "gpu_engine.h"          // gpu_bench::cliMain
#include "benchmark_results.h"   // gpu_bench::LoadResults
#include "path_service.h"        // gpu_bench::paths::{Results,Captures}Directory
#include "i18n.h"

#include <microsoft.ui.xaml.window.h>
#include <commctrl.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.Foundation.h>

#include <winrt/Windows.UI.Text.h>

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "comctl32.lib")

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Media::Animation;
using namespace Windows::Foundation;

// Local IBufferByteAccess (avoid <robuffer.h>: its global ::Windows namespace
// clashes with the using-directives above).
struct __declspec(uuid("905a0fef-bc53-11df-8c49-001e4fc686da")) __declspec(novtable)
IBufferByteAccessLocal : ::IUnknown
{
    virtual HRESULT __stdcall Buffer(uint8_t** value) = 0;
};

namespace winrt::gpu_bench_gui::implementation
{
namespace
{
    // Load the embedded app icon (app.rc id 101) into a WriteableBitmap so it
    // can be shown in the XAML UI (title-bar logo + About). Built at runtime
    // from the Win32 resource, so it works without ms-appx packaging.
    Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap loadAppIconBitmap(int px)
    {
        HICON hIcon = reinterpret_cast<HICON>(LoadImageW(
            GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101),
            IMAGE_ICON, px, px, LR_DEFAULTCOLOR));
        if (!hIcon) return nullptr;

        ICONINFO ii{};
        if (!GetIconInfo(hIcon, &ii)) { DestroyIcon(hIcon); return nullptr; }

        BITMAP bm{};
        GetObject(ii.hbmColor, sizeof(bm), &bm);
        const int w = bm.bmWidth, h = bm.bmHeight;

        BITMAPINFO bi{};
        bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth       = w;
        bi.bmiHeader.biHeight      = -h;   // top-down rows
        bi.bmiHeader.biPlanes      = 1;
        bi.bmiHeader.biBitCount    = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        std::vector<uint8_t> pixels(static_cast<size_t>(w) * h * 4);
        HDC hdc = GetDC(nullptr);
        GetDIBits(hdc, ii.hbmColor, 0, h, pixels.data(), &bi, DIB_RGB_COLORS);
        ReleaseDC(nullptr, hdc);
        DeleteObject(ii.hbmColor);
        DeleteObject(ii.hbmMask);
        DestroyIcon(hIcon);

        bool anyAlpha = false;
        for (size_t i = 3; i < pixels.size(); i += 4)
            if (pixels[i]) { anyAlpha = true; break; }
        if (!anyAlpha)
            for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;

        // WriteableBitmap expects premultiplied BGRA.
        for (size_t i = 0; i < pixels.size(); i += 4)
        {
            const uint8_t a = pixels[i + 3];
            pixels[i + 0] = static_cast<uint8_t>(pixels[i + 0] * a / 255);
            pixels[i + 1] = static_cast<uint8_t>(pixels[i + 1] * a / 255);
            pixels[i + 2] = static_cast<uint8_t>(pixels[i + 2] * a / 255);
        }

        Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap wb(w, h);
        uint8_t* dst = nullptr;
        wb.PixelBuffer().as<IBufferByteAccessLocal>()->Buffer(&dst);
        if (dst) memcpy(dst, pixels.data(), pixels.size());
        wb.Invalidate();
        return wb;
    }

    hstring locText(const char* en, const char* zh) { return winrt::to_hstring(i18n::tr(en, zh)); }
    IInspectable locContent(const char* en, const char* zh) { return winrt::box_value(locText(en, zh)); }

    hstring u8(std::string const& s)
    {
        if (s.empty()) return {};
        int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        std::wstring w(n, L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
        return hstring(w);
    }

    std::string findEngineExe()
    {
        wchar_t buf[MAX_PATH]{};
        ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
        const std::filesystem::path modulePath(buf);
        std::filesystem::path dir = modulePath.parent_path();

        // Installed/staged builds keep the CLI and GUI together. cliMain runs
        // in-process, but argv[0] is also the anchor used to find shaders.
        auto adjacent = dir / L"gpu_benchmark.exe";
        if (std::filesystem::exists(adjacent)) return adjacent.string();

        // Developer-tree compatibility for an IDE-launched GUI.
        for (int i = 0; i < 8; ++i)
        {
            for (auto rel : { L"build\\Release\\gpu_benchmark.exe", L"build\\gpu_benchmark.exe" })
            {
                auto cand = dir / rel;
                if (std::filesystem::exists(cand)) return cand.string();
            }
            if (!dir.has_parent_path()) break;
            dir = dir.parent_path();
        }

        // gpu_engine is statically linked, so the GUI can still call cliMain
        // without a separate CLI. This also anchors assets beside the GUI.
        return modulePath.string();
    }

    void configureBundledRenderDocLayer(std::string const& enginePath)
    {
        std::error_code ec;
        const auto binDir = std::filesystem::absolute(
            std::filesystem::path(enginePath), ec).parent_path();
        if (ec) return;
        const std::filesystem::path candidates[] = {
            binDir / L"tools" / L"RenderDoc",
            binDir / L".." / L"tools" / L"RenderDoc",
            binDir / L".." / L".." / L"tools" / L"RenderDoc",
        };
        for (auto const& candidate : candidates)
        {
            const auto dir = candidate.lexically_normal();
            if (std::filesystem::is_regular_file(dir / L"renderdoc.json", ec) && !ec &&
                std::filesystem::is_regular_file(dir / L"renderdoc.dll", ec) && !ec)
            {
                // The GUI probes Vulkan before its first benchmark job. Make
                // the portable layer visible now, not after that first probe.
                ::SetEnvironmentVariableW(L"VK_IMPLICIT_LAYER_PATH", dir.c_str());
                ::SetEnvironmentVariableW(L"ENABLE_VULKAN_RENDERDOC_CAPTURE", L"1");
                return;
            }
            ec.clear();
        }
    }

    std::filesystem::path findRenderDocCommand(std::string const& enginePath)
    {
        std::error_code ec;
        const auto binDir = std::filesystem::absolute(
            std::filesystem::path(enginePath), ec).parent_path();
        if (!ec)
        {
            const std::filesystem::path candidates[] = {
                binDir / L"tools" / L"RenderDoc" / L"renderdoccmd.exe",
                binDir / L".." / L"tools" / L"RenderDoc" / L"renderdoccmd.exe",
                binDir / L".." / L".." / L"tools" / L"RenderDoc" / L"renderdoccmd.exe",
            };
            for (auto const& candidate : candidates)
            {
                const auto command = candidate.lexically_normal();
                if (std::filesystem::is_regular_file(command, ec) && !ec)
                    return command;
                ec.clear();
            }
        }

        wchar_t programFiles[32768]{};
        const DWORD len = ::GetEnvironmentVariableW(
            L"ProgramFiles", programFiles,
            static_cast<DWORD>(std::size(programFiles)));
        if (len > 0 && len < std::size(programFiles))
        {
            const auto command = std::filesystem::path(programFiles)
                / L"RenderDoc" / L"renderdoccmd.exe";
            if (std::filesystem::is_regular_file(command, ec) && !ec)
                return command;
        }
        return {};
    }

    // Python is a developer fallback only. Release packages must eventually
    // ship a frozen report worker and must never claim reports were generated
    // merely because the benchmark itself completed.
    std::filesystem::path findPythonExecutable()
    {
        wchar_t path[32768]{};
        const DWORD len = ::SearchPathW(
            nullptr, L"python.exe", nullptr,
            static_cast<DWORD>(std::size(path)), path, nullptr);
        if (len > 0 && len < std::size(path))
            return std::filesystem::path(path);
        return {};
    }

    // Serialise in-process cliMain calls: the function redirects the global
    // std::cout / std::cerr stream buffers, which is inherently not thread-safe.
    static std::mutex g_cliMutex;

    // Run gpu_bench::cliMain in-process with synthesized argv, capturing stdout.
    struct CliResult { std::string output; int exitCode; };
    CliResult captureCli(std::vector<std::string> args)
    {
        std::lock_guard<std::mutex> lock(g_cliMutex);
        std::vector<char*> argv;
        argv.reserve(args.size());
        for (auto& a : args) argv.push_back(a.data());

        // Read the current process-wide SEH filter so we can restore it after
        // cliMain returns. cliMain installs its own CrashHandler which would
        // otherwise permanently replace WinUI's exception handling.
        // SetUnhandledExceptionFilter(nullptr) is the only API to read the
        // current value; it also clears the filter momentarily, so we restore
        // it immediately to minimise the unprotected window.
        auto prevSEH = ::SetUnhandledExceptionFilter(nullptr);
        ::SetUnhandledExceptionFilter(prevSEH);  // restore while we set up

        std::ostringstream cap;
        std::streambuf* old = std::cout.rdbuf(cap.rdbuf());
        std::streambuf* old_err = std::cerr.rdbuf(cap.rdbuf());
        // Tell cliMain not to call glfwTerminate() -- we may have more jobs to
        // run and the GUI manages the overall GLFW lifetime.
        gpu_bench::skipGlfwTerminate = true;

        int rc = -1;
        try { rc = gpu_bench::cliMain((int)argv.size(), argv.data()); }
        catch (std::exception const& ex) {
            cap << "\n[GUI captureCli] exception: " << ex.what() << "\n";
        }
        catch (...) {
            cap << "\n[GUI captureCli] unknown exception\n";
        }
        std::cout.rdbuf(old);
        std::cerr.rdbuf(old_err);

        // Restore the SEH filter that cliMain replaced with its own CrashHandler.
        ::SetUnhandledExceptionFilter(prevSEH);

        return { cap.str(), rc };
    }

    std::string extractScore(std::string const& out)
    {
        const char* keys[] = { "Memory rate:", "Compute rate:", "Burn rate:", "Stress rate:", "Fill rate:", "Render rate:", "Vol rate:", "Fluid rate:", "Liquid rate:", "Peak FP", "Peak INT" };
        std::istringstream ss(out);
        std::string line, fps;
        while (std::getline(ss, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            for (auto k : keys)
                if (line.find(k) != std::string::npos)
                {
                    size_t b = line.find_first_not_of(" \t");
                    return b == std::string::npos ? line : line.substr(b);
                }
            if (line.find("Avg FPS:") != std::string::npos && fps.empty()) fps = line;
        }
        size_t b = fps.find_first_not_of(" \t");
        return (b == std::string::npos) ? fps : fps.substr(b);
    }

    std::string padCol(std::string s, size_t w)
    {
        if (s.size() < w) s.append(w - s.size(), ' ');
        else if (s.size() > w && w > 1) s = s.substr(0, w - 1) + " ";
        return s;
    }

    int runProcess(std::wstring cmd, std::wstring const& cwd)
    {
        STARTUPINFOW si{}; si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        std::vector<wchar_t> mut(cmd.begin(), cmd.end()); mut.push_back(0);
        if (!::CreateProcessW(nullptr, mut.data(), nullptr, nullptr, FALSE,
                              CREATE_NO_WINDOW, nullptr,
                              cwd.empty() ? nullptr : cwd.c_str(), &si, &pi))
            return -1;
        ::WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD code = 0; ::GetExitCodeProcess(pi.hProcess, &code);
        ::CloseHandle(pi.hProcess); ::CloseHandle(pi.hThread);
        return (int)code;
    }

    enum class CpuLineType { Other, Progress, Result, Error, Meta, Topology };

    struct CpuProtocolLine
    {
        CpuLineType type{ CpuLineType::Other };
        std::map<std::string, std::string> fields;
    };

    std::string trimAscii(std::string value)
    {
        auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
            return std::isspace(c) != 0;
        });
        auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
            return std::isspace(c) != 0;
        }).base();
        if (first >= last) return {};
        return std::string(first, last);
    }

    std::string lowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    CpuProtocolLine parseCpuProtocolLine(std::string const& raw)
    {
        CpuProtocolLine parsed;
        std::istringstream stream(raw);
        std::string token;
        if (!std::getline(stream, token, '\t')) return parsed;
        token = trimAscii(token);
        if (token == "CPU_PROGRESS") parsed.type = CpuLineType::Progress;
        else if (token == "CPU_RESULT") parsed.type = CpuLineType::Result;
        else if (token == "CPU_ERROR") parsed.type = CpuLineType::Error;
        else if (token == "CPU_META") parsed.type = CpuLineType::Meta;
        else if (token == "CPU_TOPOLOGY") parsed.type = CpuLineType::Topology;
        else return parsed;

        // Unknown keys are deliberately retained. The UI consumes only the
        // fields it understands, so the CLI protocol can grow without breaking
        // older GUI builds.
        size_t positional = 0;
        while (std::getline(stream, token, '\t'))
        {
            token = trimAscii(token);
            if (token.empty()) continue;
            const auto equals = token.find('=');
            if (equals == std::string::npos)
            {
                parsed.fields["field" + std::to_string(positional++)] = token;
                continue;
            }
            auto key = lowerAscii(trimAscii(token.substr(0, equals)));
            auto value = trimAscii(token.substr(equals + 1));
            if (!key.empty()) parsed.fields[std::move(key)] = std::move(value);
        }
        return parsed;
    }

    std::string cpuField(CpuProtocolLine const& line,
                         std::initializer_list<char const*> names)
    {
        for (auto name : names)
            if (auto it = line.fields.find(name); it != line.fields.end())
                return it->second;
        return {};
    }

    std::optional<double> cpuNumber(CpuProtocolLine const& line,
                                    std::initializer_list<char const*> names)
    {
        const auto text = cpuField(line, names);
        if (text.empty()) return std::nullopt;
        try
        {
            size_t used = 0;
            const double value = std::stod(text, &used);
            if (used == text.size() && std::isfinite(value)) return value;
        }
        catch (...) {}
        return std::nullopt;
    }

    std::optional<int> cpuInteger(CpuProtocolLine const& line,
                                  std::initializer_list<char const*> names)
    {
        const auto value = cpuNumber(line, names);
        if (!value || *value < static_cast<double>(INT_MIN) ||
            *value > static_cast<double>(INT_MAX)) return std::nullopt;
        return static_cast<int>(*value);
    }

    std::string formatCpuScore(double score, std::string const& unit)
    {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::fixed << std::setprecision(score >= 1000.0 ? 1 : 3) << score;
        if (!unit.empty()) out << ' ' << unit;
        return out.str();
    }

    std::string win32ErrorText(DWORD error)
    {
        wchar_t* buffer = nullptr;
        const DWORD chars = ::FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
        if (!chars || !buffer) return "Win32 error " + std::to_string(error);
        std::wstring message(buffer, chars);
        ::LocalFree(buffer);
        while (!message.empty() && std::iswspace(message.back())) message.pop_back();
        return to_string(hstring(message));
    }

    Uri fileUri(std::filesystem::path const& p)
    {
        std::wstring s = p.wstring();
        for (auto& c : s) if (c == L'\\') c = L'/';
        return Uri(hstring(L"file:///" + s));
    }

    // Engine prints each RenderDoc capture as a "  -> <path>.rdc" line; collect them.
    std::vector<std::string> parseCapturePaths(std::string const& out)
    {
        std::vector<std::string> paths;
        std::istringstream ss(out); std::string line;
        while (std::getline(ss, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto pos = line.find("-> ");
            if (pos == std::string::npos) continue;
            std::string p = line.substr(pos + 3);
            while (!p.empty() && p.front() == ' ') p.erase(p.begin());
            while (!p.empty() && p.back()  == ' ') p.pop_back();
            if (p.size() > 4 && p.substr(p.size() - 4) == ".rdc") paths.push_back(p);
        }
        return paths;
    }

    // Min-size clamp + theme-matched resize fill (avoids the black flash during
    // a fast resize while Mica catches up). Copied from sdr2hdr.
    LRESULT CALLBACK WindowSubclassProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l,
                                        UINT_PTR, DWORD_PTR refData)
    {
        switch (msg)
        {
        case WM_GETMINMAXINFO:
        {
            UINT dpi = GetDpiForWindow(hwnd);
            double scale = dpi > 0 ? dpi / 96.0 : 1.0;
            auto* mmi = reinterpret_cast<MINMAXINFO*>(l);
            mmi->ptMinTrackSize.x = static_cast<LONG>(1120 * scale);
            mmi->ptMinTrackSize.y = static_cast<LONG>(680 * scale);
            return 0;
        }
        case WM_ERASEBKGND:
            if (auto brush = reinterpret_cast<HBRUSH>(refData))
            {
                RECT rc{}; GetClientRect(hwnd, &rc);
                FillRect(reinterpret_cast<HDC>(w), &rc, brush);
                return 1;
            }
            break;
        default: break;
        }
        return DefSubclassProc(hwnd, msg, w, l);
    }

    std::string detectCpuName()
    {
        HKEY key{};
        if (::RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                0, KEY_READ, &key) == ERROR_SUCCESS)
        {
            char buf[256]{}; DWORD sz = sizeof(buf);
            if (::RegQueryValueExA(key, "ProcessorNameString", nullptr, nullptr,
                                   reinterpret_cast<LPBYTE>(buf), &sz) == ERROR_SUCCESS)
            {
                ::RegCloseKey(key);
                std::string s(buf);
                while (!s.empty() && s.front() == ' ') s.erase(s.begin());
                while (!s.empty() && s.back()  == ' ') s.pop_back();
                return s;
            }
            ::RegCloseKey(key);
        }
        return "CPU";
    }

    bool isSoftwareGpu(std::string const& name)
    {
        return name.find("Basic Render") != std::string::npos
            || name.find("WARP") != std::string::npos
            || name.find("Software") != std::string::npos;
    }

    // "" = All; otherwise a YYYY-MM-DD cutoff (keep timestamps >= cutoff).
    std::string cutoffFor(int rangeIdx)
    {
        if (rangeIdx <= 0) return {};
        int days = (rangeIdx == 1) ? 0 : (rangeIdx == 2) ? 6 : 29;
        time_t t = time(nullptr) - static_cast<time_t>(days) * 86400;
        tm lt{}; localtime_s(&lt, &t);
        char buf[16];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
        return buf;
    }

    // Different APIs report the same physical GPU under decorated names
    // ("…RTX 5090", "…RTX 5090 (FL 12_1)", "…RTX 5090/PCIe/SSE2") — collapse to
    // a common base so they merge in the filter.
    std::string normalizeGpuName(std::string n)
    {
        auto p = n.find(" (");
        if (p != std::string::npos) n = n.substr(0, p);
        p = n.find('/');
        if (p != std::string::npos) n = n.substr(0, p);
        p = n.find(" #");                       // "…RTX 5090 #1" -> base
        if (p != std::string::npos) n = n.substr(0, p);
        // OEM-branded Adreno: "Mi Pad 5 Adreno 640 GPU" -> "Qualcomm Adreno 640"
        p = n.find("Adreno");
        if (p != std::string::npos)
        {
            std::string tail = n.substr(p);   // "Adreno 640 GPU" or "Adreno 640"
            auto g = tail.find(" GPU");
            if (g != std::string::npos) tail = tail.substr(0, g);
            n = "Qualcomm " + tail;
        }
        while (!n.empty() && n.back() == ' ') n.pop_back();
        return n.empty() ? "(unknown)" : n;
    }

    // Brand bucket from a (normalized) device name. Software renderers (WARP,
    // Basic Render Driver, SVGA…) run on the CPU, so they group under "CPU".
    std::string gpuBrand(std::string const& n)
    {
        auto has = [&](const char* s) { return n.find(s) != std::string::npos; };
        if (has("WARP") || has("Basic Render") || has("SVGA") || has("Software") || has("Microsoft"))
            return "CPU / Software";
        if (has("NVIDIA") || has("GeForce") || has("Quadro") || has("Tesla")) return "NVIDIA";
        if (has("AMD") || has("Radeon") || has("FirePro") || has("Vega"))     return "AMD";
        if (has("Intel") || has("Arc"))                                       return "Intel";
        if (has("Apple"))                                                     return "Apple";
        if (has("Adreno") || has("Qualcomm") || has("Snapdragon"))            return "Qualcomm";
        if (has("Mali") || has("ARM"))                                        return "ARM";
        if (has("Moore Threads") || has("MTT"))                               return "Moore Threads";
        return "Other";
    }

    // Series sub-bucket within a brand.
    std::string gpuSeries(std::string const& brand, std::string const& n)
    {
        auto has = [&](const char* s) { return n.find(s) != std::string::npos; };
        if (brand == "NVIDIA")
        {
            if (has("RTX"))    return "GeForce RTX";
            if (has("GTX"))    return "GeForce GTX";
            if (has("Quadro")) return "Quadro";
            if (has("Tesla"))  return "Tesla";
            return "GeForce / Other";
        }
        if (brand == "AMD")
        {
            if (has("RX"))      return "Radeon RX";
            if (has("Vega"))    return "Radeon Vega";
            if (has("FirePro")) return "Radeon Pro / FirePro";
            if (has("HD"))      return "Radeon HD";
            return "Radeon / Other";
        }
        if (brand == "Intel")
        {
            if (has("Arc"))                return "Arc";
            if (has("Iris"))               return "Iris";
            if (has("UHD") || has("HD"))   return "HD / UHD";
            return "Other";
        }
        if (brand == "Qualcomm")       return "Adreno";
        if (brand == "ARM")            return "Mali";
        if (brand == "Apple")          return "Apple Silicon";
        if (brand == "Moore Threads")  return "MTT";
        return brand;
    }

    // Forward declaration (defined below localizeTimestamp).
    std::string normalizeCpuName(std::string n);

    // A result's place in the brand -> series -> device tree. Software renderers
    // (WARP etc.) group by the CPU model they ran on.
    struct GpuLeaf { std::string brand, series, device; };
    GpuLeaf leafOf(gpu_bench::BenchmarkResult const& r)
    {
        std::string dev = normalizeGpuName(r.deviceName);
        std::string brand = gpuBrand(dev);
        std::string series = (brand == "CPU / Software")
            ? normalizeCpuName(r.cpuName.empty() ? "Unknown CPU" : r.cpuName)
            : gpuSeries(brand, dev);
        return { brand, series, dev };
    }
    std::string filterKey(GpuLeaf const& l)
    {
        return l.brand + "\x1f" + l.series + "\x1f" + l.device;
    }

    // CalendarDatePicker -> "YYYY-MM-DD" ("" if unset).
    std::string pickerDate(Controls::CalendarDatePicker const& p)
    {
        auto ref = p.Date();
        if (!ref) return {};
        time_t t = winrt::clock::to_time_t(ref.Value());
        tm lt{}; localtime_s(&lt, &t);
        char buf[16];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
        return buf;
    }

    std::string groupedNumber(std::uint32_t n)
    {
        std::string s = std::to_string(n);
        for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
            s.insert(static_cast<size_t>(i), ",");
        return s;
    }

    std::string particleLabel(std::uint32_t n)
    {
        if (n == 0) return "-";
        if (n == 65536u)    return "65K (65,536)";
        if (n == 1048576u)  return "1M (1,048,576)";
        if (n == 4194304u)  return "4M (4,194,304)";
        if (n == 16777216u) return "16M (16,777,216)";
        return groupedNumber(n);
    }

    // Convert stored ISO timestamp "YYYY-MM-DD HH:MM:SS" to locale-appropriate
    // display format. English uses Australian DD/MM/YYYY 12-hour; Chinese keeps
    // YYYY-MM-DD 24-hour.
    std::string localizeTimestamp(std::string const& ts)
    {
        if (ts.size() < 19) return ts;  // malformed, return as-is
        if (i18n::currentLang() == i18n::Lang::Zh) return ts;  // 中文: 年-月-日 24h

        // Parse "YYYY-MM-DD HH:MM:SS"
        int Y = 0, M = 0, D = 0, h = 0, m = 0, s = 0;
        if (sscanf(ts.c_str(), "%d-%d-%d %d:%d:%d", &Y, &M, &D, &h, &m, &s) < 6)
            return ts;

        // Australian: DD/MM/YYYY hh:mm:ss AM/PM
        const char* ampm = (h < 12) ? "AM" : "PM";
        int h12 = h % 12;
        if (h12 == 0) h12 = 12;
        char buf[32];
        snprintf(buf, sizeof(buf), "%02d/%02d/%04d %d:%02d:%02d %s",
                 D, M, Y, h12, m, s, ampm);
        return buf;
    }

    // Clean up CPU names for display: strip "Nth Gen" Intel prefixes,
    // collapse internal whitespace.
    std::string normalizeCpuName(std::string n)
    {
        // Strip "12th Gen ", "13th Gen ", etc.
        for (auto const* prefix : { "th Gen ", "st Gen ", "nd Gen ", "rd Gen " })
        {
            auto pos = n.find(prefix);
            if (pos != std::string::npos && pos <= 4)
            {
                n = n.substr(pos + strlen(prefix));
                break;
            }
        }
        // Collapse multiple spaces to single
        std::string out;
        out.reserve(n.size());
        bool prevSpace = false;
        for (char c : n)
        {
            if (c == ' ') { if (!prevSpace) out += c; prevSpace = true; }
            else          { out += c; prevSpace = false; }
        }
        // Trim
        while (!out.empty() && out.front() == ' ') out.erase(out.begin());
        while (!out.empty() && out.back()  == ' ') out.pop_back();
        // Add "Qualcomm" prefix for Snapdragon CPUs
        if (out.find("Snapdragon") != std::string::npos
            && out.find("Qualcomm") == std::string::npos)
            out = "Qualcomm " + out;
        return out.empty() ? "(unknown)" : out;
    }

    std::string workloadLabel(std::string const& id)
    {
        if (id == "stream")     return i18n::tr("Particle — Memory Throughput", "粒子 —— 内存吞吐");
        if (id == "gpu_burn")   return i18n::tr("GPU Burn — Plasma Bloom", "GPU Burn —— 等离子晶核");
        if (id == "gpu_stress") return i18n::tr("GraphicsBurn v1 / Component (Advanced)", "GraphicsBurn v1 / 分项（高级）");
        if (id == "nbody")      return i18n::tr("N-Body — Advanced Compute", "N-Body —— 高级计算");
        if (id == "synthpeak")  return i18n::tr("SynthPeak — Advanced Synthetic", "SynthPeak —— 高级合成测试");
        if (id == "stress")     return i18n::tr("Legacy Stress v1 — Fragment ALU/SFU", "旧版压力测试 v1 —— 片元 ALU/SFU");
        if (id == "render3d")   return i18n::tr("Legacy 3D Prototype — Billboards", "旧版 3D 原型 —— Billboard");
        if (id == "volumetric") return i18n::tr("Volumetric — Experimental Raymarch", "体积渲染 —— 实验性 Raymarch");
        if (id == "cinematic_liquid") return i18n::tr("Cinematic Liquid v2 — Interactive Pool", "电影化液体 v2 —— 互动水池");
        if (id == "cinematic_liquid_v1") return i18n::tr("Legacy Cinematic Liquid v1 — Dam Break", "旧版电影化液体 v1 —— 溃坝");
        if (id == "fluid")      return i18n::tr("Other / Legacy 2D Fluid — Vulkan-only", "其他 / 旧版 2D 流体 —— 仅 Vulkan");
        return id;
    }

    std::string workloadRunLabel(gpu_bench::BenchmarkResult const& result)
    {
        auto label = workloadLabel(result.workload);
        if (result.workload == "cinematic_liquid" &&
            result.workloadVersion == "cinematic_liquid_v1")
            return workloadLabel("cinematic_liquid_v1");
        if (result.workload == "fluid")
            return i18n::tr("Legacy 2D Fluid — unverified", "旧版 2D 流体 —— 未验证");
        if (result.workload != "gpu_burn") return label;
        constexpr char key[] = "steps=";
        auto pos = result.workloadConfig.find(key);
        if (pos == std::string::npos) return label;
        pos += sizeof(key) - 1;
        auto end = result.workloadConfig.find(';', pos);
        auto steps = result.workloadConfig.substr(pos, end - pos);
        if (!steps.empty())
            label += " [" + steps + i18n::tr(" steps]", " 步]");
        return label;
    }

    std::string historyWorkloadKey(gpu_bench::BenchmarkResult const& result)
    {
        // Persisted liquid v1/v2 rows share the historical public workload id
        // and are separated by workloadVersion. Give v1 its own UI filter
        // without rewriting old result files.
        if (result.workload == "cinematic_liquid" &&
            result.workloadVersion == "cinematic_liquid_v1")
            return "cinematic_liquid_v1";
        return result.workload;
    }

    std::string apiLabel(std::string const& id)
    {
        if (id == "DX12") return "DirectX 12";
        if (id == "DX11") return "DirectX 11";
        return id.empty() ? "(unknown)" : id;
    }

    std::set<std::string> checkedTags(StackPanel const& panel)
    {
        std::set<std::string> out;
        for (auto const& c : panel.Children())
            if (auto cb = c.try_as<CheckBox>(); cb && cb.Tag() && cb.IsChecked() && cb.IsChecked().Value())
                out.insert(to_string(unbox_value_or<hstring>(cb.Tag(), L"")));
        return out;
    }

    void setPanelChecks(StackPanel const& panel, bool value)
    {
        for (auto const& c : panel.Children())
            if (auto cb = c.try_as<CheckBox>(); cb && cb.Tag())
                cb.IsChecked(value);
    }

    std::string mostFrequentKey(std::map<std::string, int> const& counts)
    {
        std::string best;
        int bestCount = -1;
        for (auto const& [key, count] : counts)
            if (count > bestCount)
            {
                best = key;
                bestCount = count;
            }
        return best;
    }

    void appendFilterCheckBox(StackPanel const& panel,
                              std::string const& label,
                              std::string const& tag,
                              bool checked,
                              Thickness const& margin = Thickness{ 0, 0, 0, 0 })
    {
        CheckBox cb;
        cb.Content(box_value(u8(label)));
        cb.Tag(box_value(u8(tag)));
        cb.IsChecked(checked);
        cb.Margin(margin);
        panel.Children().Append(cb);
    }
}

MainWindow::MainWindow()
{
    InitializeComponent();
    m_dispatcher = Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
    m_enginePath = findEngineExe();
    m_cpuName = detectCpuName();

    if (auto native = this->try_as<::IWindowNative>())
        native->get_WindowHandle(&m_hwnd);

    auto window = this->try_as<Microsoft::UI::Xaml::Window>();
    window.ExtendsContentIntoTitleBar(true);
    window.SetTitleBar(AppTitleBar());

    if (m_hwnd)
    {
        auto windowId = Microsoft::UI::GetWindowIdFromWindow(m_hwnd);
        if (auto appWindow = Microsoft::UI::Windowing::AppWindow::GetFromWindowId(windowId))
        {
            UINT dpi = GetDpiForWindow(m_hwnd);
            double scale = dpi > 0 ? dpi / 96.0 : 1.0;
            const int32_t desiredWidth = static_cast<int32_t>(1320 * scale);
            const int32_t desiredHeight = static_cast<int32_t>(820 * scale);
            if (auto display = Microsoft::UI::Windowing::DisplayArea::GetFromWindowId(
                    windowId, Microsoft::UI::Windowing::DisplayAreaFallback::Primary))
            {
                const auto work = display.WorkArea();
                const int32_t width = (std::min)(desiredWidth, work.Width);
                const int32_t height = (std::min)(desiredHeight, work.Height);
                appWindow.MoveAndResize({
                    work.X + (work.Width - width) / 2,
                    work.Y + (work.Height - height) / 2,
                    width, height });
            }
            else
            {
                appWindow.Resize({ desiredWidth, desiredHeight });
            }
            if (Microsoft::UI::Windowing::AppWindowTitleBar::IsCustomizationSupported())
                appWindow.TitleBar().PreferredHeightOption(
                    Microsoft::UI::Windowing::TitleBarHeightOption::Tall);
        }
    }

    updateResizeBackdropColor();
    RootShell().ActualThemeChanged([this](auto&&, auto&&) {
        updateResizeBackdropColor();
        updateCaptionButtonColors();
    });
    Closed([this](auto&&, auto&&) { cancelCpuBenchmark(); });
    updateCaptionButtonColors();

    i18n::initLang(nullptr);
    applyLanguage();

    // History filters apply once, when each dropdown closes (not on every toggle).
    auto applyOnClose = [this](auto&&, auto&&) { if (m_uiReady) applyHistoryView(); };
    ApiFilterFlyout().Closed(applyOnClose);
    WorkloadFilterFlyout().Closed(applyOnClose);
    ParticleFilterFlyout().Closed(applyOnClose);
    GpuFilterFlyout().Closed(applyOnClose);

    SelectAllApis().Click([this](auto&&, auto&&) { setPanelChecks(ApiFilterPanel(), true); });
    ClearAllApis().Click([this](auto&&, auto&&) { setPanelChecks(ApiFilterPanel(), false); });
    SelectAllWorkloads().Click([this](auto&&, auto&&) { setPanelChecks(WorkloadFilterPanel(), true); });
    ClearAllWorkloads().Click([this](auto&&, auto&&) { setPanelChecks(WorkloadFilterPanel(), false); });
    SelectAllParticles().Click([this](auto&&, auto&&) { setPanelChecks(ParticleFilterPanel(), true); });
    ClearAllParticles().Click([this](auto&&, auto&&) { setPanelChecks(ParticleFilterPanel(), false); });
    SelectAllGpus().Click([this](auto&&, auto&&) { setPanelChecks(GpuFilterPanel(), true); });
    ClearAllGpus().Click([this](auto&&, auto&&) { setPanelChecks(GpuFilterPanel(), false); });

    // App icon: caption / taskbar (WM_SETICON) + title-bar logo + About card.
    if (m_hwnd)
        if (HICON hIcon = reinterpret_cast<HICON>(LoadImageW(
                GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101),
                IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED)))
        {
            SendMessageW(m_hwnd, WM_SETICON, ICON_BIG,   reinterpret_cast<LPARAM>(hIcon));
            SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
        }
    if (auto bmp = loadAppIconBitmap(40))  TitleBarIcon().Source(bmp);
    if (auto bmp = loadAppIconBitmap(128)) AboutIcon().Source(bmp);

    m_uiReady = true;
    populateGpus();
    refreshHistory();
}

std::string MainWindow::selected(ComboBox const& box)
{
    auto item = box.SelectedItem().try_as<ComboBoxItem>();
    if (!item) return "";
    if (item.Tag()) return to_string(unbox_value_or<hstring>(item.Tag(), L""));
    return to_string(unbox_value_or<hstring>(item.Content(), L""));
}

// ---- shell (from sdr2hdr) --------------------------------------------------
void MainWindow::applyTheme(int index)
{
    auto theme = index == 1 ? ElementTheme::Light
               : index == 2 ? ElementTheme::Dark
                            : ElementTheme::Default;
    if (auto root = RootShell().try_as<FrameworkElement>())
        root.RequestedTheme(theme);
    updateResizeBackdropColor();
    updateCaptionButtonColors();
}

void MainWindow::updateCaptionButtonColors()
{
    if (!m_hwnd) return;
    auto windowId = Microsoft::UI::GetWindowIdFromWindow(m_hwnd);
    auto appWindow = Microsoft::UI::Windowing::AppWindow::GetFromWindowId(windowId);
    if (!appWindow) return;
    auto tb = appWindow.TitleBar();

    using winrt::Windows::UI::Color;
    const bool dark = RootShell().ActualTheme() != ElementTheme::Light;
    const Color fg        = dark ? Color{ 255, 255, 255, 255 } : Color{ 255, 0, 0, 0 };
    const Color inactive  = dark ? Color{ 255, 155, 155, 155 } : Color{ 255, 120, 120, 120 };
    const Color clear     = Color{ 0, 0, 0, 0 };
    const Color hoverBg   = dark ? Color{ 25, 255, 255, 255 } : Color{ 18, 0, 0, 0 };
    const Color pressedBg = dark ? Color{ 50, 255, 255, 255 } : Color{ 36, 0, 0, 0 };

    tb.ButtonForegroundColor(fg);
    tb.ButtonBackgroundColor(clear);
    tb.ButtonInactiveBackgroundColor(clear);
    tb.ButtonInactiveForegroundColor(inactive);
    tb.ButtonHoverForegroundColor(fg);
    tb.ButtonHoverBackgroundColor(hoverBg);
    tb.ButtonPressedForegroundColor(fg);
    tb.ButtonPressedBackgroundColor(pressedBg);
}

void MainWindow::updateResizeBackdropColor()
{
    if (!m_hwnd) return;
    auto actual = RootShell().ActualTheme();
    COLORREF color = (actual == ElementTheme::Dark) ? RGB(32, 32, 32) : RGB(243, 243, 243);
    HBRUSH brush = CreateSolidBrush(color);
    SetWindowSubclass(m_hwnd, WindowSubclassProc, 1, reinterpret_cast<DWORD_PTR>(brush));
    if (m_bgBrush) DeleteObject(m_bgBrush);
    m_bgBrush = brush;
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void MainWindow::animatePageIn(FrameworkElement const& page)
{
    TranslateTransform tt; tt.Y(28.0);
    page.RenderTransform(tt);
    auto dur = DurationHelper::FromTimeSpan(std::chrono::milliseconds(300));
    CubicEase ease; ease.EasingMode(EasingMode::EaseOut);
    DoubleAnimation slide; slide.From(28.0); slide.To(0.0); slide.Duration(dur); slide.EasingFunction(ease);
    Storyboard::SetTarget(slide, tt); Storyboard::SetTargetProperty(slide, L"Y");
    DoubleAnimation fade; fade.From(0.0); fade.To(1.0); fade.Duration(dur);
    Storyboard::SetTarget(fade, page); Storyboard::SetTargetProperty(fade, L"Opacity");
    Storyboard sb; sb.Children().Append(slide); sb.Children().Append(fade); sb.Begin();
}

void MainWindow::showPage(int index)
{
    RunPage().Visibility(index == 0 ? Visibility::Visible : Visibility::Collapsed);
    CpuPage().Visibility(index == 5 ? Visibility::Visible : Visibility::Collapsed);
    HistoryPage().Visibility(index == 1 ? Visibility::Visible : Visibility::Collapsed);
    ChartsPage().Visibility(index == 2 ? Visibility::Visible : Visibility::Collapsed);
    SettingsPage().Visibility(index == 3 ? Visibility::Visible : Visibility::Collapsed);
    AboutPage().Visibility(index == 4 ? Visibility::Visible : Visibility::Collapsed);

    FrameworkElement active{ nullptr };
    switch (index)
    {
        case 0:  active = RunPage();      break;
        case 1:  active = HistoryPage();  break;
        case 2:  active = ChartsPage();   break;
        case 3:  active = SettingsPage(); break;
        case 4:  active = AboutPage();    break;
        case 5:  active = CpuPage();      break;
        default: active = RunPage();      break;
    }
    if (active) animatePageIn(active);
    if (index == 1) refreshHistory();
}

void MainWindow::OnNavSelectionChanged(NavigationView const&, NavigationViewSelectionChangedEventArgs const& args)
{
    if (auto item = args.SelectedItem().try_as<NavigationViewItem>())
    {
        auto tag = unbox_value_or<hstring>(item.Tag(), L"0");
        showPage(_wtoi(tag.c_str()));
    }
}

void MainWindow::OnThemeChanged(IInspectable const&, SelectionChangedEventArgs const&)
{
    if (!m_uiReady || m_suppressCombo) return;
    applyTheme(ThemeBox().SelectedIndex());
}

void MainWindow::OnLangChanged(IInspectable const&, SelectionChangedEventArgs const&)
{
    if (!m_uiReady || m_suppressCombo) return;
    int idx = LangBox().SelectedIndex();
    if (idx == 1)      i18n::initLang("en");
    else if (idx == 2) i18n::initLang("zh");
    else               i18n::initLang(nullptr);
    applyLanguage();
}

// ---- localization ----------------------------------------------------------
void MainWindow::applyLanguage()
{
    NavCpu().Content(locContent("CPU", "CPU"));
    CpuTitle().Text(locText("CPU Benchmark", "CPU 跑分"));
    CpuNameText().Text(u8(m_cpuName));
    CpuInfo().Message(locText(
        "Headless CPU test. No 3D render window is opened.",
        "纯 CPU 无窗口测试，不会打开 3D 渲染窗口。"));
    CpuModeBox().Header(locContent("Test mode", "测试模式"));
    CpuModePerCore().Content(locContent("Each logical processor", "逐个逻辑处理器"));
    CpuModeMulti().Content(locContent("All cores together", "全部核心并行"));
    CpuModeAll().Content(locContent("Per-core + all-core", "逐核 + 多核"));
    CpuDurationPresetBox().Header(locContent("Duration preset", "时长预设"));
    CpuDurationQuick().Content(locContent("Quick (1 s)", "快速（1 秒）"));
    CpuDurationFormal().Content(locContent("Formal (15 s)", "正式（15 秒）"));
    CpuTimeBox().Header(locContent("Seconds per test", "每项测试秒数"));
    CpuWarmupBox().Header(locContent("Warm-up seconds", "预热秒数"));
    CpuDurationHint().Text(locText(
        "Per-core duration is applied separately to every logical processor; the formal preset can take a long time.",
        "该时长会分别应用到每个逻辑处理器；正式逐核测试可能需要较长时间。"));
    CpuPerCoreTitle().Text(locText("Per-logical-processor results", "逐逻辑处理器成绩"));
    CpuSummaryTitle().Text(locText("Summary", "汇总"));
    CpuAverageLabel().Text(locText("Per-core average", "逐核平均"));
    CpuMultiLabel().Text(locText("All-core result", "多核成绩"));
    CpuOutputExpander().Header(locContent("Raw CLI output (live tail)", "原始 CLI 输出（实时尾部）"));
    CpuCancelButton().Content(locContent("Cancel", "取消"));
    CpuRunButton().Content(locContent("Run CPU Benchmark", "开始 CPU 跑分"));
    if (!m_cpuRunning.load()) CpuStatusText().Text(locText("Ready", "就绪"));

    NavRun().Content(locContent("Run", "运行"));
    NavHistory().Content(locContent("History", "历史"));
    NavCharts().Content(locContent("Charts", "图表"));
    NavSettings().Content(locContent("Settings", "设置"));
    NavAbout().Content(locContent("About", "关于"));

    RunTitle().Text(locText("Run", "运行"));
    PresetBox().Header(locContent("Preset", "预设"));
    PresetQuick().Content(locContent("Quick run (best API / GPU, Medium)",
                                     "快速运行（最佳 API / GPU，中等）"));
    PresetCustom().Content(locContent("Custom run (choose API / GPU / workload)",
                                      "自定义运行（选择 API / GPU / 测试项目）"));
    PresetFullOne().Content(locContent("Full analysis — selected workload / one GPU (all APIs + RenderDoc + charts)",
                                       "完整分析 —— 所选测试 / 单 GPU（全部 API + RenderDoc + 图表）"));
    PresetFullAll().Content(locContent("Full analysis — selected workload / all GPUs × APIs (+ RenderDoc + charts)",
                                       "完整分析 —— 所选测试 / 全部 GPU × API（+ RenderDoc + 图表）"));
    PresetFlights().Content(locContent("Flights test — one GPU (all APIs, custom flights)",
                                       "Flights 测试 —— 单 GPU（全部 API，自定义 flights）"));
    PresetParticles().Content(locContent("Particle test — one GPU (all APIs, custom particles)",
                                         "粒子测试 —— 单 GPU（全部 API，自定义粒子数）"));
    PresetHeadless().Content(locContent("Headless compute — one GPU (all APIs, pure compute)",
                                        "无头计算 —— 单 GPU（全部 API，纯计算）"));
    GpuBox().Header(locContent("GPU / Renderer", "GPU / 渲染器"));
    BackendBox().Header(locContent("Graphics API", "图形 API"));
    BackendAuto().Content(locContent("Auto", "自动"));
    BackendVulkan().Content(locContent("Vulkan", "Vulkan"));
    BackendDx12().Content(locContent("DirectX 12", "DirectX 12"));
    BackendDx11().Content(locContent("DirectX 11", "DirectX 11"));
    BackendOpenGL().Content(locContent("OpenGL", "OpenGL"));
    WorkloadBox().Header(locContent("Workload", "测试项目"));
    WorkloadStream().Content(locContent("Particle — Memory Throughput",
                                        "粒子 —— 内存吞吐"));
    WorkloadGpuBurn().Content(locContent("GPU Burn — Plasma Bloom",
                                          "GPU Burn —— 等离子晶核"));
    WorkloadCinematicLiquid().Content(locContent("Cinematic Liquid v2 — Interactive Pool",
                                                  "电影化液体 v2 —— 互动水池"));
    WorkloadCinematicLiquidV1().Content(locContent("Other / Legacy Cinematic Liquid v1 — Dam Break",
                                                    "其他 / 旧版电影化液体 v1 —— 溃坝"));
    WorkloadGpuStress().Content(locContent("GraphicsBurn v1 / Component (Advanced)",
                                           "GraphicsBurn v1 / 分项（高级）"));
    WorkloadNBody().Content(locContent("N-Body — Advanced Compute",
                                       "N-Body —— 高级计算"));
    WorkloadSynthPeak().Content(locContent("SynthPeak — Advanced Synthetic",
                                           "SynthPeak —— 高级合成测试"));
    WorkloadStress().Content(locContent("Legacy Stress v1 — Fragment ALU/SFU",
                                        "旧版压力测试 v1 —— 片元 ALU/SFU"));
    WorkloadRender3D().Content(locContent("Legacy 3D Prototype — Billboards",
                                          "旧版 3D 原型 —— Billboard"));
    WorkloadVolumetric().Content(locContent("Volumetric — Experimental Raymarch",
                                             "体积渲染 —— 实验性 Raymarch"));
    WorkloadFluid().Content(locContent("Other / Legacy 2D Fluid — Vulkan-only",
                                        "其他 / 旧版 2D 流体 —— 仅 Vulkan"));
    PrecisionBox().Header(locContent("Precision", "精度"));
    PrecisionFp32().Content(locContent("fp32 — standard float", "fp32 —— 标准浮点"));
    PrecisionFp16().Content(locContent("fp16 — half precision", "fp16 —— 半精度"));
    PrecisionFp64().Content(locContent("fp64 — double precision", "fp64 —— 双精度"));
    PrecisionInt32().Content(locContent("int32 — integer ops", "int32 —— 整数运算"));
    DurationUnitBox().Header(locContent("Duration", "运行时长"));
    DurationSeconds().Content(locContent("Seconds", "秒"));
    DurationFrames().Content(locContent("Frames", "帧数"));
    DurationValueBox().Header(locContent("Value", "数值"));
    ParticlePresetBox().Header(locContent("Particles", "粒子数"));
    ParticlesLight().Content(locContent("Light — 65K", "轻量 —— 65K"));
    ParticlesMedium().Content(locContent("Medium — 1M", "中等 —— 1M"));
    ParticlesHeavy().Content(locContent("Heavy — 4M", "重载 —— 4M"));
    ParticlesExtreme().Content(locContent("Extreme — 16M", "极限 —— 16M"));
    ParticlesCustom().Content(locContent("Custom…", "自定义…"));
    CustomParticleBox().Header(locContent("Custom particles", "自定义粒子数"));
    CustomParticleBox().PlaceholderText(locText("multiple of 256", "256 的倍数"));
    AdvancedLabel().Text(locText("Advanced", "高级选项"));
    HeadlessBox().Content(locContent("Headless", "无头"));
    auto headlessTip = locContent(
        "Pure compute mode: no swapchain, no rendering, no present. "
        "Useful for measuring raw compute throughput.",
        "纯计算模式：无交换链、无渲染、无 present。用于测量原始计算吞吐。");
    ToolTipService::SetToolTip(HeadlessBox(), headlessTip);
    ToolTipService::SetToolTip(HeadlessInfo(), headlessTip);
    VsyncBox().Content(locContent("V-Sync", "垂直同步"));
    HostMemBox().Content(locContent("Host memory", "主机内存"));
    auto hostMemTip = locContent(
        "Keep the particle buffer in host-visible (CPU) RAM instead of VRAM — "
        "slower on discrete GPUs; for testing non-device-local / UMA memory.",
        "把粒子缓冲放在主机可见 (CPU) 内存而非显存 —— 独显上更慢；"
        "用于测试非显存 / UMA 访问。");
    ToolTipService::SetToolTip(HostMemBox(), hostMemTip);
    ToolTipService::SetToolTip(HostMemInfo(), hostMemTip);
    RunButton().Content(locContent("Run Benchmark", "运行测试"));
    Status().Text(locText("Ready", "就绪"));

    HistoryTitle().Text(locText("History", "历史"));
    RefreshButton().Content(locContent("Refresh", "刷新"));
    DeleteButton().Content(locContent("Delete selected", "删除所选"));
    OpenResultsFolderButton().Content(locContent("Open results folder", "打开成绩目录"));
    OpenCapturesFolderButton().Content(locContent("Open captures folder", "打开抓帧目录"));
    SortBox().Header(locContent("Sort by", "排序"));
    SortTime().Content(locContent("Time (newest)", "时间（最新）"));
    SortScore().Content(locContent("Score (high→low)", "分数（高→低）"));
    SortApi().Content(locContent("Graphics API", "图形 API"));
    SortDevice().Content(locContent("GPU / Renderer", "GPU / 渲染器"));
    SortWorkload().Content(locContent("Workload", "测试项目"));
    WorkloadFilterBox().Header(locContent("Workload", "测试项目"));
    WorkloadFilterAll().Content(locContent("All workloads", "全部项目"));
    ParticleFilterBox().Header(locContent("Particles", "粒子数"));
    ParticleFilterAll().Content(locContent("All particle counts", "全部粒子数"));
    TimeRangeBox().Header(locContent("Time range", "时间范围"));
    RangeAll().Content(locContent("All", "全部"));
    RangeToday().Content(locContent("Today", "今天"));
    Range7().Content(locContent("Last 7 days", "近 7 天"));
    Range30().Content(locContent("Last 30 days", "近 30 天"));
    RangeCustom().Content(locContent("Custom range…", "自定义范围…"));
    FromDate().Header(locContent("From", "起始"));
    ToDate().Header(locContent("To", "结束"));
    FromDate().PlaceholderText(locText("select a date", "选择日期"));
    ToDate().PlaceholderText(locText("select a date", "选择日期"));
    FromDate().DateFormat(i18n::currentLang() == i18n::Lang::Zh
        ? hstring(L"{year.full}-{month.integer(2)}-{day.integer(2)}")
        : hstring(L"{day.integer(2)}/{month.integer(2)}/{year.full}"));
    ToDate().DateFormat(i18n::currentLang() == i18n::Lang::Zh
        ? hstring(L"{year.full}-{month.integer(2)}-{day.integer(2)}")
        : hstring(L"{day.integer(2)}/{month.integer(2)}/{year.full}"));
    GpuFilterLabel().Text(locText("GPUs", "显卡"));
    SelectAllGpus().Content(locContent("All", "全选"));
    ClearAllGpus().Content(locContent("None", "清空"));
    ChartsTitle().Text(locText("Charts", "图表"));
    GenChartsButton().Content(locContent("Generate Charts", "生成图表"));

    SettingsTitle().Text(locText("Settings", "设置"));
    ThemeBox().Header(locContent("Theme", "主题"));
    ThemeSystem().Content(locContent("Use system setting", "跟随系统"));
    ThemeLight().Content(locContent("Light", "浅色"));
    ThemeDark().Content(locContent("Dark", "深色"));
    LangBox().Header(locContent("Language", "语言"));
    AboutTitle().Text(locText("About", "关于"));
    AboutDesc().Text(locText("Multi-API GPU benchmark — native C++/WinRT front-end driving the engine in-process.",
                             "多 API GPU 跑分 —— 原生 C++/WinRT 前端，进程内驱动引擎。"));

    {
        const wchar_t* sys = (i18n::detectOsLang() == i18n::Lang::Zh) ? L"中文" : L"English";
        std::wstring label = std::wstring(locText("Auto", "自动").c_str()) + L" (" + sys + L")";
        LangAuto().Content(winrt::box_value(hstring(label)));
    }

    m_suppressCombo = true;
    auto refreshCombo = [](ComboBox const& cb) { int s = cb.SelectedIndex(); cb.SelectedIndex(-1); cb.SelectedIndex(s); };
    refreshCombo(ThemeBox());
    refreshCombo(LangBox());
    refreshCombo(SortBox());
    refreshCombo(WorkloadFilterBox());
    refreshCombo(ParticleFilterBox());
    refreshCombo(TimeRangeBox());
    refreshCombo(PresetBox());
    refreshCombo(DurationUnitBox());
    refreshCombo(BackendBox());
    refreshCombo(WorkloadBox());
    refreshCombo(PrecisionBox());
    refreshCombo(ParticlePresetBox());
    refreshCombo(CpuModeBox());
    refreshCombo(CpuDurationPresetBox());
    m_suppressCombo = false;

    rebuildHistoryFilters();
    updateExtraLabel();
}

// ---- CPU benchmark page ----------------------------------------------------
void MainWindow::OnCpuDurationPresetChanged(IInspectable const&,
                                             SelectionChangedEventArgs const&)
{
    if (!m_uiReady || m_suppressCombo || m_cpuRunning.load()) return;
    const auto value = selected(CpuDurationPresetBox());
    if (value.empty()) return;
    try
    {
        const double seconds = std::stod(value);
        if (std::isfinite(seconds) && seconds > 0.0) CpuTimeBox().Value(seconds);
    }
    catch (...) {}
}

void MainWindow::OnCpuTimeChanged(NumberBox const&,
                                  NumberBoxValueChangedEventArgs const& args)
{
    if (!m_uiReady || m_suppressCombo || m_cpuRunning.load()) return;
    const double value = args.NewValue();
    int preset = -1;
    if (std::isfinite(value) && std::abs(value - 1.0) < 0.0001) preset = 0;
    else if (std::isfinite(value) && std::abs(value - 15.0) < 0.0001) preset = 1;

    m_suppressCombo = true;
    CpuDurationPresetBox().SelectedIndex(preset);
    m_suppressCombo = false;
}

void MainWindow::OnCpuRun(IInspectable const&, RoutedEventArgs const&)
{
    if (m_cpuRunning.load()) return;

    auto mode = selected(CpuModeBox());
    if (mode != "per-core" && mode != "multi" && mode != "all") mode = "all";
    const double seconds = CpuTimeBox().Value();
    const double warmup = CpuWarmupBox().Value();
    if (!std::isfinite(seconds) || seconds < 0.1 || seconds > 3600.0 ||
        !std::isfinite(warmup) || warmup < 0.0 || warmup > 60.0)
    {
        CpuStatusText().Text(locText(
            "Enter a valid duration and warm-up time.",
            "请输入有效的测试时长和预热时间。"));
        return;
    }

    const std::filesystem::path engine(m_enginePath);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(engine, ec) ||
        _wcsicmp(engine.filename().c_str(), L"gpu_benchmark.exe") != 0)
    {
        CpuStatusText().Text(locText(
            "gpu_benchmark.exe was not found beside the GUI or in the build directory.",
            "未在 GUI 同目录或构建目录中找到 gpu_benchmark.exe。"));
        return;
    }

    launchCpuBenchmark(std::move(mode), seconds, warmup);
}

void MainWindow::OnCpuCancel(IInspectable const&, RoutedEventArgs const&)
{
    cancelCpuBenchmark();
}

void MainWindow::cancelCpuBenchmark()
{
    if (!m_cpuRunning.load()) return;
    m_cpuCancelRequested.store(true);
    CpuCancelButton().IsEnabled(false);
    CpuStatusText().Text(locText("Cancelling...", "正在取消…"));
    CpuCurrentCoreText().Text(locText("Stopping the CPU worker", "正在停止 CPU 测试进程"));

    std::lock_guard<std::mutex> lock(m_cpuProcessMutex);
    if (m_cpuProcess) ::TerminateProcess(m_cpuProcess, ERROR_CANCELLED);
}

void MainWindow::launchCpuBenchmark(std::string mode, double seconds,
                                    double warmupSeconds)
{
    if (m_cpuRunning.exchange(true)) return;
    m_cpuCancelRequested.store(false);
    m_cpuHadProtocolError = false;
    m_cpuCoreLabels.clear();

    CpuCoreResultsList().Items().Clear();
    CpuAverageResult().Text(L"\x2014");
    CpuMultiResult().Text(L"\x2014");
    CpuOutputBox().Text(L"");
    CpuProgressBar().Value(0.0);
    CpuProgressText().Text(L"0%");
    CpuCurrentCoreText().Text(locText("Starting CPU worker...", "正在启动 CPU 测试进程…"));
    CpuStatusText().Text(locText("Running", "运行中"));
    CpuRunButton().IsEnabled(false);
    CpuCancelButton().IsEnabled(true);
    CpuModeBox().IsEnabled(false);
    CpuDurationPresetBox().IsEnabled(false);
    CpuTimeBox().IsEnabled(false);
    CpuWarmupBox().IsEnabled(false);

    const auto engine = std::filesystem::absolute(std::filesystem::path(m_enginePath));
    const auto workingDirectory = engine.parent_path();
    auto strong = get_strong();
    auto dispatcher = m_dispatcher;

    std::thread([this, strong, dispatcher, engine, workingDirectory,
                 mode = std::move(mode), seconds, warmupSeconds]()
    {
        auto finish = [this, strong, dispatcher](DWORD exitCode,
                                                 std::string errorMessage)
        {
            const bool cancelled = m_cpuCancelRequested.load();
            dispatcher.TryEnqueue([this, strong, exitCode, cancelled,
                                   errorMessage = std::move(errorMessage)]()
            {
                m_cpuRunning.store(false);
                CpuRunButton().IsEnabled(true);
                CpuCancelButton().IsEnabled(false);
                CpuModeBox().IsEnabled(true);
                CpuDurationPresetBox().IsEnabled(true);
                CpuTimeBox().IsEnabled(true);
                CpuWarmupBox().IsEnabled(true);

                if (cancelled)
                {
                    CpuStatusText().Text(locText("Cancelled", "已取消"));
                    CpuCurrentCoreText().Text(locText("Benchmark cancelled", "CPU 跑分已取消"));
                    return;
                }
                if (exitCode != 0 || !errorMessage.empty() || m_cpuHadProtocolError)
                {
                    if (!errorMessage.empty())
                    {
                        auto output = std::wstring(CpuOutputBox().Text().c_str());
                        output += L"\r\n[GUI] ";
                        output += u8(errorMessage).c_str();
                        output += L"\r\n";
                        CpuOutputBox().Text(output);
                    }
                    CpuStatusText().Text(locText("Failed; see raw output", "失败；请查看原始输出"));
                    CpuCurrentCoreText().Text(locText("CPU benchmark failed", "CPU 跑分失败"));
                    return;
                }

                CpuProgressBar().Value(100.0);
                CpuProgressText().Text(L"100%");
                CpuStatusText().Text(locText("Done", "完成"));
                CpuCurrentCoreText().Text(locText("CPU benchmark completed", "CPU 跑分完成"));
            });
        };

        SECURITY_ATTRIBUTES security{};
        security.nLength = sizeof(security);
        security.bInheritHandle = TRUE;
        HANDLE readPipe = nullptr;
        HANDLE writePipe = nullptr;
        if (!::CreatePipe(&readPipe, &writePipe, &security, 0))
        {
            const DWORD pipeCreateError = ::GetLastError();
            finish(pipeCreateError,
                   "CreatePipe failed: " + win32ErrorText(pipeCreateError));
            return;
        }
        ::SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        std::wostringstream command;
        command.imbue(std::locale::classic());
        command << L'"' << engine.wstring() << L"\" --cpu-benchmark ";
        command << std::wstring(mode.begin(), mode.end());
        command << L" --cpu-time " << std::setprecision(12) << seconds
                << L" --cpu-warmup " << std::setprecision(12) << warmupSeconds;
        auto commandLine = command.str();
        std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
        mutableCommand.push_back(L'\0');

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = writePipe;
        startup.hStdError = writePipe;
        // A WinUI process normally has no console stdin. Passing that NULL or
        // non-inheritable pseudo handle with STARTF_USESTDHANDLES can make
        // CreateProcess fail with ERROR_INVALID_HANDLE on clean installs.
        HANDLE nullInput = ::CreateFileW(
            L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (nullInput == INVALID_HANDLE_VALUE)
        {
            const DWORD nullError = ::GetLastError();
            ::CloseHandle(writePipe);
            ::CloseHandle(readPipe);
            finish(nullError, "Could not open NUL for child stdin: " +
                   win32ErrorText(nullError));
            return;
        }
        startup.hStdInput = nullInput;
        PROCESS_INFORMATION process{};
        const BOOL created = ::CreateProcessW(
            engine.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, workingDirectory.c_str(), &startup, &process);
        const DWORD createError = created ? ERROR_SUCCESS : ::GetLastError();
        ::CloseHandle(nullInput);
        ::CloseHandle(writePipe);
        writePipe = nullptr;
        if (!created)
        {
            ::CloseHandle(readPipe);
            finish(createError, "CreateProcess failed: " + win32ErrorText(createError));
            return;
        }
        ::CloseHandle(process.hThread);

        {
            std::lock_guard<std::mutex> lock(m_cpuProcessMutex);
            m_cpuProcess = process.hProcess;
            if (m_cpuCancelRequested.load())
                ::TerminateProcess(process.hProcess, ERROR_CANCELLED);
        }

        auto dispatchLine = [this, strong, dispatcher](std::string line)
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            dispatcher.TryEnqueue([this, strong, line = std::move(line)]()
            {
                auto raw = std::wstring(CpuOutputBox().Text().c_str());
                raw += u8(line).c_str();
                raw += L"\r\n";
                // A formal per-core run can emit tens of thousands of progress
                // records. Keep a useful live tail without making every TextBox
                // update copy an ever-growing multi-megabyte string.
                constexpr std::size_t kMaxVisibleCpuOutput = 256u * 1024u;
                if (raw.size() > kMaxVisibleCpuOutput)
                {
                    const auto firstLine = raw.find(L'\n', raw.size() - kMaxVisibleCpuOutput);
                    raw.erase(0, firstLine == std::wstring::npos ?
                        raw.size() - kMaxVisibleCpuOutput : firstLine + 1);
                }
                CpuOutputBox().Text(raw);

                const auto parsed = parseCpuProtocolLine(line);
                if (parsed.type == CpuLineType::Other) return;

                if (parsed.type == CpuLineType::Meta)
                {
                    const auto name = cpuField(parsed, { "name", "cpu_name" });
                    const auto logical = cpuInteger(parsed, { "logical_count" });
                    const auto physical = cpuInteger(parsed, { "physical_count" });
                    const auto affinity = cpuField(parsed, { "affinity_capability" });
                    std::ostringstream label;
                    label << (name.empty() ? m_cpuName : name);
                    if (logical) label << "  |  " << *logical << " logical";
                    if (physical) label << " / " << *physical << " physical";
                    if (!affinity.empty()) label << "  |  " << affinity;
                    CpuNameText().Text(u8(label.str()));
                    return;
                }

                if (parsed.type == CpuLineType::Topology)
                {
                    const auto core = cpuInteger(parsed, { "core_index", "ordinal" });
                    if (!core) return;
                    const int logical = cpuInteger(parsed, { "logical_index" }).value_or(*core);
                    const int physical = cpuInteger(parsed, { "physical_core" }).value_or(*core);
                    const int smt = cpuInteger(parsed, { "smt_index" }).value_or(0);
                    const int width = cpuInteger(parsed, { "smt_width" }).value_or(1);
                    const int group = cpuInteger(parsed, { "group" }).value_or(0);
                    const auto coreClass = cpuField(parsed, { "core_class", "class" });
                    std::ostringstream label;
                    label << "LP " << logical << " / physical " << (physical + 1)
                          << " / SMT " << (smt + 1) << '/' << width;
                    if (group != 0) label << " / group " << group;
                    if (!coreClass.empty()) label << " / " << coreClass;
                    m_cpuCoreLabels[*core] = label.str();
                    return;
                }

                if (parsed.type == CpuLineType::Progress)
                {
                    const double fraction = cpuNumber(parsed, { "overall_fraction", "fraction" })
                        .value_or(cpuNumber(parsed, { "core_fraction" }).value_or(0.0));
                    const double percent = std::clamp(fraction * 100.0, 0.0, 100.0);
                    CpuProgressBar().Value(percent);
                    std::wostringstream progress;
                    progress << std::fixed << std::setprecision(1) << percent << L'%';
                    CpuProgressText().Text(progress.str());

                    const auto modeName = cpuField(parsed, { "mode" });
                    const auto phase = cpuField(parsed, { "phase" });
                    const int core = cpuInteger(parsed, { "core_index" }).value_or(-1);
                    hstring phaseText = phase == "warmup"
                        ? locText("warming up", "预热")
                        : phase == "measure"
                            ? locText("measuring", "测量")
                            : locText("finishing", "收尾");
                    if (modeName == "multi" || core < 0)
                    {
                        std::wstring status = locText("All-core", "多核").c_str();
                        status += L" · "; status += phaseText.c_str();
                        CpuCurrentCoreText().Text(status);
                    }
                    else
                    {
                        std::string coreLabel = "logical processor " + std::to_string(core + 1);
                        if (auto it = m_cpuCoreLabels.find(core); it != m_cpuCoreLabels.end())
                            coreLabel = it->second;
                        std::wstring status = u8(coreLabel).c_str();
                        status += L" · "; status += phaseText.c_str();
                        CpuCurrentCoreText().Text(status);
                    }
                    return;
                }

                if (parsed.type == CpuLineType::Error)
                {
                    m_cpuHadProtocolError = true;
                    auto message = cpuField(parsed, { "message", "error" });
                    std::replace(message.begin(), message.end(), '_', ' ');
                    CpuStatusText().Text(message.empty()
                        ? locText("CPU engine error", "CPU 引擎错误") : u8(message));
                    return;
                }

                const auto kind = cpuField(parsed, { "kind", "type" });
                const auto unit = cpuField(parsed, { "unit" });
                if (kind == "core")
                {
                    const int core = cpuInteger(parsed, { "core_index" }).value_or(-1);
                    const auto score = cpuNumber(parsed, { "score" });
                    if (core < 0 || !score) return;
                    const auto affinity = cpuField(parsed, { "affinity" });
                    const bool valid = cpuInteger(parsed, { "valid" }).value_or(1) != 0;
                    std::string label = "logical processor " + std::to_string(core + 1);
                    if (auto it = m_cpuCoreLabels.find(core); it != m_cpuCoreLabels.end())
                        label = it->second;
                    std::string row = "#" + std::to_string(core + 1) + "  " + label
                        + "  |  " + formatCpuScore(*score, unit);
                    if (!affinity.empty()) row += "  [" + affinity + "]";
                    if (!valid || affinity == "failed" || affinity == "partial")
                        row = "!  " + row;
                    CpuCoreResultsList().Items().Append(box_value(u8(row)));
                    return;
                }
                if (kind == "summary")
                {
                    const auto average = cpuNumber(parsed, { "average_score", "score" });
                    if (average)
                    {
                        std::string value = formatCpuScore(*average, unit);
                        if (const auto contract = cpuField(parsed, { "score_contract" });
                            !contract.empty()) value += "  [" + contract + "]";
                        CpuAverageResult().Text(u8(value));
                    }
                    return;
                }
                if (kind == "multi")
                {
                    const auto score = cpuNumber(parsed, { "score" });
                    if (!score) return;
                    std::string value = formatCpuScore(*score, unit);
                    if (const auto threads = cpuInteger(parsed, { "thread_count" }))
                        value += "  (" + std::to_string(*threads) + " threads)";
                    const auto affinity = cpuField(parsed, { "affinity" });
                    if (!affinity.empty()) value += "  [" + affinity + "]";
                    if (const auto contract = cpuField(parsed, { "score_contract" });
                        !contract.empty()) value += "  [" + contract + "]";
                    CpuMultiResult().Text(u8(value));
                }
            });
        };

        std::string pending;
        std::array<char, 4096> buffer{};
        DWORD bytesRead = 0;
        DWORD readError = ERROR_SUCCESS;
        while (::ReadFile(readPipe, buffer.data(), static_cast<DWORD>(buffer.size()),
                          &bytesRead, nullptr) && bytesRead > 0)
        {
            pending.append(buffer.data(), bytesRead);
            for (;;)
            {
                const auto newline = pending.find('\n');
                if (newline == std::string::npos) break;
                dispatchLine(pending.substr(0, newline));
                pending.erase(0, newline + 1);
            }
        }
        readError = ::GetLastError();
        if (!pending.empty()) dispatchLine(std::move(pending));
        ::CloseHandle(readPipe);

        ::WaitForSingleObject(process.hProcess, INFINITE);
        DWORD exitCode = 1;
        ::GetExitCodeProcess(process.hProcess, &exitCode);
        {
            std::lock_guard<std::mutex> lock(m_cpuProcessMutex);
            if (m_cpuProcess == process.hProcess) m_cpuProcess = nullptr;
        }
        ::CloseHandle(process.hProcess);

        std::string pipeError;
        if (readError != ERROR_SUCCESS && readError != ERROR_BROKEN_PIPE)
            pipeError = "stdout pipe failed: " + win32ErrorText(readError);
        finish(exitCode, std::move(pipeError));
    }).detach();
}

void MainWindow::updateExtraLabel()
{
    auto wl = selected(WorkloadBox());
    int preset = PresetBox().SelectedIndex();
    bool customRun = preset == 1;
    bool fullAnalysis = preset == 2 || preset == 3;
    bool workloadSelectable = customRun || fullAnalysis;
    bool particleTest = preset == 5;
    bool flightsTest = preset == 4;
    bool particleWorkload = wl == "stream" || wl == "render3d";
    bool showParticles = particleTest || (workloadSelectable && particleWorkload);
    bool fixedQualityLiquid = wl == "cinematic_liquid" || wl == "cinematic_liquid_v1";
    bool showExtra = flightsTest ||
        (workloadSelectable && !particleWorkload && !fixedQualityLiquid);
    bool fluidWorkload = workloadSelectable && wl == "fluid";
    std::string infoWl = workloadSelectable ? wl : "stream";

    // Custom and Full Analysis both honour the selected workload. Other
    // specialised presets retain their historical Stream-only behaviour.
    WorkloadBox().IsEnabled(workloadSelectable);
    PrecisionBox().IsEnabled(workloadSelectable && wl == "synthpeak");
    bool headlessSupported = customRun && !(wl == "gpu_burn" || wl == "gpu_stress" || wl == "stress"
        || wl == "render3d" || wl == "volumetric" || wl == "fluid"
        || wl == "cinematic_liquid" || wl == "cinematic_liquid_v1");
    if (!headlessSupported) HeadlessBox().IsChecked(false);
    HeadlessBox().IsEnabled(headlessSupported);
    bool hostMemorySupported = !(workloadSelectable && fixedQualityLiquid);
    if (!hostMemorySupported) HostMemBox().IsChecked(false);
    HostMemBox().IsEnabled(hostMemorySupported);

    ParticlePresetBox().Visibility(showParticles ? Visibility::Visible : Visibility::Collapsed);
    CustomParticleBox().Visibility(showParticles && selected(ParticlePresetBox()) == "custom"
        ? Visibility::Visible : Visibility::Collapsed);
    ExtraBox().Visibility(showExtra ? Visibility::Visible : Visibility::Collapsed);
    FluidJacobiBox().Visibility(fluidWorkload ? Visibility::Visible : Visibility::Collapsed);
    Grid::SetColumnSpan(ExtraBox(), fluidWorkload ? 1 : 2);

    if (flightsTest)             ExtraBox().Header(locContent("Flights (--flights)", "Flights (--flights)"));
    else if (wl == "nbody")      ExtraBox().Header(locContent("Bodies (--bodies)", "天体数 (--bodies)"));
    else if (wl == "gpu_burn")   ExtraBox().Header(locContent("Fixed steps (--iter)", "固定步数 (--iter)"));
    else if (wl == "gpu_stress") ExtraBox().Header(locContent("Iterations (--iter)", "迭代次数 (--iter)"));
    else if (wl == "stress")     ExtraBox().Header(locContent("Iterations (--iter)", "迭代次数 (--iter)"));
    else if (wl == "synthpeak")  ExtraBox().Header(locContent("Iterations (--iter)", "迭代次数 (--iter)"));
    else if (wl == "volumetric") ExtraBox().Header(locContent("Ray steps (--steps)", "光线步数 (--steps)"));
    else if (wl == "fluid")      ExtraBox().Header(locContent("Grid size (--grid)", "网格尺寸 (--grid)"));
    else                          ExtraBox().Header(locContent("Extra", "额外参数"));

    ExtraBox().PlaceholderText(wl == "gpu_burn"
        ? locText("leave empty: safe auto-tune; fixed 16-32", "留空：安全自动标定；固定值 16-32")
        : wl == "nbody"
        ? locText("default 65536; DX11 FL10/SM4: max 4096", "默认 65536；DX11 FL10/SM4：最多 4096")
        : wl == "volumetric"
        ? locText("optional; default 96", "可选；默认 96")
        : wl == "fluid"
        ? locText("optional; default 256", "可选；默认 256")
        : locText("optional", "可选"));
    FluidJacobiBox().Header(locContent("Jacobi iterations (--jacobi)", "Jacobi 迭代次数 (--jacobi)"));
    FluidJacobiBox().PlaceholderText(locText("optional; default 30", "可选；默认 30"));

    WorkloadInfo().IsOpen(true);
    WorkloadInfo().Severity(InfoBarSeverity::Informational);
    if (infoWl == "stream")
    {
        WorkloadInfo().Title(locText("Primary / Baseline", "主要测试 / 历史基线"));
        WorkloadInfo().Message(locText(
            "Original particle test. Preserves the historical stream workload and primarily measures memory throughput.",
            "原始粒子测试；保留历史 stream workload，主要衡量内存/显存吞吐。"));
    }
    else if (infoWl == "gpu_burn")
    {
        WorkloadInfo().Title(locText("Primary / 15s Burst", "主要测试 / 15 秒 Burst"));
        WorkloadInfo().Message(locText(
            "Original visual GraphicsBurn: a rotating Plasma Bloom core with crystalline spikes and electric corona. It is a 15-second burst, not a long thermal-soak or hardware-error certification.",
            "原创图形烤机：旋转的等离子晶核、晶刺与电弧光晕。这是 15 秒 Burst，不是长时间热饱和或硬件错误认证。"));
    }
    else if (infoWl == "cinematic_liquid")
    {
        WorkloadInfo().Title(locText("Primary / Coupled 3D Liquid v2", "主要测试 / 流固耦合 3D 液体 v2"));
        WorkloadInfo().Message(locText(
            "Large coupled 3D liquid scene with the preserved rubber-duck family, a buoyant play ball, a finite-mass soft-tethered propeller boat, and a near-1g solid-sphere entry. Water can cross the finite clear-PVC pool rim onto procedural grass beneath an atmospheric sky. The v7 score contract remains independent from earlier liquid versions.",
            "大型流固耦合 3D 液体场景：保留大黄鸭子母家族与漂浮彩球；螺旋桨船具有有限质量、软系泊和真实反冲；实心球以接近 1g 入水。水可越过有限高度的透明 PVC 池沿并落到程序化草地，场景使用大气天空；v7 成绩契约与旧液体版本严格隔离。"));
    }
    else if (infoWl == "cinematic_liquid_v1")
    {
        WorkloadInfo().Title(locText("Other / Legacy 3D Liquid v1", "其他 / 旧版 3D 液体 v1"));
        WorkloadInfo().Message(locText(
            "Preserved original asymmetric dam-break workload. Its simulation, renderer and score contract remain unchanged for historical comparison.",
            "保留原始非对称溃坝测试；模拟、渲染和成绩契约保持不变，用于历史对比。"));
    }
    else if (infoWl == "gpu_stress")
    {
        WorkloadInfo().Title(locText("Other / Advanced Component", "其他 / 高级分项"));
        WorkloadInfo().Message(locText(
            "GraphicsBurn v1 component test retained for advanced diagnosis; GPU Burn is the primary product test.",
            "保留用于高级诊断的 GraphicsBurn v1 分项；GPU Burn 才是主要产品测试。"));
    }
    else if (infoWl == "nbody")
    {
        WorkloadInfo().Title(locText("Other / Advanced Compute", "其他 / 高级计算"));
        WorkloadInfo().Message(locText("Tiled all-pairs particle compute microbenchmark.",
                                        "分块全粒子对计算微测试。"));
    }
    else if (infoWl == "synthpeak")
    {
        WorkloadInfo().Title(locText("Other / Advanced Synthetic", "其他 / 高级合成测试"));
        WorkloadInfo().Message(locText("Compiler- and driver-sensitive synthetic ALU throughput test.",
                                        "对编译器和驱动行为敏感的合成 ALU 吞吐测试。"));
    }
    else if (infoWl == "stress")
    {
        WorkloadInfo().Title(locText("Legacy Stress v1", "旧版压力测试 v1"));
        WorkloadInfo().Message(locText("Fragment ALU/SFU prototype; this is not the planned GPU Stress product test.",
                                        "片元 ALU/SFU 原型；不是规划中的正式 GPU Stress 产品测试。"));
    }
    else if (infoWl == "render3d")
    {
        WorkloadInfo().Title(locText("Legacy 3D Prototype", "旧版 3D 原型"));
        WorkloadInfo().Message(locText("Instanced billboard prototype; this is not the planned Cinematic Liquid scene.",
                                        "实例化 Billboard 原型；不是规划中的 Cinematic Liquid 综合场景。"));
    }
    else if (infoWl == "volumetric")
    {
        WorkloadInfo().Title(locText("Other / Experimental", "其他 / 实验性"));
        WorkloadInfo().Message(locText("Procedural volumetric raymarch prototype; no repository validation result yet.",
                                        "程序化体积 Raymarch 原型；仓库中尚无可复核验证结果。"));
    }
    else if (infoWl == "fluid")
    {
        WorkloadInfo().Severity(InfoBarSeverity::Warning);
        WorkloadInfo().Title(locText("Other / Legacy 2D Fluid", "其他 / 旧版 2D 流体"));
        WorkloadInfo().Message(locText(
            "Legacy projected-dye Stable Fluids prototype. It is not the planned 3D Cinematic Liquid workload; its score remains unverified and it is Vulkan-only.",
            "旧版投影染料 Stable Fluids 原型。它不是计划中的 3D 电影化液体测试；分数仍未验证，且仅支持 Vulkan。"));
    }
}

// ---- GPU enumeration -------------------------------------------------------
void MainWindow::populateGpus()
{
    GpuBox().Items().Clear();
    m_gpuIndices.clear();
    m_gpuApiSupport.clear();
    auto autoItem = ComboBoxItem(); autoItem.Content(locContent("(auto)", "（自动）"));
    GpuBox().Items().Append(autoItem);
    GpuBox().SelectedIndex(0);
    if (m_enginePath.empty()) return;

    // One row of `--list-gpus`: GPU \t idx \t name \t vk \t dx12 \t dx11
    // \t opengl \t dx11FeatureLevel \t dx11Compute. The last two fields are
    // optional so a newer GUI can still drive an older engine build.
    struct GpuRow { int idx; std::string name; std::array<bool, 5> api; };

    auto strong = get_strong();
    auto disp = m_dispatcher;
    std::string engine = m_enginePath;
    configureBundledRenderDocLayer(engine);
    std::thread([this, strong, disp, engine]()
    {
        std::string out = captureCli({ engine, "--list-gpus" }).output;
        std::vector<GpuRow> gpus;
        std::istringstream ss(out); std::string line;
        while (std::getline(ss, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind("GPU\t", 0) != 0) continue;
            std::vector<std::string> f; std::stringstream ls(line); std::string tok;
            while (std::getline(ls, tok, '\t')) f.push_back(tok);
            if (f.size() < 3) continue;
            GpuRow r;
            try { r.idx = std::stoi(f[1]); } catch (...) { continue; }
            r.name = f[2];
            // Default to supported when the field is absent (older engine).
            for (int k = 0; k < 4; ++k)
                r.api[k] = (f.size() > (size_t)(3 + k)) ? (f[3 + k] == "1") : true;
            r.api[4] = f.size() > 8 ? (f[8] == "1") : r.api[2];
            gpus.push_back(std::move(r));
        }
        disp.TryEnqueue([this, strong, gpus]()
        {
            for (auto& g : gpus)
            {
                m_gpuIndices.push_back(g.idx);
                m_gpuApiSupport.push_back(g.api);
                std::string label = isSoftwareGpu(g.name)
                    ? std::to_string(g.idx) + ": " + (m_cpuName.empty() ? "CPU" : m_cpuName) + "  (CPU / WARP)"
                    : std::to_string(g.idx) + ": " + g.name;
                auto it = ComboBoxItem();
                it.Content(box_value(u8(label)));
                GpuBox().Items().Append(it);
            }
        });
    }).detach();
}

void MainWindow::OnWorkloadChanged(IInspectable const&, SelectionChangedEventArgs const&)
{
    if (!m_uiReady) return;
    if (!m_suppressCombo)
    {
        // Parameter meanings differ radically between workloads. Do not carry
        // an N-body count into GPU Stress as an unsafe explicit iteration count.
        ExtraBox().Text(L"");
        FluidJacobiBox().Text(L"");
    }
    // PrecisionBox enable state is handled inside updateExtraLabel() which
    // correctly checks customRun; doing it here would bypass that guard.
    updateExtraLabel();
}

void MainWindow::OnParticlePresetChanged(IInspectable const&, SelectionChangedEventArgs const&)
{
    if (!m_uiReady || m_suppressCombo) return;
    updateExtraLabel();
}

void MainWindow::OnDurationUnitChanged(IInspectable const&, SelectionChangedEventArgs const&)
{
    if (!m_uiReady || m_suppressCombo) return;
    // Index 0 = Seconds (default), 1 = Frames. Swap to the unit's canonical
    // default value unless the user typed a custom one.
    bool seconds = DurationUnitBox().SelectedIndex() <= 0;
    auto cur = to_string(DurationValueBox().Text());
    if (seconds && (cur.empty() || cur == "600"))      DurationValueBox().Text(L"15");
    else if (!seconds && (cur.empty() || cur == "15")) DurationValueBox().Text(L"600");
}

// ---- run -------------------------------------------------------------------
std::string MainWindow::backendValue()
{
    return selected(BackendBox());       // "auto" when Auto is selected (Tag-based)
}

std::string MainWindow::particleValue()
{
    auto p = selected(ParticlePresetBox());
    if (p == "custom") return to_string(CustomParticleBox().Text());
    return p;                            // preset particle count
}

// Duration as engine args. Default is time-based (--time <seconds>, default 15)
// to stay comparable with the existing results database; switch the unit to
// Frames for a fixed frame-count run (--benchmark <frames>).
std::vector<std::string> MainWindow::durationArgs()
{
    bool seconds = DurationUnitBox().SelectedIndex() <= 0;   // 0 = Seconds (default)
    auto val = to_string(DurationValueBox().Text());
    if (val.empty()) val = seconds ? "15" : "600";
    return seconds ? std::vector<std::string>{ "--time", val }
                   : std::vector<std::string>{ "--benchmark", val };
}

std::vector<std::string> MainWindow::buildArgs(bool runAll)
{
    std::vector<std::string> args = { m_enginePath };
    for (auto& d : durationArgs()) args.push_back(d);

    if (runAll)
    {
        args.push_back("--run-all");     // iterates every GPU x API; no --gpu/--backend
    }
    else
    {
        auto b = backendValue();
        if (!b.empty()) { args.push_back("--backend"); args.push_back(b); }
        // Software / WARP renderer is just another entry in the GPU dropdown.
        int sel = GpuBox().SelectedIndex();
        if (sel > 0 && (size_t)(sel - 1) < m_gpuIndices.size())
        { args.push_back("--gpu"); args.push_back(std::to_string(m_gpuIndices[sel - 1])); }
    }

    auto wl = selected(WorkloadBox());
    if (wl != "stream") { args.push_back("--workload"); args.push_back(wl); }
    if (wl == "synthpeak") { args.push_back("--precision"); args.push_back(selected(PrecisionBox())); }

    auto extra = to_string(ExtraBox().Text());
    auto particles = particleValue();
    if ((wl == "stream" || wl == "render3d") && !particles.empty())
    {
        args.push_back("--particles"); args.push_back(particles);
    }
    else if (!extra.empty())
    {
        std::string flag = (wl == "nbody") ? "--bodies"
                         : (wl == "gpu_burn" || wl == "gpu_stress" || wl == "stress" || wl == "synthpeak") ? "--iter"
                         : (wl == "volumetric") ? "--steps"
                         : (wl == "fluid") ? "--grid" : "--particles";
        args.push_back(flag); args.push_back(extra);
    }
    if (wl == "fluid")
    {
        auto jacobi = to_string(FluidJacobiBox().Text());
        if (!jacobi.empty()) { args.push_back("--jacobi"); args.push_back(jacobi); }
    }

    const bool headless = HeadlessBox().IsChecked() && HeadlessBox().IsChecked().Value();
    if (headless) args.push_back("--headless");
    if (VsyncBox().IsChecked() && VsyncBox().IsChecked().Value())       args.push_back("--vsync");
    if (HostMemBox().IsChecked() && HostMemBox().IsChecked().Value())   args.push_back("--host-memory");
    if (!headless) {
        args.push_back("--capture");
        args.push_back("5");
    }
    return args;
}

// Build the cliMain invocation(s) for the selected preset (the CLI menu items).
std::vector<std::vector<std::string>> MainWindow::buildPresetJobs(bool& needCharts)
{
    needCharts = false;
    int p = PresetBox().SelectedIndex();

    std::vector<std::string> dur = durationArgs();   // --time <s> or --benchmark <frames>
    std::string extra = to_string(ExtraBox().Text());
    std::string particles = particleValue();

    // Arguments shared by the workload-aware Full Analysis presets. Keep the
    // mapping identical to Custom Run so a selected GPU Burn really reaches
    // every scheduled API/device instead of silently falling back to Stream.
    auto selectedWorkloadArgs = [&]() {
        std::vector<std::string> ex;
        const auto wl = selected(WorkloadBox());
        if (wl != "stream") { ex.push_back("--workload"); ex.push_back(wl); }
        if (wl == "synthpeak") {
            ex.push_back("--precision"); ex.push_back(selected(PrecisionBox()));
        }
        if ((wl == "stream" || wl == "render3d") && !particles.empty()) {
            ex.push_back("--particles"); ex.push_back(particles);
        } else if (!extra.empty()) {
            std::string flag = (wl == "nbody") ? "--bodies"
                             : (wl == "gpu_burn" || wl == "gpu_stress" || wl == "stress" || wl == "synthpeak") ? "--iter"
                             : (wl == "volumetric") ? "--steps"
                             : (wl == "fluid") ? "--grid" : "--particles";
            ex.push_back(flag); ex.push_back(extra);
        }
        if (wl == "fluid") {
            auto jacobi = to_string(FluidJacobiBox().Text());
            if (!jacobi.empty()) { ex.push_back("--jacobi"); ex.push_back(jacobi); }
        }
        if (VsyncBox().IsChecked() && VsyncBox().IsChecked().Value())
            ex.push_back("--vsync");
        if (HostMemBox().IsChecked() && HostMemBox().IsChecked().Value())
            ex.push_back("--host-memory");
        return ex;
    };

    int gpuRow = GpuBox().SelectedIndex();   // 0 = (auto)
    int gpuArg = -1;   // engine GPU index for one-GPU presets; -1 = auto
    const std::array<bool, 5>* sup = nullptr;   // {vulkan,dx12,dx11,opengl,dx11Compute}
    if (gpuRow > 0 && (size_t)(gpuRow - 1) < m_gpuIndices.size())
    {
        gpuArg = m_gpuIndices[gpuRow - 1];
        if ((size_t)(gpuRow - 1) < m_gpuApiSupport.size()) sup = &m_gpuApiSupport[gpuRow - 1];
    }

    // One GPU, every graphics API the GPU supports. Windows OpenGL cannot pick
    // an adapter, so it only runs when the selected row is the default GPU.
    auto multiApi = [&](std::vector<std::string> extraArgs) {
        const char* apis[4] = { "vulkan", "dx12", "dx11", "opengl" };
        const auto workload = selected(WorkloadBox());
        const bool needsDx11Compute = workload != "gpu_burn" &&
            workload != "gpu_stress" && workload != "stress" &&
            workload != "volumetric";
        std::vector<std::vector<std::string>> jobs;
        for (int k = 0; k < 4; ++k)
        {
            if (sup && !(*sup)[k]) continue;            // GPU doesn't support this API
            if (sup && k == 2 && needsDx11Compute && !(*sup)[4]) continue;
            if (k == 3 && gpuArg > 0) continue;         // OpenGL: Windows uses default GPU only
            std::vector<std::string> a = { m_enginePath };
            for (auto& d : dur) a.push_back(d);
            a.push_back("--backend"); a.push_back(apis[k]);
            if (gpuArg >= 0) { a.push_back("--gpu"); a.push_back(std::to_string(gpuArg)); }
            for (auto& e : extraArgs) a.push_back(e);
            jobs.push_back(std::move(a));
        }
        return jobs;
    };

    switch (p)
    {
        case 0:  // [0] Quick run — best API / GPU, Medium, stream
        {
            std::vector<std::string> a = { m_enginePath };
            for (auto& d : dur) a.push_back(d);
            a.push_back("--capture"); a.push_back("5");
            return { a };
        }

        case 1:  // [1] Custom run — honour the visible controls
            return { buildArgs(false) };

        case 2:  // [5] Full analysis, one GPU (all APIs + RenderDoc + charts)
        {
            needCharts = true;
            auto ex = selectedWorkloadArgs();
            ex.push_back("--capture"); ex.push_back("5");
            return multiApi(std::move(ex));
        }

        case 3:  // [6] Selected workload, all GPUs x APIs (+ RenderDoc + charts)
        {
            needCharts = true;
            return { buildArgs(true) };
        }
        case 4:  // [7] Flights test, one GPU (all APIs, custom flights + RenderDoc)
        {
            std::vector<std::string> ex;
            if (!extra.empty()) { ex.push_back("--flights"); ex.push_back(extra); }
            ex.push_back("--capture"); ex.push_back("5");
            return multiApi(ex);
        }
        case 5:  // [8] Particle test, one GPU (all APIs, custom particles + RenderDoc)
        {
            std::vector<std::string> ex;
            if (!particles.empty()) { ex.push_back("--particles"); ex.push_back(particles); }
            ex.push_back("--capture"); ex.push_back("5");
            return multiApi(ex);
        }
        case 6:  // [9] Headless compute, one GPU (all APIs, pure compute)
            return multiApi({ "--headless" });
    }
    return { buildArgs(false) };
}

void MainWindow::launchJobs(std::vector<std::vector<std::string>> jobs, bool needCharts)
{
    RunButton().IsEnabled(false);
    Busy().IsActive(true);
    Status().Text(jobs.size() > 1
        ? locText("Running… (multiple passes; render windows may appear)", "运行中…（多趟；可能弹出渲染窗口）")
        : locText("Running… (a separate render window may appear)", "运行中…（可能弹出独立渲染窗口）"));
    ResultText().Text(L"—");
    OutputBox().Text({});

    std::string repo = m_enginePath.empty() ? std::string{}
        : std::filesystem::path(m_enginePath).parent_path().parent_path().parent_path().string();

    auto strong = get_strong();
    auto disp = m_dispatcher;
    std::thread([this, strong, disp, jobs, needCharts, repo]()
    {
        std::string all, lastScore;
        bool anyFailed = false;
        for (auto const& job : jobs)
        {
            auto res = captureCli(job);
            all += res.output; all += "\n";
            if (res.exitCode != 0) anyFailed = true;
            std::string sc = extractScore(res.output);
            if (!sc.empty()) lastScore = sc;
        }

        bool postProcessFailed = false;
        std::string postProcessStatus;
        auto recordPostProcessFailure = [&](std::string const& message)
        {
            postProcessFailed = true;
            if (!postProcessStatus.empty()) postProcessStatus += " ";
            postProcessStatus += message;
            all += "[Post-processing] " + message + "\n";
        };

        // Post-processing mirrors the CLI Full-Analysis / Flights / Particle
        // paths, but all optional-tool failures are reported truthfully.
        auto caps = parseCapturePaths(all);
        if (!repo.empty())
        {
            const auto repoPath = std::filesystem::path(repo);
            const auto repoW = repoPath.wstring();
            const std::string enginePath = jobs.empty() || jobs.front().empty()
                ? std::string{} : jobs.front().front();

            // Convert captures with bundled RenderDoc first. A conventional
            // developer-machine installation remains a compatibility fallback.
            if (!caps.empty())
            {
                const auto rdccmd = findRenderDocCommand(enginePath);
                if (rdccmd.empty())
                {
                    recordPostProcessFailure(
                        "RenderDoc capture conversion unavailable: renderdoccmd.exe was not found.");
                }
                else
                {
                    for (auto const& rdc : caps)
                    {
                        std::filesystem::path rdcP(rdc), jsonOut = rdcP;
                        jsonOut.replace_extension(L".json");
                        const int rc = runProcess(
                            L"\"" + rdccmd.wstring() + L"\" convert -f \""
                            + rdcP.wstring() + L"\" -c chrome.json -o \""
                            + jsonOut.wstring() + L"\"", repoW);
                        if (rc != 0)
                            recordPostProcessFailure(
                                "RenderDoc capture conversion failed (exit "
                                + std::to_string(rc) + ").");
                    }
                }
            }
            else if (needCharts)
            {
                recordPostProcessFailure(
                    "No RenderDoc capture was produced for Full Analysis.");
            }

            if (needCharts)
            {
                const auto python = findPythonExecutable();
                const auto scripts = repoPath / L"scripts";
                const std::filesystem::path requiredScripts[] = {
                    scripts / L"rdoc_analyse.py",
                    scripts / L"plot_results.py",
                    scripts / L"export_report.py",
                    scripts / L"plot_workloads.py",
                };
                bool scriptsAvailable = true;
                std::error_code ec;
                for (auto const& script : requiredScripts)
                {
                    if (!std::filesystem::is_regular_file(script, ec) || ec)
                    {
                        scriptsAvailable = false;
                        break;
                    }
                }

                if (python.empty() || !scriptsAvailable)
                {
                    recordPostProcessFailure(
                        "Benchmark completed, but automatic reports are unavailable; "
                        "this package does not yet include the frozen report worker.");
                }
                else
                {
                    const auto resultsPath =
                        std::filesystem::path(gpu_bench::ResultsFilePath());
                    const auto dataRoot = resultsPath.parent_path().parent_path();
                    const auto reportsDir = dataRoot / L"reports";
                    const auto imagesDir = reportsDir / L"images";
                    std::filesystem::create_directories(imagesDir, ec);
                    if (ec)
                    {
                        recordPostProcessFailure(
                            "Could not create the per-user reports directory.");
                    }
                    else
                    {
                        const auto py = L"\"" + python.wstring() + L"\" ";
                        auto runReport = [&](std::wstring const& command,
                                             char const* label)
                        {
                            const int rc = runProcess(command, repoW);
                            if (rc != 0)
                                recordPostProcessFailure(
                                    std::string(label) + " failed (exit "
                                    + std::to_string(rc) + ").");
                        };

                        if (!caps.empty())
                        {
                            const auto captureDir =
                                std::filesystem::path(caps.front()).parent_path();
                            runReport(
                                py + L"\"scripts\\rdoc_analyse.py\" --captures \""
                                + captureDir.wstring() + L"\" --results \""
                                + resultsPath.wstring() + L"\" --output \""
                                + (reportsDir / L"rdoc_comparison.md").wstring()
                                + L"\"", "RenderDoc timing analysis");
                        }
                        runReport(
                            py + L"\"scripts\\plot_results.py\" --json \""
                            + resultsPath.wstring() + L"\" --save \""
                            + imagesDir.wstring() + L"\"", "Result chart generation");
                        runReport(
                            py + L"\"scripts\\export_report.py\" --json \""
                            + resultsPath.wstring() + L"\" --md \""
                            + (reportsDir / L"results-table.md").wstring()
                            + L"\"", "Markdown report generation");
                        runReport(
                            py + L"\"scripts\\export_report.py\" --json \""
                            + resultsPath.wstring() + L"\" --html \""
                            + (reportsDir / L"report.html").wstring()
                            + L"\"", "HTML report generation");
                        runReport(
                            py + L"\"scripts\\plot_workloads.py\" --input \""
                            + resultsPath.wstring() + L"\" --out \""
                            + imagesDir.wstring() + L"\"", "Workload chart generation");
                    }
                }
            }
        }

        disp.TryEnqueue([this, strong, all, lastScore, needCharts, anyFailed,
                         postProcessFailed, postProcessStatus]()
        {
            OutputBox().Text(u8(all));
            if (anyFailed)
            {
                ResultText().Text(locText("Error — see output.", "出错 —— 见输出。"));
                Status().Text(locText("Failed.", "运行失败。"));
            }
            else
            {
                ResultText().Text(lastScore.empty()
                    ? locText("Done — see output / History.", "完成 —— 见输出/历史。")
                    : u8(lastScore));
                if (postProcessFailed)
                    Status().Text(u8("Benchmark done; " + postProcessStatus));
                else
                    Status().Text(needCharts ? locText("Done (charts & report regenerated).", "完成（已重新生成图表与报告）。")
                                             : locText("Done.", "完成。"));
            }
            RunButton().IsEnabled(true);
            Busy().IsActive(false);
            refreshHistory();
        });
    }).detach();
}

void MainWindow::OnRun(IInspectable const&, RoutedEventArgs const&)
{
    if (m_enginePath.empty())
    {
        Status().Text(locText("Engine exe not found (build the CMake project first).",
                              "未找到引擎（请先用 CMake 构建）。"));
        return;
    }
    const int preset = PresetBox().SelectedIndex();
    const bool workloadSelectable = preset == 1 || preset == 2 || preset == 3;
    const auto workload = selected(WorkloadBox());
    const auto burnIterText = to_string(ExtraBox().Text());
    if (workloadSelectable && workload == "gpu_burn" && !burnIterText.empty())
    {
        bool valid = false;
        try {
            size_t used = 0;
            const int value = std::stoi(burnIterText, &used);
            valid = used == burnIterText.size() && value >= 16 && value <= 32;
        } catch (...) {}
        if (!valid)
        {
            ResultText().Text(locText(
                "GPU Burn fixed steps must be an integer from 16 to 32. Leave it empty for safe per-device auto-tuning.",
                "GPU Burn 固定步数必须是 16 到 32 的整数；留空会按设备安全自动标定。"));
            Status().Text(locText("GPU Burn settings need attention.",
                                  "请检查 GPU Burn 设置。"));
            return;
        }
    }
    if (workloadSelectable && workload == "gpu_burn" &&
        DurationUnitBox().SelectedIndex() > 0 &&
        burnIterText.empty())
    {
        ResultText().Text(locText(
            "Auto-tuned GPU Burn requires a timed run. Select Seconds, or enter a fixed step count for Frames.",
            "自动标定的 GPU Burn 需要按时间运行。请选择“秒”，或为按帧运行填写固定步数。"));
        Status().Text(locText("GPU Burn settings need attention.",
                              "请检查 GPU Burn 设置。"));
        return;
    }
    const bool vulkanOnlyFluid = workload == "fluid" || workload == "cinematic_liquid"
        || workload == "cinematic_liquid_v1";
    if ((preset == 2 || preset == 3) && vulkanOnlyFluid)
    {
        ResultText().Text(locText(
            "This fluid workload is Vulkan-only in v1 and cannot run as cross-API Full Analysis.",
            "这个流体测试在 v1 中仅支持 Vulkan，不能用于跨 API 完整分析。"));
        Status().Text(locText("Vulkan-only workload blocked for Full Analysis.",
                              "完整分析已阻止仅支持 Vulkan 的项目。"));
        return;
    }
    if (preset == 1 && vulkanOnlyFluid)
    {
        if (backendValue() != "vulkan")
        {
            ResultText().Text(locText(
                "This fluid workload is Vulkan-only. Select Vulkan explicitly to run it.",
                "这个流体测试仅支持 Vulkan。请显式选择 Vulkan 后再运行。"));
            Status().Text(locText("Vulkan-only workload blocked on this API.",
                                  "当前 API 已阻止仅支持 Vulkan 的项目。"));
            return;
        }
        int gpuRow = GpuBox().SelectedIndex();
        if (gpuRow > 0 && static_cast<size_t>(gpuRow - 1) < m_gpuApiSupport.size()
            && !m_gpuApiSupport[gpuRow - 1][0])
        {
            ResultText().Text(locText("The selected GPU does not report Vulkan support.",
                                      "所选 GPU 未报告 Vulkan 支持。"));
            Status().Text(locText("Vulkan-only workload blocked on this GPU.",
                                  "所选 GPU 不支持这个 Vulkan 项目。"));
            return;
        }
    }
    const bool needsDx11Compute = workload != "gpu_burn" &&
        workload != "gpu_stress" && workload != "stress" &&
        workload != "volumetric";
    if (preset == 1 && backendValue() == "dx11" && needsDx11Compute)
    {
        const int gpuRow = GpuBox().SelectedIndex();
        if (gpuRow > 0 && static_cast<size_t>(gpuRow - 1) < m_gpuApiSupport.size() &&
            !m_gpuApiSupport[gpuRow - 1][4])
        {
            ResultText().Text(locText(
                "The selected DX11 adapter supports raster workloads but does not expose DirectCompute 4.x/5.0.",
                "所选 DX11 适配器支持光栅负载，但没有提供 DirectCompute 4.x/5.0。"));
            Status().Text(locText("This workload needs DX11 compute support.",
                                  "这个负载需要 DX11 计算支持。"));
            return;
        }
    }
    bool needCharts = false;
    auto jobs = buildPresetJobs(needCharts);
    launchJobs(std::move(jobs), needCharts);
}

void MainWindow::OnPresetChanged(IInspectable const&, SelectionChangedEventArgs const&)
{
    if (!m_uiReady) return;
    updateExtraLabel();
}

// ---- history (via gpu_bench::LoadResults, in-process) ----------------------
void MainWindow::refreshHistory()
{
    m_results = gpu_bench::LoadResults();
    rebuildGpuFilter();
    rebuildHistoryFilters();
    m_historyFiltersInitialized = true;
    applyHistoryView();
}

void MainWindow::rebuildHistoryFilters()
{
    {
        ApiFilterLabel().Text(locText("Graphics API", "图形 API"));
        WorkloadFilterLabel().Text(locText("Workload", "测试项目"));
        ParticleFilterLabel().Text(locText("Particles", "粒子数"));
        GpuFilterLabel().Text(locText("GPUs", "显卡"));
        SelectAllApis().Content(locContent("All", "全选"));
        ClearAllApis().Content(locContent("None", "清空"));
        SelectAllWorkloads().Content(locContent("All", "全选"));
        ClearAllWorkloads().Content(locContent("None", "清空"));
        SelectAllParticles().Content(locContent("All", "全选"));
        ClearAllParticles().Content(locContent("None", "清空"));
        SelectAllGpus().Content(locContent("All", "全选"));
        ClearAllGpus().Content(locContent("None", "清空"));

        auto prevApis = checkedTags(ApiFilterPanel());
        auto prevWorkloads = checkedTags(WorkloadFilterPanel());
        auto prevParticles = checkedTags(ParticleFilterPanel());
        ApiFilterPanel().Children().Clear();
        WorkloadFilterPanel().Children().Clear();
        ParticleFilterPanel().Children().Clear();

        std::map<std::string, int> apiCounts;
        std::map<std::string, int> workloadCounts;
        std::map<std::uint32_t, int> particleCounts;
        for (auto const& r : m_results)
        {
            if (!r.graphicsApi.empty()) ++apiCounts[r.graphicsApi];
            auto historyKey = historyWorkloadKey(r);
            if (!historyKey.empty()) ++workloadCounts[historyKey];
            if (r.particleCount > 0) ++particleCounts[r.particleCount];
        }

        auto addStringFilters = [&](StackPanel const& panel,
                                    std::map<std::string, int> const& counts,
                                    std::set<std::string> const& previous,
                                    std::vector<std::string> const& preferred,
                                    auto labelOf)
        {
            std::string defaultKey = mostFrequentKey(counts);
            std::set<std::string> emitted;
            auto emit = [&](std::string const& key)
            {
                auto it = counts.find(key);
                if (it == counts.end()) return;
                bool checked = m_historyFiltersInitialized
                    ? previous.find(key) != previous.end()
                    : key == defaultKey;
                appendFilterCheckBox(panel, labelOf(key), key, checked);
                emitted.insert(key);
            };
            for (auto const& key : preferred) emit(key);
            for (auto const& [key, count] : counts)
                if (emitted.find(key) == emitted.end()) emit(key);
        };

        addStringFilters(ApiFilterPanel(), apiCounts, prevApis,
                         { "Vulkan", "DX12", "DX11", "OpenGL" },
                         [](std::string const& key) { return apiLabel(key); });
        addStringFilters(WorkloadFilterPanel(), workloadCounts, prevWorkloads,
                           { "stream", "gpu_burn", "cinematic_liquid", "gpu_stress", "nbody", "synthpeak", "stress", "render3d", "volumetric", "fluid", "cinematic_liquid_v1" },
                          [](std::string const& key) { return workloadLabel(key); });

        std::string defaultParticle;
        int bestParticleCount = -1;
        for (auto const& [n, count] : particleCounts)
            if (count > bestParticleCount)
            {
                defaultParticle = std::to_string(n);
                bestParticleCount = count;
            }
        for (auto const& [n, count] : particleCounts)
        {
            std::string key = std::to_string(n);
            bool checked = m_historyFiltersInitialized
                ? prevParticles.find(key) != prevParticles.end()
                : key == defaultParticle;
            appendFilterCheckBox(ParticleFilterPanel(), particleLabel(n), key, checked);
        }
        return;
    }

    std::string currentWorkload = selected(WorkloadFilterBox());
    std::string currentParticles = selected(ParticleFilterBox());

    WorkloadFilterBox().Items().Clear();
    WorkloadFilterAll().Content(locContent("All workloads", "全部项目"));
    WorkloadFilterAll().Tag(box_value(L"*"));
    WorkloadFilterBox().Items().Append(WorkloadFilterAll());

    ParticleFilterBox().Items().Clear();
    ParticleFilterAll().Content(locContent("All particle counts", "全部粒子数"));
    ParticleFilterAll().Tag(box_value(L"0"));
    ParticleFilterBox().Items().Append(ParticleFilterAll());

    std::set<std::string> workloads;
    std::set<std::uint32_t> particles;
    for (auto const& r : m_results)
    {
        auto historyKey = historyWorkloadKey(r);
        if (!historyKey.empty()) workloads.insert(historyKey);
        if (r.particleCount > 0) particles.insert(r.particleCount);
    }

    auto addWorkload = [&](std::string const& id, std::string const& label) {
        if (workloads.find(id) == workloads.end()) return;
        ComboBoxItem item;
        item.Tag(box_value(u8(id)));
        item.Content(box_value(u8(label)));
        WorkloadFilterBox().Items().Append(item);
    };
    addWorkload("stream", workloadLabel("stream"));
    addWorkload("gpu_burn", workloadLabel("gpu_burn"));
    addWorkload("cinematic_liquid", workloadLabel("cinematic_liquid"));
    addWorkload("cinematic_liquid_v1", workloadLabel("cinematic_liquid_v1"));
    addWorkload("gpu_stress", workloadLabel("gpu_stress"));
    addWorkload("nbody", workloadLabel("nbody"));
    addWorkload("synthpeak", workloadLabel("synthpeak"));
    addWorkload("stress", workloadLabel("stress"));
    addWorkload("render3d", workloadLabel("render3d"));
    addWorkload("volumetric", workloadLabel("volumetric"));
    addWorkload("fluid", workloadLabel("fluid"));
    for (auto const& w : workloads)
        if (w != "stream" && w != "gpu_burn" && w != "cinematic_liquid" && w != "cinematic_liquid_v1" && w != "gpu_stress" && w != "nbody" && w != "stress" && w != "synthpeak"
            && w != "render3d" && w != "volumetric" && w != "fluid")
        {
            ComboBoxItem item;
            item.Tag(box_value(u8(w)));
            item.Content(box_value(u8(w)));
            WorkloadFilterBox().Items().Append(item);
        }

    for (auto n : particles)
    {
        ComboBoxItem item;
        item.Tag(box_value(u8(std::to_string(n))));
        item.Content(box_value(u8(particleLabel(n))));
        ParticleFilterBox().Items().Append(item);
    }

    auto restore = [](ComboBox const& box, std::string const& wanted) {
        int fallback = 0;
        for (uint32_t i = 0; i < box.Items().Size(); ++i)
            if (auto item = box.Items().GetAt(i).try_as<ComboBoxItem>())
                if (to_string(unbox_value_or<hstring>(item.Tag(), L"")) == wanted)
                {
                    box.SelectedIndex(static_cast<int32_t>(i));
                    return;
                }
        box.SelectedIndex(fallback);
    };
    restore(WorkloadFilterBox(), currentWorkload.empty() ? "*" : currentWorkload);
    restore(ParticleFilterBox(), currentParticles.empty() ? "0" : currentParticles);
}

void MainWindow::rebuildGpuFilter()
{
    {
        using winrt::Windows::UI::Text::FontWeights;
        auto previous = checkedTags(GpuFilterPanel());
        GpuFilterPanel().Children().Clear();

        std::map<std::string, std::map<std::string, std::set<std::string>>> groups;
        std::map<std::string, int> counts;
        for (auto const& r : m_results)
        {
            GpuLeaf l = leafOf(r);
            std::string key = filterKey(l);
            groups[l.brand][l.series].insert(l.device);
            ++counts[key];
        }
        std::string defaultKey = mostFrequentKey(counts);

        for (auto const& [brand, seriesMap] : groups)
        {
            TextBlock bt; bt.Text(u8(brand));
            bt.FontWeight(FontWeights::SemiBold());
            bt.Margin(Thickness{ 0, 8, 0, 2 });
            GpuFilterPanel().Children().Append(bt);

            for (auto const& [series, devs] : seriesMap)
            {
                TextBlock st; st.Text(u8(series));
                st.Opacity(0.7); st.FontSize(12);
                st.Margin(Thickness{ 12, 2, 0, 0 });
                GpuFilterPanel().Children().Append(st);

                for (auto const& d : devs)
                {
                    std::string key = filterKey({ brand, series, d });
                    bool checked = m_historyFiltersInitialized
                        ? previous.find(key) != previous.end()
                        : key == defaultKey;
                    appendFilterCheckBox(GpuFilterPanel(), d, key, checked, Thickness{ 24, 0, 0, 0 });
                }
            }
        }
        return;
    }

    using winrt::Windows::UI::Text::FontWeights;
    GpuFilterPanel().Children().Clear();

    // brand -> series -> sorted distinct devices (software grouped by CPU model)
    std::map<std::string, std::map<std::string, std::set<std::string>>> groups;
    for (auto& r : m_results)
    {
        GpuLeaf l = leafOf(r);
        groups[l.brand][l.series].insert(l.device);
    }

    for (auto const& [brand, seriesMap] : groups)
    {
        TextBlock bt; bt.Text(u8(brand));
        bt.FontWeight(FontWeights::SemiBold());
        bt.Margin(Thickness{ 0, 8, 0, 2 });
        GpuFilterPanel().Children().Append(bt);

        for (auto const& [series, devs] : seriesMap)
        {
            TextBlock st; st.Text(u8(series));
            st.Opacity(0.7); st.FontSize(12);
            st.Margin(Thickness{ 12, 2, 0, 0 });
            GpuFilterPanel().Children().Append(st);

            for (auto const& d : devs)
            {
                CheckBox cb;
                cb.Content(box_value(u8(d)));
                cb.Tag(box_value(u8(filterKey({ brand, series, d }))));   // unique leaf key
                cb.IsChecked(true);
                cb.Margin(Thickness{ 24, 0, 0, 0 });
                GpuFilterPanel().Children().Append(cb);
            }
        }
    }
}

void MainWindow::applyHistoryView()
{
    {
        auto collect = [&](StackPanel const& panel,
                           DropDownButton const& button,
                           hstring const& allLabel,
                           hstring const& noneLabel)
        {
            std::set<std::string> allowed;
            int total = 0, checked = 0;
            hstring single;
            for (auto const& c : panel.Children())
                if (auto cb = c.try_as<CheckBox>(); cb && cb.Tag())
                {
                    ++total;
                    if (cb.IsChecked() && cb.IsChecked().Value())
                    {
                        ++checked;
                        allowed.insert(to_string(unbox_value_or<hstring>(cb.Tag(), L"")));
                        single = unbox_value_or<hstring>(cb.Content(), L"");
                    }
                }

            hstring label = (!total || checked == total) ? allLabel
                           : checked == 0               ? noneLabel
                           : checked == 1               ? single
                                                         : u8(std::to_string(checked) + " / " + std::to_string(total));
            button.Content(box_value(label));
            return allowed;
        };

        auto allowedApis = collect(ApiFilterPanel(), ApiFilterButton(),
                                   locText("All APIs", "全部 API"), locText("None", "无"));
        auto allowedWorkloads = collect(WorkloadFilterPanel(), WorkloadFilterButton(),
                                        locText("All workloads", "全部项目"), locText("None", "无"));
        auto allowedParticles = collect(ParticleFilterPanel(), ParticleFilterButton(),
                                        locText("All particle counts", "全部粒子数"), locText("None", "无"));
        auto allowedGpus = collect(GpuFilterPanel(), GpuFilterButton(),
                                   locText("All GPUs", "全部 GPU"), locText("None", "无"));

        bool hasApiFilter = ApiFilterPanel().Children().Size() > 0;
        bool hasWorkloadFilter = WorkloadFilterPanel().Children().Size() > 0;
        bool hasParticleFilter = ParticleFilterPanel().Children().Size() > 0;
        bool hasGpuFilter = GpuFilterPanel().Children().Size() > 0;

        int rangeIdx = TimeRangeBox().SelectedIndex();
        std::string lo, hi;
        if (rangeIdx == 4) { lo = pickerDate(FromDate()); hi = pickerDate(ToDate()); }
        else               { lo = cutoffFor(rangeIdx); }

        std::vector<const gpu_bench::BenchmarkResult*> view;
        for (auto& r : m_results)
        {
            if (hasApiFilter && allowedApis.find(r.graphicsApi) == allowedApis.end()) continue;
            if (hasWorkloadFilter && allowedWorkloads.find(historyWorkloadKey(r)) == allowedWorkloads.end()) continue;
            if (hasParticleFilter && allowedParticles.find(std::to_string(r.particleCount)) == allowedParticles.end()) continue;
            if (hasGpuFilter && allowedGpus.find(filterKey(leafOf(r))) == allowedGpus.end()) continue;
            std::string date = r.timestamp.substr(0, 10);
            if (!lo.empty() && date < lo) continue;
            if (!hi.empty() && date > hi) continue;
            view.push_back(&r);
        }

        auto cmpString = [](std::string const& a, std::string const& b)
        {
            if (a < b) return -1;
            if (a > b) return 1;
            return 0;
        };
        std::sort(view.begin(), view.end(),
            [&](const gpu_bench::BenchmarkResult* a, const gpu_bench::BenchmarkResult* b)
            {
                int c = 0;
                auto const& col = m_historySortColumn;
                if (col == "api")            c = cmpString(a->graphicsApi, b->graphicsApi);
                else if (col == "device")    c = cmpString(a->deviceName, b->deviceName);
                else if (col == "cpu")       c = cmpString(a->cpuName, b->cpuName);
                else if (col == "mem")       c = (a->vramMB < b->vramMB) ? -1 : (a->vramMB > b->vramMB) ? 1 : 0;
                else if (col == "workload")  c = cmpString(a->workload, b->workload);
                else if (col == "particles") c = (a->particleCount < b->particleCount) ? -1 : (a->particleCount > b->particleCount) ? 1 : 0;
                else if (col == "score")     c = (a->score < b->score) ? -1 : (a->score > b->score) ? 1 : 0;
                else if (col == "fps")       c = (a->avgFps < b->avgFps) ? -1 : (a->avgFps > b->avgFps) ? 1 : 0;
                else                          c = cmpString(a->timestamp, b->timestamp);
                if (c == 0) c = cmpString(a->timestamp, b->timestamp);
                return m_historySortAscending ? c < 0 : c > 0;
            });

        HistoryList().Items().Clear();
        HistoryHeaderPanel().Children().Clear();
        m_displayedIds.clear();

        struct Row { std::string time, api, dev, cpu, mem, wl, particles, score, fps; };
        std::vector<Row> rows; rows.reserve(view.size());
        for (auto* r : view)
        {
            Row x;
            x.time = localizeTimestamp(r->timestamp);
            x.api  = r->graphicsApi;
            x.dev  = normalizeGpuName(r->deviceName);
            x.cpu  = normalizeCpuName(r->cpuName);
            x.mem  = r->vramMB >= 1024 ? std::to_string(r->vramMB / 1024) + "GB"
                   : r->vramMB > 0     ? std::to_string(r->vramMB) + "MB" : "-";
            x.wl   = workloadRunLabel(*r);
            x.particles = particleLabel(r->particleCount);
            x.score = r->workload == "fluid"
                ? i18n::tr("Unverified legacy", "未验证旧版")
                : r->score > 0.0
                ? [&] { std::ostringstream o; o.setf(std::ios::fixed); o.precision(1);
                        o << r->score << ' ' << r->scoreUnit;
                        if (!r->precision.empty()) o << " (" << r->precision << ')';
                        return o.str(); }()
                : "-";
            x.fps  = std::to_string((int)r->avgFps);
            rows.push_back(std::move(x));
        }

        size_t wTime = 4, wApi = 3, wDev = 6, wCpu = 3, wMem = 3, wWl = 8, wParticles = 9, wScore = 5;
        for (auto& x : rows)
        {
            wTime  = (std::max)(wTime,  x.time.size());
            wApi   = (std::max)(wApi,   x.api.size());
            wDev   = (std::max)(wDev,   x.dev.size());
            wCpu   = (std::max)(wCpu,   x.cpu.size());
            wMem   = (std::max)(wMem,   x.mem.size());
            wWl    = (std::max)(wWl,    x.wl.size());
            wParticles = (std::max)(wParticles, x.particles.size());
            wScore = (std::max)(wScore, x.score.size());
        }
        auto pad = [](std::string s, size_t w) { if (s.size() < w) s.append(w - s.size(), ' '); return s; };
        const std::string gp = "  ";

        // U+200C (zero-width non-joiner) sentinel prevents WinUI TextBlock
        // from trimming the trailing spaces that align columns.
        const std::string sentinel = "\xE2\x80\x8C";  // UTF-8 for U+200C
        auto addHeader = [&](std::string label, size_t width, char const* column)
        {
            if (m_historySortColumn == column)
                label += m_historySortAscending ? " ^" : " v";
            width = (std::max)(width, label.size());
            TextBlock tb;
            tb.Text(u8(pad(label, width) + gp + sentinel));
            tb.Tag(box_value(u8(column)));
            tb.FontFamily(Media::FontFamily(L"Consolas"));
            tb.Opacity(0.78);
            tb.Tapped([this](IInspectable const& sender, auto const&)
            {
                auto fe = sender.as<FrameworkElement>();
                std::string column = to_string(unbox_value_or<hstring>(fe.Tag(), L"time"));
                if (m_historySortColumn == column)
                {
                    m_historySortAscending = !m_historySortAscending;
                }
                else
                {
                    m_historySortColumn = column;
                    m_historySortAscending = !(column == "time" || column == "score" || column == "fps");
                }
                applyHistoryView();
            });
            HistoryHeaderPanel().Children().Append(tb);
        };
        addHeader("Time", wTime, "time");
        addHeader("API", wApi, "api");
        addHeader("GPU/Render", wDev, "device");
        addHeader("CPU", wCpu, "cpu");
        addHeader("Mem", wMem, "mem");
        addHeader("Workload", wWl, "workload");
        addHeader("Particles", wParticles, "particles");
        addHeader("Score", wScore, "score");
        addHeader("FPS", 3, "fps");

        for (size_t i = 0; i < rows.size(); ++i)
        {
            auto& x = rows[i];
            std::string line = pad(x.time, wTime) + gp + pad(x.api, wApi) + gp + pad(x.dev, wDev)
                             + gp + pad(x.cpu, wCpu) + gp + pad(x.mem, wMem) + gp + pad(x.wl, wWl)
                             + gp + pad(x.particles, wParticles)
                             + gp + pad(x.score, wScore) + gp + x.fps;
            TextBlock tb; tb.Text(u8(line));
            tb.FontFamily(Media::FontFamily(L"Consolas"));
            HistoryList().Items().Append(tb);
            m_displayedIds.push_back(view[i]->id);
        }
        return;
    }

    // GPU filter: which device checkboxes are ticked.
    std::set<std::string> allowed;
    int total = 0, checked = 0;
    for (auto const& c : GpuFilterPanel().Children())
        if (auto cb = c.try_as<CheckBox>(); cb && cb.Tag())
        {
            ++total;
            if (cb.IsChecked() && cb.IsChecked().Value())
            { allowed.insert(to_string(unbox_value_or<hstring>(cb.Tag(), L""))); ++checked; }
        }
    bool anyBox = total > 0;
    GpuFilterButton().Content(box_value(
        (!anyBox || checked == total) ? locText("All GPUs", "全部 GPU")
        : (checked == 0)              ? locText("None", "无")
                                      : u8(std::to_string(checked) + " / " + std::to_string(total))));

    // Time range (index 4 = custom date pickers).
    int rangeIdx = TimeRangeBox().SelectedIndex();
    std::string lo, hi;
    if (rangeIdx == 4) { lo = pickerDate(FromDate()); hi = pickerDate(ToDate()); }
    else               { lo = cutoffFor(rangeIdx); }

    std::string workloadFilter = selected(WorkloadFilterBox());
    if (workloadFilter.empty()) workloadFilter = "*";
    std::uint32_t particleFilter = 0;
    try { particleFilter = static_cast<std::uint32_t>(std::stoul(selected(ParticleFilterBox()))); }
    catch (...) { particleFilter = 0; }

    std::vector<const gpu_bench::BenchmarkResult*> view;
    for (auto& r : m_results)
    {
        if (anyBox && allowed.find(filterKey(leafOf(r))) == allowed.end()) continue;
        if (workloadFilter != "*" && historyWorkloadKey(r) != workloadFilter) continue;
        if (particleFilter != 0 && r.particleCount != particleFilter) continue;
        std::string date = r.timestamp.substr(0, 10);
        if (!lo.empty() && date < lo) continue;
        if (!hi.empty() && date > hi) continue;
        view.push_back(&r);
    }

    int s = SortBox().SelectedIndex();
    std::sort(view.begin(), view.end(),
        [s](const gpu_bench::BenchmarkResult* a, const gpu_bench::BenchmarkResult* b)
        {
            switch (s)
            {
                case 1:  return a->score > b->score;
                case 2:  return a->graphicsApi < b->graphicsApi;
                case 3:  return a->deviceName  < b->deviceName;
                case 4:  return a->workload    < b->workload;
                default: return a->timestamp   > b->timestamp;  // newest first
            }
        });

    HistoryList().Items().Clear();
    m_displayedIds.clear();

    // Flatten to strings first so columns can size to the widest value (no
    // truncation); the list scrolls horizontally if the total is wide.
    struct Row { std::string time, api, dev, cpu, mem, wl, particles, score, fps; };
    std::vector<Row> rows; rows.reserve(view.size());
    for (auto* r : view)
    {
        Row x;
        x.time = r->timestamp;
        x.api  = r->graphicsApi;
        x.dev  = r->deviceName;
        x.cpu  = r->cpuName;
        x.mem  = r->vramMB >= 1024 ? std::to_string(r->vramMB / 1024) + "GB"
               : r->vramMB > 0     ? std::to_string(r->vramMB) + "MB" : "-";
        x.wl   = workloadRunLabel(*r);
        x.particles = particleLabel(r->particleCount);
        x.score = r->workload == "fluid"
            ? i18n::tr("Unverified legacy", "未验证旧版")
            : r->score > 0.0
            ? [&] { std::ostringstream o; o.setf(std::ios::fixed); o.precision(1);
                    o << r->score << ' ' << r->scoreUnit;
                    if (!r->precision.empty()) o << " (" << r->precision << ')';
                    return o.str(); }()
            : "-";
        x.fps  = std::to_string((int)r->avgFps);
        rows.push_back(std::move(x));
    }

    size_t wTime = 4, wApi = 3, wDev = 6, wCpu = 3, wMem = 3, wWl = 8, wParticles = 9, wScore = 5;
    for (auto& x : rows)
    {
        wTime  = (std::max)(wTime,  x.time.size());
        wApi   = (std::max)(wApi,   x.api.size());
        wDev   = (std::max)(wDev,   x.dev.size());
        wCpu   = (std::max)(wCpu,   x.cpu.size());
        wMem   = (std::max)(wMem,   x.mem.size());
        wWl    = (std::max)(wWl,    x.wl.size());
        wParticles = (std::max)(wParticles, x.particles.size());
        wScore = (std::max)(wScore, x.score.size());
    }
    auto pad = [](std::string s, size_t w) { if (s.size() < w) s.append(w - s.size(), ' '); return s; };
    const std::string gp = "  ";

    HistoryHeader().Text(u8(pad("Time", wTime) + gp + pad("API", wApi) + gp + pad("Device", wDev)
                            + gp + pad("CPU", wCpu) + gp + pad("Mem", wMem) + gp + pad("Workload", wWl)
                            + gp + pad("Particles", wParticles)
                            + gp + pad("Score", wScore) + gp + "FPS"));
    for (size_t i = 0; i < rows.size(); ++i)
    {
        auto& x = rows[i];
        std::string line = pad(x.time, wTime) + gp + pad(x.api, wApi) + gp + pad(x.dev, wDev)
                         + gp + pad(x.cpu, wCpu) + gp + pad(x.mem, wMem) + gp + pad(x.wl, wWl)
                         + gp + pad(x.particles, wParticles)
                         + gp + pad(x.score, wScore) + gp + x.fps;
        TextBlock tb; tb.Text(u8(line));
        tb.FontFamily(Media::FontFamily(L"Consolas"));
        HistoryList().Items().Append(tb);
        m_displayedIds.push_back(view[i]->id);
    }
}

void MainWindow::OnRefreshHistory(IInspectable const&, RoutedEventArgs const&) { refreshHistory(); }

void MainWindow::OnHistoryViewChanged(IInspectable const&, SelectionChangedEventArgs const&)
{
    if (!m_uiReady) return;
    bool custom = TimeRangeBox().SelectedIndex() == 4;
    DateRangeRow().Visibility(custom ? Visibility::Visible : Visibility::Collapsed);
    applyHistoryView();
}

void MainWindow::OnDateRangeChanged(Controls::CalendarDatePicker const&,
                                    Controls::CalendarDatePickerDateChangedEventArgs const&)
{
    if (m_uiReady) applyHistoryView();
}

void MainWindow::OnDeleteSelected(IInspectable const&, RoutedEventArgs const&)
{
    auto sel = HistoryList().SelectedItems();
    if (sel.Size() == 0) return;
    auto items = HistoryList().Items();
    std::vector<std::string> ids;
    for (auto const& s : sel)
    {
        uint32_t idx = 0;
        if (items.IndexOf(s, idx) && idx < m_displayedIds.size())
            ids.push_back(m_displayedIds[idx]);
    }
    for (auto& id : ids) gpu_bench::DeleteResult(id);
    refreshHistory();
}

// ---- open user data folders. results.json and RenderDoc .rdc captures share
// the same PathService data root but live in separate subfolders, so History
// offers one button per folder rather than pretending they are one place.
// PathService throws when a directory cannot be created; a Click handler must
// not let that escape or the whole GUI dies. ----
static void openFolderInExplorer(std::filesystem::path (*resolve)())
{
    std::filesystem::path dir;
    try { dir = resolve(); }
    catch (std::exception const&) { return; }
    ShellExecuteW(nullptr, L"open", dir.wstring().c_str(),
                  nullptr, nullptr, SW_SHOWNORMAL);
}

void MainWindow::OnOpenResultsFolder(IInspectable const&, RoutedEventArgs const&)
{
    openFolderInExplorer(&gpu_bench::paths::ResultsDirectory);
}

void MainWindow::OnOpenCapturesFolder(IInspectable const&, RoutedEventArgs const&)
{
    openFolderInExplorer(&gpu_bench::paths::CapturesDirectory);
}

// ---- charts (run plot_workloads.py, then show the PNGs) --------------------
void MainWindow::OnGenerateCharts(IInspectable const&, RoutedEventArgs const&)
{
    if (m_enginePath.empty()) { ChartsStatus().Text(locText("Engine/repo not found.", "未找到引擎/仓库。")); return; }
    auto repo = std::filesystem::path(m_enginePath).parent_path().parent_path().parent_path();
    if (!std::filesystem::exists(repo / "scripts" / "plot_workloads.py"))
    { ChartsStatus().Text(locText("scripts/plot_workloads.py not found.", "未找到 scripts/plot_workloads.py。")); return; }
    const auto python = findPythonExecutable();
    if (python.empty())
    {
        ChartsStatus().Text(locText(
            "Charts unavailable: this package does not yet include the frozen report worker.",
            "图表不可用：当前安装包尚未包含冻结的报告工具。"));
        return;
    }

    const auto resultsPath = std::filesystem::path(gpu_bench::ResultsFilePath());
    const auto imageDir = resultsPath.parent_path().parent_path() / L"reports" / L"images";
    std::error_code ec;
    std::filesystem::create_directories(imageDir, ec);
    if (ec)
    {
        ChartsStatus().Text(locText("Could not create the reports directory.",
                                    "无法创建报告目录。"));
        return;
    }

    GenChartsButton().IsEnabled(false); ChartsBusy().IsActive(true);
    ChartsStatus().Text(locText("Running plot_workloads.py…", "正在运行 plot_workloads.py…"));

    auto strong = get_strong();
    auto disp = m_dispatcher;
    std::wstring repoW = repo.wstring();
    const auto pythonW = python.wstring();
    const auto resultsW = resultsPath.wstring();
    const auto imageDirW = imageDir.wstring();
    std::thread([this, strong, disp, repoW, pythonW, resultsW, imageDirW]()
    {
        int rc = runProcess(
            L"\"" + pythonW + L"\" \"scripts\\plot_workloads.py\" --input \""
            + resultsW + L"\" --out \"" + imageDirW + L"\"", repoW);
        disp.TryEnqueue([this, strong, imageDirW, rc]()
        {
            ChartsPanel().Children().Clear();
            std::filesystem::path imgDir(imageDirW);
            const char* names[] = { "workload_stream", "workload_gpu_burn", "workload_gpu_stress",
                                    "workload_nbody", "workload_stress",
                                    "workload_render3d", "workload_volumetric",
                                    "workload_cinematic_liquid", "workload_synthpeak" };
            int shown = 0;
            for (auto n : names)
            {
                auto p = imgDir / (std::string(n) + ".png");
                if (!std::filesystem::exists(p)) continue;
                Media::Imaging::BitmapImage bmp;
                bmp.CreateOptions(Media::Imaging::BitmapCreateOptions::IgnoreImageCache);
                bmp.UriSource(fileUri(p));
                Image img; img.Source(bmp); img.Stretch(Stretch::Uniform);
                img.MaxWidth(900); img.HorizontalAlignment(HorizontalAlignment::Left);
                ChartsPanel().Children().Append(img);
                ++shown;
            }
            ChartsStatus().Text(rc == 0 ? u8("Done — " + std::to_string(shown) + " chart(s).")
                                        : u8("python exited with " + std::to_string(rc) + "."));
            GenChartsButton().IsEnabled(true); ChartsBusy().IsActive(false);
        });
    }).detach();
}

}
