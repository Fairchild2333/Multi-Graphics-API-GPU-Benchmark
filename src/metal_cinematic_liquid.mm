#ifdef HAVE_METAL

#include "metal_cinematic_liquid.h"
#include "cinematic_liquid_v2_common.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
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

std::string readTextFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("Failed to open Metal liquid shader: " + path);
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

id<MTLComputePipelineState> makeComputePSO(id<MTLDevice> device,
                                           id<MTLLibrary> lib,
                                           NSString* name) {
    id<MTLFunction> fn = [lib newFunctionWithName:name];
    if (!fn)
        throw std::runtime_error(
            std::string("Metal liquid kernel missing: ") + name.UTF8String);
    NSError* err = nil;
    id<MTLComputePipelineState> pso =
        [device newComputePipelineStateWithFunction:fn error:&err];
    if (!pso) {
        std::string msg = err ? err.localizedDescription.UTF8String : "unknown";
        throw std::runtime_error("Metal liquid PSO failed (" +
                                 std::string(name.UTF8String) + "): " + msg);
    }
    return pso;
}

}  // namespace

struct MetalLiquidV2Host::Impl {
    id<MTLDevice> device = nil;
    id<MTLLibrary> library = nil;

    id<MTLComputePipelineState> clearGrid = nil;
    id<MTLComputePipelineState> p2gMass = nil;
    id<MTLComputePipelineState> p2gStress = nil;
    id<MTLComputePipelineState> gridUpdate = nil;
    id<MTLComputePipelineState> g2p = nil;
    id<MTLComputePipelineState> rigidIntegrate = nil;
    id<MTLComputePipelineState> resolveWhitewater = nil;
    id<MTLComputePipelineState> surfaceClear = nil;
    id<MTLComputePipelineState> surfaceSplat = nil;
    id<MTLComputePipelineState> surfaceResolve = nil;
    id<MTLRenderPipelineState> renderPSO = nil;
    id<MTLSamplerState> densSampler = nil;
    id<MTLSamplerState> wwSampler = nil;
    std::uint32_t raySteps = kCinematicLiquidV2RaySteps;

    id<MTLBuffer> particles = nil;
    id<MTLBuffer> seedParticles = nil;
    id<MTLBuffer> grid = nil;
    id<MTLBuffer> bodies = nil;
    id<MTLBuffer> seedBodies = nil;
    id<MTLBuffer> impulses = nil;
    id<MTLBuffer> densityAtomic = nil;
    id<MTLTexture> densityVolume = nil;
    id<MTLTexture> whitewaterVolume = nil;

    std::uint32_t gridX = kCinematicLiquidV2GridX;
    std::uint32_t gridY = kCinematicLiquidV2GridY;
    std::uint32_t gridZ = kCinematicLiquidV2GridZ;
    std::uint32_t surfaceX = kCinematicLiquidV2SurfaceX;
    std::uint32_t surfaceY = kCinematicLiquidV2SurfaceY;
    std::uint32_t surfaceZ = kCinematicLiquidV2SurfaceZ;
    std::uint32_t particleCount = 0;
    std::uint32_t bodyCount = kCinematicLiquidV2BodyCount;
    std::uint32_t substeps = kCinematicLiquidV2Substeps;
    std::uint32_t shaderVersion = kCinematicLiquidV2ShaderVersion;
    float dx = kCinematicLiquidV2Dx;
    float particleSpacing = kCinematicLiquidV2Dx * 0.72f;
    float particleMass = 0.0f;
    float presentationTime = 0.0f;
    float simTime = 0.0f;
    bool captureChoreographyResetDone = false;
};

MetalLiquidV2Host::MetalLiquidV2Host() : impl_(std::make_unique<Impl>()) {}
MetalLiquidV2Host::~MetalLiquidV2Host() { cleanup(); }

bool MetalLiquidV2Host::active() const {
    return impl_ && impl_->particles != nil;
}

std::uint32_t MetalLiquidV2Host::particleCount() const {
    return impl_ ? impl_->particleCount : 0;
}

