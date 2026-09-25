#pragma once

#include "RHI/api/IRHIGuiBackend.hpp"

#include <Volk/volk.h>
#include <memory>

namespace RHI
{
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

        void  NewFrame() override;
        void  Render(IRHICommandList& cmd, IRHITexture& target) override;
        void* CreateTextureId(const IRHITexture& texture) override;
        void  DestroyTextureId(void* id) override;

    private:
        VulkanDevice&    m_Device;
        VkFormat         m_ColorFormat = VK_FORMAT_UNDEFINED; // the pipeline's; kept alive for its create info
        VkDescriptorPool m_Pool        = VK_NULL_HANDLE;
        VkSampler        m_Sampler     = VK_NULL_HANDLE;
    };

} // namespace RHI
