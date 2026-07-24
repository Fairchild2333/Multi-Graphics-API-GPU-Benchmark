#pragma once

#ifdef HAVE_VULKAN

#include "app_base.h"

#if defined(GPU_BENCH_NO_GLFW)
#if defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#endif
#include <vulkan/vulkan.h>
#else
#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>
#endif

#include <optional>

namespace gpu_bench {

struct QueueFamilyIndices {
    std::optional<std::uint32_t> graphicsFamily;
    std::optional<std::uint32_t> presentFamily;
    std::optional<std::uint32_t> computeFamily;

    bool IsComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value()
            && computeFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR          capabilities{};
    std::vector<VkSurfaceFormatKHR>   formats;
    std::vector<VkPresentModeKHR>     presentModes;
};

class VulkanBackend : public AppBase {
public:
    using AppBase::AppBase;

    std::string GetBackendName()    const override { return "Vulkan"; }
    std::string GetDeviceName()     const override { return deviceName_; }
    std::string GetDriverVersion()  const override { return driverVersion_; }

protected:
    void InitBackend()            override;
    void DrawFrame(float deltaTime) override;
    void CleanupBackend()         override;
    void WaitIdle()               override;

private:
    std::string deviceName_;
    std::string driverVersion_;

    VkInstance       instance_       = VK_NULL_HANDLE;
    VkSurfaceKHR    surface_        = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    std::vector<VkPhysicalDevice> physicalDeviceGroup_;
    VkDevice         device_        = VK_NULL_HANDLE;
    bool             deviceGroupAfr_ = false;
    std::uint32_t    afrDeviceCount_ = 1;
    bool             deferAfrDeviceDestroyToProcessExit_ = false;

    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_  = VK_NULL_HANDLE;
    VkQueue computeQueue_  = VK_NULL_HANDLE;

    VkSwapchainKHR              swapChain_          = VK_NULL_HANDLE;
    std::vector<VkImage>        swapChainImages_;
    std::vector<VkImageView>    swapChainImageViews_;
    std::vector<VkFramebuffer>  swapChainFramebuffers_;
    VkFormat                    swapChainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D                  swapChainExtent_{};

    VkRenderPass     renderPass_             = VK_NULL_HANDLE;
    VkPipelineLayout graphicsPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline       graphicsPipeline_       = VK_NULL_HANDLE;

    VkDescriptorSetLayout computeDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool      descriptorPool_              = VK_NULL_HANDLE;
    VkDescriptorSet       computeDescriptorSet_        = VK_NULL_HANDLE;
    VkPipelineLayout      computePipelineLayout_       = VK_NULL_HANDLE;
    VkPipeline            computePipeline_             = VK_NULL_HANDLE;

    VkBuffer       particleBuffer_       = VK_NULL_HANDLE;
    VkDeviceMemory particleBufferMemory_ = VK_NULL_HANDLE;

    // Render3D resources (true-3D billboard pass with depth)
    VkBuffer       quadBuffer_           = VK_NULL_HANDLE;
    VkDeviceMemory quadBufferMemory_     = VK_NULL_HANDLE;
    VkImage        depthImage_           = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory_     = VK_NULL_HANDLE;
    VkImageView    depthImageView_       = VK_NULL_HANDLE;
    VkFormat       depthFormat_          = VK_FORMAT_D32_SFLOAT;

    // Fluid (Stam 2D Eulerian) resources — all isolated in this struct so the
    // main compute/graphics slots stay untouched. Two state buffers (vel+dye
    // as vec4) and two pressure buffers ping-pong across passes; the jacobi
    // pass additionally reads a divergence buffer (binding 4).
    struct FluidResources {
        VkBuffer       stateA = VK_NULL_HANDLE, stateB = VK_NULL_HANDLE;
        VkDeviceMemory stateAMem = VK_NULL_HANDLE, stateBMem = VK_NULL_HANDLE;
        VkBuffer       pressA = VK_NULL_HANDLE, pressB = VK_NULL_HANDLE;
        VkDeviceMemory pressAMem = VK_NULL_HANDLE, pressBMem = VK_NULL_HANDLE;
        VkBuffer       divBuf = VK_NULL_HANDLE;
        VkDeviceMemory divMem = VK_NULL_HANDLE;

