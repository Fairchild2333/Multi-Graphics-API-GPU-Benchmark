#pragma once

#ifdef HAVE_GLES

#include "app_base.h"

#include <cstdint>

namespace gpu_bench {

// The size of the compute work groups the particle test uses under OpenGL ES
// on this device: kComputeWorkGroupSize, kComputeWorkGroupSizeEs31 where the
// driver allows no more, or 0 where the test cannot run (no OpenGL ES 3.1, or
// a limit below what OpenGL ES 3.1 promises). The front ends ask, so that a
// test card can name the score group before the run. It makes a context of
// its own on a small off-screen surface: call it while no test is running.
std::uint32_t EsParticleWorkGroupSize();

// OpenGL ES on EGL, for the embedded hosts (Android, HarmonyOS).
//
// OpenGL ES 3.1 runs two tests: the particle test and GPU Burn. Both use the
// shader sources of the desktop OpenGL backend unchanged except for the
// version line (see ToEsSource in gles_backend.cpp), so a test does the same
// work per frame under OpenGL ES as under desktop OpenGL.
//
// OpenGL ES 2.0 (BenchmarkConfig::gpuBurnEs2Legacy) runs one test, a GPU Burn
// of its own: shaders/gpu_burn_es2.*, the score group gpu_burn_es2_legacy_v1.
// It uses nothing newer than OpenGL ES 2.0 and times its frames by wall clock.
//
// The picture is always kWindowWidth x kWindowHeight, as with Vulkan; the
// system scales it to the view. A headless run draws nothing and needs no
// window: it makes its context current on a small off-screen surface.
class GlesBackend : public AppBase {
public:
    using AppBase::AppBase;
    // Also when a run ends in an error: the window must be free for the next one.
    ~GlesBackend() override;

    std::string GetBackendName()    const override {
        return config_.gpuBurnEs2Legacy ? "OpenGL ES 2.0" : "OpenGL ES";
    }
    std::string GetDeviceName()     const override { return deviceName_; }
    std::string GetDriverVersion()  const override { return driverVersion_; }
    std::string GetTimingMode()     const override {
        return synchronizedTimingFallback_
            ? "synchronized_wall_clock" : "gpu_timestamp_query";
    }

protected:
    void InitBackend()              override;
    void DrawFrame(float deltaTime) override;
    void CleanupBackend()           override;
    void WaitIdle()                 override;

private:
    void CreateContext();
    void DestroyContext();
    void CreateShaders();
    void CreateParticleBuffers();
    void CreateTimestampQueries();
    void CheckTimestampsAgainstWallClock();
    void CollectTimestampResults();
    bool UseTimestampQueries() const;
    void EnableSynchronizedTimingFallback(const char* reason);
    void Present();
    void FindGpuWait();
    void WaitForGpu();
    void CheckWaitCoversCompute();
    void WaitForComputeByReadback();

    // The OpenGL ES 2.0 GPU Burn.
    bool Es2() const { return config_.gpuBurnEs2Legacy; }
    void CreateEs2Burn();
    void DrawEs2BurnFrame(float deltaTime);

    std::uint32_t CompileShader(const std::string& file, std::uint32_t type);
    std::uint32_t CompileSource(const std::string& source, const std::string& file, std::uint32_t type);
    std::uint32_t LinkProgram(std::uint32_t first, std::uint32_t second);

    std::string deviceName_;
    std::string driverVersion_;

    // EGLDisplay / EGLSurface / EGLContext, kept as pointers so that this
    // header needs no EGL header.
    void* display_ = nullptr;
    void* surface_ = nullptr;
    void* context_ = nullptr;

    std::uint32_t computeProgram_ = 0;
    std::uint32_t renderProgram_  = 0;

    std::uint32_t ssbo_ = 0;
    std::uint32_t vao_  = 0;
    std::uint32_t ubo_  = 0;

    // One particle's worth of bytes that the CPU reads back; see
    // WaitForComputeByReadback.
    std::uint32_t readbackBuffer_ = 0;
    bool          computeWaitByReadback_ = false;

    // OpenGL ES 2.0: the triangle's corners, and where the shader takes them
    // and its two parameters.
    std::uint32_t triangleVbo_   = 0;
    int           positionSlot_  = 0;
    int           phaseUniform_  = -1;
    int           passUniform_   = -1;

    static constexpr int kTimestampsPerFrame = 4;
    static constexpr int kTimestampSlotCount = 4;
    std::uint32_t timestampQueries_[kTimestampSlotCount][kTimestampsPerFrame]{};
    void*  frameFences_[kTimestampSlotCount]{};  // GLsync per slot
    bool   timestampsSupported_ = false;
    bool   timestampQueriesAllocated_ = false;
    bool   synchronizedTimingFallback_ = false;
    int    timestampFrameCount_ = 0;
    int    currentFrame_        = 0;
    float  burnElapsed_         = 0.0f;   // GPU Burn animation clock
};

}  // namespace gpu_bench

#endif  // HAVE_GLES
