#include "RenderQueue.hpp"

#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"
#include "HedgehogRenderer/RenderPasses/InitPass/InitPass.hpp"
#include "HedgehogRenderer/RenderPasses/DepthPrepass/DepthPrePass.hpp"
#include "HedgehogRenderer/RenderPasses/ShadowmapPass/ShadowmapPass.hpp"
#include "HedgehogRenderer/RenderPasses/ForwardPass/ForwardPass.hpp"
#include "HedgehogRenderer/RenderPasses/PresentPass/PresentPass.hpp"
#include "HedgehogRenderer/RenderPasses/GuiPass/GuiPass.hpp"

#include "HedgehogSettings/Settings/HedgehogSettings.hpp"

#include "HedgehogEngine/api/Frame/FrameData.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"
#include "RHI/api/IRHITexture.hpp"

namespace Renderer
{
    RenderQueue::RenderQueue(RHI::IRHIDevice&                 device,
                             HW::Window&                      window,
                             const HedgehogSettings::Settings& settings,
                             const ResourceManager&           resourceManager)
    {
        m_InitPass      = std::make_unique<InitPass>();
        m_DepthPrePass  = std::make_unique<DepthPrePass>(device, resourceManager);
        m_ShadowmapPass = std::make_unique<ShadowmapPass>(device, settings, resourceManager);
        m_ForwardPass   = std::make_unique<ForwardPass>(device, resourceManager);
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

    void RenderQueue::Render(const FD::FrameData& frame,
                             RHI::IRHIDevice&     device,
                             RHI::IRHISwapchain&  swapchain,
                             RHI::IRHICommandList& cmd,
                             RHI::IRHIFence&      fence,
                             RHI::IRHISemaphore&  imageAvailableSemaphore,
                             RHI::IRHISemaphore&  renderFinishedSemaphore,
                             uint32_t             frameIndex,
                             const ResourceManager& resourceManager)
    {
        const uint32_t backBufferIndex = m_InitPass->Render(
            swapchain, fence, imageAvailableSemaphore, cmd);

        m_ShadowmapPass->Render(frame, resourceManager, cmd, frameIndex);
        m_DepthPrePass->Render(frame, resourceManager, cmd, frameIndex);
        m_ForwardPass->Render(frame, resourceManager, cmd, frameIndex);

        auto& sceneBuffer = const_cast<RHI::IRHITexture&>(resourceManager.GetSceneColorBuffer());
        cmd.TransitionTexture(sceneBuffer,
            RHI::ImageLayout::ColorAttachment,
            RHI::ImageLayout::ShaderReadOnly);

        m_GuiPass->Render(cmd, resourceManager);

        auto& colorBuffer = const_cast<RHI::IRHITexture&>(resourceManager.GetRHIColorBuffer());
        m_PresentPass->Render(cmd, device, swapchain, colorBuffer, backBufferIndex,
                              imageAvailableSemaphore, renderFinishedSemaphore, fence);
    }

    void RenderQueue::UpdateData(const FD::FrameData&             frame,
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
