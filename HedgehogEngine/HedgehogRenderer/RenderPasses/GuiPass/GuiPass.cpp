#include "GuiPass.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogContext/Context/ThreadContext.hpp"
#include "HedgehogContext/Context/FrameContext.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"

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

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

#include <vector>
#include <algorithm>

namespace Renderer
{
    GuiPass::GuiPass(const Context::Context& context, const ResourceManager& resourceManager)
    {
        WinManager::WindowManager::SetOnGuiCallback([]() {
            return GuiPass::IsCursorPositionInGUI();
        });

        auto& vulkanContext = context.GetVulkanContext();
        auto& rhiDevice     = vulkanContext.GetRHIDevice();
        auto& vkDevice      = static_cast<RHI::VulkanDevice&>(rhiDevice);

        // Raw VkDescriptorPool for ImGui (imgui_impl_vulkan requires a native pool)
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

        // Render pass: color-only, load/store, ColorAttachment → ColorAttachment
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
        m_RenderPass = rhiDevice.CreateRenderPass(rpDesc);

        // Framebuffer
        const auto& colorBuffer = resourceManager.GetRHIColorBuffer();
        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass       = m_RenderPass.get();
        fbDesc.m_ColorAttachments = { &colorBuffer };
        fbDesc.m_Width            = colorBuffer.GetWidth();
        fbDesc.m_Height           = colorBuffer.GetHeight();
        m_FrameBuffer = rhiDevice.CreateFramebuffer(fbDesc);

        // ImGui init
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(
            const_cast<GLFWwindow*>(vulkanContext.GetWindowManager().GetGlfwWindow()), true);

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

    void GuiPass::Render(Context::Context& context, const ResourceManager& resourceManager)
    {
        auto& threadContext = context.GetThreadContext();
        auto& commandList   = threadContext.GetCommandList();

        // Transition color buffer from Present (left by ForwardPass) to ColorAttachment
        auto& colorBuffer = const_cast<RHI::IRHITexture&>(resourceManager.GetRHIColorBuffer());
        commandList.TransitionTexture(colorBuffer,
            RHI::ImageLayout::Present,
            RHI::ImageLayout::ColorAttachment);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        DrawGui(context);
        ImGui::Render();

        RHI::ClearValue colorClear;
        colorClear.m_Color = { 0.0f, 0.0f, 0.0f, 1.0f };
        commandList.BeginRenderPass(*m_RenderPass, *m_FrameBuffer, { colorClear });

        auto& vkCmdList = static_cast<RHI::VulkanCommandList&>(commandList);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmdList.GetHandle());

        commandList.EndRenderPass();
    }

    void GuiPass::Cleanup(const Context::Context& context)
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        auto& vkDevice = static_cast<RHI::VulkanDevice&>(context.GetVulkanContext().GetRHIDevice());
        vkDestroyDescriptorPool(vkDevice.GetHandle(), m_ImGuiPool, nullptr);
        m_ImGuiPool = VK_NULL_HANDLE;

        m_FrameBuffer.reset();
        m_RenderPass.reset();
    }

    void GuiPass::ResizeResources(const Context::Context& context, const ResourceManager& resourceManager)
    {
        auto& rhiDevice         = context.GetVulkanContext().GetRHIDevice();
        const auto& colorBuffer = resourceManager.GetRHIColorBuffer();

        m_FrameBuffer.reset();

        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass       = m_RenderPass.get();
        fbDesc.m_ColorAttachments = { &colorBuffer };
        fbDesc.m_Width            = colorBuffer.GetWidth();
        fbDesc.m_Height           = colorBuffer.GetHeight();
        m_FrameBuffer = rhiDevice.CreateFramebuffer(fbDesc);
    }

    bool GuiPass::IsCursorPositionInGUI()
    {
        ImGuiIO& io = ImGui::GetIO();

        if (io.WantCaptureMouse)
            return true;

        return false;
    }

    void GuiPass::UploadFonts()
    {
    }

    void GuiPass::DrawGui(Context::Context& context)
    {
        DrawInspector(context);
        DrawSceneInspector(context);
        DrawMainMenu(context);
        DrawSettingsWindow(context);
        // TODO remove
        ImGui::ShowDemoWindow();
    }

}