        VkDescriptorSetLayout computeSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool      computePool      = VK_NULL_HANDLE;
        // Immutable per-pass sets. stateA is canonical at every frame boundary:
        // advect A->B, project B->A, render A.
        VkDescriptorSet       setAdvect  = VK_NULL_HANDLE;  // in=A, out=B, press unused
        VkDescriptorSet       setDiv     = VK_NULL_HANDLE;  // in=B, out=div
        VkDescriptorSet       setJacA    = VK_NULL_HANDLE;  // in=pressA, out=pressB, div
        VkDescriptorSet       setJacB    = VK_NULL_HANDLE;  // in=pressB, out=pressA, div
        VkDescriptorSet       setSub     = VK_NULL_HANDLE;  // in=B, out=A, press=final
        VkDescriptorSetLayout renderSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool      renderPool      = VK_NULL_HANDLE;
        VkDescriptorSet       setRender  = VK_NULL_HANDLE;

        VkPipelineLayout computeLayout = VK_NULL_HANDLE;
        VkPipeline       advectPipe    = VK_NULL_HANDLE;
        VkPipeline       divPipe       = VK_NULL_HANDLE;
        VkPipeline       jacobiPipe    = VK_NULL_HANDLE;
        VkPipeline       subtractPipe  = VK_NULL_HANDLE;
        VkPipelineLayout renderLayout  = VK_NULL_HANDLE;
        VkPipeline       renderPipe    = VK_NULL_HANDLE;

        std::uint32_t gridSize = 0;
        float         simTime  = 0.0f;
    } fluid_;

    // Cinematic Liquid: true 3D MLS-MPM simulation plus a filterable density
    // volume consumed by a fullscreen free-surface raymarch.  This is kept
    // separate from the legacy 2D Eulerian dye workload above so neither its
    // resources nor its score contract can be confused with the old test.
    struct CinematicLiquidResources {
        VkBuffer       particles = VK_NULL_HANDLE;
        VkDeviceMemory particlesMem = VK_NULL_HANDLE;
        VkBuffer       seedParticles = VK_NULL_HANDLE;
        VkDeviceMemory seedParticlesMem = VK_NULL_HANDLE;
        VkBuffer       grid = VK_NULL_HANDLE;
        VkDeviceMemory gridMem = VK_NULL_HANDLE;
        VkBuffer       bodies = VK_NULL_HANDLE;
        VkDeviceMemory bodiesMem = VK_NULL_HANDLE;
        VkBuffer       seedBodies = VK_NULL_HANDLE;
        VkDeviceMemory seedBodiesMem = VK_NULL_HANDLE;
        VkBuffer       bodyImpulses = VK_NULL_HANDLE;
        VkDeviceMemory bodyImpulsesMem = VK_NULL_HANDLE;

        VkImage        densityImage = VK_NULL_HANDLE;
        VkDeviceMemory densityImageMem = VK_NULL_HANDLE;
        VkImageView    densityImageView = VK_NULL_HANDLE;
        VkSampler      densitySampler = VK_NULL_HANDLE;
        // V2-only physically-derived whitewater field.  The resolve pass
        // derives it from free-surface density, kinetic energy and the APIC
        // grid velocity gradient; v1 leaves these handles null.
        VkImage        whitewaterImage = VK_NULL_HANDLE;
        VkDeviceMemory whitewaterImageMem = VK_NULL_HANDLE;
        VkImageView    whitewaterImageView = VK_NULL_HANDLE;

        // V2 surface reconstruction is deliberately decoupled from the
        // 4 cm MLS-MPM dynamics grid.  Particles splat a normalized Spiky^2
        // density into this fixed-point buffer, then resolve it to the
        // independently reconstructed R32F density image above.
        VkBuffer       densityAtomicBuffer = VK_NULL_HANDLE;
        VkDeviceMemory densityAtomicBufferMem = VK_NULL_HANDLE;
        VkDescriptorSetLayout surfaceSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool      surfacePool = VK_NULL_HANDLE;
        VkDescriptorSet       surfaceSet = VK_NULL_HANDLE;
        VkPipelineLayout      surfaceLayout = VK_NULL_HANDLE;
        VkPipeline            surfaceClearPipe = VK_NULL_HANDLE;
        VkPipeline            surfaceSplatPipe = VK_NULL_HANDLE;
        VkPipeline            surfaceResolvePipe = VK_NULL_HANDLE;

