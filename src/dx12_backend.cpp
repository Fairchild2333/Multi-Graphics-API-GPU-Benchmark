#ifdef HAVE_DX12

#include "dx12_backend.h"
#include "mini_mat.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace gpu_bench {

using Microsoft::WRL::ComPtr;

static std::string HrToHex(HRESULT hr) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setfill('0') << std::setw(8) << static_cast<unsigned long>(hr);
    return oss.str();
}

static const char* CrossNodeSharingTierName(D3D12_CROSS_NODE_SHARING_TIER tier) {
    switch (tier) {
    case D3D12_CROSS_NODE_SHARING_TIER_1_EMULATED: return "Tier 1 Emulated";
    case D3D12_CROSS_NODE_SHARING_TIER_1:          return "Tier 1";
    case D3D12_CROSS_NODE_SHARING_TIER_2:          return "Tier 2";
#ifdef D3D12_CROSS_NODE_SHARING_TIER_3
    case D3D12_CROSS_NODE_SHARING_TIER_3:          return "Tier 3";
#endif
    default:                                        return "Not supported";
    }
}

void DX12Backend::ThrowIfFailed(HRESULT hr, const char* msg) {
    if (FAILED(hr)) {
        std::string full = std::string(msg) + " (HRESULT " + HrToHex(hr) + ")";
        std::cout << full << std::endl;
        throw std::runtime_error(full);
    }
}

Microsoft::WRL::ComPtr<ID3DBlob> DX12Backend::CompileShader(const std::string& path,
                                                             const char* entry,
                                                             const char* target) {
    auto src = ReadFileBytes(path);
    ComPtr<ID3DBlob> shader, errors;
    HRESULT hr = D3DCompile(src.data(), src.size(), path.c_str(),
                            nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                            entry, target, 0, 0,
                            &shader, &errors);
    if (FAILED(hr)) {
        std::string msg = "Shader compilation failed: " + path;
        if (errors) msg += "\n" + std::string(static_cast<char*>(errors->GetBufferPointer()),
                                              errors->GetBufferSize());
        std::cout << msg << std::endl;
        throw std::runtime_error(msg);
    }
    return shader;
}

// -----------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------

void DX12Backend::InitBackend() {
    frameCount_ = config_.framesInFlight;
    // FLIP swap chains require at least 2 buffers. Fluid forces framesInFlight=1
    // for the Vulkan submission model; bump the DX12 ring so CreateSwapChain works.
    if (config_.workload == Workload::Fluid && frameCount_ < 2)
        frameCount_ = 2;
    frameFenceValues_.resize(frameCount_, 0);
    renderTargets_.resize(frameCount_);
    commandAllocators_.resize(frameCount_);

    std::cout << "[DX12 Init] Creating device..." << std::endl;
    CreateDevice();
    std::cout << "[DX12 Init] Creating command queue..." << std::endl;
    CreateCommandQueue();
    if (!config_.headless) {
        std::cout << "[DX12 Init] Creating swap chain..." << std::endl;
        CreateSwapChain();
    }
    std::cout << "[DX12 Init] Creating descriptor heaps..." << std::endl;
    CreateDescriptorHeaps();
    if (!config_.headless) {
        std::cout << "[DX12 Init] Creating render targets..." << std::endl;
        CreateRenderTargets();
    }
    std::cout << "[DX12 Init] Creating command allocators..." << std::endl;
    CreateCommandAllocatorsAndList();
    std::cout << "[DX12 Init] Creating fence..." << std::endl;
    CreateFence();

    if (config_.workload == Workload::Fluid) {
        std::cout << "[DX12 Init] Creating fluid resources..." << std::endl;
        CreateFluidResources();
        std::cout << "[DX12 Init] Creating timestamp resources..." << std::endl;
        CreateTimestampResources();
        std::cout << "[DX12 Init] Initialisation complete (fluid)." << std::endl;
        return;
    }

    std::cout << "[DX12 Init] Creating root signatures..." << std::endl;
    CreateRootSignatures();
    std::cout << "[DX12 Init] Creating pipeline states (compiling shaders)..." << std::endl;
    CreatePipelineStates();
    if (linkedSfr_) {
        std::cout << "[DX12 Init] Creating linked-node SFR resources..." << std::endl;
        CreateSfrResources();
    }
    std::cout << "[DX12 Init] Creating particle buffer..." << std::endl;
    CreateParticleBuffer();
    std::cout << "[DX12 Init] Creating timestamp resources..." << std::endl;
    CreateTimestampResources();
    std::cout << "[DX12 Init] Initialisation complete." << std::endl;
}

void DX12Backend::CreateDevice() {
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory_)), "CreateDXGIFactory1 failed");

    // Sentinel -2: WARP software renderer requested from main.cpp.
    if (requestedGpuIndex_ == -2) {
        std::cout << "[DX12] Using Microsoft WARP software renderer (CPU)." << std::endl;
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory_->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)),
                      "EnumWarpAdapter failed");
        ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                        IID_PPV_ARGS(&device_)),
                      "D3D12CreateDevice (WARP) failed");
        deviceName_ = "Microsoft WARP (CPU Software Renderer)";
        driverVersion_ = "WARP";
        std::cout << "Selected GPU: " << deviceName_ << std::endl;
        return;
    }

    static const D3D_FEATURE_LEVEL kFeatureLevels[] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    static const char* kFeatureLevelNames[] = {
        "12_1", "12_0", "11_1", "11_0",
    };
    constexpr int kNumFeatureLevels = _countof(kFeatureLevels);

    // Enumerate adapters, deduplicating by LUID so that identical GPUs
    // with distinct LUIDs are listed separately.
    struct AdapterEntry {
        ComPtr<IDXGIAdapter1> adapter;
        DXGI_ADAPTER_DESC1 desc;
        UINT rawIndex;
        int featureLevelIdx = -1;
    };
    std::vector<AdapterEntry> uniqueAdapters;
    {
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; factory_->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);

            bool duplicate = false;
            for (const auto& existing : uniqueAdapters) {
                if (existing.desc.AdapterLuid.HighPart == desc.AdapterLuid.HighPart &&
                    existing.desc.AdapterLuid.LowPart  == desc.AdapterLuid.LowPart) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                uniqueAdapters.push_back({adapter, desc, i, -1});
            }
            adapter.Reset();
        }
    }

    std::cout << "Available GPUs:\n";
    for (std::uint32_t i = 0; i < uniqueAdapters.size(); ++i) {
        auto& entry = uniqueAdapters[i];
        bool isSoftware = (entry.desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
        char name[256]{};
        size_t converted = 0;
        wcstombs_s(&converted, name, sizeof(name), entry.desc.Description, _TRUNCATE);
        std::cout << "  [" << i << "] " << name
                  << (isSoftware ? " (Software)" : " (Hardware)")
                  << "  VRAM: " << (entry.desc.DedicatedVideoMemory / (1024 * 1024)) << " MB"
                  << std::endl;

        for (int fl = 0; fl < kNumFeatureLevels; ++fl) {
            HRESULT hr = D3D12CreateDevice(entry.adapter.Get(), kFeatureLevels[fl],
                                           __uuidof(ID3D12Device), nullptr);
            if (SUCCEEDED(hr)) {
                entry.featureLevelIdx = fl;
                std::cout << "    -> D3D12 supported (Feature Level " << kFeatureLevelNames[fl] << ")\n";
                break;
            }
        }
        if (entry.featureLevelIdx < 0) {
            std::cout << "    -> Skipped: D3D12 not supported at any feature level\n";
        }
    }

    // Collect D3D12-capable hardware adapters.
    std::vector<std::uint32_t> d3d12Indices;
    for (std::uint32_t i = 0; i < uniqueAdapters.size(); ++i) {
        if (uniqueAdapters[i].featureLevelIdx >= 0)
            d3d12Indices.push_back(i);
    }

    // Select adapter: --gpu flag, single-choice auto, or interactive prompt.
    std::uint32_t chosen = 0;
    bool hasChoice = false;

    // Try LUID-based selection first (reliable across factory instances).
    if (config_.adapterLuidHigh != 0 || config_.adapterLuidLow != 0) {
        for (std::uint32_t i = 0; i < uniqueAdapters.size(); ++i) {
            const auto& d = uniqueAdapters[i].desc;
            if (d.AdapterLuid.HighPart == static_cast<LONG>(config_.adapterLuidHigh) &&
                d.AdapterLuid.LowPart  == static_cast<DWORD>(config_.adapterLuidLow) &&
                uniqueAdapters[i].featureLevelIdx >= 0) {
                chosen = i;
                hasChoice = true;
                break;
            }
        }
        if (!hasChoice)
            throw std::runtime_error("Requested GPU (LUID) not found or does not support D3D12");
    } else if (requestedGpuIndex_ >= 0) {
        auto idx = static_cast<std::uint32_t>(requestedGpuIndex_);
        if (idx >= uniqueAdapters.size() || uniqueAdapters[idx].featureLevelIdx < 0)
            throw std::runtime_error("Requested GPU index " +
                std::to_string(requestedGpuIndex_) + " does not support D3D12");
        chosen = idx;
        hasChoice = true;
    } else if (d3d12Indices.size() == 1) {
        chosen = d3d12Indices[0];
        hasChoice = true;
    } else if (d3d12Indices.size() > 1) {
        SIZE_T bestMem = 0;
        for (auto i : d3d12Indices) {
            if (uniqueAdapters[i].desc.DedicatedVideoMemory > bestMem) {
                bestMem = uniqueAdapters[i].desc.DedicatedVideoMemory;
                chosen = i;
            }
        }
        std::cout << "Multiple D3D12 GPUs detected. Default: [" << chosen << "]\n"
                  << "Enter GPU index (or 'b' to go back): " << std::flush;
        std::string line;
        if (std::getline(std::cin, line) && !line.empty()) {
            if (line == "b" || line == "B")
                throw gpu_bench::BackToMenuException();
            auto idx = static_cast<std::uint32_t>(std::stoi(line));
            if (idx >= uniqueAdapters.size() || uniqueAdapters[idx].featureLevelIdx < 0)
                throw std::runtime_error("GPU index " + line + " does not support D3D12");
            chosen = idx;
        }
        hasChoice = true;
    }

    if (!hasChoice || d3d12Indices.empty()) {
        throw std::runtime_error(
            "No hardware GPU with D3D12 support was found.\n"
            "  Your GPU may only support D3D11. Try: --backend dx11");
    }

    {
        auto& entry = uniqueAdapters[chosen];
        D3D_FEATURE_LEVEL selectedFL = kFeatureLevels[entry.featureLevelIdx];
        HRESULT hr = D3D12CreateDevice(entry.adapter.Get(), selectedFL,
                                       IID_PPV_ARGS(&device_));
        if (FAILED(hr))
            throw std::runtime_error("D3D12CreateDevice failed for adapter ["
                + std::to_string(chosen) + "] at FL "
                + kFeatureLevelNames[entry.featureLevelIdx]
                + " (HRESULT " + HrToHex(hr) + ")");

        char name[256]{};
        size_t converted = 0;
        wcstombs_s(&converted, name, sizeof(name), entry.desc.Description, _TRUNCATE);
        deviceName_ = name;
        deviceName_ += " (FL ";
        deviceName_ += kFeatureLevelNames[entry.featureLevelIdx];
        deviceName_ += ")";

        // Query driver version via DXGI CheckInterfaceSupport
        LARGE_INTEGER umdVer{};
        ComPtr<IDXGIAdapter> baseAdapter;
        if (SUCCEEDED(entry.adapter.As(&baseAdapter)) &&
            SUCCEEDED(baseAdapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umdVer))) {
            auto v = static_cast<std::uint64_t>(umdVer.QuadPart);
            driverVersion_ = std::to_string((v >> 48) & 0xffff) + "."
                           + std::to_string((v >> 32) & 0xffff) + "."
                           + std::to_string((v >> 16) & 0xffff) + "."
                           + std::to_string(v & 0xffff);
        }
    }

    std::cout << "Selected GPU: " << deviceName_;
    if (!driverVersion_.empty())
        std::cout << "  |  Driver: " << driverVersion_;
    std::cout << std::endl;

    deviceNodeCount_ = device_->GetNodeCount();
    if (config_.multiGpuMode == MultiGpuMode::Off) {
        if (config_.adapterNodeIndex >= deviceNodeCount_ ||
            config_.adapterNodeIndex >= 32) {
            throw std::runtime_error(
                "Requested DX12 adapter node " +
                std::to_string(config_.adapterNodeIndex) +
                " is outside this linked adapter's " +
                std::to_string(deviceNodeCount_) + " node(s)");
        }
        singleNodeIndex_ = config_.adapterNodeIndex;
        // NodeMask 0 is the conventional single-node default.  A linked
        // adapter must use an explicit bit so node 1 is not silently mapped
        // back to node 0 by D3D12.
        singleNodeMask_ = deviceNodeCount_ > 1
            ? (1u << singleNodeIndex_)
            : 0u;
        if (deviceNodeCount_ > 1) {
            std::cout << "[DX12 Single-GPU] Linked adapter node "
                      << singleNodeIndex_ << "/" << (deviceNodeCount_ - 1)
                      << " selected; node mask 0x" << std::hex
                      << singleNodeMask_ << std::dec << "\n";
        }
    }

    if (config_.multiGpuMode != MultiGpuMode::Off) {
        const UINT nodeCount = deviceNodeCount_;
        if (nodeCount < 2)
            throw std::runtime_error(
                "DX12 multi-GPU mode requires a linked-adapter device with at least two nodes");
        linkedAfr_ = true;
        linkedSfr_ = config_.multiGpuMode == MultiGpuMode::Sfr;
        afrNodeCount_ = 2;
        frameNodeMasks_.resize(frameCount_);
        for (UINT i = 0; i < frameCount_; ++i) {
            frameNodeMasks_[i] = linkedSfr_ ? 1u : (1u << (i % afrNodeCount_));
        }
        std::cout << (linkedSfr_ ? "[DX12 SFR]" : "[DX12 AFR]")
                  << " Linked adapter accepted: " << nodeCount
                  << " nodes; using node masks 0x1/0x2\n";

        D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
        if (SUCCEEDED(device_->CheckFeatureSupport(
                D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)))) {
            crossNodeSharingTier_ = options.CrossNodeSharingTier;
            std::cout << "[DX12 Multi-GPU] Cross-node sharing: "
                      << CrossNodeSharingTierName(crossNodeSharingTier_)
                      << " (" << static_cast<unsigned>(crossNodeSharingTier_) << ")\n";
            if (crossNodeSharingTier_ ==
                D3D12_CROSS_NODE_SHARING_TIER_NOT_SUPPORTED) {
                std::cout << "[DX12 Multi-GPU] Warning: SFR composition cannot use "
                             "a cross-node resource copy on this device.\n";
            }
        } else {
            std::cout << "[DX12 Multi-GPU] Warning: cross-node sharing tier query failed; "
                         "SFR precondition is unverified.\n";
        }
    }
}

