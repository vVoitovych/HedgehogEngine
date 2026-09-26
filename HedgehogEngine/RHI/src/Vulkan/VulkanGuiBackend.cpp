#include "VulkanGuiBackend.hpp"
#include "VulkanDevice.hpp"
#include "VulkanCommandList.hpp"
#include "VulkanTexture.hpp"
#include "VulkanTypes.hpp"

#include "RHI/api/IRHICommandList.hpp"
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

        // Dynamic rendering (Vulkan 1.3 core): the pipeline is built for one colour format and
        // needs no render pass or framebuffer, so Render() can draw into any texture of it.
        m_ColorFormat = VulkanTypes::ToVkFormat(desc.ColorFormat);

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance       = m_Device.GetInstance();
        initInfo.PhysicalDevice = m_Device.GetPhysicalDevice();
        initInfo.Device         = m_Device.GetHandle();
        initInfo.QueueFamily    = m_Device.GetQueueFamilyIndices().GraphicsFamily.value();
        initInfo.Queue          = m_Device.GetGraphicsQueue();
        initInfo.DescriptorPool = m_Pool;
        initInfo.MinImageCount  = desc.MinImageCount;
        initInfo.ImageCount     = desc.ImageCount;
        initInfo.UseDynamicRendering = true;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount    = 1;
        initInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_ColorFormat;
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

    void VulkanGuiBackend::Render(IRHICommandList& cmd, IRHITexture& target)
    {
        RenderingAttachment color;
        color.Texture     = &target;
        color.LoadOp      = LoadOp::Clear;
        color.StoreOp     = StoreOp::Store;
        color.Clear.Color = { 0.0f, 0.0f, 0.0f, 1.0f };

        RenderingInfo info;
        info.ColorAttachments = { color };
        info.Width            = target.GetWidth();
        info.Height           = target.GetHeight();
        cmd.BeginRendering(info);

        auto& vkCmd = static_cast<VulkanCommandList&>(cmd);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmd.GetHandle());

        cmd.EndRendering();
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
