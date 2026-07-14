#pragma once

#include "RHI/api/IRHIGuiBackend.hpp"

#include <Volk/volk.h>
#include <memory>

namespace RHI
{
    class IRHIRenderPass;
    class VulkanDevice;

    class VulkanGuiBackend final : public IRHIGuiBackend
    {
    public:
        VulkanGuiBackend(VulkanDevice& device, const GuiBackendDesc& desc);
        ~VulkanGuiBackend() override;

        VulkanGuiBackend(const VulkanGuiBackend&)            = delete;
        VulkanGuiBackend& operator=(const VulkanGuiBackend&) = delete;
        VulkanGuiBackend(VulkanGuiBackend&&)                 = delete;
        VulkanGuiBackend& operator=(VulkanGuiBackend&&)      = delete;

        void            NewFrame() override;
        void            Render(IRHICommandList& cmd, IRHIFramebuffer& framebuffer) override;
        IRHIRenderPass& GetRenderPass() override;
        void*           CreateTextureId(const IRHITexture& texture) override;
        void            DestroyTextureId(void* id) override;

    private:
        VulkanDevice&                   m_Device;
        std::unique_ptr<IRHIRenderPass> m_RenderPass;
        VkDescriptorPool                m_Pool    = VK_NULL_HANDLE;
        VkSampler                       m_Sampler = VK_NULL_HANDLE;
    };

} // namespace RHI
