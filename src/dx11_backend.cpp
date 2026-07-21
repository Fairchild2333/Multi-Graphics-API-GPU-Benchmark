#ifdef HAVE_DX11

#include "dx11_backend.h"
#include "mini_mat.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace gpu_bench {

using Microsoft::WRL::ComPtr;

void DX11Backend::ThrowIfFailed(HRESULT hr, const char* msg) {
    if (FAILED(hr)) throw std::runtime_error(msg);
}

Microsoft::WRL::ComPtr<ID3DBlob> DX11Backend::CompileShader(const std::string& path,
                                                             const char* entry,
                                                             const char* target) {
    auto src = ReadFileBytes(path);
    ComPtr<ID3DBlob> shader, errors;
    HRESULT hr = D3DCompile(src.data(), src.size(), path.c_str(),
                            nullptr, nullptr, entry, target, 0, 0,
                            &shader, &errors);
    if (FAILED(hr)) {
        std::string msg = "Shader compilation failed: " + path;
        if (errors) msg += "\n" + std::string(static_cast<char*>(errors->GetBufferPointer()),
                                              errors->GetBufferSize());
        throw std::runtime_error(msg);
    }
    return shader;
}

// -----------------------------------------------------------------------
// Init
// -----------------------------------------------------------------------

void DX11Backend::InitBackend() {
    const bool fluid = (config_.workload == Workload::Fluid);
    if (!fluid && !isFragmentOnlyWorkload(config_.workload) &&
        (config_.particleCount == 0 ||
         config_.particleCount % kComputeWorkGroupSize != 0)) {
        throw std::runtime_error(
            "DX11 compute workloads require a non-zero particle/body count "
            "aligned to the 256-thread workgroup size.");
    }
    std::cout << "[DX11 Init] Creating device" << (config_.headless ? " (headless)..." : " and swap chain...") << std::endl;
    CreateDeviceAndSwapChain();
    if (config_.multiGpuMode == MultiGpuMode::Afr) {
        // D3D11 has no public node-mask/device-group submission API.  On a
        // CrossFire driver this presents the normal single-device frame stream
        // and leaves AFR ownership to the UMD.  Keep enough queued frames for
        // the driver to pipeline alternate physical GPUs.
        ComPtr<IDXGIDevice1> dxgiDevice;
        if (SUCCEEDED(device_.As(&dxgiDevice)))
            dxgiDevice->SetMaximumFrameLatency(
                (std::max)(2u, config_.framesInFlight));
        std::cout
            << "[DX11 AFR] Driver-managed implicit CrossFire requested. "
               "D3D11 cannot select or verify the physical GPU for each frame; "
               "confirm both GPUs with driver telemetry.\n";
    }
    if (featureLevel_ < D3D_FEATURE_LEVEL_11_0 &&
        config_.workload == Workload::NBody &&
        config_.particleCount > 4096u) {
        throw std::runtime_error(
            "The DX11 SM4 safety profile limits N-body to 4,096 bodies to "
            "avoid a watchdog/TDR-length dispatch. Select 4096 or fewer bodies.");
    }
    if ((fluid || !isFragmentOnlyWorkload(config_.workload)) &&
        !computeShadersSupported_) {
        throw std::runtime_error(
            "This Direct3D feature-level 10 device/driver does not expose "
            "DirectCompute 4.x. Fragment-only DX11 workloads remain available.");
    }
    const std::uint64_t dispatchGroups =
        std::uint64_t(config_.particleCount) / kComputeWorkGroupSize;
    if (!fluid && !isFragmentOnlyWorkload(config_.workload) &&
        dispatchGroups > D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION) {
        throw std::runtime_error(
            "DX11 workload exceeds the 65,535 thread-group Dispatch limit; "
            "select a smaller particle count/difficulty.");
    }
    if (!config_.headless) {
        std::cout << "[DX11 Init] Creating render target..." << std::endl;
        CreateRenderTarget();
    }
    if (fluid) {
        std::cout << "[DX11 Init] Creating fluid resources..." << std::endl;
        CreateFluidResources();
        std::cout << "[DX11 Init] Creating timestamp queries..." << std::endl;
        CreateTimestampQueries();
        std::cout << "[DX11 Init] Initialisation complete (fluid)." << std::endl;
        return;
    }
    std::cout << "[DX11 Init] Compiling shaders..." << std::endl;
    CreateShaders();
    if (!isFragmentOnlyWorkload(config_.workload)) {
        std::cout << "[DX11 Init] Creating particle buffers..." << std::endl;
        CreateParticleBuffers();
    }
    std::cout << "[DX11 Init] Creating compute params CB..." << std::endl;
    CreateComputeParamsCB();
    std::cout << "[DX11 Init] Creating timestamp queries..." << std::endl;
    CreateTimestampQueries();
    std::cout << "[DX11 Init] Initialisation complete." << std::endl;
}

