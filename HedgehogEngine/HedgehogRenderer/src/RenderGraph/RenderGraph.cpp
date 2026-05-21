#include "RenderGraph.hpp"

#include "RenderContext.hpp"
#include "RenderNodes/IRenderNode.hpp"
#include "ResourceManager/ResourceManager.hpp"

#include "HedgehogEngine/api/Frame/FrameData.hpp"
#include "HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "RHI/api/IRHIDevice.hpp"

#include "Logger/api/Logger.hpp"

#include <cassert>

namespace Renderer
{
    void RenderGraph::AddNode(std::unique_ptr<IRenderNode> node)
    {
        m_Nodes.push_back(std::move(node));
    }

    void RenderGraph::Compile(const ResourceManager& resourceManager)
    {
        assert(!m_Nodes.empty() && "RenderGraph::Compile() called with no nodes");

        for (auto& node : m_Nodes)
            node->Setup(*this);

        BuildTextureRegistry(resourceManager);
    }

    void RenderGraph::BuildTextureRegistry(const ResourceManager& resourceManager)
    {
        m_TextureRegistry.clear();
        m_TextureRegistry["RHIColorBuffer"]  = &resourceManager.GetRHIColorBuffer();
        m_TextureRegistry["RHIDepthBuffer"]  = &resourceManager.GetRHIDepthBuffer();
        m_TextureRegistry["RHIShadowMap"]    = &resourceManager.GetRHIShadowMap();
        m_TextureRegistry["RHIShadowMask"]   = &resourceManager.GetRHIShadowMask();
        m_TextureRegistry["SceneColorBuffer"] = &resourceManager.GetSceneColorBuffer();
    }

    void RenderGraph::Execute(RenderContext& ctx)
    {
        for (auto& node : m_Nodes)
        {
            if (node->IsEnabled())
                node->Execute(ctx);
        }
    }

    void RenderGraph::Cleanup(RHI::IRHIDevice& device)
    {
        for (auto& node : m_Nodes)
            node->Cleanup(device);
    }

    void RenderGraph::BeginFrame()
    {
        for (auto& node : m_Nodes)
            node->BeginFrame();
    }

    void RenderGraph::DiscardFrame()
    {
        for (auto& node : m_Nodes)
            node->DiscardFrame();
    }

    void* RenderGraph::GetSceneViewTextureId() const
    {
        for (const auto& node : m_Nodes)
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
        for (auto& node : m_Nodes)
            node->UpdateData(frame, frameIndex, settings);
    }

    void RenderGraph::OnWindowResize(RHI::IRHIDevice& device,
                                     const ResourceManager& resourceManager)
    {
        for (auto& node : m_Nodes)
            node->OnWindowResize(device, resourceManager);

        BuildTextureRegistry(resourceManager);
    }

    void RenderGraph::OnSceneViewResize(RHI::IRHIDevice& device,
                                        const ResourceManager& resourceManager)
    {
        for (auto& node : m_Nodes)
            node->OnSceneViewResize(device, resourceManager);

        BuildTextureRegistry(resourceManager);
    }

    void RenderGraph::OnSettingsChanged(RHI::IRHIDevice& device,
                                        const HedgehogSettings::Settings& settings,
                                        const ResourceManager& resourceManager)
    {
        for (auto& node : m_Nodes)
            node->OnSettingsChanged(device, settings, resourceManager);

        BuildTextureRegistry(resourceManager);
    }
}
