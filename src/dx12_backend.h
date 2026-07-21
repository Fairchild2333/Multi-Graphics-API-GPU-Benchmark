#pragma once

#ifdef HAVE_DX12

#include "app_base.h"

#include <d3d12.h>
#include <dxgi1_5.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace gpu_bench {

class DX12Backend : public AppBase {
public:
    using AppBase::AppBase;

    std::string GetBackendName()    const override { return "DX12"; }
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

    void CreateDevice();
    void CreateCommandQueue();
    void CreateSwapChain();
    void CreateDescriptorHeaps();
    void CreateRenderTargets();
    void CreateCommandAllocatorsAndList();
    void CreateRootSignatures();
    void CreatePipelineStates();
    void CreateParticleBuffer();
    void CreateSfrResources();
    void CreateTimestampResources();
    void CreateFence();
    void WaitForGpu();
    void CollectTimestampResults();
    void CreateFluidResources();
    void CleanupFluidResources();
    void RecordFluidFrame(float deltaTime);
    void DrawSfrFrame(float deltaTime);

    UINT SingleNodeMask() const { return linkedAfr_ ? 0u : singleNodeMask_; }
    void ApplySingleNodeHeapMasks(D3D12_HEAP_PROPERTIES& heap) const {
        const UINT mask = SingleNodeMask();
        if (mask != 0) {
            heap.CreationNodeMask = mask;
            heap.VisibleNodeMask = mask;
        }
    }

    std::string deviceName_;
    std::string driverVersion_;

    static constexpr UINT kTimestampsPerFrame = 4;
    UINT frameCount_ = kMaxFramesInFlight;

    ComPtr<IDXGIFactory4>           factory_;
    ComPtr<ID3D12Device>            device_;
    ComPtr<ID3D12CommandQueue>      commandQueue_;
    std::vector<ComPtr<ID3D12CommandQueue>> afrCommandQueues_;
    ComPtr<IDXGISwapChain3>         swapChain_;
    bool                            linkedAfr_ = false;
    bool                            linkedSfr_ = false;
    UINT                            deviceNodeCount_ = 1;
    UINT                            singleNodeIndex_ = 0;
    UINT                            singleNodeMask_ = 0;
    UINT                            afrNodeCount_ = 1;
    D3D12_CROSS_NODE_SHARING_TIER  crossNodeSharingTier_ =
        D3D12_CROSS_NODE_SHARING_TIER_NOT_SUPPORTED;
    std::vector<UINT>               frameNodeMasks_;

    ComPtr<ID3D12DescriptorHeap>    rtvHeap_;
    std::vector<ComPtr<ID3D12DescriptorHeap>> afrRtvHeaps_;
    UINT                            rtvDescriptorSize_ = 0;
    std::vector<ComPtr<ID3D12Resource>> renderTargets_;

    ComPtr<ID3D12DescriptorHeap>    cbvSrvUavHeap_;

    std::vector<ComPtr<ID3D12CommandAllocator>> commandAllocators_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;
    std::vector<ComPtr<ID3D12GraphicsCommandList>> frameCommandLists_;
    std::vector<ComPtr<ID3D12CommandAllocator>> sfrSecondaryAllocators_;
    std::vector<ComPtr<ID3D12GraphicsCommandList>> sfrSecondaryCommandLists_;
    std::vector<ComPtr<ID3D12CommandAllocator>> sfrComposeAllocators_;
    std::vector<ComPtr<ID3D12GraphicsCommandList>> sfrComposeCommandLists_;

    ComPtr<ID3D12RootSignature>     computeRootSig_;
    ComPtr<ID3D12PipelineState>     computePSO_;
    ComPtr<ID3D12RootSignature>     graphicsRootSig_;
    ComPtr<ID3D12PipelineState>     graphicsPSO_;
    std::vector<ComPtr<ID3D12RootSignature>> afrGraphicsRootSigs_;
    std::vector<ComPtr<ID3D12PipelineState>> afrGraphicsPSOs_;