void DX11Backend::CreateDeviceAndSwapChain() {
    HWND hwnd = config_.headless ? nullptr : glfwGetWin32Window(window_);

    ComPtr<IDXGIFactory2> factory;
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "CreateDXGIFactory1 failed");

    // Sentinel -2: WARP software renderer requested from main.cpp.
    if (requestedGpuIndex_ == -2) {
        std::cout << "[DX11] Using Microsoft WARP software renderer (CPU)." << std::endl;

        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        };
        D3D_FEATURE_LEVEL actualFL{};
        UINT flags = 0;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        ThrowIfFailed(D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
            featureLevels, _countof(featureLevels), D3D11_SDK_VERSION,
            &device_, &actualFL, &context_),
            "D3D11CreateDevice (WARP) failed");

        featureLevel_ = actualFL;
        computeShadersSupported_ = true;

        deviceName_ = "Microsoft WARP (CPU Software Renderer)";
        driverVersion_ = "WARP";
        std::cout << "Selected GPU: " << deviceName_ << std::endl;

        if (!config_.headless) {
            DXGI_SWAP_CHAIN_DESC1 sd{};
            sd.Width            = kWindowWidth;
            sd.Height           = kWindowHeight;
            sd.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
            sd.SampleDesc.Count = 1;
            sd.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sd.BufferCount      = config_.framesInFlight + 1;
            sd.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
            if (!config_.vsync) sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

            ThrowIfFailed(factory->CreateSwapChainForHwnd(
                device_.Get(), hwnd, &sd, nullptr, nullptr, &swapChain_),
                "CreateSwapChainForHwnd (WARP) failed");
        }
        return;
    }

    // Normal hardware adapter path follows.
    struct AdapterEntry {
        ComPtr<IDXGIAdapter1> adapter;
        DXGI_ADAPTER_DESC1 desc;
        UINT rawIndex;
    };
    std::vector<AdapterEntry> uniqueAdapters;
    {
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);

            // Deduplicate by LUID (not VendorId+DeviceId+SubSysId) so that
            // identical GPUs with distinct LUIDs are listed separately.
            bool duplicate = false;
            for (const auto& existing : uniqueAdapters) {
                if (existing.desc.AdapterLuid.HighPart == desc.AdapterLuid.HighPart &&
                    existing.desc.AdapterLuid.LowPart  == desc.AdapterLuid.LowPart) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                uniqueAdapters.push_back({adapter, desc, i});
            }
            adapter.Reset();
        }
    }

    std::cout << "Available GPUs:\n";
    for (std::uint32_t i = 0; i < uniqueAdapters.size(); ++i) {
        const auto& entry = uniqueAdapters[i];
        bool isSoftware = (entry.desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
        char name[256]{};
        size_t converted = 0;
        wcstombs_s(&converted, name, sizeof(name), entry.desc.Description, _TRUNCATE);
        std::cout << "  [" << i << "] " << name
                  << (isSoftware ? " (Software)" : " (Hardware)")
                  << "  VRAM: " << (entry.desc.DedicatedVideoMemory / (1024 * 1024)) << " MB"
                  << std::endl;
    }

    // Select adapter: --gpu flag, single-choice auto, or interactive prompt.
    std::uint32_t chosen = 0;
    std::vector<std::uint32_t> hwIndices;
    for (std::uint32_t i = 0; i < uniqueAdapters.size(); ++i) {
        if ((uniqueAdapters[i].desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
            hwIndices.push_back(i);
    }

    // Try LUID-based selection first (reliable across factory instances).
    bool luidMatched = false;
    if (config_.adapterLuidHigh != 0 || config_.adapterLuidLow != 0) {
        for (std::uint32_t i = 0; i < uniqueAdapters.size(); ++i) {
            const auto& d = uniqueAdapters[i].desc;
            if (d.AdapterLuid.HighPart == static_cast<LONG>(config_.adapterLuidHigh) &&
                d.AdapterLuid.LowPart  == static_cast<DWORD>(config_.adapterLuidLow)) {
                chosen = i;
                luidMatched = true;
                break;
            }
        }
        if (!luidMatched)
            throw std::runtime_error("Requested GPU (LUID) not found for DX11");
    } else if (requestedGpuIndex_ >= 0) {
        auto idx = static_cast<std::uint32_t>(requestedGpuIndex_);
        if (idx >= uniqueAdapters.size())
            throw std::runtime_error("Requested GPU index " +
                std::to_string(requestedGpuIndex_) + " is out of range");
        chosen = idx;
    } else if (hwIndices.size() == 1) {
        chosen = hwIndices[0];
    } else if (hwIndices.size() > 1) {
        // Pick the one with the most dedicated VRAM by default, but let
        // the user override interactively.
        SIZE_T bestMem = 0;
        for (auto i : hwIndices) {
            if (uniqueAdapters[i].desc.DedicatedVideoMemory > bestMem) {
                bestMem = uniqueAdapters[i].desc.DedicatedVideoMemory;
                chosen = i;
            }
        }
        std::cout << "Multiple hardware GPUs detected. Default: [" << chosen << "]\n"
                  << "Enter GPU index (or 'b' to go back): " << std::flush;
        std::string line;
        if (std::getline(std::cin, line) && !line.empty()) {
            if (line == "b" || line == "B")
                throw gpu_bench::BackToMenuException();
            auto idx = static_cast<std::uint32_t>(std::stoi(line));
            if (idx >= uniqueAdapters.size())
                throw std::runtime_error("GPU index " + line + " is out of range");
            chosen = idx;
        }
    }

    ComPtr<IDXGIAdapter1> bestAdapter = uniqueAdapters[chosen].adapter;

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL actualFeatureLevel{};
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverType = bestAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;
    ThrowIfFailed(D3D11CreateDevice(
        bestAdapter.Get(), driverType, nullptr, flags,
        featureLevels, _countof(featureLevels), D3D11_SDK_VERSION,
        &device_, &actualFeatureLevel, &context_),
        "D3D11CreateDevice failed");

    featureLevel_ = actualFeatureLevel;
    computeShadersSupported_ = actualFeatureLevel >= D3D_FEATURE_LEVEL_11_0;
    if (!computeShadersSupported_) {
        D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS options{};
        computeShadersSupported_ = SUCCEEDED(device_->CheckFeatureSupport(
            D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS,
            &options, sizeof(options))) &&
            options.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x;
    }

    // Query the actual adapter the device is using via DXGI
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> actualAdapter;
    std::string adapterType = "Unknown";
    if (SUCCEEDED(device_.As(&dxgiDevice)) &&
        SUCCEEDED(dxgiDevice->GetAdapter(&actualAdapter))) {
        DXGI_ADAPTER_DESC adDesc{};
        actualAdapter->GetDesc(&adDesc);
        char name[256]{};
        size_t converted = 0;
        wcstombs_s(&converted, name, sizeof(name), adDesc.Description, _TRUNCATE);
        deviceName_ = name;

        bool isSoftware = (adDesc.VendorId == 0x1414 && adDesc.DeviceId == 0x8c);
        adapterType = isSoftware ? "Software (WARP/Basic Render)" : "Hardware";

        LARGE_INTEGER umdVer{};
        if (SUCCEEDED(actualAdapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umdVer))) {
            auto v = static_cast<std::uint64_t>(umdVer.QuadPart);
            driverVersion_ = std::to_string((v >> 48) & 0xffff) + "."
                           + std::to_string((v >> 32) & 0xffff) + "."
                           + std::to_string((v >> 16) & 0xffff) + "."
                           + std::to_string(v & 0xffff);
        }
    } else if (bestAdapter) {
        DXGI_ADAPTER_DESC1 desc{};
        bestAdapter->GetDesc1(&desc);
        char name[256]{};
        size_t converted = 0;
        wcstombs_s(&converted, name, sizeof(name), desc.Description, _TRUNCATE);
        deviceName_ = name;
        adapterType = "Hardware";
    } else {
        deviceName_ = "Default Hardware Adapter";
    }

    auto flName = [](D3D_FEATURE_LEVEL fl) -> const char* {
        switch (fl) {
        case D3D_FEATURE_LEVEL_11_1: return "11_1";
        case D3D_FEATURE_LEVEL_11_0: return "11_0";
        case D3D_FEATURE_LEVEL_10_1: return "10_1";
        case D3D_FEATURE_LEVEL_10_0: return "10_0";
        default: return "unknown";
        }
    };

    std::cout << "Selected GPU: " << deviceName_ << '\n'
              << "  Driver type:   " << adapterType << '\n'
              << "  Feature Level: " << flName(actualFeatureLevel) << '\n'
              << "  Shader Model:  "
              << (actualFeatureLevel >= D3D_FEATURE_LEVEL_11_0 ? "5.0" : "4.0")
              << '\n'
              << "  DirectCompute: "
              << (computeShadersSupported_ ? "available" : "not exposed")
              << '\n';
    const std::string capabilityTag = std::string("D3D_FL=") +
        flName(actualFeatureLevel) + ";SM=" +
        (actualFeatureLevel >= D3D_FEATURE_LEVEL_11_0 ? "5.0" : "4.0") +
        ";DirectCompute=" + (computeShadersSupported_ ? "yes" : "no");
    driverVersion_ = driverVersion_.empty()
        ? capabilityTag : driverVersion_ + ";" + capabilityTag;
    if (!driverVersion_.empty())
        std::cout << "  Driver:        " << driverVersion_ << '\n';
    std::cout << std::flush;

    // Check tearing support
    ComPtr<IDXGIFactory5> factory5;
    if (SUCCEEDED(factory.As(&factory5))) {
        BOOL allow = FALSE;
        if (SUCCEEDED(factory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow, sizeof(allow))))
            tearingSupported_ = (allow == TRUE);
    }

    // Create swap chain (skip in headless mode)
    if (!config_.headless) {
        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.Width            = kWindowWidth;
        sd.Height           = kWindowHeight;
        sd.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount      = config_.framesInFlight + 1;
        sd.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        if (tearingSupported_)
            sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        ThrowIfFailed(factory->CreateSwapChainForHwnd(
            device_.Get(), hwnd, &sd, nullptr, nullptr, &swapChain_),
            "CreateSwapChainForHwnd failed");
    }
}