void MetalLiquidV2Host::cleanup() {
    if (!impl_) return;
    impl_->clearGrid = nil;
    impl_->p2gMass = nil;
    impl_->p2gStress = nil;
    impl_->gridUpdate = nil;
    impl_->g2p = nil;
    impl_->rigidIntegrate = nil;
    impl_->resolveWhitewater = nil;
    impl_->surfaceClear = nil;
    impl_->surfaceSplat = nil;
    impl_->surfaceResolve = nil;
    impl_->renderPSO = nil;
    impl_->densSampler = nil;
    impl_->particles = nil;
    impl_->seedParticles = nil;
    impl_->grid = nil;
    impl_->bodies = nil;
    impl_->seedBodies = nil;
    impl_->impulses = nil;
    impl_->densityAtomic = nil;
    impl_->densityVolume = nil;
    impl_->whitewaterVolume = nil;
    impl_->library = nil;
    impl_->device = nil;
}

void MetalLiquidV2Host::init(void* mtlDevice, const std::string& shaderPath) {
    cleanup();
    impl_ = std::make_unique<Impl>();
    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)mtlDevice;
        if (!device)
            throw std::runtime_error("MetalLiquidV2Host: null device");
        impl_->device = device;

        const auto source = readTextFile(shaderPath);
        NSString* nsSource =
            [[NSString alloc] initWithBytes:source.data()
                                     length:source.size()
                                   encoding:NSUTF8StringEncoding];
        MTLCompileOptions* opts = [[MTLCompileOptions alloc] init];
        ConfigureFastMath(opts);
        NSError* error = nil;
        impl_->library = [device newLibraryWithSource:nsSource
                                              options:opts
                                                error:&error];
        if (!impl_->library) {
            std::string msg = error ? error.localizedDescription.UTF8String
                                    : "unknown";
            throw std::runtime_error(
                "Metal liquid shader compile failed: " + msg);
        }

        impl_->clearGrid = makeComputePSO(device, impl_->library, @"clearGrid");
        impl_->p2gMass = makeComputePSO(device, impl_->library, @"p2gMass");
        impl_->p2gStress = makeComputePSO(device, impl_->library, @"p2gStress");
        impl_->gridUpdate = makeComputePSO(device, impl_->library, @"gridUpdate");
        impl_->g2p = makeComputePSO(device, impl_->library, @"g2p");
        impl_->rigidIntegrate =
            makeComputePSO(device, impl_->library, @"rigidIntegrate");
        impl_->resolveWhitewater =
            makeComputePSO(device, impl_->library, @"resolveWhitewater");
        impl_->surfaceClear =
            makeComputePSO(device, impl_->library, @"surfaceClear");
        impl_->surfaceSplat =
            makeComputePSO(device, impl_->library, @"surfaceSplat");
        impl_->surfaceResolve =
            makeComputePSO(device, impl_->library, @"surfaceResolve");

        id<MTLFunction> vert =
            [impl_->library newFunctionWithName:@"liquidVertex"];
        id<MTLFunction> frag =
            [impl_->library newFunctionWithName:@"liquidFragment"];
        if (!vert || !frag)
            throw std::runtime_error("Metal liquid present shaders missing");
        MTLRenderPipelineDescriptor* rp =
            [[MTLRenderPipelineDescriptor alloc] init];
        rp.vertexFunction = vert;
        rp.fragmentFunction = frag;
        rp.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        rp.colorAttachments[0].blendingEnabled = NO;
        impl_->renderPSO =
            [device newRenderPipelineStateWithDescriptor:rp error:&error];
        if (!impl_->renderPSO) {
            std::string msg = error ? error.localizedDescription.UTF8String
                                    : "unknown";
            throw std::runtime_error("Metal liquid render PSO failed: " + msg);
        }

        MTLSamplerDescriptor* sd = [[MTLSamplerDescriptor alloc] init];
        sd.minFilter = MTLSamplerMinMagFilterLinear;
        sd.magFilter = MTLSamplerMinMagFilterLinear;
        sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
        sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
        sd.rAddressMode = MTLSamplerAddressModeClampToEdge;
        impl_->densSampler = [device newSamplerStateWithDescriptor:sd];
        impl_->wwSampler = [device newSamplerStateWithDescriptor:sd];

        impl_->particleSpacing = impl_->dx * 0.72f;
        impl_->particleMass = kLiquidV2RestDensity * impl_->particleSpacing *
                              impl_->particleSpacing * impl_->particleSpacing;

        std::vector<MlsMpmParticleGpu> seed;
        BuildCinematicLiquidV2ParticleSeed(seed, impl_->dx,
                                           impl_->particleSpacing,
                                           impl_->particleMass);
        impl_->particleCount = static_cast<std::uint32_t>(seed.size());

        std::vector<CinematicLiquidBodyStateGpu> bodySeed;
        BuildCinematicLiquidV2BodySeed(bodySeed);
        impl_->bodyCount = static_cast<std::uint32_t>(bodySeed.size());

        const NSUInteger particleBytes =
            sizeof(MlsMpmParticleGpu) * impl_->particleCount;
        const NSUInteger gridCells =
            NSUInteger(impl_->gridX) * impl_->gridY * impl_->gridZ;
        const NSUInteger gridBytes = sizeof(std::int32_t) * 4 * gridCells;
        const NSUInteger bodyBytes =
            sizeof(CinematicLiquidBodyStateGpu) * impl_->bodyCount;
        const NSUInteger impulseBytes =
            sizeof(CinematicLiquidBodyImpulseGpu) * impl_->bodyCount;
        const NSUInteger surfaceVoxels =
            NSUInteger(impl_->surfaceX) * impl_->surfaceY * impl_->surfaceZ;

        impl_->seedParticles =
            [device newBufferWithBytes:seed.data()
                               length:particleBytes
                              options:MTLResourceStorageModeShared];
        impl_->particles =
            [device newBufferWithBytes:seed.data()
                               length:particleBytes
                              options:MTLResourceStorageModeShared];
        impl_->seedBodies =
            [device newBufferWithBytes:bodySeed.data()
                               length:bodyBytes
                              options:MTLResourceStorageModeShared];
        impl_->bodies =
            [device newBufferWithBytes:bodySeed.data()
                               length:bodyBytes
                              options:MTLResourceStorageModeShared];
        impl_->grid = [device newBufferWithLength:gridBytes
                                          options:MTLResourceStorageModeShared];
        impl_->impulses =
            [device newBufferWithLength:impulseBytes
                               options:MTLResourceStorageModeShared];
        impl_->densityAtomic =
            [device newBufferWithLength:sizeof(std::uint32_t) * surfaceVoxels
                               options:MTLResourceStorageModeShared];
        if (!impl_->particles || !impl_->grid || !impl_->bodies ||
            !impl_->impulses || !impl_->densityAtomic)
            throw std::runtime_error("Metal liquid buffer allocation failed");

        auto makeVolume = [&](NSUInteger w, NSUInteger h, NSUInteger d) {
            MTLTextureDescriptor* td = [[MTLTextureDescriptor alloc] init];
            td.textureType = MTLTextureType3D;
            td.pixelFormat = MTLPixelFormatR32Float;
            td.width = w;
            td.height = h;
            td.depth = d;
            td.mipmapLevelCount = 1;
            td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
            td.storageMode = MTLStorageModePrivate;
            id<MTLTexture> tex = [device newTextureWithDescriptor:td];
            if (!tex)
                throw std::runtime_error("Metal liquid 3D texture alloc failed");
            return tex;
        };
        impl_->densityVolume =
            makeVolume(impl_->surfaceX, impl_->surfaceY, impl_->surfaceZ);
        impl_->whitewaterVolume =
            makeVolume(impl_->gridX, impl_->gridY, impl_->gridZ);

        std::cout << "[Metal] Cinematic Liquid v2 MLS-MPM preview host: "
                  << impl_->particleCount << " particles, grid "
                  << impl_->gridX << "x" << impl_->gridY << "x" << impl_->gridZ
                  << ", " << impl_->substeps << " substeps/frame, "
                  << impl_->raySteps
                  << "-step raymarch present (metal_preview scores)\n";
    }
}

