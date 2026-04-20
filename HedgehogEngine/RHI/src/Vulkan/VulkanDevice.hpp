#pragma once

#include "RHI/api/IRHIDevice.hpp"

#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace RHI
{

// Internal queue family info — not part of the public RHI interface.
struct QueueFamilyIndices
{
    std::optional<uint32_t> m_GraphicsFamily;
    std::optional<uint32_t> m_PresentFamily;

    bool IsComplete() const
    {
        return m_GraphicsFamily.has_value() && m_PresentFamily.has_value();
    }
};

class VulkanDevice final : public IRHIDevice
{
public:
    explicit VulkanDevice(const NativeWindowDesc& desc);
    ~VulkanDevice() override;

    VulkanDevice(const VulkanDevice&)            = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    VulkanDevice(VulkanDevice&&)                 = delete;
    VulkanDevice& operator=(VulkanDevice&&)      = delete;

    // ── IRHIDevice ────────────────────────────────────────────────────────────

    std::unique_ptr<IRHIBuffer> CreateBuffer(
        size_t size, BufferUsage usage, MemoryUsage memUsage) const override;

    std::unique_ptr<IRHITexture> CreateTexture(const TextureDesc& desc) const override;

    std::unique_ptr<IRHISampler> CreateSampler(const SamplerDesc& desc) const override;

    std::unique_ptr<IRHIShader> CreateShader(
        const std::string& filePath, ShaderStage stage) const override;

    std::unique_ptr<IRHIDescriptorSetLayout> CreateDescriptorSetLayout(
        const std::vector<DescriptorBinding>& bindings) const override;

    std::unique_ptr<IRHIDescriptorPool> CreateDescriptorPool(
        uint32_t maxSets, const std::vector<PoolSize>& poolSizes) const override;

    std::unique_ptr<IRHIDescriptorSet> AllocateDescriptorSet(
        const IRHIDescriptorPool& pool, const IRHIDescriptorSetLayout& layout) const override;

    std::unique_ptr<IRHIRenderPass>  CreateRenderPass(const RenderPassDesc& desc) const override;
    std::unique_ptr<IRHIFramebuffer> CreateFramebuffer(const FramebufferDesc& desc) const override;
    std::unique_ptr<IRHIPipeline>    CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) const override;

    std::unique_ptr<IRHICommandList> CreateCommandList() const override;
    std::unique_ptr<IRHISwapchain>   CreateSwapchain(uint32_t width, uint32_t height) const override;

    std::unique_ptr<IRHIFence>     CreateFence(bool signaled = false) const override;
    std::unique_ptr<IRHISemaphore> CreateSemaphore() const override;

    void SubmitCommandList(
        const IRHICommandList&             commandList,
        const std::vector<IRHISemaphore*>& waitSemaphores,
        const std::vector<IRHISemaphore*>& signalSemaphores,
        IRHIFence*                         signalFence) override;

    Format GetPreferredDepthFormat() const override;

    void ExecuteImmediately(std::function<void(IRHICommandList&)> func) const override;

    void WaitIdle() const override;

    // ── Internal accessors (Vulkan backend only) ──────────────────────────────

    VkInstance       GetInstance()       const { return m_Instance;       }
    VkDevice         GetHandle()         const { return m_Device;         }
    VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
    VkSurfaceKHR     GetSurface()        const { return m_Surface;        }
    VkQueue          GetGraphicsQueue()  const { return m_GraphicsQueue;  }
    VkQueue          GetPresentQueue()   const { return m_PresentQueue;   }
    VkCommandPool    GetCommandPool()    const { return m_CommandPool;    }
    VmaAllocator     GetAllocator()      const { return m_Allocator;      }

    const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_Indices; }

private:
    void CreateInstance();
    void SetupDebugMessenger();
    void CreateSurface(void* nativeHandle);
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateAllocator();
    void CreateCommandPool();

    bool                     IsDeviceSuitable(VkPhysicalDevice device) const;
    bool                     CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
    int                      GetDeviceScore(VkPhysicalDevice device) const;
    QueueFamilyIndices        FindQueueFamilies(VkPhysicalDevice device) const;
    std::vector<const char*> GetRequiredExtensions() const;
    bool                     CheckValidationLayerSupport() const;

    VkInstance               m_Instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR             m_Surface        = VK_NULL_HANDLE;
    VkPhysicalDevice         m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice                 m_Device         = VK_NULL_HANDLE;

    VkQueue                  m_GraphicsQueue  = VK_NULL_HANDLE;
    VkQueue                  m_PresentQueue   = VK_NULL_HANDLE;

    QueueFamilyIndices       m_Indices;

    VmaAllocator             m_Allocator      = VK_NULL_HANDLE;
    VkCommandPool            m_CommandPool    = VK_NULL_HANDLE;

    const char**             m_VkExtensions      = nullptr;
    uint32_t                 m_VkExtensionCount  = 0;

    std::vector<const char*> m_ValidationLayers;
    std::vector<const char*> m_DeviceExtensions;
};

} // namespace RHI
