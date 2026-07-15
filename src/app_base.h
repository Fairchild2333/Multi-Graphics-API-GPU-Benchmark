#pragma once

#include "gpu_common.h"
#include "benchmark_results.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

struct GLFWwindow;

namespace gpu_bench {

class AppBase {
public:
    AppBase(std::int32_t gpuIndex, std::string shaderDir,
            BenchmarkConfig config = {});
    virtual ~AppBase();

    AppBase(const AppBase&) = delete;
    AppBase& operator=(const AppBase&) = delete;

    void Run();

    virtual std::string GetBackendName() const = 0;
    virtual std::string GetDeviceName() const  = 0;
    virtual std::string GetDriverVersion() const { return ""; }
    virtual bool NeedsOpenGLContext() const { return false; }
    const std::string& GetLastCapturePath() const { return lastCapturePath_; }

protected:
    virtual void InitBackend()            = 0;
    virtual void DrawFrame(float deltaTime) = 0;
    virtual void CleanupBackend()         = 0;
    virtual void WaitIdle()               = 0;

    void AccumulateTiming(double computeMs, double renderMs, double totalGpuMs);

    static std::vector<char> ReadFileBytes(const std::string& filename);
    static std::string GetCpuName();
    static std::string GetOsVersion();

    std::int32_t    requestedGpuIndex_;
    std::string     shaderDir_;
    BenchmarkConfig config_;
    GLFWwindow*     window_ = nullptr;
    std::vector<Particle> initialParticles_;

    bool IsRenderDocAttached() const { return rdocApi_ != nullptr; }
    void TriggerRenderDocCapture();
    std::uint32_t GetRenderDocCaptureCount() const;

private:
    void InitRenderDoc();
    void UpdateRenderDocCapturePath();
    void InitWindow();
    void GenerateInitialParticles();
    void MainLoop();
    void ReportTimingIfDue(double deltaTime);
    void PrintSummary() const;
    BenchmarkResult CollectResult() const;
    void CleanupWindow();

    // Thermal-stability analysis helpers.
    // computeAxisScore: same formula as CollectResult's derived score, but
    // factored out so we can recompute it for a single timing window.
    double computeAxisScore(double computeMs, double renderMs) const;
    // recordWindowSample: called from ReportTimingIfDue at each 1s boundary
    // once warmup is done. Pushes the window score and runs the stable/
    // throttled detection over the trailing samples.
    void recordWindowSample(double avgComputeMs, double avgRenderMs);

    void* rdocApi_ = nullptr;
    std::string rdocCaptureDir_;
    bool     rdocCaptureRequested_ = false;
    uint32_t rdocCaptureCount_     = 0;
    uint32_t rdocCaptureAttemptCount_ = 0;
    bool     rdocCaptureAttemptExcluded_ = false;
    std::string lastCapturePath_;

    double        lastFrameTime_      = 0.0;
    double        runStartTime_       = 0.0;
    double        accumComputeMs_     = 0.0;
    double        accumRenderMs_      = 0.0;
    double        accumTotalGpuMs_    = 0.0;
    std::uint32_t timingSampleCount_  = 0;
    double        timingReportTimer_  = 0.0;
    std::uint32_t frameCount_         = 0;
    bool          warmupDone_         = false;

    std::uint32_t totalFrameCount_    = 0;
    std::uint32_t timingSamplesToSkip_ = 0;
    double        excludedCaptureSec_  = 0.0;

    double benchMinComputeMs_  = std::numeric_limits<double>::max();
    double benchMaxComputeMs_  = 0.0;
    double benchMinRenderMs_   = std::numeric_limits<double>::max();
    double benchMaxRenderMs_   = 0.0;
    double benchMinTotalGpuMs_ = std::numeric_limits<double>::max();
    double benchMaxTotalGpuMs_ = 0.0;
    double benchSumComputeMs_  = 0.0;
    double benchSumRenderMs_   = 0.0;
    double benchSumTotalGpuMs_ = 0.0;
    std::uint32_t benchSampleCount_ = 0;

    double benchStartTime_     = 0.0;
    double benchEndTime_       = 0.0;
    std::uint32_t benchMeasuredFrames_ = 0;
    double benchMinFrameTime_  = std::numeric_limits<double>::max();

    // ---- Thermal-stability tracking --------------------------------------
    // Per-window axis score (GB/s, GFLOP/s, GSample/s, ...). A new sample is
    // pushed every kTimingReportIntervalSec (1s) once warmup is done. The
    // trailing 30 samples (~30s) feed a rolling mean + coefficient-of-variation
    // used to detect throttling and to report a "stable score".
    std::vector<double> windowScores_;
    std::vector<double> windowRenderMs_;
    double stableScore_        = 0.0;   // last mean of a stable 5-window stretch
    double stableVariancePct_  = -1.0;  // CV (%) at the stable point; -1 = never
    bool   thermalStable_      = false;
    double throttlePct_        = 0.0;   // (earlyMean - lateMean) / earlyMean
    bool   gpuStressCalibrationDone_ = false;
    bool   gpuBurnCalibrationDone_   = false;
};

}  // namespace gpu_bench
