#pragma once

#include "Profiling/FrameStats.hpp"

#include <cstdint>
#include <memory>

namespace HedgehogEngine
{
    struct FrameData;
}

namespace RHI
{
    class IRHIDevice;
    class IRHISwapchain;
    class IRHICommandList;
    class IRHIFence;
    class IRHISemaphore;
}

namespace HW
{
    class Window;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace FS
{
    class FileSystemManager;
}

namespace HR
{
    class ResourceRegistry;
}

namespace Renderer
{
    class RenderGraph;
    class InitPass;
    class DepthPrePass;
    class ShadowmapPass;
    class ForwardPass;
    class PresentPass;
    class GuiPass;

    class RenderQueue
    {
    public:
        RenderQueue(RHI::IRHIDevice&                  device,
                    HW::Window&                       window,
                    const HedgehogSettings::Settings& settings,
                    RenderGraph&                      graph,
                    HR::ResourceRegistry&             resourceRegistry,
                    const FS::FileSystemManager&      fileSystem);
        ~RenderQueue();

        RenderQueue(const RenderQueue&)            = delete;
        RenderQueue& operator=(const RenderQueue&) = delete;
        RenderQueue(RenderQueue&&)                 = delete;
        RenderQueue& operator=(RenderQueue&&)      = delete;

        void Cleanup(RHI::IRHIDevice& device);

        void  BeginGui();
        void  DiscardGui();
        void* GetSceneViewTextureId() const;

        // frame/frameIndex/settings feed graph.Update(ctx); the rest feed graph.Execute(ctx).
        void Render(const HedgehogEngine::FrameData& frame,
                    RHI::IRHIDevice&     device,
                    RHI::IRHISwapchain&  swapchain,
                    RHI::IRHICommandList& cmd,
                    RHI::IRHIFence&      fence,
                    RHI::IRHISemaphore&  imageAvailableSemaphore,
                    RHI::IRHISemaphore&  renderFinishedSemaphore,
                    uint32_t             frameIndex,
                    const HedgehogSettings::Settings& settings,
                    HR::ResourceRegistry& resourceRegistry);

        FrameStats& GetFrameStats() { return m_FrameStats; }

    private:
        FrameStats m_FrameStats;

        RenderGraph* m_Graph = nullptr; // non-owning; owned by Renderer

        std::unique_ptr<InitPass>     m_InitPass;
        std::unique_ptr<DepthPrePass> m_DepthPrePass;
        std::unique_ptr<ShadowmapPass> m_ShadowmapPass;
        std::unique_ptr<ForwardPass>  m_ForwardPass;
        std::unique_ptr<PresentPass>  m_PresentPass;
        std::unique_ptr<GuiPass>      m_GuiPass;
    };
}
