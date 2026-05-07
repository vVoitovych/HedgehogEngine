#pragma once

#include <cstdint>

namespace HedgehogEngine
{
    struct FrameData;
}

namespace RHI
{
    class IRHIDevice;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace Renderer
{
    class ResourceManager;
    struct RenderContext;

    class IRenderNode
    {
    public:
        virtual ~IRenderNode() = default;

        // Called once per frame to execute GPU work.
        virtual void Render(RenderContext& ctx) = 0;

        // Called once per frame before Render() for CPU-side state updates.
        // Default: no-op. Override in nodes that maintain per-frame computed state
        // (e.g. ShadowmapNode cascade matrices).
        virtual void PreRender(const HedgehogEngine::FrameData& frame,
                                     uint32_t frameIndex,
                                     const HedgehogSettings::Settings& settings) {}

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
