#include "RenderQueue.hpp"

#include "IRenderNode.hpp"
#include "PreRenderContext.hpp"
#include "RenderContext.hpp"
#include "NodeFactory/RenderQueueLoader.hpp"
#include "NodeFactory/NodeFactory.hpp"

#include "ResourceManager/ResourceManager.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>

namespace Renderer
{
    RenderQueue::RenderQueue(RHI::IRHIDevice& device, ResourceManager& resourceManager)
    {
        const auto configs = RenderQueueLoader::Load(MAIN_RENDER_QUEUE_PATH);
        assert(!configs.empty() && "Main.rq failed to parse or is empty");

        NodeFactory factory(device, resourceManager);
        for (const auto& cfg : configs)
        {
            auto node = factory.Create(cfg);
            if (!node)
            {
                LOGERROR("RenderQueue: skipping pass '", cfg.m_Name, "' (factory returned null)");
                continue;
            }
            m_NodeNames.push_back(cfg.m_Name);
            m_Nodes.push_back(std::move(node));
        }
    }

    RenderQueue::~RenderQueue() = default;

    void RenderQueue::Cleanup(RHI::IRHIDevice& device)
    {
        for (auto& node : m_Nodes)
            node->Cleanup(device);
    }

    void RenderQueue::OnBeginFrame()
    {
        for (auto& node : m_Nodes)
            node->OnBeginFrame();
    }

    void RenderQueue::OnDiscardFrame()
    {
        for (auto& node : m_Nodes)
            node->OnDiscardFrame();
    }

    void* RenderQueue::QueryNodeExport(const std::string& nodeName, const std::string& key) const
    {
        for (size_t i = 0; i < m_Nodes.size(); ++i)
        {
            if (m_NodeNames[i] == nodeName)
                return m_Nodes[i]->ExportResource(key);
        }
#ifdef DEBUG
        LOGWARNING("RenderQueue::QueryNodeExport: node '", nodeName, "' not found (key='", key, "')");
#endif
        return nullptr;
    }

    void RenderQueue::AppendNode(const std::string& name, std::unique_ptr<IRenderNode> node)
    {
        m_NodeNames.push_back(name);
        m_Nodes.push_back(std::move(node));
    }

    void RenderQueue::PreRender(const PreRenderContext& ctx)
    {
        for (auto& node : m_Nodes)
            node->PreRender(ctx);
    }

    void RenderQueue::Render(RenderContext& ctx)
    {
        for (auto& node : m_Nodes)
            node->Render(ctx);
    }

} // namespace Renderer