void DX12Backend::CreateCommandQueue() {
    const UINT queueCount = linkedAfr_ ? afrNodeCount_ : 1u;
    afrCommandQueues_.resize(queueCount);
    for (UINT node = 0; node < queueCount; ++node) {
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        qd.NodeMask = linkedAfr_ ? (1u << node) : SingleNodeMask();
        ThrowIfFailed(device_->CreateCommandQueue(
                          &qd, IID_PPV_ARGS(&afrCommandQueues_[node])),
                      "CreateCommandQueue failed");
    }
    commandQueue_ = afrCommandQueues_[0];
}

void DX12Backend::CreateSwapChain() {
    HWND hwnd = glfwGetWin32Window(window_);

    ComPtr<IDXGIFactory5> factory5;
    if (SUCCEEDED(factory_.As(&factory5))) {
        BOOL allow = FALSE;
        if (SUCCEEDED(factory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow))))
            tearingSupported_ = (allow == TRUE);
    }

    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.BufferCount      = frameCount_;
    sd.Width            = kWindowWidth;
    sd.Height           = kWindowHeight;
    sd.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.SampleDesc.Count = 1;
    if (tearingSupported_)
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    ComPtr<IDXGISwapChain1> sc1;
    ThrowIfFailed(factory_->CreateSwapChainForHwnd(
        commandQueue_.Get(), hwnd, &sd, nullptr, nullptr, &sc1),
        "CreateSwapChainForHwnd failed");
    factory_->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    ThrowIfFailed(sc1.As(&swapChain_), "QueryInterface IDXGISwapChain3 failed");
    if (linkedAfr_ || SingleNodeMask() != 0) {
        std::vector<IUnknown*> presentQueues(frameCount_);
        for (UINT i = 0; i < frameCount_; ++i) {
            presentQueues[i] = linkedAfr_
                ? (linkedSfr_ ? afrCommandQueues_[0].Get()
                              : afrCommandQueues_[i % afrNodeCount_].Get())
                : commandQueue_.Get();
        }
        std::vector<UINT> bufferNodeMasks;
        const UINT* nodeMasks = frameNodeMasks_.data();
        if (!linkedAfr_) {
            bufferNodeMasks.assign(frameCount_, SingleNodeMask());
            nodeMasks = bufferNodeMasks.data();
        }
        ThrowIfFailed(swapChain_->ResizeBuffers1(
                          frameCount_, kWindowWidth, kWindowHeight,
                          DXGI_FORMAT_R8G8B8A8_UNORM, sd.Flags,
                          nodeMasks, presentQueues.data()),
                      "ResizeBuffers1 (linked-adapter placement) failed");
    }
    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
}

void DX12Backend::CreateDescriptorHeaps() {
    if (!config_.headless) {
        const UINT heapCount = linkedAfr_ ? afrNodeCount_ : 1u;
        afrRtvHeaps_.resize(heapCount);
        for (UINT node = 0; node < heapCount; ++node) {
            D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
            rtvDesc.NumDescriptors = frameCount_;
            rtvDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvDesc.NodeMask       = linkedAfr_ ? (1u << node) : SingleNodeMask();
            ThrowIfFailed(device_->CreateDescriptorHeap(
                              &rtvDesc, IID_PPV_ARGS(&afrRtvHeaps_[node])),
                          "CreateDescriptorHeap RTV failed");
        }
        rtvHeap_ = afrRtvHeaps_[0];
        rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    D3D12_DESCRIPTOR_HEAP_DESC uavDesc{};
    uavDesc.NumDescriptors = 1;
    uavDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    uavDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    uavDesc.NodeMask       = SingleNodeMask();
    ThrowIfFailed(device_->CreateDescriptorHeap(&uavDesc, IID_PPV_ARGS(&cbvSrvUavHeap_)),
                  "CreateDescriptorHeap CBV/SRV/UAV failed");

    if (config_.workload == Workload::Render3D && !config_.headless) {
        D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{};
        dsvDesc.NumDescriptors = 1;
        dsvDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvDesc.NodeMask       = SingleNodeMask();
        ThrowIfFailed(device_->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&dsvHeap_)),
                      "CreateDescriptorHeap DSV failed");
    }
}

void DX12Backend::CreateRenderTargets() {
    for (UINT i = 0; i < frameCount_; ++i) {
        ThrowIfFailed(swapChain_->GetBuffer(i, IID_PPV_ARGS(&renderTargets_[i])),
                      "GetBuffer failed");
        const UINT node = linkedSfr_ ? 0u : (linkedAfr_ ? (i % afrNodeCount_) : 0u);
        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            afrRtvHeaps_[node]->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(i) * rtvDescriptorSize_;
        device_->CreateRenderTargetView(renderTargets_[i].Get(), nullptr, handle);
    }

    if (config_.workload == Workload::Render3D) {
        D3D12_HEAP_PROPERTIES dp{}; dp.Type = D3D12_HEAP_TYPE_DEFAULT;
        ApplySingleNodeHeapMasks(dp);
        D3D12_RESOURCE_DESC rd{};
        rd.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        rd.Width            = kWindowWidth;
        rd.Height           = kWindowHeight;
        rd.DepthOrArraySize = 1;
        rd.MipLevels        = 1;
        rd.Format           = DXGI_FORMAT_D32_FLOAT;
        rd.SampleDesc.Count = 1;
        rd.Flags            = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        D3D12_CLEAR_VALUE cv{}; cv.Format = DXGI_FORMAT_D32_FLOAT; cv.DepthStencil.Depth = 1.0f;
        ThrowIfFailed(device_->CreateCommittedResource(
            &dp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv,
            IID_PPV_ARGS(&depthBuffer_)), "CreateCommittedResource (depth) failed");
        device_->CreateDepthStencilView(depthBuffer_.Get(), nullptr,
            dsvHeap_->GetCPUDescriptorHandleForHeapStart());
    }
}

void DX12Backend::CreateCommandAllocatorsAndList() {
    frameCommandLists_.resize(frameCount_);
    for (UINT i = 0; i < frameCount_; ++i) {
        ThrowIfFailed(device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators_[i])),
            "CreateCommandAllocator failed");
        const UINT nodeMask = linkedAfr_ ? frameNodeMasks_[i] : SingleNodeMask();
        ThrowIfFailed(device_->CreateCommandList(
            nodeMask, D3D12_COMMAND_LIST_TYPE_DIRECT,
            commandAllocators_[i].Get(), nullptr,
            IID_PPV_ARGS(&frameCommandLists_[i])),
            "CreateCommandList failed");
        frameCommandLists_[i]->Close();
    }
    commandList_ = frameCommandLists_[0];
}

// -----------------------------------------------------------------------
// Root signatures & PSOs
// -----------------------------------------------------------------------

