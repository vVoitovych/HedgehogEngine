#include "RenderGraph.hpp"

#include "RenderContext.hpp"
#include "RenderNodes/IRenderNode.hpp"
#include "ResourceManager/ResourceManager.hpp"

#include "HedgehogEngine/api/Frame/FrameData.hpp"
#include "HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHICommandList.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>
#include <unordered_map>
#include <utility>

namespace Renderer
{
    void RenderGraph::AddNode(IRenderNode* node)
    {
        assert(node && "RenderGraph::AddNode: null node");
        m_Nodes.push_back(node);
    }

    void RenderGraph::Compile(const ResourceManager& resourceManager)
    {
        assert(!m_Nodes.empty() && "RenderGraph::Compile() called with no nodes");

        for (IRenderNode* node : m_Nodes)
            node->Setup(*this);

        BuildTextureRegistry(resourceManager);
        BuildBarrierPlan();
    }

    void RenderGraph::BuildTextureRegistry(const ResourceManager& resourceManager)
    {
        m_TextureRegistry.clear();
        m_TextureRegistry["RHIColorBuffer"]   = &resourceManager.GetRHIColorBuffer();
        m_TextureRegistry["RHIDepthBuffer"]   = &resourceManager.GetRHIDepthBuffer();
        m_TextureRegistry["RHIShadowMap"]     = &resourceManager.GetRHIShadowMap();
        m_TextureRegistry["RHIShadowMask"]    = &resourceManager.GetRHIShadowMask();
        m_TextureRegistry["SceneColorBuffer"] = &resourceManager.GetSceneColorBuffer();
    }

    void RenderGraph::BuildBarrierPlan()
    {
        m_BarrierPlan.assign(m_Nodes.size(), {});

        // Track the last node that wrote each resource and the layout it left it in.
        std::unordered_map<std::string, std::pair<size_t, RHI::ImageLayout>> lastWrite;

        for (size_t i = 0; i < m_Nodes.size(); ++i)
        {
            const RenderNodeDesc& desc = m_Nodes[i]->GetDesc();

            // For each declared input: if a prior write exists, plan a transition barrier.
            for (const auto& slot : desc.inputs)
            {
                auto writeIt = lastWrite.find(slot.name);
                auto texIt   = m_TextureRegistry.find(slot.name);
                if (writeIt != lastWrite.end() && texIt != m_TextureRegistry.end())
                {
                    m_BarrierPlan[i].push_back({
                        texIt->second,
                        writeIt->second.second,  // from: producer's post-write layout
                        slot.layout              // to:   consumer's required layout
                    });
                }
            }

            // Record this node's output writes.
            for (const auto& slot : desc.outputs)
                lastWrite[slot.name] = { i, slot.layout };
        }
    }

    void RenderGraph::Execute(RenderContext& ctx)
    {
        auto& cmd = ctx.GetCommandList();
        for (size_t i = 0; i < m_Nodes.size(); ++i)
        {
            for (const auto& barrier : m_BarrierPlan[i])
            {
                cmd.TransitionTexture(
                    const_cast<RHI::IRHITexture&>(*barrier.m_Texture),
                    barrier.m_FromLayout,
                    barrier.m_ToLayout);
            }

            if (m_Nodes[i]->IsEnabled())
                m_Nodes[i]->Execute(ctx);
        }
    }

    void RenderGraph::Cleanup(RHI::IRHIDevice& device)
    {
        for (IRenderNode* node : m_Nodes)
            node->Cleanup(device);
        m_Nodes.clear();
    }

    void RenderGraph::BeginFrame()
    {
        for (IRenderNode* node : m_Nodes)
            node->BeginFrame();
    }

    void RenderGraph::DiscardFrame()
    {
        for (IRenderNode* node : m_Nodes)
            node->DiscardFrame();
    }

    void* RenderGraph::GetSceneViewTextureId() const
    {
        for (IRenderNode* node : m_Nodes)
        {
            if (void* id = node->GetSceneViewTextureId())
                return id;
        }
        return nullptr;
    }

    void RenderGraph::UpdateData(const HedgehogEngine::FrameData&   frame,
                                 uint32_t                           frameIndex,
                                 const HedgehogSettings::Settings&  settings)
    {
        for (IRenderNode* node : m_Nodes)
            node->UpdateData(frame, frameIndex, settings);
    }

    void RenderGraph::OnWindowResize(RHI::IRHIDevice& device,
                                     const ResourceManager& resourceManager)
    {
        for (IRenderNode* node : m_Nodes)
            node->OnWindowResize(device, resourceManager);

        BuildTextureRegistry(resourceManager);
        BuildBarrierPlan();
    }

    void RenderGraph::OnSceneViewResize(RHI::IRHIDevice& device,
                                        const ResourceManager& resourceManager)
    {
        for (IRenderNode* node : m_Nodes)
            node->OnSceneViewResize(device, resourceManager);

        BuildTextureRegistry(resourceManager);
        BuildBarrierPlan();
    }

    void RenderGraph::OnSettingsChanged(RHI::IRHIDevice& device,
                                        const HedgehogSettings::Settings& settings,
                                        const ResourceManager& resourceManager)
    {
        for (IRenderNode* node : m_Nodes)
            node->OnSettingsChanged(device, settings, resourceManager);

        BuildTextureRegistry(resourceManager);
        BuildBarrierPlan();
    }
}
