#pragma once

#include "MainWindow.g.h"
#include "benchmark_results.h"
#include <winrt/Microsoft.UI.Dispatching.h>
#include <atomic>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace winrt::gpu_bench_gui::implementation
{
    // One measured point of a memory-bandwidth run (a MEMBW_POINT line).
    struct MemBwCurvePoint
    {
        std::string kernel;  // read | write | copy | host_read | host_write | l1 | l1_single
        double bytes{ 0.0 }; // data-set size, or private block size for the L1 kernels
        double gbps{ 0.0 };  // median bandwidth
    };

    // One periodic timing line of a GPU worker.
    struct GpuRunSample
    {
        double seconds{ 0.0 }; // since the run began
        double computeMs{ 0.0 };
        double renderMs{ 0.0 };
        bool warmup{ false };
    };

    // Score and timing samples of one GPU x API job of the last run.
    struct GpuRunChart
    {
        std::string label;  // "GPU - API"
        std::string metric; // score name as the worker prints it
        std::string unit;
        double score{ 0.0 };
        std::vector<GpuRunSample> samples;
    };

    // Score of one logical processor in the CPU compute test.
    struct CpuCoreScore
    {
        int core{ 0 };
        double score{ 0.0 };
        std::string unit;
        std::string label;
    };

    // One CPUMEM_LEVEL line of a CPU cache / memory sweep.
    struct CpuMemLevelRow
    {
        std::string mode;   // single | all
        std::string level;  // L1 .. L4 | memory
        double cacheBytes{ 0.0 };
        double read{ 0.0 };
        double write{ 0.0 };
        double copy{ 0.0 };
        double writeStream{ 0.0 };
        double copyStream{ 0.0 };
        double zeroFill{ 0.0 };
        bool measured{ false };
        bool settled{ true };
        // Read off the curve because the OS reports no size for this cache;
        // its capacity lies between the two values.
        bool fromCurve{ false };
        double capacityLow{ 0.0 };
        double capacityHigh{ 0.0 };
    };

    // One CPUMEM_CACHE line.
    struct CpuMemCache
    {
        int cpuClass{ 0 };
        int level{ 0 };
        double sizeBytes{ 0.0 };
        int sharedCores{ 1 };
    };

    // One CPUMEM_CLASS line: a class of cores and the mode id of its sweep.
    struct CpuMemClassInfo
    {
        int rank{ 0 };
        std::string mode; // single | single_<rank>
        std::string name;
        int cores{ 0 };
        int referenceLp{ 0 };
    };

    // The CPUMEM_STORAGE line.
    struct CpuMemStorageInfo
    {
        bool seen{ false };
        bool measured{ false };
        bool directIo{ false };
        std::string backing;
        std::string skipped;
        double seqRead{ 0.0 };
        double seqWrite{ 0.0 };
        double randRead{ 0.0 };
        double randWrite{ 0.0 };
        double randReadIops{ 0.0 };
        double randWriteIops{ 0.0 };
    };

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
        void OnMultiGpuChanged(
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
        void OnBurnStepPresetChanged(
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
        void OnCpuModeChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void OnCpuMemChartHostSizeChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& args);
        void OnGpuCliHostSizeChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& args);
        void OnMemBwChartHostSizeChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& args);
        void OnGpuRunChartHostSizeChanged(
            winrt::Windows::Foundation::IInspectable const& sender,
            winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& args);
        void OnChartsHostSizeChanged(
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

        enum class GpuRunIssueKind : int
        {
            UnsupportedGpuApi = 0,
            OpenGlRouting,
            ApiUnavailable,
            VulkanRuntimeMissing,
            OpenGlVersion,
            WorkloadUnsupported,
            WorkerTimeout,
            DeviceLost,
            ShaderPipeline,
            SwapchainOutOfDate,
            ResourceAllocation,
            BurnStepsClamped, // informational: run succeeded with reduced steps
            Dx12SecondaryNodeRenderDoc, // informational: capture auto-disabled on linked node >= 1
            Dx12RenderDocFallback, // informational: score succeeded after no-capture retry
            Dx12LegacyIntelRenderDoc, // HD 6000 legacy driver; newer driver captured successfully
            Dx11SynchronizedTiming, // informational: invalid D3D11 timestamps replaced by sync timing
            OpenGlSynchronizedTiming, // informational: invalid GL timestamps replaced by sync timing
            SuspectResult, // informational: retained score appears physically implausible
            DisplayBound, // informational: frames waited for the display, the score may be too low
            Unknown,
        };

        struct GpuRunIssue
        {
            GpuRunIssueKind kind{ GpuRunIssueKind::Unknown };
            std::string target; // display-only "GPU — API" label
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
        void renderMemBwCharts();
        void renderGpuRunCharts();
        void renderCpuCoreChart();
        void renderChartsPage();
        void setTaskbarProgress(bool active, double fraction,
                                bool indeterminate = false);
        void dismissNumberBoxPopupsOnOutsideClick(
            winrt::Microsoft::UI::Xaml::DependencyObject const& src);
        void collapseSpinSelectionSoon(
            winrt::Microsoft::UI::Xaml::Controls::NumberBox const& box);
        void updateGpuProgressTick();
        void stopGpuProgress(winrt::hstring const& stage, bool complete);
        void updateExtraLabel();
        void updateDurationValueEnabled();
        bool isUnlimitedDuration();
        std::string durationUnitTag();
        double durationAmountValue();
        void syncCaptureControls();
        void syncMultiGpuControls();
        void applyMultiGpuRenderDocOverride(bool active);
        void applyPlatformFeatureVisibility();
        bool hasLinkedIdenticalDualGpu() const;
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
        std::string burnStepValue();   // selected fixed GPU Burn steps
        // Duration as engine args: {"--time","<s>"}, {"--benchmark","<frames>"},
        // or {"--no-time-limit"} when Duration is "Until Cancel".
        std::vector<std::string> durationArgs();
        // Build child-process CLI invocation(s) for the selected preset (sets needCharts).
        std::vector<std::vector<std::string>> buildPresetJobs(
            bool& needCharts, std::vector<std::string>& skippedJobs);
        void launchJobs(std::vector<std::vector<std::string>> jobs, bool needCharts,
                        std::vector<std::string> skippedJobs);
        void launchCpuBenchmark(std::string mode, double seconds, double warmupSeconds);
        void updateCpuModeControls();
        void clearCpuResults();
        void handleCpuMemLine(std::string const& line);
        void renderCpuMemResults();
        void renderCpuMemCharts();
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
        std::vector<std::string> m_gpuNames; // probe name aligned with m_gpuIndices
        std::vector<std::array<bool, 5>> m_gpuApiSupport;  // {vulkan,dx12,dx11,opengl,dx11Compute}
        std::vector<std::uint32_t> m_gpuDx12NodeCounts; // linked-adapter node count per GPU row
        std::vector<bool> m_gpuSoftware;   // software/WARP row aligned with m_gpuIndices
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
        // Last CPU cache / memory sweep, kept so its table and charts can be
        // rebuilt on resize, theme and language changes. UI-thread only.
        std::vector<CpuMemLevelRow> m_cpuMemLevels;
        std::vector<MemBwCurvePoint> m_cpuMemPoints; // kernel is "<mode>/<kernel>"
        std::vector<CpuMemCache> m_cpuMemCaches;
        std::vector<CpuMemClassInfo> m_cpuMemClasses;
        std::vector<CpuCoreScore> m_cpuCoreScores; // CPU compute test, one per logical processor
        CpuMemStorageInfo m_cpuMemStorage;
        int m_cpuMemAllThreads{ 0 };
        bool m_cpuMemPageMeasured{ false };
        double m_cpuMemPageGbps{ 0.0 };
        double m_cpuMemPageNs{ 0.0 };
        double m_cpuMemPageBytes{ 0.0 };
        bool m_cpuMemView{ false }; // the CPU page shows the cache / memory sweep
        bool m_cpuMemDone{ false };
        double m_cpuMemChartWidth{ 0.0 };

        std::vector<gpu_bench::BenchmarkResult> m_results;   // loaded history
        std::vector<std::string> m_displayedIds;             // result id per visible row
        bool m_historyFiltersInitialized{ false };
        HistoryCategory m_historyCategory{ HistoryCategory::Gpu };
        bool m_showLegacyHistory{ false };
        bool m_showHeadlessHistory{ false };
        std::string m_historySortColumn{ "time" };
        bool m_historySortAscending{ false };
        std::string m_lastScoreEn; // English score line; re-localised on language change
        // Curves of the last memory-bandwidth run, kept so the charts can be
        // redrawn on resize, theme and language changes.
        std::vector<MemBwCurvePoint> m_memBwPoints;
        bool m_memBwSystemMemory{ false };
        double m_memBwChartWidth{ 0.0 };
        // Jobs of the last GPU run, kept so their charts can be redrawn.
        std::vector<GpuRunChart> m_gpuRunCharts;
        double m_gpuRunChartWidth{ 0.0 };
        double m_chartsPageWidth{ 0.0 };
        bool m_lastScoreCacheHint{ false }; // VRAM run with a working set small enough for L2
        std::vector<std::string> m_lastSkippedJobs; // known-unsupported GPU/API combinations
        std::vector<GpuRunIssue> m_lastGpuRunIssues; // exact/common worker failure classes
        bool m_lastPostProcessFailed{ false };
        bool m_suppressRenderDocUi{ false };
        bool m_headlessRenderDocOverrideActive{ false };
        bool m_renderDocBeforeHeadless{ true };
        bool m_captureBeforeHeadless{ true };
        bool m_multiGpuRenderDocOverrideActive{ false };
        bool m_renderDocBeforeMultiGpu{ true };
        bool m_captureBeforeMultiGpu{ true };
    };
}

namespace winrt::gpu_bench_gui::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow> {};
}
