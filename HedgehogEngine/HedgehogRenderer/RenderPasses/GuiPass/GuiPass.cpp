#include "GuiPass.hpp"

#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIRenderPass.hpp"
#include "RHI/api/IRHIFramebuffer.hpp"
#include "RHI/api/IRHITexture.hpp"
#include "RHI/api/RHITypes.hpp"
#include "RHI/src/Vulkan/VulkanDevice.hpp"
#include "RHI/src/Vulkan/VulkanRenderPass.hpp"
#include "RHI/src/Vulkan/VulkanCommandList.hpp"
#include "RHI/src/Vulkan/VulkanTexture.hpp"

#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

#include <algorithm>
#include <vector>

namespace Renderer
{
    GuiPass::GuiPass(HW::Window& window, RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        window.SetGuiCallback([]() { return GuiPass::IsCursorPositionInGUI(); });

        auto& vkDevice = static_cast<const RHI::VulkanDevice&>(device);

        // Raw VkDescriptorPool for ImGui
        VkDescriptorPoolSize poolSizes[] = {
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
        };
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets       = 1000;
        poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
        poolInfo.pPoolSizes    = poolSizes;
        vkCreateDescriptorPool(vkDevice.GetHandle(), &poolInfo, nullptr, &m_ImGuiPool);

        // Render pass: color-only, clear+store (scene is in a separate texture)
        RHI::RenderPassDesc rpDesc;
        rpDesc.m_ColorAttachments.push_back(RHI::AttachmentDesc{
            resourceManager.GetRHIColorBuffer().GetFormat(),
            RHI::LoadOp::Clear,
            RHI::StoreOp::Store,
            RHI::LoadOp::DontCare,
            RHI::StoreOp::DontCare,
            RHI::ImageLayout::Undefined,
            RHI::ImageLayout::ColorAttachment
        });
        m_RenderPass = device.CreateRenderPass(rpDesc);

        const auto& colorBuffer = resourceManager.GetRHIColorBuffer();
        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass       = m_RenderPass.get();
        fbDesc.m_ColorAttachments = { &colorBuffer };
        fbDesc.m_Width            = colorBuffer.GetWidth();
        fbDesc.m_Height           = colorBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(window.GetNativeHandle(), true);

        auto& vkRenderPass = static_cast<RHI::VulkanRenderPass&>(*m_RenderPass);

        ImGui_ImplVulkan_InitInfo initInfo = {};
        initInfo.Instance        = vkDevice.GetInstance();
        initInfo.PhysicalDevice  = vkDevice.GetPhysicalDevice();
        initInfo.Device          = vkDevice.GetHandle();
        initInfo.QueueFamily     = vkDevice.GetQueueFamilyIndices().m_GraphicsFamily.value();
        initInfo.Queue           = vkDevice.GetGraphicsQueue();
        initInfo.PipelineCache   = VK_NULL_HANDLE;
        initInfo.DescriptorPool  = m_ImGuiPool;
        initInfo.Allocator       = nullptr;
        initInfo.MinImageCount   = MAX_FRAMES_IN_FLIGHT;
        initInfo.ImageCount      = MAX_FRAMES_IN_FLIGHT;
        initInfo.CheckVkResultFn = nullptr;
        initInfo.PipelineInfoMain.RenderPass = vkRenderPass.GetHandle();
        ImGui_ImplVulkan_Init(&initInfo);

        UploadFonts();

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter    = VK_FILTER_LINEAR;
        samplerInfo.minFilter    = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        vkCreateSampler(vkDevice.GetHandle(), &samplerInfo, nullptr, &m_SceneSampler);

        CreateSceneViewDescSet(resourceManager);
    }

    GuiPass::~GuiPass()
    {
    }

    void GuiPass::BeginFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void GuiPass::DiscardFrame()
    {
        ImGui::EndFrame();
    }

    void GuiPass::Render(RHI::IRHICommandList& cmd, const ResourceManager& resourceManager)
    {
        ImGui::Render();

        RHI::ClearValue colorClear;
        colorClear.m_Color = { 0.0f, 0.0f, 0.0f, 1.0f };
        cmd.BeginRenderPass(*m_RenderPass, *m_FrameBuffer, { colorClear });

        auto& vkCmdList = static_cast<RHI::VulkanCommandList&>(cmd);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmdList.GetHandle());

        cmd.EndRenderPass();
    }

    void GuiPass::Cleanup(RHI::IRHIDevice& device)
    {
        if (m_SceneViewDescSet != VK_NULL_HANDLE)
        {
            ImGui_ImplVulkan_RemoveTexture(m_SceneViewDescSet);
            m_SceneViewDescSet = VK_NULL_HANDLE;
        }

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        auto& vkDevice = static_cast<const RHI::VulkanDevice&>(device);

        if (m_SceneSampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(vkDevice.GetHandle(), m_SceneSampler, nullptr);
            m_SceneSampler = VK_NULL_HANDLE;
        }

        vkDestroyDescriptorPool(vkDevice.GetHandle(), m_ImGuiPool, nullptr);
        m_ImGuiPool = VK_NULL_HANDLE;

        m_FrameBuffer.reset();
        m_RenderPass.reset();
    }

    void GuiPass::ResizeResources(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        const auto& colorBuffer = resourceManager.GetRHIColorBuffer();

        m_FrameBuffer.reset();

        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass       = m_RenderPass.get();
        fbDesc.m_ColorAttachments = { &colorBuffer };
        fbDesc.m_Width            = colorBuffer.GetWidth();
        fbDesc.m_Height           = colorBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);

        if (m_SceneViewDescSet != VK_NULL_HANDLE)
        {
            ImGui_ImplVulkan_RemoveTexture(m_SceneViewDescSet);
            m_SceneViewDescSet = VK_NULL_HANDLE;
        }
        CreateSceneViewDescSet(resourceManager);
    }

    bool GuiPass::IsCursorPositionInGUI()
    {
        return ImGui::GetIO().WantCaptureMouse;
    }

    void* GuiPass::GetSceneViewTextureId() const
    {
        return static_cast<void*>(m_SceneViewDescSet);
    }

    void GuiPass::UploadFonts()
    {
    }

    void GuiPass::RecreateSceneDescriptor(const ResourceManager& resourceManager)
    {
        if (m_SceneViewDescSet != VK_NULL_HANDLE)
        {
            ImGui_ImplVulkan_RemoveTexture(m_SceneViewDescSet);
            m_SceneViewDescSet = VK_NULL_HANDLE;
        }
        CreateSceneViewDescSet(resourceManager);
    }

    void GuiPass::CreateSceneViewDescSet(const ResourceManager& resourceManager)
    {
        const auto& sceneBuffer = static_cast<const RHI::VulkanTexture&>(
            resourceManager.GetSceneColorBuffer());
        m_SceneViewDescSet = ImGui_ImplVulkan_AddTexture(
            m_SceneSampler,
            sceneBuffer.GetViewHandle(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}
