#include "RenderQueue.hpp"

#include "Profiling/Profiler.hpp"
#include "ResourceManager/ResourceManager.hpp"
#include "RenderPasses/InitPass/InitPass.hpp"
#include "RenderPasses/DepthPrepass/DepthPrePass.hpp"
#include "RenderPasses/ShadowmapPass/ShadowmapPass.hpp"
#include "RenderPasses/ForwardPass/ForwardPass.hpp"
#include "RenderPasses/PresentPass/PresentPass.hpp"
#include "RenderPasses/GuiPass/GuiPass.hpp"

#include "HedgehogSettings/api/HedgehogSettings.hpp"

#include "HedgehogCommon/api/Frame/FrameData.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    RenderQueue::RenderQueue(RHI::IRHIDevice&                  device,
                             HW::Window&                       window,
                             const HedgehogSettings::Settings& settings,
                             ResourceManager&                  resourceManager,
                             const FS::FileSystemManager&      fileSystem)
    {
        m_InitPass      = std::make_unique<InitPass>();
        m_DepthPrePass  = std::make_unique<DepthPrePass>(device, resourceManager, fileSystem);
        m_ShadowmapPass = std::make_unique<ShadowmapPass>(device, settings, resourceManager, fileSystem);
        m_ForwardPass   = std::make_unique<ForwardPass>(device, resourceManager, fileSystem);
        m_GuiPass       = std::make_unique<GuiPass>(window, device, resourceManager);
        m_PresentPass   = std::make_unique<PresentPass>();
    }

    RenderQueue::~RenderQueue()
    {
    }

    void RenderQueue::Cleanup(RHI::IRHIDevice& device)
    {
        m_InitPass->Cleanup();
        m_DepthPrePass->Cleanup(device);
        m_ShadowmapPass->Cleanup(device);
        m_ForwardPass->Cleanup(device);
        m_GuiPass->Cleanup(device);
        m_PresentPass->Cleanup();
    }

    void RenderQueue::BeginGui()
    {
        m_GuiPass->BeginFrame();
    }

    void RenderQueue::DiscardGui()
    {
        m_GuiPass->DiscardFrame();
    }

    void RenderQueue::Render(const HedgehogEngine::FrameData& frame,
                             RHI::IRHIDevice&     device,
                             RHI::IRHISwapchain&  swapchain,
                             RHI::IRHICommandList& cmd,
                             RHI::IRHIFence&      fence,
                             RHI::IRHISemaphore&  imageAvailableSemaphore,
                             RHI::IRHISemaphore&  renderFinishedSemaphore,
                             uint32_t             frameIndex,
                             const ResourceManager& resourceManager)
    {
        uint32_t backBufferIndex = 0;
        {
            HH_PROFILE_ZONE("InitPass");
            ScopedCpuSample sample(m_FrameStats, "InitPass");
            backBufferIndex = m_InitPass->Render(
                swapchain, fence, imageAvailableSemaphore, cmd);
        }

        {
            HH_PROFILE_ZONE("ShadowmapPass");
            ScopedCpuSample sample(m_FrameStats, "ShadowmapPass");
            m_ShadowmapPass->Render(frame, resourceManager, cmd, frameIndex);
        }

        {
            HH_PROFILE_ZONE("DepthPrePass");
            ScopedCpuSample sample(m_FrameStats, "DepthPrePass");
            m_DepthPrePass->Render(frame, resourceManager, cmd, frameIndex);
        }

        {
            HH_PROFILE_ZONE("ForwardPass");
            ScopedCpuSample sample(m_FrameStats, "ForwardPass");
            m_ForwardPass->Render(frame, resourceManager, cmd, frameIndex);
        }

        {
            HH_PROFILE_ZONE("GuiPass");
            ScopedCpuSample sample(m_FrameStats, "GuiPass");

            auto& sceneBuffer = const_cast<RHI::IRHITexture&>(resourceManager.GetSceneColorBuffer());
            cmd.TransitionTexture(sceneBuffer,
                RHI::ImageLayout::ColorAttachment,
                RHI::ImageLayout::ShaderReadOnly);

            m_GuiPass->Render(cmd, resourceManager);
        }

        {
            HH_PROFILE_ZONE("PresentPass");
            ScopedCpuSample sample(m_FrameStats, "PresentPass");

            auto& colorBuffer = const_cast<RHI::IRHITexture&>(resourceManager.GetRHIColorBuffer());
            m_PresentPass->Render(cmd, device, swapchain, colorBuffer, backBufferIndex,
                                  imageAvailableSemaphore, renderFinishedSemaphore, fence);
        }
    }

    void RenderQueue::UpdateData(const HedgehogEngine::FrameData&             frame,
                                 uint32_t                          frameIndex,
                                 const HedgehogSettings::Settings& settings)
    {
        m_ShadowmapPass->UpdateData(frame, frameIndex, settings);
    }

    void RenderQueue::ResizeResources(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        // Window resize: only GuiPass framebuffer depends on swapchain/RHIColorBuffer size.
        // DepthPrePass and ForwardPass are resized by ResizeSceneView when the panel size changes.
        m_GuiPass->ResizeResources(device, resourceManager);
    }

    void RenderQueue::ResizeSceneView(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        m_DepthPrePass->ResizeResources(device, resourceManager);
        m_ForwardPass->ResizeResources(device, resourceManager);
        m_GuiPass->RecreateSceneDescriptor(resourceManager);
    }

    void* RenderQueue::GetSceneViewTextureId() const
    {
        return m_GuiPass->GetSceneViewTextureId();
    }

    void RenderQueue::UpdateResources(RHI::IRHIDevice&                  device,
                                      const HedgehogSettings::Settings&  settings,
                                      const ResourceManager&             resourceManager)
    {
        m_ShadowmapPass->UpdateResources(device, settings, resourceManager);
    }
}
