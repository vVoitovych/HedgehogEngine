#include "GuiPass.hpp"

#include "ResourceManager/ResourceManager.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIFramebuffer.hpp"
#include "RHI/api/IRHIGuiBackend.hpp"
#include "RHI/api/RHITypes.hpp"

#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"

namespace Renderer
{
    GuiPass::GuiPass(HW::Window& window, RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        ImGui::StyleColorsDark();

        {
            constexpr ImVec4 k_PanelBg(2.0f / 255.0f, 12.0f / 255.0f, 30.0f / 255.0f, 1.0f);
            ImVec4* colors = ImGui::GetStyle().Colors;
            colors[ImGuiCol_WindowBg]  = k_PanelBg;
            colors[ImGuiCol_ChildBg]   = k_PanelBg;
            colors[ImGuiCol_PopupBg]   = k_PanelBg;
            colors[ImGuiCol_MenuBarBg] = k_PanelBg;
        }

        ImGui_ImplGlfw_InitForVulkan(window.GetNativeHandle(), true);

        const auto& colorBuffer = resourceManager.GetRHIColorBuffer();

        RHI::GuiBackendDesc backendDesc;
        backendDesc.MinImageCount = HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        backendDesc.ImageCount    = HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        backendDesc.ColorFormat   = colorBuffer.GetFormat();
        m_GuiBackend = device.CreateGuiBackend(backendDesc);

        RHI::FramebufferDesc fbDesc;
        fbDesc.RenderPass       = &m_GuiBackend->GetRenderPass();
        fbDesc.ColorAttachments = { &colorBuffer };
        fbDesc.Width            = colorBuffer.GetWidth();
        fbDesc.Height           = colorBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);

        CreateSceneViewDescSet(resourceManager);
        CreateGameViewDescSet(resourceManager);
    }

    GuiPass::~GuiPass()
    {
    }

    void GuiPass::BeginFrame()
    {
        m_GuiBackend->NewFrame();
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
        m_GuiBackend->Render(cmd, *m_FrameBuffer);
    }

    void GuiPass::Cleanup(RHI::IRHIDevice& /*device*/)
    {
        m_GuiBackend->DestroyTextureId(m_SceneViewId);
        m_SceneViewId = nullptr;
        m_GuiBackend->DestroyTextureId(m_GameViewId);
        m_GameViewId = nullptr;

        m_FrameBuffer.reset();
        m_GuiBackend.reset();   // calls ImGui_ImplVulkan_Shutdown() before DestroyContext

        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void GuiPass::ResizeResources(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        const auto& colorBuffer = resourceManager.GetRHIColorBuffer();

        m_FrameBuffer.reset();

        RHI::FramebufferDesc fbDesc;
        fbDesc.RenderPass       = &m_GuiBackend->GetRenderPass();
        fbDesc.ColorAttachments = { &colorBuffer };
        fbDesc.Width            = colorBuffer.GetWidth();
        fbDesc.Height           = colorBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);

        m_GuiBackend->DestroyTextureId(m_SceneViewId);
        m_SceneViewId = nullptr;
        CreateSceneViewDescSet(resourceManager);
    }

    void GuiPass::RecreateSceneDescriptor(const ResourceManager& resourceManager)
    {
        m_GuiBackend->DestroyTextureId(m_SceneViewId);
        m_SceneViewId = nullptr;
        CreateSceneViewDescSet(resourceManager);
    }

    void GuiPass::RecreateGameDescriptor(const ResourceManager& resourceManager)
    {
        m_GuiBackend->DestroyTextureId(m_GameViewId);
        m_GameViewId = nullptr;
        CreateGameViewDescSet(resourceManager);
    }

    void* GuiPass::GetSceneViewTextureId() const
    {
        return m_SceneViewId;
    }

    void* GuiPass::GetGameViewTextureId() const
    {
        return m_GameViewId;
    }

    void GuiPass::CreateSceneViewDescSet(const ResourceManager& resourceManager)
    {
        m_SceneViewId = m_GuiBackend->CreateTextureId(resourceManager.GetSceneColorBuffer());
    }

    void GuiPass::CreateGameViewDescSet(const ResourceManager& resourceManager)
    {
        m_GameViewId = m_GuiBackend->CreateTextureId(resourceManager.GetColorBuffer(RenderTargetId::Game));
    }
}
