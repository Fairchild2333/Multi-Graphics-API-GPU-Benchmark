#include "pch.h"
#define DISABLE_XAML_GENERATED_MAIN
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "App.xaml.g.hpp"
#include "gpu_engine.h"   // gpu_bench::cliMain

#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace
{
    // WinUI apps are /SUBSYSTEM:WINDOWS. When launched with command-line args,
    // behave as the console benchmark (gpu_bench::cliMain) by attaching to the
    // parent console (or allocating one) and re-pointing the CRT streams.
    int runAsCli()
    {
        if (::GetConsoleWindow() == nullptr)
        {
            if (!::AttachConsole(ATTACH_PARENT_PROCESS))
                ::AllocConsole();
        }
        FILE* f = nullptr;
        ::freopen_s(&f, "CONOUT$", "w", stdout);
        ::freopen_s(&f, "CONOUT$", "w", stderr);
        ::freopen_s(&f, "CONIN$",  "r", stdin);
        return gpu_bench::cliMain(__argc, __argv);
    }

    void appendCrashLog(char const* where, char const* detail)
    {
        try
        {
            wchar_t dir[MAX_PATH]{};
            DWORD n = ::GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH);
            if (n == 0 || n >= MAX_PATH) return;
            std::filesystem::path path =
                std::filesystem::path(dir) / L"GpuComputeBenchmark" / L"gui-crash.log";
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
            std::ofstream out(path, std::ios::app);
            if (!out) return;
            SYSTEMTIME st{};
            ::GetLocalTime(&st);
            out << st.wYear << '-' << st.wMonth << '-' << st.wDay << ' '
                << st.wHour << ':' << st.wMinute << ':' << st.wSecond
                << " [" << (where ? where : "?") << "] "
                << (detail ? detail : "") << '\n';
        }
        catch (...) {}
    }
}

namespace winrt::gpu_bench_gui::implementation
{
    App::App()
    {
        InitializeComponent();

        // Keep the shell alive on XAML/WinRT exceptions so a single failed
        // callback does not look like a mysterious silent quit.
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            try
            {
                auto msg = winrt::to_string(e.Message());
                appendCrashLog("UnhandledException", msg.c_str());
            }
            catch (...)
            {
                appendCrashLog("UnhandledException", "(message unavailable)");
            }
            e.Handled(true);
        });
    }

    void App::OnLaunched(LaunchActivatedEventArgs const&)
    {
        window = make<MainWindow>();
        // Activate before any deferred UI work (GPU Detecting / History) runs on
        // the dispatcher — otherwise WinUI throws "xamlRoot" and can quit.
        window.Activate();
    }
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    // Any command-line argument -> run as the console tool; none -> show GUI.
    if (__argc > 1)
        return runAsCli();

    winrt::init_apartment(winrt::apartment_type::single_threaded);
    ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
    {
        ::winrt::make<::winrt::gpu_bench_gui::implementation::App>();
    });
    return 0;
}
