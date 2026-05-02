#pragma once

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
        // Default: no-op. Override in passes that maintain per-frame computed state
        // (e.g. ShadowmapPass cascade matrices).
        virtual void UpdateData(const HedgehogEngine::FrameData& frame,
                                uint32_t frameIndex,
                                const HedgehogSettings::Settings& settings) {}

        // Lifecycle and resize callbacks — default no-op.
        virtual void Cleanup(RHI::IRHIDevice& device) {}
        virtual void OnResizeFramebuffer(RHI::IRHIDevice& device, const ResourceManager& rm) {}
        virtual void OnResizeSceneView(RHI::IRHIDevice& device, const ResourceManager& rm) {}
        virtual void OnUpdateResources(RHI::IRHIDevice& device,
                                       const HedgehogSettings::Settings& settings,
                                       const ResourceManager& rm) {}

    protected:
        IRenderNode() = default;

        IRenderNode(const IRenderNode&)            = delete;
        IRenderNode& operator=(const IRenderNode&) = delete;
        IRenderNode(IRenderNode&&)                 = delete;
        IRenderNode& operator=(IRenderNode&&)      = delete;
    };

} // namespace Renderer
