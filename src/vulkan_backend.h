#pragma once

#ifdef HAVE_VULKAN

#include "app_base.h"

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

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
    VkDevice         device_        = VK_NULL_HANDLE;

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
        // Two sets so we can swap in/out bindings between passes without
        // re-writing descriptor updates each frame.
        VkDescriptorSet       setAdvect  = VK_NULL_HANDLE;  // in=A, out=B, press unused
        VkDescriptorSet       setDiv     = VK_NULL_HANDLE;  // in=A, out=div
        VkDescriptorSet       setJacA    = VK_NULL_HANDLE;  // in=pressA, out=pressB, div
        VkDescriptorSet       setJacB    = VK_NULL_HANDLE;  // in=pressB, out=pressA, div
        VkDescriptorSet       setSub     = VK_NULL_HANDLE;  // in=A, out=B, press=final
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

    VkCommandPool              commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkCommandBuffer> headlessCmdBuffers_;

    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence>     inFlightFences_;
    std::vector<VkFence>                        imagesInFlight_;
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
    void RecordFluidFrame(VkCommandBuffer cmd, float deltaTime, std::uint32_t imageIndex);
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
    void RecordCommandBuffer(std::uint32_t imageIndex, float deltaTime);
    void CreateSyncObjects();
    void CreateTimestampQueryPool();
    void CollectTimestampResults(std::uint32_t frameSlot);
    void CleanupSwapChain();
};

}  // namespace gpu_bench

#endif  // HAVE_VULKAN
