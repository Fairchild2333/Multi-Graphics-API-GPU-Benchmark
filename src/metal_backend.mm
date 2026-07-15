#ifdef HAVE_METAL

#include "metal_backend.h"
#include "mini_mat.h"

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <array>
#include <cstring>
#include <iostream>
#include <mutex>

namespace gpu_bench {

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
    id<MTLTexture>              offscreenTex = nil;

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
    return "macOS " + std::to_string(v.majorVersion) + "."
         + std::to_string(v.minorVersion) + "."
         + std::to_string(v.patchVersion);
}

// -----------------------------------------------------------------------
// Initialisation
// -----------------------------------------------------------------------

void MetalBackend::InitBackend() {
    if (config_.workload == Workload::Fluid) {
        throw std::runtime_error(
            "Fluid is an unverified Vulkan-only Developer Preview; Metal fallback is disabled");
    }
    if (config_.workload == Workload::GpuStressV1) {
        throw std::runtime_error(
            "GPU Stress v1 is not supported on Metal; use Vulkan, DX12, DX11, or OpenGL");
    }
    if (config_.workload == Workload::GpuBurnV1) {
        throw std::runtime_error(
            "GPU Burn v1 is not supported on Metal; use Vulkan, DX12, DX11, or OpenGL");
    }
    @autoreleasepool {
        // --- Device selection ---------------------------------------------------
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

        // --- Frames-in-flight from config (respects --flights) ------------------
        impl_->framesInFlight = config_.framesInFlight;
        impl_->frames.resize(impl_->framesInFlight);

        // --- CAMetalLayer (skipped in headless mode) ----------------------------
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

        // --- Frame semaphore ----------------------------------------------------
        impl_->frameSemaphore = dispatch_semaphore_create(impl_->framesInFlight);

        // --- Command queue ------------------------------------------------------
        impl_->commandQueue = [impl_->device newCommandQueue];

        // --- Compile Metal shader library from source ---------------------------
        std::string shaderPath = shaderDir_ + "particle.metal";
        auto shaderSource = ReadFileBytes(shaderPath);
        NSString* source =
            [[NSString alloc] initWithBytes:shaderSource.data()
                                     length:shaderSource.size()
                                   encoding:NSUTF8StringEncoding];

        MTLCompileOptions* opts = [[MTLCompileOptions alloc] init];
        opts.mathMode = MTLMathModeFast;

        NSError* error = nil;
        impl_->library = [impl_->device newLibraryWithSource:source
                                                     options:opts
                                                       error:&error];
        if (!impl_->library) {
            std::string msg = error
                ? [[error localizedDescription] UTF8String]
                : "unknown error";
            throw std::runtime_error("Metal shader compilation failed: " + msg);
        }

        // --- Compute pipeline ---------------------------------------------------
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

        // --- Render pipeline (skipped in headless mode) -------------------------
        if (!config_.headless) {
            const bool fractal   = (config_.workload == Workload::StressFractal);
            const bool volumetric= (config_.workload == Workload::Volumetric);
            const bool render3d  = (config_.workload == Workload::Render3D);
            NSString* vfn = fractal ? @"fractalVertex"
                         : volumetric ? @"volumetricVertex"
                         : render3d ? @"render3dVertex" : @"vertexMain";
            NSString* ffn = fractal ? @"fractalFragment"
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
            ca.pixelFormat                 = MTLPixelFormatBGRA8Unorm;
            ca.blendingEnabled             = YES;
            ca.sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
            ca.destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
            ca.rgbBlendOperation           = MTLBlendOperationAdd;
            ca.sourceAlphaBlendFactor      = MTLBlendFactorOne;
            ca.destinationAlphaBlendFactor = MTLBlendFactorZero;
            ca.alphaBlendOperation         = MTLBlendOperationAdd;

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

        // --- Particle buffer (shared memory — ideal for Apple Silicon) ----------
        const NSUInteger bufSize = sizeof(Particle) * config_.particleCount;
        impl_->particleBuf =
            [impl_->device newBufferWithBytes:initialParticles_.data()
                                       length:bufSize
                                      options:MTLResourceStorageModeShared];
        if (!impl_->particleBuf)
            throw std::runtime_error("Failed to create particle buffer");

        std::cout << "Created particle buffer: " << config_.particleCount
                  << " particles\n";
        std::cout << "[Profiling] GPU command-buffer timestamps enabled\n";

        // --- Offscreen render target (benchmark mode only) ----------------------
        // On macOS, ProMotion displays lock nextDrawable to refresh rate even with
        // displaySyncEnabled=NO. In benchmark mode, use offscreen texture and
        // present periodically to avoid VSync throttling.
        // In interactive (non-benchmark) mode, present every frame normally.
        if (!config_.headless && config_.benchmarkMode && !config_.vsync) {
            MTLTextureDescriptor* texDesc =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                  width:kWindowWidth
                                                                 height:kWindowHeight
                                                              mipmapped:NO];
            texDesc.usage = MTLTextureUsageRenderTarget;
            texDesc.storageMode = MTLStorageModePrivate;
            impl_->offscreenTex = [impl_->device newTextureWithDescriptor:texDesc];
            if (!impl_->offscreenTex)
                throw std::runtime_error("Failed to create offscreen texture");
            std::cout << "[Metal] Benchmark mode: using offscreen render target to bypass ProMotion VSync\n";
        }

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

        // === WINDOWED PATH: compute + render ====================================

        // --- Determine render target --------------------------------------------
        const bool useOffscreen = impl_->offscreenTex != nil;
        const bool presentThisFrame = !useOffscreen ||
                                      (impl_->totalFrames % 60 == 0);

        id<CAMetalDrawable> drawable = nil;
        id<MTLTexture> renderTarget = nil;

        if (presentThisFrame) {
            drawable = [impl_->metalLayer nextDrawable];
            if (drawable)
                renderTarget = drawable.texture;
        }
        if (!renderTarget && useOffscreen)
            renderTarget = impl_->offscreenTex;
        if (!renderTarget) {
            dispatch_semaphore_signal(impl_->frameSemaphore);
            return;
        }

        // --- Compute command buffer ---------------------------------------------
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
        [computeCB commit];

        // --- Render command buffer (queued after compute — GPU executes in order)
        const bool render3d = (config_.workload == Workload::Render3D);
        MTLRenderPassDescriptor* rpDesc = [MTLRenderPassDescriptor new];
        rpDesc.colorAttachments[0].texture    = renderTarget;
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
        if (config_.workload == Workload::StressFractal) {
            fractalElapsed_ += deltaTime;
            FractalParams fp{ fractalElapsed_, 1.0f, config_.fractalIter, 0 };
            [renderEnc setFragmentBytes:&fp length:sizeof(FractalParams) atIndex:0];
            [renderEnc drawPrimitives:MTLPrimitiveTypeTriangle
                          vertexStart:0
                          vertexCount:3];   // fullscreen triangle, no vertex buffer
        } else if (config_.workload == Workload::Volumetric) {
            fractalElapsed_ += deltaTime;   // reused as noise-field animation time
            VolumetricParams vp{ fractalElapsed_, 0.05f, config_.volumetricSteps, 0 };
            [renderEnc setFragmentBytes:&vp length:sizeof(VolumetricParams) atIndex:0];
            [renderEnc drawPrimitives:MTLPrimitiveTypeTriangle
                          vertexStart:0
                          vertexCount:3];   // fullscreen triangle, no vertex buffer
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
        dispatch_semaphore_t sem = impl_->frameSemaphore;

        [renderCB addCompletedHandler:^(id<MTLCommandBuffer> cb) {
            dispatch_semaphore_signal(sem);

            const double cs = capturedComputeCB.GPUStartTime;
            const double ce = capturedComputeCB.GPUEndTime;
            const double rs = cb.GPUStartTime;
            const double re = cb.GPUEndTime;

            if (ce > cs && re > rs) {
                const double computeMs = (ce - cs) * 1000.0;
                const double renderMs  = (re - rs) * 1000.0;
                const double totalMs   = (re - cs) * 1000.0;

                std::lock_guard<std::mutex> lock(implPtr->timingMutex);
                implPtr->pendingTimings.push_back({computeMs, renderMs, totalMs});
            }
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

    impl_->computePSO   = nil;
    impl_->renderPSO    = nil;
    impl_->particleBuf  = nil;
    impl_->library       = nil;
    impl_->commandQueue  = nil;
    impl_->metalLayer    = nil;
    impl_->device        = nil;
}

}  // namespace gpu_bench

#endif  // HAVE_METAL