        VkDescriptorSetLayout computeSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool      computePool = VK_NULL_HANDLE;
        VkDescriptorSet       computeSet = VK_NULL_HANDLE;
        VkPipelineLayout      computeLayout = VK_NULL_HANDLE;
        VkPipeline            clearGridPipe = VK_NULL_HANDLE;
        VkPipeline            p2gMassPipe = VK_NULL_HANDLE;
        VkPipeline            p2gStressPipe = VK_NULL_HANDLE;
        VkPipeline            gridUpdatePipe = VK_NULL_HANDLE;
        VkPipeline            g2pPipe = VK_NULL_HANDLE;
        VkPipeline            rigidIntegratePipe = VK_NULL_HANDLE;
        VkPipeline            resolveDensityPipe = VK_NULL_HANDLE;

        VkDescriptorSetLayout renderSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool      renderPool = VK_NULL_HANDLE;
        VkDescriptorSet       renderSet = VK_NULL_HANDLE;
        VkPipelineLayout      renderLayout = VK_NULL_HANDLE;
        VkPipeline            renderPipe = VK_NULL_HANDLE;

        // Experimental SPH vertical slice (`--liquid-solver sph`).  Solver
        // buffers live in the reference simulation units; the shared 80-byte
        // particle buffer above remains the world-space presentation ABI, so
        // the surface splat/raymarch and rigid pipelines are reused as-is.
        bool           isSph = false;
        std::uint32_t  sphTableSize = 0;
        VkBuffer       sphPositions = VK_NULL_HANDLE;
        VkDeviceMemory sphPositionsMem = VK_NULL_HANDLE;
        VkBuffer       sphVelocities = VK_NULL_HANDLE;
        VkDeviceMemory sphVelocitiesMem = VK_NULL_HANDLE;
        VkBuffer       sphPredicted = VK_NULL_HANDLE;
        VkDeviceMemory sphPredictedMem = VK_NULL_HANDLE;
        VkBuffer       sphDensities = VK_NULL_HANDLE;
        VkDeviceMemory sphDensitiesMem = VK_NULL_HANDLE;
        VkBuffer       sphKeys = VK_NULL_HANDLE;
        VkDeviceMemory sphKeysMem = VK_NULL_HANDLE;
        VkBuffer       sphCellCounts = VK_NULL_HANDLE;
        VkDeviceMemory sphCellCountsMem = VK_NULL_HANDLE;
        VkBuffer       sphCellStarts = VK_NULL_HANDLE;
        VkDeviceMemory sphCellStartsMem = VK_NULL_HANDLE;
        VkBuffer       sphCellCursor = VK_NULL_HANDLE;
        VkDeviceMemory sphCellCursorMem = VK_NULL_HANDLE;
        VkBuffer       sphSortedIndices = VK_NULL_HANDLE;
        VkDeviceMemory sphSortedIndicesMem = VK_NULL_HANDLE;
        VkBuffer       sphScanSums = VK_NULL_HANDLE;
        VkDeviceMemory sphScanSumsMem = VK_NULL_HANDLE;
        VkBuffer       sphScanSums2 = VK_NULL_HANDLE;
        VkDeviceMemory sphScanSums2Mem = VK_NULL_HANDLE;
        VkBuffer       sphSeedPositions = VK_NULL_HANDLE;
        VkDeviceMemory sphSeedPositionsMem = VK_NULL_HANDLE;
        VkDescriptorSetLayout sphSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool      sphPool = VK_NULL_HANDLE;
        VkDescriptorSet       sphSet = VK_NULL_HANDLE;
        VkPipelineLayout      sphLayout = VK_NULL_HANDLE;
        VkPipeline sphExternalPipe = VK_NULL_HANDLE;
        VkPipeline sphHashCountPipe = VK_NULL_HANDLE;
        VkPipeline sphScanBlockPipe = VK_NULL_HANDLE;
        VkPipeline sphScanAddPipe = VK_NULL_HANDLE;
        VkPipeline sphScatterPipe = VK_NULL_HANDLE;
        VkPipeline sphDensityPipe = VK_NULL_HANDLE;
        VkPipeline sphPressurePipe = VK_NULL_HANDLE;
        VkPipeline sphViscosityPipe = VK_NULL_HANDLE;
        VkPipeline sphIntegratePipe = VK_NULL_HANDLE;

        std::uint32_t gridX = 0, gridY = 0, gridZ = 0;
        std::uint32_t surfaceX = 0, surfaceY = 0, surfaceZ = 0;
        std::uint32_t particleCount = 0;
        std::uint32_t bodyCount = 0;
        std::uint32_t substeps = 0;
        std::uint32_t raySteps = 0;
        std::uint32_t shaderVersion = 0;
        float dx = 0.0f;
        float particleSpacing = 0.0f;
        float particleMass = 0.0f;
        float simTime = 0.0f;
        float presentationTime = 0.0f;
        bool isV2 = false;
        bool captureChoreographyResetDone = false;
    } cinematicLiquid_;

