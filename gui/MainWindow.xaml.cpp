#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.xaml.g.hpp"

#include "gpu_engine.h"          // gpu_bench::cliMain
#include "benchmark_results.h"   // gpu_bench::LoadResults
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
#include <ctime>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
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
        std::filesystem::path dir = std::filesystem::path(buf).parent_path();
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
        return {};
    }

    // Run gpu_bench::cliMain in-process with synthesized argv, capturing stdout.
    std::string captureCli(std::vector<std::string> args)
    {
        std::vector<char*> argv;
        argv.reserve(args.size());
        for (auto& a : args) argv.push_back(a.data());
        std::ostringstream cap;
        std::streambuf* old = std::cout.rdbuf(cap.rdbuf());
        try { gpu_bench::cliMain((int)argv.size(), argv.data()); } catch (...) {}
        std::cout.rdbuf(old);
        return cap.str();
    }

    std::string extractScore(std::string const& out)
    {
        const char* keys[] = { "Compute rate:", "Fill rate:", "Render rate:", "Peak FP", "Peak INT" };
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
        if (id == "stream")    return i18n::tr("stream - memory bandwidth", "stream - 显存带宽");
        if (id == "nbody")     return i18n::tr("nbody - FP32 compute", "nbody - FP32 计算");
        if (id == "stress")    return i18n::tr("stress - fragment fill", "stress - 片元填充");
        if (id == "synthpeak") return i18n::tr("synthpeak - peak ALU", "synthpeak - 峰值 ALU");
        if (id == "render3d")  return i18n::tr("render3d - 3D scene rendering", "render3d - 3D 场景渲染");
        return id;
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

    // Make benchmark output (results.json) land in the repo root, where the
    // History tab and plot_workloads.py also look.
    if (!m_enginePath.empty())
    {
        auto repo = std::filesystem::path(m_enginePath).parent_path().parent_path().parent_path();
        std::error_code ec; std::filesystem::current_path(repo, ec);
    }

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
            appWindow.Resize({ static_cast<int32_t>(1320 * scale), static_cast<int32_t>(820 * scale) });
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
        default: active = AboutPage();    break;
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
                                      "自定义运行（选择 API / GPU / 负载）"));
    PresetFullOne().Content(locContent("Full analysis — one GPU (all APIs + RenderDoc + charts)",
                                       "完整分析 —— 单 GPU（全部 API + RenderDoc + 图表）"));
    PresetFullAll().Content(locContent("Full analysis — all GPUs × APIs (+ RenderDoc + charts)",
                                       "完整分析 —— 全部 GPU × API（+ RenderDoc + 图表）"));
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
    WorkloadBox().Header(locContent("Workload", "负载"));
    WorkloadStream().Content(locContent("stream — bandwidth (GB/s)",
                                        "stream —— 显存带宽 (GB/s)"));
    WorkloadNBody().Content(locContent("nbody — FP32 compute (GFLOP/s)",
                                       "nbody —— FP32 计算 (GFLOP/s)"));
    WorkloadStress().Content(locContent("stress — fragment fill (G-iter/s)",
                                        "stress —— 片元填充 (G-iter/s)"));
    WorkloadSynthPeak().Content(locContent("synthpeak — peak ALU (GFLOPS/GIOPS)",
                                           "synthpeak —— 峰值 ALU (GFLOPS/GIOPS)"));
    WorkloadRender3D().Content(locContent("render3d — 3D rendering test",
                                          "render3d —— 3D 渲染测试"));
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
    SortBox().Header(locContent("Sort by", "排序"));
    SortTime().Content(locContent("Time (newest)", "时间（最新）"));
    SortScore().Content(locContent("Score (high→low)", "分数（高→低）"));
    SortApi().Content(locContent("Graphics API", "图形 API"));
    SortDevice().Content(locContent("GPU / Renderer", "GPU / 渲染器"));
    SortWorkload().Content(locContent("Workload", "负载"));
    WorkloadFilterBox().Header(locContent("Workload", "负载"));
    WorkloadFilterAll().Content(locContent("All workloads", "全部负载"));
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
    m_suppressCombo = false;

    rebuildHistoryFilters();
    updateExtraLabel();
}

