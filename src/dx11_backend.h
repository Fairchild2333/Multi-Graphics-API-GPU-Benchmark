#pragma once

#ifdef HAVE_DX11

#include "app_base.h"

#include <d3d11.h>
#include <dxgi1_5.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace gpu_bench {

class DX11Backend : public AppBase {
public:
    using AppBase::AppBase;

    std::string GetBackendName()    const override { return "DX11"; }
    std::string GetDeviceName()     const override { return deviceName_; }
    std::string GetDriverVersion()  const override { return driverVersion_; }

protected:
    void InitBackend()              override;
    void DrawFrame(float deltaTime) override;
    void CleanupBackend()           override;
    void WaitIdle()                 override;

private:
    template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    static void ThrowIfFailed(HRESULT hr, const char* msg);
    ComPtr<ID3DBlob> CompileShader(const std::string& path,
                                   const char* entry, const char* target);

    void CreateDeviceAndSwapChain();
    void CreateRenderTarget();
    void CreateShaders();
    void CreateParticleBuffers();
    void CreateComputeParamsCB();
    void CreateTimestampQueries();
    void CreateFluidResources();
    void CleanupFluidResources();
    void RecordFluidFrame(float deltaTime);

    void CollectTimestampResults();

    std::string deviceName_;
    std::string driverVersion_;

    ComPtr<ID3D11Device>           device_;
    ComPtr<ID3D11DeviceContext>    context_;
    ComPtr<IDXGISwapChain1>        swapChain_;
    D3D_FEATURE_LEVEL              featureLevel_ = D3D_FEATURE_LEVEL_11_0;
    bool                           computeShadersSupported_ = true;

    ComPtr<ID3D11RenderTargetView> rtv_;

    ComPtr<ID3D11ComputeShader>    computeShader_;
    ComPtr<ID3D11VertexShader>     vertexShader_;
    ComPtr<ID3D11PixelShader>      pixelShader_;
    ComPtr<ID3D11InputLayout>      inputLayout_;

    // Structured buffer for compute UAV
    ComPtr<ID3D11Buffer>           computeBuffer_;
    ComPtr<ID3D11UnorderedAccessView> computeUAV_;
    // Vertex buffer (copy destination each frame)
    ComPtr<ID3D11Buffer>           vertexBuffer_;

    ComPtr<ID3D11Buffer>           computeParamsCB_;

    // Blend state for alpha blending
    ComPtr<ID3D11BlendState>       blendState_;
    ComPtr<ID3D11RasterizerState>  rasterizerState_;

    // Render3D resources (instanced billboards with depth)
    ComPtr<ID3D11Buffer>             quadBuffer_;
    ComPtr<ID3D11Buffer>             cam3dCB_;
    ComPtr<ID3D11Texture2D>          depthTex_;
    ComPtr<ID3D11DepthStencilView>   depthView_;
    ComPtr<ID3D11DepthStencilState>  depthState_;

    // Timestamp queries -- deeper ring buffer so slow GPUs have time to finish
    static constexpr UINT kTimestampsPerFrame = 4;
    static constexpr UINT kTimestampSlotCount = 8;
    ComPtr<ID3D11Query> disjointQueries_[kTimestampSlotCount];
    ComPtr<ID3D11Query> timestampQueries_[kTimestampSlotCount][kTimestampsPerFrame];
    bool timestampsSupported_    = false;
    bool tearingSupported_       = false;
    bool timestampDiagPrinted_   = false;
    // A slot stays reserved (not reissued) until its queries resolve; results
    // are polled non-blocking, so headless frame rates are not throttled.
    bool slotPending_[kTimestampSlotCount]{};
    UINT slotAge_[kTimestampSlotCount]{};
    UINT64 lastGoodFrequency_    = 0;
    std::uint32_t currentFrame_  = 0;
    float fractalElapsed_        = 0.0f;   // StressFractal palette time

    struct FluidResources {
        ComPtr<ID3D11Buffer> stateA, stateB, pressA, pressB, divBuf;
        ComPtr<ID3D11UnorderedAccessView> uavStateA, uavStateB, uavPressA, uavPressB, uavDiv;
        ComPtr<ID3D11ShaderResourceView> srvStateA;
        ComPtr<ID3D11ComputeShader> csAdvect, csDiv, csJacobi, csSubtract;
        ComPtr<ID3D11VertexShader> vs;
        ComPtr<ID3D11PixelShader> ps;
        ComPtr<ID3D11Buffer> paramsCB, renderCB;
        ComPtr<ID3D11RasterizerState> rast;
        std::uint32_t gridSize = 0;
        float simTime = 0.0f;
        bool active = false;
    } fluid_;
};

}  // namespace gpu_bench

#endif  // HAVE_DX11