    ComPtr<ID3D12Resource>          particleBuffer_;
    ComPtr<ID3D12Resource>          particleUpload_;
    D3D12_VERTEX_BUFFER_VIEW        vbView_{};

    // DX12 linked-node split-frame rendering.  Node 1 shades the right half
    // into a node-local full-size target (preserving the shader's screen-space
    // coordinates), then copies that half into a node-0-owned cross-adapter
    // buffer.  Node 0 consumes the buffer after a GPU-side fence wait.
    std::vector<ComPtr<ID3D12Resource>> sfrSecondaryTargets_;
    std::vector<ComPtr<ID3D12Heap>> sfrCrossAdapterHeaps_;
    std::vector<ComPtr<ID3D12Resource>> sfrCrossAdapterBuffers_;
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> sfrCrossFootprints_;

    // Render3D resources (instanced billboards with depth)
    ComPtr<ID3D12Resource>          quadBuffer_;
    D3D12_VERTEX_BUFFER_VIEW        quadView_{};
    ComPtr<ID3D12DescriptorHeap>    dsvHeap_;
    ComPtr<ID3D12Resource>          depthBuffer_;

    ComPtr<ID3D12QueryHeap>         timestampHeap_;
    ComPtr<ID3D12Resource>          timestampReadback_;
    UINT64                          gpuFrequency_ = 0;
    std::vector<ComPtr<ID3D12QueryHeap>> afrTimestampHeaps_;
    std::vector<ComPtr<ID3D12Resource>> afrTimestampReadbacks_;
    std::vector<UINT64>             afrGpuFrequencies_;
    bool                            timestampsSupported_ = false;

    ComPtr<ID3D12Fence>             fence_;
    HANDLE                          fenceEvent_ = nullptr;
    std::vector<ComPtr<ID3D12Fence>> afrFences_;
    std::vector<HANDLE>             afrFenceEvents_;
    std::vector<UINT64>             afrNextFenceValues_;
    std::vector<UINT64>             frameFenceValues_;
    UINT                            frameIndex_ = 0;
    bool                            tearingSupported_ = false;
    float                           fractalElapsed_ = 0.0f;   // StressFractal palette time

    // Legacy 2D Fluid (Stam): isolated multi-pass compute + fullscreen dye render.
    struct FluidResources {
        ComPtr<ID3D12Resource> stateA;
        ComPtr<ID3D12Resource> stateB;
        ComPtr<ID3D12Resource> pressA;
        ComPtr<ID3D12Resource> pressB;
        ComPtr<ID3D12Resource> divBuf;
        ComPtr<ID3D12Resource> zeroPressUpload; // cleared pressA each frame

        ComPtr<ID3D12RootSignature> computeRootSig;
        ComPtr<ID3D12RootSignature> graphicsRootSig;
        ComPtr<ID3D12PipelineState> advectPSO;
        ComPtr<ID3D12PipelineState> divPSO;
        ComPtr<ID3D12PipelineState> jacobiPSO;
        ComPtr<ID3D12PipelineState> subtractPSO;
        ComPtr<ID3D12PipelineState> renderPSO;

        ComPtr<ID3D12DescriptorHeap> heap; // 5 tables × 5 UAVs + 1 SRV
        D3D12_GPU_DESCRIPTOR_HANDLE tableAdvect{};
        D3D12_GPU_DESCRIPTOR_HANDLE tableDiv{};
        D3D12_GPU_DESCRIPTOR_HANDLE tableJacA{};
        D3D12_GPU_DESCRIPTOR_HANDLE tableJacB{};
        D3D12_GPU_DESCRIPTOR_HANDLE tableSub{};
        D3D12_GPU_DESCRIPTOR_HANDLE tableRender{};

        std::uint32_t gridSize = 0;
        float         simTime  = 0.0f;
        bool          active   = false;
    } fluid_;
};

}  // namespace gpu_bench

#endif  // HAVE_DX12
