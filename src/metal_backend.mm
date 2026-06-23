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

    std::string deviceName;

    struct FrameResources {
        id<MTLCommandBuffer> computeCB = nil;
        id<MTLCommandBuffer> renderCB  = nil;
    };
    std::array<FrameResources, kMaxFramesInFlight> frames{};
    std::uint32_t currentFrame = 0;
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

        // --- CAMetalLayer on the GLFW window ------------------------------------
        NSWindow* nsWindow = glfwGetCocoaWindow(window_);
        impl_->metalLayer  = [CAMetalLayer layer];
        impl_->metalLayer.device       = impl_->device;
        impl_->metalLayer.pixelFormat  = MTLPixelFormatBGRA8Unorm;
        impl_->metalLayer.drawableSize = CGSizeMake(kWindowWidth, kWindowHeight);
        impl_->metalLayer.framebufferOnly = YES;
        impl_->metalLayer.displaySyncEnabled = config_.vsync ? YES : NO;

        nsWindow.contentView.layer     = impl_->metalLayer;
        nsWindow.contentView.wantsLayer = YES;

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

        // --- Render pipeline ----------------------------------------------------
        const bool fractal  = (config_.workload == Workload::StressFractal);
        const bool render3d = (config_.workload == Workload::Render3D);
        NSString* vfn = fractal ? @"fractalVertex" : render3d ? @"render3dVertex" : @"vertexMain";
        NSString* ffn = fractal ? @"fractalFragment" : render3d ? @"render3dFragment" : @"fragmentMain";
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

        // --- Render3D depth resources ------------------------------------------
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
    }
}

// -----------------------------------------------------------------------
// Per-frame rendering
// -----------------------------------------------------------------------

void MetalBackend::DrawFrame(float deltaTime) {
    @autoreleasepool {
        const std::uint32_t frameSlot = impl_->currentFrame;

        // Collect GPU timing from this slot's previous cycle (N-2 frames ago)
        auto& prev = impl_->frames[frameSlot];
        if (prev.renderCB != nil) {
            [prev.renderCB waitUntilCompleted];

            const double cs = prev.computeCB.GPUStartTime;
            const double ce = prev.computeCB.GPUEndTime;
            const double rs = prev.renderCB.GPUStartTime;
            const double re = prev.renderCB.GPUEndTime;

            if (ce > cs && re > rs) {
                AccumulateTiming((ce - cs) * 1000.0,
                                 (re - rs) * 1000.0,
                                 (re - cs) * 1000.0);
            }
            prev.computeCB = nil;
            prev.renderCB  = nil;
        }

        id<CAMetalDrawable> drawable = [impl_->metalLayer nextDrawable];
        if (!drawable) return;

        // --- Compute pass -------------------------------------------------------
        id<MTLCommandBuffer> computeCB =
            [impl_->commandQueue commandBuffer];
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

        // --- Render pass --------------------------------------------------------
        const bool render3d = (config_.workload == Workload::Render3D);
        MTLRenderPassDescriptor* rpDesc = [MTLRenderPassDescriptor new];
        rpDesc.colorAttachments[0].texture    = drawable.texture;
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

        id<MTLCommandBuffer> renderCB =
            [impl_->commandQueue commandBuffer];
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

        [renderCB presentDrawable:drawable];
        [renderCB commit];

        // Store command buffers for timing collection next cycle
        impl_->frames[frameSlot].computeCB = computeCB;
        impl_->frames[frameSlot].renderCB  = renderCB;

        impl_->currentFrame =
            (frameSlot + 1) % kMaxFramesInFlight;
    }
}

// -----------------------------------------------------------------------
// Synchronisation & cleanup
// -----------------------------------------------------------------------

void MetalBackend::WaitIdle() {
    if (!impl_ || !impl_->commandQueue) return;
    for (auto& fr : impl_->frames) {
        if (fr.renderCB != nil) {
            [fr.renderCB waitUntilCompleted];
            fr.computeCB = nil;
            fr.renderCB  = nil;
        }
    }
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
