#pragma once

#include "GraphPasses/GraphPassServices.hpp"

#include "HedgehogRenderer/Frame/SharedPhase.hpp"
#include "HedgehogRenderer/Graph/GraphAssetLibrary.hpp"
#include "HedgehogRenderer/Graph/GraphInstantiator.hpp"
#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"
#include "HedgehogRenderer/Targets/RenderTargetRegistry.hpp"
#include "HedgehogRenderer/Views/ViewManager.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace RHI
{
    class IRHICommandList;
    class IRHIFence;
    class IRHISemaphore;
}

namespace HR
{
    class ResourceRegistry;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace Renderer
{
    // The command list and synchronization objects of the frame slot being recorded, owned by
    // the renderer's ThreadContext and shared with the legacy path.
    struct FrameSync
    {
        RHI::IRHICommandList& Cmd;
        RHI::IRHIFence&       Fence;
        RHI::IRHISemaphore&   ImageAvailable;
        RHI::IRHISemaphore&   RenderFinished;
        uint32_t              FrameIndex = 0;
    };

    // The render-graph frame (RENDERING.md section 3.1), behind Renderer::RenderFrame. It owns
    // everything the new path adds: the pass services, the shipped graph assets, the render-target
    // registry and the views, the shared phase, and one RenderGraphRuntime per frame in flight, so
    // a pooled transient is never reused while an earlier frame may still be reading it.
    //
    // One frame: acquire, build and order the views, declare the shared phase and then each view's
    // graph into this slot's runtime, execute it as one recorded stream, blit the viewport target
    // to the swapchain, submit and present. The fence wait and swapchain resize happen before, in
    // Renderer::RenderFrame, because the legacy resources resize with them.
    //
    // No graph writes the swapchain's format yet, so "main" is presented through the viewport: a
    // view targeting main (a camera, as in a game build) renders into the viewport instead. Only
    // one view may present. When several target main or the viewport, the highest priority one
    // renders and the others are skipped with a warning (RENDERING.md section 8).
    class FrameRenderer
    {
    public:
        // The target the editor's view renders into and the frame presents, until a UI pass
        // composites the editor's panels into "main".
        static constexpr const char* VIEWPORT_TARGET = "viewport";

        FrameRenderer(RHI::IRHIDevice& device, RHI::IRHISwapchain& swapchain, const FS::FileSystemManager& fileSystem);
        ~FrameRenderer();

        FrameRenderer(const FrameRenderer&)            = delete;
        FrameRenderer& operator=(const FrameRenderer&) = delete;
        FrameRenderer(FrameRenderer&&)                 = delete;
        FrameRenderer& operator=(FrameRenderer&&)      = delete;

        ViewManager& GetViewManager() { return m_Views; }

        // For a renderer built without the legacy path, whose ForwardPass would do this.
        void ProvideMaterialLayout(HR::ResourceRegistry& registry)
        {
            m_Services.ProvideMaterialLayout(m_Device, registry);
        }

        // The swapchain was recreated; "main" and swapchain-relative targets follow at this
        // frame's end.
        void NotifySwapchainResized() { m_Targets.NotifySwapchainResized(); }

        // Everything after the fence wait: acquire, build, execute, submit and present.
        void Render(const HX::RenderScene& scene, const HR::ResourceRegistry& resources,
                    const HedgehogSettings::Settings& settings, const FrameSync& sync);

    private:
        void           FillSceneFrame(const HX::RenderScene& scene, const HR::ResourceRegistry& resources,
                                      const HedgehogSettings::Settings& settings);
        GraphFrameData MakeViewFrame(const View& view) const;
        bool           DeclareView(RenderGraphRuntime& graph, const View& view, GraphFrameData& frame,
                                   GraphFrameContext& context, const SharedPhaseOutputs& shared);
        void           Present(const FrameSync& sync, uint32_t imageIndex, bool hasViewport);
        static bool    Presents(const View& view);
        void           ReportOnce(const std::string& message);

        RHI::IRHIDevice&    m_Device;
        RHI::IRHISwapchain& m_Swapchain;

        GraphPassServices    m_Services;
        PassBuilderRegistry  m_Registry;
        GraphAssetLibrary    m_Library;
        GraphInstantiator    m_Instantiator;
        RenderTargetRegistry m_Targets;
        ViewManager          m_Views;
        SharedPhase          m_Shared;

        std::array<std::unique_ptr<RenderGraphRuntime>, HedgehogEngine::MAX_FRAMES_IN_FLIGHT> m_Runtimes;

        // Rebuilt every frame, reused so steady-state frames allocate little.
        GraphFrameData                           m_SceneFrame; // what every view shares
        std::vector<MeshDrawRange>               m_Meshes;
        std::vector<const RHI::IRHIDescriptorSet*> m_MaterialSets;
        std::vector<GraphFrameData>              m_ViewFrames;
        std::vector<GraphFrameContext>           m_ViewContexts;
        std::vector<RGTexture>                   m_OutputTargets;
        std::vector<GraphOutputRequirement>      m_OutputContract;

        uint64_t    m_FrameNumber = 0;
        std::string m_LastReport;
    };
}
