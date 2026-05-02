#include "RenderQueue.hpp"

#include "IRenderNode.hpp"
#include "RenderContext.hpp"
#include "Nodes/GuiNode.hpp"
#include "NodeFactory/RenderQueueLoader.hpp"
#include "NodeFactory/NodeFactory.hpp"

#include "ResourceManager/ResourceManager.hpp"

#include "HedgehogSettings/Settings/HedgehogSettings.hpp"

#include "HedgehogEngine/api/Frame/FrameData.hpp"

#include <cassert>

namespace Renderer
{
    RenderQueue::RenderQueue(RHI::IRHIDevice&                  device,
                              HW::Window&                       window,
                              const HedgehogSettings::Settings& settings,
                              ResourceManager&                  resourceManager)
    {
        const auto configs = RenderQueueLoader::Load(MAIN_RENDER_QUEUE_PATH);
        assert(!configs.empty() && "Main.rq failed to parse or is empty");

        NodeFactory factory(device, window, settings, resourceManager);
        for (const auto& cfg : configs)
            m_Nodes.push_back(factory.Create(cfg));

        for (auto& node : m_Nodes)
        {
            if (auto* gui = dynamic_cast<GuiNode*>(node.get()))
            {
                m_GuiNode = gui;
                break;
            }
        }
        assert(m_GuiNode && "Main.rq must contain a Gui node");
    }

    RenderQueue::~RenderQueue() = default;

    void RenderQueue::Cleanup(RHI::IRHIDevice& device)
    {
        for (auto& node : m_Nodes)
            node->Cleanup(device);
    }

    void RenderQueue::BeginGui()
    {
        m_GuiNode->BeginFrame();
    }

    void RenderQueue::DiscardGui()
    {
        m_GuiNode->DiscardFrame();
    }

    void* RenderQueue::GetSceneViewTextureId() const
    {
        return m_GuiNode->GetSceneViewTextureId();
    }

    void RenderQueue::Render(const HedgehogEngine::FrameData& frame,
                              RHI::IRHIDevice&      device,
                              RHI::IRHISwapchain&   swapchain,
                              RHI::IRHICommandList& cmd,
                              RHI::IRHIFence&       fence,
                              RHI::IRHISemaphore&   imageAvailableSemaphore,
                              RHI::IRHISemaphore&   renderFinishedSemaphore,
                              uint32_t              frameIndex,
                              const ResourceManager& resourceManager)
    {
        RenderContext ctx;
        ctx.m_FrameData       = &frame;
        ctx.m_Device          = &device;
        ctx.m_Swapchain       = &swapchain;
        ctx.m_Cmd             = &cmd;
        ctx.m_Fence           = &fence;
        ctx.m_ImageAvailable  = &imageAvailableSemaphore;
        ctx.m_RenderFinished  = &renderFinishedSemaphore;
        ctx.m_ResourceManager = &resourceManager;
        ctx.m_FrameIndex      = frameIndex;

        for (auto& node : m_Nodes)
            node->Render(ctx);
    }

    void RenderQueue::UpdateData(const HedgehogEngine::FrameData&  frame,
                                  uint32_t                          frameIndex,
                                  const HedgehogSettings::Settings& settings)
    {
        for (auto& node : m_Nodes)
            node->UpdateData(frame, frameIndex, settings);
    }

    void RenderQueue::ResizeResources(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        for (auto& node : m_Nodes)
            node->OnResizeFramebuffer(device, resourceManager);
    }

    void RenderQueue::ResizeSceneView(RHI::IRHIDevice& device, const ResourceManager& resourceManager)
    {
        for (auto& node : m_Nodes)
            node->OnResizeSceneView(device, resourceManager);
    }

    void RenderQueue::UpdateResources(RHI::IRHIDevice&                  device,
                                       const HedgehogSettings::Settings& settings,
                                       const ResourceManager&            resourceManager)
    {
        for (auto& node : m_Nodes)
            node->OnUpdateResources(device, settings, resourceManager);
    }

} // namespace Renderer
