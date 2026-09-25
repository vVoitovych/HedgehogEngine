#pragma once

#include "RHITypes.hpp"

namespace RHI
{
    class IRHICommandList;
    class IRHITexture;

    // ColorFormat is the format of every texture Render() draws into: the backend's pipeline is
    // built for it, so a target of another format needs another backend.
    struct GuiBackendDesc
    {
        uint32_t MinImageCount;
        uint32_t ImageCount;
        Format   ColorFormat;
    };

    class IRHIGuiBackend
    {
    public:
        virtual ~IRHIGuiBackend() = default;

        IRHIGuiBackend(const IRHIGuiBackend&)            = delete;
        IRHIGuiBackend& operator=(const IRHIGuiBackend&) = delete;
        IRHIGuiBackend(IRHIGuiBackend&&)                 = delete;
        IRHIGuiBackend& operator=(IRHIGuiBackend&&)      = delete;

        virtual void NewFrame() = 0;

        // Records the current ImGui draw data into target with dynamic rendering, clearing it to
        // opaque black first. The caller transitions target to ColorAttachment beforehand.
        virtual void Render(IRHICommandList& cmd, IRHITexture& target) = 0;

        // Registers a texture for use as ImTextureID. Caller owns the returned id.
        virtual void* CreateTextureId(const IRHITexture& texture) = 0;

        // Releases a previously registered texture id. Safe to call with nullptr.
        virtual void  DestroyTextureId(void* id) = 0;

    protected:
        IRHIGuiBackend() = default;
    };

} // namespace RHI
