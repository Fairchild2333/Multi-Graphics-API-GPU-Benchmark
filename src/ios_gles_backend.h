#pragma once
#include "app_base.h"
namespace gpu_bench {
// iOS EAGL raster path for the existing ES 2.0 legacy GPU Burn contract.
// ES 3.1 compute workloads have no corresponding API on iOS.
class IosGlesBackend final : public AppBase {
public:
    using AppBase::AppBase;
    ~IosGlesBackend() override;
    std::string GetBackendName() const override { return "OpenGL ES 2.0"; }
    std::string GetDeviceName() const override { return deviceName_; }
    std::string GetDriverVersion() const override { return driverVersion_; }
    std::string GetTimingMode() const override { return "synchronized_wall_clock"; }
protected:
    void InitBackend() override;
    void DrawFrame(float deltaTime) override;
    void CleanupBackend() override;
    void WaitIdle() override;
private:
    struct State;
    State* state_ = nullptr;
    std::string deviceName_, driverVersion_;
    float elapsed_ = 0;
};
bool IosGlesAvailable();
std::uint32_t IosGlesMaxSteps();
}
