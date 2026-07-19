#include "GuiPass.hpp"

#include "Profiling/Profiler.hpp"
#include "RenderGraph/RenderGraph.hpp"
#include "RenderGraph/RenderGraphBuilder.hpp"

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
    GuiPass::GuiPass(HW::Window& window, RHI::IRHIDevice& device)
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

        RHI::GuiBackendDesc backendDesc;
        backendDesc.m_MinImageCount = HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        backendDesc.m_ImageCount    = HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        backendDesc.m_ColorFormat   = RHI::Format::R16G16B16A16Unorm; // guiColor's format
        m_GuiBackend = device.CreateGuiBackend(backendDesc);
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

    void GuiPass::Setup(RenderGraphBuilder& builder)
    {
        // Sampled: this pass reads sceneColor through an ImGui image (shader-sampled), so the
        // graph must transition it to ShaderReadOnly before Execute() — the one barrier the
        // plan calls out moving from RenderQueue::Render into the graph's per-pass step.
        m_SceneColorHandle = builder.ReadSampled(GraphResourceNames::SCENE_COLOR);
        m_GuiColorHandle   = builder.Write(GraphResourceNames::GUI_COLOR, RHI::ImageLayout::ColorAttachment);
    }

    void GuiPass::CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph)
    {
        // Called whenever guiColor (Swapchain) OR sceneColor (SceneView) invalidates — rebuilds
        // both the framebuffer and the ImGui scene-view descriptor unconditionally each time.
        // Harmless (a few descriptor/framebuffer allocations at resize time, not per-frame) and
        // keeps the pass simple; both rebuilds only ever happen at the same deferred, post-submit
        // point in Renderer::DrawFrame as before migration (see the scene-view descriptor risk
        // note in workflow/current-plan.md), so the ImTextureID lifetime guarantee is unchanged.
        const auto& guiColor = graph.GetTexture(m_GuiColorHandle);

        m_FrameBuffer.reset();
        RHI::FramebufferDesc fbDesc;
        fbDesc.m_RenderPass       = &m_GuiBackend->GetRenderPass();
        fbDesc.m_ColorAttachments = { &guiColor };
        fbDesc.m_Width            = guiColor.GetWidth();
        fbDesc.m_Height           = guiColor.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);

        m_GuiBackend->DestroyTextureId(m_SceneViewId);
        m_SceneViewId = nullptr;
        const auto& sceneColor = graph.GetTexture(m_SceneColorHandle);
        m_SceneViewId = m_GuiBackend->CreateTextureId(sceneColor);
    }

    void GuiPass::Execute(RenderGraphContext& ctx)
    {
        HH_PROFILE_ZONE("GuiPass");

        ImGui::Render();
        m_GuiBackend->Render(*ctx.m_CommandList, *m_FrameBuffer);
    }

    void GuiPass::Cleanup(RHI::IRHIDevice& /*device*/)
    {
        m_GuiBackend->DestroyTextureId(m_SceneViewId);
        m_SceneViewId = nullptr;

        m_FrameBuffer.reset();
        m_GuiBackend.reset();   // calls ImGui_ImplVulkan_Shutdown() before DestroyContext

        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void* GuiPass::GetSceneViewTextureId() const
    {
        return m_SceneViewId;
    }
}
