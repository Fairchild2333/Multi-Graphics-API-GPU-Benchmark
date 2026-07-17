#pragma once

#include "MainWindow.g.h"
#include "benchmark_results.h"
#include <winrt/Microsoft.UI.Dispatching.h>
#include <atomic>
#include <array>
#include <map>
#include <memory>
#include <mutex>
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
        void OnGpuCancel(winrt::Windows::Foundation::IInspectable const& sender,
                         winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnPresetChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnGpuSelectionChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnWorkloadChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnShowLegacyChecked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnRenderDocChecked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnCaptureChecked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnCaptureValueChanged(
            winrt::Microsoft::UI::Xaml::Controls::NumberBox const& sender,
            winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& args);
        void OnApiPickerDropDownOpened(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Windows::Foundation::IInspectable const& args);
        void OnApiPickerTapped(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const& args);
        void OnDurationUnitChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnDurationValueChanged(
            winrt::Microsoft::UI::Xaml::Controls::NumberBox const& sender,
            winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& args);
        void OnParticlePresetChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

        // CPU page
        void OnCpuRun(winrt::Windows::Foundation::IInspectable const& sender,
                      winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnCpuCancel(winrt::Windows::Foundation::IInspectable const& sender,
                         winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnCpuDurationPresetChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnCpuTimeChanged(
            winrt::Microsoft::UI::Xaml::Controls::NumberBox const& sender,
            winrt::Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const& args);
        void OnGpuCliHostSizeChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& args);
        void OnCpuCliHostSizeChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& args);
        void OnGpuPagePointerPressed(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void OnCpuPagePointerPressed(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);

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
        void OnOpenResultsFolder(winrt::Windows::Foundation::IInspectable const& sender,
                                 winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnOpenCapturesFolder(winrt::Windows::Foundation::IInspectable const& sender,
                                  winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void OnHistoryCategoryChanged(
            winrt::Microsoft::UI::Xaml::Controls::SelectorBar const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectorBarSelectionChangedEventArgs const& args);
        void OnHistoryLegacyChecked(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        enum class ActiveTask : int
        {
            None = 0,
            GpuBenchmark,
            CpuBenchmark,
            Charts,
        };

        // Footer traffic light: green=ready/success, yellow=running, red=error/fail.
        enum class StatusLight : int
        {
            Ready = 0,
            Running,
            Error,
        };

        enum class HistoryCategory : int
        {
            Gpu = 0,
            Cpu,
        };

        void applyLanguage();
        void configureCpuNumberBoxes();
        void setStatusLight(winrt::Microsoft::UI::Xaml::Shapes::Ellipse const& light,
                            StatusLight kind);
        void setGpuStatus(StatusLight kind, winrt::hstring const& text);
        void setCpuStatus(StatusLight kind, winrt::hstring const& text);
        winrt::hstring gpuRunningStatusText() const;
        void refreshActiveGpuStatusLanguage();
        void applyWorkloadVisibility();
        void applyTheme(int index);
        void updateCaptionButtonColors();
        void updateResizeBackdropColor();
        void animatePageIn(winrt::Microsoft::UI::Xaml::FrameworkElement const& page);
        void showPage(int index);
        void populateGpus();
        void rebuildApiPicker(bool preserveSelection);
        void updateApiPickerSummary();
        void closeApiPickerDropDown();
        void updateResultHint();
        void renderResultScore();
        void setTaskbarProgress(bool active, double fraction,
                                bool indeterminate = false);
        void updateGpuProgressTick();
        void stopGpuProgress(winrt::hstring const& stage, bool complete);
        void updateExtraLabel();
        void updateDurationValueEnabled();
        bool isUnlimitedDuration();
        std::string durationUnitTag();
        double durationAmountValue();
        void syncCaptureControls();
        void appendCaptureArgs(std::vector<std::string>& dest);
        void refreshAboutVersion();
        void refreshHistory();         // (re)load from disk + rebuild filters + render
        void applyHistoryView();       // filter + sort + render m_results
        void rebuildGpuFilter(bool preserveSelection = true);       // GPU tree or CPU list
        void rebuildHistoryFilters(bool preserveSelection = true);  // API + workload + particles/steps
        void updateHistoryFilterVisibility();
        void syncHistoryCategoryFromUi();
        std::string selected(winrt::Microsoft::UI::Xaml::Controls::ComboBox const& box);
        std::vector<std::string> selectedApis();
        std::string particleValue();   // "" means use engine default
        // Duration as engine args: {"--time","<s>"}, {"--benchmark","<frames>"},
        // or {"--no-time-limit"} when Duration is "Until Cancel".
        std::vector<std::string> durationArgs();
        // Build child-process CLI invocation(s) for the selected preset (sets needCharts).
        std::vector<std::vector<std::string>> buildPresetJobs(bool& needCharts);
        void launchJobs(std::vector<std::vector<std::string>> jobs, bool needCharts);
        void launchCpuBenchmark(std::string mode, double seconds, double warmupSeconds);
        void cancelCpuBenchmark();
        void cancelGpuBenchmark();
        bool tryBeginTask(ActiveTask task);
        void endTask(ActiveTask task);
        void syncActionButtonsEnabled();
        bool uiAlive() const;

        Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{ nullptr };
        HWND  m_hwnd{ nullptr };

        // Taskbar progress (ITaskbarList3) + GPU run progress estimation.
        // GPU progress state is only touched on the UI thread.
        winrt::com_ptr<ITaskbarList3> m_taskbar{ nullptr };
        bool m_taskbarInitTried{ false };
        Microsoft::UI::Xaml::DispatcherTimer m_gpuProgressTimer{ nullptr };
        size_t m_gpuProgressJobs{ 0 };
        size_t m_gpuProgressJobIndex{ 0 };
        double m_gpuProgressJobExpectedSec{ 15.0 };
        bool   m_gpuProgressIndeterminate{ false };
        std::string m_gpuProgressApiLabel; // English API label for mid-run language refresh
        std::chrono::steady_clock::time_point m_gpuProgressJobStart{};
        HBRUSH m_bgBrush{ nullptr };
        bool  m_uiReady{ false };
        bool  m_suppressCombo{ false };
        std::string m_enginePath;          // UTF-8 path to isolated gpu_benchmark.exe worker
        std::vector<int> m_gpuIndices;     // engine GPU index per GpuBox row after "(auto)"
        std::vector<std::array<bool, 5>> m_gpuApiSupport;  // {vulkan,dx12,dx11,opengl,dx11Compute}
        bool m_gpuEnumerationComplete{ false };
        bool m_apiSelectionInitialized{ false };
        std::string m_cpuName;             // for relabelling the software (WARP) renderer

        std::atomic<ActiveTask> m_activeTask{ ActiveTask::None };
        std::atomic_bool m_cpuRunning{ false };
        std::atomic_bool m_cpuCancelRequested{ false };
        std::atomic_bool m_gpuCancelRequested{ false };
        std::atomic_bool m_closing{ false };
        std::mutex m_cpuProcessMutex;
        HANDLE m_cpuProcess{ nullptr };    // owned and closed by the CPU worker thread
        // Shared so workers can Wait on the handle after Closed drops the window's ref.
        std::shared_ptr<void> m_gpuCancelEvent;
        std::map<int, std::string> m_cpuCoreLabels; // populated from CPU_TOPOLOGY on the UI thread
        bool m_cpuHadProtocolError{ false }; // UI-thread only

        std::vector<gpu_bench::BenchmarkResult> m_results;   // loaded history
        std::vector<std::string> m_displayedIds;             // result id per visible row
        bool m_historyFiltersInitialized{ false };
        HistoryCategory m_historyCategory{ HistoryCategory::Gpu };
        bool m_showLegacyHistory{ false };
        bool m_showHeadlessHistory{ false };
        std::string m_historySortColumn{ "time" };
        bool m_historySortAscending{ false };
        std::string m_lastScoreEn; // English score line; re-localised on language change
        bool m_lastScoreCacheHint{ false }; // VRAM run with a working set small enough for L2
        bool m_suppressRenderDocUi{ false };
    };
}

namespace winrt::gpu_bench_gui::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {};
}