void DX11Backend::CreateRenderTarget() {
    ComPtr<ID3D11Texture2D> backBuffer;
    ThrowIfFailed(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)),
                  "GetBuffer failed");
    ThrowIfFailed(device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv_),
                  "CreateRenderTargetView failed");

    // Render3D needs a depth buffer for occlusion.
    if (config_.workload == Workload::Render3D) {
        D3D11_TEXTURE2D_DESC dd{};
        dd.Width      = kWindowWidth;
        dd.Height     = kWindowHeight;
        dd.MipLevels  = 1;
        dd.ArraySize  = 1;
        dd.Format     = DXGI_FORMAT_D32_FLOAT;
        dd.SampleDesc.Count = 1;
        dd.Usage      = D3D11_USAGE_DEFAULT;
        dd.BindFlags  = D3D11_BIND_DEPTH_STENCIL;
        ThrowIfFailed(device_->CreateTexture2D(&dd, nullptr, &depthTex_),
                      "CreateTexture2D (depth) failed");
        ThrowIfFailed(device_->CreateDepthStencilView(depthTex_.Get(), nullptr, &depthView_),
                      "CreateDepthStencilView failed");

        D3D11_DEPTH_STENCIL_DESC dsd{};
        dsd.DepthEnable    = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsd.DepthFunc      = D3D11_COMPARISON_LESS;
        ThrowIfFailed(device_->CreateDepthStencilState(&dsd, &depthState_),
                      "CreateDepthStencilState failed");
    }
}

