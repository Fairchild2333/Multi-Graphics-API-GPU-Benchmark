#pragma once

#ifdef HAVE_METAL

#include <cstdint>
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
    std::uint32_t particleCount() const;

    // Encodes one frame (substeps + surface + raymarch present). The returned
    // command buffers are retained and uncommitted so the caller can install
    // timing handlers before committing them in compute-then-render order.
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
