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
                              RHI::IRHIDevice&                 device,
                              RHI::IRHICommandList&            cmd,
                              uint32_t                         frameIndex,
                              const ResourceManager&           resourceManager)
    {
        RenderContext ctx{frame, device, cmd, resourceManager, frameIndex};

        for (auto& node : m_Nodes)
            node->Render(ctx);
    }

    void RenderQueue::PreRender(const HedgehogEngine::FrameData&  frame,
                                       uint32_t                          frameIndex,
                                       const HedgehogSettings::Settings& settings)
    {
        for (auto& node : m_Nodes)
            node->PreRender(frame, frameIndex, settings);
    }

} // namespace Renderer
