#include "GuiPass.hpp"

#include "RenderGraph/RenderGraph.hpp"
#include "RenderGraph/RenderGraphBuilder.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "Profiling/Profiler.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIFramebuffer.hpp"
#include "RHI/api/IRHIGuiBackend.hpp"
#include "RHI/api/IRHITexture.hpp"
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
        backendDesc.MinImageCount = HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        backendDesc.ImageCount    = HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        // Renderer-wide colour convention (see PassInitContext.hpp) — matches the guiColor graph
        // texture's format declared in Setup(), since the backend is built before Setup() runs.
        backendDesc.ColorFormat = RHI::Format::R16G16B16A16Unorm;
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
        for (const auto& source : m_ViewSources)
            builder.ImportReadSampled(source);

        GraphTextureDesc desc;
        desc.TextureSizeClass = SizeClass::SwapchainRelative;
        desc.Format           = RHI::Format::R16G16B16A16Unorm;
        desc.Usage            = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::TransferSrc;
        builder.CreateTexture(GraphResourceNames::GUI_COLOR, desc);
        builder.Write(GraphResourceNames::GUI_COLOR, RHI::ImageLayout::ColorAttachment);
    }

    void GuiPass::CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph)
    {
        auto& colorBuffer = graph.GetTexture(GraphResourceNames::GUI_COLOR);

        m_FrameBuffer.reset();

        RHI::FramebufferDesc fbDesc;
        fbDesc.RenderPass       = &m_GuiBackend->GetRenderPass();
        fbDesc.ColorAttachments = { &colorBuffer };
        fbDesc.Width            = colorBuffer.GetWidth();
        fbDesc.Height           = colorBuffer.GetHeight();
        m_FrameBuffer = device.CreateFramebuffer(fbDesc);

        // Rebuild every view's ImGui texture id. Simpler than tracking exactly which import
        // changed generation (this only runs on a resize, which is already a WaitIdle event).
        for (auto& [name, id] : m_ViewTextureIds)
            m_GuiBackend->DestroyTextureId(id);
        m_ViewTextureIds.clear();

        for (const auto& source : m_ViewSources)
            m_ViewTextureIds[source] = m_GuiBackend->CreateTextureId(graph.GetTexture(source));
    }

    void GuiPass::Execute(RenderGraphContext& ctx)
    {
        HH_PROFILE_ZONE("GuiPass");

        ImGui::Render();
        m_GuiBackend->Render(*ctx.CommandList, *m_FrameBuffer);
    }

    void GuiPass::Cleanup(RHI::IRHIDevice& /*device*/)
    {
        for (auto& [name, id] : m_ViewTextureIds)
            m_GuiBackend->DestroyTextureId(id);
        m_ViewTextureIds.clear();

        m_FrameBuffer.reset();
        m_GuiBackend.reset();   // calls ImGui_ImplVulkan_Shutdown() before DestroyContext

        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void* GuiPass::GetViewTextureId(const std::string& viewOutputName) const
    {
        const auto it = m_ViewTextureIds.find(viewOutputName);
        return it != m_ViewTextureIds.end() ? it->second : nullptr;
    }
}