void DX11Backend::CreateShaders() {
    const bool downlevel = featureLevel_ < D3D_FEATURE_LEVEL_11_0;
    if (!isFragmentOnlyWorkload(config_.workload)) {
        std::string csFile;
        if (config_.workload == Workload::SynthPeak) {
            if (config_.peakPrecision == Precision::FP16)
                throw std::runtime_error("SynthPeak FP16 is not supported on the DX backend (FXC); use Vulkan/Metal");
            if (downlevel && config_.peakPrecision == Precision::FP64)
                throw std::runtime_error("SynthPeak FP64 requires DX11 feature level 11_0 / shader model 5.0");
            csFile = (config_.peakPrecision == Precision::FP64)  ? "synthpeak_fp64.hlsl"
                   : (config_.peakPrecision == Precision::INT32) ? "synthpeak_int32.hlsl"
                   :                                               "synthpeak_fp32.hlsl";
        } else {
            csFile = (config_.workload == Workload::NBody) ? "nbody.hlsl" : "compute.hlsl";
        }
        auto csBlob = CompileShader(shaderDir_ + csFile, "CSMain",
                                    downlevel ? "cs_4_0" : "cs_5_0");
        ThrowIfFailed(device_->CreateComputeShader(
            csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &computeShader_),
            "CreateComputeShader failed");
    }

    if (!config_.headless) {
        const bool fractal   = (config_.workload == Workload::StressFractal);
        const bool gpuStress = (config_.workload == Workload::GpuStressV1);
        const bool gpuBurn   = isGpuBurnWorkload(config_.workload);
        const bool volumetric= (config_.workload == Workload::Volumetric);
        const bool render3d  = (config_.workload == Workload::Render3D);
        const char* burnFile = "gpu_burn.hlsl";
        const char* vsFile = gpuBurn ? burnFile
                             : gpuStress ? "gpu_stress.hlsl"
                            : (fractal || volumetric) ? (fractal ? "fractal.hlsl" : "volumetric.hlsl")
                            : render3d ? "render3d.hlsl" : "particle_vs.hlsl";
        const char* psFile = gpuBurn ? burnFile
                             : gpuStress ? "gpu_stress.hlsl"
                            : (fractal || volumetric) ? (fractal ? "fractal.hlsl" : "volumetric.hlsl")
                            : render3d ? "render3d.hlsl" : "particle_ps.hlsl";
        auto vsBlob = CompileShader(shaderDir_ + vsFile, "VSMain",
                                    downlevel ? "vs_4_0" : "vs_5_0");
        auto psBlob = CompileShader(shaderDir_ + psFile, "PSMain",
                                    downlevel ? "ps_4_0" : "ps_5_0");

        ThrowIfFailed(device_->CreateVertexShader(
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader_),
            "CreateVertexShader failed");

        ThrowIfFailed(device_->CreatePixelShader(
            psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader_),
            "CreatePixelShader failed");

        if (render3d) {
            // Slot 0 = quad corner (per-vertex); slot 1 = particle (per-instance).
            D3D11_INPUT_ELEMENT_DESC layout[] = {
                { "CORNER",   0, DXGI_FORMAT_R32G32_FLOAT,       0,  0, D3D11_INPUT_PER_VERTEX_DATA,   0 },
                { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
                { "VELOCITY", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
            };
            ThrowIfFailed(device_->CreateInputLayout(
                layout, _countof(layout),
                vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout_),
                "CreateInputLayout (render3d) failed");
        } else if (!fractal && !gpuStress && !gpuBurn && !volumetric) {
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "VELOCITY", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        ThrowIfFailed(device_->CreateInputLayout(
            layout, _countof(layout),
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout_),
            "CreateInputLayout failed");
        }

        // Blend state (alpha blending)
        D3D11_BLEND_DESC bd{};
        bd.RenderTarget[0].BlendEnable           = TRUE;
        bd.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
        bd.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        ThrowIfFailed(device_->CreateBlendState(&bd, &blendState_), "CreateBlendState failed");

        // Rasterizer state (no culling)
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.DepthClipEnable = TRUE;
        ThrowIfFailed(device_->CreateRasterizerState(&rd, &rasterizerState_),
                      "CreateRasterizerState failed");
    }
}

void DX11Backend::CreateParticleBuffers() {
    const UINT bufSize = sizeof(Particle) * config_.particleCount;

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = initialParticles_.data();

    // Structured buffer for compute (UAV)
    D3D11_BUFFER_DESC cbd{};
    cbd.ByteWidth           = bufSize;
    cbd.Usage               = D3D11_USAGE_DEFAULT;
    cbd.BindFlags           = D3D11_BIND_UNORDERED_ACCESS;
    cbd.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    cbd.StructureByteStride = sizeof(Particle);
    ThrowIfFailed(device_->CreateBuffer(&cbd, &initData, &computeBuffer_),
                  "CreateBuffer (compute) failed");

    // Create UAV for the structured buffer
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.ViewDimension      = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = config_.particleCount;
    ThrowIfFailed(device_->CreateUnorderedAccessView(
        computeBuffer_.Get(), &uavDesc, &computeUAV_),
        "CreateUnorderedAccessView failed");

    // Vertex buffer (copy destination each frame) -- not needed in headless
    if (!config_.headless) {
        D3D11_BUFFER_DESC vbd{};
        vbd.ByteWidth = bufSize;
        vbd.Usage     = D3D11_USAGE_DEFAULT;
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        ThrowIfFailed(device_->CreateBuffer(&vbd, &initData, &vertexBuffer_),
                      "CreateBuffer (vertex) failed");
    }

    // Render3D: static quad VBO + dynamic camera cbuffer.
    if (config_.workload == Workload::Render3D && !config_.headless) {
        const float quad[12] = { -1,-1, 1,-1, 1,1, -1,-1, 1,1, -1,1 };
        D3D11_BUFFER_DESC qd{};
        qd.ByteWidth = sizeof(quad);
        qd.Usage     = D3D11_USAGE_IMMUTABLE;
        qd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA qinit{}; qinit.pSysMem = quad;
        ThrowIfFailed(device_->CreateBuffer(&qd, &qinit, &quadBuffer_),
                      "CreateBuffer (quad) failed");

        D3D11_BUFFER_DESC ccb{};
        ccb.ByteWidth      = sizeof(Render3DParams);   // 112, multiple of 16
        ccb.Usage          = D3D11_USAGE_DYNAMIC;
        ccb.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        ccb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ThrowIfFailed(device_->CreateBuffer(&ccb, nullptr, &cam3dCB_),
                      "CreateBuffer (cam3d) failed");
    }

    std::cout << "Created particle buffers: " << config_.particleCount << " particles\n";
}

void DX11Backend::CreateComputeParamsCB() {
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth      = 16;  // must be 16-byte aligned
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ThrowIfFailed(device_->CreateBuffer(&bd, nullptr, &computeParamsCB_),
                  "CreateBuffer (CB) failed");
}

void DX11Backend::CreateTimestampQueries() {
    D3D11_QUERY_DESC dj{};
    dj.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

    D3D11_QUERY_DESC ts{};
    ts.Query = D3D11_QUERY_TIMESTAMP;

    for (UINT f = 0; f < kTimestampSlotCount; ++f) {
        if (FAILED(device_->CreateQuery(&dj, &disjointQueries_[f]))) {
            std::cout << "[Profiling] DX11 timestamp disjoint query creation failed -- disabled.\n";
            return;
        }
        for (UINT q = 0; q < kTimestampsPerFrame; ++q) {
            if (FAILED(device_->CreateQuery(&ts, &timestampQueries_[f][q]))) {
                std::cout << "[Profiling] DX11 timestamp query creation failed -- disabled.\n";
                return;
            }
        }
    }

    timestampsSupported_ = true;
    std::cout << "[Profiling] DX11 timestamp queries enabled.\n";
}

void DX11Backend::CollectTimestampResults() {
    if (!timestampsSupported_) return;

    // Non-blocking: poll every in-flight slot once per frame and free the
    // ones that resolved. Pending slots stay reserved (DrawFrame does not
    // reissue them), so results are never discarded. Headless has no
    // Present() to pace the CPU — the old retry/Sleep scheme either stalled
    // the frame loop or gave up and reused slots before they resolved,
    // permanently disabling timing at high frame rates.
    const auto declareUnavailable = [this] {
        timestampsSupported_ = false;
        std::cout << "[Profiling] DX11 timestamps permanently unavailable "
                     "(driver never resolves queries).\n";
    };

    for (UINT slot = 0; slot < kTimestampSlotCount; ++slot) {
        if (!slotPending_[slot]) continue;

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
        HRESULT hr = context_->GetData(disjointQueries_[slot].Get(),
                                       &disjoint, sizeof(disjoint),
                                       D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if (hr == S_FALSE) {
            if (++slotAge_[slot] > 8192) { declareUnavailable(); return; }
            continue;
        }
        if (FAILED(hr)) {
            slotPending_[slot] = false;
            slotAge_[slot] = 0;
            continue;
        }

        UINT64 ts[kTimestampsPerFrame]{};
        bool ready = true, failed = false;
        for (UINT i = 0; i < kTimestampsPerFrame && ready && !failed; ++i) {
            hr = context_->GetData(timestampQueries_[slot][i].Get(),
                                   &ts[i], sizeof(UINT64),
                                   D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (hr == S_FALSE) ready = false;
            else if (FAILED(hr)) failed = true;
        }
        if (failed) {
            slotPending_[slot] = false;
            slotAge_[slot] = 0;
            continue;
        }
        if (!ready) {
            if (++slotAge_[slot] > 8192) { declareUnavailable(); return; }
            continue;
        }

        slotPending_[slot] = false;
        slotAge_[slot] = 0;

        // Determine frequency: prefer this frame's, fall back to last good one.
        UINT64 freq = disjoint.Frequency;
        if (disjoint.Disjoint) {
            // GPU clock changed mid-frame.  Timestamps may be less accurate
            // but are still usable with the last known stable frequency.
            if (lastGoodFrequency_ > 0)
                freq = lastGoodFrequency_;
            // else: first frame and already disjoint — use reported freq anyway
        } else {
            lastGoodFrequency_ = freq;
        }
        if (freq == 0) continue;

        double toMs = 1000.0 / static_cast<double>(freq);
        double computeMs = static_cast<double>(ts[1] - ts[0]) * toMs;
        double renderMs  = static_cast<double>(ts[3] - ts[2]) * toMs;
        double totalMs   = static_cast<double>(ts[3] - ts[0]) * toMs;

        // Sanity check: discard obviously bogus samples (DX11 headless can
        // produce garbage when the driver mixes up query data across frames).
        if (computeMs > 1000.0 || renderMs > 1000.0 || totalMs > 1000.0)
            continue;
        if (computeMs < 0.0 || renderMs < 0.0 || totalMs < 0.0)
            continue;

        AccumulateTiming(computeMs, renderMs, totalMs);
    }
}

// -----------------------------------------------------------------------
// Frame
// -----------------------------------------------------------------------

void DX11Backend::DrawFrame(float deltaTime) {
    CollectTimestampResults();

    UINT slot = currentFrame_;
    // Reserve the slot only if its previous queries have been consumed.
    const bool issueTimestamps = timestampsSupported_ && !slotPending_[slot];

    // Begin timestamp disjoint
    if (issueTimestamps)
        context_->Begin(disjointQueries_[slot].Get());

    // --- Compute pass ---
    if (issueTimestamps)
        context_->End(timestampQueries_[slot][0].Get());

    if (fluid_.active) {
        RecordFluidFrame(deltaTime);
        return;
    }

    // Compute pass — skipped entirely for the fragment-only workloads.
    if (!isFragmentOnlyWorkload(config_.workload)) {
        // Update compute params
        D3D11_MAPPED_SUBRESOURCE mapped{};
        context_->Map(computeParamsCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (config_.workload == Workload::SynthPeak) {
            PeakParams params{ config_.peakIters, 0.9999f, 0.0001f, 0 };
            std::memcpy(mapped.pData, &params, sizeof(params));
        } else if (config_.workload == Workload::NBody) {
            NBodyParams params{ deltaTime, config_.softening, config_.particleCount, 0 };
            std::memcpy(mapped.pData, &params, sizeof(params));
        } else {
            ComputeParams params{ deltaTime, 0.9f };
            std::memcpy(mapped.pData, &params, sizeof(params));
        }
        context_->Unmap(computeParamsCB_.Get(), 0);

        context_->CSSetShader(computeShader_.Get(), nullptr, 0);
        ID3D11UnorderedAccessView* uavs[] = { computeUAV_.Get() };
        context_->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
        ID3D11Buffer* cbs[] = { computeParamsCB_.Get() };
        context_->CSSetConstantBuffers(0, 1, cbs);
        context_->Dispatch(config_.particleCount / kComputeWorkGroupSize, 1, 1);

        // Unbind UAV
        ID3D11UnorderedAccessView* nullUav[] = { nullptr };
        context_->CSSetUnorderedAccessViews(0, 1, nullUav, nullptr);
    }

    if (issueTimestamps)
        context_->End(timestampQueries_[slot][1].Get());

    if (config_.headless) {
        // Headless: mirror compute timestamps as render timestamps
        if (issueTimestamps) {
            context_->End(timestampQueries_[slot][2].Get());
            context_->End(timestampQueries_[slot][3].Get());
        }
    } else if (isFragmentOnlyWorkload(config_.workload)) {
        // --- Fragment-only pass (fullscreen triangle, no compute/VB) ---
        if (issueTimestamps)
            context_->End(timestampQueries_[slot][2].Get());

        const float clearColor[] = { 0.04f, 0.08f, 0.14f, 1.0f };
        context_->ClearRenderTargetView(rtv_.Get(), clearColor);
        context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), nullptr);
        context_->RSSetState(rasterizerState_.Get());

        D3D11_VIEWPORT vp{ 0, 0, static_cast<float>(kWindowWidth),
                           static_cast<float>(kWindowHeight), 0.0f, 1.0f };
        context_->RSSetViewports(1, &vp);

        context_->IASetInputLayout(nullptr);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
        context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
        ID3D11Buffer* pscb[] = { computeParamsCB_.Get() };
        context_->PSSetConstantBuffers(0, 1, pscb);

        // Reuse computeParamsCB_ (16 bytes) for the pixel-shader params.
        if (config_.workload == Workload::GpuStressV1) {
            for (std::uint32_t pass = 0; pass < kGpuStressV1DrawsPerFrame; ++pass) {
                GpuStressV1Params sp{ static_cast<float>(pass), 1.0f,
                                      config_.gpuStressIter, kGpuStressV1ShaderVersion };
                D3D11_MAPPED_SUBRESOURCE fm{};
                context_->Map(computeParamsCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &fm);
                std::memcpy(fm.pData, &sp, sizeof(sp));
                context_->Unmap(computeParamsCB_.Get(), 0);
                context_->Draw(3, 0);
            }
        } else if (isGpuBurnWorkload(config_.workload)) {
            fractalElapsed_ += deltaTime;
            for (std::uint32_t pass = 0; pass < gpuBurnDrawsPerFrame(config_.workload); ++pass) {
                GpuBurnParams bp{ fractalElapsed_, static_cast<float>(pass),
                                  config_.gpuBurnIter, gpuBurnShaderVersion(config_.workload) };
                D3D11_MAPPED_SUBRESOURCE fm{};
                context_->Map(computeParamsCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &fm);
                std::memcpy(fm.pData, &bp, sizeof(bp));
                context_->Unmap(computeParamsCB_.Get(), 0);
                context_->Draw(3, 0);
            }
        } else {
            fractalElapsed_ += deltaTime;
            D3D11_MAPPED_SUBRESOURCE fm{};
            context_->Map(computeParamsCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &fm);
            if (config_.workload == Workload::Volumetric) {
                VolumetricParams vol{ fractalElapsed_, 0.05f, config_.volumetricSteps, 0 };
                std::memcpy(fm.pData, &vol, sizeof(vol));
            } else {
                FractalParams fp{ fractalElapsed_, 1.0f, config_.fractalIter, 0 };
                std::memcpy(fm.pData, &fp, sizeof(fp));
            }
            context_->Unmap(computeParamsCB_.Get(), 0);
            context_->Draw(3, 0);
        }

        if (issueTimestamps)
            context_->End(timestampQueries_[slot][3].Get());
    } else if (config_.workload == Workload::Render3D) {
        // Copy compute buffer (updated particles) to the per-instance buffer.
        context_->CopyResource(vertexBuffer_.Get(), computeBuffer_.Get());

        if (issueTimestamps)
            context_->End(timestampQueries_[slot][2].Get());

        const float clearColor[] = { 0.04f, 0.08f, 0.14f, 1.0f };
        context_->ClearRenderTargetView(rtv_.Get(), clearColor);
        context_->ClearDepthStencilView(depthView_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), depthView_.Get());
        context_->OMSetDepthStencilState(depthState_.Get(), 0);
        context_->OMSetBlendState(blendState_.Get(), nullptr, 0xFFFFFFFF);
        context_->RSSetState(rasterizerState_.Get());

        D3D11_VIEWPORT vp{ 0, 0, static_cast<float>(kWindowWidth),
                           static_cast<float>(kWindowHeight), 0.0f, 1.0f };
        context_->RSSetViewports(1, &vp);

        // Update camera cbuffer.
        fractalElapsed_ += deltaTime;
        Render3DParams r3{};
        const float aspect = static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight);
        render3dCamera(fractalElapsed_, aspect, /*flipY*/false, /*z01*/true,
                       r3.viewProj, r3.camRight, r3.camUp);
        r3.pointSize = 0.02f;
        D3D11_MAPPED_SUBRESOURCE cm{};
        context_->Map(cam3dCB_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &cm);
        std::memcpy(cm.pData, &r3, sizeof(r3));
        context_->Unmap(cam3dCB_.Get(), 0);

        context_->IASetInputLayout(inputLayout_.Get());
        ID3D11Buffer* vbs[2] = { quadBuffer_.Get(), vertexBuffer_.Get() };
        UINT strides[2] = { sizeof(float) * 2, sizeof(Particle) };
        UINT offsets[2] = { 0, 0 };
        context_->IASetVertexBuffers(0, 2, vbs, strides, offsets);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
        context_->VSSetConstantBuffers(0, 1, cam3dCB_.GetAddressOf());
        context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
        context_->DrawInstanced(6, config_.particleCount, 0, 0);

        // Restore default (no depth) for any other passes.
        context_->OMSetDepthStencilState(nullptr, 0);

        if (issueTimestamps)
            context_->End(timestampQueries_[slot][3].Get());
    } else {
        // Copy compute buffer to vertex buffer
        context_->CopyResource(vertexBuffer_.Get(), computeBuffer_.Get());

        // --- Graphics pass ---
        if (issueTimestamps)
            context_->End(timestampQueries_[slot][2].Get());

        const float clearColor[] = { 0.04f, 0.08f, 0.14f, 1.0f };
        context_->ClearRenderTargetView(rtv_.Get(), clearColor);
        context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), nullptr);
        context_->OMSetBlendState(blendState_.Get(), nullptr, 0xFFFFFFFF);
        context_->RSSetState(rasterizerState_.Get());

        D3D11_VIEWPORT vp{ 0, 0, static_cast<float>(kWindowWidth),
                           static_cast<float>(kWindowHeight), 0.0f, 1.0f };
        context_->RSSetViewports(1, &vp);

        context_->IASetInputLayout(inputLayout_.Get());
        UINT stride = sizeof(Particle);
        UINT offset = 0;
        context_->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

        context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
        context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
        context_->Draw(config_.particleCount, 0);

        if (issueTimestamps)
            context_->End(timestampQueries_[slot][3].Get());
    }

    // End timestamp disjoint
    if (issueTimestamps)
        context_->End(disjointQueries_[slot].Get());

    if (config_.headless) {
        // Flush replaces Present() — submits queued GPU commands so
        // timestamp queries can resolve without stalling in Sleep() retries.
        context_->Flush();
    } else {
        UINT presentFlags = 0;
        if (!config_.vsync && tearingSupported_)
            presentFlags = DXGI_PRESENT_ALLOW_TEARING;
        swapChain_->Present(config_.vsync ? 1 : 0, presentFlags);
    }

    if (issueTimestamps) slotPending_[slot] = true;
    currentFrame_ = (currentFrame_ + 1) % kTimestampSlotCount;
}