void DX12Backend::CreateRootSignatures() {
    // Compute root signature: UAV table (u0) + 32-bit constants (b0, 2 floats)
    {
        D3D12_DESCRIPTOR_RANGE uavRange{};
        uavRange.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRange.NumDescriptors     = 1;
        uavRange.BaseShaderRegister = 0;

        D3D12_ROOT_PARAMETER params[2]{};
        params[0].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[0].DescriptorTable.NumDescriptorRanges = 1;
        params[0].DescriptorTable.pDescriptorRanges   = &uavRange;
        params[0].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;

        params[1].ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[1].Constants.ShaderRegister = 0;
        params[1].Constants.Num32BitValues = 3;  // max(ComputeParams=2, NBodyParams=3)
        params[1].ShaderVisibility         = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = 2;
        desc.pParameters   = params;

        ComPtr<ID3DBlob> sig, err;
        ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                                  &sig, &err),
                      "Serialize compute root signature failed");
        ThrowIfFailed(device_->CreateRootSignature(SingleNodeMask(), sig->GetBufferPointer(),
                                                   sig->GetBufferSize(),
                                                   IID_PPV_ARGS(&computeRootSig_)),
                      "CreateRootSignature (compute) failed");
    }

    // Graphics root signature: empty (particles) or PS root constants (fractal)
    if (!config_.headless) {
        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        D3D12_ROOT_PARAMETER fparam{};
        // GPU Stress v1 has a dedicated 16-byte parameter block. Legacy
        // Fractal/Volumetric retain their original 3-constant signatures.
        if (config_.workload == Workload::GpuStressV1 ||
            isGpuBurnWorkload(config_.workload)) {
            fparam.ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            fparam.Constants.ShaderRegister = 0;
            fparam.Constants.Num32BitValues = 4;
            fparam.ShaderVisibility         = D3D12_SHADER_VISIBILITY_PIXEL;
            desc.NumParameters = 1;
            desc.pParameters   = &fparam;
        } else if (config_.workload == Workload::StressFractal
                   || config_.workload == Workload::Volumetric) {
            fparam.ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            fparam.Constants.ShaderRegister = 0;
            fparam.Constants.Num32BitValues = 3;
            fparam.ShaderVisibility         = D3D12_SHADER_VISIBILITY_PIXEL;
            desc.NumParameters = 1;
            desc.pParameters   = &fparam;
        } else if (config_.workload == Workload::Render3D) {
            fparam.ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            fparam.Constants.ShaderRegister = 0;
            fparam.Constants.Num32BitValues = sizeof(Render3DParams) / 4;  // 28 DWORDs
            fparam.ShaderVisibility         = D3D12_SHADER_VISIBILITY_VERTEX;
            desc.NumParameters = 1;
            desc.pParameters   = &fparam;
        }

        ComPtr<ID3DBlob> sig, err;
        ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                                  &sig, &err),
                      "Serialize graphics root signature failed");
        const UINT rootCount = linkedAfr_ ? afrNodeCount_ : 1u;
        afrGraphicsRootSigs_.resize(rootCount);
        for (UINT node = 0; node < rootCount; ++node) {
            const UINT nodeMask = linkedAfr_ ? (1u << node) : SingleNodeMask();
            ThrowIfFailed(device_->CreateRootSignature(
                              nodeMask, sig->GetBufferPointer(), sig->GetBufferSize(),
                              IID_PPV_ARGS(&afrGraphicsRootSigs_[node])),
                          "CreateRootSignature (graphics) failed");
        }
        graphicsRootSig_ = afrGraphicsRootSigs_[0];
    }
}

void DX12Backend::CreatePipelineStates() {
    const bool fp16Peak = (config_.workload == Workload::SynthPeak
                           && config_.peakPrecision == Precision::FP16);
    if (fp16Peak) {
        // FXC has no true float16_t; load the precompiled signed DXIL (SM 6.2,
        // -enable-16bit-types) and verify the device supports native 16-bit ops.
        D3D12_FEATURE_DATA_SHADER_MODEL sm{ D3D_SHADER_MODEL_6_2 };
        bool sm62 = SUCCEEDED(device_->CheckFeatureSupport(
            D3D12_FEATURE_SHADER_MODEL, &sm, sizeof(sm)))
            && sm.HighestShaderModel >= D3D_SHADER_MODEL_6_2;
        D3D12_FEATURE_DATA_D3D12_OPTIONS4 o4{};
        device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS4, &o4, sizeof(o4));
        if (!sm62 || !o4.Native16BitShaderOpsSupported)
            throw std::runtime_error("DX12 FP16 SynthPeak needs Shader Model 6.2 + Native16BitShaderOps");

        std::vector<char> cso;
        try { cso = ReadFileBytes(shaderDir_ + "synthpeak_fp16.cso"); }
        catch (...) {
            throw std::runtime_error("synthpeak_fp16.cso missing (Windows SDK dxc.exe not found at build time)");
        }
        D3D12_COMPUTE_PIPELINE_STATE_DESC d{};
        d.pRootSignature = computeRootSig_.Get();
        d.CS = { cso.data(), cso.size() };
        d.NodeMask = SingleNodeMask();
        ThrowIfFailed(device_->CreateComputePipelineState(&d, IID_PPV_ARGS(&computePSO_)),
                      "CreateComputePipelineState (fp16) failed");
    } else {
        std::string csFile;
        if (config_.workload == Workload::SynthPeak) {
            csFile = (config_.peakPrecision == Precision::FP64)  ? "synthpeak_fp64.hlsl"
                   : (config_.peakPrecision == Precision::INT32) ? "synthpeak_int32.hlsl"
                   :                                               "synthpeak_fp32.hlsl";
        } else {
            csFile = (config_.workload == Workload::NBody) ? "nbody.hlsl" : "compute.hlsl";
        }
        auto csBlob = CompileShader(shaderDir_ + csFile, "CSMain", "cs_5_1");

        D3D12_COMPUTE_PIPELINE_STATE_DESC d{};
        d.pRootSignature = computeRootSig_.Get();
        d.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
        d.NodeMask = SingleNodeMask();
        ThrowIfFailed(device_->CreateComputePipelineState(&d, IID_PPV_ARGS(&computePSO_)),
                      "CreateComputePipelineState failed");
    }

    if (config_.headless) return;

    const bool fractal   = (config_.workload == Workload::StressFractal);
    const bool gpuStress = (config_.workload == Workload::GpuStressV1);
    const bool gpuBurn   = isGpuBurnWorkload(config_.workload);
    const bool volumetric= (config_.workload == Workload::Volumetric);
    const bool render3d  = (config_.workload == Workload::Render3D);
    // Fractal and Volumetric both use a single HLSL file with VSMain + PSMain
    // and rely on SV_VertexID (no vertex buffer / input layout).
    const char* burnFile = "gpu_burn.hlsl";
    const char* gvs = gpuBurn ? burnFile
                    : gpuStress ? "gpu_stress.hlsl"
                    : (fractal || volumetric) ? (fractal ? "fractal.hlsl" : "volumetric.hlsl")
                    : render3d ? "render3d.hlsl" : "particle_vs.hlsl";
    const char* gps = gpuBurn ? burnFile
                    : gpuStress ? "gpu_stress.hlsl"
                    : (fractal || volumetric) ? (fractal ? "fractal.hlsl" : "volumetric.hlsl")
                    : render3d ? "render3d.hlsl" : "particle_ps.hlsl";
    auto vsBlob = CompileShader(shaderDir_ + gvs, "VSMain", "vs_5_1");
    auto psBlob = CompileShader(shaderDir_ + gps, "PSMain", "ps_5_1");

    // Graphics PSO
    {
        D3D12_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "VELOCITY", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };
        // Render3D: slot 0 corner (per-vertex), slot 1 pos/vel (per-instance).
        D3D12_INPUT_ELEMENT_DESC layout3d[] = {
            { "CORNER",   0, DXGI_FORMAT_R32G32_FLOAT,       0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 },
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "VELOCITY", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
        if (render3d)             d.InputLayout = { layout3d, _countof(layout3d) };
        else if (!fractal && !gpuStress && !gpuBurn && !volumetric) d.InputLayout = { layout, _countof(layout) };  // fullscreen workloads use SV_VertexID
        d.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        d.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

        d.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
        d.RasterizerState.CullMode              = D3D12_CULL_MODE_NONE;
        d.RasterizerState.DepthClipEnable       = TRUE;
        d.RasterizerState.FrontCounterClockwise = FALSE;

        // Fractal/Volumetric write opaque colour; particle/render3d use alpha blend.
        const bool opaque = (fractal || gpuStress || gpuBurn || volumetric);
        d.BlendState.RenderTarget[0].BlendEnable           = opaque ? FALSE : TRUE;
        d.BlendState.RenderTarget[0].SrcBlend               = D3D12_BLEND_SRC_ALPHA;
        d.BlendState.RenderTarget[0].DestBlend              = D3D12_BLEND_INV_SRC_ALPHA;
        d.BlendState.RenderTarget[0].BlendOp                = D3D12_BLEND_OP_ADD;
        d.BlendState.RenderTarget[0].SrcBlendAlpha          = D3D12_BLEND_ONE;
        d.BlendState.RenderTarget[0].DestBlendAlpha         = D3D12_BLEND_ZERO;
        d.BlendState.RenderTarget[0].BlendOpAlpha           = D3D12_BLEND_OP_ADD;
        d.BlendState.RenderTarget[0].RenderTargetWriteMask  = D3D12_COLOR_WRITE_ENABLE_ALL;

        if (render3d) {
            d.DepthStencilState.DepthEnable    = TRUE;
            d.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            d.DepthStencilState.DepthFunc      = D3D12_COMPARISON_FUNC_LESS;
            d.DSVFormat                        = DXGI_FORMAT_D32_FLOAT;
        }

        d.SampleMask            = UINT_MAX;
        d.PrimitiveTopologyType = (fractal || gpuStress || gpuBurn || volumetric || render3d) ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE
                                                                       : D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        d.NumRenderTargets      = 1;
        d.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.SampleDesc.Count      = 1;

        const UINT psoCount = linkedAfr_ ? afrNodeCount_ : 1u;
        afrGraphicsPSOs_.resize(psoCount);
        for (UINT node = 0; node < psoCount; ++node) {
            d.pRootSignature = afrGraphicsRootSigs_[node].Get();
            d.NodeMask = linkedAfr_ ? (1u << node) : SingleNodeMask();
            ThrowIfFailed(device_->CreateGraphicsPipelineState(
                              &d, IID_PPV_ARGS(&afrGraphicsPSOs_[node])),
                          "CreateGraphicsPipelineState failed");
        }
        graphicsPSO_ = afrGraphicsPSOs_[0];
    }
}

// -----------------------------------------------------------------------
// Resources
// -----------------------------------------------------------------------

