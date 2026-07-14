#include "VulkanGuiBackend.hpp"
#include "VulkanDevice.hpp"
#include "VulkanCommandList.hpp"
#include "VulkanRenderPass.hpp"
#include "VulkanTexture.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIFramebuffer.hpp"
#include "RHI/api/RHITypes.hpp"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"

#include <array>

namespace RHI
{
    VulkanGuiBackend::VulkanGuiBackend(VulkanDevice& device, const GuiBackendDesc& desc)
        : m_Device(device)
    {
        constexpr std::array<VkDescriptorPoolSize, 11> k_PoolSizes = {{
            { VK_DESCRIPTOR_TYPE_SAMPLER,                1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       1000 },
        }};

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets       = 1000;
        poolInfo.poolSizeCount = static_cast<uint32_t>(k_PoolSizes.size());
        poolInfo.pPoolSizes    = k_PoolSizes.data();
        vkCreateDescriptorPool(m_Device.GetHandle(), &poolInfo, nullptr, &m_Pool);

        RenderPassDesc rpDesc;
        rpDesc.m_ColorAttachments.push_back(AttachmentDesc{
            desc.m_ColorFormat,
            LoadOp::Clear,
            StoreOp::Store,
            LoadOp::DontCare,
            StoreOp::DontCare,
            ImageLayout::Undefined,
            ImageLayout::ColorAttachment
        });
        m_RenderPass = m_Device.CreateRenderPass(rpDesc);

        auto& vkRenderPass = static_cast<VulkanRenderPass&>(*m_RenderPass);

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance       = m_Device.GetInstance();
        initInfo.PhysicalDevice = m_Device.GetPhysicalDevice();
        initInfo.Device         = m_Device.GetHandle();
        initInfo.QueueFamily    = m_Device.GetQueueFamilyIndices().m_GraphicsFamily.value();
        initInfo.Queue          = m_Device.GetGraphicsQueue();
        initInfo.DescriptorPool = m_Pool;
        initInfo.MinImageCount  = desc.m_MinImageCount;
        initInfo.ImageCount     = desc.m_ImageCount;
        initInfo.PipelineInfoMain.RenderPass = vkRenderPass.GetHandle();
        ImGui_ImplVulkan_Init(&initInfo);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter    = VK_FILTER_LINEAR;
        samplerInfo.minFilter    = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(m_Device.GetHandle(), &samplerInfo, nullptr, &m_Sampler);
    }

    VulkanGuiBackend::~VulkanGuiBackend()
    {
        ImGui_ImplVulkan_Shutdown();

        if (m_Sampler != VK_NULL_HANDLE)
            vkDestroySampler(m_Device.GetHandle(), m_Sampler, nullptr);

        vkDestroyDescriptorPool(m_Device.GetHandle(), m_Pool, nullptr);
    }

    void VulkanGuiBackend::NewFrame()
    {
        ImGui_ImplVulkan_NewFrame();
    }

    void VulkanGuiBackend::Render(IRHICommandList& cmd, IRHIFramebuffer& framebuffer)
    {
        ClearValue colorClear;
        colorClear.m_Color = { 0.0f, 0.0f, 0.0f, 1.0f };
        cmd.BeginRenderPass(*m_RenderPass, framebuffer, { colorClear });

        auto& vkCmd = static_cast<VulkanCommandList&>(cmd);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmd.GetHandle());

        cmd.EndRenderPass();
    }

    IRHIRenderPass& VulkanGuiBackend::GetRenderPass()
    {
        return *m_RenderPass;
    }

    void* VulkanGuiBackend::CreateTextureId(const IRHITexture& texture)
    {
        const auto& vkTexture = static_cast<const VulkanTexture&>(texture);
        return ImGui_ImplVulkan_AddTexture(
            m_Sampler,
            vkTexture.GetViewHandle(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void VulkanGuiBackend::DestroyTextureId(void* id)
    {
        if (id != nullptr)
            ImGui_ImplVulkan_RemoveTexture(static_cast<VkDescriptorSet>(id));
    }

} // namespace RHI
