#include "pch.h"
#define DISABLE_XAML_GENERATED_MAIN
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "App.xaml.g.hpp"
#include "gpu_engine.h"   // gpu_bench::cliMain

#include <cstdio>

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
}

namespace winrt::gpu_bench_gui::implementation
{
    App::App()
    {
        InitializeComponent();
    }

    void App::OnLaunched(LaunchActivatedEventArgs const&)
    {
        window = make<MainWindow>();
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
