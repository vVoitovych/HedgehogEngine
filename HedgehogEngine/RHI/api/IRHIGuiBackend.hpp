#pragma once

#include "RHITypes.hpp"

namespace RHI
{
    class IRHICommandList;
    class IRHIFramebuffer;
    class IRHIRenderPass;
    class IRHITexture;

    struct GuiBackendDesc
    {
        uint32_t m_MinImageCount;
        uint32_t m_ImageCount;
        Format   m_ColorFormat;
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

        // Executes a BeginRenderPass/RenderDrawData/EndRenderPass sequence.
        virtual void Render(IRHICommandList& cmd, IRHIFramebuffer& framebuffer) = 0;

        // Returns the render pass used internally so callers can create compatible framebuffers.
        virtual IRHIRenderPass& GetRenderPass() = 0;

        // Registers a texture for use as ImTextureID. Caller owns the returned id.
        virtual void* CreateTextureId(const IRHITexture& texture) = 0;

        // Releases a previously registered texture id. Safe to call with nullptr.
        virtual void  DestroyTextureId(void* id) = 0;

    protected:
        IRHIGuiBackend() = default;
    };

} // namespace RHI
