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

        // Render pass: color-only, load/store
        RHI::RenderPassDesc rpDesc;
        rpDesc.m_ColorAttachments.push_back(RHI::AttachmentDesc{
            resourceManager.GetRHIColorBuffer().GetFormat(),
            RHI::LoadOp::Load,
            RHI::StoreOp::Store,
            RHI::LoadOp::DontCare,
            RHI::StoreOp::DontCare,
            RHI::ImageLayout::ColorAttachment,
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
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        auto& vkDevice = static_cast<const RHI::VulkanDevice&>(device);
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
    }

    bool GuiPass::IsCursorPositionInGUI()
    {
        return ImGui::GetIO().WantCaptureMouse;
    }

    void GuiPass::UploadFonts()
    {
    }
}
