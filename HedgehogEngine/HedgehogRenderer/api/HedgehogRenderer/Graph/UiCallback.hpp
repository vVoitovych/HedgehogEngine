#pragma once

namespace RHI
{
    class IRHICommandList;
    class IRHITexture;
}

namespace Renderer
{
    // The application's UI (RENDERING.md section 7): the renderer never draws it itself and never
    // includes a UI library. It hands the application a colour target already transitioned to
    // ColorAttachment, and the application records into it. A plain function pointer and context,
    // not std::function, so render-graph pass data can hold it.
    struct UiCallback
    {
        void (*Record)(void* user, RHI::IRHICommandList& cmd, RHI::IRHITexture& target) = nullptr;
        void* User = nullptr;

        explicit operator bool() const { return Record != nullptr; }
        void operator()(RHI::IRHICommandList& cmd, RHI::IRHITexture& target) const { Record(User, cmd, target); }
    };
}