void DX12Backend::CreateParticleBuffer() {
    const UINT bufferSize = sizeof(Particle) * config_.particleCount;

    // Default heap buffer (GPU-only, UAV-capable)
    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    ApplySingleNodeHeapMasks(defaultHeap);

    D3D12_RESOURCE_DESC rd{};
    rd.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width              = bufferSize;
    rd.Height             = 1;
    rd.DepthOrArraySize   = 1;
    rd.MipLevels          = 1;
    rd.SampleDesc.Count   = 1;
    rd.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    rd.Flags              = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    ThrowIfFailed(device_->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_PPV_ARGS(&particleBuffer_)),
        "CreateCommittedResource (particle) failed");

    // Upload heap for initial data
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    ApplySingleNodeHeapMasks(uploadHeap);
    rd.Flags = D3D12_RESOURCE_FLAG_NONE;

    ThrowIfFailed(device_->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &rd,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&particleUpload_)),
        "CreateCommittedResource (upload) failed");

    // Copy initial particle data
    void* mapped = nullptr;
    particleUpload_->Map(0, nullptr, &mapped);
    std::memcpy(mapped, initialParticles_.data(), bufferSize);
    particleUpload_->Unmap(0, nullptr);

    // Execute copy on the GPU
    commandAllocators_[0]->Reset();
    commandList_->Reset(commandAllocators_[0].Get(), nullptr);

    commandList_->CopyBufferRegion(particleBuffer_.Get(), 0,
                                   particleUpload_.Get(), 0, bufferSize);

    D3D12_RESOURCE_BARRIER b{};
    b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource   = particleBuffer_.Get();
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    b.Transition.StateAfter  = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &b);

    commandList_->Close();
    ID3D12CommandList* lists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, lists);
    WaitForGpu();

    // Create UAV descriptor for compute
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.ViewDimension              = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements         = config_.particleCount;
    uavDesc.Buffer.StructureByteStride = sizeof(Particle);
    device_->CreateUnorderedAccessView(particleBuffer_.Get(), nullptr, &uavDesc,
                                       cbvSrvUavHeap_->GetCPUDescriptorHandleForHeapStart());

    // Vertex buffer view
    vbView_.BufferLocation = particleBuffer_->GetGPUVirtualAddress();
    vbView_.SizeInBytes    = bufferSize;
    vbView_.StrideInBytes  = sizeof(Particle);

    // Render3D: static quad in an upload-heap buffer (tiny, CPU-written once).
    if (config_.workload == Workload::Render3D && !config_.headless) {
        const float quad[12] = { -1,-1, 1,-1, 1,1, -1,-1, 1,1, -1,1 };
        D3D12_HEAP_PROPERTIES uh{}; uh.Type = D3D12_HEAP_TYPE_UPLOAD;
        ApplySingleNodeHeapMasks(uh);
        D3D12_RESOURCE_DESC qd{};
        qd.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        qd.Width            = sizeof(quad);
        qd.Height           = 1; qd.DepthOrArraySize = 1; qd.MipLevels = 1;
        qd.SampleDesc.Count = 1;
        qd.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ThrowIfFailed(device_->CreateCommittedResource(
            &uh, D3D12_HEAP_FLAG_NONE, &qd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&quadBuffer_)), "CreateCommittedResource (quad) failed");
        void* qm = nullptr;
        quadBuffer_->Map(0, nullptr, &qm);
        std::memcpy(qm, quad, sizeof(quad));
        quadBuffer_->Unmap(0, nullptr);
        quadView_.BufferLocation = quadBuffer_->GetGPUVirtualAddress();
        quadView_.SizeInBytes    = sizeof(quad);
        quadView_.StrideInBytes  = sizeof(float) * 2;
    }

    std::cout << "Created particle buffer: " << config_.particleCount << " particles\n";
}

void DX12Backend::CreateSfrResources() {
    if (crossNodeSharingTier_ == D3D12_CROSS_NODE_SHARING_TIER_NOT_SUPPORTED) {
        throw std::runtime_error(
            "DX12 SFR requires cross-node copy support; this linked adapter reports none");
    }

    const UINT halfWidth = kWindowWidth / 2;
    sfrSecondaryTargets_.resize(frameCount_);
    sfrCrossAdapterHeaps_.resize(frameCount_);
    sfrCrossAdapterBuffers_.resize(frameCount_);
    sfrCrossFootprints_.resize(frameCount_);
    sfrSecondaryAllocators_.resize(frameCount_);
    sfrSecondaryCommandLists_.resize(frameCount_);
    sfrComposeAllocators_.resize(frameCount_);
    sfrComposeCommandLists_.resize(frameCount_);

    D3D12_RESOURCE_DESC secondaryDesc{};
    secondaryDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    secondaryDesc.Width = kWindowWidth;
    secondaryDesc.Height = kWindowHeight;
    secondaryDesc.DepthOrArraySize = 1;
    secondaryDesc.MipLevels = 1;
    secondaryDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    secondaryDesc.SampleDesc.Count = 1;
    secondaryDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    secondaryDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_RESOURCE_DESC halfDesc = secondaryDesc;
    halfDesc.Width = halfWidth;
    halfDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT numRows = 0;
    UINT64 rowSize = 0;
    UINT64 totalBytes = 0;
    device_->GetCopyableFootprints(
        &halfDesc, 0, 1, 0, &footprint, &numRows, &rowSize, &totalBytes);
    if (totalBytes == 0)
        throw std::runtime_error("DX12 SFR failed to obtain a half-frame copy footprint");

    D3D12_CLEAR_VALUE clear{};
    clear.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clear.Color[0] = 0.04f;
    clear.Color[1] = 0.08f;
    clear.Color[2] = 0.14f;
    clear.Color[3] = 1.0f;

    D3D12_HEAP_PROPERTIES node1Heap{};
    node1Heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    node1Heap.CreationNodeMask = 2;
    node1Heap.VisibleNodeMask = 2;

    D3D12_HEAP_PROPERTIES crossHeap{};
    crossHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    crossHeap.CreationNodeMask = 1;
    crossHeap.VisibleNodeMask = 3;

    D3D12_RESOURCE_DESC crossBufferDesc{};
    crossBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    crossBufferDesc.Width = totalBytes;
    crossBufferDesc.Height = 1;
    crossBufferDesc.DepthOrArraySize = 1;
    crossBufferDesc.MipLevels = 1;
    crossBufferDesc.SampleDesc.Count = 1;
    crossBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    crossBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;

    const D3D12_RESOURCE_ALLOCATION_INFO crossAllocation =
        device_->GetResourceAllocationInfo(3, 1, &crossBufferDesc);
    if (crossAllocation.SizeInBytes == 0 ||
        crossAllocation.SizeInBytes == UINT64_MAX) {
        throw std::runtime_error("DX12 SFR cross-adapter allocation is unsupported");
    }

    for (UINT i = 0; i < frameCount_; ++i) {
        ThrowIfFailed(device_->CreateCommittedResource(
            &node1Heap, D3D12_HEAP_FLAG_NONE, &secondaryDesc,
            D3D12_RESOURCE_STATE_RENDER_TARGET, &clear,
            IID_PPV_ARGS(&sfrSecondaryTargets_[i])),
            "CreateCommittedResource (SFR node-1 target) failed");

        D3D12_CPU_DESCRIPTOR_HANDLE rtv =
            afrRtvHeaps_[1]->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += static_cast<SIZE_T>(i) * rtvDescriptorSize_;
        device_->CreateRenderTargetView(sfrSecondaryTargets_[i].Get(), nullptr, rtv);

        D3D12_HEAP_DESC crossHeapDesc{};
        crossHeapDesc.SizeInBytes = crossAllocation.SizeInBytes;
        crossHeapDesc.Properties = crossHeap;
        crossHeapDesc.Alignment = crossAllocation.Alignment;
        // Cross-adapter resources are placed in an explicit shared heap; old
        // WDDM/GCN drivers reject the equivalent committed-resource shortcut.
        crossHeapDesc.Flags = static_cast<D3D12_HEAP_FLAGS>(
            D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER);
        ThrowIfFailed(device_->CreateHeap(
            &crossHeapDesc, IID_PPV_ARGS(&sfrCrossAdapterHeaps_[i])),
            "CreateHeap (SFR cross-adapter) failed");
        ThrowIfFailed(device_->CreatePlacedResource(
            sfrCrossAdapterHeaps_[i].Get(), 0, &crossBufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&sfrCrossAdapterBuffers_[i])),
            "CreatePlacedResource (SFR cross-adapter buffer) failed");
        sfrCrossFootprints_[i] = footprint;

        ThrowIfFailed(device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&sfrSecondaryAllocators_[i])),
            "CreateCommandAllocator (SFR secondary) failed");
        ThrowIfFailed(device_->CreateCommandList(
            2, D3D12_COMMAND_LIST_TYPE_DIRECT,
            sfrSecondaryAllocators_[i].Get(), nullptr,
            IID_PPV_ARGS(&sfrSecondaryCommandLists_[i])),
            "CreateCommandList (SFR secondary) failed");
        ThrowIfFailed(sfrSecondaryCommandLists_[i]->Close(),
                      "CloseCommandList (SFR secondary) failed");

        ThrowIfFailed(device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&sfrComposeAllocators_[i])),
            "CreateCommandAllocator (SFR compose) failed");
        ThrowIfFailed(device_->CreateCommandList(
            1, D3D12_COMMAND_LIST_TYPE_DIRECT,
            sfrComposeAllocators_[i].Get(), nullptr,
            IID_PPV_ARGS(&sfrComposeCommandLists_[i])),
            "CreateCommandList (SFR compose) failed");
        ThrowIfFailed(sfrComposeCommandLists_[i]->Close(),
                      "CloseCommandList (SFR compose) failed");
    }

    std::cout << "[DX12 SFR] Resources ready: 50/50 vertical split, "
              << totalBytes << "-byte cross-node buffer per flight\n";
}

void DX12Backend::CreateTimestampResources() {
    const UINT nodeCount = linkedAfr_ ? afrNodeCount_ : 1u;
    afrTimestampHeaps_.resize(nodeCount);
    afrTimestampReadbacks_.resize(nodeCount);
    afrGpuFrequencies_.resize(nodeCount, 0);

    D3D12_RESOURCE_DESC rd{};
    rd.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width            = sizeof(UINT64) * kTimestampsPerFrame * frameCount_;
    rd.Height           = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels        = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    for (UINT node = 0; node < nodeCount; ++node) {
        auto* queue = afrCommandQueues_[node].Get();
        if (FAILED(queue->GetTimestampFrequency(&afrGpuFrequencies_[node])) ||
            afrGpuFrequencies_[node] == 0) {
            std::cout << "[Profiling] Node " << node
                      << " timestamps unsupported -- disabled.\n";
            return;
        }

        D3D12_QUERY_HEAP_DESC qhd{};
        qhd.Type  = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        qhd.Count = kTimestampsPerFrame * frameCount_;
        qhd.NodeMask = linkedAfr_ ? (1u << node) : SingleNodeMask();
        if (FAILED(device_->CreateQueryHeap(
                &qhd, IID_PPV_ARGS(&afrTimestampHeaps_[node])))) {
            std::cout << "[Profiling] Failed to create node query heap -- disabled.\n";
            return;
        }

        D3D12_HEAP_PROPERTIES readbackHeap{};
        readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
        const UINT readbackNodeMask = linkedAfr_
            ? (1u << node)
            : (SingleNodeMask() != 0 ? SingleNodeMask() : 1u);
        readbackHeap.CreationNodeMask = readbackNodeMask;
        readbackHeap.VisibleNodeMask = readbackNodeMask;
        if (FAILED(device_->CreateCommittedResource(
                &readbackHeap, D3D12_HEAP_FLAG_NONE, &rd,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                IID_PPV_ARGS(&afrTimestampReadbacks_[node])))) {
            std::cout << "[Profiling] Failed to create node readback buffer -- disabled.\n";
            return;
        }
    }

    timestampHeap_ = afrTimestampHeaps_[0];
    timestampReadback_ = afrTimestampReadbacks_[0];
    gpuFrequency_ = afrGpuFrequencies_[0];
    timestampsSupported_ = true;
    std::cout << "[Profiling] DX12 timestamp queries enabled on " << nodeCount
              << " node(s)\n";
}