void MetalLiquidV2Host::encodeFrame(void* mtlCommandQueue,
                                    float deltaTime,
                                    void* mtlRenderTargetTexture,
                                    void* mtlDrawableOrNull,
                                    void** outComputeCB,
                                    void** outRenderCB) {
    if (!active())
        throw std::runtime_error("MetalLiquidV2Host not initialised");
    if (outComputeCB) *outComputeCB = nullptr;
    if (outRenderCB) *outRenderCB = nullptr;

    @autoreleasepool {
        auto& r = *impl_;
        id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)mtlCommandQueue;
        id<MTLTexture> target =
            (__bridge id<MTLTexture>)mtlRenderTargetTexture;
        id<CAMetalDrawable> drawable =
            mtlDrawableOrNull ? (__bridge id<CAMetalDrawable>)mtlDrawableOrNull
                              : nil;

        const std::uint32_t gridCellCount = r.gridX * r.gridY * r.gridZ;
        const std::uint32_t particleGroups = (r.particleCount + 255u) / 256u;
        const std::uint32_t gridClearGroups = (gridCellCount + 255u) / 256u;
        const std::uint32_t gridGroupsX = (r.gridX + 7u) / 8u;
        const std::uint32_t gridGroupsY = (r.gridY + 7u) / 8u;
        const std::uint32_t gridGroupsZ = (r.gridZ + 7u) / 8u;
        const std::uint32_t surfGroupsX = (r.surfaceX + 7u) / 8u;
        const std::uint32_t surfGroupsY = (r.surfaceY + 7u) / 8u;
        const std::uint32_t surfGroupsZ = (r.surfaceZ + 7u) / 8u;
        const std::uint32_t bodyGroups = (r.bodyCount + 63u) / 64u;

        const float wallDt = std::max(deltaTime, 0.0f);
        const float frameDt = std::clamp(wallDt, 0.0f, 1.0f / 30.0f);
        const float substepDt =
            std::max(frameDt / float(r.substeps), 1e-6f);
        r.presentationTime += wallDt;
        r.simTime += frameDt;

        CinematicLiquidV2PushConstants pc{};
        FillCinematicLiquidV2ComputePush(pc, r.gridX, r.gridY, r.gridZ,
                                         r.particleCount, substepDt,
                                         r.particleMass, r.dx, r.bodyCount,
                                         r.shaderVersion, r.presentationTime);
        const float surfaceVoxelSize =
            (float(r.gridX) * r.dx) / float(r.surfaceX);
        CinematicLiquidV2SurfacePushConstants sp{};
        FillCinematicLiquidV2SurfacePush(sp, r.surfaceX, r.surfaceY, r.surfaceZ,
                                         r.particleCount, surfaceVoxelSize,
                                         r.particleSpacing, r.particleMass,
                                         r.shaderVersion);

        id<MTLCommandBuffer> computeCB = [queue commandBuffer];

        // 4s choreography restage (CPU memcpy mirrors Vulkan buffer copy).
        if (!r.captureChoreographyResetDone && r.presentationTime >= 4.0f) {
            std::memcpy(r.particles.contents, r.seedParticles.contents,
                        r.particles.length);
            std::memcpy(r.bodies.contents, r.seedBodies.contents,
                        r.bodies.length);
            std::memset(r.impulses.contents, 0, r.impulses.length);
            r.simTime = 0.0f;
            r.captureChoreographyResetDone = true;
        }

        auto bindCommon = [&](id<MTLComputeCommandEncoder> enc) {
            [enc setBytes:&pc length:sizeof(pc) atIndex:0];
            [enc setBuffer:r.particles offset:0 atIndex:1];
            [enc setBuffer:r.grid offset:0 atIndex:2];
            [enc setBuffer:r.bodies offset:0 atIndex:3];
            [enc setBuffer:r.impulses offset:0 atIndex:4];
            [enc setBuffer:r.densityAtomic offset:0 atIndex:5];
            [enc setBytes:&sp length:sizeof(sp) atIndex:6];
            [enc setTexture:r.densityVolume atIndex:0];
            [enc setTexture:r.whitewaterVolume atIndex:1];
        };

        auto dispatch1D = [&](id<MTLComputePipelineState> pso,
                              std::uint32_t groups) {
            id<MTLComputeCommandEncoder> enc = [computeCB computeCommandEncoder];
            [enc setComputePipelineState:pso];
            bindCommon(enc);
            [enc dispatchThreadgroups:MTLSizeMake(groups, 1, 1)
                threadsPerThreadgroup:MTLSizeMake(256, 1, 1)];
            [enc endEncoding];
        };

        auto dispatch3D = [&](id<MTLComputePipelineState> pso,
                              std::uint32_t gx, std::uint32_t gy,
                              std::uint32_t gz, std::uint32_t tg) {
            id<MTLComputeCommandEncoder> enc = [computeCB computeCommandEncoder];
            [enc setComputePipelineState:pso];
            bindCommon(enc);
            [enc dispatchThreadgroups:MTLSizeMake(gx, gy, gz)
                threadsPerThreadgroup:MTLSizeMake(tg, tg, tg)];
            [enc endEncoding];
        };

        for (std::uint32_t sub = 0; sub < r.substeps; ++sub) {
            FillCinematicLiquidV2ComputePush(pc, r.gridX, r.gridY, r.gridZ,
                                             r.particleCount, substepDt,
                                             r.particleMass, r.dx, r.bodyCount,
                                             r.shaderVersion,
                                             r.presentationTime);
            dispatch1D(r.clearGrid, gridClearGroups);
            dispatch1D(r.p2gMass, particleGroups);
            dispatch1D(r.p2gStress, particleGroups);
            dispatch3D(r.gridUpdate, gridGroupsX, gridGroupsY, gridGroupsZ, 8);
            dispatch1D(r.g2p, particleGroups);
            {
                id<MTLComputeCommandEncoder> enc =
                    [computeCB computeCommandEncoder];
                [enc setComputePipelineState:r.rigidIntegrate];
                bindCommon(enc);
                [enc dispatchThreadgroups:MTLSizeMake(bodyGroups, 1, 1)
                    threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
                [enc endEncoding];
            }
        }

        dispatch3D(r.resolveWhitewater, gridGroupsX, gridGroupsY, gridGroupsZ,
                   8);
        dispatch3D(r.surfaceClear, surfGroupsX, surfGroupsY, surfGroupsZ, 8);
        dispatch1D(r.surfaceSplat, particleGroups);
        dispatch3D(r.surfaceResolve, surfGroupsX, surfGroupsY, surfGroupsZ, 8);

        if (outComputeCB)
            *outComputeCB = (__bridge_retained void*)computeCB;

        if (!target)
            return;

        id<MTLCommandBuffer> renderCB = [queue commandBuffer];
        MTLRenderPassDescriptor* rp = [MTLRenderPassDescriptor new];
        rp.colorAttachments[0].texture = target;
        rp.colorAttachments[0].loadAction = MTLLoadActionClear;
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        rp.colorAttachments[0].clearColor =
            MTLClearColorMake(0.04, 0.08, 0.14, 1.0);

        const std::uint32_t width =
            target ? static_cast<std::uint32_t>(target.width) : 1u;
        const std::uint32_t height =
            target ? static_cast<std::uint32_t>(target.height) : 1u;
        const float aspect =
            float(width) / float((std::max)(height, 1u));
        // Metal present target is BGRA8Unorm (not sRGB) — match Vulkan UNORM
        // fallback so finishColor applies the explicit encode.
        CinematicLiquidV2RenderPushConstants renderPc{};
        FillCinematicLiquidV2RenderPush(
            renderPc, r.presentationTime, aspect, r.gridX, r.gridY, r.gridZ,
            r.dx, r.raySteps, r.shaderVersion, width, height, r.bodyCount,
            /*swapchainIsSrgb=*/false);

        id<MTLRenderCommandEncoder> renc =
            [renderCB renderCommandEncoderWithDescriptor:rp];
        [renc setRenderPipelineState:r.renderPSO];
        [renc setFragmentBytes:&renderPc length:sizeof(renderPc) atIndex:0];
        [renc setFragmentBuffer:r.bodies offset:0 atIndex:1];
        [renc setFragmentTexture:r.densityVolume atIndex:0];
        [renc setFragmentTexture:r.whitewaterVolume atIndex:1];
        [renc setFragmentSamplerState:r.densSampler atIndex:0];
        [renc setFragmentSamplerState:r.wwSampler atIndex:1];
        [renc drawPrimitives:MTLPrimitiveTypeTriangle
                 vertexStart:0
                 vertexCount:3];
        [renc endEncoding];

        if (drawable)
            [renderCB presentDrawable:drawable];
        if (outRenderCB)
            *outRenderCB = (__bridge_retained void*)renderCB;
    }
}

}  // namespace gpu_bench

#endif  // HAVE_METAL
