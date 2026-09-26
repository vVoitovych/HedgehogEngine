#include "RHIImGui/GuiRenderer.hpp"

#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHITexture.hpp"
#include "RHI/api/Vulkan/VulkanNative.hpp"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"

#include <array>
#include <cassert>

namespace RHIImGui
{
    namespace
    {
        // ImGui's Vulkan backend (imgui_impl_vulkan), fed the Vulkan backend's native handles.
        class VulkanGuiRenderer final : public IGuiRenderer
        {
        public:
            VulkanGuiRenderer(RHI::IRHIDevice& device, const GuiRendererDesc& desc)
                : m_Handles(RHI::VulkanNative::GetDeviceHandles(device))
            {
                constexpr std::array<VkDescriptorPoolSize, 11> POOL_SIZES = {{
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
                poolInfo.poolSizeCount = static_cast<uint32_t>(POOL_SIZES.size());
                poolInfo.pPoolSizes    = POOL_SIZES.data();
                [[maybe_unused]] const VkResult poolResult =
                    vkCreateDescriptorPool(m_Handles.Device, &poolInfo, nullptr, &m_Pool);
                assert(poolResult == VK_SUCCESS && "VulkanGuiRenderer: vkCreateDescriptorPool failed.");

                // Dynamic rendering (Vulkan 1.3 core): the pipeline is built for one colour format and
                // needs no render pass or framebuffer, so Render() can draw into any texture of it.
                m_ColorFormat = RHI::VulkanNative::ToVkFormat(desc.ColorFormat);

                ImGui_ImplVulkan_InitInfo initInfo{};
                initInfo.Instance            = m_Handles.Instance;
                initInfo.PhysicalDevice      = m_Handles.PhysicalDevice;
                initInfo.Device              = m_Handles.Device;
                initInfo.QueueFamily         = m_Handles.GraphicsQueueFamily;
                initInfo.Queue               = m_Handles.GraphicsQueue;
                initInfo.DescriptorPool      = m_Pool;
                initInfo.MinImageCount       = desc.MinImageCount;
                initInfo.ImageCount          = desc.ImageCount;
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
                [[maybe_unused]] const VkResult samplerResult =
                    vkCreateSampler(m_Handles.Device, &samplerInfo, nullptr, &m_Sampler);
                assert(samplerResult == VK_SUCCESS && "VulkanGuiRenderer: vkCreateSampler failed.");
            }

            ~VulkanGuiRenderer() override
            {
                ImGui_ImplVulkan_Shutdown();
                if (m_Sampler != VK_NULL_HANDLE)
                    vkDestroySampler(m_Handles.Device, m_Sampler, nullptr);
                vkDestroyDescriptorPool(m_Handles.Device, m_Pool, nullptr);
            }

            void NewFrame() override { ImGui_ImplVulkan_NewFrame(); }

            void Render(RHI::IRHICommandList& cmd, RHI::IRHITexture& target) override
            {
                RHI::RenderingAttachment color;
                color.Texture     = &target;
                color.LoadOp      = RHI::LoadOp::Clear;
                color.StoreOp     = RHI::StoreOp::Store;
                color.Clear.Color = { 0.0f, 0.0f, 0.0f, 1.0f };

                RHI::RenderingInfo info;
                info.ColorAttachments = { color };
                info.Width            = target.GetWidth();
                info.Height           = target.GetHeight();
                cmd.BeginRendering(info);
                ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), RHI::VulkanNative::GetCommandBuffer(cmd));
                cmd.EndRendering();
            }

            void* CreateTextureId(const RHI::IRHITexture& texture) override
            {
                return ImGui_ImplVulkan_AddTexture(m_Sampler, RHI::VulkanNative::GetImageView(texture),
                                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            void DestroyTextureId(void* id) override
            {
                if (id != nullptr)
                    ImGui_ImplVulkan_RemoveTexture(static_cast<VkDescriptorSet>(id));
            }

        private:
            RHI::VulkanNative::DeviceHandles m_Handles;
            VkDescriptorPool                 m_Pool        = VK_NULL_HANDLE;
            VkSampler                        m_Sampler     = VK_NULL_HANDLE;
            VkFormat                         m_ColorFormat = VK_FORMAT_UNDEFINED;
        };
    }

    std::unique_ptr<IGuiRenderer> CreateGuiRenderer(RHI::IRHIDevice& device, const GuiRendererDesc& desc)
    {
        return std::make_unique<VulkanGuiRenderer>(device, desc);
    }
}