void DX12Backend::CreateFence() {
    const UINT nodeCount = linkedAfr_ ? afrNodeCount_ : 1u;
    afrFences_.resize(nodeCount);
    afrFenceEvents_.resize(nodeCount, nullptr);
    afrNextFenceValues_.resize(nodeCount, 1);
    for (UINT node = 0; node < nodeCount; ++node) {
        ThrowIfFailed(device_->CreateFence(
                          0, D3D12_FENCE_FLAG_NONE,
                          IID_PPV_ARGS(&afrFences_[node])),
                      "CreateFence failed");
        afrFenceEvents_[node] = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!afrFenceEvents_[node])
            throw std::runtime_error("CreateEvent failed");
    }
    fence_ = afrFences_[0];
    fenceEvent_ = afrFenceEvents_[0];
}

void DX12Backend::WaitForGpu() {
    for (UINT node = 0; node < afrCommandQueues_.size(); ++node) {
        auto& queue = afrCommandQueues_[node];
        if (!queue) continue;
        const UINT64 value = afrNextFenceValues_[node]++;
        queue->Signal(afrFences_[node].Get(), value);
        afrFences_[node]->SetEventOnCompletion(value, afrFenceEvents_[node]);
        WaitForSingleObjectEx(afrFenceEvents_[node], INFINITE, FALSE);
    }
}

void DX12Backend::WaitIdle() {
    WaitForGpu();
}

void DX12Backend::CollectTimestampResults() {
    if (!timestampsSupported_) return;

    const UINT node = linkedSfr_ ? 0u : (linkedAfr_ ? (frameIndex_ % afrNodeCount_) : 0u);
    auto& readback = afrTimestampReadbacks_[node];
    const UINT64 frequency = afrGpuFrequencies_[node];

    const UINT base = frameIndex_ * kTimestampsPerFrame;
    const UINT64 byteOffset = base * sizeof(UINT64);

    D3D12_RANGE readRange{ byteOffset, byteOffset + kTimestampsPerFrame * sizeof(UINT64) };
    void* mapped = nullptr;
    if (FAILED(readback->Map(0, &readRange, &mapped))) return;

    auto* ts = reinterpret_cast<UINT64*>(static_cast<char*>(mapped) + byteOffset);

    if (ts[0] != 0 && ts[3] != 0) {
        double toMs = 1000.0 / static_cast<double>(frequency);
        AccumulateTiming(
            static_cast<double>(ts[1] - ts[0]) * toMs,
            static_cast<double>(ts[3] - ts[2]) * toMs,
            static_cast<double>(ts[3] - ts[0]) * toMs);
    }

    D3D12_RANGE writeRange{ 0, 0 };
    readback->Unmap(0, &writeRange);
}

// -----------------------------------------------------------------------
// Frame
// -----------------------------------------------------------------------

