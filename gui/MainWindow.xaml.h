#pragma once

#include "MainWindow.g.h"
#include "benchmark_results.h"
#include <winrt/Microsoft.UI.Dispatching.h>
#include <string>
#include <vector>

namespace winrt::gpu_bench_gui::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        // Navigation / shell
        void OnNavSelectionChanged(
            winrt::Microsoft::UI::Xaml::Controls::NavigationView const& sender,
            winrt::Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args);
        void OnThemeChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnLangChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

        // Run page
        void OnRun(winrt::Windows::Foundation::IInspectable const& sender,
                   winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnPresetChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnWorkloadChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

        // History / charts
        void OnRefreshHistory(winrt::Windows::Foundation::IInspectable const& sender,
                              winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnDeleteSelected(winrt::Windows::Foundation::IInspectable const& sender,
                              winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnHistoryViewChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnDateRangeChanged(
            winrt::Microsoft::UI::Xaml::Controls::CalendarDatePicker const& sender,
            winrt::Microsoft::UI::Xaml::Controls::CalendarDatePickerDateChangedEventArgs const& args);
        void OnGenerateCharts(winrt::Windows::Foundation::IInspectable const& sender,
                              winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        void applyLanguage();
        void applyTheme(int index);
        void updateCaptionButtonColors();
        void updateResizeBackdropColor();
        void animatePageIn(winrt::Microsoft::UI::Xaml::FrameworkElement const& page);
        void showPage(int index);
        void populateGpus();
        void updateExtraLabel();
        void refreshHistory();         // (re)load from disk + rebuild filters + render
        void applyHistoryView();       // filter + sort + render m_results
        void rebuildGpuFilter();       // distinct-device toggle menu items
        std::string selected(winrt::Microsoft::UI::Xaml::Controls::ComboBox const& box);
        std::string backendValue();    // capitalized display -> engine token
        std::vector<std::string> buildArgs(bool runAll);
        // Build the cliMain invocation(s) for the selected preset (sets needCharts).
        std::vector<std::vector<std::string>> buildPresetJobs(bool& needCharts);
        void launchJobs(std::vector<std::vector<std::string>> jobs, bool needCharts);

        Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
        HWND  m_hwnd{ nullptr };
        HBRUSH m_bgBrush{ nullptr };
        bool  m_uiReady{ false };
        bool  m_suppressCombo{ false };
        std::string m_enginePath;          // build/Release/gpu_benchmark.exe (shader dir)
        std::vector<int> m_gpuIndices;     // engine GPU index per GpuBox row after "(auto)"
        std::string m_cpuName;             // for relabelling the software (WARP) renderer

        std::vector<gpu_bench::BenchmarkResult> m_results;   // loaded history
        std::vector<std::string> m_displayedIds;             // result id per visible row
    };
}

namespace winrt::gpu_bench_gui::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {};
}
