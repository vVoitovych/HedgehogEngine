#pragma once

#include <string>

namespace RHI { class IRHIDevice; }

namespace Renderer
{
    struct PreRenderContext;
    struct RenderContext;

    class IRenderNode
    {
    public:
        virtual ~IRenderNode() = default;

        // Optional per-frame CPU lifecycle hooks — called before command recording.
        virtual void OnBeginFrame()   {}
        virtual void OnDiscardFrame() {}

        // Called every frame before command recording begins.
        // Responsible for resource creation, framebuffer rebuilds, descriptor writes.
        // GPU is idle for the current frame slot when this runs (fence waited by Renderer).
        virtual void PreRender(const PreRenderContext& ctx) {}

        // Called once per frame to execute GPU work — pure command recording only.
        virtual void Render(RenderContext& ctx) = 0;

        // Optional opaque resource export — e.g. a descriptor set handle.
        virtual void* ExportResource(const std::string& key) const { return nullptr; }

        // Called on shutdown or queue reload.
        virtual void Cleanup(RHI::IRHIDevice& device) {}

    protected:
        IRenderNode() = default;

        IRenderNode(const IRenderNode&)            = delete;
        IRenderNode& operator=(const IRenderNode&) = delete;
        IRenderNode(IRenderNode&&)                 = delete;
        IRenderNode& operator=(IRenderNode&&)      = delete;
    };

} // namespace Renderer
