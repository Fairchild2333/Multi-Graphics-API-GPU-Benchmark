#ifdef HAVE_METAL

#include "metal_backend.h"
#include "metal_cinematic_liquid.h"
#include "mini_mat.h"

#if !defined(GPU_BENCH_NO_GLFW)
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif

#import <Metal/Metal.h>
#import <TargetConditionals.h>
#import <QuartzCore/CAMetalLayer.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gpu_bench {
namespace {

void ConfigureFastMath(MTLCompileOptions* options) {
    if (@available(macOS 15.0, iOS 18.0, *)) {
        options.mathMode = MTLMathModeFast;
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        options.fastMathEnabled = YES;
#pragma clang diagnostic pop
    }
}

}  // namespace

// -----------------------------------------------------------------------
// Pimpl — all Objective-C objects live here so the header stays pure C++
// -----------------------------------------------------------------------

struct MetalBackend::Impl {
    id<MTLDevice>               device       = nil;
    id<MTLCommandQueue>         commandQueue = nil;
    id<MTLLibrary>              library      = nil;
    id<MTLComputePipelineState> computePSO   = nil;
    id<MTLRenderPipelineState>  renderPSO    = nil;
    id<MTLBuffer>               particleBuf  = nil;
    CAMetalLayer*               metalLayer   = nil;
    id<MTLTexture>              depthTex     = nil;   // Render3D
    id<MTLDepthStencilState>    depthState   = nil;   // Render3D

    std::string deviceName;
    std::uint32_t framesInFlight = kMaxFramesInFlight;

    // Semaphore to limit frames in flight
    dispatch_semaphore_t frameSemaphore = nullptr;

    // Per-frame timing collected asynchronously via addCompletedHandler
    struct FrameResources {
        id<MTLCommandBuffer> cmdBuf = nil;
        double computeStartTime = 0;
        double computeEndTime   = 0;
    };
    std::vector<FrameResources> frames{};
    std::uint32_t currentFrame = 0;
    std::uint64_t totalFrames  = 0;

    // Async timing collection
    std::mutex timingMutex;
    struct PendingTiming {
        double computeMs;
        double renderMs;
        double totalMs;
    };
    std::vector<PendingTiming> pendingTimings;

    // Cinematic Liquid v2 (MLS-MPM) Metal preview host; null when unused.
    std::unique_ptr<MetalLiquidV2Host> liquid;

    // MTLCaptureManager session (one capture at a time).
    bool capturing = false;
    std::string captureOutputPath;

    // --- Decoupled presenter (windowed, VSync off) --------------------------
    // `nextDrawable` blocks until the display releases a drawable, so any
    // thread that calls it is pinned to the refresh rate — on a 120 Hz
    // ProMotion panel that caps the benchmark no matter what
    // `displaySyncEnabled = NO` says. The benchmark thread therefore never
    // touches a drawable: it renders every frame into one of these textures,
    // and a presenter thread blits the newest finished one onto a drawable at
    // whatever rate the display allows. Render throughput becomes independent
    // of refresh, and the preview shows the latest completed frame instead of
    // an arbitrary 1-in-N subset.
    enum class SlotState { Free, Rendering, Ready, Presenting };
    std::vector<id<MTLTexture>> presentTextures;
    std::vector<SlotState>      presentSlotState;
    id<MTLCommandQueue>         presentQueue = nil;
    std::thread                 presentThread;
    std::atomic<bool>           presentRunning{false};
    std::mutex                  presentMutex;

    bool presenterActive() const { return !presentTextures.empty(); }

