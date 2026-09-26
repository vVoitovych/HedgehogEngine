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

namespace Renderer
{
    class ResourceManager;
    class DepthPrePass;
    class ShadowmapPass;
    class ForwardPass;

    // The legacy geometry passes. Nothing records them any more: the frame-level work around them
    // (acquire, UI, submit, present) is gone, and they are deleted next.
    class RenderQueue
    {
    public:
        RenderQueue(RHI::IRHIDevice&                  device,
                    const HedgehogSettings::Settings& settings,
                    ResourceManager&                  resourceManager,
                    const FS::FileSystemManager&      fileSystem);
        ~RenderQueue();

        RenderQueue(const RenderQueue&)            = delete;
        RenderQueue& operator=(const RenderQueue&) = delete;
        RenderQueue(RenderQueue&&)                 = delete;
        RenderQueue& operator=(RenderQueue&&)      = delete;

        void Cleanup(RHI::IRHIDevice& device);

        // Records the shadow map, depth prepass and forward passes into cmd.
        void Render(const HedgehogEngine::FrameData& frame,
                    RHI::IRHICommandList&            cmd,
                    uint32_t                         frameIndex,
                    const ResourceManager&           resourceManager);

        void UpdateData(const HedgehogEngine::FrameData&             frame,
                        uint32_t                          frameIndex,
                        const HedgehogSettings::Settings& settings);

        void ResizeSceneView(RHI::IRHIDevice& device, const ResourceManager& resourceManager);

        void UpdateResources(RHI::IRHIDevice&                 device,
                             const HedgehogSettings::Settings& settings,
                             const ResourceManager&            resourceManager);

        FrameStats& GetFrameStats() { return m_FrameStats; }

    private:
        FrameStats m_FrameStats;

        std::unique_ptr<DepthPrePass>  m_DepthPrePass;
        std::unique_ptr<ShadowmapPass> m_ShadowmapPass;
        std::unique_ptr<ForwardPass>   m_ForwardPass;
    };
}