void DX12Backend::DrawSfrFrame(float deltaTime) {
    const UINT slot = frameIndex_;
    auto& primaryFence = afrFences_[0];
    if (primaryFence->GetCompletedValue() < frameFenceValues_[slot]) {
        primaryFence->SetEventOnCompletion(
            frameFenceValues_[slot], afrFenceEvents_[0]);
        WaitForSingleObjectEx(afrFenceEvents_[0], INFINITE, FALSE);
    }

    CollectTimestampResults();

    auto& primaryAlloc = commandAllocators_[slot];
    auto& primaryList = frameCommandLists_[slot];
    auto& secondaryAlloc = sfrSecondaryAllocators_[slot];
    auto& secondaryList = sfrSecondaryCommandLists_[slot];
    auto& composeAlloc = sfrComposeAllocators_[slot];
    auto& composeList = sfrComposeCommandLists_[slot];

    ThrowIfFailed(primaryAlloc->Reset(), "Reset (SFR primary allocator) failed");
    ThrowIfFailed(primaryList->Reset(primaryAlloc.Get(), nullptr),
                  "Reset (SFR primary list) failed");
    ThrowIfFailed(secondaryAlloc->Reset(), "Reset (SFR secondary allocator) failed");
    ThrowIfFailed(secondaryList->Reset(secondaryAlloc.Get(), nullptr),
                  "Reset (SFR secondary list) failed");

    const UINT halfWidth = kWindowWidth / 2;
    const UINT tsBase = slot * kTimestampsPerFrame;
    fractalElapsed_ += deltaTime;

    auto recordHalf = [&](ID3D12GraphicsCommandList* list,
                          UINT node, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                          LONG left, LONG right) {
        const float clearColor[] = { 0.04f, 0.08f, 0.14f, 1.0f };
        list->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
        list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

        D3D12_VIEWPORT vp{ 0, 0, static_cast<float>(kWindowWidth),
                           static_cast<float>(kWindowHeight), 0.0f, 1.0f };
        D3D12_RECT sc{ left, 0, right, static_cast<LONG>(kWindowHeight) };
        list->RSSetViewports(1, &vp);
        list->RSSetScissorRects(1, &sc);
        list->SetGraphicsRootSignature(afrGraphicsRootSigs_[node].Get());
        list->SetPipelineState(afrGraphicsPSOs_[node].Get());
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        for (std::uint32_t pass = 0; pass < gpuBurnDrawsPerFrame(config_.workload); ++pass) {
            GpuBurnParams bp{ fractalElapsed_, static_cast<float>(pass),
                              config_.gpuBurnIter,
                              gpuBurnShaderVersion(config_.workload) };
            list->SetGraphicsRoot32BitConstants(0, 4, &bp, 0);
            list->DrawInstanced(3, 1, 0, 0);
        }
    };

    // Node 0 shades the left half directly into the presentation buffer.
    D3D12_RESOURCE_BARRIER primaryBarrier{};
    primaryBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    primaryBarrier.Transition.pResource = renderTargets_[slot].Get();
    primaryBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    primaryBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    primaryBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    primaryList->ResourceBarrier(1, &primaryBarrier);

    if (timestampsSupported_) {
        primaryList->EndQuery(afrTimestampHeaps_[0].Get(),
                              D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 0);
        primaryList->EndQuery(afrTimestampHeaps_[0].Get(),
                              D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 1);
        primaryList->EndQuery(afrTimestampHeaps_[0].Get(),
                              D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 2);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE primaryRtv =
        afrRtvHeaps_[0]->GetCPUDescriptorHandleForHeapStart();
    primaryRtv.ptr += static_cast<SIZE_T>(slot) * rtvDescriptorSize_;
    recordHalf(primaryList.Get(), 0, primaryRtv, 0, halfWidth);

    primaryBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    primaryBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    primaryList->ResourceBarrier(1, &primaryBarrier);
    ThrowIfFailed(primaryList->Close(), "Close (SFR primary list) failed");

    // Node 1 shades only the right half, retaining a full-size viewport so the
    // existing shader sees exactly the same UV and SV_Position values.
    D3D12_CPU_DESCRIPTOR_HANDLE secondaryRtv =
        afrRtvHeaps_[1]->GetCPUDescriptorHandleForHeapStart();
    secondaryRtv.ptr += static_cast<SIZE_T>(slot) * rtvDescriptorSize_;
    recordHalf(secondaryList.Get(), 1, secondaryRtv, halfWidth, kWindowWidth);

    D3D12_RESOURCE_BARRIER secondaryBarrier{};
    secondaryBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    secondaryBarrier.Transition.pResource = sfrSecondaryTargets_[slot].Get();
    secondaryBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    secondaryBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    secondaryBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    secondaryList->ResourceBarrier(1, &secondaryBarrier);

    D3D12_TEXTURE_COPY_LOCATION crossDst{};
    crossDst.pResource = sfrCrossAdapterBuffers_[slot].Get();
    crossDst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    crossDst.PlacedFootprint = sfrCrossFootprints_[slot];
    D3D12_TEXTURE_COPY_LOCATION secondarySrc{};
    secondarySrc.pResource = sfrSecondaryTargets_[slot].Get();
    secondarySrc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    secondarySrc.SubresourceIndex = 0;
    D3D12_BOX rightHalf{ halfWidth, 0, 0, kWindowWidth, kWindowHeight, 1 };
    secondaryList->CopyTextureRegion(
        &crossDst, 0, 0, 0, &secondarySrc, &rightHalf);

    std::swap(secondaryBarrier.Transition.StateBefore,
              secondaryBarrier.Transition.StateAfter);
    secondaryList->ResourceBarrier(1, &secondaryBarrier);
    ThrowIfFailed(secondaryList->Close(), "Close (SFR secondary list) failed");

    // Queue both halves before inserting the primary queue's cross-node wait,
    // otherwise the left half would serialize behind node 1.
    ID3D12CommandList* primaryLists[] = { primaryList.Get() };
    ID3D12CommandList* secondaryLists[] = { secondaryList.Get() };
    afrCommandQueues_[1]->ExecuteCommandLists(1, secondaryLists);
    afrCommandQueues_[0]->ExecuteCommandLists(1, primaryLists);

    const UINT64 secondaryDone = afrNextFenceValues_[1]++;
    ThrowIfFailed(afrCommandQueues_[1]->Signal(afrFences_[1].Get(), secondaryDone),
                  "Signal (SFR secondary) failed");
    ThrowIfFailed(afrCommandQueues_[0]->Wait(afrFences_[1].Get(), secondaryDone),
                  "Wait (SFR cross-node) failed");

    ThrowIfFailed(composeAlloc->Reset(), "Reset (SFR compose allocator) failed");
    ThrowIfFailed(composeList->Reset(composeAlloc.Get(), nullptr),
                  "Reset (SFR compose list) failed");

    D3D12_RESOURCE_BARRIER composeBarriers[2]{};
    composeBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    composeBarriers[0].Transition.pResource = sfrCrossAdapterBuffers_[slot].Get();
    composeBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    composeBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    composeBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    composeList->ResourceBarrier(1, &composeBarriers[0]);

    D3D12_TEXTURE_COPY_LOCATION backBufferDst{};
    backBufferDst.pResource = renderTargets_[slot].Get();
    backBufferDst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    backBufferDst.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION crossSrc{};
    crossSrc.pResource = sfrCrossAdapterBuffers_[slot].Get();
    crossSrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    crossSrc.PlacedFootprint = sfrCrossFootprints_[slot];
    composeList->CopyTextureRegion(
        &backBufferDst, halfWidth, 0, 0, &crossSrc, nullptr);

    composeBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    composeBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    composeBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    composeBarriers[1].Transition.pResource = renderTargets_[slot].Get();
    composeBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    composeBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    composeBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    composeList->ResourceBarrier(2, composeBarriers);

    if (timestampsSupported_) {
        composeList->EndQuery(afrTimestampHeaps_[0].Get(),
                              D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 3);
        composeList->ResolveQueryData(
            afrTimestampHeaps_[0].Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            tsBase, kTimestampsPerFrame, afrTimestampReadbacks_[0].Get(),
            tsBase * sizeof(UINT64));
    }
    ThrowIfFailed(composeList->Close(), "Close (SFR compose list) failed");
    ID3D12CommandList* composeLists[] = { composeList.Get() };
    afrCommandQueues_[0]->ExecuteCommandLists(1, composeLists);

    UINT presentFlags = 0;
    if (!config_.vsync && tearingSupported_)
        presentFlags = DXGI_PRESENT_ALLOW_TEARING;
    ThrowIfFailed(swapChain_->Present(config_.vsync ? 1 : 0, presentFlags),
                  "SwapChain Present (SFR) failed");

    const UINT64 primaryDone = afrNextFenceValues_[0]++;
    ThrowIfFailed(afrCommandQueues_[0]->Signal(afrFences_[0].Get(), primaryDone),
                  "Signal (SFR primary) failed");
    frameFenceValues_[slot] = primaryDone;
    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
}

void DX12Backend::DrawFrame(float deltaTime) {
    if (linkedSfr_) {
        DrawSfrFrame(deltaTime);
        return;
    }

    // Wait for previous use of this frame slot
    const UINT frameNode = linkedAfr_ ? (frameIndex_ % afrNodeCount_) : 0u;
    auto& frameFence = afrFences_[frameNode];
    const HANDLE frameFenceEvent = afrFenceEvents_[frameNode];
    if (frameFence->GetCompletedValue() < frameFenceValues_[frameIndex_]) {
        frameFence->SetEventOnCompletion(frameFenceValues_[frameIndex_], frameFenceEvent);
        WaitForSingleObjectEx(frameFenceEvent, INFINITE, FALSE);
    }

    CollectTimestampResults();

    const UINT activeNode = linkedAfr_ ? (frameIndex_ % afrNodeCount_) : 0u;
    commandList_ = frameCommandLists_[frameIndex_];
    commandQueue_ = afrCommandQueues_[activeNode];
    if (timestampsSupported_) {
        timestampHeap_ = afrTimestampHeaps_[activeNode];
        timestampReadback_ = afrTimestampReadbacks_[activeNode];
        gpuFrequency_ = afrGpuFrequencies_[activeNode];
    }
    if (linkedAfr_) {
        graphicsRootSig_ = afrGraphicsRootSigs_[activeNode];
        graphicsPSO_ = afrGraphicsPSOs_[activeNode];
        rtvHeap_ = afrRtvHeaps_[activeNode];
    }

    auto& alloc = commandAllocators_[frameIndex_];
    alloc->Reset();
    commandList_->Reset(alloc.Get(), nullptr);

    const UINT tsBase = frameIndex_ * kTimestampsPerFrame;

    if (fluid_.active) {
        RecordFluidFrame(deltaTime);
        return;
    }

    if (config_.headless) {
        // --- Headless: compute-only path ---
        // Transition VBV -> UAV
        D3D12_RESOURCE_BARRIER b{};
        b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource   = particleBuffer_.Get();
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        b.Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList_->ResourceBarrier(1, &b);

        if (timestampsSupported_)
            commandList_->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 0);

        commandList_->SetComputeRootSignature(computeRootSig_.Get());
        commandList_->SetPipelineState(computePSO_.Get());
        ID3D12DescriptorHeap* heaps[] = { cbvSrvUavHeap_.Get() };
        commandList_->SetDescriptorHeaps(1, heaps);
        commandList_->SetComputeRootDescriptorTable(0,
            cbvSrvUavHeap_->GetGPUDescriptorHandleForHeapStart());
        if (config_.workload == Workload::SynthPeak) {
            PeakParams params{ config_.peakIters, 0.9999f, 0.0001f, 0 };
            commandList_->SetComputeRoot32BitConstants(1, 3, &params, 0);
        } else if (config_.workload == Workload::NBody) {
            NBodyParams params{ deltaTime, config_.softening, config_.particleCount, 0 };
            commandList_->SetComputeRoot32BitConstants(1, 3, &params, 0);
        } else {
            ComputeParams params{ deltaTime, 0.9f };
            commandList_->SetComputeRoot32BitConstants(1, 2, &params, 0);
        }
        commandList_->Dispatch(config_.particleCount / kComputeWorkGroupSize, 1, 1);

        if (timestampsSupported_)
            commandList_->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 1);

        // UAV barrier then back to VBV for consistency
        D3D12_RESOURCE_BARRIER uavB{};
        uavB.Type           = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavB.UAV.pResource  = particleBuffer_.Get();
        commandList_->ResourceBarrier(1, &uavB);
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        b.Transition.StateAfter  = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        commandList_->ResourceBarrier(1, &b);

        // Mirror timestamps 2/3 = 0/1 for headless (no render)
        if (timestampsSupported_) {
            commandList_->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 2);
            commandList_->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 3);
        }

        if (timestampsSupported_)
            commandList_->ResolveQueryData(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
                                           tsBase, kTimestampsPerFrame,
                                           timestampReadback_.Get(),
                                           tsBase * sizeof(UINT64));

        ThrowIfFailed(commandList_->Close(), "CommandList Close failed");
        ID3D12CommandList* lists[] = { commandList_.Get() };
        commandQueue_->ExecuteCommandLists(1, lists);

        const UINT64 signalValue = afrNextFenceValues_[activeNode]++;
        commandQueue_->Signal(afrFences_[activeNode].Get(), signalValue);
        frameFenceValues_[frameIndex_] = signalValue;
        frameIndex_ = (frameIndex_ + 1) % frameCount_;
        return;
    }

    // --- Normal (windowed) path ---

    D3D12_RESOURCE_BARRIER barriers[2]{};

    if (isFragmentOnlyWorkload(config_.workload)) {
        // Fragment-only: no compute pass, no particle buffer use.
        if (timestampsSupported_) {
            commandList_->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 0);
            commandList_->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 1);
        }
    } else {
    // --- Transition particle buffer VBV -> UAV ---
    barriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource   = particleBuffer_.Get();
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barriers[0]);

    // --- Compute pass ---
    if (timestampsSupported_)
        commandList_->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 0);

    commandList_->SetComputeRootSignature(computeRootSig_.Get());
    commandList_->SetPipelineState(computePSO_.Get());

    ID3D12DescriptorHeap* heaps[] = { cbvSrvUavHeap_.Get() };
    commandList_->SetDescriptorHeaps(1, heaps);
    commandList_->SetComputeRootDescriptorTable(0,
        cbvSrvUavHeap_->GetGPUDescriptorHandleForHeapStart());

    if (config_.workload == Workload::SynthPeak) {
        PeakParams params{ config_.peakIters, 0.9999f, 0.0001f, 0 };
        commandList_->SetComputeRoot32BitConstants(1, 3, &params, 0);
    } else if (config_.workload == Workload::NBody) {
        NBodyParams params{ deltaTime, config_.softening, config_.particleCount, 0 };
        commandList_->SetComputeRoot32BitConstants(1, 3, &params, 0);
    } else {
        ComputeParams params{ deltaTime, 0.9f };
        commandList_->SetComputeRoot32BitConstants(1, 2, &params, 0);
    }
    commandList_->Dispatch(config_.particleCount / kComputeWorkGroupSize, 1, 1);

    if (timestampsSupported_)
        commandList_->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 1);

    // UAV barrier then transition to VBV
    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type           = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource  = particleBuffer_.Get();
    commandList_->ResourceBarrier(1, &uavBarrier);

    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    commandList_->ResourceBarrier(1, &barriers[0]);
    }

    // --- Transition render target PRESENT -> RT ---
    barriers[0].Transition.pResource   = renderTargets_[frameIndex_].Get();
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &barriers[0]);

    // --- Graphics pass ---
    if (timestampsSupported_)
        commandList_->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 2);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(frameIndex_) * rtvDescriptorSize_;

    const float clearColor[] = { 0.04f, 0.08f, 0.14f, 1.0f };
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    if (config_.workload == Workload::Render3D) {
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
        commandList_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsv);
    } else {
        commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    }

    D3D12_VIEWPORT vp{ 0, 0, static_cast<float>(kWindowWidth),
                       static_cast<float>(kWindowHeight), 0.0f, 1.0f };
    D3D12_RECT sc{ 0, 0, static_cast<LONG>(kWindowWidth),
                   static_cast<LONG>(kWindowHeight) };
    commandList_->RSSetViewports(1, &vp);
    commandList_->RSSetScissorRects(1, &sc);

    commandList_->SetGraphicsRootSignature(graphicsRootSig_.Get());
    commandList_->SetPipelineState(graphicsPSO_.Get());
    if (config_.workload == Workload::StressFractal) {
        fractalElapsed_ += deltaTime;
        FractalParams fp{ fractalElapsed_, 1.0f, config_.fractalIter, 0 };
        commandList_->SetGraphicsRoot32BitConstants(0, 3, &fp, 0);
        commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList_->DrawInstanced(3, 1, 0, 0);   // fullscreen triangle, no VB
    } else if (config_.workload == Workload::GpuStressV1) {
        commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        for (std::uint32_t pass = 0; pass < kGpuStressV1DrawsPerFrame; ++pass) {
            GpuStressV1Params sp{ static_cast<float>(pass), 1.0f,
                                  config_.gpuStressIter, kGpuStressV1ShaderVersion };
            commandList_->SetGraphicsRoot32BitConstants(0, 4, &sp, 0);
            commandList_->DrawInstanced(3, 1, 0, 0);
        }
    } else if (isGpuBurnWorkload(config_.workload)) {
        fractalElapsed_ += deltaTime;
        commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        for (std::uint32_t pass = 0; pass < gpuBurnDrawsPerFrame(config_.workload); ++pass) {
            GpuBurnParams bp{ fractalElapsed_, static_cast<float>(pass),
                              config_.gpuBurnIter, gpuBurnShaderVersion(config_.workload) };
            commandList_->SetGraphicsRoot32BitConstants(0, 4, &bp, 0);
            commandList_->DrawInstanced(3, 1, 0, 0);
        }
    } else if (config_.workload == Workload::Volumetric) {
        fractalElapsed_ += deltaTime;   // reused as noise-field animation time
        VolumetricParams vol{ fractalElapsed_, 0.05f, config_.volumetricSteps, 0 };
        commandList_->SetGraphicsRoot32BitConstants(0, 3, &vol, 0);
        commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList_->DrawInstanced(3, 1, 0, 0);   // fullscreen triangle, no VB
    } else if (config_.workload == Workload::Render3D) {
        fractalElapsed_ += deltaTime;
        Render3DParams r3{};
        const float aspect = static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight);
        render3dCamera(fractalElapsed_, aspect, /*flipY*/false, /*z01*/true,
                       r3.viewProj, r3.camRight, r3.camUp);
        r3.pointSize = 0.02f;
        commandList_->SetGraphicsRoot32BitConstants(0, sizeof(Render3DParams) / 4, &r3, 0);
        commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        D3D12_VERTEX_BUFFER_VIEW vbs[2] = { quadView_, vbView_ };
        commandList_->IASetVertexBuffers(0, 2, vbs);
        commandList_->DrawInstanced(6, config_.particleCount, 0, 0);
    } else {
        commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
        commandList_->IASetVertexBuffers(0, 1, &vbView_);
        commandList_->DrawInstanced(config_.particleCount, 1, 0, 0);
    }

    if (timestampsSupported_)
        commandList_->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 3);

    // Resolve timestamps
    if (timestampsSupported_)
        commandList_->ResolveQueryData(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
                                       tsBase, kTimestampsPerFrame,
                                       timestampReadback_.Get(),
                                       tsBase * sizeof(UINT64));

    // --- Transition render target RT -> PRESENT ---
    barriers[0].Transition.pResource   = renderTargets_[frameIndex_].Get();
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &barriers[0]);

    ThrowIfFailed(commandList_->Close(), "CommandList Close failed");

    ID3D12CommandList* lists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, lists);

    UINT presentFlags = 0;
    if (!config_.vsync && tearingSupported_)
        presentFlags = DXGI_PRESENT_ALLOW_TEARING;
    ThrowIfFailed(swapChain_->Present(config_.vsync ? 1 : 0, presentFlags),
                  "SwapChain Present failed");

    // Signal fence for this frame
    const UINT64 signalValue = afrNextFenceValues_[activeNode]++;
    commandQueue_->Signal(afrFences_[activeNode].Get(), signalValue);
    frameFenceValues_[frameIndex_] = signalValue;

    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
}