    VkCommandPool              commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkCommandBuffer> headlessCmdBuffers_;

    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence>     inFlightFences_;
    std::vector<VkFence>                        imagesInFlight_;
    std::vector<bool>        timestampSlotReady_;
    std::uint32_t currentFrame_ = 0;
    float         fractalElapsed_ = 0.0f;   // StressFractal palette animation time

    static constexpr std::uint32_t kTimestampsPerFrame = 4;
    VkQueryPool timestampQueryPool_ = VK_NULL_HANDLE;
    float       timestampPeriodNs_  = 0.0f;
    bool        timestampsSupported_ = false;

    PFN_vkCmdBeginDebugUtilsLabelEXT  vkCmdBeginDebugUtilsLabel_  = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT    vkCmdEndDebugUtilsLabel_    = nullptr;
    PFN_vkSetDebugUtilsObjectNameEXT  vkSetDebugUtilsObjectName_  = nullptr;
    bool debugUtilsAvailable_ = false;

    void LoadDebugUtilsFunctions();
    void SetObjectName(VkObjectType type, std::uint64_t handle, const char* name) const;
    void BeginDebugLabel(VkCommandBuffer cmd, const char* name, float r, float g, float b) const;
    void EndDebugLabel(VkCommandBuffer cmd) const;

    static constexpr const char* kRequiredDeviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    void CreateInstance();
    void CreateSurface();
    QueueFamilyIndices      FindQueueFamilies(VkPhysicalDevice dev) const;
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice dev) const;
    bool CheckDeviceExtensionSupport(VkPhysicalDevice dev) const;
    bool IsDeviceSuitable(VkPhysicalDevice dev) const;
    VkShaderModule CreateShaderModule(const std::vector<char>& code) const;
    std::uint32_t  FindMemoryType(std::uint32_t typeFilter, VkMemoryPropertyFlags props) const;
    static const char* DeviceTypeName(VkPhysicalDeviceType type);
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateParticleBuffer();
    void CreateGraphicsPipeline();
    void CreateFullscreenPipeline(const char* vertSpv, const char* fragSpv);
    void CreateRender3DPipeline();
    // Fluid (Stam 2D Eulerian): 4 compute passes + 1 fullscreen render pass.
    // All fluid resources are isolated from the other workloads' pipeline
    // slots so existing paths are untouched.
    void CreateFluidResources();
    void CleanupFluidResources();
    void RecordFluidFrame(VkCommandBuffer cmd, float deltaTime,
                          std::uint32_t imageIndex, std::uint32_t timestampBase);
    void CreateCinematicLiquidResources();
    void CreateCinematicLiquidV2Resources();
    void CreateCinematicLiquidSphResources(const std::vector<float>& simSeed);
    void RecordCinematicLiquidSphFrame(VkCommandBuffer cmd, float deltaTime,
                                       std::uint32_t imageIndex,
                                       std::uint32_t timestampBase);
    void CleanupCinematicLiquidResources();
    void RecordCinematicLiquidFrame(VkCommandBuffer cmd, float deltaTime,
                                    std::uint32_t imageIndex,
                                    std::uint32_t timestampBase);
    void RecordCinematicLiquidV2Frame(VkCommandBuffer cmd, float deltaTime,
                                      std::uint32_t imageIndex,
                                      std::uint32_t timestampBase);
    void CreateDepthResources();
    void CreateQuadBuffer();
    void CreateComputeDescriptorSetLayout();
    void CreateComputeDescriptorResources();
    void CreateComputePipeline();
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>&) const;
    VkPresentModeKHR   ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>&) const;
    VkExtent2D         ChooseSwapExtent(const VkSurfaceCapabilitiesKHR&) const;
    void CreateSwapChain();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateCommandBuffers();
    void RecordCommandBuffer(std::uint32_t imageIndex, float deltaTime,
                             std::uint32_t deviceMask);
    void CreateSyncObjects();
    void CreateTimestampQueryPool();
    void CollectTimestampResults(std::uint32_t frameSlot);
    void CleanupSwapChain();
};

}  // namespace gpu_bench

#endif  // HAVE_VULKAN
