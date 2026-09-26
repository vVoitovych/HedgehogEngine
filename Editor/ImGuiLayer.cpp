#include "ImGuiLayer.hpp"

#include "HedgehogRenderer/Renderer.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "RHI/api/IRHIGuiBackend.hpp"
#include "RHI/api/IRHITexture.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"

#include <algorithm>

namespace Editor
{
    ImGuiLayer::ImGuiLayer(HW::Window& window)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForVulkan(window.GetNativeHandle(), true);
    }

    ImGuiLayer::~ImGuiLayer() = default;

    void ImGuiLayer::BeginFrame(Renderer::Renderer& renderer)
    {
        if (!m_Backend)
            m_Backend = renderer.CreateGuiBackend(true);

        ++m_Frame;
        ReleaseTextureIds(true);

        m_Backend->NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::EndFrame()
    {
        ImGui::Render();
    }

    void* ImGuiLayer::GetTextureId(const std::string& slot, const RHI::IRHITexture* texture)
    {
        ShownTexture& shown = m_Shown[slot];
        const uint32_t width  = texture ? texture->GetWidth() : 0;
        const uint32_t height = texture ? texture->GetHeight() : 0;
        if (shown.Texture == texture && shown.Width == width && shown.Height == height)
            return shown.Id;

        if (shown.Id)
            m_Retired.push_back({ shown.Id, m_Frame });
        shown = { texture, width, height, texture ? m_Backend->CreateTextureId(*texture) : nullptr };
        return shown.Id;
    }

    void ImGuiLayer::Shutdown(Renderer::Renderer& renderer)
    {
        renderer.WaitIdle();
        ReleaseTextureIds(false);
        m_Backend.reset();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiLayer::Record(void* user, RHI::IRHICommandList& cmd, RHI::IRHITexture& target)
    {
        static_cast<ImGuiLayer*>(user)->m_Backend->Render(cmd, target);
    }

    // onlyExpired: release only ids retired long enough ago that no frame in flight can use them.
    void ImGuiLayer::ReleaseTextureIds(bool onlyExpired)
    {
        const auto expired = [&](const RetiredId& retired)
        {
            return !onlyExpired || m_Frame - retired.Frame > HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        };
        for (const RetiredId& retired : m_Retired)
        {
            if (expired(retired))
                m_Backend->DestroyTextureId(retired.Id);
        }
        m_Retired.erase(std::remove_if(m_Retired.begin(), m_Retired.end(), expired), m_Retired.end());

        if (!onlyExpired)
        {
            for (auto& [slot, shown] : m_Shown)
                m_Backend->DestroyTextureId(shown.Id);
            m_Shown.clear();
        }
    }
}
