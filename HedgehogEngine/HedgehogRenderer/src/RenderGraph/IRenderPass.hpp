#pragma once

namespace RHI
{
    class IRHIDevice;
}

namespace Renderer
{
    class RenderGraph;
    class RenderGraphBuilder;
    struct RenderGraphContext;

    // A pass never touches transient-texture lifetime directly: it declares reads/writes
    // in Setup() and receives ready-to-use textures/framebuffers via CreateFramebuffers(),
    // called once at graph compile and again whenever a size class it depends on is invalidated.
    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;

        // Stable literal zone name (e.g. "ForwardPass") — the executor uses it for
        // HH_PROFILE_ZONE/FrameStats sampling so per-pass timings keep the same identity they
        // had when RenderQueue recorded them by hand around each bespoke Render() call.
        virtual const char* GetName() const = 0;

        virtual void Setup(RenderGraphBuilder& builder) = 0;
        virtual void CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph) = 0;
        virtual void Update(const RenderGraphContext& ctx) { (void)ctx; }
        virtual void Execute(RenderGraphContext& ctx) = 0;
        virtual void Cleanup(RHI::IRHIDevice& device) = 0;
    };
}