void DX11Backend::WaitIdle() {
    if (context_) context_->Flush();
}

void DX11Backend::CleanupBackend() {
    WaitIdle();
    CleanupFluidResources();
}

void DX11Backend::CreateFluidResources() {
    fluid_.gridSize = config_.fluidGridSize;
    const std::uint32_t N = fluid_.gridSize;
    if (N < 64 || N > 512)
        throw std::invalid_argument("Fluid --grid must be between 64 and 512");
    if (config_.fluidJacobiIters < 1 || config_.fluidJacobiIters > 64)
        throw std::invalid_argument("Fluid --jacobi must be between 1 and 64");

    const UINT cellCount = N * N;
    const UINT stateBytes = sizeof(float) * 4 * cellCount;
    const UINT pressBytes = sizeof(float) * cellCount;

    std::vector<float> init(cellCount * 4);
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

    auto makeBuf = [&](UINT bytes, UINT stride, UINT bind, const void* data,
                       ComPtr<ID3D11Buffer>& buf,
                       ComPtr<ID3D11UnorderedAccessView>* uav,
                       ComPtr<ID3D11ShaderResourceView>* srv) {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = bytes;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = bind;
        bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bd.StructureByteStride = stride;
        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = data;
        ThrowIfFailed(device_->CreateBuffer(&bd, data ? &sd : nullptr, &buf),
                      "CreateBuffer (fluid) failed");
        if (uav) {
            D3D11_UNORDERED_ACCESS_VIEW_DESC ud{};
            ud.Format = DXGI_FORMAT_UNKNOWN;
            ud.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            ud.Buffer.NumElements = bytes / stride;
            ThrowIfFailed(device_->CreateUnorderedAccessView(buf.Get(), &ud, uav->GetAddressOf()),
                          "CreateUnorderedAccessView (fluid) failed");
        }
        if (srv) {
            D3D11_SHADER_RESOURCE_VIEW_DESC sdv{};
            sdv.Format = DXGI_FORMAT_UNKNOWN;
            sdv.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            sdv.Buffer.NumElements = bytes / stride;
            ThrowIfFailed(device_->CreateShaderResourceView(buf.Get(), &sdv, srv->GetAddressOf()),
                          "CreateShaderResourceView (fluid) failed");
        }
    };

    makeBuf(stateBytes, 16, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE,
            init.data(), fluid_.stateA, &fluid_.uavStateA, &fluid_.srvStateA);
    makeBuf(stateBytes, 16, D3D11_BIND_UNORDERED_ACCESS, nullptr, fluid_.stateB, &fluid_.uavStateB, nullptr);
    makeBuf(pressBytes, 4, D3D11_BIND_UNORDERED_ACCESS, nullptr, fluid_.pressA, &fluid_.uavPressA, nullptr);
    makeBuf(pressBytes, 4, D3D11_BIND_UNORDERED_ACCESS, nullptr, fluid_.pressB, &fluid_.uavPressB, nullptr);
    makeBuf(pressBytes, 4, D3D11_BIND_UNORDERED_ACCESS, nullptr, fluid_.divBuf, &fluid_.uavDiv, nullptr);

    const std::string hlsl = shaderDir_ + "fluid.hlsl";
    auto makeCs = [&](const char* entry, ComPtr<ID3D11ComputeShader>& out) {
        auto blob = CompileShader(hlsl, entry, "cs_5_0");
        ThrowIfFailed(device_->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(),
                                                   nullptr, &out),
                      (std::string("CreateComputeShader ") + entry).c_str());
    };
    makeCs("CSAdvect", fluid_.csAdvect);
    makeCs("CSDivergence", fluid_.csDiv);
    makeCs("CSJacobi", fluid_.csJacobi);
    makeCs("CSSubtract", fluid_.csSubtract);

    {
        auto vs = CompileShader(hlsl, "VSMain", "vs_5_0");
        auto ps = CompileShader(hlsl, "PSMain", "ps_5_0");
        ThrowIfFailed(device_->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(),
                                                  nullptr, &fluid_.vs),
                      "CreateVertexShader (fluid) failed");
        ThrowIfFailed(device_->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(),
                                                 nullptr, &fluid_.ps),
                      "CreatePixelShader (fluid) failed");
    }

    {
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = 16;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ThrowIfFailed(device_->CreateBuffer(&bd, nullptr, &fluid_.paramsCB),
                      "CreateBuffer (fluid params) failed");
        ThrowIfFailed(device_->CreateBuffer(&bd, nullptr, &fluid_.renderCB),
                      "CreateBuffer (fluid render) failed");
    }

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    ThrowIfFailed(device_->CreateRasterizerState(&rd, &fluid_.rast),
                  "CreateRasterizerState (fluid) failed");

    fluid_.simTime = 0.0f;
    fluid_.active = true;
}