// -----------------------------------------------------------------------
// Cleanup
// -----------------------------------------------------------------------

void DX12Backend::CleanupBackend() {
    WaitForGpu();
    CleanupFluidResources();

    for (auto& eventHandle : afrFenceEvents_) {
        if (eventHandle) CloseHandle(eventHandle);
        eventHandle = nullptr;
    }
    fenceEvent_ = nullptr;
}

// -----------------------------------------------------------------------
// Legacy 2D Fluid (Stam) — multi-pass compute + fullscreen dye render
// -----------------------------------------------------------------------

void DX12Backend::CreateFluidResources() {
    fluid_.gridSize = config_.fluidGridSize;
    const std::uint32_t N = fluid_.gridSize;
    if (N < 64 || N > 512)
        throw std::invalid_argument("Fluid --grid must be between 64 and 512");
    if (config_.fluidJacobiIters < 1 || config_.fluidJacobiIters > 64)
        throw std::invalid_argument("Fluid --jacobi must be between 1 and 64");

    const UINT64 stateSize = sizeof(float) * 4 * N * N;
    const UINT64 pressSize = sizeof(float) * N * N;

    auto createDefaultBuffer = [&](UINT64 size, ComPtr<ID3D12Resource>& out) {
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
        ApplySingleNodeHeapMasks(hp);
        D3D12_RESOURCE_DESC rd{};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = size;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.SampleDesc.Count = 1;
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        ThrowIfFailed(device_->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON,
            nullptr, IID_PPV_ARGS(&out)), "CreateCommittedResource (fluid) failed");
    };

    createDefaultBuffer(stateSize, fluid_.stateA);
    createDefaultBuffer(stateSize, fluid_.stateB);
    createDefaultBuffer(pressSize, fluid_.pressA);
    createDefaultBuffer(pressSize, fluid_.pressB);
    createDefaultBuffer(pressSize, fluid_.divBuf);

    // Seed stateA (same dual-emitter field as Vulkan).
    std::vector<float> init(static_cast<size_t>(stateSize / sizeof(float)));
    for (std::uint32_t y = 0; y < N; ++y) {
        for (std::uint32_t x = 0; x < N; ++x) {
            const size_t i = (size_t(y) * N + x) * 4;
            const float fx = (float(x) + 0.5f) / float(N) - 0.5f;
            const float fy = (float(y) + 0.5f) / float(N) - 0.5f;
            const float envelope = std::exp(-(fx * fx + fy * fy) * 7.0f);
            init[i + 0] = -fy * 0.32f * envelope;
            init[i + 1] =  fx * 0.32f * envelope;
            const float ax = fx + 0.25f, ay = fy - 0.04f;
            const float bx = fx - 0.25f, by = fy + 0.04f;
            init[i + 2] = std::exp(-(ax * ax + ay * ay) * 120.0f);
            init[i + 3] = std::exp(-(bx * bx + by * by) * 120.0f);
        }
    }

    auto createUpload = [&](UINT64 size, const void* data, ComPtr<ID3D12Resource>& out) {
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        ApplySingleNodeHeapMasks(hp);
        D3D12_RESOURCE_DESC rd{};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = size;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.SampleDesc.Count = 1;
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ThrowIfFailed(device_->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&out)), "CreateCommittedResource (fluid upload) failed");
        void* mapped = nullptr;
        ThrowIfFailed(out->Map(0, nullptr, &mapped), "Map fluid upload failed");
        if (data) std::memcpy(mapped, data, static_cast<size_t>(size));
        else std::memset(mapped, 0, static_cast<size_t>(size));
        out->Unmap(0, nullptr);
    };

    ComPtr<ID3D12Resource> stateUpload, zeroUpload;
    createUpload(stateSize, init.data(), stateUpload);
    createUpload(pressSize, nullptr, zeroUpload);
    fluid_.zeroPressUpload = zeroUpload;

    // One-shot upload: stateA seeded, pressure/div cleared.
    commandAllocators_[0]->Reset();
    commandList_->Reset(commandAllocators_[0].Get(), nullptr);
    commandList_->CopyBufferRegion(fluid_.stateA.Get(), 0, stateUpload.Get(), 0, stateSize);
    commandList_->CopyBufferRegion(fluid_.pressA.Get(), 0, zeroUpload.Get(), 0, pressSize);
    commandList_->CopyBufferRegion(fluid_.pressB.Get(), 0, zeroUpload.Get(), 0, pressSize);
    commandList_->CopyBufferRegion(fluid_.divBuf.Get(), 0, zeroUpload.Get(), 0, pressSize);
    {
        D3D12_RESOURCE_BARRIER barriers[5]{};
        ID3D12Resource* bufs[] = {
            fluid_.stateA.Get(), fluid_.stateB.Get(), fluid_.pressA.Get(),
            fluid_.pressB.Get(), fluid_.divBuf.Get()
        };
        for (int i = 0; i < 5; ++i) {
            barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[i].Transition.pResource = bufs[i];
            barriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }
        // stateB never received a copy; it starts in COMMON. Transition COMMON->UAV.
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        commandList_->ResourceBarrier(5, barriers);
    }
    ThrowIfFailed(commandList_->Close(), "Fluid upload Close failed");
    ID3D12CommandList* lists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, lists);
    WaitForGpu();

    // Root signatures
    {
        D3D12_DESCRIPTOR_RANGE uavRange{};
        uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRange.NumDescriptors = 5;
        uavRange.BaseShaderRegister = 0;

        D3D12_ROOT_PARAMETER params[2]{};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[0].DescriptorTable.NumDescriptorRanges = 1;
        params[0].DescriptorTable.pDescriptorRanges = &uavRange;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[1].Constants.ShaderRegister = 0;
        params[1].Constants.Num32BitValues = 4;
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = 2;
        desc.pParameters = params;
        ComPtr<ID3DBlob> sig, err;
        ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err),
                      "Serialize fluid compute root signature failed");
        ThrowIfFailed(device_->CreateRootSignature(SingleNodeMask(), sig->GetBufferPointer(), sig->GetBufferSize(),
                                                   IID_PPV_ARGS(&fluid_.computeRootSig)),
                      "CreateRootSignature (fluid compute) failed");
    }
    {
        D3D12_DESCRIPTOR_RANGE srvRange{};
        srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors = 1;
        srvRange.BaseShaderRegister = 0;

        D3D12_ROOT_PARAMETER params[2]{};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[0].DescriptorTable.NumDescriptorRanges = 1;
        params[0].DescriptorTable.pDescriptorRanges = &srvRange;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[1].Constants.ShaderRegister = 0;
        params[1].Constants.Num32BitValues = 4;
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = 2;
        desc.pParameters = params;
        ComPtr<ID3DBlob> sig, err;
        ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err),
                      "Serialize fluid graphics root signature failed");
        ThrowIfFailed(device_->CreateRootSignature(SingleNodeMask(), sig->GetBufferPointer(), sig->GetBufferSize(),
                                                   IID_PPV_ARGS(&fluid_.graphicsRootSig)),
                      "CreateRootSignature (fluid graphics) failed");
    }

    const std::string hlsl = shaderDir_ + "fluid.hlsl";
    auto makeCs = [&](const char* entry, ComPtr<ID3D12PipelineState>& pso) {
        auto blob = CompileShader(hlsl, entry, "cs_5_1");
        D3D12_COMPUTE_PIPELINE_STATE_DESC d{};
        d.pRootSignature = fluid_.computeRootSig.Get();
        d.CS = { blob->GetBufferPointer(), blob->GetBufferSize() };
        d.NodeMask = SingleNodeMask();
        ThrowIfFailed(device_->CreateComputePipelineState(&d, IID_PPV_ARGS(&pso)),
                      (std::string("CreateComputePipelineState (") + entry + ") failed").c_str());
    };
    makeCs("CSAdvect", fluid_.advectPSO);
    makeCs("CSDivergence", fluid_.divPSO);
    makeCs("CSJacobi", fluid_.jacobiPSO);
    makeCs("CSSubtract", fluid_.subtractPSO);

    {
        auto vs = CompileShader(hlsl, "VSMain", "vs_5_1");
        auto ps = CompileShader(hlsl, "PSMain", "ps_5_1");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
        d.pRootSignature = fluid_.graphicsRootSig.Get();
        d.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
        d.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
        d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        d.RasterizerState.DepthClipEnable = TRUE;
        d.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        d.SampleMask = UINT_MAX;
        d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        d.NumRenderTargets = 1;
        d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.SampleDesc.Count = 1;
        d.NodeMask = SingleNodeMask();
        ThrowIfFailed(device_->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&fluid_.renderPSO)),
                      "CreateGraphicsPipelineState (fluid) failed");
    }

    // Descriptor heap: 5 UAV tables × 5 + 1 SRV = 26
    {
        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        hd.NumDescriptors = 26;
        hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        hd.NodeMask = SingleNodeMask();
        ThrowIfFailed(device_->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&fluid_.heap)),
                      "CreateDescriptorHeap (fluid) failed");
    }

    const UINT stride = device_->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto cpuAt = [&](UINT index) {
        D3D12_CPU_DESCRIPTOR_HANDLE h = fluid_.heap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += static_cast<SIZE_T>(index) * stride;
        return h;
    };
    auto gpuAt = [&](UINT index) {
        D3D12_GPU_DESCRIPTOR_HANDLE h = fluid_.heap->GetGPUDescriptorHandleForHeapStart();
        h.ptr += static_cast<SIZE_T>(index) * stride;
        return h;
    };

    auto writeUav = [&](UINT index, ID3D12Resource* res, UINT elementStride, UINT numElements) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC u{};
        u.Format = DXGI_FORMAT_UNKNOWN;
        u.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        u.Buffer.FirstElement = 0;
        u.Buffer.NumElements = numElements;
        u.Buffer.StructureByteStride = elementStride;
        device_->CreateUnorderedAccessView(res, nullptr, &u, cpuAt(index));
    };
    auto writeSrv = [&](UINT index, ID3D12Resource* res, UINT elementStride, UINT numElements) {
        D3D12_SHADER_RESOURCE_VIEW_DESC s{};
        s.Format = DXGI_FORMAT_UNKNOWN;
        s.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        s.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        s.Buffer.FirstElement = 0;
        s.Buffer.NumElements = numElements;
        s.Buffer.StructureByteStride = elementStride;
        device_->CreateShaderResourceView(res, &s, cpuAt(index));
    };

    const UINT cellCount = N * N;
    // Table layout (5 UAVs each): [0]advect [5]div [10]jacA [15]jacB [20]sub; [25]=SRV
    // Advect: u0=A u1=B u2=pressA u3=pressB u4=div
    writeUav(0, fluid_.stateA.Get(), 16, cellCount);
    writeUav(1, fluid_.stateB.Get(), 16, cellCount);
    writeUav(2, fluid_.pressA.Get(), 4, cellCount);
    writeUav(3, fluid_.pressB.Get(), 4, cellCount);
    writeUav(4, fluid_.divBuf.Get(), 4, cellCount);
    fluid_.tableAdvect = gpuAt(0);

    // Div: u0=B u1=B u2=pressA u3=pressB u4=div
    writeUav(5, fluid_.stateB.Get(), 16, cellCount);
    writeUav(6, fluid_.stateB.Get(), 16, cellCount);
    writeUav(7, fluid_.pressA.Get(), 4, cellCount);
    writeUav(8, fluid_.pressB.Get(), 4, cellCount);
    writeUav(9, fluid_.divBuf.Get(), 4, cellCount);
    fluid_.tableDiv = gpuAt(5);

    // Jacobi A: in=pressA out=pressB
    writeUav(10, fluid_.stateA.Get(), 16, cellCount);
    writeUav(11, fluid_.stateB.Get(), 16, cellCount);
    writeUav(12, fluid_.pressA.Get(), 4, cellCount);
    writeUav(13, fluid_.pressB.Get(), 4, cellCount);
    writeUav(14, fluid_.divBuf.Get(), 4, cellCount);
    fluid_.tableJacA = gpuAt(10);

    // Jacobi B: in=pressB out=pressA
    writeUav(15, fluid_.stateA.Get(), 16, cellCount);
    writeUav(16, fluid_.stateB.Get(), 16, cellCount);
    writeUav(17, fluid_.pressB.Get(), 4, cellCount);
    writeUav(18, fluid_.pressA.Get(), 4, cellCount);
    writeUav(19, fluid_.divBuf.Get(), 4, cellCount);
    fluid_.tableJacB = gpuAt(15);

    // Subtract: in=B out=A inP=final pressure
    ID3D12Resource* finalPress = (config_.fluidJacobiIters & 1u)
        ? fluid_.pressB.Get() : fluid_.pressA.Get();
    writeUav(20, fluid_.stateB.Get(), 16, cellCount);
    writeUav(21, fluid_.stateA.Get(), 16, cellCount);
    writeUav(22, finalPress, 4, cellCount);
    writeUav(23, fluid_.pressB.Get(), 4, cellCount);
    writeUav(24, fluid_.divBuf.Get(), 4, cellCount);
    fluid_.tableSub = gpuAt(20);

    writeSrv(25, fluid_.stateA.Get(), 16, cellCount);
    fluid_.tableRender = gpuAt(25);

    fluid_.simTime = 0.0f;
    fluid_.active = true;
}

