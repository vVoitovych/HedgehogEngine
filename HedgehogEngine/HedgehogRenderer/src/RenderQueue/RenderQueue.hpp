#pragma once

#include "Profiling/FrameStats.hpp"

#include "HedgehogMath/api/Matrix.hpp"

#include <cstdint>
#include <memory>
#include <optional>

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
    class InitPass;
    class DepthPrePass;
    class ShadowmapPass;
    class ForwardPass;
    class GizmoPass;
    class PresentPass;
    class GuiPass;

    class RenderQueue
    {
    public:
        RenderQueue(RHI::IRHIDevice&                  device,
                    HW::Window&                       window,
                    const HedgehogSettings::Settings& settings,
                    ResourceManager&                  resourceManager,
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
        void* GetGameViewTextureId() const;

        void Render(const HedgehogEngine::FrameData& frame,
                    RHI::IRHIDevice&     device,
                    RHI::IRHISwapchain&  swapchain,
                    RHI::IRHICommandList& cmd,
                    RHI::IRHIFence&      fence,
                    RHI::IRHISemaphore&  imageAvailableSemaphore,
                    RHI::IRHISemaphore&  renderFinishedSemaphore,
                    uint32_t             frameIndex,
                    bool                 gameViewVisible,
                    const std::optional<HM::Matrix4x4>& selectedGizmo,
                    const ResourceManager& resourceManager);

        void UpdateData(const HedgehogEngine::FrameData&             frame,
                        uint32_t                          frameIndex,
                        const HedgehogSettings::Settings& settings);

        void ResizeResources(RHI::IRHIDevice& device, const ResourceManager& resourceManager);
        void ResizeSceneView(RHI::IRHIDevice& device, const ResourceManager& resourceManager);
        void ResizeGameView(RHI::IRHIDevice& device, const ResourceManager& resourceManager);

        void UpdateResources(RHI::IRHIDevice&                 device,
                             const HedgehogSettings::Settings& settings,
                             const ResourceManager&            resourceManager);

        FrameStats& GetFrameStats() { return m_FrameStats; }

    private:
        FrameStats m_FrameStats;

        std::unique_ptr<InitPass>     m_InitPass;
        std::unique_ptr<DepthPrePass> m_DepthPrePass;
        std::unique_ptr<ShadowmapPass> m_ShadowmapPass;
        std::unique_ptr<ForwardPass>  m_ForwardPass;
        std::unique_ptr<GizmoPass>    m_GizmoPass;
        std::unique_ptr<PresentPass>  m_PresentPass;
        std::unique_ptr<GuiPass>      m_GuiPass;
    };
}
