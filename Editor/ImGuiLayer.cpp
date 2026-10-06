#include "ImGuiLayer.hpp"

#include "EditorTheme.hpp"

#include "HedgehogRenderer/Renderer.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "Logger/api/Logger.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHIImGui/GuiRenderer.hpp"
#include "RHI/api/IRHITexture.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace Editor
{
    namespace
    {
        constexpr const char* UI_FONT_PATH = "engine://Content/Fonts/Karla-Regular.ttf";
    }

    ImGuiLayer::ImGuiLayer(HW::Window& window, const std::filesystem::path& iniPath)
        : m_IniPath(iniPath.string())
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = m_IniPath.c_str();
        Theme::Apply(ImGui::GetStyle());
        ImGui_ImplGlfw_InitForVulkan(window.GetNativeHandle(), true);
    }

    ImGuiLayer::~ImGuiLayer() = default;

    void ImGuiLayer::CreateBackend(const Renderer::RendererDevice& device)
    {
        RHIImGui::GuiRendererDesc desc;
        desc.MinImageCount = HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        desc.ImageCount    = HedgehogEngine::MAX_FRAMES_IN_FLIGHT;
        desc.ColorFormat   = device.PresentFormat;
        m_Device  = &device.Device;
        m_Backend = RHIImGui::CreateGuiRenderer(device.Device, desc);

        if (device.PresentFormat == RHI::Format::R8G8B8A8Srgb || device.PresentFormat == RHI::Format::B8G8R8A8Srgb)
            Theme::ConvertToLinear(ImGui::GetStyle());
    }

    void ImGuiLayer::LoadFonts(const FS::FileSystemManager& fileSystem)
    {
        ImFontAtlas& atlas = *ImGui::GetIO().Fonts;

        // The first font added is the default one.
        const std::optional<std::vector<std::byte>> data = fileSystem.ReadFile(UI_FONT_PATH);
        if (data && !data->empty())
        {
            // The atlas takes ownership and frees the data with ImGui's allocator.
            void* owned = ImGui::MemAlloc(data->size());
            std::memcpy(owned, data->data(), data->size());
            atlas.AddFontFromMemoryTTF(owned, static_cast<int>(data->size()), Theme::FONT_SIZE);
        }
        else
        {
            LOGWARNING("[Editor] ", UI_FONT_PATH, " could not be read; the UI uses ImGui's default font.");
        }

        m_MonoFont = atlas.AddFontDefaultBitmap();
    }

    void ImGuiLayer::BeginFrame()
    {
        assert(m_Backend && "ImGuiLayer: pass CreateBackend to the Renderer as its DeviceReadyCallback.");

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

    void ImGuiLayer::Shutdown()
    {
        if (m_Device)
            m_Device->WaitIdle();
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