void MainWindow::updateExtraLabel()
{
    auto wl = selected(WorkloadBox());
    int preset = PresetBox().SelectedIndex();
    bool customRun = preset == 1;
    bool particleTest = preset == 5;
    bool flightsTest = preset == 4;
    bool particleWorkload = wl == "stream" || wl == "render3d";
    bool showParticles = particleTest || (customRun && particleWorkload);
    bool showExtra = flightsTest || (customRun && !particleWorkload);

    ParticlePresetBox().Visibility(showParticles ? Visibility::Visible : Visibility::Collapsed);
    CustomParticleBox().Visibility(showParticles && selected(ParticlePresetBox()) == "custom"
        ? Visibility::Visible : Visibility::Collapsed);
    ExtraBox().Visibility(showExtra ? Visibility::Visible : Visibility::Collapsed);

    if (flightsTest)            ExtraBox().Header(locContent("Flights (--flights)", "Flights (--flights)"));
    else if (wl == "nbody")     ExtraBox().Header(locContent("Bodies (--bodies)", "天体数 (--bodies)"));
    else if (wl == "stress")    ExtraBox().Header(locContent("Iterations (--iter)", "迭代次数 (--iter)"));
    else if (wl == "synthpeak") ExtraBox().Header(locContent("Iterations (--iter)", "迭代次数 (--iter)"));
    else                        ExtraBox().Header(locContent("Extra", "额外参数"));
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

    // One row of `--list-gpus`: GPU \t idx \t name \t vk \t dx12 \t dx11 \t opengl
    struct GpuRow { int idx; std::string name; std::array<bool, 4> api; };

    auto strong = get_strong();
    auto disp = m_dispatcher;
    std::string engine = m_enginePath;
    std::thread([this, strong, disp, engine]()
    {
        std::string out = captureCli({ engine, "--list-gpus" });
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
    PrecisionBox().IsEnabled(selected(WorkloadBox()) == "synthpeak");
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
    return selected(BackendBox());       // "" = Auto
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
                         : (wl == "stress" || wl == "synthpeak") ? "--iter" : "--particles";
        args.push_back(flag); args.push_back(extra);
    }

    if (HeadlessBox().IsChecked() && HeadlessBox().IsChecked().Value()) args.push_back("--headless");
    if (VsyncBox().IsChecked() && VsyncBox().IsChecked().Value())       args.push_back("--vsync");
    if (HostMemBox().IsChecked() && HostMemBox().IsChecked().Value())   args.push_back("--host-memory");
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

    int gpuRow = GpuBox().SelectedIndex();   // 0 = (auto)
    int gpuArg = -1;   // engine GPU index for one-GPU presets; -1 = auto
    const std::array<bool, 4>* sup = nullptr;   // {vulkan,dx12,dx11,opengl} support
    if (gpuRow > 0 && (size_t)(gpuRow - 1) < m_gpuIndices.size())
    {
        gpuArg = m_gpuIndices[gpuRow - 1];
        if ((size_t)(gpuRow - 1) < m_gpuApiSupport.size()) sup = &m_gpuApiSupport[gpuRow - 1];
    }

    // One GPU, every graphics API the GPU supports (mirrors CLI menu 5/7/8/9:
    // always the Stream workload; Windows OpenGL can't pick a GPU so it only runs
    // on the first GPU).
    auto multiApi = [&](std::vector<std::string> extraArgs) {
        const char* apis[4] = { "vulkan", "dx12", "dx11", "opengl" };
        std::vector<std::vector<std::string>> jobs;
        for (int k = 0; k < 4; ++k)
        {
            if (sup && !(*sup)[k]) continue;            // GPU doesn't support this API
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
            return { a };
        }

        case 1:  // [1] Custom run — honour the visible controls
            return { buildArgs(false) };

        case 2:  // [5] Full analysis, one GPU (all APIs + RenderDoc + charts)
            needCharts = true;
            return multiApi({ "--capture", "5" });

        case 3:  // [6] Full analysis, all GPUs x APIs (+ RenderDoc + charts) — Stream
        {
            needCharts = true;
            std::vector<std::string> a = { m_enginePath };
            for (auto& d : dur) a.push_back(d);
            a.push_back("--run-all"); a.push_back("--capture"); a.push_back("5");
            return { a };
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
        for (auto const& job : jobs)
        {
            std::string out = captureCli(job);
            all += out; all += "\n";
            std::string sc = extractScore(out);
            if (!sc.empty()) lastScore = sc;
        }

        // Post-processing mirrors the CLI Full-Analysis / Flights / Particle paths.
        if (!repo.empty())
        {
            std::wstring repoW = std::filesystem::path(repo).wstring();

            // 1. Convert any RenderDoc captures (.rdc -> chrome JSON), like menu 5/6/7/8.
            //    Only possible when launched under RenderDoc and renderdoccmd is present.
            auto caps = parseCapturePaths(all);
            std::filesystem::path rdccmd = L"C:\\Program Files\\RenderDoc\\renderdoccmd.exe";
            if (!caps.empty() && std::filesystem::exists(rdccmd))
                for (auto const& rdc : caps)
                {
                    std::filesystem::path rdcP(rdc), jsonOut = rdcP; jsonOut.replace_extension(L".json");
                    runProcess(L"\"" + rdccmd.wstring() + L"\" convert -f \"" + rdcP.wstring()
                               + L"\" -c chrome.json -o \"" + jsonOut.wstring() + L"\"", repoW);
                }

            if (needCharts)
            {
                // 2. RenderDoc timing analysis (only when captures exist) — menu 5/6.
                if (!caps.empty())
                {
                    std::wstring resultsW = std::filesystem::path(gpu_bench::ResultsFilePath()).wstring();
                    runProcess(L"python scripts\\rdoc_analyse.py --captures rdoc_captures "
                               L"--results \"" + resultsW + L"\" --output docs\\rdoc_comparison.md", repoW);
                }
                // 3. The CLI chart + report toolchain (menu 5/6).
                runProcess(L"python scripts\\plot_results.py --save docs\\images", repoW);
                runProcess(L"python scripts\\export_report.py --md docs\\results-table.md", repoW);
                runProcess(L"python scripts\\export_report.py --html docs\\report.html", repoW);
                // 4. Workload charts for the GUI Charts tab (GUI extra, not in CLI).
                runProcess(L"python scripts\\plot_workloads.py", repoW);
            }
        }

        disp.TryEnqueue([this, strong, all, lastScore, needCharts]()
        {
            OutputBox().Text(u8(all));
            ResultText().Text(lastScore.empty()
                ? locText("Done — see output / History.", "完成 —— 见输出/历史。")
                : u8(lastScore));
            Status().Text(needCharts ? locText("Done (charts & report regenerated).", "完成（已重新生成图表与报告）。")
                                     : locText("Done.", "完成。"));
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
        WorkloadFilterLabel().Text(locText("Workload", "负载"));
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
            if (!r.workload.empty()) ++workloadCounts[r.workload];
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
                         { "stream", "nbody", "stress", "synthpeak", "render3d" },
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
    WorkloadFilterAll().Content(locContent("All workloads", "全部负载"));
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
        if (!r.workload.empty()) workloads.insert(r.workload);
        if (r.particleCount > 0) particles.insert(r.particleCount);
    }

    auto addWorkload = [&](std::string const& id, std::string const& label) {
        if (workloads.find(id) == workloads.end()) return;
        ComboBoxItem item;
        item.Tag(box_value(u8(id)));
        item.Content(box_value(u8(label)));
        WorkloadFilterBox().Items().Append(item);
    };
    addWorkload("stream", "stream — bandwidth");
    addWorkload("nbody", "nbody — compute");
    addWorkload("stress", "stress — fragment fill");
    addWorkload("synthpeak", "synthpeak — peak ALU");
    addWorkload("render3d", "render3d — 3D rendering test");
    for (auto const& w : workloads)
        if (w != "stream" && w != "nbody" && w != "stress" && w != "synthpeak" && w != "render3d")
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
                                        locText("All workloads", "全部负载"), locText("None", "无"));
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
            if (hasWorkloadFilter && allowedWorkloads.find(r.workload) == allowedWorkloads.end()) continue;
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
            x.wl   = r->workload;
            x.particles = particleLabel(r->particleCount);
            x.score = r->score > 0.0
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
        if (workloadFilter != "*" && r.workload != workloadFilter) continue;
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
        x.wl   = r->workload;
        x.particles = particleLabel(r->particleCount);
        x.score = r->score > 0.0
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

// ---- charts (run plot_workloads.py, then show the PNGs) --------------------
void MainWindow::OnGenerateCharts(IInspectable const&, RoutedEventArgs const&)
{
    if (m_enginePath.empty()) { ChartsStatus().Text(locText("Engine/repo not found.", "未找到引擎/仓库。")); return; }
    auto repo = std::filesystem::path(m_enginePath).parent_path().parent_path().parent_path();
    if (!std::filesystem::exists(repo / "scripts" / "plot_workloads.py"))
    { ChartsStatus().Text(locText("scripts/plot_workloads.py not found.", "未找到 scripts/plot_workloads.py。")); return; }

    GenChartsButton().IsEnabled(false); ChartsBusy().IsActive(true);
    ChartsStatus().Text(locText("Running plot_workloads.py…", "正在运行 plot_workloads.py…"));

    auto strong = get_strong();
    auto disp = m_dispatcher;
    std::wstring repoW = repo.wstring();
    std::thread([this, strong, disp, repoW]()
    {
        int rc = runProcess(L"python scripts\\plot_workloads.py", repoW);
        disp.TryEnqueue([this, strong, repoW, rc]()
        {
            ChartsPanel().Children().Clear();
            std::filesystem::path imgDir = std::filesystem::path(repoW) / "docs" / "images";
            const char* names[] = { "workload_stream", "workload_nbody", "workload_stress",
                                    "workload_render3d", "workload_synthpeak" };
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
