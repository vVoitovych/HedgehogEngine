#pragma once

#include "HedgehogRenderer/Graph/UiCallback.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace HW
{
    class Window;
}

namespace RHI
{
    class IRHIGuiBackend;
    class IRHITexture;
}

namespace Renderer
{
    class Renderer;
}

namespace Editor
{
    // The editor's ImGui (RENDERING.md section 7). The editor, never the renderer, owns the ImGui
    // context, the GLFW platform backend and the GUI renderer; the renderer only hands a colour
    // target to GetUiCallback() and never includes an ImGui header.
    //
    // The GUI renderer is built for one colour format, and the two frame paths record their UI into
    // targets of different formats (the legacy colour buffer, the swapchain), so it is rebuilt when
    // the path changes. Texture ids for images shown in the UI are reissued when their texture is
    // replaced, and a replaced id is released only once no frame in flight can still use it.
    class ImGuiLayer
    {
    public:
        explicit ImGuiLayer(HW::Window& window);
        ~ImGuiLayer();

        ImGuiLayer(const ImGuiLayer&)            = delete;
        ImGuiLayer& operator=(const ImGuiLayer&) = delete;
        ImGuiLayer(ImGuiLayer&&)                 = delete;
        ImGuiLayer& operator=(ImGuiLayer&&)      = delete;

        // Starts an ImGui frame, for the path that will render it.
        void BeginFrame(Renderer::Renderer& renderer, bool forRenderGraph);
        // Ends it and builds the draw data the UiCallback records.
        void EndFrame();

        // An ImTextureID for texture, shown under slot ("scene", "game"). Stable while the texture is;
        // nullptr for no texture.
        void* GetTextureId(const std::string& slot, const RHI::IRHITexture* texture);

        Renderer::UiCallback GetUiCallback() { return { &Record, this }; }

        // Releases the GUI renderer and the context. Call before the renderer is cleaned up.
        void Shutdown(Renderer::Renderer& renderer);

    private:
        struct ShownTexture
        {
            const RHI::IRHITexture* Texture = nullptr;
            uint32_t                Width   = 0; // with the pointer, so a new texture at a reused
            uint32_t                Height  = 0; // address still gets a new id
            void*                   Id      = nullptr;
        };

        struct RetiredId
        {
            void*    Id    = nullptr;
            uint64_t Frame = 0;
        };

        static void Record(void* user, RHI::IRHICommandList& cmd, RHI::IRHITexture& target);
        void        ReleaseTextureIds(bool onlyExpired);

        std::unique_ptr<RHI::IRHIGuiBackend>          m_Backend;
        bool                                          m_BackendForRenderGraph = false;
        std::unordered_map<std::string, ShownTexture> m_Shown;
        std::vector<RetiredId>                        m_Retired;
        uint64_t                                      m_Frame = 0;
    };
}
