#pragma once

#ifdef HAVE_OPENGL

#include "app_base.h"

#include <cstdint>

namespace gpu_bench {

class OpenGLBackend : public AppBase {
public:
    using AppBase::AppBase;

    std::string GetBackendName()    const override { return "OpenGL"; }
    std::string GetDeviceName()     const override { return deviceName_; }
    std::string GetDriverVersion()  const override { return driverVersion_; }
    std::string GetTimingMode()     const override {
        return synchronizedTimingFallback_
            ? "synchronized_wall_clock" : "gpu_timestamp_query";
    }
    bool NeedsOpenGLContext() const override { return true; }

protected:
    void InitBackend()              override;
    void DrawFrame(float deltaTime) override;
    void CleanupBackend()           override;
    void WaitIdle()                 override;

private:
    void CreateShaders();
    void CreateParticleBuffers();
    void CreateTimestampQueries();
    void CollectTimestampResults();
    bool UseTimestampQueries() const;
    void EnableSynchronizedTimingFallback(const char* reason);
    void CreateFluidResources();
    void CleanupFluidResources();
    void RecordFluidFrame(float deltaTime);

    std::uint32_t CompileShaderGL(const std::string& path, std::uint32_t type);
    std::uint32_t LinkProgramGL(std::uint32_t s1, std::uint32_t s2);

    std::string deviceName_;
    std::string driverVersion_;

    std::uint32_t computeProgram_ = 0;
    std::uint32_t renderProgram_  = 0;

    std::uint32_t ssbo_ = 0;
    std::uint32_t vao_  = 0;
    std::uint32_t ubo_  = 0;

    struct FluidResources {
        std::uint32_t advectProg = 0, divProg = 0, jacobiProg = 0, subtractProg = 0, renderProg = 0;
        std::uint32_t stateA = 0, stateB = 0, pressA = 0, pressB = 0, divBuf = 0;
        std::uint32_t paramsUbo = 0, renderUbo = 0, emptyVao = 0;
        std::uint32_t gridSize = 0;
        float simTime = 0.0f;
        bool active = false;
    } fluid_;

    // Render3D resources (instanced billboards, default-FBO depth)
    std::uint32_t quadVbo_     = 0;
    std::uint32_t render3dVao_ = 0;
    std::uint32_t cam3dUbo_    = 0;

    static constexpr int kTimestampsPerFrame = 4;
    static constexpr int kTimestampSlotCount = 4;
    std::uint32_t timestampQueries_[kTimestampSlotCount][kTimestampsPerFrame]{};
    void*  frameFences_[kTimestampSlotCount]{};  // GLsync per slot
    bool   timestampsSupported_ = false;
    bool   timestampQueriesAllocated_ = false;
    bool   synchronizedTimingFallback_ = false;
    int    timestampFrameCount_ = 0;
    int    currentFrame_        = 0;
    float  fractalElapsed_      = 0.0f;   // StressFractal palette time
};

}  // namespace gpu_bench

#endif  // HAVE_OPENGL
