#pragma once

#ifdef HAVE_METAL

#include <memory>
#include <string>

namespace gpu_bench {

// Metal MLS-MPM v2 host (compute + raymarch present; scores stay *_metal_preview).
// Opaque ObjC types stay in the .mm; callers pass id<> as void*.
class MetalLiquidV2Host {
public:
    MetalLiquidV2Host();
    ~MetalLiquidV2Host();

    MetalLiquidV2Host(const MetalLiquidV2Host&) = delete;
    MetalLiquidV2Host& operator=(const MetalLiquidV2Host&) = delete;

    void init(void* mtlDevice, const std::string& shaderPath);
    bool active() const;

    // Encodes one frame (substeps + surface + raymarch present) and commits.
    // Timing is filled asynchronously by the caller via command-buffer handlers;
    // this helper returns the committed compute/render command buffers as void*.
    void encodeFrame(void* mtlCommandQueue,
                     float deltaTime,
                     void* mtlRenderTargetTexture,
                     void* mtlDrawableOrNull,
                     void** outComputeCB,
                     void** outRenderCB);

    void cleanup();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace gpu_bench

#endif  // HAVE_METAL
