#include "RenderQueue.hpp"

#include "RenderNodes/IRenderNode.hpp"
#include "RenderNodes/InitNode.hpp"
#include "RenderNodes/ShadowmapNode.hpp"
#include "RenderNodes/DepthPrepassNode.hpp"
#include "RenderNodes/ForwardNode.hpp"
#include "RenderNodes/GuiNode.hpp"
#include "RenderNodes/PresentNode.hpp"

#include "HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "HedgehogEngine/api/Frame/FrameData.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHISyncPrimitive.hpp"

namespace Renderer
{
    RenderQueue::RenderQueue(RHI::IRHIDevice&                  device,
                             HW::Window&                       window,
                             const HedgehogSettings::Settings& settings,
                             ResourceManager&                  resourceManager)
    {
        m_Nodes.push_back(std::make_unique<InitNode>());
        m_Nodes.push_back(std::make_unique<ShadowmapNode>(device, settings, resourceManager));
        m_Nodes.push_back(std::make_unique<DepthPrepassNode>(device, resourceManager));
        m_Nodes.push_back(std::make_unique<ForwardNode>(device, resourceManager));
        m_Nodes.push_back(std::make_unique<GuiNode>(window, device, resourceManager));
        m_Nodes.push_back(std::make_unique<PresentNode>());
    }

    RenderQueue::~RenderQueue() = default;

    void RenderQueue::Cleanup(RHI::IRHIDevice& device)
    {
        for (auto& node : m_Nodes)
            node->Cleanup(device);
    }

    void RenderQueue::BeginGui()
    {
        for (auto& node : m_Nodes)
            node->BeginFrame();
    }

    void RenderQueue::DiscardGui()
    {
        for (auto& node : m_Nodes)
            node->DiscardFrame();
    }

    void RenderQueue::Render(const HedgehogEngine::FrameData& frame,
                             RHI::IRHIDevice&                 device,
                             RHI::IRHISwapchain&              swapchain,
                             RHI::IRHICommandList&            cmd,
                             RHI::IRHIFence&                  fence,
                             RHI::IRHISemaphore&              imageAvailableSemaphore,
                             RHI::IRHISemaphore&              renderFinishedSemaphore,
                             uint32_t                         frameIndex,
                             const ResourceManager&           resourceManager)
    {
        NodeContext ctx{
            .frame                   = frame,
            .device                  = device,
            .swapchain               = swapchain,
            .cmd                     = cmd,
            .fence                   = fence,
            .imageAvailableSemaphore = imageAvailableSemaphore,
            .renderFinishedSemaphore = renderFinishedSemaphore,
            .frameIndex              = frameIndex,
            .resourceManager         = resourceManager,
        };

        for (auto& node : m_Nodes)
        {
            if (node->IsEnabled())
                node->Execute(ctx);
        }
    }

    void RenderQueue::UpdateData(const HedgehogEngine::FrameData&  frame,
                                 uint32_t                          frameIndex,
                                 const HedgehogSettings::Settings& settings)
    {
        for (auto& node : m_Nodes)
            node->UpdateData(frame, frameIndex, settings);
    }

    void RenderQueue::ResizeResources(RHI::IRHIDevice& device,
                                      const ResourceManager& resourceManager)
    {
        for (auto& node : m_Nodes)
            node->OnWindowResize(device, resourceManager);
    }

    void RenderQueue::ResizeSceneView(RHI::IRHIDevice& device,
                                      const ResourceManager& resourceManager)
    {
        for (auto& node : m_Nodes)
            node->OnSceneViewResize(device, resourceManager);
    }

    void RenderQueue::UpdateResources(RHI::IRHIDevice&                  device,
                                      const HedgehogSettings::Settings& settings,
                                      const ResourceManager&            resourceManager)
    {
        for (auto& node : m_Nodes)
            node->OnSettingsChanged(device, settings, resourceManager);
    }

    void* RenderQueue::GetSceneViewTextureId() const
    {
        for (const auto& node : m_Nodes)
        {
            if (void* id = node->GetSceneViewTextureId())
                return id;
        }
        return nullptr;
    }
}