void DX11Backend::CleanupFluidResources() {
    if (!fluid_.active) return;
    fluid_ = FluidResources{};
}

void DX11Backend::RecordFluidFrame(float deltaTime) {
    UINT slot = currentFrame_;
    // Reserve the slot only if its previous queries have been consumed.
    const bool issueTimestamps = timestampsSupported_ && !slotPending_[slot];
    const std::uint32_t N = fluid_.gridSize;
    const float stepDt = (std::min)((std::max)(deltaTime, 0.0f), 1.0f / 30.0f);
    fluid_.simTime += (std::min)((std::max)(deltaTime, 0.0f), 0.1f);
    const FluidParams fp{ stepDt, 1.0f / float(N), N, fluid_.simTime };
    const UINT groups = (N + 15u) / 16u;

    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        context_->Map(fluid_.paramsCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        std::memcpy(mapped.pData, &fp, sizeof(fp));
        context_->Unmap(fluid_.paramsCB.Get(), 0);
    }
    ID3D11Buffer* cbs[] = { fluid_.paramsCB.Get() };
    context_->CSSetConstantBuffers(0, 1, cbs);

    // Clear pressA
    const UINT zeros[4] = {0, 0, 0, 0};
    context_->ClearUnorderedAccessViewUint(fluid_.uavPressA.Get(), zeros);

    auto bind5 = [&](ID3D11UnorderedAccessView* u0, ID3D11UnorderedAccessView* u1,
                     ID3D11UnorderedAccessView* u2, ID3D11UnorderedAccessView* u3,
                     ID3D11UnorderedAccessView* u4) {
        ID3D11UnorderedAccessView* uavs[] = { u0, u1, u2, u3, u4 };
        context_->CSSetUnorderedAccessViews(0, 5, uavs, nullptr);
    };
    auto unbindCs = [&]() {
        ID3D11UnorderedAccessView* nulls[5] = {};
        context_->CSSetUnorderedAccessViews(0, 5, nulls, nullptr);
        context_->CSSetShader(nullptr, nullptr, 0);
    };

    context_->CSSetShader(fluid_.csAdvect.Get(), nullptr, 0);
    bind5(fluid_.uavStateA.Get(), fluid_.uavStateB.Get(),
          fluid_.uavPressA.Get(), fluid_.uavPressB.Get(), fluid_.uavDiv.Get());
    context_->Dispatch(groups, groups, 1);

    context_->CSSetShader(fluid_.csDiv.Get(), nullptr, 0);
    bind5(fluid_.uavStateB.Get(), fluid_.uavStateB.Get(),
          fluid_.uavPressA.Get(), fluid_.uavPressB.Get(), fluid_.uavDiv.Get());
    context_->Dispatch(groups, groups, 1);

    context_->CSSetShader(fluid_.csJacobi.Get(), nullptr, 0);
    for (std::uint32_t i = 0; i < config_.fluidJacobiIters; ++i) {
        const bool aToB = (i & 1u) == 0u;
        bind5(fluid_.uavStateA.Get(), fluid_.uavStateB.Get(),
              aToB ? fluid_.uavPressA.Get() : fluid_.uavPressB.Get(),
              aToB ? fluid_.uavPressB.Get() : fluid_.uavPressA.Get(),
              fluid_.uavDiv.Get());
        context_->Dispatch(groups, groups, 1);
    }

    ID3D11UnorderedAccessView* finalPress =
        (config_.fluidJacobiIters & 1u) ? fluid_.uavPressB.Get() : fluid_.uavPressA.Get();
    context_->CSSetShader(fluid_.csSubtract.Get(), nullptr, 0);
    bind5(fluid_.uavStateB.Get(), fluid_.uavStateA.Get(),
          finalPress, fluid_.uavPressB.Get(), fluid_.uavDiv.Get());
    context_->Dispatch(groups, groups, 1);
    unbindCs();

    if (issueTimestamps)
        context_->End(timestampQueries_[slot][1].Get());

    const float clearColor[] = { 0.04f, 0.08f, 0.14f, 1.0f };
    context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(rtv_.Get(), clearColor);

    if (issueTimestamps)
        context_->End(timestampQueries_[slot][2].Get());

    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(kWindowWidth);
    vp.Height = static_cast<float>(kWindowHeight);
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);
    context_->RSSetState(fluid_.rast.Get());

    FluidRenderParams rp{ N, fluid_.simTime, 0.88f, 1u };
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        context_->Map(fluid_.renderCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        std::memcpy(mapped.pData, &rp, sizeof(rp));
        context_->Unmap(fluid_.renderCB.Get(), 0);
    }

    context_->IASetInputLayout(nullptr);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(fluid_.vs.Get(), nullptr, 0);
    context_->PSSetShader(fluid_.ps.Get(), nullptr, 0);
    ID3D11ShaderResourceView* srvs[] = { fluid_.srvStateA.Get() };
    context_->PSSetShaderResources(0, 1, srvs);
    ID3D11Buffer* pcb[] = { fluid_.renderCB.Get() };
    context_->PSSetConstantBuffers(0, 1, pcb);
    context_->VSSetConstantBuffers(0, 1, pcb);
    context_->Draw(3, 0);

    ID3D11ShaderResourceView* nullSrv[] = { nullptr };
    context_->PSSetShaderResources(0, 1, nullSrv);

    if (issueTimestamps)
        context_->End(timestampQueries_[slot][3].Get());
    if (issueTimestamps)
        context_->End(disjointQueries_[slot].Get());

    UINT presentFlags = 0;
    if (!config_.vsync && tearingSupported_)
        presentFlags = DXGI_PRESENT_ALLOW_TEARING;
    swapChain_->Present(config_.vsync ? 1 : 0, presentFlags);

    if (issueTimestamps) slotPending_[slot] = true;
    currentFrame_ = (currentFrame_ + 1) % kTimestampSlotCount;
}

}  // namespace gpu_bench

#endif  // HAVE_DX11