    /// Claim a texture the presenter is not using. Never returns -1 while the
    /// ring is larger than framesInFlight + 2.
    int acquireRenderSlot() {
        std::lock_guard<std::mutex> lock(presentMutex);
        for (std::size_t i = 0; i < presentSlotState.size(); ++i) {
            if (presentSlotState[i] == SlotState::Free) {
                presentSlotState[i] = SlotState::Rendering;
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    /// Called from the frame's completion handler: the texture now holds a
    /// finished frame. Only the newest one is kept queued, so a fast benchmark
    /// never makes the presenter fall behind showing stale frames.
    void publishRenderSlot(int slot) {
        if (slot < 0) return;
        std::lock_guard<std::mutex> lock(presentMutex);
        if (slot >= static_cast<int>(presentSlotState.size())) return;
        for (auto& state : presentSlotState)
            if (state == SlotState::Ready) state = SlotState::Free;
        presentSlotState[slot] = SlotState::Ready;
    }

    void startPresenter() {
        presentRunning.store(true, std::memory_order_relaxed);
        presentThread = std::thread([this]() { presenterLoop(); });
    }

    void stopPresenter() {
        if (!presentThread.joinable()) return;
        presentRunning.store(false, std::memory_order_relaxed);
        // The loop may be parked inside nextDrawable; allowsNextDrawableTimeout
        // guarantees it returns rather than hanging the join.
        presentThread.join();
    }

    void presenterLoop() {
        while (presentRunning.load(std::memory_order_relaxed)) {
            @autoreleasepool {
                int slot = -1;
                {
                    std::lock_guard<std::mutex> lock(presentMutex);
                    for (std::size_t i = 0; i < presentSlotState.size(); ++i) {
                        if (presentSlotState[i] == SlotState::Ready) {
                            presentSlotState[i] = SlotState::Presenting;
                            slot = static_cast<int>(i);
                            break;
                        }
                    }
                }
                if (slot < 0) {
                    // Workload slower than the display: nothing new to show, so
                    // idle briefly instead of spinning on the drawable pool.
                    std::this_thread::sleep_for(std::chrono::microseconds(300));
                    continue;
                }

                // This is the only nextDrawable call in the backend, and it is
                // deliberately on this thread: blocking here paces the preview
                // to the refresh rate without touching benchmark throughput.
                id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
                if (drawable) {
                    id<MTLCommandBuffer> blitCB = [presentQueue commandBuffer];
                    id<MTLBlitCommandEncoder> blit = [blitCB blitCommandEncoder];
                    [blit copyFromTexture:presentTextures[slot]
                              sourceSlice:0
                              sourceLevel:0
                             sourceOrigin:MTLOriginMake(0, 0, 0)
                               sourceSize:MTLSizeMake(kWindowWidth, kWindowHeight, 1)
                                toTexture:drawable.texture
                         destinationSlice:0
                         destinationLevel:0
                        destinationOrigin:MTLOriginMake(0, 0, 0)];
                    [blit endEncoding];
                    [blitCB presentDrawable:drawable];
                    [blitCB commit];
                    // Hold the slot until the GPU has actually read it.
                    [blitCB waitUntilCompleted];
                }

                std::lock_guard<std::mutex> lock(presentMutex);
                presentSlotState[slot] = SlotState::Free;
            }
        }
    }
};

// -----------------------------------------------------------------------
// Construction / destruction
// -----------------------------------------------------------------------

MetalBackend::MetalBackend(std::int32_t gpuIndex, std::string shaderDir,
                           BenchmarkConfig config)
    : AppBase(gpuIndex, std::move(shaderDir), config),
      impl_(std::make_unique<Impl>()) {}

MetalBackend::~MetalBackend() = default;

std::string MetalBackend::GetDeviceName() const {
    return impl_ ? impl_->deviceName : "";
}

std::string MetalBackend::GetDriverVersion() const {
    NSProcessInfo* pi = [NSProcessInfo processInfo];
    NSOperatingSystemVersion v = [pi operatingSystemVersion];
#if TARGET_OS_IOS
    std::string prefix = "iOS ";
#else
    std::string prefix = "macOS ";
#endif
    return prefix + std::to_string(v.majorVersion) + "."
         + std::to_string(v.minorVersion) + "."
         + std::to_string(v.patchVersion);
}

bool MetalBackend::SupportsNativeGpuCapture() const {
    if (!impl_ || !impl_->commandQueue)
        return false;
    @autoreleasepool {
        MTLCaptureManager* mgr = [MTLCaptureManager sharedCaptureManager];
        return mgr != nil &&
               [mgr supportsDestination:MTLCaptureDestinationGPUTraceDocument];
    }
}

bool MetalBackend::BeginNativeGpuCapture(const std::string& outputPath) {
    if (!impl_ || !impl_->commandQueue || impl_->capturing)
        return false;
    @autoreleasepool {
        MTLCaptureManager* mgr = [MTLCaptureManager sharedCaptureManager];
        if (!mgr ||
            ![mgr supportsDestination:MTLCaptureDestinationGPUTraceDocument])
            return false;

        std::string path = outputPath;
        if (path.size() < 10 ||
            path.substr(path.size() - 10) != ".gputrace")
            path += ".gputrace";

        NSURL* url = [NSURL fileURLWithPath:
            [NSString stringWithUTF8String:path.c_str()]];
        MTLCaptureDescriptor* desc = [[MTLCaptureDescriptor alloc] init];
        desc.captureObject = impl_->commandQueue;
        desc.destination = MTLCaptureDestinationGPUTraceDocument;
        desc.outputURL = url;

        NSError* error = nil;
        if (![mgr startCaptureWithDescriptor:desc error:&error]) {
            std::string msg = error
                ? [[error localizedDescription] UTF8String]
                : "unknown";
            std::cout << "[Capture] MTLCaptureManager start failed: " << msg
                      << "\n";
            return false;
        }
        impl_->capturing = true;
        impl_->captureOutputPath = path;
        return true;
    }
}

bool MetalBackend::EndNativeGpuCapture(std::string& outPath) {
    outPath.clear();
    if (!impl_ || !impl_->capturing)
        return false;
    @autoreleasepool {
        MTLCaptureManager* mgr = [MTLCaptureManager sharedCaptureManager];
        if (mgr && mgr.isCapturing)
            [mgr stopCapture];
        outPath = impl_->captureOutputPath;
        impl_->capturing = false;
        impl_->captureOutputPath.clear();
        return !outPath.empty();
    }
}

// -----------------------------------------------------------------------
// Initialisation
// -----------------------------------------------------------------------

void MetalBackend::InitBackend() {
    if (config_.workload == Workload::Fluid) {
        throw std::runtime_error(
            "Fluid is an unverified Vulkan-only Developer Preview; Metal fallback is disabled");
    }
    if (config_.workload == Workload::CinematicLiquidV1) {
        throw std::runtime_error(
            "Cinematic Liquid v1 is Vulkan-only; Metal hosts v2 MLS-MPM preview only");
    }
    if (config_.workload == Workload::GpuStressV1) {
        throw std::runtime_error(
            "GPU Stress v1 is not supported on Metal; use Vulkan, DX12, DX11, or OpenGL");
    }
    @autoreleasepool {
        // --- Device selection ---------------------------------------------------
#if TARGET_OS_IPHONE
        // iOS has a single system GPU; MTLCopyAllDevices is macOS-only.
        id<MTLDevice> systemDevice = MTLCreateSystemDefaultDevice();
        if (!systemDevice)
            throw std::runtime_error("No Metal device found");
        impl_->device = systemDevice;
        impl_->deviceName = [impl_->device.name UTF8String];
        std::cout << "Selected GPU: " << impl_->deviceName << std::endl;
#else
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
#pragma clang diagnostic pop

        if (devices.count == 0)
            throw std::runtime_error("No Metal device found");

        std::cout << "Available GPUs:\n";
        for (NSUInteger i = 0; i < devices.count; ++i) {
            id<MTLDevice> dev = devices[i];
            std::uint64_t vramMB = dev.recommendedMaxWorkingSetSize / (1024 * 1024);
            std::string tag;
            if (dev.isLowPower) tag = " (low-power / iGPU)";
            else if (dev.isHeadless) tag = " (headless / compute-only)";
            std::cout << "  [" << i << "] " << [dev.name UTF8String]
                      << " — " << vramMB << " MB" << tag << '\n';
        }

        NSUInteger chosen = 0;
        if (requestedGpuIndex_ >= 0) {
            auto idx = static_cast<NSUInteger>(requestedGpuIndex_);
            if (idx >= devices.count)
                throw std::runtime_error("Requested GPU index out of range");
            chosen = idx;
        } else if (devices.count > 1) {
            std::cout << "Enter GPU index (or 'b' to go back): " << std::flush;
            std::string line;
            if (std::getline(std::cin, line)) {
                if (line == "b" || line == "B")
                    throw gpu_bench::BackToMenuException();
                if (!line.empty())
                    chosen = static_cast<NSUInteger>(std::stoi(line));
            }
        }

        impl_->device     = devices[chosen];
        impl_->deviceName = [impl_->device.name UTF8String];
        std::cout << "Selected GPU [" << chosen << "]: " << impl_->deviceName
                  << std::endl;
#endif

        // --- Frames-in-flight from config (respects --flights) ------------------
        impl_->framesInFlight = config_.framesInFlight;
        impl_->frames.resize(impl_->framesInFlight);

#if !defined(GPU_BENCH_NO_GLFW)
        if (!config_.headless) {
            NSWindow* nsWindow = glfwGetCocoaWindow(window_);
            impl_->metalLayer  = [CAMetalLayer layer];
            impl_->metalLayer.device       = impl_->device;
            impl_->metalLayer.pixelFormat  = MTLPixelFormatBGRA8Unorm;
            impl_->metalLayer.drawableSize = CGSizeMake(kWindowWidth, kWindowHeight);
            impl_->metalLayer.framebufferOnly = YES;
            impl_->metalLayer.maximumDrawableCount = 3;
            impl_->metalLayer.displaySyncEnabled = config_.vsync ? YES : NO;

            nsWindow.contentView.layer     = impl_->metalLayer;
            nsWindow.contentView.wantsLayer = YES;
        }
#else
        if (!config_.headless && window_ != nullptr) {
            impl_->metalLayer = (__bridge CAMetalLayer*)window_;
            impl_->metalLayer.device       = impl_->device;
            impl_->metalLayer.pixelFormat  = MTLPixelFormatBGRA8Unorm;
            impl_->metalLayer.drawableSize = CGSizeMake(kWindowWidth, kWindowHeight);
            impl_->metalLayer.framebufferOnly = YES;
            impl_->metalLayer.maximumDrawableCount = 3;
        }
#endif

        // --- Frame semaphore ----------------------------------------------------
        impl_->frameSemaphore = dispatch_semaphore_create(impl_->framesInFlight);

        // --- Command queue ------------------------------------------------------
        impl_->commandQueue = [impl_->device newCommandQueue];

        auto enableOffscreenIfNeeded = [&]() {
            if (config_.headless || config_.vsync || !impl_->metalLayer)
                return;

            // framebufferOnly must be off so the drawable can be a blit
            // destination for the presenter.
            impl_->metalLayer.framebufferOnly = NO;
            impl_->metalLayer.allowsNextDrawableTimeout = YES;

            const std::size_t slots = impl_->framesInFlight + 3;
            impl_->presentTextures.reserve(slots);
            for (std::size_t i = 0; i < slots; ++i) {
                MTLTextureDescriptor* texDesc =
                    [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                       width:kWindowWidth
                                                                      height:kWindowHeight
                                                                   mipmapped:NO];
                texDesc.usage = MTLTextureUsageRenderTarget;
                texDesc.storageMode = MTLStorageModePrivate;
                id<MTLTexture> tex = [impl_->device newTextureWithDescriptor:texDesc];
                if (!tex)
                    throw std::runtime_error("Failed to create present texture");
                impl_->presentTextures.push_back(tex);
            }
            impl_->presentSlotState.assign(slots, Impl::SlotState::Free);

            // A dedicated queue keeps the blit out of the timed queue, so the
            // preview never serialises behind benchmark work.
            impl_->presentQueue = [impl_->device newCommandQueue];
            impl_->startPresenter();

            std::cout << "[Metal] Decoupled presenter: benchmark renders offscreen "
                         "uncapped, preview presents at display refresh\n";
        };

        // --- Cinematic Liquid v2 (separate library / host) -----------------------
        if (config_.workload == Workload::CinematicLiquid) {
            if (config_.liquidSolverSph) {
                throw std::runtime_error(
                    "Cinematic Liquid SPH is not implemented on Metal; use --liquid-solver mpm "
                    "or Vulkan");
            }
            impl_->liquid = std::make_unique<MetalLiquidV2Host>();
            impl_->liquid->init((__bridge void*)impl_->device,
                                shaderDir_ + "cinematic_liquid_v2.metal");
            config_.particleCount = impl_->liquid->particleCount();
            config_.memoryLabelOverride = "Unified-memory";
            enableOffscreenIfNeeded();
            std::cout << "[Profiling] GPU command-buffer timestamps enabled\n";
            return;
        }

        // --- Compile Metal shader library from source ---------------------------
        const bool gpuBurn = isGpuBurnWorkload(config_.workload);
        std::string shaderPath = shaderDir_ + (gpuBurn ? "gpu_burn.metal" : "particle.metal");
        auto shaderSource = ReadFileBytes(shaderPath);
        NSString* source =
            [[NSString alloc] initWithBytes:shaderSource.data()
                                     length:shaderSource.size()
                                   encoding:NSUTF8StringEncoding];

        MTLCompileOptions* opts = [[MTLCompileOptions alloc] init];
        ConfigureFastMath(opts);

        NSError* error = nil;
        impl_->library = [impl_->device newLibraryWithSource:source
                                                     options:opts
                                                       error:&error];
        if (!impl_->library) {
            std::string msg = error
                ? [[error localizedDescription] UTF8String]
                : "unknown error";
            throw std::runtime_error("Metal shader compilation failed (" +
                                     shaderPath + "): " + msg);
        }

        if (!gpuBurn) {
            // --- Compute pipeline (particle / nbody / synthpeak) ----------------
            NSString* kernelName = @"computeMain";
            if (config_.workload == Workload::NBody) {
                kernelName = @"nbodyMain";
            } else if (config_.workload == Workload::SynthPeak) {
                switch (config_.peakPrecision) {
                    case Precision::FP16:  kernelName = @"synthFp16";  break;
                    case Precision::INT32: kernelName = @"synthInt32"; break;
                    case Precision::FP64:
                        throw std::runtime_error("SynthPeak FP64 is not supported on Apple GPUs (no double); use Vulkan");
                    default:               kernelName = @"synthFp32";  break;
                }
            }
            id<MTLFunction> computeFunc =
                [impl_->library newFunctionWithName:kernelName];
            if (!computeFunc)
                throw std::runtime_error("compute kernel function not found in shader");

            impl_->computePSO =
                [impl_->device newComputePipelineStateWithFunction:computeFunc
                                                             error:&error];
            if (!impl_->computePSO) {
                std::string msg = error
                    ? [[error localizedDescription] UTF8String]
                    : "unknown error";
                throw std::runtime_error("Compute pipeline creation failed: " + msg);
            }
        }

        // --- Render pipeline (skipped in headless mode) -------------------------
        if (!config_.headless) {
            const bool fractal   = (config_.workload == Workload::StressFractal);
            const bool volumetric= (config_.workload == Workload::Volumetric);
            const bool render3d  = (config_.workload == Workload::Render3D);
            NSString* vfn = gpuBurn ? @"gpuBurnVertex"
                         : fractal ? @"fractalVertex"
                         : volumetric ? @"volumetricVertex"
                         : render3d ? @"render3dVertex" : @"vertexMain";
            NSString* ffn = gpuBurn ? @"gpuBurnFragment"
                         : fractal ? @"fractalFragment"
                         : volumetric ? @"volumetricFragment"
                         : render3d ? @"render3dFragment" : @"fragmentMain";
            id<MTLFunction> vertFunc = [impl_->library newFunctionWithName:vfn];
            id<MTLFunction> fragFunc = [impl_->library newFunctionWithName:ffn];
            if (!vertFunc || !fragFunc)
                throw std::runtime_error("Vertex/fragment functions not found");

            MTLRenderPipelineDescriptor* rpDesc =
                [[MTLRenderPipelineDescriptor alloc] init];
            rpDesc.vertexFunction   = vertFunc;
            rpDesc.fragmentFunction = fragFunc;
            if (render3d)
                rpDesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

            MTLRenderPipelineColorAttachmentDescriptor* ca =
                rpDesc.colorAttachments[0];
            ca.pixelFormat = MTLPixelFormatBGRA8Unorm;
            if (gpuBurn) {
                // Opaque two-pass burn (pass1 uses discard); no alpha blend.
                ca.blendingEnabled = NO;
            } else {
                ca.blendingEnabled             = YES;
                ca.sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
                ca.destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
                ca.rgbBlendOperation           = MTLBlendOperationAdd;
                ca.sourceAlphaBlendFactor      = MTLBlendFactorOne;
                ca.destinationAlphaBlendFactor = MTLBlendFactorZero;
                ca.alphaBlendOperation         = MTLBlendOperationAdd;
            }

            impl_->renderPSO =
                [impl_->device newRenderPipelineStateWithDescriptor:rpDesc
                                                              error:&error];
            if (!impl_->renderPSO) {
                std::string msg = error
                    ? [[error localizedDescription] UTF8String]
                    : "unknown error";
                throw std::runtime_error("Render pipeline creation failed: " + msg);
            }

            // --- Render3D depth resources ----------------------------------------
            if (render3d) {
                MTLTextureDescriptor* dtd =
                    [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                       width:kWindowWidth
                                                                      height:kWindowHeight
                                                                   mipmapped:NO];
                dtd.usage       = MTLTextureUsageRenderTarget;
                dtd.storageMode = MTLStorageModePrivate;
                impl_->depthTex = [impl_->device newTextureWithDescriptor:dtd];

                MTLDepthStencilDescriptor* dsd = [[MTLDepthStencilDescriptor alloc] init];
                dsd.depthCompareFunction = MTLCompareFunctionLess;
                dsd.depthWriteEnabled    = YES;
                impl_->depthState = [impl_->device newDepthStencilStateWithDescriptor:dsd];
            }
        }

        if (!gpuBurn) {
            // --- Particle buffer (Shared / UMA — Apple Silicon default path) ----
            const NSUInteger bufSize = sizeof(Particle) * config_.particleCount;
            impl_->particleBuf =
                [impl_->device newBufferWithBytes:initialParticles_.data()
                                           length:bufSize
                                          options:MTLResourceStorageModeShared];
            if (!impl_->particleBuf)
                throw std::runtime_error("Failed to create particle buffer");

            // Honest score metadata: Shared buffers are not discrete VRAM.
            config_.memoryLabelOverride = "Unified-memory";

            std::cout << "Created particle buffer: " << config_.particleCount
                      << " particles (MTLResourceStorageModeShared / Unified-memory)\n";
        } else {
            std::cout << "[Metal] GPU Burn: fullscreen raymarch ("
                      << gpuBurnDrawsPerFrame(config_.workload)
                      << " draws/frame, steps=" << config_.gpuBurnIter << ")\n";
        }
        std::cout << "[Profiling] GPU command-buffer timestamps enabled\n";

        // --- Offscreen render target (timed / frame bench, VSync off) -----------
        // ProMotion can lock nextDrawable to refresh even with
        // displaySyncEnabled=NO. Formal 15s time-mode (not only --benchmark)
        // must use an offscreen target and present periodically so throughput
        // stays comparable to Windows vsync-off runs.
        enableOffscreenIfNeeded();

        if (config_.headless)
            std::cout << "[Metal] Headless mode: pure compute, no rendering\n";
    }
}

// -----------------------------------------------------------------------
// Per-frame rendering
// -----------------------------------------------------------------------

void MetalBackend::DrawFrame(float deltaTime) {
    @autoreleasepool {
        // Drain any async timing results collected by completed handlers
        {
            std::lock_guard<std::mutex> lock(impl_->timingMutex);
            for (auto& t : impl_->pendingTimings) {
                AccumulateTiming(t.computeMs, t.renderMs, t.totalMs);
            }
            impl_->pendingTimings.clear();
        }

        // Wait for a frame slot to become available (non-blocking if GPU is fast)
        dispatch_semaphore_wait(impl_->frameSemaphore, DISPATCH_TIME_FOREVER);

        const std::uint32_t frameSlot = impl_->currentFrame;

        // === HEADLESS PATH: compute only, no render =============================
        if (config_.headless) {
            id<MTLCommandBuffer> computeCB = [impl_->commandQueue commandBuffer];

            id<MTLComputeCommandEncoder> computeEnc =
                [computeCB computeCommandEncoder];

            [computeEnc setComputePipelineState:impl_->computePSO];
            [computeEnc setBuffer:impl_->particleBuf offset:0 atIndex:0];

            if (config_.workload == Workload::SynthPeak) {
                PeakParams params{ config_.peakIters, 0.9999f, 0.0001f, 0 };
                [computeEnc setBytes:&params length:sizeof(PeakParams) atIndex:1];
            } else if (config_.workload == Workload::NBody) {
                NBodyParams params{ deltaTime, config_.softening, config_.particleCount, 0 };
                [computeEnc setBytes:&params length:sizeof(NBodyParams) atIndex:1];
            } else {
                ComputeParams params{ deltaTime, 0.9f };
                [computeEnc setBytes:&params length:sizeof(ComputeParams) atIndex:1];
            }

            const MTLSize tgSize  = MTLSizeMake(kComputeWorkGroupSize, 1, 1);
            const MTLSize tgCount = MTLSizeMake(
                config_.particleCount / kComputeWorkGroupSize, 1, 1);
            [computeEnc dispatchThreadgroups:tgCount
                       threadsPerThreadgroup:tgSize];
            [computeEnc endEncoding];

            // Async timing + semaphore signal for headless
            __block auto* implPtr = impl_.get();
            dispatch_semaphore_t sem = impl_->frameSemaphore;

            [computeCB addCompletedHandler:^(id<MTLCommandBuffer> cb) {
                dispatch_semaphore_signal(sem);

                const double cs = cb.GPUStartTime;
                const double ce = cb.GPUEndTime;

                if (ce > cs) {
                    const double computeMs = (ce - cs) * 1000.0;
                    // Headless: no render, timing mirrors compute
                    std::lock_guard<std::mutex> lock(implPtr->timingMutex);
                    implPtr->pendingTimings.push_back({computeMs, 0.0, computeMs});
                }
            }];

            [computeCB commit];

            ++impl_->totalFrames;
            impl_->currentFrame =
                (frameSlot + 1) % impl_->framesInFlight;
            return;
        }

        // === WINDOWED PATH ======================================================

        // --- Determine render target --------------------------------------------
        const bool usePresenter = impl_->presenterActive();

        id<CAMetalDrawable> drawable = nil;
        id<MTLTexture> renderTarget = nil;
        int presentSlot = -1;

        if (usePresenter) {
            // Never acquire a drawable here — that is what pinned the whole
            // benchmark to the display refresh. Render into the ring and let
            // the presenter thread deal with the swapchain.
            presentSlot = impl_->acquireRenderSlot();
            if (presentSlot >= 0)
                renderTarget = impl_->presentTextures[presentSlot];
        } else {
            // VSync on: present directly, pacing to the display is the point.
            drawable = [impl_->metalLayer nextDrawable];
            if (drawable)
                renderTarget = drawable.texture;
        }

        if (!renderTarget) {
            dispatch_semaphore_signal(impl_->frameSemaphore);
            return;
        }

        // --- Cinematic Liquid v2 preview ----------------------------------------
        if (impl_->liquid && impl_->liquid->active()) {
            void* computeCBPtr = nullptr;
            void* renderCBPtr = nullptr;
            impl_->liquid->encodeFrame(
                (__bridge void*)impl_->commandQueue,
                deltaTime,
                (__bridge void*)renderTarget,
                drawable ? (__bridge void*)drawable : nullptr,
                &computeCBPtr,
                &renderCBPtr);

            __block auto* implPtr = impl_.get();
            __block id<MTLCommandBuffer> capturedComputeCB =
                (__bridge_transfer id<MTLCommandBuffer>)computeCBPtr;
            id<MTLCommandBuffer> renderCB =
                (__bridge_transfer id<MTLCommandBuffer>)renderCBPtr;
            dispatch_semaphore_t sem = impl_->frameSemaphore;

            __block int publishSlot = presentSlot;
            id<MTLCommandBuffer> timingCB = renderCB ? renderCB : capturedComputeCB;
            [timingCB addCompletedHandler:^(id<MTLCommandBuffer> cb) {
                dispatch_semaphore_signal(sem);
                implPtr->publishRenderSlot(publishSlot);
                const double rs = cb.GPUStartTime;
                const double re = cb.GPUEndTime;
                if (!(re > rs))
                    return;
                double computeMs = 0.0;
                double renderMs = (re - rs) * 1000.0;
                double totalMs = renderMs;
                if (capturedComputeCB && renderCB) {
                    const double cs = capturedComputeCB.GPUStartTime;
                    const double ce = capturedComputeCB.GPUEndTime;
                    if (ce > cs) {
                        computeMs = (ce - cs) * 1000.0;
                        totalMs = (re - cs) * 1000.0;
                        renderMs = (re - rs) * 1000.0;
                    }
                } else if (capturedComputeCB && !renderCB) {
                    computeMs = (re - rs) * 1000.0;
                    renderMs = 0.0;
                    totalMs = computeMs;
                }
                std::lock_guard<std::mutex> lock(implPtr->timingMutex);
                implPtr->pendingTimings.push_back({computeMs, renderMs, totalMs});
            }];

            [capturedComputeCB commit];
            if (renderCB)
                [renderCB commit];

            ++impl_->totalFrames;
            impl_->currentFrame = (frameSlot + 1) % impl_->framesInFlight;
            return;
        }

        const bool gpuBurn  = isGpuBurnWorkload(config_.workload);
        const bool render3d = (config_.workload == Workload::Render3D);

        // --- Optional compute (particle / nbody / synthpeak); GPU Burn skips ---
        id<MTLCommandBuffer> computeCB = nil;
        if (!gpuBurn) {
            computeCB = [impl_->commandQueue commandBuffer];
            id<MTLComputeCommandEncoder> computeEnc =
                [computeCB computeCommandEncoder];

            [computeEnc setComputePipelineState:impl_->computePSO];
            [computeEnc setBuffer:impl_->particleBuf offset:0 atIndex:0];

            if (config_.workload == Workload::SynthPeak) {
                PeakParams params{ config_.peakIters, 0.9999f, 0.0001f, 0 };
                [computeEnc setBytes:&params length:sizeof(PeakParams) atIndex:1];
            } else if (config_.workload == Workload::NBody) {
                NBodyParams params{ deltaTime, config_.softening, config_.particleCount, 0 };
                [computeEnc setBytes:&params length:sizeof(NBodyParams) atIndex:1];
            } else {
                ComputeParams params{ deltaTime, 0.9f };
                [computeEnc setBytes:&params length:sizeof(ComputeParams) atIndex:1];
            }

            const MTLSize tgSize  = MTLSizeMake(kComputeWorkGroupSize, 1, 1);
            const MTLSize tgCount = MTLSizeMake(
                config_.particleCount / kComputeWorkGroupSize, 1, 1);
            [computeEnc dispatchThreadgroups:tgCount
                       threadsPerThreadgroup:tgSize];
            [computeEnc endEncoding];
            [computeCB commit];
        }

        // --- Render command buffer ----------------------------------------------
        MTLRenderPassDescriptor* rpDesc = [MTLRenderPassDescriptor new];
        rpDesc.colorAttachments[0].texture     = renderTarget;
        rpDesc.colorAttachments[0].loadAction  = MTLLoadActionClear;
        rpDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
        rpDesc.colorAttachments[0].clearColor  =
            MTLClearColorMake(0.04, 0.08, 0.14, 1.0);
        if (render3d) {
            rpDesc.depthAttachment.texture     = impl_->depthTex;
            rpDesc.depthAttachment.loadAction  = MTLLoadActionClear;
            rpDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
            rpDesc.depthAttachment.clearDepth  = 1.0;
        }

        id<MTLCommandBuffer> renderCB = [impl_->commandQueue commandBuffer];
        id<MTLRenderCommandEncoder> renderEnc =
            [renderCB renderCommandEncoderWithDescriptor:rpDesc];

        [renderEnc setRenderPipelineState:impl_->renderPSO];
        if (gpuBurn) {
            fractalElapsed_ += deltaTime;
            for (std::uint32_t pass = 0; pass < gpuBurnDrawsPerFrame(config_.workload); ++pass) {
                GpuBurnParams bp{ fractalElapsed_, static_cast<float>(pass),
                                  config_.gpuBurnIter, gpuBurnShaderVersion(config_.workload) };
                [renderEnc setFragmentBytes:&bp length:sizeof(GpuBurnParams) atIndex:0];
                [renderEnc drawPrimitives:MTLPrimitiveTypeTriangle
                              vertexStart:0
                              vertexCount:3];
            }
        } else if (config_.workload == Workload::StressFractal) {
            fractalElapsed_ += deltaTime;
            FractalParams fp{ fractalElapsed_, 1.0f, config_.fractalIter, 0 };
            [renderEnc setFragmentBytes:&fp length:sizeof(FractalParams) atIndex:0];
            [renderEnc drawPrimitives:MTLPrimitiveTypeTriangle
                          vertexStart:0
                          vertexCount:3];
        } else if (config_.workload == Workload::Volumetric) {
            fractalElapsed_ += deltaTime;
            VolumetricParams vp{ fractalElapsed_, 0.05f, config_.volumetricSteps, 0 };
            [renderEnc setFragmentBytes:&vp length:sizeof(VolumetricParams) atIndex:0];
            [renderEnc drawPrimitives:MTLPrimitiveTypeTriangle
                          vertexStart:0
                          vertexCount:3];
        } else if (render3d) {
            [renderEnc setDepthStencilState:impl_->depthState];
            fractalElapsed_ += deltaTime;
            Render3DParams r3{};
            const float aspect = (float)kWindowWidth / (float)kWindowHeight;
            render3dCamera(fractalElapsed_, aspect, /*flipY*/false, /*z01*/true,
                           r3.viewProj, r3.camRight, r3.camUp);
            r3.pointSize = 0.02f;
            [renderEnc setVertexBuffer:impl_->particleBuf offset:0 atIndex:1];
            [renderEnc setVertexBytes:&r3 length:sizeof(Render3DParams) atIndex:2];
            [renderEnc drawPrimitives:MTLPrimitiveTypeTriangle
                          vertexStart:0
                          vertexCount:6
                        instanceCount:config_.particleCount];
        } else {
            [renderEnc setVertexBuffer:impl_->particleBuf offset:0 atIndex:0];
            [renderEnc drawPrimitives:MTLPrimitiveTypePoint
                          vertexStart:0
                          vertexCount:config_.particleCount];
        }
        [renderEnc endEncoding];

        if (drawable)
            [renderCB presentDrawable:drawable];

        // --- Async timing + semaphore signal ------------------------------------
        __block auto* implPtr = impl_.get();
        __block id<MTLCommandBuffer> capturedComputeCB = computeCB;
        __block BOOL burnOnly = gpuBurn ? YES : NO;
        __block int publishSlot = presentSlot;
        dispatch_semaphore_t sem = impl_->frameSemaphore;

        [renderCB addCompletedHandler:^(id<MTLCommandBuffer> cb) {
            dispatch_semaphore_signal(sem);
            implPtr->publishRenderSlot(publishSlot);

            const double rs = cb.GPUStartTime;
            const double re = cb.GPUEndTime;
            if (!(re > rs))
                return;

            const double renderMs = (re - rs) * 1000.0;
            double computeMs = 0.0;
            double totalMs   = renderMs;
            if (!burnOnly && capturedComputeCB) {
                const double cs = capturedComputeCB.GPUStartTime;
                const double ce = capturedComputeCB.GPUEndTime;
                if (ce > cs) {
                    computeMs = (ce - cs) * 1000.0;
                    totalMs   = (re - cs) * 1000.0;
                }
            }

            std::lock_guard<std::mutex> lock(implPtr->timingMutex);
            implPtr->pendingTimings.push_back({computeMs, renderMs, totalMs});
        }];

        [renderCB commit];

        ++impl_->totalFrames;
        impl_->currentFrame =
            (frameSlot + 1) % impl_->framesInFlight;
    }
}

// -----------------------------------------------------------------------
// Synchronisation & cleanup
// -----------------------------------------------------------------------

void MetalBackend::WaitIdle() {
    if (!impl_ || !impl_->commandQueue) return;
    // Wait for all in-flight frames to complete
    for (std::uint32_t i = 0; i < impl_->framesInFlight; ++i) {
        dispatch_semaphore_wait(impl_->frameSemaphore, DISPATCH_TIME_FOREVER);
    }
    for (std::uint32_t i = 0; i < impl_->framesInFlight; ++i) {
        dispatch_semaphore_signal(impl_->frameSemaphore);
    }

    // Drain remaining timing data
    std::lock_guard<std::mutex> lock(impl_->timingMutex);
    for (auto& t : impl_->pendingTimings) {
        AccumulateTiming(t.computeMs, t.renderMs, t.totalMs);
    }
    impl_->pendingTimings.clear();
}

void MetalBackend::CleanupBackend() {
    WaitIdle();
    if (!impl_) return;

    if (impl_->capturing) {
        @autoreleasepool {
            MTLCaptureManager* mgr = [MTLCaptureManager sharedCaptureManager];
            if (mgr && mgr.isCapturing)
                [mgr stopCapture];
        }
        impl_->capturing = false;
        impl_->captureOutputPath.clear();
    }

    // Must stop before any Metal object it touches is released.
    impl_->stopPresenter();
    impl_->presentTextures.clear();
    impl_->presentSlotState.clear();
    impl_->presentQueue = nil;

    if (impl_->liquid) {
        impl_->liquid->cleanup();
        impl_->liquid.reset();
    }
    impl_->computePSO   = nil;
    impl_->renderPSO    = nil;
    impl_->particleBuf  = nil;
    impl_->depthTex     = nil;
    impl_->depthState   = nil;
    impl_->library       = nil;
    impl_->commandQueue  = nil;
    impl_->metalLayer    = nil;
    impl_->device        = nil;
}

}  // namespace gpu_bench

#endif  // HAVE_METAL