void DX12Backend::CleanupFluidResources() {
    if (!fluid_.active) return;
    WaitForGpu();
    fluid_ = FluidResources{};
}

void DX12Backend::RecordFluidFrame(float deltaTime) {
    const UINT tsBase = frameIndex_ * kTimestampsPerFrame;
    const std::uint32_t N = fluid_.gridSize;
    const float stepDt = (std::min)((std::max)(deltaTime, 0.0f), 1.0f / 30.0f);
    fluid_.simTime += (std::min)((std::max)(deltaTime, 0.0f), 0.1f);
    const FluidParams fp{ stepDt, 1.0f / float(N), N, fluid_.simTime };
    const UINT groups = (N + 15u) / 16u;
    const UINT64 pressSize = sizeof(float) * N * N;

    auto uavBarrier = [&](ID3D12Resource* res) {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        b.UAV.pResource = res;
        commandList_->ResourceBarrier(1, &b);
    };

    if (timestampsSupported_)
        commandList_->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 0);

    ID3D12DescriptorHeap* heaps[] = { fluid_.heap.Get() };
    commandList_->SetDescriptorHeaps(1, heaps);
    commandList_->SetComputeRootSignature(fluid_.computeRootSig.Get());
    commandList_->SetComputeRoot32BitConstants(1, 4, &fp, 0);

    // Clear pressA each frame (Jacobi warm start).
    {
        D3D12_RESOURCE_BARRIER toCopy{};
        toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toCopy.Transition.pResource = fluid_.pressA.Get();
        toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList_->ResourceBarrier(1, &toCopy);
        commandList_->CopyBufferRegion(fluid_.pressA.Get(), 0,
                                       fluid_.zeroPressUpload.Get(), 0, pressSize);
        toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        commandList_->ResourceBarrier(1, &toCopy);
    }

    commandList_->SetPipelineState(fluid_.advectPSO.Get());
    commandList_->SetComputeRootDescriptorTable(0, fluid_.tableAdvect);
    commandList_->Dispatch(groups, groups, 1);
    uavBarrier(fluid_.stateB.Get());

    commandList_->SetPipelineState(fluid_.divPSO.Get());
    commandList_->SetComputeRootDescriptorTable(0, fluid_.tableDiv);
    commandList_->Dispatch(groups, groups, 1);
    uavBarrier(fluid_.divBuf.Get());

    commandList_->SetPipelineState(fluid_.jacobiPSO.Get());
    D3D12_GPU_DESCRIPTOR_HANDLE jacTables[2] = { fluid_.tableJacA, fluid_.tableJacB };
    for (std::uint32_t i = 0; i < config_.fluidJacobiIters; ++i) {
        commandList_->SetComputeRootDescriptorTable(0, jacTables[i & 1u]);
        commandList_->Dispatch(groups, groups, 1);
        uavBarrier((i & 1u) ? fluid_.pressA.Get() : fluid_.pressB.Get());
    }

    commandList_->SetPipelineState(fluid_.subtractPSO.Get());
    commandList_->SetComputeRootDescriptorTable(0, fluid_.tableSub);
    commandList_->Dispatch(groups, groups, 1);
    uavBarrier(fluid_.stateA.Get());

    // stateA: UAV -> NON_PIXEL_SHADER | PIXEL_SHADER for SRV sample in PS
    {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = fluid_.stateA.Get();
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                                  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList_->ResourceBarrier(1, &b);
    }

    if (timestampsSupported_) {
        commandList_->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 1);
        commandList_->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 2);
    }

    D3D12_RESOURCE_BARRIER rtBarrier{};
    rtBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    rtBarrier.Transition.pResource = renderTargets_[frameIndex_].Get();
    rtBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    rtBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    rtBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &rtBarrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(frameIndex_) * rtvDescriptorSize_;
    const float clearColor[] = { 0.04f, 0.08f, 0.14f, 1.0f };
    commandList_->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    D3D12_VIEWPORT vp{ 0, 0, static_cast<float>(kWindowWidth),
                       static_cast<float>(kWindowHeight), 0.0f, 1.0f };
    D3D12_RECT sc{ 0, 0, static_cast<LONG>(kWindowWidth),
                   static_cast<LONG>(kWindowHeight) };
    commandList_->RSSetViewports(1, &vp);
    commandList_->RSSetScissorRects(1, &sc);

    commandList_->SetGraphicsRootSignature(fluid_.graphicsRootSig.Get());
    commandList_->SetPipelineState(fluid_.renderPSO.Get());
    commandList_->SetGraphicsRootDescriptorTable(0, fluid_.tableRender);
    FluidRenderParams rp{ N, fluid_.simTime, 0.88f, 1u };
    commandList_->SetGraphicsRoot32BitConstants(1, 4, &rp, 0);
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->DrawInstanced(3, 1, 0, 0);

    if (timestampsSupported_)
        commandList_->EndQuery(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 3);

    // stateA back to UAV for next frame
    {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = fluid_.stateA.Get();
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList_->ResourceBarrier(1, &b);
    }

    rtBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    rtBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &rtBarrier);

    if (timestampsSupported_)
        commandList_->ResolveQueryData(timestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
                                       tsBase, kTimestampsPerFrame,
                                       timestampReadback_.Get(),
                                       tsBase * sizeof(UINT64));

    ThrowIfFailed(commandList_->Close(), "Fluid CommandList Close failed");
    ID3D12CommandList* cmdLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, cmdLists);

    UINT presentFlags = 0;
    if (!config_.vsync && tearingSupported_)
        presentFlags = DXGI_PRESENT_ALLOW_TEARING;
    swapChain_->Present(config_.vsync ? 1 : 0, presentFlags);

    const UINT64 signalValue = afrNextFenceValues_[0]++;
    commandQueue_->Signal(fence_.Get(), signalValue);
    frameFenceValues_[frameIndex_] = signalValue;
    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
}

}  // namespace gpu_bench

#endif  // HAVE_DX12
