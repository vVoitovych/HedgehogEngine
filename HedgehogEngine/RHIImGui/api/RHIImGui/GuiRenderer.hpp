#pragma once

#include "RHI/api/RHITypes.hpp"

#include <cstdint>
#include <memory>

namespace RHI
{
    class IRHIDevice;
    class IRHICommandList;
    class IRHITexture;
}

// ImGui's GPU renderer on top of the RHI. It lives outside RHI so that RHI links no UI library, and
// outside HedgehogRenderer so that the renderer never knows the UI: only the application (the
// Editor) uses it, recording into the colour target the result view's Ui pass hands it.
namespace RHIImGui
{
    // ColorFormat is the format of every texture Render() draws into: the renderer's pipeline is
    // built for it, so a target of another format needs another GuiRenderer.
    struct GuiRendererDesc
    {
        uint32_t    MinImageCount = 2;
        uint32_t    ImageCount    = 2;
        RHI::Format ColorFormat   = RHI::Format::Undefined;
    };

    class IGuiRenderer
    {
    public:
        virtual ~IGuiRenderer() = default;

        IGuiRenderer(const IGuiRenderer&)            = delete;
        IGuiRenderer& operator=(const IGuiRenderer&) = delete;
        IGuiRenderer(IGuiRenderer&&)                 = delete;
        IGuiRenderer& operator=(IGuiRenderer&&)      = delete;

        virtual void NewFrame() = 0;

        // Records the current ImGui draw data into target with dynamic rendering, clearing it to
        // opaque black first. target is already a colour attachment (RenderTarget).
        virtual void Render(RHI::IRHICommandList& cmd, RHI::IRHITexture& target) = 0;

        // Registers a texture, read as a shader resource, for use as an ImTextureID. The caller
        // owns the id and releases it with DestroyTextureId once no frame in flight uses it.
        [[nodiscard]] virtual void* CreateTextureId(const RHI::IRHITexture& texture) = 0;

        // Releases an id from CreateTextureId. Safe to call with nullptr.
        virtual void DestroyTextureId(void* id) = 0;

    protected:
        IGuiRenderer() = default;
    };

    // Create after ImGui::CreateContext() and the platform backend's init. The device must outlive
    // the renderer.
    [[nodiscard]] std::unique_ptr<IGuiRenderer> CreateGuiRenderer(RHI::IRHIDevice& device, const GuiRendererDesc& desc);
}
